
 

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/rcupdate.h>
#include <linux/if_arp.h>
#include <linux/can/can-ml.h>
#include <linux/can/core.h>

#include "af_can.h"

 

#define CAN_PROC_STATS       "stats"
#define CAN_PROC_RESET_STATS "reset_stats"
#define CAN_PROC_RCVLIST_ALL "rcvlist_all"
#define CAN_PROC_RCVLIST_FIL "rcvlist_fil"
#define CAN_PROC_RCVLIST_INV "rcvlist_inv"
#define CAN_PROC_RCVLIST_SFF "rcvlist_sff"
#define CAN_PROC_RCVLIST_EFF "rcvlist_eff"
#define CAN_PROC_RCVLIST_ERR "rcvlist_err"

static int user_reset;

static const char rx_list_name[][8] = {
	[RX_ERR] = "rx_err",
	[RX_ALL] = "rx_all",
	[RX_FIL] = "rx_fil",
	[RX_INV] = "rx_inv",
};

 

static void can_init_stats(struct net *net)
{
	struct can_pkg_stats *pkg_stats = net->can.pkg_stats;
	struct can_rcv_lists_stats *rcv_lists_stats = net->can.rcv_lists_stats;
	 
	memset(pkg_stats, 0, sizeof(struct can_pkg_stats));
	pkg_stats->jiffies_init = jiffies;

	rcv_lists_stats->stats_reset++;

	if (user_reset) {
		user_reset = 0;
		rcv_lists_stats->user_reset++;
	}
}

static unsigned long calc_rate(unsigned long oldjif, unsigned long newjif,
			       unsigned long count)
{
	if (oldjif == newjif)
		return 0;

	 
	if (count > (ULONG_MAX / HZ)) {
		printk(KERN_ERR "can: calc_rate: count exceeded! %ld\n",
		       count);
		return 99999999;
	}

	return (count * HZ) / (newjif - oldjif);
}

void can_stat_update(struct timer_list *t)
{
	struct net *net = from_timer(net, t, can.stattimer);
	struct can_pkg_stats *pkg_stats = net->can.pkg_stats;
	unsigned long j = jiffies;  

	 
	if (user_reset)
		can_init_stats(net);

	 
	if (j < pkg_stats->jiffies_init)
		can_init_stats(net);

	 
	if (pkg_stats->rx_frames > (ULONG_MAX / HZ))
		can_init_stats(net);

	 
	if (pkg_stats->tx_frames > (ULONG_MAX / HZ))
		can_init_stats(net);

	 
	if (pkg_stats->matches > (ULONG_MAX / 100))
		can_init_stats(net);

	 
	if (pkg_stats->rx_frames)
		pkg_stats->total_rx_match_ratio = (pkg_stats->matches * 100) /
			pkg_stats->rx_frames;

	pkg_stats->total_tx_rate = calc_rate(pkg_stats->jiffies_init, j,
					    pkg_stats->tx_frames);
	pkg_stats->total_rx_rate = calc_rate(pkg_stats->jiffies_init, j,
					    pkg_stats->rx_frames);

	 
	if (pkg_stats->rx_frames_delta)
		pkg_stats->current_rx_match_ratio =
			(pkg_stats->matches_delta * 100) /
			pkg_stats->rx_frames_delta;

	pkg_stats->current_tx_rate = calc_rate(0, HZ, pkg_stats->tx_frames_delta);
	pkg_stats->current_rx_rate = calc_rate(0, HZ, pkg_stats->rx_frames_delta);

	 
	if (pkg_stats->max_tx_rate < pkg_stats->current_tx_rate)
		pkg_stats->max_tx_rate = pkg_stats->current_tx_rate;

	if (pkg_stats->max_rx_rate < pkg_stats->current_rx_rate)
		pkg_stats->max_rx_rate = pkg_stats->current_rx_rate;

	if (pkg_stats->max_rx_match_ratio < pkg_stats->current_rx_match_ratio)
		pkg_stats->max_rx_match_ratio = pkg_stats->current_rx_match_ratio;

	 
	pkg_stats->tx_frames_delta = 0;
	pkg_stats->rx_frames_delta = 0;
	pkg_stats->matches_delta   = 0;

	 
	mod_timer(&net->can.stattimer, round_jiffies(jiffies + HZ));
}

 

