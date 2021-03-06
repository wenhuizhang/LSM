/*
 *  linux/mm/swapfile.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *  Swap reorganised 29.12.95, Stephen Tweedie
 */

#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/swapctl.h>
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include <linux/namei.h>
#include <linux/shm.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>

#include <asm/pgtable.h>
#include <linux/swapops.h>

spinlock_t swaplock = SPIN_LOCK_UNLOCKED;
unsigned int nr_swapfiles;
int total_swap_pages;
static int swap_overflow;

static const char Bad_file[] = "Bad swap file entry ";
static const char Unused_file[] = "Unused swap file entry ";
static const char Bad_offset[] = "Bad swap offset entry ";
static const char Unused_offset[] = "Unused swap offset entry ";

struct swap_list_t swap_list = {-1, -1};

struct swap_info_struct swap_info[MAX_SWAPFILES];

#define SWAPFILE_CLUSTER 256

static inline int scan_swap_map(struct swap_info_struct *si)
{
	unsigned long offset;
	/* 
	 * We try to cluster swap pages by allocating them
	 * sequentially in swap.  Once we've allocated
	 * SWAPFILE_CLUSTER pages this way, however, we resort to
	 * first-free allocation, starting a new cluster.  This
	 * prevents us from scattering swap pages all over the entire
	 * swap partition, so that we reduce overall disk seek times
	 * between swap pages.  -- sct */
	if (si->cluster_nr) {
		while (si->cluster_next <= si->highest_bit) {
			offset = si->cluster_next++;
			if (si->swap_map[offset])
				continue;
			si->cluster_nr--;
			goto got_page;
		}
	}
	si->cluster_nr = SWAPFILE_CLUSTER;

	/* try to find an empty (even not aligned) cluster. */
	offset = si->lowest_bit;
 check_next_cluster:
	if (offset+SWAPFILE_CLUSTER-1 <= si->highest_bit)
	{
		int nr;
		for (nr = offset; nr < offset+SWAPFILE_CLUSTER; nr++)
			if (si->swap_map[nr])
			{
				offset = nr+1;
				goto check_next_cluster;
			}
		/* We found a completly empty cluster, so start
		 * using it.
		 */
		goto got_page;
	}
	/* No luck, so now go finegrined as usual. -Andrea */
	for (offset = si->lowest_bit; offset <= si->highest_bit ; offset++) {
		if (si->swap_map[offset])
			continue;
		si->lowest_bit = offset+1;
	got_page:
		if (offset == si->lowest_bit)
			si->lowest_bit++;
		if (offset == si->highest_bit)
			si->highest_bit--;
		if (si->lowest_bit > si->highest_bit) {
			si->lowest_bit = si->max;
			si->highest_bit = 0;
		}
		si->swap_map[offset] = 1;
		nr_swap_pages--;
		si->cluster_next = offset+1;
		return offset;
	}
	si->lowest_bit = si->max;
	si->highest_bit = 0;
	return 0;
}

swp_entry_t get_swap_page(void)
{
	struct swap_info_struct * p;
	unsigned long offset;
	swp_entry_t entry;
	int type, wrapped = 0;

	entry.val = 0;	/* Out of memory */
	swap_list_lock();
	type = swap_list.next;
	if (type < 0)
		goto out;
	if (nr_swap_pages <= 0)
		goto out;

	while (1) {
		p = &swap_info[type];
		if ((p->flags & SWP_ACTIVE) == SWP_ACTIVE) {
			swap_device_lock(p);
			offset = scan_swap_map(p);
			swap_device_unlock(p);
			if (offset) {
				entry = swp_entry(type,offset);
				type = swap_info[type].next;
				if (type < 0 ||
					p->prio != swap_info[type].prio) {
						swap_list.next = swap_list.head;
				} else {
					swap_list.next = type;
				}
				goto out;
			}
		}
		type = p->next;
		if (!wrapped) {
			if (type < 0 || p->prio != swap_info[type].prio) {
				type = swap_list.head;
				wrapped = 1;
			}
		} else
			if (type < 0)
				goto out;	/* out of swap space */
	}
out:
	swap_list_unlock();
	return entry;
}

static struct swap_info_struct * swap_info_get(swp_entry_t entry)
{
	struct swap_info_struct * p;
	unsigned long offset, type;

	if (!entry.val)
		goto out;
	type = swp_type(entry);
	if (type >= nr_swapfiles)
		goto bad_nofile;
	p = & swap_info[type];
	if (!(p->flags & SWP_USED))
		goto bad_device;
	offset = swp_offset(entry);
	if (offset >= p->max)
		goto bad_offset;
	if (!p->swap_map[offset])
		goto bad_free;
	swap_list_lock();
	if (p->prio > swap_info[swap_list.next].prio)
		swap_list.next = type;
	swap_device_lock(p);
	return p;

bad_free:
	printk(KERN_ERR "swap_free: %s%08lx\n", Unused_offset, entry.val);
	goto out;
bad_offset:
	printk(KERN_ERR "swap_free: %s%08lx\n", Bad_offset, entry.val);
	goto out;
bad_device:
	printk(KERN_ERR "swap_free: %s%08lx\n", Unused_file, entry.val);
	goto out;
bad_nofile:
	printk(KERN_ERR "swap_free: %s%08lx\n", Bad_file, entry.val);
out:
	return NULL;
}	

