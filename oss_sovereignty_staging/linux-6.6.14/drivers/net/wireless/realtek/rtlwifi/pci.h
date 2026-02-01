 
 

#ifndef __RTL_PCI_H__
#define __RTL_PCI_H__

#include <linux/pci.h>
 
#define RTL_PCI_RX_MPDU_QUEUE			0
#define RTL_PCI_RX_CMD_QUEUE			1
#define RTL_PCI_MAX_RX_QUEUE			2

#define RTL_PCI_MAX_RX_COUNT			512 
#define RTL_PCI_MAX_TX_QUEUE_COUNT		9

#define RT_TXDESC_NUM				128
#define TX_DESC_NUM_92E				512
#define TX_DESC_NUM_8822B			512
#define RT_TXDESC_NUM_BE_QUEUE			256

#define BK_QUEUE				0
#define BE_QUEUE				1
#define VI_QUEUE				2
#define VO_QUEUE				3
#define BEACON_QUEUE				4
#define TXCMD_QUEUE				5
#define MGNT_QUEUE				6
#define HIGH_QUEUE				7
#define HCCA_QUEUE				8
#define H2C_QUEUE				TXCMD_QUEUE	 

#define RTL_PCI_DEVICE(vend, dev, cfg)  \
	.vendor = (vend), \
	.device = (dev), \
	.subvendor = PCI_ANY_ID, \
	.subdevice = PCI_ANY_ID,\
	.driver_data = (kernel_ulong_t)&(cfg)

#define INTEL_VENDOR_ID				0x8086
#define SIS_VENDOR_ID				0x1039
#define ATI_VENDOR_ID				0x1002
#define ATI_DEVICE_ID				0x7914
#define AMD_VENDOR_ID				0x1022

#define PCI_MAX_BRIDGE_NUMBER			255
#define PCI_MAX_DEVICES				32
#define PCI_MAX_FUNCTION			8

#define PCI_CONF_ADDRESS	0x0CF8	 
#define PCI_CONF_DATA		0x0CFC	 

#define PCI_CLASS_BRIDGE_DEV		0x06
#define PCI_SUBCLASS_BR_PCI_TO_PCI	0x04
#define PCI_CAPABILITY_ID_PCI_EXPRESS	0x10
#define PCI_CAP_ID_EXP			0x10

#define U1DONTCARE			0xFF
#define U2DONTCARE			0xFFFF
#define U4DONTCARE			0xFFFFFFFF

#define RTL_PCI_8192_DID	0x8192	 
#define RTL_PCI_8192SE_DID	0x8192	 
#define RTL_PCI_8174_DID	0x8174	 
#define RTL_PCI_8173_DID	0x8173	 
#define RTL_PCI_8172_DID	0x8172	 
#define RTL_PCI_8171_DID	0x8171	 
#define RTL_PCI_8723AE_DID	0x8723	 
#define RTL_PCI_0045_DID	0x0045	 
#define RTL_PCI_0046_DID	0x0046	 
#define RTL_PCI_0044_DID	0x0044	 
#define RTL_PCI_0047_DID	0x0047	 
#define RTL_PCI_700F_DID	0x700F
#define RTL_PCI_701F_DID	0x701F
#define RTL_PCI_DLINK_DID	0x3304
#define RTL_PCI_8723AE_DID	0x8723	 
#define RTL_PCI_8192CET_DID	0x8191	 
#define RTL_PCI_8192CE_DID	0x8178	 
#define RTL_PCI_8191CE_DID	0x8177	 
#define RTL_PCI_8188CE_DID	0x8176	 
#define RTL_PCI_8192CU_DID	0x8191	 
#define RTL_PCI_8192DE_DID	0x8193	 
#define RTL_PCI_8192DE_DID2	0x002B	 
#define RTL_PCI_8188EE_DID	0x8179   
#define RTL_PCI_8723BE_DID	0xB723   
#define RTL_PCI_8192EE_DID	0x818B	 
#define RTL_PCI_8821AE_DID	0x8821	 
#define RTL_PCI_8812AE_DID	0x8812	 
#define RTL_PCI_8822BE_DID	0xB822	 

 
#define RTL_MEM_MAPPED_IO_RANGE_8190PCI		0x1000
#define RTL_MEM_MAPPED_IO_RANGE_8192PCIE	0x4000
#define RTL_MEM_MAPPED_IO_RANGE_8192SE		0x4000
#define RTL_MEM_MAPPED_IO_RANGE_8192CE		0x4000
#define RTL_MEM_MAPPED_IO_RANGE_8192DE		0x4000

#define RTL_PCI_REVISION_ID_8190PCI		0x00
#define RTL_PCI_REVISION_ID_8192PCIE		0x01
#define RTL_PCI_REVISION_ID_8192SE		0x10
#define RTL_PCI_REVISION_ID_8192CE		0x1
#define RTL_PCI_REVISION_ID_8192DE		0x0

#define RTL_DEFAULT_HARDWARE_TYPE	HARDWARE_TYPE_RTL8192CE

enum pci_bridge_vendor {
	PCI_BRIDGE_VENDOR_INTEL = 0x0,	 
	PCI_BRIDGE_VENDOR_ATI,		 
	PCI_BRIDGE_VENDOR_AMD,		 
	PCI_BRIDGE_VENDOR_SIS,		 
	PCI_BRIDGE_VENDOR_UNKNOWN,	 
	PCI_BRIDGE_VENDOR_MAX,
};

