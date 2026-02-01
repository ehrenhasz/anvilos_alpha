
 

#ifndef	BRCMFMAC_SDIO_H
#define	BRCMFMAC_SDIO_H

#include <linux/skbuff.h>
#include <linux/firmware.h>
#include "firmware.h"

#define SDIOD_FBR_SIZE		0x100

 
#define SDIO_FUNC_ENABLE_1	0x02
#define SDIO_FUNC_ENABLE_2	0x04

 
#define SDIO_FUNC_READY_1	0x02
#define SDIO_FUNC_READY_2	0x04

 
#define INTR_STATUS_FUNC1	0x2
#define INTR_STATUS_FUNC2	0x4

 
#define REG_F0_REG_MASK		0x7FF
#define REG_F1_MISC_MASK	0x1FFFF

 

#define SDIO_CCCR_BRCM_CARDCAP			0xf0
#define SDIO_CCCR_BRCM_CARDCAP_CMD14_SUPPORT	BIT(1)
#define SDIO_CCCR_BRCM_CARDCAP_CMD14_EXT	BIT(2)
#define SDIO_CCCR_BRCM_CARDCAP_CMD_NODEC	BIT(3)

 
#define SDIO_CCCR_IEN_FUNC0			BIT(0)
#define SDIO_CCCR_IEN_FUNC1			BIT(1)
#define SDIO_CCCR_IEN_FUNC2			BIT(2)

#define SDIO_CCCR_BRCM_CARDCTRL			0xf1
#define SDIO_CCCR_BRCM_CARDCTRL_WLANRESET	BIT(1)

#define SDIO_CCCR_BRCM_SEPINT			0xf2
#define SDIO_CCCR_BRCM_SEPINT_MASK		BIT(0)
#define SDIO_CCCR_BRCM_SEPINT_OE		BIT(1)
#define SDIO_CCCR_BRCM_SEPINT_ACT_HI		BIT(2)

 

 
#define SBSDIO_SPROM_CS			0x10000
 
#define SBSDIO_SPROM_INFO		0x10001
 
#define SBSDIO_SPROM_DATA_LOW		0x10002
 
#define SBSDIO_SPROM_DATA_HIGH		0x10003
 
#define SBSDIO_SPROM_ADDR_LOW		0x10004
 
#define SBSDIO_GPIO_SELECT		0x10005
 
#define SBSDIO_GPIO_OUT			0x10006
 
#define SBSDIO_GPIO_EN			0x10007
 
#define SBSDIO_WATERMARK		0x10008
 
#define SBSDIO_DEVICE_CTL		0x10009

 
#define SBSDIO_FUNC1_SBADDRLOW		0x1000A
 
#define SBSDIO_FUNC1_SBADDRMID		0x1000B
 
#define SBSDIO_FUNC1_SBADDRHIGH		0x1000C
 
#define SBSDIO_FUNC1_FRAMECTRL		0x1000D
 
#define SBSDIO_FUNC1_CHIPCLKCSR		0x1000E
 
#define SBSDIO_FUNC1_SDIOPULLUP		0x1000F
 
#define SBSDIO_FUNC1_WFRAMEBCLO		0x10019
 
#define SBSDIO_FUNC1_WFRAMEBCHI		0x1001A
 
#define SBSDIO_FUNC1_RFRAMEBCLO		0x1001B
 
#define SBSDIO_FUNC1_RFRAMEBCHI		0x1001C
 
#define SBSDIO_FUNC1_MESBUSYCTRL	0x1001D
 
#define SBSDIO_MESBUSY_RXFIFO_WM_MASK	0x7F
#define SBSDIO_MESBUSY_RXFIFO_WM_SHIFT	0
 
#define SBSDIO_MESBUSYCTRL_ENAB		0x80
#define SBSDIO_MESBUSYCTRL_ENAB_SHIFT	7

 
#define SBSDIO_FUNC1_WAKEUPCTRL		0x1001E
#define SBSDIO_FUNC1_WCTRL_ALPWAIT_MASK		0x1
#define SBSDIO_FUNC1_WCTRL_ALPWAIT_SHIFT	0
#define SBSDIO_FUNC1_WCTRL_HTWAIT_MASK		0x2
#define SBSDIO_FUNC1_WCTRL_HTWAIT_SHIFT		1
#define SBSDIO_FUNC1_SLEEPCSR		0x1001F
#define SBSDIO_FUNC1_SLEEPCSR_KSO_MASK		0x1
#define SBSDIO_FUNC1_SLEEPCSR_KSO_SHIFT		0
#define SBSDIO_FUNC1_SLEEPCSR_KSO_EN		1
#define SBSDIO_FUNC1_SLEEPCSR_DEVON_MASK	0x2
#define SBSDIO_FUNC1_SLEEPCSR_DEVON_SHIFT	1

