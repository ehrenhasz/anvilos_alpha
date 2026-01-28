#ifndef _IXGBE_FCOE_H
#define _IXGBE_FCOE_H
#include <scsi/fc/fc_fs.h>
#include <scsi/fc/fc_fcoe.h>
#define IXGBE_RXDADV_FCSTAT_SHIFT	4
#define IXGBE_BUFFCNT_MAX	256	 
#define IXGBE_FCPTR_ALIGN	16
#define IXGBE_FCPTR_MAX	(IXGBE_BUFFCNT_MAX * sizeof(dma_addr_t))
#define IXGBE_FCBUFF_4KB	0x0
#define IXGBE_FCBUFF_8KB	0x1
#define IXGBE_FCBUFF_16KB	0x2
#define IXGBE_FCBUFF_64KB	0x3
#define IXGBE_FCBUFF_MAX	65536	 
#define IXGBE_FCBUFF_MIN	4096	 
#define IXGBE_FCOE_DDP_MAX	512	 
#define IXGBE_FCOE_DDP_MAX_X550	2048	 
#define IXGBE_FCOE_DEFTC	3
#define IXGBE_FCERR_BADCRC       0x00100000
#define __IXGBE_FCOE_TARGET	1
struct ixgbe_fcoe_ddp {
	int len;
	u32 err;
	unsigned int sgc;
	struct scatterlist *sgl;
	dma_addr_t udp;
	u64 *udl;
	struct dma_pool *pool;
};
struct ixgbe_fcoe_ddp_pool {
	struct dma_pool *pool;
	u64 noddp;
	u64 noddp_ext_buff;
};
struct ixgbe_fcoe {
	struct ixgbe_fcoe_ddp_pool __percpu *ddp_pool;
	atomic_t refcnt;
	spinlock_t lock;
	struct ixgbe_fcoe_ddp ddp[IXGBE_FCOE_DDP_MAX_X550];
	void *extra_ddp_buffer;
	dma_addr_t extra_ddp_buffer_dma;
	unsigned long mode;
	u8 up;
};
#endif  
