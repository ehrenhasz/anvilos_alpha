
 

 

#include "rxe.h"

 
static int rxe_mcast_add(struct rxe_dev *rxe, union ib_gid *mgid)
{
	unsigned char ll_addr[ETH_ALEN];

	ipv6_eth_mc_map((struct in6_addr *)mgid->raw, ll_addr);

	return dev_mc_add(rxe->ndev, ll_addr);
}

 
static int rxe_mcast_del(struct rxe_dev *rxe, union ib_gid *mgid)
{
	unsigned char ll_addr[ETH_ALEN];

	ipv6_eth_mc_map((struct in6_addr *)mgid->raw, ll_addr);

	return dev_mc_del(rxe->ndev, ll_addr);
}

 
static void __rxe_insert_mcg(struct rxe_mcg *mcg)
{
	struct rb_root *tree = &mcg->rxe->mcg_tree;
	struct rb_node **link = &tree->rb_node;
	struct rb_node *node = NULL;
	struct rxe_mcg *tmp;
	int cmp;

	while (*link) {
		node = *link;
		tmp = rb_entry(node, struct rxe_mcg, node);

		cmp = memcmp(&tmp->mgid, &mcg->mgid, sizeof(mcg->mgid));
		if (cmp > 0)
			link = &(*link)->rb_left;
		else
			link = &(*link)->rb_right;
	}

	rb_link_node(&mcg->node, node, link);
	rb_insert_color(&mcg->node, tree);
}

 
static void __rxe_remove_mcg(struct rxe_mcg *mcg)
{
	rb_erase(&mcg->node, &mcg->rxe->mcg_tree);
}

 
static struct rxe_mcg *__rxe_lookup_mcg(struct rxe_dev *rxe,
					union ib_gid *mgid)
{
	struct rb_root *tree = &rxe->mcg_tree;
	struct rxe_mcg *mcg;
	struct rb_node *node;
	int cmp;

	node = tree->rb_node;

	while (node) {
		mcg = rb_entry(node, struct rxe_mcg, node);

		cmp = memcmp(&mcg->mgid, mgid, sizeof(*mgid));

		if (cmp > 0)
			node = node->rb_left;
		else if (cmp < 0)
			node = node->rb_right;
		else
			break;
	}

	if (node) {
		kref_get(&mcg->ref_cnt);
		return mcg;
	}

	return NULL;
}

 
struct rxe_mcg *rxe_lookup_mcg(struct rxe_dev *rxe, union ib_gid *mgid)
{
	struct rxe_mcg *mcg;

	spin_lock_bh(&rxe->mcg_lock);
	mcg = __rxe_lookup_mcg(rxe, mgid);
	spin_unlock_bh(&rxe->mcg_lock);

	return mcg;
}

 
static void __rxe_init_mcg(struct rxe_dev *rxe, union ib_gid *mgid,
			   struct rxe_mcg *mcg)
{
	kref_init(&mcg->ref_cnt);
	memcpy(&mcg->mgid, mgid, sizeof(mcg->mgid));
	INIT_LIST_HEAD(&mcg->qp_list);
	mcg->rxe = rxe;

	 
	kref_get(&mcg->ref_cnt);
	__rxe_insert_mcg(mcg);
}

 
static struct rxe_mcg *rxe_get_mcg(struct rxe_dev *rxe, union ib_gid *mgid)
{
	struct rxe_mcg *mcg, *tmp;
	int err;

	if (rxe->attr.max_mcast_grp == 0)
		return ERR_PTR(-EINVAL);

	 
	mcg = rxe_lookup_mcg(rxe, mgid);
	if (mcg)
		return mcg;

	 
	if (atomic_inc_return(&rxe->mcg_num) > rxe->attr.max_mcast_grp) {
		err = -ENOMEM;
		goto err_dec;
	}

	 
	mcg = kzalloc(sizeof(*mcg), GFP_KERNEL);
	if (!mcg) {
		err = -ENOMEM;
		goto err_dec;
	}

	spin_lock_bh(&rxe->mcg_lock);
	 
	tmp = __rxe_lookup_mcg(rxe, mgid);
	if (tmp) {
		spin_unlock_bh(&rxe->mcg_lock);
		atomic_dec(&rxe->mcg_num);
		kfree(mcg);
		return tmp;
	}

	__rxe_init_mcg(rxe, mgid, mcg);
	spin_unlock_bh(&rxe->mcg_lock);

	 
	err = rxe_mcast_add(rxe, mgid);
	if (!err)
		return mcg;

