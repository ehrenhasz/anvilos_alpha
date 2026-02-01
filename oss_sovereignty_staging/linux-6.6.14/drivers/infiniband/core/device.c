 

#include <linux/module.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <net/net_namespace.h>
#include <linux/security.h>
#include <linux/notifier.h>
#include <linux/hashtable.h>
#include <rdma/rdma_netlink.h>
#include <rdma/ib_addr.h>
#include <rdma/ib_cache.h>
#include <rdma/rdma_counter.h>

#include "core_priv.h"
#include "restrack.h"

MODULE_AUTHOR("Roland Dreier");
MODULE_DESCRIPTION("core kernel InfiniBand API");
MODULE_LICENSE("Dual BSD/GPL");

struct workqueue_struct *ib_comp_wq;
struct workqueue_struct *ib_comp_unbound_wq;
struct workqueue_struct *ib_wq;
EXPORT_SYMBOL_GPL(ib_wq);
static struct workqueue_struct *ib_unreg_wq;

 

 
static DEFINE_XARRAY_FLAGS(devices, XA_FLAGS_ALLOC);
static DECLARE_RWSEM(devices_rwsem);
#define DEVICE_REGISTERED XA_MARK_1

static u32 highest_client_id;
#define CLIENT_REGISTERED XA_MARK_1
static DEFINE_XARRAY_FLAGS(clients, XA_FLAGS_ALLOC);
static DECLARE_RWSEM(clients_rwsem);

static void ib_client_put(struct ib_client *client)
{
	if (refcount_dec_and_test(&client->uses))
		complete(&client->uses_zero);
}

 
#define CLIENT_DATA_REGISTERED XA_MARK_1

unsigned int rdma_dev_net_id;

 
static DEFINE_XARRAY_FLAGS(rdma_nets, XA_FLAGS_ALLOC);
 
static DECLARE_RWSEM(rdma_nets_rwsem);

bool ib_devices_shared_netns = true;
module_param_named(netns_mode, ib_devices_shared_netns, bool, 0444);
MODULE_PARM_DESC(netns_mode,
		 "Share device among net namespaces; default=1 (shared)");
 
bool rdma_dev_access_netns(const struct ib_device *dev, const struct net *net)
{
	return (ib_devices_shared_netns ||
		net_eq(read_pnet(&dev->coredev.rdma_net), net));
}
EXPORT_SYMBOL(rdma_dev_access_netns);

 
static void *xan_find_marked(struct xarray *xa, unsigned long *indexp,
			     xa_mark_t filter)
{
	XA_STATE(xas, xa, *indexp);
	void *entry;

	rcu_read_lock();
	do {
		entry = xas_find_marked(&xas, ULONG_MAX, filter);
		if (xa_is_zero(entry))
			break;
	} while (xas_retry(&xas, entry));
	rcu_read_unlock();

	if (entry) {
		*indexp = xas.xa_index;
		if (xa_is_zero(entry))
			return NULL;
		return entry;
	}
	return XA_ERROR(-ENOENT);
}
#define xan_for_each_marked(xa, index, entry, filter)                          \
	for (index = 0, entry = xan_find_marked(xa, &(index), filter);         \
	     !xa_is_err(entry);                                                \
	     (index)++, entry = xan_find_marked(xa, &(index), filter))

 
static DEFINE_SPINLOCK(ndev_hash_lock);
static DECLARE_HASHTABLE(ndev_hash, 5);

static void free_netdevs(struct ib_device *ib_dev);
static void ib_unregister_work(struct work_struct *work);
static void __ib_unregister_device(struct ib_device *device);
static int ib_security_change(struct notifier_block *nb, unsigned long event,
			      void *lsm_data);
static void ib_policy_change_task(struct work_struct *work);
static DECLARE_WORK(ib_policy_change_work, ib_policy_change_task);

static void __ibdev_printk(const char *level, const struct ib_device *ibdev,
			   struct va_format *vaf)
{
	if (ibdev && ibdev->dev.parent)
		dev_printk_emit(level[1] - '0',
				ibdev->dev.parent,
				"%s %s %s: %pV",
				dev_driver_string(ibdev->dev.parent),
				dev_name(ibdev->dev.parent),
				dev_name(&ibdev->dev),
				vaf);
	else if (ibdev)
		printk("%s%s: %pV",
		       level, dev_name(&ibdev->dev), vaf);
	else
		printk("%s(NULL ib_device): %pV", level, vaf);
}

void ibdev_printk(const char *level, const struct ib_device *ibdev,
		  const char *format, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, format);

	vaf.fmt = format;
	vaf.va = &args;

	__ibdev_printk(level, ibdev, &vaf);

	va_end(args);
}
EXPORT_SYMBOL(ibdev_printk);

#define define_ibdev_printk_level(func, level)                  \
void func(const struct ib_device *ibdev, const char *fmt, ...)  \
{                                                               \
	struct va_format vaf;                                   \
	va_list args;                                           \
								\
	va_start(args, fmt);                                    \
								\
	vaf.fmt = fmt;                                          \
	vaf.va = &args;                                         \
								\
	__ibdev_printk(level, ibdev, &vaf);                     \
								\
	va_end(args);                                           \
}                                                               \
EXPORT_SYMBOL(func);

define_ibdev_printk_level(ibdev_emerg, KERN_EMERG);
define_ibdev_printk_level(ibdev_alert, KERN_ALERT);
define_ibdev_printk_level(ibdev_crit, KERN_CRIT);
define_ibdev_printk_level(ibdev_err, KERN_ERR);
define_ibdev_printk_level(ibdev_warn, KERN_WARNING);
define_ibdev_printk_level(ibdev_notice, KERN_NOTICE);
define_ibdev_printk_level(ibdev_info, KERN_INFO);

static struct notifier_block ibdev_lsm_nb = {
	.notifier_call = ib_security_change,
};

static int rdma_dev_change_netns(struct ib_device *device, struct net *cur_net,
				 struct net *net);

 
struct ib_port_data_rcu {
	struct rcu_head rcu_head;
	struct ib_port_data pdata[];
};

static void ib_device_check_mandatory(struct ib_device *device)
{
#define IB_MANDATORY_FUNC(x) { offsetof(struct ib_device_ops, x), #x }
	static const struct {
		size_t offset;
		char  *name;
	} mandatory_table[] = {
		IB_MANDATORY_FUNC(query_device),
		IB_MANDATORY_FUNC(query_port),
		IB_MANDATORY_FUNC(alloc_pd),
		IB_MANDATORY_FUNC(dealloc_pd),
		IB_MANDATORY_FUNC(create_qp),
		IB_MANDATORY_FUNC(modify_qp),
		IB_MANDATORY_FUNC(destroy_qp),
		IB_MANDATORY_FUNC(post_send),
		IB_MANDATORY_FUNC(post_recv),
		IB_MANDATORY_FUNC(create_cq),
		IB_MANDATORY_FUNC(destroy_cq),
		IB_MANDATORY_FUNC(poll_cq),
		IB_MANDATORY_FUNC(req_notify_cq),
		IB_MANDATORY_FUNC(get_dma_mr),
		IB_MANDATORY_FUNC(reg_user_mr),
		IB_MANDATORY_FUNC(dereg_mr),
		IB_MANDATORY_FUNC(get_port_immutable)
	};
	int i;

	device->kverbs_provider = true;
	for (i = 0; i < ARRAY_SIZE(mandatory_table); ++i) {
		if (!*(void **) ((void *) &device->ops +
				 mandatory_table[i].offset)) {
			device->kverbs_provider = false;
			break;
		}
	}
}

 
struct ib_device *ib_device_get_by_index(const struct net *net, u32 index)
{
	struct ib_device *device;

	down_read(&devices_rwsem);
	device = xa_load(&devices, index);
	if (device) {
		if (!rdma_dev_access_netns(device, net)) {
			device = NULL;
			goto out;
		}

		if (!ib_device_try_get(device))
			device = NULL;
	}
out:
	up_read(&devices_rwsem);
	return device;
}

 
void ib_device_put(struct ib_device *device)
{
	if (refcount_dec_and_test(&device->refcount))
		complete(&device->unreg_completion);
}
EXPORT_SYMBOL(ib_device_put);

static struct ib_device *__ib_device_get_by_name(const char *name)
{
	struct ib_device *device;
	unsigned long index;

	xa_for_each (&devices, index, device)
		if (!strcmp(name, dev_name(&device->dev)))
			return device;

	return NULL;
}

 
struct ib_device *ib_device_get_by_name(const char *name,
					enum rdma_driver_id driver_id)
{
	struct ib_device *device;

	down_read(&devices_rwsem);
	device = __ib_device_get_by_name(name);
	if (device && driver_id != RDMA_DRIVER_UNKNOWN &&
	    device->ops.driver_id != driver_id)
		device = NULL;

	if (device) {
		if (!ib_device_try_get(device))
			device = NULL;
	}
	up_read(&devices_rwsem);
	return device;
}
EXPORT_SYMBOL(ib_device_get_by_name);

static int rename_compat_devs(struct ib_device *device)
{
	struct ib_core_device *cdev;
	unsigned long index;
	int ret = 0;

	mutex_lock(&device->compat_devs_mutex);
	xa_for_each (&device->compat_devs, index, cdev) {
		ret = device_rename(&cdev->dev, dev_name(&device->dev));
		if (ret) {
			dev_warn(&cdev->dev,
				 "Fail to rename compatdev to new name %s\n",
				 dev_name(&device->dev));
			break;
		}
	}
	mutex_unlock(&device->compat_devs_mutex);
	return ret;
}

