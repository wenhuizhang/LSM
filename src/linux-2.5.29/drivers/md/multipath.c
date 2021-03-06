/*
 * multipath.c : Multiple Devices driver for Linux
 *
 * Copyright (C) 1999, 2000, 2001 Ingo Molnar, Red Hat
 *
 * Copyright (C) 1996, 1997, 1998 Ingo Molnar, Miguel de Icaza, Gadi Oxman
 *
 * MULTIPATH management functions.
 *
 * derived from raid1.c.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example /usr/src/linux/COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/raid/multipath.h>
#include <linux/bio.h>
#include <linux/buffer_head.h>
#include <asm/atomic.h>

#define MAJOR_NR MD_MAJOR
#define MD_DRIVER
#define MD_PERSONALITY
#define DEVICE_NR(device) (minor(device))

#define MAX_WORK_PER_DISK 128

#define	NR_RESERVED_BUFS	32


/*
 * The following can be used to debug the driver
 */
#define MULTIPATH_DEBUG	0

#if MULTIPATH_DEBUG
#define PRINTK(x...)   printk(x)
#define inline
#define __inline__
#else
#define PRINTK(x...)  do { } while (0)
#endif


static mdk_personality_t multipath_personality;
static spinlock_t retry_list_lock = SPIN_LOCK_UNLOCKED;
struct multipath_bh *multipath_retry_list = NULL, **multipath_retry_tail;


static void *mp_pool_alloc(int gfp_flags, void *data)
{
	struct multipath_bh *mpb;
	mpb = kmalloc(sizeof(*mpb), gfp_flags);
	if (mpb) 
		memset(mpb, 0, sizeof(*mpb));
	return mpb;
}

static void mp_pool_free(void *mpb, void *data)
{
	kfree(mpb);
}

static int multipath_map (mddev_t *mddev, struct block_device **bdev)
{
	multipath_conf_t *conf = mddev_to_conf(mddev);
	int i, disks = MD_SB_DISKS;

	/*
	 * Later we do read balancing on the read side 
	 * now we use the first available disk.
	 */

	for (i = 0; i < disks; i++) {
		if (conf->multipaths[i].operational) {
			*bdev = conf->multipaths[i].bdev;
			return (0);
		}
	}

	printk (KERN_ERR "multipath_map(): no more operational IO paths?\n");
	return (-1);
}

static void multipath_reschedule_retry (struct multipath_bh *mp_bh)
{
	unsigned long flags;
	mddev_t *mddev = mp_bh->mddev;
	multipath_conf_t *conf = mddev_to_conf(mddev);

	spin_lock_irqsave(&retry_list_lock, flags);
	if (multipath_retry_list == NULL)
		multipath_retry_tail = &multipath_retry_list;
	*multipath_retry_tail = mp_bh;
	multipath_retry_tail = &mp_bh->next_mp;
	mp_bh->next_mp = NULL;
	spin_unlock_irqrestore(&retry_list_lock, flags);
	md_wakeup_thread(conf->thread);
}


/*
 * multipath_end_bh_io() is called when we have finished servicing a multipathed
 * operation and are ready to return a success/failure code to the buffer
 * cache layer.
 */
static void multipath_end_bh_io (struct multipath_bh *mp_bh, int uptodate)
{
	struct bio *bio = mp_bh->master_bio;
	multipath_conf_t *conf = mddev_to_conf(mp_bh->mddev);

	bio_endio(bio, uptodate);
	mempool_free(mp_bh, conf->pool);
}

void multipath_end_request(struct bio *bio)
{
	int uptodate = test_bit(BIO_UPTODATE, &bio->bi_flags);
	struct multipath_bh * mp_bh = (struct multipath_bh *)(bio->bi_private);
	multipath_conf_t *conf;
	struct block_device *bdev;
	if (uptodate) {
		multipath_end_bh_io(mp_bh, uptodate);
		return;
	}
	/*
	 * oops, IO error:
	 */
	conf = mddev_to_conf(mp_bh->mddev);
	bdev = conf->multipaths[mp_bh->path].bdev;
	md_error (mp_bh->mddev, bdev);
	printk(KERN_ERR "multipath: %s: rescheduling sector %lu\n", 
		 bdev_partition_name(bdev), bio->bi_sector);
	multipath_reschedule_retry(mp_bh);
	return;
}

/*
 * This routine returns the disk from which the requested read should
 * be done.
 */