static void can_print_rcvlist(struct seq_file *m, struct hlist_head *rx_list,
			      struct net_device *dev)
{
	struct receiver *r;

	hlist_for_each_entry_rcu(r, rx_list, list) {
		char *fmt = (r->can_id & CAN_EFF_FLAG)?
			"   %-5s  %08x  %08x  %pK  %pK  %8ld  %s\n" :
			"   %-5s     %03x    %08x  %pK  %pK  %8ld  %s\n";

		seq_printf(m, fmt, DNAME(dev), r->can_id, r->mask,
				r->func, r->data, r->matches, r->ident);
	}
}

static void can_print_recv_banner(struct seq_file *m)
{
	 
	if (IS_ENABLED(CONFIG_64BIT))
		seq_puts(m, "  device   can_id   can_mask      function          userdata       matches  ident\n");
	else
		seq_puts(m, "  device   can_id   can_mask  function  userdata   matches  ident\n");
}

static int can_stats_proc_show(struct seq_file *m, void *v)
{
	struct net *net = m->private;
	struct can_pkg_stats *pkg_stats = net->can.pkg_stats;
	struct can_rcv_lists_stats *rcv_lists_stats = net->can.rcv_lists_stats;

	seq_putc(m, '\n');
	seq_printf(m, " %8ld transmitted frames (TXF)\n", pkg_stats->tx_frames);
	seq_printf(m, " %8ld received frames (RXF)\n", pkg_stats->rx_frames);
	seq_printf(m, " %8ld matched frames (RXMF)\n", pkg_stats->matches);

	seq_putc(m, '\n');

	if (net->can.stattimer.function == can_stat_update) {
		seq_printf(m, " %8ld %% total match ratio (RXMR)\n",
				pkg_stats->total_rx_match_ratio);

		seq_printf(m, " %8ld frames/s total tx rate (TXR)\n",
				pkg_stats->total_tx_rate);
		seq_printf(m, " %8ld frames/s total rx rate (RXR)\n",
				pkg_stats->total_rx_rate);

		seq_putc(m, '\n');

		seq_printf(m, " %8ld %% current match ratio (CRXMR)\n",
				pkg_stats->current_rx_match_ratio);

		seq_printf(m, " %8ld frames/s current tx rate (CTXR)\n",
				pkg_stats->current_tx_rate);
		seq_printf(m, " %8ld frames/s current rx rate (CRXR)\n",
				pkg_stats->current_rx_rate);

		seq_putc(m, '\n');

		seq_printf(m, " %8ld %% max match ratio (MRXMR)\n",
				pkg_stats->max_rx_match_ratio);

		seq_printf(m, " %8ld frames/s max tx rate (MTXR)\n",
				pkg_stats->max_tx_rate);
		seq_printf(m, " %8ld frames/s max rx rate (MRXR)\n",
				pkg_stats->max_rx_rate);

		seq_putc(m, '\n');
	}

	seq_printf(m, " %8ld current receive list entries (CRCV)\n",
			rcv_lists_stats->rcv_entries);
	seq_printf(m, " %8ld maximum receive list entries (MRCV)\n",
			rcv_lists_stats->rcv_entries_max);

	if (rcv_lists_stats->stats_reset)
		seq_printf(m, "\n %8ld statistic resets (STR)\n",
				rcv_lists_stats->stats_reset);

	if (rcv_lists_stats->user_reset)
		seq_printf(m, " %8ld user statistic resets (USTR)\n",
				rcv_lists_stats->user_reset);

	seq_putc(m, '\n');
	return 0;
}