int ib_device_rename(struct ib_device *ibdev, const char *name)
{
	unsigned long index;
	void *client_data;
	int ret;

	down_write(&devices_rwsem);
	if (!strcmp(name, dev_name(&ibdev->dev))) {
		up_write(&devices_rwsem);
		return 0;
	}

	if (__ib_device_get_by_name(name)) {
		up_write(&devices_rwsem);
		return -EEXIST;
	}

	ret = device_rename(&ibdev->dev, name);
	if (ret) {
		up_write(&devices_rwsem);
		return ret;
	}

	strscpy(ibdev->name, name, IB_DEVICE_NAME_MAX);
	ret = rename_compat_devs(ibdev);

	downgrade_write(&devices_rwsem);
	down_read(&ibdev->client_data_rwsem);
	xan_for_each_marked(&ibdev->client_data, index, client_data,
			    CLIENT_DATA_REGISTERED) {
		struct ib_client *client = xa_load(&clients, index);

		if (!client || !client->rename)
			continue;

		client->rename(ibdev, client_data);
	}
	up_read(&ibdev->client_data_rwsem);
	up_read(&devices_rwsem);
	return 0;
}

int ib_device_set_dim(struct ib_device *ibdev, u8 use_dim)
{
	if (use_dim > 1)
		return -EINVAL;
	ibdev->use_cq_dim = use_dim;

	return 0;
}

static int alloc_name(struct ib_device *ibdev, const char *name)
{
	struct ib_device *device;
	unsigned long index;
	struct ida inuse;
	int rc;
	int i;

	lockdep_assert_held_write(&devices_rwsem);
	ida_init(&inuse);
	xa_for_each (&devices, index, device) {
		char buf[IB_DEVICE_NAME_MAX];

		if (sscanf(dev_name(&device->dev), name, &i) != 1)
			continue;
		if (i < 0 || i >= INT_MAX)
			continue;
		snprintf(buf, sizeof buf, name, i);
		if (strcmp(buf, dev_name(&device->dev)) != 0)
			continue;

		rc = ida_alloc_range(&inuse, i, i, GFP_KERNEL);
		if (rc < 0)
			goto out;
	}

	rc = ida_alloc(&inuse, GFP_KERNEL);
	if (rc < 0)
		goto out;

	rc = dev_set_name(&ibdev->dev, name, rc);
out:
	ida_destroy(&inuse);
	return rc;
}

static void ib_device_release(struct device *device)
{
	struct ib_device *dev = container_of(device, struct ib_device, dev);

	free_netdevs(dev);
	WARN_ON(refcount_read(&dev->refcount));
	if (dev->hw_stats_data)
		ib_device_release_hw_stats(dev->hw_stats_data);
	if (dev->port_data) {
		ib_cache_release_one(dev);
		ib_security_release_port_pkey_list(dev);
		rdma_counter_release(dev);
		kfree_rcu(container_of(dev->port_data, struct ib_port_data_rcu,
				       pdata[0]),
			  rcu_head);
	}

	mutex_destroy(&dev->unregistration_lock);
	mutex_destroy(&dev->compat_devs_mutex);

	xa_destroy(&dev->compat_devs);
	xa_destroy(&dev->client_data);
	kfree_rcu(dev, rcu_head);
}

static int ib_device_uevent(const struct device *device,
			    struct kobj_uevent_env *env)
{
	if (add_uevent_var(env, "NAME=%s", dev_name(device)))
		return -ENOMEM;

	 

	return 0;
}

static const void *net_namespace(const struct device *d)
{
	const struct ib_core_device *coredev =
			container_of(d, struct ib_core_device, dev);

	return read_pnet(&coredev->rdma_net);
}

static struct class ib_class = {
	.name    = "infiniband",
	.dev_release = ib_device_release,
	.dev_uevent = ib_device_uevent,
	.ns_type = &net_ns_type_operations,
	.namespace = net_namespace,
};

static void rdma_init_coredev(struct ib_core_device *coredev,
			      struct ib_device *dev, struct net *net)
{
	 
	BUILD_BUG_ON(offsetof(struct ib_device, coredev.dev) !=
		     offsetof(struct ib_device, dev));

	coredev->dev.class = &ib_class;
	coredev->dev.groups = dev->groups;
	device_initialize(&coredev->dev);
	coredev->owner = dev;
	INIT_LIST_HEAD(&coredev->port_list);
	write_pnet(&coredev->rdma_net, net);
}

 
struct ib_device *_ib_alloc_device(size_t size)
{
	struct ib_device *device;
	unsigned int i;

	if (WARN_ON(size < sizeof(struct ib_device)))
		return NULL;

	device = kzalloc(size, GFP_KERNEL);
	if (!device)
		return NULL;

	if (rdma_restrack_init(device)) {
		kfree(device);
		return NULL;
	}

	rdma_init_coredev(&device->coredev, device, &init_net);

	INIT_LIST_HEAD(&device->event_handler_list);
	spin_lock_init(&device->qp_open_list_lock);
	init_rwsem(&device->event_handler_rwsem);
	mutex_init(&device->unregistration_lock);
	 
	xa_init_flags(&device->client_data, XA_FLAGS_ALLOC);
	init_rwsem(&device->client_data_rwsem);
	xa_init_flags(&device->compat_devs, XA_FLAGS_ALLOC);
	mutex_init(&device->compat_devs_mutex);
	init_completion(&device->unreg_completion);
	INIT_WORK(&device->unregistration_work, ib_unregister_work);

	spin_lock_init(&device->cq_pools_lock);
	for (i = 0; i < ARRAY_SIZE(device->cq_pools); i++)
		INIT_LIST_HEAD(&device->cq_pools[i]);

	rwlock_init(&device->cache_lock);

	device->uverbs_cmd_mask =
		BIT_ULL(IB_USER_VERBS_CMD_ALLOC_MW) |
		BIT_ULL(IB_USER_VERBS_CMD_ALLOC_PD) |
		BIT_ULL(IB_USER_VERBS_CMD_ATTACH_MCAST) |
		BIT_ULL(IB_USER_VERBS_CMD_CLOSE_XRCD) |
		BIT_ULL(IB_USER_VERBS_CMD_CREATE_AH) |
		BIT_ULL(IB_USER_VERBS_CMD_CREATE_COMP_CHANNEL) |
		BIT_ULL(IB_USER_VERBS_CMD_CREATE_CQ) |
		BIT_ULL(IB_USER_VERBS_CMD_CREATE_QP) |
		BIT_ULL(IB_USER_VERBS_CMD_CREATE_SRQ) |
		BIT_ULL(IB_USER_VERBS_CMD_CREATE_XSRQ) |
		BIT_ULL(IB_USER_VERBS_CMD_DEALLOC_MW) |
		BIT_ULL(IB_USER_VERBS_CMD_DEALLOC_PD) |
		BIT_ULL(IB_USER_VERBS_CMD_DEREG_MR) |
		BIT_ULL(IB_USER_VERBS_CMD_DESTROY_AH) |
		BIT_ULL(IB_USER_VERBS_CMD_DESTROY_CQ) |
		BIT_ULL(IB_USER_VERBS_CMD_DESTROY_QP) |
		BIT_ULL(IB_USER_VERBS_CMD_DESTROY_SRQ) |
		BIT_ULL(IB_USER_VERBS_CMD_DETACH_MCAST) |
		BIT_ULL(IB_USER_VERBS_CMD_GET_CONTEXT) |
		BIT_ULL(IB_USER_VERBS_CMD_MODIFY_QP) |
		BIT_ULL(IB_USER_VERBS_CMD_MODIFY_SRQ) |
		BIT_ULL(IB_USER_VERBS_CMD_OPEN_QP) |
		BIT_ULL(IB_USER_VERBS_CMD_OPEN_XRCD) |
		BIT_ULL(IB_USER_VERBS_CMD_QUERY_DEVICE) |
		BIT_ULL(IB_USER_VERBS_CMD_QUERY_PORT) |
		BIT_ULL(IB_USER_VERBS_CMD_QUERY_QP) |
		BIT_ULL(IB_USER_VERBS_CMD_QUERY_SRQ) |
		BIT_ULL(IB_USER_VERBS_CMD_REG_MR) |
		BIT_ULL(IB_USER_VERBS_CMD_REREG_MR) |
		BIT_ULL(IB_USER_VERBS_CMD_RESIZE_CQ);
	return device;
}
EXPORT_SYMBOL(_ib_alloc_device);

 
void ib_dealloc_device(struct ib_device *device)
{
	if (device->ops.dealloc_driver)
		device->ops.dealloc_driver(device);

	 
	down_write(&devices_rwsem);
	if (xa_load(&devices, device->index) == device)
		xa_erase(&devices, device->index);
	up_write(&devices_rwsem);

	 
	free_netdevs(device);

	WARN_ON(!xa_empty(&device->compat_devs));
	WARN_ON(!xa_empty(&device->client_data));
	WARN_ON(refcount_read(&device->refcount));
	rdma_restrack_clean(device);
	 
	put_device(&device->dev);
}
EXPORT_SYMBOL(ib_dealloc_device);

 
static int add_client_context(struct ib_device *device,
			      struct ib_client *client)
{
	int ret = 0;

