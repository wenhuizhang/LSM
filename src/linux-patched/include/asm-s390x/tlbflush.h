#ifndef _S390X_TLBFLUSH_H
#define _S390X_TLBFLUSH_H

#include <linux/config.h>
#include <linux/mm.h>
#include <asm/processor.h>

/*
 * TLB flushing:
 *
 *  - flush_tlb() flushes the current mm struct TLBs
 *  - flush_tlb_all() flushes all processes TLBs 
 *  - flush_tlb_mm(mm) flushes the specified mm context TLB's
 *  - flush_tlb_page(vma, vmaddr) flushes one page
 *  - flush_tlb_range(vma, start, end) flushes a range of pages
 *  - flush_tlb_kernel_range(start, end) flushes a range of kernel pages
 *  - flush_tlb_pgtables(mm, start, end) flushes a range of page tables
 */

/*
 * S/390 has three ways of flushing TLBs
 * 'ptlb' does a flush of the local processor
 * 'csp' flushes the TLBs on all PUs of a SMP
 * 'ipte' invalidates a pte in a page table and flushes that out of
 * the TLBs of all PUs of a SMP
 */

#define local_flush_tlb() \
do {  __asm__ __volatile__("ptlb": : :"memory"); } while (0)

#ifndef CONFIG_SMP

/*
 * We always need to flush, since s390 does not flush tlb
 * on each context switch
 */

static inline void flush_tlb(void)
{
	local_flush_tlb();
}
static inline void flush_tlb_all(void)
{
	local_flush_tlb();
}
static inline void flush_tlb_mm(struct mm_struct *mm) 
{
	local_flush_tlb();
}
static inline void flush_tlb_page(struct vm_area_struct *vma,
				  unsigned long addr)
{
	local_flush_tlb();
}
static inline void flush_tlb_range(struct vm_area_struct *vma,
				   unsigned long start, unsigned long end)
{
	local_flush_tlb();
}

#define flush_tlb_kernel_range(start, end) \
	local_flush_tlb();

#else

#include <asm/smp.h>

extern void smp_ptlb_all(void);

static inline void global_flush_tlb(void)
{
	long dummy = 0;

	__asm__ __volatile__ (
                "    la   4,1(%0)\n"
                "    slr  2,2\n"
                "    slr  3,3\n"
                "    csp  2,4"
                : : "a" (&dummy) : "cc", "2", "3", "4" );
}

/*
 * We only have to do global flush of tlb if process run since last
 * flush on any other pu than current. 
 * If we have threads (mm->count > 1) we always do a global flush, 
 * since the process runs on more than one processor at the same time.
 */

static inline void __flush_tlb_mm(struct mm_struct * mm)
{
	if ((smp_num_cpus > 1) &&
	    ((atomic_read(&mm->mm_count) != 1) ||
	     (mm->cpu_vm_mask != (1UL << smp_processor_id())))) {
		mm->cpu_vm_mask = (1UL << smp_processor_id());
		global_flush_tlb();
	} else {                 
		local_flush_tlb();
	}
}

static inline void flush_tlb(void)
{
	__flush_tlb_mm(current->mm);
}
static inline void flush_tlb_all(void)
{
	global_flush_tlb();
}
static inline void flush_tlb_mm(struct mm_struct *mm) 
{
	__flush_tlb_mm(mm); 
}
static inline void flush_tlb_page(struct vm_area_struct *vma,
				  unsigned long addr)
{
	__flush_tlb_mm(vma->vm_mm);
}
static inline void flush_tlb_range(struct vm_area_struct *vma,
				   unsigned long start, unsigned long end)
{
	__flush_tlb_mm(vma->vm_mm); 
}

#define flush_tlb_kernel_range(start, end) \
	__flush_tlb_mm(&init_mm)

#endif

static inline void flush_tlb_pgtables(struct mm_struct *mm,
                                      unsigned long start, unsigned long end)
{
        /* S/390 does not keep any page table caches in TLB */
}

#endif /* _S390X_TLBFLUSH_H */
