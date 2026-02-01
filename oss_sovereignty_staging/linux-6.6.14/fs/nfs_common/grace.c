
 

#include <linux/module.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>
#include <linux/fs.h>
#include <linux/filelock.h>

static unsigned int grace_net_id;
static DEFINE_SPINLOCK(grace_lock);

 
void
locks_start_grace(struct net *net, struct lock_manager *lm)
{
	struct list_head *grace_list = net_generic(net, grace_net_id);

	spin_lock(&grace_lock);
	if (list_empty(&lm->list))
		list_add(&lm->list, grace_list);
	else
		WARN(1, "double list_add attempt detected in net %x %s\n",
		     net->ns.inum, (net == &init_net) ? "(init_net)" : "");
	spin_unlock(&grace_lock);
}
EXPORT_SYMBOL_GPL(locks_start_grace);

 
void
locks_end_grace(struct lock_manager *lm)
{
	spin_lock(&grace_lock);
	list_del_init(&lm->list);
	spin_unlock(&grace_lock);
}
EXPORT_SYMBOL_GPL(locks_end_grace);

static bool
__state_in_grace(struct net *net, bool open)
{
	struct list_head *grace_list = net_generic(net, grace_net_id);
	struct lock_manager *lm;

	if (!open)
		return !list_empty(grace_list);

	spin_lock(&grace_lock);
	list_for_each_entry(lm, grace_list, list) {
		if (lm->block_opens) {
			spin_unlock(&grace_lock);
			return true;
		}
	}
	spin_unlock(&grace_lock);
	return false;
}

 
bool locks_in_grace(struct net *net)
{
	return __state_in_grace(net, false);
}
EXPORT_SYMBOL_GPL(locks_in_grace);

bool opens_in_grace(struct net *net)
{
	return __state_in_grace(net, true);
}
EXPORT_SYMBOL_GPL(opens_in_grace);

static int __net_init
grace_init_net(struct net *net)
{
	struct list_head *grace_list = net_generic(net, grace_net_id);

	INIT_LIST_HEAD(grace_list);
	return 0;
}

static void __net_exit
grace_exit_net(struct net *net)
{
	struct list_head *grace_list = net_generic(net, grace_net_id);

	WARN_ONCE(!list_empty(grace_list),
		  "net %x %s: grace_list is not empty\n",
		  net->ns.inum, __func__);
}

static struct pernet_operations grace_net_ops = {
	.init = grace_init_net,
	.exit = grace_exit_net,
	.id   = &grace_net_id,
	.size = sizeof(struct list_head),
};

static int __init
init_grace(void)
{
	return register_pernet_subsys(&grace_net_ops);
}

static void __exit
exit_grace(void)
{
	unregister_pernet_subsys(&grace_net_ops);
}

MODULE_AUTHOR("Jeff Layton <jlayton@primarydata.com>");
MODULE_LICENSE("GPL");
module_init(init_grace)
module_exit(exit_grace)