	if (!device->kverbs_provider && !client->no_kverbs_req)
		return 0;

	down_write(&device->client_data_rwsem);
	 
	if (!refcount_inc_not_zero(&client->uses))
		goto out_unlock;
	refcount_inc(&device->refcount);

	 
	if (xa_get_mark(&device->client_data, client->client_id,
		    CLIENT_DATA_REGISTERED))
		goto out;

	ret = xa_err(xa_store(&device->client_data, client->client_id, NULL,
			      GFP_KERNEL));
	if (ret)
		goto out;
	downgrade_write(&device->client_data_rwsem);
	if (client->add) {
		if (client->add(device)) {
			 
			xa_erase(&device->client_data, client->client_id);
			up_read(&device->client_data_rwsem);
			ib_device_put(device);
			ib_client_put(client);
			return 0;
		}
	}

	 
	xa_set_mark(&device->client_data, client->client_id,
		    CLIENT_DATA_REGISTERED);
	up_read(&device->client_data_rwsem);
	return 0;

out:
	ib_device_put(device);
	ib_client_put(client);
out_unlock:
	up_write(&device->client_data_rwsem);
	return ret;
}

static void remove_client_context(struct ib_device *device,
				  unsigned int client_id)
{
	struct ib_client *client;
	void *client_data;

	down_write(&device->client_data_rwsem);
	if (!xa_get_mark(&device->client_data, client_id,
			 CLIENT_DATA_REGISTERED)) {
		up_write(&device->client_data_rwsem);
		return;
	}
	client_data = xa_load(&device->client_data, client_id);
	xa_clear_mark(&device->client_data, client_id, CLIENT_DATA_REGISTERED);
	client = xa_load(&clients, client_id);
	up_write(&device->client_data_rwsem);

	 
	if (client->remove)
		client->remove(device, client_data);

	xa_erase(&device->client_data, client_id);
	ib_device_put(device);
	ib_client_put(client);
}

static int alloc_port_data(struct ib_device *device)
{
	struct ib_port_data_rcu *pdata_rcu;
	u32 port;

	if (device->port_data)
		return 0;

	 
	if (WARN_ON(!device->phys_port_cnt))
		return -EINVAL;

	 
	if (WARN_ON(device->phys_port_cnt == U32_MAX))
		return -EINVAL;

	 
	pdata_rcu = kzalloc(struct_size(pdata_rcu, pdata,
					size_add(rdma_end_port(device), 1)),
			    GFP_KERNEL);
	if (!pdata_rcu)
		return -ENOMEM;
	 
	device->port_data = pdata_rcu->pdata;

	rdma_for_each_port (device, port) {
		struct ib_port_data *pdata = &device->port_data[port];

		pdata->ib_dev = device;
		spin_lock_init(&pdata->pkey_list_lock);
		INIT_LIST_HEAD(&pdata->pkey_list);
		spin_lock_init(&pdata->netdev_lock);
		INIT_HLIST_NODE(&pdata->ndev_hash_link);
	}
	return 0;
}

static int verify_immutable(const struct ib_device *dev, u32 port)
{
	return WARN_ON(!rdma_cap_ib_mad(dev, port) &&
			    rdma_max_mad_size(dev, port) != 0);
}

static int setup_port_data(struct ib_device *device)
{
	u32 port;
	int ret;

	ret = alloc_port_data(device);
	if (ret)
		return ret;

	rdma_for_each_port (device, port) {
		struct ib_port_data *pdata = &device->port_data[port];

		ret = device->ops.get_port_immutable(device, port,
						     &pdata->immutable);
		if (ret)
			return ret;

		if (verify_immutable(device, port))
			return -EINVAL;
	}
	return 0;
}

 
const struct ib_port_immutable*
ib_port_immutable_read(struct ib_device *dev, unsigned int port)
{
	WARN_ON(!rdma_is_port_valid(dev, port));
	return &dev->port_data[port].immutable;
}
EXPORT_SYMBOL(ib_port_immutable_read);

void ib_get_device_fw_str(struct ib_device *dev, char *str)
{
	if (dev->ops.get_dev_fw_str)
		dev->ops.get_dev_fw_str(dev, str);
	else
		str[0] = '\0';
}
EXPORT_SYMBOL(ib_get_device_fw_str);

static void ib_policy_change_task(struct work_struct *work)
{
	struct ib_device *dev;
	unsigned long index;

	down_read(&devices_rwsem);
	xa_for_each_marked (&devices, index, dev, DEVICE_REGISTERED) {
		unsigned int i;

		rdma_for_each_port (dev, i) {
			u64 sp;
			ib_get_cached_subnet_prefix(dev, i, &sp);
			ib_security_cache_change(dev, i, sp);
		}
	}
	up_read(&devices_rwsem);
}

static int ib_security_change(struct notifier_block *nb, unsigned long event,
			      void *lsm_data)
{
	if (event != LSM_POLICY_CHANGE)
		return NOTIFY_DONE;

	schedule_work(&ib_policy_change_work);
	ib_mad_agent_security_change();

	return NOTIFY_OK;
}

static void compatdev_release(struct device *dev)
{
	struct ib_core_device *cdev =
		container_of(dev, struct ib_core_device, dev);

	kfree(cdev);
}

static int add_one_compat_dev(struct ib_device *device,
			      struct rdma_dev_net *rnet)
{
	struct ib_core_device *cdev;
	int ret;

	lockdep_assert_held(&rdma_nets_rwsem);
	if (!ib_devices_shared_netns)
		return 0;

	 
	if (net_eq(read_pnet(&rnet->net),
		   read_pnet(&device->coredev.rdma_net)))
		return 0;

	 
	mutex_lock(&device->compat_devs_mutex);
	cdev = xa_load(&device->compat_devs, rnet->id);
	if (cdev) {
		ret = 0;
		goto done;
	}
	ret = xa_reserve(&device->compat_devs, rnet->id, GFP_KERNEL);
	if (ret)
		goto done;

	cdev = kzalloc(sizeof(*cdev), GFP_KERNEL);
	if (!cdev) {
		ret = -ENOMEM;
		goto cdev_err;
	}

	cdev->dev.parent = device->dev.parent;
	rdma_init_coredev(cdev, device, read_pnet(&rnet->net));
	cdev->dev.release = compatdev_release;
	ret = dev_set_name(&cdev->dev, "%s", dev_name(&device->dev));
	if (ret)
		goto add_err;

	ret = device_add(&cdev->dev);
	if (ret)
		goto add_err;
	ret = ib_setup_port_attrs(cdev);
	if (ret)
		goto port_err;

	ret = xa_err(xa_store(&device->compat_devs, rnet->id,
			      cdev, GFP_KERNEL));
	if (ret)
		goto insert_err;

	mutex_unlock(&device->compat_devs_mutex);
	return 0;

insert_err:
	ib_free_port_attrs(cdev);
port_err:
	device_del(&cdev->dev);
add_err:
	put_device(&cdev->dev);
cdev_err:
	xa_release(&device->compat_devs, rnet->id);
done:
	mutex_unlock(&device->compat_devs_mutex);
	return ret;
}

static void remove_one_compat_dev(struct ib_device *device, u32 id)
{
	struct ib_core_device *cdev;

	mutex_lock(&device->compat_devs_mutex);
	cdev = xa_erase(&device->compat_devs, id);
	mutex_unlock(&device->compat_devs_mutex);
	if (cdev) {
		ib_free_port_attrs(cdev);
		device_del(&cdev->dev);
		put_device(&cdev->dev);
	}
}

static void remove_compat_devs(struct ib_device *device)
{
	struct ib_core_device *cdev;
	unsigned long index;

	xa_for_each (&device->compat_devs, index, cdev)
		remove_one_compat_dev(device, index);
}

static int add_compat_devs(struct ib_device *device)
{
	struct rdma_dev_net *rnet;
	unsigned long index;
	int ret = 0;

	lockdep_assert_held(&devices_rwsem);

	down_read(&rdma_nets_rwsem);
	xa_for_each (&rdma_nets, index, rnet) {
		ret = add_one_compat_dev(device, rnet);
		if (ret)
			break;
	}
	up_read(&rdma_nets_rwsem);
	return ret;
}

static void remove_all_compat_devs(void)
{
	struct ib_compat_device *cdev;
	struct ib_device *dev;
	unsigned long index;

	down_read(&devices_rwsem);
	xa_for_each (&devices, index, dev) {
		unsigned long c_index = 0;

		 
		down_read(&rdma_nets_rwsem);
		xa_for_each (&dev->compat_devs, c_index, cdev)
			remove_one_compat_dev(dev, c_index);
		up_read(&rdma_nets_rwsem);
	}
	up_read(&devices_rwsem);
}

static int add_all_compat_devs(void)
{
	struct rdma_dev_net *rnet;
	struct ib_device *dev;
	unsigned long index;
	int ret = 0;

	down_read(&devices_rwsem);
	xa_for_each_marked (&devices, index, dev, DEVICE_REGISTERED) {
		unsigned long net_index = 0;

		 
		down_read(&rdma_nets_rwsem);
		xa_for_each (&rdma_nets, net_index, rnet) {
			ret = add_one_compat_dev(dev, rnet);
			if (ret)
				break;
		}
		up_read(&rdma_nets_rwsem);
	}
	up_read(&devices_rwsem);
	if (ret)
		remove_all_compat_devs();
	return ret;
}

