
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/pnp.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/leds.h>
#include <linux/spinlock.h>
#include <linux/pci_ids.h>
#include <linux/io.h>
#include <linux/bitrev.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <media/rc-core.h>

#define DRVNAME "winbond-cir"

 
#define WBCIR_REG_WCEIR_CTL	0x03  
#define WBCIR_REG_WCEIR_STS	0x04  
#define WBCIR_REG_WCEIR_EV_EN	0x05  
#define WBCIR_REG_WCEIR_CNTL	0x06  
#define WBCIR_REG_WCEIR_CNTH	0x07  
#define WBCIR_REG_WCEIR_INDEX	0x08  
#define WBCIR_REG_WCEIR_DATA	0x09  
#define WBCIR_REG_WCEIR_CSL	0x0A  
#define WBCIR_REG_WCEIR_CFG1	0x0B  
#define WBCIR_REG_WCEIR_CFG2	0x0C  

 
#define WBCIR_REG_ECEIR_CTS	0x00  
#define WBCIR_REG_ECEIR_CCTL	0x01  
#define WBCIR_REG_ECEIR_CNT_LO	0x02  
#define WBCIR_REG_ECEIR_CNT_HI	0x03  
#define WBCIR_REG_ECEIR_IREM	0x04  

 
#define WBCIR_REG_SP3_BSR	0x03  
				       
#define WBCIR_REG_SP3_RXDATA	0x00  
#define WBCIR_REG_SP3_TXDATA	0x00  
#define WBCIR_REG_SP3_IER	0x01  
#define WBCIR_REG_SP3_EIR	0x02  
#define WBCIR_REG_SP3_FCR	0x02  
#define WBCIR_REG_SP3_MCR	0x04  
#define WBCIR_REG_SP3_LSR	0x05  
#define WBCIR_REG_SP3_MSR	0x06  
#define WBCIR_REG_SP3_ASCR	0x07  
				       
#define WBCIR_REG_SP3_BGDL	0x00  
#define WBCIR_REG_SP3_BGDH	0x01  
#define WBCIR_REG_SP3_EXCR1	0x02  
#define WBCIR_REG_SP3_EXCR2	0x04  
#define WBCIR_REG_SP3_TXFLV	0x06  
#define WBCIR_REG_SP3_RXFLV	0x07  
				       
#define WBCIR_REG_SP3_MRID	0x00  
#define WBCIR_REG_SP3_SH_LCR	0x01  
#define WBCIR_REG_SP3_SH_FCR	0x02  
				       
#define WBCIR_REG_SP3_IRCR1	0x02  
				       
#define WBCIR_REG_SP3_IRCR2	0x04  
				       
#define WBCIR_REG_SP3_IRCR3	0x00  
#define WBCIR_REG_SP3_SIR_PW	0x02  
				       
#define WBCIR_REG_SP3_IRRXDC	0x00  
#define WBCIR_REG_SP3_IRTXMC	0x01  
#define WBCIR_REG_SP3_RCCFG	0x02  
#define WBCIR_REG_SP3_IRCFG1	0x04  
#define WBCIR_REG_SP3_IRCFG4	0x07  

 

 
#define WBCIR_IRQ_NONE		0x00
 
#define WBCIR_IRQ_RX		0x01
 
#define WBCIR_IRQ_TX_LOW	0x02
 
#define WBCIR_IRQ_ERR		0x04
 
#define WBCIR_IRQ_TX_EMPTY	0x20
 
#define WBCIR_LED_ENABLE	0x80
 
#define WBCIR_RX_AVAIL		0x01
 
#define WBCIR_RX_OVERRUN	0x02
 
#define WBCIR_TX_EOT		0x04
 
#define WBCIR_RX_DISABLE	0x20
 
#define WBCIR_TX_UNDERRUN	0x40
 
#define WBCIR_EXT_ENABLE	0x01
 
#define WBCIR_REGSEL_COMPARE	0x10
 
#define WBCIR_REGSEL_MASK	0x20
 
#define WBCIR_REG_ADDR0		0x00
 
#define WBCIR_CNTR_EN		0x01
 
#define WBCIR_CNTR_R		0x02
 
#define WBCIR_IRTX_INV		0x04
 
