
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "fman_memac.h"
#include "fman.h"
#include "mac.h"

#include <linux/slab.h>
#include <linux/io.h>
#include <linux/pcs-lynx.h>
#include <linux/phy.h>
#include <linux/phy_fixed.h>
#include <linux/phy/phy.h>
#include <linux/of_mdio.h>

 
#define MEMAC_NUM_OF_PADDRS 7

 
#define CMD_CFG_REG_LOWP_RXETY	0x01000000  
#define CMD_CFG_TX_LOWP_ENA	0x00800000  
#define CMD_CFG_PFC_MODE	0x00080000  
#define CMD_CFG_NO_LEN_CHK	0x00020000  
#define CMD_CFG_SW_RESET	0x00001000  
#define CMD_CFG_TX_PAD_EN	0x00000800  
#define CMD_CFG_PAUSE_IGNORE	0x00000100  
#define CMD_CFG_CRC_FWD		0x00000040  
#define CMD_CFG_PAD_EN		0x00000020  
#define CMD_CFG_PROMIS_EN	0x00000010  
#define CMD_CFG_RX_EN		0x00000002  
#define CMD_CFG_TX_EN		0x00000001  

 
#define TX_FIFO_SECTIONS_TX_EMPTY_MASK			0xFFFF0000
#define TX_FIFO_SECTIONS_TX_AVAIL_MASK			0x0000FFFF
#define TX_FIFO_SECTIONS_TX_EMPTY_DEFAULT_10G		0x00400000
#define TX_FIFO_SECTIONS_TX_EMPTY_DEFAULT_1G		0x00100000
#define TX_FIFO_SECTIONS_TX_AVAIL_10G			0x00000019
#define TX_FIFO_SECTIONS_TX_AVAIL_1G			0x00000020
#define TX_FIFO_SECTIONS_TX_AVAIL_SLOW_10G		0x00000060

#define GET_TX_EMPTY_DEFAULT_VALUE(_val)				\
do {									\
	_val &= ~TX_FIFO_SECTIONS_TX_EMPTY_MASK;			\
	((_val == TX_FIFO_SECTIONS_TX_AVAIL_10G) ?			\
			(_val |= TX_FIFO_SECTIONS_TX_EMPTY_DEFAULT_10G) :\
			(_val |= TX_FIFO_SECTIONS_TX_EMPTY_DEFAULT_1G));\
} while (0)

 

#define IF_MODE_MASK		0x00000003  
#define IF_MODE_10G		0x00000000  
#define IF_MODE_MII		0x00000001  
#define IF_MODE_GMII		0x00000002  
#define IF_MODE_RGMII		0x00000004
#define IF_MODE_RGMII_AUTO	0x00008000
#define IF_MODE_RGMII_1000	0x00004000  
#define IF_MODE_RGMII_100	0x00000000  
#define IF_MODE_RGMII_10	0x00002000  
#define IF_MODE_RGMII_SP_MASK	0x00006000  
#define IF_MODE_RGMII_FD	0x00001000  
#define IF_MODE_HD		0x00000040  

 
#define HASH_CTRL_MCAST_EN	0x00000100
 
#define HASH_CTRL_ADDR_MASK	0x0000003F
 
#define GROUP_ADDRESS		0x0000010000000000LL
#define HASH_TABLE_SIZE		64	 

 
#define MEMAC_IMASK_MGI		0x40000000  
#define MEMAC_IMASK_TSECC_ER	0x20000000  
#define MEMAC_IMASK_TECC_ER	0x02000000  
#define MEMAC_IMASK_RECC_ER	0x01000000  

#define MEMAC_ALL_ERRS_IMASK					\
		((u32)(MEMAC_IMASK_TSECC_ER	|	\
		       MEMAC_IMASK_TECC_ER		|	\
		       MEMAC_IMASK_RECC_ER		|	\
		       MEMAC_IMASK_MGI))

#define MEMAC_IEVNT_PCS			0x80000000  
#define MEMAC_IEVNT_AN			0x40000000  
#define MEMAC_IEVNT_LT			0x20000000  
#define MEMAC_IEVNT_MGI			0x00004000  
#define MEMAC_IEVNT_TS_ECC_ER		0x00002000  
#define MEMAC_IEVNT_RX_FIFO_OVFL	0x00001000  
#define MEMAC_IEVNT_TX_FIFO_UNFL	0x00000800  
#define MEMAC_IEVNT_TX_FIFO_OVFL	0x00000400  
#define MEMAC_IEVNT_TX_ECC_ER		0x00000200  
#define MEMAC_IEVNT_RX_ECC_ER		0x00000100  
#define MEMAC_IEVNT_LI_FAULT		0x00000080  
#define MEMAC_IEVNT_RX_EMPTY		0x00000040  
#define MEMAC_IEVNT_TX_EMPTY		0x00000020  
#define MEMAC_IEVNT_RX_LOWP		0x00000010  
#define MEMAC_IEVNT_PHY_LOS		0x00000004  
#define MEMAC_IEVNT_REM_FAULT		0x00000002  
#define MEMAC_IEVNT_LOC_FAULT		0x00000001  

