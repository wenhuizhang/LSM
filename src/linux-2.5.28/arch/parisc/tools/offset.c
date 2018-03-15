/* $Id: offset.c,v 1.2 2000/01/31 13:42:59 jsm Exp $
 *
 * offset.c: Calculate pt_regs and task_struct offsets.
 *
 * Copyright (C) 1996 David S. Miller
 * Made portable by Ralf Baechle
 * Adapted to parisc by Philipp Rumpf, (C) 1999 SuSE GmbH Nuernberg */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/version.h>

#include <asm/ptrace.h>
#include <asm/processor.h>
#include <asm/hardirq.h>

#define text(t) __asm__("\n@@@" t)
#define _offset(type, member) (&(((type *)NULL)->member))

#define offset(string, ptr, member) \
	__asm__("\n@@@" string "%0" : : "i" (_offset(ptr, member)))
#define size(string, size) \
	__asm__("\n@@@" string "%0" : : "i" (sizeof(size)))
#define align(x,y) (((x)+(2*(y))-1)-(((x)+(y)-1)%(y)))
#define size_align(string, size, algn) \
	__asm__("\n@@@" string "%0" : : "i" \
		align(sizeof(size),algn))
#define linefeed text("")

text("/* DO NOT TOUCH, AUTOGENERATED BY OFFSET.C */");
linefeed;
text("#ifndef _PARISC_OFFSET_H");
text("#define _PARISC_OFFSET_H");
linefeed;

