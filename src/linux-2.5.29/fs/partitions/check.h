#include <linux/pagemap.h>
#include <linux/blkdev.h>

/*
 * add_gd_partition adds a partitions details to the devices partition
 * description.
 */

enum { MAX_PART = 256 };

struct parsed_partitions {
	char name[40];
	struct {
		unsigned long from;
		unsigned long size;
		int flags;
	} parts[MAX_PART];
	int next;
	int limit;
};

static inline void
put_partition(struct parsed_partitions *p, int n, int from, int size)
{
	if (n < p->limit) {
		p->parts[n].from = from;
		p->parts[n].size = size;
		printk(" %s%d", p->name, n);
	}
}

extern int warn_no_part;