static void swap_info_put(struct swap_info_struct * p)
{
	swap_device_unlock(p);
	swap_list_unlock();
}

static int swap_entry_free(struct swap_info_struct *p, unsigned long offset)
{
	int count = p->swap_map[offset];

	if (count < SWAP_MAP_MAX) {
		count--;
		p->swap_map[offset] = count;
		if (!count) {
			if (offset < p->lowest_bit)
				p->lowest_bit = offset;
			if (offset > p->highest_bit)
				p->highest_bit = offset;
			nr_swap_pages++;
		}
	}
	return count;
}

/*
 * Caller has made sure that the swapdevice corresponding to entry
 * is still around or has not been recycled.
 */
void swap_free(swp_entry_t entry)
{
	struct swap_info_struct * p;

	p = swap_info_get(entry);
	if (p) {
		swap_entry_free(p, swp_offset(entry));
		swap_info_put(p);
	}
}

/*
 * Check if we're the only user of a swap page,
 * when the page is locked.
 */
static int exclusive_swap_page(struct page *page)
{
	int retval = 0;
	struct swap_info_struct * p;
	swp_entry_t entry;

	entry.val = page->index;
	p = swap_info_get(entry);
	if (p) {
		/* Is the only swap cache user the cache itself? */
		if (p->swap_map[swp_offset(entry)] == 1) {
			/* Recheck the page count with the pagecache lock held.. */
			read_lock(&swapper_space.page_lock);
			if (page_count(page) - !!PagePrivate(page) == 2)
				retval = 1;
			read_unlock(&swapper_space.page_lock);
		}
		swap_info_put(p);
	}
	return retval;
}

/*
 * We can use this swap cache entry directly
 * if there are no other references to it.
 *
 * Here "exclusive_swap_page()" does the real
 * work, but we opportunistically check whether
 * we need to get all the locks first..
 */
int can_share_swap_page(struct page *page)
{
	int retval = 0;

	if (!PageLocked(page))
		BUG();
	switch (page_count(page)) {
	case 3:
		if (!PagePrivate(page))
			break;
		/* Fallthrough */
	case 2:
		if (!PageSwapCache(page))
			break;
		retval = exclusive_swap_page(page);
		break;
	case 1:
		if (PageReserved(page))
			break;
		retval = 1;
	}
	return retval;
}

/*
 * Work out if there are any other processes sharing this
 * swap cache page. Free it if you can. Return success.
 */
int remove_exclusive_swap_page(struct page *page)
{
	int retval;
	struct swap_info_struct * p;
	swp_entry_t entry;

	BUG_ON(page_has_buffers(page));
	BUG_ON(!PageLocked(page));

	if (!PageSwapCache(page))
		return 0;
	if (PageWriteback(page))
		return 0;
	if (page_count(page) != 2) /* 2: us + cache */
		return 0;

	entry.val = page->index;
	p = swap_info_get(entry);
	if (!p)
		return 0;

	/* Is the only swap cache user the cache itself? */
	retval = 0;
	if (p->swap_map[swp_offset(entry)] == 1) {
		/* Recheck the page count with the pagecache lock held.. */
		write_lock(&swapper_space.page_lock);
		if ((page_count(page) == 2) && !PageWriteback(page)) {
			__delete_from_swap_cache(page);
			SetPageDirty(page);
			retval = 1;
		}
		write_unlock(&swapper_space.page_lock);
	}
	swap_info_put(p);

	if (retval) {
		swap_free(entry);
		page_cache_release(page);
	}

	return retval;
}

/*
 * Free the swap entry like above, but also try to
 * free the page cache entry if it is the last user.
 */
void free_swap_and_cache(swp_entry_t entry)
{
	struct swap_info_struct * p;
	struct page *page = NULL;

	p = swap_info_get(entry);
	if (p) {
		if (swap_entry_free(p, swp_offset(entry)) == 1)
			page = find_trylock_page(&swapper_space, entry.val);
		swap_info_put(p);
	}
	if (page) {
		int one_user;

		BUG_ON(page_has_buffers(page));
		page_cache_get(page);
		one_user = (page_count(page) == 2);
		/* Only cache user (+us), or swap space full? Free it! */
		if (!PageWriteback(page) && (one_user || vm_swap_full())) {
			delete_from_swap_cache(page);
			SetPageDirty(page);
		}
		unlock_page(page);
		page_cache_release(page);
	}
}

/*
 * The swap entry has been read in advance, and we return 1 to indicate
 * that the page has been used or is no longer needed.
 *
 * Always set the resulting pte to be nowrite (the same as COW pages
 * after one process has exited).  We don't know just how many PTEs will
 * share this swap entry, so be cautious and let do_wp_page work out
 * what to do if a write is requested later.
 */
/* mmlist_lock and vma->vm_mm->page_table_lock are held */
static inline void unuse_pte(struct vm_area_struct * vma, unsigned long address,
	pte_t *dir, swp_entry_t entry, struct page* page)
{
	pte_t pte = *dir;

	if (likely(pte_to_swp_entry(pte).val != entry.val))
		return;
	if (unlikely(pte_none(pte) || pte_present(pte)))
		return;
	get_page(page);
	set_pte(dir, pte_mkold(mk_pte(page, vma->vm_page_prot)));
	page_add_rmap(page, dir);
	swap_free(entry);
	++vma->vm_mm->rss;
}