#define DEFAULT_PAUSE_QUANTA	0xf000
#define DEFAULT_FRAME_LENGTH	0x600
#define DEFAULT_TX_IPG_LENGTH	12

#define CLXY_PAUSE_QUANTA_CLX_PQNT	0x0000FFFF
#define CLXY_PAUSE_QUANTA_CLY_PQNT	0xFFFF0000
#define CLXY_PAUSE_THRESH_CLX_QTH	0x0000FFFF
#define CLXY_PAUSE_THRESH_CLY_QTH	0xFFFF0000

struct mac_addr {
	 
	u32 mac_addr_l;
	 
	u32 mac_addr_u;
};

 
struct memac_regs {
	u32 res0000[2];			 
	u32 command_config;		 
	struct mac_addr mac_addr0;	 
	u32 maxfrm;			 
	u32 res0018[1];
	u32 rx_fifo_sections;		 
	u32 tx_fifo_sections;		 
	u32 res0024[2];
	u32 hashtable_ctrl;		 
	u32 res0030[4];
	u32 ievent;			 
	u32 tx_ipg_length;		 
	u32 res0048;
	u32 imask;			 
	u32 res0050;
	u32 pause_quanta[4];		 
	u32 pause_thresh[4];		 
	u32 rx_pause_status;		 
	u32 res0078[2];
	struct mac_addr mac_addr[MEMAC_NUM_OF_PADDRS]; 
	u32 lpwake_timer;		 
	u32 sleep_timer;		 
	u32 res00c0[8];
	u32 statn_config;		 
	u32 res00e4[7];
	 
	u32 reoct_l;
	u32 reoct_u;
	u32 roct_l;
	u32 roct_u;
	u32 raln_l;
	u32 raln_u;
	u32 rxpf_l;
	u32 rxpf_u;
	u32 rfrm_l;
	u32 rfrm_u;
	u32 rfcs_l;
	u32 rfcs_u;
	u32 rvlan_l;
	u32 rvlan_u;
	u32 rerr_l;
	u32 rerr_u;
	u32 ruca_l;
	u32 ruca_u;
	u32 rmca_l;
	u32 rmca_u;
	u32 rbca_l;
	u32 rbca_u;
	u32 rdrp_l;
	u32 rdrp_u;
	u32 rpkt_l;
	u32 rpkt_u;
	u32 rund_l;
	u32 rund_u;
	u32 r64_l;
	u32 r64_u;
	u32 r127_l;
	u32 r127_u;
	u32 r255_l;
	u32 r255_u;
	u32 r511_l;
	u32 r511_u;
	u32 r1023_l;
	u32 r1023_u;
	u32 r1518_l;
	u32 r1518_u;
	u32 r1519x_l;
	u32 r1519x_u;
	u32 rovr_l;
	u32 rovr_u;
	u32 rjbr_l;
	u32 rjbr_u;
	u32 rfrg_l;
	u32 rfrg_u;
	u32 rcnp_l;
	u32 rcnp_u;
	u32 rdrntp_l;
	u32 rdrntp_u;
	u32 res01d0[12];
	 
	u32 teoct_l;
	u32 teoct_u;
	u32 toct_l;
	u32 toct_u;
	u32 res0210[2];
	u32 txpf_l;
	u32 txpf_u;
	u32 tfrm_l;
	u32 tfrm_u;
	u32 tfcs_l;
	u32 tfcs_u;
	u32 tvlan_l;
	u32 tvlan_u;
	u32 terr_l;
	u32 terr_u;
	u32 tuca_l;
	u32 tuca_u;
	u32 tmca_l;
	u32 tmca_u;
	u32 tbca_l;
	u32 tbca_u;
	u32 res0258[2];
	u32 tpkt_l;
	u32 tpkt_u;
	u32 tund_l;
	u32 tund_u;
	u32 t64_l;
	u32 t64_u;
	u32 t127_l;
	u32 t127_u;
	u32 t255_l;
	u32 t255_u;
	u32 t511_l;
	u32 t511_u;
	u32 t1023_l;
	u32 t1023_u;
	u32 t1518_l;
	u32 t1518_u;
	u32 t1519x_l;
	u32 t1519x_u;
	u32 res02a8[6];
	u32 tcnp_l;
	u32 tcnp_u;
	u32 res02c8[14];
	 
	u32 if_mode;		 
	u32 if_status;		 
	u32 res0308[14];
	 
	u32 hg_config;		 
	u32 res0344[3];
	u32 hg_pause_quanta;	 
	u32 res0354[3];
	u32 hg_pause_thresh;	 
	u32 res0364[3];
	u32 hgrx_pause_status;	 
	u32 hg_fifos_status;	 
	u32 rhm;		 
	u32 thm;		 
};

struct memac_cfg {
	bool reset_on_init;
	bool pause_ignore;
	bool promiscuous_mode_enable;
	struct fixed_phy_status *fixed_link;
	u16 max_frame_length;
	u16 pause_quanta;
	u32 tx_ipg_length;
};

struct fman_mac {
	 
	struct memac_regs __iomem *regs;
	 
	u64 addr;
	struct mac_device *dev_id;  
	fman_mac_exception_cb *exception_cb;
	fman_mac_exception_cb *event_cb;
	 
	struct eth_hash_t *multicast_addr_hash;
	 
