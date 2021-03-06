#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/bitops.h>
#include <linux/smp.h>
#include <asm/processor.h>
#include <asm/thread_info.h>
#include <asm/msr.h>

#include "cpu.h"

static int disable_x86_serial_nr __initdata = 1;
static int disable_P4_HT __initdata = 0;
extern int trap_init_f00f_bug(void);

/*
 *	Early probe support logic for ppro memory erratum #50
 *
 *	This is called before we do cpu ident work
 */
 
int __init ppro_with_ram_bug(void)
{
	char vendor_id[16];
	int ident;

	/* Must have CPUID */
	if(!have_cpuid_p())
		return 0;
	if(cpuid_eax(0)<1)
		return 0;
	
	/* Must be Intel */
	cpuid(0, &ident, 
		(int *)&vendor_id[0],
		(int *)&vendor_id[8],
		(int *)&vendor_id[4]);
	
	if(memcmp(vendor_id, "IntelInside", 12))
		return 0;
	
	ident = cpuid_eax(1);

	/* Model 6 */

	if(((ident>>8)&15)!=6)
		return 0;
	
	/* Pentium Pro */

	if(((ident>>4)&15)!=1)
		return 0;
	
	if((ident&15) < 8)
	{
		printk(KERN_INFO "Pentium Pro with Errata#50 detected. Taking evasive action.\n");
		return 1;
	}
	printk(KERN_INFO "Your Pentium Pro seems ok.\n");
	return 0;
}
	
static void __init squash_the_stupid_serial_number(struct cpuinfo_x86 *c)
{
	if( test_bit(X86_FEATURE_PN, c->x86_capability) &&
	    disable_x86_serial_nr ) {
		/* Disable processor serial number */
		unsigned long lo,hi;
		rdmsr(MSR_IA32_BBL_CR_CTL,lo,hi);
		lo |= 0x200000;
		wrmsr(MSR_IA32_BBL_CR_CTL,lo,hi);
		printk(KERN_NOTICE "CPU serial number disabled.\n");
		clear_bit(X86_FEATURE_PN, c->x86_capability);

		/* Disabling the serial number may affect the cpuid level */
		c->cpuid_level = cpuid_eax(0);
	}
}

static int __init x86_serial_nr_setup(char *s)
{
	disable_x86_serial_nr = 0;
	return 1;
}
__setup("serialnumber", x86_serial_nr_setup);

static int __init P4_disable_ht(char *s)
{
	disable_P4_HT = 1;
	return 1;
}
__setup("noht", P4_disable_ht);

#define LVL_1_INST	1
#define LVL_1_DATA	2
#define LVL_2		3
#define LVL_3		4

struct _cache_table
{
	unsigned char descriptor;
	char cache_type;
	short size;
};

/* all the cache descriptor types we care about (no TLB or trace cache entries) */
static struct _cache_table cache_table[] __initdata =
{
	{ 0x06, LVL_1_INST, 8 },
	{ 0x08, LVL_1_INST, 16 },
	{ 0x0A, LVL_1_DATA, 8 },
	{ 0x0C, LVL_1_DATA, 16 },
	{ 0x22, LVL_3,      512 },
	{ 0x23, LVL_3,      1024 },
	{ 0x25, LVL_3,      2048 },
	{ 0x29, LVL_3,      4096 },
	{ 0x41, LVL_2,      128 },
	{ 0x42, LVL_2,      256 },
	{ 0x43, LVL_2,      512 },
	{ 0x44, LVL_2,      1024 },
	{ 0x45, LVL_2,      2048 },
	{ 0x66, LVL_1_DATA, 8 },
	{ 0x67, LVL_1_DATA, 16 },
	{ 0x68, LVL_1_DATA, 32 },
	{ 0x79, LVL_2,      128 },
	{ 0x7A, LVL_2,      256 },
	{ 0x7B, LVL_2,      512 },
	{ 0x7C, LVL_2,      1024 },
	{ 0x82, LVL_2,      256 },
	{ 0x84, LVL_2,      1024 },
	{ 0x85, LVL_2,      2048 },
	{ 0x00, 0, 0}
};

