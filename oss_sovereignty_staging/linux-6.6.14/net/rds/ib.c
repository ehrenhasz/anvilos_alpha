 
#include <linux/kernel.h>
#include <linux/in.h>
#include <linux/if.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/if_arp.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <net/addrconf.h>

#include "rds_single_path.h"
#include "rds.h"
#include "ib.h"
#include "ib_mr.h"

static unsigned int rds_ib_mr_1m_pool_size = RDS_MR_1M_POOL_SIZE;
static unsigned int rds_ib_mr_8k_pool_size = RDS_MR_8K_POOL_SIZE;
unsigned int rds_ib_retry_count = RDS_IB_DEFAULT_RETRY_COUNT;
static atomic_t rds_ib_unloading;

module_param(rds_ib_mr_1m_pool_size, int, 0444);
MODULE_PARM_DESC(rds_ib_mr_1m_pool_size, " Max number of 1M mr per HCA");
module_param(rds_ib_mr_8k_pool_size, int, 0444);
MODULE_PARM_DESC(rds_ib_mr_8k_pool_size, " Max number of 8K mr per HCA");
module_param(rds_ib_retry_count, int, 0444);
MODULE_PARM_DESC(rds_ib_retry_count, " Number of hw retries before reporting an error");

 
DECLARE_RWSEM(rds_ib_devices_lock);
struct list_head rds_ib_devices;

 
DEFINE_SPINLOCK(ib_nodev_conns_lock);
LIST_HEAD(ib_nodev_conns);

static void rds_ib_nodev_connect(void)
{
	struct rds_ib_connection *ic;

	spin_lock(&ib_nodev_conns_lock);
	list_for_each_entry(ic, &ib_nodev_conns, ib_node)
		rds_conn_connect_if_down(ic->conn);
	spin_unlock(&ib_nodev_conns_lock);
}

static void rds_ib_dev_shutdown(struct rds_ib_device *rds_ibdev)
{
	struct rds_ib_connection *ic;
	unsigned long flags;

	spin_lock_irqsave(&rds_ibdev->spinlock, flags);
	list_for_each_entry(ic, &rds_ibdev->conn_list, ib_node)
		rds_conn_path_drop(&ic->conn->c_path[0], true);
	spin_unlock_irqrestore(&rds_ibdev->spinlock, flags);
}

 
static void rds_ib_dev_free(struct work_struct *work)
{
	struct rds_ib_ipaddr *i_ipaddr, *i_next;
	struct rds_ib_device *rds_ibdev = container_of(work,
					struct rds_ib_device, free_work);

	if (rds_ibdev->mr_8k_pool)
		rds_ib_destroy_mr_pool(rds_ibdev->mr_8k_pool);
	if (rds_ibdev->mr_1m_pool)
		rds_ib_destroy_mr_pool(rds_ibdev->mr_1m_pool);
	if (rds_ibdev->pd)
		ib_dealloc_pd(rds_ibdev->pd);

	list_for_each_entry_safe(i_ipaddr, i_next, &rds_ibdev->ipaddr_list, list) {
		list_del(&i_ipaddr->list);
		kfree(i_ipaddr);
	}

	kfree(rds_ibdev->vector_load);

	kfree(rds_ibdev);
}

void rds_ib_dev_put(struct rds_ib_device *rds_ibdev)
{
	BUG_ON(refcount_read(&rds_ibdev->refcount) == 0);
	if (refcount_dec_and_test(&rds_ibdev->refcount))
		queue_work(rds_wq, &rds_ibdev->free_work);
}

