/*
 * BK Id: %F% %I% %G% %U% %#%
 */
/*
 * Smp support for ppc.
 *
 * Written by Cort Dougan (cort@cs.nmt.edu) borrowing a great
 * deal of code from the sparc and intel versions.
 *
 * Copyright (C) 1999 Cort Dougan <cort@cs.nmt.edu>
 *
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/delay.h>
#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/cache.h>

#include <asm/ptrace.h>
#include <asm/atomic.h>
#include <asm/irq.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/hardirq.h>
#include <asm/softirq.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/smp.h>
#include <asm/residual.h>
#include <asm/time.h>
#include <asm/thread_info.h>
#include <asm/tlbflush.h>

int smp_threads_ready;
volatile int smp_commenced;
int smp_tb_synchronized;
struct cpuinfo_PPC cpu_data[NR_CPUS];
struct klock_info_struct klock_info = { KLOCK_CLEAR, 0 };
atomic_t ipi_recv;
atomic_t ipi_sent;
spinlock_t kernel_flag __cacheline_aligned_in_smp = SPIN_LOCK_UNLOCKED;
unsigned int prof_multiplier[NR_CPUS] = { [1 ... NR_CPUS-1] = 1 };
unsigned int prof_counter[NR_CPUS] = { [1 ... NR_CPUS-1] = 1 };
unsigned long cache_decay_ticks = HZ/100;
unsigned long cpu_online_map = 1UL;
unsigned long cpu_possible_map = 1UL;
int smp_hw_index[NR_CPUS];
struct thread_info *secondary_ti;

/* SMP operations for this machine */
static struct smp_ops_t *smp_ops;

/* all cpu mappings are 1-1 -- Cort */
volatile unsigned long cpu_callin_map[NR_CPUS];

#define TB_SYNC_PASSES 4
volatile unsigned long __initdata tb_sync_flag = 0;
volatile unsigned long __initdata tb_offset = 0;

int start_secondary(void *);
extern int cpu_idle(void *unused);
void smp_call_function_interrupt(void);
static int __smp_call_function(void (*func) (void *info), void *info,
			       int wait, int target);

/* Since OpenPIC has only 4 IPIs, we use slightly different message numbers.
 * 
 * Make sure this matches openpic_request_IPIs in open_pic.c, or what shows up
 * in /proc/interrupts will be wrong!!! --Troy */
#define PPC_MSG_CALL_FUNCTION	0
#define PPC_MSG_RESCHEDULE	1
#define PPC_MSG_INVALIDATE_TLB	2
#define PPC_MSG_XMON_BREAK	3

#define smp_message_pass(t,m,d,w) \
    do { if (smp_ops) \
	     atomic_inc(&ipi_sent); \
	     smp_ops->message_pass((t),(m),(d),(w)); \
       } while(0)

/* 
 * Common functions
 */
void smp_local_timer_interrupt(struct pt_regs * regs)
{
	int cpu = smp_processor_id();

	if (!--prof_counter[cpu]) {
		update_process_times(user_mode(regs));
		prof_counter[cpu]=prof_multiplier[cpu];
	}
}

void smp_message_recv(int msg, struct pt_regs *regs)
{
	atomic_inc(&ipi_recv);
	
	switch( msg ) {
	case PPC_MSG_CALL_FUNCTION:
		smp_call_function_interrupt();
		break;
	case PPC_MSG_RESCHEDULE: 
		set_need_resched();
		break;
	case PPC_MSG_INVALIDATE_TLB:
		_tlbia();
		break;
#ifdef CONFIG_XMON
	case PPC_MSG_XMON_BREAK:
		xmon(regs);
		break;
#endif /* CONFIG_XMON */
	default:
		printk("SMP %d: smp_message_recv(): unknown msg %d\n",
		       smp_processor_id(), msg);
		break;
	}
}

/*
 * 750's don't broadcast tlb invalidates so
 * we have to emulate that behavior.
 *   -- Cort
 */
void smp_send_tlb_invalidate(int cpu)
{
	if ( PVR_VER(mfspr(PVR)) == 8 )
		smp_message_pass(MSG_ALL_BUT_SELF, PPC_MSG_INVALIDATE_TLB, 0, 0);
}