void output_task_ptreg_defines(void)
{
	text("/* PA-RISC task pt_regs offsets. */");
	offset("#define TASK_REGS       ", struct task_struct, thread.regs);
	offset("#define TASK_PT_PSW     ", struct task_struct, thread.regs.gr[ 0]);
	offset("#define TASK_PT_GR1     ", struct task_struct, thread.regs.gr[ 1]);
	offset("#define TASK_PT_GR2     ", struct task_struct, thread.regs.gr[ 2]);
	offset("#define TASK_PT_GR3     ", struct task_struct, thread.regs.gr[ 3]);
	offset("#define TASK_PT_GR4     ", struct task_struct, thread.regs.gr[ 4]);
	offset("#define TASK_PT_GR5     ", struct task_struct, thread.regs.gr[ 5]);
	offset("#define TASK_PT_GR6     ", struct task_struct, thread.regs.gr[ 6]);
	offset("#define TASK_PT_GR7     ", struct task_struct, thread.regs.gr[ 7]);
	offset("#define TASK_PT_GR8     ", struct task_struct, thread.regs.gr[ 8]);
	offset("#define TASK_PT_GR9     ", struct task_struct, thread.regs.gr[ 9]);
	offset("#define TASK_PT_GR10    ", struct task_struct, thread.regs.gr[10]);
	offset("#define TASK_PT_GR11    ", struct task_struct, thread.regs.gr[11]);
	offset("#define TASK_PT_GR12    ", struct task_struct, thread.regs.gr[12]);
	offset("#define TASK_PT_GR13    ", struct task_struct, thread.regs.gr[13]);
	offset("#define TASK_PT_GR14    ", struct task_struct, thread.regs.gr[14]);
	offset("#define TASK_PT_GR15    ", struct task_struct, thread.regs.gr[15]);
	offset("#define TASK_PT_GR16    ", struct task_struct, thread.regs.gr[16]);
	offset("#define TASK_PT_GR17    ", struct task_struct, thread.regs.gr[17]);
	offset("#define TASK_PT_GR18    ", struct task_struct, thread.regs.gr[18]);
	offset("#define TASK_PT_GR19    ", struct task_struct, thread.regs.gr[19]);
	offset("#define TASK_PT_GR20    ", struct task_struct, thread.regs.gr[20]);
	offset("#define TASK_PT_GR21    ", struct task_struct, thread.regs.gr[21]);
	offset("#define TASK_PT_GR22    ", struct task_struct, thread.regs.gr[22]);
	offset("#define TASK_PT_GR23    ", struct task_struct, thread.regs.gr[23]);
	offset("#define TASK_PT_GR24    ", struct task_struct, thread.regs.gr[24]);
	offset("#define TASK_PT_GR25    ", struct task_struct, thread.regs.gr[25]);
	offset("#define TASK_PT_GR26    ", struct task_struct, thread.regs.gr[26]);
	offset("#define TASK_PT_GR27    ", struct task_struct, thread.regs.gr[27]);
	offset("#define TASK_PT_GR28    ", struct task_struct, thread.regs.gr[28]);
	offset("#define TASK_PT_GR29    ", struct task_struct, thread.regs.gr[29]);
	offset("#define TASK_PT_GR30    ", struct task_struct, thread.regs.gr[30]);
	offset("#define TASK_PT_GR31    ", struct task_struct, thread.regs.gr[31]);
	offset("#define TASK_PT_FR0     ", struct task_struct, thread.regs.fr[ 0]);
	offset("#define TASK_PT_FR1     ", struct task_struct, thread.regs.fr[ 1]);
	offset("#define TASK_PT_FR2     ", struct task_struct, thread.regs.fr[ 2]);
	offset("#define TASK_PT_FR3     ", struct task_struct, thread.regs.fr[ 3]);
	offset("#define TASK_PT_FR4     ", struct task_struct, thread.regs.fr[ 4]);
	offset("#define TASK_PT_FR5     ", struct task_struct, thread.regs.fr[ 5]);
	offset("#define TASK_PT_FR6     ", struct task_struct, thread.regs.fr[ 6]);
	offset("#define TASK_PT_FR7     ", struct task_struct, thread.regs.fr[ 7]);
	offset("#define TASK_PT_FR8     ", struct task_struct, thread.regs.fr[ 8]);
	offset("#define TASK_PT_FR9     ", struct task_struct, thread.regs.fr[ 9]);
	offset("#define TASK_PT_FR10    ", struct task_struct, thread.regs.fr[10]);
	offset("#define TASK_PT_FR11    ", struct task_struct, thread.regs.fr[11]);
	offset("#define TASK_PT_FR12    ", struct task_struct, thread.regs.fr[12]);
	offset("#define TASK_PT_FR13    ", struct task_struct, thread.regs.fr[13]);
	offset("#define TASK_PT_FR14    ", struct task_struct, thread.regs.fr[14]);
	offset("#define TASK_PT_FR15    ", struct task_struct, thread.regs.fr[15]);
	offset("#define TASK_PT_FR16    ", struct task_struct, thread.regs.fr[16]);
	offset("#define TASK_PT_FR17    ", struct task_struct, thread.regs.fr[17]);
	offset("#define TASK_PT_FR18    ", struct task_struct, thread.regs.fr[18]);
	offset("#define TASK_PT_FR19    ", struct task_struct, thread.regs.fr[19]);
	offset("#define TASK_PT_FR20    ", struct task_struct, thread.regs.fr[20]);
	offset("#define TASK_PT_FR21    ", struct task_struct, thread.regs.fr[21]);
	offset("#define TASK_PT_FR22    ", struct task_struct, thread.regs.fr[22]);
	offset("#define TASK_PT_FR23    ", struct task_struct, thread.regs.fr[23]);
	offset("#define TASK_PT_FR24    ", struct task_struct, thread.regs.fr[24]);
	offset("#define TASK_PT_FR25    ", struct task_struct, thread.regs.fr[25]);
	offset("#define TASK_PT_FR26    ", struct task_struct, thread.regs.fr[26]);
	offset("#define TASK_PT_FR27    ", struct task_struct, thread.regs.fr[27]);
	offset("#define TASK_PT_FR28    ", struct task_struct, thread.regs.fr[28]);
	offset("#define TASK_PT_FR29    ", struct task_struct, thread.regs.fr[29]);
	offset("#define TASK_PT_FR30    ", struct task_struct, thread.regs.fr[30]);
	offset("#define TASK_PT_FR31    ", struct task_struct, thread.regs.fr[31]);
	offset("#define TASK_PT_SR0     ", struct task_struct, thread.regs.sr[ 0]);
	offset("#define TASK_PT_SR1     ", struct task_struct, thread.regs.sr[ 1]);
	offset("#define TASK_PT_SR2     ", struct task_struct, thread.regs.sr[ 2]);
	offset("#define TASK_PT_SR3     ", struct task_struct, thread.regs.sr[ 3]);
	offset("#define TASK_PT_SR4     ", struct task_struct, thread.regs.sr[ 4]);
	offset("#define TASK_PT_SR5     ", struct task_struct, thread.regs.sr[ 5]);
	offset("#define TASK_PT_SR6     ", struct task_struct, thread.regs.sr[ 6]);
	offset("#define TASK_PT_SR7     ", struct task_struct, thread.regs.sr[ 7]);
	offset("#define TASK_PT_IASQ0   ", struct task_struct, thread.regs.iasq[0]);
	offset("#define TASK_PT_IASQ1   ", struct task_struct, thread.regs.iasq[1]);
	offset("#define TASK_PT_IAOQ0   ", struct task_struct, thread.regs.iaoq[0]);
	offset("#define TASK_PT_IAOQ1   ", struct task_struct, thread.regs.iaoq[1]);
	offset("#define TASK_PT_CR24    ", struct task_struct, thread.regs.cr24);
	offset("#define TASK_PT_CR25    ", struct task_struct, thread.regs.cr25);
	offset("#define TASK_PT_CR26    ", struct task_struct, thread.regs.cr26);
	offset("#define TASK_PT_CR27    ", struct task_struct, thread.regs.cr27);
	offset("#define TASK_PT_CR30    ", struct task_struct, thread.regs.cr30);
	offset("#define TASK_PT_ORIG_R28 ", struct task_struct, thread.regs.orig_r28);
	offset("#define TASK_PT_KSP     ", struct task_struct, thread.regs.ksp);
	offset("#define TASK_PT_KPC     ", struct task_struct, thread.regs.kpc);
	offset("#define TASK_PT_SAR     ", struct task_struct, thread.regs.sar);
	offset("#define TASK_PT_CR11    ", struct task_struct, thread.regs.sar);
	offset("#define TASK_PT_IIR     ", struct task_struct, thread.regs.iir);
	offset("#define TASK_PT_ISR     ", struct task_struct, thread.regs.isr);
	offset("#define TASK_PT_IOR     ", struct task_struct, thread.regs.ior);
	offset("#define TASK_PT_CR_PID0 ", struct task_struct, thread.regs.cr_pid[0]);
	offset("#define TASK_PT_CR_PID1 ", struct task_struct, thread.regs.cr_pid[1]);
	offset("#define TASK_PT_CR_PID2 ", struct task_struct, thread.regs.cr_pid[2]);
	offset("#define TASK_PT_CR_PID3 ", struct task_struct, thread.regs.cr_pid[3]);
	size("#define TASK_SZ      ", struct task_struct);
	size_align("#define TASK_SZ_ALGN  ", struct task_struct, 64);
	linefeed;
}