#define WBCIR_RX_T_OV		0x40

 
enum wbcir_bank {
	WBCIR_BANK_0          = 0x00,
	WBCIR_BANK_1          = 0x80,
	WBCIR_BANK_2          = 0xE0,
	WBCIR_BANK_3          = 0xE4,
	WBCIR_BANK_4          = 0xE8,
	WBCIR_BANK_5          = 0xEC,
	WBCIR_BANK_6          = 0xF0,
	WBCIR_BANK_7          = 0xF4,
};

 
enum wbcir_protocol {
	IR_PROTOCOL_RC5          = 0x0,
	IR_PROTOCOL_NEC          = 0x1,
	IR_PROTOCOL_RC6          = 0x2,
};

 
enum wbcir_rxstate {
	WBCIR_RXSTATE_INACTIVE = 0,
	WBCIR_RXSTATE_ACTIVE,
	WBCIR_RXSTATE_ERROR
};

 
enum wbcir_txstate {
	WBCIR_TXSTATE_INACTIVE = 0,
	WBCIR_TXSTATE_ACTIVE,
	WBCIR_TXSTATE_ERROR
};

 
#define WBCIR_NAME	"Winbond CIR"
#define WBCIR_ID_FAMILY          0xF1  
#define	WBCIR_ID_CHIP            0x04  
#define WAKEUP_IOMEM_LEN         0x10  
#define EHFUNC_IOMEM_LEN         0x10  
#define SP_IOMEM_LEN             0x08  

 
struct wbcir_data {
	spinlock_t spinlock;
	struct rc_dev *dev;
	struct led_classdev led;

	unsigned long wbase;         
	unsigned long ebase;         
	unsigned long sbase;         
	unsigned int  irq;           
	u8 irqmask;

	 
	enum wbcir_rxstate rxstate;
	int carrier_report_enabled;
	u32 pulse_duration;

	 
	enum wbcir_txstate txstate;
	u32 txlen;
	u32 txoff;
	u32 *txbuf;
	u8 txmask;
	u32 txcarrier;
};

static bool invert;  
module_param(invert, bool, 0444);
MODULE_PARM_DESC(invert, "Invert the signal from the IR receiver");

static bool txandrx;  
module_param(txandrx, bool, 0444);
MODULE_PARM_DESC(txandrx, "Allow simultaneous TX and RX");


 

 
static void
wbcir_set_bits(unsigned long addr, u8 bits, u8 mask)
{
	u8 val;

	val = inb(addr);
	val = ((val & ~mask) | (bits & mask));
	outb(val, addr);
}

 
static inline void
wbcir_select_bank(struct wbcir_data *data, enum wbcir_bank bank)
{
	outb(bank, data->sbase + WBCIR_REG_SP3_BSR);
}

static inline void
wbcir_set_irqmask(struct wbcir_data *data, u8 irqmask)
{
	if (data->irqmask == irqmask)
		return;

	wbcir_select_bank(data, WBCIR_BANK_0);
	outb(irqmask, data->sbase + WBCIR_REG_SP3_IER);
	data->irqmask = irqmask;
}

static enum led_brightness
wbcir_led_brightness_get(struct led_classdev *led_cdev)
{
	struct wbcir_data *data = container_of(led_cdev,
					       struct wbcir_data,
					       led);

	if (inb(data->ebase + WBCIR_REG_ECEIR_CTS) & WBCIR_LED_ENABLE)
		return LED_FULL;
	else
		return LED_OFF;
}

static void
wbcir_led_brightness_set(struct led_classdev *led_cdev,
			 enum led_brightness brightness)
{
	struct wbcir_data *data = container_of(led_cdev,
					       struct wbcir_data,
					       led);

	wbcir_set_bits(data->ebase + WBCIR_REG_ECEIR_CTS,
		       brightness == LED_OFF ? 0x00 : WBCIR_LED_ENABLE,
		       WBCIR_LED_ENABLE);
}

 
static u8
wbcir_to_rc6cells(u8 val)
{
	u8 coded = 0x00;
	int i;

	val &= 0x0F;
	for (i = 0; i < 4; i++) {
		if (val & 0x01)
			coded |= 0x02 << (i * 2);
		else
			coded |= 0x01 << (i * 2);
		val >>= 1;
	}

	return coded;
}

 

static void
wbcir_carrier_report(struct wbcir_data *data)
{
	unsigned counter = inb(data->ebase + WBCIR_REG_ECEIR_CNT_LO) |
			inb(data->ebase + WBCIR_REG_ECEIR_CNT_HI) << 8;

	if (counter > 0 && counter < 0xffff) {
		struct ir_raw_event ev = {
			.carrier_report = 1,
			.carrier = DIV_ROUND_CLOSEST(counter * 1000000u,
						data->pulse_duration)
		};

		ir_raw_event_store(data->dev, &ev);
	}

	 
	data->pulse_duration = 0;
	wbcir_set_bits(data->ebase + WBCIR_REG_ECEIR_CCTL, WBCIR_CNTR_R,
						WBCIR_CNTR_EN | WBCIR_CNTR_R);
	wbcir_set_bits(data->ebase + WBCIR_REG_ECEIR_CCTL, WBCIR_CNTR_EN,
						WBCIR_CNTR_EN | WBCIR_CNTR_R);
}

