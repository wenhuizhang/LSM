/*
 *  linux/fs/sysv/balloc.c
 *
 *  minix/bitmap.c
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  ext/freelists.c
 *  Copyright (C) 1992  Remy Card (card@masi.ibp.fr)
 *
 *  xenix/alloc.c
 *  Copyright (C) 1992  Doug Evans
 *
 *  coh/alloc.c
 *  Copyright (C) 1993  Pascal Haible, Bruno Haible
 *
 *  sysv/balloc.c
 *  Copyright (C) 1993  Bruno Haible
 *
 *  This file contains code for allocating/freeing blocks.
 */

#include <linux/buffer_head.h>
#include "sysv.h"

/* We don't trust the value of
   sb->sv_sbd2->s_tfree = *sb->sv_free_blocks
   but we nevertheless keep it up to date. */

static inline u32 *get_chunk(struct super_block *sb, struct buffer_head *bh)
{
	char *bh_data = bh->b_data;

	if (SYSV_SB(sb)->s_type == FSTYPE_SYSV4)
		return (u32*)(bh_data+4);
	else
		return (u32*)(bh_data+2);
}

/* NOTE NOTE NOTE: nr is a block number _as_ _stored_ _on_ _disk_ */

void sysv_free_block(struct super_block * sb, u32 nr)
{
	struct sysv_sb_info * sbi = SYSV_SB(sb);
	struct buffer_head * bh;
	u32 *blocks = sbi->s_bcache;
	unsigned count;
	unsigned block = fs32_to_cpu(sbi, nr);

	/*
	 * This code does not work at all for AFS (it has a bitmap
	 * free list).  As AFS is supposed to be read-only no one
	 * should call this for an AFS filesystem anyway...
	 */
	if (sbi->s_type == FSTYPE_AFS)
		return;

	if (block < sbi->s_firstdatazone || block >= sbi->s_nzones) {
		printk("sysv_free_block: trying to free block not in datazone\n");
		return;
	}

	lock_super(sb);
	count = fs16_to_cpu(sbi, *sbi->s_bcache_count);

	if (count > sbi->s_flc_size) {
		printk("sysv_free_block: flc_count > flc_size\n");
		unlock_super(sb);
		return;
	}
	/* If the free list head in super-block is full, it is copied
	 * into this block being freed, ditto if it's completely empty
	 * (applies only on Coherent).
	 */
	if (count == sbi->s_flc_size || count == 0) {
		block += sbi->s_block_base;
		bh = sb_getblk(sb, block);
		if (!bh) {
			printk("sysv_free_block: getblk() failed\n");
			unlock_super(sb);
			return;
		}
		memset(bh->b_data, 0, sb->s_blocksize);
		*(u16*)bh->b_data = cpu_to_fs16(sbi, count);
		memcpy(get_chunk(sb,bh), blocks, count * sizeof(sysv_zone_t));
		mark_buffer_dirty(bh);
		set_buffer_uptodate(bh);
		brelse(bh);
		count = 0;
	}
	sbi->s_bcache[count++] = nr;

	*sbi->s_bcache_count = cpu_to_fs16(sbi, count);
	fs32_add(sbi, sbi->s_free_blocks, 1);
	dirty_sb(sb);
	unlock_super(sb);
}

u32 sysv_new_block(struct super_block * sb)
{
	struct sysv_sb_info *sbi = SYSV_SB(sb);
	unsigned int block;
	u32 nr;
	struct buffer_head * bh;
	unsigned count;

	lock_super(sb);
	count = fs16_to_cpu(sbi, *sbi->s_bcache_count);

	if (count == 0) /* Applies only to Coherent FS */
		goto Enospc;
	nr = sbi->s_bcache[--count];
	if (nr == 0)  /* Applies only to Xenix FS, SystemV FS */
		goto Enospc;

	block = fs32_to_cpu(sbi, nr);

	*sbi->s_bcache_count = cpu_to_fs16(sbi, count);

	if (block < sbi->s_firstdatazone || block >= sbi->s_nzones) {
		printk("sysv_new_block: new block %d is not in data zone\n",
			block);
		goto Enospc;
	}

	if (count == 0) { /* the last block continues the free list */
		unsigned count;

		block += sbi->s_block_base;
		if (!(bh = sb_bread(sb, block))) {
			printk("sysv_new_block: cannot read free-list block\n");
			/* retry this same block next time */
			*sbi->s_bcache_count = cpu_to_fs16(sbi, 1);
			goto Enospc;
		}
		count = fs16_to_cpu(sbi, *(u16*)bh->b_data);
		if (count > sbi->s_flc_size) {
			printk("sysv_new_block: free-list block with >flc_size entries\n");
			brelse(bh);
			goto Enospc;
		}
		*sbi->s_bcache_count = cpu_to_fs16(sbi, count);
		memcpy(sbi->s_bcache, get_chunk(sb, bh),
				count * sizeof(sysv_zone_t));
		brelse(bh);
	}
	/* Now the free list head in the superblock is valid again. */
	fs32_add(sbi, sbi->s_free_blocks, -1);
	dirty_sb(sb);
	unlock_super(sb);
	return nr;

Enospc:
	unlock_super(sb);
	return 0;
}

unsigned long sysv_count_free_blocks(struct super_block * sb)
{
	struct sysv_sb_info * sbi = SYSV_SB(sb);
	int sb_count;
	int count;
	struct buffer_head * bh = NULL;
	u32 *blocks;
	unsigned block;
	int n;

	/*
	 * This code does not work at all for AFS (it has a bitmap
	 * free list).  As AFS is supposed to be read-only we just
	 * lie and say it has no free block at all.
	 */
	if (sbi->s_type == FSTYPE_AFS)
		return 0;

	lock_super(sb);
	sb_count = fs32_to_cpu(sbi, *sbi->s_free_blocks);

	if (0)
		goto trust_sb;

	/* this causes a lot of disk traffic ... */
	count = 0;
	n = fs16_to_cpu(sbi, *sbi->s_bcache_count);
	blocks = sbi->s_bcache;
	while (1) {
		if (n > sbi->s_flc_size)
			goto E2big;
		block = 0;
		while (n && (block = blocks[--n]) != 0)
			count++;
		if (block == 0)
			break;

		block = fs32_to_cpu(sbi, block);
		if (bh)
			brelse(bh);

		if (block < sbi->s_firstdatazone || block >= sbi->s_nzones)
			goto Einval;
		block += sbi->s_block_base;
		bh = sb_bread(sb, block);
		if (!bh)
			goto Eio;
		n = fs16_to_cpu(sbi, *(u16*)bh->b_data);
		blocks = get_chunk(sb, bh);
	}
	if (bh)
		brelse(bh);
	if (count != sb_count)
		goto Ecount;
done:
	unlock_super(sb);
	return count;

Einval:
	printk("sysv_count_free_blocks: new block %d is not in data zone\n",
		block);
	goto trust_sb;
Eio:
	printk("sysv_count_free_blocks: cannot read free-list block\n");
	goto trust_sb;
E2big:
	printk("sysv_count_free_blocks: >flc_size entries in free-list block\n");
	if (bh)
		brelse(bh);
trust_sb:
	count = sb_count;
	goto done;
Ecount:
	printk("sysv_count_free_blocks: free block count was %d, "
		"correcting to %d\n", sb_count, count);
	if (!(sb->s_flags & MS_RDONLY)) {
		*sbi->s_free_blocks = cpu_to_fs32(sbi, count);
		dirty_sb(sb);
	}
	goto done;
}
