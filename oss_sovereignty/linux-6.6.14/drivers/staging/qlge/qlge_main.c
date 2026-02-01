
 
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/pagemap.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/dmapool.h>
#include <linux/mempool.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <net/ipv6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/prefetch.h>
#include <net/ip6_checksum.h>

#include "qlge.h"
#include "qlge_devlink.h"

char qlge_driver_name[] = DRV_NAME;
const char qlge_driver_version[] = DRV_VERSION;

MODULE_AUTHOR("Ron Mercer <ron.mercer@qlogic.com>");
MODULE_DESCRIPTION(DRV_STRING " ");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

static const u32 default_msg =
	NETIF_MSG_DRV | NETIF_MSG_PROBE | NETIF_MSG_LINK |
	NETIF_MSG_IFDOWN |
	NETIF_MSG_IFUP |
	NETIF_MSG_RX_ERR |
	NETIF_MSG_TX_ERR |
	NETIF_MSG_HW | NETIF_MSG_WOL | 0;

static int debug = -1;	 
module_param(debug, int, 0664);
MODULE_PARM_DESC(debug, "Debug level (0=none,...,16=all)");

#define MSIX_IRQ 0
#define MSI_IRQ 1
#define LEG_IRQ 2
static int qlge_irq_type = MSIX_IRQ;
module_param(qlge_irq_type, int, 0664);
MODULE_PARM_DESC(qlge_irq_type, "0 = MSI-X, 1 = MSI, 2 = Legacy.");

static int qlge_mpi_coredump;
module_param(qlge_mpi_coredump, int, 0);
MODULE_PARM_DESC(qlge_mpi_coredump,
		 "Option to enable MPI firmware dump. Default is OFF - Do Not allocate memory. ");

static int qlge_force_coredump;
module_param(qlge_force_coredump, int, 0);
MODULE_PARM_DESC(qlge_force_coredump,
		 "Option to allow force of firmware core dump. Default is OFF - Do not allow.");

static const struct pci_device_id qlge_pci_tbl[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, QLGE_DEVICE_ID_8012)},
	{PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, QLGE_DEVICE_ID_8000)},
	 
	{0,}
};

MODULE_DEVICE_TABLE(pci, qlge_pci_tbl);

static int qlge_wol(struct qlge_adapter *);
static void qlge_set_multicast_list(struct net_device *);
static int qlge_adapter_down(struct qlge_adapter *);
static int qlge_adapter_up(struct qlge_adapter *);

 
static int qlge_sem_trylock(struct qlge_adapter *qdev, u32 sem_mask)
{
	u32 sem_bits = 0;

	switch (sem_mask) {
	case SEM_XGMAC0_MASK:
		sem_bits = SEM_SET << SEM_XGMAC0_SHIFT;
		break;
	case SEM_XGMAC1_MASK:
		sem_bits = SEM_SET << SEM_XGMAC1_SHIFT;
		break;
	case SEM_ICB_MASK:
		sem_bits = SEM_SET << SEM_ICB_SHIFT;
		break;
	case SEM_MAC_ADDR_MASK:
		sem_bits = SEM_SET << SEM_MAC_ADDR_SHIFT;
		break;
	case SEM_FLASH_MASK:
		sem_bits = SEM_SET << SEM_FLASH_SHIFT;
		break;
	case SEM_PROBE_MASK:
		sem_bits = SEM_SET << SEM_PROBE_SHIFT;
		break;
	case SEM_RT_IDX_MASK:
		sem_bits = SEM_SET << SEM_RT_IDX_SHIFT;
		break;
	case SEM_PROC_REG_MASK:
		sem_bits = SEM_SET << SEM_PROC_REG_SHIFT;
		break;
	default:
		netif_alert(qdev, probe, qdev->ndev, "bad Semaphore mask!.\n");
		return -EINVAL;
	}

	qlge_write32(qdev, SEM, sem_bits | sem_mask);
	return !(qlge_read32(qdev, SEM) & sem_bits);
}

int qlge_sem_spinlock(struct qlge_adapter *qdev, u32 sem_mask)
{
	unsigned int wait_count = 30;

	do {
		if (!qlge_sem_trylock(qdev, sem_mask))
			return 0;
		udelay(100);
	} while (--wait_count);
	return -ETIMEDOUT;
}

void qlge_sem_unlock(struct qlge_adapter *qdev, u32 sem_mask)
{
	qlge_write32(qdev, SEM, sem_mask);
	qlge_read32(qdev, SEM);	 
}

 
int qlge_wait_reg_rdy(struct qlge_adapter *qdev, u32 reg, u32 bit, u32 err_bit)
{
	u32 temp;
	int count;

	for (count = 0; count < UDELAY_COUNT; count++) {
		temp = qlge_read32(qdev, reg);

		 
		if (temp & err_bit) {
			netif_alert(qdev, probe, qdev->ndev,
				    "register 0x%.08x access error, value = 0x%.08x!.\n",
				    reg, temp);
			return -EIO;
		} else if (temp & bit) {
			return 0;
		}
		udelay(UDELAY_DELAY);
	}
	netif_alert(qdev, probe, qdev->ndev,
		    "Timed out waiting for reg %x to come ready.\n", reg);
	return -ETIMEDOUT;
}

 
static int qlge_wait_cfg(struct qlge_adapter *qdev, u32 bit)
{
	int count;
	u32 temp;

	for (count = 0; count < UDELAY_COUNT; count++) {
		temp = qlge_read32(qdev, CFG);
		if (temp & CFG_LE)
			return -EIO;
		if (!(temp & bit))
			return 0;
		udelay(UDELAY_DELAY);
	}
	return -ETIMEDOUT;
}

 
int qlge_write_cfg(struct qlge_adapter *qdev, void *ptr, int size, u32 bit,
		   u16 q_id)
{
	u64 map;
	int status = 0;
	int direction;
	u32 mask;
	u32 value;

	if (bit & (CFG_LRQ | CFG_LR | CFG_LCQ))
		direction = DMA_TO_DEVICE;
	else
		direction = DMA_FROM_DEVICE;

	map = dma_map_single(&qdev->pdev->dev, ptr, size, direction);
	if (dma_mapping_error(&qdev->pdev->dev, map)) {
		netif_err(qdev, ifup, qdev->ndev, "Couldn't map DMA area.\n");
		return -ENOMEM;
	}

	status = qlge_sem_spinlock(qdev, SEM_ICB_MASK);
	if (status)
		goto lock_failed;

	status = qlge_wait_cfg(qdev, bit);
	if (status) {
		netif_err(qdev, ifup, qdev->ndev,
			  "Timed out waiting for CFG to come ready.\n");
		goto exit;
	}

	qlge_write32(qdev, ICB_L, (u32)map);
	qlge_write32(qdev, ICB_H, (u32)(map >> 32));

	mask = CFG_Q_MASK | (bit << 16);
	value = bit | (q_id << CFG_Q_SHIFT);
	qlge_write32(qdev, CFG, (mask | value));

	 
	status = qlge_wait_cfg(qdev, bit);
exit:
	qlge_sem_unlock(qdev, SEM_ICB_MASK);	 
lock_failed:
	dma_unmap_single(&qdev->pdev->dev, map, size, direction);
	return status;
}

 
int qlge_get_mac_addr_reg(struct qlge_adapter *qdev, u32 type, u16 index,
			  u32 *value)
{
	u32 offset = 0;
	int status;

	switch (type) {
	case MAC_ADDR_TYPE_MULTI_MAC:
	case MAC_ADDR_TYPE_CAM_MAC: {
		status = qlge_wait_reg_rdy(qdev, MAC_ADDR_IDX, MAC_ADDR_MW, 0);
		if (status)
			break;
		qlge_write32(qdev, MAC_ADDR_IDX,
			     (offset++) |  
				   (index << MAC_ADDR_IDX_SHIFT) |  
				   MAC_ADDR_ADR | MAC_ADDR_RS |
				   type);  
		status = qlge_wait_reg_rdy(qdev, MAC_ADDR_IDX, MAC_ADDR_MR, 0);
		if (status)
			break;
		*value++ = qlge_read32(qdev, MAC_ADDR_DATA);
		status = qlge_wait_reg_rdy(qdev, MAC_ADDR_IDX, MAC_ADDR_MW, 0);
		if (status)
			break;
		qlge_write32(qdev, MAC_ADDR_IDX,
			     (offset++) |  
				   (index << MAC_ADDR_IDX_SHIFT) |  
				   MAC_ADDR_ADR | MAC_ADDR_RS |
				   type);  
		status = qlge_wait_reg_rdy(qdev, MAC_ADDR_IDX, MAC_ADDR_MR, 0);
		if (status)
			break;
		*value++ = qlge_read32(qdev, MAC_ADDR_DATA);
		if (type == MAC_ADDR_TYPE_CAM_MAC) {
			status = qlge_wait_reg_rdy(qdev, MAC_ADDR_IDX,
						   MAC_ADDR_MW, 0);
			if (status)
				break;
			qlge_write32(qdev, MAC_ADDR_IDX,
				     (offset++) |  
					   (index
					    << MAC_ADDR_IDX_SHIFT) |  
					   MAC_ADDR_ADR |
					   MAC_ADDR_RS | type);  
			status = qlge_wait_reg_rdy(qdev, MAC_ADDR_IDX,
						   MAC_ADDR_MR, 0);
			if (status)
				break;
			*value++ = qlge_read32(qdev, MAC_ADDR_DATA);
		}
		break;
	}
	case MAC_ADDR_TYPE_VLAN:
	case MAC_ADDR_TYPE_MULTI_FLTR:
	default:
		netif_crit(qdev, ifup, qdev->ndev,
			   "Address type %d not yet supported.\n", type);
		status = -EPERM;
	}
	return status;
}

 
static int qlge_set_mac_addr_reg(struct qlge_adapter *qdev, const u8 *addr,
				 u32 type, u16 index)
{
	u32 offset = 0;
	int status = 0;

	switch (type) {
	case MAC_ADDR_TYPE_MULTI_MAC: {
		u32 upper = (addr[0] << 8) | addr[1];
		u32 lower = (addr[2] << 24) | (addr[3] << 16) | (addr[4] << 8) |
			    (addr[5]);

		status = qlge_wait_reg_rdy(qdev, MAC_ADDR_IDX, MAC_ADDR_MW, 0);
		if (status)
			break;
		qlge_write32(qdev, MAC_ADDR_IDX,
			     (offset++) | (index << MAC_ADDR_IDX_SHIFT) | type |
				   MAC_ADDR_E);
		qlge_write32(qdev, MAC_ADDR_DATA, lower);
		status = qlge_wait_reg_rdy(qdev, MAC_ADDR_IDX, MAC_ADDR_MW, 0);
		if (status)
			break;
		qlge_write32(qdev, MAC_ADDR_IDX,
			     (offset++) | (index << MAC_ADDR_IDX_SHIFT) | type |
				   MAC_ADDR_E);

		qlge_write32(qdev, MAC_ADDR_DATA, upper);
		status = qlge_wait_reg_rdy(qdev, MAC_ADDR_IDX, MAC_ADDR_MW, 0);
		break;
	}
	case MAC_ADDR_TYPE_CAM_MAC: {
		u32 cam_output;
		u32 upper = (addr[0] << 8) | addr[1];
		u32 lower = (addr[2] << 24) | (addr[3] << 16) | (addr[4] << 8) |
			    (addr[5]);
		status = qlge_wait_reg_rdy(qdev, MAC_ADDR_IDX, MAC_ADDR_MW, 0);
		if (status)
			break;
		qlge_write32(qdev, MAC_ADDR_IDX,
			     (offset++) |  
				   (index << MAC_ADDR_IDX_SHIFT) |  
				   type);  
		qlge_write32(qdev, MAC_ADDR_DATA, lower);
		status = qlge_wait_reg_rdy(qdev, MAC_ADDR_IDX, MAC_ADDR_MW, 0);
		if (status)
			break;
		qlge_write32(qdev, MAC_ADDR_IDX,
			     (offset++) |  
				   (index << MAC_ADDR_IDX_SHIFT) |  
				   type);  
		qlge_write32(qdev, MAC_ADDR_DATA, upper);
		status = qlge_wait_reg_rdy(qdev, MAC_ADDR_IDX, MAC_ADDR_MW, 0);
		if (status)
			break;
		qlge_write32(qdev, MAC_ADDR_IDX,
			     (offset) |  
				   (index << MAC_ADDR_IDX_SHIFT) |  
				   type);  
		 
		cam_output = (CAM_OUT_ROUTE_NIC |
			      (qdev->func << CAM_OUT_FUNC_SHIFT) |
			      (0 << CAM_OUT_CQ_ID_SHIFT));
		if (qdev->ndev->features & NETIF_F_HW_VLAN_CTAG_RX)
			cam_output |= CAM_OUT_RV;
		 
		qlge_write32(qdev, MAC_ADDR_DATA, cam_output);
		break;
	}
	case MAC_ADDR_TYPE_VLAN: {
		u32 enable_bit = *((u32 *)&addr[0]);
		 
		status = qlge_wait_reg_rdy(qdev, MAC_ADDR_IDX, MAC_ADDR_MW, 0);
		if (status)
			break;
		qlge_write32(qdev, MAC_ADDR_IDX,
			     offset |  
				   (index << MAC_ADDR_IDX_SHIFT) |  
				   type |  
				   enable_bit);  
		break;
	}
	case MAC_ADDR_TYPE_MULTI_FLTR:
	default:
		netif_crit(qdev, ifup, qdev->ndev,
			   "Address type %d not yet supported.\n", type);
		status = -EPERM;
	}
	return status;
}

 
static int qlge_set_mac_addr(struct qlge_adapter *qdev, int set)
{
	int status;
	char zero_mac_addr[ETH_ALEN];
	char *addr;

	if (set) {
		addr = &qdev->current_mac_addr[0];
		netif_printk(qdev, ifup, KERN_DEBUG, qdev->ndev,
			     "Set Mac addr %pM\n", addr);
	} else {
		eth_zero_addr(zero_mac_addr);
		addr = &zero_mac_addr[0];
		netif_printk(qdev, ifup, KERN_DEBUG, qdev->ndev,
			     "Clearing MAC address\n");
	}
	status = qlge_sem_spinlock(qdev, SEM_MAC_ADDR_MASK);
	if (status)
		return status;
	status = qlge_set_mac_addr_reg(qdev, (const u8 *)addr,
				       MAC_ADDR_TYPE_CAM_MAC,
				       qdev->func * MAX_CQ);
	qlge_sem_unlock(qdev, SEM_MAC_ADDR_MASK);
	if (status)
		netif_err(qdev, ifup, qdev->ndev,
			  "Failed to init mac address.\n");
	return status;
}

void qlge_link_on(struct qlge_adapter *qdev)
{
	netif_err(qdev, link, qdev->ndev, "Link is up.\n");
	netif_carrier_on(qdev->ndev);
	qlge_set_mac_addr(qdev, 1);
}

void qlge_link_off(struct qlge_adapter *qdev)
{
	netif_err(qdev, link, qdev->ndev, "Link is down.\n");
	netif_carrier_off(qdev->ndev);
	qlge_set_mac_addr(qdev, 0);
}

 
int qlge_get_routing_reg(struct qlge_adapter *qdev, u32 index, u32 *value)
{
	int status = 0;

	status = qlge_wait_reg_rdy(qdev, RT_IDX, RT_IDX_MW, 0);
	if (status)
		goto exit;

	qlge_write32(qdev, RT_IDX,
		     RT_IDX_TYPE_NICQ | RT_IDX_RS | (index << RT_IDX_IDX_SHIFT));
	status = qlge_wait_reg_rdy(qdev, RT_IDX, RT_IDX_MR, 0);
	if (status)
		goto exit;
	*value = qlge_read32(qdev, RT_DATA);
exit:
	return status;
}

 
static int qlge_set_routing_reg(struct qlge_adapter *qdev, u32 index, u32 mask,
				int enable)
{
	int status = -EINVAL;  
	u32 value = 0;

	switch (mask) {
	case RT_IDX_CAM_HIT:
		{
			value = RT_IDX_DST_CAM_Q |	 
			    RT_IDX_TYPE_NICQ |	 
			    (RT_IDX_CAM_HIT_SLOT << RT_IDX_IDX_SHIFT); 
			break;
		}
	case RT_IDX_VALID:	 
		{
			value = RT_IDX_DST_DFLT_Q |	 
			    RT_IDX_TYPE_NICQ |	 
			    (RT_IDX_PROMISCUOUS_SLOT << RT_IDX_IDX_SHIFT); 
			break;
		}
	case RT_IDX_ERR:	 
		{
			value = RT_IDX_DST_DFLT_Q |	 
			    RT_IDX_TYPE_NICQ |	 
			    (RT_IDX_ALL_ERR_SLOT << RT_IDX_IDX_SHIFT); 
			break;
		}
	case RT_IDX_IP_CSUM_ERR:  
		{
			value = RT_IDX_DST_DFLT_Q |  
				RT_IDX_TYPE_NICQ |  
				(RT_IDX_IP_CSUM_ERR_SLOT <<
				RT_IDX_IDX_SHIFT);  
			break;
		}
	case RT_IDX_TU_CSUM_ERR:  
		{
			value = RT_IDX_DST_DFLT_Q |  
				RT_IDX_TYPE_NICQ |  
				(RT_IDX_TCP_UDP_CSUM_ERR_SLOT <<
				RT_IDX_IDX_SHIFT);  
			break;
		}
	case RT_IDX_BCAST:	 
		{
			value = RT_IDX_DST_DFLT_Q |	 
			    RT_IDX_TYPE_NICQ |	 
			    (RT_IDX_BCAST_SLOT << RT_IDX_IDX_SHIFT); 
			break;
		}
	case RT_IDX_MCAST:	 
		{
			value = RT_IDX_DST_DFLT_Q |	 
			    RT_IDX_TYPE_NICQ |	 
			    (RT_IDX_ALLMULTI_SLOT << RT_IDX_IDX_SHIFT); 
			break;
		}
	case RT_IDX_MCAST_MATCH:	 
		{
			value = RT_IDX_DST_DFLT_Q |	 
			    RT_IDX_TYPE_NICQ |	 
			    (RT_IDX_MCAST_MATCH_SLOT << RT_IDX_IDX_SHIFT); 
			break;
		}
	case RT_IDX_RSS_MATCH:	 
		{
			value = RT_IDX_DST_RSS |	 
			    RT_IDX_TYPE_NICQ |	 
			    (RT_IDX_RSS_MATCH_SLOT << RT_IDX_IDX_SHIFT); 
			break;
		}
	case 0:		 
		{
			value = RT_IDX_DST_DFLT_Q |	 
			    RT_IDX_TYPE_NICQ |	 
			    (index << RT_IDX_IDX_SHIFT); 
			break;
		}
	default:
		netif_err(qdev, ifup, qdev->ndev,
			  "Mask type %d not yet supported.\n", mask);
		status = -EPERM;
		goto exit;
	}

	if (value) {
		status = qlge_wait_reg_rdy(qdev, RT_IDX, RT_IDX_MW, 0);
		if (status)
			goto exit;
		value |= (enable ? RT_IDX_E : 0);
		qlge_write32(qdev, RT_IDX, value);
		qlge_write32(qdev, RT_DATA, enable ? mask : 0);
	}
exit:
	return status;
}

static void qlge_enable_interrupts(struct qlge_adapter *qdev)
{
	qlge_write32(qdev, INTR_EN, (INTR_EN_EI << 16) | INTR_EN_EI);
}

static void qlge_disable_interrupts(struct qlge_adapter *qdev)
{
	qlge_write32(qdev, INTR_EN, (INTR_EN_EI << 16));
}

static void qlge_enable_completion_interrupt(struct qlge_adapter *qdev, u32 intr)
{
	struct intr_context *ctx = &qdev->intr_context[intr];

	qlge_write32(qdev, INTR_EN, ctx->intr_en_mask);
}

