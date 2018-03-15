/*
 *  linux/fs/fat/file.c
 *
 *  Written 1992,1993 by Werner Almesberger
 *
 *  regular file handling primitives for fat-based filesystems
 */

#include <linux/time.h>
#include <linux/msdos_fs.h>
#include <linux/fat_cvf.h>
#include <linux/smp_lock.h>
#include <linux/buffer_head.h>

#define PRINTK(x)
#define Printk(x) printk x

struct file_operations fat_file_operations = {
	llseek:		generic_file_llseek,
	read:		fat_file_read,
	write:		fat_file_write,
	mmap:		generic_file_mmap,
	fsync:		file_fsync,
};

struct inode_operations fat_file_inode_operations = {
	truncate:	fat_truncate,
	setattr:	fat_notify_change,
};

ssize_t fat_file_read(
	struct file *filp,
	char *buf,
	size_t count,
	loff_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	return MSDOS_SB(inode->i_sb)->cvf_format
			->cvf_file_read(filp,buf,count,ppos);
}


int fat_get_block(struct inode *inode, sector_t iblock, struct buffer_head *bh_result, int create)
{
	struct super_block *sb = inode->i_sb;
	unsigned long phys;

	phys = fat_bmap(inode, iblock);
	if (phys < 0)
		return phys;
	if (phys) {
		map_bh(bh_result, sb, phys);
		return 0;
	}
	if (!create)
		return 0;
	if (iblock != MSDOS_I(inode)->mmu_private >> sb->s_blocksize_bits) {
		BUG();
		return -EIO;
	}
	if (!(iblock % MSDOS_SB(sb)->cluster_size)) {
		int error;

		error = fat_add_cluster(inode);
		if (error < 0)
			return error;
	}
	MSDOS_I(inode)->mmu_private += sb->s_blocksize;
	phys = fat_bmap(inode, iblock);
	if (phys < 0)
		return phys;
	if (!phys)
		BUG();
	set_buffer_new(bh_result);
	map_bh(bh_result, sb, phys);
	return 0;
}

ssize_t fat_file_write(
	struct file *filp,
	const char *buf,
	size_t count,
	loff_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	return MSDOS_SB(sb)->cvf_format
			->cvf_file_write(filp,buf,count,ppos);
}

ssize_t default_fat_file_write(
	struct file *filp,
	const char *buf,
	size_t count,
	loff_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	int retval;

	retval = generic_file_write(filp, buf, count, ppos);
	if (retval > 0) {
		inode->i_mtime = inode->i_ctime = CURRENT_TIME;
		MSDOS_I(inode)->i_attrs |= ATTR_ARCH;
		mark_inode_dirty(inode);
	}
	return retval;
}

void fat_truncate(struct inode *inode)
{
	struct msdos_sb_info *sbi = MSDOS_SB(inode->i_sb);
	int cluster;

	/* Why no return value?  Surely the disk could fail... */
	if (IS_RDONLY (inode))
		return /* -EPERM */;
	if (IS_IMMUTABLE(inode))
		return /* -EPERM */;
	cluster = 1 << sbi->cluster_bits;
	/* 
	 * This protects against truncating a file bigger than it was then
	 * trying to write into the hole.
	 */
	if (MSDOS_I(inode)->mmu_private > inode->i_size)
		MSDOS_I(inode)->mmu_private = inode->i_size;

	lock_kernel();
	fat_free(inode, (inode->i_size + (cluster - 1)) >> sbi->cluster_bits);
	MSDOS_I(inode)->i_attrs |= ATTR_ARCH;
	unlock_kernel();
	inode->i_ctime = inode->i_mtime = CURRENT_TIME;
	mark_inode_dirty(inode);
}