void output_ptreg_defines(void)
{
	text("/* PA-RISC pt_regs offsets. */");
	offset("#define PT_PSW     ", struct pt_regs, gr[ 0]);
	offset("#define PT_GR1     ", struct pt_regs, gr[ 1]);
	offset("#define PT_GR2     ", struct pt_regs, gr[ 2]);
	offset("#define PT_GR3     ", struct pt_regs, gr[ 3]);
	offset("#define PT_GR4     ", struct pt_regs, gr[ 4]);
	offset("#define PT_GR5     ", struct pt_regs, gr[ 5]);
	offset("#define PT_GR6     ", struct pt_regs, gr[ 6]);
	offset("#define PT_GR7     ", struct pt_regs, gr[ 7]);
	offset("#define PT_GR8     ", struct pt_regs, gr[ 8]);
	offset("#define PT_GR9     ", struct pt_regs, gr[ 9]);
	offset("#define PT_GR10    ", struct pt_regs, gr[10]);
	offset("#define PT_GR11    ", struct pt_regs, gr[11]);
	offset("#define PT_GR12    ", struct pt_regs, gr[12]);
	offset("#define PT_GR13    ", struct pt_regs, gr[13]);
	offset("#define PT_GR14    ", struct pt_regs, gr[14]);
	offset("#define PT_GR15    ", struct pt_regs, gr[15]);
	offset("#define PT_GR16    ", struct pt_regs, gr[16]);
	offset("#define PT_GR17    ", struct pt_regs, gr[17]);
	offset("#define PT_GR18    ", struct pt_regs, gr[18]);
	offset("#define PT_GR19    ", struct pt_regs, gr[19]);
	offset("#define PT_GR20    ", struct pt_regs, gr[20]);
	offset("#define PT_GR21    ", struct pt_regs, gr[21]);
	offset("#define PT_GR22    ", struct pt_regs, gr[22]);
	offset("#define PT_GR23    ", struct pt_regs, gr[23]);
	offset("#define PT_GR24    ", struct pt_regs, gr[24]);
	offset("#define PT_GR25    ", struct pt_regs, gr[25]);
	offset("#define PT_GR26    ", struct pt_regs, gr[26]);
	offset("#define PT_GR27    ", struct pt_regs, gr[27]);
	offset("#define PT_GR28    ", struct pt_regs, gr[28]);
	offset("#define PT_GR29    ", struct pt_regs, gr[29]);
	offset("#define PT_GR30    ", struct pt_regs, gr[30]);
	offset("#define PT_GR31    ", struct pt_regs, gr[31]);
	offset("#define PT_FR0     ", struct pt_regs, fr[ 0]);
	offset("#define PT_FR1     ", struct pt_regs, fr[ 1]);
	offset("#define PT_FR2     ", struct pt_regs, fr[ 2]);
	offset("#define PT_FR3     ", struct pt_regs, fr[ 3]);
	offset("#define PT_FR4     ", struct pt_regs, fr[ 4]);
	offset("#define PT_FR5     ", struct pt_regs, fr[ 5]);
	offset("#define PT_FR6     ", struct pt_regs, fr[ 6]);
	offset("#define PT_FR7     ", struct pt_regs, fr[ 7]);
	offset("#define PT_FR8     ", struct pt_regs, fr[ 8]);
	offset("#define PT_FR9     ", struct pt_regs, fr[ 9]);
	offset("#define PT_FR10    ", struct pt_regs, fr[10]);
	offset("#define PT_FR11    ", struct pt_regs, fr[11]);
	offset("#define PT_FR12    ", struct pt_regs, fr[12]);
	offset("#define PT_FR13    ", struct pt_regs, fr[13]);
	offset("#define PT_FR14    ", struct pt_regs, fr[14]);
	offset("#define PT_FR15    ", struct pt_regs, fr[15]);
	offset("#define PT_FR16    ", struct pt_regs, fr[16]);
	offset("#define PT_FR17    ", struct pt_regs, fr[17]);
	offset("#define PT_FR18    ", struct pt_regs, fr[18]);
	offset("#define PT_FR19    ", struct pt_regs, fr[19]);
	offset("#define PT_FR20    ", struct pt_regs, fr[20]);
	offset("#define PT_FR21    ", struct pt_regs, fr[21]);
	offset("#define PT_FR22    ", struct pt_regs, fr[22]);
	offset("#define PT_FR23    ", struct pt_regs, fr[23]);
	offset("#define PT_FR24    ", struct pt_regs, fr[24]);
	offset("#define PT_FR25    ", struct pt_regs, fr[25]);
	offset("#define PT_FR26    ", struct pt_regs, fr[26]);
	offset("#define PT_FR27    ", struct pt_regs, fr[27]);
	offset("#define PT_FR28    ", struct pt_regs, fr[28]);
	offset("#define PT_FR29    ", struct pt_regs, fr[29]);
	offset("#define PT_FR30    ", struct pt_regs, fr[30]);
	offset("#define PT_FR31    ", struct pt_regs, fr[31]);
	offset("#define PT_SR0     ", struct pt_regs, sr[ 0]);
	offset("#define PT_SR1     ", struct pt_regs, sr[ 1]);
	offset("#define PT_SR2     ", struct pt_regs, sr[ 2]);
	offset("#define PT_SR3     ", struct pt_regs, sr[ 3]);
	offset("#define PT_SR4     ", struct pt_regs, sr[ 4]);
	offset("#define PT_SR5     ", struct pt_regs, sr[ 5]);
	offset("#define PT_SR6     ", struct pt_regs, sr[ 6]);
	offset("#define PT_SR7     ", struct pt_regs, sr[ 7]);
	offset("#define PT_IASQ0   ", struct pt_regs, iasq[0]);
	offset("#define PT_IASQ1   ", struct pt_regs, iasq[1]);
	offset("#define PT_IAOQ0   ", struct pt_regs, iaoq[0]);
	offset("#define PT_IAOQ1   ", struct pt_regs, iaoq[1]);
	offset("#define PT_CR24    ", struct pt_regs, cr24);
	offset("#define PT_CR25    ", struct pt_regs, cr25);
	offset("#define PT_CR26    ", struct pt_regs, cr26);
	offset("#define PT_CR27    ", struct pt_regs, cr27);
	offset("#define PT_CR30    ", struct pt_regs, cr30);
	offset("#define PT_ORIG_R28 ", struct pt_regs, orig_r28);
	offset("#define PT_KSP     ", struct pt_regs, ksp);
	offset("#define PT_KPC     ", struct pt_regs, kpc);
	offset("#define PT_SAR     ", struct pt_regs, sar);
	offset("#define PT_CR11    ", struct pt_regs, sar);
	offset("#define PT_IIR     ", struct pt_regs, iir);
	offset("#define PT_ISR     ", struct pt_regs, isr);
	offset("#define PT_IOR     ", struct pt_regs, ior);
	offset("#define PT_CR_PID0 ", struct pt_regs, cr_pid[0]);
	offset("#define PT_CR_PID1 ", struct pt_regs, cr_pid[1]);
	offset("#define PT_CR_PID2 ", struct pt_regs, cr_pid[2]);
	offset("#define PT_CR_PID3 ", struct pt_regs, cr_pid[3]);
	size("#define PT_SIZE    ", struct pt_regs);
	size_align("#define PT_SZ_ALGN  ", struct pt_regs, 64);
	linefeed;
}