#define SBSDIO_FUNC1_MISC_REG_START	0x10000	 
#define SBSDIO_FUNC1_MISC_REG_LIMIT	0x1001F	 

 

 
#define SBSDIO_SB_OFT_ADDR_MASK		0x07FFF
#define SBSDIO_SB_OFT_ADDR_LIMIT	0x08000
 
#define SBSDIO_SB_ACCESS_2_4B_FLAG	0x08000

 
#define SBSDIO_SBWINDOW_MASK		0xffff8000

#define SDIOH_READ              0	 
#define SDIOH_WRITE             1	 

#define SDIOH_DATA_FIX          0	 
#define SDIOH_DATA_INC          1	 

 
#define SUCCESS	0
#define ERROR	1

 
#define BRCMF_SDALIGN	(1 << 6)

 
#define BRCMF_WD_POLL	msecs_to_jiffies(10)

 
enum brcmf_sdiod_state {
	BRCMF_SDIOD_DOWN,
	BRCMF_SDIOD_DATA,
	BRCMF_SDIOD_NOMEDIUM
};

struct brcmf_sdreg {
	int func;
	int offset;
	int value;
};

struct brcmf_sdio;
struct brcmf_sdiod_freezer;

struct brcmf_sdio_dev {
	struct sdio_func *func1;
	struct sdio_func *func2;
	u32 sbwad;			 
	struct brcmf_core *cc_core;	 
	struct brcmf_sdio *bus;
	struct device *dev;
	struct brcmf_bus *bus_if;
	struct brcmf_mp_device *settings;
	bool oob_irq_requested;
	bool sd_irq_requested;
	bool irq_en;			 
	spinlock_t irq_en_lock;
	bool sg_support;
	uint max_request_size;
	ushort max_segment_count;
	uint max_segment_size;
	uint txglomsz;
	struct sg_table sgtable;
	char fw_name[BRCMF_FW_NAME_LEN];
	char nvram_name[BRCMF_FW_NAME_LEN];
	char clm_name[BRCMF_FW_NAME_LEN];
	bool wowl_enabled;
	bool func1_power_manageable;
	bool func2_power_manageable;
	enum brcmf_sdiod_state state;
	struct brcmf_sdiod_freezer *freezer;
	const struct firmware *clm_fw;
};

 
struct sdpcmd_regs {
	u32 corecontrol;		 
	u32 corestatus;			 
	u32 PAD[1];
	u32 biststatus;			 

	 
	u16 pcmciamesportaladdr;	 
	u16 PAD[1];
	u16 pcmciamesportalmask;	 
	u16 PAD[1];
	u16 pcmciawrframebc;		 
	u16 PAD[1];
	u16 pcmciaunderflowtimer;	 
	u16 PAD[1];

	 
	u32 intstatus;			 
	u32 hostintmask;		 
	u32 intmask;			 
	u32 sbintstatus;		 
	u32 sbintmask;			 
	u32 funcintmask;		 
	u32 PAD[2];
	u32 tosbmailbox;		 
	u32 tohostmailbox;		 
	u32 tosbmailboxdata;		 
	u32 tohostmailboxdata;		 

	 
	u32 sdioaccess;			 
	u32 PAD[3];

	 
	u8 pcmciaframectrl;		 
	u8 PAD[3];
	u8 pcmciawatermark;		 
	u8 PAD[155];

	 
	u32 intrcvlazy;			 
	u32 PAD[3];

	 
	u32 cmd52rd;			 
	u32 cmd52wr;			 
	u32 cmd53rd;			 
	u32 cmd53wr;			 
	u32 abort;			 
	u32 datacrcerror;		 
	u32 rdoutofsync;		 
	u32 wroutofsync;		 
	u32 writebusy;			 
	u32 readwait;			 
	u32 readterm;			 
	u32 writeterm;			 
	u32 PAD[40];
	u32 clockctlstatus;		 
	u32 PAD[7];

