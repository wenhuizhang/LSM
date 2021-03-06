/*
 * PowerPC64 Segment Translation Support.
 *
 * Dave Engebretsen and Mike Corrigan {engebret|mikejc}@us.ibm.com
 *    Copyright (c) 2001 Dave Engebretsen
 *
 * Copyright (C) 2002 Anton Blanchard <anton@au.ibm.com>, IBM
 * 
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

/* XXX Note: Changes for bolted region have not been merged - Anton */

#include <linux/config.h>
#include <asm/pgtable.h>
#include <asm/mmu.h>
#include <asm/mmu_context.h>
#include <asm/paca.h>
#include <asm/naca.h>
#include <asm/pmc.h>

int make_ste(unsigned long stab, unsigned long esid, unsigned long vsid);
void make_slbe(unsigned long esid, unsigned long vsid, int large);

/*
 * Build an entry for the base kernel segment and put it into
 * the segment table or SLB.  All other segment table or SLB
 * entries are faulted in.
 */
void stab_initialize(unsigned long stab)
{
	unsigned long esid, vsid; 

	esid = GET_ESID(KERNELBASE);
	vsid = get_kernel_vsid(esid << SID_SHIFT); 

	if (cpu_has_slb()) {
		/* Invalidate the entire SLB & all the ERATS */
#ifdef CONFIG_PPC_ISERIES
		asm volatile("isync; slbia; isync":::"memory");
#else
		asm volatile("isync":::"memory");
		asm volatile("slbmte  %0,%0"::"r" (0) : "memory");
		asm volatile("isync; slbia; isync":::"memory");
		make_slbe(esid, vsid, 0);
#endif
	} else {
		asm volatile("isync; slbia; isync":::"memory");
		make_ste(stab, esid, vsid);

		/* Order update */
		asm volatile("sync":::"memory"); 
	}
}

/*
 * Create a segment table entry for the given esid/vsid pair.
 */
int make_ste(unsigned long stab, unsigned long esid, unsigned long vsid)
{
	unsigned long entry, group, old_esid, castout_entry, i;
	unsigned int global_entry;
	STE *ste, *castout_ste;

	/* Search the primary group first. */
	global_entry = (esid & 0x1f) << 3;
	ste = (STE *)(stab | ((esid & 0x1f) << 7)); 

	/* Find an empty entry, if one exists. */
	for (group = 0; group < 2; group++) {
		for (entry = 0; entry < 8; entry++, ste++) {
			if (!(ste->dw0.dw0.v)) {
				ste->dw1.dw1.vsid = vsid;
				ste->dw0.dw0.esid = esid;
				ste->dw0.dw0.kp = 1;
				asm volatile("eieio":::"memory");
				ste->dw0.dw0.v = 1;
				return(global_entry | entry);
			}
		}
		/* Now search the secondary group. */
		global_entry = ((~esid) & 0x1f) << 3;
		ste = (STE *)(stab | (((~esid) & 0x1f) << 7)); 
	}

	/*
	 * Could not find empty entry, pick one with a round robin selection.
	 * Search all entries in the two groups.  Note that the first time
	 * we get here, we start with entry 1 so the initializer
	 * can be common with the SLB castout code.
	 */

	/* This assumes we never castout when initializing the stab. */
	PMC_SW_PROCESSOR(stab_capacity_castouts); 

	castout_entry = get_paca()->xStab_data.next_round_robin;
	for (i = 0; i < 16; i++) {
		if (castout_entry < 8) {
			global_entry = (esid & 0x1f) << 3;
			ste = (STE *)(stab | ((esid & 0x1f) << 7)); 
			castout_ste = ste + castout_entry;
		} else {
			global_entry = ((~esid) & 0x1f) << 3;
			ste = (STE *)(stab | (((~esid) & 0x1f) << 7)); 
			castout_ste = ste + (castout_entry - 8);
		}

		/* Dont cast out the first kernel segment */
		if (castout_ste->dw0.dw0.esid != GET_ESID(KERNELBASE))
			break;

		castout_entry = (castout_entry + 1) & 0xf;
	}

	get_paca()->xStab_data.next_round_robin = (castout_entry + 1) & 0xf;

	/* Modify the old entry to the new value. */

	/* Force previous translations to complete. DRENG */
	asm volatile("isync" : : : "memory" );

	castout_ste->dw0.dw0.v = 0;
	asm volatile("sync" : : : "memory" );    /* Order update */
	castout_ste->dw1.dw1.vsid = vsid;
	old_esid = castout_ste->dw0.dw0.esid;
	castout_ste->dw0.dw0.esid = esid;
	castout_ste->dw0.dw0.kp = 1;
	asm volatile("eieio" : : : "memory" );   /* Order update */
	castout_ste->dw0.dw0.v  = 1;
	asm volatile("slbie  %0" : : "r" (old_esid << SID_SHIFT)); 
	/* Ensure completion of slbie */
	asm volatile("sync" : : : "memory" );

	return (global_entry | (castout_entry & 0x7));
}

