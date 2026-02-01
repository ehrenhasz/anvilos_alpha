
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "fman_tgec.h"
#include "fman.h"
#include "mac.h"

#include <linux/slab.h>
#include <linux/bitrev.h>
#include <linux/io.h>
#include <linux/crc32.h>
#include <linux/netdevice.h>

 
#define TGEC_TX_IPG_LENGTH_MASK	0x000003ff

 
#define CMD_CFG_EN_TIMESTAMP		0x00100000
#define CMD_CFG_NO_LEN_CHK		0x00020000
#define CMD_CFG_PAUSE_IGNORE		0x00000100
#define CMF_CFG_CRC_FWD			0x00000040
#define CMD_CFG_PROMIS_EN		0x00000010
#define CMD_CFG_RX_EN			0x00000002
#define CMD_CFG_TX_EN			0x00000001

 
#define TGEC_IMASK_MDIO_SCAN_EVENT	0x00010000
#define TGEC_IMASK_MDIO_CMD_CMPL	0x00008000
#define TGEC_IMASK_REM_FAULT		0x00004000
#define TGEC_IMASK_LOC_FAULT		0x00002000
#define TGEC_IMASK_TX_ECC_ER		0x00001000
#define TGEC_IMASK_TX_FIFO_UNFL	0x00000800
#define TGEC_IMASK_TX_FIFO_OVFL	0x00000400
#define TGEC_IMASK_TX_ER		0x00000200
#define TGEC_IMASK_RX_FIFO_OVFL	0x00000100
#define TGEC_IMASK_RX_ECC_ER		0x00000080
#define TGEC_IMASK_RX_JAB_FRM		0x00000040
#define TGEC_IMASK_RX_OVRSZ_FRM	0x00000020
#define TGEC_IMASK_RX_RUNT_FRM		0x00000010
#define TGEC_IMASK_RX_FRAG_FRM		0x00000008
#define TGEC_IMASK_RX_LEN_ER		0x00000004
#define TGEC_IMASK_RX_CRC_ER		0x00000002
#define TGEC_IMASK_RX_ALIGN_ER		0x00000001

 
#define TGEC_HASH_MCAST_SHIFT		23
#define TGEC_HASH_MCAST_EN		0x00000200
#define TGEC_HASH_ADR_MSK		0x000001ff

#define DEFAULT_TX_IPG_LENGTH			12
#define DEFAULT_MAX_FRAME_LENGTH		0x600
#define DEFAULT_PAUSE_QUANT			0xf000

 
#define TGEC_NUM_OF_PADDRS          1

 
#define GROUP_ADDRESS               0x0000010000000000LL

 
#define TGEC_HASH_TABLE_SIZE             512

 
struct tgec_regs {
	u32 tgec_id;		 
	u32 reserved001[1];	 
	u32 command_config;	 
	u32 mac_addr_0;		 
	u32 mac_addr_1;		 
	u32 maxfrm;		 
	u32 pause_quant;	 
	u32 rx_fifo_sections;	 
	u32 tx_fifo_sections;	 
	u32 rx_fifo_almost_f_e;	 
	u32 tx_fifo_almost_f_e;	 
	u32 hashtable_ctrl;	 
	u32 mdio_cfg_status;	 
	u32 mdio_command;	 
	u32 mdio_data;		 
	u32 mdio_regaddr;	 
	u32 status;		 
	u32 tx_ipg_len;		 
	u32 mac_addr_2;		 
	u32 mac_addr_3;		 
	u32 rx_fifo_ptr_rd;	 
	u32 rx_fifo_ptr_wr;	 
	u32 tx_fifo_ptr_rd;	 
	u32 tx_fifo_ptr_wr;	 
	u32 imask;		 
	u32 ievent;		 
	u32 udp_port;		 
	u32 type_1588v2;	 
	u32 reserved070[4];	 
	 