static int rds_ib_add_one(struct ib_device *device)
{
	struct rds_ib_device *rds_ibdev;
	int ret;

	 
	if (device->node_type != RDMA_NODE_IB_CA)
		return -EOPNOTSUPP;

	 
	if (!(device->attrs.device_cap_flags & IB_DEVICE_MEM_MGT_EXTENSIONS))
		return -EOPNOTSUPP;

	rds_ibdev = kzalloc_node(sizeof(struct rds_ib_device), GFP_KERNEL,
				 ibdev_to_node(device));
	if (!rds_ibdev)
		return -ENOMEM;

	spin_lock_init(&rds_ibdev->spinlock);
	refcount_set(&rds_ibdev->refcount, 1);
	INIT_WORK(&rds_ibdev->free_work, rds_ib_dev_free);

	INIT_LIST_HEAD(&rds_ibdev->ipaddr_list);
	INIT_LIST_HEAD(&rds_ibdev->conn_list);

	rds_ibdev->max_wrs = device->attrs.max_qp_wr;
	rds_ibdev->max_sge = min(device->attrs.max_send_sge, RDS_IB_MAX_SGE);

	rds_ibdev->odp_capable =
		!!(device->attrs.kernel_cap_flags &
		   IBK_ON_DEMAND_PAGING) &&
		!!(device->attrs.odp_caps.per_transport_caps.rc_odp_caps &
		   IB_ODP_SUPPORT_WRITE) &&
		!!(device->attrs.odp_caps.per_transport_caps.rc_odp_caps &
		   IB_ODP_SUPPORT_READ);

	rds_ibdev->max_1m_mrs = device->attrs.max_mr ?
		min_t(unsigned int, (device->attrs.max_mr / 2),
		      rds_ib_mr_1m_pool_size) : rds_ib_mr_1m_pool_size;

	rds_ibdev->max_8k_mrs = device->attrs.max_mr ?
		min_t(unsigned int, ((device->attrs.max_mr / 2) * RDS_MR_8K_SCALE),
		      rds_ib_mr_8k_pool_size) : rds_ib_mr_8k_pool_size;

	rds_ibdev->max_initiator_depth = device->attrs.max_qp_init_rd_atom;
	rds_ibdev->max_responder_resources = device->attrs.max_qp_rd_atom;

	rds_ibdev->vector_load = kcalloc(device->num_comp_vectors,
					 sizeof(int),
					 GFP_KERNEL);
	if (!rds_ibdev->vector_load) {
		pr_err("RDS/IB: %s failed to allocate vector memory\n",
			__func__);
		ret = -ENOMEM;
		goto put_dev;
	}

	rds_ibdev->dev = device;
	rds_ibdev->pd = ib_alloc_pd(device, 0);
	if (IS_ERR(rds_ibdev->pd)) {
		ret = PTR_ERR(rds_ibdev->pd);
		rds_ibdev->pd = NULL;
		goto put_dev;
	}

	rds_ibdev->mr_1m_pool =
		rds_ib_create_mr_pool(rds_ibdev, RDS_IB_MR_1M_POOL);
	if (IS_ERR(rds_ibdev->mr_1m_pool)) {
		ret = PTR_ERR(rds_ibdev->mr_1m_pool);
		rds_ibdev->mr_1m_pool = NULL;
		goto put_dev;
	}

	rds_ibdev->mr_8k_pool =
		rds_ib_create_mr_pool(rds_ibdev, RDS_IB_MR_8K_POOL);
	if (IS_ERR(rds_ibdev->mr_8k_pool)) {
		ret = PTR_ERR(rds_ibdev->mr_8k_pool);
		rds_ibdev->mr_8k_pool = NULL;
		goto put_dev;
	}

	rdsdebug("RDS/IB: max_mr = %d, max_wrs = %d, max_sge = %d, max_1m_mrs = %d, max_8k_mrs = %d\n",
		 device->attrs.max_mr, rds_ibdev->max_wrs, rds_ibdev->max_sge,
		 rds_ibdev->max_1m_mrs, rds_ibdev->max_8k_mrs);

	pr_info("RDS/IB: %s: added\n", device->name);

	down_write(&rds_ib_devices_lock);
	list_add_tail_rcu(&rds_ibdev->list, &rds_ib_devices);
	up_write(&rds_ib_devices_lock);
	refcount_inc(&rds_ibdev->refcount);

	ib_set_client_data(device, &rds_ib_client, rds_ibdev);

	rds_ib_nodev_connect();
	return 0;

put_dev:
	rds_ib_dev_put(rds_ibdev);
	return ret;
}

 
struct rds_ib_device *rds_ib_get_client_data(struct ib_device *device)
{
	struct rds_ib_device *rds_ibdev;