static void __init init_intel(struct cpuinfo_x86 *c)
{
	char *p = NULL;
	unsigned int l1i = 0, l1d = 0, l2 = 0, l3 = 0; /* Cache sizes */

#ifdef CONFIG_X86_F00F_BUG
	/*
	 * All current models of Pentium and Pentium with MMX technology CPUs
	 * have the F0 0F bug, which lets nonpriviledged users lock up the system.
	 * Note that the workaround only should be initialized once...
	 */
	c->f00f_bug = 0;
	if ( c->x86 == 5 ) {
		static int f00f_workaround_enabled = 0;

		c->f00f_bug = 1;
		if ( !f00f_workaround_enabled ) {
			trap_init_f00f_bug();
			printk(KERN_NOTICE "Intel Pentium with F0 0F bug - workaround enabled.\n");
			f00f_workaround_enabled = 1;
		}
	}
#endif


	if (c->cpuid_level > 1) {
		/* supports eax=2  call */
		int i, j, n;
		int regs[4];
		unsigned char *dp = (unsigned char *)regs;

		/* Number of times to iterate */
		n = cpuid_eax(2) & 0xFF;

		for ( i = 0 ; i < n ; i++ ) {
			cpuid(2, &regs[0], &regs[1], &regs[2], &regs[3]);
			
			/* If bit 31 is set, this is an unknown format */
			for ( j = 0 ; j < 3 ; j++ ) {
				if ( regs[j] < 0 ) regs[j] = 0;
			}

			/* Byte 0 is level count, not a descriptor */
			for ( j = 1 ; j < 16 ; j++ ) {
				unsigned char des = dp[j];
				unsigned char k = 0;

				/* look up this descriptor in the table */
				while (cache_table[k].descriptor != 0)
				{
					if (cache_table[k].descriptor == des) {
						switch (cache_table[k].cache_type) {
						case LVL_1_INST:
							l1i += cache_table[k].size;
							break;
						case LVL_1_DATA:
							l1d += cache_table[k].size;
							break;
						case LVL_2:
							l2 += cache_table[k].size;
							break;
						case LVL_3:
							l3 += cache_table[k].size;
							break;
						}

						break;
					}

					k++;
				}
			}
		}
		if ( l1i || l1d )
			printk(KERN_INFO "CPU: L1 I cache: %dK, L1 D cache: %dK\n",
			      l1i, l1d);
		if ( l2 )
			printk(KERN_INFO "CPU: L2 cache: %dK\n", l2);
		if ( l3 )
			printk(KERN_INFO "CPU: L3 cache: %dK\n", l3);

		/*
		 * This assumes the L3 cache is shared; it typically lives in
		 * the northbridge.  The L1 caches are included by the L2
		 * cache, and so should not be included for the purpose of
		 * SMP switching weights.
		 */
		c->x86_cache_size = l2 ? l2 : (l1i+l1d);
	}

	/* SEP CPUID bug: Pentium Pro reports SEP but doesn't have it */
	if ( c->x86 == 6 && c->x86_model < 3 && c->x86_mask < 3 )
		clear_bit(X86_FEATURE_SEP, c->x86_capability);
	
	/* Names for the Pentium II/Celeron processors 
	   detectable only by also checking the cache size.
	   Dixon is NOT a Celeron. */
	if (c->x86 == 6) {
		switch (c->x86_model) {
		case 5:
			if (l2 == 0)
				p = "Celeron (Covington)";
			if (l2 == 256)
				p = "Mobile Pentium II (Dixon)";
			break;
			
		case 6:
			if (l2 == 128)
				p = "Celeron (Mendocino)";
			break;
			
		case 8:
			if (l2 == 128)
				p = "Celeron (Coppermine)";
			break;
		}
	}

	if ( p )
		strcpy(c->x86_model_id, p);
	
#ifdef CONFIG_SMP
	if (test_bit(X86_FEATURE_HT, c->x86_capability) && !disable_P4_HT) {
		extern	int phys_proc_id[NR_CPUS];
		
		u32 	eax, ebx, ecx, edx;
		int 	index_lsb, index_msb, tmp;
		int	initial_apic_id;
		int 	cpu = smp_processor_id();

		cpuid(1, &eax, &ebx, &ecx, &edx);
		smp_num_siblings = (ebx & 0xff0000) >> 16;

		if (smp_num_siblings == 1) {
			printk(KERN_INFO  "CPU: Hyper-Threading is disabled\n");
		} else if (smp_num_siblings > 1 ) {
			index_lsb = 0;
			index_msb = 31;
			/*
			 * At this point we only support two siblings per
			 * processor package.
			 */
#define NR_SIBLINGS	2
			if (smp_num_siblings != NR_SIBLINGS) {
				printk(KERN_WARNING "CPU: Unsupported number of the siblings %d", smp_num_siblings);
				smp_num_siblings = 1;
				goto too_many_siblings;
			}
			tmp = smp_num_siblings;
			while ((tmp & 1) == 0) {
				tmp >>=1 ;
				index_lsb++;
			}
			tmp = smp_num_siblings;
			while ((tmp & 0x80000000 ) == 0) {
				tmp <<=1 ;
				index_msb--;
			}
			if (index_lsb != index_msb )
				index_msb++;
			initial_apic_id = ebx >> 24 & 0xff;
			phys_proc_id[cpu] = initial_apic_id >> index_msb;

			printk(KERN_INFO  "CPU: Physical Processor ID: %d\n",
                               phys_proc_id[cpu]);
		}

	}
too_many_siblings:

	if (disable_P4_HT)
		clear_bit(X86_FEATURE_HT, c->x86_capability);
#endif

	/* Disable the PN if appropriate */
	squash_the_stupid_serial_number(c);
}

