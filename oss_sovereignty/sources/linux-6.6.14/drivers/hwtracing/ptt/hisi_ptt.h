


#ifndef _HISI_PTT_H
#define _HISI_PTT_H

#include <linux/bits.h>
#include <linux/cpumask.h>
#include <linux/device.h>
#include <linux/kfifo.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/pci.h>
#include <linux/perf_event.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#define DRV_NAME "hisi_ptt"


#define HISI_PTT_TUNING_CTRL		0x0000
#define   HISI_PTT_TUNING_CTRL_CODE	GENMASK(15, 0)
#define   HISI_PTT_TUNING_CTRL_SUB	GENMASK(23, 16)
#define HISI_PTT_TUNING_DATA		0x0004
#define   HISI_PTT_TUNING_DATA_VAL_MASK	GENMASK(15, 0)
#define HISI_PTT_TRACE_ADDR_SIZE	0x0800
#define HISI_PTT_TRACE_ADDR_BASE_LO_0	0x0810
#define HISI_PTT_TRACE_ADDR_BASE_HI_0	0x0814
#define HISI_PTT_TRACE_ADDR_STRIDE	0x8
#define HISI_PTT_TRACE_CTRL		0x0850
#define   HISI_PTT_TRACE_CTRL_EN	BIT(0)
#define   HISI_PTT_TRACE_CTRL_RST	BIT(1)
#define   HISI_PTT_TRACE_CTRL_RXTX_SEL	GENMASK(3, 2)
#define   HISI_PTT_TRACE_CTRL_TYPE_SEL	GENMASK(7, 4)
#define   HISI_PTT_TRACE_CTRL_DATA_FORMAT	BIT(14)
#define   HISI_PTT_TRACE_CTRL_FILTER_MODE	BIT(15)
#define   HISI_PTT_TRACE_CTRL_TARGET_SEL	GENMASK(31, 16)
#define HISI_PTT_TRACE_INT_STAT		0x0890
#define   HISI_PTT_TRACE_INT_STAT_MASK	GENMASK(3, 0)
#define HISI_PTT_TRACE_INT_MASK		0x0894
#define HISI_PTT_TUNING_INT_STAT	0x0898
#define   HISI_PTT_TUNING_INT_STAT_MASK	BIT(0)
#define HISI_PTT_TRACE_WR_STS		0x08a0
#define   HISI_PTT_TRACE_WR_STS_WRITE	GENMASK(27, 0)
#define   HISI_PTT_TRACE_WR_STS_BUFFER	GENMASK(29, 28)
#define HISI_PTT_TRACE_STS		0x08b0
#define   HISI_PTT_TRACE_IDLE		BIT(0)
#define HISI_PTT_DEVICE_RANGE		0x0fe0
#define   HISI_PTT_DEVICE_RANGE_UPPER	GENMASK(31, 16)
#define   HISI_PTT_DEVICE_RANGE_LOWER	GENMASK(15, 0)
#define HISI_PTT_LOCATION		0x0fe8
#define   HISI_PTT_CORE_ID		GENMASK(15, 0)
#define   HISI_PTT_SICL_ID		GENMASK(31, 16)


#define HISI_PTT_TRACE_DMA_IRQ			0
#define HISI_PTT_TRACE_BUF_CNT			4
#define HISI_PTT_TRACE_BUF_SIZE			SZ_4M
#define HISI_PTT_TRACE_TOTAL_BUF_SIZE		(HISI_PTT_TRACE_BUF_SIZE * \
						 HISI_PTT_TRACE_BUF_CNT)

#define HISI_PTT_RESET_TIMEOUT_US	10UL
#define HISI_PTT_RESET_POLL_INTERVAL_US	1UL

#define HISI_PTT_WAIT_TUNE_TIMEOUT_US	1000000UL
#define HISI_PTT_WAIT_TRACE_TIMEOUT_US	100UL
#define HISI_PTT_WAIT_POLL_INTERVAL_US	10UL


#define HISI_PTT_FILTER_UPDATE_FIFO_SIZE	16

#define HISI_PTT_WORK_DELAY_MS			100UL

#define HISI_PCIE_CORE_PORT_ID(devfn)	((PCI_SLOT(devfn) & 0x7) << 1)


#define HISI_PTT_PMU_FILTER_IS_PORT	BIT(19)
#define HISI_PTT_PMU_FILTER_VAL_MASK	GENMASK(15, 0)
#define HISI_PTT_PMU_DIRECTION_MASK	GENMASK(23, 20)
#define HISI_PTT_PMU_TYPE_MASK		GENMASK(31, 24)
#define HISI_PTT_PMU_FORMAT_MASK	GENMASK(35, 32)


struct hisi_ptt_tune_desc {
	struct hisi_ptt *hisi_ptt;
	const char *name;
	u32 event_code;
};


struct hisi_ptt_dma_buffer {
	dma_addr_t dma;
	void *addr;
};


struct hisi_ptt_trace_ctrl {
	struct hisi_ptt_dma_buffer *trace_buf;
	struct perf_output_handle handle;
	u32 buf_index;
	int on_cpu;
	bool started;
	bool is_port;
	u32 direction:2;
	u32 filter:16;
	u32 format:1;
	u32 type:4;
};


#define HISI_PTT_RP_FILTERS_GRP_NAME	"root_port_filters"
#define HISI_PTT_REQ_FILTERS_GRP_NAME	"requester_filters"


struct hisi_ptt_filter_desc {
	struct device_attribute attr;
	struct list_head list;
	bool is_port;
	char *name;
	u16 devid;
};


struct hisi_ptt_filter_update_info {
	bool is_port;
	bool is_add;
	u16 devid;
};


struct hisi_ptt_pmu_buf {
	size_t length;
	int nr_pages;
	void *base;
	long pos;
};


struct hisi_ptt {
	struct hisi_ptt_trace_ctrl trace_ctrl;
	struct notifier_block hisi_ptt_nb;
	struct hlist_node hotplug_node;
	struct pmu hisi_ptt_pmu;
	void __iomem *iobase;
	struct pci_dev *pdev;
	struct mutex tune_lock;
	spinlock_t pmu_lock;
	int trace_irq;
	u32 upper_bdf;
	u32 lower_bdf;

	
	struct list_head port_filters;
	struct list_head req_filters;
	struct mutex filter_lock;
	bool sysfs_inited;
	u16 port_mask;

	
	struct delayed_work work;
	spinlock_t filter_update_lock;
	DECLARE_KFIFO(filter_update_kfifo, struct hisi_ptt_filter_update_info,
		      HISI_PTT_FILTER_UPDATE_FIFO_SIZE);
};

#define to_hisi_ptt(pmu) container_of(pmu, struct hisi_ptt, hisi_ptt_pmu)

#endif 
