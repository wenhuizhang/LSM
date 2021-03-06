#ifndef _LINUX_JIFFIES_H
#define _LINUX_JIFFIES_H

#include <linux/types.h>
#include <asm/param.h>			/* for HZ */

/*
 * The 64-bit value is not volatile - you MUST NOT read it
 * without holding read_lock_irq(&xtime_lock)
 */
extern u64 jiffies_64;
extern unsigned long volatile jiffies;

/*
 *	These inlines deal with timer wrapping correctly. You are 
 *	strongly encouraged to use them
 *	1. Because people otherwise forget
 *	2. Because if the timer wrap changes in future you wont have to
 *	   alter your driver code.
 *
 * time_after(a,b) returns true if the time a is after time b.
 *
 * Do this with "<0" and ">=0" to only test the sign of the result. A
 * good compiler would generate better code (and a really good compiler
 * wouldn't care). Gcc is currently neither.
 */
#define time_after(a,b)		((long)(b) - (long)(a) < 0)
#define time_before(a,b)	time_after(b,a)

#define time_after_eq(a,b)	((long)(a) - (long)(b) >= 0)
#define time_before_eq(a,b)	time_after_eq(b,a)

#endif
