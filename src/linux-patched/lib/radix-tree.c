/*
 * Copyright (C) 2001 Momchil Velikov
 * Portions Copyright (C) 2001 Christoph Hellwig
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mempool.h>
#include <linux/module.h>
#include <linux/radix-tree.h>
#include <linux/slab.h>
#include <linux/string.h>

/*
 * Radix tree node definition.
 */
#define RADIX_TREE_MAP_SHIFT  6
#define RADIX_TREE_MAP_SIZE  (1UL << RADIX_TREE_MAP_SHIFT)
#define RADIX_TREE_MAP_MASK  (RADIX_TREE_MAP_SIZE-1)

struct radix_tree_node {
	unsigned int	count;
	void		*slots[RADIX_TREE_MAP_SIZE];
};

struct radix_tree_path {
	struct radix_tree_node *node, **slot;
};

#define RADIX_TREE_INDEX_BITS  (8 /* CHAR_BIT */ * sizeof(unsigned long))

/*
 * Radix tree node cache.
 */
static kmem_cache_t *radix_tree_node_cachep;
static mempool_t *radix_tree_node_pool;

static inline struct radix_tree_node *
radix_tree_node_alloc(struct radix_tree_root *root)
{
	return mempool_alloc(radix_tree_node_pool, root->gfp_mask);
}

static inline void
radix_tree_node_free(struct radix_tree_node *node)
{
	mempool_free(node, radix_tree_node_pool);
}

/*
 *	Return the maximum key which can be store into a
 *	radix tree with height HEIGHT.
 */
static inline unsigned long radix_tree_maxindex(unsigned int height)
{
	unsigned int tmp = height * RADIX_TREE_MAP_SHIFT;
	unsigned long index = (~0UL >> (RADIX_TREE_INDEX_BITS - tmp - 1)) >> 1;

	if (tmp >= RADIX_TREE_INDEX_BITS)
		index = ~0UL;
	return index;
}


/*
 *	Extend a radix tree so it can store key @index.
 */
static int radix_tree_extend(struct radix_tree_root *root, unsigned long index)
{
	struct radix_tree_node *node;
	unsigned int height;

	/* Figure out what the height should be.  */
	height = root->height + 1;
	while (index > radix_tree_maxindex(height))
		height++;

	if (root->rnode) {
		do {
			if (!(node = radix_tree_node_alloc(root)))
				return -ENOMEM;

			/* Increase the height.  */
			node->slots[0] = root->rnode;
			if (root->rnode)
				node->count = 1;
			root->rnode = node;
			root->height++;
		} while (height > root->height);
	} else 
		root->height = height;

	return 0;
}


/**
 *	radix_tree_reserve    -    reserve space in a radix tree
 *	@root:		radix tree root
 *	@index:		index key
 *	@pslot:		pointer to reserved slot
 *
 *	Reserve a slot in a radix tree for the key @index.
 */
int radix_tree_reserve(struct radix_tree_root *root, unsigned long index, void ***pslot)
{
	struct radix_tree_node *node = NULL, *tmp, **slot;
	unsigned int height, shift;
	int error;

	/* Make sure the tree is high enough.  */
	if (index > radix_tree_maxindex(root->height)) {
		error = radix_tree_extend(root, index);
		if (error)
			return error;
	}
    
	slot = &root->rnode;
	height = root->height;
	shift = (height-1) * RADIX_TREE_MAP_SHIFT;

	while (height > 0) {
		if (*slot == NULL) {
			/* Have to add a child node.  */
			if (!(tmp = radix_tree_node_alloc(root)))
				return -ENOMEM;
			*slot = tmp;
			if (node)
				node->count++;
		}

		/* Go a level down.  */
		node = *slot;
		slot = (struct radix_tree_node **)
			(node->slots + ((index >> shift) & RADIX_TREE_MAP_MASK));
		shift -= RADIX_TREE_MAP_SHIFT;
		height--;
	}

	if (*slot != NULL)
		return -EEXIST;
	if (node)
		node->count++;

	*pslot = (void **)slot;
	**pslot = RADIX_TREE_SLOT_RESERVED;
	return 0;
}

EXPORT_SYMBOL(radix_tree_reserve);