	rcu_read_lock();
	rds_ibdev = ib_get_client_data(device, &rds_ib_client);
	if (rds_ibdev)
		refcount_inc(&rds_ibdev->refcount);
	rcu_read_unlock();
	return rds_ibdev;
}

 
static void rds_ib_remove_one(struct ib_device *device, void *client_data)
{
	struct rds_ib_device *rds_ibdev = client_data;

	rds_ib_dev_shutdown(rds_ibdev);

	 
	ib_set_client_data(device, &rds_ib_client, NULL);

	down_write(&rds_ib_devices_lock);
	list_del_rcu(&rds_ibdev->list);
	up_write(&rds_ib_devices_lock);

	 
	synchronize_rcu();
	rds_ib_dev_put(rds_ibdev);
	rds_ib_dev_put(rds_ibdev);
}

struct ib_client rds_ib_client = {
	.name   = "rds_ib",
	.add    = rds_ib_add_one,
	.remove = rds_ib_remove_one
};

static int rds_ib_conn_info_visitor(struct rds_connection *conn,
				    void *buffer)
{
	struct rds_info_rdma_connection *iinfo = buffer;
	struct rds_ib_connection *ic = conn->c_transport_data;

	 
	if (conn->c_trans != &rds_ib_transport)
		return 0;
	if (conn->c_isv6)
		return 0;

	iinfo->src_addr = conn->c_laddr.s6_addr32[3];
	iinfo->dst_addr = conn->c_faddr.s6_addr32[3];
	if (ic) {
		iinfo->tos = conn->c_tos;
		iinfo->sl = ic->i_sl;
	}

	memset(&iinfo->src_gid, 0, sizeof(iinfo->src_gid));
	memset(&iinfo->dst_gid, 0, sizeof(iinfo->dst_gid));
	if (rds_conn_state(conn) == RDS_CONN_UP) {
		struct rds_ib_device *rds_ibdev;

		rdma_read_gids(ic->i_cm_id, (union ib_gid *)&iinfo->src_gid,
			       (union ib_gid *)&iinfo->dst_gid);

		rds_ibdev = ic->rds_ibdev;
		iinfo->max_send_wr = ic->i_send_ring.w_nr;
		iinfo->max_recv_wr = ic->i_recv_ring.w_nr;
		iinfo->max_send_sge = rds_ibdev->max_sge;
		rds_ib_get_mr_info(rds_ibdev, iinfo);
		iinfo->cache_allocs = atomic_read(&ic->i_cache_allocs);
	}
	return 1;
}

#if IS_ENABLED(CONFIG_IPV6)
 
static int rds6_ib_conn_info_visitor(struct rds_connection *conn,
				     void *buffer)
{
	struct rds6_info_rdma_connection *iinfo6 = buffer;
	struct rds_ib_connection *ic = conn->c_transport_data;

	 
	if (conn->c_trans != &rds_ib_transport)
		return 0;

	iinfo6->src_addr = conn->c_laddr;
	iinfo6->dst_addr = conn->c_faddr;
	if (ic) {
		iinfo6->tos = conn->c_tos;
		iinfo6->sl = ic->i_sl;
	}

	memset(&iinfo6->src_gid, 0, sizeof(iinfo6->src_gid));
	memset(&iinfo6->dst_gid, 0, sizeof(iinfo6->dst_gid));

	if (rds_conn_state(conn) == RDS_CONN_UP) {
		struct rds_ib_device *rds_ibdev;

		rdma_read_gids(ic->i_cm_id, (union ib_gid *)&iinfo6->src_gid,
			       (union ib_gid *)&iinfo6->dst_gid);
		rds_ibdev = ic->rds_ibdev;
		iinfo6->max_send_wr = ic->i_send_ring.w_nr;
		iinfo6->max_recv_wr = ic->i_recv_ring.w_nr;
		iinfo6->max_send_sge = rds_ibdev->max_sge;
		rds6_ib_get_mr_info(rds_ibdev, iinfo6);
		iinfo6->cache_allocs = atomic_read(&ic->i_cache_allocs);
	}
	return 1;
}
#endif