/* mmlist_lock and vma->vm_mm->page_table_lock are held */
static inline void unuse_pmd(struct vm_area_struct * vma, pmd_t *dir,
	unsigned long address, unsigned long size, unsigned long offset,
	swp_entry_t entry, struct page* page)
{
	pte_t * pte;
	unsigned long end;

	if (pmd_none(*dir))
		return;
	if (pmd_bad(*dir)) {
		pmd_ERROR(*dir);
		pmd_clear(dir);
		return;
	}
	pte = pte_offset_map(dir, address);
	offset += address & PMD_MASK;
	address &= ~PMD_MASK;
	end = address + size;
	if (end > PMD_SIZE)
		end = PMD_SIZE;
	do {
		unuse_pte(vma, offset+address-vma->vm_start, pte, entry, page);
		address += PAGE_SIZE;
		pte++;
	} while (address && (address < end));
	pte_unmap(pte - 1);
}

/* mmlist_lock and vma->vm_mm->page_table_lock are held */
static inline void unuse_pgd(struct vm_area_struct * vma, pgd_t *dir,
	unsigned long address, unsigned long size,
	swp_entry_t entry, struct page* page)
{
	pmd_t * pmd;
	unsigned long offset, end;

	if (pgd_none(*dir))
		return;
	if (pgd_bad(*dir)) {
		pgd_ERROR(*dir);
		pgd_clear(dir);
		return;
	}
	pmd = pmd_offset(dir, address);
	offset = address & PGDIR_MASK;
	address &= ~PGDIR_MASK;
	end = address + size;
	if (end > PGDIR_SIZE)
		end = PGDIR_SIZE;
	if (address >= end)
		BUG();
	do {
		unuse_pmd(vma, pmd, address, end - address, offset, entry,
			  page);
		address = (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address && (address < end));
}

/* mmlist_lock and vma->vm_mm->page_table_lock are held */
static void unuse_vma(struct vm_area_struct * vma, pgd_t *pgdir,
			swp_entry_t entry, struct page* page)
{
	unsigned long start = vma->vm_start, end = vma->vm_end;

	if (start >= end)
		BUG();
	do {
		unuse_pgd(vma, pgdir, start, end - start, entry, page);
		start = (start + PGDIR_SIZE) & PGDIR_MASK;
		pgdir++;
	} while (start && (start < end));
}

static void unuse_process(struct mm_struct * mm,
			swp_entry_t entry, struct page* page)
{
	struct vm_area_struct* vma;

	/*
	 * Go through process' page directory.
	 */
	spin_lock(&mm->page_table_lock);
	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		pgd_t * pgd = pgd_offset(mm, vma->vm_start);
		unuse_vma(vma, pgd, entry, page);
	}
	spin_unlock(&mm->page_table_lock);
	return;
}

/*
 * Scan swap_map from current position to next entry still in use.
 * Recycle to start on reaching the end, returning 0 when empty.
 */
static int find_next_to_unuse(struct swap_info_struct *si, int prev)
{
	int max = si->max;
	int i = prev;
	int count;

	/*
	 * No need for swap_device_lock(si) here: we're just looking
	 * for whether an entry is in use, not modifying it; false
	 * hits are okay, and sys_swapoff() has already prevented new
	 * allocations from this area (while holding swap_list_lock()).
	 */
	for (;;) {
		if (++i >= max) {
			if (!prev) {
				i = 0;
				break;
			}
			/*
			 * No entries in use at top of swap_map,
			 * loop back to start and recheck there.
			 */
			max = prev + 1;
			prev = 0;
			i = 1;
		}
		count = si->swap_map[i];
		if (count && count != SWAP_MAP_BAD)
			break;
	}
	return i;
}

/*
 * We completely avoid races by reading each swap page in advance,
 * and then search for the process using it.  All the necessary
 * page table adjustments can then be made atomically.
 */