	u32 tfrm_u;		 
	u32 tfrm_l;		 
	u32 rfrm_u;		 
	u32 rfrm_l;		 
	u32 rfcs_u;		 
	u32 rfcs_l;		 
	u32 raln_u;		 
	u32 raln_l;		 
	u32 txpf_u;		 
	u32 txpf_l;		 
	u32 rxpf_u;		 
	u32 rxpf_l;		 
	u32 rlong_u;		 
	u32 rlong_l;		 
	u32 rflr_u;		 
	u32 rflr_l;		 
	u32 tvlan_u;		 
	u32 tvlan_l;		 
	u32 rvlan_u;		 
	u32 rvlan_l;		 
	u32 toct_u;		 
	u32 toct_l;		 
	u32 roct_u;		 
	u32 roct_l;		 
	u32 ruca_u;		 
	u32 ruca_l;		 
	u32 rmca_u;		 
	u32 rmca_l;		 
	u32 rbca_u;		 
	u32 rbca_l;		 
	u32 terr_u;		 
	u32 terr_l;		 
	u32 reserved100[2];	 
	u32 tuca_u;		 
	u32 tuca_l;		 
	u32 tmca_u;		 
	u32 tmca_l;		 
	u32 tbca_u;		 
	u32 tbca_l;		 
	u32 rdrp_u;		 
	u32 rdrp_l;		 
	u32 reoct_u;		 
	u32 reoct_l;		 
	u32 rpkt_u;		 
	u32 rpkt_l;		 
	u32 trund_u;		 
	u32 trund_l;		 
	u32 r64_u;		 
	u32 r64_l;		 
	u32 r127_u;		 
	u32 r127_l;		 
	u32 r255_u;		 
	u32 r255_l;		 
	u32 r511_u;		 
	u32 r511_l;		 
	u32 r1023_u;		 
	u32 r1023_l;		 
	u32 r1518_u;		 
	u32 r1518_l;		 
	u32 r1519x_u;		 
	u32 r1519x_l;		 
	u32 trovr_u;		 
	u32 trovr_l;		 
	u32 trjbr_u;		 
	u32 trjbr_l;		 
	u32 trfrg_u;		 
	u32 trfrg_l;		 
	u32 rerr_u;		 
	u32 rerr_l;		 
};

struct tgec_cfg {
	bool pause_ignore;
	bool promiscuous_mode_enable;
	u16 max_frame_length;
	u16 pause_quant;
	u32 tx_ipg_length;
};

struct fman_mac {
	 
	struct tgec_regs __iomem *regs;
	 
	u64 addr;
	u16 max_speed;
	struct mac_device *dev_id;  
	fman_mac_exception_cb *exception_cb;
	fman_mac_exception_cb *event_cb;
	 
	struct eth_hash_t *multicast_addr_hash;
	 
	struct eth_hash_t *unicast_addr_hash;
	u8 mac_id;
	u32 exceptions;
	struct tgec_cfg *cfg;
	void *fm;
	struct fman_rev_info fm_rev_info;
	bool allmulti_enabled;
};

static void set_mac_address(struct tgec_regs __iomem *regs, const u8 *adr)
{
	u32 tmp0, tmp1;

	tmp0 = (u32)(adr[0] | adr[1] << 8 | adr[2] << 16 | adr[3] << 24);
	tmp1 = (u32)(adr[4] | adr[5] << 8);
	iowrite32be(tmp0, &regs->mac_addr_0);
	iowrite32be(tmp1, &regs->mac_addr_1);
}

static void set_dflts(struct tgec_cfg *cfg)
{
	cfg->promiscuous_mode_enable = false;
	cfg->pause_ignore = false;
	cfg->tx_ipg_length = DEFAULT_TX_IPG_LENGTH;
	cfg->max_frame_length = DEFAULT_MAX_FRAME_LENGTH;
	cfg->pause_quant = DEFAULT_PAUSE_QUANT;
}

static int init(struct tgec_regs __iomem *regs, struct tgec_cfg *cfg,
		u32 exception_mask)
{
	u32 tmp;

	 
	tmp = CMF_CFG_CRC_FWD;
	if (cfg->promiscuous_mode_enable)
		tmp |= CMD_CFG_PROMIS_EN;
	if (cfg->pause_ignore)
		tmp |= CMD_CFG_PAUSE_IGNORE;
	 
	tmp |= CMD_CFG_NO_LEN_CHK;
	iowrite32be(tmp, &regs->command_config);

	 
	iowrite32be((u32)cfg->max_frame_length, &regs->maxfrm);
	 
	iowrite32be(cfg->pause_quant, &regs->pause_quant);

	 
	iowrite32be(0xffffffff, &regs->ievent);
	iowrite32be(ioread32be(&regs->imask) | exception_mask, &regs->imask);

	return 0;
}

