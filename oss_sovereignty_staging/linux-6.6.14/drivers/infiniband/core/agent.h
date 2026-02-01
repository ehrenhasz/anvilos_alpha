 

#ifndef __AGENT_H_
#define __AGENT_H_

#include <linux/err.h>
#include <rdma/ib_mad.h>

extern int ib_agent_port_open(struct ib_device *device, int port_num);

extern int ib_agent_port_close(struct ib_device *device, int port_num);

extern void agent_send_response(const struct ib_mad_hdr *mad_hdr, const struct ib_grh *grh,
				const struct ib_wc *wc, const struct ib_device *device,
				int port_num, int qpn, size_t resp_mad_len, bool opa);

#endif	 
