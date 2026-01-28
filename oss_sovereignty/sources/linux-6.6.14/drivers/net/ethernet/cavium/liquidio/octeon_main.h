


#ifndef _OCTEON_MAIN_H_
#define  _OCTEON_MAIN_H_

#include <linux/sched/signal.h>

#if BITS_PER_LONG == 32
#define CVM_CAST64(v) ((long long)(v))
#elif BITS_PER_LONG == 64
#define CVM_CAST64(v) ((long long)(long)(v))
#else
#error "Unknown system architecture"
#endif

#define DRV_NAME "LiquidIO"

struct octeon_device_priv {
	
	struct tasklet_struct droq_tasklet;
	unsigned long napi_mask;
	struct octeon_device *dev;
};


struct octnet_buf_free_info {
	
	struct lio *lio;

	
	struct sk_buff *skb;

	
	struct octnic_gather *g;

	
	u64 dptr;

	
	struct octeon_soft_command *sc;
};


int octeon_report_sent_bytes_to_bql(void *buf, int reqtype);
void octeon_update_tx_completion_counters(void *buf, int reqtype,
					  unsigned int *pkts_compl,
					  unsigned int *bytes_compl);
void octeon_report_tx_completion_to_bql(void *txq, unsigned int pkts_compl,
					unsigned int bytes_compl);
void octeon_pf_changed_vf_macaddr(struct octeon_device *oct, u8 *mac);

void octeon_schedule_rxq_oom_work(struct octeon_device *oct,
				  struct octeon_droq *droq);


static inline void octeon_swap_8B_data(u64 *data, u32 blocks)
{
	while (blocks) {
		cpu_to_be64s(data);
		blocks--;
		data++;
	}
}


static inline void octeon_unmap_pci_barx(struct octeon_device *oct, int baridx)
{
	dev_dbg(&oct->pci_dev->dev, "Freeing PCI mapped regions for Bar%d\n",
		baridx);

	if (oct->mmio[baridx].done)
		iounmap(oct->mmio[baridx].hw_addr);

	if (oct->mmio[baridx].start)
		pci_release_region(oct->pci_dev, baridx * 2);
}


static inline int octeon_map_pci_barx(struct octeon_device *oct,
				      int baridx, int max_map_len)
{
	u32 mapped_len = 0;

	if (pci_request_region(oct->pci_dev, baridx * 2, DRV_NAME)) {
		dev_err(&oct->pci_dev->dev, "pci_request_region failed for bar %d\n",
			baridx);
		return 1;
	}

	oct->mmio[baridx].start = pci_resource_start(oct->pci_dev, baridx * 2);
	oct->mmio[baridx].len = pci_resource_len(oct->pci_dev, baridx * 2);

	mapped_len = oct->mmio[baridx].len;
	if (!mapped_len)
		goto err_release_region;

	if (max_map_len && (mapped_len > max_map_len))
		mapped_len = max_map_len;

	oct->mmio[baridx].hw_addr =
		ioremap(oct->mmio[baridx].start, mapped_len);
	oct->mmio[baridx].mapped_len = mapped_len;

	dev_dbg(&oct->pci_dev->dev, "BAR%d start: 0x%llx mapped %u of %u bytes\n",
		baridx, oct->mmio[baridx].start, mapped_len,
		oct->mmio[baridx].len);

	if (!oct->mmio[baridx].hw_addr) {
		dev_err(&oct->pci_dev->dev, "error ioremap for bar %d\n",
			baridx);
		goto err_release_region;
	}
	oct->mmio[baridx].done = 1;

	return 0;

err_release_region:
	pci_release_region(oct->pci_dev, baridx * 2);
	return 1;
}


static inline int
wait_for_sc_completion_timeout(struct octeon_device *oct_dev,
			       struct octeon_soft_command *sc,
			       unsigned long timeout)
{
	int errno = 0;
	long timeout_jiff;

	if (timeout)
		timeout_jiff = msecs_to_jiffies(timeout);
	else
		timeout_jiff = MAX_SCHEDULE_TIMEOUT;

	timeout_jiff =
		wait_for_completion_interruptible_timeout(&sc->complete,
							  timeout_jiff);
	if (timeout_jiff == 0) {
		dev_err(&oct_dev->pci_dev->dev, "%s: sc is timeout\n",
			__func__);
		WRITE_ONCE(sc->caller_is_done, true);
		errno = -ETIME;
	} else if (timeout_jiff == -ERESTARTSYS) {
		dev_err(&oct_dev->pci_dev->dev, "%s: sc is interrupted\n",
			__func__);
		WRITE_ONCE(sc->caller_is_done, true);
		errno = -EINTR;
	} else  if (sc->sc_status == OCTEON_REQUEST_TIMEOUT) {
		dev_err(&oct_dev->pci_dev->dev, "%s: sc has fatal timeout\n",
			__func__);
		WRITE_ONCE(sc->caller_is_done, true);
		errno = -EBUSY;
	}

	return errno;
}

#ifndef ROUNDUP4
#define ROUNDUP4(val) (((val) + 3) & 0xfffffffc)
#endif

#ifndef ROUNDUP8
#define ROUNDUP8(val) (((val) + 7) & 0xfffffff8)
#endif

#ifndef ROUNDUP16
#define ROUNDUP16(val) (((val) + 15) & 0xfffffff0)
#endif

#ifndef ROUNDUP128
#define ROUNDUP128(val) (((val) + 127) & 0xffffff80)
#endif

#endif 