static int check_init_parameters(struct fman_mac *tgec)
{
	if (!tgec->exception_cb) {
		pr_err("uninitialized exception_cb\n");
		return -EINVAL;
	}
	if (!tgec->event_cb) {
		pr_err("uninitialized event_cb\n");
		return -EINVAL;
	}

	return 0;
}

static int get_exception_flag(enum fman_mac_exceptions exception)
{
	u32 bit_mask;

	switch (exception) {
	case FM_MAC_EX_10G_MDIO_SCAN_EVENT:
		bit_mask = TGEC_IMASK_MDIO_SCAN_EVENT;
		break;
	case FM_MAC_EX_10G_MDIO_CMD_CMPL:
		bit_mask = TGEC_IMASK_MDIO_CMD_CMPL;
		break;
	case FM_MAC_EX_10G_REM_FAULT:
		bit_mask = TGEC_IMASK_REM_FAULT;
		break;
	case FM_MAC_EX_10G_LOC_FAULT:
		bit_mask = TGEC_IMASK_LOC_FAULT;
		break;
	case FM_MAC_EX_10G_TX_ECC_ER:
		bit_mask = TGEC_IMASK_TX_ECC_ER;
		break;
	case FM_MAC_EX_10G_TX_FIFO_UNFL:
		bit_mask = TGEC_IMASK_TX_FIFO_UNFL;
		break;
	case FM_MAC_EX_10G_TX_FIFO_OVFL:
		bit_mask = TGEC_IMASK_TX_FIFO_OVFL;
		break;
	case FM_MAC_EX_10G_TX_ER:
		bit_mask = TGEC_IMASK_TX_ER;
		break;
	case FM_MAC_EX_10G_RX_FIFO_OVFL:
		bit_mask = TGEC_IMASK_RX_FIFO_OVFL;
		break;
	case FM_MAC_EX_10G_RX_ECC_ER:
		bit_mask = TGEC_IMASK_RX_ECC_ER;
		break;
	case FM_MAC_EX_10G_RX_JAB_FRM:
		bit_mask = TGEC_IMASK_RX_JAB_FRM;
		break;
	case FM_MAC_EX_10G_RX_OVRSZ_FRM:
		bit_mask = TGEC_IMASK_RX_OVRSZ_FRM;
		break;
	case FM_MAC_EX_10G_RX_RUNT_FRM:
		bit_mask = TGEC_IMASK_RX_RUNT_FRM;
		break;
	case FM_MAC_EX_10G_RX_FRAG_FRM:
		bit_mask = TGEC_IMASK_RX_FRAG_FRM;
		break;
	case FM_MAC_EX_10G_RX_LEN_ER:
		bit_mask = TGEC_IMASK_RX_LEN_ER;
		break;
	case FM_MAC_EX_10G_RX_CRC_ER:
		bit_mask = TGEC_IMASK_RX_CRC_ER;
		break;
	case FM_MAC_EX_10G_RX_ALIGN_ER:
		bit_mask = TGEC_IMASK_RX_ALIGN_ER;
		break;
	default:
		bit_mask = 0;
		break;
	}

	return bit_mask;
}