static void qlge_disable_completion_interrupt(struct qlge_adapter *qdev, u32 intr)
{
	struct intr_context *ctx = &qdev->intr_context[intr];

	qlge_write32(qdev, INTR_EN, ctx->intr_dis_mask);
}

static void qlge_enable_all_completion_interrupts(struct qlge_adapter *qdev)
{
	int i;

	for (i = 0; i < qdev->intr_count; i++)
		qlge_enable_completion_interrupt(qdev, i);
}

static int qlge_validate_flash(struct qlge_adapter *qdev, u32 size, const char *str)
{
	int status, i;
	u16 csum = 0;
	__le16 *flash = (__le16 *)&qdev->flash;

	status = strncmp((char *)&qdev->flash, str, 4);
	if (status) {
		netif_err(qdev, ifup, qdev->ndev, "Invalid flash signature.\n");
		return	status;
	}

	for (i = 0; i < size; i++)
		csum += le16_to_cpu(*flash++);

	if (csum)
		netif_err(qdev, ifup, qdev->ndev,
			  "Invalid flash checksum, csum = 0x%.04x.\n", csum);

	return csum;
}

static int qlge_read_flash_word(struct qlge_adapter *qdev, int offset, __le32 *data)
{
	int status = 0;
	 
	status = qlge_wait_reg_rdy(qdev,
				   FLASH_ADDR, FLASH_ADDR_RDY, FLASH_ADDR_ERR);
	if (status)
		goto exit;
	 
	qlge_write32(qdev, FLASH_ADDR, FLASH_ADDR_R | offset);
	 
	status = qlge_wait_reg_rdy(qdev,
				   FLASH_ADDR, FLASH_ADDR_RDY, FLASH_ADDR_ERR);
	if (status)
		goto exit;
	 
	*data = cpu_to_le32(qlge_read32(qdev, FLASH_DATA));
exit:
	return status;
}

static int qlge_get_8000_flash_params(struct qlge_adapter *qdev)
{
	u32 i, size;
	int status;
	__le32 *p = (__le32 *)&qdev->flash;
	u32 offset;
	u8 mac_addr[6];

	 
	if (!qdev->port)
		offset = FUNC0_FLASH_OFFSET / sizeof(u32);
	else
		offset = FUNC1_FLASH_OFFSET / sizeof(u32);

	if (qlge_sem_spinlock(qdev, SEM_FLASH_MASK))
		return -ETIMEDOUT;

	size = sizeof(struct flash_params_8000) / sizeof(u32);
	for (i = 0; i < size; i++, p++) {
		status = qlge_read_flash_word(qdev, i + offset, p);
		if (status) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Error reading flash.\n");
			goto exit;
		}
	}

	status = qlge_validate_flash(qdev,
				     sizeof(struct flash_params_8000) /
				   sizeof(u16),
				   "8000");
	if (status) {
		netif_err(qdev, ifup, qdev->ndev, "Invalid flash.\n");
		status = -EINVAL;
		goto exit;
	}

	 
	if (qdev->flash.flash_params_8000.data_type1 == 2)
		memcpy(mac_addr,
		       qdev->flash.flash_params_8000.mac_addr1,
		       qdev->ndev->addr_len);
	else
		memcpy(mac_addr,
		       qdev->flash.flash_params_8000.mac_addr,
		       qdev->ndev->addr_len);

	if (!is_valid_ether_addr(mac_addr)) {
		netif_err(qdev, ifup, qdev->ndev, "Invalid MAC address.\n");
		status = -EINVAL;
		goto exit;
	}

	eth_hw_addr_set(qdev->ndev, mac_addr);

exit:
	qlge_sem_unlock(qdev, SEM_FLASH_MASK);
	return status;
}

static int qlge_get_8012_flash_params(struct qlge_adapter *qdev)
{
	int i;
	int status;
	__le32 *p = (__le32 *)&qdev->flash;
	u32 offset = 0;
	u32 size = sizeof(struct flash_params_8012) / sizeof(u32);

	 
	if (qdev->port)
		offset = size;

	if (qlge_sem_spinlock(qdev, SEM_FLASH_MASK))
		return -ETIMEDOUT;

	for (i = 0; i < size; i++, p++) {
		status = qlge_read_flash_word(qdev, i + offset, p);
		if (status) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Error reading flash.\n");
			goto exit;
		}
	}

	status = qlge_validate_flash(qdev,
				     sizeof(struct flash_params_8012) /
				       sizeof(u16),
				     "8012");
	if (status) {
		netif_err(qdev, ifup, qdev->ndev, "Invalid flash.\n");
		status = -EINVAL;
		goto exit;
	}

	if (!is_valid_ether_addr(qdev->flash.flash_params_8012.mac_addr)) {
		status = -EINVAL;
		goto exit;
	}

	eth_hw_addr_set(qdev->ndev, qdev->flash.flash_params_8012.mac_addr);

exit:
	qlge_sem_unlock(qdev, SEM_FLASH_MASK);
	return status;
}

 
static int qlge_write_xgmac_reg(struct qlge_adapter *qdev, u32 reg, u32 data)
{
	int status;
	 
	status = qlge_wait_reg_rdy(qdev,
				   XGMAC_ADDR, XGMAC_ADDR_RDY, XGMAC_ADDR_XME);
	if (status)
		return status;
	 
	qlge_write32(qdev, XGMAC_DATA, data);
	 
	qlge_write32(qdev, XGMAC_ADDR, reg);
	return status;
}

 
int qlge_read_xgmac_reg(struct qlge_adapter *qdev, u32 reg, u32 *data)
{
	int status = 0;
	 
	status = qlge_wait_reg_rdy(qdev,
				   XGMAC_ADDR, XGMAC_ADDR_RDY, XGMAC_ADDR_XME);
	if (status)
		goto exit;
	 
	qlge_write32(qdev, XGMAC_ADDR, reg | XGMAC_ADDR_R);
	 
	status = qlge_wait_reg_rdy(qdev,
				   XGMAC_ADDR, XGMAC_ADDR_RDY, XGMAC_ADDR_XME);
	if (status)
		goto exit;
	 
	*data = qlge_read32(qdev, XGMAC_DATA);
exit:
	return status;
}

 
int qlge_read_xgmac_reg64(struct qlge_adapter *qdev, u32 reg, u64 *data)
{
	int status = 0;
	u32 hi = 0;
	u32 lo = 0;

	status = qlge_read_xgmac_reg(qdev, reg, &lo);
	if (status)
		goto exit;

	status = qlge_read_xgmac_reg(qdev, reg + 4, &hi);
	if (status)
		goto exit;

	*data = (u64)lo | ((u64)hi << 32);

exit:
	return status;
}

static int qlge_8000_port_initialize(struct qlge_adapter *qdev)
{
	int status;
	 
	status = qlge_mb_about_fw(qdev);
	if (status)
		goto exit;
	status = qlge_mb_get_fw_state(qdev);
	if (status)
		goto exit;
	 
	queue_delayed_work(qdev->workqueue, &qdev->mpi_port_cfg_work, 0);
exit:
	return status;
}

 
static int qlge_8012_port_initialize(struct qlge_adapter *qdev)
{
	int status = 0;
	u32 data;

	if (qlge_sem_trylock(qdev, qdev->xg_sem_mask)) {
		 
		netif_info(qdev, link, qdev->ndev,
			   "Another function has the semaphore, so wait for the port init bit to come ready.\n");
		status = qlge_wait_reg_rdy(qdev, STS, qdev->port_init, 0);
		if (status) {
			netif_crit(qdev, link, qdev->ndev,
				   "Port initialize timed out.\n");
		}
		return status;
	}

	netif_info(qdev, link, qdev->ndev, "Got xgmac semaphore!.\n");
	 
	status = qlge_read_xgmac_reg(qdev, GLOBAL_CFG, &data);
	if (status)
		goto end;
	data |= GLOBAL_CFG_RESET;
	status = qlge_write_xgmac_reg(qdev, GLOBAL_CFG, data);
	if (status)
		goto end;

	 
	data &= ~GLOBAL_CFG_RESET;	 
	data |= GLOBAL_CFG_JUMBO;	 
	data |= GLOBAL_CFG_TX_STAT_EN;
	data |= GLOBAL_CFG_RX_STAT_EN;
	status = qlge_write_xgmac_reg(qdev, GLOBAL_CFG, data);
	if (status)
		goto end;

	 
	status = qlge_read_xgmac_reg(qdev, TX_CFG, &data);
	if (status)
		goto end;
	data &= ~TX_CFG_RESET;	 
	data |= TX_CFG_EN;	 
	status = qlge_write_xgmac_reg(qdev, TX_CFG, data);
	if (status)
		goto end;

	 
	status = qlge_read_xgmac_reg(qdev, RX_CFG, &data);
	if (status)
		goto end;
	data &= ~RX_CFG_RESET;	 
	data |= RX_CFG_EN;	 
	status = qlge_write_xgmac_reg(qdev, RX_CFG, data);
	if (status)
		goto end;

	 
	status =
	    qlge_write_xgmac_reg(qdev, MAC_TX_PARAMS, MAC_TX_PARAMS_JUMBO | (0x2580 << 16));
	if (status)
		goto end;
	status =
	    qlge_write_xgmac_reg(qdev, MAC_RX_PARAMS, 0x2580);
	if (status)
		goto end;

	 
	qlge_write32(qdev, STS, ((qdev->port_init << 16) | qdev->port_init));
end:
	qlge_sem_unlock(qdev, qdev->xg_sem_mask);
	return status;
}

static inline unsigned int qlge_lbq_block_size(struct qlge_adapter *qdev)
{
	return PAGE_SIZE << qdev->lbq_buf_order;
}

static struct qlge_bq_desc *qlge_get_curr_buf(struct qlge_bq *bq)
{
	struct qlge_bq_desc *bq_desc;

	bq_desc = &bq->queue[bq->next_to_clean];
	bq->next_to_clean = QLGE_BQ_WRAP(bq->next_to_clean + 1);

	return bq_desc;
}

static struct qlge_bq_desc *qlge_get_curr_lchunk(struct qlge_adapter *qdev,
						 struct rx_ring *rx_ring)
{
	struct qlge_bq_desc *lbq_desc = qlge_get_curr_buf(&rx_ring->lbq);

	dma_sync_single_for_cpu(&qdev->pdev->dev, lbq_desc->dma_addr,
				qdev->lbq_buf_size, DMA_FROM_DEVICE);

	if ((lbq_desc->p.pg_chunk.offset + qdev->lbq_buf_size) ==
	    qlge_lbq_block_size(qdev)) {
		 
		dma_unmap_page(&qdev->pdev->dev, lbq_desc->dma_addr,
			       qlge_lbq_block_size(qdev), DMA_FROM_DEVICE);
	}

	return lbq_desc;
}

 
static void qlge_update_cq(struct rx_ring *rx_ring)
{
	rx_ring->cnsmr_idx++;
	rx_ring->curr_entry++;
	if (unlikely(rx_ring->cnsmr_idx == rx_ring->cq_len)) {
		rx_ring->cnsmr_idx = 0;
		rx_ring->curr_entry = rx_ring->cq_base;
	}
}

static void qlge_write_cq_idx(struct rx_ring *rx_ring)
{
	qlge_write_db_reg(rx_ring->cnsmr_idx, rx_ring->cnsmr_idx_db_reg);
}

static const char * const bq_type_name[] = {
	[QLGE_SB] = "sbq",
	[QLGE_LB] = "lbq",
};

 
static int qlge_refill_sb(struct rx_ring *rx_ring,
			  struct qlge_bq_desc *sbq_desc, gfp_t gfp)
{
	struct qlge_adapter *qdev = rx_ring->qdev;
	struct sk_buff *skb;

	if (sbq_desc->p.skb)
		return 0;

	netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
		     "ring %u sbq: getting new skb for index %d.\n",
		     rx_ring->cq_id, sbq_desc->index);

	skb = __netdev_alloc_skb(qdev->ndev, SMALL_BUFFER_SIZE, gfp);
	if (!skb)
		return -ENOMEM;
	skb_reserve(skb, QLGE_SB_PAD);

	sbq_desc->dma_addr = dma_map_single(&qdev->pdev->dev, skb->data,
					    SMALL_BUF_MAP_SIZE,
					    DMA_FROM_DEVICE);
	if (dma_mapping_error(&qdev->pdev->dev, sbq_desc->dma_addr)) {
		netif_err(qdev, ifup, qdev->ndev, "PCI mapping failed.\n");
		dev_kfree_skb_any(skb);
		return -EIO;
	}
	*sbq_desc->buf_ptr = cpu_to_le64(sbq_desc->dma_addr);

	sbq_desc->p.skb = skb;
	return 0;
}

 
static int qlge_refill_lb(struct rx_ring *rx_ring,
			  struct qlge_bq_desc *lbq_desc, gfp_t gfp)
{
	struct qlge_adapter *qdev = rx_ring->qdev;
	struct qlge_page_chunk *master_chunk = &rx_ring->master_chunk;

	if (!master_chunk->page) {
		struct page *page;
		dma_addr_t dma_addr;

		page = alloc_pages(gfp | __GFP_COMP, qdev->lbq_buf_order);
		if (unlikely(!page))
			return -ENOMEM;
		dma_addr = dma_map_page(&qdev->pdev->dev, page, 0,
					qlge_lbq_block_size(qdev),
					DMA_FROM_DEVICE);
		if (dma_mapping_error(&qdev->pdev->dev, dma_addr)) {
			__free_pages(page, qdev->lbq_buf_order);
			netif_err(qdev, drv, qdev->ndev,
				  "PCI mapping failed.\n");
			return -EIO;
		}
		master_chunk->page = page;
		master_chunk->va = page_address(page);
		master_chunk->offset = 0;
		rx_ring->chunk_dma_addr = dma_addr;
	}

	lbq_desc->p.pg_chunk = *master_chunk;
	lbq_desc->dma_addr = rx_ring->chunk_dma_addr;
	*lbq_desc->buf_ptr = cpu_to_le64(lbq_desc->dma_addr +
					 lbq_desc->p.pg_chunk.offset);

	 
	master_chunk->offset += qdev->lbq_buf_size;
	if (master_chunk->offset == qlge_lbq_block_size(qdev)) {
		master_chunk->page = NULL;
	} else {
		master_chunk->va += qdev->lbq_buf_size;
		get_page(master_chunk->page);
	}

	return 0;
}

 
static int qlge_refill_bq(struct qlge_bq *bq, gfp_t gfp)
{
	struct rx_ring *rx_ring = QLGE_BQ_CONTAINER(bq);
	struct qlge_adapter *qdev = rx_ring->qdev;
	struct qlge_bq_desc *bq_desc;
	int refill_count;
	int retval;
	int i;

	refill_count = QLGE_BQ_WRAP(QLGE_BQ_ALIGN(bq->next_to_clean - 1) -
				    bq->next_to_use);
	if (!refill_count)
		return 0;

	i = bq->next_to_use;
	bq_desc = &bq->queue[i];
	i -= QLGE_BQ_LEN;
	do {
		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
			     "ring %u %s: try cleaning idx %d\n",
			     rx_ring->cq_id, bq_type_name[bq->type], i);

		if (bq->type == QLGE_SB)
			retval = qlge_refill_sb(rx_ring, bq_desc, gfp);
		else
			retval = qlge_refill_lb(rx_ring, bq_desc, gfp);
		if (retval < 0) {
			netif_err(qdev, ifup, qdev->ndev,
				  "ring %u %s: Could not get a page chunk, idx %d\n",
				  rx_ring->cq_id, bq_type_name[bq->type], i);
			break;
		}

		bq_desc++;
		i++;
		if (unlikely(!i)) {
			bq_desc = &bq->queue[0];
			i -= QLGE_BQ_LEN;
		}
		refill_count--;
	} while (refill_count);
	i += QLGE_BQ_LEN;

	if (bq->next_to_use != i) {
		if (QLGE_BQ_ALIGN(bq->next_to_use) != QLGE_BQ_ALIGN(i)) {
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "ring %u %s: updating prod idx = %d.\n",
				     rx_ring->cq_id, bq_type_name[bq->type],
				     i);
			qlge_write_db_reg(i, bq->prod_idx_db_reg);
		}
		bq->next_to_use = i;
	}

	return retval;
}

static void qlge_update_buffer_queues(struct rx_ring *rx_ring, gfp_t gfp,
				      unsigned long delay)
{
	bool sbq_fail, lbq_fail;

	sbq_fail = !!qlge_refill_bq(&rx_ring->sbq, gfp);
	lbq_fail = !!qlge_refill_bq(&rx_ring->lbq, gfp);

	 
	if ((sbq_fail && QLGE_BQ_HW_OWNED(&rx_ring->sbq) < 2) ||
	    (lbq_fail && QLGE_BQ_HW_OWNED(&rx_ring->lbq) <
	     DIV_ROUND_UP(9000, LARGE_BUFFER_MAX_SIZE)))
		 
		queue_delayed_work_on(smp_processor_id(), system_long_wq,
				      &rx_ring->refill_work, delay);
}

static void qlge_slow_refill(struct work_struct *work)
{
	struct rx_ring *rx_ring = container_of(work, struct rx_ring,
					       refill_work.work);
	struct napi_struct *napi = &rx_ring->napi;

	napi_disable(napi);
	qlge_update_buffer_queues(rx_ring, GFP_KERNEL, HZ / 2);
	napi_enable(napi);

	local_bh_disable();
	 
	napi_schedule(napi);
	 
	local_bh_enable();
}

 
static void qlge_unmap_send(struct qlge_adapter *qdev,
			    struct tx_ring_desc *tx_ring_desc, int mapped)
{
	int i;

	for (i = 0; i < mapped; i++) {
		if (i == 0 || (i == 7 && mapped > 7)) {
			 
			if (i == 7) {
				netif_printk(qdev, tx_done, KERN_DEBUG,
					     qdev->ndev,
					     "unmapping OAL area.\n");
			}
			dma_unmap_single(&qdev->pdev->dev,
					 dma_unmap_addr(&tx_ring_desc->map[i],
							mapaddr),
					 dma_unmap_len(&tx_ring_desc->map[i],
						       maplen),
					 DMA_TO_DEVICE);
		} else {
			netif_printk(qdev, tx_done, KERN_DEBUG, qdev->ndev,
				     "unmapping frag %d.\n", i);
			dma_unmap_page(&qdev->pdev->dev,
				       dma_unmap_addr(&tx_ring_desc->map[i],
						      mapaddr),
				       dma_unmap_len(&tx_ring_desc->map[i],
						     maplen), DMA_TO_DEVICE);
		}
	}
}

 
static int qlge_map_send(struct qlge_adapter *qdev,
			 struct qlge_ob_mac_iocb_req *mac_iocb_ptr,
			 struct sk_buff *skb, struct tx_ring_desc *tx_ring_desc)
{
	int len = skb_headlen(skb);
	dma_addr_t map;
	int frag_idx, err, map_idx = 0;
	struct tx_buf_desc *tbd = mac_iocb_ptr->tbd;
	int frag_cnt = skb_shinfo(skb)->nr_frags;

	if (frag_cnt) {
		netif_printk(qdev, tx_queued, KERN_DEBUG, qdev->ndev,
			     "frag_cnt = %d.\n", frag_cnt);
	}
	 
	map = dma_map_single(&qdev->pdev->dev, skb->data, len, DMA_TO_DEVICE);

	err = dma_mapping_error(&qdev->pdev->dev, map);
	if (err) {
		netif_err(qdev, tx_queued, qdev->ndev,
			  "PCI mapping failed with error: %d\n", err);

		return NETDEV_TX_BUSY;
	}

