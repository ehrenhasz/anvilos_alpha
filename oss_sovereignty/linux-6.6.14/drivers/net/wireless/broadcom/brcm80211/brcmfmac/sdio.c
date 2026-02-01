
 

#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/printk.h>
#include <linux/pci_ids.h>
#include <linux/netdevice.h>
#include <linux/interrupt.h>
#include <linux/sched/signal.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/semaphore.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/bcma/bcma.h>
#include <linux/debugfs.h>
#include <linux/vmalloc.h>
#include <asm/unaligned.h>
#include <defs.h>
#include <brcmu_wifi.h>
#include <brcmu_utils.h>
#include <brcm_hw_ids.h>
#include <soc.h>
#include "sdio.h"
#include "chip.h"
#include "firmware.h"
#include "core.h"
#include "common.h"
#include "bcdc.h"

#define DCMD_RESP_TIMEOUT	msecs_to_jiffies(2500)
#define CTL_DONE_TIMEOUT	msecs_to_jiffies(2500)

 
#define DEFAULT_F2_WATERMARK    0x8
#define CY_4373_F2_WATERMARK    0x40
#define CY_4373_F1_MESBUSYCTRL  (CY_4373_F2_WATERMARK | SBSDIO_MESBUSYCTRL_ENAB)
#define CY_43012_F2_WATERMARK    0x60
#define CY_43012_MES_WATERMARK  0x50
#define CY_43012_MESBUSYCTRL    (CY_43012_MES_WATERMARK | \
				 SBSDIO_MESBUSYCTRL_ENAB)
#define CY_4339_F2_WATERMARK    48
#define CY_4339_MES_WATERMARK	80
#define CY_4339_MESBUSYCTRL	(CY_4339_MES_WATERMARK | \
				 SBSDIO_MESBUSYCTRL_ENAB)
#define CY_43455_F2_WATERMARK	0x60
#define CY_43455_MES_WATERMARK	0x50
#define CY_43455_MESBUSYCTRL	(CY_43455_MES_WATERMARK | \
				 SBSDIO_MESBUSYCTRL_ENAB)
#define CY_435X_F2_WATERMARK	0x40
#define CY_435X_F1_MESBUSYCTRL	(CY_435X_F2_WATERMARK | \
				 SBSDIO_MESBUSYCTRL_ENAB)

#ifdef DEBUG

#define BRCMF_TRAP_INFO_SIZE	80

#define CBUF_LEN	(128)

 
#define CONSOLE_BUFFER_MAX	2024

struct rte_log_le {
	__le32 buf;		 
	__le32 buf_size;
	__le32 idx;
	char *_buf_compat;	 
};

struct rte_console {
	 
	uint vcons_in;
	uint vcons_out;

	 
	struct rte_log_le log_le;

	 
	uint cbuf_idx;
	char cbuf[CBUF_LEN];
};

#endif				 
#include <chipcommon.h>

#include "bus.h"
#include "debug.h"
#include "tracepoint.h"

#define TXQLEN		2048	 
#define TXHI		(TXQLEN - 256)	 
#define TXLOW		(TXHI - 256)	 
#define PRIOMASK	7

#define TXRETRIES	2	 

#define BRCMF_RXBOUND	50	 

#define BRCMF_TXBOUND	20	 

#define BRCMF_TXMINMAX	1	 

#define MEMBLOCK	2048	 
#define MAX_DATA_BUF	(32 * 1024)	 

#define BRCMF_FIRSTREAD	(1 << 6)

 

 
#define SBSDIO_DEVCTL_SETBUSY		0x01
 
#define SBSDIO_DEVCTL_SPI_INTR_SYNC	0x02
 
#define SBSDIO_DEVCTL_CA_INT_ONLY	0x04
 
#define SBSDIO_DEVCTL_PADS_ISO		0x08
 
#define SBSDIO_DEVCTL_F2WM_ENAB		0x10
 
#define SBSDIO_DEVCTL_SB_RST_CTL	0x30
 
#define SBSDIO_DEVCTL_RST_CORECTL	0x00
 
#define SBSDIO_DEVCTL_RST_BPRESET	0x10
 
#define SBSDIO_DEVCTL_RST_NOBPRESET	0x20

 

 
#define SBSDIO_CIS_BASE_COMMON		0x1000
 
#define SBSDIO_CIS_SIZE_LIMIT		0x200
 
#define SBSDIO_CIS_OFT_ADDR_MASK	0x1FFFF

 
#define SBSDIO_CIS_MANFID_TUPLE_LEN	6

#define SD_REG(field) \
		(offsetof(struct sdpcmd_regs, field))

 
 
#define SBSDIO_FORCE_ALP		0x01
 
#define SBSDIO_FORCE_HT			0x02
 
#define SBSDIO_FORCE_ILP		0x04
 
#define SBSDIO_ALP_AVAIL_REQ		0x08
 
#define SBSDIO_HT_AVAIL_REQ		0x10
 
#define SBSDIO_FORCE_HW_CLKREQ_OFF	0x20
 
#define SBSDIO_ALP_AVAIL		0x40
 
#define SBSDIO_HT_AVAIL			0x80
#define SBSDIO_CSR_MASK			0x1F
#define SBSDIO_AVBITS		(SBSDIO_HT_AVAIL | SBSDIO_ALP_AVAIL)
#define SBSDIO_ALPAV(regval)	((regval) & SBSDIO_AVBITS)
#define SBSDIO_HTAV(regval)	(((regval) & SBSDIO_AVBITS) == SBSDIO_AVBITS)
#define SBSDIO_ALPONLY(regval)	(SBSDIO_ALPAV(regval) && !SBSDIO_HTAV(regval))
#define SBSDIO_CLKAV(regval, alponly) \
	(SBSDIO_ALPAV(regval) && (alponly ? 1 : SBSDIO_HTAV(regval)))

 
#define I_SMB_SW0	(1 << 0)	 
#define I_SMB_SW1	(1 << 1)	 
#define I_SMB_SW2	(1 << 2)	 
#define I_SMB_SW3	(1 << 3)	 
#define I_SMB_SW_MASK	0x0000000f	 
#define I_SMB_SW_SHIFT	0	 
#define I_HMB_SW0	(1 << 4)	 
#define I_HMB_SW1	(1 << 5)	 
#define I_HMB_SW2	(1 << 6)	 
#define I_HMB_SW3	(1 << 7)	 
#define I_HMB_SW_MASK	0x000000f0	 
#define I_HMB_SW_SHIFT	4	 
#define I_WR_OOSYNC	(1 << 8)	 
#define I_RD_OOSYNC	(1 << 9)	 
#define	I_PC		(1 << 10)	 
#define	I_PD		(1 << 11)	 
#define	I_DE		(1 << 12)	 
#define	I_RU		(1 << 13)	 
#define	I_RO		(1 << 14)	 
#define	I_XU		(1 << 15)	 
#define	I_RI		(1 << 16)	 
#define I_BUSPWR	(1 << 17)	 
#define I_XMTDATA_AVAIL (1 << 23)	 
#define	I_XI		(1 << 24)	 
#define I_RF_TERM	(1 << 25)	 
#define I_WF_TERM	(1 << 26)	 
#define I_PCMCIA_XU	(1 << 27)	 
#define I_SBINT		(1 << 28)	 
#define I_CHIPACTIVE	(1 << 29)	 
#define I_SRESET	(1 << 30)	 
#define I_IOE2		(1U << 31)	 
#define	I_ERRORS	(I_PC | I_PD | I_DE | I_RU | I_RO | I_XU)
#define I_DMA		(I_RI | I_XI | I_ERRORS)

 
#define CC_CISRDY		(1 << 0)	 
#define CC_BPRESEN		(1 << 1)	 
#define CC_F2RDY		(1 << 2)	 
#define CC_CLRPADSISO		(1 << 3)	 
#define CC_XMTDATAAVAIL_MODE	(1 << 4)
#define CC_XMTDATAAVAIL_CTRL	(1 << 5)

 
#define SFC_RF_TERM	(1 << 0)	 
#define SFC_WF_TERM	(1 << 1)	 
#define SFC_CRC4WOOS	(1 << 2)	 
#define SFC_ABORTALL	(1 << 3)	 

 

 
#define SMB_NAK		(1 << 0)	 
#define SMB_INT_ACK	(1 << 1)	 
#define SMB_USE_OOB	(1 << 2)	 
#define SMB_DEV_INT	(1 << 3)	 

 
#define SMB_DATA_VERSION_SHIFT	16	 

 

 
#define I_HMB_FC_STATE	I_HMB_SW0	 
#define I_HMB_FC_CHANGE	I_HMB_SW1	 
#define I_HMB_FRAME_IND	I_HMB_SW2	 
#define I_HMB_HOST_INT	I_HMB_SW3	 

 
#define HMB_DATA_NAKHANDLED	0x0001	 
#define HMB_DATA_DEVREADY	0x0002	 
#define HMB_DATA_FC		0x0004	 
#define HMB_DATA_FWREADY	0x0008	 
#define HMB_DATA_FWHALT		0x0010	 

#define HMB_DATA_FCDATA_MASK	0xff000000
#define HMB_DATA_FCDATA_SHIFT	24

#define HMB_DATA_VERSION_MASK	0x00ff0000
#define HMB_DATA_VERSION_SHIFT	16

 

 
#define SDPCM_PROT_VERSION	4

 
#define SDPCM_SHARED_VERSION       0x0003
#define SDPCM_SHARED_VERSION_MASK  0x00FF
#define SDPCM_SHARED_ASSERT_BUILT  0x0100
#define SDPCM_SHARED_ASSERT        0x0200
#define SDPCM_SHARED_TRAP          0x0400

 
#define MAX_HDR_READ	(1 << 6)
#define MAX_RX_DATASZ	2048

 
#undef PMU_MAX_TRANSITION_DLY
#define PMU_MAX_TRANSITION_DLY 1000000

 
#define BRCMF_INIT_CLKCTL1	(SBSDIO_FORCE_HW_CLKREQ_OFF |	\
					SBSDIO_ALP_AVAIL_REQ)

 
#define F2SYNC	(SDIO_REQ_4BYTE | SDIO_REQ_FIXED)

#define BRCMF_IDLE_ACTIVE	0	 
#define BRCMF_IDLE_INTERVAL	1

#define KSO_WAIT_US 50
#define MAX_KSO_ATTEMPTS (PMU_MAX_TRANSITION_DLY/KSO_WAIT_US)
#define BRCMF_SDIO_MAX_ACCESS_ERRORS	5

#ifdef DEBUG
 
struct brcmf_console {
	uint count;		 
	uint log_addr;		 
	struct rte_log_le log_le;	 
	uint bufsize;		 
	u8 *buf;		 
	uint last;		 
};

struct brcmf_trap_info {
	__le32		type;
	__le32		epc;
	__le32		cpsr;
	__le32		spsr;
	__le32		r0;	 
	__le32		r1;	 
	__le32		r2;	 
	__le32		r3;	 
	__le32		r4;	 
	__le32		r5;	 
	__le32		r6;	 
	__le32		r7;	 
	__le32		r8;	 
	__le32		r9;	 
	__le32		r10;	 
	__le32		r11;	 
	__le32		r12;	 
	__le32		r13;	 
	__le32		r14;	 
	__le32		pc;	 
};
#endif				 

struct sdpcm_shared {
	u32 flags;
	u32 trap_addr;
	u32 assert_exp_addr;
	u32 assert_file_addr;
	u32 assert_line;
	u32 console_addr;	 
	u32 msgtrace_addr;
	u8 tag[32];
	u32 brpt_addr;
};

struct sdpcm_shared_le {
	__le32 flags;
	__le32 trap_addr;
	__le32 assert_exp_addr;
	__le32 assert_file_addr;
	__le32 assert_line;
	__le32 console_addr;	 
	__le32 msgtrace_addr;
	u8 tag[32];
	__le32 brpt_addr;
};

 
struct brcmf_sdio_hdrinfo {
	u8 seq_num;
	u8 channel;
	u16 len;
	u16 len_left;
	u16 len_nxtfrm;
	u8 dat_offset;
	bool lastfrm;
	u16 tail_pad;
};

 
struct brcmf_sdio_count {
	uint intrcount;		 
	uint lastintrs;		 
	uint pollcnt;		 
	uint regfails;		 
	uint tx_sderrs;		 
	uint fcqueued;		 
	uint rxrtx;		 
	uint rx_toolong;	 
	uint rxc_errors;	 
	uint rx_hdrfail;	 
	uint rx_badhdr;		 
	uint rx_badseq;		 
	uint fc_rcvd;		 
	uint fc_xoff;		 
	uint fc_xon;		 
	uint rxglomfail;	 
	uint rxglomframes;	 
	uint rxglompkts;	 
	uint f2rxhdrs;		 
	uint f2rxdata;		 
	uint f2txdata;		 
	uint f1regdata;		 
	uint tickcnt;		 
	ulong tx_ctlerrs;	 
	ulong tx_ctlpkts;	 
	ulong rx_ctlerrs;	 
	ulong rx_ctlpkts;	 
	ulong rx_readahead_cnt;	 
};

 
 
struct brcmf_sdio {
	struct brcmf_sdio_dev *sdiodev;	 
	struct brcmf_chip *ci;	 
	struct brcmf_core *sdio_core;  

	u32 hostintmask;	 
	atomic_t intstatus;	 
	atomic_t fcstate;	 

	uint blocksize;		 
	uint roundup;		 

	struct pktq txq;	 
	u8 flowcontrol;	 
	u8 tx_seq;		 
	u8 tx_max;		 

	u8 *hdrbuf;		 
	u8 *rxhdr;		 
	u8 rx_seq;		 
	struct brcmf_sdio_hdrinfo cur_read;
				 
	bool rxskip;		 
	bool rxpending;		 

	uint rxbound;		 
	uint txbound;		 
	uint txminmax;

	struct sk_buff *glomd;	 
	struct sk_buff_head glom;  

	u8 *rxbuf;		 
	uint rxblen;		 
	u8 *rxctl;		 
	u8 *rxctl_orig;		 
	uint rxlen;		 
	spinlock_t rxctl_lock;	 

	u8 sdpcm_ver;	 

	bool intr;		 
	bool poll;		 
	atomic_t ipend;		 
	uint spurious;		 
	uint pollrate;		 
	uint polltick;		 

#ifdef DEBUG
	uint console_interval;
	struct brcmf_console console;	 
	uint console_addr;	 
#endif				 

	uint clkstate;		 
	s32 idletime;		 
	s32 idlecount;		 
	s32 idleclock;		 
	bool rxflow_mode;	 
	bool rxflow;		 
	bool alp_only;		 

	u8 *ctrl_frame_buf;
	u16 ctrl_frame_len;
	bool ctrl_frame_stat;
	int ctrl_frame_err;

	spinlock_t txq_lock;		 
	wait_queue_head_t ctrl_wait;
	wait_queue_head_t dcmd_resp_wait;

	struct timer_list timer;
	struct completion watchdog_wait;
	struct task_struct *watchdog_tsk;
	bool wd_active;

	struct workqueue_struct *brcmf_wq;
	struct work_struct datawork;
	bool dpc_triggered;
	bool dpc_running;

	bool txoff;		 
	struct brcmf_sdio_count sdcnt;
	bool sr_enabled;  
	bool sleeping;

	u8 tx_hdrlen;		 
	bool txglom;		 
	u16 head_align;		 
	u16 sgentry_align;	 
};

 
#define CLK_NONE	0
#define CLK_SDONLY	1
#define CLK_PENDING	2
#define CLK_AVAIL	3

#ifdef DEBUG
static int qcount[NUMPRIO];
#endif				 

#define DEFAULT_SDIO_DRIVE_STRENGTH	6	 

#define RETRYCHAN(chan) ((chan) == SDPCM_EVENT_CHANNEL)

 
static const uint max_roundup = 512;

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
#define ALIGNMENT  8
#else
#define ALIGNMENT  4
#endif

enum brcmf_sdio_frmtype {
	BRCMF_SDIO_FT_NORMAL,
	BRCMF_SDIO_FT_SUPER,
	BRCMF_SDIO_FT_SUB,
};

#define SDIOD_DRVSTR_KEY(chip, pmu)     (((unsigned int)(chip) << 16) | (pmu))

 
struct sdiod_drive_str {
	u8 strength;	 
	u8 sel;		 
};

 
static const struct sdiod_drive_str sdiod_drvstr_tab1_1v8[] = {
	{32, 0x6},
	{26, 0x7},
	{22, 0x4},
	{16, 0x5},
	{12, 0x2},
	{8, 0x3},
	{4, 0x0},
	{0, 0x1}
};

 
static const struct sdiod_drive_str sdiod_drive_strength_tab5_1v8[] = {
	{6, 0x7},
	{5, 0x6},
	{4, 0x5},
	{3, 0x4},
	{2, 0x2},
	{1, 0x1},
	{0, 0x0}
};

 
static const struct sdiod_drive_str sdiod_drvstr_tab6_1v8[] = {
	{3, 0x3},
	{2, 0x2},
	{1, 0x1},
	{0, 0x0} };

 
static const struct sdiod_drive_str sdiod_drvstr_tab2_3v3[] = {
	{16, 0x7},
	{12, 0x5},
	{8,  0x3},
	{4,  0x1}
};

BRCMF_FW_DEF(43143, "brcmfmac43143-sdio");
BRCMF_FW_DEF(43241B0, "brcmfmac43241b0-sdio");
BRCMF_FW_DEF(43241B4, "brcmfmac43241b4-sdio");
BRCMF_FW_DEF(43241B5, "brcmfmac43241b5-sdio");
BRCMF_FW_DEF(4329, "brcmfmac4329-sdio");
BRCMF_FW_DEF(4330, "brcmfmac4330-sdio");
BRCMF_FW_DEF(4334, "brcmfmac4334-sdio");
BRCMF_FW_DEF(43340, "brcmfmac43340-sdio");
BRCMF_FW_DEF(4335, "brcmfmac4335-sdio");
BRCMF_FW_DEF(43362, "brcmfmac43362-sdio");
BRCMF_FW_DEF(4339, "brcmfmac4339-sdio");
BRCMF_FW_DEF(43430A0, "brcmfmac43430a0-sdio");
 
BRCMF_FW_CLM_DEF(43430A1, "brcmfmac43430-sdio");
BRCMF_FW_DEF(43430B0, "brcmfmac43430b0-sdio");
BRCMF_FW_CLM_DEF(43439, "brcmfmac43439-sdio");
BRCMF_FW_CLM_DEF(43455, "brcmfmac43455-sdio");
BRCMF_FW_DEF(43456, "brcmfmac43456-sdio");
BRCMF_FW_CLM_DEF(4354, "brcmfmac4354-sdio");
BRCMF_FW_CLM_DEF(4356, "brcmfmac4356-sdio");
BRCMF_FW_DEF(4359, "brcmfmac4359-sdio");
BRCMF_FW_CLM_DEF(4373, "brcmfmac4373-sdio");
BRCMF_FW_CLM_DEF(43012, "brcmfmac43012-sdio");
BRCMF_FW_CLM_DEF(43752, "brcmfmac43752-sdio");

 
MODULE_FIRMWARE(BRCMF_FW_DEFAULT_PATH "brcmfmac*-sdio.*.txt");

 
MODULE_FIRMWARE(BRCMF_FW_DEFAULT_PATH "brcmfmac*-sdio.*.bin");