static void
wbcir_idle_rx(struct rc_dev *dev, bool idle)
{
	struct wbcir_data *data = dev->priv;

	if (!idle && data->rxstate == WBCIR_RXSTATE_INACTIVE)
		data->rxstate = WBCIR_RXSTATE_ACTIVE;

	if (idle && data->rxstate != WBCIR_RXSTATE_INACTIVE) {
		data->rxstate = WBCIR_RXSTATE_INACTIVE;

		if (data->carrier_report_enabled)
			wbcir_carrier_report(data);

		 
		outb(WBCIR_RX_DISABLE, data->sbase + WBCIR_REG_SP3_ASCR);
	}
}

static void
wbcir_irq_rx(struct wbcir_data *data, struct pnp_dev *device)
{
	u8 irdata;
	struct ir_raw_event rawir = {};

	 
	while (inb(data->sbase + WBCIR_REG_SP3_LSR) & WBCIR_RX_AVAIL) {
		irdata = inb(data->sbase + WBCIR_REG_SP3_RXDATA);
		if (data->rxstate == WBCIR_RXSTATE_ERROR)
			continue;

		rawir.duration = ((irdata & 0x7F) + 1) *
			(data->carrier_report_enabled ? 2 : 10);
		rawir.pulse = irdata & 0x80 ? false : true;

		if (rawir.pulse)
			data->pulse_duration += rawir.duration;

		ir_raw_event_store_with_filter(data->dev, &rawir);
	}

	ir_raw_event_handle(data->dev);
}

static void
wbcir_irq_tx(struct wbcir_data *data)
{
	unsigned int space;
	unsigned int used;
	u8 bytes[16];
	u8 byte;

	if (!data->txbuf)
		return;

	switch (data->txstate) {
	case WBCIR_TXSTATE_INACTIVE:
		 
		space = 16;
		break;
	case WBCIR_TXSTATE_ACTIVE:
		 
		space = 13;
		break;
	case WBCIR_TXSTATE_ERROR:
		space = 0;
		break;
	default:
		return;
	}

	 
	for (used = 0; used < space && data->txoff != data->txlen; used++) {
		if (data->txbuf[data->txoff] == 0) {
			data->txoff++;
			continue;
		}
		byte = min((u32)0x80, data->txbuf[data->txoff]);
		data->txbuf[data->txoff] -= byte;
		byte--;
		byte |= (data->txoff % 2 ? 0x80 : 0x00);  
		bytes[used] = byte;
	}

	while (data->txoff != data->txlen && data->txbuf[data->txoff] == 0)
		data->txoff++;

	if (used == 0) {
		 
		if (data->txstate == WBCIR_TXSTATE_ERROR)
			 
			outb(WBCIR_TX_UNDERRUN, data->sbase + WBCIR_REG_SP3_ASCR);
		wbcir_set_irqmask(data, WBCIR_IRQ_RX | WBCIR_IRQ_ERR);
		kfree(data->txbuf);
		data->txbuf = NULL;
		data->txstate = WBCIR_TXSTATE_INACTIVE;
	} else if (data->txoff == data->txlen) {
		 
		outsb(data->sbase + WBCIR_REG_SP3_TXDATA, bytes, used - 1);
		outb(WBCIR_TX_EOT, data->sbase + WBCIR_REG_SP3_ASCR);
		outb(bytes[used - 1], data->sbase + WBCIR_REG_SP3_TXDATA);
		wbcir_set_irqmask(data, WBCIR_IRQ_RX | WBCIR_IRQ_ERR |
				  WBCIR_IRQ_TX_EMPTY);
	} else {
		 
		outsb(data->sbase + WBCIR_REG_SP3_RXDATA, bytes, used);
		if (data->txstate == WBCIR_TXSTATE_INACTIVE) {
			wbcir_set_irqmask(data, WBCIR_IRQ_RX | WBCIR_IRQ_ERR |
					  WBCIR_IRQ_TX_LOW);
			data->txstate = WBCIR_TXSTATE_ACTIVE;
		}
	}
}

static irqreturn_t
wbcir_irq_handler(int irqno, void *cookie)
{
	struct pnp_dev *device = cookie;
	struct wbcir_data *data = pnp_get_drvdata(device);
	unsigned long flags;
	u8 status;

	spin_lock_irqsave(&data->spinlock, flags);
	wbcir_select_bank(data, WBCIR_BANK_0);
	status = inb(data->sbase + WBCIR_REG_SP3_EIR);
	status &= data->irqmask;

	if (!status) {
		spin_unlock_irqrestore(&data->spinlock, flags);
		return IRQ_NONE;
	}

	if (status & WBCIR_IRQ_ERR) {
		 
		if (inb(data->sbase + WBCIR_REG_SP3_LSR) & WBCIR_RX_OVERRUN) {
			data->rxstate = WBCIR_RXSTATE_ERROR;
			ir_raw_event_overflow(data->dev);
		}

		 
		if (inb(data->sbase + WBCIR_REG_SP3_ASCR) & WBCIR_TX_UNDERRUN)
			data->txstate = WBCIR_TXSTATE_ERROR;
	}

	if (status & WBCIR_IRQ_RX)
		wbcir_irq_rx(data, device);

	if (status & (WBCIR_IRQ_TX_LOW | WBCIR_IRQ_TX_EMPTY))
		wbcir_irq_tx(data);

	spin_unlock_irqrestore(&data->spinlock, flags);
	return IRQ_HANDLED;
}

 