void smp_send_reschedule(int cpu)
{
	/*
	 * This is only used if `cpu' is running an idle task,
	 * so it will reschedule itself anyway...
	 *
	 * This isn't the case anymore since the other CPU could be
	 * sleeping and won't reschedule until the next interrupt (such
	 * as the timer).
	 *  -- Cort
	 */
	/* This is only used if `cpu' is running an idle task,
	   so it will reschedule itself anyway... */
	smp_message_pass(cpu, PPC_MSG_RESCHEDULE, 0, 0);
}

#ifdef CONFIG_XMON
void smp_send_xmon_break(int cpu)
{
	smp_message_pass(cpu, PPC_MSG_XMON_BREAK, 0, 0);
}
#endif /* CONFIG_XMON */

static void stop_this_cpu(void *dummy)
{
	local_irq_disable();
	while (1)
		;
}

void smp_send_stop(void)
{
	smp_call_function(stop_this_cpu, NULL, 1, 0);
}

/*
 * Structure and data for smp_call_function(). This is designed to minimise
 * static memory requirements. It also looks cleaner.
 * Stolen from the i386 version.
 */
static spinlock_t call_lock = SPIN_LOCK_UNLOCKED;

static struct call_data_struct {
	void (*func) (void *info);
	void *info;
	atomic_t started;
	atomic_t finished;
	int wait;
} *call_data;

/*
 * this function sends a 'generic call function' IPI to all other CPUs
 * in the system.
 */

int smp_call_function(void (*func) (void *info), void *info, int nonatomic,
		      int wait)
/*
 * [SUMMARY] Run a function on all other CPUs.
 * <func> The function to run. This must be fast and non-blocking.
 * <info> An arbitrary pointer to pass to the function.
 * <nonatomic> currently unused.
 * <wait> If true, wait (atomically) until function has completed on other CPUs.
 * [RETURNS] 0 on success, else a negative status code. Does not return until
 * remote CPUs are nearly ready to execute <<func>> or are or have executed.
 *
 * You must not call this function with disabled interrupts or from a
 * hardware interrupt handler or from a bottom half handler.
 */
{
	/* FIXME: get cpu lock with hotplug cpus, or change this to
           bitmask. --RR */
	if (num_online_cpus() <= 1)
		return 0;
	return __smp_call_function(func, info, wait, MSG_ALL_BUT_SELF);
}

static int __smp_call_function(void (*func) (void *info), void *info,
			       int wait, int target)
{
	struct call_data_struct data;
	int ret = -1;
	int timeout;
	int ncpus = 1;

	if (target == MSG_ALL_BUT_SELF)
		ncpus = num_online_cpus() - 1;
	else if (target == MSG_ALL)
		ncpus = num_online_cpus();

	data.func = func;
	data.info = info;
	atomic_set(&data.started, 0);
	data.wait = wait;
	if (wait)
		atomic_set(&data.finished, 0);

	spin_lock(&call_lock);
	call_data = &data;
	/* Send a message to all other CPUs and wait for them to respond */
	smp_message_pass(target, PPC_MSG_CALL_FUNCTION, 0, 0);

	/* Wait for response */
	timeout = 1000000;
	while (atomic_read(&data.started) != ncpus) {
		if (--timeout == 0) {
			printk("smp_call_function on cpu %d: other cpus not responding (%d)\n",
			       smp_processor_id(), atomic_read(&data.started));
			goto out;
		}
		barrier();
		udelay(1);
	}

	if (wait) {
		timeout = 1000000;
		while (atomic_read(&data.finished) != ncpus) {
			if (--timeout == 0) {
				printk("smp_call_function on cpu %d: other cpus not finishing (%d/%d)\n",
				       smp_processor_id(), atomic_read(&data.finished), atomic_read(&data.started));
				goto out;
			}
			barrier();
			udelay(1);
		}
	}
	ret = 0;

 out:
	spin_unlock(&call_lock);
	return ret;
}