static const struct brcmf_firmware_mapping brcmf_sdio_fwnames[] = {
	BRCMF_FW_ENTRY(BRCM_CC_43143_CHIP_ID, 0xFFFFFFFF, 43143),
	BRCMF_FW_ENTRY(BRCM_CC_43241_CHIP_ID, 0x0000001F, 43241B0),
	BRCMF_FW_ENTRY(BRCM_CC_43241_CHIP_ID, 0x00000020, 43241B4),
	BRCMF_FW_ENTRY(BRCM_CC_43241_CHIP_ID, 0xFFFFFFC0, 43241B5),
	BRCMF_FW_ENTRY(BRCM_CC_4329_CHIP_ID, 0xFFFFFFFF, 4329),
	BRCMF_FW_ENTRY(BRCM_CC_4330_CHIP_ID, 0xFFFFFFFF, 4330),
	BRCMF_FW_ENTRY(BRCM_CC_4334_CHIP_ID, 0xFFFFFFFF, 4334),
	BRCMF_FW_ENTRY(BRCM_CC_43340_CHIP_ID, 0xFFFFFFFF, 43340),
	BRCMF_FW_ENTRY(BRCM_CC_43341_CHIP_ID, 0xFFFFFFFF, 43340),
	BRCMF_FW_ENTRY(BRCM_CC_4335_CHIP_ID, 0xFFFFFFFF, 4335),
	BRCMF_FW_ENTRY(BRCM_CC_43362_CHIP_ID, 0xFFFFFFFE, 43362),
	BRCMF_FW_ENTRY(BRCM_CC_4339_CHIP_ID, 0xFFFFFFFF, 4339),
	BRCMF_FW_ENTRY(BRCM_CC_43430_CHIP_ID, 0x00000001, 43430A0),
	BRCMF_FW_ENTRY(BRCM_CC_43430_CHIP_ID, 0x00000002, 43430A1),
	BRCMF_FW_ENTRY(BRCM_CC_43430_CHIP_ID, 0xFFFFFFFC, 43430B0),
	BRCMF_FW_ENTRY(BRCM_CC_4345_CHIP_ID, 0x00000200, 43456),
	BRCMF_FW_ENTRY(BRCM_CC_4345_CHIP_ID, 0xFFFFFDC0, 43455),
	BRCMF_FW_ENTRY(BRCM_CC_43454_CHIP_ID, 0x00000040, 43455),
	BRCMF_FW_ENTRY(BRCM_CC_4354_CHIP_ID, 0xFFFFFFFF, 4354),
	BRCMF_FW_ENTRY(BRCM_CC_4356_CHIP_ID, 0xFFFFFFFF, 4356),
	BRCMF_FW_ENTRY(BRCM_CC_4359_CHIP_ID, 0xFFFFFFFF, 4359),
	BRCMF_FW_ENTRY(CY_CC_4373_CHIP_ID, 0xFFFFFFFF, 4373),
	BRCMF_FW_ENTRY(CY_CC_43012_CHIP_ID, 0xFFFFFFFF, 43012),
	BRCMF_FW_ENTRY(CY_CC_43439_CHIP_ID, 0xFFFFFFFF, 43439),
	BRCMF_FW_ENTRY(CY_CC_43752_CHIP_ID, 0xFFFFFFFF, 43752)
};

#define TXCTL_CREDITS	2

static void pkt_align(struct sk_buff *p, int len, int align)
{
	uint datalign;
	datalign = (unsigned long)(p->data);
	datalign = roundup(datalign, (align)) - datalign;
	if (datalign)
		skb_pull(p, datalign);
	__skb_trim(p, len);
}

 
static bool data_ok(struct brcmf_sdio *bus)
{
	u8 tx_rsv = 0;

	 
	if (bus->ctrl_frame_stat)
		tx_rsv = TXCTL_CREDITS;

	return (bus->tx_max - bus->tx_seq - tx_rsv) != 0 &&
	       ((bus->tx_max - bus->tx_seq - tx_rsv) & 0x80) == 0;

}

 
static bool txctl_ok(struct brcmf_sdio *bus)
{
	return (bus->tx_max - bus->tx_seq) != 0 &&
	       ((bus->tx_max - bus->tx_seq) & 0x80) == 0;
}

static int
brcmf_sdio_kso_control(struct brcmf_sdio *bus, bool on)
{
	u8 wr_val = 0, rd_val, cmp_val, bmask;
	int err = 0;
	int err_cnt = 0;
	int try_cnt = 0;

	brcmf_dbg(TRACE, "Enter: on=%d\n", on);

	sdio_retune_crc_disable(bus->sdiodev->func1);

	 
	if (on)
		sdio_retune_hold_now(bus->sdiodev->func1);

	wr_val = (on << SBSDIO_FUNC1_SLEEPCSR_KSO_SHIFT);
	 
	brcmf_sdiod_writeb(bus->sdiodev, SBSDIO_FUNC1_SLEEPCSR, wr_val, &err);

	 
	if (!on && bus->ci->chip == CY_CC_43012_CHIP_ID)
		return err;

	if (on) {
		 
		cmp_val = SBSDIO_FUNC1_SLEEPCSR_KSO_MASK |
			  SBSDIO_FUNC1_SLEEPCSR_DEVON_MASK;
		bmask = cmp_val;
		usleep_range(2000, 3000);
	} else {
		 
		cmp_val = 0;
		 
		bmask = SBSDIO_FUNC1_SLEEPCSR_KSO_MASK;
	}

	do {
		 
		rd_val = brcmf_sdiod_readb(bus->sdiodev, SBSDIO_FUNC1_SLEEPCSR,
					   &err);
		if (!err) {
			if ((rd_val & bmask) == cmp_val)
				break;
			err_cnt = 0;
		}
		 
		if (err && (err_cnt++ > BRCMF_SDIO_MAX_ACCESS_ERRORS))
			break;

		udelay(KSO_WAIT_US);
		brcmf_sdiod_writeb(bus->sdiodev, SBSDIO_FUNC1_SLEEPCSR, wr_val,
				   &err);

	} while (try_cnt++ < MAX_KSO_ATTEMPTS);

	if (try_cnt > 2)
		brcmf_dbg(SDIO, "try_cnt=%d rd_val=0x%x err=%d\n", try_cnt,
			  rd_val, err);

	if (try_cnt > MAX_KSO_ATTEMPTS)
		brcmf_err("max tries: rd_val=0x%x err=%d\n", rd_val, err);

	if (on)
		sdio_retune_release(bus->sdiodev->func1);

	sdio_retune_crc_enable(bus->sdiodev->func1);

	return err;
}

#define HOSTINTMASK		(I_HMB_SW_MASK | I_CHIPACTIVE)

 
static int brcmf_sdio_htclk(struct brcmf_sdio *bus, bool on, bool pendok)
{
	int err;
	u8 clkctl, clkreq, devctl;
	unsigned long timeout;

	brcmf_dbg(SDIO, "Enter\n");

	clkctl = 0;

	if (bus->sr_enabled) {
		bus->clkstate = (on ? CLK_AVAIL : CLK_SDONLY);
		return 0;
	}

	if (on) {
		 
		clkreq =
		    bus->alp_only ? SBSDIO_ALP_AVAIL_REQ : SBSDIO_HT_AVAIL_REQ;

		brcmf_sdiod_writeb(bus->sdiodev, SBSDIO_FUNC1_CHIPCLKCSR,
				   clkreq, &err);
		if (err) {
			brcmf_err("HT Avail request error: %d\n", err);
			return -EBADE;
		}

		 
		clkctl = brcmf_sdiod_readb(bus->sdiodev,
					   SBSDIO_FUNC1_CHIPCLKCSR, &err);
		if (err) {
			brcmf_err("HT Avail read error: %d\n", err);
			return -EBADE;
		}

		 
		if (!SBSDIO_CLKAV(clkctl, bus->alp_only) && pendok) {
			 
			devctl = brcmf_sdiod_readb(bus->sdiodev,
						   SBSDIO_DEVICE_CTL, &err);
			if (err) {
				brcmf_err("Devctl error setting CA: %d\n", err);
				return -EBADE;
			}

			devctl |= SBSDIO_DEVCTL_CA_INT_ONLY;
			brcmf_sdiod_writeb(bus->sdiodev, SBSDIO_DEVICE_CTL,
					   devctl, &err);
			brcmf_dbg(SDIO, "CLKCTL: set PENDING\n");
			bus->clkstate = CLK_PENDING;

			return 0;
		} else if (bus->clkstate == CLK_PENDING) {
			 
			devctl = brcmf_sdiod_readb(bus->sdiodev,
						   SBSDIO_DEVICE_CTL, &err);
			devctl &= ~SBSDIO_DEVCTL_CA_INT_ONLY;
			brcmf_sdiod_writeb(bus->sdiodev, SBSDIO_DEVICE_CTL,
					   devctl, &err);
		}

		 
		timeout = jiffies +
			  msecs_to_jiffies(PMU_MAX_TRANSITION_DLY/1000);
		while (!SBSDIO_CLKAV(clkctl, bus->alp_only)) {
			clkctl = brcmf_sdiod_readb(bus->sdiodev,
						   SBSDIO_FUNC1_CHIPCLKCSR,
						   &err);
			if (time_after(jiffies, timeout))
				break;
			else
				usleep_range(5000, 10000);
		}
		if (err) {
			brcmf_err("HT Avail request error: %d\n", err);
			return -EBADE;
		}
		if (!SBSDIO_CLKAV(clkctl, bus->alp_only)) {
			brcmf_err("HT Avail timeout (%d): clkctl 0x%02x\n",
				  PMU_MAX_TRANSITION_DLY, clkctl);
			return -EBADE;
		}

		 
		bus->clkstate = CLK_AVAIL;
		brcmf_dbg(SDIO, "CLKCTL: turned ON\n");

#if defined(DEBUG)
		if (!bus->alp_only) {
			if (SBSDIO_ALPONLY(clkctl))
				brcmf_err("HT Clock should be on\n");
		}
#endif				 

	} else {
		clkreq = 0;

		if (bus->clkstate == CLK_PENDING) {
			 
			devctl = brcmf_sdiod_readb(bus->sdiodev,
						   SBSDIO_DEVICE_CTL, &err);
			devctl &= ~SBSDIO_DEVCTL_CA_INT_ONLY;
			brcmf_sdiod_writeb(bus->sdiodev, SBSDIO_DEVICE_CTL,
					   devctl, &err);
		}

		bus->clkstate = CLK_SDONLY;
		brcmf_sdiod_writeb(bus->sdiodev, SBSDIO_FUNC1_CHIPCLKCSR,
				   clkreq, &err);
		brcmf_dbg(SDIO, "CLKCTL: turned OFF\n");
		if (err) {
			brcmf_err("Failed access turning clock off: %d\n",
				  err);
			return -EBADE;
		}
	}
	return 0;
}

 
static int brcmf_sdio_sdclk(struct brcmf_sdio *bus, bool on)
{
	brcmf_dbg(SDIO, "Enter\n");

	if (on)
		bus->clkstate = CLK_SDONLY;
	else
		bus->clkstate = CLK_NONE;

	return 0;
}

 
static int brcmf_sdio_clkctl(struct brcmf_sdio *bus, uint target, bool pendok)
{
#ifdef DEBUG
	uint oldstate = bus->clkstate;
#endif				 

	brcmf_dbg(SDIO, "Enter\n");

	 
	if (bus->clkstate == target)
		return 0;

	switch (target) {
	case CLK_AVAIL:
		 
		if (bus->clkstate == CLK_NONE)
			brcmf_sdio_sdclk(bus, true);
		 
		brcmf_sdio_htclk(bus, true, pendok);
		break;

	case CLK_SDONLY:
		 
		if (bus->clkstate == CLK_NONE)
			brcmf_sdio_sdclk(bus, true);
		else if (bus->clkstate == CLK_AVAIL)
			brcmf_sdio_htclk(bus, false, false);
		else
			brcmf_err("request for %d -> %d\n",
				  bus->clkstate, target);
		break;

	case CLK_NONE:
		 
		if (bus->clkstate == CLK_AVAIL)
			brcmf_sdio_htclk(bus, false, false);
		 
		brcmf_sdio_sdclk(bus, false);
		break;
	}
#ifdef DEBUG
	brcmf_dbg(SDIO, "%d -> %d\n", oldstate, bus->clkstate);
#endif				 

	return 0;
}

static int
brcmf_sdio_bus_sleep(struct brcmf_sdio *bus, bool sleep, bool pendok)
{
	int err = 0;
	u8 clkcsr;

	brcmf_dbg(SDIO, "Enter: request %s currently %s\n",
		  (sleep ? "SLEEP" : "WAKE"),
		  (bus->sleeping ? "SLEEP" : "WAKE"));

	 
	if (bus->sr_enabled) {
		 
		if (sleep == bus->sleeping)
			goto end;

		 
		if (sleep) {
			clkcsr = brcmf_sdiod_readb(bus->sdiodev,
						   SBSDIO_FUNC1_CHIPCLKCSR,
						   &err);
			if ((clkcsr & SBSDIO_CSR_MASK) == 0) {
				brcmf_dbg(SDIO, "no clock, set ALP\n");
				brcmf_sdiod_writeb(bus->sdiodev,
						   SBSDIO_FUNC1_CHIPCLKCSR,
						   SBSDIO_ALP_AVAIL_REQ, &err);
			}
			err = brcmf_sdio_kso_control(bus, false);
		} else {
			err = brcmf_sdio_kso_control(bus, true);
		}
		if (err) {
			brcmf_err("error while changing bus sleep state %d\n",
				  err);
			goto done;
		}
	}

end:
	 
	if (sleep) {
		if (!bus->sr_enabled)
			brcmf_sdio_clkctl(bus, CLK_NONE, pendok);
	} else {
		brcmf_sdio_clkctl(bus, CLK_AVAIL, pendok);
		brcmf_sdio_wd_timer(bus, true);
	}
	bus->sleeping = sleep;
	brcmf_dbg(SDIO, "new state %s\n",
		  (sleep ? "SLEEP" : "WAKE"));
done:
	brcmf_dbg(SDIO, "Exit: err=%d\n", err);
	return err;

}

#ifdef DEBUG
static inline bool brcmf_sdio_valid_shared_address(u32 addr)
{
	return !(addr == 0 || ((~addr >> 16) & 0xffff) == (addr & 0xffff));
}

static int brcmf_sdio_readshared(struct brcmf_sdio *bus,
				 struct sdpcm_shared *sh)
{
	u32 addr = 0;
	int rv;
	u32 shaddr = 0;
	struct sdpcm_shared_le sh_le;
	__le32 addr_le;

	sdio_claim_host(bus->sdiodev->func1);
	brcmf_sdio_bus_sleep(bus, false, false);

	 
	shaddr = bus->ci->rambase + bus->ci->ramsize - 4;
	if (!bus->ci->rambase && brcmf_chip_sr_capable(bus->ci))
		shaddr -= bus->ci->srsize;
	rv = brcmf_sdiod_ramrw(bus->sdiodev, false, shaddr,
			       (u8 *)&addr_le, 4);
	if (rv < 0)
		goto fail;

	 
	addr = le32_to_cpu(addr_le);
	if (!brcmf_sdio_valid_shared_address(addr)) {
		brcmf_err("invalid sdpcm_shared address 0x%08X\n", addr);
		rv = -EINVAL;
		goto fail;
	}

	brcmf_dbg(INFO, "sdpcm_shared address 0x%08X\n", addr);

	 
	rv = brcmf_sdiod_ramrw(bus->sdiodev, false, addr, (u8 *)&sh_le,
			       sizeof(struct sdpcm_shared_le));
	if (rv < 0)
		goto fail;

	sdio_release_host(bus->sdiodev->func1);

	 
	sh->flags = le32_to_cpu(sh_le.flags);
	sh->trap_addr = le32_to_cpu(sh_le.trap_addr);
	sh->assert_exp_addr = le32_to_cpu(sh_le.assert_exp_addr);
	sh->assert_file_addr = le32_to_cpu(sh_le.assert_file_addr);
	sh->assert_line = le32_to_cpu(sh_le.assert_line);
	sh->console_addr = le32_to_cpu(sh_le.console_addr);
	sh->msgtrace_addr = le32_to_cpu(sh_le.msgtrace_addr);

	if ((sh->flags & SDPCM_SHARED_VERSION_MASK) > SDPCM_SHARED_VERSION) {
		brcmf_err("sdpcm shared version unsupported: dhd %d dongle %d\n",
			  SDPCM_SHARED_VERSION,
			  sh->flags & SDPCM_SHARED_VERSION_MASK);
		return -EPROTO;
	}
	return 0;

fail:
	brcmf_err("unable to obtain sdpcm_shared info: rv=%d (addr=0x%x)\n",
		  rv, addr);
	sdio_release_host(bus->sdiodev->func1);
	return rv;
}

static void brcmf_sdio_get_console_addr(struct brcmf_sdio *bus)
{
	struct sdpcm_shared sh;

	if (brcmf_sdio_readshared(bus, &sh) == 0)
		bus->console_addr = sh.console_addr;
}
#else
static void brcmf_sdio_get_console_addr(struct brcmf_sdio *bus)
{
}
#endif  

static u32 brcmf_sdio_hostmail(struct brcmf_sdio *bus)
{
	struct brcmf_sdio_dev *sdiod = bus->sdiodev;
	struct brcmf_core *core = bus->sdio_core;
	u32 intstatus = 0;
	u32 hmb_data;
	u8 fcbits;
	int ret;

	brcmf_dbg(SDIO, "Enter\n");

	 
	hmb_data = brcmf_sdiod_readl(sdiod,
				     core->base + SD_REG(tohostmailboxdata),
				     &ret);

	if (!ret)
		brcmf_sdiod_writel(sdiod, core->base + SD_REG(tosbmailbox),
				   SMB_INT_ACK, &ret);

	bus->sdcnt.f1regdata += 2;

	 
	if (hmb_data & HMB_DATA_FWHALT) {
		brcmf_dbg(SDIO, "mailbox indicates firmware halted\n");
		brcmf_fw_crashed(&sdiod->func1->dev);
	}

	 
	if (hmb_data & HMB_DATA_NAKHANDLED) {
		brcmf_dbg(SDIO, "Dongle reports NAK handled, expect rtx of %d\n",
			  bus->rx_seq);
		if (!bus->rxskip)
			brcmf_err("unexpected NAKHANDLED!\n");

		bus->rxskip = false;
		intstatus |= I_HMB_FRAME_IND;
	}

	 
	if (hmb_data & (HMB_DATA_DEVREADY | HMB_DATA_FWREADY)) {
		bus->sdpcm_ver =
		    (hmb_data & HMB_DATA_VERSION_MASK) >>
		    HMB_DATA_VERSION_SHIFT;
		if (bus->sdpcm_ver != SDPCM_PROT_VERSION)
			brcmf_err("Version mismatch, dongle reports %d, "
				  "expecting %d\n",
				  bus->sdpcm_ver, SDPCM_PROT_VERSION);
		else
			brcmf_dbg(SDIO, "Dongle ready, protocol version %d\n",
				  bus->sdpcm_ver);

		 
		brcmf_sdio_get_console_addr(bus);
	}

	 
	if (hmb_data & HMB_DATA_FC) {
		fcbits = (hmb_data & HMB_DATA_FCDATA_MASK) >>
							HMB_DATA_FCDATA_SHIFT;

		if (fcbits & ~bus->flowcontrol)
			bus->sdcnt.fc_xoff++;

		if (bus->flowcontrol & ~fcbits)
			bus->sdcnt.fc_xon++;

		bus->sdcnt.fc_rcvd++;
		bus->flowcontrol = fcbits;
	}

	 
	if (hmb_data & ~(HMB_DATA_DEVREADY |
			 HMB_DATA_NAKHANDLED |
			 HMB_DATA_FC |
			 HMB_DATA_FWREADY |
			 HMB_DATA_FWHALT |
			 HMB_DATA_FCDATA_MASK | HMB_DATA_VERSION_MASK))
		brcmf_err("Unknown mailbox data content: 0x%02x\n",
			  hmb_data);

	return intstatus;
}