	struct eth_hash_t *unicast_addr_hash;
	u8 mac_id;
	u32 exceptions;
	struct memac_cfg *memac_drv_param;
	void *fm;
	struct fman_rev_info fm_rev_info;
	struct phy *serdes;
	struct phylink_pcs *sgmii_pcs;
	struct phylink_pcs *qsgmii_pcs;
	struct phylink_pcs *xfi_pcs;
	bool allmulti_enabled;
	bool rgmii_no_half_duplex;
};

static void add_addr_in_paddr(struct memac_regs __iomem *regs, const u8 *adr,
			      u8 paddr_num)
{
	u32 tmp0, tmp1;

	tmp0 = (u32)(adr[0] | adr[1] << 8 | adr[2] << 16 | adr[3] << 24);
	tmp1 = (u32)(adr[4] | adr[5] << 8);

	if (paddr_num == 0) {
		iowrite32be(tmp0, &regs->mac_addr0.mac_addr_l);
		iowrite32be(tmp1, &regs->mac_addr0.mac_addr_u);
	} else {
		iowrite32be(tmp0, &regs->mac_addr[paddr_num - 1].mac_addr_l);
		iowrite32be(tmp1, &regs->mac_addr[paddr_num - 1].mac_addr_u);
	}
}

static int reset(struct memac_regs __iomem *regs)
{
	u32 tmp;
	int count;

	tmp = ioread32be(&regs->command_config);

	tmp |= CMD_CFG_SW_RESET;

	iowrite32be(tmp, &regs->command_config);

	count = 100;
	do {
		udelay(1);
	} while ((ioread32be(&regs->command_config) & CMD_CFG_SW_RESET) &&
		 --count);

	if (count == 0)
		return -EBUSY;

	return 0;
}

static void set_exception(struct memac_regs __iomem *regs, u32 val,
			  bool enable)
{
	u32 tmp;

	tmp = ioread32be(&regs->imask);
	if (enable)
		tmp |= val;
	else
		tmp &= ~val;

	iowrite32be(tmp, &regs->imask);
}

static int init(struct memac_regs __iomem *regs, struct memac_cfg *cfg,
		u32 exceptions)
{
	u32 tmp;

	 
	tmp = 0;
	if (cfg->promiscuous_mode_enable)
		tmp |= CMD_CFG_PROMIS_EN;
	if (cfg->pause_ignore)
		tmp |= CMD_CFG_PAUSE_IGNORE;

	 
	tmp |= CMD_CFG_NO_LEN_CHK;
	 
	tmp |= CMD_CFG_TX_PAD_EN;

	tmp |= CMD_CFG_CRC_FWD;

	iowrite32be(tmp, &regs->command_config);

	 
	iowrite32be((u32)cfg->max_frame_length, &regs->maxfrm);

	 
	iowrite32be((u32)cfg->pause_quanta, &regs->pause_quanta[0]);
	iowrite32be((u32)0, &regs->pause_thresh[0]);

	 
	iowrite32be(0xffffffff, &regs->ievent);
	set_exception(regs, exceptions, true);

	return 0;
}

static void set_dflts(struct memac_cfg *cfg)
{
	cfg->reset_on_init = false;
	cfg->promiscuous_mode_enable = false;
	cfg->pause_ignore = false;
	cfg->tx_ipg_length = DEFAULT_TX_IPG_LENGTH;
	cfg->max_frame_length = DEFAULT_FRAME_LENGTH;
	cfg->pause_quanta = DEFAULT_PAUSE_QUANTA;
}

static u32 get_mac_addr_hash_code(u64 eth_addr)
{
	u64 mask1, mask2;
	u32 xor_val = 0;
	u8 i, j;

	for (i = 0; i < 6; i++) {
		mask1 = eth_addr & (u64)0x01;
		eth_addr >>= 1;

		for (j = 0; j < 7; j++) {
			mask2 = eth_addr & (u64)0x01;
			mask1 ^= mask2;
			eth_addr >>= 1;
		}

		xor_val |= (mask1 << (5 - i));
	}

	return xor_val;
}

static int check_init_parameters(struct fman_mac *memac)
{
	if (!memac->exception_cb) {
		pr_err("Uninitialized exception handler\n");
		return -EINVAL;
	}
	if (!memac->event_cb) {
		pr_warn("Uninitialize event handler\n");
		return -EINVAL;
	}

	return 0;
}

static int get_exception_flag(enum fman_mac_exceptions exception)
{
	u32 bit_mask;

	switch (exception) {
	case FM_MAC_EX_10G_TX_ECC_ER:
		bit_mask = MEMAC_IMASK_TECC_ER;
		break;
	case FM_MAC_EX_10G_RX_ECC_ER:
		bit_mask = MEMAC_IMASK_RECC_ER;
		break;
	case FM_MAC_EX_TS_FIFO_ECC_ERR:
		bit_mask = MEMAC_IMASK_TSECC_ER;
		break;
	case FM_MAC_EX_MAGIC_PACKET_INDICATION:
		bit_mask = MEMAC_IMASK_MGI;
		break;
	default:
		bit_mask = 0;
		break;
	}

	return bit_mask;
}

