/*
 *  linux/fs/super.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  super.c contains code to handle: - mount structures
 *                                   - super-block tables
 *                                   - filesystem drivers list
 *                                   - mount system call
 *                                   - umount system call
 *                                   - ustat system call
 *
 * GK 2/5/95  -  Changed to support mounting the root fs via NFS
 *
 *  Added kerneld support: Jacques Gelinas and Bjorn Ekwall
 *  Added change_root: Werner Almesberger & Hans Lermen, Feb '96
 *  Added options to /proc/mounts:
 *    Torbj�rn Lindh (torbjorn.lindh@gopta.se), April 14, 1996.
 *  Added devfs support: Richard Gooch <rgooch@atnf.csiro.au>, 13-JAN-1998
 *  Heavily rewritten for 'one fs - one tree' dcache architecture. AV, Mar 2000
 */

#include <linux/config.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/acct.h>
#include <linux/blkdev.h>
#include <linux/quotaops.h>
#include <linux/namei.h>
#include <linux/buffer_head.h>		/* for fsync_super() */
#include <asm/uaccess.h>

#include <linux/security.h>

void get_filesystem(struct file_system_type *fs);
void put_filesystem(struct file_system_type *fs);
struct file_system_type *get_fs_type(const char *name);

LIST_HEAD(super_blocks);
spinlock_t sb_lock = SPIN_LOCK_UNLOCKED;

/**
 *	alloc_super	-	create new superblock
 *
 *	Allocates and initializes a new &struct super_block.  alloc_super()
 *	returns a pointer new superblock or %NULL if allocation had failed.
 */
static struct super_block *alloc_super(void)
{
	struct super_block *s = kmalloc(sizeof(struct super_block),  GFP_USER);
	if (s) {
		memset(s, 0, sizeof(struct super_block));
		if (security_ops->sb_alloc_security(s)) {
			kfree(s);
			s = NULL;
			goto out;
		}
		INIT_LIST_HEAD(&s->s_dirty);
		INIT_LIST_HEAD(&s->s_io);
		INIT_LIST_HEAD(&s->s_locked_inodes);
		INIT_LIST_HEAD(&s->s_files);
		INIT_LIST_HEAD(&s->s_instances);
		INIT_LIST_HEAD(&s->s_anon);
		init_rwsem(&s->s_umount);
		sema_init(&s->s_lock, 1);
		down_write(&s->s_umount);
		s->s_count = S_BIAS;
		atomic_set(&s->s_active, 1);
		sema_init(&s->s_vfs_rename_sem,1);
		sema_init(&s->s_dquot.dqio_sem, 1);
		sema_init(&s->s_dquot.dqoff_sem, 1);
		s->s_maxbytes = MAX_NON_LFS;
		s->dq_op = sb_dquot_ops;
		s->s_qcop = sb_quotactl_ops;
	}
out:
	return s;
}

/**
 *	destroy_super	-	frees a superblock
 *	@s: superblock to free
 *
 *	Frees a superblock.
 */
static inline void destroy_super(struct super_block *s)
{
	security_ops->sb_free_security(s);
	kfree(s);
}

/* Superblock refcounting  */

/**
 *	put_super	-	drop a temporary reference to superblock
 *	@s: superblock in question
 *
 *	Drops a temporary reference, frees superblock if there's no
 *	references left.
 */
static inline void put_super(struct super_block *s)
{
	spin_lock(&sb_lock);
	if (!--s->s_count)
		destroy_super(s);
	spin_unlock(&sb_lock);
}

/**
 *	deactivate_super	-	drop an active reference to superblock
 *	@s: superblock to deactivate
 *
 *	Drops an active reference to superblock, acquiring a temprory one if
 *	there is no active references left.  In that case we lock superblock,
 *	tell fs driver to shut it down and drop the temporary reference we
 *	had just acquired.
 */
