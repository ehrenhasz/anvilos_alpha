
 

#include <rdma/ib_mad.h>
#include "mad.h"
#include "vt.h"

 
int rvt_process_mad(struct ib_device *ibdev, int mad_flags, u32 port_num,
		    const struct ib_wc *in_wc, const struct ib_grh *in_grh,
		    const struct ib_mad_hdr *in, size_t in_mad_size,
		    struct ib_mad_hdr *out, size_t *out_mad_size,
		    u16 *out_mad_pkey_index)
{
	 
	return IB_MAD_RESULT_FAILURE;
}

static void rvt_send_mad_handler(struct ib_mad_agent *agent,
				 struct ib_mad_send_wc *mad_send_wc)
{
	ib_free_send_mad(mad_send_wc->send_buf);
}

 
int rvt_create_mad_agents(struct rvt_dev_info *rdi)
{
	struct ib_mad_agent *agent;
	struct rvt_ibport *rvp;
	int p;
	int ret;

	for (p = 0; p < rdi->dparms.nports; p++) {
		rvp = rdi->ports[p];
		agent = ib_register_mad_agent(&rdi->ibdev, p + 1,
					      IB_QPT_SMI,
					      NULL, 0, rvt_send_mad_handler,
					      NULL, NULL, 0);
		if (IS_ERR(agent)) {
			ret = PTR_ERR(agent);
			goto err;
		}

		rvp->send_agent = agent;

		if (rdi->driver_f.notify_create_mad_agent)
			rdi->driver_f.notify_create_mad_agent(rdi, p);
	}

	return 0;

err:
	for (p = 0; p < rdi->dparms.nports; p++) {
		rvp = rdi->ports[p];
		if (rvp->send_agent) {
			agent = rvp->send_agent;
			rvp->send_agent = NULL;
			ib_unregister_mad_agent(agent);
			if (rdi->driver_f.notify_free_mad_agent)
				rdi->driver_f.notify_free_mad_agent(rdi, p);
		}
	}

	return ret;
}

 
void rvt_free_mad_agents(struct rvt_dev_info *rdi)
{
	struct ib_mad_agent *agent;
	struct rvt_ibport *rvp;
	int p;

	for (p = 0; p < rdi->dparms.nports; p++) {
		rvp = rdi->ports[p];
		if (rvp->send_agent) {
			agent = rvp->send_agent;
			rvp->send_agent = NULL;
			ib_unregister_mad_agent(agent);
		}
		if (rvp->sm_ah) {
			rdma_destroy_ah(&rvp->sm_ah->ibah,
					RDMA_DESTROY_AH_SLEEPABLE);
			rvp->sm_ah = NULL;
		}

		if (rdi->driver_f.notify_free_mad_agent)
			rdi->driver_f.notify_free_mad_agent(rdi, p);
	}
}