int rdma_compatdev_set(u8 enable)
{
	struct rdma_dev_net *rnet;
	unsigned long index;
	int ret = 0;

	down_write(&rdma_nets_rwsem);
	if (ib_devices_shared_netns == enable) {
		up_write(&rdma_nets_rwsem);
		return 0;
	}

	 
	xa_for_each (&rdma_nets, index, rnet) {
		ret++;
		break;
	}
	if (!ret)
		ib_devices_shared_netns = enable;
	up_write(&rdma_nets_rwsem);
	if (ret)
		return -EBUSY;

	if (enable)
		ret = add_all_compat_devs();
	else
		remove_all_compat_devs();
	return ret;
}

static void rdma_dev_exit_net(struct net *net)
{
	struct rdma_dev_net *rnet = rdma_net_to_dev_net(net);
	struct ib_device *dev;
	unsigned long index;
	int ret;

	down_write(&rdma_nets_rwsem);
	 
	ret = xa_err(xa_store(&rdma_nets, rnet->id, NULL, GFP_KERNEL));
	WARN_ON(ret);
	up_write(&rdma_nets_rwsem);

	down_read(&devices_rwsem);
	xa_for_each (&devices, index, dev) {
		get_device(&dev->dev);
		 
		up_read(&devices_rwsem);

		remove_one_compat_dev(dev, rnet->id);

		 
		rdma_dev_change_netns(dev, net, &init_net);

		put_device(&dev->dev);
		down_read(&devices_rwsem);
	}
	up_read(&devices_rwsem);

	rdma_nl_net_exit(rnet);
	xa_erase(&rdma_nets, rnet->id);
}

static __net_init int rdma_dev_init_net(struct net *net)
{
	struct rdma_dev_net *rnet = rdma_net_to_dev_net(net);
	unsigned long index;
	struct ib_device *dev;
	int ret;

	write_pnet(&rnet->net, net);

	ret = rdma_nl_net_init(rnet);
	if (ret)
		return ret;

	 
	if (net_eq(net, &init_net))
		return 0;

	ret = xa_alloc(&rdma_nets, &rnet->id, rnet, xa_limit_32b, GFP_KERNEL);
	if (ret) {
		rdma_nl_net_exit(rnet);
		return ret;
	}

	down_read(&devices_rwsem);
	xa_for_each_marked (&devices, index, dev, DEVICE_REGISTERED) {
		 
		down_read(&rdma_nets_rwsem);
		ret = add_one_compat_dev(dev, rnet);
		up_read(&rdma_nets_rwsem);
		if (ret)
			break;
	}
	up_read(&devices_rwsem);

	if (ret)
		rdma_dev_exit_net(net);

	return ret;
}

 
static int assign_name(struct ib_device *device, const char *name)
{
	static u32 last_id;
	int ret;

	down_write(&devices_rwsem);
	 
	if (strchr(name, '%'))
		ret = alloc_name(device, name);
	else
		ret = dev_set_name(&device->dev, name);
	if (ret)
		goto out;

	if (__ib_device_get_by_name(dev_name(&device->dev))) {
		ret = -ENFILE;
		goto out;
	}
	strscpy(device->name, dev_name(&device->dev), IB_DEVICE_NAME_MAX);

	ret = xa_alloc_cyclic(&devices, &device->index, device, xa_limit_31b,
			&last_id, GFP_KERNEL);
	if (ret > 0)
		ret = 0;

out:
	up_write(&devices_rwsem);
	return ret;
}

 
static int setup_device(struct ib_device *device)
{
	struct ib_udata uhw = {.outlen = 0, .inlen = 0};
	int ret;

	ib_device_check_mandatory(device);

	ret = setup_port_data(device);
	if (ret) {
		dev_warn(&device->dev, "Couldn't create per-port data\n");
		return ret;
	}

	memset(&device->attrs, 0, sizeof(device->attrs));
	ret = device->ops.query_device(device, &device->attrs, &uhw);
	if (ret) {
		dev_warn(&device->dev,
			 "Couldn't query the device attributes\n");
		return ret;
	}

	return 0;
}

static void disable_device(struct ib_device *device)
{
	u32 cid;

	WARN_ON(!refcount_read(&device->refcount));

	down_write(&devices_rwsem);
	xa_clear_mark(&devices, device->index, DEVICE_REGISTERED);
	up_write(&devices_rwsem);

	 
	down_read(&clients_rwsem);
	cid = highest_client_id;
	up_read(&clients_rwsem);
	while (cid) {
		cid--;
		remove_client_context(device, cid);
	}

	ib_cq_pool_cleanup(device);

	 
	ib_device_put(device);
	wait_for_completion(&device->unreg_completion);

	 
	remove_compat_devs(device);
}

 
static int enable_device_and_get(struct ib_device *device)
{
	struct ib_client *client;
	unsigned long index;
	int ret = 0;

	 
	refcount_set(&device->refcount, 2);
	down_write(&devices_rwsem);
	xa_set_mark(&devices, device->index, DEVICE_REGISTERED);

	 
	downgrade_write(&devices_rwsem);

	if (device->ops.enable_driver) {
		ret = device->ops.enable_driver(device);
		if (ret)
			goto out;
	}

	down_read(&clients_rwsem);
	xa_for_each_marked (&clients, index, client, CLIENT_REGISTERED) {
		ret = add_client_context(device, client);
		if (ret)
			break;
	}
	up_read(&clients_rwsem);
	if (!ret)
		ret = add_compat_devs(device);
out:
	up_read(&devices_rwsem);
	return ret;
}

static void prevent_dealloc_device(struct ib_device *ib_dev)
{
}

 
int ib_register_device(struct ib_device *device, const char *name,
		       struct device *dma_device)
{
	int ret;

	ret = assign_name(device, name);
	if (ret)
		return ret;

	 
	WARN_ON(dma_device && !dma_device->dma_parms);
	device->dma_device = dma_device;

	ret = setup_device(device);
	if (ret)
		return ret;

	ret = ib_cache_setup_one(device);
	if (ret) {
		dev_warn(&device->dev,
			 "Couldn't set up InfiniBand P_Key/GID cache\n");
		return ret;
	}

	device->groups[0] = &ib_dev_attr_group;
	device->groups[1] = device->ops.device_group;
	ret = ib_setup_device_attrs(device);
	if (ret)
		goto cache_cleanup;

	ib_device_register_rdmacg(device);

	rdma_counter_init(device);

	 
	dev_set_uevent_suppress(&device->dev, true);
	ret = device_add(&device->dev);
	if (ret)
		goto cg_cleanup;

	ret = ib_setup_port_attrs(&device->coredev);
	if (ret) {
		dev_warn(&device->dev,
			 "Couldn't register device with driver model\n");
		goto dev_cleanup;
	}

	ret = enable_device_and_get(device);
	if (ret) {
		void (*dealloc_fn)(struct ib_device *);

		 
		dealloc_fn = device->ops.dealloc_driver;
		device->ops.dealloc_driver = prevent_dealloc_device;
		ib_device_put(device);
		__ib_unregister_device(device);
		device->ops.dealloc_driver = dealloc_fn;
		dev_set_uevent_suppress(&device->dev, false);
		return ret;
	}
	dev_set_uevent_suppress(&device->dev, false);
	 
	kobject_uevent(&device->dev.kobj, KOBJ_ADD);
	ib_device_put(device);

	return 0;

dev_cleanup:
	device_del(&device->dev);
cg_cleanup:
	dev_set_uevent_suppress(&device->dev, false);
	ib_device_unregister_rdmacg(device);
cache_cleanup:
	ib_cache_cleanup_one(device);
	return ret;
}
EXPORT_SYMBOL(ib_register_device);

 
static void __ib_unregister_device(struct ib_device *ib_dev)
{
	 
	mutex_lock(&ib_dev->unregistration_lock);
	if (!refcount_read(&ib_dev->refcount))
		goto out;

	disable_device(ib_dev);

	 
	free_netdevs(ib_dev);

	ib_free_port_attrs(&ib_dev->coredev);
	device_del(&ib_dev->dev);
	ib_device_unregister_rdmacg(ib_dev);
	ib_cache_cleanup_one(ib_dev);

	 
	if (ib_dev->ops.dealloc_driver &&
	    ib_dev->ops.dealloc_driver != prevent_dealloc_device) {
		WARN_ON(kref_read(&ib_dev->dev.kobj.kref) <= 1);
		ib_dealloc_device(ib_dev);
	}
out:
	mutex_unlock(&ib_dev->unregistration_lock);
}

 
void ib_unregister_device(struct ib_device *ib_dev)
{
	get_device(&ib_dev->dev);
	__ib_unregister_device(ib_dev);
	put_device(&ib_dev->dev);
}
EXPORT_SYMBOL(ib_unregister_device);

 
void ib_unregister_device_and_put(struct ib_device *ib_dev)
{
	WARN_ON(!ib_dev->ops.dealloc_driver);
	get_device(&ib_dev->dev);
	ib_device_put(ib_dev);
	__ib_unregister_device(ib_dev);
	put_device(&ib_dev->dev);
}
EXPORT_SYMBOL(ib_unregister_device_and_put);

 
void ib_unregister_driver(enum rdma_driver_id driver_id)
{
	struct ib_device *ib_dev;
	unsigned long index;

	down_read(&devices_rwsem);
	xa_for_each (&devices, index, ib_dev) {
		if (ib_dev->ops.driver_id != driver_id)
			continue;

		get_device(&ib_dev->dev);
		up_read(&devices_rwsem);

		WARN_ON(!ib_dev->ops.dealloc_driver);
		__ib_unregister_device(ib_dev);

		put_device(&ib_dev->dev);
		down_read(&devices_rwsem);
	}
	up_read(&devices_rwsem);
}
EXPORT_SYMBOL(ib_unregister_driver);

