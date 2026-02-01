
 

#include <linux/slab.h>
#include "ah.h"
#include "vt.h"  

 
int rvt_check_ah(struct ib_device *ibdev,
		 struct rdma_ah_attr *ah_attr)
{
	int err;
	int port_num = rdma_ah_get_port_num(ah_attr);
	struct ib_port_attr port_attr;
	struct rvt_dev_info *rdi = ib_to_rvt(ibdev);
	u8 ah_flags = rdma_ah_get_ah_flags(ah_attr);
	u8 static_rate = rdma_ah_get_static_rate(ah_attr);

	err = ib_query_port(ibdev, port_num, &port_attr);
	if (err)
		return -EINVAL;
	if (port_num < 1 ||
	    port_num > ibdev->phys_port_cnt)
		return -EINVAL;
	if (static_rate != IB_RATE_PORT_CURRENT &&
	    ib_rate_to_mbps(static_rate) < 0)
		return -EINVAL;
	if ((ah_flags & IB_AH_GRH) &&
	    rdma_ah_read_grh(ah_attr)->sgid_index >= port_attr.gid_tbl_len)
		return -EINVAL;
	if (rdi->driver_f.check_ah)
		return rdi->driver_f.check_ah(ibdev, ah_attr);
	return 0;
}
EXPORT_SYMBOL(rvt_check_ah);

 
int rvt_create_ah(struct ib_ah *ibah, struct rdma_ah_init_attr *init_attr,
		  struct ib_udata *udata)
{
	struct rvt_ah *ah = ibah_to_rvtah(ibah);
	struct rvt_dev_info *dev = ib_to_rvt(ibah->device);
	unsigned long flags;

	if (rvt_check_ah(ibah->device, init_attr->ah_attr))
		return -EINVAL;

	spin_lock_irqsave(&dev->n_ahs_lock, flags);
	if (dev->n_ahs_allocated == dev->dparms.props.max_ah) {
		spin_unlock_irqrestore(&dev->n_ahs_lock, flags);
		return -ENOMEM;
	}

	dev->n_ahs_allocated++;
	spin_unlock_irqrestore(&dev->n_ahs_lock, flags);

	rdma_copy_ah_attr(&ah->attr, init_attr->ah_attr);

	if (dev->driver_f.notify_new_ah)
		dev->driver_f.notify_new_ah(ibah->device,
					    init_attr->ah_attr, ah);

	return 0;
}

 
int rvt_destroy_ah(struct ib_ah *ibah, u32 destroy_flags)
{
	struct rvt_dev_info *dev = ib_to_rvt(ibah->device);
	struct rvt_ah *ah = ibah_to_rvtah(ibah);
	unsigned long flags;

	spin_lock_irqsave(&dev->n_ahs_lock, flags);
	dev->n_ahs_allocated--;
	spin_unlock_irqrestore(&dev->n_ahs_lock, flags);

	rdma_destroy_ah_attr(&ah->attr);
	return 0;
}

 
int rvt_modify_ah(struct ib_ah *ibah, struct rdma_ah_attr *ah_attr)
{
	struct rvt_ah *ah = ibah_to_rvtah(ibah);

	if (rvt_check_ah(ibah->device, ah_attr))
		return -EINVAL;

	ah->attr = *ah_attr;

	return 0;
}

 
int rvt_query_ah(struct ib_ah *ibah, struct rdma_ah_attr *ah_attr)
{
	struct rvt_ah *ah = ibah_to_rvtah(ibah);

	*ah_attr = ah->attr;

	return 0;
}