static void tgec_err_exception(void *handle)
{
	struct fman_mac *tgec = (struct fman_mac *)handle;
	struct tgec_regs __iomem *regs = tgec->regs;
	u32 event;

	 
	event = ioread32be(&regs->ievent) &
			   ~(TGEC_IMASK_MDIO_SCAN_EVENT |
			   TGEC_IMASK_MDIO_CMD_CMPL);

	event &= ioread32be(&regs->imask);

	iowrite32be(event, &regs->ievent);

	if (event & TGEC_IMASK_REM_FAULT)
		tgec->exception_cb(tgec->dev_id, FM_MAC_EX_10G_REM_FAULT);
	if (event & TGEC_IMASK_LOC_FAULT)
		tgec->exception_cb(tgec->dev_id, FM_MAC_EX_10G_LOC_FAULT);
	if (event & TGEC_IMASK_TX_ECC_ER)
		tgec->exception_cb(tgec->dev_id, FM_MAC_EX_10G_TX_ECC_ER);
	if (event & TGEC_IMASK_TX_FIFO_UNFL)
		tgec->exception_cb(tgec->dev_id, FM_MAC_EX_10G_TX_FIFO_UNFL);
	if (event & TGEC_IMASK_TX_FIFO_OVFL)
		tgec->exception_cb(tgec->dev_id, FM_MAC_EX_10G_TX_FIFO_OVFL);
	if (event & TGEC_IMASK_TX_ER)
		tgec->exception_cb(tgec->dev_id, FM_MAC_EX_10G_TX_ER);
	if (event & TGEC_IMASK_RX_FIFO_OVFL)
		tgec->exception_cb(tgec->dev_id, FM_MAC_EX_10G_RX_FIFO_OVFL);
	if (event & TGEC_IMASK_RX_ECC_ER)
		tgec->exception_cb(tgec->dev_id, FM_MAC_EX_10G_RX_ECC_ER);
	if (event & TGEC_IMASK_RX_JAB_FRM)
		tgec->exception_cb(tgec->dev_id, FM_MAC_EX_10G_RX_JAB_FRM);
	if (event & TGEC_IMASK_RX_OVRSZ_FRM)
		tgec->exception_cb(tgec->dev_id, FM_MAC_EX_10G_RX_OVRSZ_FRM);
	if (event & TGEC_IMASK_RX_RUNT_FRM)
		tgec->exception_cb(tgec->dev_id, FM_MAC_EX_10G_RX_RUNT_FRM);
	if (event & TGEC_IMASK_RX_FRAG_FRM)
		tgec->exception_cb(tgec->dev_id, FM_MAC_EX_10G_RX_FRAG_FRM);
	if (event & TGEC_IMASK_RX_LEN_ER)
		tgec->exception_cb(tgec->dev_id, FM_MAC_EX_10G_RX_LEN_ER);
	if (event & TGEC_IMASK_RX_CRC_ER)
		tgec->exception_cb(tgec->dev_id, FM_MAC_EX_10G_RX_CRC_ER);
	if (event & TGEC_IMASK_RX_ALIGN_ER)
		tgec->exception_cb(tgec->dev_id, FM_MAC_EX_10G_RX_ALIGN_ER);
}

static void free_init_resources(struct fman_mac *tgec)
{
	fman_unregister_intr(tgec->fm, FMAN_MOD_MAC, tgec->mac_id,
			     FMAN_INTR_TYPE_ERR);

	 
	free_hash_table(tgec->multicast_addr_hash);
	tgec->multicast_addr_hash = NULL;

	 
	free_hash_table(tgec->unicast_addr_hash);
	tgec->unicast_addr_hash = NULL;
}

static int tgec_enable(struct fman_mac *tgec)
{
	return 0;
}

static void tgec_disable(struct fman_mac *tgec)
{
}

static int tgec_set_promiscuous(struct fman_mac *tgec, bool new_val)
{
	struct tgec_regs __iomem *regs = tgec->regs;
	u32 tmp;

	tmp = ioread32be(&regs->command_config);
	if (new_val)
		tmp |= CMD_CFG_PROMIS_EN;
	else
		tmp &= ~CMD_CFG_PROMIS_EN;
	iowrite32be(tmp, &regs->command_config);

	return 0;
}

static int tgec_set_tx_pause_frames(struct fman_mac *tgec,
				    u8 __maybe_unused priority, u16 pause_time,
				    u16 __maybe_unused thresh_time)
{
	struct tgec_regs __iomem *regs = tgec->regs;

	iowrite32be((u32)pause_time, &regs->pause_quant);

	return 0;
}

static int tgec_accept_rx_pause_frames(struct fman_mac *tgec, bool en)
{
	struct tgec_regs __iomem *regs = tgec->regs;
	u32 tmp;

	tmp = ioread32be(&regs->command_config);
	if (!en)
		tmp |= CMD_CFG_PAUSE_IGNORE;
	else
		tmp &= ~CMD_CFG_PAUSE_IGNORE;
	iowrite32be(tmp, &regs->command_config);

	return 0;
}

static void tgec_mac_config(struct phylink_config *config, unsigned int mode,
			    const struct phylink_link_state *state)
{
}