static int
wbcir_set_carrier_report(struct rc_dev *dev, int enable)
{
	struct wbcir_data *data = dev->priv;
	unsigned long flags;

	spin_lock_irqsave(&data->spinlock, flags);

	if (data->carrier_report_enabled == enable) {
		spin_unlock_irqrestore(&data->spinlock, flags);
		return 0;
	}

	data->pulse_duration = 0;
	wbcir_set_bits(data->ebase + WBCIR_REG_ECEIR_CCTL, WBCIR_CNTR_R,
						WBCIR_CNTR_EN | WBCIR_CNTR_R);

	if (enable && data->dev->idle)
		wbcir_set_bits(data->ebase + WBCIR_REG_ECEIR_CCTL,
				WBCIR_CNTR_EN, WBCIR_CNTR_EN | WBCIR_CNTR_R);

	 
	wbcir_select_bank(data, WBCIR_BANK_2);
	data->dev->rx_resolution = enable ? 2 : 10;
	outb(enable ? 0x03 : 0x0f, data->sbase + WBCIR_REG_SP3_BGDL);
	outb(0x00, data->sbase + WBCIR_REG_SP3_BGDH);

	 
	wbcir_select_bank(data, WBCIR_BANK_7);
	wbcir_set_bits(data->sbase + WBCIR_REG_SP3_RCCFG,
				enable ? WBCIR_RX_T_OV : 0, WBCIR_RX_T_OV);

	data->carrier_report_enabled = enable;
	spin_unlock_irqrestore(&data->spinlock, flags);

	return 0;
}

static int
wbcir_txcarrier(struct rc_dev *dev, u32 carrier)
{
	struct wbcir_data *data = dev->priv;
	unsigned long flags;
	u8 val;
	u32 freq;

	freq = DIV_ROUND_CLOSEST(carrier, 1000);
	if (freq < 30 || freq > 60)
		return -EINVAL;

	switch (freq) {
	case 58:
	case 59:
	case 60:
		val = freq - 58;
		freq *= 1000;
		break;
	case 57:
		val = freq - 27;
		freq = 56900;
		break;
	default:
		val = freq - 27;
		freq *= 1000;
		break;
	}

	spin_lock_irqsave(&data->spinlock, flags);
	if (data->txstate != WBCIR_TXSTATE_INACTIVE) {
		spin_unlock_irqrestore(&data->spinlock, flags);
		return -EBUSY;
	}

	if (data->txcarrier != freq) {
		wbcir_select_bank(data, WBCIR_BANK_7);
		wbcir_set_bits(data->sbase + WBCIR_REG_SP3_IRTXMC, val, 0x1F);
		data->txcarrier = freq;
	}

	spin_unlock_irqrestore(&data->spinlock, flags);
	return 0;
}

static int
wbcir_txmask(struct rc_dev *dev, u32 mask)
{
	struct wbcir_data *data = dev->priv;
	unsigned long flags;
	u8 val;

	 
	if (mask > 15)
		return 4;

	 
	switch (mask) {
	case 0x1:
		val = 0x0;
		break;
	case 0x2:
		val = 0x1;
		break;
	case 0x4:
		val = 0x2;
		break;
	case 0x8:
		val = 0x3;
		break;
	default:
		return -EINVAL;
	}

	spin_lock_irqsave(&data->spinlock, flags);
	if (data->txstate != WBCIR_TXSTATE_INACTIVE) {
		spin_unlock_irqrestore(&data->spinlock, flags);
		return -EBUSY;
	}

	if (data->txmask != mask) {
		wbcir_set_bits(data->ebase + WBCIR_REG_ECEIR_CTS, val, 0x0c);
		data->txmask = mask;
	}

	spin_unlock_irqrestore(&data->spinlock, flags);
	return 0;
}