static int can_reset_stats_proc_show(struct seq_file *m, void *v)
{
	struct net *net = m->private;
	struct can_rcv_lists_stats *rcv_lists_stats = net->can.rcv_lists_stats;
	struct can_pkg_stats *pkg_stats = net->can.pkg_stats;

	user_reset = 1;

	if (net->can.stattimer.function == can_stat_update) {
		seq_printf(m, "Scheduled statistic reset #%ld.\n",
				rcv_lists_stats->stats_reset + 1);
	} else {
		if (pkg_stats->jiffies_init != jiffies)
			can_init_stats(net);

		seq_printf(m, "Performed statistic reset #%ld.\n",
				rcv_lists_stats->stats_reset);
	}
	return 0;
}

static inline void can_rcvlist_proc_show_one(struct seq_file *m, int idx,
					     struct net_device *dev,
					     struct can_dev_rcv_lists *dev_rcv_lists)
{
	if (!hlist_empty(&dev_rcv_lists->rx[idx])) {
		can_print_recv_banner(m);
		can_print_rcvlist(m, &dev_rcv_lists->rx[idx], dev);
	} else
		seq_printf(m, "  (%s: no entry)\n", DNAME(dev));

}

static int can_rcvlist_proc_show(struct seq_file *m, void *v)
{
	 
	int idx = (int)(long)pde_data(m->file->f_inode);
	struct net_device *dev;
	struct can_dev_rcv_lists *dev_rcv_lists;
	struct net *net = m->private;

	seq_printf(m, "\nreceive list '%s':\n", rx_list_name[idx]);

	rcu_read_lock();

	 
	dev_rcv_lists = net->can.rx_alldev_list;
	can_rcvlist_proc_show_one(m, idx, NULL, dev_rcv_lists);

	 
	for_each_netdev_rcu(net, dev) {
		struct can_ml_priv *can_ml = can_get_ml_priv(dev);

		if (can_ml)
			can_rcvlist_proc_show_one(m, idx, dev,
						  &can_ml->dev_rcv_lists);
	}

	rcu_read_unlock();

	seq_putc(m, '\n');
	return 0;
}

static inline void can_rcvlist_proc_show_array(struct seq_file *m,
					       struct net_device *dev,
					       struct hlist_head *rcv_array,
					       unsigned int rcv_array_sz)
{
	unsigned int i;
	int all_empty = 1;

	 
	for (i = 0; i < rcv_array_sz; i++)
		if (!hlist_empty(&rcv_array[i])) {
			all_empty = 0;
			break;
		}

	if (!all_empty) {
		can_print_recv_banner(m);
		for (i = 0; i < rcv_array_sz; i++) {
			if (!hlist_empty(&rcv_array[i]))
				can_print_rcvlist(m, &rcv_array[i], dev);
		}
	} else
		seq_printf(m, "  (%s: no entry)\n", DNAME(dev));
}

static int can_rcvlist_sff_proc_show(struct seq_file *m, void *v)
{
	struct net_device *dev;
	struct can_dev_rcv_lists *dev_rcv_lists;
	struct net *net = m->private;

	 
	seq_puts(m, "\nreceive list 'rx_sff':\n");

	rcu_read_lock();

	 
	dev_rcv_lists = net->can.rx_alldev_list;
	can_rcvlist_proc_show_array(m, NULL, dev_rcv_lists->rx_sff,
				    ARRAY_SIZE(dev_rcv_lists->rx_sff));

	 
	for_each_netdev_rcu(net, dev) {
		struct can_ml_priv *can_ml = can_get_ml_priv(dev);

		if (can_ml) {
			dev_rcv_lists = &can_ml->dev_rcv_lists;
			can_rcvlist_proc_show_array(m, dev, dev_rcv_lists->rx_sff,
						    ARRAY_SIZE(dev_rcv_lists->rx_sff));
		}
	}

	rcu_read_unlock();

	seq_putc(m, '\n');
	return 0;
}

