
 

#define KMSG_COMPONENT "IPVS"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/random.h>

#include <net/ip_vs.h>

 
static struct ip_vs_dest *ip_vs_twos_schedule(struct ip_vs_service *svc,
					      const struct sk_buff *skb,
					      struct ip_vs_iphdr *iph)
{
	struct ip_vs_dest *dest, *choice1 = NULL, *choice2 = NULL;
	int rweight1, rweight2, weight1 = -1, weight2 = -1, overhead1 = 0;
	int overhead2, total_weight = 0, weight;

	IP_VS_DBG(6, "%s(): Scheduling...\n", __func__);

	 
	list_for_each_entry_rcu(dest, &svc->destinations, n_list) {
		if (!(dest->flags & IP_VS_DEST_F_OVERLOAD)) {
			weight = atomic_read(&dest->weight);
			if (weight > 0) {
				total_weight += weight;
				choice1 = dest;
			}
		}
	}

	if (!choice1) {
		ip_vs_scheduler_err(svc, "no destination available");
		return NULL;
	}

	 
	total_weight += 1;
	rweight1 = get_random_u32_below(total_weight);
	rweight2 = get_random_u32_below(total_weight);

	 
	list_for_each_entry_rcu(dest, &svc->destinations, n_list) {
		if (dest->flags & IP_VS_DEST_F_OVERLOAD)
			continue;

		weight = atomic_read(&dest->weight);
		if (weight <= 0)
			continue;

		rweight1 -= weight;
		rweight2 -= weight;

		if (rweight1 <= 0 && weight1 == -1) {
			choice1 = dest;
			weight1 = weight;
			overhead1 = ip_vs_dest_conn_overhead(dest);
		}

		if (rweight2 <= 0 && weight2 == -1) {
			choice2 = dest;
			weight2 = weight;
			overhead2 = ip_vs_dest_conn_overhead(dest);
		}

		if (weight1 != -1 && weight2 != -1)
			goto nextstage;
	}

nextstage:
	if (choice2 && (weight2 * overhead1) > (weight1 * overhead2))
		choice1 = choice2;

	IP_VS_DBG_BUF(6, "twos: server %s:%u conns %d refcnt %d weight %d\n",
		      IP_VS_DBG_ADDR(choice1->af, &choice1->addr),
		      ntohs(choice1->port), atomic_read(&choice1->activeconns),
		      refcount_read(&choice1->refcnt),
		      atomic_read(&choice1->weight));

	return choice1;
}

static struct ip_vs_scheduler ip_vs_twos_scheduler = {
	.name = "twos",
	.refcnt = ATOMIC_INIT(0),
	.module = THIS_MODULE,
	.n_list = LIST_HEAD_INIT(ip_vs_twos_scheduler.n_list),
	.schedule = ip_vs_twos_schedule,
};

static int __init ip_vs_twos_init(void)
{
	return register_ip_vs_scheduler(&ip_vs_twos_scheduler);
}

static void __exit ip_vs_twos_cleanup(void)
{
	unregister_ip_vs_scheduler(&ip_vs_twos_scheduler);
	synchronize_rcu();
}

module_init(ip_vs_twos_init);
module_exit(ip_vs_twos_cleanup);
MODULE_LICENSE("GPL");