static int try_to_unuse(unsigned int type)
{
	struct swap_info_struct * si = &swap_info[type];
	struct mm_struct *start_mm;
	unsigned short *swap_map;
	unsigned short swcount;
	struct page *page;
	swp_entry_t entry;
	int i = 0;
	int retval = 0;
	int reset_overflow = 0;

	/*
	 * When searching mms for an entry, a good strategy is to
	 * start at the first mm we freed the previous entry from
	 * (though actually we don't notice whether we or coincidence
	 * freed the entry).  Initialize this start_mm with a hold.
	 *
	 * A simpler strategy would be to start at the last mm we
	 * freed the previous entry from; but that would take less
	 * advantage of mmlist ordering (now preserved by swap_out()),
	 * which clusters forked address spaces together, most recent
	 * child immediately after parent.  If we race with dup_mmap(),
	 * we very much want to resolve parent before child, otherwise
	 * we may miss some entries: using last mm would invert that.
	 */
	start_mm = &init_mm;
	atomic_inc(&init_mm.mm_users);

	/*
	 * Keep on scanning until all entries have gone.  Usually,
	 * one pass through swap_map is enough, but not necessarily:
	 * mmput() removes mm from mmlist before exit_mmap() and its
	 * zap_page_range().  That's not too bad, those entries are
	 * on their way out, and handled faster there than here.
	 * do_munmap() behaves similarly, taking the range out of mm's
	 * vma list before zap_page_range().  But unfortunately, when
	 * unmapping a part of a vma, it takes the whole out first,
	 * then reinserts what's left after (might even reschedule if
	 * open() method called) - so swap entries may be invisible
	 * to swapoff for a while, then reappear - but that is rare.
	 */
	while ((i = find_next_to_unuse(si, i))) {
		/* 
		 * Get a page for the entry, using the existing swap
		 * cache page if there is one.  Otherwise, get a clean
		 * page and read the swap into it. 
		 */
		swap_map = &si->swap_map[i];
		entry = swp_entry(type, i);
		page = read_swap_cache_async(entry);
		if (!page) {
			/*
			 * Either swap_duplicate() failed because entry
			 * has been freed independently, and will not be
			 * reused since sys_swapoff() already disabled
			 * allocation from here, or alloc_page() failed.
			 */
			if (!*swap_map)
				continue;
			retval = -ENOMEM;
			break;
		}

		/*
		 * Don't hold on to start_mm if it looks like exiting.
		 */
		if (atomic_read(&start_mm->mm_users) == 1) {
			mmput(start_mm);
			start_mm = &init_mm;
			atomic_inc(&init_mm.mm_users);
		}

		/*
		 * Wait for and lock page.  When do_swap_page races with
		 * try_to_unuse, do_swap_page can handle the fault much
		 * faster than try_to_unuse can locate the entry.  This
		 * apparently redundant "wait_on_page_locked" lets try_to_unuse
		 * defer to do_swap_page in such a case - in some tests,
		 * do_swap_page and try_to_unuse repeatedly compete.
		 */
		wait_on_page_locked(page);
		wait_on_page_writeback(page);
		lock_page(page);
		wait_on_page_writeback(page);

		/*
		 * Remove all references to entry, without blocking.
		 * Whenever we reach init_mm, there's no address space
		 * to search, but use it as a reminder to search shmem.
		 */
		swcount = *swap_map;
		if (swcount > 1) {
			flush_page_to_ram(page);
			if (start_mm == &init_mm)
				shmem_unuse(entry, page);
			else
				unuse_process(start_mm, entry, page);
		}
		if (*swap_map > 1) {
			int set_start_mm = (*swap_map >= swcount);
			struct list_head *p = &start_mm->mmlist;
			struct mm_struct *new_start_mm = start_mm;
			struct mm_struct *mm;

			spin_lock(&mmlist_lock);
			while (*swap_map > 1 &&
					(p = p->next) != &start_mm->mmlist) {
				mm = list_entry(p, struct mm_struct, mmlist);
				swcount = *swap_map;
				if (mm == &init_mm) {
					set_start_mm = 1;
					shmem_unuse(entry, page);
				} else
					unuse_process(mm, entry, page);
				if (set_start_mm && *swap_map < swcount) {
					new_start_mm = mm;
					set_start_mm = 0;
				}
			}
			atomic_inc(&new_start_mm->mm_users);
			spin_unlock(&mmlist_lock);
			mmput(start_mm);
			start_mm = new_start_mm;
		}

		/*
		 * How could swap count reach 0x7fff when the maximum
		 * pid is 0x7fff, and there's no way to repeat a swap
		 * page within an mm (except in shmem, where it's the
		 * shared object which takes the reference count)?
		 * We believe SWAP_MAP_MAX cannot occur in Linux 2.4.
		 *
		 * If that's wrong, then we should worry more about
		 * exit_mmap() and do_munmap() cases described above:
		 * we might be resetting SWAP_MAP_MAX too early here.
		 * We know "Undead"s can happen, they're okay, so don't
		 * report them; but do report if we reset SWAP_MAP_MAX.
		 */
		if (*swap_map == SWAP_MAP_MAX) {
			swap_list_lock();
			swap_device_lock(si);
			nr_swap_pages++;
			*swap_map = 1;
			swap_device_unlock(si);
			swap_list_unlock();
			reset_overflow = 1;
		}

		/*
		 * If a reference remains (rare), we would like to leave
		 * the page in the swap cache; but try_to_swap_out could
		 * then re-duplicate the entry once we drop page lock,
		 * so we might loop indefinitely; also, that page could
		 * not be swapped out to other storage meanwhile.  So:
		 * delete from cache even if there's another reference,
		 * after ensuring that the data has been saved to disk -
		 * since if the reference remains (rarer), it will be
		 * read from disk into another page.  Splitting into two
		 * pages would be incorrect if swap supported "shared
		 * private" pages, but they are handled by tmpfs files.
		 * Note shmem_unuse already deleted its from swap cache.
		 */
		if ((*swap_map > 1) && PageDirty(page) && PageSwapCache(page)) {
			swap_writepage(page);
			lock_page(page);
			wait_on_page_writeback(page);
		}
		if (PageSwapCache(page))
			delete_from_swap_cache(page);

		/*
		 * So we could skip searching mms once swap count went
		 * to 1, we did not mark any present ptes as dirty: must
		 * mark page dirty so try_to_swap_out will preserve it.
		 */
		SetPageDirty(page);
		unlock_page(page);
		page_cache_release(page);

		/*
		 * Make sure that we aren't completely killing
		 * interactive performance.  Interruptible check on
		 * signal_pending() would be nice, but changes the spec?
		 */
		if (need_resched())
			schedule();
	}

	mmput(start_mm);
	if (reset_overflow) {
		printk(KERN_WARNING "swapoff: cleared swap entry overflow\n");
		swap_overflow = 0;
	}
	return retval;
}