static unsigned int intel_size_cache(struct cpuinfo_x86 * c, unsigned int size)
{
	/* Intel PIII Tualatin. This comes in two flavours.
	 * One has 256kb of cache, the other 512. We have no way
	 * to determine which, so we use a boottime override
	 * for the 512kb model, and assume 256 otherwise.
	 */
	if ((c->x86 == 6) && (c->x86_model == 11) && (size == 0))
		size = 256;
	return size;
}

static struct cpu_dev intel_cpu_dev __initdata = {
	c_vendor:	"Intel",
	c_ident: 	{ "GenuineIntel" },
	c_models: {
		{ X86_VENDOR_INTEL,	4,
		  { 
			  [0] "486 DX-25/33", 
			  [1] "486 DX-50", 
			  [2] "486 SX", 
			  [3] "486 DX/2", 
			  [4] "486 SL", 
			  [5] "486 SX/2", 
			  [7] "486 DX/2-WB", 
			  [8] "486 DX/4", 
			  [9] "486 DX/4-WB"
		  }
		},
		{ X86_VENDOR_INTEL,	5,
		  { 
			  [0] "Pentium 60/66 A-step", 
			  [1] "Pentium 60/66", 
			  [2] "Pentium 75 - 200",
			  [3] "OverDrive PODP5V83", 
			  [4] "Pentium MMX",
			  [7] "Mobile Pentium 75 - 200", 
			  [8] "Mobile Pentium MMX"
		  }
		},
	},
	c_init:		init_intel,
	c_identify:	generic_identify,
	c_size_cache:	intel_size_cache,
};

__init int intel_cpu_init(void)
{
	cpu_devs[X86_VENDOR_INTEL] = &intel_cpu_dev;
	return 0;
}

// arch_initcall(intel_cpu_init);