static void rds_ib_ic_info(struct socket *sock, unsigned int len,
			   struct rds_info_iterator *iter,
			   struct rds_info_lengths *lens)
{
	u64 buffer[(sizeof(struct rds_info_rdma_connection) + 7) / 8];

	rds_for_each_conn_info(sock, len, iter, lens,
				rds_ib_conn_info_visitor,
				buffer,
				sizeof(struct rds_info_rdma_connection));
}

#if IS_ENABLED(CONFIG_IPV6)
 
static void rds6_ib_ic_info(struct socket *sock, unsigned int len,
			    struct rds_info_iterator *iter,
			    struct rds_info_lengths *lens)
{
	u64 buffer[(sizeof(struct rds6_info_rdma_connection) + 7) / 8];

	rds_for_each_conn_info(sock, len, iter, lens,
			       rds6_ib_conn_info_visitor,
			       buffer,
			       sizeof(struct rds6_info_rdma_connection));
}
#endif

 
static int rds_ib_laddr_check(struct net *net, const struct in6_addr *addr,
			      __u32 scope_id)
{
	int ret;
	struct rdma_cm_id *cm_id;
#if IS_ENABLED(CONFIG_IPV6)
	struct sockaddr_in6 sin6;
#endif
	struct sockaddr_in sin;
	struct sockaddr *sa;
	bool isv4;

	isv4 = ipv6_addr_v4mapped(addr);
	 
	cm_id = rdma_create_id(&init_net, rds_rdma_cm_event_handler,
			       NULL, RDMA_PS_TCP, IB_QPT_RC);
	if (IS_ERR(cm_id))
		return PTR_ERR(cm_id);

	if (isv4) {
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = addr->s6_addr32[3];
		sa = (struct sockaddr *)&sin;
	} else {
#if IS_ENABLED(CONFIG_IPV6)
		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_addr = *addr;
		sin6.sin6_scope_id = scope_id;
		sa = (struct sockaddr *)&sin6;

		 
		if (ipv6_addr_type(addr) & IPV6_ADDR_LINKLOCAL) {
			struct net_device *dev;

			if (scope_id == 0) {
				ret = -EADDRNOTAVAIL;
				goto out;
			}

			 
			dev = dev_get_by_index(&init_net, scope_id);
			if (!dev) {
				ret = -EADDRNOTAVAIL;
				goto out;
			}
			if (!ipv6_chk_addr(&init_net, addr, dev, 1)) {
				dev_put(dev);
				ret = -EADDRNOTAVAIL;
				goto out;
			}
			dev_put(dev);
		}
#else
		ret = -EADDRNOTAVAIL;
		goto out;
#endif
	}

	 
	ret = rdma_bind_addr(cm_id, sa);
	 
	if (ret || !cm_id->device ||
	    cm_id->device->node_type != RDMA_NODE_IB_CA)
		ret = -EADDRNOTAVAIL;

	rdsdebug("addr %pI6c%%%u ret %d node type %d\n",
		 addr, scope_id, ret,
		 cm_id->device ? cm_id->device->node_type : -1);

out:
	rdma_destroy_id(cm_id);

	return ret;
}