/*
 * Use this swapdev's extent info to locate the (PAGE_SIZE) block which
 * corresponds to page offset `offset'.
 */
sector_t map_swap_page(struct swap_info_struct *sis, pgoff_t offset)
{
	struct swap_extent *se = sis->curr_swap_extent;
	struct swap_extent *start_se = se;

	for ( ; ; ) {
		struct list_head *lh;

		if (se->start_page <= offset &&
				offset < (se->start_page + se->nr_pages)) {
			return se->start_block + (offset - se->start_page);
		}
		lh = se->list.prev;
		if (lh == &sis->extent_list)
			lh = lh->prev;
		se = list_entry(lh, struct swap_extent, list);
		sis->curr_swap_extent = se;
		BUG_ON(se == start_se);		/* It *must* be present */
	}
}

/*
 * Free all of a swapdev's extent information
 */
static void destroy_swap_extents(struct swap_info_struct *sis)
{
	while (!list_empty(&sis->extent_list)) {
		struct swap_extent *se;

		se = list_entry(sis->extent_list.next,
				struct swap_extent, list);
		list_del(&se->list);
		kfree(se);
	}
	sis->nr_extents = 0;
}

/*
 * Add a block range (and the corresponding page range) into this swapdev's
 * extent list.  The extent list is kept sorted in block order.
 *
 * This function rather assumes that it is called in ascending sector_t order.
 * It doesn't look for extent coalescing opportunities.
 */
static int
add_swap_extent(struct swap_info_struct *sis, unsigned long start_page,
		unsigned long nr_pages, sector_t start_block)
{
	struct swap_extent *se;
	struct swap_extent *new_se;
	struct list_head *lh;

	lh = sis->extent_list.next;	/* The highest-addressed block */
	while (lh != &sis->extent_list) {
		se = list_entry(lh, struct swap_extent, list);
		if (se->start_block + se->nr_pages == start_block) {
			/* Merge it */
			se->nr_pages += nr_pages;
			return 0;
		}
		lh = lh->next;
	}

	/*
	 * No merge.  Insert a new extent, preserving ordering.
	 */
	new_se = kmalloc(sizeof(*se), GFP_KERNEL);
	if (new_se == NULL)
		return -ENOMEM;
	new_se->start_page = start_page;
	new_se->nr_pages = nr_pages;
	new_se->start_block = start_block;

	lh = sis->extent_list.prev;	/* The lowest block */
	while (lh != &sis->extent_list) {
		se = list_entry(lh, struct swap_extent, list);
		if (se->start_block > start_block)
			break;
		lh = lh->prev;
	}
	list_add_tail(&new_se->list, lh);
	sis->nr_extents++;
	return 0;
}

/*
 * A `swap extent' is a simple thing which maps a contiguous range of pages
 * onto a contiguous range of disk blocks.  An ordered list of swap extents
 * is built at swapon time and is then used at swap_writepage/swap_readpage
 * time for locating where on disk a page belongs.
 *
 * If the swapfile is an S_ISBLK block device, a single extent is installed.
 * This is done so that the main operating code can treat S_ISBLK and S_ISREG
 * swap files identically.
 *
 * Whether the swapdev is an S_ISREG file or an S_ISBLK blockdev, the swap
 * extent list operates in PAGE_SIZE disk blocks.  Both S_ISREG and S_ISBLK
 * swapfiles are handled *identically* after swapon time.
 *
 * For S_ISREG swapfiles, setup_swap_extents() will walk all the file's blocks
 * and will parse them into an ordered extent list, in PAGE_SIZE chunks.  If
 * some stray blocks are found which do not fall within the PAGE_SIZE alignment
 * requirements, they are simply tossed out - we will never use those blocks
 * for swapping.
 *
 * The amount of disk space which a single swap extent represents varies.
 * Typically it is in the 1-4 megabyte range.  So we can have hundreds of
 * extents in the list.  To avoid much list walking, we cache the previous
 * search location in `curr_swap_extent', and start new searches from there.
 * This is extremely effective.  The average number of iterations in
 * map_swap_page() has been measured at about 0.3 per page.  - akpm.
 */
static int setup_swap_extents(struct swap_info_struct *sis)
{
	struct inode *inode;
	unsigned blocks_per_page;
	unsigned long page_no;
	unsigned blkbits;
	sector_t probe_block;
	sector_t last_block;
	int ret;

	inode = sis->swap_file->f_dentry->d_inode;
	if (S_ISBLK(inode->i_mode)) {
		ret = add_swap_extent(sis, 0, sis->max, 0);
		goto done;
	}

	blkbits = inode->i_blkbits;
	blocks_per_page = PAGE_SIZE >> blkbits;

	/*
	 * Map all the blocks into the extent list.  This code doesn't try
	 * to be very smart.
	 */
	probe_block = 0;
	page_no = 0;
	last_block = inode->i_size >> blkbits;
	while ((probe_block + blocks_per_page) <= last_block &&
			page_no < sis->max) {
		unsigned block_in_page;
		sector_t first_block;

		first_block = bmap(inode, probe_block);
		if (first_block == 0)
			goto bad_bmap;

		/*
		 * It must be PAGE_SIZE aligned on-disk
		 */
		if (first_block & (blocks_per_page - 1)) {
			probe_block++;
			goto reprobe;
		}

		for (block_in_page = 1; block_in_page < blocks_per_page;
					block_in_page++) {
			sector_t block;

			block = bmap(inode, probe_block + block_in_page);
			if (block == 0)
				goto bad_bmap;
			if (block != first_block + block_in_page) {
				/* Discontiguity */
				probe_block++;
				goto reprobe;
			}
		}

		/*
		 * We found a PAGE_SIZE-length, PAGE_SIZE-aligned run of blocks
		 */
		ret = add_swap_extent(sis, page_no, 1,
				first_block >> (PAGE_SHIFT - blkbits));
		if (ret)
			goto out;
		page_no++;
		probe_block += blocks_per_page;
reprobe:
		continue;
	}
	ret = 0;
	if (page_no == 0)
		ret = -EINVAL;
	sis->max = page_no;
	sis->highest_bit = page_no - 1;
done:
	sis->curr_swap_extent = list_entry(sis->extent_list.prev,
					struct swap_extent, list);
	goto out;
bad_bmap:
	printk(KERN_ERR "swapon: swapfile has holes\n");
	ret = -EINVAL;
out:
	return ret;
}