static void tgec_link_up(struct phylink_config *config, struct phy_device *phy,
			 unsigned int mode, phy_interface_t interface,
			 int speed, int duplex, bool tx_pause, bool rx_pause)
{
	struct mac_device *mac_dev = fman_config_to_mac(config);
	struct fman_mac *tgec = mac_dev->fman_mac;
	struct tgec_regs __iomem *regs = tgec->regs;
	u16 pause_time = tx_pause ? FSL_FM_PAUSE_TIME_ENABLE :
			 FSL_FM_PAUSE_TIME_DISABLE;
	u32 tmp;

	tgec_set_tx_pause_frames(tgec, 0, pause_time, 0);
	tgec_accept_rx_pause_frames(tgec, rx_pause);
	mac_dev->update_speed(mac_dev, speed);

	tmp = ioread32be(&regs->command_config);
	tmp |= CMD_CFG_RX_EN | CMD_CFG_TX_EN;
	iowrite32be(tmp, &regs->command_config);
}

static void tgec_link_down(struct phylink_config *config, unsigned int mode,
			   phy_interface_t interface)
{
	struct fman_mac *tgec = fman_config_to_mac(config)->fman_mac;
	struct tgec_regs __iomem *regs = tgec->regs;
	u32 tmp;

	tmp = ioread32be(&regs->command_config);
	tmp &= ~(CMD_CFG_RX_EN | CMD_CFG_TX_EN);
	iowrite32be(tmp, &regs->command_config);
}

static const struct phylink_mac_ops tgec_mac_ops = {
	.mac_config = tgec_mac_config,
	.mac_link_up = tgec_link_up,
	.mac_link_down = tgec_link_down,
};

static int tgec_modify_mac_address(struct fman_mac *tgec,
				   const enet_addr_t *p_enet_addr)
{
	tgec->addr = ENET_ADDR_TO_UINT64(*p_enet_addr);
	set_mac_address(tgec->regs, (const u8 *)(*p_enet_addr));

	return 0;
}

static int tgec_add_hash_mac_address(struct fman_mac *tgec,
				     enet_addr_t *eth_addr)
{
	struct tgec_regs __iomem *regs = tgec->regs;
	struct eth_hash_entry *hash_entry;
	u32 crc = 0xFFFFFFFF, hash;
	u64 addr;

	addr = ENET_ADDR_TO_UINT64(*eth_addr);

	if (!(addr & GROUP_ADDRESS)) {
		 
		pr_err("Unicast Address\n");
		return -EINVAL;
	}
	 
	crc = crc32_le(crc, (u8 *)eth_addr, ETH_ALEN);
	crc = bitrev32(crc);
	 
	hash = (crc >> TGEC_HASH_MCAST_SHIFT) & TGEC_HASH_ADR_MSK;

	 
	hash_entry = kmalloc(sizeof(*hash_entry), GFP_ATOMIC);
	if (!hash_entry)
		return -ENOMEM;
	hash_entry->addr = addr;
	INIT_LIST_HEAD(&hash_entry->node);

	list_add_tail(&hash_entry->node,
		      &tgec->multicast_addr_hash->lsts[hash]);
	iowrite32be((hash | TGEC_HASH_MCAST_EN), &regs->hashtable_ctrl);

	return 0;
}

static int tgec_set_allmulti(struct fman_mac *tgec, bool enable)
{
	u32 entry;
	struct tgec_regs __iomem *regs = tgec->regs;

	if (enable) {
		for (entry = 0; entry < TGEC_HASH_TABLE_SIZE; entry++)
			iowrite32be(entry | TGEC_HASH_MCAST_EN,
				    &regs->hashtable_ctrl);
	} else {
		for (entry = 0; entry < TGEC_HASH_TABLE_SIZE; entry++)
			iowrite32be(entry & ~TGEC_HASH_MCAST_EN,
				    &regs->hashtable_ctrl);
	}

	tgec->allmulti_enabled = enable;

	return 0;
}

static int tgec_set_tstamp(struct fman_mac *tgec, bool enable)
{
	struct tgec_regs __iomem *regs = tgec->regs;
	u32 tmp;

	tmp = ioread32be(&regs->command_config);

	if (enable)
		tmp |= CMD_CFG_EN_TIMESTAMP;
	else
		tmp &= ~CMD_CFG_EN_TIMESTAMP;

	iowrite32be(tmp, &regs->command_config);

	return 0;
}