	tbd->len = cpu_to_le32(len);
	tbd->addr = cpu_to_le64(map);
	dma_unmap_addr_set(&tx_ring_desc->map[map_idx], mapaddr, map);
	dma_unmap_len_set(&tx_ring_desc->map[map_idx], maplen, len);
	map_idx++;

	 
	for (frag_idx = 0; frag_idx < frag_cnt; frag_idx++, map_idx++) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[frag_idx];

		tbd++;
		if (frag_idx == 6 && frag_cnt > 7) {
			 
			 
			map = dma_map_single(&qdev->pdev->dev, &tx_ring_desc->oal,
					     sizeof(struct qlge_oal),
					     DMA_TO_DEVICE);
			err = dma_mapping_error(&qdev->pdev->dev, map);
			if (err) {
				netif_err(qdev, tx_queued, qdev->ndev,
					  "PCI mapping outbound address list with error: %d\n",
					  err);
				goto map_error;
			}

			tbd->addr = cpu_to_le64(map);
			 
			tbd->len =
			    cpu_to_le32((sizeof(struct tx_buf_desc) *
					 (frag_cnt - frag_idx)) | TX_DESC_C);
			dma_unmap_addr_set(&tx_ring_desc->map[map_idx], mapaddr,
					   map);
			dma_unmap_len_set(&tx_ring_desc->map[map_idx], maplen,
					  sizeof(struct qlge_oal));
			tbd = (struct tx_buf_desc *)&tx_ring_desc->oal;
			map_idx++;
		}

		map = skb_frag_dma_map(&qdev->pdev->dev, frag, 0, skb_frag_size(frag),
				       DMA_TO_DEVICE);

		err = dma_mapping_error(&qdev->pdev->dev, map);
		if (err) {
			netif_err(qdev, tx_queued, qdev->ndev,
				  "PCI mapping frags failed with error: %d.\n",
				  err);
			goto map_error;
		}

		tbd->addr = cpu_to_le64(map);
		tbd->len = cpu_to_le32(skb_frag_size(frag));
		dma_unmap_addr_set(&tx_ring_desc->map[map_idx], mapaddr, map);
		dma_unmap_len_set(&tx_ring_desc->map[map_idx], maplen,
				  skb_frag_size(frag));
	}
	 
	tx_ring_desc->map_cnt = map_idx;
	 
	tbd->len = cpu_to_le32(le32_to_cpu(tbd->len) | TX_DESC_E);
	return NETDEV_TX_OK;

map_error:
	 
	qlge_unmap_send(qdev, tx_ring_desc, map_idx);
	return NETDEV_TX_BUSY;
}

 
static void qlge_categorize_rx_err(struct qlge_adapter *qdev, u8 rx_err,
				   struct rx_ring *rx_ring)
{
	struct nic_stats *stats = &qdev->nic_stats;

	stats->rx_err_count++;
	rx_ring->rx_errors++;

	switch (rx_err & IB_MAC_IOCB_RSP_ERR_MASK) {
	case IB_MAC_IOCB_RSP_ERR_CODE_ERR:
		stats->rx_code_err++;
		break;
	case IB_MAC_IOCB_RSP_ERR_OVERSIZE:
		stats->rx_oversize_err++;
		break;
	case IB_MAC_IOCB_RSP_ERR_UNDERSIZE:
		stats->rx_undersize_err++;
		break;
	case IB_MAC_IOCB_RSP_ERR_PREAMBLE:
		stats->rx_preamble_err++;
		break;
	case IB_MAC_IOCB_RSP_ERR_FRAME_LEN:
		stats->rx_frame_len_err++;
		break;
	case IB_MAC_IOCB_RSP_ERR_CRC:
		stats->rx_crc_err++;
		break;
	default:
		break;
	}
}

 
static void qlge_update_mac_hdr_len(struct qlge_adapter *qdev,
				    struct qlge_ib_mac_iocb_rsp *ib_mac_rsp,
				    void *page, size_t *len)
{
	u16 *tags;

	if (qdev->ndev->features & NETIF_F_HW_VLAN_CTAG_RX)
		return;
	if (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_V) {
		tags = (u16 *)page;
		 
		if (tags[6] == ETH_P_8021Q &&
		    tags[8] == ETH_P_8021Q)
			*len += 2 * VLAN_HLEN;
		else
			*len += VLAN_HLEN;
	}
}

 
static void qlge_process_mac_rx_gro_page(struct qlge_adapter *qdev,
					 struct rx_ring *rx_ring,
					 struct qlge_ib_mac_iocb_rsp *ib_mac_rsp,
					 u32 length, u16 vlan_id)
{
	struct sk_buff *skb;
	struct qlge_bq_desc *lbq_desc = qlge_get_curr_lchunk(qdev, rx_ring);
	struct napi_struct *napi = &rx_ring->napi;

	 
	if (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_ERR_MASK) {
		qlge_categorize_rx_err(qdev, ib_mac_rsp->flags2, rx_ring);
		put_page(lbq_desc->p.pg_chunk.page);
		return;
	}
	napi->dev = qdev->ndev;

	skb = napi_get_frags(napi);
	if (!skb) {
		netif_err(qdev, drv, qdev->ndev,
			  "Couldn't get an skb, exiting.\n");
		rx_ring->rx_dropped++;
		put_page(lbq_desc->p.pg_chunk.page);
		return;
	}
	prefetch(lbq_desc->p.pg_chunk.va);
	__skb_fill_page_desc(skb, skb_shinfo(skb)->nr_frags,
			     lbq_desc->p.pg_chunk.page,
			     lbq_desc->p.pg_chunk.offset,
			     length);

	skb->len += length;
	skb->data_len += length;
	skb->truesize += length;
	skb_shinfo(skb)->nr_frags++;

	rx_ring->rx_packets++;
	rx_ring->rx_bytes += length;
	skb->ip_summed = CHECKSUM_UNNECESSARY;
	skb_record_rx_queue(skb, rx_ring->cq_id);
	if (vlan_id != 0xffff)
		__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), vlan_id);
	napi_gro_frags(napi);
}

 
static void qlge_process_mac_rx_page(struct qlge_adapter *qdev,
				     struct rx_ring *rx_ring,
				     struct qlge_ib_mac_iocb_rsp *ib_mac_rsp,
				     u32 length, u16 vlan_id)
{
	struct net_device *ndev = qdev->ndev;
	struct sk_buff *skb = NULL;
	void *addr;
	struct qlge_bq_desc *lbq_desc = qlge_get_curr_lchunk(qdev, rx_ring);
	struct napi_struct *napi = &rx_ring->napi;
	size_t hlen = ETH_HLEN;

	skb = netdev_alloc_skb(ndev, length);
	if (!skb) {
		rx_ring->rx_dropped++;
		put_page(lbq_desc->p.pg_chunk.page);
		return;
	}

	addr = lbq_desc->p.pg_chunk.va;
	prefetch(addr);

	 
	if (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_ERR_MASK) {
		qlge_categorize_rx_err(qdev, ib_mac_rsp->flags2, rx_ring);
		goto err_out;
	}

	 
	qlge_update_mac_hdr_len(qdev, ib_mac_rsp, addr, &hlen);

	 
	if (skb->len > ndev->mtu + hlen) {
		netif_err(qdev, drv, qdev->ndev,
			  "Segment too small, dropping.\n");
		rx_ring->rx_dropped++;
		goto err_out;
	}
	skb_put_data(skb, addr, hlen);
	netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
		     "%d bytes of headers and data in large. Chain page to new skb and pull tail.\n",
		     length);
	skb_fill_page_desc(skb, 0, lbq_desc->p.pg_chunk.page,
			   lbq_desc->p.pg_chunk.offset + hlen, length - hlen);
	skb->len += length - hlen;
	skb->data_len += length - hlen;
	skb->truesize += length - hlen;

	rx_ring->rx_packets++;
	rx_ring->rx_bytes += skb->len;
	skb->protocol = eth_type_trans(skb, ndev);
	skb_checksum_none_assert(skb);

	if ((ndev->features & NETIF_F_RXCSUM) &&
	    !(ib_mac_rsp->flags1 & IB_MAC_CSUM_ERR_MASK)) {
		 
		if (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_T) {
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "TCP checksum done!\n");
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		} else if ((ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_U) &&
			   (ib_mac_rsp->flags3 & IB_MAC_IOCB_RSP_V4)) {
			 
			struct iphdr *iph =
				(struct iphdr *)((u8 *)addr + hlen);
			if (!(iph->frag_off &
			      htons(IP_MF | IP_OFFSET))) {
				skb->ip_summed = CHECKSUM_UNNECESSARY;
				netif_printk(qdev, rx_status, KERN_DEBUG,
					     qdev->ndev,
					     "UDP checksum done!\n");
			}
		}
	}

	skb_record_rx_queue(skb, rx_ring->cq_id);
	if (vlan_id != 0xffff)
		__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), vlan_id);
	if (skb->ip_summed == CHECKSUM_UNNECESSARY)
		napi_gro_receive(napi, skb);
	else
		netif_receive_skb(skb);
	return;
err_out:
	dev_kfree_skb_any(skb);
	put_page(lbq_desc->p.pg_chunk.page);
}

 
static void qlge_process_mac_rx_skb(struct qlge_adapter *qdev,
				    struct rx_ring *rx_ring,
				    struct qlge_ib_mac_iocb_rsp *ib_mac_rsp,
				    u32 length, u16 vlan_id)
{
	struct qlge_bq_desc *sbq_desc = qlge_get_curr_buf(&rx_ring->sbq);
	struct net_device *ndev = qdev->ndev;
	struct sk_buff *skb, *new_skb;

	skb = sbq_desc->p.skb;
	 
	new_skb = netdev_alloc_skb(qdev->ndev, length + NET_IP_ALIGN);
	if (!new_skb) {
		rx_ring->rx_dropped++;
		return;
	}
	skb_reserve(new_skb, NET_IP_ALIGN);

	dma_sync_single_for_cpu(&qdev->pdev->dev, sbq_desc->dma_addr,
				SMALL_BUF_MAP_SIZE, DMA_FROM_DEVICE);

	skb_put_data(new_skb, skb->data, length);

	skb = new_skb;

	 
	if (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_ERR_MASK) {
		qlge_categorize_rx_err(qdev, ib_mac_rsp->flags2, rx_ring);
		dev_kfree_skb_any(skb);
		return;
	}

	 
	if (test_bit(QL_SELFTEST, &qdev->flags)) {
		qlge_check_lb_frame(qdev, skb);
		dev_kfree_skb_any(skb);
		return;
	}

	 
	if (skb->len > ndev->mtu + ETH_HLEN) {
		dev_kfree_skb_any(skb);
		rx_ring->rx_dropped++;
		return;
	}

	prefetch(skb->data);
	if (ib_mac_rsp->flags1 & IB_MAC_IOCB_RSP_M_MASK) {
		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
			     "%s Multicast.\n",
			     (ib_mac_rsp->flags1 & IB_MAC_IOCB_RSP_M_MASK) ==
			     IB_MAC_IOCB_RSP_M_HASH ? "Hash" :
			     (ib_mac_rsp->flags1 & IB_MAC_IOCB_RSP_M_MASK) ==
			     IB_MAC_IOCB_RSP_M_REG ? "Registered" :
			     (ib_mac_rsp->flags1 & IB_MAC_IOCB_RSP_M_MASK) ==
			     IB_MAC_IOCB_RSP_M_PROM ? "Promiscuous" : "");
	}
	if (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_P)
		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
			     "Promiscuous Packet.\n");

	rx_ring->rx_packets++;
	rx_ring->rx_bytes += skb->len;
	skb->protocol = eth_type_trans(skb, ndev);
	skb_checksum_none_assert(skb);

	 
	if ((ndev->features & NETIF_F_RXCSUM) &&
	    !(ib_mac_rsp->flags1 & IB_MAC_CSUM_ERR_MASK)) {
		 
		if (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_T) {
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "TCP checksum done!\n");
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		} else if ((ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_U) &&
			   (ib_mac_rsp->flags3 & IB_MAC_IOCB_RSP_V4)) {
			 
			struct iphdr *iph = (struct iphdr *)skb->data;

			if (!(iph->frag_off &
			      htons(IP_MF | IP_OFFSET))) {
				skb->ip_summed = CHECKSUM_UNNECESSARY;
				netif_printk(qdev, rx_status, KERN_DEBUG,
					     qdev->ndev,
					     "UDP checksum done!\n");
			}
		}
	}

	skb_record_rx_queue(skb, rx_ring->cq_id);
	if (vlan_id != 0xffff)
		__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), vlan_id);
	if (skb->ip_summed == CHECKSUM_UNNECESSARY)
		napi_gro_receive(&rx_ring->napi, skb);
	else
		netif_receive_skb(skb);
}

static void qlge_realign_skb(struct sk_buff *skb, int len)
{
	void *temp_addr = skb->data;

	 
	skb->data -= QLGE_SB_PAD - NET_IP_ALIGN;
	skb->tail -= QLGE_SB_PAD - NET_IP_ALIGN;
	memmove(skb->data, temp_addr, len);
}

 
static struct sk_buff *qlge_build_rx_skb(struct qlge_adapter *qdev,
					 struct rx_ring *rx_ring,
					 struct qlge_ib_mac_iocb_rsp *ib_mac_rsp)
{
	u32 length = le32_to_cpu(ib_mac_rsp->data_len);
	u32 hdr_len = le32_to_cpu(ib_mac_rsp->hdr_len);
	struct qlge_bq_desc *lbq_desc, *sbq_desc;
	struct sk_buff *skb = NULL;
	size_t hlen = ETH_HLEN;

	 
	if (ib_mac_rsp->flags4 & IB_MAC_IOCB_RSP_HV &&
	    ib_mac_rsp->flags4 & IB_MAC_IOCB_RSP_HS) {
		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
			     "Header of %d bytes in small buffer.\n", hdr_len);
		 
		sbq_desc = qlge_get_curr_buf(&rx_ring->sbq);
		dma_unmap_single(&qdev->pdev->dev, sbq_desc->dma_addr,
				 SMALL_BUF_MAP_SIZE, DMA_FROM_DEVICE);
		skb = sbq_desc->p.skb;
		qlge_realign_skb(skb, hdr_len);
		skb_put(skb, hdr_len);
		sbq_desc->p.skb = NULL;
	}

	 
	if (unlikely(!length)) {	 
		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
			     "No Data buffer in this packet.\n");
		return skb;
	}

	if (ib_mac_rsp->flags3 & IB_MAC_IOCB_RSP_DS) {
		if (ib_mac_rsp->flags4 & IB_MAC_IOCB_RSP_HS) {
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "Headers in small, data of %d bytes in small, combine them.\n",
				     length);
			 
			sbq_desc = qlge_get_curr_buf(&rx_ring->sbq);
			dma_sync_single_for_cpu(&qdev->pdev->dev,
						sbq_desc->dma_addr,
						SMALL_BUF_MAP_SIZE,
						DMA_FROM_DEVICE);
			skb_put_data(skb, sbq_desc->p.skb->data, length);
		} else {
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "%d bytes in a single small buffer.\n",
				     length);
			sbq_desc = qlge_get_curr_buf(&rx_ring->sbq);
			skb = sbq_desc->p.skb;
			qlge_realign_skb(skb, length);
			skb_put(skb, length);
			dma_unmap_single(&qdev->pdev->dev, sbq_desc->dma_addr,
					 SMALL_BUF_MAP_SIZE,
					 DMA_FROM_DEVICE);
			sbq_desc->p.skb = NULL;
		}
	} else if (ib_mac_rsp->flags3 & IB_MAC_IOCB_RSP_DL) {
		if (ib_mac_rsp->flags4 & IB_MAC_IOCB_RSP_HS) {
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "Header in small, %d bytes in large. Chain large to small!\n",
				     length);
			 
			lbq_desc = qlge_get_curr_lchunk(qdev, rx_ring);
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "Chaining page at offset = %d, for %d bytes  to skb.\n",
				     lbq_desc->p.pg_chunk.offset, length);
			skb_fill_page_desc(skb, 0, lbq_desc->p.pg_chunk.page,
					   lbq_desc->p.pg_chunk.offset, length);
			skb->len += length;
			skb->data_len += length;
			skb->truesize += length;
		} else {
			 
			lbq_desc = qlge_get_curr_lchunk(qdev, rx_ring);
			skb = netdev_alloc_skb(qdev->ndev, length);
			if (!skb) {
				netif_printk(qdev, probe, KERN_DEBUG, qdev->ndev,
					     "No skb available, drop the packet.\n");
				return NULL;
			}
			dma_unmap_page(&qdev->pdev->dev, lbq_desc->dma_addr,
				       qdev->lbq_buf_size,
				       DMA_FROM_DEVICE);
			skb_reserve(skb, NET_IP_ALIGN);
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "%d bytes of headers and data in large. Chain page to new skb and pull tail.\n",
				     length);
			skb_fill_page_desc(skb, 0, lbq_desc->p.pg_chunk.page,
					   lbq_desc->p.pg_chunk.offset,
					   length);
			skb->len += length;
			skb->data_len += length;
			skb->truesize += length;
			qlge_update_mac_hdr_len(qdev, ib_mac_rsp,
						lbq_desc->p.pg_chunk.va,
						&hlen);
			__pskb_pull_tail(skb, hlen);
		}
	} else {
		 
		int size, i = 0;

		sbq_desc = qlge_get_curr_buf(&rx_ring->sbq);
		dma_unmap_single(&qdev->pdev->dev, sbq_desc->dma_addr,
				 SMALL_BUF_MAP_SIZE, DMA_FROM_DEVICE);
		if (!(ib_mac_rsp->flags4 & IB_MAC_IOCB_RSP_HS)) {
			 
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "%d bytes of headers & data in chain of large.\n",
				     length);
			skb = sbq_desc->p.skb;
			sbq_desc->p.skb = NULL;
			skb_reserve(skb, NET_IP_ALIGN);
		}
		do {
			lbq_desc = qlge_get_curr_lchunk(qdev, rx_ring);
			size = min(length, qdev->lbq_buf_size);

			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "Adding page %d to skb for %d bytes.\n",
				     i, size);
			skb_fill_page_desc(skb, i,
					   lbq_desc->p.pg_chunk.page,
					   lbq_desc->p.pg_chunk.offset, size);
			skb->len += size;
			skb->data_len += size;
			skb->truesize += size;
			length -= size;
			i++;
		} while (length > 0);
		qlge_update_mac_hdr_len(qdev, ib_mac_rsp, lbq_desc->p.pg_chunk.va,
					&hlen);
		__pskb_pull_tail(skb, hlen);
	}
	return skb;
}

 
static void qlge_process_mac_split_rx_intr(struct qlge_adapter *qdev,
					   struct rx_ring *rx_ring,
					   struct qlge_ib_mac_iocb_rsp *ib_mac_rsp,
					   u16 vlan_id)
{
	struct net_device *ndev = qdev->ndev;
	struct sk_buff *skb = NULL;

	skb = qlge_build_rx_skb(qdev, rx_ring, ib_mac_rsp);
	if (unlikely(!skb)) {
		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
			     "No skb available, drop packet.\n");
		rx_ring->rx_dropped++;
		return;
	}

	 
	if (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_ERR_MASK) {
		qlge_categorize_rx_err(qdev, ib_mac_rsp->flags2, rx_ring);
		dev_kfree_skb_any(skb);
		return;
	}

	 
	if (skb->len > ndev->mtu + ETH_HLEN) {
		dev_kfree_skb_any(skb);
		rx_ring->rx_dropped++;
		return;
	}

	 
	if (test_bit(QL_SELFTEST, &qdev->flags)) {
		qlge_check_lb_frame(qdev, skb);
		dev_kfree_skb_any(skb);
		return;
	}

	prefetch(skb->data);
	if (ib_mac_rsp->flags1 & IB_MAC_IOCB_RSP_M_MASK) {
		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev, "%s Multicast.\n",
			     (ib_mac_rsp->flags1 & IB_MAC_IOCB_RSP_M_MASK) ==
			     IB_MAC_IOCB_RSP_M_HASH ? "Hash" :
			     (ib_mac_rsp->flags1 & IB_MAC_IOCB_RSP_M_MASK) ==
			     IB_MAC_IOCB_RSP_M_REG ? "Registered" :
			     (ib_mac_rsp->flags1 & IB_MAC_IOCB_RSP_M_MASK) ==
			     IB_MAC_IOCB_RSP_M_PROM ? "Promiscuous" : "");
		rx_ring->rx_multicast++;
	}
	if (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_P) {
		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
			     "Promiscuous Packet.\n");
	}

	skb->protocol = eth_type_trans(skb, ndev);
	skb_checksum_none_assert(skb);

	 
	if ((ndev->features & NETIF_F_RXCSUM) &&
	    !(ib_mac_rsp->flags1 & IB_MAC_CSUM_ERR_MASK)) {
		 
		if (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_T) {
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "TCP checksum done!\n");
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		} else if ((ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_U) &&
			   (ib_mac_rsp->flags3 & IB_MAC_IOCB_RSP_V4)) {
			 
			struct iphdr *iph = (struct iphdr *)skb->data;

			if (!(iph->frag_off &
			      htons(IP_MF | IP_OFFSET))) {
				skb->ip_summed = CHECKSUM_UNNECESSARY;
				netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
					     "TCP checksum done!\n");
			}
		}
	}

	rx_ring->rx_packets++;
	rx_ring->rx_bytes += skb->len;
	skb_record_rx_queue(skb, rx_ring->cq_id);
	if (vlan_id != 0xffff)
		__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), vlan_id);
	if (skb->ip_summed == CHECKSUM_UNNECESSARY)
		napi_gro_receive(&rx_ring->napi, skb);
	else
		netif_receive_skb(skb);
}

 
static unsigned long qlge_process_mac_rx_intr(struct qlge_adapter *qdev,
					      struct rx_ring *rx_ring,
					      struct qlge_ib_mac_iocb_rsp *ib_mac_rsp)
{
	u32 length = le32_to_cpu(ib_mac_rsp->data_len);
	u16 vlan_id = ((ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_V) &&
		       (qdev->ndev->features & NETIF_F_HW_VLAN_CTAG_RX)) ?
		((le16_to_cpu(ib_mac_rsp->vlan_id) &
		  IB_MAC_IOCB_RSP_VLAN_MASK)) : 0xffff;

