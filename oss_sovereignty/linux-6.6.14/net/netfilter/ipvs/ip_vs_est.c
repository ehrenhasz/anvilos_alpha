
 

#define KMSG_COMPONENT "IPVS"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/sysctl.h>
#include <linux/list.h>

#include <net/ip_vs.h>

 

static struct lock_class_key __ipvs_est_key;

static void ip_vs_est_calc_phase(struct netns_ipvs *ipvs);
static void ip_vs_est_drain_temp_list(struct netns_ipvs *ipvs);

static void ip_vs_chain_estimation(struct hlist_head *chain)
{
	struct ip_vs_estimator *e;
	struct ip_vs_cpu_stats *c;
	struct ip_vs_stats *s;
	u64 rate;

	hlist_for_each_entry_rcu(e, chain, list) {
		u64 conns, inpkts, outpkts, inbytes, outbytes;
		u64 kconns = 0, kinpkts = 0, koutpkts = 0;
		u64 kinbytes = 0, koutbytes = 0;
		unsigned int start;
		int i;

		if (kthread_should_stop())
			break;

		s = container_of(e, struct ip_vs_stats, est);
		for_each_possible_cpu(i) {
			c = per_cpu_ptr(s->cpustats, i);
			do {
				start = u64_stats_fetch_begin(&c->syncp);
				conns = u64_stats_read(&c->cnt.conns);
				inpkts = u64_stats_read(&c->cnt.inpkts);
				outpkts = u64_stats_read(&c->cnt.outpkts);
				inbytes = u64_stats_read(&c->cnt.inbytes);
				outbytes = u64_stats_read(&c->cnt.outbytes);
			} while (u64_stats_fetch_retry(&c->syncp, start));
			kconns += conns;
			kinpkts += inpkts;
			koutpkts += outpkts;
			kinbytes += inbytes;
			koutbytes += outbytes;
		}

		spin_lock(&s->lock);

		s->kstats.conns = kconns;
		s->kstats.inpkts = kinpkts;
		s->kstats.outpkts = koutpkts;
		s->kstats.inbytes = kinbytes;
		s->kstats.outbytes = koutbytes;

		 
		rate = (s->kstats.conns - e->last_conns) << 9;
		e->last_conns = s->kstats.conns;
		e->cps += ((s64)rate - (s64)e->cps) >> 2;

		rate = (s->kstats.inpkts - e->last_inpkts) << 9;
		e->last_inpkts = s->kstats.inpkts;
		e->inpps += ((s64)rate - (s64)e->inpps) >> 2;

		rate = (s->kstats.outpkts - e->last_outpkts) << 9;
		e->last_outpkts = s->kstats.outpkts;
		e->outpps += ((s64)rate - (s64)e->outpps) >> 2;

		 
		rate = (s->kstats.inbytes - e->last_inbytes) << 4;
		e->last_inbytes = s->kstats.inbytes;
		e->inbps += ((s64)rate - (s64)e->inbps) >> 2;

		rate = (s->kstats.outbytes - e->last_outbytes) << 4;
		e->last_outbytes = s->kstats.outbytes;
		e->outbps += ((s64)rate - (s64)e->outbps) >> 2;
		spin_unlock(&s->lock);
	}
}

static void ip_vs_tick_estimation(struct ip_vs_est_kt_data *kd, int row)
{
	struct ip_vs_est_tick_data *td;
	int cid;

	rcu_read_lock();
	td = rcu_dereference(kd->ticks[row]);
	if (!td)
		goto out;
	for_each_set_bit(cid, td->present, IPVS_EST_TICK_CHAINS) {
		if (kthread_should_stop())
			break;
		ip_vs_chain_estimation(&td->chains[cid]);
		cond_resched_rcu();
		td = rcu_dereference(kd->ticks[row]);
		if (!td)
			break;
	}

out:
	rcu_read_unlock();
}