static int can_rcvlist_eff_proc_show(struct seq_file *m, void *v)
{
	struct net_device *dev;
	struct can_dev_rcv_lists *dev_rcv_lists;
	struct net *net = m->private;

	 
	seq_puts(m, "\nreceive list 'rx_eff':\n");

	rcu_read_lock();

	 
	dev_rcv_lists = net->can.rx_alldev_list;
	can_rcvlist_proc_show_array(m, NULL, dev_rcv_lists->rx_eff,
				    ARRAY_SIZE(dev_rcv_lists->rx_eff));

	 
	for_each_netdev_rcu(net, dev) {
		struct can_ml_priv *can_ml = can_get_ml_priv(dev);

		if (can_ml) {
			dev_rcv_lists = &can_ml->dev_rcv_lists;
			can_rcvlist_proc_show_array(m, dev, dev_rcv_lists->rx_eff,
						    ARRAY_SIZE(dev_rcv_lists->rx_eff));
		}
	}

	rcu_read_unlock();

	seq_putc(m, '\n');
	return 0;
}

 
void can_init_proc(struct net *net)
{
	 
	net->can.proc_dir = proc_net_mkdir(net, "can", net->proc_net);

	if (!net->can.proc_dir) {
		printk(KERN_INFO "can: failed to create /proc/net/can . "
			   "CONFIG_PROC_FS missing?\n");
		return;
	}

	 
	net->can.pde_stats = proc_create_net_single(CAN_PROC_STATS, 0644,
			net->can.proc_dir, can_stats_proc_show, NULL);
	net->can.pde_reset_stats = proc_create_net_single(CAN_PROC_RESET_STATS,
			0644, net->can.proc_dir, can_reset_stats_proc_show,
			NULL);
	net->can.pde_rcvlist_err = proc_create_net_single(CAN_PROC_RCVLIST_ERR,
			0644, net->can.proc_dir, can_rcvlist_proc_show,
			(void *)RX_ERR);
	net->can.pde_rcvlist_all = proc_create_net_single(CAN_PROC_RCVLIST_ALL,
			0644, net->can.proc_dir, can_rcvlist_proc_show,
			(void *)RX_ALL);
	net->can.pde_rcvlist_fil = proc_create_net_single(CAN_PROC_RCVLIST_FIL,
			0644, net->can.proc_dir, can_rcvlist_proc_show,
			(void *)RX_FIL);
	net->can.pde_rcvlist_inv = proc_create_net_single(CAN_PROC_RCVLIST_INV,
			0644, net->can.proc_dir, can_rcvlist_proc_show,
			(void *)RX_INV);
	net->can.pde_rcvlist_eff = proc_create_net_single(CAN_PROC_RCVLIST_EFF,
			0644, net->can.proc_dir, can_rcvlist_eff_proc_show, NULL);
	net->can.pde_rcvlist_sff = proc_create_net_single(CAN_PROC_RCVLIST_SFF,
			0644, net->can.proc_dir, can_rcvlist_sff_proc_show, NULL);
}

 
void can_remove_proc(struct net *net)
{
	if (!net->can.proc_dir)
		return;

	if (net->can.pde_stats)
		remove_proc_entry(CAN_PROC_STATS, net->can.proc_dir);

	if (net->can.pde_reset_stats)
		remove_proc_entry(CAN_PROC_RESET_STATS, net->can.proc_dir);

	if (net->can.pde_rcvlist_err)
		remove_proc_entry(CAN_PROC_RCVLIST_ERR, net->can.proc_dir);

	if (net->can.pde_rcvlist_all)
		remove_proc_entry(CAN_PROC_RCVLIST_ALL, net->can.proc_dir);

	if (net->can.pde_rcvlist_fil)
		remove_proc_entry(CAN_PROC_RCVLIST_FIL, net->can.proc_dir);

	if (net->can.pde_rcvlist_inv)
		remove_proc_entry(CAN_PROC_RCVLIST_INV, net->can.proc_dir);

	if (net->can.pde_rcvlist_eff)
		remove_proc_entry(CAN_PROC_RCVLIST_EFF, net->can.proc_dir);

	if (net->can.pde_rcvlist_sff)
		remove_proc_entry(CAN_PROC_RCVLIST_SFF, net->can.proc_dir);

	remove_proc_entry("can", net->proc_net);
}