	if (ib_mac_rsp->flags4 & IB_MAC_IOCB_RSP_HV) {
		 
		qlge_process_mac_split_rx_intr(qdev, rx_ring, ib_mac_rsp,
					       vlan_id);
	} else if (ib_mac_rsp->flags3 & IB_MAC_IOCB_RSP_DS) {
		 
		qlge_process_mac_rx_skb(qdev, rx_ring, ib_mac_rsp, length,
					vlan_id);
	} else if ((ib_mac_rsp->flags3 & IB_MAC_IOCB_RSP_DL) &&
		   !(ib_mac_rsp->flags1 & IB_MAC_CSUM_ERR_MASK) &&
		   (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_T)) {
		 
		qlge_process_mac_rx_gro_page(qdev, rx_ring, ib_mac_rsp, length,
					     vlan_id);
	} else if (ib_mac_rsp->flags3 & IB_MAC_IOCB_RSP_DL) {
		 
		qlge_process_mac_rx_page(qdev, rx_ring, ib_mac_rsp, length,
					 vlan_id);
	} else {
		 
		qlge_process_mac_split_rx_intr(qdev, rx_ring, ib_mac_rsp,
					       vlan_id);
	}

	return (unsigned long)length;
}

 
static void qlge_process_mac_tx_intr(struct qlge_adapter *qdev,
				     struct qlge_ob_mac_iocb_rsp *mac_rsp)
{
	struct tx_ring *tx_ring;
	struct tx_ring_desc *tx_ring_desc;

	tx_ring = &qdev->tx_ring[mac_rsp->txq_idx];
	tx_ring_desc = &tx_ring->q[mac_rsp->tid];
	qlge_unmap_send(qdev, tx_ring_desc, tx_ring_desc->map_cnt);
	tx_ring->tx_bytes += (tx_ring_desc->skb)->len;
	tx_ring->tx_packets++;
	dev_kfree_skb(tx_ring_desc->skb);
	tx_ring_desc->skb = NULL;

	if (unlikely(mac_rsp->flags1 & (OB_MAC_IOCB_RSP_E |
					OB_MAC_IOCB_RSP_S |
					OB_MAC_IOCB_RSP_L |
					OB_MAC_IOCB_RSP_P | OB_MAC_IOCB_RSP_B))) {
		if (mac_rsp->flags1 & OB_MAC_IOCB_RSP_E) {
			netif_warn(qdev, tx_done, qdev->ndev,
				   "Total descriptor length did not match transfer length.\n");
		}
		if (mac_rsp->flags1 & OB_MAC_IOCB_RSP_S) {
			netif_warn(qdev, tx_done, qdev->ndev,
				   "Frame too short to be valid, not sent.\n");
		}
		if (mac_rsp->flags1 & OB_MAC_IOCB_RSP_L) {
			netif_warn(qdev, tx_done, qdev->ndev,
				   "Frame too long, but sent anyway.\n");
		}
		if (mac_rsp->flags1 & OB_MAC_IOCB_RSP_B) {
			netif_warn(qdev, tx_done, qdev->ndev,
				   "PCI backplane error. Frame not sent.\n");
		}
	}
	atomic_inc(&tx_ring->tx_count);
}

 
void qlge_queue_fw_error(struct qlge_adapter *qdev)
{
	qlge_link_off(qdev);
	queue_delayed_work(qdev->workqueue, &qdev->mpi_reset_work, 0);
}

void qlge_queue_asic_error(struct qlge_adapter *qdev)
{
	qlge_link_off(qdev);
	qlge_disable_interrupts(qdev);
	 
	clear_bit(QL_ADAPTER_UP, &qdev->flags);
	 
	set_bit(QL_ASIC_RECOVERY, &qdev->flags);
	queue_delayed_work(qdev->workqueue, &qdev->asic_reset_work, 0);
}

static void qlge_process_chip_ae_intr(struct qlge_adapter *qdev,
				      struct qlge_ib_ae_iocb_rsp *ib_ae_rsp)
{
	switch (ib_ae_rsp->event) {
	case MGMT_ERR_EVENT:
		netif_err(qdev, rx_err, qdev->ndev,
			  "Management Processor Fatal Error.\n");
		qlge_queue_fw_error(qdev);
		return;

	case CAM_LOOKUP_ERR_EVENT:
		netdev_err(qdev->ndev, "Multiple CAM hits lookup occurred.\n");
		netdev_err(qdev->ndev, "This event shouldn't occur.\n");
		qlge_queue_asic_error(qdev);
		return;

	case SOFT_ECC_ERROR_EVENT:
		netdev_err(qdev->ndev, "Soft ECC error detected.\n");
		qlge_queue_asic_error(qdev);
		break;

	case PCI_ERR_ANON_BUF_RD:
		netdev_err(qdev->ndev,
			   "PCI error occurred when reading anonymous buffers from rx_ring %d.\n",
			   ib_ae_rsp->q_id);
		qlge_queue_asic_error(qdev);
		break;

	default:
		netif_err(qdev, drv, qdev->ndev, "Unexpected event %d.\n",
			  ib_ae_rsp->event);
		qlge_queue_asic_error(qdev);
		break;
	}
}

static int qlge_clean_outbound_rx_ring(struct rx_ring *rx_ring)
{
	struct qlge_adapter *qdev = rx_ring->qdev;
	u32 prod = qlge_read_sh_reg(rx_ring->prod_idx_sh_reg);
	struct qlge_ob_mac_iocb_rsp *net_rsp = NULL;
	int count = 0;

	struct tx_ring *tx_ring;
	 
	while (prod != rx_ring->cnsmr_idx) {
		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
			     "cq_id = %d, prod = %d, cnsmr = %d\n",
			     rx_ring->cq_id, prod, rx_ring->cnsmr_idx);

		net_rsp = (struct qlge_ob_mac_iocb_rsp *)rx_ring->curr_entry;
		rmb();
		switch (net_rsp->opcode) {
		case OPCODE_OB_MAC_TSO_IOCB:
		case OPCODE_OB_MAC_IOCB:
			qlge_process_mac_tx_intr(qdev, net_rsp);
			break;
		default:
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "Hit default case, not handled! dropping the packet, opcode = %x.\n",
				     net_rsp->opcode);
		}
		count++;
		qlge_update_cq(rx_ring);
		prod = qlge_read_sh_reg(rx_ring->prod_idx_sh_reg);
	}
	if (!net_rsp)
		return 0;
	qlge_write_cq_idx(rx_ring);
	tx_ring = &qdev->tx_ring[net_rsp->txq_idx];
	if (__netif_subqueue_stopped(qdev->ndev, tx_ring->wq_id)) {
		if ((atomic_read(&tx_ring->tx_count) > (tx_ring->wq_len / 4)))
			 
			netif_wake_subqueue(qdev->ndev, tx_ring->wq_id);
	}

	return count;
}

static int qlge_clean_inbound_rx_ring(struct rx_ring *rx_ring, int budget)
{
	struct qlge_adapter *qdev = rx_ring->qdev;
	u32 prod = qlge_read_sh_reg(rx_ring->prod_idx_sh_reg);
	struct qlge_net_rsp_iocb *net_rsp;
	int count = 0;

	 
	while (prod != rx_ring->cnsmr_idx) {
		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
			     "cq_id = %d, prod = %d, cnsmr = %d\n",
			     rx_ring->cq_id, prod, rx_ring->cnsmr_idx);

		net_rsp = rx_ring->curr_entry;
		rmb();
		switch (net_rsp->opcode) {
		case OPCODE_IB_MAC_IOCB:
			qlge_process_mac_rx_intr(qdev, rx_ring,
						 (struct qlge_ib_mac_iocb_rsp *)
						 net_rsp);
			break;

		case OPCODE_IB_AE_IOCB:
			qlge_process_chip_ae_intr(qdev, (struct qlge_ib_ae_iocb_rsp *)
						  net_rsp);
			break;
		default:
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "Hit default case, not handled! dropping the packet, opcode = %x.\n",
				     net_rsp->opcode);
			break;
		}
		count++;
		qlge_update_cq(rx_ring);
		prod = qlge_read_sh_reg(rx_ring->prod_idx_sh_reg);
		if (count == budget)
			break;
	}
	qlge_update_buffer_queues(rx_ring, GFP_ATOMIC, 0);
	qlge_write_cq_idx(rx_ring);
	return count;
}

static int qlge_napi_poll_msix(struct napi_struct *napi, int budget)
{
	struct rx_ring *rx_ring = container_of(napi, struct rx_ring, napi);
	struct qlge_adapter *qdev = rx_ring->qdev;
	struct rx_ring *trx_ring;
	int i, work_done = 0;
	struct intr_context *ctx = &qdev->intr_context[rx_ring->cq_id];

	netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
		     "Enter, NAPI POLL cq_id = %d.\n", rx_ring->cq_id);

	 
	for (i = qdev->rss_ring_count; i < qdev->rx_ring_count; i++) {
		trx_ring = &qdev->rx_ring[i];
		 
		if ((ctx->irq_mask & (1 << trx_ring->cq_id)) &&
		    (qlge_read_sh_reg(trx_ring->prod_idx_sh_reg) !=
		     trx_ring->cnsmr_idx)) {
			netif_printk(qdev, intr, KERN_DEBUG, qdev->ndev,
				     "%s: Servicing TX completion ring %d.\n",
				     __func__, trx_ring->cq_id);
			qlge_clean_outbound_rx_ring(trx_ring);
		}
	}

	 
	if (qlge_read_sh_reg(rx_ring->prod_idx_sh_reg) !=
	    rx_ring->cnsmr_idx) {
		netif_printk(qdev, intr, KERN_DEBUG, qdev->ndev,
			     "%s: Servicing RX completion ring %d.\n",
			     __func__, rx_ring->cq_id);
		work_done = qlge_clean_inbound_rx_ring(rx_ring, budget);
	}

	if (work_done < budget) {
		napi_complete_done(napi, work_done);
		qlge_enable_completion_interrupt(qdev, rx_ring->irq);
	}
	return work_done;
}

static void qlge_vlan_mode(struct net_device *ndev, netdev_features_t features)
{
	struct qlge_adapter *qdev = netdev_to_qdev(ndev);

	if (features & NETIF_F_HW_VLAN_CTAG_RX) {
		qlge_write32(qdev, NIC_RCV_CFG, NIC_RCV_CFG_VLAN_MASK |
			     NIC_RCV_CFG_VLAN_MATCH_AND_NON);
	} else {
		qlge_write32(qdev, NIC_RCV_CFG, NIC_RCV_CFG_VLAN_MASK);
	}
}

 
static int qlge_update_hw_vlan_features(struct net_device *ndev,
					netdev_features_t features)
{
	struct qlge_adapter *qdev = netdev_to_qdev(ndev);
	bool need_restart = netif_running(ndev);
	int status = 0;

	if (need_restart) {
		status = qlge_adapter_down(qdev);
		if (status) {
			netif_err(qdev, link, qdev->ndev,
				  "Failed to bring down the adapter\n");
			return status;
		}
	}

	 
	ndev->features = features;

	if (need_restart) {
		status = qlge_adapter_up(qdev);
		if (status) {
			netif_err(qdev, link, qdev->ndev,
				  "Failed to bring up the adapter\n");
			return status;
		}
	}

	return status;
}

static int qlge_set_features(struct net_device *ndev,
			     netdev_features_t features)
{
	netdev_features_t changed = ndev->features ^ features;
	int err;

	if (changed & NETIF_F_HW_VLAN_CTAG_RX) {
		 
		err = qlge_update_hw_vlan_features(ndev, features);
		if (err)
			return err;

		qlge_vlan_mode(ndev, features);
	}

	return 0;
}

static int __qlge_vlan_rx_add_vid(struct qlge_adapter *qdev, u16 vid)
{
	u32 enable_bit = MAC_ADDR_E;
	int err;

	err = qlge_set_mac_addr_reg(qdev, (u8 *)&enable_bit,
				    MAC_ADDR_TYPE_VLAN, vid);
	if (err)
		netif_err(qdev, ifup, qdev->ndev,
			  "Failed to init vlan address.\n");
	return err;
}

static int qlge_vlan_rx_add_vid(struct net_device *ndev, __be16 proto, u16 vid)
{
	struct qlge_adapter *qdev = netdev_to_qdev(ndev);
	int status;
	int err;

	status = qlge_sem_spinlock(qdev, SEM_MAC_ADDR_MASK);
	if (status)
		return status;

	err = __qlge_vlan_rx_add_vid(qdev, vid);
	set_bit(vid, qdev->active_vlans);

	qlge_sem_unlock(qdev, SEM_MAC_ADDR_MASK);

	return err;
}

static int __qlge_vlan_rx_kill_vid(struct qlge_adapter *qdev, u16 vid)
{
	u32 enable_bit = 0;
	int err;

	err = qlge_set_mac_addr_reg(qdev, (u8 *)&enable_bit,
				    MAC_ADDR_TYPE_VLAN, vid);
	if (err)
		netif_err(qdev, ifup, qdev->ndev,
			  "Failed to clear vlan address.\n");
	return err;
}

static int qlge_vlan_rx_kill_vid(struct net_device *ndev, __be16 proto, u16 vid)
{
	struct qlge_adapter *qdev = netdev_to_qdev(ndev);
	int status;
	int err;

	status = qlge_sem_spinlock(qdev, SEM_MAC_ADDR_MASK);
	if (status)
		return status;

	err = __qlge_vlan_rx_kill_vid(qdev, vid);
	clear_bit(vid, qdev->active_vlans);

	qlge_sem_unlock(qdev, SEM_MAC_ADDR_MASK);

	return err;
}

static void qlge_restore_vlan(struct qlge_adapter *qdev)
{
	int status;
	u16 vid;

	status = qlge_sem_spinlock(qdev, SEM_MAC_ADDR_MASK);
	if (status)
		return;

	for_each_set_bit(vid, qdev->active_vlans, VLAN_N_VID)
		__qlge_vlan_rx_add_vid(qdev, vid);

	qlge_sem_unlock(qdev, SEM_MAC_ADDR_MASK);
}

 
static irqreturn_t qlge_msix_rx_isr(int irq, void *dev_id)
{
	struct rx_ring *rx_ring = dev_id;

	napi_schedule(&rx_ring->napi);
	return IRQ_HANDLED;
}

 
static irqreturn_t qlge_isr(int irq, void *dev_id)
{
	struct rx_ring *rx_ring = dev_id;
	struct qlge_adapter *qdev = rx_ring->qdev;
	struct intr_context *intr_context = &qdev->intr_context[0];
	u32 var;
	int work_done = 0;

	 
	if (!test_bit(QL_MSIX_ENABLED, &qdev->flags))
		qlge_disable_completion_interrupt(qdev, 0);

	var = qlge_read32(qdev, STS);

	 
	if (var & STS_FE) {
		qlge_disable_completion_interrupt(qdev, 0);
		qlge_queue_asic_error(qdev);
		netdev_err(qdev->ndev, "Got fatal error, STS = %x.\n", var);
		var = qlge_read32(qdev, ERR_STS);
		netdev_err(qdev->ndev, "Resetting chip. Error Status Register = 0x%x\n", var);
		return IRQ_HANDLED;
	}

	 
	if ((var & STS_PI) &&
	    (qlge_read32(qdev, INTR_MASK) & INTR_MASK_PI)) {
		 
		netif_err(qdev, intr, qdev->ndev,
			  "Got MPI processor interrupt.\n");
		qlge_write32(qdev, INTR_MASK, (INTR_MASK_PI << 16));
		queue_delayed_work_on(smp_processor_id(),
				      qdev->workqueue, &qdev->mpi_work, 0);
		work_done++;
	}

	 
	var = qlge_read32(qdev, ISR1);
	if (var & intr_context->irq_mask) {
		netif_info(qdev, intr, qdev->ndev,
			   "Waking handler for rx_ring[0].\n");
		napi_schedule(&rx_ring->napi);
		work_done++;
	} else {
		 
		qlge_enable_completion_interrupt(qdev, 0);
	}

	return work_done ? IRQ_HANDLED : IRQ_NONE;
}

static int qlge_tso(struct sk_buff *skb, struct qlge_ob_mac_tso_iocb_req *mac_iocb_ptr)
{
	if (skb_is_gso(skb)) {
		int err;
		__be16 l3_proto = vlan_get_protocol(skb);

		err = skb_cow_head(skb, 0);
		if (err < 0)
			return err;

		mac_iocb_ptr->opcode = OPCODE_OB_MAC_TSO_IOCB;
		mac_iocb_ptr->flags3 |= OB_MAC_TSO_IOCB_IC;
		mac_iocb_ptr->frame_len = cpu_to_le32((u32)skb->len);
		mac_iocb_ptr->total_hdrs_len =
			cpu_to_le16(skb_tcp_all_headers(skb));
		mac_iocb_ptr->net_trans_offset =
			cpu_to_le16(skb_network_offset(skb) |
				    skb_transport_offset(skb)
				    << OB_MAC_TRANSPORT_HDR_SHIFT);
		mac_iocb_ptr->mss = cpu_to_le16(skb_shinfo(skb)->gso_size);
		mac_iocb_ptr->flags2 |= OB_MAC_TSO_IOCB_LSO;
		if (likely(l3_proto == htons(ETH_P_IP))) {
			struct iphdr *iph = ip_hdr(skb);

			iph->check = 0;
			mac_iocb_ptr->flags1 |= OB_MAC_TSO_IOCB_IP4;
			tcp_hdr(skb)->check = ~csum_tcpudp_magic(iph->saddr,
								 iph->daddr, 0,
								 IPPROTO_TCP,
								 0);
		} else if (l3_proto == htons(ETH_P_IPV6)) {
			mac_iocb_ptr->flags1 |= OB_MAC_TSO_IOCB_IP6;
			tcp_hdr(skb)->check =
				~csum_ipv6_magic(&ipv6_hdr(skb)->saddr,
						 &ipv6_hdr(skb)->daddr,
						 0, IPPROTO_TCP, 0);
		}
		return 1;
	}
	return 0;
}

