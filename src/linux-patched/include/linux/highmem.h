#ifndef _LINUX_HIGHMEM_H
#define _LINUX_HIGHMEM_H

#include <linux/config.h>
#include <linux/fs.h>
#include <asm/cacheflush.h>

#ifdef CONFIG_HIGHMEM

extern struct page *highmem_start_page;

#include <asm/highmem.h>

/* declarations for linux/mm/highmem.c */
unsigned int nr_free_highpages(void);

extern void check_highmem_ptes(void);

#else /* CONFIG_HIGHMEM */

static inline unsigned int nr_free_highpages(void) { return 0; }

static inline void *kmap(struct page *page) { return page_address(page); }

#define kunmap(page) do { } while (0)

#define kmap_atomic(page,idx)		kmap(page)
#define kunmap_atomic(page,idx)		kunmap(page)

#endif /* CONFIG_HIGHMEM */

/* when CONFIG_HIGHMEM is not set these will be plain clear/copy_page */
static inline void clear_user_highpage(struct page *page, unsigned long vaddr)
{
	void *addr = kmap_atomic(page, KM_USER0);
	clear_user_page(addr, vaddr, page);
	kunmap_atomic(addr, KM_USER0);
}

static inline void clear_highpage(struct page *page)
{
	void *kaddr = kmap_atomic(page, KM_USER0);
	clear_page(kaddr);
	kunmap_atomic(kaddr, KM_USER0);
}

/*
 * Same but also flushes aliased cache contents to RAM.
 */
static inline void memclear_highpage_flush(struct page *page, unsigned int offset, unsigned int size)
{
	void *kaddr;

	if (offset + size > PAGE_SIZE)
		BUG();

	kaddr = kmap_atomic(page, KM_USER0);
	memset((char *)kaddr + offset, 0, size);
	flush_dcache_page(page);
	flush_page_to_ram(page);
	kunmap_atomic(kaddr, KM_USER0);
}

static inline void copy_user_highpage(struct page *to, struct page *from, unsigned long vaddr)
{
	char *vfrom, *vto;

	vfrom = kmap_atomic(from, KM_USER0);
	vto = kmap_atomic(to, KM_USER1);
	copy_user_page(vto, vfrom, vaddr, to);
	kunmap_atomic(vfrom, KM_USER0);
	kunmap_atomic(vto, KM_USER1);
}

#endif /* _LINUX_HIGHMEM_H */
