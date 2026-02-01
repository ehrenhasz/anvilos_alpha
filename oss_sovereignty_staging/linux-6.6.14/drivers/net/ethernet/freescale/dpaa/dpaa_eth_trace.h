 
 

#undef TRACE_SYSTEM
#define TRACE_SYSTEM	dpaa_eth

#if !defined(_DPAA_ETH_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _DPAA_ETH_TRACE_H

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include "dpaa_eth.h"
#include <linux/tracepoint.h>

#define fd_format_name(format)	{ qm_fd_##format, #format }
#define fd_format_list	\
	fd_format_name(contig),	\
	fd_format_name(sg)

 

 
DECLARE_EVENT_CLASS(dpaa_eth_fd,
	 
	TP_PROTO(struct net_device *netdev,
		 struct qman_fq *fq,
		 const struct qm_fd *fd),

	 
	TP_ARGS(netdev, fq, fd),

	 
	TP_STRUCT__entry(
		__field(u32,	fqid)
		__field(u64,	fd_addr)
		__field(u8,	fd_format)
		__field(u16,	fd_offset)
		__field(u32,	fd_length)
		__field(u32,	fd_status)
		__string(name,	netdev->name)
	),

	 
	TP_fast_assign(
		__entry->fqid = fq->fqid;
		__entry->fd_addr = qm_fd_addr_get64(fd);
		__entry->fd_format = qm_fd_get_format(fd);
		__entry->fd_offset = qm_fd_get_offset(fd);
		__entry->fd_length = qm_fd_get_length(fd);
		__entry->fd_status = fd->status;
		__assign_str(name, netdev->name);
	),

	 
	TP_printk("[%s] fqid=%d, fd: addr=0x%llx, format=%s, off=%u, len=%u, status=0x%08x",
		  __get_str(name), __entry->fqid, __entry->fd_addr,
		  __print_symbolic(__entry->fd_format, fd_format_list),
		  __entry->fd_offset, __entry->fd_length, __entry->fd_status)
);

 

 
DEFINE_EVENT(dpaa_eth_fd, dpaa_tx_fd,

	TP_PROTO(struct net_device *netdev,
		 struct qman_fq *fq,
		 const struct qm_fd *fd),

	TP_ARGS(netdev, fq, fd)
);

 
DEFINE_EVENT(dpaa_eth_fd, dpaa_rx_fd,

	TP_PROTO(struct net_device *netdev,
		 struct qman_fq *fq,
		 const struct qm_fd *fd),

	TP_ARGS(netdev, fq, fd)
);

 
DEFINE_EVENT(dpaa_eth_fd, dpaa_tx_conf_fd,

	TP_PROTO(struct net_device *netdev,
		 struct qman_fq *fq,
		 const struct qm_fd *fd),

	TP_ARGS(netdev, fq, fd)
);

 

#endif  

 
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE	dpaa_eth_trace
#include <trace/define_trace.h>
