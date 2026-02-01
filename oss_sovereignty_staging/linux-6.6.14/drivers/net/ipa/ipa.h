 

 
#ifndef _IPA_H_
#define _IPA_H_

#include <linux/types.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/pm_wakeup.h>

#include "ipa_version.h"
#include "gsi.h"
#include "ipa_mem.h"
#include "ipa_qmi.h"
#include "ipa_endpoint.h"
#include "ipa_interrupt.h"

struct clk;
struct icc_path;
struct net_device;
struct platform_device;

struct ipa_power;
struct ipa_smp2p;
struct ipa_interrupt;

 
struct ipa {
	struct gsi gsi;
	enum ipa_version version;
	struct platform_device *pdev;
	struct completion completion;
	struct notifier_block nb;
	void *notifier;
	struct ipa_smp2p *smp2p;
	struct ipa_power *power;

	dma_addr_t table_addr;
	__le64 *table_virt;
	u32 route_count;
	u32 modem_route_count;
	u32 filter_count;

	struct ipa_interrupt *interrupt;
	bool uc_powered;
	bool uc_loaded;

	void __iomem *reg_virt;
	const struct regs *regs;

	dma_addr_t mem_addr;
	void *mem_virt;
	u32 mem_offset;
	u32 mem_size;
	u32 mem_count;
	const struct ipa_mem *mem;

	unsigned long imem_iova;
	size_t imem_size;

	unsigned long smem_iova;
	size_t smem_size;

	dma_addr_t zero_addr;
	void *zero_virt;
	size_t zero_size;

	 
	u32 endpoint_count;
	u32 available_count;
	unsigned long *defined;		 
	unsigned long *available;	 
	u64 filtered;			 
	unsigned long *set_up;
	unsigned long *enabled;

	u32 modem_tx_count;
	struct ipa_endpoint endpoint[IPA_ENDPOINT_MAX];
	struct ipa_endpoint *channel_map[GSI_CHANNEL_COUNT_MAX];
	struct ipa_endpoint *name_map[IPA_ENDPOINT_COUNT];

	bool setup_complete;

	atomic_t modem_state;		 
	struct net_device *modem_netdev;
	struct ipa_qmi qmi;
};

 
int ipa_setup(struct ipa *ipa);

#endif  