static void memac_err_exception(void *handle)
{
	struct fman_mac *memac = (struct fman_mac *)handle;
	struct memac_regs __iomem *regs = memac->regs;
	u32 event, imask;

	event = ioread32be(&regs->ievent);
	imask = ioread32be(&regs->imask);

	 
	event &= ((imask & MEMAC_ALL_ERRS_IMASK) >> 16);

	iowrite32be(event, &regs->ievent);

	if (event & MEMAC_IEVNT_TS_ECC_ER)
		memac->exception_cb(memac->dev_id, FM_MAC_EX_TS_FIFO_ECC_ERR);
	if (event & MEMAC_IEVNT_TX_ECC_ER)
		memac->exception_cb(memac->dev_id, FM_MAC_EX_10G_TX_ECC_ER);
	if (event & MEMAC_IEVNT_RX_ECC_ER)
		memac->exception_cb(memac->dev_id, FM_MAC_EX_10G_RX_ECC_ER);
}

static void memac_exception(void *handle)
{
	struct fman_mac *memac = (struct fman_mac *)handle;
	struct memac_regs __iomem *regs = memac->regs;
	u32 event, imask;

	event = ioread32be(&regs->ievent);
	imask = ioread32be(&regs->imask);

	 
	event &= ((imask & MEMAC_ALL_ERRS_IMASK) >> 16);

	iowrite32be(event, &regs->ievent);

	if (event & MEMAC_IEVNT_MGI)
		memac->exception_cb(memac->dev_id,
				    FM_MAC_EX_MAGIC_PACKET_INDICATION);
}

static void free_init_resources(struct fman_mac *memac)
{
	fman_unregister_intr(memac->fm, FMAN_MOD_MAC, memac->mac_id,
			     FMAN_INTR_TYPE_ERR);

	fman_unregister_intr(memac->fm, FMAN_MOD_MAC, memac->mac_id,
			     FMAN_INTR_TYPE_NORMAL);

	 
	free_hash_table(memac->multicast_addr_hash);
	memac->multicast_addr_hash = NULL;

	 
	free_hash_table(memac->unicast_addr_hash);
	memac->unicast_addr_hash = NULL;
}

static int memac_enable(struct fman_mac *memac)
{
	int ret;

	ret = phy_init(memac->serdes);
	if (ret) {
		dev_err(memac->dev_id->dev,
			"could not initialize serdes: %pe\n", ERR_PTR(ret));
		return ret;
	}

	ret = phy_power_on(memac->serdes);
	if (ret) {
		dev_err(memac->dev_id->dev,
			"could not power on serdes: %pe\n", ERR_PTR(ret));
		phy_exit(memac->serdes);
	}

	return ret;
}

static void memac_disable(struct fman_mac *memac)
{
	phy_power_off(memac->serdes);
	phy_exit(memac->serdes);
}

static int memac_set_promiscuous(struct fman_mac *memac, bool new_val)
{
	struct memac_regs __iomem *regs = memac->regs;
	u32 tmp;

	tmp = ioread32be(&regs->command_config);
	if (new_val)
		tmp |= CMD_CFG_PROMIS_EN;
	else
		tmp &= ~CMD_CFG_PROMIS_EN;

	iowrite32be(tmp, &regs->command_config);

	return 0;
}

static int memac_set_tx_pause_frames(struct fman_mac *memac, u8 priority,
				     u16 pause_time, u16 thresh_time)
{
	struct memac_regs __iomem *regs = memac->regs;
	u32 tmp;

	tmp = ioread32be(&regs->tx_fifo_sections);

	GET_TX_EMPTY_DEFAULT_VALUE(tmp);
	iowrite32be(tmp, &regs->tx_fifo_sections);

	tmp = ioread32be(&regs->command_config);
	tmp &= ~CMD_CFG_PFC_MODE;

	iowrite32be(tmp, &regs->command_config);

	tmp = ioread32be(&regs->pause_quanta[priority / 2]);
	if (priority % 2)
		tmp &= CLXY_PAUSE_QUANTA_CLX_PQNT;
	else
		tmp &= CLXY_PAUSE_QUANTA_CLY_PQNT;
	tmp |= ((u32)pause_time << (16 * (priority % 2)));
	iowrite32be(tmp, &regs->pause_quanta[priority / 2]);

	tmp = ioread32be(&regs->pause_thresh[priority / 2]);
	if (priority % 2)
		tmp &= CLXY_PAUSE_THRESH_CLX_QTH;
	else
		tmp &= CLXY_PAUSE_THRESH_CLY_QTH;
	tmp |= ((u32)thresh_time << (16 * (priority % 2)));
	iowrite32be(tmp, &regs->pause_thresh[priority / 2]);

	return 0;
}

static int memac_accept_rx_pause_frames(struct fman_mac *memac, bool en)
{
	struct memac_regs __iomem *regs = memac->regs;
	u32 tmp;

	tmp = ioread32be(&regs->command_config);
	if (en)
		tmp &= ~CMD_CFG_PAUSE_IGNORE;
	else
		tmp |= CMD_CFG_PAUSE_IGNORE;

	iowrite32be(tmp, &regs->command_config);

	return 0;
}