void output_task_defines(void)
{
	text("/* PARISC task_struct offsets. */");
	offset("#define TASK_STATE         ", struct task_struct, state);
	offset("#define TASK_FLAGS         ", struct task_struct, flags);
#error	offset("#define TASK_SIGPENDING    ", struct task_struct, sigpending);
	offset("#define TASK_SEGMENT       ", struct task_struct, addr_limit);
#error	offset("#define TASK_NEED_RESCHED  ", struct task_struct, need_resched);
	offset("#define TASK_COUNTER       ", struct task_struct, counter);
#error	offset("#define TASK_PTRACE        ", struct task_struct, ptrace);
	offset("#define TASK_NICE          ", struct task_struct, nice);
	offset("#define TASK_MM            ", struct task_struct, mm);
	offset("#define TASK_PROCESSOR     ", struct task_struct, processor);
	size  ("#define TASK_SZ          ", struct task_struct);
	size_align("#define TASK_SZ_ALGN       ", struct task_struct, 64);
	linefeed;
}

void output_irq_stat_defines(void)
{
	text("/* PARISC irq_cpustat_t offsets. */");
	offset("#define IRQSTAT_SI_ACTIVE  ", irq_cpustat_t, __softirq_active);
	offset("#define IRQSTAT_SI_MASK    ", irq_cpustat_t, __softirq_mask);
	size  ("#define IRQSTAT_SZ         ", irq_cpustat_t);
	linefeed;
}

