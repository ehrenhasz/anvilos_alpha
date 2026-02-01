
 
#include <linux/atomic.h>
#include <linux/types.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/rcupdate.h>
#include <linux/rculist.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/sunrpc/xprt.h>
#include <linux/sunrpc/addr.h>
#include <linux/sunrpc/xprtmultipath.h>

#include "sysfs.h"

typedef struct rpc_xprt *(*xprt_switch_find_xprt_t)(struct rpc_xprt_switch *xps,
		const struct rpc_xprt *cur);

static const struct rpc_xprt_iter_ops rpc_xprt_iter_singular;
static const struct rpc_xprt_iter_ops rpc_xprt_iter_roundrobin;
static const struct rpc_xprt_iter_ops rpc_xprt_iter_listall;
static const struct rpc_xprt_iter_ops rpc_xprt_iter_listoffline;

static void xprt_switch_add_xprt_locked(struct rpc_xprt_switch *xps,
		struct rpc_xprt *xprt)
{
	if (unlikely(xprt_get(xprt) == NULL))
		return;
	list_add_tail_rcu(&xprt->xprt_switch, &xps->xps_xprt_list);
	smp_wmb();
	if (xps->xps_nxprts == 0)
		xps->xps_net = xprt->xprt_net;
	xps->xps_nxprts++;
	xps->xps_nactive++;
}

 
void rpc_xprt_switch_add_xprt(struct rpc_xprt_switch *xps,
		struct rpc_xprt *xprt)
{
	if (xprt == NULL)
		return;
	spin_lock(&xps->xps_lock);
	if (xps->xps_net == xprt->xprt_net || xps->xps_net == NULL)
		xprt_switch_add_xprt_locked(xps, xprt);
	spin_unlock(&xps->xps_lock);
	rpc_sysfs_xprt_setup(xps, xprt, GFP_KERNEL);
}

static void xprt_switch_remove_xprt_locked(struct rpc_xprt_switch *xps,
		struct rpc_xprt *xprt, bool offline)
{
	if (unlikely(xprt == NULL))
		return;
	if (!test_bit(XPRT_OFFLINE, &xprt->state) && offline)
		xps->xps_nactive--;
	xps->xps_nxprts--;
	if (xps->xps_nxprts == 0)
		xps->xps_net = NULL;
	smp_wmb();
	list_del_rcu(&xprt->xprt_switch);
}

 
void rpc_xprt_switch_remove_xprt(struct rpc_xprt_switch *xps,
		struct rpc_xprt *xprt, bool offline)
{
	spin_lock(&xps->xps_lock);
	xprt_switch_remove_xprt_locked(xps, xprt, offline);
	spin_unlock(&xps->xps_lock);
	xprt_put(xprt);
}

static DEFINE_IDA(rpc_xprtswitch_ids);

void xprt_multipath_cleanup_ids(void)
{
	ida_destroy(&rpc_xprtswitch_ids);
}

static int xprt_switch_alloc_id(struct rpc_xprt_switch *xps, gfp_t gfp_flags)
{
	int id;

	id = ida_alloc(&rpc_xprtswitch_ids, gfp_flags);
	if (id < 0)
		return id;

	xps->xps_id = id;
	return 0;
}

static void xprt_switch_free_id(struct rpc_xprt_switch *xps)
{
	ida_free(&rpc_xprtswitch_ids, xps->xps_id);
}

 
struct rpc_xprt_switch *xprt_switch_alloc(struct rpc_xprt *xprt,
		gfp_t gfp_flags)
{
	struct rpc_xprt_switch *xps;

	xps = kmalloc(sizeof(*xps), gfp_flags);
	if (xps != NULL) {
		spin_lock_init(&xps->xps_lock);
		kref_init(&xps->xps_kref);
		xprt_switch_alloc_id(xps, gfp_flags);
		xps->xps_nxprts = xps->xps_nactive = 0;
		atomic_long_set(&xps->xps_queuelen, 0);
		xps->xps_net = NULL;
		INIT_LIST_HEAD(&xps->xps_xprt_list);
		xps->xps_iter_ops = &rpc_xprt_iter_singular;
		rpc_sysfs_xprt_switch_setup(xps, xprt, gfp_flags);
		xprt_switch_add_xprt_locked(xps, xprt);
		xps->xps_nunique_destaddr_xprts = 1;
		rpc_sysfs_xprt_setup(xps, xprt, gfp_flags);
	}

	return xps;
}