static int
wbcir_tx(struct rc_dev *dev, unsigned *b, unsigned count)
{
	struct wbcir_data *data = dev->priv;
	unsigned *buf;
	unsigned i;
	unsigned long flags;

	buf = kmalloc_array(count, sizeof(*b), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	 
	for (i = 0; i < count; i++)
		buf[i] = DIV_ROUND_CLOSEST(b[i], 10);

	 
	spin_lock_irqsave(&data->spinlock, flags);
	if (data->txstate != WBCIR_TXSTATE_INACTIVE) {
		spin_unlock_irqrestore(&data->spinlock, flags);
		kfree(buf);
		return -EBUSY;
	}

	 
	data->txbuf = buf;
	data->txlen = count;
	data->txoff = 0;
	wbcir_irq_tx(data);

	 
	spin_unlock_irqrestore(&data->spinlock, flags);
	return count;
}

 

static void
wbcir_shutdown(struct pnp_dev *device)
{
	struct device *dev = &device->dev;
	struct wbcir_data *data = pnp_get_drvdata(device);
	struct rc_dev *rc = data->dev;
	bool do_wake = true;
	u8 match[11];
	u8 mask[11];
	u8 rc6_csl = 0;
	u8 proto;
	u32 wake_sc = rc->scancode_wakeup_filter.data;
	u32 mask_sc = rc->scancode_wakeup_filter.mask;
	int i;

	memset(match, 0, sizeof(match));
	memset(mask, 0, sizeof(mask));

	if (!mask_sc || !device_may_wakeup(dev)) {
		do_wake = false;
		goto finish;
	}

	switch (rc->wakeup_protocol) {
	case RC_PROTO_RC5:
		 
		mask[0]  = (mask_sc & 0x003f);
		mask[0] |= (mask_sc & 0x0300) >> 2;
		mask[1]  = (mask_sc & 0x1c00) >> 10;
		if (mask_sc & 0x0040)		       
			match[1] |= 0x10;

		match[0]  = (wake_sc & 0x003F);        
		match[0] |= (wake_sc & 0x0300) >> 2;   
		match[1]  = (wake_sc & 0x1c00) >> 10;  
		if (!(wake_sc & 0x0040))	       
			match[1] |= 0x10;

		proto = IR_PROTOCOL_RC5;
		break;

	case RC_PROTO_NEC:
		mask[1] = bitrev8(mask_sc);
		mask[0] = mask[1];
		mask[3] = bitrev8(mask_sc >> 8);
		mask[2] = mask[3];

		match[1] = bitrev8(wake_sc);
		match[0] = ~match[1];
		match[3] = bitrev8(wake_sc >> 8);
		match[2] = ~match[3];

		proto = IR_PROTOCOL_NEC;
		break;

	case RC_PROTO_NECX:
		mask[1] = bitrev8(mask_sc);
		mask[0] = mask[1];
		mask[2] = bitrev8(mask_sc >> 8);
		mask[3] = bitrev8(mask_sc >> 16);

		match[1] = bitrev8(wake_sc);
		match[0] = ~match[1];
		match[2] = bitrev8(wake_sc >> 8);
		match[3] = bitrev8(wake_sc >> 16);

		proto = IR_PROTOCOL_NEC;
		break;

	case RC_PROTO_NEC32:
		mask[0] = bitrev8(mask_sc);
		mask[1] = bitrev8(mask_sc >> 8);
		mask[2] = bitrev8(mask_sc >> 16);
		mask[3] = bitrev8(mask_sc >> 24);

		match[0] = bitrev8(wake_sc);
		match[1] = bitrev8(wake_sc >> 8);
		match[2] = bitrev8(wake_sc >> 16);
		match[3] = bitrev8(wake_sc >> 24);

		proto = IR_PROTOCOL_NEC;
		break;

	case RC_PROTO_RC6_0:
		 
		match[0] = wbcir_to_rc6cells(wake_sc >> 0);
		mask[0]  = wbcir_to_rc6cells(mask_sc >> 0);
		match[1] = wbcir_to_rc6cells(wake_sc >> 4);
		mask[1]  = wbcir_to_rc6cells(mask_sc >> 4);

		 
		match[2] = wbcir_to_rc6cells(wake_sc >>  8);
		mask[2]  = wbcir_to_rc6cells(mask_sc >>  8);
		match[3] = wbcir_to_rc6cells(wake_sc >> 12);
		mask[3]  = wbcir_to_rc6cells(mask_sc >> 12);

		 
		match[4] = 0x50;  
		mask[4]  = 0xF0;
		match[5] = 0x09;  
		mask[5]  = 0x0F;

		rc6_csl = 44;
		proto = IR_PROTOCOL_RC6;
		break;

	case RC_PROTO_RC6_6A_24:
	case RC_PROTO_RC6_6A_32:
	case RC_PROTO_RC6_MCE:
		i = 0;

		 
		match[i]  = wbcir_to_rc6cells(wake_sc >>  0);
		mask[i++] = wbcir_to_rc6cells(mask_sc >>  0);
		match[i]  = wbcir_to_rc6cells(wake_sc >>  4);
		mask[i++] = wbcir_to_rc6cells(mask_sc >>  4);

		 
		match[i]  = wbcir_to_rc6cells(wake_sc >>  8);
		mask[i++] = wbcir_to_rc6cells(mask_sc >>  8);
		match[i]  = wbcir_to_rc6cells(wake_sc >> 12);
		mask[i++] = wbcir_to_rc6cells(mask_sc >> 12);

		 
		match[i]  = wbcir_to_rc6cells(wake_sc >> 16);
		mask[i++] = wbcir_to_rc6cells(mask_sc >> 16);

		if (rc->wakeup_protocol == RC_PROTO_RC6_6A_20) {
			rc6_csl = 52;
		} else {
			match[i]  = wbcir_to_rc6cells(wake_sc >> 20);
			mask[i++] = wbcir_to_rc6cells(mask_sc >> 20);

			if (rc->wakeup_protocol == RC_PROTO_RC6_6A_24) {
				rc6_csl = 60;
			} else {
				 
				match[i]  = wbcir_to_rc6cells(wake_sc >> 24);
				mask[i++] = wbcir_to_rc6cells(mask_sc >> 24);
				match[i]  = wbcir_to_rc6cells(wake_sc >> 28);
				mask[i++] = wbcir_to_rc6cells(mask_sc >> 28);
				rc6_csl = 76;
			}
		}

		 
		match[i]  = 0x93;  
		mask[i++] = 0xFF;
		match[i]  = 0x0A;  
		mask[i++] = 0x0F;
		proto = IR_PROTOCOL_RC6;
		break;
	default:
		do_wake = false;
		break;
	}

finish:
	if (do_wake) {
		 
		wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_INDEX,
			       WBCIR_REGSEL_COMPARE | WBCIR_REG_ADDR0,
			       0x3F);
		outsb(data->wbase + WBCIR_REG_WCEIR_DATA, match, 11);
		wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_INDEX,
			       WBCIR_REGSEL_MASK | WBCIR_REG_ADDR0,
			       0x3F);
		outsb(data->wbase + WBCIR_REG_WCEIR_DATA, mask, 11);

		 
		outb(rc6_csl, data->wbase + WBCIR_REG_WCEIR_CSL);

		 
		wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_STS, 0x17, 0x17);

		 
		wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_EV_EN, 0x01, 0x07);

		 
		wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_CTL,
			       (proto << 4) | 0x01, 0x31);

	} else {
		 
		wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_EV_EN, 0x00, 0x07);

		 
		wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_CTL, 0x00, 0x01);
	}

	 
	wbcir_set_irqmask(data, WBCIR_IRQ_NONE);
	disable_irq(data->irq);
}

 
static int
wbcir_set_wakeup_filter(struct rc_dev *rc, struct rc_scancode_filter *filter)
{
	return 0;
}