#ifdef PRUMPF_HAD_MORE_TIME
void output_thread_defines(void)
{
	text("/* PARISC specific thread_struct offsets. */");
	offset("#define THREAD_REG16   ", struct task_struct, thread.reg16);
	offset("#define THREAD_REG17   ", struct task_struct, thread.reg17);
	offset("#define THREAD_REG18   ", struct task_struct, thread.reg18);
	offset("#define THREAD_REG19   ", struct task_struct, thread.reg19);
	offset("#define THREAD_REG20   ", struct task_struct, thread.reg20);
	offset("#define THREAD_REG21   ", struct task_struct, thread.reg21);
	offset("#define THREAD_REG22   ", struct task_struct, thread.reg22);
	offset("#define THREAD_REG23   ", struct task_struct, thread.reg23);
	offset("#define THREAD_REG29   ", struct task_struct, thread.reg29);
	offset("#define THREAD_REG30   ", struct task_struct, thread.reg30);
	offset("#define THREAD_REG31   ", struct task_struct, thread.reg31);
	offset("#define THREAD_STATUS  ", struct task_struct, thread.cp0_status);
	offset("#define THREAD_FPU     ", struct task_struct, thread.fpu);
	offset("#define THREAD_BVADDR  ", struct task_struct, thread.cp0_badvaddr);
	offset("#define THREAD_BUADDR  ", struct task_struct, thread.cp0_baduaddr);
	offset("#define THREAD_ECODE   ", struct task_struct, thread.error_code);
	offset("#define THREAD_TRAPNO  ", struct task_struct, thread.trap_no);
	offset("#define THREAD_PGDIR   ", struct task_struct, thread.pg_dir);
	offset("#define THREAD_MFLAGS  ", struct task_struct, thread.mflags);
	offset("#define THREAD_CURDS   ", struct task_struct, thread.current_ds);
	offset("#define THREAD_TRAMP   ", struct task_struct, thread.irix_trampoline);
	offset("#define THREAD_OLDCTX  ", struct task_struct, thread.irix_oldctx);
	linefeed;
}