static int ip_vs_estimation_kthread(void *data)
{
	struct ip_vs_est_kt_data *kd = data;
	struct netns_ipvs *ipvs = kd->ipvs;
	int row = kd->est_row;
	unsigned long now;
	int id = kd->id;
	long gap;

	if (id > 0) {
		if (!ipvs->est_chain_max)
			return 0;
	} else {
		if (!ipvs->est_chain_max) {
			ipvs->est_calc_phase = 1;
			 
			smp_mb();
		}

		 
		if (ipvs->est_calc_phase)
			ip_vs_est_calc_phase(ipvs);
	}

	while (1) {
		if (!id && !hlist_empty(&ipvs->est_temp_list))
			ip_vs_est_drain_temp_list(ipvs);
		set_current_state(TASK_IDLE);
		if (kthread_should_stop())
			break;

		 
		now = jiffies;
		gap = kd->est_timer - now;
		if (gap > 0) {
			if (gap > IPVS_EST_TICK) {
				kd->est_timer = now - IPVS_EST_TICK;
				gap = IPVS_EST_TICK;
			}
			schedule_timeout(gap);
		} else {
			__set_current_state(TASK_RUNNING);
			if (gap < -8 * IPVS_EST_TICK)
				kd->est_timer = now;
		}

		if (kd->tick_len[row])
			ip_vs_tick_estimation(kd, row);

		row++;
		if (row >= IPVS_EST_NTICKS)
			row = 0;
		WRITE_ONCE(kd->est_row, row);
		kd->est_timer += IPVS_EST_TICK;
	}
	__set_current_state(TASK_RUNNING);

	return 0;
}

 
void ip_vs_est_reload_start(struct netns_ipvs *ipvs)
{
	 
	if (!ipvs->enable)
		return;
	ip_vs_est_stopped_recalc(ipvs);
	 
	atomic_inc(&ipvs->est_genid);
	queue_delayed_work(system_long_wq, &ipvs->est_reload_work, 0);
}

 
int ip_vs_est_kthread_start(struct netns_ipvs *ipvs,
			    struct ip_vs_est_kt_data *kd)
{
	unsigned long now;
	int ret = 0;
	long gap;

	lockdep_assert_held(&ipvs->est_mutex);

	if (kd->task)
		goto out;
	now = jiffies;
	gap = kd->est_timer - now;
	 
	if (abs(gap) > 4 * IPVS_EST_TICK)
		kd->est_timer = now;
	kd->task = kthread_create(ip_vs_estimation_kthread, kd, "ipvs-e:%d:%d",
				  ipvs->gen, kd->id);
	if (IS_ERR(kd->task)) {
		ret = PTR_ERR(kd->task);
		kd->task = NULL;
		goto out;
	}

	set_user_nice(kd->task, sysctl_est_nice(ipvs));
	set_cpus_allowed_ptr(kd->task, sysctl_est_cpulist(ipvs));

	pr_info("starting estimator thread %d...\n", kd->id);
	wake_up_process(kd->task);

out:
	return ret;
}

void ip_vs_est_kthread_stop(struct ip_vs_est_kt_data *kd)
{
	if (kd->task) {
		pr_info("stopping estimator thread %d...\n", kd->id);
		kthread_stop(kd->task);
		kd->task = NULL;
	}
}

 
static void ip_vs_est_set_params(struct netns_ipvs *ipvs,
				 struct ip_vs_est_kt_data *kd)
{
	kd->chain_max = ipvs->est_chain_max;
	 