static int
wbcir_suspend(struct pnp_dev *device, pm_message_t state)
{
	struct wbcir_data *data = pnp_get_drvdata(device);
	led_classdev_suspend(&data->led);
	wbcir_shutdown(device);
	return 0;
}

static void
wbcir_init_hw(struct wbcir_data *data)
{
	 
	wbcir_set_irqmask(data, WBCIR_IRQ_NONE);

	 
	wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_CTL, invert ? 8 : 0, 0x09);

	 
	wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_STS, 0x17, 0x17);

	 
	wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_EV_EN, 0x00, 0x07);

	 
	wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_CFG1, 0x4A, 0x7F);

	 
	if (invert)
		outb(WBCIR_IRTX_INV, data->ebase + WBCIR_REG_ECEIR_CCTL);
	else
		outb(0x00, data->ebase + WBCIR_REG_ECEIR_CCTL);

	 
	outb(0x10, data->ebase + WBCIR_REG_ECEIR_CTS);
	data->txmask = 0x1;

	 
	wbcir_select_bank(data, WBCIR_BANK_2);
	outb(WBCIR_EXT_ENABLE, data->sbase + WBCIR_REG_SP3_EXCR1);

	 

	 
	outb(0x30, data->sbase + WBCIR_REG_SP3_EXCR2);

	 
	outb(0x0f, data->sbase + WBCIR_REG_SP3_BGDL);
	outb(0x00, data->sbase + WBCIR_REG_SP3_BGDH);

	 
	wbcir_select_bank(data, WBCIR_BANK_0);
	outb(0xC0, data->sbase + WBCIR_REG_SP3_MCR);
	inb(data->sbase + WBCIR_REG_SP3_LSR);  
	inb(data->sbase + WBCIR_REG_SP3_MSR);  

	 
	wbcir_select_bank(data, WBCIR_BANK_7);
	outb(0x90, data->sbase + WBCIR_REG_SP3_RCCFG);

	 
	wbcir_select_bank(data, WBCIR_BANK_4);
	outb(0x00, data->sbase + WBCIR_REG_SP3_IRCR1);

	 
	wbcir_select_bank(data, WBCIR_BANK_5);
	outb(txandrx ? 0x03 : 0x02, data->sbase + WBCIR_REG_SP3_IRCR2);

	 
	wbcir_select_bank(data, WBCIR_BANK_6);
	outb(0x20, data->sbase + WBCIR_REG_SP3_IRCR3);

	 
	wbcir_select_bank(data, WBCIR_BANK_7);
	outb(0xF2, data->sbase + WBCIR_REG_SP3_IRRXDC);

	 
	outb(0x69, data->sbase + WBCIR_REG_SP3_IRTXMC);
	data->txcarrier = 36000;

	 
	if (invert)
		outb(0x10, data->sbase + WBCIR_REG_SP3_IRCFG4);
	else
		outb(0x00, data->sbase + WBCIR_REG_SP3_IRCFG4);

	 
	wbcir_select_bank(data, WBCIR_BANK_0);
	outb(0x97, data->sbase + WBCIR_REG_SP3_FCR);

	 
	outb(0xE0, data->sbase + WBCIR_REG_SP3_ASCR);

	 
	data->rxstate = WBCIR_RXSTATE_INACTIVE;
	wbcir_idle_rx(data->dev, true);

	 
	if (data->txstate == WBCIR_TXSTATE_ACTIVE) {
		kfree(data->txbuf);
		data->txbuf = NULL;
		data->txstate = WBCIR_TXSTATE_INACTIVE;
	}

	 
	wbcir_set_irqmask(data, WBCIR_IRQ_RX | WBCIR_IRQ_ERR);
}