struct rtl_pci_capabilities_header {
	u8 capability_id;
	u8 next;
};

 
struct rtl_tx_buffer_desc {
	u32 dword[4 * (1 << (BUFDESC_SEG_NUM + 1))];
} __packed;

struct rtl_tx_desc {
	u32 dword[16];
} __packed;

struct rtl_rx_buffer_desc {  
	u32 dword[4];
} __packed;

struct rtl_rx_desc {  
	u32 dword[8];
} __packed;

struct rtl_tx_cmd_desc {
	u32 dword[16];
} __packed;

struct rtl8192_tx_ring {
	struct rtl_tx_desc *desc;
	dma_addr_t dma;
	unsigned int idx;
	unsigned int entries;
	struct sk_buff_head queue;
	 
	struct rtl_tx_buffer_desc *buffer_desc;  
	dma_addr_t buffer_desc_dma;  
	u16 cur_tx_wp;  
	u16 cur_tx_rp;  
};

struct rtl8192_rx_ring {
	struct rtl_rx_desc *desc;
	dma_addr_t dma;
	unsigned int idx;
	struct sk_buff *rx_buf[RTL_PCI_MAX_RX_COUNT];
	 
	struct rtl_rx_buffer_desc *buffer_desc;  
	u16 next_rx_rp;  
};

struct rtl_pci {
	struct pci_dev *pdev;
	bool irq_enabled;

	bool driver_is_goingto_unload;
	bool up_first_time;
	bool first_init;
	bool being_init_adapter;
	bool init_ready;

	 
	struct rtl8192_tx_ring tx_ring[RTL_PCI_MAX_TX_QUEUE_COUNT];
	int txringcount[RTL_PCI_MAX_TX_QUEUE_COUNT];
	u32 transmit_config;

	 
	struct rtl8192_rx_ring rx_ring[RTL_PCI_MAX_RX_QUEUE];
	int rxringcount;
	u16 rxbuffersize;
	u32 receive_config;

	 
	u8 irq_alloc;
	u32 irq_mask[4];	 
	u32 sys_irq_mask;

	 
	u32 reg_bcn_ctrl_val;

	   u8 const_pci_aspm;
	u8 const_amdpci_aspm;
	u8 const_hwsw_rfoff_d3;
	u8 const_support_pciaspm;
	 
	u8 const_hostpci_aspm_setting;
	 
	u8 const_devicepci_aspm_setting;
	 
	bool support_aspm;
	bool support_backdoor;

	 
	enum acm_method acm_method;

	u16 shortretry_limit;
	u16 longretry_limit;

	 
	bool msi_support;
	bool using_msi;
	 
	bool int_clear;
};

struct mp_adapter {
	u8 linkctrl_reg;

	u8 busnumber;
	u8 devnumber;
	u8 funcnumber;

	u8 pcibridge_busnum;
	u8 pcibridge_devnum;
	u8 pcibridge_funcnum;

	u8 pcibridge_vendor;
	u16 pcibridge_vendorid;
	u16 pcibridge_deviceid;

	bool amd_l1_patch;
};

struct rtl_pci_priv {
	struct bt_coexist_info bt_coexist;
	struct rtl_led_ctl ledctl;
	struct rtl_pci dev;
	struct mp_adapter ndis_adapter;
};

#define rtl_pcipriv(hw)		(((struct rtl_pci_priv *)(rtl_priv(hw))->priv))
#define rtl_pcidev(pcipriv)	(&((pcipriv)->dev))

int rtl_pci_reset_trx_ring(struct ieee80211_hw *hw);

extern const struct rtl_intf_ops rtl_pci_ops;

int rtl_pci_probe(struct pci_dev *pdev,
		  const struct pci_device_id *id);
void rtl_pci_disconnect(struct pci_dev *pdev);
#ifdef CONFIG_PM_SLEEP
int rtl_pci_suspend(struct device *dev);
int rtl_pci_resume(struct device *dev);
#endif  
static inline u8 pci_read8_sync(struct rtl_priv *rtlpriv, u32 addr)
{
	return readb((u8 __iomem *)rtlpriv->io.pci_mem_start + addr);
}

static inline u16 pci_read16_sync(struct rtl_priv *rtlpriv, u32 addr)
{
	return readw((u8 __iomem *)rtlpriv->io.pci_mem_start + addr);
}

static inline u32 pci_read32_sync(struct rtl_priv *rtlpriv, u32 addr)
{
	return readl((u8 __iomem *)rtlpriv->io.pci_mem_start + addr);
}

static inline void pci_write8_async(struct rtl_priv *rtlpriv, u32 addr, u8 val)
{
	writeb(val, (u8 __iomem *)rtlpriv->io.pci_mem_start + addr);
}

static inline void pci_write16_async(struct rtl_priv *rtlpriv,
				     u32 addr, u16 val)
{
	writew(val, (u8 __iomem *)rtlpriv->io.pci_mem_start + addr);
}

static inline void pci_write32_async(struct rtl_priv *rtlpriv,
				     u32 addr, u32 val)
{
	writel(val, (u8 __iomem *)rtlpriv->io.pci_mem_start + addr);
}

static inline u16 calc_fifo_space(u16 rp, u16 wp, u16 size)
{
	if (rp <= wp)
		return size - 1 + rp - wp;
	return rp - wp - 1;
}

#endif