void output_mm_defines(void)
{
	text("/* Linux mm_struct offsets. */");
	offset("#define MM_COUNT      ", struct mm_struct, count);
	offset("#define MM_PGD        ", struct mm_struct, pgd);
	offset("#define MM_CONTEXT    ", struct mm_struct, context);
	linefeed;
}

void output_sc_defines(void)
{
	text("/* Linux sigcontext offsets. */");
	offset("#define SC_REGMASK    ", struct sigcontext, sc_regmask);
	offset("#define SC_STATUS     ", struct sigcontext, sc_status);
	offset("#define SC_PC         ", struct sigcontext, sc_pc);
	offset("#define SC_REGS       ", struct sigcontext, sc_regs);
	offset("#define SC_FPREGS     ", struct sigcontext, sc_fpregs);
	offset("#define SC_OWNEDFP    ", struct sigcontext, sc_ownedfp);
	offset("#define SC_FPC_CSR    ", struct sigcontext, sc_fpc_csr);
	offset("#define SC_FPC_EIR    ", struct sigcontext, sc_fpc_eir);
	offset("#define SC_SSFLAGS    ", struct sigcontext, sc_ssflags);
	offset("#define SC_MDHI       ", struct sigcontext, sc_mdhi);
	offset("#define SC_MDLO       ", struct sigcontext, sc_mdlo);
	offset("#define SC_CAUSE      ", struct sigcontext, sc_cause);
	offset("#define SC_BADVADDR   ", struct sigcontext, sc_badvaddr);
	offset("#define SC_SIGSET     ", struct sigcontext, sc_sigset);
	linefeed;
}

#endif

text("#endif /* !(_PARISC_OFFSET_H) */");
