
 

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include <linux/mISDNif.h>
#include <linux/export.h>
#include "core.h"

static u_int *debug;
static LIST_HEAD(iclock_list);
static DEFINE_RWLOCK(iclock_lock);
static u16 iclock_count;		 
static ktime_t iclock_timestamp;	 
static int iclock_timestamp_valid;	 
static struct mISDNclock *iclock_current;

void
mISDN_init_clock(u_int *dp)
{
	debug = dp;
	iclock_timestamp = ktime_get();
}

static void
select_iclock(void)
{
	struct mISDNclock *iclock, *bestclock = NULL, *lastclock = NULL;
	int pri = -128;

	list_for_each_entry(iclock, &iclock_list, list) {
		if (iclock->pri > pri) {
			pri = iclock->pri;
			bestclock = iclock;
		}
		if (iclock_current == iclock)
			lastclock = iclock;
	}
	if (lastclock && bestclock != lastclock) {
		 
		if (*debug & DEBUG_CLOCK)
			printk(KERN_DEBUG "Old clock source '%s' disable.\n",
			       lastclock->name);
		lastclock->ctl(lastclock->priv, 0);
	}
	if (bestclock && bestclock != iclock_current) {
		 
		if (*debug & DEBUG_CLOCK)
			printk(KERN_DEBUG "New clock source '%s' enable.\n",
			       bestclock->name);
		bestclock->ctl(bestclock->priv, 1);
	}
	if (bestclock != iclock_current) {
		 
		iclock_timestamp_valid = 0;
	}
	iclock_current = bestclock;
}

struct mISDNclock
*mISDN_register_clock(char *name, int pri, clockctl_func_t *ctl, void *priv)
{
	u_long			flags;
	struct mISDNclock	*iclock;

	if (*debug & (DEBUG_CORE | DEBUG_CLOCK))
		printk(KERN_DEBUG "%s: %s %d\n", __func__, name, pri);
	iclock = kzalloc(sizeof(struct mISDNclock), GFP_ATOMIC);
	if (!iclock) {
		printk(KERN_ERR "%s: No memory for clock entry.\n", __func__);
		return NULL;
	}
	strncpy(iclock->name, name, sizeof(iclock->name) - 1);
	iclock->pri = pri;
	iclock->priv = priv;
	iclock->ctl = ctl;
	write_lock_irqsave(&iclock_lock, flags);
	list_add_tail(&iclock->list, &iclock_list);
	select_iclock();
	write_unlock_irqrestore(&iclock_lock, flags);
	return iclock;
}
EXPORT_SYMBOL(mISDN_register_clock);

void
mISDN_unregister_clock(struct mISDNclock *iclock)
{
	u_long	flags;

	if (*debug & (DEBUG_CORE | DEBUG_CLOCK))
		printk(KERN_DEBUG "%s: %s %d\n", __func__, iclock->name,
		       iclock->pri);
	write_lock_irqsave(&iclock_lock, flags);
	if (iclock_current == iclock) {
		if (*debug & DEBUG_CLOCK)
			printk(KERN_DEBUG
			       "Current clock source '%s' unregisters.\n",
			       iclock->name);
		iclock->ctl(iclock->priv, 0);
	}
	list_del(&iclock->list);
	select_iclock();
	write_unlock_irqrestore(&iclock_lock, flags);
}
EXPORT_SYMBOL(mISDN_unregister_clock);

void
mISDN_clock_update(struct mISDNclock *iclock, int samples, ktime_t *timestamp)
{
	u_long		flags;
	ktime_t		timestamp_now;
	u16		delta;

	write_lock_irqsave(&iclock_lock, flags);
	if (iclock_current != iclock) {
		printk(KERN_ERR "%s: '%s' sends us clock updates, but we do "
		       "listen to '%s'. This is a bug!\n", __func__,
		       iclock->name,
		       iclock_current ? iclock_current->name : "nothing");
		iclock->ctl(iclock->priv, 0);
		write_unlock_irqrestore(&iclock_lock, flags);
		return;
	}
	if (iclock_timestamp_valid) {
		 
		iclock_count += samples;
		if (timestamp) {  
			iclock_timestamp = *timestamp;
		} else	{
			iclock_timestamp = ktime_get();
		}
	} else {
		 
		if (timestamp) {  
			timestamp_now = *timestamp;
		} else {
			timestamp_now = ktime_get();
		}
		delta = ktime_divns(ktime_sub(timestamp_now, iclock_timestamp),
				(NSEC_PER_SEC / 8000));
		 
		iclock_count += delta;
		iclock_timestamp = timestamp_now;
		iclock_timestamp_valid = 1;
		if (*debug & DEBUG_CLOCK)
			printk("Received first clock from source '%s'.\n",
			       iclock_current ? iclock_current->name : "nothing");
	}
	write_unlock_irqrestore(&iclock_lock, flags);
}
EXPORT_SYMBOL(mISDN_clock_update);

unsigned short
mISDN_clock_get(void)
{
	u_long		flags;
	ktime_t		timestamp_now;
	u16		delta;
	u16		count;

	read_lock_irqsave(&iclock_lock, flags);
	 
	timestamp_now = ktime_get();
	delta = ktime_divns(ktime_sub(timestamp_now, iclock_timestamp),
			(NSEC_PER_SEC / 8000));
	 
	count =	iclock_count + delta;
	read_unlock_irqrestore(&iclock_lock, flags);
	return count;
}
EXPORT_SYMBOL(mISDN_clock_get);