static void brcmf_sdio_rxfail(struct brcmf_sdio *bus, bool abort, bool rtx)
{
	struct brcmf_sdio_dev *sdiod = bus->sdiodev;
	struct brcmf_core *core = bus->sdio_core;
	uint retries = 0;
	u16 lastrbc;
	u8 hi, lo;
	int err;

	brcmf_err("%sterminate frame%s\n",
		  abort ? "abort command, " : "",
		  rtx ? ", send NAK" : "");

	if (abort)
		brcmf_sdiod_abort(bus->sdiodev, bus->sdiodev->func2);

	brcmf_sdiod_writeb(bus->sdiodev, SBSDIO_FUNC1_FRAMECTRL, SFC_RF_TERM,
			   &err);
	bus->sdcnt.f1regdata++;

	 
	for (lastrbc = retries = 0xffff; retries > 0; retries--) {
		hi = brcmf_sdiod_readb(bus->sdiodev, SBSDIO_FUNC1_RFRAMEBCHI,
				       &err);
		lo = brcmf_sdiod_readb(bus->sdiodev, SBSDIO_FUNC1_RFRAMEBCLO,
				       &err);
		bus->sdcnt.f1regdata += 2;

		if ((hi == 0) && (lo == 0))
			break;

		if ((hi > (lastrbc >> 8)) && (lo > (lastrbc & 0x00ff))) {
			brcmf_err("count growing: last 0x%04x now 0x%04x\n",
				  lastrbc, (hi << 8) + lo);
		}
		lastrbc = (hi << 8) + lo;
	}

	if (!retries)
		brcmf_err("count never zeroed: last 0x%04x\n", lastrbc);
	else
		brcmf_dbg(SDIO, "flush took %d iterations\n", 0xffff - retries);

	if (rtx) {
		bus->sdcnt.rxrtx++;
		brcmf_sdiod_writel(sdiod, core->base + SD_REG(tosbmailbox),
				   SMB_NAK, &err);

		bus->sdcnt.f1regdata++;
		if (err == 0)
			bus->rxskip = true;
	}

	 
	bus->cur_read.len = 0;
}

static void brcmf_sdio_txfail(struct brcmf_sdio *bus)
{
	struct brcmf_sdio_dev *sdiodev = bus->sdiodev;
	u8 i, hi, lo;

	 
	brcmf_err("sdio error, abort command and terminate frame\n");
	bus->sdcnt.tx_sderrs++;

	brcmf_sdiod_abort(sdiodev, sdiodev->func2);
	brcmf_sdiod_writeb(sdiodev, SBSDIO_FUNC1_FRAMECTRL, SFC_WF_TERM, NULL);
	bus->sdcnt.f1regdata++;

	for (i = 0; i < 3; i++) {
		hi = brcmf_sdiod_readb(sdiodev, SBSDIO_FUNC1_WFRAMEBCHI, NULL);
		lo = brcmf_sdiod_readb(sdiodev, SBSDIO_FUNC1_WFRAMEBCLO, NULL);
		bus->sdcnt.f1regdata += 2;
		if ((hi == 0) && (lo == 0))
			break;
	}
}

 
static uint brcmf_sdio_glom_len(struct brcmf_sdio *bus)
{
	struct sk_buff *p;
	uint total;

	total = 0;
	skb_queue_walk(&bus->glom, p)
		total += p->len;
	return total;
}

static void brcmf_sdio_free_glom(struct brcmf_sdio *bus)
{
	struct sk_buff *cur, *next;

	skb_queue_walk_safe(&bus->glom, cur, next) {
		skb_unlink(cur, &bus->glom);
		brcmu_pkt_buf_free_skb(cur);
	}
}

 
#define SDPCM_HWHDR_LEN			4
#define SDPCM_HWEXT_LEN			8
#define SDPCM_SWHDR_LEN			8
#define SDPCM_HDRLEN			(SDPCM_HWHDR_LEN + SDPCM_SWHDR_LEN)
 
#define SDPCM_SEQ_MASK			0x000000ff
#define SDPCM_SEQ_WRAP			256
#define SDPCM_CHANNEL_MASK		0x00000f00
#define SDPCM_CHANNEL_SHIFT		8
#define SDPCM_CONTROL_CHANNEL		0	 
#define SDPCM_EVENT_CHANNEL		1	 
#define SDPCM_DATA_CHANNEL		2	 
#define SDPCM_GLOM_CHANNEL		3	 
#define SDPCM_TEST_CHANNEL		15	 
#define SDPCM_GLOMDESC(p)		(((u8 *)p)[1] & 0x80)
#define SDPCM_NEXTLEN_MASK		0x00ff0000
#define SDPCM_NEXTLEN_SHIFT		16
#define SDPCM_DOFFSET_MASK		0xff000000
#define SDPCM_DOFFSET_SHIFT		24
#define SDPCM_FCMASK_MASK		0x000000ff
#define SDPCM_WINDOW_MASK		0x0000ff00
#define SDPCM_WINDOW_SHIFT		8

static inline u8 brcmf_sdio_getdatoffset(u8 *swheader)
{
	u32 hdrvalue;
	hdrvalue = le32_to_cpu(*(__le32 *)swheader);
	return (u8)((hdrvalue & SDPCM_DOFFSET_MASK) >> SDPCM_DOFFSET_SHIFT);
}

static inline bool brcmf_sdio_fromevntchan(u8 *swheader)
{
	u32 hdrvalue;
	u8 ret;

	hdrvalue = le32_to_cpu(*(__le32 *)swheader);
	ret = (u8)((hdrvalue & SDPCM_CHANNEL_MASK) >> SDPCM_CHANNEL_SHIFT);

	return (ret == SDPCM_EVENT_CHANNEL);
}

static int brcmf_sdio_hdparse(struct brcmf_sdio *bus, u8 *header,
			      struct brcmf_sdio_hdrinfo *rd,
			      enum brcmf_sdio_frmtype type)
{
	u16 len, checksum;
	u8 rx_seq, fc, tx_seq_max;
	u32 swheader;

	trace_brcmf_sdpcm_hdr(SDPCM_RX, header);

	 
	len = get_unaligned_le16(header);
	checksum = get_unaligned_le16(header + sizeof(u16));
	 
	if (!(len | checksum)) {
		bus->rxpending = false;
		return -ENODATA;
	}
	if ((u16)(~(len ^ checksum))) {
		brcmf_err("HW header checksum error\n");
		bus->sdcnt.rx_badhdr++;
		brcmf_sdio_rxfail(bus, false, false);
		return -EIO;
	}
	if (len < SDPCM_HDRLEN) {
		brcmf_err("HW header length error\n");
		return -EPROTO;
	}
	if (type == BRCMF_SDIO_FT_SUPER &&
	    (roundup(len, bus->blocksize) != rd->len)) {
		brcmf_err("HW superframe header length error\n");
		return -EPROTO;
	}
	if (type == BRCMF_SDIO_FT_SUB && len > rd->len) {
		brcmf_err("HW subframe header length error\n");
		return -EPROTO;
	}
	rd->len = len;

	 
	header += SDPCM_HWHDR_LEN;
	swheader = le32_to_cpu(*(__le32 *)header);
	if (type == BRCMF_SDIO_FT_SUPER && SDPCM_GLOMDESC(header)) {
		brcmf_err("Glom descriptor found in superframe head\n");
		rd->len = 0;
		return -EINVAL;
	}
	rx_seq = (u8)(swheader & SDPCM_SEQ_MASK);
	rd->channel = (swheader & SDPCM_CHANNEL_MASK) >> SDPCM_CHANNEL_SHIFT;
	if (len > MAX_RX_DATASZ && rd->channel != SDPCM_CONTROL_CHANNEL &&
	    type != BRCMF_SDIO_FT_SUPER) {
		brcmf_err("HW header length too long\n");
		bus->sdcnt.rx_toolong++;
		brcmf_sdio_rxfail(bus, false, false);
		rd->len = 0;
		return -EPROTO;
	}
	if (type == BRCMF_SDIO_FT_SUPER && rd->channel != SDPCM_GLOM_CHANNEL) {
		brcmf_err("Wrong channel for superframe\n");
		rd->len = 0;
		return -EINVAL;
	}
	if (type == BRCMF_SDIO_FT_SUB && rd->channel != SDPCM_DATA_CHANNEL &&
	    rd->channel != SDPCM_EVENT_CHANNEL) {
		brcmf_err("Wrong channel for subframe\n");
		rd->len = 0;
		return -EINVAL;
	}
	rd->dat_offset = brcmf_sdio_getdatoffset(header);
	if (rd->dat_offset < SDPCM_HDRLEN || rd->dat_offset > rd->len) {
		brcmf_err("seq %d: bad data offset\n", rx_seq);
		bus->sdcnt.rx_badhdr++;
		brcmf_sdio_rxfail(bus, false, false);
		rd->len = 0;
		return -ENXIO;
	}
	if (rd->seq_num != rx_seq) {
		brcmf_dbg(SDIO, "seq %d, expected %d\n", rx_seq, rd->seq_num);
		bus->sdcnt.rx_badseq++;
		rd->seq_num = rx_seq;
	}
	 
	if (type == BRCMF_SDIO_FT_SUB)
		return 0;
	rd->len_nxtfrm = (swheader & SDPCM_NEXTLEN_MASK) >> SDPCM_NEXTLEN_SHIFT;
	if (rd->len_nxtfrm << 4 > MAX_RX_DATASZ) {
		 
		if (rd->channel != SDPCM_GLOM_CHANNEL)
			brcmf_err("seq %d: next length error\n", rx_seq);
		rd->len_nxtfrm = 0;
	}
	swheader = le32_to_cpu(*(__le32 *)(header + 4));
	fc = swheader & SDPCM_FCMASK_MASK;
	if (bus->flowcontrol != fc) {
		if (~bus->flowcontrol & fc)
			bus->sdcnt.fc_xoff++;
		if (bus->flowcontrol & ~fc)
			bus->sdcnt.fc_xon++;
		bus->sdcnt.fc_rcvd++;
		bus->flowcontrol = fc;
	}
	tx_seq_max = (swheader & SDPCM_WINDOW_MASK) >> SDPCM_WINDOW_SHIFT;
	if ((u8)(tx_seq_max - bus->tx_seq) > 0x40) {
		brcmf_err("seq %d: max tx seq number error\n", rx_seq);
		tx_seq_max = bus->tx_seq + 2;
	}
	bus->tx_max = tx_seq_max;

	return 0;
}

static inline void brcmf_sdio_update_hwhdr(u8 *header, u16 frm_length)
{
	*(__le16 *)header = cpu_to_le16(frm_length);
	*(((__le16 *)header) + 1) = cpu_to_le16(~frm_length);
}

static void brcmf_sdio_hdpack(struct brcmf_sdio *bus, u8 *header,
			      struct brcmf_sdio_hdrinfo *hd_info)
{
	u32 hdrval;
	u8 hdr_offset;

	brcmf_sdio_update_hwhdr(header, hd_info->len);
	hdr_offset = SDPCM_HWHDR_LEN;

	if (bus->txglom) {
		hdrval = (hd_info->len - hdr_offset) | (hd_info->lastfrm << 24);
		*((__le32 *)(header + hdr_offset)) = cpu_to_le32(hdrval);
		hdrval = (u16)hd_info->tail_pad << 16;
		*(((__le32 *)(header + hdr_offset)) + 1) = cpu_to_le32(hdrval);
		hdr_offset += SDPCM_HWEXT_LEN;
	}

	hdrval = hd_info->seq_num;
	hdrval |= (hd_info->channel << SDPCM_CHANNEL_SHIFT) &
		  SDPCM_CHANNEL_MASK;
	hdrval |= (hd_info->dat_offset << SDPCM_DOFFSET_SHIFT) &
		  SDPCM_DOFFSET_MASK;
	*((__le32 *)(header + hdr_offset)) = cpu_to_le32(hdrval);
	*(((__le32 *)(header + hdr_offset)) + 1) = 0;
	trace_brcmf_sdpcm_hdr(SDPCM_TX + !!(bus->txglom), header);
}

static u8 brcmf_sdio_rxglom(struct brcmf_sdio *bus, u8 rxseq)
{
	u16 dlen, totlen;
	u8 *dptr, num = 0;
	u16 sublen;
	struct sk_buff *pfirst, *pnext;

	int errcode;
	u8 doff;

	struct brcmf_sdio_hdrinfo rd_new;

	 
	 

	brcmf_dbg(SDIO, "start: glomd %p glom %p\n",
		  bus->glomd, skb_peek(&bus->glom));

	 
	if (bus->glomd) {
		pfirst = pnext = NULL;
		dlen = (u16) (bus->glomd->len);
		dptr = bus->glomd->data;
		if (!dlen || (dlen & 1)) {
			brcmf_err("bad glomd len(%d), ignore descriptor\n",
				  dlen);
			dlen = 0;
		}

		for (totlen = num = 0; dlen; num++) {
			 
			sublen = get_unaligned_le16(dptr);
			dlen -= sizeof(u16);
			dptr += sizeof(u16);
			if ((sublen < SDPCM_HDRLEN) ||
			    ((num == 0) && (sublen < (2 * SDPCM_HDRLEN)))) {
				brcmf_err("descriptor len %d bad: %d\n",
					  num, sublen);
				pnext = NULL;
				break;
			}
			if (sublen % bus->sgentry_align) {
				brcmf_err("sublen %d not multiple of %d\n",
					  sublen, bus->sgentry_align);
			}
			totlen += sublen;

			 
			if (!dlen) {
				sublen +=
				    (roundup(totlen, bus->blocksize) - totlen);
				totlen = roundup(totlen, bus->blocksize);
			}

			 
			pnext = brcmu_pkt_buf_get_skb(sublen + bus->sgentry_align);
			if (pnext == NULL) {
				brcmf_err("bcm_pkt_buf_get_skb failed, num %d len %d\n",
					  num, sublen);
				break;
			}
			skb_queue_tail(&bus->glom, pnext);

			 
			pkt_align(pnext, sublen, bus->sgentry_align);
		}

		 
		if (pnext) {
			brcmf_dbg(GLOM, "allocated %d-byte packet chain for %d subframes\n",
				  totlen, num);
			if (BRCMF_GLOM_ON() && bus->cur_read.len &&
			    totlen != bus->cur_read.len) {
				brcmf_dbg(GLOM, "glomdesc mismatch: nextlen %d glomdesc %d rxseq %d\n",
					  bus->cur_read.len, totlen, rxseq);
			}
			pfirst = pnext = NULL;
		} else {
			brcmf_sdio_free_glom(bus);
			num = 0;
		}

		 
		brcmu_pkt_buf_free_skb(bus->glomd);
		bus->glomd = NULL;
		bus->cur_read.len = 0;
	}

	 
	if (!skb_queue_empty(&bus->glom)) {
		if (BRCMF_GLOM_ON()) {
			brcmf_dbg(GLOM, "try superframe read, packet chain:\n");
			skb_queue_walk(&bus->glom, pnext) {
				brcmf_dbg(GLOM, "    %p: %p len 0x%04x (%d)\n",
					  pnext, (u8 *) (pnext->data),
					  pnext->len, pnext->len);
			}
		}

		pfirst = skb_peek(&bus->glom);
		dlen = (u16) brcmf_sdio_glom_len(bus);

		 
		sdio_claim_host(bus->sdiodev->func1);
		errcode = brcmf_sdiod_recv_chain(bus->sdiodev,
						 &bus->glom, dlen);
		sdio_release_host(bus->sdiodev->func1);
		bus->sdcnt.f2rxdata++;

		 
		if (errcode < 0) {
			brcmf_err("glom read of %d bytes failed: %d\n",
				  dlen, errcode);

			sdio_claim_host(bus->sdiodev->func1);
			brcmf_sdio_rxfail(bus, true, false);
			bus->sdcnt.rxglomfail++;
			brcmf_sdio_free_glom(bus);
			sdio_release_host(bus->sdiodev->func1);
			return 0;
		}

		brcmf_dbg_hex_dump(BRCMF_GLOM_ON(),
				   pfirst->data, min_t(int, pfirst->len, 48),
				   "SUPERFRAME:\n");

		rd_new.seq_num = rxseq;
		rd_new.len = dlen;
		sdio_claim_host(bus->sdiodev->func1);
		errcode = brcmf_sdio_hdparse(bus, pfirst->data, &rd_new,
					     BRCMF_SDIO_FT_SUPER);
		sdio_release_host(bus->sdiodev->func1);
		bus->cur_read.len = rd_new.len_nxtfrm << 4;

		 
		skb_pull(pfirst, rd_new.dat_offset);
		num = 0;

		 
		skb_queue_walk(&bus->glom, pnext) {
			 
			if (errcode)
				break;

			rd_new.len = pnext->len;
			rd_new.seq_num = rxseq++;
			sdio_claim_host(bus->sdiodev->func1);
			errcode = brcmf_sdio_hdparse(bus, pnext->data, &rd_new,
						     BRCMF_SDIO_FT_SUB);
			sdio_release_host(bus->sdiodev->func1);
			brcmf_dbg_hex_dump(BRCMF_GLOM_ON(),
					   pnext->data, 32, "subframe:\n");

			num++;
		}

		if (errcode) {
			 
			sdio_claim_host(bus->sdiodev->func1);
			brcmf_sdio_rxfail(bus, true, false);
			bus->sdcnt.rxglomfail++;
			brcmf_sdio_free_glom(bus);
			sdio_release_host(bus->sdiodev->func1);
			bus->cur_read.len = 0;
			return 0;
		}

		 

		skb_queue_walk_safe(&bus->glom, pfirst, pnext) {
			dptr = (u8 *) (pfirst->data);
			sublen = get_unaligned_le16(dptr);
			doff = brcmf_sdio_getdatoffset(&dptr[SDPCM_HWHDR_LEN]);

			brcmf_dbg_hex_dump(BRCMF_BYTES_ON() && BRCMF_DATA_ON(),
					   dptr, pfirst->len,
					   "Rx Subframe Data:\n");

			__skb_trim(pfirst, sublen);
			skb_pull(pfirst, doff);

			if (pfirst->len == 0) {
				skb_unlink(pfirst, &bus->glom);
				brcmu_pkt_buf_free_skb(pfirst);
				continue;
			}

			brcmf_dbg_hex_dump(BRCMF_GLOM_ON(),
					   pfirst->data,
					   min_t(int, pfirst->len, 32),
					   "subframe %d to stack, %p (%p/%d) nxt/lnk %p/%p\n",
					   bus->glom.qlen, pfirst, pfirst->data,
					   pfirst->len, pfirst->next,
					   pfirst->prev);
			skb_unlink(pfirst, &bus->glom);
			if (brcmf_sdio_fromevntchan(&dptr[SDPCM_HWHDR_LEN]))
				brcmf_rx_event(bus->sdiodev->dev, pfirst);
			else
				brcmf_rx_frame(bus->sdiodev->dev, pfirst,
					       false, false);
			bus->sdcnt.rxglompkts++;
		}

		bus->sdcnt.rxglomframes++;
	}
	return num;
}

static int brcmf_sdio_dcmd_resp_wait(struct brcmf_sdio *bus, uint *condition,
				     bool *pending)
{
	DECLARE_WAITQUEUE(wait, current);
	int timeout = DCMD_RESP_TIMEOUT;

	 
	add_wait_queue(&bus->dcmd_resp_wait, &wait);
	set_current_state(TASK_INTERRUPTIBLE);

	while (!(*condition) && (!signal_pending(current) && timeout))
		timeout = schedule_timeout(timeout);

	if (signal_pending(current))
		*pending = true;

	set_current_state(TASK_RUNNING);
	remove_wait_queue(&bus->dcmd_resp_wait, &wait);

	return timeout;
}