static void qlge_hw_csum_setup(struct sk_buff *skb,
			       struct qlge_ob_mac_tso_iocb_req *mac_iocb_ptr)
{
	int len;
	struct iphdr *iph = ip_hdr(skb);
	__sum16 *check;

	mac_iocb_ptr->opcode = OPCODE_OB_MAC_TSO_IOCB;
	mac_iocb_ptr->frame_len = cpu_to_le32((u32)skb->len);
	mac_iocb_ptr->net_trans_offset =
		cpu_to_le16(skb_network_offset(skb) |
			    skb_transport_offset(skb) << OB_MAC_TRANSPORT_HDR_SHIFT);

	mac_iocb_ptr->flags1 |= OB_MAC_TSO_IOCB_IP4;
	len = (ntohs(iph->tot_len) - (iph->ihl << 2));
	if (likely(iph->protocol == IPPROTO_TCP)) {
		check = &(tcp_hdr(skb)->check);
		mac_iocb_ptr->flags2 |= OB_MAC_TSO_IOCB_TC;
		mac_iocb_ptr->total_hdrs_len =
			cpu_to_le16(skb_transport_offset(skb) +
				    (tcp_hdr(skb)->doff << 2));
	} else {
		check = &(udp_hdr(skb)->check);
		mac_iocb_ptr->flags2 |= OB_MAC_TSO_IOCB_UC;
		mac_iocb_ptr->total_hdrs_len =
			cpu_to_le16(skb_transport_offset(skb) +
				    sizeof(struct udphdr));
	}
	*check = ~csum_tcpudp_magic(iph->saddr,
				    iph->daddr, len, iph->protocol, 0);
}

static netdev_tx_t qlge_send(struct sk_buff *skb, struct net_device *ndev)
{
	struct qlge_adapter *qdev = netdev_to_qdev(ndev);
	struct qlge_ob_mac_iocb_req *mac_iocb_ptr;
	struct tx_ring_desc *tx_ring_desc;
	int tso;
	struct tx_ring *tx_ring;
	u32 tx_ring_idx = (u32)skb->queue_mapping;

	tx_ring = &qdev->tx_ring[tx_ring_idx];

	if (skb_padto(skb, ETH_ZLEN))
		return NETDEV_TX_OK;

	if (unlikely(atomic_read(&tx_ring->tx_count) < 2)) {
		netif_info(qdev, tx_queued, qdev->ndev,
			   "%s: BUG! shutting down tx queue %d due to lack of resources.\n",
			   __func__, tx_ring_idx);
		netif_stop_subqueue(ndev, tx_ring->wq_id);
		tx_ring->tx_errors++;
		return NETDEV_TX_BUSY;
	}
	tx_ring_desc = &tx_ring->q[tx_ring->prod_idx];
	mac_iocb_ptr = tx_ring_desc->queue_entry;
	memset((void *)mac_iocb_ptr, 0, sizeof(*mac_iocb_ptr));

	mac_iocb_ptr->opcode = OPCODE_OB_MAC_IOCB;
	mac_iocb_ptr->tid = tx_ring_desc->index;
	 
	mac_iocb_ptr->txq_idx = tx_ring_idx;
	tx_ring_desc->skb = skb;

	mac_iocb_ptr->frame_len = cpu_to_le16((u16)skb->len);

	if (skb_vlan_tag_present(skb)) {
		netif_printk(qdev, tx_queued, KERN_DEBUG, qdev->ndev,
			     "Adding a vlan tag %d.\n", skb_vlan_tag_get(skb));
		mac_iocb_ptr->flags3 |= OB_MAC_IOCB_V;
		mac_iocb_ptr->vlan_tci = cpu_to_le16(skb_vlan_tag_get(skb));
	}
	tso = qlge_tso(skb, (struct qlge_ob_mac_tso_iocb_req *)mac_iocb_ptr);
	if (tso < 0) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	} else if (unlikely(!tso) && (skb->ip_summed == CHECKSUM_PARTIAL)) {
		qlge_hw_csum_setup(skb,
				   (struct qlge_ob_mac_tso_iocb_req *)mac_iocb_ptr);
	}
	if (qlge_map_send(qdev, mac_iocb_ptr, skb, tx_ring_desc) !=
	    NETDEV_TX_OK) {
		netif_err(qdev, tx_queued, qdev->ndev,
			  "Could not map the segments.\n");
		tx_ring->tx_errors++;
		return NETDEV_TX_BUSY;
	}

	tx_ring->prod_idx++;
	if (tx_ring->prod_idx == tx_ring->wq_len)
		tx_ring->prod_idx = 0;
	wmb();

	qlge_write_db_reg_relaxed(tx_ring->prod_idx, tx_ring->prod_idx_db_reg);
	netif_printk(qdev, tx_queued, KERN_DEBUG, qdev->ndev,
		     "tx queued, slot %d, len %d\n",
		     tx_ring->prod_idx, skb->len);

	atomic_dec(&tx_ring->tx_count);

	if (unlikely(atomic_read(&tx_ring->tx_count) < 2)) {
		netif_stop_subqueue(ndev, tx_ring->wq_id);
		if ((atomic_read(&tx_ring->tx_count) > (tx_ring->wq_len / 4)))
			 
			netif_wake_subqueue(qdev->ndev, tx_ring->wq_id);
	}
	return NETDEV_TX_OK;
}

static void qlge_free_shadow_space(struct qlge_adapter *qdev)
{
	if (qdev->rx_ring_shadow_reg_area) {
		dma_free_coherent(&qdev->pdev->dev,
				  PAGE_SIZE,
				  qdev->rx_ring_shadow_reg_area,
				  qdev->rx_ring_shadow_reg_dma);
		qdev->rx_ring_shadow_reg_area = NULL;
	}
	if (qdev->tx_ring_shadow_reg_area) {
		dma_free_coherent(&qdev->pdev->dev,
				  PAGE_SIZE,
				  qdev->tx_ring_shadow_reg_area,
				  qdev->tx_ring_shadow_reg_dma);
		qdev->tx_ring_shadow_reg_area = NULL;
	}
}

static int qlge_alloc_shadow_space(struct qlge_adapter *qdev)
{
	qdev->rx_ring_shadow_reg_area =
		dma_alloc_coherent(&qdev->pdev->dev, PAGE_SIZE,
				   &qdev->rx_ring_shadow_reg_dma, GFP_ATOMIC);
	if (!qdev->rx_ring_shadow_reg_area) {
		netif_err(qdev, ifup, qdev->ndev,
			  "Allocation of RX shadow space failed.\n");
		return -ENOMEM;
	}

	qdev->tx_ring_shadow_reg_area =
		dma_alloc_coherent(&qdev->pdev->dev, PAGE_SIZE,
				   &qdev->tx_ring_shadow_reg_dma, GFP_ATOMIC);
	if (!qdev->tx_ring_shadow_reg_area) {
		netif_err(qdev, ifup, qdev->ndev,
			  "Allocation of TX shadow space failed.\n");
		goto err_wqp_sh_area;
	}
	return 0;

err_wqp_sh_area:
	dma_free_coherent(&qdev->pdev->dev,
			  PAGE_SIZE,
			  qdev->rx_ring_shadow_reg_area,
			  qdev->rx_ring_shadow_reg_dma);
	return -ENOMEM;
}

static void qlge_init_tx_ring(struct qlge_adapter *qdev, struct tx_ring *tx_ring)
{
	struct tx_ring_desc *tx_ring_desc;
	int i;
	struct qlge_ob_mac_iocb_req *mac_iocb_ptr;

	mac_iocb_ptr = tx_ring->wq_base;
	tx_ring_desc = tx_ring->q;
	for (i = 0; i < tx_ring->wq_len; i++) {
		tx_ring_desc->index = i;
		tx_ring_desc->skb = NULL;
		tx_ring_desc->queue_entry = mac_iocb_ptr;
		mac_iocb_ptr++;
		tx_ring_desc++;
	}
	atomic_set(&tx_ring->tx_count, tx_ring->wq_len);
}

static void qlge_free_tx_resources(struct qlge_adapter *qdev,
				   struct tx_ring *tx_ring)
{
	if (tx_ring->wq_base) {
		dma_free_coherent(&qdev->pdev->dev, tx_ring->wq_size,
				  tx_ring->wq_base, tx_ring->wq_base_dma);
		tx_ring->wq_base = NULL;
	}
	kfree(tx_ring->q);
	tx_ring->q = NULL;
}

static int qlge_alloc_tx_resources(struct qlge_adapter *qdev,
				   struct tx_ring *tx_ring)
{
	tx_ring->wq_base =
		dma_alloc_coherent(&qdev->pdev->dev, tx_ring->wq_size,
				   &tx_ring->wq_base_dma, GFP_ATOMIC);

	if (!tx_ring->wq_base ||
	    tx_ring->wq_base_dma & WQ_ADDR_ALIGN)
		goto pci_alloc_err;

	tx_ring->q =
		kmalloc_array(tx_ring->wq_len, sizeof(struct tx_ring_desc),
			      GFP_KERNEL);
	if (!tx_ring->q)
		goto err;

	return 0;
err:
	dma_free_coherent(&qdev->pdev->dev, tx_ring->wq_size,
			  tx_ring->wq_base, tx_ring->wq_base_dma);
	tx_ring->wq_base = NULL;
pci_alloc_err:
	netif_err(qdev, ifup, qdev->ndev, "tx_ring alloc failed.\n");
	return -ENOMEM;
}

static void qlge_free_lbq_buffers(struct qlge_adapter *qdev, struct rx_ring *rx_ring)
{
	struct qlge_bq *lbq = &rx_ring->lbq;
	unsigned int last_offset;

	last_offset = qlge_lbq_block_size(qdev) - qdev->lbq_buf_size;
	while (lbq->next_to_clean != lbq->next_to_use) {
		struct qlge_bq_desc *lbq_desc =
			&lbq->queue[lbq->next_to_clean];

		if (lbq_desc->p.pg_chunk.offset == last_offset)
			dma_unmap_page(&qdev->pdev->dev, lbq_desc->dma_addr,
				       qlge_lbq_block_size(qdev),
				       DMA_FROM_DEVICE);
		put_page(lbq_desc->p.pg_chunk.page);

		lbq->next_to_clean = QLGE_BQ_WRAP(lbq->next_to_clean + 1);
	}

	if (rx_ring->master_chunk.page) {
		dma_unmap_page(&qdev->pdev->dev, rx_ring->chunk_dma_addr,
			       qlge_lbq_block_size(qdev), DMA_FROM_DEVICE);
		put_page(rx_ring->master_chunk.page);
		rx_ring->master_chunk.page = NULL;
	}
}

static void qlge_free_sbq_buffers(struct qlge_adapter *qdev, struct rx_ring *rx_ring)
{
	int i;

	for (i = 0; i < QLGE_BQ_LEN; i++) {
		struct qlge_bq_desc *sbq_desc = &rx_ring->sbq.queue[i];

		if (!sbq_desc) {
			netif_err(qdev, ifup, qdev->ndev,
				  "sbq_desc %d is NULL.\n", i);
			return;
		}
		if (sbq_desc->p.skb) {
			dma_unmap_single(&qdev->pdev->dev, sbq_desc->dma_addr,
					 SMALL_BUF_MAP_SIZE,
					 DMA_FROM_DEVICE);
			dev_kfree_skb(sbq_desc->p.skb);
			sbq_desc->p.skb = NULL;
		}
	}
}

 
static void qlge_free_rx_buffers(struct qlge_adapter *qdev)
{
	int i;

	for (i = 0; i < qdev->rx_ring_count; i++) {
		struct rx_ring *rx_ring = &qdev->rx_ring[i];

		if (rx_ring->lbq.queue)
			qlge_free_lbq_buffers(qdev, rx_ring);
		if (rx_ring->sbq.queue)
			qlge_free_sbq_buffers(qdev, rx_ring);
	}
}

static void qlge_alloc_rx_buffers(struct qlge_adapter *qdev)
{
	int i;

	for (i = 0; i < qdev->rss_ring_count; i++)
		qlge_update_buffer_queues(&qdev->rx_ring[i], GFP_KERNEL,
					  HZ / 2);
}

static int qlge_init_bq(struct qlge_bq *bq)
{
	struct rx_ring *rx_ring = QLGE_BQ_CONTAINER(bq);
	struct qlge_adapter *qdev = rx_ring->qdev;
	struct qlge_bq_desc *bq_desc;
	__le64 *buf_ptr;
	int i;

	bq->base = dma_alloc_coherent(&qdev->pdev->dev, QLGE_BQ_SIZE,
				      &bq->base_dma, GFP_ATOMIC);
	if (!bq->base)
		return -ENOMEM;

	bq->queue = kmalloc_array(QLGE_BQ_LEN, sizeof(struct qlge_bq_desc),
				  GFP_KERNEL);
	if (!bq->queue)
		return -ENOMEM;

	buf_ptr = bq->base;
	bq_desc = &bq->queue[0];
	for (i = 0; i < QLGE_BQ_LEN; i++, buf_ptr++, bq_desc++) {
		bq_desc->p.skb = NULL;
		bq_desc->index = i;
		bq_desc->buf_ptr = buf_ptr;
	}

	return 0;
}

static void qlge_free_rx_resources(struct qlge_adapter *qdev,
				   struct rx_ring *rx_ring)
{
	 
	if (rx_ring->sbq.base) {
		dma_free_coherent(&qdev->pdev->dev, QLGE_BQ_SIZE,
				  rx_ring->sbq.base, rx_ring->sbq.base_dma);
		rx_ring->sbq.base = NULL;
	}

	 
	kfree(rx_ring->sbq.queue);
	rx_ring->sbq.queue = NULL;

	 
	if (rx_ring->lbq.base) {
		dma_free_coherent(&qdev->pdev->dev, QLGE_BQ_SIZE,
				  rx_ring->lbq.base, rx_ring->lbq.base_dma);
		rx_ring->lbq.base = NULL;
	}

	 
	kfree(rx_ring->lbq.queue);
	rx_ring->lbq.queue = NULL;

	 
	if (rx_ring->cq_base) {
		dma_free_coherent(&qdev->pdev->dev,
				  rx_ring->cq_size,
				  rx_ring->cq_base, rx_ring->cq_base_dma);
		rx_ring->cq_base = NULL;
	}
}

 
static int qlge_alloc_rx_resources(struct qlge_adapter *qdev,
				   struct rx_ring *rx_ring)
{
	 
	rx_ring->cq_base =
		dma_alloc_coherent(&qdev->pdev->dev, rx_ring->cq_size,
				   &rx_ring->cq_base_dma, GFP_ATOMIC);

	if (!rx_ring->cq_base) {
		netif_err(qdev, ifup, qdev->ndev, "rx_ring alloc failed.\n");
		return -ENOMEM;
	}

	if (rx_ring->cq_id < qdev->rss_ring_count &&
	    (qlge_init_bq(&rx_ring->sbq) || qlge_init_bq(&rx_ring->lbq))) {
		qlge_free_rx_resources(qdev, rx_ring);
		return -ENOMEM;
	}

	return 0;
}

static void qlge_tx_ring_clean(struct qlge_adapter *qdev)
{
	struct tx_ring *tx_ring;
	struct tx_ring_desc *tx_ring_desc;
	int i, j;

	 
	for (j = 0; j < qdev->tx_ring_count; j++) {
		tx_ring = &qdev->tx_ring[j];
		for (i = 0; i < tx_ring->wq_len; i++) {
			tx_ring_desc = &tx_ring->q[i];
			if (tx_ring_desc && tx_ring_desc->skb) {
				netif_err(qdev, ifdown, qdev->ndev,
					  "Freeing lost SKB %p, from queue %d, index %d.\n",
					  tx_ring_desc->skb, j,
					  tx_ring_desc->index);
				qlge_unmap_send(qdev, tx_ring_desc,
						tx_ring_desc->map_cnt);
				dev_kfree_skb(tx_ring_desc->skb);
				tx_ring_desc->skb = NULL;
			}
		}
	}
}

static void qlge_free_mem_resources(struct qlge_adapter *qdev)
{
	int i;

	for (i = 0; i < qdev->tx_ring_count; i++)
		qlge_free_tx_resources(qdev, &qdev->tx_ring[i]);
	for (i = 0; i < qdev->rx_ring_count; i++)
		qlge_free_rx_resources(qdev, &qdev->rx_ring[i]);
	qlge_free_shadow_space(qdev);
}

