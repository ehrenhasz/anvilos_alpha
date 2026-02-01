
 

#include <linux/slab.h>
#include "pd.h"

 
int rvt_alloc_pd(struct ib_pd *ibpd, struct ib_udata *udata)
{
	struct ib_device *ibdev = ibpd->device;
	struct rvt_dev_info *dev = ib_to_rvt(ibdev);
	struct rvt_pd *pd = ibpd_to_rvtpd(ibpd);
	int ret = 0;

	 

	spin_lock(&dev->n_pds_lock);
	if (dev->n_pds_allocated == dev->dparms.props.max_pd) {
		spin_unlock(&dev->n_pds_lock);
		ret = -ENOMEM;
		goto bail;
	}

	dev->n_pds_allocated++;
	spin_unlock(&dev->n_pds_lock);

	 
	pd->user = !!udata;

bail:
	return ret;
}

 
int rvt_dealloc_pd(struct ib_pd *ibpd, struct ib_udata *udata)
{
	struct rvt_dev_info *dev = ib_to_rvt(ibpd->device);

	spin_lock(&dev->n_pds_lock);
	dev->n_pds_allocated--;
	spin_unlock(&dev->n_pds_lock);
	return 0;
}