static int brcmf_sdio_dcmd_resp_wake(struct brcmf_sdio *bus)
{
	wake_up_interruptible(&bus->dcmd_resp_wait);

	return 0;
}
static void
brcmf_sdio_read_control(struct brcmf_sdio *bus, u8 *hdr, uint len, uint doff)
{
	uint rdlen, pad;
	u8 *buf = NULL, *rbuf;
	int sdret;

	brcmf_dbg(SDIO, "Enter\n");
	if (bus->rxblen)
		buf = vzalloc(bus->rxblen);
	if (!buf)
		goto done;

	rbuf = bus->rxbuf;
	pad = ((unsigned long)rbuf % bus->head_align);
	if (pad)
		rbuf += (bus->head_align - pad);

	 
	memcpy(buf, hdr, BRCMF_FIRSTREAD);
	if (len <= BRCMF_FIRSTREAD)
		goto gotpkt;

	 
	rdlen = len - BRCMF_FIRSTREAD;
	if (bus->roundup && bus->blocksize && (rdlen > bus->blocksize)) {
		pad = bus->blocksize - (rdlen % bus->blocksize);
		if ((pad <= bus->roundup) && (pad < bus->blocksize) &&
		    ((len + pad) < bus->sdiodev->bus_if->maxctl))
			rdlen += pad;
	} else if (rdlen % bus->head_align) {
		rdlen += bus->head_align - (rdlen % bus->head_align);
	}

	 
	if ((rdlen + BRCMF_FIRSTREAD) > bus->sdiodev->bus_if->maxctl) {
		brcmf_err("%d-byte control read exceeds %d-byte buffer\n",
			  rdlen, bus->sdiodev->bus_if->maxctl);
		brcmf_sdio_rxfail(bus, false, false);
		goto done;
	}

	if ((len - doff) > bus->sdiodev->bus_if->maxctl) {
		brcmf_err("%d-byte ctl frame (%d-byte ctl data) exceeds %d-byte limit\n",
			  len, len - doff, bus->sdiodev->bus_if->maxctl);
		bus->sdcnt.rx_toolong++;
		brcmf_sdio_rxfail(bus, false, false);
		goto done;
	}

	 
	sdret = brcmf_sdiod_recv_buf(bus->sdiodev, rbuf, rdlen);
	bus->sdcnt.f2rxdata++;

	 
	if (sdret < 0) {
		brcmf_err("read %d control bytes failed: %d\n",
			  rdlen, sdret);
		bus->sdcnt.rxc_errors++;
		brcmf_sdio_rxfail(bus, true, true);
		goto done;
	} else
		memcpy(buf + BRCMF_FIRSTREAD, rbuf, rdlen);

gotpkt:

	brcmf_dbg_hex_dump(BRCMF_BYTES_ON() && BRCMF_CTL_ON(),
			   buf, len, "RxCtrl:\n");

	 
	spin_lock_bh(&bus->rxctl_lock);
	if (bus->rxctl) {
		brcmf_err("last control frame is being processed.\n");
		spin_unlock_bh(&bus->rxctl_lock);
		vfree(buf);
		goto done;
	}
	bus->rxctl = buf + doff;
	bus->rxctl_orig = buf;
	bus->rxlen = len - doff;
	spin_unlock_bh(&bus->rxctl_lock);

done:
	 
	brcmf_sdio_dcmd_resp_wake(bus);
}

 
static void brcmf_sdio_pad(struct brcmf_sdio *bus, u16 *pad, u16 *rdlen)
{
	if (bus->roundup && bus->blocksize && *rdlen > bus->blocksize) {
		*pad = bus->blocksize - (*rdlen % bus->blocksize);
		if (*pad <= bus->roundup && *pad < bus->blocksize &&
		    *rdlen + *pad + BRCMF_FIRSTREAD < MAX_RX_DATASZ)
			*rdlen += *pad;
	} else if (*rdlen % bus->head_align) {
		*rdlen += bus->head_align - (*rdlen % bus->head_align);
	}
}

static uint brcmf_sdio_readframes(struct brcmf_sdio *bus, uint maxframes)
{
	struct sk_buff *pkt;		 
	u16 pad;		 
	uint rxleft = 0;	 
	int ret;		 
	uint rxcount = 0;	 
	struct brcmf_sdio_hdrinfo *rd = &bus->cur_read, rd_new;
	u8 head_read = 0;

	brcmf_dbg(SDIO, "Enter\n");

	 
	bus->rxpending = true;

	for (rd->seq_num = bus->rx_seq, rxleft = maxframes;
	     !bus->rxskip && rxleft && bus->sdiodev->state == BRCMF_SDIOD_DATA;
	     rd->seq_num++, rxleft--) {

		 
		if (bus->glomd || !skb_queue_empty(&bus->glom)) {
			u8 cnt;
			brcmf_dbg(GLOM, "calling rxglom: glomd %p, glom %p\n",
				  bus->glomd, skb_peek(&bus->glom));
			cnt = brcmf_sdio_rxglom(bus, rd->seq_num);
			brcmf_dbg(GLOM, "rxglom returned %d\n", cnt);
			rd->seq_num += cnt - 1;
			rxleft = (rxleft > cnt) ? (rxleft - cnt) : 1;
			continue;
		}

		rd->len_left = rd->len;
		 
		sdio_claim_host(bus->sdiodev->func1);
		if (!rd->len) {
			ret = brcmf_sdiod_recv_buf(bus->sdiodev,
						   bus->rxhdr, BRCMF_FIRSTREAD);
			bus->sdcnt.f2rxhdrs++;
			if (ret < 0) {
				brcmf_err("RXHEADER FAILED: %d\n",
					  ret);
				bus->sdcnt.rx_hdrfail++;
				brcmf_sdio_rxfail(bus, true, true);
				sdio_release_host(bus->sdiodev->func1);
				continue;
			}

			brcmf_dbg_hex_dump(BRCMF_BYTES_ON() || BRCMF_HDRS_ON(),
					   bus->rxhdr, SDPCM_HDRLEN,
					   "RxHdr:\n");

			if (brcmf_sdio_hdparse(bus, bus->rxhdr, rd,
					       BRCMF_SDIO_FT_NORMAL)) {
				sdio_release_host(bus->sdiodev->func1);
				if (!bus->rxpending)
					break;
				else
					continue;
			}

			if (rd->channel == SDPCM_CONTROL_CHANNEL) {
				brcmf_sdio_read_control(bus, bus->rxhdr,
							rd->len,
							rd->dat_offset);
				 
				rd->len = rd->len_nxtfrm << 4;
				rd->len_nxtfrm = 0;
				 
				rd->channel = SDPCM_EVENT_CHANNEL;
				sdio_release_host(bus->sdiodev->func1);
				continue;
			}
			rd->len_left = rd->len > BRCMF_FIRSTREAD ?
				       rd->len - BRCMF_FIRSTREAD : 0;
			head_read = BRCMF_FIRSTREAD;
		}

		brcmf_sdio_pad(bus, &pad, &rd->len_left);

		pkt = brcmu_pkt_buf_get_skb(rd->len_left + head_read +
					    bus->head_align);
		if (!pkt) {
			 
			brcmf_err("brcmu_pkt_buf_get_skb failed\n");
			brcmf_sdio_rxfail(bus, false,
					    RETRYCHAN(rd->channel));
			sdio_release_host(bus->sdiodev->func1);
			continue;
		}
		skb_pull(pkt, head_read);
		pkt_align(pkt, rd->len_left, bus->head_align);

		ret = brcmf_sdiod_recv_pkt(bus->sdiodev, pkt);
		bus->sdcnt.f2rxdata++;
		sdio_release_host(bus->sdiodev->func1);

		if (ret < 0) {
			brcmf_err("read %d bytes from channel %d failed: %d\n",
				  rd->len, rd->channel, ret);
			brcmu_pkt_buf_free_skb(pkt);
			sdio_claim_host(bus->sdiodev->func1);
			brcmf_sdio_rxfail(bus, true,
					    RETRYCHAN(rd->channel));
			sdio_release_host(bus->sdiodev->func1);
			continue;
		}

		if (head_read) {
			skb_push(pkt, head_read);
			memcpy(pkt->data, bus->rxhdr, head_read);
			head_read = 0;
		} else {
			memcpy(bus->rxhdr, pkt->data, SDPCM_HDRLEN);
			rd_new.seq_num = rd->seq_num;
			sdio_claim_host(bus->sdiodev->func1);
			if (brcmf_sdio_hdparse(bus, bus->rxhdr, &rd_new,
					       BRCMF_SDIO_FT_NORMAL)) {
				rd->len = 0;
				brcmf_sdio_rxfail(bus, true, true);
				sdio_release_host(bus->sdiodev->func1);
				brcmu_pkt_buf_free_skb(pkt);
				continue;
			}
			bus->sdcnt.rx_readahead_cnt++;
			if (rd->len != roundup(rd_new.len, 16)) {
				brcmf_err("frame length mismatch:read %d, should be %d\n",
					  rd->len,
					  roundup(rd_new.len, 16) >> 4);
				rd->len = 0;
				brcmf_sdio_rxfail(bus, true, true);
				sdio_release_host(bus->sdiodev->func1);
				brcmu_pkt_buf_free_skb(pkt);
				continue;
			}
			sdio_release_host(bus->sdiodev->func1);
			rd->len_nxtfrm = rd_new.len_nxtfrm;
			rd->channel = rd_new.channel;
			rd->dat_offset = rd_new.dat_offset;

			brcmf_dbg_hex_dump(!(BRCMF_BYTES_ON() &&
					     BRCMF_DATA_ON()) &&
					   BRCMF_HDRS_ON(),
					   bus->rxhdr, SDPCM_HDRLEN,
					   "RxHdr:\n");

			if (rd_new.channel == SDPCM_CONTROL_CHANNEL) {
				brcmf_err("readahead on control packet %d?\n",
					  rd_new.seq_num);
				 
				rd->len = 0;
				sdio_claim_host(bus->sdiodev->func1);
				brcmf_sdio_rxfail(bus, false, true);
				sdio_release_host(bus->sdiodev->func1);
				brcmu_pkt_buf_free_skb(pkt);
				continue;
			}
		}

		brcmf_dbg_hex_dump(BRCMF_BYTES_ON() && BRCMF_DATA_ON(),
				   pkt->data, rd->len, "Rx Data:\n");

		 
		if (rd->channel == SDPCM_GLOM_CHANNEL) {
			if (SDPCM_GLOMDESC(&bus->rxhdr[SDPCM_HWHDR_LEN])) {
				brcmf_dbg(GLOM, "glom descriptor, %d bytes:\n",
					  rd->len);
				brcmf_dbg_hex_dump(BRCMF_GLOM_ON(),
						   pkt->data, rd->len,
						   "Glom Data:\n");
				__skb_trim(pkt, rd->len);
				skb_pull(pkt, SDPCM_HDRLEN);
				bus->glomd = pkt;
			} else {
				brcmf_err("%s: glom superframe w/o "
					  "descriptor!\n", __func__);
				sdio_claim_host(bus->sdiodev->func1);
				brcmf_sdio_rxfail(bus, false, false);
				sdio_release_host(bus->sdiodev->func1);
			}
			 
			rd->len = rd->len_nxtfrm << 4;
			rd->len_nxtfrm = 0;
			 
			rd->channel = SDPCM_EVENT_CHANNEL;
			continue;
		}

		 
		__skb_trim(pkt, rd->len);
		skb_pull(pkt, rd->dat_offset);

		if (pkt->len == 0)
			brcmu_pkt_buf_free_skb(pkt);
		else if (rd->channel == SDPCM_EVENT_CHANNEL)
			brcmf_rx_event(bus->sdiodev->dev, pkt);
		else
			brcmf_rx_frame(bus->sdiodev->dev, pkt,
				       false, false);

		 
		rd->len = rd->len_nxtfrm << 4;
		rd->len_nxtfrm = 0;
		 
		rd->channel = SDPCM_EVENT_CHANNEL;
	}

	rxcount = maxframes - rxleft;
	 
	if (!rxleft)
		brcmf_dbg(DATA, "hit rx limit of %d frames\n", maxframes);
	else
		brcmf_dbg(DATA, "processed %d frames\n", rxcount);
	 
	if (bus->rxskip)
		rd->seq_num--;
	bus->rx_seq = rd->seq_num;

	return rxcount;
}

static void
brcmf_sdio_wait_event_wakeup(struct brcmf_sdio *bus)
{
	wake_up_interruptible(&bus->ctrl_wait);
	return;
}

static int brcmf_sdio_txpkt_hdalign(struct brcmf_sdio *bus, struct sk_buff *pkt)
{
	struct brcmf_bus_stats *stats;
	u16 head_pad;
	u8 *dat_buf;

	dat_buf = (u8 *)(pkt->data);

	 
	head_pad = ((unsigned long)dat_buf % bus->head_align);
	if (head_pad) {
		if (skb_headroom(pkt) < head_pad) {
			stats = &bus->sdiodev->bus_if->stats;
			atomic_inc(&stats->pktcowed);
			if (skb_cow_head(pkt, head_pad)) {
				atomic_inc(&stats->pktcow_failed);
				return -ENOMEM;
			}
			head_pad = 0;
		}
		skb_push(pkt, head_pad);
		dat_buf = (u8 *)(pkt->data);
	}
	memset(dat_buf, 0, head_pad + bus->tx_hdrlen);
	return head_pad;
}

 
 
#define ALIGN_SKB_FLAG		0x8000
 
#define ALIGN_SKB_CHOP_LEN_MASK	0x7fff

static int brcmf_sdio_txpkt_prep_sg(struct brcmf_sdio *bus,
				    struct sk_buff_head *pktq,
				    struct sk_buff *pkt, u16 total_len)
{
	struct brcmf_sdio_dev *sdiodev;
	struct sk_buff *pkt_pad;
	u16 tail_pad, tail_chop, chain_pad;
	unsigned int blksize;
	bool lastfrm;
	int ntail, ret;

	sdiodev = bus->sdiodev;
	blksize = sdiodev->func2->cur_blksize;
	 
	WARN_ON(blksize % bus->sgentry_align);

	 
	lastfrm = skb_queue_is_last(pktq, pkt);
	tail_pad = 0;
	tail_chop = pkt->len % bus->sgentry_align;
	if (tail_chop)
		tail_pad = bus->sgentry_align - tail_chop;
	chain_pad = (total_len + tail_pad) % blksize;
	if (lastfrm && chain_pad)
		tail_pad += blksize - chain_pad;
	if (skb_tailroom(pkt) < tail_pad && pkt->len > blksize) {
		pkt_pad = brcmu_pkt_buf_get_skb(tail_pad + tail_chop +
						bus->head_align);
		if (pkt_pad == NULL)
			return -ENOMEM;
		ret = brcmf_sdio_txpkt_hdalign(bus, pkt_pad);
		if (unlikely(ret < 0)) {
			kfree_skb(pkt_pad);
			return ret;
		}
		memcpy(pkt_pad->data,
		       pkt->data + pkt->len - tail_chop,
		       tail_chop);
		*(u16 *)(pkt_pad->cb) = ALIGN_SKB_FLAG + tail_chop;
		skb_trim(pkt, pkt->len - tail_chop);
		skb_trim(pkt_pad, tail_pad + tail_chop);
		__skb_queue_after(pktq, pkt, pkt_pad);
	} else {
		ntail = pkt->data_len + tail_pad -
			(pkt->end - pkt->tail);
		if (skb_cloned(pkt) || ntail > 0)
			if (pskb_expand_head(pkt, 0, ntail, GFP_ATOMIC))
				return -ENOMEM;
		if (skb_linearize(pkt))
			return -ENOMEM;
		__skb_put(pkt, tail_pad);
	}

	return tail_pad;
}

 
static int
brcmf_sdio_txpkt_prep(struct brcmf_sdio *bus, struct sk_buff_head *pktq,
		      uint chan)
{
	u16 head_pad, total_len;
	struct sk_buff *pkt_next;
	u8 txseq;
	int ret;
	struct brcmf_sdio_hdrinfo hd_info = {0};

	txseq = bus->tx_seq;
	total_len = 0;
	skb_queue_walk(pktq, pkt_next) {
		 
		if (*(u16 *)(pkt_next->cb) & ALIGN_SKB_FLAG)
			continue;

		 
		ret = brcmf_sdio_txpkt_hdalign(bus, pkt_next);
		if (ret < 0)
			return ret;
		head_pad = (u16)ret;
		if (head_pad)
			memset(pkt_next->data + bus->tx_hdrlen, 0, head_pad);

		total_len += pkt_next->len;

		hd_info.len = pkt_next->len;
		hd_info.lastfrm = skb_queue_is_last(pktq, pkt_next);
		if (bus->txglom && pktq->qlen > 1) {
			ret = brcmf_sdio_txpkt_prep_sg(bus, pktq,
						       pkt_next, total_len);
			if (ret < 0)
				return ret;
			hd_info.tail_pad = (u16)ret;
			total_len += (u16)ret;
		}

		hd_info.channel = chan;
		hd_info.dat_offset = head_pad + bus->tx_hdrlen;
		hd_info.seq_num = txseq++;

		 
		brcmf_sdio_hdpack(bus, pkt_next->data, &hd_info);

		if (BRCMF_BYTES_ON() &&
		    ((BRCMF_CTL_ON() && chan == SDPCM_CONTROL_CHANNEL) ||
		     (BRCMF_DATA_ON() && chan != SDPCM_CONTROL_CHANNEL)))
			brcmf_dbg_hex_dump(true, pkt_next->data, hd_info.len,
					   "Tx Frame:\n");
		else if (BRCMF_HDRS_ON())
			brcmf_dbg_hex_dump(true, pkt_next->data,
					   head_pad + bus->tx_hdrlen,
					   "Tx Header:\n");
	}
	 
	if (bus->txglom)
		brcmf_sdio_update_hwhdr(__skb_peek(pktq)->data, total_len);
	return 0;
}

 
static void
brcmf_sdio_txpkt_postp(struct brcmf_sdio *bus, struct sk_buff_head *pktq)
{
	u8 *hdr;
	u32 dat_offset;
	u16 tail_pad;
	u16 dummy_flags, chop_len;
	struct sk_buff *pkt_next, *tmp, *pkt_prev;

	skb_queue_walk_safe(pktq, pkt_next, tmp) {
		dummy_flags = *(u16 *)(pkt_next->cb);
		if (dummy_flags & ALIGN_SKB_FLAG) {
			chop_len = dummy_flags & ALIGN_SKB_CHOP_LEN_MASK;
			if (chop_len) {
				pkt_prev = pkt_next->prev;
				skb_put(pkt_prev, chop_len);
			}
			__skb_unlink(pkt_next, pktq);
			brcmu_pkt_buf_free_skb(pkt_next);
		} else {
			hdr = pkt_next->data + bus->tx_hdrlen - SDPCM_SWHDR_LEN;
			dat_offset = le32_to_cpu(*(__le32 *)hdr);
			dat_offset = (dat_offset & SDPCM_DOFFSET_MASK) >>
				     SDPCM_DOFFSET_SHIFT;
			skb_pull(pkt_next, dat_offset);
			if (bus->txglom) {
				tail_pad = le16_to_cpu(*(__le16 *)(hdr - 2));
				skb_trim(pkt_next, pkt_next->len - tail_pad);
			}
		}
	}
}

 
 
static int brcmf_sdio_txpkt(struct brcmf_sdio *bus, struct sk_buff_head *pktq,
			    uint chan)
{
	int ret;
	struct sk_buff *pkt_next, *tmp;

	brcmf_dbg(TRACE, "Enter\n");

	ret = brcmf_sdio_txpkt_prep(bus, pktq, chan);
	if (ret)
		goto done;

	sdio_claim_host(bus->sdiodev->func1);
	ret = brcmf_sdiod_send_pkt(bus->sdiodev, pktq);
	bus->sdcnt.f2txdata++;

	if (ret < 0)
		brcmf_sdio_txfail(bus);

	sdio_release_host(bus->sdiodev->func1);

done:
	brcmf_sdio_txpkt_postp(bus, pktq);
	if (ret == 0)
		bus->tx_seq = (bus->tx_seq + pktq->qlen) % SDPCM_SEQ_WRAP;
	skb_queue_walk_safe(pktq, pkt_next, tmp) {
		__skb_unlink(pkt_next, pktq);
		brcmf_proto_bcdc_txcomplete(bus->sdiodev->dev, pkt_next,
					    ret == 0);
	}
	return ret;
}