asmlinkage long sys_swapoff(const char * specialfile)
{
	struct swap_info_struct * p = NULL;
	unsigned short *swap_map;
	struct file *swap_file;
	struct nameidata nd;
	int i, type, prev;
	int err;
	
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	err = user_path_walk(specialfile, &nd);
	if (err)
		goto out;

	prev = -1;
	swap_list_lock();
	for (type = swap_list.head; type >= 0; type = swap_info[type].next) {
		p = swap_info + type;
		if ((p->flags & SWP_ACTIVE) == SWP_ACTIVE) {
			if (p->swap_file->f_dentry == nd.dentry)
				break;
		}
		prev = type;
	}
	err = -EINVAL;
	if (type < 0) {
		swap_list_unlock();
		goto out_dput;
	}

	if (prev < 0) {
		swap_list.head = p->next;
	} else {
		swap_info[prev].next = p->next;
	}
	if (type == swap_list.next) {
		/* just pick something that's safe... */
		swap_list.next = swap_list.head;
	}
	nr_swap_pages -= p->pages;
	total_swap_pages -= p->pages;
	p->flags &= ~SWP_WRITEOK;
	swap_list_unlock();
	err = try_to_unuse(type);
	if (err) {
		/* re-insert swap space back into swap_list */
		swap_list_lock();
		for (prev = -1, i = swap_list.head; i >= 0; prev = i, i = swap_info[i].next)
			if (p->prio >= swap_info[i].prio)
				break;
		p->next = i;
		if (prev < 0)
			swap_list.head = swap_list.next = p - swap_info;
		else
			swap_info[prev].next = p - swap_info;
		nr_swap_pages += p->pages;
		total_swap_pages += p->pages;
		p->flags |= SWP_WRITEOK;
		swap_list_unlock();
		goto out_dput;
	}
	swap_list_lock();
	swap_device_lock(p);
	swap_file = p->swap_file;
	p->swap_file = NULL;
	p->max = 0;
	swap_map = p->swap_map;
	p->swap_map = NULL;
	p->flags = 0;
	destroy_swap_extents(p);
	swap_device_unlock(p);
	swap_list_unlock();
	vfree(swap_map);
	if (S_ISBLK(swap_file->f_dentry->d_inode->i_mode)) {
		struct block_device *bdev;
		bdev = swap_file->f_dentry->d_inode->i_bdev;
		set_blocksize(bdev, p->old_block_size);
		bd_release(bdev);
	}
	filp_close(swap_file, NULL);
	err = 0;

out_dput:
	path_release(&nd);
out:
	return err;
}

int get_swaparea_info(char *buf)
{
	char * page = (char *) __get_free_page(GFP_KERNEL);
	struct swap_info_struct *ptr = swap_info;
	int i, len;

	if (!page)
		return -ENOMEM;

	len = sprintf(buf, "Filename\t\t\t\tType\t\tSize\tUsed\tPriority\n");
	for (i = 0 ; i < nr_swapfiles ; i++, ptr++) {
		int j, usedswap;
		struct file *file;
		char *path;

		if (!(ptr->flags & SWP_USED) || !ptr->swap_map)
			continue;

		file = ptr->swap_file;
		path = d_path(file->f_dentry, file->f_vfsmnt, page, PAGE_SIZE);
		for (j = 0,usedswap = 0; j < ptr->max; ++j)
			switch (ptr->swap_map[j]) {
				case SWAP_MAP_BAD:
				case 0:
					continue;
				default:
					usedswap++;
			}
		len += sprintf(buf + len, "%-39s %s\t%d\t%d\t%d\n",
			       path,
			       S_ISBLK(file->f_dentry->d_inode->i_mode) ?
					"partition" : "file\t",
			       ptr->pages << (PAGE_SHIFT - 10),
			       usedswap << (PAGE_SHIFT - 10),
			       ptr->prio);
	}
	free_page((unsigned long) page);
	return len;
}

/*
 * Written 01/25/92 by Simmule Turner, heavily changed by Linus.
 *
 * The swapon system call
 */