	if (IPVS_EST_TICK_CHAINS == 1)
		kd->chain_max *= IPVS_EST_CHAIN_FACTOR;
	kd->tick_max = IPVS_EST_TICK_CHAINS * kd->chain_max;
	kd->est_max_count = IPVS_EST_NTICKS * kd->tick_max;
}

 
static int ip_vs_est_add_kthread(struct netns_ipvs *ipvs)
{
	struct ip_vs_est_kt_data *kd = NULL;
	int id = ipvs->est_kt_count;
	int ret = -ENOMEM;
	void *arr = NULL;
	int i;

	if ((unsigned long)ipvs->est_kt_count >= ipvs->est_max_threads &&
	    ipvs->enable && ipvs->est_max_threads)
		return -EINVAL;

	mutex_lock(&ipvs->est_mutex);

	for (i = 0; i < id; i++) {
		if (!ipvs->est_kt_arr[i])
			break;
	}
	if (i >= id) {
		arr = krealloc_array(ipvs->est_kt_arr, id + 1,
				     sizeof(struct ip_vs_est_kt_data *),
				     GFP_KERNEL);
		if (!arr)
			goto out;
		ipvs->est_kt_arr = arr;
	} else {
		id = i;
	}

	kd = kzalloc(sizeof(*kd), GFP_KERNEL);
	if (!kd)
		goto out;
	kd->ipvs = ipvs;
	bitmap_fill(kd->avail, IPVS_EST_NTICKS);
	kd->est_timer = jiffies;
	kd->id = id;
	ip_vs_est_set_params(ipvs, kd);

	 
	if (!id && !kd->calc_stats) {
		kd->calc_stats = ip_vs_stats_alloc();
		if (!kd->calc_stats)
			goto out;
	}

	 
	if (ipvs->enable && !ip_vs_est_stopped(ipvs)) {
		ret = ip_vs_est_kthread_start(ipvs, kd);
		if (ret < 0)
			goto out;
	}

	if (arr)
		ipvs->est_kt_count++;
	ipvs->est_kt_arr[id] = kd;
	kd = NULL;
	 
	ipvs->est_add_ktid = id;
	ret = 0;

out:
	mutex_unlock(&ipvs->est_mutex);
	if (kd) {
		ip_vs_stats_free(kd->calc_stats);
		kfree(kd);
	}

	return ret;
}

 
static void ip_vs_est_update_ktid(struct netns_ipvs *ipvs)
{
	int ktid, best = ipvs->est_kt_count;
	struct ip_vs_est_kt_data *kd;

	for (ktid = 0; ktid < ipvs->est_kt_count; ktid++) {
		kd = ipvs->est_kt_arr[ktid];
		if (kd) {
			if (kd->est_count < kd->est_max_count) {
				best = ktid;
				break;
			}
		} else if (ktid < best) {
			best = ktid;
		}
	}
	ipvs->est_add_ktid = best;
}

 
static int ip_vs_enqueue_estimator(struct netns_ipvs *ipvs,
				   struct ip_vs_estimator *est)
{
	struct ip_vs_est_kt_data *kd = NULL;
	struct ip_vs_est_tick_data *td;
	int ktid, row, crow, cid, ret;
	int delay = est->ktrow;

	BUILD_BUG_ON_MSG(IPVS_EST_TICK_CHAINS > 127,
			 "Too many chains for ktcid");

	if (ipvs->est_add_ktid < ipvs->est_kt_count) {
		kd = ipvs->est_kt_arr[ipvs->est_add_ktid];
		if (kd)
			goto add_est;
	}

	ret = ip_vs_est_add_kthread(ipvs);
	if (ret < 0)
		goto out;
	kd = ipvs->est_kt_arr[ipvs->est_add_ktid];

add_est:
	ktid = kd->id;
	 
	if (kd->est_count >= 2 * kd->tick_max || delay < IPVS_EST_NTICKS - 1)
		crow = READ_ONCE(kd->est_row);
	else
		crow = kd->add_row;
	crow += delay;
	if (crow >= IPVS_EST_NTICKS)
		crow -= IPVS_EST_NTICKS;
	 
	if (delay >= IPVS_EST_NTICKS - 1) {
		 
		row = crow;
		if (crow < IPVS_EST_NTICKS - 1) {
			crow++;
			row = find_last_bit(kd->avail, crow);
		}
		if (row >= crow)
			row = find_last_bit(kd->avail, IPVS_EST_NTICKS);
	} else {
		 
		row = IPVS_EST_NTICKS;
		if (crow > 0)
			row = find_next_bit(kd->avail, IPVS_EST_NTICKS, crow);
		if (row >= IPVS_EST_NTICKS)
			row = find_first_bit(kd->avail, IPVS_EST_NTICKS);
	}

	td = rcu_dereference_protected(kd->ticks[row], 1);
	if (!td) {
		td = kzalloc(sizeof(*td), GFP_KERNEL);
		if (!td) {
			ret = -ENOMEM;
			goto out;
		}
		rcu_assign_pointer(kd->ticks[row], td);
	}

	cid = find_first_zero_bit(td->full, IPVS_EST_TICK_CHAINS);

	kd->est_count++;
	kd->tick_len[row]++;
	if (!td->chain_len[cid])
		__set_bit(cid, td->present);
	td->chain_len[cid]++;
	est->ktid = ktid;
	est->ktrow = row;
	est->ktcid = cid;
	hlist_add_head_rcu(&est->list, &td->chains[cid]);

	if (td->chain_len[cid] >= kd->chain_max) {
		__set_bit(cid, td->full);
		if (kd->tick_len[row] >= kd->tick_max)
			__clear_bit(row, kd->avail);
	}

	 
	if (kd->est_count == kd->est_max_count)
		ip_vs_est_update_ktid(ipvs);