static uint brcmf_sdio_sendfromq(struct brcmf_sdio *bus, uint maxframes)
{
	struct sk_buff *pkt;
	struct sk_buff_head pktq;
	u32 intstat_addr = bus->sdio_core->base + SD_REG(intstatus);
	u32 intstatus = 0;
	int ret = 0, prec_out, i;
	uint cnt = 0;
	u8 tx_prec_map, pkt_num;

	brcmf_dbg(TRACE, "Enter\n");

	tx_prec_map = ~bus->flowcontrol;

	 
	for (cnt = 0; (cnt < maxframes) && data_ok(bus);) {
		pkt_num = 1;
		if (bus->txglom)
			pkt_num = min_t(u8, bus->tx_max - bus->tx_seq,
					bus->sdiodev->txglomsz);
		pkt_num = min_t(u32, pkt_num,
				brcmu_pktq_mlen(&bus->txq, ~bus->flowcontrol));
		__skb_queue_head_init(&pktq);
		spin_lock_bh(&bus->txq_lock);
		for (i = 0; i < pkt_num; i++) {
			pkt = brcmu_pktq_mdeq(&bus->txq, tx_prec_map,
					      &prec_out);
			if (pkt == NULL)
				break;
			__skb_queue_tail(&pktq, pkt);
		}
		spin_unlock_bh(&bus->txq_lock);
		if (i == 0)
			break;

		ret = brcmf_sdio_txpkt(bus, &pktq, SDPCM_DATA_CHANNEL);

		cnt += i;

		 
		if (!bus->intr) {
			 
			sdio_claim_host(bus->sdiodev->func1);
			intstatus = brcmf_sdiod_readl(bus->sdiodev,
						      intstat_addr, &ret);
			sdio_release_host(bus->sdiodev->func1);

			bus->sdcnt.f2txdata++;
			if (ret != 0)
				break;
			if (intstatus & bus->hostintmask)
				atomic_set(&bus->ipend, 1);
		}
	}

	 
	if ((bus->sdiodev->state == BRCMF_SDIOD_DATA) &&
	    bus->txoff && (pktq_len(&bus->txq) < TXLOW)) {
		bus->txoff = false;
		brcmf_proto_bcdc_txflowblock(bus->sdiodev->dev, false);
	}

	return cnt;
}

static int brcmf_sdio_tx_ctrlframe(struct brcmf_sdio *bus, u8 *frame, u16 len)
{
	u8 doff;
	u16 pad;
	uint retries = 0;
	struct brcmf_sdio_hdrinfo hd_info = {0};
	int ret;

	brcmf_dbg(SDIO, "Enter\n");

	 
	frame -= bus->tx_hdrlen;
	len += bus->tx_hdrlen;

	 
	doff = ((unsigned long)frame % bus->head_align);
	if (doff) {
		frame -= doff;
		len += doff;
		memset(frame + bus->tx_hdrlen, 0, doff);
	}

	 
	pad = 0;
	if (bus->roundup && bus->blocksize && (len > bus->blocksize)) {
		pad = bus->blocksize - (len % bus->blocksize);
		if ((pad > bus->roundup) || (pad >= bus->blocksize))
			pad = 0;
	} else if (len % bus->head_align) {
		pad = bus->head_align - (len % bus->head_align);
	}
	len += pad;

	hd_info.len = len - pad;
	hd_info.channel = SDPCM_CONTROL_CHANNEL;
	hd_info.dat_offset = doff + bus->tx_hdrlen;
	hd_info.seq_num = bus->tx_seq;
	hd_info.lastfrm = true;
	hd_info.tail_pad = pad;
	brcmf_sdio_hdpack(bus, frame, &hd_info);

	if (bus->txglom)
		brcmf_sdio_update_hwhdr(frame, len);

	brcmf_dbg_hex_dump(BRCMF_BYTES_ON() && BRCMF_CTL_ON(),
			   frame, len, "Tx Frame:\n");
	brcmf_dbg_hex_dump(!(BRCMF_BYTES_ON() && BRCMF_CTL_ON()) &&
			   BRCMF_HDRS_ON(),
			   frame, min_t(u16, len, 16), "TxHdr:\n");

	do {
		ret = brcmf_sdiod_send_buf(bus->sdiodev, frame, len);

		if (ret < 0)
			brcmf_sdio_txfail(bus);
		else
			bus->tx_seq = (bus->tx_seq + 1) % SDPCM_SEQ_WRAP;
	} while (ret < 0 && retries++ < TXRETRIES);

	return ret;
}

static bool brcmf_chip_is_ulp(struct brcmf_chip *ci)
{
	if (ci->chip == CY_CC_43012_CHIP_ID)
		return true;
	else
		return false;
}

static void brcmf_sdio_bus_stop(struct device *dev)
{
	struct brcmf_bus *bus_if = dev_get_drvdata(dev);
	struct brcmf_sdio_dev *sdiodev = bus_if->bus_priv.sdio;
	struct brcmf_sdio *bus = sdiodev->bus;
	struct brcmf_core *core = bus->sdio_core;
	u32 local_hostintmask;
	u8 saveclk, bpreq;
	int err;

	brcmf_dbg(TRACE, "Enter\n");

	if (bus->watchdog_tsk) {
		send_sig(SIGTERM, bus->watchdog_tsk, 1);
		kthread_stop(bus->watchdog_tsk);
		bus->watchdog_tsk = NULL;
	}

	if (sdiodev->state != BRCMF_SDIOD_NOMEDIUM) {
		sdio_claim_host(sdiodev->func1);

		 
		brcmf_sdio_bus_sleep(bus, false, false);

		 
		brcmf_sdiod_writel(sdiodev, core->base + SD_REG(hostintmask),
				   0, NULL);

		local_hostintmask = bus->hostintmask;
		bus->hostintmask = 0;

		 
		saveclk = brcmf_sdiod_readb(sdiodev, SBSDIO_FUNC1_CHIPCLKCSR,
					    &err);
		if (!err) {
			bpreq = saveclk;
			bpreq |= brcmf_chip_is_ulp(bus->ci) ?
				SBSDIO_HT_AVAIL_REQ : SBSDIO_FORCE_HT;
			brcmf_sdiod_writeb(sdiodev,
					   SBSDIO_FUNC1_CHIPCLKCSR,
					   bpreq, &err);
		}
		if (err)
			brcmf_err("Failed to force clock for F2: err %d\n",
				  err);

		 
		brcmf_dbg(INTR, "disable SDIO interrupts\n");
		sdio_disable_func(sdiodev->func2);

		 
		brcmf_sdiod_writel(sdiodev, core->base + SD_REG(intstatus),
				   local_hostintmask, NULL);

		sdio_release_host(sdiodev->func1);
	}
	 
	brcmu_pktq_flush(&bus->txq, true, NULL, NULL);

	 
	brcmu_pkt_buf_free_skb(bus->glomd);
	brcmf_sdio_free_glom(bus);

	 
	spin_lock_bh(&bus->rxctl_lock);
	bus->rxlen = 0;
	spin_unlock_bh(&bus->rxctl_lock);
	brcmf_sdio_dcmd_resp_wake(bus);

	 
	bus->rxskip = false;
	bus->tx_seq = bus->rx_seq = 0;
}

static inline void brcmf_sdio_clrintr(struct brcmf_sdio *bus)
{
	struct brcmf_sdio_dev *sdiodev;
	unsigned long flags;

	sdiodev = bus->sdiodev;
	if (sdiodev->oob_irq_requested) {
		spin_lock_irqsave(&sdiodev->irq_en_lock, flags);
		if (!sdiodev->irq_en && !atomic_read(&bus->ipend)) {
			enable_irq(sdiodev->settings->bus.sdio.oob_irq_nr);
			sdiodev->irq_en = true;
		}
		spin_unlock_irqrestore(&sdiodev->irq_en_lock, flags);
	}
}

static int brcmf_sdio_intr_rstatus(struct brcmf_sdio *bus)
{
	struct brcmf_core *core = bus->sdio_core;
	u32 addr;
	unsigned long val;
	int ret;

	addr = core->base + SD_REG(intstatus);

	val = brcmf_sdiod_readl(bus->sdiodev, addr, &ret);
	bus->sdcnt.f1regdata++;
	if (ret != 0)
		return ret;

	val &= bus->hostintmask;
	atomic_set(&bus->fcstate, !!(val & I_HMB_FC_STATE));

	 
	if (val) {
		brcmf_sdiod_writel(bus->sdiodev, addr, val, &ret);
		bus->sdcnt.f1regdata++;
		atomic_or(val, &bus->intstatus);
	}

	return ret;
}

static void brcmf_sdio_dpc(struct brcmf_sdio *bus)
{
	struct brcmf_sdio_dev *sdiod = bus->sdiodev;
	u32 newstatus = 0;
	u32 intstat_addr = bus->sdio_core->base + SD_REG(intstatus);
	unsigned long intstatus;
	uint txlimit = bus->txbound;	 
	uint framecnt;			 
	int err = 0;

	brcmf_dbg(SDIO, "Enter\n");

	sdio_claim_host(bus->sdiodev->func1);

	 
	if (!bus->sr_enabled && bus->clkstate == CLK_PENDING) {
		u8 clkctl, devctl = 0;

#ifdef DEBUG
		 
		devctl = brcmf_sdiod_readb(bus->sdiodev, SBSDIO_DEVICE_CTL,
					   &err);
#endif				 

		 
		clkctl = brcmf_sdiod_readb(bus->sdiodev,
					   SBSDIO_FUNC1_CHIPCLKCSR, &err);

		brcmf_dbg(SDIO, "DPC: PENDING, devctl 0x%02x clkctl 0x%02x\n",
			  devctl, clkctl);

		if (SBSDIO_HTAV(clkctl)) {
			devctl = brcmf_sdiod_readb(bus->sdiodev,
						   SBSDIO_DEVICE_CTL, &err);
			devctl &= ~SBSDIO_DEVCTL_CA_INT_ONLY;
			brcmf_sdiod_writeb(bus->sdiodev,
					   SBSDIO_DEVICE_CTL, devctl, &err);
			bus->clkstate = CLK_AVAIL;
		}
	}

	 
	brcmf_sdio_bus_sleep(bus, false, true);

	 
	if (atomic_read(&bus->ipend) > 0) {
		atomic_set(&bus->ipend, 0);
		err = brcmf_sdio_intr_rstatus(bus);
	}

	 
	intstatus = atomic_xchg(&bus->intstatus, 0);

	 
	if (intstatus & I_HMB_FC_CHANGE) {
		intstatus &= ~I_HMB_FC_CHANGE;
		brcmf_sdiod_writel(sdiod, intstat_addr, I_HMB_FC_CHANGE, &err);

		newstatus = brcmf_sdiod_readl(sdiod, intstat_addr, &err);

		bus->sdcnt.f1regdata += 2;
		atomic_set(&bus->fcstate,
			   !!(newstatus & (I_HMB_FC_STATE | I_HMB_FC_CHANGE)));
		intstatus |= (newstatus & bus->hostintmask);
	}

	 
	if (intstatus & I_HMB_HOST_INT) {
		intstatus &= ~I_HMB_HOST_INT;
		intstatus |= brcmf_sdio_hostmail(bus);
	}

	sdio_release_host(bus->sdiodev->func1);

	 
	if (intstatus & I_WR_OOSYNC) {
		brcmf_err("Dongle reports WR_OOSYNC\n");
		intstatus &= ~I_WR_OOSYNC;
	}

	if (intstatus & I_RD_OOSYNC) {
		brcmf_err("Dongle reports RD_OOSYNC\n");
		intstatus &= ~I_RD_OOSYNC;
	}

	if (intstatus & I_SBINT) {
		brcmf_err("Dongle reports SBINT\n");
		intstatus &= ~I_SBINT;
	}

	 
	if (intstatus & I_CHIPACTIVE) {
		brcmf_dbg(SDIO, "Dongle reports CHIPACTIVE\n");
		intstatus &= ~I_CHIPACTIVE;
	}

	 
	if (bus->rxskip)
		intstatus &= ~I_HMB_FRAME_IND;

	 
	if ((intstatus & I_HMB_FRAME_IND) && (bus->clkstate == CLK_AVAIL)) {
		brcmf_sdio_readframes(bus, bus->rxbound);
		if (!bus->rxpending)
			intstatus &= ~I_HMB_FRAME_IND;
	}

	 
	if (intstatus)
		atomic_or(intstatus, &bus->intstatus);

	brcmf_sdio_clrintr(bus);

	if (bus->ctrl_frame_stat && (bus->clkstate == CLK_AVAIL) &&
	    txctl_ok(bus)) {
		sdio_claim_host(bus->sdiodev->func1);
		if (bus->ctrl_frame_stat) {
			err = brcmf_sdio_tx_ctrlframe(bus,  bus->ctrl_frame_buf,
						      bus->ctrl_frame_len);
			bus->ctrl_frame_err = err;
			wmb();
			bus->ctrl_frame_stat = false;
			if (err)
				brcmf_err("sdio ctrlframe tx failed err=%d\n",
					  err);
		}
		sdio_release_host(bus->sdiodev->func1);
		brcmf_sdio_wait_event_wakeup(bus);
	}
	 
	if ((bus->clkstate == CLK_AVAIL) && !atomic_read(&bus->fcstate) &&
	    brcmu_pktq_mlen(&bus->txq, ~bus->flowcontrol) && txlimit &&
	    data_ok(bus)) {
		framecnt = bus->rxpending ? min(txlimit, bus->txminmax) :
					    txlimit;
		brcmf_sdio_sendfromq(bus, framecnt);
	}

	if ((bus->sdiodev->state != BRCMF_SDIOD_DATA) || (err != 0)) {
		brcmf_err("failed backplane access over SDIO, halting operation\n");
		atomic_set(&bus->intstatus, 0);
		if (bus->ctrl_frame_stat) {
			sdio_claim_host(bus->sdiodev->func1);
			if (bus->ctrl_frame_stat) {
				bus->ctrl_frame_err = -ENODEV;
				wmb();
				bus->ctrl_frame_stat = false;
				brcmf_sdio_wait_event_wakeup(bus);
			}
			sdio_release_host(bus->sdiodev->func1);
		}
	} else if (atomic_read(&bus->intstatus) ||
		   atomic_read(&bus->ipend) > 0 ||
		   (!atomic_read(&bus->fcstate) &&
		    brcmu_pktq_mlen(&bus->txq, ~bus->flowcontrol) &&
		    data_ok(bus))) {
		bus->dpc_triggered = true;
	}
}

static struct pktq *brcmf_sdio_bus_gettxq(struct device *dev)
{
	struct brcmf_bus *bus_if = dev_get_drvdata(dev);
	struct brcmf_sdio_dev *sdiodev = bus_if->bus_priv.sdio;
	struct brcmf_sdio *bus = sdiodev->bus;

	return &bus->txq;
}

static bool brcmf_sdio_prec_enq(struct pktq *q, struct sk_buff *pkt, int prec)
{
	struct sk_buff *p;
	int eprec = -1;		 

	 
	if (!pktq_pfull(q, prec) && !pktq_full(q)) {
		brcmu_pktq_penq(q, prec, pkt);
		return true;
	}

	 
	if (pktq_pfull(q, prec)) {
		eprec = prec;
	} else if (pktq_full(q)) {
		p = brcmu_pktq_peek_tail(q, &eprec);
		if (eprec > prec)
			return false;
	}

	 
	if (eprec >= 0) {
		 
		if (eprec == prec)
			return false;	 
		 
		p = brcmu_pktq_pdeq_tail(q, eprec);
		if (p == NULL)
			brcmf_err("brcmu_pktq_pdeq_tail() failed\n");
		brcmu_pkt_buf_free_skb(p);
	}

	 
	p = brcmu_pktq_penq(q, prec, pkt);
	if (p == NULL)
		brcmf_err("brcmu_pktq_penq() failed\n");

	return p != NULL;
}

static int brcmf_sdio_bus_txdata(struct device *dev, struct sk_buff *pkt)
{
	int ret = -EBADE;
	uint prec;
	struct brcmf_bus *bus_if = dev_get_drvdata(dev);
	struct brcmf_sdio_dev *sdiodev = bus_if->bus_priv.sdio;
	struct brcmf_sdio *bus = sdiodev->bus;

	brcmf_dbg(TRACE, "Enter: pkt: data %p len %d\n", pkt->data, pkt->len);
	if (sdiodev->state != BRCMF_SDIOD_DATA)
		return -EIO;

	 
	skb_push(pkt, bus->tx_hdrlen);
	 

	 
	prec = brcmf_map_prio_to_prec(bus_if->drvr->config,
				      (pkt->priority & PRIOMASK));

	 
	brcmf_dbg(TRACE, "deferring pktq len %d\n", pktq_len(&bus->txq));
	bus->sdcnt.fcqueued++;

	 
	spin_lock_bh(&bus->txq_lock);
	 
	*(u16 *)(pkt->cb) = 0;
	if (!brcmf_sdio_prec_enq(&bus->txq, pkt, prec)) {
		skb_pull(pkt, bus->tx_hdrlen);
		brcmf_err("out of bus->txq !!!\n");
		ret = -ENOSR;
	} else {
		ret = 0;
	}

	if (pktq_len(&bus->txq) >= TXHI) {
		bus->txoff = true;
		brcmf_proto_bcdc_txflowblock(dev, true);
	}
	spin_unlock_bh(&bus->txq_lock);

#ifdef DEBUG
	if (pktq_plen(&bus->txq, prec) > qcount[prec])
		qcount[prec] = pktq_plen(&bus->txq, prec);
#endif

	brcmf_sdio_trigger_dpc(bus);
	return ret;
}

#ifdef DEBUG
#define CONSOLE_LINE_MAX	192

static int brcmf_sdio_readconsole(struct brcmf_sdio *bus)
{
	struct brcmf_console *c = &bus->console;
	u8 line[CONSOLE_LINE_MAX], ch;
	u32 n, idx, addr;
	int rv;

	 
	if (bus->console_addr == 0)
		return 0;

	 
	addr = bus->console_addr + offsetof(struct rte_console, log_le);
	rv = brcmf_sdiod_ramrw(bus->sdiodev, false, addr, (u8 *)&c->log_le,
			       sizeof(c->log_le));
	if (rv < 0)
		return rv;

	 
	if (c->buf == NULL) {
		c->bufsize = le32_to_cpu(c->log_le.buf_size);
		c->buf = kmalloc(c->bufsize, GFP_ATOMIC);
		if (c->buf == NULL)
			return -ENOMEM;
	}

	idx = le32_to_cpu(c->log_le.idx);

	 
	if (idx > c->bufsize)
		return -EBADE;

	 
	if (idx == c->last)
		return 0;

	 
	addr = le32_to_cpu(c->log_le.buf);
	rv = brcmf_sdiod_ramrw(bus->sdiodev, false, addr, c->buf, c->bufsize);
	if (rv < 0)
		return rv;

	while (c->last != idx) {
		for (n = 0; n < CONSOLE_LINE_MAX - 2; n++) {
			if (c->last == idx) {
				 
				if (c->last >= n)
					c->last -= n;
				else
					c->last = c->bufsize - n;
				goto break2;
			}
			ch = c->buf[c->last];
			c->last = (c->last + 1) % c->bufsize;
			if (ch == '\n')
				break;
			line[n] = ch;
		}

		if (n > 0) {
			if (line[n - 1] == '\r')
				n--;
			line[n] = 0;
			pr_debug("CONSOLE: %s\n", line);
		}
	}
break2:

	return 0;
}
#endif				 

static int
brcmf_sdio_bus_txctl(struct device *dev, unsigned char *msg, uint msglen)
{
	struct brcmf_bus *bus_if = dev_get_drvdata(dev);
	struct brcmf_sdio_dev *sdiodev = bus_if->bus_priv.sdio;
	struct brcmf_sdio *bus = sdiodev->bus;
	int ret;

	brcmf_dbg(TRACE, "Enter\n");
	if (sdiodev->state != BRCMF_SDIOD_DATA)
		return -EIO;

	 
	bus->ctrl_frame_buf = msg;
	bus->ctrl_frame_len = msglen;
	wmb();
	bus->ctrl_frame_stat = true;

	brcmf_sdio_trigger_dpc(bus);
	wait_event_interruptible_timeout(bus->ctrl_wait, !bus->ctrl_frame_stat,
					 CTL_DONE_TIMEOUT);
	ret = 0;
	if (bus->ctrl_frame_stat) {
		sdio_claim_host(bus->sdiodev->func1);
		if (bus->ctrl_frame_stat) {
			brcmf_dbg(SDIO, "ctrl_frame timeout\n");
			bus->ctrl_frame_stat = false;
			ret = -ETIMEDOUT;
		}
		sdio_release_host(bus->sdiodev->func1);
	}
	if (!ret) {
		brcmf_dbg(SDIO, "ctrl_frame complete, err=%d\n",
			  bus->ctrl_frame_err);
		rmb();
		ret = bus->ctrl_frame_err;
	}

	if (ret)
		bus->sdcnt.tx_ctlerrs++;
	else
		bus->sdcnt.tx_ctlpkts++;

	return ret;
}

