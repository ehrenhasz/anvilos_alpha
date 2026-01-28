




#ifndef _E1000_H_
#define _E1000_H_

#include <linux/stddef.h>
#include <linux/module.h>
#include <linux/types.h>
#include <asm/byteorder.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/pagemap.h>
#include <linux/dma-mapping.h>
#include <linux/bitops.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/capability.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <net/pkt_sched.h>
#include <linux/list.h>
#include <linux/reboot.h>
#include <net/checksum.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>

#define BAR_0		0
#define BAR_1		1

#define INTEL_E1000_ETHERNET_DEVICE(device_id) {\
	PCI_DEVICE(PCI_VENDOR_ID_INTEL, device_id)}

struct e1000_adapter;

#include "e1000_hw.h"

#define E1000_MAX_INTR			10


#define E1000_CHECK_RESET_COUNT	50


#define E1000_DEFAULT_TXD		256
#define E1000_MAX_TXD			256
#define E1000_MIN_TXD			48
#define E1000_MAX_82544_TXD		4096

#define E1000_DEFAULT_RXD		256
#define E1000_MAX_RXD			256
#define E1000_MIN_RXD			48
#define E1000_MAX_82544_RXD		4096

#define E1000_MIN_ITR_USECS		10 
#define E1000_MAX_ITR_USECS		10000 


#define MAXIMUM_ETHERNET_VLAN_SIZE	1522


#define E1000_RXBUFFER_128		128    
#define E1000_RXBUFFER_256		256    
#define E1000_RXBUFFER_512		512
#define E1000_RXBUFFER_1024		1024
#define E1000_RXBUFFER_2048		2048
#define E1000_RXBUFFER_4096		4096
#define E1000_RXBUFFER_8192		8192
#define E1000_RXBUFFER_16384		16384


#define E1000_SMARTSPEED_DOWNSHIFT	3
#define E1000_SMARTSPEED_MAX		15


#define E1000_PBA_BYTES_SHIFT		0xA
#define E1000_TX_HEAD_ADDR_SHIFT	7
#define E1000_PBA_TX_MASK		0xFFFF0000


#define E1000_FC_HIGH_DIFF	0x1638 
#define E1000_FC_LOW_DIFF	0x1640 

#define E1000_FC_PAUSE_TIME	0xFFFF 


#define E1000_TX_QUEUE_WAKE	16

#define E1000_RX_BUFFER_WRITE	16 

#define AUTO_ALL_MODES		0
#define E1000_EEPROM_82544_APM	0x0004
#define E1000_EEPROM_APME	0x0400

#ifndef E1000_MASTER_SLAVE

#define E1000_MASTER_SLAVE	e1000_ms_hw_default
#endif

#define E1000_MNG_VLAN_NONE	(-1)


struct e1000_tx_buffer {
	struct sk_buff *skb;
	dma_addr_t dma;
	unsigned long time_stamp;
	u16 length;
	u16 next_to_watch;
	bool mapped_as_page;
	unsigned short segs;
	unsigned int bytecount;
};

struct e1000_rx_buffer {
	union {
		struct page *page; 
		u8 *data; 
	} rxbuf;
	dma_addr_t dma;
};

struct e1000_tx_ring {
	
	void *desc;
	
	dma_addr_t dma;
	
	unsigned int size;
	
	unsigned int count;
	
	unsigned int next_to_use;
	
	unsigned int next_to_clean;
	
	struct e1000_tx_buffer *buffer_info;

	u16 tdh;
	u16 tdt;
	bool last_tx_tso;
};

struct e1000_rx_ring {
	
	void *desc;
	
	dma_addr_t dma;
	
	unsigned int size;
	
	unsigned int count;
	
	unsigned int next_to_use;
	
	unsigned int next_to_clean;
	
	struct e1000_rx_buffer *buffer_info;
	struct sk_buff *rx_skb_top;

	
	int cpu;

	u16 rdh;
	u16 rdt;
};

#define E1000_DESC_UNUSED(R)						\
({									\
	unsigned int clean = smp_load_acquire(&(R)->next_to_clean);	\
	unsigned int use = READ_ONCE((R)->next_to_use);			\
	(clean > use ? 0 : (R)->count) + clean - use - 1;		\
})

#define E1000_RX_DESC_EXT(R, i)						\
	(&(((union e1000_rx_desc_extended *)((R).desc))[i]))
#define E1000_GET_DESC(R, i, type)	(&(((struct type *)((R).desc))[i]))
#define E1000_RX_DESC(R, i)		E1000_GET_DESC(R, i, e1000_rx_desc)
#define E1000_TX_DESC(R, i)		E1000_GET_DESC(R, i, e1000_tx_desc)
#define E1000_CONTEXT_DESC(R, i)	E1000_GET_DESC(R, i, e1000_context_desc)



