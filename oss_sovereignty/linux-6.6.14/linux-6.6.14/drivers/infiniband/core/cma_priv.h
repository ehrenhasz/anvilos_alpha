#ifndef _CMA_PRIV_H
#define _CMA_PRIV_H
enum rdma_cm_state {
	RDMA_CM_IDLE,
	RDMA_CM_ADDR_QUERY,
	RDMA_CM_ADDR_RESOLVED,
	RDMA_CM_ROUTE_QUERY,
	RDMA_CM_ROUTE_RESOLVED,
	RDMA_CM_CONNECT,
	RDMA_CM_DISCONNECT,
	RDMA_CM_ADDR_BOUND,
	RDMA_CM_LISTEN,
	RDMA_CM_DEVICE_REMOVAL,
	RDMA_CM_DESTROYING
};
struct rdma_id_private {
	struct rdma_cm_id	id;
	struct rdma_bind_list	*bind_list;
	struct hlist_node	node;
	union {
		struct list_head device_item;  
		struct list_head listen_any_item;  
	};
	union {
		struct list_head listen_item;
		struct list_head listen_list;
	};
	struct list_head        id_list_entry;
	struct cma_device	*cma_dev;
	struct list_head	mc_list;
	int			internal_id;
	enum rdma_cm_state	state;
	spinlock_t		lock;
	struct mutex		qp_mutex;
	struct completion	comp;
	refcount_t refcount;
	struct mutex		handler_mutex;
	int			backlog;
	int			timeout_ms;
	struct ib_sa_query	*query;
	int			query_id;
	union {
		struct ib_cm_id	*ib;
		struct iw_cm_id	*iw;
	} cm_id;
	u32			seq_num;
	u32			qkey;
	u32			qp_num;
	u32			options;
	u8			srq;
	u8			tos;
	u8			tos_set:1;
	u8                      timeout_set:1;
	u8			min_rnr_timer_set:1;
	u8			reuseaddr;
	u8			afonly;
	u8			timeout;
	u8			min_rnr_timer;
	u8 used_resolve_ip;
	enum ib_gid_type	gid_type;
	struct rdma_restrack_entry     res;
	struct rdma_ucm_ece ece;
};
#if IS_ENABLED(CONFIG_INFINIBAND_ADDR_TRANS_CONFIGFS)
int cma_configfs_init(void);
void cma_configfs_exit(void);
#else
static inline int cma_configfs_init(void)
{
	return 0;
}
static inline void cma_configfs_exit(void)
{
}
#endif
void cma_dev_get(struct cma_device *dev);
void cma_dev_put(struct cma_device *dev);
typedef bool (*cma_device_filter)(struct ib_device *, void *);
struct cma_device *cma_enum_devices_by_ibdev(cma_device_filter filter,
					     void *cookie);
int cma_get_default_gid_type(struct cma_device *dev, u32 port);
int cma_set_default_gid_type(struct cma_device *dev, u32 port,
			     enum ib_gid_type default_gid_type);
int cma_get_default_roce_tos(struct cma_device *dev, u32 port);
int cma_set_default_roce_tos(struct cma_device *dev, u32 port,
			     u8 default_roce_tos);
struct ib_device *cma_get_ib_dev(struct cma_device *dev);
#endif  