#ifdef DEBUG
static int brcmf_sdio_dump_console(struct seq_file *seq, struct brcmf_sdio *bus,
				   struct sdpcm_shared *sh)
{
	u32 addr, console_ptr, console_size, console_index;
	char *conbuf = NULL;
	__le32 sh_val;
	int rv;

	 
	addr = sh->console_addr + offsetof(struct rte_console, log_le);
	rv = brcmf_sdiod_ramrw(bus->sdiodev, false, addr,
			       (u8 *)&sh_val, sizeof(u32));
	if (rv < 0)
		return rv;
	console_ptr = le32_to_cpu(sh_val);

	addr = sh->console_addr + offsetof(struct rte_console, log_le.buf_size);
	rv = brcmf_sdiod_ramrw(bus->sdiodev, false, addr,
			       (u8 *)&sh_val, sizeof(u32));
	if (rv < 0)
		return rv;
	console_size = le32_to_cpu(sh_val);

	addr = sh->console_addr + offsetof(struct rte_console, log_le.idx);
	rv = brcmf_sdiod_ramrw(bus->sdiodev, false, addr,
			       (u8 *)&sh_val, sizeof(u32));
	if (rv < 0)
		return rv;
	console_index = le32_to_cpu(sh_val);

	 
	if (console_size <= CONSOLE_BUFFER_MAX)
		conbuf = vzalloc(console_size+1);

	if (!conbuf)
		return -ENOMEM;

	 
	conbuf[console_size] = '\0';
	rv = brcmf_sdiod_ramrw(bus->sdiodev, false, console_ptr, (u8 *)conbuf,
			       console_size);
	if (rv < 0)
		goto done;

	rv = seq_write(seq, conbuf + console_index,
		       console_size - console_index);
	if (rv < 0)
		goto done;

	if (console_index > 0)
		rv = seq_write(seq, conbuf, console_index - 1);

done:
	vfree(conbuf);
	return rv;
}

static int brcmf_sdio_trap_info(struct seq_file *seq, struct brcmf_sdio *bus,
				struct sdpcm_shared *sh)
{
	int error;
	struct brcmf_trap_info tr;

	if ((sh->flags & SDPCM_SHARED_TRAP) == 0) {
		brcmf_dbg(INFO, "no trap in firmware\n");
		return 0;
	}

	error = brcmf_sdiod_ramrw(bus->sdiodev, false, sh->trap_addr, (u8 *)&tr,
				  sizeof(struct brcmf_trap_info));
	if (error < 0)
		return error;

	if (seq)
		seq_printf(seq,
			   "dongle trap info: type 0x%x @ epc 0x%08x\n"
			   "  cpsr 0x%08x spsr 0x%08x sp 0x%08x\n"
			   "  lr   0x%08x pc   0x%08x offset 0x%x\n"
			   "  r0   0x%08x r1   0x%08x r2 0x%08x r3 0x%08x\n"
			   "  r4   0x%08x r5   0x%08x r6 0x%08x r7 0x%08x\n",
			   le32_to_cpu(tr.type), le32_to_cpu(tr.epc),
			   le32_to_cpu(tr.cpsr), le32_to_cpu(tr.spsr),
			   le32_to_cpu(tr.r13), le32_to_cpu(tr.r14),
			   le32_to_cpu(tr.pc), sh->trap_addr,
			   le32_to_cpu(tr.r0), le32_to_cpu(tr.r1),
			   le32_to_cpu(tr.r2), le32_to_cpu(tr.r3),
			   le32_to_cpu(tr.r4), le32_to_cpu(tr.r5),
			   le32_to_cpu(tr.r6), le32_to_cpu(tr.r7));
	else
		pr_debug("dongle trap info: type 0x%x @ epc 0x%08x\n"
			 "  cpsr 0x%08x spsr 0x%08x sp 0x%08x\n"
			 "  lr   0x%08x pc   0x%08x offset 0x%x\n"
			 "  r0   0x%08x r1   0x%08x r2 0x%08x r3 0x%08x\n"
			 "  r4   0x%08x r5   0x%08x r6 0x%08x r7 0x%08x\n",
			 le32_to_cpu(tr.type), le32_to_cpu(tr.epc),
			 le32_to_cpu(tr.cpsr), le32_to_cpu(tr.spsr),
			 le32_to_cpu(tr.r13), le32_to_cpu(tr.r14),
			 le32_to_cpu(tr.pc), sh->trap_addr,
			 le32_to_cpu(tr.r0), le32_to_cpu(tr.r1),
			 le32_to_cpu(tr.r2), le32_to_cpu(tr.r3),
			 le32_to_cpu(tr.r4), le32_to_cpu(tr.r5),
			 le32_to_cpu(tr.r6), le32_to_cpu(tr.r7));
	return 0;
}

static int brcmf_sdio_assert_info(struct seq_file *seq, struct brcmf_sdio *bus,
				  struct sdpcm_shared *sh)
{
	int error = 0;
	char file[80] = "?";
	char expr[80] = "<???>";

	if ((sh->flags & SDPCM_SHARED_ASSERT_BUILT) == 0) {
		brcmf_dbg(INFO, "firmware not built with -assert\n");
		return 0;
	} else if ((sh->flags & SDPCM_SHARED_ASSERT) == 0) {
		brcmf_dbg(INFO, "no assert in dongle\n");
		return 0;
	}

	sdio_claim_host(bus->sdiodev->func1);
	if (sh->assert_file_addr != 0) {
		error = brcmf_sdiod_ramrw(bus->sdiodev, false,
					  sh->assert_file_addr, (u8 *)file, 80);
		if (error < 0)
			return error;
	}
	if (sh->assert_exp_addr != 0) {
		error = brcmf_sdiod_ramrw(bus->sdiodev, false,
					  sh->assert_exp_addr, (u8 *)expr, 80);
		if (error < 0)
			return error;
	}
	sdio_release_host(bus->sdiodev->func1);

	seq_printf(seq, "dongle assert: %s:%d: assert(%s)\n",
		   file, sh->assert_line, expr);
	return 0;
}

static int brcmf_sdio_checkdied(struct brcmf_sdio *bus)
{
	int error;
	struct sdpcm_shared sh;

	error = brcmf_sdio_readshared(bus, &sh);

	if (error < 0)
		return error;

	if ((sh.flags & SDPCM_SHARED_ASSERT_BUILT) == 0)
		brcmf_dbg(INFO, "firmware not built with -assert\n");
	else if (sh.flags & SDPCM_SHARED_ASSERT)
		brcmf_err("assertion in dongle\n");

	if (sh.flags & SDPCM_SHARED_TRAP) {
		brcmf_err("firmware trap in dongle\n");
		brcmf_sdio_trap_info(NULL, bus, &sh);
	}

	return 0;
}

static int brcmf_sdio_died_dump(struct seq_file *seq, struct brcmf_sdio *bus)
{
	int error = 0;
	struct sdpcm_shared sh;

	error = brcmf_sdio_readshared(bus, &sh);
	if (error < 0)
		goto done;

	error = brcmf_sdio_assert_info(seq, bus, &sh);
	if (error < 0)
		goto done;

	error = brcmf_sdio_trap_info(seq, bus, &sh);
	if (error < 0)
		goto done;

	error = brcmf_sdio_dump_console(seq, bus, &sh);

done:
	return error;
}

static int brcmf_sdio_forensic_read(struct seq_file *seq, void *data)
{
	struct brcmf_bus *bus_if = dev_get_drvdata(seq->private);
	struct brcmf_sdio *bus = bus_if->bus_priv.sdio->bus;

	return brcmf_sdio_died_dump(seq, bus);
}

static int brcmf_debugfs_sdio_count_read(struct seq_file *seq, void *data)
{
	struct brcmf_bus *bus_if = dev_get_drvdata(seq->private);
	struct brcmf_sdio_dev *sdiodev = bus_if->bus_priv.sdio;
	struct brcmf_sdio_count *sdcnt = &sdiodev->bus->sdcnt;

	seq_printf(seq,
		   "intrcount:    %u\nlastintrs:    %u\n"
		   "pollcnt:      %u\nregfails:     %u\n"
		   "tx_sderrs:    %u\nfcqueued:     %u\n"
		   "rxrtx:        %u\nrx_toolong:   %u\n"
		   "rxc_errors:   %u\nrx_hdrfail:   %u\n"
		   "rx_badhdr:    %u\nrx_badseq:    %u\n"
		   "fc_rcvd:      %u\nfc_xoff:      %u\n"
		   "fc_xon:       %u\nrxglomfail:   %u\n"
		   "rxglomframes: %u\nrxglompkts:   %u\n"
		   "f2rxhdrs:     %u\nf2rxdata:     %u\n"
		   "f2txdata:     %u\nf1regdata:    %u\n"
		   "tickcnt:      %u\ntx_ctlerrs:   %lu\n"
		   "tx_ctlpkts:   %lu\nrx_ctlerrs:   %lu\n"
		   "rx_ctlpkts:   %lu\nrx_readahead: %lu\n",
		   sdcnt->intrcount, sdcnt->lastintrs,
		   sdcnt->pollcnt, sdcnt->regfails,
		   sdcnt->tx_sderrs, sdcnt->fcqueued,
		   sdcnt->rxrtx, sdcnt->rx_toolong,
		   sdcnt->rxc_errors, sdcnt->rx_hdrfail,
		   sdcnt->rx_badhdr, sdcnt->rx_badseq,
		   sdcnt->fc_rcvd, sdcnt->fc_xoff,
		   sdcnt->fc_xon, sdcnt->rxglomfail,
		   sdcnt->rxglomframes, sdcnt->rxglompkts,
		   sdcnt->f2rxhdrs, sdcnt->f2rxdata,
		   sdcnt->f2txdata, sdcnt->f1regdata,
		   sdcnt->tickcnt, sdcnt->tx_ctlerrs,
		   sdcnt->tx_ctlpkts, sdcnt->rx_ctlerrs,
		   sdcnt->rx_ctlpkts, sdcnt->rx_readahead_cnt);

	return 0;
}

static void brcmf_sdio_debugfs_create(struct device *dev)
{
	struct brcmf_bus *bus_if = dev_get_drvdata(dev);
	struct brcmf_pub *drvr = bus_if->drvr;
	struct brcmf_sdio_dev *sdiodev = bus_if->bus_priv.sdio;
	struct brcmf_sdio *bus = sdiodev->bus;
	struct dentry *dentry = brcmf_debugfs_get_devdir(drvr);

	if (IS_ERR_OR_NULL(dentry))
		return;

	bus->console_interval = BRCMF_CONSOLE;

	brcmf_debugfs_add_entry(drvr, "forensics", brcmf_sdio_forensic_read);
	brcmf_debugfs_add_entry(drvr, "counters",
				brcmf_debugfs_sdio_count_read);
	debugfs_create_u32("console_interval", 0644, dentry,
			   &bus->console_interval);
}
#else
static int brcmf_sdio_checkdied(struct brcmf_sdio *bus)
{
	return 0;
}

static void brcmf_sdio_debugfs_create(struct device *dev)
{
}
#endif  

static int
brcmf_sdio_bus_rxctl(struct device *dev, unsigned char *msg, uint msglen)
{
	int timeleft;
	uint rxlen = 0;
	bool pending;
	u8 *buf;
	struct brcmf_bus *bus_if = dev_get_drvdata(dev);
	struct brcmf_sdio_dev *sdiodev = bus_if->bus_priv.sdio;
	struct brcmf_sdio *bus = sdiodev->bus;

	brcmf_dbg(TRACE, "Enter\n");
	if (sdiodev->state != BRCMF_SDIOD_DATA)
		return -EIO;

	 
	timeleft = brcmf_sdio_dcmd_resp_wait(bus, &bus->rxlen, &pending);

	spin_lock_bh(&bus->rxctl_lock);
	rxlen = bus->rxlen;
	memcpy(msg, bus->rxctl, min(msglen, rxlen));
	bus->rxctl = NULL;
	buf = bus->rxctl_orig;
	bus->rxctl_orig = NULL;
	bus->rxlen = 0;
	spin_unlock_bh(&bus->rxctl_lock);
	vfree(buf);

	if (rxlen) {
		brcmf_dbg(CTL, "resumed on rxctl frame, got %d expected %d\n",
			  rxlen, msglen);
	} else if (timeleft == 0) {
		brcmf_err("resumed on timeout\n");
		brcmf_sdio_checkdied(bus);
	} else if (pending) {
		brcmf_dbg(CTL, "cancelled\n");
		return -ERESTARTSYS;
	} else {
		brcmf_dbg(CTL, "resumed for unknown reason?\n");
		brcmf_sdio_checkdied(bus);
	}

	if (rxlen)
		bus->sdcnt.rx_ctlpkts++;
	else
		bus->sdcnt.rx_ctlerrs++;

	return rxlen ? (int)rxlen : -ETIMEDOUT;
}

#ifdef DEBUG
static bool
brcmf_sdio_verifymemory(struct brcmf_sdio_dev *sdiodev, u32 ram_addr,
			u8 *ram_data, uint ram_sz)
{
	char *ram_cmp;
	int err;
	bool ret = true;
	int address;
	int offset;
	int len;

	 
	brcmf_dbg(INFO, "Compare RAM dl & ul at 0x%08x; size=%d\n", ram_addr,
		  ram_sz);
	ram_cmp = kmalloc(MEMBLOCK, GFP_KERNEL);
	 
	if (!ram_cmp)
		return true;

	address = ram_addr;
	offset = 0;
	while (offset < ram_sz) {
		len = ((offset + MEMBLOCK) < ram_sz) ? MEMBLOCK :
		      ram_sz - offset;
		err = brcmf_sdiod_ramrw(sdiodev, false, address, ram_cmp, len);
		if (err) {
			brcmf_err("error %d on reading %d membytes at 0x%08x\n",
				  err, len, address);
			ret = false;
			break;
		} else if (memcmp(ram_cmp, &ram_data[offset], len)) {
			brcmf_err("Downloaded RAM image is corrupted, block offset is %d, len is %d\n",
				  offset, len);
			ret = false;
			break;
		}
		offset += len;
		address += len;
	}

	kfree(ram_cmp);

	return ret;
}
#else	 
static bool
brcmf_sdio_verifymemory(struct brcmf_sdio_dev *sdiodev, u32 ram_addr,
			u8 *ram_data, uint ram_sz)
{
	return true;
}
#endif	 

static int brcmf_sdio_download_code_file(struct brcmf_sdio *bus,
					 const struct firmware *fw)
{
	int err;

	brcmf_dbg(TRACE, "Enter\n");

	err = brcmf_sdiod_ramrw(bus->sdiodev, true, bus->ci->rambase,
				(u8 *)fw->data, fw->size);
	if (err)
		brcmf_err("error %d on writing %d membytes at 0x%08x\n",
			  err, (int)fw->size, bus->ci->rambase);
	else if (!brcmf_sdio_verifymemory(bus->sdiodev, bus->ci->rambase,
					  (u8 *)fw->data, fw->size))
		err = -EIO;

	return err;
}

static int brcmf_sdio_download_nvram(struct brcmf_sdio *bus,
				     void *vars, u32 varsz)
{
	int address;
	int err;

	brcmf_dbg(TRACE, "Enter\n");

	address = bus->ci->ramsize - varsz + bus->ci->rambase;
	err = brcmf_sdiod_ramrw(bus->sdiodev, true, address, vars, varsz);
	if (err)
		brcmf_err("error %d on writing %d nvram bytes at 0x%08x\n",
			  err, varsz, address);
	else if (!brcmf_sdio_verifymemory(bus->sdiodev, address, vars, varsz))
		err = -EIO;

	return err;
}

static int brcmf_sdio_download_firmware(struct brcmf_sdio *bus,
					const struct firmware *fw,
					void *nvram, u32 nvlen)
{
	int bcmerror;
	u32 rstvec;

	sdio_claim_host(bus->sdiodev->func1);
	brcmf_sdio_clkctl(bus, CLK_AVAIL, false);

	rstvec = get_unaligned_le32(fw->data);
	brcmf_dbg(SDIO, "firmware rstvec: %x\n", rstvec);

	bcmerror = brcmf_sdio_download_code_file(bus, fw);
	release_firmware(fw);
	if (bcmerror) {
		brcmf_err("dongle image file download failed\n");
		brcmf_fw_nvram_free(nvram);
		goto err;
	}

	bcmerror = brcmf_sdio_download_nvram(bus, nvram, nvlen);
	brcmf_fw_nvram_free(nvram);
	if (bcmerror) {
		brcmf_err("dongle nvram file download failed\n");
		goto err;
	}

	 
	if (!brcmf_chip_set_active(bus->ci, rstvec)) {
		brcmf_err("error getting out of ARM core reset\n");
		bcmerror = -EIO;
		goto err;
	}

err:
	brcmf_sdio_clkctl(bus, CLK_SDONLY, false);
	sdio_release_host(bus->sdiodev->func1);
	return bcmerror;
}

static bool brcmf_sdio_aos_no_decode(struct brcmf_sdio *bus)
{
	if (bus->ci->chip == CY_CC_43012_CHIP_ID ||
	    bus->ci->chip == CY_CC_43752_CHIP_ID)
		return true;
	else
		return false;
}

static void brcmf_sdio_sr_init(struct brcmf_sdio *bus)
{
	int err = 0;
	u8 val;
	u8 wakeupctrl;
	u8 cardcap;
	u8 chipclkcsr;

	brcmf_dbg(TRACE, "Enter\n");

	if (brcmf_chip_is_ulp(bus->ci)) {
		wakeupctrl = SBSDIO_FUNC1_WCTRL_ALPWAIT_SHIFT;
		chipclkcsr = SBSDIO_HT_AVAIL_REQ;
	} else {
		wakeupctrl = SBSDIO_FUNC1_WCTRL_HTWAIT_SHIFT;
		chipclkcsr = SBSDIO_FORCE_HT;
	}

	if (brcmf_sdio_aos_no_decode(bus)) {
		cardcap = SDIO_CCCR_BRCM_CARDCAP_CMD_NODEC;
	} else {
		cardcap = (SDIO_CCCR_BRCM_CARDCAP_CMD14_SUPPORT |
			   SDIO_CCCR_BRCM_CARDCAP_CMD14_EXT);
	}

	val = brcmf_sdiod_readb(bus->sdiodev, SBSDIO_FUNC1_WAKEUPCTRL, &err);
	if (err) {
		brcmf_err("error reading SBSDIO_FUNC1_WAKEUPCTRL\n");
		return;
	}
	val |= 1 << wakeupctrl;
	brcmf_sdiod_writeb(bus->sdiodev, SBSDIO_FUNC1_WAKEUPCTRL, val, &err);
	if (err) {
		brcmf_err("error writing SBSDIO_FUNC1_WAKEUPCTRL\n");
		return;
	}

	 
	brcmf_sdiod_func0_wb(bus->sdiodev, SDIO_CCCR_BRCM_CARDCAP,
			     cardcap,
			     &err);
	if (err) {
		brcmf_err("error writing SDIO_CCCR_BRCM_CARDCAP\n");
		return;
	}

	brcmf_sdiod_writeb(bus->sdiodev, SBSDIO_FUNC1_CHIPCLKCSR,
			   chipclkcsr, &err);
	if (err) {
		brcmf_err("error writing SBSDIO_FUNC1_CHIPCLKCSR\n");
		return;
	}

	 
	bus->sr_enabled = true;
	brcmf_dbg(INFO, "SR enabled\n");
}

 
static int brcmf_sdio_kso_init(struct brcmf_sdio *bus)
{
	struct brcmf_core *core = bus->sdio_core;
	u8 val;
	int err = 0;

	brcmf_dbg(TRACE, "Enter\n");

	 
	if (core->rev < 12)
		return 0;

	val = brcmf_sdiod_readb(bus->sdiodev, SBSDIO_FUNC1_SLEEPCSR, &err);
	if (err) {
		brcmf_err("error reading SBSDIO_FUNC1_SLEEPCSR\n");
		return err;
	}

	if (!(val & SBSDIO_FUNC1_SLEEPCSR_KSO_MASK)) {
		val |= (SBSDIO_FUNC1_SLEEPCSR_KSO_EN <<
			SBSDIO_FUNC1_SLEEPCSR_KSO_SHIFT);
		brcmf_sdiod_writeb(bus->sdiodev, SBSDIO_FUNC1_SLEEPCSR,
				   val, &err);
		if (err) {
			brcmf_err("error writing SBSDIO_FUNC1_SLEEPCSR\n");
			return err;
		}
	}

	return 0;
}