static int
wbcir_resume(struct pnp_dev *device)
{
	struct wbcir_data *data = pnp_get_drvdata(device);

	wbcir_init_hw(data);
	enable_irq(data->irq);
	led_classdev_resume(&data->led);

	return 0;
}

static int
wbcir_probe(struct pnp_dev *device, const struct pnp_device_id *dev_id)
{
	struct device *dev = &device->dev;
	struct wbcir_data *data;
	int err;

	if (!(pnp_port_len(device, 0) == EHFUNC_IOMEM_LEN &&
	      pnp_port_len(device, 1) == WAKEUP_IOMEM_LEN &&
	      pnp_port_len(device, 2) == SP_IOMEM_LEN)) {
		dev_err(dev, "Invalid resources\n");
		return -ENODEV;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}

	pnp_set_drvdata(device, data);

	spin_lock_init(&data->spinlock);
	data->ebase = pnp_port_start(device, 0);
	data->wbase = pnp_port_start(device, 1);
	data->sbase = pnp_port_start(device, 2);
	data->irq = pnp_irq(device, 0);

	if (data->wbase == 0 || data->ebase == 0 ||
	    data->sbase == 0 || data->irq == -1) {
		err = -ENODEV;
		dev_err(dev, "Invalid resources\n");
		goto exit_free_data;
	}

	dev_dbg(&device->dev, "Found device (w: 0x%lX, e: 0x%lX, s: 0x%lX, i: %u)\n",
		data->wbase, data->ebase, data->sbase, data->irq);

	data->led.name = "cir::activity";
	data->led.default_trigger = "rc-feedback";
	data->led.brightness_set = wbcir_led_brightness_set;
	data->led.brightness_get = wbcir_led_brightness_get;
	err = led_classdev_register(&device->dev, &data->led);
	if (err)
		goto exit_free_data;

	data->dev = rc_allocate_device(RC_DRIVER_IR_RAW);
	if (!data->dev) {
		err = -ENOMEM;
		goto exit_unregister_led;
	}

	data->dev->driver_name = DRVNAME;
	data->dev->device_name = WBCIR_NAME;
	data->dev->input_phys = "wbcir/cir0";
	data->dev->input_id.bustype = BUS_HOST;
	data->dev->input_id.vendor = PCI_VENDOR_ID_WINBOND;
	data->dev->input_id.product = WBCIR_ID_FAMILY;
	data->dev->input_id.version = WBCIR_ID_CHIP;
	data->dev->map_name = RC_MAP_RC6_MCE;
	data->dev->s_idle = wbcir_idle_rx;
	data->dev->s_carrier_report = wbcir_set_carrier_report;
	data->dev->s_tx_mask = wbcir_txmask;
	data->dev->s_tx_carrier = wbcir_txcarrier;
	data->dev->tx_ir = wbcir_tx;
	data->dev->priv = data;
	data->dev->dev.parent = &device->dev;
	data->dev->min_timeout = 1;
	data->dev->timeout = IR_DEFAULT_TIMEOUT;
	data->dev->max_timeout = 10 * IR_DEFAULT_TIMEOUT;
	data->dev->rx_resolution = 2;
	data->dev->allowed_protocols = RC_PROTO_BIT_ALL_IR_DECODER;
	data->dev->allowed_wakeup_protocols = RC_PROTO_BIT_NEC |
		RC_PROTO_BIT_NECX | RC_PROTO_BIT_NEC32 | RC_PROTO_BIT_RC5 |
		RC_PROTO_BIT_RC6_0 | RC_PROTO_BIT_RC6_6A_20 |
		RC_PROTO_BIT_RC6_6A_24 | RC_PROTO_BIT_RC6_6A_32 |
		RC_PROTO_BIT_RC6_MCE;
	data->dev->wakeup_protocol = RC_PROTO_RC6_MCE;
	data->dev->scancode_wakeup_filter.data = 0x800f040c;
	data->dev->scancode_wakeup_filter.mask = 0xffff7fff;
	data->dev->s_wakeup_filter = wbcir_set_wakeup_filter;

	err = rc_register_device(data->dev);
	if (err)
		goto exit_free_rc;

	if (!request_region(data->wbase, WAKEUP_IOMEM_LEN, DRVNAME)) {
		dev_err(dev, "Region 0x%lx-0x%lx already in use!\n",
			data->wbase, data->wbase + WAKEUP_IOMEM_LEN - 1);
		err = -EBUSY;
		goto exit_unregister_device;
	}

	if (!request_region(data->ebase, EHFUNC_IOMEM_LEN, DRVNAME)) {
		dev_err(dev, "Region 0x%lx-0x%lx already in use!\n",
			data->ebase, data->ebase + EHFUNC_IOMEM_LEN - 1);
		err = -EBUSY;
		goto exit_release_wbase;
	}

	if (!request_region(data->sbase, SP_IOMEM_LEN, DRVNAME)) {
		dev_err(dev, "Region 0x%lx-0x%lx already in use!\n",
			data->sbase, data->sbase + SP_IOMEM_LEN - 1);
		err = -EBUSY;
		goto exit_release_ebase;
	}

	err = request_irq(data->irq, wbcir_irq_handler,
			  0, DRVNAME, device);
	if (err) {
		dev_err(dev, "Failed to claim IRQ %u\n", data->irq);
		err = -EBUSY;
		goto exit_release_sbase;
	}

	device_init_wakeup(&device->dev, 1);

	wbcir_init_hw(data);

	return 0;

exit_release_sbase:
	release_region(data->sbase, SP_IOMEM_LEN);
exit_release_ebase:
	release_region(data->ebase, EHFUNC_IOMEM_LEN);
exit_release_wbase:
	release_region(data->wbase, WAKEUP_IOMEM_LEN);
exit_unregister_device:
	rc_unregister_device(data->dev);
	data->dev = NULL;
exit_free_rc:
	rc_free_device(data->dev);
exit_unregister_led:
	led_classdev_unregister(&data->led);
exit_free_data:
	kfree(data);
	pnp_set_drvdata(device, NULL);
exit:
	return err;
}