	kfree(mcg);
err_dec:
	atomic_dec(&rxe->mcg_num);
	return ERR_PTR(err);
}

 
void rxe_cleanup_mcg(struct kref *kref)
{
	struct rxe_mcg *mcg = container_of(kref, typeof(*mcg), ref_cnt);

	kfree(mcg);
}

 
static void __rxe_destroy_mcg(struct rxe_mcg *mcg)
{
	struct rxe_dev *rxe = mcg->rxe;

	 
	__rxe_remove_mcg(mcg);
	kref_put(&mcg->ref_cnt, rxe_cleanup_mcg);

	atomic_dec(&rxe->mcg_num);
}

 
static void rxe_destroy_mcg(struct rxe_mcg *mcg)
{
	 
	rxe_mcast_del(mcg->rxe, &mcg->mgid);

	spin_lock_bh(&mcg->rxe->mcg_lock);
	__rxe_destroy_mcg(mcg);
	spin_unlock_bh(&mcg->rxe->mcg_lock);
}

 
static int __rxe_init_mca(struct rxe_qp *qp, struct rxe_mcg *mcg,
			  struct rxe_mca *mca)
{
	struct rxe_dev *rxe = to_rdev(qp->ibqp.device);
	int n;

	n = atomic_inc_return(&rxe->mcg_attach);
	if (n > rxe->attr.max_total_mcast_qp_attach) {
		atomic_dec(&rxe->mcg_attach);
		return -ENOMEM;
	}

	n = atomic_inc_return(&mcg->qp_num);
	if (n > rxe->attr.max_mcast_qp_attach) {
		atomic_dec(&mcg->qp_num);
		atomic_dec(&rxe->mcg_attach);
		return -ENOMEM;
	}

	atomic_inc(&qp->mcg_num);

	rxe_get(qp);
	mca->qp = qp;

	list_add_tail(&mca->qp_list, &mcg->qp_list);

	return 0;
}

 
static int rxe_attach_mcg(struct rxe_mcg *mcg, struct rxe_qp *qp)
{
	struct rxe_dev *rxe = mcg->rxe;
	struct rxe_mca *mca, *tmp;
	int err;

	 
	spin_lock_bh(&rxe->mcg_lock);
	list_for_each_entry(mca, &mcg->qp_list, qp_list) {
		if (mca->qp == qp) {
			spin_unlock_bh(&rxe->mcg_lock);
			return 0;
		}
	}
	spin_unlock_bh(&rxe->mcg_lock);

	 
	mca = kzalloc(sizeof(*mca), GFP_KERNEL);
	if (!mca)
		return -ENOMEM;

	spin_lock_bh(&rxe->mcg_lock);
	 
	list_for_each_entry(tmp, &mcg->qp_list, qp_list) {
		if (tmp->qp == qp) {
			kfree(mca);
			err = 0;
			goto out;
		}
	}

	err = __rxe_init_mca(qp, mcg, mca);
	if (err)
		kfree(mca);
out:
	spin_unlock_bh(&rxe->mcg_lock);
	return err;
}

 
static void __rxe_cleanup_mca(struct rxe_mca *mca, struct rxe_mcg *mcg)
{
	list_del(&mca->qp_list);

	atomic_dec(&mcg->qp_num);
	atomic_dec(&mcg->rxe->mcg_attach);
	atomic_dec(&mca->qp->mcg_num);
	rxe_put(mca->qp);

	kfree(mca);
}

 
static int rxe_detach_mcg(struct rxe_mcg *mcg, struct rxe_qp *qp)
{
	struct rxe_dev *rxe = mcg->rxe;
	struct rxe_mca *mca, *tmp;

	spin_lock_bh(&rxe->mcg_lock);
	list_for_each_entry_safe(mca, tmp, &mcg->qp_list, qp_list) {
		if (mca->qp == qp) {
			__rxe_cleanup_mca(mca, mcg);

			 
			if (atomic_read(&mcg->qp_num) <= 0)
				__rxe_destroy_mcg(mcg);

			spin_unlock_bh(&rxe->mcg_lock);
			return 0;
		}
	}

	 
	spin_unlock_bh(&rxe->mcg_lock);
	return -EINVAL;
}

 
int rxe_attach_mcast(struct ib_qp *ibqp, union ib_gid *mgid, u16 mlid)
{
	int err;
	struct rxe_dev *rxe = to_rdev(ibqp->device);
	struct rxe_qp *qp = to_rqp(ibqp);
	struct rxe_mcg *mcg;

	 
	mcg = rxe_get_mcg(rxe, mgid);
	if (IS_ERR(mcg))
		return PTR_ERR(mcg);

	err = rxe_attach_mcg(mcg, qp);

	 
	if (atomic_read(&mcg->qp_num) == 0)
		rxe_destroy_mcg(mcg);

	kref_put(&mcg->ref_cnt, rxe_cleanup_mcg);

	return err;
}

 
int rxe_detach_mcast(struct ib_qp *ibqp, union ib_gid *mgid, u16 mlid)
{
	struct rxe_dev *rxe = to_rdev(ibqp->device);
	struct rxe_qp *qp = to_rqp(ibqp);
	struct rxe_mcg *mcg;
	int err;

	mcg = rxe_lookup_mcg(rxe, mgid);
	if (!mcg)
		return -EINVAL;

	err = rxe_detach_mcg(mcg, qp);
	kref_put(&mcg->ref_cnt, rxe_cleanup_mcg);

	return err;
}