static void memac_validate(struct phylink_config *config,
			   unsigned long *supported,
			   struct phylink_link_state *state)
{
	struct fman_mac *memac = fman_config_to_mac(config)->fman_mac;
	unsigned long caps = config->mac_capabilities;

	if (phy_interface_mode_is_rgmii(state->interface) &&
	    memac->rgmii_no_half_duplex)
		caps &= ~(MAC_10HD | MAC_100HD);

	phylink_validate_mask_caps(supported, state, caps);
}

 
static u32 memac_if_mode(phy_interface_t interface)
{
	switch (interface) {
	case PHY_INTERFACE_MODE_MII:
		return IF_MODE_MII;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_TXID:
		return IF_MODE_GMII | IF_MODE_RGMII;
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_1000BASEX:
	case PHY_INTERFACE_MODE_QSGMII:
		return IF_MODE_GMII;
	case PHY_INTERFACE_MODE_10GBASER:
		return IF_MODE_10G;
	default:
		WARN_ON_ONCE(1);
		return 0;
	}
}

static struct phylink_pcs *memac_select_pcs(struct phylink_config *config,
					    phy_interface_t iface)
{
	struct fman_mac *memac = fman_config_to_mac(config)->fman_mac;

	switch (iface) {
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_1000BASEX:
		return memac->sgmii_pcs;
	case PHY_INTERFACE_MODE_QSGMII:
		return memac->qsgmii_pcs;
	case PHY_INTERFACE_MODE_10GBASER:
		return memac->xfi_pcs;
	default:
		return NULL;
	}
}

static int memac_prepare(struct phylink_config *config, unsigned int mode,
			 phy_interface_t iface)
{
	struct fman_mac *memac = fman_config_to_mac(config)->fman_mac;

	switch (iface) {
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_1000BASEX:
	case PHY_INTERFACE_MODE_QSGMII:
	case PHY_INTERFACE_MODE_10GBASER:
		return phy_set_mode_ext(memac->serdes, PHY_MODE_ETHERNET,
					iface);
	default:
		return 0;
	}
}

static void memac_mac_config(struct phylink_config *config, unsigned int mode,
			     const struct phylink_link_state *state)
{
	struct mac_device *mac_dev = fman_config_to_mac(config);
	struct memac_regs __iomem *regs = mac_dev->fman_mac->regs;
	u32 tmp = ioread32be(&regs->if_mode);

	tmp &= ~(IF_MODE_MASK | IF_MODE_RGMII);
	tmp |= memac_if_mode(state->interface);
	if (phylink_autoneg_inband(mode))
		tmp |= IF_MODE_RGMII_AUTO;
	iowrite32be(tmp, &regs->if_mode);
}

static void memac_link_up(struct phylink_config *config, struct phy_device *phy,
			  unsigned int mode, phy_interface_t interface,
			  int speed, int duplex, bool tx_pause, bool rx_pause)
{
	struct mac_device *mac_dev = fman_config_to_mac(config);
	struct fman_mac *memac = mac_dev->fman_mac;
	struct memac_regs __iomem *regs = memac->regs;
	u32 tmp = memac_if_mode(interface);
	u16 pause_time = tx_pause ? FSL_FM_PAUSE_TIME_ENABLE :
			 FSL_FM_PAUSE_TIME_DISABLE;

	memac_set_tx_pause_frames(memac, 0, pause_time, 0);
	memac_accept_rx_pause_frames(memac, rx_pause);

	if (duplex == DUPLEX_HALF)
		tmp |= IF_MODE_HD;

	switch (speed) {
	case SPEED_1000:
		tmp |= IF_MODE_RGMII_1000;
		break;
	case SPEED_100:
		tmp |= IF_MODE_RGMII_100;
		break;
	case SPEED_10:
		tmp |= IF_MODE_RGMII_10;
		break;
	}
	iowrite32be(tmp, &regs->if_mode);

	 

	if (speed == SPEED_10000) {
		if (memac->fm_rev_info.major == 6 &&
		    memac->fm_rev_info.minor == 4)
			tmp = TX_FIFO_SECTIONS_TX_AVAIL_SLOW_10G;
		else
			tmp = TX_FIFO_SECTIONS_TX_AVAIL_10G;
		tmp |= TX_FIFO_SECTIONS_TX_EMPTY_DEFAULT_10G;
	} else {
		tmp = TX_FIFO_SECTIONS_TX_AVAIL_1G |
		      TX_FIFO_SECTIONS_TX_EMPTY_DEFAULT_1G;
	}
	iowrite32be(tmp, &regs->tx_fifo_sections);

	mac_dev->update_speed(mac_dev, speed);

	tmp = ioread32be(&regs->command_config);
	tmp |= CMD_CFG_RX_EN | CMD_CFG_TX_EN;
	iowrite32be(tmp, &regs->command_config);
}

static void memac_link_down(struct phylink_config *config, unsigned int mode,
			    phy_interface_t interface)
{
	struct fman_mac *memac = fman_config_to_mac(config)->fman_mac;
	struct memac_regs __iomem *regs = memac->regs;
	u32 tmp;

	 
	tmp = ioread32be(&regs->command_config);
	tmp &= ~(CMD_CFG_RX_EN | CMD_CFG_TX_EN);
	iowrite32be(tmp, &regs->command_config);
}

