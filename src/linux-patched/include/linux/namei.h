#ifndef _LINUX_NAMEI_H
#define _LINUX_NAMEI_H

#include <linux/linkage.h>

struct vfsmount;

struct nameidata {
	struct dentry	*dentry;
	struct vfsmount *mnt;
	struct qstr	last;
	unsigned int	flags;
	int		last_type;
	struct dentry	*old_dentry;
	struct vfsmount	*old_mnt;
};

/*
 * Type of the last component on LOOKUP_PARENT
 */
enum {LAST_NORM, LAST_ROOT, LAST_DOT, LAST_DOTDOT, LAST_BIND};

/*
 * The bitmask for a lookup event:
 *  - follow links at the end
 *  - require a directory
 *  - ending slashes ok even for nonexistent files
 *  - internal "there are more path compnents" flag
 *  - locked when lookup done with dcache_lock held
 */
#define LOOKUP_FOLLOW		 1
#define LOOKUP_DIRECTORY	 2
#define LOOKUP_CONTINUE		 4
#define LOOKUP_PARENT		16
#define LOOKUP_NOALT		32


extern int FASTCALL(__user_walk(const char *, unsigned, struct nameidata *));
#define user_path_walk(name,nd) \
	__user_walk(name, LOOKUP_FOLLOW, nd)
#define user_path_walk_link(name,nd) \
	__user_walk(name, 0, nd)
extern int FASTCALL(path_lookup(const char *, unsigned, struct nameidata *));
extern int FASTCALL(path_walk(const char *, struct nameidata *));
extern int FASTCALL(link_path_walk(const char *, struct nameidata *));
extern void path_release(struct nameidata *);

extern struct dentry * lookup_one_len(const char *, struct dentry *, int);
extern struct dentry * lookup_hash(struct qstr *, struct dentry *);

extern int follow_down(struct vfsmount **, struct dentry **);
extern int follow_up(struct vfsmount **, struct dentry **);

extern struct dentry *lock_rename(struct dentry *, struct dentry *);
extern void unlock_rename(struct dentry *, struct dentry *);

#endif /* _LINUX_NAMEI_H */