static int multipath_read_balance (multipath_conf_t *conf)
{
	int disk;

	for (disk = 0; disk < MD_SB_DISKS; disk++)	
		if (conf->multipaths[disk].operational)
			return disk;
	BUG();
	return 0;
}

static int multipath_make_request (request_queue_t *q, struct bio * bio)
{
	mddev_t *mddev = q->queuedata;
	multipath_conf_t *conf = mddev_to_conf(mddev);
	struct multipath_bh * mp_bh;
	struct multipath_info *multipath;

	mp_bh = mempool_alloc(conf->pool, GFP_NOIO);

	mp_bh->master_bio = bio;
	mp_bh->mddev = mddev;

	/*
	 * read balancing logic:
	 */
	mp_bh->path = multipath_read_balance(conf);
	multipath = conf->multipaths + mp_bh->path;

	mp_bh->bio = *bio;
	mp_bh->bio.bi_bdev = multipath->bdev;
	mp_bh->bio.bi_end_io = multipath_end_request;
	mp_bh->bio.bi_private = mp_bh;
	generic_make_request(&mp_bh->bio);
	return 0;
}

static int multipath_status (char *page, mddev_t *mddev)
{
	multipath_conf_t *conf = mddev_to_conf(mddev);
	int sz = 0, i;
	
	sz += sprintf (page+sz, " [%d/%d] [", conf->raid_disks,
						 conf->working_disks);
	for (i = 0; i < conf->raid_disks; i++)
		sz += sprintf (page+sz, "%s",
			conf->multipaths[i].operational ? "U" : "_");
	sz += sprintf (page+sz, "]");
	return sz;
}

#define LAST_DISK KERN_ALERT \
"multipath: only one IO path left and IO error.\n"

#define NO_SPARE_DISK KERN_ALERT \
"multipath: no spare IO path left!\n"

#define DISK_FAILED KERN_ALERT \
"multipath: IO failure on %s, disabling IO path. \n" \
"	Operation continuing on %d IO paths.\n"

static void mark_disk_bad (mddev_t *mddev, int failed)
{
	multipath_conf_t *conf = mddev_to_conf(mddev);
	struct multipath_info *multipath = conf->multipaths+failed;

	multipath->operational = 0;
	mddev->sb_dirty = 1;
	conf->working_disks--;
	printk (DISK_FAILED, bdev_partition_name (multipath->bdev),
				 conf->working_disks);
}

/*
 * Careful, this can execute in IRQ contexts as well!
 */
static int multipath_error (mddev_t *mddev, struct block_device *bdev)
{
	multipath_conf_t *conf = mddev_to_conf(mddev);
	struct multipath_info * multipaths = conf->multipaths;
	int disks = MD_SB_DISKS;
	int i;


	if (conf->working_disks <= 1) {
		/*
		 * Uh oh, we can do nothing if this is our last path, but
		 * first check if this is a queued request for a device
		 * which has just failed.
		 */
		for (i = 0; i < disks; i++) {
			if (multipaths[i].bdev == bdev && !multipaths[i].operational)
				return 0;
		}
		printk (LAST_DISK);
		return 1; /* leave it active... it's all we have */
	} else {
		/*
		 * Mark disk as unusable
		 */
		for (i = 0; i < disks; i++) {
			if (multipaths[i].bdev == bdev && multipaths[i].operational) {
				mark_disk_bad(mddev, i);
				break;
			}
		}
	}
	return 0;
}

#undef LAST_DISK
#undef NO_SPARE_DISK
#undef DISK_FAILED


static void print_multipath_conf (multipath_conf_t *conf)
{
	int i;
	struct multipath_info *tmp;

	printk("MULTIPATH conf printout:\n");
	if (!conf) {
		printk("(conf==NULL)\n");
		return;
	}
	printk(" --- wd:%d rd:%d\n", conf->working_disks,
			 conf->raid_disks);

	for (i = 0; i < MD_SB_DISKS; i++) {
		tmp = conf->multipaths + i;
		if (tmp->operational || tmp->used_slot)
			printk(" disk%d, o:%d, us:%d dev:%s\n",
				i,tmp->operational,
				tmp->used_slot,
				bdev_partition_name(tmp->bdev));
	}
}