static int qlge_alloc_mem_resources(struct qlge_adapter *qdev)
{
	int i;

	 
	if (qlge_alloc_shadow_space(qdev))
		return -ENOMEM;

	for (i = 0; i < qdev->rx_ring_count; i++) {
		if (qlge_alloc_rx_resources(qdev, &qdev->rx_ring[i]) != 0) {
			netif_err(qdev, ifup, qdev->ndev,
				  "RX resource allocation failed.\n");
			goto err_mem;
		}
	}
	 
	for (i = 0; i < qdev->tx_ring_count; i++) {
		if (qlge_alloc_tx_resources(qdev, &qdev->tx_ring[i]) != 0) {
			netif_err(qdev, ifup, qdev->ndev,
				  "TX resource allocation failed.\n");
			goto err_mem;
		}
	}
	return 0;

err_mem:
	qlge_free_mem_resources(qdev);
	return -ENOMEM;
}

 
static int qlge_start_rx_ring(struct qlge_adapter *qdev, struct rx_ring *rx_ring)
{
	struct cqicb *cqicb = &rx_ring->cqicb;
	void *shadow_reg = qdev->rx_ring_shadow_reg_area +
		(rx_ring->cq_id * RX_RING_SHADOW_SPACE);
	u64 shadow_reg_dma = qdev->rx_ring_shadow_reg_dma +
		(rx_ring->cq_id * RX_RING_SHADOW_SPACE);
	void __iomem *doorbell_area =
		qdev->doorbell_area + (DB_PAGE_SIZE * (128 + rx_ring->cq_id));
	int err = 0;
	u64 dma;
	__le64 *base_indirect_ptr;
	int page_entries;

	 
	rx_ring->prod_idx_sh_reg = shadow_reg;
	rx_ring->prod_idx_sh_reg_dma = shadow_reg_dma;
	*rx_ring->prod_idx_sh_reg = 0;
	shadow_reg += sizeof(u64);
	shadow_reg_dma += sizeof(u64);
	rx_ring->lbq.base_indirect = shadow_reg;
	rx_ring->lbq.base_indirect_dma = shadow_reg_dma;
	shadow_reg += (sizeof(u64) * MAX_DB_PAGES_PER_BQ(QLGE_BQ_LEN));
	shadow_reg_dma += (sizeof(u64) * MAX_DB_PAGES_PER_BQ(QLGE_BQ_LEN));
	rx_ring->sbq.base_indirect = shadow_reg;
	rx_ring->sbq.base_indirect_dma = shadow_reg_dma;

	 
	rx_ring->cnsmr_idx_db_reg = (u32 __iomem *)doorbell_area;
	rx_ring->cnsmr_idx = 0;
	rx_ring->curr_entry = rx_ring->cq_base;

	 
	rx_ring->valid_db_reg = doorbell_area + 0x04;

	 
	rx_ring->lbq.prod_idx_db_reg = (u32 __iomem *)(doorbell_area + 0x18);

	 
	rx_ring->sbq.prod_idx_db_reg = (u32 __iomem *)(doorbell_area + 0x1c);

	memset((void *)cqicb, 0, sizeof(struct cqicb));
	cqicb->msix_vect = rx_ring->irq;

	cqicb->len = cpu_to_le16(QLGE_FIT16(rx_ring->cq_len) | LEN_V |
				 LEN_CPP_CONT);

	cqicb->addr = cpu_to_le64(rx_ring->cq_base_dma);

	cqicb->prod_idx_addr = cpu_to_le64(rx_ring->prod_idx_sh_reg_dma);

	 
	cqicb->flags = FLAGS_LC |	 
		FLAGS_LV |		 
		FLAGS_LI;		 
	if (rx_ring->cq_id < qdev->rss_ring_count) {
		cqicb->flags |= FLAGS_LL;	 
		dma = (u64)rx_ring->lbq.base_dma;
		base_indirect_ptr = rx_ring->lbq.base_indirect;

		for (page_entries = 0;
		     page_entries < MAX_DB_PAGES_PER_BQ(QLGE_BQ_LEN);
		     page_entries++) {
			base_indirect_ptr[page_entries] = cpu_to_le64(dma);
			dma += DB_PAGE_SIZE;
		}
		cqicb->lbq_addr = cpu_to_le64(rx_ring->lbq.base_indirect_dma);
		cqicb->lbq_buf_size =
			cpu_to_le16(QLGE_FIT16(qdev->lbq_buf_size));
		cqicb->lbq_len = cpu_to_le16(QLGE_FIT16(QLGE_BQ_LEN));
		rx_ring->lbq.next_to_use = 0;
		rx_ring->lbq.next_to_clean = 0;

		cqicb->flags |= FLAGS_LS;	 
		dma = (u64)rx_ring->sbq.base_dma;
		base_indirect_ptr = rx_ring->sbq.base_indirect;

		for (page_entries = 0;
		     page_entries < MAX_DB_PAGES_PER_BQ(QLGE_BQ_LEN);
		     page_entries++) {
			base_indirect_ptr[page_entries] = cpu_to_le64(dma);
			dma += DB_PAGE_SIZE;
		}
		cqicb->sbq_addr =
			cpu_to_le64(rx_ring->sbq.base_indirect_dma);
		cqicb->sbq_buf_size = cpu_to_le16(SMALL_BUFFER_SIZE);
		cqicb->sbq_len = cpu_to_le16(QLGE_FIT16(QLGE_BQ_LEN));
		rx_ring->sbq.next_to_use = 0;
		rx_ring->sbq.next_to_clean = 0;
	}
	if (rx_ring->cq_id < qdev->rss_ring_count) {
		 
		netif_napi_add(qdev->ndev, &rx_ring->napi,
			       qlge_napi_poll_msix);
		cqicb->irq_delay = cpu_to_le16(qdev->rx_coalesce_usecs);
		cqicb->pkt_delay = cpu_to_le16(qdev->rx_max_coalesced_frames);
	} else {
		cqicb->irq_delay = cpu_to_le16(qdev->tx_coalesce_usecs);
		cqicb->pkt_delay = cpu_to_le16(qdev->tx_max_coalesced_frames);
	}
	err = qlge_write_cfg(qdev, cqicb, sizeof(struct cqicb),
			     CFG_LCQ, rx_ring->cq_id);
	if (err) {
		netif_err(qdev, ifup, qdev->ndev, "Failed to load CQICB.\n");
		return err;
	}
	return err;
}

static int qlge_start_tx_ring(struct qlge_adapter *qdev, struct tx_ring *tx_ring)
{
	struct wqicb *wqicb = (struct wqicb *)tx_ring;
	void __iomem *doorbell_area =
		qdev->doorbell_area + (DB_PAGE_SIZE * tx_ring->wq_id);
	void *shadow_reg = qdev->tx_ring_shadow_reg_area +
		(tx_ring->wq_id * sizeof(u64));
	u64 shadow_reg_dma = qdev->tx_ring_shadow_reg_dma +
		(tx_ring->wq_id * sizeof(u64));
	int err = 0;

	 
	 
	tx_ring->prod_idx_db_reg = (u32 __iomem *)doorbell_area;
	tx_ring->prod_idx = 0;
	 
	tx_ring->valid_db_reg = doorbell_area + 0x04;

	 
	tx_ring->cnsmr_idx_sh_reg = shadow_reg;
	tx_ring->cnsmr_idx_sh_reg_dma = shadow_reg_dma;

	wqicb->len = cpu_to_le16(tx_ring->wq_len | Q_LEN_V | Q_LEN_CPP_CONT);
	wqicb->flags = cpu_to_le16(Q_FLAGS_LC |
				   Q_FLAGS_LB | Q_FLAGS_LI | Q_FLAGS_LO);
	wqicb->cq_id_rss = cpu_to_le16(tx_ring->cq_id);
	wqicb->rid = 0;
	wqicb->addr = cpu_to_le64(tx_ring->wq_base_dma);

	wqicb->cnsmr_idx_addr = cpu_to_le64(tx_ring->cnsmr_idx_sh_reg_dma);

	qlge_init_tx_ring(qdev, tx_ring);

	err = qlge_write_cfg(qdev, wqicb, sizeof(*wqicb), CFG_LRQ,
			     (u16)tx_ring->wq_id);
	if (err) {
		netif_err(qdev, ifup, qdev->ndev, "Failed to load tx_ring.\n");
		return err;
	}
	return err;
}

static void qlge_disable_msix(struct qlge_adapter *qdev)
{
	if (test_bit(QL_MSIX_ENABLED, &qdev->flags)) {
		pci_disable_msix(qdev->pdev);
		clear_bit(QL_MSIX_ENABLED, &qdev->flags);
		kfree(qdev->msi_x_entry);
		qdev->msi_x_entry = NULL;
	} else if (test_bit(QL_MSI_ENABLED, &qdev->flags)) {
		pci_disable_msi(qdev->pdev);
		clear_bit(QL_MSI_ENABLED, &qdev->flags);
	}
}

 
static void qlge_enable_msix(struct qlge_adapter *qdev)
{
	int i, err;

	 
	if (qlge_irq_type == MSIX_IRQ) {
		 
		qdev->msi_x_entry = kcalloc(qdev->intr_count,
					    sizeof(struct msix_entry),
					    GFP_KERNEL);
		if (!qdev->msi_x_entry) {
			qlge_irq_type = MSI_IRQ;
			goto msi;
		}

		for (i = 0; i < qdev->intr_count; i++)
			qdev->msi_x_entry[i].entry = i;

		err = pci_enable_msix_range(qdev->pdev, qdev->msi_x_entry,
					    1, qdev->intr_count);
		if (err < 0) {
			kfree(qdev->msi_x_entry);
			qdev->msi_x_entry = NULL;
			netif_warn(qdev, ifup, qdev->ndev,
				   "MSI-X Enable failed, trying MSI.\n");
			qlge_irq_type = MSI_IRQ;
		} else {
			qdev->intr_count = err;
			set_bit(QL_MSIX_ENABLED, &qdev->flags);
			netif_info(qdev, ifup, qdev->ndev,
				   "MSI-X Enabled, got %d vectors.\n",
				   qdev->intr_count);
			return;
		}
	}
msi:
	qdev->intr_count = 1;
	if (qlge_irq_type == MSI_IRQ) {
		if (pci_alloc_irq_vectors(qdev->pdev, 1, 1, PCI_IRQ_MSI) >= 0) {
			set_bit(QL_MSI_ENABLED, &qdev->flags);
			netif_info(qdev, ifup, qdev->ndev,
				   "Running with MSI interrupts.\n");
			return;
		}
	}
	qlge_irq_type = LEG_IRQ;
	set_bit(QL_LEGACY_ENABLED, &qdev->flags);
	netif_printk(qdev, ifup, KERN_DEBUG, qdev->ndev,
		     "Running with legacy interrupts.\n");
}

 
static void qlge_set_tx_vect(struct qlge_adapter *qdev)
{
	int i, j, vect;
	u32 tx_rings_per_vector = qdev->tx_ring_count / qdev->intr_count;

	if (likely(test_bit(QL_MSIX_ENABLED, &qdev->flags))) {
		 
		for (vect = 0, j = 0, i = qdev->rss_ring_count;
		     i < qdev->rx_ring_count; i++) {
			if (j == tx_rings_per_vector) {
				vect++;
				j = 0;
			}
			qdev->rx_ring[i].irq = vect;
			j++;
		}
	} else {
		 
		for (i = 0; i < qdev->rx_ring_count; i++)
			qdev->rx_ring[i].irq = 0;
	}
}

 
static void qlge_set_irq_mask(struct qlge_adapter *qdev, struct intr_context *ctx)
{
	int j, vect = ctx->intr;
	u32 tx_rings_per_vector = qdev->tx_ring_count / qdev->intr_count;

	if (likely(test_bit(QL_MSIX_ENABLED, &qdev->flags))) {
		 
		ctx->irq_mask = (1 << qdev->rx_ring[vect].cq_id);
		 
		for (j = 0; j < tx_rings_per_vector; j++) {
			ctx->irq_mask |=
				(1 << qdev->rx_ring[qdev->rss_ring_count +
				 (vect * tx_rings_per_vector) + j].cq_id);
		}
	} else {
		 
		for (j = 0; j < qdev->rx_ring_count; j++)
			ctx->irq_mask |= (1 << qdev->rx_ring[j].cq_id);
	}
}

 
static void qlge_resolve_queues_to_irqs(struct qlge_adapter *qdev)
{
	int i = 0;
	struct intr_context *intr_context = &qdev->intr_context[0];

	if (likely(test_bit(QL_MSIX_ENABLED, &qdev->flags))) {
		 
		for (i = 0; i < qdev->intr_count; i++, intr_context++) {
			qdev->rx_ring[i].irq = i;
			intr_context->intr = i;
			intr_context->qdev = qdev;
			 
			qlge_set_irq_mask(qdev, intr_context);
			 
			intr_context->intr_en_mask =
				INTR_EN_TYPE_MASK | INTR_EN_INTR_MASK |
				INTR_EN_TYPE_ENABLE | INTR_EN_IHD_MASK | INTR_EN_IHD
				| i;
			intr_context->intr_dis_mask =
				INTR_EN_TYPE_MASK | INTR_EN_INTR_MASK |
				INTR_EN_TYPE_DISABLE | INTR_EN_IHD_MASK |
				INTR_EN_IHD | i;
			intr_context->intr_read_mask =
				INTR_EN_TYPE_MASK | INTR_EN_INTR_MASK |
				INTR_EN_TYPE_READ | INTR_EN_IHD_MASK | INTR_EN_IHD |
				i;
			if (i == 0) {
				 
				intr_context->handler = qlge_isr;
				sprintf(intr_context->name, "%s-rx-%d",
					qdev->ndev->name, i);
			} else {
				 
				intr_context->handler = qlge_msix_rx_isr;
				sprintf(intr_context->name, "%s-rx-%d",
					qdev->ndev->name, i);
			}
		}
	} else {
		 
		intr_context->intr = 0;
		intr_context->qdev = qdev;
		 
		intr_context->intr_en_mask =
			INTR_EN_TYPE_MASK | INTR_EN_INTR_MASK | INTR_EN_TYPE_ENABLE;
		intr_context->intr_dis_mask =
			INTR_EN_TYPE_MASK | INTR_EN_INTR_MASK |
			INTR_EN_TYPE_DISABLE;
		if (test_bit(QL_LEGACY_ENABLED, &qdev->flags)) {
			 
			intr_context->intr_en_mask |= INTR_EN_EI << 16 |
				INTR_EN_EI;
			intr_context->intr_dis_mask |= INTR_EN_EI << 16;
		}
		intr_context->intr_read_mask =
			INTR_EN_TYPE_MASK | INTR_EN_INTR_MASK | INTR_EN_TYPE_READ;
		 
		intr_context->handler = qlge_isr;
		sprintf(intr_context->name, "%s-single_irq", qdev->ndev->name);
		 
		qlge_set_irq_mask(qdev, intr_context);
	}
	 
	qlge_set_tx_vect(qdev);
}

static void qlge_free_irq(struct qlge_adapter *qdev)
{
	int i;
	struct intr_context *intr_context = &qdev->intr_context[0];

	for (i = 0; i < qdev->intr_count; i++, intr_context++) {
		if (intr_context->hooked) {
			if (test_bit(QL_MSIX_ENABLED, &qdev->flags)) {
				free_irq(qdev->msi_x_entry[i].vector,
					 &qdev->rx_ring[i]);
			} else {
				free_irq(qdev->pdev->irq, &qdev->rx_ring[0]);
			}
		}
	}
	qlge_disable_msix(qdev);
}

static int qlge_request_irq(struct qlge_adapter *qdev)
{
	int i;
	int status = 0;
	struct pci_dev *pdev = qdev->pdev;
	struct intr_context *intr_context = &qdev->intr_context[0];

	qlge_resolve_queues_to_irqs(qdev);

	for (i = 0; i < qdev->intr_count; i++, intr_context++) {
		if (test_bit(QL_MSIX_ENABLED, &qdev->flags)) {
			status = request_irq(qdev->msi_x_entry[i].vector,
					     intr_context->handler,
					     0,
					     intr_context->name,
					     &qdev->rx_ring[i]);
			if (status) {
				netif_err(qdev, ifup, qdev->ndev,
					  "Failed request for MSIX interrupt %d.\n",
					  i);
				goto err_irq;
			}
		} else {
			netif_printk(qdev, ifup, KERN_DEBUG, qdev->ndev,
				     "trying msi or legacy interrupts.\n");
			netif_printk(qdev, ifup, KERN_DEBUG, qdev->ndev,
				     "%s: irq = %d.\n", __func__, pdev->irq);
			netif_printk(qdev, ifup, KERN_DEBUG, qdev->ndev,
				     "%s: context->name = %s.\n", __func__,
				     intr_context->name);
			netif_printk(qdev, ifup, KERN_DEBUG, qdev->ndev,
				     "%s: dev_id = 0x%p.\n", __func__,
				     &qdev->rx_ring[0]);
			status =
				request_irq(pdev->irq, qlge_isr,
					    test_bit(QL_MSI_ENABLED, &qdev->flags)
					    ? 0
					    : IRQF_SHARED,
					    intr_context->name, &qdev->rx_ring[0]);
			if (status)
				goto err_irq;

			netif_err(qdev, ifup, qdev->ndev,
				  "Hooked intr 0, queue type RX_Q, with name %s.\n",
				  intr_context->name);
		}
		intr_context->hooked = 1;
	}
	return status;
err_irq:
	netif_err(qdev, ifup, qdev->ndev, "Failed to get the interrupts!!!\n");
	qlge_free_irq(qdev);
	return status;
}

static int qlge_start_rss(struct qlge_adapter *qdev)
{
	static const u8 init_hash_seed[] = {
		0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
		0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
		0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
		0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
		0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa
	};
	struct ricb *ricb = &qdev->ricb;
	int status = 0;
	int i;
	u8 *hash_id = (u8 *)ricb->hash_cq_id;

	memset((void *)ricb, 0, sizeof(*ricb));

	ricb->base_cq = RSS_L4K;
	ricb->flags =
		(RSS_L6K | RSS_LI | RSS_LB | RSS_LM | RSS_RT4 | RSS_RT6);
	ricb->mask = cpu_to_le16((u16)(0x3ff));

	 
	for (i = 0; i < 1024; i++)
		hash_id[i] = (i & (qdev->rss_ring_count - 1));

	memcpy((void *)&ricb->ipv6_hash_key[0], init_hash_seed, 40);
	memcpy((void *)&ricb->ipv4_hash_key[0], init_hash_seed, 16);

	status = qlge_write_cfg(qdev, ricb, sizeof(*ricb), CFG_LR, 0);
	if (status) {
		netif_err(qdev, ifup, qdev->ndev, "Failed to load RICB.\n");
		return status;
	}
	return status;
}

static int qlge_clear_routing_entries(struct qlge_adapter *qdev)
{
	int i, status = 0;

	status = qlge_sem_spinlock(qdev, SEM_RT_IDX_MASK);
	if (status)
		return status;
	 
	for (i = 0; i < 16; i++) {
		status = qlge_set_routing_reg(qdev, i, 0, 0);
		if (status) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Failed to init routing register for CAM packets.\n");
			break;
		}
	}
	qlge_sem_unlock(qdev, SEM_RT_IDX_MASK);
	return status;
}

 
static int qlge_route_initialize(struct qlge_adapter *qdev)
{
	int status = 0;

	 
	status = qlge_clear_routing_entries(qdev);
	if (status)
		return status;

	status = qlge_sem_spinlock(qdev, SEM_RT_IDX_MASK);
	if (status)
		return status;

	status = qlge_set_routing_reg(qdev, RT_IDX_IP_CSUM_ERR_SLOT,
				      RT_IDX_IP_CSUM_ERR, 1);
	if (status) {
		netif_err(qdev, ifup, qdev->ndev,
			  "Failed to init routing register for IP CSUM error packets.\n");
		goto exit;
	}
	status = qlge_set_routing_reg(qdev, RT_IDX_TCP_UDP_CSUM_ERR_SLOT,
				      RT_IDX_TU_CSUM_ERR, 1);
	if (status) {
		netif_err(qdev, ifup, qdev->ndev,
			  "Failed to init routing register for TCP/UDP CSUM error packets.\n");
		goto exit;
	}
	status = qlge_set_routing_reg(qdev, RT_IDX_BCAST_SLOT, RT_IDX_BCAST, 1);
	if (status) {
		netif_err(qdev, ifup, qdev->ndev,
			  "Failed to init routing register for broadcast packets.\n");
		goto exit;
	}
	 
	if (qdev->rss_ring_count > 1) {
		status = qlge_set_routing_reg(qdev, RT_IDX_RSS_MATCH_SLOT,
					      RT_IDX_RSS_MATCH, 1);
		if (status) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Failed to init routing register for MATCH RSS packets.\n");
			goto exit;
		}
	}

	status = qlge_set_routing_reg(qdev, RT_IDX_CAM_HIT_SLOT,
				      RT_IDX_CAM_HIT, 1);
	if (status)
		netif_err(qdev, ifup, qdev->ndev,
			  "Failed to init routing register for CAM packets.\n");
exit:
	qlge_sem_unlock(qdev, SEM_RT_IDX_MASK);
	return status;
}

int qlge_cam_route_initialize(struct qlge_adapter *qdev)
{
	int status, set;

	 
	set = qlge_read32(qdev, STS);
	set &= qdev->port_link_up;
	status = qlge_set_mac_addr(qdev, set);
	if (status) {
		netif_err(qdev, ifup, qdev->ndev, "Failed to init mac address.\n");
		return status;
	}

	status = qlge_route_initialize(qdev);
	if (status)
		netif_err(qdev, ifup, qdev->ndev, "Failed to init routing table.\n");

	return status;
}