void smp_call_function_interrupt(void)
{
	void (*func) (void *info) = call_data->func;
	void *info = call_data->info;
	int wait = call_data->wait;

	/*
	 * Notify initiating CPU that I've grabbed the data and am
	 * about to execute the function
	 */
	atomic_inc(&call_data->started);
	/*
	 * At this point the info structure may be out of scope unless wait==1
	 */
	(*func)(info);
	if (wait)
		atomic_inc(&call_data->finished);
}

#if 0 /* Old boot code. */
void __init smp_boot_cpus(void)
{
	int i, cpu_nr;
	struct task_struct *p;

	printk("Entering SMP Mode...\n");
        smp_store_cpu_info(0);
	cpu_online_map = 1UL;

	/*
	 * assume for now that the first cpu booted is
	 * cpu 0, the master -- Cort
	 */
	cpu_callin_map[0] = 1;

	for (i = 0; i < NR_CPUS; i++) {
		prof_counter[i] = 1;
		prof_multiplier[i] = 1;
	}

	/*
	 * XXX very rough.
	 */
	cache_decay_ticks = HZ/100;

	smp_ops = ppc_md.smp_ops;
	if (smp_ops == NULL) {
		printk("SMP not supported on this machine.\n");
		return;
	}

	/* Probe platform for CPUs */
	cpu_nr = smp_ops->probe();

	/*
	 * only check for cpus we know exist.  We keep the callin map
	 * with cpus at the bottom -- Cort
	 */
	if (cpu_nr > max_cpus)
		cpu_nr = max_cpus;
#ifdef CONFIG_PPC_ISERIES
	smp_iSeries_space_timers( cpu_nr );
#endif
	for (i = 1; i < cpu_nr; i++) {
		int c;
		struct pt_regs regs;
		
		/* create a process for the processor */
		/* only regs.msr is actually used, and 0 is OK for it */
		memset(&regs, 0, sizeof(struct pt_regs));
		p = do_fork(CLONE_VM|CLONE_IDLETASK, 0, &regs, 0);
		if (IS_ERR(p))
			panic("failed fork for CPU %d", i);
		init_idle(p, i);
		unhash_process(p);

		secondary_ti = p->thread_info;
		p->thread_info->cpu = i;

		/*
		 * There was a cache flush loop here to flush the cache
		 * to memory for the first 8MB of RAM.  The cache flush
		 * has been pushed into the kick_cpu function for those
		 * platforms that need it.
		 */

		/* wake up cpus */
		smp_ops->kick_cpu(i);
		
		/*
		 * wait to see if the cpu made a callin (is actually up).
		 * use this value that I found through experimentation.
		 * -- Cort
		 */
		for (c = 1000; c && !cpu_callin_map[i]; c--)
			udelay(100);
		
		if (cpu_callin_map[i]) {
			char buf[32];
			sprintf(buf, "found cpu %d", i);
			if (ppc_md.progress) ppc_md.progress(buf, 0x350+i);
			printk("Processor %d found.\n", i);
		} else {
			char buf[32];
			sprintf(buf, "didn't find cpu %d", i);
			if (ppc_md.progress) ppc_md.progress(buf, 0x360+i);
			printk("Processor %d is stuck.\n", i);
		}
	}

	/* Setup CPU 0 last (important) */
	smp_ops->setup_cpu(0);
	
	/* FIXME: Not with hotplug CPUS --RR */
	if (num_online_cpus() < 2)
		smp_tb_synchronized = 1;
}