static int multipath_add_disk(mddev_t *mddev, mdk_rdev_t *rdev)
{
	multipath_conf_t *conf = mddev->private;
	int err = 1;
	struct multipath_info *p = conf->multipaths + rdev->raid_disk;

	print_multipath_conf(conf);
	spin_lock_irq(&conf->device_lock);
	if (!p->used_slot) {
		p->bdev = rdev->bdev;
		p->operational = 1;
		p->used_slot = 1;
		conf->working_disks++;
		err = 0;
	}
	if (err)
		MD_BUG();
	spin_unlock_irq(&conf->device_lock);

	print_multipath_conf(conf);
	return err;
}

static int multipath_remove_disk(mddev_t *mddev, int number)
{
	multipath_conf_t *conf = mddev->private;
	int err = 1;
	struct multipath_info *p = conf->multipaths + number;

	print_multipath_conf(conf);
	spin_lock_irq(&conf->device_lock);

	if (p->used_slot) {
		if (p->operational) {
			printk(KERN_ERR "hot-remove-disk, slot %d is identified but is still operational!\n", number);
			err = -EBUSY;
			goto abort;
		}
		p->bdev = NULL;
		p->used_slot = 0;
		err = 0;
	}
	if (err)
		MD_BUG();
abort:
	spin_unlock_irq(&conf->device_lock);

	print_multipath_conf(conf);
	return err;
}

#define IO_ERROR KERN_ALERT \
"multipath: %s: unrecoverable IO read error for block %lu\n"

#define REDIRECT_SECTOR KERN_ERR \
"multipath: %s: redirecting sector %lu to another IO path\n"

/*
 * This is a kernel thread which:
 *
 *	1.	Retries failed read operations on working multipaths.
 *	2.	Updates the raid superblock when problems encounter.
 *	3.	Performs writes following reads for array syncronising.
 */

static void multipathd (void *data)
{
	struct multipath_bh *mp_bh;
	struct bio *bio;
	unsigned long flags;
	mddev_t *mddev;
	struct block_device *bdev;

	for (;;) {
		spin_lock_irqsave(&retry_list_lock, flags);
		mp_bh = multipath_retry_list;
		if (!mp_bh)
			break;
		multipath_retry_list = mp_bh->next_mp;
		spin_unlock_irqrestore(&retry_list_lock, flags);

		mddev = mp_bh->mddev;
		bio = &mp_bh->bio;
		bio->bi_sector = mp_bh->master_bio->bi_sector;
		bdev = bio->bi_bdev;
		
		multipath_map (mddev, &bio->bi_bdev);
		if (bio->bi_bdev == bdev) {
			printk(IO_ERROR,
				bdev_partition_name(bio->bi_bdev), bio->bi_sector);
			multipath_end_bh_io(mp_bh, 0);
		} else {
			printk(REDIRECT_SECTOR,
				bdev_partition_name(bio->bi_bdev), bio->bi_sector);
			generic_make_request(bio);
		}
	}
	spin_unlock_irqrestore(&retry_list_lock, flags);
}
#undef IO_ERROR
#undef REDIRECT_SECTOR

#define INVALID_LEVEL KERN_WARNING \
"multipath: md%d: raid level not set to multipath IO (%d)\n"

#define NO_SB KERN_ERR \
"multipath: disabled IO path %s (couldn't access raid superblock)\n"

#define ERRORS KERN_ERR \
"multipath: disabled IO path %s (errors detected)\n"

#define NOT_IN_SYNC KERN_ERR \
"multipath: making IO path %s a spare path (not in sync)\n"

#define INCONSISTENT KERN_ERR \
"multipath: disabled IO path %s (inconsistent descriptor)\n"

#define ALREADY_RUNNING KERN_ERR \
"multipath: disabled IO path %s (multipath %d already operational)\n"

#define OPERATIONAL KERN_INFO \
"multipath: device %s operational as IO path %d\n"

#define MEM_ERROR KERN_ERR \
"multipath: couldn't allocate memory for md%d\n"

#define SPARE KERN_INFO \
"multipath: spare IO path %s\n"

#define NONE_OPERATIONAL KERN_ERR \
"multipath: no operational IO paths for md%d\n"

#define SB_DIFFERENCES KERN_ERR \
"multipath: detected IO path differences!\n"

#define ARRAY_IS_ACTIVE KERN_INFO \
"multipath: array md%d active with %d out of %d IO paths\n"

#define THREAD_ERROR KERN_ERR \
"multipath: couldn't allocate thread for md%d\n"