asmlinkage long sys_swapon(const char * specialfile, int swap_flags)
{
	struct swap_info_struct * p;
	char *name = NULL;
	struct block_device *bdev = NULL;
	struct file *swap_file = NULL;
	struct address_space *mapping;
	unsigned int type;
	int i, prev;
	int error;
	static int least_priority = 0;
	union swap_header *swap_header = 0;
	int swap_header_version;
	int nr_good_pages = 0;
	unsigned long maxpages = 1;
	int swapfilesize;
	unsigned short *swap_map;
	struct page *page = NULL;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	swap_list_lock();
	p = swap_info;
	for (type = 0 ; type < nr_swapfiles ; type++,p++)
		if (!(p->flags & SWP_USED))
			break;
	error = -EPERM;
	if (type >= MAX_SWAPFILES) {
		swap_list_unlock();
		goto out;
	}
	if (type >= nr_swapfiles)
		nr_swapfiles = type+1;
	INIT_LIST_HEAD(&p->extent_list);
	p->flags = SWP_USED;
	p->nr_extents = 0;
	p->swap_file = NULL;
	p->old_block_size = 0;
	p->swap_map = NULL;
	p->lowest_bit = 0;
	p->highest_bit = 0;
	p->cluster_nr = 0;
	p->sdev_lock = SPIN_LOCK_UNLOCKED;
	p->next = -1;
	if (swap_flags & SWAP_FLAG_PREFER) {
		p->prio =
		  (swap_flags & SWAP_FLAG_PRIO_MASK)>>SWAP_FLAG_PRIO_SHIFT;
	} else {
		p->prio = --least_priority;
	}
	swap_list_unlock();
	name = getname(specialfile);
	error = PTR_ERR(name);
	if (IS_ERR(name))
		goto bad_swap_2;
	swap_file = filp_open(name, O_RDWR, 0);
	error = PTR_ERR(swap_file);
	if (IS_ERR(swap_file)) {
		swap_file = NULL;
		goto bad_swap_2;
	}

	p->swap_file = swap_file;

	error = -EINVAL;
	if (S_ISBLK(swap_file->f_dentry->d_inode->i_mode)) {
		bdev = swap_file->f_dentry->d_inode->i_bdev;
		error = bd_claim(bdev, sys_swapon);
		if (error < 0) {
			bdev = NULL;
			goto bad_swap;
		}
		p->old_block_size = block_size(bdev);
		error = set_blocksize(swap_file->f_dentry->d_inode->i_bdev,
				      PAGE_SIZE);
		if (error < 0)
			goto bad_swap;
		p->bdev = bdev;
	} else if (S_ISREG(swap_file->f_dentry->d_inode->i_mode)) {
		p->bdev = swap_file->f_dentry->d_inode->i_sb->s_bdev;
	} else {
		goto bad_swap;
	}

	mapping = swap_file->f_dentry->d_inode->i_mapping;
	swapfilesize = mapping->host->i_size >> PAGE_SHIFT;

	error = -EBUSY;
	for (i = 0 ; i < nr_swapfiles ; i++) {
		struct swap_info_struct *q = &swap_info[i];
		if (i == type || !q->swap_file)
			continue;
		if (mapping == q->swap_file->f_dentry->d_inode->i_mapping)
			goto bad_swap;
	}

	/*
	 * Read the swap header.
	 */
	page = read_cache_page(mapping, 0,
			(filler_t *)mapping->a_ops->readpage, swap_file);
	if (IS_ERR(page)) {
		error = PTR_ERR(page);
		goto bad_swap;
	}
	wait_on_page_locked(page);
	if (!PageUptodate(page))
		goto bad_swap;
	kmap(page);
	swap_header = page_address(page);

	if (!memcmp("SWAP-SPACE",swap_header->magic.magic,10))
		swap_header_version = 1;
	else if (!memcmp("SWAPSPACE2",swap_header->magic.magic,10))
		swap_header_version = 2;
	else {
		printk("Unable to find swap-space signature\n");
		error = -EINVAL;
		goto bad_swap;
	}
	
	switch (swap_header_version) {
	case 1:
		printk(KERN_ERR "version 0 swap is no longer supported. "
			"Use mkswap -v1 %s\n", name);
		error = -EINVAL;
		goto bad_swap;
	case 2:
		/* Check the swap header's sub-version and the size of
                   the swap file and bad block lists */
		if (swap_header->info.version != 1) {
			printk(KERN_WARNING
			       "Unable to handle swap header version %d\n",
			       swap_header->info.version);
			error = -EINVAL;
			goto bad_swap;
		}

		p->lowest_bit  = 1;
		maxpages = swp_offset(swp_entry(0,~0UL)) - 1;
		if (maxpages > swap_header->info.last_page)
			maxpages = swap_header->info.last_page;
		p->highest_bit = maxpages - 1;

		error = -EINVAL;
		if (swap_header->info.nr_badpages > MAX_SWAP_BADPAGES)
			goto bad_swap;
		
		/* OK, set up the swap map and apply the bad block list */
		if (!(p->swap_map = vmalloc(maxpages * sizeof(short)))) {
			error = -ENOMEM;
			goto bad_swap;
		}

		error = 0;
		memset(p->swap_map, 0, maxpages * sizeof(short));
		for (i=0; i<swap_header->info.nr_badpages; i++) {
			int page = swap_header->info.badpages[i];
			if (page <= 0 || page >= swap_header->info.last_page)
				error = -EINVAL;
			else
				p->swap_map[page] = SWAP_MAP_BAD;
		}
		nr_good_pages = swap_header->info.last_page -
				swap_header->info.nr_badpages -
				1 /* header page */;
		if (error) 
			goto bad_swap;
	}
	
	if (swapfilesize && maxpages > swapfilesize) {
		printk(KERN_WARNING
		       "Swap area shorter than signature indicates\n");
		error = -EINVAL;
		goto bad_swap;
	}
	if (!nr_good_pages) {
		printk(KERN_WARNING "Empty swap-file\n");
		error = -EINVAL;
		goto bad_swap;
	}
	p->swap_map[0] = SWAP_MAP_BAD;
	p->max = maxpages;
	p->pages = nr_good_pages;

	if (setup_swap_extents(p))
		goto bad_swap;

	swap_list_lock();
	swap_device_lock(p);
	p->flags = SWP_ACTIVE;
	nr_swap_pages += nr_good_pages;
	total_swap_pages += nr_good_pages;
	printk(KERN_INFO "Adding %dk swap on %s.  Priority:%d extents:%d\n",
		nr_good_pages<<(PAGE_SHIFT-10), name,
		p->prio, p->nr_extents);

	/* insert swap space into swap_list: */
	prev = -1;
	for (i = swap_list.head; i >= 0; i = swap_info[i].next) {
		if (p->prio >= swap_info[i].prio) {
			break;
		}
		prev = i;
	}
	p->next = i;
	if (prev < 0) {
		swap_list.head = swap_list.next = p - swap_info;
	} else {
		swap_info[prev].next = p - swap_info;
	}
	swap_device_unlock(p);
	swap_list_unlock();
	error = 0;
	goto out;
bad_swap:
	if (bdev) {
		set_blocksize(bdev, p->old_block_size);
		bd_release(bdev);
	}
bad_swap_2:
	swap_list_lock();
	swap_map = p->swap_map;
	p->swap_file = NULL;
	p->swap_map = NULL;
	p->flags = 0;
	if (!(swap_flags & SWAP_FLAG_PREFER))
		++least_priority;
	swap_list_unlock();
	destroy_swap_extents(p);
	if (swap_map)
		vfree(swap_map);
	if (swap_file && !IS_ERR(swap_file))
		filp_close(swap_file, NULL);
out:
	if (page && !IS_ERR(page)) {
		kunmap(page);
		page_cache_release(page);
	}
	if (name)
		putname(name);
	return error;
}