	u32 PAD[128];			 

	 
	char cis[512];			 

	 
	char pcmciafcr[256];		 
	u16 PAD[55];

	 
	u16 backplanecsr;		 
	u16 backplaneaddr0;		 
	u16 backplaneaddr1;		 
	u16 backplaneaddr2;		 
	u16 backplaneaddr3;		 
	u16 backplanedata0;		 
	u16 backplanedata1;		 
	u16 backplanedata2;		 
	u16 backplanedata3;		 
	u16 PAD[31];

	 
	u16 spromstatus;		 
	u32 PAD[464];

	u16 PAD[0x80];
};

 
int brcmf_sdiod_intr_register(struct brcmf_sdio_dev *sdiodev);
void brcmf_sdiod_intr_unregister(struct brcmf_sdio_dev *sdiodev);

 
 
#define brcmf_sdiod_func0_rb(sdiodev, addr, r) \
	sdio_f0_readb((sdiodev)->func1, (addr), (r))

#define brcmf_sdiod_func0_wb(sdiodev, addr, v, ret) \
	sdio_f0_writeb((sdiodev)->func1, (v), (addr), (ret))

 
#define brcmf_sdiod_readb(sdiodev, addr, r) \
	sdio_readb((sdiodev)->func1, (addr), (r))

#define brcmf_sdiod_writeb(sdiodev, addr, v, ret) \
	sdio_writeb((sdiodev)->func1, (v), (addr), (ret))

u32 brcmf_sdiod_readl(struct brcmf_sdio_dev *sdiodev, u32 addr, int *ret);
void brcmf_sdiod_writel(struct brcmf_sdio_dev *sdiodev, u32 addr, u32 data,
			int *ret);

 
int brcmf_sdiod_send_pkt(struct brcmf_sdio_dev *sdiodev,
			 struct sk_buff_head *pktq);
int brcmf_sdiod_send_buf(struct brcmf_sdio_dev *sdiodev, u8 *buf, uint nbytes);

int brcmf_sdiod_recv_pkt(struct brcmf_sdio_dev *sdiodev, struct sk_buff *pkt);
int brcmf_sdiod_recv_buf(struct brcmf_sdio_dev *sdiodev, u8 *buf, uint nbytes);
int brcmf_sdiod_recv_chain(struct brcmf_sdio_dev *sdiodev,
			   struct sk_buff_head *pktq, uint totlen);

 

 
#define SDIO_REQ_4BYTE	0x1
 
#define SDIO_REQ_FIXED	0x2

 
int brcmf_sdiod_ramrw(struct brcmf_sdio_dev *sdiodev, bool write, u32 address,
		      u8 *data, uint size);

 
int brcmf_sdiod_abort(struct brcmf_sdio_dev *sdiodev, struct sdio_func *func);

void brcmf_sdiod_sgtable_alloc(struct brcmf_sdio_dev *sdiodev);
void brcmf_sdiod_change_state(struct brcmf_sdio_dev *sdiodev,
			      enum brcmf_sdiod_state state);
bool brcmf_sdiod_freezing(struct brcmf_sdio_dev *sdiodev);
void brcmf_sdiod_try_freeze(struct brcmf_sdio_dev *sdiodev);
void brcmf_sdiod_freezer_count(struct brcmf_sdio_dev *sdiodev);
void brcmf_sdiod_freezer_uncount(struct brcmf_sdio_dev *sdiodev);

int brcmf_sdiod_probe(struct brcmf_sdio_dev *sdiodev);
int brcmf_sdiod_remove(struct brcmf_sdio_dev *sdiodev);

struct brcmf_sdio *brcmf_sdio_probe(struct brcmf_sdio_dev *sdiodev);
void brcmf_sdio_remove(struct brcmf_sdio *bus);
void brcmf_sdio_isr(struct brcmf_sdio *bus, bool in_isr);

void brcmf_sdio_wd_timer(struct brcmf_sdio *bus, bool active);
void brcmf_sdio_wowl_config(struct device *dev, bool enabled);
int brcmf_sdio_sleep(struct brcmf_sdio *bus, bool sleep);
void brcmf_sdio_trigger_dpc(struct brcmf_sdio *bus);

#endif  