static void xprt_switch_free_entries(struct rpc_xprt_switch *xps)
{
	spin_lock(&xps->xps_lock);
	while (!list_empty(&xps->xps_xprt_list)) {
		struct rpc_xprt *xprt;

		xprt = list_first_entry(&xps->xps_xprt_list,
				struct rpc_xprt, xprt_switch);
		xprt_switch_remove_xprt_locked(xps, xprt, true);
		spin_unlock(&xps->xps_lock);
		xprt_put(xprt);
		spin_lock(&xps->xps_lock);
	}
	spin_unlock(&xps->xps_lock);
}

static void xprt_switch_free(struct kref *kref)
{
	struct rpc_xprt_switch *xps = container_of(kref,
			struct rpc_xprt_switch, xps_kref);

	xprt_switch_free_entries(xps);
	rpc_sysfs_xprt_switch_destroy(xps);
	xprt_switch_free_id(xps);
	kfree_rcu(xps, xps_rcu);
}

 
struct rpc_xprt_switch *xprt_switch_get(struct rpc_xprt_switch *xps)
{
	if (xps != NULL && kref_get_unless_zero(&xps->xps_kref))
		return xps;
	return NULL;
}

 
void xprt_switch_put(struct rpc_xprt_switch *xps)
{
	if (xps != NULL)
		kref_put(&xps->xps_kref, xprt_switch_free);
}

 
void rpc_xprt_switch_set_roundrobin(struct rpc_xprt_switch *xps)
{
	if (READ_ONCE(xps->xps_iter_ops) != &rpc_xprt_iter_roundrobin)
		WRITE_ONCE(xps->xps_iter_ops, &rpc_xprt_iter_roundrobin);
}

static
const struct rpc_xprt_iter_ops *xprt_iter_ops(const struct rpc_xprt_iter *xpi)
{
	if (xpi->xpi_ops != NULL)
		return xpi->xpi_ops;
	return rcu_dereference(xpi->xpi_xpswitch)->xps_iter_ops;
}

static
void xprt_iter_no_rewind(struct rpc_xprt_iter *xpi)
{
}

static
void xprt_iter_default_rewind(struct rpc_xprt_iter *xpi)
{
	WRITE_ONCE(xpi->xpi_cursor, NULL);
}

static
bool xprt_is_active(const struct rpc_xprt *xprt)
{
	return (kref_read(&xprt->kref) != 0 &&
		!test_bit(XPRT_OFFLINE, &xprt->state));
}

static
struct rpc_xprt *xprt_switch_find_first_entry(struct list_head *head)
{
	struct rpc_xprt *pos;

	list_for_each_entry_rcu(pos, head, xprt_switch) {
		if (xprt_is_active(pos))
			return pos;
	}
	return NULL;
}

static
struct rpc_xprt *xprt_switch_find_first_entry_offline(struct list_head *head)
{
	struct rpc_xprt *pos;

	list_for_each_entry_rcu(pos, head, xprt_switch) {
		if (!xprt_is_active(pos))
			return pos;
	}
	return NULL;
}

static
struct rpc_xprt *xprt_iter_first_entry(struct rpc_xprt_iter *xpi)
{
	struct rpc_xprt_switch *xps = rcu_dereference(xpi->xpi_xpswitch);

	if (xps == NULL)
		return NULL;
	return xprt_switch_find_first_entry(&xps->xps_xprt_list);
}

static
struct rpc_xprt *_xprt_switch_find_current_entry(struct list_head *head,
						 const struct rpc_xprt *cur,
						 bool find_active)
{
	struct rpc_xprt *pos;
	bool found = false;

	list_for_each_entry_rcu(pos, head, xprt_switch) {
		if (cur == pos)
			found = true;
		if (found && ((find_active && xprt_is_active(pos)) ||
			      (!find_active && !xprt_is_active(pos))))
			return pos;
	}
	return NULL;
}

static
struct rpc_xprt *xprt_switch_find_current_entry(struct list_head *head,
						const struct rpc_xprt *cur)
{
	return _xprt_switch_find_current_entry(head, cur, true);
}

static
struct rpc_xprt * _xprt_iter_current_entry(struct rpc_xprt_iter *xpi,
		struct rpc_xprt *first_entry(struct list_head *head),
		struct rpc_xprt *current_entry(struct list_head *head,
					       const struct rpc_xprt *cur))
{
	struct rpc_xprt_switch *xps = rcu_dereference(xpi->xpi_xpswitch);
	struct list_head *head;

	if (xps == NULL)
		return NULL;
	head = &xps->xps_xprt_list;
	if (xpi->xpi_cursor == NULL || xps->xps_nxprts < 2)
		return first_entry(head);
	return current_entry(head, xpi->xpi_cursor);
}