/*
 * Create a segment buffer entry for the given esid/vsid pair.
 */
void make_slbe(unsigned long esid, unsigned long vsid, int large)
{
	int kernel_segment = 0;
	unsigned long entry, castout_entry;
	union {
		unsigned long word0;
		slb_dword0    data;
	} esid_data;
	union {
		unsigned long word0;
		slb_dword1    data;
	} vsid_data;

	if (REGION_ID(esid << SID_SHIFT) >= KERNEL_REGION_ID)
		kernel_segment = 1;

	/*
	 * Find an empty entry, if one exists.
	 */
	for (entry = 0; entry < naca->slb_size; entry++) {
		asm volatile("slbmfee  %0,%1" 
			     : "=r" (esid_data) : "r" (entry)); 
		if (!esid_data.data.v) {
			/* 
			 * Write the new SLB entry.
			 */
			vsid_data.word0 = 0;
			vsid_data.data.vsid = vsid;
			vsid_data.data.kp = 1;
			if (large)
				vsid_data.data.l = 1;
			if (kernel_segment)
				vsid_data.data.c = 1;

			esid_data.word0 = 0;
			esid_data.data.esid = esid;
			esid_data.data.v = 1;
			esid_data.data.index = entry;

			/* slbie not needed as no previous mapping existed. */
			/* Order update  */
			asm volatile("isync" : : : "memory");
			asm volatile("slbmte  %0,%1" 
				     : : "r" (vsid_data), 
				     "r" (esid_data)); 
			/* Order update  */
			asm volatile("isync" : : : "memory");
			return;
		}
	}

	/*
	 * Could not find empty entry, pick one with a round robin selection.
	 */

	PMC_SW_PROCESSOR(stab_capacity_castouts); 

	castout_entry = get_paca()->xStab_data.next_round_robin;
	entry = castout_entry; 
	castout_entry++; 
	if (castout_entry >= naca->slb_size)
		castout_entry = 1; 
	get_paca()->xStab_data.next_round_robin = castout_entry;

	/* slbie not needed as the previous mapping is still valid. */
	
	/* 
	 * Write the new SLB entry.
	 */
	vsid_data.word0 = 0;
	vsid_data.data.vsid = vsid;
	vsid_data.data.kp = 1;
	if (large)
		vsid_data.data.l = 1;
	if (kernel_segment)
		vsid_data.data.c = 1;

	esid_data.word0 = 0;
	esid_data.data.esid = esid;
	esid_data.data.v = 1;
	esid_data.data.index = entry;

	asm volatile("isync" : : : "memory");   /* Order update */
	asm volatile("slbmte  %0,%1" 
		     : : "r" (vsid_data), "r" (esid_data)); 
	asm volatile("isync" : : : "memory" );   /* Order update */
}

static inline void __ste_allocate(unsigned long esid, unsigned long vsid,
				  int kernel_segment)
{
	if (cpu_has_slb()) {
#ifndef CONFIG_PPC_ISERIES
		if (REGION_ID(esid << SID_SHIFT) == KERNEL_REGION_ID)
			make_slbe(esid, vsid, 1); 
		else
#endif
			make_slbe(esid, vsid, 0); 
	} else {
		unsigned char top_entry, stab_entry, *segments; 

		stab_entry = make_ste(get_paca()->xStab_data.virt, esid, vsid);
		PMC_SW_PROCESSOR_A(stab_entry_use, stab_entry & 0xf); 

		segments = get_paca()->xSegments;		
		top_entry = get_paca()->stab_cache_pointer;
		if (!kernel_segment && top_entry < STAB_CACHE_SIZE) {
			segments[top_entry] = stab_entry;
			if (top_entry == STAB_CACHE_SIZE)
				top_entry = 0xff;
			top_entry++;
			get_paca()->stab_cache_pointer = top_entry;
		}
	}
}

/*
 * Allocate a segment table entry for the given ea.
 */
int ste_allocate(unsigned long ea)
{
	unsigned long vsid, esid;
	int kernel_segment = 0;

	PMC_SW_PROCESSOR(stab_faults); 

	/* Check for invalid effective addresses. */
	if (!IS_VALID_EA(ea))
		return 1;

	/* Kernel or user address? */
	if (REGION_ID(ea) >= KERNEL_REGION_ID) {
		kernel_segment = 1;
		vsid = get_kernel_vsid(ea);
	} else {
		struct mm_struct *mm = current->mm;
		if (mm)
			vsid = get_vsid(mm->context, ea);
		else
			return 1;
	}

	esid = GET_ESID(ea);
	__ste_allocate(esid, vsid, kernel_segment);
	if (!cpu_has_slb()) {
		/* Order update */
		asm volatile("sync":::"memory"); 
	}

	return 0;
}