static void ib_unregister_work(struct work_struct *work)
{
	struct ib_device *ib_dev =
		container_of(work, struct ib_device, unregistration_work);

	__ib_unregister_device(ib_dev);
	put_device(&ib_dev->dev);
}

 
void ib_unregister_device_queued(struct ib_device *ib_dev)
{
	WARN_ON(!refcount_read(&ib_dev->refcount));
	WARN_ON(!ib_dev->ops.dealloc_driver);
	get_device(&ib_dev->dev);
	if (!queue_work(ib_unreg_wq, &ib_dev->unregistration_work))
		put_device(&ib_dev->dev);
}
EXPORT_SYMBOL(ib_unregister_device_queued);

 
static int rdma_dev_change_netns(struct ib_device *device, struct net *cur_net,
				 struct net *net)
{
	int ret2 = -EINVAL;
	int ret;

	mutex_lock(&device->unregistration_lock);

	 
	if (refcount_read(&device->refcount) == 0 ||
	    !net_eq(cur_net, read_pnet(&device->coredev.rdma_net))) {
		ret = -ENODEV;
		goto out;
	}

	kobject_uevent(&device->dev.kobj, KOBJ_REMOVE);
	disable_device(device);

	 
	write_pnet(&device->coredev.rdma_net, net);

	down_read(&devices_rwsem);
	 
	ret = device_rename(&device->dev, dev_name(&device->dev));
	up_read(&devices_rwsem);
	if (ret) {
		dev_warn(&device->dev,
			 "%s: Couldn't rename device after namespace change\n",
			 __func__);
		 
		write_pnet(&device->coredev.rdma_net, cur_net);
	}

	ret2 = enable_device_and_get(device);
	if (ret2) {
		 
		dev_warn(&device->dev,
			 "%s: Couldn't re-enable device after namespace change\n",
			 __func__);
	}
	kobject_uevent(&device->dev.kobj, KOBJ_ADD);

	ib_device_put(device);
out:
	mutex_unlock(&device->unregistration_lock);
	if (ret)
		return ret;
	return ret2;
}

int ib_device_set_netns_put(struct sk_buff *skb,
			    struct ib_device *dev, u32 ns_fd)
{
	struct net *net;
	int ret;

	net = get_net_ns_by_fd(ns_fd);
	if (IS_ERR(net)) {
		ret = PTR_ERR(net);
		goto net_err;
	}

	if (!netlink_ns_capable(skb, net->user_ns, CAP_NET_ADMIN)) {
		ret = -EPERM;
		goto ns_err;
	}

	 
	if (!dev->ops.disassociate_ucontext || ib_devices_shared_netns) {
		ret = -EOPNOTSUPP;
		goto ns_err;
	}

	get_device(&dev->dev);
	ib_device_put(dev);
	ret = rdma_dev_change_netns(dev, current->nsproxy->net_ns, net);
	put_device(&dev->dev);

	put_net(net);
	return ret;

ns_err:
	put_net(net);
net_err:
	ib_device_put(dev);
	return ret;
}

static struct pernet_operations rdma_dev_net_ops = {
	.init = rdma_dev_init_net,
	.exit = rdma_dev_exit_net,
	.id = &rdma_dev_net_id,
	.size = sizeof(struct rdma_dev_net),
};

static int assign_client_id(struct ib_client *client)
{
	int ret;

	down_write(&clients_rwsem);
	 
	client->client_id = highest_client_id;
	ret = xa_insert(&clients, client->client_id, client, GFP_KERNEL);
	if (ret)
		goto out;

	highest_client_id++;
	xa_set_mark(&clients, client->client_id, CLIENT_REGISTERED);

out:
	up_write(&clients_rwsem);
	return ret;
}

static void remove_client_id(struct ib_client *client)
{
	down_write(&clients_rwsem);
	xa_erase(&clients, client->client_id);
	for (; highest_client_id; highest_client_id--)
		if (xa_load(&clients, highest_client_id - 1))
			break;
	up_write(&clients_rwsem);
}

 
int ib_register_client(struct ib_client *client)
{
	struct ib_device *device;
	unsigned long index;
	int ret;

	refcount_set(&client->uses, 1);
	init_completion(&client->uses_zero);
	ret = assign_client_id(client);
	if (ret)
		return ret;

	down_read(&devices_rwsem);
	xa_for_each_marked (&devices, index, device, DEVICE_REGISTERED) {
		ret = add_client_context(device, client);
		if (ret) {
			up_read(&devices_rwsem);
			ib_unregister_client(client);
			return ret;
		}
	}
	up_read(&devices_rwsem);
	return 0;
}
EXPORT_SYMBOL(ib_register_client);

 
void ib_unregister_client(struct ib_client *client)
{
	struct ib_device *device;
	unsigned long index;

	down_write(&clients_rwsem);
	ib_client_put(client);
	xa_clear_mark(&clients, client->client_id, CLIENT_REGISTERED);
	up_write(&clients_rwsem);

	 
	rcu_read_lock();
	xa_for_each (&devices, index, device) {
		if (!ib_device_try_get(device))
			continue;
		rcu_read_unlock();

		remove_client_context(device, client->client_id);

		ib_device_put(device);
		rcu_read_lock();
	}
	rcu_read_unlock();

	 
	wait_for_completion(&client->uses_zero);
	remove_client_id(client);
}
EXPORT_SYMBOL(ib_unregister_client);

static int __ib_get_global_client_nl_info(const char *client_name,
					  struct ib_client_nl_info *res)
{
	struct ib_client *client;
	unsigned long index;
	int ret = -ENOENT;

	down_read(&clients_rwsem);
	xa_for_each_marked (&clients, index, client, CLIENT_REGISTERED) {
		if (strcmp(client->name, client_name) != 0)
			continue;
		if (!client->get_global_nl_info) {
			ret = -EOPNOTSUPP;
			break;
		}
		ret = client->get_global_nl_info(res);
		if (WARN_ON(ret == -ENOENT))
			ret = -EINVAL;
		if (!ret && res->cdev)
			get_device(res->cdev);
		break;
	}
	up_read(&clients_rwsem);
	return ret;
}

static int __ib_get_client_nl_info(struct ib_device *ibdev,
				   const char *client_name,
				   struct ib_client_nl_info *res)
{
	unsigned long index;
	void *client_data;
	int ret = -ENOENT;

	down_read(&ibdev->client_data_rwsem);
	xan_for_each_marked (&ibdev->client_data, index, client_data,
			     CLIENT_DATA_REGISTERED) {
		struct ib_client *client = xa_load(&clients, index);

		if (!client || strcmp(client->name, client_name) != 0)
			continue;
		if (!client->get_nl_info) {
			ret = -EOPNOTSUPP;
			break;
		}
		ret = client->get_nl_info(ibdev, client_data, res);
		if (WARN_ON(ret == -ENOENT))
			ret = -EINVAL;

		 
		if (!ret && res->cdev)
			get_device(res->cdev);
		break;
	}
	up_read(&ibdev->client_data_rwsem);

	return ret;
}

 
int ib_get_client_nl_info(struct ib_device *ibdev, const char *client_name,
			  struct ib_client_nl_info *res)
{
	int ret;

	if (ibdev)
		ret = __ib_get_client_nl_info(ibdev, client_name, res);
	else
		ret = __ib_get_global_client_nl_info(client_name, res);
#ifdef CONFIG_MODULES
	if (ret == -ENOENT) {
		request_module("rdma-client-%s", client_name);
		if (ibdev)
			ret = __ib_get_client_nl_info(ibdev, client_name, res);
		else
			ret = __ib_get_global_client_nl_info(client_name, res);
	}
#endif
	if (ret) {
		if (ret == -ENOENT)
			return -EOPNOTSUPP;
		return ret;
	}

	if (WARN_ON(!res->cdev))
		return -EINVAL;
	return 0;
}

 
void ib_set_client_data(struct ib_device *device, struct ib_client *client,
			void *data)
{
	void *rc;

	if (WARN_ON(IS_ERR(data)))
		data = NULL;

	rc = xa_store(&device->client_data, client->client_id, data,
		      GFP_KERNEL);
	WARN_ON(xa_is_err(rc));
}
EXPORT_SYMBOL(ib_set_client_data);

 
void ib_register_event_handler(struct ib_event_handler *event_handler)
{
	down_write(&event_handler->device->event_handler_rwsem);
	list_add_tail(&event_handler->list,
		      &event_handler->device->event_handler_list);
	up_write(&event_handler->device->event_handler_rwsem);
}
EXPORT_SYMBOL(ib_register_event_handler);

 
void ib_unregister_event_handler(struct ib_event_handler *event_handler)
{
	down_write(&event_handler->device->event_handler_rwsem);
	list_del(&event_handler->list);
	up_write(&event_handler->device->event_handler_rwsem);
}
EXPORT_SYMBOL(ib_unregister_event_handler);