static int qlge_adapter_initialize(struct qlge_adapter *qdev)
{
	u32 value, mask;
	int i;
	int status = 0;

	 
	value = SYS_EFE | SYS_FAE;
	mask = value << 16;
	qlge_write32(qdev, SYS, mask | value);

	 
	value = NIC_RCV_CFG_DFQ;
	mask = NIC_RCV_CFG_DFQ_MASK;
	if (qdev->ndev->features & NETIF_F_HW_VLAN_CTAG_RX) {
		value |= NIC_RCV_CFG_RV;
		mask |= (NIC_RCV_CFG_RV << 16);
	}
	qlge_write32(qdev, NIC_RCV_CFG, (mask | value));

	 
	qlge_write32(qdev, INTR_MASK, (INTR_MASK_PI << 16) | INTR_MASK_PI);

	 
	value = FSC_FE | FSC_EPC_INBOUND | FSC_EPC_OUTBOUND |
		FSC_EC | FSC_VM_PAGE_4K;
	value |= SPLT_SETTING;

	 
	mask = FSC_VM_PAGESIZE_MASK |
		FSC_DBL_MASK | FSC_DBRST_MASK | (value << 16);
	qlge_write32(qdev, FSC, mask | value);

	qlge_write32(qdev, SPLT_HDR, SPLT_LEN);

	 
	qlge_write32(qdev, RST_FO, RST_FO_RR_MASK | RST_FO_RR_RCV_FUNC_CQ);
	 
	value = qlge_read32(qdev, MGMT_RCV_CFG);
	value &= ~MGMT_RCV_CFG_RM;
	mask = 0xffff0000;

	 
	qlge_write32(qdev, MGMT_RCV_CFG, mask);
	qlge_write32(qdev, MGMT_RCV_CFG, mask | value);

	 
	if (qdev->pdev->subsystem_device == 0x0068 ||
	    qdev->pdev->subsystem_device == 0x0180)
		qdev->wol = WAKE_MAGIC;

	 
	for (i = 0; i < qdev->rx_ring_count; i++) {
		status = qlge_start_rx_ring(qdev, &qdev->rx_ring[i]);
		if (status) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Failed to start rx ring[%d].\n", i);
			return status;
		}
	}

	 
	if (qdev->rss_ring_count > 1) {
		status = qlge_start_rss(qdev);
		if (status) {
			netif_err(qdev, ifup, qdev->ndev, "Failed to start RSS.\n");
			return status;
		}
	}

	 
	for (i = 0; i < qdev->tx_ring_count; i++) {
		status = qlge_start_tx_ring(qdev, &qdev->tx_ring[i]);
		if (status) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Failed to start tx ring[%d].\n", i);
			return status;
		}
	}

	 
	status = qdev->nic_ops->port_initialize(qdev);
	if (status)
		netif_err(qdev, ifup, qdev->ndev, "Failed to start port.\n");

	 
	status = qlge_cam_route_initialize(qdev);
	if (status) {
		netif_err(qdev, ifup, qdev->ndev,
			  "Failed to init CAM/Routing tables.\n");
		return status;
	}

	 
	for (i = 0; i < qdev->rss_ring_count; i++)
		napi_enable(&qdev->rx_ring[i].napi);

	return status;
}

 
static int qlge_adapter_reset(struct qlge_adapter *qdev)
{
	u32 value;
	int status = 0;
	unsigned long end_jiffies;

	 
	status = qlge_clear_routing_entries(qdev);
	if (status) {
		netif_err(qdev, ifup, qdev->ndev, "Failed to clear routing bits.\n");
		return status;
	}

	 
	if (!test_bit(QL_ASIC_RECOVERY, &qdev->flags)) {
		 
		qlge_mb_set_mgmnt_traffic_ctl(qdev, MB_SET_MPI_TFK_STOP);

		 
		qlge_wait_fifo_empty(qdev);
	} else {
		clear_bit(QL_ASIC_RECOVERY, &qdev->flags);
	}

	qlge_write32(qdev, RST_FO, (RST_FO_FR << 16) | RST_FO_FR);

	end_jiffies = jiffies + usecs_to_jiffies(30);
	do {
		value = qlge_read32(qdev, RST_FO);
		if ((value & RST_FO_FR) == 0)
			break;
		cpu_relax();
	} while (time_before(jiffies, end_jiffies));

	if (value & RST_FO_FR) {
		netif_err(qdev, ifdown, qdev->ndev,
			  "ETIMEDOUT!!! errored out of resetting the chip!\n");
		status = -ETIMEDOUT;
	}

	 
	qlge_mb_set_mgmnt_traffic_ctl(qdev, MB_SET_MPI_TFK_RESUME);
	return status;
}

static void qlge_display_dev_info(struct net_device *ndev)
{
	struct qlge_adapter *qdev = netdev_to_qdev(ndev);

	netif_info(qdev, probe, qdev->ndev,
		   "Function #%d, Port %d, NIC Roll %d, NIC Rev = %d, XG Roll = %d, XG Rev = %d.\n",
		   qdev->func,
		   qdev->port,
		   qdev->chip_rev_id & 0x0000000f,
		   qdev->chip_rev_id >> 4 & 0x0000000f,
		   qdev->chip_rev_id >> 8 & 0x0000000f,
		   qdev->chip_rev_id >> 12 & 0x0000000f);
	netif_info(qdev, probe, qdev->ndev,
		   "MAC address %pM\n", ndev->dev_addr);
}

static int qlge_wol(struct qlge_adapter *qdev)
{
	int status = 0;
	u32 wol = MB_WOL_DISABLE;

	 

	if (qdev->wol & (WAKE_ARP | WAKE_MAGICSECURE | WAKE_PHY | WAKE_UCAST |
			 WAKE_MCAST | WAKE_BCAST)) {
		netif_err(qdev, ifdown, qdev->ndev,
			  "Unsupported WOL parameter. qdev->wol = 0x%x.\n",
			  qdev->wol);
		return -EINVAL;
	}

	if (qdev->wol & WAKE_MAGIC) {
		status = qlge_mb_wol_set_magic(qdev, 1);
		if (status) {
			netif_err(qdev, ifdown, qdev->ndev,
				  "Failed to set magic packet on %s.\n",
				  qdev->ndev->name);
			return status;
		}
		netif_info(qdev, drv, qdev->ndev,
			   "Enabled magic packet successfully on %s.\n",
			   qdev->ndev->name);

		wol |= MB_WOL_MAGIC_PKT;
	}

	if (qdev->wol) {
		wol |= MB_WOL_MODE_ON;
		status = qlge_mb_wol_mode(qdev, wol);
		netif_err(qdev, drv, qdev->ndev,
			  "WOL %s (wol code 0x%x) on %s\n",
			  (status == 0) ? "Successfully set" : "Failed",
			  wol, qdev->ndev->name);
	}

	return status;
}

static void qlge_cancel_all_work_sync(struct qlge_adapter *qdev)
{
	 
	if (test_bit(QL_ADAPTER_UP, &qdev->flags))
		cancel_delayed_work_sync(&qdev->asic_reset_work);
	cancel_delayed_work_sync(&qdev->mpi_reset_work);
	cancel_delayed_work_sync(&qdev->mpi_work);
	cancel_delayed_work_sync(&qdev->mpi_idc_work);
	cancel_delayed_work_sync(&qdev->mpi_port_cfg_work);
}

static int qlge_adapter_down(struct qlge_adapter *qdev)
{
	int i, status = 0;

	qlge_link_off(qdev);

	qlge_cancel_all_work_sync(qdev);

	for (i = 0; i < qdev->rss_ring_count; i++)
		napi_disable(&qdev->rx_ring[i].napi);

	clear_bit(QL_ADAPTER_UP, &qdev->flags);

	qlge_disable_interrupts(qdev);

	qlge_tx_ring_clean(qdev);

	 
	for (i = 0; i < qdev->rss_ring_count; i++)
		netif_napi_del(&qdev->rx_ring[i].napi);

	status = qlge_adapter_reset(qdev);
	if (status)
		netif_err(qdev, ifdown, qdev->ndev, "reset(func #%d) FAILED!\n",
			  qdev->func);
	qlge_free_rx_buffers(qdev);

	return status;
}

static int qlge_adapter_up(struct qlge_adapter *qdev)
{
	int err = 0;

	err = qlge_adapter_initialize(qdev);
	if (err) {
		netif_info(qdev, ifup, qdev->ndev, "Unable to initialize adapter.\n");
		goto err_init;
	}
	set_bit(QL_ADAPTER_UP, &qdev->flags);
	qlge_alloc_rx_buffers(qdev);
	 
	if ((qlge_read32(qdev, STS) & qdev->port_init) &&
	    (qlge_read32(qdev, STS) & qdev->port_link_up))
		qlge_link_on(qdev);
	 
	clear_bit(QL_ALLMULTI, &qdev->flags);
	clear_bit(QL_PROMISCUOUS, &qdev->flags);
	qlge_set_multicast_list(qdev->ndev);

	 
	qlge_restore_vlan(qdev);

	qlge_enable_interrupts(qdev);
	qlge_enable_all_completion_interrupts(qdev);
	netif_tx_start_all_queues(qdev->ndev);

	return 0;
err_init:
	qlge_adapter_reset(qdev);
	return err;
}

static void qlge_release_adapter_resources(struct qlge_adapter *qdev)
{
	qlge_free_mem_resources(qdev);
	qlge_free_irq(qdev);
}

static int qlge_get_adapter_resources(struct qlge_adapter *qdev)
{
	if (qlge_alloc_mem_resources(qdev)) {
		netif_err(qdev, ifup, qdev->ndev, "Unable to  allocate memory.\n");
		return -ENOMEM;
	}
	return qlge_request_irq(qdev);
}

static int qlge_close(struct net_device *ndev)
{
	struct qlge_adapter *qdev = netdev_to_qdev(ndev);
	int i;

	 
	if (test_bit(QL_EEH_FATAL, &qdev->flags)) {
		netif_err(qdev, drv, qdev->ndev, "EEH fatal did unload.\n");
		clear_bit(QL_EEH_FATAL, &qdev->flags);
		return 0;
	}

	 
	while (!test_bit(QL_ADAPTER_UP, &qdev->flags))
		msleep(1);

	 
	for (i = 0; i < qdev->rss_ring_count; i++)
		cancel_delayed_work_sync(&qdev->rx_ring[i].refill_work);

	qlge_adapter_down(qdev);
	qlge_release_adapter_resources(qdev);
	return 0;
}

static void qlge_set_lb_size(struct qlge_adapter *qdev)
{
	if (qdev->ndev->mtu <= 1500)
		qdev->lbq_buf_size = LARGE_BUFFER_MIN_SIZE;
	else
		qdev->lbq_buf_size = LARGE_BUFFER_MAX_SIZE;
	qdev->lbq_buf_order = get_order(qdev->lbq_buf_size);
}

static int qlge_configure_rings(struct qlge_adapter *qdev)
{
	int i;
	struct rx_ring *rx_ring;
	struct tx_ring *tx_ring;
	int cpu_cnt = min_t(int, MAX_CPUS, num_online_cpus());

	 
	qdev->intr_count = cpu_cnt;
	qlge_enable_msix(qdev);
	 
	qdev->rss_ring_count = qdev->intr_count;
	qdev->tx_ring_count = cpu_cnt;
	qdev->rx_ring_count = qdev->tx_ring_count + qdev->rss_ring_count;

	for (i = 0; i < qdev->tx_ring_count; i++) {
		tx_ring = &qdev->tx_ring[i];
		memset((void *)tx_ring, 0, sizeof(*tx_ring));
		tx_ring->qdev = qdev;
		tx_ring->wq_id = i;
		tx_ring->wq_len = qdev->tx_ring_size;
		tx_ring->wq_size =
			tx_ring->wq_len * sizeof(struct qlge_ob_mac_iocb_req);

		 
		tx_ring->cq_id = qdev->rss_ring_count + i;
	}

	for (i = 0; i < qdev->rx_ring_count; i++) {
		rx_ring = &qdev->rx_ring[i];
		memset((void *)rx_ring, 0, sizeof(*rx_ring));
		rx_ring->qdev = qdev;
		rx_ring->cq_id = i;
		rx_ring->cpu = i % cpu_cnt;	 
		if (i < qdev->rss_ring_count) {
			 
			rx_ring->cq_len = qdev->rx_ring_size;
			rx_ring->cq_size =
				rx_ring->cq_len * sizeof(struct qlge_net_rsp_iocb);
			rx_ring->lbq.type = QLGE_LB;
			rx_ring->sbq.type = QLGE_SB;
			INIT_DELAYED_WORK(&rx_ring->refill_work,
					  &qlge_slow_refill);
		} else {
			 
			 
			rx_ring->cq_len = qdev->tx_ring_size;
			rx_ring->cq_size =
				rx_ring->cq_len * sizeof(struct qlge_net_rsp_iocb);
		}
	}
	return 0;
}

static int qlge_open(struct net_device *ndev)
{
	struct qlge_adapter *qdev = netdev_to_qdev(ndev);
	int err = 0;

	err = qlge_adapter_reset(qdev);
	if (err)
		return err;

	qlge_set_lb_size(qdev);
	err = qlge_configure_rings(qdev);
	if (err)
		return err;

	err = qlge_get_adapter_resources(qdev);
	if (err)
		goto error_up;

	err = qlge_adapter_up(qdev);
	if (err)
		goto error_up;

	return err;

error_up:
	qlge_release_adapter_resources(qdev);
	return err;
}

static int qlge_change_rx_buffers(struct qlge_adapter *qdev)
{
	int status;

	 
	if (!test_bit(QL_ADAPTER_UP, &qdev->flags)) {
		int i = 4;

		while (--i && !test_bit(QL_ADAPTER_UP, &qdev->flags)) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Waiting for adapter UP...\n");
			ssleep(1);
		}

		if (!i) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Timed out waiting for adapter UP\n");
			return -ETIMEDOUT;
		}
	}

	status = qlge_adapter_down(qdev);
	if (status)
		goto error;

	qlge_set_lb_size(qdev);

	status = qlge_adapter_up(qdev);
	if (status)
		goto error;

	return status;
error:
	netif_alert(qdev, ifup, qdev->ndev,
		    "Driver up/down cycle failed, closing device.\n");
	set_bit(QL_ADAPTER_UP, &qdev->flags);
	dev_close(qdev->ndev);
	return status;
}

static int qlge_change_mtu(struct net_device *ndev, int new_mtu)
{
	struct qlge_adapter *qdev = netdev_to_qdev(ndev);
	int status;

	if (ndev->mtu == 1500 && new_mtu == 9000)
		netif_err(qdev, ifup, qdev->ndev, "Changing to jumbo MTU.\n");
	else if (ndev->mtu == 9000 && new_mtu == 1500)
		netif_err(qdev, ifup, qdev->ndev, "Changing to normal MTU.\n");
	else
		return -EINVAL;

	queue_delayed_work(qdev->workqueue,
			   &qdev->mpi_port_cfg_work, 3 * HZ);

	ndev->mtu = new_mtu;

	if (!netif_running(qdev->ndev))
		return 0;

	status = qlge_change_rx_buffers(qdev);
	if (status) {
		netif_err(qdev, ifup, qdev->ndev,
			  "Changing MTU failed.\n");
	}

	return status;
}

static struct net_device_stats *qlge_get_stats(struct net_device
					       *ndev)
{
	struct qlge_adapter *qdev = netdev_to_qdev(ndev);
	struct rx_ring *rx_ring = &qdev->rx_ring[0];
	struct tx_ring *tx_ring = &qdev->tx_ring[0];
	unsigned long pkts, mcast, dropped, errors, bytes;
	int i;

	 
	pkts = mcast = dropped = errors = bytes = 0;
	for (i = 0; i < qdev->rss_ring_count; i++, rx_ring++) {
		pkts += rx_ring->rx_packets;
		bytes += rx_ring->rx_bytes;
		dropped += rx_ring->rx_dropped;
		errors += rx_ring->rx_errors;
		mcast += rx_ring->rx_multicast;
	}
	ndev->stats.rx_packets = pkts;
	ndev->stats.rx_bytes = bytes;
	ndev->stats.rx_dropped = dropped;
	ndev->stats.rx_errors = errors;
	ndev->stats.multicast = mcast;

	 
	pkts = errors = bytes = 0;
	for (i = 0; i < qdev->tx_ring_count; i++, tx_ring++) {
		pkts += tx_ring->tx_packets;
		bytes += tx_ring->tx_bytes;
		errors += tx_ring->tx_errors;
	}
	ndev->stats.tx_packets = pkts;
	ndev->stats.tx_bytes = bytes;
	ndev->stats.tx_errors = errors;
	return &ndev->stats;
}

static void qlge_set_multicast_list(struct net_device *ndev)
{
	struct qlge_adapter *qdev = netdev_to_qdev(ndev);
	struct netdev_hw_addr *ha;
	int i, status;

	status = qlge_sem_spinlock(qdev, SEM_RT_IDX_MASK);
	if (status)
		return;
	 
	if (ndev->flags & IFF_PROMISC) {
		if (!test_bit(QL_PROMISCUOUS, &qdev->flags)) {
			if (qlge_set_routing_reg
			    (qdev, RT_IDX_PROMISCUOUS_SLOT, RT_IDX_VALID, 1)) {
				netif_err(qdev, hw, qdev->ndev,
					  "Failed to set promiscuous mode.\n");
			} else {
				set_bit(QL_PROMISCUOUS, &qdev->flags);
			}
		}
	} else {
		if (test_bit(QL_PROMISCUOUS, &qdev->flags)) {
			if (qlge_set_routing_reg
			    (qdev, RT_IDX_PROMISCUOUS_SLOT, RT_IDX_VALID, 0)) {
				netif_err(qdev, hw, qdev->ndev,
					  "Failed to clear promiscuous mode.\n");
			} else {
				clear_bit(QL_PROMISCUOUS, &qdev->flags);
			}
		}
	}

	 
	if ((ndev->flags & IFF_ALLMULTI) ||
	    (netdev_mc_count(ndev) > MAX_MULTICAST_ENTRIES)) {
		if (!test_bit(QL_ALLMULTI, &qdev->flags)) {
			if (qlge_set_routing_reg
			    (qdev, RT_IDX_ALLMULTI_SLOT, RT_IDX_MCAST, 1)) {
				netif_err(qdev, hw, qdev->ndev,
					  "Failed to set all-multi mode.\n");
			} else {
				set_bit(QL_ALLMULTI, &qdev->flags);
			}
		}
	} else {
		if (test_bit(QL_ALLMULTI, &qdev->flags)) {
			if (qlge_set_routing_reg
			    (qdev, RT_IDX_ALLMULTI_SLOT, RT_IDX_MCAST, 0)) {
				netif_err(qdev, hw, qdev->ndev,
					  "Failed to clear all-multi mode.\n");
			} else {
				clear_bit(QL_ALLMULTI, &qdev->flags);
			}
		}
	}

	if (!netdev_mc_empty(ndev)) {
		status = qlge_sem_spinlock(qdev, SEM_MAC_ADDR_MASK);
		if (status)
			goto exit;
		i = 0;
		netdev_for_each_mc_addr(ha, ndev) {
			if (qlge_set_mac_addr_reg(qdev, (u8 *)ha->addr,
						  MAC_ADDR_TYPE_MULTI_MAC, i)) {
				netif_err(qdev, hw, qdev->ndev,
					  "Failed to loadmulticast address.\n");
				qlge_sem_unlock(qdev, SEM_MAC_ADDR_MASK);
				goto exit;
			}
			i++;
		}
		qlge_sem_unlock(qdev, SEM_MAC_ADDR_MASK);
		if (qlge_set_routing_reg
		    (qdev, RT_IDX_MCAST_MATCH_SLOT, RT_IDX_MCAST_MATCH, 1)) {
			netif_err(qdev, hw, qdev->ndev,
				  "Failed to set multicast match mode.\n");
		} else {
			set_bit(QL_ALLMULTI, &qdev->flags);
		}
	}
exit:
	qlge_sem_unlock(qdev, SEM_RT_IDX_MASK);
}

static int qlge_set_mac_address(struct net_device *ndev, void *p)
{
	struct qlge_adapter *qdev = netdev_to_qdev(ndev);
	struct sockaddr *addr = p;
	int status;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;
	eth_hw_addr_set(ndev, addr->sa_data);
	 
	memcpy(qdev->current_mac_addr, ndev->dev_addr, ndev->addr_len);

	status = qlge_sem_spinlock(qdev, SEM_MAC_ADDR_MASK);
	if (status)
		return status;
	status = qlge_set_mac_addr_reg(qdev, (const u8 *)ndev->dev_addr,
				       MAC_ADDR_TYPE_CAM_MAC,
				       qdev->func * MAX_CQ);
	if (status)
		netif_err(qdev, hw, qdev->ndev, "Failed to load MAC address.\n");
	qlge_sem_unlock(qdev, SEM_MAC_ADDR_MASK);
	return status;
}