static
struct rpc_xprt *xprt_iter_current_entry(struct rpc_xprt_iter *xpi)
{
	return _xprt_iter_current_entry(xpi, xprt_switch_find_first_entry,
			xprt_switch_find_current_entry);
}

static
struct rpc_xprt *xprt_switch_find_current_entry_offline(struct list_head *head,
		const struct rpc_xprt *cur)
{
	return _xprt_switch_find_current_entry(head, cur, false);
}

static
struct rpc_xprt *xprt_iter_current_entry_offline(struct rpc_xprt_iter *xpi)
{
	return _xprt_iter_current_entry(xpi,
			xprt_switch_find_first_entry_offline,
			xprt_switch_find_current_entry_offline);
}

bool rpc_xprt_switch_has_addr(struct rpc_xprt_switch *xps,
			      const struct sockaddr *sap)
{
	struct list_head *head;
	struct rpc_xprt *pos;

	if (xps == NULL || sap == NULL)
		return false;

	head = &xps->xps_xprt_list;
	list_for_each_entry_rcu(pos, head, xprt_switch) {
		if (rpc_cmp_addr_port(sap, (struct sockaddr *)&pos->addr)) {
			pr_info("RPC:   addr %s already in xprt switch\n",
				pos->address_strings[RPC_DISPLAY_ADDR]);
			return true;
		}
	}
	return false;
}

static
struct rpc_xprt *xprt_switch_find_next_entry(struct list_head *head,
		const struct rpc_xprt *cur, bool check_active)
{
	struct rpc_xprt *pos, *prev = NULL;
	bool found = false;

	list_for_each_entry_rcu(pos, head, xprt_switch) {
		if (cur == prev)
			found = true;
		 
		if (found && ((check_active && xprt_is_active(pos)) ||
			      (!check_active && !xprt_is_active(pos))))
			return pos;
		prev = pos;
	}
	return NULL;
}

static
struct rpc_xprt *xprt_switch_set_next_cursor(struct rpc_xprt_switch *xps,
		struct rpc_xprt **cursor,
		xprt_switch_find_xprt_t find_next)
{
	struct rpc_xprt *pos, *old;

	old = smp_load_acquire(cursor);
	pos = find_next(xps, old);
	smp_store_release(cursor, pos);
	return pos;
}

static
struct rpc_xprt *xprt_iter_next_entry_multiple(struct rpc_xprt_iter *xpi,
		xprt_switch_find_xprt_t find_next)
{
	struct rpc_xprt_switch *xps = rcu_dereference(xpi->xpi_xpswitch);

	if (xps == NULL)
		return NULL;
	return xprt_switch_set_next_cursor(xps, &xpi->xpi_cursor, find_next);
}

static
struct rpc_xprt *__xprt_switch_find_next_entry_roundrobin(struct list_head *head,
		const struct rpc_xprt *cur)
{
	struct rpc_xprt *ret;

	ret = xprt_switch_find_next_entry(head, cur, true);
	if (ret != NULL)
		return ret;
	return xprt_switch_find_first_entry(head);
}

static
struct rpc_xprt *xprt_switch_find_next_entry_roundrobin(struct rpc_xprt_switch *xps,
		const struct rpc_xprt *cur)
{
	struct list_head *head = &xps->xps_xprt_list;
	struct rpc_xprt *xprt;
	unsigned int nactive;

	for (;;) {
		unsigned long xprt_queuelen, xps_queuelen;

		xprt = __xprt_switch_find_next_entry_roundrobin(head, cur);
		if (!xprt)
			break;
		xprt_queuelen = atomic_long_read(&xprt->queuelen);
		xps_queuelen = atomic_long_read(&xps->xps_queuelen);
		nactive = READ_ONCE(xps->xps_nactive);
		 
		if (xprt_queuelen * nactive <= xps_queuelen)
			break;
		cur = xprt;
	}
	return xprt;
}

static
struct rpc_xprt *xprt_iter_next_entry_roundrobin(struct rpc_xprt_iter *xpi)
{
	return xprt_iter_next_entry_multiple(xpi,
			xprt_switch_find_next_entry_roundrobin);
}

static
struct rpc_xprt *xprt_switch_find_next_entry_all(struct rpc_xprt_switch *xps,
		const struct rpc_xprt *cur)
{
	return xprt_switch_find_next_entry(&xps->xps_xprt_list, cur, true);
}

static
struct rpc_xprt *xprt_switch_find_next_entry_offline(struct rpc_xprt_switch *xps,
		const struct rpc_xprt *cur)
{
	return xprt_switch_find_next_entry(&xps->xps_xprt_list, cur, false);
}

static
struct rpc_xprt *xprt_iter_next_entry_all(struct rpc_xprt_iter *xpi)
{
	return xprt_iter_next_entry_multiple(xpi,
			xprt_switch_find_next_entry_all);
}