static int brcmf_sdio_bus_preinit(struct device *dev)
{
	struct brcmf_bus *bus_if = dev_get_drvdata(dev);
	struct brcmf_sdio_dev *sdiodev = bus_if->bus_priv.sdio;
	struct brcmf_sdio *bus = sdiodev->bus;
	struct brcmf_core *core = bus->sdio_core;
	u32 value;
	__le32 iovar;
	int err;

	 
	if (WARN_ON(!bus_if->maxctl))
		return -EINVAL;

	 
	bus_if->maxctl += bus->roundup;
	value = roundup((bus_if->maxctl + SDPCM_HDRLEN), ALIGNMENT);
	value += bus->head_align;
	bus->rxbuf = kmalloc(value, GFP_ATOMIC);
	if (bus->rxbuf)
		bus->rxblen = value;

	 
	if (core->rev < 12) {
		 
		iovar = 0;
		err = brcmf_iovar_data_set(dev, "bus:txglom", &iovar,
					   sizeof(iovar));
	} else {
		 
		value = sdiodev->settings->bus.sdio.sd_sgentry_align;
		 
		iovar = cpu_to_le32(max_t(u32, value, ALIGNMENT));
		err = brcmf_iovar_data_set(dev, "bus:txglomalign", &iovar,
					   sizeof(iovar));
	}

	if (err < 0)
		goto done;

	bus->tx_hdrlen = SDPCM_HWHDR_LEN + SDPCM_SWHDR_LEN;
	if (sdiodev->sg_support) {
		bus->txglom = false;
		iovar = cpu_to_le32(1);
		err = brcmf_iovar_data_set(bus->sdiodev->dev, "bus:rxglom",
					   &iovar, sizeof(iovar));
		if (err < 0) {
			 
			err = 0;
		} else {
			bus->txglom = true;
			bus->tx_hdrlen += SDPCM_HWEXT_LEN;
		}
	}
	brcmf_bus_add_txhdrlen(bus->sdiodev->dev, bus->tx_hdrlen);

done:
	return err;
}

static size_t brcmf_sdio_bus_get_ramsize(struct device *dev)
{
	struct brcmf_bus *bus_if = dev_get_drvdata(dev);
	struct brcmf_sdio_dev *sdiodev = bus_if->bus_priv.sdio;
	struct brcmf_sdio *bus = sdiodev->bus;

	return bus->ci->ramsize - bus->ci->srsize;
}

static int brcmf_sdio_bus_get_memdump(struct device *dev, void *data,
				      size_t mem_size)
{
	struct brcmf_bus *bus_if = dev_get_drvdata(dev);
	struct brcmf_sdio_dev *sdiodev = bus_if->bus_priv.sdio;
	struct brcmf_sdio *bus = sdiodev->bus;
	int err;
	int address;
	int offset;
	int len;

	brcmf_dbg(INFO, "dump at 0x%08x: size=%zu\n", bus->ci->rambase,
		  mem_size);

	address = bus->ci->rambase;
	offset = err = 0;
	sdio_claim_host(sdiodev->func1);
	while (offset < mem_size) {
		len = ((offset + MEMBLOCK) < mem_size) ? MEMBLOCK :
		      mem_size - offset;
		err = brcmf_sdiod_ramrw(sdiodev, false, address, data, len);
		if (err) {
			brcmf_err("error %d on reading %d membytes at 0x%08x\n",
				  err, len, address);
			goto done;
		}
		data += len;
		offset += len;
		address += len;
	}

done:
	sdio_release_host(sdiodev->func1);
	return err;
}

void brcmf_sdio_trigger_dpc(struct brcmf_sdio *bus)
{
	if (!bus->dpc_triggered) {
		bus->dpc_triggered = true;
		queue_work(bus->brcmf_wq, &bus->datawork);
	}
}

void brcmf_sdio_isr(struct brcmf_sdio *bus, bool in_isr)
{
	brcmf_dbg(TRACE, "Enter\n");

	if (!bus) {
		brcmf_err("bus is null pointer, exiting\n");
		return;
	}

	 
	bus->sdcnt.intrcount++;
	if (in_isr)
		atomic_set(&bus->ipend, 1);
	else
		if (brcmf_sdio_intr_rstatus(bus)) {
			brcmf_err("failed backplane access\n");
		}

	 
	if (!bus->intr)
		brcmf_err("isr w/o interrupt configured!\n");

	bus->dpc_triggered = true;
	queue_work(bus->brcmf_wq, &bus->datawork);
}

static void brcmf_sdio_bus_watchdog(struct brcmf_sdio *bus)
{
	brcmf_dbg(TIMER, "Enter\n");

	 
	if (!bus->sr_enabled &&
	    bus->poll && (++bus->polltick >= bus->pollrate)) {
		u32 intstatus = 0;

		 
		bus->polltick = 0;

		 
		if (!bus->intr ||
		    (bus->sdcnt.intrcount == bus->sdcnt.lastintrs)) {

			if (!bus->dpc_triggered) {
				u8 devpend;

				sdio_claim_host(bus->sdiodev->func1);
				devpend = brcmf_sdiod_func0_rb(bus->sdiodev,
						  SDIO_CCCR_INTx, NULL);
				sdio_release_host(bus->sdiodev->func1);
				intstatus = devpend & (INTR_STATUS_FUNC1 |
						       INTR_STATUS_FUNC2);
			}

			 
			if (intstatus) {
				bus->sdcnt.pollcnt++;
				atomic_set(&bus->ipend, 1);

				bus->dpc_triggered = true;
				queue_work(bus->brcmf_wq, &bus->datawork);
			}
		}

		 
		bus->sdcnt.lastintrs = bus->sdcnt.intrcount;
	}
#ifdef DEBUG
	 
	if (bus->sdiodev->state == BRCMF_SDIOD_DATA && BRCMF_FWCON_ON() &&
	    bus->console_interval != 0) {
		bus->console.count += jiffies_to_msecs(BRCMF_WD_POLL);
		if (bus->console.count >= bus->console_interval) {
			bus->console.count -= bus->console_interval;
			sdio_claim_host(bus->sdiodev->func1);
			 
			brcmf_sdio_bus_sleep(bus, false, false);
			if (brcmf_sdio_readconsole(bus) < 0)
				 
				bus->console_interval = 0;
			sdio_release_host(bus->sdiodev->func1);
		}
	}
#endif				 

	 
	if (!bus->dpc_triggered) {
		rmb();
		if ((!bus->dpc_running) && (bus->idletime > 0) &&
		    (bus->clkstate == CLK_AVAIL)) {
			bus->idlecount++;
			if (bus->idlecount > bus->idletime) {
				brcmf_dbg(SDIO, "idle\n");
				sdio_claim_host(bus->sdiodev->func1);
#ifdef DEBUG
				if (!BRCMF_FWCON_ON() ||
				    bus->console_interval == 0)
#endif
					brcmf_sdio_wd_timer(bus, false);
				bus->idlecount = 0;
				brcmf_sdio_bus_sleep(bus, true, false);
				sdio_release_host(bus->sdiodev->func1);
			}
		} else {
			bus->idlecount = 0;
		}
	} else {
		bus->idlecount = 0;
	}
}

static void brcmf_sdio_dataworker(struct work_struct *work)
{
	struct brcmf_sdio *bus = container_of(work, struct brcmf_sdio,
					      datawork);

	bus->dpc_running = true;
	wmb();
	while (READ_ONCE(bus->dpc_triggered)) {
		bus->dpc_triggered = false;
		brcmf_sdio_dpc(bus);
		bus->idlecount = 0;
	}
	bus->dpc_running = false;
	if (brcmf_sdiod_freezing(bus->sdiodev)) {
		brcmf_sdiod_change_state(bus->sdiodev, BRCMF_SDIOD_DOWN);
		brcmf_sdiod_try_freeze(bus->sdiodev);
		brcmf_sdiod_change_state(bus->sdiodev, BRCMF_SDIOD_DATA);
	}
}

static void
brcmf_sdio_drivestrengthinit(struct brcmf_sdio_dev *sdiodev,
			     struct brcmf_chip *ci, u32 drivestrength)
{
	const struct sdiod_drive_str *str_tab = NULL;
	u32 str_mask;
	u32 str_shift;
	u32 i;
	u32 drivestrength_sel = 0;
	u32 cc_data_temp;
	u32 addr;

	if (!(ci->cc_caps & CC_CAP_PMU))
		return;

	switch (SDIOD_DRVSTR_KEY(ci->chip, ci->pmurev)) {
	case SDIOD_DRVSTR_KEY(BRCM_CC_4330_CHIP_ID, 12):
		str_tab = sdiod_drvstr_tab1_1v8;
		str_mask = 0x00003800;
		str_shift = 11;
		break;
	case SDIOD_DRVSTR_KEY(BRCM_CC_4334_CHIP_ID, 17):
		str_tab = sdiod_drvstr_tab6_1v8;
		str_mask = 0x00001800;
		str_shift = 11;
		break;
	case SDIOD_DRVSTR_KEY(BRCM_CC_43143_CHIP_ID, 17):
		 
		i = ARRAY_SIZE(sdiod_drvstr_tab2_3v3) - 1;
		if (drivestrength >= sdiod_drvstr_tab2_3v3[i].strength) {
			str_tab = sdiod_drvstr_tab2_3v3;
			str_mask = 0x00000007;
			str_shift = 0;
		} else
			brcmf_err("Invalid SDIO Drive strength for chip %s, strength=%d\n",
				  ci->name, drivestrength);
		break;
	case SDIOD_DRVSTR_KEY(BRCM_CC_43362_CHIP_ID, 13):
		str_tab = sdiod_drive_strength_tab5_1v8;
		str_mask = 0x00003800;
		str_shift = 11;
		break;
	default:
		brcmf_dbg(INFO, "No SDIO driver strength init needed for chip %s rev %d pmurev %d\n",
			  ci->name, ci->chiprev, ci->pmurev);
		break;
	}

	if (str_tab != NULL) {
		struct brcmf_core *pmu = brcmf_chip_get_pmu(ci);

		for (i = 0; str_tab[i].strength != 0; i++) {
			if (drivestrength >= str_tab[i].strength) {
				drivestrength_sel = str_tab[i].sel;
				break;
			}
		}
		addr = CORE_CC_REG(pmu->base, chipcontrol_addr);
		brcmf_sdiod_writel(sdiodev, addr, 1, NULL);
		cc_data_temp = brcmf_sdiod_readl(sdiodev, addr, NULL);
		cc_data_temp &= ~str_mask;
		drivestrength_sel <<= str_shift;
		cc_data_temp |= drivestrength_sel;
		brcmf_sdiod_writel(sdiodev, addr, cc_data_temp, NULL);

		brcmf_dbg(INFO, "SDIO: %d mA (req=%d mA) drive strength selected, set to 0x%08x\n",
			  str_tab[i].strength, drivestrength, cc_data_temp);
	}
}

static int brcmf_sdio_buscoreprep(void *ctx)
{
	struct brcmf_sdio_dev *sdiodev = ctx;
	int err = 0;
	u8 clkval, clkset;

	 
	clkset = SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_ALP_AVAIL_REQ;
	brcmf_sdiod_writeb(sdiodev, SBSDIO_FUNC1_CHIPCLKCSR, clkset, &err);
	if (err) {
		brcmf_err("error writing for HT off\n");
		return err;
	}

	 
	 
	clkval = brcmf_sdiod_readb(sdiodev, SBSDIO_FUNC1_CHIPCLKCSR, NULL);

	if ((clkval & ~SBSDIO_AVBITS) != clkset) {
		brcmf_err("ChipClkCSR access: wrote 0x%02x read 0x%02x\n",
			  clkset, clkval);
		return -EACCES;
	}

	SPINWAIT(((clkval = brcmf_sdiod_readb(sdiodev, SBSDIO_FUNC1_CHIPCLKCSR,
					      NULL)),
		 !SBSDIO_ALPAV(clkval)),
		 PMU_MAX_TRANSITION_DLY);

	if (!SBSDIO_ALPAV(clkval)) {
		brcmf_err("timeout on ALPAV wait, clkval 0x%02x\n",
			  clkval);
		return -EBUSY;
	}

	clkset = SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_FORCE_ALP;
	brcmf_sdiod_writeb(sdiodev, SBSDIO_FUNC1_CHIPCLKCSR, clkset, &err);
	udelay(65);

	 
	brcmf_sdiod_writeb(sdiodev, SBSDIO_FUNC1_SDIOPULLUP, 0, NULL);

	return 0;
}

static void brcmf_sdio_buscore_activate(void *ctx, struct brcmf_chip *chip,
					u32 rstvec)
{
	struct brcmf_sdio_dev *sdiodev = ctx;
	struct brcmf_core *core = sdiodev->bus->sdio_core;
	u32 reg_addr;

	 
	reg_addr = core->base + SD_REG(intstatus);
	brcmf_sdiod_writel(sdiodev, reg_addr, 0xFFFFFFFF, NULL);

	if (rstvec)
		 
		brcmf_sdiod_ramrw(sdiodev, true, 0, (void *)&rstvec,
				  sizeof(rstvec));
}

static u32 brcmf_sdio_buscore_read32(void *ctx, u32 addr)
{
	struct brcmf_sdio_dev *sdiodev = ctx;
	u32 val, rev;

	val = brcmf_sdiod_readl(sdiodev, addr, NULL);

	 
	if (addr == CORE_CC_REG(SI_ENUM_BASE_DEFAULT, chipid) &&
	    (sdiodev->func1->device == SDIO_DEVICE_ID_BROADCOM_4339 ||
	     sdiodev->func1->device == SDIO_DEVICE_ID_BROADCOM_4335_4339)) {
		rev = (val & CID_REV_MASK) >> CID_REV_SHIFT;
		if (rev >= 2) {
			val &= ~CID_ID_MASK;
			val |= BRCM_CC_4339_CHIP_ID;
		}
	}

	return val;
}

static void brcmf_sdio_buscore_write32(void *ctx, u32 addr, u32 val)
{
	struct brcmf_sdio_dev *sdiodev = ctx;

	brcmf_sdiod_writel(sdiodev, addr, val, NULL);
}

static const struct brcmf_buscore_ops brcmf_sdio_buscore_ops = {
	.prepare = brcmf_sdio_buscoreprep,
	.activate = brcmf_sdio_buscore_activate,
	.read32 = brcmf_sdio_buscore_read32,
	.write32 = brcmf_sdio_buscore_write32,
};

static bool
brcmf_sdio_probe_attach(struct brcmf_sdio *bus)
{
	struct brcmf_sdio_dev *sdiodev;
	u8 clkctl = 0;
	int err = 0;
	int reg_addr;
	u32 reg_val;
	u32 drivestrength;
	u32 enum_base;

	sdiodev = bus->sdiodev;
	sdio_claim_host(sdiodev->func1);

	enum_base = brcmf_chip_enum_base(sdiodev->func1->device);

	pr_debug("F1 signature read @0x%08x=0x%4x\n", enum_base,
		 brcmf_sdiod_readl(sdiodev, enum_base, NULL));

	 

	brcmf_sdiod_writeb(sdiodev, SBSDIO_FUNC1_CHIPCLKCSR, BRCMF_INIT_CLKCTL1,
			   &err);
	if (!err)
		clkctl = brcmf_sdiod_readb(sdiodev, SBSDIO_FUNC1_CHIPCLKCSR,
					   &err);

	if (err || ((clkctl & ~SBSDIO_AVBITS) != BRCMF_INIT_CLKCTL1)) {
		brcmf_err("ChipClkCSR access: err %d wrote 0x%02x read 0x%02x\n",
			  err, BRCMF_INIT_CLKCTL1, clkctl);
		goto fail;
	}

	bus->ci = brcmf_chip_attach(sdiodev, sdiodev->func1->device,
				    &brcmf_sdio_buscore_ops);
	if (IS_ERR(bus->ci)) {
		brcmf_err("brcmf_chip_attach failed!\n");
		bus->ci = NULL;
		goto fail;
	}

	 
	bus->sdio_core   = brcmf_chip_get_core(bus->ci, BCMA_CORE_SDIO_DEV);
	if (!bus->sdio_core)
		goto fail;

	 
	sdiodev->cc_core = brcmf_chip_get_core(bus->ci, BCMA_CORE_CHIPCOMMON);
	if (!sdiodev->cc_core)
		goto fail;

	sdiodev->settings = brcmf_get_module_param(sdiodev->dev,
						   BRCMF_BUSTYPE_SDIO,
						   bus->ci->chip,
						   bus->ci->chiprev);
	if (!sdiodev->settings) {
		brcmf_err("Failed to get device parameters\n");
		goto fail;
	}
	 
	bus->head_align = ALIGNMENT;
	bus->sgentry_align = ALIGNMENT;
	if (sdiodev->settings->bus.sdio.sd_head_align > ALIGNMENT)
		bus->head_align = sdiodev->settings->bus.sdio.sd_head_align;
	if (sdiodev->settings->bus.sdio.sd_sgentry_align > ALIGNMENT)
		bus->sgentry_align =
				sdiodev->settings->bus.sdio.sd_sgentry_align;

	 
	brcmf_sdiod_sgtable_alloc(sdiodev);

	 
	if (IS_ENABLED(CONFIG_PM_SLEEP) &&
	    (sdio_get_host_pm_caps(sdiodev->func1) & MMC_PM_KEEP_POWER) &&
	    ((sdio_get_host_pm_caps(sdiodev->func1) & MMC_PM_WAKE_SDIO_IRQ) ||
	     (sdiodev->settings->bus.sdio.oob_irq_supported)))
		sdiodev->bus_if->wowl_supported = true;

	if (brcmf_sdio_kso_init(bus)) {
		brcmf_err("error enabling KSO\n");
		goto fail;
	}

	if (sdiodev->settings->bus.sdio.drive_strength)
		drivestrength = sdiodev->settings->bus.sdio.drive_strength;
	else
		drivestrength = DEFAULT_SDIO_DRIVE_STRENGTH;
	brcmf_sdio_drivestrengthinit(sdiodev, bus->ci, drivestrength);

	 
	reg_val = brcmf_sdiod_func0_rb(sdiodev, SDIO_CCCR_BRCM_CARDCTRL, &err);
	if (err)
		goto fail;

	reg_val |= SDIO_CCCR_BRCM_CARDCTRL_WLANRESET;

	brcmf_sdiod_func0_wb(sdiodev, SDIO_CCCR_BRCM_CARDCTRL, reg_val, &err);
	if (err)
		goto fail;

	 
	reg_addr = CORE_CC_REG(brcmf_chip_get_pmu(bus->ci)->base, pmucontrol);
	reg_val = brcmf_sdiod_readl(sdiodev, reg_addr, &err);
	if (err)
		goto fail;

	reg_val |= (BCMA_CC_PMU_CTL_RES_RELOAD << BCMA_CC_PMU_CTL_RES_SHIFT);

	brcmf_sdiod_writel(sdiodev, reg_addr, reg_val, &err);
	if (err)
		goto fail;

	sdio_release_host(sdiodev->func1);

	brcmu_pktq_init(&bus->txq, (PRIOMASK + 1), TXQLEN);

	 
	bus->hdrbuf = kzalloc(MAX_HDR_READ + bus->head_align, GFP_KERNEL);
	if (!bus->hdrbuf)
		return false;
	 
	bus->rxhdr = (u8 *) roundup((unsigned long)&bus->hdrbuf[0],
				    bus->head_align);

	 
	bus->intr = true;
	bus->poll = false;
	if (bus->poll)
		bus->pollrate = 1;

	return true;

fail:
	sdio_release_host(sdiodev->func1);
	return false;
}

