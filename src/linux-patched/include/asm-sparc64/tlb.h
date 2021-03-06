#ifndef _SPARC64_TLB_H
#define _SPARC64_TLB_H

#define tlb_flush(tlb)			\
do {	if ((tlb)->fullmm)		\
		flush_tlb_mm((tlb)->mm);\
} while (0)

#define tlb_start_vma(tlb, vma) \
do {	if (!(tlb)->fullmm)	\
		flush_cache_range(vma, vma->vm_start, vma->vm_end); \
} while (0)

#define tlb_end_vma(tlb, vma)	\
do {	if (!(tlb)->fullmm)	\
		flush_tlb_range(vma, vma->vm_start, vma->vm_end); \
} while (0)

#define tlb_remove_tlb_entry(tlb, ptep, address) \
	do { } while (0)

#include <asm-generic/tlb.h>

#define pmd_free_tlb(tlb, pmd)	pmd_free(pmd)
#define pte_free_tlb(tlb, pte)	pte_free(pte)

#endif /* _SPARC64_TLB_H */