void __init smp_software_tb_sync(int cpu)
{
#define PASSES 4	/* 4 passes.. */
	int pass;
	int i, j;

	/* stop - start will be the number of timebase ticks it takes for cpu0
	 * to send a message to all others and the first reponse to show up.
	 *
	 * ASSUMPTION: this time is similiar for all cpus
	 * ASSUMPTION: the time to send a one-way message is ping/2
	 */
	register unsigned long start = 0;
	register unsigned long stop = 0;
	register unsigned long temp = 0;

	set_tb(0, 0);

	/* multiple passes to get in l1 cache.. */
	for (pass = 2; pass < 2+PASSES; pass++){
		if (cpu == 0){
			mb();
			for (i = j = 1; i < NR_CPUS; i++, j++){
				/* skip stuck cpus */
				while (!cpu_callin_map[j])
					++j;
				while (cpu_callin_map[j] != pass)
					barrier();
			}
			mb();
			tb_sync_flag = pass;
			start = get_tbl();	/* start timing */
			while (tb_sync_flag)
				mb();
			stop = get_tbl();	/* end timing */
			/* theoretically, the divisor should be 2, but
			 * I get better results on my dual mtx. someone
			 * please report results on other smp machines..
			 */
			tb_offset = (stop-start)/4;
			mb();
			tb_sync_flag = pass;
			udelay(10);
			mb();
			tb_sync_flag = 0;
			mb();
			set_tb(0,0);
			mb();
		} else {
			cpu_callin_map[cpu] = pass;
			mb();
			while (!tb_sync_flag)
				mb();		/* wait for cpu0 */
			mb();
			tb_sync_flag = 0;	/* send response for timing */
			mb();
			while (!tb_sync_flag)
				mb();
			temp = tb_offset;	/* make sure offset is loaded */
			while (tb_sync_flag)
				mb();
			set_tb(0,temp);		/* now, set the timebase */
			mb();
		}
	}
	if (cpu == 0) {
		smp_tb_synchronized = 1;
		printk("smp_software_tb_sync: %d passes, final offset: %ld\n",
			PASSES, tb_offset);
	}
	/* so time.c doesn't get confused */
	set_dec(tb_ticks_per_jiffy);
	last_jiffy_stamp(cpu) = 0;
}

void __init smp_commence(void)
{
	/*
	 *	Lets the callin's below out of their loop.
	 */
	if (ppc_md.progress) ppc_md.progress("smp_commence", 0x370);
	wmb();
	smp_commenced = 1;

	/* if the smp_ops->setup_cpu function has not already synched the
	 * timebases with a nicer hardware-based method, do so now
	 *
	 * I am open to suggestions for improvements to this method
	 * -- Troy <hozer@drgw.net>
	 *
	 * NOTE: if you are debugging, set smp_tb_synchronized for now
	 * since if this code runs pretty early and needs all cpus that
	 * reported in in smp_callin_map to be working
	 *
	 * NOTE2: this code doesn't seem to work on > 2 cpus. -- paulus/BenH
	 */
	/* FIXME: This doesn't work with hotplug CPUs --RR */
	if (!smp_tb_synchronized && num_online_cpus() == 2) {
		unsigned long flags;
		local_irq_save(flags);	
		smp_software_tb_sync(0);
		local_irq_restore(flags);
	}
}

void __init smp_callin(void)
{
	int cpu = smp_processor_id();
	
        smp_store_cpu_info(cpu);
	set_dec(tb_ticks_per_jiffy);
	/* Set online before we acknowledge. */
	set_bit(cpu, &cpu_online_map);
	wmb();
	cpu_callin_map[cpu] = 1;

	smp_ops->setup_cpu(cpu);

	while (!smp_commenced)
		barrier();

	/* see smp_commence for more info */
	if (!smp_tb_synchronized && num_online_cpus() == 2) {
		smp_software_tb_sync(cpu);
	}
	local_irq_enable();
}

/* intel needs this */
void __init initialize_secondary(void)
{
}

/* Activate a secondary processor. */
int __init start_secondary(void *unused)
{
	atomic_inc(&init_mm.mm_count);
	current->active_mm = &init_mm;
	smp_callin();
	return cpu_idle(NULL);
}

void __init smp_setup(char *str, int *ints)
{
}

int __init setup_profiling_timer(unsigned int multiplier)
{
	return 0;
}

void __init smp_store_cpu_info(int id)
{
        struct cpuinfo_PPC *c = &cpu_data[id];

	/* assume bogomips are same for everything */
        c->loops_per_jiffy = loops_per_jiffy;
        c->pvr = mfspr(PVR);
}

static int __init maxcpus(char *str)
{
	get_option(&str, &max_cpus);
	return 1;
}

__setup("maxcpus=", maxcpus);
#else /* New boot code */
/* FIXME: Do this properly for all archs --RR */
static spinlock_t timebase_lock = SPIN_LOCK_UNLOCKED;
static unsigned int timebase_upper = 0, timebase_lower = 0;