static int
brcmf_sdio_watchdog_thread(void *data)
{
	struct brcmf_sdio *bus = (struct brcmf_sdio *)data;
	int wait;

	allow_signal(SIGTERM);
	 
	brcmf_sdiod_freezer_count(bus->sdiodev);
	while (1) {
		if (kthread_should_stop())
			break;
		brcmf_sdiod_freezer_uncount(bus->sdiodev);
		wait = wait_for_completion_interruptible(&bus->watchdog_wait);
		brcmf_sdiod_freezer_count(bus->sdiodev);
		brcmf_sdiod_try_freeze(bus->sdiodev);
		if (!wait) {
			brcmf_sdio_bus_watchdog(bus);
			 
			bus->sdcnt.tickcnt++;
			reinit_completion(&bus->watchdog_wait);
		} else
			break;
	}
	return 0;
}

static void
brcmf_sdio_watchdog(struct timer_list *t)
{
	struct brcmf_sdio *bus = from_timer(bus, t, timer);

	if (bus->watchdog_tsk) {
		complete(&bus->watchdog_wait);
		 
		if (bus->wd_active)
			mod_timer(&bus->timer,
				  jiffies + BRCMF_WD_POLL);
	}
}

static int brcmf_sdio_get_blob(struct device *dev, const struct firmware **fw,
			       enum brcmf_blob_type type)
{
	struct brcmf_bus *bus_if = dev_get_drvdata(dev);
	struct brcmf_sdio_dev *sdiodev = bus_if->bus_priv.sdio;

	switch (type) {
	case BRCMF_BLOB_CLM:
		*fw = sdiodev->clm_fw;
		sdiodev->clm_fw = NULL;
		break;
	default:
		return -ENOENT;
	}

	if (!*fw)
		return -ENOENT;

	return 0;
}

static int brcmf_sdio_bus_reset(struct device *dev)
{
	struct brcmf_bus *bus_if = dev_get_drvdata(dev);
	struct brcmf_sdio_dev *sdiodev = bus_if->bus_priv.sdio;

	brcmf_dbg(SDIO, "Enter\n");

	 
	brcmf_sdiod_intr_unregister(sdiodev);

	brcmf_sdiod_remove(sdiodev);

	 
	sdio_claim_host(sdiodev->func1);
	mmc_hw_reset(sdiodev->func1->card);
	sdio_release_host(sdiodev->func1);

	brcmf_bus_change_state(sdiodev->bus_if, BRCMF_BUS_DOWN);
	return 0;
}

static void brcmf_sdio_bus_remove(struct device *dev)
{
	struct brcmf_bus *bus_if = dev_get_drvdata(dev);
	struct brcmf_sdio_dev *sdiod = bus_if->bus_priv.sdio;

	device_release_driver(&sdiod->func2->dev);
	device_release_driver(&sdiod->func1->dev);
}

static const struct brcmf_bus_ops brcmf_sdio_bus_ops = {
	.stop = brcmf_sdio_bus_stop,
	.preinit = brcmf_sdio_bus_preinit,
	.txdata = brcmf_sdio_bus_txdata,
	.txctl = brcmf_sdio_bus_txctl,
	.rxctl = brcmf_sdio_bus_rxctl,
	.gettxq = brcmf_sdio_bus_gettxq,
	.wowl_config = brcmf_sdio_wowl_config,
	.get_ramsize = brcmf_sdio_bus_get_ramsize,
	.get_memdump = brcmf_sdio_bus_get_memdump,
	.get_blob = brcmf_sdio_get_blob,
	.debugfs_create = brcmf_sdio_debugfs_create,
	.reset = brcmf_sdio_bus_reset,
	.remove = brcmf_sdio_bus_remove,
};

#define BRCMF_SDIO_FW_CODE	0
#define BRCMF_SDIO_FW_NVRAM	1
#define BRCMF_SDIO_FW_CLM	2

static void brcmf_sdio_firmware_callback(struct device *dev, int err,
					 struct brcmf_fw_request *fwreq)
{
	struct brcmf_bus *bus_if = dev_get_drvdata(dev);
	struct brcmf_sdio_dev *sdiod = bus_if->bus_priv.sdio;
	struct brcmf_sdio *bus = sdiod->bus;
	struct brcmf_core *core = bus->sdio_core;
	const struct firmware *code;
	void *nvram;
	u32 nvram_len;
	u8 saveclk, bpreq;
	u8 devctl;

	brcmf_dbg(TRACE, "Enter: dev=%s, err=%d\n", dev_name(dev), err);

	if (err)
		goto fail;

	code = fwreq->items[BRCMF_SDIO_FW_CODE].binary;
	nvram = fwreq->items[BRCMF_SDIO_FW_NVRAM].nv_data.data;
	nvram_len = fwreq->items[BRCMF_SDIO_FW_NVRAM].nv_data.len;
	sdiod->clm_fw = fwreq->items[BRCMF_SDIO_FW_CLM].binary;
	kfree(fwreq);

	 
	bus->alp_only = true;
	err = brcmf_sdio_download_firmware(bus, code, nvram, nvram_len);
	if (err)
		goto fail;
	bus->alp_only = false;

	 
	bus->sdcnt.tickcnt = 0;
	brcmf_sdio_wd_timer(bus, true);

	sdio_claim_host(sdiod->func1);

	 
	brcmf_sdio_clkctl(bus, CLK_AVAIL, false);
	if (bus->clkstate != CLK_AVAIL)
		goto release;

	 
	saveclk = brcmf_sdiod_readb(sdiod, SBSDIO_FUNC1_CHIPCLKCSR, &err);
	if (!err) {
		bpreq = saveclk;
		bpreq |= brcmf_chip_is_ulp(bus->ci) ?
			SBSDIO_HT_AVAIL_REQ : SBSDIO_FORCE_HT;
		brcmf_sdiod_writeb(sdiod, SBSDIO_FUNC1_CHIPCLKCSR,
				   bpreq, &err);
	}
	if (err) {
		brcmf_err("Failed to force clock for F2: err %d\n", err);
		goto release;
	}

	 
	brcmf_sdiod_writel(sdiod, core->base + SD_REG(tosbmailboxdata),
			   SDPCM_PROT_VERSION << SMB_DATA_VERSION_SHIFT, NULL);

	err = sdio_enable_func(sdiod->func2);

	brcmf_dbg(INFO, "enable F2: err=%d\n", err);

	 
	if (!err) {
		 
		bus->hostintmask = HOSTINTMASK;
		brcmf_sdiod_writel(sdiod, core->base + SD_REG(hostintmask),
				   bus->hostintmask, NULL);

		switch (sdiod->func1->device) {
		case SDIO_DEVICE_ID_BROADCOM_CYPRESS_4373:
		case SDIO_DEVICE_ID_BROADCOM_CYPRESS_43752:
			brcmf_dbg(INFO, "set F2 watermark to 0x%x*4 bytes\n",
				  CY_4373_F2_WATERMARK);
			brcmf_sdiod_writeb(sdiod, SBSDIO_WATERMARK,
					   CY_4373_F2_WATERMARK, &err);
			devctl = brcmf_sdiod_readb(sdiod, SBSDIO_DEVICE_CTL,
						   &err);
			devctl |= SBSDIO_DEVCTL_F2WM_ENAB;
			brcmf_sdiod_writeb(sdiod, SBSDIO_DEVICE_CTL, devctl,
					   &err);
			brcmf_sdiod_writeb(sdiod, SBSDIO_FUNC1_MESBUSYCTRL,
					   CY_4373_F1_MESBUSYCTRL, &err);
			break;
		case SDIO_DEVICE_ID_BROADCOM_CYPRESS_43012:
			brcmf_dbg(INFO, "set F2 watermark to 0x%x*4 bytes\n",
				  CY_43012_F2_WATERMARK);
			brcmf_sdiod_writeb(sdiod, SBSDIO_WATERMARK,
					   CY_43012_F2_WATERMARK, &err);
			devctl = brcmf_sdiod_readb(sdiod, SBSDIO_DEVICE_CTL,
						   &err);
			devctl |= SBSDIO_DEVCTL_F2WM_ENAB;
			brcmf_sdiod_writeb(sdiod, SBSDIO_DEVICE_CTL, devctl,
					   &err);
			brcmf_sdiod_writeb(sdiod, SBSDIO_FUNC1_MESBUSYCTRL,
					   CY_43012_MESBUSYCTRL, &err);
			break;
		case SDIO_DEVICE_ID_BROADCOM_4329:
		case SDIO_DEVICE_ID_BROADCOM_4339:
			brcmf_dbg(INFO, "set F2 watermark to 0x%x*4 bytes\n",
				  CY_4339_F2_WATERMARK);
			brcmf_sdiod_writeb(sdiod, SBSDIO_WATERMARK,
					   CY_4339_F2_WATERMARK, &err);
			devctl = brcmf_sdiod_readb(sdiod, SBSDIO_DEVICE_CTL,
						   &err);
			devctl |= SBSDIO_DEVCTL_F2WM_ENAB;
			brcmf_sdiod_writeb(sdiod, SBSDIO_DEVICE_CTL, devctl,
					   &err);
			brcmf_sdiod_writeb(sdiod, SBSDIO_FUNC1_MESBUSYCTRL,
					   CY_4339_MESBUSYCTRL, &err);
			break;
		case SDIO_DEVICE_ID_BROADCOM_43455:
			brcmf_dbg(INFO, "set F2 watermark to 0x%x*4 bytes\n",
				  CY_43455_F2_WATERMARK);
			brcmf_sdiod_writeb(sdiod, SBSDIO_WATERMARK,
					   CY_43455_F2_WATERMARK, &err);
			devctl = brcmf_sdiod_readb(sdiod, SBSDIO_DEVICE_CTL,
						   &err);
			devctl |= SBSDIO_DEVCTL_F2WM_ENAB;
			brcmf_sdiod_writeb(sdiod, SBSDIO_DEVICE_CTL, devctl,
					   &err);
			brcmf_sdiod_writeb(sdiod, SBSDIO_FUNC1_MESBUSYCTRL,
					   CY_43455_MESBUSYCTRL, &err);
			break;
		case SDIO_DEVICE_ID_BROADCOM_4359:
		case SDIO_DEVICE_ID_BROADCOM_4354:
		case SDIO_DEVICE_ID_BROADCOM_4356:
			brcmf_dbg(INFO, "set F2 watermark to 0x%x*4 bytes\n",
				  CY_435X_F2_WATERMARK);
			brcmf_sdiod_writeb(sdiod, SBSDIO_WATERMARK,
					   CY_435X_F2_WATERMARK, &err);
			devctl = brcmf_sdiod_readb(sdiod, SBSDIO_DEVICE_CTL,
						   &err);
			devctl |= SBSDIO_DEVCTL_F2WM_ENAB;
			brcmf_sdiod_writeb(sdiod, SBSDIO_DEVICE_CTL, devctl,
					   &err);
			brcmf_sdiod_writeb(sdiod, SBSDIO_FUNC1_MESBUSYCTRL,
					   CY_435X_F1_MESBUSYCTRL, &err);
			break;
		default:
			brcmf_sdiod_writeb(sdiod, SBSDIO_WATERMARK,
					   DEFAULT_F2_WATERMARK, &err);
			break;
		}
	} else {
		 
		sdio_disable_func(sdiod->func2);
		goto checkdied;
	}

	if (brcmf_chip_sr_capable(bus->ci)) {
		brcmf_sdio_sr_init(bus);
	} else {
		 
		brcmf_sdiod_writeb(sdiod, SBSDIO_FUNC1_CHIPCLKCSR,
				   saveclk, &err);
	}

	if (err == 0) {
		 
		sdiod->bus_if->dev = sdiod->dev;
		sdiod->bus_if->ops = &brcmf_sdio_bus_ops;
		sdiod->bus_if->chip = bus->ci->chip;
		sdiod->bus_if->chiprev = bus->ci->chiprev;

		 
		brcmf_sdiod_change_state(bus->sdiodev, BRCMF_SDIOD_DATA);

		err = brcmf_sdiod_intr_register(sdiod);
		if (err != 0)
			brcmf_err("intr register failed:%d\n", err);
	}

	 
	if (err != 0) {
		brcmf_sdio_clkctl(bus, CLK_NONE, false);
		goto checkdied;
	}

	sdio_release_host(sdiod->func1);

	err = brcmf_alloc(sdiod->dev, sdiod->settings);
	if (err) {
		brcmf_err("brcmf_alloc failed\n");
		goto claim;
	}

	 
	err = brcmf_attach(sdiod->dev);
	if (err != 0) {
		brcmf_err("brcmf_attach failed\n");
		goto free;
	}

	 
	return;

free:
	brcmf_free(sdiod->dev);
claim:
	sdio_claim_host(sdiod->func1);
checkdied:
	brcmf_sdio_checkdied(bus);
release:
	sdio_release_host(sdiod->func1);
fail:
	brcmf_dbg(TRACE, "failed: dev=%s, err=%d\n", dev_name(dev), err);
	device_release_driver(&sdiod->func2->dev);
	device_release_driver(dev);
}

static struct brcmf_fw_request *
brcmf_sdio_prepare_fw_request(struct brcmf_sdio *bus)
{
	struct brcmf_fw_request *fwreq;
	struct brcmf_fw_name fwnames[] = {
		{ ".bin", bus->sdiodev->fw_name },
		{ ".txt", bus->sdiodev->nvram_name },
		{ ".clm_blob", bus->sdiodev->clm_name },
	};

	fwreq = brcmf_fw_alloc_request(bus->ci->chip, bus->ci->chiprev,
				       brcmf_sdio_fwnames,
				       ARRAY_SIZE(brcmf_sdio_fwnames),
				       fwnames, ARRAY_SIZE(fwnames));
	if (!fwreq)
		return NULL;

	fwreq->items[BRCMF_SDIO_FW_CODE].type = BRCMF_FW_TYPE_BINARY;
	fwreq->items[BRCMF_SDIO_FW_NVRAM].type = BRCMF_FW_TYPE_NVRAM;
	fwreq->items[BRCMF_SDIO_FW_CLM].type = BRCMF_FW_TYPE_BINARY;
	fwreq->items[BRCMF_SDIO_FW_CLM].flags = BRCMF_FW_REQF_OPTIONAL;
	fwreq->board_types[0] = bus->sdiodev->settings->board_type;

	return fwreq;
}

struct brcmf_sdio *brcmf_sdio_probe(struct brcmf_sdio_dev *sdiodev)
{
	int ret;
	struct brcmf_sdio *bus;
	struct workqueue_struct *wq;
	struct brcmf_fw_request *fwreq;

	brcmf_dbg(TRACE, "Enter\n");

	 
	bus = kzalloc(sizeof(struct brcmf_sdio), GFP_ATOMIC);
	if (!bus)
		goto fail;

	bus->sdiodev = sdiodev;
	sdiodev->bus = bus;
	skb_queue_head_init(&bus->glom);
	bus->txbound = BRCMF_TXBOUND;
	bus->rxbound = BRCMF_RXBOUND;
	bus->txminmax = BRCMF_TXMINMAX;
	bus->tx_seq = SDPCM_SEQ_WRAP - 1;

	 
	wq = alloc_ordered_workqueue("brcmf_wq/%s", WQ_MEM_RECLAIM | WQ_HIGHPRI,
				     dev_name(&sdiodev->func1->dev));
	if (!wq) {
		brcmf_err("insufficient memory to create txworkqueue\n");
		goto fail;
	}
	brcmf_sdiod_freezer_count(sdiodev);
	INIT_WORK(&bus->datawork, brcmf_sdio_dataworker);
	bus->brcmf_wq = wq;

	 
	if (!(brcmf_sdio_probe_attach(bus))) {
		brcmf_err("brcmf_sdio_probe_attach failed\n");
		goto fail;
	}

	spin_lock_init(&bus->rxctl_lock);
	spin_lock_init(&bus->txq_lock);
	init_waitqueue_head(&bus->ctrl_wait);
	init_waitqueue_head(&bus->dcmd_resp_wait);

	 
	timer_setup(&bus->timer, brcmf_sdio_watchdog, 0);
	 
	init_completion(&bus->watchdog_wait);
	bus->watchdog_tsk = kthread_run(brcmf_sdio_watchdog_thread,
					bus, "brcmf_wdog/%s",
					dev_name(&sdiodev->func1->dev));
	if (IS_ERR(bus->watchdog_tsk)) {
		pr_warn("brcmf_watchdog thread failed to start\n");
		bus->watchdog_tsk = NULL;
	}
	 
	bus->dpc_triggered = false;
	bus->dpc_running = false;

	 
	bus->tx_hdrlen = SDPCM_HWHDR_LEN + SDPCM_SWHDR_LEN;

	 
	bus->blocksize = bus->sdiodev->func2->cur_blksize;
	bus->roundup = min(max_roundup, bus->blocksize);

	sdio_claim_host(bus->sdiodev->func1);

	 
	sdio_disable_func(bus->sdiodev->func2);

	bus->rxflow = false;

	 
	brcmf_sdiod_writeb(bus->sdiodev, SBSDIO_FUNC1_CHIPCLKCSR, 0, NULL);

	sdio_release_host(bus->sdiodev->func1);

	 
	bus->clkstate = CLK_SDONLY;
	bus->idletime = BRCMF_IDLE_INTERVAL;
	bus->idleclock = BRCMF_IDLE_ACTIVE;

	 
	bus->sr_enabled = false;

	brcmf_dbg(INFO, "completed!!\n");

	fwreq = brcmf_sdio_prepare_fw_request(bus);
	if (!fwreq) {
		ret = -ENOMEM;
		goto fail;
	}

	ret = brcmf_fw_get_firmwares(sdiodev->dev, fwreq,
				     brcmf_sdio_firmware_callback);
	if (ret != 0) {
		brcmf_err("async firmware request failed: %d\n", ret);
		kfree(fwreq);
		goto fail;
	}

	return bus;

fail:
	brcmf_sdio_remove(bus);
	return NULL;
}

 
void brcmf_sdio_remove(struct brcmf_sdio *bus)
{
	brcmf_dbg(TRACE, "Enter\n");

	if (bus) {
		 
		if (bus->watchdog_tsk) {
			send_sig(SIGTERM, bus->watchdog_tsk, 1);
			kthread_stop(bus->watchdog_tsk);
			bus->watchdog_tsk = NULL;
		}

		 
		brcmf_sdiod_intr_unregister(bus->sdiodev);

		brcmf_detach(bus->sdiodev->dev);
		brcmf_free(bus->sdiodev->dev);

		cancel_work_sync(&bus->datawork);
		if (bus->brcmf_wq)
			destroy_workqueue(bus->brcmf_wq);

		if (bus->ci) {
			if (bus->sdiodev->state != BRCMF_SDIOD_NOMEDIUM) {
				sdio_claim_host(bus->sdiodev->func1);
				brcmf_sdio_wd_timer(bus, false);
				brcmf_sdio_clkctl(bus, CLK_AVAIL, false);
				 
				msleep(20);
				brcmf_chip_set_passive(bus->ci);
				brcmf_sdio_clkctl(bus, CLK_NONE, false);
				sdio_release_host(bus->sdiodev->func1);
			}
			brcmf_chip_detach(bus->ci);
		}
		if (bus->sdiodev->settings)
			brcmf_release_module_param(bus->sdiodev->settings);

		release_firmware(bus->sdiodev->clm_fw);
		bus->sdiodev->clm_fw = NULL;
		kfree(bus->rxbuf);
		kfree(bus->hdrbuf);
		kfree(bus);
	}

	brcmf_dbg(TRACE, "Disconnected\n");
}

void brcmf_sdio_wd_timer(struct brcmf_sdio *bus, bool active)
{
	 
	if (!active && bus->wd_active) {
		del_timer_sync(&bus->timer);
		bus->wd_active = false;
		return;
	}

	 
	if (bus->sdiodev->state != BRCMF_SDIOD_DATA)
		return;

	if (active) {
		if (!bus->wd_active) {
			 
			bus->timer.expires = jiffies + BRCMF_WD_POLL;
			add_timer(&bus->timer);
			bus->wd_active = true;
		} else {
			 
			mod_timer(&bus->timer, jiffies + BRCMF_WD_POLL);
		}
	}
}

int brcmf_sdio_sleep(struct brcmf_sdio *bus, bool sleep)
{
	int ret;

	sdio_claim_host(bus->sdiodev->func1);
	ret = brcmf_sdio_bus_sleep(bus, sleep, false);
	sdio_release_host(bus->sdiodev->func1);

	return ret;
}