void deactivate_super(struct super_block *s)
{
	struct file_system_type *fs = s->s_type;
	if (atomic_dec_and_lock(&s->s_active, &sb_lock)) {
		s->s_count -= S_BIAS-1;
		spin_unlock(&sb_lock);
		down_write(&s->s_umount);
		fs->kill_sb(s);
		put_filesystem(fs);
		put_super(s);
	}
}

/**
 *	grab_super	- acquire an active reference
 *	@s	- reference we are trying to make active
 *
 *	Tries to acquire an active reference.  grab_super() is used when we
 * 	had just found a superblock in super_blocks or fs_type->fs_supers
 *	and want to turn it into a full-blown active reference.  grab_super()
 *	is called with sb_lock held and drops it.  Returns 1 in case of
 *	success, 0 if we had failed (superblock contents was already dead or
 *	dying when grab_super() had been called).
 */
static int grab_super(struct super_block *s)
{
	s->s_count++;
	spin_unlock(&sb_lock);
	down_write(&s->s_umount);
	if (s->s_root) {
		spin_lock(&sb_lock);
		if (s->s_count > S_BIAS) {
			atomic_inc(&s->s_active);
			s->s_count--;
			spin_unlock(&sb_lock);
			return 1;
		}
		spin_unlock(&sb_lock);
	}
	up_write(&s->s_umount);
	put_super(s);
	yield();
	return 0;
}

/**
 *	generic_shutdown_super	-	common helper for ->kill_sb()
 *	@sb: superblock to kill
 *
 *	generic_shutdown_super() does all fs-independent work on superblock
 *	shutdown.  Typical ->kill_sb() should pick all fs-specific objects
 *	that need destruction out of superblock, call generic_shutdown_super()
 *	and release aforementioned objects.  Note: dentries and inodes _are_
 *	taken care of and do not need specific handling.
 */
void generic_shutdown_super(struct super_block *sb)
{
	struct dentry *root = sb->s_root;
	struct super_operations *sop = sb->s_op;

	if (root) {
		sb->s_root = NULL;
		shrink_dcache_parent(root);
		shrink_dcache_anon(&sb->s_anon);
		dput(root);
		fsync_super(sb);
		lock_super(sb);
		lock_kernel();
		sb->s_flags &= ~MS_ACTIVE;
		/* bad name - it should be evict_inodes() */
		invalidate_inodes(sb);
		if (sop) {
			if (sop->write_super && sb->s_dirt)
				sop->write_super(sb);
			if (sop->put_super)
				sop->put_super(sb);
		}

		/* Forget any remaining inodes */
		if (invalidate_inodes(sb)) {
			printk("VFS: Busy inodes after unmount. "
			   "Self-destruct in 5 seconds.  Have a nice day...\n");
		}

		unlock_kernel();
		unlock_super(sb);
	}
	spin_lock(&sb_lock);
	list_del(&sb->s_list);
	list_del(&sb->s_instances);
	spin_unlock(&sb_lock);
	up_write(&sb->s_umount);
}

/**
 *	sget	-	find or create a superblock
 *	@type:	filesystem type superblock should belong to
 *	@test:	comparison callback
 *	@set:	setup callback
 *	@data:	argument to each of them
 */
struct super_block *sget(struct file_system_type *type,
			int (*test)(struct super_block *,void *),
			int (*set)(struct super_block *,void *),
			void *data)
{
	struct super_block *s = alloc_super();
	struct list_head *p;
	int err;

	if (!s)
		return ERR_PTR(-ENOMEM);

retry:
	spin_lock(&sb_lock);
	if (test) list_for_each(p, &type->fs_supers) {
		struct super_block *old;
		old = list_entry(p, struct super_block, s_instances);
		if (!test(old, data))
			continue;
		if (!grab_super(old))
			goto retry;
		destroy_super(s);
		return old;
	}
	err = set(s, data);
	if (err) {
		spin_unlock(&sb_lock);
		destroy_super(s);
		return ERR_PTR(err);
	}
	s->s_type = type;
	list_add(&s->s_list, super_blocks.prev);
	list_add(&s->s_instances, &type->fs_supers);
	spin_unlock(&sb_lock);
	get_filesystem(type);
	return s;
}