/**
 *	radix_tree_insert    -    insert into a radix tree
 *	@root:		radix tree root
 *	@index:		index key
 *	@item:		item to insert
 *
 *	Insert an item into the radix tree at position @index.
 */
int radix_tree_insert(struct radix_tree_root *root, unsigned long index, void *item)
{
	void **slot;
	int error;

	error = radix_tree_reserve(root, index, &slot);
	if (!error)
		*slot = item;
	return error;
}

EXPORT_SYMBOL(radix_tree_insert);


/**
 *	radix_tree_lookup    -    perform lookup operation on a radix tree
 *	@root:		radix tree root
 *	@index:		index key
 *
 *	Lookup them item at the position @index in the radix tree @root.
 */
void *radix_tree_lookup(struct radix_tree_root *root, unsigned long index)
{
	unsigned int height, shift;
	struct radix_tree_node **slot;

	height = root->height;
	if (index > radix_tree_maxindex(height))
		return NULL;

	shift = (height-1) * RADIX_TREE_MAP_SHIFT;
	slot = &root->rnode;

	while (height > 0) {
		if (*slot == NULL)
			return NULL;

		slot = (struct radix_tree_node **)
			((*slot)->slots + ((index >> shift) & RADIX_TREE_MAP_MASK));
		shift -= RADIX_TREE_MAP_SHIFT;
		height--;
	}

	return (void *) *slot;
}

EXPORT_SYMBOL(radix_tree_lookup);


/**
 *	radix_tree_delete    -    delete an item from a radix tree
 *	@root:		radix tree root
 *	@index:		index key
 *
 *	Remove the item at @index from the radix tree rooted at @root.
 */
int radix_tree_delete(struct radix_tree_root *root, unsigned long index)
{
	struct radix_tree_path path[RADIX_TREE_INDEX_BITS/RADIX_TREE_MAP_SHIFT + 2], *pathp = path;
	unsigned int height, shift;

	height = root->height;
	if (index > radix_tree_maxindex(height))
		return -ENOENT;

	shift = (height-1) * RADIX_TREE_MAP_SHIFT;
	pathp->node = NULL;
	pathp->slot = &root->rnode;

	while (height > 0) {
		if (*pathp->slot == NULL)
			return -ENOENT;

		pathp[1].node = *pathp[0].slot;
		pathp[1].slot = (struct radix_tree_node **)
		    (pathp[1].node->slots + ((index >> shift) & RADIX_TREE_MAP_MASK));
		pathp++;
		shift -= RADIX_TREE_MAP_SHIFT;
		height--;
	}

	if (*pathp[0].slot == NULL)
		return -ENOENT;

	*pathp[0].slot = NULL;
	while (pathp[0].node && --pathp[0].node->count == 0) {
		pathp--;
		*pathp[0].slot = NULL;
		radix_tree_node_free(pathp[1].node);
	}

	return 0;
}

EXPORT_SYMBOL(radix_tree_delete);

static void radix_tree_node_ctor(void *node, kmem_cache_t *cachep, unsigned long flags)
{
	memset(node, 0, sizeof(struct radix_tree_node));
}

static void *radix_tree_node_pool_alloc(int gfp_mask, void *data)
{
	return kmem_cache_alloc(radix_tree_node_cachep, gfp_mask);
}

static void radix_tree_node_pool_free(void *node, void *data)
{
	kmem_cache_free(radix_tree_node_cachep, node);
}

/*
 * FIXME!  512 nodes is 200-300k of memory.  This needs to be
 * scaled by the amount of available memory, and hopefully
 * reduced also.
 */
void __init radix_tree_init(void)
{
	radix_tree_node_cachep = kmem_cache_create("radix_tree_node",
			sizeof(struct radix_tree_node), 0,
			SLAB_HWCACHE_ALIGN, radix_tree_node_ctor, NULL);
	if (!radix_tree_node_cachep)
		panic ("Failed to create radix_tree_node cache\n");
	radix_tree_node_pool = mempool_create(512, radix_tree_node_pool_alloc,
			radix_tree_node_pool_free, NULL);
	if (!radix_tree_node_pool)
		panic ("Failed to create radix_tree_node pool\n");
}