static int tgec_del_hash_mac_address(struct fman_mac *tgec,
				     enet_addr_t *eth_addr)
{
	struct tgec_regs __iomem *regs = tgec->regs;
	struct eth_hash_entry *hash_entry = NULL;
	struct list_head *pos;
	u32 crc = 0xFFFFFFFF, hash;
	u64 addr;

	addr = ((*(u64 *)eth_addr) >> 16);

	 
	crc = crc32_le(crc, (u8 *)eth_addr, ETH_ALEN);
	crc = bitrev32(crc);
	 
	hash = (crc >> TGEC_HASH_MCAST_SHIFT) & TGEC_HASH_ADR_MSK;

	list_for_each(pos, &tgec->multicast_addr_hash->lsts[hash]) {
		hash_entry = ETH_HASH_ENTRY_OBJ(pos);
		if (hash_entry && hash_entry->addr == addr) {
			list_del_init(&hash_entry->node);
			kfree(hash_entry);
			break;
		}
	}

	if (!tgec->allmulti_enabled) {
		if (list_empty(&tgec->multicast_addr_hash->lsts[hash]))
			iowrite32be((hash & ~TGEC_HASH_MCAST_EN),
				    &regs->hashtable_ctrl);
	}

	return 0;
}

static int tgec_set_exception(struct fman_mac *tgec,
			      enum fman_mac_exceptions exception, bool enable)
{
	struct tgec_regs __iomem *regs = tgec->regs;
	u32 bit_mask = 0;

	bit_mask = get_exception_flag(exception);
	if (bit_mask) {
		if (enable)
			tgec->exceptions |= bit_mask;
		else
			tgec->exceptions &= ~bit_mask;
	} else {
		pr_err("Undefined exception\n");
		return -EINVAL;
	}
	if (enable)
		iowrite32be(ioread32be(&regs->imask) | bit_mask, &regs->imask);
	else
		iowrite32be(ioread32be(&regs->imask) & ~bit_mask, &regs->imask);

	return 0;
}

static int tgec_init(struct fman_mac *tgec)
{
	struct tgec_cfg *cfg;
	enet_addr_t eth_addr;
	int err;

	if (DEFAULT_RESET_ON_INIT &&
	    (fman_reset_mac(tgec->fm, tgec->mac_id) != 0)) {
		pr_err("Can't reset MAC!\n");
		return -EINVAL;
	}

	err = check_init_parameters(tgec);
	if (err)
		return err;

	cfg = tgec->cfg;

	if (tgec->addr) {
		MAKE_ENET_ADDR_FROM_UINT64(tgec->addr, eth_addr);
		set_mac_address(tgec->regs, (const u8 *)eth_addr);
	}

	 
	 
	if (tgec->fm_rev_info.major <= 2)
		tgec->exceptions &= ~(TGEC_IMASK_REM_FAULT |
				      TGEC_IMASK_LOC_FAULT);

	err = init(tgec->regs, cfg, tgec->exceptions);
	if (err) {
		free_init_resources(tgec);
		pr_err("TGEC version doesn't support this i/f mode\n");
		return err;
	}

	 
	err = fman_set_mac_max_frame(tgec->fm, tgec->mac_id,
				     cfg->max_frame_length);
	if (err) {
		pr_err("Setting max frame length FAILED\n");
		free_init_resources(tgec);
		return -EINVAL;
	}

	 
	if (tgec->fm_rev_info.major == 2) {
		struct tgec_regs __iomem *regs = tgec->regs;
		u32 tmp;

		 
		tmp = (ioread32be(&regs->tx_ipg_len) &
		       ~TGEC_TX_IPG_LENGTH_MASK) | 12;

		iowrite32be(tmp, &regs->tx_ipg_len);
	}

	tgec->multicast_addr_hash = alloc_hash_table(TGEC_HASH_TABLE_SIZE);
	if (!tgec->multicast_addr_hash) {
		free_init_resources(tgec);
		pr_err("allocation hash table is FAILED\n");
		return -ENOMEM;
	}

	tgec->unicast_addr_hash = alloc_hash_table(TGEC_HASH_TABLE_SIZE);
	if (!tgec->unicast_addr_hash) {
		free_init_resources(tgec);
		pr_err("allocation hash table is FAILED\n");
		return -ENOMEM;
	}

	fman_register_intr(tgec->fm, FMAN_MOD_MAC, tgec->mac_id,
			   FMAN_INTR_TYPE_ERR, tgec_err_exception, tgec);

	kfree(cfg);
	tgec->cfg = NULL;

	return 0;
}