static void rds_ib_unregister_client(void)
{
	ib_unregister_client(&rds_ib_client);
	 
	flush_workqueue(rds_wq);
}

static void rds_ib_set_unloading(void)
{
	atomic_set(&rds_ib_unloading, 1);
}

static bool rds_ib_is_unloading(struct rds_connection *conn)
{
	struct rds_conn_path *cp = &conn->c_path[0];

	return (test_bit(RDS_DESTROY_PENDING, &cp->cp_flags) ||
		atomic_read(&rds_ib_unloading) != 0);
}

void rds_ib_exit(void)
{
	rds_ib_set_unloading();
	synchronize_rcu();
	rds_info_deregister_func(RDS_INFO_IB_CONNECTIONS, rds_ib_ic_info);
#if IS_ENABLED(CONFIG_IPV6)
	rds_info_deregister_func(RDS6_INFO_IB_CONNECTIONS, rds6_ib_ic_info);
#endif
	rds_ib_unregister_client();
	rds_ib_destroy_nodev_conns();
	rds_ib_sysctl_exit();
	rds_ib_recv_exit();
	rds_trans_unregister(&rds_ib_transport);
	rds_ib_mr_exit();
}

static u8 rds_ib_get_tos_map(u8 tos)
{
	 
	return tos;
}

struct rds_transport rds_ib_transport = {
	.laddr_check		= rds_ib_laddr_check,
	.xmit_path_complete	= rds_ib_xmit_path_complete,
	.xmit			= rds_ib_xmit,
	.xmit_rdma		= rds_ib_xmit_rdma,
	.xmit_atomic		= rds_ib_xmit_atomic,
	.recv_path		= rds_ib_recv_path,
	.conn_alloc		= rds_ib_conn_alloc,
	.conn_free		= rds_ib_conn_free,
	.conn_path_connect	= rds_ib_conn_path_connect,
	.conn_path_shutdown	= rds_ib_conn_path_shutdown,
	.inc_copy_to_user	= rds_ib_inc_copy_to_user,
	.inc_free		= rds_ib_inc_free,
	.cm_initiate_connect	= rds_ib_cm_initiate_connect,
	.cm_handle_connect	= rds_ib_cm_handle_connect,
	.cm_connect_complete	= rds_ib_cm_connect_complete,
	.stats_info_copy	= rds_ib_stats_info_copy,
	.exit			= rds_ib_exit,
	.get_mr			= rds_ib_get_mr,
	.sync_mr		= rds_ib_sync_mr,
	.free_mr		= rds_ib_free_mr,
	.flush_mrs		= rds_ib_flush_mrs,
	.get_tos_map		= rds_ib_get_tos_map,
	.t_owner		= THIS_MODULE,
	.t_name			= "infiniband",
	.t_unloading		= rds_ib_is_unloading,
	.t_type			= RDS_TRANS_IB
};

int rds_ib_init(void)
{
	int ret;

	INIT_LIST_HEAD(&rds_ib_devices);

	ret = rds_ib_mr_init();
	if (ret)
		goto out;

	ret = ib_register_client(&rds_ib_client);
	if (ret)
		goto out_mr_exit;

	ret = rds_ib_sysctl_init();
	if (ret)
		goto out_ibreg;

	ret = rds_ib_recv_init();
	if (ret)
		goto out_sysctl;

	rds_trans_register(&rds_ib_transport);

	rds_info_register_func(RDS_INFO_IB_CONNECTIONS, rds_ib_ic_info);
#if IS_ENABLED(CONFIG_IPV6)
	rds_info_register_func(RDS6_INFO_IB_CONNECTIONS, rds6_ib_ic_info);
#endif

	goto out;

out_sysctl:
	rds_ib_sysctl_exit();
out_ibreg:
	rds_ib_unregister_client();
out_mr_exit:
	rds_ib_mr_exit();
out:
	return ret;
}

MODULE_LICENSE("GPL");