static
struct rpc_xprt *xprt_iter_next_entry_offline(struct rpc_xprt_iter *xpi)
{
	return xprt_iter_next_entry_multiple(xpi,
			xprt_switch_find_next_entry_offline);
}

 
void xprt_iter_rewind(struct rpc_xprt_iter *xpi)
{
	rcu_read_lock();
	xprt_iter_ops(xpi)->xpi_rewind(xpi);
	rcu_read_unlock();
}

static void __xprt_iter_init(struct rpc_xprt_iter *xpi,
		struct rpc_xprt_switch *xps,
		const struct rpc_xprt_iter_ops *ops)
{
	rcu_assign_pointer(xpi->xpi_xpswitch, xprt_switch_get(xps));
	xpi->xpi_cursor = NULL;
	xpi->xpi_ops = ops;
}

 
void xprt_iter_init(struct rpc_xprt_iter *xpi,
		struct rpc_xprt_switch *xps)
{
	__xprt_iter_init(xpi, xps, NULL);
}

 
void xprt_iter_init_listall(struct rpc_xprt_iter *xpi,
		struct rpc_xprt_switch *xps)
{
	__xprt_iter_init(xpi, xps, &rpc_xprt_iter_listall);
}

void xprt_iter_init_listoffline(struct rpc_xprt_iter *xpi,
		struct rpc_xprt_switch *xps)
{
	__xprt_iter_init(xpi, xps, &rpc_xprt_iter_listoffline);
}

 
struct rpc_xprt_switch *xprt_iter_xchg_switch(struct rpc_xprt_iter *xpi,
		struct rpc_xprt_switch *newswitch)
{
	struct rpc_xprt_switch __rcu *oldswitch;

	 
	oldswitch = xchg(&xpi->xpi_xpswitch, RCU_INITIALIZER(newswitch));
	if (newswitch != NULL)
		xprt_iter_rewind(xpi);
	return rcu_dereference_protected(oldswitch, true);
}

 
void xprt_iter_destroy(struct rpc_xprt_iter *xpi)
{
	xprt_switch_put(xprt_iter_xchg_switch(xpi, NULL));
}

 
struct rpc_xprt *xprt_iter_xprt(struct rpc_xprt_iter *xpi)
{
	WARN_ON_ONCE(!rcu_read_lock_held());
	return xprt_iter_ops(xpi)->xpi_xprt(xpi);
}

static
struct rpc_xprt *xprt_iter_get_helper(struct rpc_xprt_iter *xpi,
		struct rpc_xprt *(*fn)(struct rpc_xprt_iter *))
{
	struct rpc_xprt *ret;

	do {
		ret = fn(xpi);
		if (ret == NULL)
			break;
		ret = xprt_get(ret);
	} while (ret == NULL);
	return ret;
}

 
struct rpc_xprt *xprt_iter_get_xprt(struct rpc_xprt_iter *xpi)
{
	struct rpc_xprt *xprt;

	rcu_read_lock();
	xprt = xprt_iter_get_helper(xpi, xprt_iter_ops(xpi)->xpi_xprt);
	rcu_read_unlock();
	return xprt;
}

 
struct rpc_xprt *xprt_iter_get_next(struct rpc_xprt_iter *xpi)
{
	struct rpc_xprt *xprt;

	rcu_read_lock();
	xprt = xprt_iter_get_helper(xpi, xprt_iter_ops(xpi)->xpi_next);
	rcu_read_unlock();
	return xprt;
}

 
static
const struct rpc_xprt_iter_ops rpc_xprt_iter_singular = {
	.xpi_rewind = xprt_iter_no_rewind,
	.xpi_xprt = xprt_iter_first_entry,
	.xpi_next = xprt_iter_first_entry,
};

 
static
const struct rpc_xprt_iter_ops rpc_xprt_iter_roundrobin = {
	.xpi_rewind = xprt_iter_default_rewind,
	.xpi_xprt = xprt_iter_current_entry,
	.xpi_next = xprt_iter_next_entry_roundrobin,
};

 
static
const struct rpc_xprt_iter_ops rpc_xprt_iter_listall = {
	.xpi_rewind = xprt_iter_default_rewind,
	.xpi_xprt = xprt_iter_current_entry,
	.xpi_next = xprt_iter_next_entry_all,
};

static
const struct rpc_xprt_iter_ops rpc_xprt_iter_listoffline = {
	.xpi_rewind = xprt_iter_default_rewind,
	.xpi_xprt = xprt_iter_current_entry_offline,
	.xpi_next = xprt_iter_next_entry_offline,
};
