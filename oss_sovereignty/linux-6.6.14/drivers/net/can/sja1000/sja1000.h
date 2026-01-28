#ifndef SJA1000_DEV_H
#define SJA1000_DEV_H
#include <linux/irqreturn.h>
#include <linux/can/dev.h>
#include <linux/can/platform/sja1000.h>
#define SJA1000_ECHO_SKB_MAX	1  
#define SJA1000_MAX_IRQ 20	 
#define SJA1000_MOD		0x00
#define SJA1000_CMR		0x01
#define SJA1000_SR		0x02
#define SJA1000_IR		0x03
#define SJA1000_IER		0x04
#define SJA1000_ALC		0x0B
#define SJA1000_ECC		0x0C
#define SJA1000_EWL		0x0D
#define SJA1000_RXERR		0x0E
#define SJA1000_TXERR		0x0F
#define SJA1000_ACCC0		0x10
#define SJA1000_ACCC1		0x11
#define SJA1000_ACCC2		0x12
#define SJA1000_ACCC3		0x13
#define SJA1000_ACCM0		0x14
#define SJA1000_ACCM1		0x15
#define SJA1000_ACCM2		0x16
#define SJA1000_ACCM3		0x17
#define SJA1000_RMC		0x1D
#define SJA1000_RBSA		0x1E
#define SJA1000_BTR0		0x06
#define SJA1000_BTR1		0x07
#define SJA1000_OCR		0x08
#define SJA1000_CDR		0x1F
#define SJA1000_FI		0x10
#define SJA1000_SFF_BUF		0x13
#define SJA1000_EFF_BUF		0x15
#define SJA1000_FI_FF		0x80
#define SJA1000_FI_RTR		0x40
#define SJA1000_ID1		0x11
#define SJA1000_ID2		0x12
#define SJA1000_ID3		0x13
#define SJA1000_ID4		0x14
#define SJA1000_CAN_RAM		0x20
#define MOD_RM		0x01
#define MOD_LOM		0x02
#define MOD_STM		0x04
#define MOD_AFM		0x08
#define MOD_SM		0x10
#define CMD_SRR		0x10
#define CMD_CDO		0x08
#define CMD_RRB		0x04
#define CMD_AT		0x02
#define CMD_TR		0x01
#define IRQ_BEI		0x80
#define IRQ_ALI		0x40
#define IRQ_EPI		0x20
#define IRQ_WUI		0x10
#define IRQ_DOI		0x08
#define IRQ_EI		0x04
#define IRQ_TI		0x02
#define IRQ_RI		0x01
#define IRQ_ALL		0xFF
#define IRQ_OFF		0x00
#define SR_BS		0x80
#define SR_ES		0x40
#define SR_TS		0x20
#define SR_RS		0x10
#define SR_TCS		0x08
#define SR_TBS		0x04
#define SR_DOS		0x02
#define SR_RBS		0x01
#define SR_CRIT (SR_BS|SR_ES)
#define ECC_SEG		0x1F
#define ECC_DIR		0x20
#define ECC_ERR		6
#define ECC_BIT		0x00
#define ECC_FORM	0x40
#define ECC_STUFF	0x80
#define ECC_MASK	0xc0
#define SJA1000_CUSTOM_IRQ_HANDLER	BIT(0)
#define SJA1000_QUIRK_NO_CDR_REG	BIT(1)
#define SJA1000_QUIRK_RESET_ON_OVERRUN	BIT(2)
struct sja1000_priv {
	struct can_priv can;	 
	struct sk_buff *echo_skb;
	u8 (*read_reg) (const struct sja1000_priv *priv, int reg);
	void (*write_reg) (const struct sja1000_priv *priv, int reg, u8 val);
	void (*pre_irq) (const struct sja1000_priv *priv);
	void (*post_irq) (const struct sja1000_priv *priv);
	void *priv;		 
	struct net_device *dev;
	void __iomem *reg_base;	  
	unsigned long irq_flags;  
	spinlock_t cmdreg_lock;   
	u16 flags;		 
	u8 ocr;			 
	u8 cdr;			 
};
struct net_device *alloc_sja1000dev(int sizeof_priv);
void free_sja1000dev(struct net_device *dev);
int register_sja1000dev(struct net_device *dev);
void unregister_sja1000dev(struct net_device *dev);
irqreturn_t sja1000_interrupt(int irq, void *dev_id);
#endif  