	ret = 0;

out:
	return ret;
}

 
int ip_vs_start_estimator(struct netns_ipvs *ipvs, struct ip_vs_stats *stats)
{
	struct ip_vs_estimator *est = &stats->est;
	int ret;

	if (!ipvs->est_max_threads && ipvs->enable)
		ipvs->est_max_threads = ip_vs_est_max_threads(ipvs);

	est->ktid = -1;
	est->ktrow = IPVS_EST_NTICKS - 1;	 

	 
	ret = 0;
	if (!ipvs->est_kt_count || !ipvs->est_kt_arr[0])
		ret = ip_vs_est_add_kthread(ipvs);
	if (ret >= 0)
		hlist_add_head(&est->list, &ipvs->est_temp_list);
	else
		INIT_HLIST_NODE(&est->list);
	return ret;
}

static void ip_vs_est_kthread_destroy(struct ip_vs_est_kt_data *kd)
{
	if (kd) {
		if (kd->task) {
			pr_info("stop unused estimator thread %d...\n", kd->id);
			kthread_stop(kd->task);
		}
		ip_vs_stats_free(kd->calc_stats);
		kfree(kd);
	}
}

 
void ip_vs_stop_estimator(struct netns_ipvs *ipvs, struct ip_vs_stats *stats)
{
	struct ip_vs_estimator *est = &stats->est;
	struct ip_vs_est_tick_data *td;
	struct ip_vs_est_kt_data *kd;
	int ktid = est->ktid;
	int row = est->ktrow;
	int cid = est->ktcid;

	 
	if (hlist_unhashed(&est->list))
		return;

	 

	 
	if (ktid < 0) {
		hlist_del(&est->list);
		goto end_kt0;
	}

	hlist_del_rcu(&est->list);
	kd = ipvs->est_kt_arr[ktid];
	td = rcu_dereference_protected(kd->ticks[row], 1);
	__clear_bit(cid, td->full);
	td->chain_len[cid]--;
	if (!td->chain_len[cid])
		__clear_bit(cid, td->present);
	kd->tick_len[row]--;
	__set_bit(row, kd->avail);
	if (!kd->tick_len[row]) {
		RCU_INIT_POINTER(kd->ticks[row], NULL);
		kfree_rcu(td, rcu_head);
	}
	kd->est_count--;
	if (kd->est_count) {
		 
		if (ktid < ipvs->est_add_ktid)
			ipvs->est_add_ktid = ktid;
		return;
	}

	if (ktid > 0) {
		mutex_lock(&ipvs->est_mutex);
		ip_vs_est_kthread_destroy(kd);
		ipvs->est_kt_arr[ktid] = NULL;
		if (ktid == ipvs->est_kt_count - 1) {
			ipvs->est_kt_count--;
			while (ipvs->est_kt_count > 1 &&
			       !ipvs->est_kt_arr[ipvs->est_kt_count - 1])
				ipvs->est_kt_count--;
		}
		mutex_unlock(&ipvs->est_mutex);

		 
		if (ktid == ipvs->est_add_ktid)
			ip_vs_est_update_ktid(ipvs);
	}

end_kt0:
	 
	if (ipvs->est_kt_count == 1 && hlist_empty(&ipvs->est_temp_list)) {
		kd = ipvs->est_kt_arr[0];
		if (!kd || !kd->est_count) {
			mutex_lock(&ipvs->est_mutex);
			if (kd) {
				ip_vs_est_kthread_destroy(kd);
				ipvs->est_kt_arr[0] = NULL;
			}
			ipvs->est_kt_count--;
			mutex_unlock(&ipvs->est_mutex);
			ipvs->est_add_ktid = 0;
		}
	}
}

 
static void ip_vs_est_drain_temp_list(struct netns_ipvs *ipvs)
{
	struct ip_vs_estimator *est;

	while (1) {
		int max = 16;

		mutex_lock(&__ip_vs_mutex);

		while (max-- > 0) {
			est = hlist_entry_safe(ipvs->est_temp_list.first,
					       struct ip_vs_estimator, list);
			if (est) {
				if (kthread_should_stop())
					goto unlock;
				hlist_del_init(&est->list);
				if (ip_vs_enqueue_estimator(ipvs, est) >= 0)
					continue;
				est->ktid = -1;
				hlist_add_head(&est->list,
					       &ipvs->est_temp_list);
				 
			}
			goto unlock;
		}
		mutex_unlock(&__ip_vs_mutex);
		cond_resched();
	}

unlock:
	mutex_unlock(&__ip_vs_mutex);
}

 
static int ip_vs_est_calc_limits(struct netns_ipvs *ipvs, int *chain_max)
{
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(wq);
	struct ip_vs_est_kt_data *kd;
	struct hlist_head chain;
	struct ip_vs_stats *s;
	int cache_factor = 4;
	int i, loops, ntest;
	s32 min_est = 0;
	ktime_t t1, t2;
	int max = 8;
	int ret = 1;
	s64 diff;
	u64 val;

	INIT_HLIST_HEAD(&chain);
	mutex_lock(&__ip_vs_mutex);
	kd = ipvs->est_kt_arr[0];
	mutex_unlock(&__ip_vs_mutex);
	s = kd ? kd->calc_stats : NULL;
	if (!s)
		goto out;
	hlist_add_head(&s->est.list, &chain);

	loops = 1;
	 
	for (ntest = 0; ntest < 12; ntest++) {
		if (!(ntest & 3)) {
			 
			wait_event_idle_timeout(wq, kthread_should_stop(),
						HZ / 50);
			if (!ipvs->enable || kthread_should_stop())
				goto stop;
		}

		local_bh_disable();
		rcu_read_lock();

		 
		ip_vs_chain_estimation(&chain);

		t1 = ktime_get();
		for (i = loops * cache_factor; i > 0; i--)
			ip_vs_chain_estimation(&chain);
		t2 = ktime_get();

		rcu_read_unlock();
		local_bh_enable();

		if (!ipvs->enable || kthread_should_stop())
			goto stop;
		cond_resched();

		diff = ktime_to_ns(ktime_sub(t2, t1));
		if (diff <= 1 * NSEC_PER_USEC) {
			 
			loops *= 2;
			continue;
		}
		if (diff >= NSEC_PER_SEC)
			continue;
		val = diff;
		do_div(val, loops);
		if (!min_est || val < min_est) {
			min_est = val;
			 
			val = 95 * NSEC_PER_USEC;
			if (val >= min_est) {
				do_div(val, min_est);
				max = (int)val;
			} else {
				max = 1;
			}
		}
	}

out:
	if (s)
		hlist_del_init(&s->est.list);
	*chain_max = max;
	return ret;

stop:
	ret = 0;
	goto out;
}

 
static void ip_vs_est_calc_phase(struct netns_ipvs *ipvs)
{
	int genid = atomic_read(&ipvs->est_genid);
	struct ip_vs_est_tick_data *td;
	struct ip_vs_est_kt_data *kd;
	struct ip_vs_estimator *est;
	struct ip_vs_stats *stats;
	int id, row, cid, delay;
	bool last, last_td;
	int chain_max;
	int step;

	if (!ip_vs_est_calc_limits(ipvs, &chain_max))
		return;

	mutex_lock(&__ip_vs_mutex);

	 
	mutex_lock(&ipvs->est_mutex);
	for (id = 1; id < ipvs->est_kt_count; id++) {
		 
		if (!ipvs->enable)
			goto unlock2;
		kd = ipvs->est_kt_arr[id];
		if (!kd)
			continue;
		ip_vs_est_kthread_stop(kd);
	}
	mutex_unlock(&ipvs->est_mutex);

	 
	step = 0;

	 
	delay = IPVS_EST_NTICKS;

next_delay:
	delay--;
	if (delay < 0)
		goto end_dequeue;

last_kt:
	 
	id = ipvs->est_kt_count;

next_kt:
	if (!ipvs->enable || kthread_should_stop())
		goto unlock;
	id--;
	if (id < 0)
		goto next_delay;
	kd = ipvs->est_kt_arr[id];
	if (!kd)
		goto next_kt;
	 
	if (!id && kd->est_count <= 1)
		goto next_delay;

	row = kd->est_row + delay;
	if (row >= IPVS_EST_NTICKS)
		row -= IPVS_EST_NTICKS;
	td = rcu_dereference_protected(kd->ticks[row], 1);
	if (!td)
		goto next_kt;

	cid = 0;

walk_chain:
	if (kthread_should_stop())
		goto unlock;
	step++;
	if (!(step & 63)) {
		 
		mutex_unlock(&__ip_vs_mutex);
		cond_resched();
		mutex_lock(&__ip_vs_mutex);

		 
		if (id >= ipvs->est_kt_count)
			goto last_kt;
		if (kd != ipvs->est_kt_arr[id])
			goto next_kt;
		 
		if (td != rcu_dereference_protected(kd->ticks[row], 1))
			goto next_kt;
		 
	}
	est = hlist_entry_safe(td->chains[cid].first, struct ip_vs_estimator,
			       list);
	if (!est) {
		cid++;
		if (cid >= IPVS_EST_TICK_CHAINS)
			goto next_kt;
		goto walk_chain;
	}
	 
	last = kd->est_count <= 1;
	 
	if (!id && last)
		goto next_delay;
	last_td = kd->tick_len[row] <= 1;
	stats = container_of(est, struct ip_vs_stats, est);
	ip_vs_stop_estimator(ipvs, stats);
	 
	est->ktid = -1;
	est->ktrow = row - kd->est_row;
	if (est->ktrow < 0)
		est->ktrow += IPVS_EST_NTICKS;
	hlist_add_head(&est->list, &ipvs->est_temp_list);
	 
	if (last)
		goto next_kt;
	 
	if (last_td)
		goto next_kt;
	goto walk_chain;

end_dequeue:
	 
	if (!ipvs->est_kt_count)
		goto unlock;
	kd = ipvs->est_kt_arr[0];
	if (!kd)
		goto unlock;
	kd->add_row = kd->est_row;
	ipvs->est_chain_max = chain_max;
	ip_vs_est_set_params(ipvs, kd);

	pr_info("using max %d ests per chain, %d per kthread\n",
		kd->chain_max, kd->est_max_count);

	 
	if (ipvs->tot_stats && !hlist_unhashed(&ipvs->tot_stats->s.est.list) &&
	    ipvs->tot_stats->s.est.ktid == -1) {
		hlist_del(&ipvs->tot_stats->s.est.list);
		hlist_add_head(&ipvs->tot_stats->s.est.list,
			       &ipvs->est_temp_list);
	}

	mutex_lock(&ipvs->est_mutex);

	 
	if (genid == atomic_read(&ipvs->est_genid))
		ipvs->est_calc_phase = 0;

unlock2:
	mutex_unlock(&ipvs->est_mutex);

unlock:
	mutex_unlock(&__ip_vs_mutex);
}