static const struct phylink_mac_ops memac_mac_ops = {
	.validate = memac_validate,
	.mac_select_pcs = memac_select_pcs,
	.mac_prepare = memac_prepare,
	.mac_config = memac_mac_config,
	.mac_link_up = memac_link_up,
	.mac_link_down = memac_link_down,
};

static int memac_modify_mac_address(struct fman_mac *memac,
				    const enet_addr_t *enet_addr)
{
	add_addr_in_paddr(memac->regs, (const u8 *)(*enet_addr), 0);

	return 0;
}

static int memac_add_hash_mac_address(struct fman_mac *memac,
				      enet_addr_t *eth_addr)
{
	struct memac_regs __iomem *regs = memac->regs;
	struct eth_hash_entry *hash_entry;
	u32 hash;
	u64 addr;

	addr = ENET_ADDR_TO_UINT64(*eth_addr);

	if (!(addr & GROUP_ADDRESS)) {
		 
		pr_err("Unicast Address\n");
		return -EINVAL;
	}
	hash = get_mac_addr_hash_code(addr) & HASH_CTRL_ADDR_MASK;

	 
	hash_entry = kmalloc(sizeof(*hash_entry), GFP_ATOMIC);
	if (!hash_entry)
		return -ENOMEM;
	hash_entry->addr = addr;
	INIT_LIST_HEAD(&hash_entry->node);

	list_add_tail(&hash_entry->node,
		      &memac->multicast_addr_hash->lsts[hash]);
	iowrite32be(hash | HASH_CTRL_MCAST_EN, &regs->hashtable_ctrl);

	return 0;
}

static int memac_set_allmulti(struct fman_mac *memac, bool enable)
{
	u32 entry;
	struct memac_regs __iomem *regs = memac->regs;

	if (enable) {
		for (entry = 0; entry < HASH_TABLE_SIZE; entry++)
			iowrite32be(entry | HASH_CTRL_MCAST_EN,
				    &regs->hashtable_ctrl);
	} else {
		for (entry = 0; entry < HASH_TABLE_SIZE; entry++)
			iowrite32be(entry & ~HASH_CTRL_MCAST_EN,
				    &regs->hashtable_ctrl);
	}

	memac->allmulti_enabled = enable;

	return 0;
}

static int memac_set_tstamp(struct fman_mac *memac, bool enable)
{
	return 0;  
}

static int memac_del_hash_mac_address(struct fman_mac *memac,
				      enet_addr_t *eth_addr)
{
	struct memac_regs __iomem *regs = memac->regs;
	struct eth_hash_entry *hash_entry = NULL;
	struct list_head *pos;
	u32 hash;
	u64 addr;

	addr = ENET_ADDR_TO_UINT64(*eth_addr);

	hash = get_mac_addr_hash_code(addr) & HASH_CTRL_ADDR_MASK;

	list_for_each(pos, &memac->multicast_addr_hash->lsts[hash]) {
		hash_entry = ETH_HASH_ENTRY_OBJ(pos);
		if (hash_entry && hash_entry->addr == addr) {
			list_del_init(&hash_entry->node);
			kfree(hash_entry);
			break;
		}
	}

	if (!memac->allmulti_enabled) {
		if (list_empty(&memac->multicast_addr_hash->lsts[hash]))
			iowrite32be(hash & ~HASH_CTRL_MCAST_EN,
				    &regs->hashtable_ctrl);
	}

	return 0;
}

static int memac_set_exception(struct fman_mac *memac,
			       enum fman_mac_exceptions exception, bool enable)
{
	u32 bit_mask = 0;

	bit_mask = get_exception_flag(exception);
	if (bit_mask) {
		if (enable)
			memac->exceptions |= bit_mask;
		else
			memac->exceptions &= ~bit_mask;
	} else {
		pr_err("Undefined exception\n");
		return -EINVAL;
	}
	set_exception(memac->regs, bit_mask, enable);

	return 0;
}

static int memac_init(struct fman_mac *memac)
{
	struct memac_cfg *memac_drv_param;
	enet_addr_t eth_addr;
	int err;
	u32 reg32 = 0;

	err = check_init_parameters(memac);
	if (err)
		return err;

	memac_drv_param = memac->memac_drv_param;

	 
	if (memac_drv_param->reset_on_init) {
		err = reset(memac->regs);
		if (err) {
			pr_err("mEMAC reset failed\n");
			return err;
		}
	}

	 
	if (memac->addr != 0) {
		MAKE_ENET_ADDR_FROM_UINT64(memac->addr, eth_addr);
		add_addr_in_paddr(memac->regs, (const u8 *)eth_addr, 0);
	}

	init(memac->regs, memac->memac_drv_param, memac->exceptions);

	 
	if ((memac->fm_rev_info.major == 6) &&
	    ((memac->fm_rev_info.minor == 0) ||
	    (memac->fm_rev_info.minor == 3))) {
		 
		reg32 = ioread32be(&memac->regs->command_config);
		reg32 &= ~CMD_CFG_CRC_FWD;
		iowrite32be(reg32, &memac->regs->command_config);
	}

	 
	err = fman_set_mac_max_frame(memac->fm, memac->mac_id,
				     memac_drv_param->max_frame_length);
	if (err) {
		pr_err("settings Mac max frame length is FAILED\n");
		return err;
	}

	memac->multicast_addr_hash = alloc_hash_table(HASH_TABLE_SIZE);
	if (!memac->multicast_addr_hash) {
		free_init_resources(memac);
		pr_err("allocation hash table is FAILED\n");
		return -ENOMEM;
	}

	memac->unicast_addr_hash = alloc_hash_table(HASH_TABLE_SIZE);
	if (!memac->unicast_addr_hash) {
		free_init_resources(memac);
		pr_err("allocation hash table is FAILED\n");
		return -ENOMEM;
	}

	fman_register_intr(memac->fm, FMAN_MOD_MAC, memac->mac_id,
			   FMAN_INTR_TYPE_ERR, memac_err_exception, memac);

	fman_register_intr(memac->fm, FMAN_MOD_MAC, memac->mac_id,
			   FMAN_INTR_TYPE_NORMAL, memac_exception, memac);

	return 0;
}

