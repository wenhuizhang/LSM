#ifndef _LINUX_TIMER_H
#define _LINUX_TIMER_H

#include <linux/config.h>
#include <linux/list.h>

/*
 * In Linux 2.4, static timers have been removed from the kernel.
 * Timers may be dynamically created and destroyed, and should be initialized
 * by a call to init_timer() upon creation.
 *
 * The "data" field enables use of a common timeout function for several
 * timeouts. You can use this field to distinguish between the different
 * invocations.
 */
struct timer_list {
	struct list_head list;
	unsigned long expires;
	unsigned long data;
	void (*function)(unsigned long);
};

extern void add_timer(struct timer_list * timer);
extern int del_timer(struct timer_list * timer);

#ifdef CONFIG_SMP
extern int del_timer_sync(struct timer_list * timer);
#else
#define del_timer_sync(t)	del_timer(t)
#endif

/*
 * mod_timer is a more efficient way to update the expire field of an
 * active timer (if the timer is inactive it will be activated)
 * mod_timer(a,b) is equivalent to del_timer(a); a->expires = b; add_timer(a).
 * If the timer is known to be not pending (ie, in the handler), mod_timer
 * is less efficient than a->expires = b; add_timer(a).
 */
int mod_timer(struct timer_list *timer, unsigned long expires);

extern void it_real_fn(unsigned long);

static inline void init_timer(struct timer_list * timer)
{
	timer->list.next = timer->list.prev = NULL;
}

static inline int timer_pending (const struct timer_list * timer)
{
	return timer->list.next != NULL;
}

#endif
