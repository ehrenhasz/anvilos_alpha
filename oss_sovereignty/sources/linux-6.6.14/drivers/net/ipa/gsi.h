


#ifndef _GSI_H_
#define _GSI_H_

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/netdevice.h>

#include "ipa_version.h"


#define GSI_CHANNEL_COUNT_MAX	28
#define GSI_EVT_RING_COUNT_MAX	28


#define GSI_TLV_MAX		64

struct device;
struct scatterlist;
struct platform_device;

struct gsi;
struct gsi_trans;
struct gsi_channel_data;
struct ipa_gsi_endpoint_data;

struct gsi_ring {
	void *virt;			
	dma_addr_t addr;		
	u32 count;			

	
	u32 index;
};


struct gsi_trans_pool {
	void *base;			
	u32 count;			
	u32 free;			
	u32 size;			
	u32 max_alloc;			
	dma_addr_t addr;		
};

struct gsi_trans_info {
	atomic_t tre_avail;		

	u16 free_id;			
	u16 allocated_id;		
	u16 committed_id;		
	u16 pending_id;			
	u16 completed_id;		
	u16 polled_id;			
	struct gsi_trans *trans;	
	struct gsi_trans **map;		

	struct gsi_trans_pool sg_pool;	
	struct gsi_trans_pool cmd_pool;	
};


enum gsi_channel_state {
	GSI_CHANNEL_STATE_NOT_ALLOCATED		= 0x0,
	GSI_CHANNEL_STATE_ALLOCATED		= 0x1,
	GSI_CHANNEL_STATE_STARTED		= 0x2,
	GSI_CHANNEL_STATE_STOPPED		= 0x3,
	GSI_CHANNEL_STATE_STOP_IN_PROC		= 0x4,
	GSI_CHANNEL_STATE_FLOW_CONTROLLED	= 0x5,	
	GSI_CHANNEL_STATE_ERROR			= 0xf,
};


struct gsi_channel {
	struct gsi *gsi;
	bool toward_ipa;
	bool command;			

	u8 trans_tre_max;		
	u16 tre_count;
	u16 event_count;

	struct gsi_ring tre_ring;
	u32 evt_ring_id;

	
	u64 byte_count;			
	u64 trans_count;		
	u64 queued_byte_count;		
	u64 queued_trans_count;		
	u64 compl_byte_count;		
	u64 compl_trans_count;		

	struct gsi_trans_info trans_info;

	struct napi_struct napi;
};


enum gsi_evt_ring_state {
	GSI_EVT_RING_STATE_NOT_ALLOCATED	= 0x0,
	GSI_EVT_RING_STATE_ALLOCATED		= 0x1,
	GSI_EVT_RING_STATE_ERROR		= 0xf,
};

struct gsi_evt_ring {
	struct gsi_channel *channel;
	struct gsi_ring ring;
};

struct gsi {
	struct device *dev;		
	enum ipa_version version;
	void __iomem *virt;		
	const struct regs *regs;

	u32 irq;
	u32 channel_count;
	u32 evt_ring_count;
	u32 event_bitmap;		
	u32 modem_channel_bitmap;	
	u32 type_enabled_bitmap;	
	u32 ieob_enabled_bitmap;	
	int result;			
	struct completion completion;	
	struct mutex mutex;		
	struct gsi_channel channel[GSI_CHANNEL_COUNT_MAX];
	struct gsi_evt_ring evt_ring[GSI_EVT_RING_COUNT_MAX];
	struct net_device dummy_dev;	
};


int gsi_setup(struct gsi *gsi);


void gsi_teardown(struct gsi *gsi);


u32 gsi_channel_tre_max(struct gsi *gsi, u32 channel_id);


int gsi_channel_start(struct gsi *gsi, u32 channel_id);


int gsi_channel_stop(struct gsi *gsi, u32 channel_id);


void gsi_modem_channel_flow_control(struct gsi *gsi, u32 channel_id,
				    bool enable);


void gsi_channel_reset(struct gsi *gsi, u32 channel_id, bool doorbell);


void gsi_suspend(struct gsi *gsi);


void gsi_resume(struct gsi *gsi);


int gsi_channel_suspend(struct gsi *gsi, u32 channel_id);


int gsi_channel_resume(struct gsi *gsi, u32 channel_id);


int gsi_init(struct gsi *gsi, struct platform_device *pdev,
	     enum ipa_version version, u32 count,
	     const struct ipa_gsi_endpoint_data *data);


void gsi_exit(struct gsi *gsi);

#endif 