struct e1000_adapter {
	unsigned long active_vlans[BITS_TO_LONGS(VLAN_N_VID)];
	u16 mng_vlan_id;
	u32 bd_number;
	u32 rx_buffer_len;
	u32 wol;
	u32 smartspeed;
	u32 en_mng_pt;
	u16 link_speed;
	u16 link_duplex;
	spinlock_t stats_lock;
	unsigned int total_tx_bytes;
	unsigned int total_tx_packets;
	unsigned int total_rx_bytes;
	unsigned int total_rx_packets;
	
	u32 itr;
	u32 itr_setting;
	u16 tx_itr;
	u16 rx_itr;

	u8 fc_autoneg;

	
	struct e1000_tx_ring *tx_ring;      
	unsigned int restart_queue;
	u32 txd_cmd;
	u32 tx_int_delay;
	u32 tx_abs_int_delay;
	u32 gotcl;
	u64 gotcl_old;
	u64 tpt_old;
	u64 colc_old;
	u32 tx_timeout_count;
	u32 tx_fifo_head;
	u32 tx_head_addr;
	u32 tx_fifo_size;
	u8  tx_timeout_factor;
	atomic_t tx_fifo_stall;
	bool pcix_82544;
	bool detect_tx_hung;
	bool dump_buffers;

	
	bool (*clean_rx)(struct e1000_adapter *adapter,
			 struct e1000_rx_ring *rx_ring,
			 int *work_done, int work_to_do);
	void (*alloc_rx_buf)(struct e1000_adapter *adapter,
			     struct e1000_rx_ring *rx_ring,
			     int cleaned_count);
	struct e1000_rx_ring *rx_ring;      
	struct napi_struct napi;

	int num_tx_queues;
	int num_rx_queues;

	u64 hw_csum_err;
	u64 hw_csum_good;
	u32 alloc_rx_buff_failed;
	u32 rx_int_delay;
	u32 rx_abs_int_delay;
	bool rx_csum;
	u32 gorcl;
	u64 gorcl_old;

	
	struct net_device *netdev;
	struct pci_dev *pdev;

	
	struct e1000_hw hw;
	struct e1000_hw_stats stats;
	struct e1000_phy_info phy_info;
	struct e1000_phy_stats phy_stats;

	u32 test_icr;
	struct e1000_tx_ring test_tx_ring;
	struct e1000_rx_ring test_rx_ring;

	int msg_enable;

	
	bool tso_force;
	bool smart_power_down;	
	bool quad_port_a;
	unsigned long flags;
	u32 eeprom_wol;

	
	int bars;
	int need_ioport;

	bool discarding;

	struct work_struct reset_task;
	struct delayed_work watchdog_task;
	struct delayed_work fifo_stall_task;
	struct delayed_work phy_info_task;
};

enum e1000_state_t {
	__E1000_TESTING,
	__E1000_RESETTING,
	__E1000_DOWN,
	__E1000_DISABLED
};

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

struct net_device *e1000_get_hw_dev(struct e1000_hw *hw);
#define e_dbg(format, arg...) \
	netdev_dbg(e1000_get_hw_dev(hw), format, ## arg)
#define e_err(msglvl, format, arg...) \
	netif_err(adapter, msglvl, adapter->netdev, format, ## arg)
#define e_info(msglvl, format, arg...) \
	netif_info(adapter, msglvl, adapter->netdev, format, ## arg)
#define e_warn(msglvl, format, arg...) \
	netif_warn(adapter, msglvl, adapter->netdev, format, ## arg)
#define e_notice(msglvl, format, arg...) \
	netif_notice(adapter, msglvl, adapter->netdev, format, ## arg)
#define e_dev_info(format, arg...) \
	dev_info(&adapter->pdev->dev, format, ## arg)
#define e_dev_warn(format, arg...) \
	dev_warn(&adapter->pdev->dev, format, ## arg)
#define e_dev_err(format, arg...) \
	dev_err(&adapter->pdev->dev, format, ## arg)

extern char e1000_driver_name[];

int e1000_open(struct net_device *netdev);
int e1000_close(struct net_device *netdev);
int e1000_up(struct e1000_adapter *adapter);
void e1000_down(struct e1000_adapter *adapter);
void e1000_reinit_locked(struct e1000_adapter *adapter);
void e1000_reset(struct e1000_adapter *adapter);
int e1000_set_spd_dplx(struct e1000_adapter *adapter, u32 spd, u8 dplx);
int e1000_setup_all_rx_resources(struct e1000_adapter *adapter);
int e1000_setup_all_tx_resources(struct e1000_adapter *adapter);
void e1000_free_all_rx_resources(struct e1000_adapter *adapter);
void e1000_free_all_tx_resources(struct e1000_adapter *adapter);
void e1000_update_stats(struct e1000_adapter *adapter);
bool e1000_has_link(struct e1000_adapter *adapter);
void e1000_power_up_phy(struct e1000_adapter *);
void e1000_set_ethtool_ops(struct net_device *netdev);
void e1000_check_options(struct e1000_adapter *adapter);

#endif 