void ib_dispatch_event_clients(struct ib_event *event)
{
	struct ib_event_handler *handler;

	down_read(&event->device->event_handler_rwsem);

	list_for_each_entry(handler, &event->device->event_handler_list, list)
		handler->handler(handler, event);

	up_read(&event->device->event_handler_rwsem);
}

static int iw_query_port(struct ib_device *device,
			   u32 port_num,
			   struct ib_port_attr *port_attr)
{
	struct in_device *inetdev;
	struct net_device *netdev;

	memset(port_attr, 0, sizeof(*port_attr));

	netdev = ib_device_get_netdev(device, port_num);
	if (!netdev)
		return -ENODEV;

	port_attr->max_mtu = IB_MTU_4096;
	port_attr->active_mtu = ib_mtu_int_to_enum(netdev->mtu);

	if (!netif_carrier_ok(netdev)) {
		port_attr->state = IB_PORT_DOWN;
		port_attr->phys_state = IB_PORT_PHYS_STATE_DISABLED;
	} else {
		rcu_read_lock();
		inetdev = __in_dev_get_rcu(netdev);

		if (inetdev && inetdev->ifa_list) {
			port_attr->state = IB_PORT_ACTIVE;
			port_attr->phys_state = IB_PORT_PHYS_STATE_LINK_UP;
		} else {
			port_attr->state = IB_PORT_INIT;
			port_attr->phys_state =
				IB_PORT_PHYS_STATE_PORT_CONFIGURATION_TRAINING;
		}

		rcu_read_unlock();
	}

	dev_put(netdev);
	return device->ops.query_port(device, port_num, port_attr);
}

static int __ib_query_port(struct ib_device *device,
			   u32 port_num,
			   struct ib_port_attr *port_attr)
{
	int err;

	memset(port_attr, 0, sizeof(*port_attr));

	err = device->ops.query_port(device, port_num, port_attr);
	if (err || port_attr->subnet_prefix)
		return err;

	if (rdma_port_get_link_layer(device, port_num) !=
	    IB_LINK_LAYER_INFINIBAND)
		return 0;

	ib_get_cached_subnet_prefix(device, port_num,
				    &port_attr->subnet_prefix);
	return 0;
}

 
int ib_query_port(struct ib_device *device,
		  u32 port_num,
		  struct ib_port_attr *port_attr)
{
	if (!rdma_is_port_valid(device, port_num))
		return -EINVAL;

	if (rdma_protocol_iwarp(device, port_num))
		return iw_query_port(device, port_num, port_attr);
	else
		return __ib_query_port(device, port_num, port_attr);
}
EXPORT_SYMBOL(ib_query_port);

static void add_ndev_hash(struct ib_port_data *pdata)
{
	unsigned long flags;

	might_sleep();

	spin_lock_irqsave(&ndev_hash_lock, flags);
	if (hash_hashed(&pdata->ndev_hash_link)) {
		hash_del_rcu(&pdata->ndev_hash_link);
		spin_unlock_irqrestore(&ndev_hash_lock, flags);
		 
		synchronize_rcu();
		spin_lock_irqsave(&ndev_hash_lock, flags);
	}
	if (pdata->netdev)
		hash_add_rcu(ndev_hash, &pdata->ndev_hash_link,
			     (uintptr_t)pdata->netdev);
	spin_unlock_irqrestore(&ndev_hash_lock, flags);
}

 
int ib_device_set_netdev(struct ib_device *ib_dev, struct net_device *ndev,
			 u32 port)
{
	struct net_device *old_ndev;
	struct ib_port_data *pdata;
	unsigned long flags;
	int ret;

	 
	ret = alloc_port_data(ib_dev);
	if (ret)
		return ret;

	if (!rdma_is_port_valid(ib_dev, port))
		return -EINVAL;

	pdata = &ib_dev->port_data[port];
	spin_lock_irqsave(&pdata->netdev_lock, flags);
	old_ndev = rcu_dereference_protected(
		pdata->netdev, lockdep_is_held(&pdata->netdev_lock));
	if (old_ndev == ndev) {
		spin_unlock_irqrestore(&pdata->netdev_lock, flags);
		return 0;
	}

	if (old_ndev)
		netdev_tracker_free(ndev, &pdata->netdev_tracker);
	if (ndev)
		netdev_hold(ndev, &pdata->netdev_tracker, GFP_ATOMIC);
	rcu_assign_pointer(pdata->netdev, ndev);
	spin_unlock_irqrestore(&pdata->netdev_lock, flags);

	add_ndev_hash(pdata);
	if (old_ndev)
		__dev_put(old_ndev);

	return 0;
}
EXPORT_SYMBOL(ib_device_set_netdev);

static void free_netdevs(struct ib_device *ib_dev)
{
	unsigned long flags;
	u32 port;

	if (!ib_dev->port_data)
		return;

	rdma_for_each_port (ib_dev, port) {
		struct ib_port_data *pdata = &ib_dev->port_data[port];
		struct net_device *ndev;

		spin_lock_irqsave(&pdata->netdev_lock, flags);
		ndev = rcu_dereference_protected(
			pdata->netdev, lockdep_is_held(&pdata->netdev_lock));
		if (ndev) {
			spin_lock(&ndev_hash_lock);
			hash_del_rcu(&pdata->ndev_hash_link);
			spin_unlock(&ndev_hash_lock);

			 
			rcu_assign_pointer(pdata->netdev, NULL);
			netdev_put(ndev, &pdata->netdev_tracker);
		}
		spin_unlock_irqrestore(&pdata->netdev_lock, flags);
	}
}

struct net_device *ib_device_get_netdev(struct ib_device *ib_dev,
					u32 port)
{
	struct ib_port_data *pdata;
	struct net_device *res;

	if (!rdma_is_port_valid(ib_dev, port))
		return NULL;

	pdata = &ib_dev->port_data[port];

	 
	if (ib_dev->ops.get_netdev)
		res = ib_dev->ops.get_netdev(ib_dev, port);
	else {
		spin_lock(&pdata->netdev_lock);
		res = rcu_dereference_protected(
			pdata->netdev, lockdep_is_held(&pdata->netdev_lock));
		if (res)
			dev_hold(res);
		spin_unlock(&pdata->netdev_lock);
	}

	 
	if (res && res->reg_state != NETREG_REGISTERED) {
		dev_put(res);
		return NULL;
	}

	return res;
}

 
struct ib_device *ib_device_get_by_netdev(struct net_device *ndev,
					  enum rdma_driver_id driver_id)
{
	struct ib_device *res = NULL;
	struct ib_port_data *cur;

	rcu_read_lock();
	hash_for_each_possible_rcu (ndev_hash, cur, ndev_hash_link,
				    (uintptr_t)ndev) {
		if (rcu_access_pointer(cur->netdev) == ndev &&
		    (driver_id == RDMA_DRIVER_UNKNOWN ||
		     cur->ib_dev->ops.driver_id == driver_id) &&
		    ib_device_try_get(cur->ib_dev)) {
			res = cur->ib_dev;
			break;
		}
	}
	rcu_read_unlock();

	return res;
}
EXPORT_SYMBOL(ib_device_get_by_netdev);

 
void ib_enum_roce_netdev(struct ib_device *ib_dev,
			 roce_netdev_filter filter,
			 void *filter_cookie,
			 roce_netdev_callback cb,
			 void *cookie)
{
	u32 port;

	rdma_for_each_port (ib_dev, port)
		if (rdma_protocol_roce(ib_dev, port)) {
			struct net_device *idev =
				ib_device_get_netdev(ib_dev, port);

			if (filter(ib_dev, port, idev, filter_cookie))
				cb(ib_dev, port, idev, cookie);

			if (idev)
				dev_put(idev);
		}
}

 
void ib_enum_all_roce_netdevs(roce_netdev_filter filter,
			      void *filter_cookie,
			      roce_netdev_callback cb,
			      void *cookie)
{
	struct ib_device *dev;
	unsigned long index;

	down_read(&devices_rwsem);
	xa_for_each_marked (&devices, index, dev, DEVICE_REGISTERED)
		ib_enum_roce_netdev(dev, filter, filter_cookie, cb, cookie);
	up_read(&devices_rwsem);
}

 
int ib_enum_all_devs(nldev_callback nldev_cb, struct sk_buff *skb,
		     struct netlink_callback *cb)
{
	unsigned long index;
	struct ib_device *dev;
	unsigned int idx = 0;
	int ret = 0;

	down_read(&devices_rwsem);
	xa_for_each_marked (&devices, index, dev, DEVICE_REGISTERED) {
		if (!rdma_dev_access_netns(dev, sock_net(skb->sk)))
			continue;

		ret = nldev_cb(dev, skb, cb, idx);
		if (ret)
			break;
		idx++;
	}
	up_read(&devices_rwsem);
	return ret;
}

 
int ib_query_pkey(struct ib_device *device,
		  u32 port_num, u16 index, u16 *pkey)
{
	if (!rdma_is_port_valid(device, port_num))
		return -EINVAL;

	if (!device->ops.query_pkey)
		return -EOPNOTSUPP;

	return device->ops.query_pkey(device, port_num, index, pkey);
}
EXPORT_SYMBOL(ib_query_pkey);

 
int ib_modify_device(struct ib_device *device,
		     int device_modify_mask,
		     struct ib_device_modify *device_modify)
{
	if (!device->ops.modify_device)
		return -EOPNOTSUPP;

