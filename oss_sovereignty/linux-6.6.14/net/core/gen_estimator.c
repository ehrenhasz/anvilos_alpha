
 

#include <linux/uaccess.h>
#include <linux/bitops.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/in.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/seqlock.h>
#include <net/sock.h>
#include <net/gen_stats.h>

 

struct net_rate_estimator {
	struct gnet_stats_basic_sync	*bstats;
	spinlock_t		*stats_lock;
	bool			running;
	struct gnet_stats_basic_sync __percpu *cpu_bstats;
	u8			ewma_log;
	u8			intvl_log;  

	seqcount_t		seq;
	u64			last_packets;
	u64			last_bytes;

	u64			avpps;
	u64			avbps;

	unsigned long           next_jiffies;
	struct timer_list       timer;
	struct rcu_head		rcu;
};

static void est_fetch_counters(struct net_rate_estimator *e,
			       struct gnet_stats_basic_sync *b)
{
	gnet_stats_basic_sync_init(b);
	if (e->stats_lock)
		spin_lock(e->stats_lock);

	gnet_stats_add_basic(b, e->cpu_bstats, e->bstats, e->running);

	if (e->stats_lock)
		spin_unlock(e->stats_lock);

}

static void est_timer(struct timer_list *t)
{
	struct net_rate_estimator *est = from_timer(est, t, timer);
	struct gnet_stats_basic_sync b;
	u64 b_bytes, b_packets;
	u64 rate, brate;

	est_fetch_counters(est, &b);
	b_bytes = u64_stats_read(&b.bytes);
	b_packets = u64_stats_read(&b.packets);

	brate = (b_bytes - est->last_bytes) << (10 - est->intvl_log);
	brate = (brate >> est->ewma_log) - (est->avbps >> est->ewma_log);

	rate = (b_packets - est->last_packets) << (10 - est->intvl_log);
	rate = (rate >> est->ewma_log) - (est->avpps >> est->ewma_log);

	write_seqcount_begin(&est->seq);
	est->avbps += brate;
	est->avpps += rate;
	write_seqcount_end(&est->seq);

	est->last_bytes = b_bytes;
	est->last_packets = b_packets;

	est->next_jiffies += ((HZ/4) << est->intvl_log);

	if (unlikely(time_after_eq(jiffies, est->next_jiffies))) {
		 
		est->next_jiffies = jiffies + 1;
	}
	mod_timer(&est->timer, est->next_jiffies);
}

 
int gen_new_estimator(struct gnet_stats_basic_sync *bstats,
		      struct gnet_stats_basic_sync __percpu *cpu_bstats,
		      struct net_rate_estimator __rcu **rate_est,
		      spinlock_t *lock,
		      bool running,
		      struct nlattr *opt)
{
	struct gnet_estimator *parm = nla_data(opt);
	struct net_rate_estimator *old, *est;
	struct gnet_stats_basic_sync b;
	int intvl_log;

	if (nla_len(opt) < sizeof(*parm))
		return -EINVAL;

	 
	if (parm->interval < -2 || parm->interval > 3)
		return -EINVAL;

	if (parm->ewma_log == 0 || parm->ewma_log >= 31)
		return -EINVAL;

	est = kzalloc(sizeof(*est), GFP_KERNEL);
	if (!est)
		return -ENOBUFS;

	seqcount_init(&est->seq);
	intvl_log = parm->interval + 2;
	est->bstats = bstats;
	est->stats_lock = lock;
	est->running  = running;
	est->ewma_log = parm->ewma_log;
	est->intvl_log = intvl_log;
	est->cpu_bstats = cpu_bstats;

	if (lock)
		local_bh_disable();
	est_fetch_counters(est, &b);
	if (lock)
		local_bh_enable();
	est->last_bytes = u64_stats_read(&b.bytes);
	est->last_packets = u64_stats_read(&b.packets);

	if (lock)
		spin_lock_bh(lock);
	old = rcu_dereference_protected(*rate_est, 1);
	if (old) {
		del_timer_sync(&old->timer);
		est->avbps = old->avbps;
		est->avpps = old->avpps;
	}

	est->next_jiffies = jiffies + ((HZ/4) << intvl_log);
	timer_setup(&est->timer, est_timer, 0);
	mod_timer(&est->timer, est->next_jiffies);

	rcu_assign_pointer(*rate_est, est);
	if (lock)
		spin_unlock_bh(lock);
	if (old)
		kfree_rcu(old, rcu);
	return 0;
}
EXPORT_SYMBOL(gen_new_estimator);

 
void gen_kill_estimator(struct net_rate_estimator __rcu **rate_est)
{
	struct net_rate_estimator *est;

	est = xchg((__force struct net_rate_estimator **)rate_est, NULL);
	if (est) {
		timer_shutdown_sync(&est->timer);
		kfree_rcu(est, rcu);
	}
}
EXPORT_SYMBOL(gen_kill_estimator);

 
int gen_replace_estimator(struct gnet_stats_basic_sync *bstats,
			  struct gnet_stats_basic_sync __percpu *cpu_bstats,
			  struct net_rate_estimator __rcu **rate_est,
			  spinlock_t *lock,
			  bool running, struct nlattr *opt)
{
	return gen_new_estimator(bstats, cpu_bstats, rate_est,
				 lock, running, opt);
}
EXPORT_SYMBOL(gen_replace_estimator);

 
bool gen_estimator_active(struct net_rate_estimator __rcu **rate_est)
{
	return !!rcu_access_pointer(*rate_est);
}
EXPORT_SYMBOL(gen_estimator_active);

bool gen_estimator_read(struct net_rate_estimator __rcu **rate_est,
			struct gnet_stats_rate_est64 *sample)
{
	struct net_rate_estimator *est;
	unsigned seq;

	rcu_read_lock();
	est = rcu_dereference(*rate_est);
	if (!est) {
		rcu_read_unlock();
		return false;
	}

	do {
		seq = read_seqcount_begin(&est->seq);
		sample->bps = est->avbps >> 8;
		sample->pps = est->avpps >> 8;
	} while (read_seqcount_retry(&est->seq, seq));

	rcu_read_unlock();
	return true;
}
EXPORT_SYMBOL(gen_estimator_read);