static void pcs_put(struct phylink_pcs *pcs)
{
	if (IS_ERR_OR_NULL(pcs))
		return;

	lynx_pcs_destroy(pcs);
}

static int memac_free(struct fman_mac *memac)
{
	free_init_resources(memac);

	pcs_put(memac->sgmii_pcs);
	pcs_put(memac->qsgmii_pcs);
	pcs_put(memac->xfi_pcs);
	kfree(memac->memac_drv_param);
	kfree(memac);

	return 0;
}

static struct fman_mac *memac_config(struct mac_device *mac_dev,
				     struct fman_mac_params *params)
{
	struct fman_mac *memac;
	struct memac_cfg *memac_drv_param;

	 
	memac = kzalloc(sizeof(*memac), GFP_KERNEL);
	if (!memac)
		return NULL;

	 
	memac_drv_param = kzalloc(sizeof(*memac_drv_param), GFP_KERNEL);
	if (!memac_drv_param) {
		memac_free(memac);
		return NULL;
	}

	 
	memac->memac_drv_param = memac_drv_param;

	set_dflts(memac_drv_param);

	memac->addr = ENET_ADDR_TO_UINT64(mac_dev->addr);

	memac->regs = mac_dev->vaddr;
	memac->mac_id = params->mac_id;
	memac->exceptions = (MEMAC_IMASK_TSECC_ER | MEMAC_IMASK_TECC_ER |
			     MEMAC_IMASK_RECC_ER | MEMAC_IMASK_MGI);
	memac->exception_cb = params->exception_cb;
	memac->event_cb = params->event_cb;
	memac->dev_id = mac_dev;
	memac->fm = params->fm;

	 
	fman_get_revision(memac->fm, &memac->fm_rev_info);

	return memac;
}

static struct phylink_pcs *memac_pcs_create(struct device_node *mac_node,
					    int index)
{
	struct device_node *node;
	struct phylink_pcs *pcs;

	node = of_parse_phandle(mac_node, "pcsphy-handle", index);
	if (!node)
		return ERR_PTR(-ENODEV);

	pcs = lynx_pcs_create_fwnode(of_fwnode_handle(node));
	of_node_put(node);

	return pcs;
}

static bool memac_supports(struct mac_device *mac_dev, phy_interface_t iface)
{
	 
	if (!mac_dev->fman_mac->serdes)
		return mac_dev->phy_if == iface;
	 
	return !phy_validate(mac_dev->fman_mac->serdes, PHY_MODE_ETHERNET,
			     iface, NULL);
}

int memac_initialization(struct mac_device *mac_dev,
			 struct device_node *mac_node,
			 struct fman_mac_params *params)
{
	int			 err;
	struct device_node      *fixed;
	struct phylink_pcs	*pcs;
	struct fman_mac		*memac;
	unsigned long		 capabilities;
	unsigned long		*supported;

	mac_dev->phylink_ops		= &memac_mac_ops;
	mac_dev->set_promisc		= memac_set_promiscuous;
	mac_dev->change_addr		= memac_modify_mac_address;
	mac_dev->add_hash_mac_addr	= memac_add_hash_mac_address;
	mac_dev->remove_hash_mac_addr	= memac_del_hash_mac_address;
	mac_dev->set_exception		= memac_set_exception;
	mac_dev->set_allmulti		= memac_set_allmulti;
	mac_dev->set_tstamp		= memac_set_tstamp;
	mac_dev->set_multi		= fman_set_multi;
	mac_dev->enable			= memac_enable;
	mac_dev->disable		= memac_disable;

	mac_dev->fman_mac = memac_config(mac_dev, params);
	if (!mac_dev->fman_mac)
		return -EINVAL;

	memac = mac_dev->fman_mac;
	memac->memac_drv_param->max_frame_length = fman_get_max_frm();
	memac->memac_drv_param->reset_on_init = true;

	err = of_property_match_string(mac_node, "pcs-handle-names", "xfi");
	if (err >= 0) {
		memac->xfi_pcs = memac_pcs_create(mac_node, err);
		if (IS_ERR(memac->xfi_pcs)) {
			err = PTR_ERR(memac->xfi_pcs);
			dev_err_probe(mac_dev->dev, err, "missing xfi pcs\n");
			goto _return_fm_mac_free;
		}
	} else if (err != -EINVAL && err != -ENODATA) {
		goto _return_fm_mac_free;
	}