	return device->ops.modify_device(device, device_modify_mask,
					 device_modify);
}
EXPORT_SYMBOL(ib_modify_device);

 
int ib_modify_port(struct ib_device *device,
		   u32 port_num, int port_modify_mask,
		   struct ib_port_modify *port_modify)
{
	int rc;

	if (!rdma_is_port_valid(device, port_num))
		return -EINVAL;

	if (device->ops.modify_port)
		rc = device->ops.modify_port(device, port_num,
					     port_modify_mask,
					     port_modify);
	else if (rdma_protocol_roce(device, port_num) &&
		 ((port_modify->set_port_cap_mask & ~IB_PORT_CM_SUP) == 0 ||
		  (port_modify->clr_port_cap_mask & ~IB_PORT_CM_SUP) == 0))
		rc = 0;
	else
		rc = -EOPNOTSUPP;
	return rc;
}
EXPORT_SYMBOL(ib_modify_port);

 
int ib_find_gid(struct ib_device *device, union ib_gid *gid,
		u32 *port_num, u16 *index)
{
	union ib_gid tmp_gid;
	u32 port;
	int ret, i;

	rdma_for_each_port (device, port) {
		if (!rdma_protocol_ib(device, port))
			continue;

		for (i = 0; i < device->port_data[port].immutable.gid_tbl_len;
		     ++i) {
			ret = rdma_query_gid(device, port, i, &tmp_gid);
			if (ret)
				continue;

			if (!memcmp(&tmp_gid, gid, sizeof *gid)) {
				*port_num = port;
				if (index)
					*index = i;
				return 0;
			}
		}
	}

	return -ENOENT;
}
EXPORT_SYMBOL(ib_find_gid);

 
int ib_find_pkey(struct ib_device *device,
		 u32 port_num, u16 pkey, u16 *index)
{
	int ret, i;
	u16 tmp_pkey;
	int partial_ix = -1;

	for (i = 0; i < device->port_data[port_num].immutable.pkey_tbl_len;
	     ++i) {
		ret = ib_query_pkey(device, port_num, i, &tmp_pkey);
		if (ret)
			return ret;
		if ((pkey & 0x7fff) == (tmp_pkey & 0x7fff)) {
			 
			if (tmp_pkey & 0x8000) {
				*index = i;
				return 0;
			}
			if (partial_ix < 0)
				partial_ix = i;
		}
	}

	 
	if (partial_ix >= 0) {
		*index = partial_ix;
		return 0;
	}
	return -ENOENT;
}
EXPORT_SYMBOL(ib_find_pkey);

 
struct net_device *ib_get_net_dev_by_params(struct ib_device *dev,
					    u32 port,
					    u16 pkey,
					    const union ib_gid *gid,
					    const struct sockaddr *addr)
{
	struct net_device *net_dev = NULL;
	unsigned long index;
	void *client_data;

	if (!rdma_protocol_ib(dev, port))
		return NULL;

	 
	down_read(&dev->client_data_rwsem);
	xan_for_each_marked (&dev->client_data, index, client_data,
			     CLIENT_DATA_REGISTERED) {
		struct ib_client *client = xa_load(&clients, index);

		if (!client || !client->get_net_dev_by_params)
			continue;

		net_dev = client->get_net_dev_by_params(dev, port, pkey, gid,
							addr, client_data);
		if (net_dev)
			break;
	}
	up_read(&dev->client_data_rwsem);

	return net_dev;
}
EXPORT_SYMBOL(ib_get_net_dev_by_params);

void ib_set_device_ops(struct ib_device *dev, const struct ib_device_ops *ops)
{
	struct ib_device_ops *dev_ops = &dev->ops;
#define SET_DEVICE_OP(ptr, name)                                               \
	do {                                                                   \
		if (ops->name)                                                 \
			if (!((ptr)->name))				       \
				(ptr)->name = ops->name;                       \
	} while (0)

#define SET_OBJ_SIZE(ptr, name) SET_DEVICE_OP(ptr, size_##name)

	if (ops->driver_id != RDMA_DRIVER_UNKNOWN) {
		WARN_ON(dev_ops->driver_id != RDMA_DRIVER_UNKNOWN &&
			dev_ops->driver_id != ops->driver_id);
		dev_ops->driver_id = ops->driver_id;
	}
	if (ops->owner) {
		WARN_ON(dev_ops->owner && dev_ops->owner != ops->owner);
		dev_ops->owner = ops->owner;
	}
	if (ops->uverbs_abi_ver)
		dev_ops->uverbs_abi_ver = ops->uverbs_abi_ver;

	dev_ops->uverbs_no_driver_id_binding |=
		ops->uverbs_no_driver_id_binding;

	SET_DEVICE_OP(dev_ops, add_gid);
	SET_DEVICE_OP(dev_ops, advise_mr);
	SET_DEVICE_OP(dev_ops, alloc_dm);
	SET_DEVICE_OP(dev_ops, alloc_hw_device_stats);
	SET_DEVICE_OP(dev_ops, alloc_hw_port_stats);
	SET_DEVICE_OP(dev_ops, alloc_mr);
	SET_DEVICE_OP(dev_ops, alloc_mr_integrity);
	SET_DEVICE_OP(dev_ops, alloc_mw);
	SET_DEVICE_OP(dev_ops, alloc_pd);
	SET_DEVICE_OP(dev_ops, alloc_rdma_netdev);
	SET_DEVICE_OP(dev_ops, alloc_ucontext);
	SET_DEVICE_OP(dev_ops, alloc_xrcd);
	SET_DEVICE_OP(dev_ops, attach_mcast);
	SET_DEVICE_OP(dev_ops, check_mr_status);
	SET_DEVICE_OP(dev_ops, counter_alloc_stats);
	SET_DEVICE_OP(dev_ops, counter_bind_qp);
	SET_DEVICE_OP(dev_ops, counter_dealloc);
	SET_DEVICE_OP(dev_ops, counter_unbind_qp);
	SET_DEVICE_OP(dev_ops, counter_update_stats);
	SET_DEVICE_OP(dev_ops, create_ah);
	SET_DEVICE_OP(dev_ops, create_counters);
	SET_DEVICE_OP(dev_ops, create_cq);
	SET_DEVICE_OP(dev_ops, create_flow);
	SET_DEVICE_OP(dev_ops, create_qp);
	SET_DEVICE_OP(dev_ops, create_rwq_ind_table);
	SET_DEVICE_OP(dev_ops, create_srq);
	SET_DEVICE_OP(dev_ops, create_user_ah);
	SET_DEVICE_OP(dev_ops, create_wq);
	SET_DEVICE_OP(dev_ops, dealloc_dm);
	SET_DEVICE_OP(dev_ops, dealloc_driver);
	SET_DEVICE_OP(dev_ops, dealloc_mw);
	SET_DEVICE_OP(dev_ops, dealloc_pd);
	SET_DEVICE_OP(dev_ops, dealloc_ucontext);
	SET_DEVICE_OP(dev_ops, dealloc_xrcd);
	SET_DEVICE_OP(dev_ops, del_gid);
	SET_DEVICE_OP(dev_ops, dereg_mr);
	SET_DEVICE_OP(dev_ops, destroy_ah);
	SET_DEVICE_OP(dev_ops, destroy_counters);
	SET_DEVICE_OP(dev_ops, destroy_cq);
	SET_DEVICE_OP(dev_ops, destroy_flow);
	SET_DEVICE_OP(dev_ops, destroy_flow_action);
	SET_DEVICE_OP(dev_ops, destroy_qp);
	SET_DEVICE_OP(dev_ops, destroy_rwq_ind_table);
	SET_DEVICE_OP(dev_ops, destroy_srq);
	SET_DEVICE_OP(dev_ops, destroy_wq);
	SET_DEVICE_OP(dev_ops, device_group);
	SET_DEVICE_OP(dev_ops, detach_mcast);
	SET_DEVICE_OP(dev_ops, disassociate_ucontext);
	SET_DEVICE_OP(dev_ops, drain_rq);
	SET_DEVICE_OP(dev_ops, drain_sq);
	SET_DEVICE_OP(dev_ops, enable_driver);
	SET_DEVICE_OP(dev_ops, fill_res_cm_id_entry);
	SET_DEVICE_OP(dev_ops, fill_res_cq_entry);
	SET_DEVICE_OP(dev_ops, fill_res_cq_entry_raw);
	SET_DEVICE_OP(dev_ops, fill_res_mr_entry);
	SET_DEVICE_OP(dev_ops, fill_res_mr_entry_raw);
	SET_DEVICE_OP(dev_ops, fill_res_qp_entry);
	SET_DEVICE_OP(dev_ops, fill_res_qp_entry_raw);
	SET_DEVICE_OP(dev_ops, fill_stat_mr_entry);
	SET_DEVICE_OP(dev_ops, get_dev_fw_str);
	SET_DEVICE_OP(dev_ops, get_dma_mr);
	SET_DEVICE_OP(dev_ops, get_hw_stats);
	SET_DEVICE_OP(dev_ops, get_link_layer);
	SET_DEVICE_OP(dev_ops, get_netdev);
	SET_DEVICE_OP(dev_ops, get_numa_node);
	SET_DEVICE_OP(dev_ops, get_port_immutable);
	SET_DEVICE_OP(dev_ops, get_vector_affinity);
	SET_DEVICE_OP(dev_ops, get_vf_config);
	SET_DEVICE_OP(dev_ops, get_vf_guid);
	SET_DEVICE_OP(dev_ops, get_vf_stats);
	SET_DEVICE_OP(dev_ops, iw_accept);
	SET_DEVICE_OP(dev_ops, iw_add_ref);
	SET_DEVICE_OP(dev_ops, iw_connect);
	SET_DEVICE_OP(dev_ops, iw_create_listen);
	SET_DEVICE_OP(dev_ops, iw_destroy_listen);
	SET_DEVICE_OP(dev_ops, iw_get_qp);
	SET_DEVICE_OP(dev_ops, iw_reject);
	SET_DEVICE_OP(dev_ops, iw_rem_ref);
	SET_DEVICE_OP(dev_ops, map_mr_sg);
	SET_DEVICE_OP(dev_ops, map_mr_sg_pi);
	SET_DEVICE_OP(dev_ops, mmap);
	SET_DEVICE_OP(dev_ops, mmap_free);
	SET_DEVICE_OP(dev_ops, modify_ah);
	SET_DEVICE_OP(dev_ops, modify_cq);
	SET_DEVICE_OP(dev_ops, modify_device);
	SET_DEVICE_OP(dev_ops, modify_hw_stat);
	SET_DEVICE_OP(dev_ops, modify_port);
	SET_DEVICE_OP(dev_ops, modify_qp);
	SET_DEVICE_OP(dev_ops, modify_srq);
	SET_DEVICE_OP(dev_ops, modify_wq);
	SET_DEVICE_OP(dev_ops, peek_cq);
	SET_DEVICE_OP(dev_ops, poll_cq);
	SET_DEVICE_OP(dev_ops, port_groups);
	SET_DEVICE_OP(dev_ops, post_recv);
	SET_DEVICE_OP(dev_ops, post_send);
	SET_DEVICE_OP(dev_ops, post_srq_recv);
	SET_DEVICE_OP(dev_ops, process_mad);
	SET_DEVICE_OP(dev_ops, query_ah);
	SET_DEVICE_OP(dev_ops, query_device);
	SET_DEVICE_OP(dev_ops, query_gid);
	SET_DEVICE_OP(dev_ops, query_pkey);
	SET_DEVICE_OP(dev_ops, query_port);
	SET_DEVICE_OP(dev_ops, query_qp);
	SET_DEVICE_OP(dev_ops, query_srq);
	SET_DEVICE_OP(dev_ops, query_ucontext);
	SET_DEVICE_OP(dev_ops, rdma_netdev_get_params);
	SET_DEVICE_OP(dev_ops, read_counters);
	SET_DEVICE_OP(dev_ops, reg_dm_mr);
	SET_DEVICE_OP(dev_ops, reg_user_mr);
	SET_DEVICE_OP(dev_ops, reg_user_mr_dmabuf);
	SET_DEVICE_OP(dev_ops, req_notify_cq);
	SET_DEVICE_OP(dev_ops, rereg_user_mr);
	SET_DEVICE_OP(dev_ops, resize_cq);
	SET_DEVICE_OP(dev_ops, set_vf_guid);
	SET_DEVICE_OP(dev_ops, set_vf_link_state);

	SET_OBJ_SIZE(dev_ops, ib_ah);
	SET_OBJ_SIZE(dev_ops, ib_counters);
	SET_OBJ_SIZE(dev_ops, ib_cq);
	SET_OBJ_SIZE(dev_ops, ib_mw);
	SET_OBJ_SIZE(dev_ops, ib_pd);
	SET_OBJ_SIZE(dev_ops, ib_qp);
	SET_OBJ_SIZE(dev_ops, ib_rwq_ind_table);
	SET_OBJ_SIZE(dev_ops, ib_srq);
	SET_OBJ_SIZE(dev_ops, ib_ucontext);
	SET_OBJ_SIZE(dev_ops, ib_xrcd);
}
EXPORT_SYMBOL(ib_set_device_ops);