static void qlge_tx_timeout(struct net_device *ndev, unsigned int txqueue)
{
	struct qlge_adapter *qdev = netdev_to_qdev(ndev);

	qlge_queue_asic_error(qdev);
}

static void qlge_asic_reset_work(struct work_struct *work)
{
	struct qlge_adapter *qdev =
		container_of(work, struct qlge_adapter, asic_reset_work.work);
	int status;

	rtnl_lock();
	status = qlge_adapter_down(qdev);
	if (status)
		goto error;

	status = qlge_adapter_up(qdev);
	if (status)
		goto error;

	 
	clear_bit(QL_ALLMULTI, &qdev->flags);
	clear_bit(QL_PROMISCUOUS, &qdev->flags);
	qlge_set_multicast_list(qdev->ndev);

	rtnl_unlock();
	return;
error:
	netif_alert(qdev, ifup, qdev->ndev,
		    "Driver up/down cycle failed, closing device\n");

	set_bit(QL_ADAPTER_UP, &qdev->flags);
	dev_close(qdev->ndev);
	rtnl_unlock();
}

static const struct nic_operations qla8012_nic_ops = {
	.get_flash		= qlge_get_8012_flash_params,
	.port_initialize	= qlge_8012_port_initialize,
};

static const struct nic_operations qla8000_nic_ops = {
	.get_flash		= qlge_get_8000_flash_params,
	.port_initialize	= qlge_8000_port_initialize,
};

 
static int qlge_get_alt_pcie_func(struct qlge_adapter *qdev)
{
	int status = 0;
	u32 temp;
	u32 nic_func1, nic_func2;

	status = qlge_read_mpi_reg(qdev, MPI_TEST_FUNC_PORT_CFG,
				   &temp);
	if (status)
		return status;

	nic_func1 = ((temp >> MPI_TEST_NIC1_FUNC_SHIFT) &
		     MPI_TEST_NIC_FUNC_MASK);
	nic_func2 = ((temp >> MPI_TEST_NIC2_FUNC_SHIFT) &
		     MPI_TEST_NIC_FUNC_MASK);

	if (qdev->func == nic_func1)
		qdev->alt_func = nic_func2;
	else if (qdev->func == nic_func2)
		qdev->alt_func = nic_func1;
	else
		status = -EIO;

	return status;
}

static int qlge_get_board_info(struct qlge_adapter *qdev)
{
	int status;

	qdev->func =
		(qlge_read32(qdev, STS) & STS_FUNC_ID_MASK) >> STS_FUNC_ID_SHIFT;
	if (qdev->func > 3)
		return -EIO;

	status = qlge_get_alt_pcie_func(qdev);
	if (status)
		return status;

	qdev->port = (qdev->func < qdev->alt_func) ? 0 : 1;
	if (qdev->port) {
		qdev->xg_sem_mask = SEM_XGMAC1_MASK;
		qdev->port_link_up = STS_PL1;
		qdev->port_init = STS_PI1;
		qdev->mailbox_in = PROC_ADDR_MPI_RISC | PROC_ADDR_FUNC2_MBI;
		qdev->mailbox_out = PROC_ADDR_MPI_RISC | PROC_ADDR_FUNC2_MBO;
	} else {
		qdev->xg_sem_mask = SEM_XGMAC0_MASK;
		qdev->port_link_up = STS_PL0;
		qdev->port_init = STS_PI0;
		qdev->mailbox_in = PROC_ADDR_MPI_RISC | PROC_ADDR_FUNC0_MBI;
		qdev->mailbox_out = PROC_ADDR_MPI_RISC | PROC_ADDR_FUNC0_MBO;
	}
	qdev->chip_rev_id = qlge_read32(qdev, REV_ID);
	qdev->device_id = qdev->pdev->device;
	if (qdev->device_id == QLGE_DEVICE_ID_8012)
		qdev->nic_ops = &qla8012_nic_ops;
	else if (qdev->device_id == QLGE_DEVICE_ID_8000)
		qdev->nic_ops = &qla8000_nic_ops;
	return status;
}

static void qlge_release_all(struct pci_dev *pdev)
{
	struct qlge_adapter *qdev = pci_get_drvdata(pdev);

	if (qdev->workqueue) {
		destroy_workqueue(qdev->workqueue);
		qdev->workqueue = NULL;
	}

	if (qdev->reg_base)
		iounmap(qdev->reg_base);
	if (qdev->doorbell_area)
		iounmap(qdev->doorbell_area);
	vfree(qdev->mpi_coredump);
	pci_release_regions(pdev);
}

static int qlge_init_device(struct pci_dev *pdev, struct qlge_adapter *qdev,
			    int cards_found)
{
	struct net_device *ndev = qdev->ndev;
	int err = 0;

	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "PCI device enable failed.\n");
		return err;
	}

	qdev->pdev = pdev;
	pci_set_drvdata(pdev, qdev);

	 
	err = pcie_set_readrq(pdev, 4096);
	if (err) {
		dev_err(&pdev->dev, "Set readrq failed.\n");
		goto err_disable_pci;
	}

	err = pci_request_regions(pdev, DRV_NAME);
	if (err) {
		dev_err(&pdev->dev, "PCI region request failed.\n");
		goto err_disable_pci;
	}

	pci_set_master(pdev);
	if (!dma_set_mask(&pdev->dev, DMA_BIT_MASK(64))) {
		set_bit(QL_DMA64, &qdev->flags);
		err = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(64));
	} else {
		err = dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));
		if (!err)
			err = dma_set_coherent_mask(&pdev->dev,
						    DMA_BIT_MASK(32));
	}

	if (err) {
		dev_err(&pdev->dev, "No usable DMA configuration.\n");
		goto err_release_pci;
	}

	 
	pdev->needs_freset = 1;
	pci_save_state(pdev);
	qdev->reg_base =
		ioremap(pci_resource_start(pdev, 1), pci_resource_len(pdev, 1));
	if (!qdev->reg_base) {
		dev_err(&pdev->dev, "Register mapping failed.\n");
		err = -ENOMEM;
		goto err_release_pci;
	}

	qdev->doorbell_area_size = pci_resource_len(pdev, 3);
	qdev->doorbell_area =
		ioremap(pci_resource_start(pdev, 3), pci_resource_len(pdev, 3));
	if (!qdev->doorbell_area) {
		dev_err(&pdev->dev, "Doorbell register mapping failed.\n");
		err = -ENOMEM;
		goto err_iounmap_base;
	}

	err = qlge_get_board_info(qdev);
	if (err) {
		dev_err(&pdev->dev, "Register access failed.\n");
		err = -EIO;
		goto err_iounmap_doorbell;
	}
	qdev->msg_enable = netif_msg_init(debug, default_msg);
	spin_lock_init(&qdev->stats_lock);

	if (qlge_mpi_coredump) {
		qdev->mpi_coredump =
			vmalloc(sizeof(struct qlge_mpi_coredump));
		if (!qdev->mpi_coredump) {
			err = -ENOMEM;
			goto err_iounmap_doorbell;
		}
		if (qlge_force_coredump)
			set_bit(QL_FRC_COREDUMP, &qdev->flags);
	}
	 
	err = qdev->nic_ops->get_flash(qdev);
	if (err) {
		dev_err(&pdev->dev, "Invalid FLASH.\n");
		goto err_free_mpi_coredump;
	}

	 
	memcpy(qdev->current_mac_addr, ndev->dev_addr, ndev->addr_len);

	 
	qdev->tx_ring_size = NUM_TX_RING_ENTRIES;
	qdev->rx_ring_size = NUM_RX_RING_ENTRIES;

	 
	qdev->rx_coalesce_usecs = DFLT_COALESCE_WAIT;
	qdev->tx_coalesce_usecs = DFLT_COALESCE_WAIT;
	qdev->rx_max_coalesced_frames = DFLT_INTER_FRAME_WAIT;
	qdev->tx_max_coalesced_frames = DFLT_INTER_FRAME_WAIT;

	 
	qdev->workqueue = alloc_ordered_workqueue("%s", WQ_MEM_RECLAIM,
						  ndev->name);
	if (!qdev->workqueue) {
		err = -ENOMEM;
		goto err_free_mpi_coredump;
	}

	INIT_DELAYED_WORK(&qdev->asic_reset_work, qlge_asic_reset_work);
	INIT_DELAYED_WORK(&qdev->mpi_reset_work, qlge_mpi_reset_work);
	INIT_DELAYED_WORK(&qdev->mpi_work, qlge_mpi_work);
	INIT_DELAYED_WORK(&qdev->mpi_port_cfg_work, qlge_mpi_port_cfg_work);
	INIT_DELAYED_WORK(&qdev->mpi_idc_work, qlge_mpi_idc_work);
	init_completion(&qdev->ide_completion);
	mutex_init(&qdev->mpi_mutex);

	if (!cards_found) {
		dev_info(&pdev->dev, "%s\n", DRV_STRING);
		dev_info(&pdev->dev, "Driver name: %s, Version: %s.\n",
			 DRV_NAME, DRV_VERSION);
	}
	return 0;

err_free_mpi_coredump:
	vfree(qdev->mpi_coredump);
err_iounmap_doorbell:
	iounmap(qdev->doorbell_area);
err_iounmap_base:
	iounmap(qdev->reg_base);
err_release_pci:
	pci_release_regions(pdev);
err_disable_pci:
	pci_disable_device(pdev);

	return err;
}

static const struct net_device_ops qlge_netdev_ops = {
	.ndo_open		= qlge_open,
	.ndo_stop		= qlge_close,
	.ndo_start_xmit		= qlge_send,
	.ndo_change_mtu		= qlge_change_mtu,
	.ndo_get_stats		= qlge_get_stats,
	.ndo_set_rx_mode	= qlge_set_multicast_list,
	.ndo_set_mac_address	= qlge_set_mac_address,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_tx_timeout		= qlge_tx_timeout,
	.ndo_set_features	= qlge_set_features,
	.ndo_vlan_rx_add_vid	= qlge_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid	= qlge_vlan_rx_kill_vid,
};

static void qlge_timer(struct timer_list *t)
{
	struct qlge_adapter *qdev = from_timer(qdev, t, timer);
	u32 var = 0;

	var = qlge_read32(qdev, STS);
	if (pci_channel_offline(qdev->pdev)) {
		netif_err(qdev, ifup, qdev->ndev, "EEH STS = 0x%.08x.\n", var);
		return;
	}

	mod_timer(&qdev->timer, jiffies + (5 * HZ));
}

static const struct devlink_ops qlge_devlink_ops;

static int qlge_probe(struct pci_dev *pdev,
		      const struct pci_device_id *pci_entry)
{
	struct qlge_netdev_priv *ndev_priv;
	struct qlge_adapter *qdev = NULL;
	struct net_device *ndev = NULL;
	struct devlink *devlink;
	static int cards_found;
	int err;

	devlink = devlink_alloc(&qlge_devlink_ops, sizeof(struct qlge_adapter),
				&pdev->dev);
	if (!devlink)
		return -ENOMEM;

	qdev = devlink_priv(devlink);

	ndev = alloc_etherdev_mq(sizeof(struct qlge_netdev_priv),
				 min(MAX_CPUS,
				     netif_get_num_default_rss_queues()));
	if (!ndev) {
		err = -ENOMEM;
		goto devlink_free;
	}

	ndev_priv = netdev_priv(ndev);
	ndev_priv->qdev = qdev;
	ndev_priv->ndev = ndev;
	qdev->ndev = ndev;
	err = qlge_init_device(pdev, qdev, cards_found);
	if (err < 0)
		goto netdev_free;

	SET_NETDEV_DEV(ndev, &pdev->dev);
	ndev->hw_features = NETIF_F_SG |
		NETIF_F_IP_CSUM |
		NETIF_F_TSO |
		NETIF_F_TSO_ECN |
		NETIF_F_HW_VLAN_CTAG_TX |
		NETIF_F_HW_VLAN_CTAG_RX |
		NETIF_F_HW_VLAN_CTAG_FILTER |
		NETIF_F_RXCSUM;
	ndev->features = ndev->hw_features;
	ndev->vlan_features = ndev->hw_features;
	 
	ndev->vlan_features &= ~(NETIF_F_HW_VLAN_CTAG_FILTER |
				 NETIF_F_HW_VLAN_CTAG_TX |
				 NETIF_F_HW_VLAN_CTAG_RX);

	if (test_bit(QL_DMA64, &qdev->flags))
		ndev->features |= NETIF_F_HIGHDMA;

	 
	ndev->tx_queue_len = qdev->tx_ring_size;
	ndev->irq = pdev->irq;

	ndev->netdev_ops = &qlge_netdev_ops;
	ndev->ethtool_ops = &qlge_ethtool_ops;
	ndev->watchdog_timeo = 10 * HZ;

	 
	ndev->min_mtu = ETH_DATA_LEN;
	ndev->max_mtu = 9000;

	err = register_netdev(ndev);
	if (err) {
		dev_err(&pdev->dev, "net device registration failed.\n");
		goto cleanup_pdev;
	}

	err = qlge_health_create_reporters(qdev);
	if (err)
		goto unregister_netdev;

	 
	timer_setup(&qdev->timer, qlge_timer, TIMER_DEFERRABLE);
	mod_timer(&qdev->timer, jiffies + (5 * HZ));
	qlge_link_off(qdev);
	qlge_display_dev_info(ndev);
	atomic_set(&qdev->lb_count, 0);
	cards_found++;
	devlink_register(devlink);
	return 0;

unregister_netdev:
	unregister_netdev(ndev);
cleanup_pdev:
	qlge_release_all(pdev);
	pci_disable_device(pdev);
netdev_free:
	free_netdev(ndev);
devlink_free:
	devlink_free(devlink);

	return err;
}

netdev_tx_t qlge_lb_send(struct sk_buff *skb, struct net_device *ndev)
{
	return qlge_send(skb, ndev);
}

int qlge_clean_lb_rx_ring(struct rx_ring *rx_ring, int budget)
{
	return qlge_clean_inbound_rx_ring(rx_ring, budget);
}

static void qlge_remove(struct pci_dev *pdev)
{
	struct qlge_adapter *qdev = pci_get_drvdata(pdev);
	struct net_device *ndev = qdev->ndev;
	struct devlink *devlink = priv_to_devlink(qdev);

	devlink_unregister(devlink);
	del_timer_sync(&qdev->timer);
	qlge_cancel_all_work_sync(qdev);
	unregister_netdev(ndev);
	qlge_release_all(pdev);
	pci_disable_device(pdev);
	devlink_health_reporter_destroy(qdev->reporter);
	devlink_free(devlink);
	free_netdev(ndev);
}

 
static void qlge_eeh_close(struct net_device *ndev)
{
	struct qlge_adapter *qdev = netdev_to_qdev(ndev);
	int i;

	if (netif_carrier_ok(ndev)) {
		netif_carrier_off(ndev);
		netif_stop_queue(ndev);
	}

	 
	qlge_cancel_all_work_sync(qdev);

	for (i = 0; i < qdev->rss_ring_count; i++)
		netif_napi_del(&qdev->rx_ring[i].napi);

	clear_bit(QL_ADAPTER_UP, &qdev->flags);
	qlge_tx_ring_clean(qdev);
	qlge_free_rx_buffers(qdev);
	qlge_release_adapter_resources(qdev);
}

 
static pci_ers_result_t qlge_io_error_detected(struct pci_dev *pdev,
					       pci_channel_state_t state)
{
	struct qlge_adapter *qdev = pci_get_drvdata(pdev);
	struct net_device *ndev = qdev->ndev;

	switch (state) {
	case pci_channel_io_normal:
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_frozen:
		netif_device_detach(ndev);
		del_timer_sync(&qdev->timer);
		if (netif_running(ndev))
			qlge_eeh_close(ndev);
		pci_disable_device(pdev);
		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		dev_err(&pdev->dev,
			"%s: pci_channel_io_perm_failure.\n", __func__);
		del_timer_sync(&qdev->timer);
		qlge_eeh_close(ndev);
		set_bit(QL_EEH_FATAL, &qdev->flags);
		return PCI_ERS_RESULT_DISCONNECT;
	}

	 
	return PCI_ERS_RESULT_NEED_RESET;
}

 
static pci_ers_result_t qlge_io_slot_reset(struct pci_dev *pdev)
{
	struct qlge_adapter *qdev = pci_get_drvdata(pdev);

	pdev->error_state = pci_channel_io_normal;

	pci_restore_state(pdev);
	if (pci_enable_device(pdev)) {
		netif_err(qdev, ifup, qdev->ndev,
			  "Cannot re-enable PCI device after reset.\n");
		return PCI_ERS_RESULT_DISCONNECT;
	}
	pci_set_master(pdev);

	if (qlge_adapter_reset(qdev)) {
		netif_err(qdev, drv, qdev->ndev, "reset FAILED!\n");
		set_bit(QL_EEH_FATAL, &qdev->flags);
		return PCI_ERS_RESULT_DISCONNECT;
	}

	return PCI_ERS_RESULT_RECOVERED;
}

static void qlge_io_resume(struct pci_dev *pdev)
{
	struct qlge_adapter *qdev = pci_get_drvdata(pdev);
	struct net_device *ndev = qdev->ndev;
	int err = 0;

	if (netif_running(ndev)) {
		err = qlge_open(ndev);
		if (err) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Device initialization failed after reset.\n");
			return;
		}
	} else {
		netif_err(qdev, ifup, qdev->ndev,
			  "Device was not running prior to EEH.\n");
	}
	mod_timer(&qdev->timer, jiffies + (5 * HZ));
	netif_device_attach(ndev);
}

static const struct pci_error_handlers qlge_err_handler = {
	.error_detected = qlge_io_error_detected,
	.slot_reset = qlge_io_slot_reset,
	.resume = qlge_io_resume,
};

static int __maybe_unused qlge_suspend(struct device *dev_d)
{
	struct pci_dev *pdev = to_pci_dev(dev_d);
	struct qlge_adapter *qdev;
	struct net_device *ndev;
	int err;

	qdev = pci_get_drvdata(pdev);
	ndev = qdev->ndev;
	netif_device_detach(ndev);
	del_timer_sync(&qdev->timer);

	if (netif_running(ndev)) {
		err = qlge_adapter_down(qdev);
		if (!err)
			return err;
	}

	qlge_wol(qdev);

	return 0;
}

static int __maybe_unused qlge_resume(struct device *dev_d)
{
	struct pci_dev *pdev = to_pci_dev(dev_d);
	struct qlge_adapter *qdev;
	struct net_device *ndev;
	int err;

	qdev = pci_get_drvdata(pdev);
	ndev = qdev->ndev;

	pci_set_master(pdev);

	device_wakeup_disable(dev_d);

	if (netif_running(ndev)) {
		err = qlge_adapter_up(qdev);
		if (err)
			return err;
	}

	mod_timer(&qdev->timer, jiffies + (5 * HZ));
	netif_device_attach(ndev);

	return 0;
}

static void qlge_shutdown(struct pci_dev *pdev)
{
	qlge_suspend(&pdev->dev);
}

static SIMPLE_DEV_PM_OPS(qlge_pm_ops, qlge_suspend, qlge_resume);

static struct pci_driver qlge_driver = {
	.name = DRV_NAME,
	.id_table = qlge_pci_tbl,
	.probe = qlge_probe,
	.remove = qlge_remove,
	.driver.pm = &qlge_pm_ops,
	.shutdown = qlge_shutdown,
	.err_handler = &qlge_err_handler
};

module_pci_driver(qlge_driver);