struct vfsmount *alloc_vfsmnt(char *name);
void free_vfsmnt(struct vfsmount *mnt);

void drop_super(struct super_block *sb)
{
	up_read(&sb->s_umount);
	put_super(sb);
}

static inline void write_super(struct super_block *sb)
{
	lock_super(sb);
	if (sb->s_root && sb->s_dirt)
		if (sb->s_op && sb->s_op->write_super)
			sb->s_op->write_super(sb);
	unlock_super(sb);
}

/*
 * Note: check the dirty flag before waiting, so we don't
 * hold up the sync while mounting a device. (The newly
 * mounted device won't need syncing.)
 */
void sync_supers(void)
{
	struct super_block * sb;
restart:
	spin_lock(&sb_lock);
	sb = sb_entry(super_blocks.next);
	while (sb != sb_entry(&super_blocks))
		if (sb->s_dirt) {
			sb->s_count++;
			spin_unlock(&sb_lock);
			down_read(&sb->s_umount);
			write_super(sb);
			drop_super(sb);
			goto restart;
		} else
			sb = sb_entry(sb->s_list.next);
	spin_unlock(&sb_lock);
}

/**
 *	get_super	-	get the superblock of a device
 *	@dev: device to get the superblock for
 *	
 *	Scans the superblock list and finds the superblock of the file system
 *	mounted on the device given. %NULL is returned if no match is found.
 */

struct super_block * get_super(struct block_device *bdev)
{
	struct list_head *p;
	if (!bdev)
		return NULL;
rescan:
	spin_lock(&sb_lock);
	list_for_each(p, &super_blocks) {
		struct super_block *s = sb_entry(p);
		if (s->s_bdev == bdev) {
			s->s_count++;
			spin_unlock(&sb_lock);
			down_read(&s->s_umount);
			if (s->s_root)
				return s;
			drop_super(s);
			goto rescan;
		}
	}
	spin_unlock(&sb_lock);
	return NULL;
}
 
struct super_block * user_get_super(dev_t dev)
{
	struct list_head *p;

rescan:
	spin_lock(&sb_lock);
	list_for_each(p, &super_blocks) {
		struct super_block *s = sb_entry(p);
		if (s->s_dev ==  dev) {
			s->s_count++;
			spin_unlock(&sb_lock);
			down_read(&s->s_umount);
			if (s->s_root)
				return s;
			drop_super(s);
			goto rescan;
		}
	}
	spin_unlock(&sb_lock);
	return NULL;
}

asmlinkage long sys_ustat(dev_t dev, struct ustat * ubuf)
{
        struct super_block *s;
        struct ustat tmp;
        struct statfs sbuf;
	int err = -EINVAL;

        s = user_get_super(dev);
        if (s == NULL)
                goto out;
	err = vfs_statfs(s, &sbuf);
	drop_super(s);
	if (err)
		goto out;

        memset(&tmp,0,sizeof(struct ustat));
        tmp.f_tfree = sbuf.f_bfree;
        tmp.f_tinode = sbuf.f_ffree;

        err = copy_to_user(ubuf,&tmp,sizeof(struct ustat)) ? -EFAULT : 0;
out:
	return err;
}

/**
 *	do_remount_sb	-	asks filesystem to change mount options.
 *	@sb:	superblock in question
 *	@flags:	numeric part of options
 *	@data:	the rest of options
 *
 *	Alters the mount options of a mounted file system.
 */