void si_swapinfo(struct sysinfo *val)
{
	unsigned int i;
	unsigned long nr_to_be_unused = 0;

	swap_list_lock();
	for (i = 0; i < nr_swapfiles; i++) {
		unsigned int j;
		if (!(swap_info[i].flags & SWP_USED) ||
		     (swap_info[i].flags & SWP_WRITEOK))
			continue;
		for (j = 0; j < swap_info[i].max; ++j) {
			switch (swap_info[i].swap_map[j]) {
				case 0:
				case SWAP_MAP_BAD:
					continue;
				default:
					nr_to_be_unused++;
			}
		}
	}
	val->freeswap = nr_swap_pages + nr_to_be_unused;
	val->totalswap = total_swap_pages + nr_to_be_unused;
	swap_list_unlock();
}

/*
 * Verify that a swap entry is valid and increment its swap map count.
 *
 * Note: if swap_map[] reaches SWAP_MAP_MAX the entries are treated as
 * "permanent", but will be reclaimed by the next swapoff.
 */
int swap_duplicate(swp_entry_t entry)
{
	struct swap_info_struct * p;
	unsigned long offset, type;
	int result = 0;

	type = swp_type(entry);
	if (type >= nr_swapfiles)
		goto bad_file;
	p = type + swap_info;
	offset = swp_offset(entry);

	swap_device_lock(p);
	if (offset < p->max && p->swap_map[offset]) {
		if (p->swap_map[offset] < SWAP_MAP_MAX - 1) {
			p->swap_map[offset]++;
			result = 1;
		} else if (p->swap_map[offset] <= SWAP_MAP_MAX) {
			if (swap_overflow++ < 5)
				printk(KERN_WARNING "swap_dup: swap entry overflow\n");
			p->swap_map[offset] = SWAP_MAP_MAX;
			result = 1;
		}
	}
	swap_device_unlock(p);
out:
	return result;

bad_file:
	printk(KERN_ERR "swap_dup: %s%08lx\n", Bad_file, entry.val);
	goto out;
}

struct swap_info_struct *
get_swap_info_struct(unsigned type)
{
	return &swap_info[type];
}

/*
 * swap_device_lock prevents swap_map being freed. Don't grab an extra
 * reference on the swaphandle, it doesn't matter if it becomes unused.
 */
int valid_swaphandles(swp_entry_t entry, unsigned long *offset)
{
	int ret = 0, i = 1 << page_cluster;
	unsigned long toff;
	struct swap_info_struct *swapdev = swp_type(entry) + swap_info;

	if (!page_cluster)	/* no readahead */
		return 0;
	toff = (swp_offset(entry) >> page_cluster) << page_cluster;
	if (!toff)		/* first page is swap header */
		toff++, i--;
	*offset = toff;

	swap_device_lock(swapdev);
	do {
		/* Don't read-ahead past the end of the swap area */
		if (toff >= swapdev->max)
			break;
		/* Don't read in free or bad pages */
		if (!swapdev->swap_map[toff])
			break;
		if (swapdev->swap_map[toff] == SWAP_MAP_BAD)
			break;
		toff++;
		ret++;
	} while (--i);
	swap_device_unlock(swapdev);
	return ret;
}
