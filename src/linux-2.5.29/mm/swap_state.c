/*
 *  linux/mm/swap_state.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *  Swap reorganised 29.12.95, Stephen Tweedie
 *
 *  Rewritten to use page cache, (C) 1998 Stephen Tweedie
 */

#include <linux/mm.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/swapctl.h>
#include <linux/init.h>
#include <linux/pagemap.h>
#include <linux/smp_lock.h>
#include <linux/buffer_head.h>	/* block_sync_page() */

#include <asm/pgtable.h>

/*
 * swapper_inode doesn't do anything much.  It is really only here to
 * avoid some special-casing in other parts of the kernel.
 */
static struct inode swapper_inode = {
	i_mapping:	&swapper_space,
};

extern struct address_space_operations swap_aops;

struct address_space swapper_space = {
	page_tree:	RADIX_TREE_INIT(GFP_ATOMIC),
	page_lock:	RW_LOCK_UNLOCKED,
	clean_pages:	LIST_HEAD_INIT(swapper_space.clean_pages),
	dirty_pages:	LIST_HEAD_INIT(swapper_space.dirty_pages),
	io_pages:	LIST_HEAD_INIT(swapper_space.io_pages),
	locked_pages:	LIST_HEAD_INIT(swapper_space.locked_pages),
	host:		&swapper_inode,
	a_ops:		&swap_aops,
	i_shared_lock:	SPIN_LOCK_UNLOCKED,
	private_lock:	SPIN_LOCK_UNLOCKED,
	private_list:	LIST_HEAD_INIT(swapper_space.private_list),
};

#ifdef SWAP_CACHE_INFO
#define INC_CACHE_INFO(x)	(swap_cache_info.x++)

static struct {
	unsigned long add_total;
	unsigned long del_total;
	unsigned long find_success;
	unsigned long find_total;
	unsigned long noent_race;
	unsigned long exist_race;
} swap_cache_info;

void show_swap_cache_info(void)
{
	printk("Swap cache: add %lu, delete %lu, find %lu/%lu, race %lu+%lu\n",
		swap_cache_info.add_total, swap_cache_info.del_total,
		swap_cache_info.find_success, swap_cache_info.find_total,
		swap_cache_info.noent_race, swap_cache_info.exist_race);
}
#else
#define INC_CACHE_INFO(x)	do { } while (0)
#endif

int add_to_swap_cache(struct page *page, swp_entry_t entry)
{
	int error;

	if (page->mapping)
		BUG();
	if (!swap_duplicate(entry)) {
		INC_CACHE_INFO(noent_race);
		return -ENOENT;
	}
	error = add_to_page_cache(page, &swapper_space, entry.val);
	if (error != 0) {
		swap_free(entry);
		if (error == -EEXIST)
			INC_CACHE_INFO(exist_race);
		return error;
	}
	if (!PageLocked(page))
		BUG();
	if (!PageSwapCache(page))
		BUG();
	INC_CACHE_INFO(add_total);
	return 0;
}

/*
 * This must be called only on pages that have
 * been verified to be in the swap cache.
 */
void __delete_from_swap_cache(struct page *page)
{
	BUG_ON(!PageLocked(page));
	BUG_ON(!PageSwapCache(page));
	BUG_ON(PageWriteback(page));
	ClearPageDirty(page);
	__remove_inode_page(page);
	INC_CACHE_INFO(del_total);
}

/**
 * add_to_swap - allocate swap space for a page
 * @page: page we want to move to swap
 *
 * Allocate swap space for the page and add the page to the
 * swap cache.  Caller needs to hold the page lock. 
 */
