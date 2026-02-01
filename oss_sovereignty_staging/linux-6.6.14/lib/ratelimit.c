
 

#include <linux/ratelimit.h>
#include <linux/jiffies.h>
#include <linux/export.h>

 
int ___ratelimit(struct ratelimit_state *rs, const char *func)
{
	 
	int interval = READ_ONCE(rs->interval);
	int burst = READ_ONCE(rs->burst);
	unsigned long flags;
	int ret;

	if (!interval)
		return 1;

	 
	if (!raw_spin_trylock_irqsave(&rs->lock, flags))
		return 0;

	if (!rs->begin)
		rs->begin = jiffies;

	if (time_is_before_jiffies(rs->begin + interval)) {
		if (rs->missed) {
			if (!(rs->flags & RATELIMIT_MSG_ON_RELEASE)) {
				printk_deferred(KERN_WARNING
						"%s: %d callbacks suppressed\n",
						func, rs->missed);
				rs->missed = 0;
			}
		}
		rs->begin   = jiffies;
		rs->printed = 0;
	}
	if (burst && burst > rs->printed) {
		rs->printed++;
		ret = 1;
	} else {
		rs->missed++;
		ret = 0;
	}
	raw_spin_unlock_irqrestore(&rs->lock, flags);

	return ret;
}
EXPORT_SYMBOL(___ratelimit);
