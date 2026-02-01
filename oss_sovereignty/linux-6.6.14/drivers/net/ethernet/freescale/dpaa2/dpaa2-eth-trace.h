 
 

#undef TRACE_SYSTEM
#define TRACE_SYSTEM	dpaa2_eth

#if !defined(_DPAA2_ETH_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _DPAA2_ETH_TRACE_H

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/tracepoint.h>

#define TR_FMT "[%s] fd: addr=0x%llx, len=%u, off=%u"
 
#define TR_BUF_FMT "[%s] vaddr=%p size=%zu dma_addr=%pad map_size=%zu bpid=%d"

 

 
DECLARE_EVENT_CLASS(dpaa2_eth_fd,
		     
		    TP_PROTO(struct net_device *netdev,
			     const struct dpaa2_fd *fd),

		     
		    TP_ARGS(netdev, fd),

		     
		    TP_STRUCT__entry(
				     __field(u64, fd_addr)
				     __field(u32, fd_len)
				     __field(u16, fd_offset)
				     __string(name, netdev->name)
		    ),

		     
		    TP_fast_assign(
				   __entry->fd_addr = dpaa2_fd_get_addr(fd);
				   __entry->fd_len = dpaa2_fd_get_len(fd);
				   __entry->fd_offset = dpaa2_fd_get_offset(fd);
				   __assign_str(name, netdev->name);
		    ),

		     
		    TP_printk(TR_FMT,
			      __get_str(name),
			      __entry->fd_addr,
			      __entry->fd_len,
			      __entry->fd_offset)
);

 

 
DEFINE_EVENT(dpaa2_eth_fd, dpaa2_tx_fd,
	     TP_PROTO(struct net_device *netdev,
		      const struct dpaa2_fd *fd),

	     TP_ARGS(netdev, fd)
);

 
DEFINE_EVENT(dpaa2_eth_fd, dpaa2_tx_xsk_fd,
	     TP_PROTO(struct net_device *netdev,
		      const struct dpaa2_fd *fd),

	     TP_ARGS(netdev, fd)
);

 
DEFINE_EVENT(dpaa2_eth_fd, dpaa2_rx_fd,
	     TP_PROTO(struct net_device *netdev,
		      const struct dpaa2_fd *fd),

	     TP_ARGS(netdev, fd)
);

 
DEFINE_EVENT(dpaa2_eth_fd, dpaa2_rx_xsk_fd,
	     TP_PROTO(struct net_device *netdev,
		      const struct dpaa2_fd *fd),

	     TP_ARGS(netdev, fd)
);

 
DEFINE_EVENT(dpaa2_eth_fd, dpaa2_tx_conf_fd,
	     TP_PROTO(struct net_device *netdev,
		      const struct dpaa2_fd *fd),

	     TP_ARGS(netdev, fd)
);

 
DECLARE_EVENT_CLASS(dpaa2_eth_buf,
		     
		    TP_PROTO(struct net_device *netdev,
			      
			    void *vaddr,
			    size_t size,
			     
			    dma_addr_t dma_addr,
			    size_t map_size,
			     
			    u16 bpid),

		     
		    TP_ARGS(netdev, vaddr, size, dma_addr, map_size, bpid),

		     
		    TP_STRUCT__entry(
				      __field(void *, vaddr)
				      __field(size_t, size)
				      __field(dma_addr_t, dma_addr)
				      __field(size_t, map_size)
				      __field(u16, bpid)
				      __string(name, netdev->name)
		    ),

		     
		    TP_fast_assign(
				   __entry->vaddr = vaddr;
				   __entry->size = size;
				   __entry->dma_addr = dma_addr;
				   __entry->map_size = map_size;
				   __entry->bpid = bpid;
				   __assign_str(name, netdev->name);
		    ),

		     
		    TP_printk(TR_BUF_FMT,
			      __get_str(name),
			      __entry->vaddr,
			      __entry->size,
			      &__entry->dma_addr,
			      __entry->map_size,
			      __entry->bpid)
);

 
DEFINE_EVENT(dpaa2_eth_buf, dpaa2_eth_buf_seed,
	     TP_PROTO(struct net_device *netdev,
		      void *vaddr,
		      size_t size,
		      dma_addr_t dma_addr,
		      size_t map_size,
		      u16 bpid),

	     TP_ARGS(netdev, vaddr, size, dma_addr, map_size, bpid)
);

 
DEFINE_EVENT(dpaa2_eth_buf, dpaa2_xsk_buf_seed,
	     TP_PROTO(struct net_device *netdev,
		      void *vaddr,
		      size_t size,
		      dma_addr_t dma_addr,
		      size_t map_size,
		      u16 bpid),

	     TP_ARGS(netdev, vaddr, size, dma_addr, map_size, bpid)
);

 

#endif  

 
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE	dpaa2-eth-trace
#include <trace/define_trace.h>