int add_to_swap(struct page * page)
{
	swp_entry_t entry;
	int flags;

	if (!PageLocked(page))
		BUG();

	for (;;) {
		entry = get_swap_page();
		if (!entry.val)
			return 0;

		/* Radix-tree node allocations are performing
		 * GFP_ATOMIC allocations under PF_MEMALLOC.  
		 * They can completely exhaust the page allocator.  
		 *
		 * So PF_MEMALLOC is dropped here.  This causes the slab 
		 * allocations to fail earlier, so radix-tree nodes will 
		 * then be allocated from the mempool reserves.
		 *
		 * We're still using __GFP_HIGH for radix-tree node
		 * allocations, so some of the emergency pools are available,
		 * just not all of them.
		 */

		flags = current->flags;
		current->flags &= ~PF_MEMALLOC;
		current->flags |= PF_NOWARN;
		ClearPageUptodate(page);		/* why? */

		/*
		 * Add it to the swap cache and mark it dirty
		 * (adding to the page cache will clear the dirty
		 * and uptodate bits, so we need to do it again)
		 */
		switch (add_to_swap_cache(page, entry)) {
		case 0:				/* Success */
			current->flags = flags;
			SetPageUptodate(page);
			set_page_dirty(page);
			swap_free(entry);
			return 1;
		case -ENOMEM:			/* radix-tree allocation */
			current->flags = flags;
			swap_free(entry);
			return 0;
		default:			/* ENOENT: raced */
			break;
		}
		/* Raced with "speculative" read_swap_cache_async */
		current->flags = flags;
		swap_free(entry);
	}
}

/*
 * This must be called only on pages that have
 * been verified to be in the swap cache and locked.
 * It will never put the page into the free list,
 * the caller has a reference on the page.
 */
void delete_from_swap_cache(struct page *page)
{
	swp_entry_t entry;

	BUG_ON(!PageLocked(page));
	BUG_ON(PageWriteback(page));
	BUG_ON(page_has_buffers(page));
  
	entry.val = page->index;

	write_lock(&swapper_space.page_lock);
	__delete_from_swap_cache(page);
	write_unlock(&swapper_space.page_lock);

	swap_free(entry);
	page_cache_release(page);
}

int move_to_swap_cache(struct page *page, swp_entry_t entry)
{
	struct address_space *mapping = page->mapping;
	void **pslot;
	int err;

	if (!mapping)
		BUG();

	if (!swap_duplicate(entry)) {
		INC_CACHE_INFO(noent_race);
		return -ENOENT;
	}

	write_lock(&swapper_space.page_lock);
	write_lock(&mapping->page_lock);

	err = radix_tree_reserve(&swapper_space.page_tree, entry.val, &pslot);
	if (!err) {
		/* Remove it from the page cache */
		__remove_inode_page (page);

		/* Add it to the swap cache */
		*pslot = page;
		/*
		 * This code used to clear PG_uptodate, PG_error, PG_arch1,
		 * PG_referenced and PG_checked.  What _should_ it clear?
		 */
		ClearPageUptodate(page);
		ClearPageReferenced(page);

		SetPageLocked(page);
		ClearPageDirty(page);
		___add_to_page_cache(page, &swapper_space, entry.val);
	}

	write_unlock(&mapping->page_lock);
	write_unlock(&swapper_space.page_lock);

	if (!err) {
		INC_CACHE_INFO(add_total);
		return 0;
	}

	swap_free(entry);

	if (err == -EEXIST)
		INC_CACHE_INFO(exist_race);

	return err;
}

int move_from_swap_cache(struct page *page, unsigned long index,
		struct address_space *mapping)
{
	void **pslot;
	int err;

	BUG_ON(!PageLocked(page));
	BUG_ON(PageWriteback(page));
	BUG_ON(page_has_buffers(page));

	write_lock(&swapper_space.page_lock);
	write_lock(&mapping->page_lock);