void __devinit
smp_generic_give_timebase(void)
{
	spin_lock(&timebase_lock);
	do {
		timebase_upper = get_tbu();
		timebase_lower = get_tbl();
	} while (timebase_upper != get_tbu());
	spin_unlock(&timebase_lock);

	while (timebase_upper || timebase_lower)
		rmb();
}

void __devinit
smp_generic_take_timebase(void)
{
	int done = 0;

	while (!done) {
		spin_lock(&timebase_lock);
		if (timebase_upper || timebase_lower) {
			set_tb(timebase_upper, timebase_lower);
			timebase_upper = 0;
			timebase_lower = 0;
			done = 1;
		}
		spin_unlock(&timebase_lock);
	}
}

static void __devinit smp_store_cpu_info(int id)
{
        struct cpuinfo_PPC *c = &cpu_data[id];

	/* assume bogomips are same for everything */
        c->loops_per_jiffy = loops_per_jiffy;
        c->pvr = mfspr(PVR);
}

void __init smp_prepare_cpus(unsigned int max_cpus)
{
	int num_cpus;

	/* Fixup boot cpu */
        smp_store_cpu_info(smp_processor_id());
	cpu_callin_map[smp_processor_id()] = 1;

	smp_ops = ppc_md.smp_ops;
	if (smp_ops == NULL) {
		printk("SMP not supported on this machine.\n");
		return;
	}

	/* Probe platform for CPUs: always linear. */
	num_cpus = smp_ops->probe();
	cpu_possible_map = (1 << num_cpus)-1;

	if (smp_ops->space_timers)
		smp_ops->space_timers(num_cpus);
}

int __init setup_profiling_timer(unsigned int multiplier)
{
	return 0;
}

/* Processor coming up starts here */
int __devinit start_secondary(void *unused)
{
	int cpu;

	atomic_inc(&init_mm.mm_count);
	current->active_mm = &init_mm;

	cpu = smp_processor_id();
        smp_store_cpu_info(cpu);
	set_dec(tb_ticks_per_jiffy);
	cpu_callin_map[cpu] = 1;

	printk("CPU %i done callin...\n", cpu);
	smp_ops->setup_cpu(cpu);
	printk("CPU %i done setup...\n", cpu);
	smp_ops->take_timebase();
	printk("CPU %i done timebase take...\n", cpu);

	return cpu_idle(NULL);
}

int __cpu_up(unsigned int cpu)
{
	struct pt_regs regs;
	struct task_struct *p;
	char buf[32];
	int c;

	/* create a process for the processor */
	/* only regs.msr is actually used, and 0 is OK for it */
	memset(&regs, 0, sizeof(struct pt_regs));
	p = do_fork(CLONE_VM|CLONE_IDLETASK, 0, &regs, 0);
	if (IS_ERR(p))
		panic("failed fork for CPU %u: %li", cpu, PTR_ERR(p));

	init_idle(p, cpu);
	unhash_process(p);

	secondary_ti = p->thread_info;
	p->thread_info->cpu = cpu;

	/*
	 * There was a cache flush loop here to flush the cache
	 * to memory for the first 8MB of RAM.  The cache flush
	 * has been pushed into the kick_cpu function for those
	 * platforms that need it.
	 */

	/* wake up cpu */
	smp_ops->kick_cpu(cpu);
		
	/*
	 * wait to see if the cpu made a callin (is actually up).
	 * use this value that I found through experimentation.
	 * -- Cort
	 */
	for (c = 1000; c && !cpu_callin_map[cpu]; c--)
		udelay(100);

	if (!cpu_callin_map[cpu]) {
		sprintf(buf, "didn't find cpu %u", cpu);
		if (ppc_md.progress) ppc_md.progress(buf, 0x360+cpu);
		printk("Processor %u is stuck.\n", cpu);
		return -ENOENT;
	}

	sprintf(buf, "found cpu %u", cpu);
	if (ppc_md.progress) ppc_md.progress(buf, 0x350+cpu);
	printk("Processor %d found.\n", cpu);

	smp_ops->give_timebase();
	set_bit(cpu, &cpu_online_map);
	return 0;
}

void smp_cpus_done(unsigned int max_cpus)
{
	smp_ops->setup_cpu(0);
}
#endif