static int tgec_free(struct fman_mac *tgec)
{
	free_init_resources(tgec);

	kfree(tgec->cfg);
	kfree(tgec);

	return 0;
}

static struct fman_mac *tgec_config(struct mac_device *mac_dev,
				    struct fman_mac_params *params)
{
	struct fman_mac *tgec;
	struct tgec_cfg *cfg;

	 
	tgec = kzalloc(sizeof(*tgec), GFP_KERNEL);
	if (!tgec)
		return NULL;

	 
	cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
	if (!cfg) {
		tgec_free(tgec);
		return NULL;
	}

	 
	tgec->cfg = cfg;

	set_dflts(cfg);

	tgec->regs = mac_dev->vaddr;
	tgec->addr = ENET_ADDR_TO_UINT64(mac_dev->addr);
	tgec->mac_id = params->mac_id;
	tgec->exceptions = (TGEC_IMASK_MDIO_SCAN_EVENT	|
			    TGEC_IMASK_REM_FAULT	|
			    TGEC_IMASK_LOC_FAULT	|
			    TGEC_IMASK_TX_ECC_ER	|
			    TGEC_IMASK_TX_FIFO_UNFL	|
			    TGEC_IMASK_TX_FIFO_OVFL	|
			    TGEC_IMASK_TX_ER		|
			    TGEC_IMASK_RX_FIFO_OVFL	|
			    TGEC_IMASK_RX_ECC_ER	|
			    TGEC_IMASK_RX_JAB_FRM	|
			    TGEC_IMASK_RX_OVRSZ_FRM	|
			    TGEC_IMASK_RX_RUNT_FRM	|
			    TGEC_IMASK_RX_FRAG_FRM	|
			    TGEC_IMASK_RX_CRC_ER	|
			    TGEC_IMASK_RX_ALIGN_ER);
	tgec->exception_cb = params->exception_cb;
	tgec->event_cb = params->event_cb;
	tgec->dev_id = mac_dev;
	tgec->fm = params->fm;

	 
	fman_get_revision(tgec->fm, &tgec->fm_rev_info);

	return tgec;
}

int tgec_initialization(struct mac_device *mac_dev,
			struct device_node *mac_node,
			struct fman_mac_params *params)
{
	int err;
	struct fman_mac		*tgec;

	mac_dev->phylink_ops		= &tgec_mac_ops;
	mac_dev->set_promisc		= tgec_set_promiscuous;
	mac_dev->change_addr		= tgec_modify_mac_address;
	mac_dev->add_hash_mac_addr	= tgec_add_hash_mac_address;
	mac_dev->remove_hash_mac_addr	= tgec_del_hash_mac_address;
	mac_dev->set_exception		= tgec_set_exception;
	mac_dev->set_allmulti		= tgec_set_allmulti;
	mac_dev->set_tstamp		= tgec_set_tstamp;
	mac_dev->set_multi		= fman_set_multi;
	mac_dev->enable			= tgec_enable;
	mac_dev->disable		= tgec_disable;

	mac_dev->fman_mac = tgec_config(mac_dev, params);
	if (!mac_dev->fman_mac) {
		err = -EINVAL;
		goto _return;
	}

	 
	if (mac_dev->phy_if == PHY_INTERFACE_MODE_XGMII)
		mac_dev->phy_if = PHY_INTERFACE_MODE_XAUI;

	__set_bit(PHY_INTERFACE_MODE_XAUI,
		  mac_dev->phylink_config.supported_interfaces);
	mac_dev->phylink_config.mac_capabilities =
		MAC_SYM_PAUSE | MAC_ASYM_PAUSE | MAC_10000FD;

	tgec = mac_dev->fman_mac;
	tgec->cfg->max_frame_length = fman_get_max_frm();
	err = tgec_init(tgec);
	if (err < 0)
		goto _return_fm_mac_free;

	 
	err = tgec_set_exception(tgec, FM_MAC_EX_10G_TX_ECC_ER, false);
	if (err < 0)
		goto _return_fm_mac_free;

	pr_info("FMan XGEC version: 0x%08x\n",
		ioread32be(&tgec->regs->tgec_id));
	goto _return;

_return_fm_mac_free:
	tgec_free(mac_dev->fman_mac);

_return:
	return err;
}