static void
wbcir_remove(struct pnp_dev *device)
{
	struct wbcir_data *data = pnp_get_drvdata(device);

	 
	wbcir_set_irqmask(data, WBCIR_IRQ_NONE);
	free_irq(data->irq, device);

	 
	wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_STS, 0x17, 0x17);

	 
	wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_CTL, 0x00, 0x01);

	 
	wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_EV_EN, 0x00, 0x07);

	rc_unregister_device(data->dev);

	led_classdev_unregister(&data->led);

	 
	wbcir_led_brightness_set(&data->led, LED_OFF);

	release_region(data->wbase, WAKEUP_IOMEM_LEN);
	release_region(data->ebase, EHFUNC_IOMEM_LEN);
	release_region(data->sbase, SP_IOMEM_LEN);

	kfree(data);

	pnp_set_drvdata(device, NULL);
}

static const struct pnp_device_id wbcir_ids[] = {
	{ "WEC1022", 0 },
	{ "", 0 }
};
MODULE_DEVICE_TABLE(pnp, wbcir_ids);

static struct pnp_driver wbcir_driver = {
	.name     = DRVNAME,
	.id_table = wbcir_ids,
	.probe    = wbcir_probe,
	.remove   = wbcir_remove,
	.suspend  = wbcir_suspend,
	.resume   = wbcir_resume,
	.shutdown = wbcir_shutdown
};

static int __init
wbcir_init(void)
{
	int ret;

	ret = pnp_register_driver(&wbcir_driver);
	if (ret)
		pr_err("Unable to register driver\n");

	return ret;
}

static void __exit
wbcir_exit(void)
{
	pnp_unregister_driver(&wbcir_driver);
}

module_init(wbcir_init);
module_exit(wbcir_exit);

MODULE_AUTHOR("David Härdeman <david@hardeman.nu>");
MODULE_DESCRIPTION("Winbond SuperI/O Consumer IR Driver");
MODULE_LICENSE("GPL");