unsigned long ppc64_preload_all_segments;
unsigned long ppc64_stab_preload = 1;
#define STAB_PRESSURE 0
#define USE_SLBIE_ON_STAB 0

/*
 * preload all 16 segments for a 32 bit process and the PC and SP segments
 * for a 64 bit process.
 */
static void preload_stab(struct task_struct *tsk, struct mm_struct *mm)
{
	if (ppc64_preload_all_segments && test_tsk_thread_flag(tsk, TIF_32BIT)) {
		unsigned long esid, vsid;

		for (esid = 0; esid < 16; esid++) {
			vsid = get_vsid(mm->context, esid << SID_SHIFT);
			__ste_allocate(esid, vsid, 0);
		}
	} else {
		unsigned long pc = KSTK_EIP(tsk);
		unsigned long stack = KSTK_ESP(tsk);
		unsigned long pc_segment = pc & ~SID_MASK;
		unsigned long stack_segment = stack & ~SID_MASK;
		unsigned long vsid;

		if (pc) {
			if (REGION_ID(pc) >= KERNEL_REGION_ID)
				BUG();
			vsid = get_vsid(mm->context, pc);
			__ste_allocate(GET_ESID(pc), vsid, 0);
		}

		if (stack && (pc_segment != stack_segment)) {
			if (REGION_ID(stack) >= KERNEL_REGION_ID)
				BUG();
			vsid = get_vsid(mm->context, stack);
			__ste_allocate(GET_ESID(stack), vsid, 0);
		}
	}

	if (!cpu_has_slb()) {
		/* Order update */
		asm volatile("sync" : : : "memory"); 
	}
}

/* Flush all user entries from the segment table of the current processor. */
void flush_stab(struct task_struct *tsk, struct mm_struct *mm)
{
	if (cpu_has_slb()) {
		if (!STAB_PRESSURE && test_thread_flag(TIF_32BIT)) {
			union {
				unsigned long word0;
				slb_dword0 data;
			} esid_data;
			unsigned long esid;

			asm volatile("isync" : : : "memory");
			for (esid = 0; esid < 16; esid++) {
				esid_data.word0 = 0;
				esid_data.data.esid = esid;
				asm volatile("slbie %0" : : "r" (esid_data));
			}
			asm volatile("isync" : : : "memory");
		} else {
			asm volatile("isync; slbia; isync":::"memory");
		}

		PMC_SW_PROCESSOR(stab_invalidations);
	} else {
		STE *stab = (STE *) get_paca()->xStab_data.virt;
		STE *ste;
		unsigned long flags;

		/* Force previous translations to complete. DRENG */
		asm volatile("isync" : : : "memory");

		local_irq_save(flags);
		if (get_paca()->stab_cache_pointer != 0xff && !STAB_PRESSURE) {
			int i;
			unsigned char *segments = get_paca()->xSegments;

			for (i = 0; i < get_paca()->stab_cache_pointer; i++) {
				ste = stab + segments[i]; 
				ste->dw0.dw0.v = 0;
				PMC_SW_PROCESSOR(stab_invalidations); 
			}

#if USE_SLBIE_ON_STAB
			asm volatile("sync":::"memory");
			for (i = 0; i < get_paca()->stab_cache_pointer; i++) {
				ste = stab + segments[i]; 
				asm volatile("slbie  %0" : :
					"r" (ste->dw0.dw0.esid << SID_SHIFT)); 
			}
			asm volatile("sync":::"memory");
#else
			asm volatile("sync; slbia; sync":::"memory");
#endif

		} else {
			unsigned long entry;

			/* Invalidate all entries. */
			ste = stab;

			/* Never flush the first entry. */ 
			ste += 1;
			for (entry = 1;
			     entry < (PAGE_SIZE / sizeof(STE)); 
			     entry++, ste++) {
				unsigned long ea;
				ea = ste->dw0.dw0.esid << SID_SHIFT;
				if (STAB_PRESSURE || ea < KERNELBASE) {
					ste->dw0.dw0.v = 0;
					PMC_SW_PROCESSOR(stab_invalidations); 
				}
			}

			asm volatile("sync; slbia; sync":::"memory");
		}

		get_paca()->stab_cache_pointer = 0;
		local_irq_restore(flags);
	}

	if (ppc64_stab_preload)
		preload_stab(tsk, mm);
}