int do_remount_sb(struct super_block *sb, int flags, void *data)
{
	int retval;
	
	if (!(flags & MS_RDONLY) && bdev_read_only(sb->s_bdev))
		return -EACCES;
		/*flags |= MS_RDONLY;*/
	if (flags & MS_RDONLY)
		acct_auto_close(sb);
	shrink_dcache_sb(sb);
	fsync_super(sb);
	/* If we are remounting RDONLY, make sure there are no rw files open */
	if ((flags & MS_RDONLY) && !(sb->s_flags & MS_RDONLY))
		if (!fs_may_remount_ro(sb))
			return -EBUSY;
	if (sb->s_op && sb->s_op->remount_fs) {
		lock_super(sb);
		retval = sb->s_op->remount_fs(sb, &flags, data);
		unlock_super(sb);
		if (retval)
			return retval;
	}
	sb->s_flags = (sb->s_flags & ~MS_RMT_MASK) | (flags & MS_RMT_MASK);
	return 0;
}

/*
 * Unnamed block devices are dummy devices used by virtual
 * filesystems which don't use real block-devices.  -- jrs
 */

enum {Max_anon = 256};
static unsigned long unnamed_dev_in_use[Max_anon/(8*sizeof(unsigned long))];
static spinlock_t unnamed_dev_lock = SPIN_LOCK_UNLOCKED;/* protects the above */

int set_anon_super(struct super_block *s, void *data)
{
	int dev;
	spin_lock(&unnamed_dev_lock);
	dev = find_first_zero_bit(unnamed_dev_in_use, Max_anon);
	if (dev == Max_anon) {
		spin_unlock(&unnamed_dev_lock);
		return -EMFILE;
	}
	set_bit(dev, unnamed_dev_in_use);
	spin_unlock(&unnamed_dev_lock);
	s->s_dev = MKDEV(0, dev);
	return 0;
}

void kill_anon_super(struct super_block *sb)
{
	int slot = MINOR(sb->s_dev);
	generic_shutdown_super(sb);
	spin_lock(&unnamed_dev_lock);
	clear_bit(slot, unnamed_dev_in_use);
	spin_unlock(&unnamed_dev_lock);
}

void kill_litter_super(struct super_block *sb)
{
	if (sb->s_root)
		d_genocide(sb->s_root);
	kill_anon_super(sb);
}

static int set_bdev_super(struct super_block *s, void *data)
{
	s->s_bdev = data;
	s->s_dev = s->s_bdev->bd_dev;
	return 0;
}

static int test_bdev_super(struct super_block *s, void *data)
{
	return (void *)s->s_bdev == data;
}

struct super_block *get_sb_bdev(struct file_system_type *fs_type,
	int flags, char *dev_name, void * data,
	int (*fill_super)(struct super_block *, void *, int))
{
	struct inode *inode;
	struct block_device *bdev;
	struct block_device_operations *bdops;
	devfs_handle_t de;
	struct super_block * s;
	struct nameidata nd;
	kdev_t dev;
	int error = 0;
	mode_t mode = FMODE_READ; /* we always need it ;-) */

	/* What device it is? */
	if (!dev_name || !*dev_name)
		return ERR_PTR(-EINVAL);
	error = path_lookup(dev_name, LOOKUP_FOLLOW, &nd);
	if (error)
		return ERR_PTR(error);
	inode = nd.dentry->d_inode;
	error = -ENOTBLK;
	if (!S_ISBLK(inode->i_mode))
		goto out;
	error = -EACCES;
	if (nd.mnt->mnt_flags & MNT_NODEV)
		goto out;
	error = bd_acquire(inode);
	if (error)
		goto out;
	bdev = inode->i_bdev;
	de = devfs_get_handle_from_inode (inode);
	bdops = devfs_get_ops (de);         /*  Increments module use count  */
	if (bdops) bdev->bd_op = bdops;
	/* Done with lookups, semaphore down */
	dev = to_kdev_t(bdev->bd_dev);
	if (!(flags & MS_RDONLY))
		mode |= FMODE_WRITE;
	error = blkdev_get(bdev, mode, 0, BDEV_FS);
	devfs_put_ops (de);   /*  Decrement module use count now we're safe  */
	if (error)
		goto out;
	check_disk_change(dev);
	error = -EACCES;
	if (!(flags & MS_RDONLY) && bdev_read_only(bdev))
		goto out1;
	error = bd_claim(bdev, fs_type);
	if (error)
		goto out1;

	s = sget(fs_type, test_bdev_super, set_bdev_super, bdev);
	if (IS_ERR(s)) {
		bd_release(bdev);
		blkdev_put(bdev, BDEV_FS);
	} else if (s->s_root) {
		if ((flags ^ s->s_flags) & MS_RDONLY) {
			up_write(&s->s_umount);
			deactivate_super(s);
			s = ERR_PTR(-EBUSY);
		}
		bd_release(bdev);
		blkdev_put(bdev, BDEV_FS);
	} else {
		s->s_flags = flags;
		strncpy(s->s_id, bdevname(bdev), sizeof(s->s_id));
		s->s_old_blocksize = block_size(bdev);
		sb_set_blocksize(s, s->s_old_blocksize);
		error = fill_super(s, data, flags & MS_VERBOSE ? 1 : 0);
		if (error) {
			up_write(&s->s_umount);
			deactivate_super(s);
			s = ERR_PTR(error);
		} else
			s->s_flags |= MS_ACTIVE;
	}
	path_release(&nd);
	return s;

out1:
	blkdev_put(bdev, BDEV_FS);
out:
	path_release(&nd);
	return ERR_PTR(error);
}