	err = radix_tree_reserve(&mapping->page_tree, index, &pslot);
	if (!err) {
		swp_entry_t entry;

		entry.val = page->index;
		__delete_from_swap_cache(page);

		*pslot = page;

		/*
		 * This code used to clear PG_uptodate, PG_error, PG_referenced,
		 * PG_arch_1 and PG_checked.  It's not really clear why.
		 */
		ClearPageUptodate(page);
		ClearPageReferenced(page);

		/*
		 * ___add_to_page_cache puts the page on ->clean_pages,
		 * but it's dirty.  If it's on ->clean_pages, it will basically
		 * never get written out.
		 */
		SetPageDirty(page);
		___add_to_page_cache(page, mapping, index);
		/* fix that up */
		list_del(&page->list);
		list_add(&page->list, &mapping->dirty_pages);
		write_unlock(&mapping->page_lock);
		write_unlock(&swapper_space.page_lock);

		/* Do this outside ->page_lock */
		swap_free(entry);
		return 0;
	}

	write_unlock(&mapping->page_lock);
	write_unlock(&swapper_space.page_lock);
	return err;
}

/* 
 * Perform a free_page(), also freeing any swap cache associated with
 * this page if it is the last user of the page. Can not do a lock_page,
 * as we are holding the page_table_lock spinlock.
 */
void free_page_and_swap_cache(struct page *page)
{
	/* 
	 * If we are the only user, then try to free up the swap cache. 
	 * 
	 * Its ok to check for PageSwapCache without the page lock
	 * here because we are going to recheck again inside 
	 * exclusive_swap_page() _with_ the lock. 
	 * 					- Marcelo
	 */
	if (PageSwapCache(page) && !TestSetPageLocked(page)) {
		remove_exclusive_swap_page(page);
		unlock_page(page);
	}
	page_cache_release(page);
}

/*
 * Lookup a swap entry in the swap cache. A found page will be returned
 * unlocked and with its refcount incremented - we rely on the kernel
 * lock getting page table operations atomic even if we drop the page
 * lock before returning.
 */
struct page * lookup_swap_cache(swp_entry_t entry)
{
	struct page *found;

	found = find_get_page(&swapper_space, entry.val);
	/*
	 * Unsafe to assert PageSwapCache and mapping on page found:
	 * if SMP nothing prevents swapoff from deleting this page from
	 * the swap cache at this moment.  find_lock_page would prevent
	 * that, but no need to change: we _have_ got the right page.
	 */
	INC_CACHE_INFO(find_total);
	if (found)
		INC_CACHE_INFO(find_success);
	return found;
}

/* 
 * Locate a page of swap in physical memory, reserving swap cache space
 * and reading the disk if it is not already cached.
 * A failure return means that either the page allocation failed or that
 * the swap entry is no longer in use.
 */
struct page * read_swap_cache_async(swp_entry_t entry)
{
	struct page *found_page, *new_page = NULL;
	int err;

	do {
		/*
		 * First check the swap cache.  Since this is normally
		 * called after lookup_swap_cache() failed, re-calling
		 * that would confuse statistics: use find_get_page()
		 * directly.
		 */
		found_page = find_get_page(&swapper_space, entry.val);
		if (found_page)
			break;

		/*
		 * Get a new page to read into from swap.
		 */
		if (!new_page) {
			new_page = alloc_page(GFP_HIGHUSER);
			if (!new_page)
				break;		/* Out of memory */
		}

		/*
		 * Associate the page with swap entry in the swap cache.
		 * May fail (-ENOENT) if swap entry has been freed since
		 * our caller observed it.  May fail (-EEXIST) if there
		 * is already a page associated with this entry in the
		 * swap cache: added by a racing read_swap_cache_async,
		 * or by try_to_swap_out (or shmem_writepage) re-using
		 * the just freed swap entry for an existing page.
		 * May fail (-ENOMEM) if radix-tree node allocation failed.
		 */
		err = add_to_swap_cache(new_page, entry);
		if (!err) {
			/*
			 * Initiate read into locked page and return.
			 */
			swap_readpage(NULL, new_page);
			return new_page;
		}
	} while (err != -ENOENT && err != -ENOMEM);

	if (new_page)
		page_cache_release(new_page);
	return found_page;
}