	err = of_property_match_string(mac_node, "pcs-handle-names", "qsgmii");
	if (err >= 0) {
		memac->qsgmii_pcs = memac_pcs_create(mac_node, err);
		if (IS_ERR(memac->qsgmii_pcs)) {
			err = PTR_ERR(memac->qsgmii_pcs);
			dev_err_probe(mac_dev->dev, err,
				      "missing qsgmii pcs\n");
			goto _return_fm_mac_free;
		}
	} else if (err != -EINVAL && err != -ENODATA) {
		goto _return_fm_mac_free;
	}

	 
	err = of_property_match_string(mac_node, "pcs-handle-names", "sgmii");
	if (err == -EINVAL || err == -ENODATA)
		pcs = memac_pcs_create(mac_node, 0);
	else if (err < 0)
		goto _return_fm_mac_free;
	else
		pcs = memac_pcs_create(mac_node, err);

	if (IS_ERR(pcs)) {
		err = PTR_ERR(pcs);
		dev_err_probe(mac_dev->dev, err, "missing pcs\n");
		goto _return_fm_mac_free;
	}

	 
	if (err && mac_dev->phy_if == PHY_INTERFACE_MODE_XGMII)
		memac->xfi_pcs = pcs;
	else
		memac->sgmii_pcs = pcs;

	memac->serdes = devm_of_phy_optional_get(mac_dev->dev, mac_node,
						 "serdes");
	if (!memac->serdes) {
		dev_dbg(mac_dev->dev, "could not get (optional) serdes\n");
	} else if (IS_ERR(memac->serdes)) {
		err = PTR_ERR(memac->serdes);
		goto _return_fm_mac_free;
	}

	 
	if (mac_dev->phy_if == PHY_INTERFACE_MODE_XGMII)
		mac_dev->phy_if = PHY_INTERFACE_MODE_10GBASER;

	 
	supported = mac_dev->phylink_config.supported_interfaces;

	 

	if (memac->sgmii_pcs &&
	    (memac_supports(mac_dev, PHY_INTERFACE_MODE_SGMII) ||
	     memac_supports(mac_dev, PHY_INTERFACE_MODE_1000BASEX))) {
		__set_bit(PHY_INTERFACE_MODE_SGMII, supported);
		__set_bit(PHY_INTERFACE_MODE_1000BASEX, supported);
	}

	if (memac->sgmii_pcs &&
	    memac_supports(mac_dev, PHY_INTERFACE_MODE_2500BASEX))
		__set_bit(PHY_INTERFACE_MODE_2500BASEX, supported);

	if (memac->qsgmii_pcs &&
	    memac_supports(mac_dev, PHY_INTERFACE_MODE_QSGMII))
		__set_bit(PHY_INTERFACE_MODE_QSGMII, supported);
	else if (mac_dev->phy_if == PHY_INTERFACE_MODE_QSGMII)
		dev_warn(mac_dev->dev, "no QSGMII pcs specified\n");

	if (memac->xfi_pcs &&
	    memac_supports(mac_dev, PHY_INTERFACE_MODE_10GBASER)) {
		__set_bit(PHY_INTERFACE_MODE_10GBASER, supported);
	} else {
		 
		phy_interface_set_rgmii(supported);
		__set_bit(PHY_INTERFACE_MODE_MII, supported);
	}

	capabilities = MAC_SYM_PAUSE | MAC_ASYM_PAUSE | MAC_10 | MAC_100;
	capabilities |= MAC_1000FD | MAC_2500FD | MAC_10000FD;

	 
	if (of_machine_is_compatible("fsl,ls1043a") ||
	    of_machine_is_compatible("fsl,ls1046a") ||
	    of_machine_is_compatible("fsl,B4QDS"))
		capabilities &= ~(MAC_10HD | MAC_100HD);

	mac_dev->phylink_config.mac_capabilities = capabilities;

	 
	if (of_machine_is_compatible("fsl,T2080QDS") ||
	    of_machine_is_compatible("fsl,T2080RDB") ||
	    of_machine_is_compatible("fsl,T2081QDS") ||
	    of_machine_is_compatible("fsl,T4240QDS") ||
	    of_machine_is_compatible("fsl,T4240RDB"))
		memac->rgmii_no_half_duplex = true;

	 
	fixed = of_get_child_by_name(mac_node, "fixed-link");
	if (!fixed && !of_property_read_bool(mac_node, "fixed-link") &&
	    !of_property_read_bool(mac_node, "managed") &&
	    mac_dev->phy_if != PHY_INTERFACE_MODE_MII &&
	    !phy_interface_mode_is_rgmii(mac_dev->phy_if))
		mac_dev->phylink_config.ovr_an_inband = true;
	of_node_put(fixed);

	err = memac_init(mac_dev->fman_mac);
	if (err < 0)
		goto _return_fm_mac_free;

	dev_info(mac_dev->dev, "FMan MEMAC\n");

	return 0;

_return_fm_mac_free:
	memac_free(mac_dev->fman_mac);
	return err;
}