void kill_block_super(struct super_block *sb)
{
	struct block_device *bdev = sb->s_bdev;
	generic_shutdown_super(sb);
	set_blocksize(bdev, sb->s_old_blocksize);
	bd_release(bdev);
	blkdev_put(bdev, BDEV_FS);
}

struct super_block *get_sb_nodev(struct file_system_type *fs_type,
	int flags, void *data,
	int (*fill_super)(struct super_block *, void *, int))
{
	int error;
	struct super_block *s = sget(fs_type, NULL, set_anon_super, NULL);

	if (IS_ERR(s))
		return s;

	s->s_flags = flags;

	error = fill_super(s, data, flags & MS_VERBOSE ? 1 : 0);
	if (error) {
		up_write(&s->s_umount);
		deactivate_super(s);
		return ERR_PTR(error);
	}
	s->s_flags |= MS_ACTIVE;
	return s;
}

static int compare_single(struct super_block *s, void *p)
{
	return 1;
}

struct super_block *get_sb_single(struct file_system_type *fs_type,
	int flags, void *data,
	int (*fill_super)(struct super_block *, void *, int))
{
	struct super_block *s;
	int error;

	s = sget(fs_type, compare_single, set_anon_super, NULL);
	if (IS_ERR(s))
		return s;
	if (!s->s_root) {
		s->s_flags = flags;
		error = fill_super(s, data, flags & MS_VERBOSE ? 1 : 0);
		if (error) {
			up_write(&s->s_umount);
			deactivate_super(s);
			return ERR_PTR(error);
		}
		s->s_flags |= MS_ACTIVE;
	}
	do_remount_sb(s, flags, data);
	return s;
}

struct vfsmount *
do_kern_mount(const char *fstype, int flags, char *name, void *data)
{
	struct file_system_type *type = get_fs_type(fstype);
	struct super_block *sb = ERR_PTR(-ENOMEM);
	struct vfsmount *mnt;

	if (!type)
		return ERR_PTR(-ENODEV);

	mnt = alloc_vfsmnt(name);
	if (!mnt)
		goto out;
	sb = type->get_sb(type, flags, name, data);
	if (IS_ERR(sb))
		goto out_mnt;
	mnt->mnt_sb = sb;
	mnt->mnt_root = dget(sb->s_root);
	mnt->mnt_mountpoint = sb->s_root;
	mnt->mnt_parent = mnt;
	up_write(&sb->s_umount);
	put_filesystem(type);
	return mnt;
out_mnt:
	free_vfsmnt(mnt);
out:
	put_filesystem(type);
	return (struct vfsmount *)sb;
}

struct vfsmount *kern_mount(struct file_system_type *type)
{
	return do_kern_mount(type->name, 0, (char *)type->name, NULL);
}