static int multipath_run (mddev_t *mddev)
{
	multipath_conf_t *conf;
	int disk_idx;
	struct multipath_info *disk;
	mdk_rdev_t *rdev;
	struct list_head *tmp;
	int num_rdevs = 0;

	MOD_INC_USE_COUNT;

	if (mddev->level != LEVEL_MULTIPATH) {
		printk(INVALID_LEVEL, mdidx(mddev), mddev->level);
		goto out;
	}
	/*
	 * copy the already verified devices into our private MULTIPATH
	 * bookkeeping area. [whatever we allocate in multipath_run(),
	 * should be freed in multipath_stop()]
	 */

	conf = kmalloc(sizeof(multipath_conf_t), GFP_KERNEL);
	mddev->private = conf;
	if (!conf) {
		printk(MEM_ERROR, mdidx(mddev));
		goto out;
	}
	memset(conf, 0, sizeof(*conf));

	ITERATE_RDEV(mddev,rdev,tmp) {
		if (rdev->faulty) {
			/* this is a "should never happen" case and if it */
			/* ever does happen, a continue; won't help */
			printk(ERRORS, bdev_partition_name(rdev->bdev));
			continue;
		} else {
			/* this is a "should never happen" case and if it */
			/* ever does happen, a continue; won't help */
			if (!rdev->sb) {
				MD_BUG();
				continue;
			}
		}
		if (rdev->desc_nr == -1) {
			MD_BUG();
			continue;
		}

		disk_idx = rdev->raid_disk;
		disk = conf->multipaths + disk_idx;

		/*
		 * Mark all disks as active to start with, there are no
		 * spares.  multipath_read_balance deals with choose
		 * the "best" operational device.
		 */
		disk->bdev = rdev->bdev;
		disk->operational = 1;
		disk->used_slot = 1;
		num_rdevs++;
	}

	conf->raid_disks = mddev->raid_disks = num_rdevs;
	mddev->sb_dirty = 1;
	conf->mddev = mddev;
	conf->device_lock = SPIN_LOCK_UNLOCKED;

	if (!conf->working_disks) {
		printk(NONE_OPERATIONAL, mdidx(mddev));
		goto out_free_conf;
	}

	conf->pool = mempool_create(NR_RESERVED_BUFS,
				    mp_pool_alloc, mp_pool_free,
				    NULL);
	if (conf->pool == NULL) {
		printk(MEM_ERROR, mdidx(mddev));
		goto out_free_conf;
	}

	{
		const char * name = "multipathd";

		conf->thread = md_register_thread(multipathd, conf, name);
		if (!conf->thread) {
			printk(THREAD_ERROR, mdidx(mddev));
			goto out_free_conf;
		}
	}

	printk(ARRAY_IS_ACTIVE, mdidx(mddev), conf->working_disks,
			mddev->raid_disks);
	/*
	 * Ok, everything is just fine now
	 */
	return 0;

out_free_conf:
	if (conf->pool)
		mempool_destroy(conf->pool);
	kfree(conf);
	mddev->private = NULL;
out:
	MOD_DEC_USE_COUNT;
	return -EIO;
}

#undef INVALID_LEVEL
#undef NO_SB
#undef ERRORS
#undef NOT_IN_SYNC
#undef INCONSISTENT
#undef ALREADY_RUNNING
#undef OPERATIONAL
#undef SPARE
#undef NONE_OPERATIONAL
#undef SB_DIFFERENCES
#undef ARRAY_IS_ACTIVE

static int multipath_stop (mddev_t *mddev)
{
	multipath_conf_t *conf = mddev_to_conf(mddev);

	md_unregister_thread(conf->thread);
	mempool_destroy(conf->pool);
	kfree(conf);
	mddev->private = NULL;
	MOD_DEC_USE_COUNT;
	return 0;
}

static mdk_personality_t multipath_personality=
{
	.name		= "multipath",
	.make_request	= multipath_make_request,
	.run		= multipath_run,
	.stop		= multipath_stop,
	.status		= multipath_status,
	.error_handler	= multipath_error,
	.hot_add_disk	= multipath_add_disk,
	.hot_remove_disk= multipath_remove_disk,
};

static int __init multipath_init (void)
{
	return register_md_personality (MULTIPATH, &multipath_personality);
}

static void __exit multipath_exit (void)
{
	unregister_md_personality (MULTIPATH);
}

module_init(multipath_init);
module_exit(multipath_exit);
MODULE_LICENSE("GPL");