#ifdef CONFIG_INFINIBAND_VIRT_DMA
int ib_dma_virt_map_sg(struct ib_device *dev, struct scatterlist *sg, int nents)
{
	struct scatterlist *s;
	int i;

	for_each_sg(sg, s, nents, i) {
		sg_dma_address(s) = (uintptr_t)sg_virt(s);
		sg_dma_len(s) = s->length;
	}
	return nents;
}
EXPORT_SYMBOL(ib_dma_virt_map_sg);
#endif  

static const struct rdma_nl_cbs ibnl_ls_cb_table[RDMA_NL_LS_NUM_OPS] = {
	[RDMA_NL_LS_OP_RESOLVE] = {
		.doit = ib_nl_handle_resolve_resp,
		.flags = RDMA_NL_ADMIN_PERM,
	},
	[RDMA_NL_LS_OP_SET_TIMEOUT] = {
		.doit = ib_nl_handle_set_timeout,
		.flags = RDMA_NL_ADMIN_PERM,
	},
	[RDMA_NL_LS_OP_IP_RESOLVE] = {
		.doit = ib_nl_handle_ip_res_resp,
		.flags = RDMA_NL_ADMIN_PERM,
	},
};

static int __init ib_core_init(void)
{
	int ret = -ENOMEM;

	ib_wq = alloc_workqueue("infiniband", 0, 0);
	if (!ib_wq)
		return -ENOMEM;

	ib_unreg_wq = alloc_workqueue("ib-unreg-wq", WQ_UNBOUND,
				      WQ_UNBOUND_MAX_ACTIVE);
	if (!ib_unreg_wq)
		goto err;

	ib_comp_wq = alloc_workqueue("ib-comp-wq",
			WQ_HIGHPRI | WQ_MEM_RECLAIM | WQ_SYSFS, 0);
	if (!ib_comp_wq)
		goto err_unbound;

	ib_comp_unbound_wq =
		alloc_workqueue("ib-comp-unb-wq",
				WQ_UNBOUND | WQ_HIGHPRI | WQ_MEM_RECLAIM |
				WQ_SYSFS, WQ_UNBOUND_MAX_ACTIVE);
	if (!ib_comp_unbound_wq)
		goto err_comp;

	ret = class_register(&ib_class);
	if (ret) {
		pr_warn("Couldn't create InfiniBand device class\n");
		goto err_comp_unbound;
	}

	rdma_nl_init();

	ret = addr_init();
	if (ret) {
		pr_warn("Couldn't init IB address resolution\n");
		goto err_ibnl;
	}

	ret = ib_mad_init();
	if (ret) {
		pr_warn("Couldn't init IB MAD\n");
		goto err_addr;
	}

	ret = ib_sa_init();
	if (ret) {
		pr_warn("Couldn't init SA\n");
		goto err_mad;
	}

	ret = register_blocking_lsm_notifier(&ibdev_lsm_nb);
	if (ret) {
		pr_warn("Couldn't register LSM notifier. ret %d\n", ret);
		goto err_sa;
	}

	ret = register_pernet_device(&rdma_dev_net_ops);
	if (ret) {
		pr_warn("Couldn't init compat dev. ret %d\n", ret);
		goto err_compat;
	}

	nldev_init();
	rdma_nl_register(RDMA_NL_LS, ibnl_ls_cb_table);
	ret = roce_gid_mgmt_init();
	if (ret) {
		pr_warn("Couldn't init RoCE GID management\n");
		goto err_parent;
	}

	return 0;

err_parent:
	rdma_nl_unregister(RDMA_NL_LS);
	nldev_exit();
	unregister_pernet_device(&rdma_dev_net_ops);
err_compat:
	unregister_blocking_lsm_notifier(&ibdev_lsm_nb);
err_sa:
	ib_sa_cleanup();
err_mad:
	ib_mad_cleanup();
err_addr:
	addr_cleanup();
err_ibnl:
	class_unregister(&ib_class);
err_comp_unbound:
	destroy_workqueue(ib_comp_unbound_wq);
err_comp:
	destroy_workqueue(ib_comp_wq);
err_unbound:
	destroy_workqueue(ib_unreg_wq);
err:
	destroy_workqueue(ib_wq);
	return ret;
}

static void __exit ib_core_cleanup(void)
{
	roce_gid_mgmt_cleanup();
	rdma_nl_unregister(RDMA_NL_LS);
	nldev_exit();
	unregister_pernet_device(&rdma_dev_net_ops);
	unregister_blocking_lsm_notifier(&ibdev_lsm_nb);
	ib_sa_cleanup();
	ib_mad_cleanup();
	addr_cleanup();
	rdma_nl_exit();
	class_unregister(&ib_class);
	destroy_workqueue(ib_comp_unbound_wq);
	destroy_workqueue(ib_comp_wq);
	 
	destroy_workqueue(ib_wq);
	destroy_workqueue(ib_unreg_wq);
	WARN_ON(!xa_empty(&clients));
	WARN_ON(!xa_empty(&devices));
}

MODULE_ALIAS_RDMA_NETLINK(RDMA_NL_LS, 4);

 
fs_initcall(ib_core_init);
module_exit(ib_core_cleanup);