void ip_vs_zero_estimator(struct ip_vs_stats *stats)
{
	struct ip_vs_estimator *est = &stats->est;
	struct ip_vs_kstats *k = &stats->kstats;

	 
	est->last_inbytes = k->inbytes;
	est->last_outbytes = k->outbytes;
	est->last_conns = k->conns;
	est->last_inpkts = k->inpkts;
	est->last_outpkts = k->outpkts;
	est->cps = 0;
	est->inpps = 0;
	est->outpps = 0;
	est->inbps = 0;
	est->outbps = 0;
}

 
void ip_vs_read_estimator(struct ip_vs_kstats *dst, struct ip_vs_stats *stats)
{
	struct ip_vs_estimator *e = &stats->est;

	dst->cps = (e->cps + 0x1FF) >> 10;
	dst->inpps = (e->inpps + 0x1FF) >> 10;
	dst->outpps = (e->outpps + 0x1FF) >> 10;
	dst->inbps = (e->inbps + 0xF) >> 5;
	dst->outbps = (e->outbps + 0xF) >> 5;
}

int __net_init ip_vs_estimator_net_init(struct netns_ipvs *ipvs)
{
	INIT_HLIST_HEAD(&ipvs->est_temp_list);
	ipvs->est_kt_arr = NULL;
	ipvs->est_max_threads = 0;
	ipvs->est_calc_phase = 0;
	ipvs->est_chain_max = 0;
	ipvs->est_kt_count = 0;
	ipvs->est_add_ktid = 0;
	atomic_set(&ipvs->est_genid, 0);
	atomic_set(&ipvs->est_genid_done, 0);
	__mutex_init(&ipvs->est_mutex, "ipvs->est_mutex", &__ipvs_est_key);
	return 0;
}

void __net_exit ip_vs_estimator_net_cleanup(struct netns_ipvs *ipvs)
{
	int i;

	for (i = 0; i < ipvs->est_kt_count; i++)
		ip_vs_est_kthread_destroy(ipvs->est_kt_arr[i]);
	kfree(ipvs->est_kt_arr);
	mutex_destroy(&ipvs->est_mutex);
}
