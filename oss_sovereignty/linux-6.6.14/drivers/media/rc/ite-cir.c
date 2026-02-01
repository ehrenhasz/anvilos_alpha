
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pnp.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/bitops.h>
#include <media/rc-core.h>
#include <linux/pci_ids.h>

#include "ite-cir.h"

 

 
static long sample_period = NSEC_PER_SEC / 115200;
module_param(sample_period, long, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(sample_period, "sample period");

 
static int model_number = -1;
module_param(model_number, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(model_number, "Use this model number, don't autodetect");


 

 
static inline bool ite_is_high_carrier_freq(unsigned int freq)
{
	return freq >= ITE_HCF_MIN_CARRIER_FREQ;
}

 
static u8 ite_get_carrier_freq_bits(unsigned int freq)
{
	if (ite_is_high_carrier_freq(freq)) {
		if (freq < 425000)
			return ITE_CFQ_400;

		else if (freq < 465000)
			return ITE_CFQ_450;

		else if (freq < 490000)
			return ITE_CFQ_480;

		else
			return ITE_CFQ_500;
	} else {
			 
		if (freq < ITE_LCF_MIN_CARRIER_FREQ)
			freq = ITE_LCF_MIN_CARRIER_FREQ;
		if (freq > ITE_LCF_MAX_CARRIER_FREQ)
			freq = ITE_LCF_MAX_CARRIER_FREQ;

		 
		freq = DIV_ROUND_CLOSEST(freq - ITE_LCF_MIN_CARRIER_FREQ, 1000);

		return (u8) freq;
	}
}

 
static u8 ite_get_pulse_width_bits(unsigned int freq, int duty_cycle)
{
	unsigned long period_ns, on_ns;

	 
	if (freq < ITE_LCF_MIN_CARRIER_FREQ)
		freq = ITE_LCF_MIN_CARRIER_FREQ;
	if (freq > ITE_HCF_MAX_CARRIER_FREQ)
		freq = ITE_HCF_MAX_CARRIER_FREQ;

	period_ns = 1000000000UL / freq;
	on_ns = period_ns * duty_cycle / 100;

	if (ite_is_high_carrier_freq(freq)) {
		if (on_ns < 750)
			return ITE_TXMPW_A;

		else if (on_ns < 850)
			return ITE_TXMPW_B;

		else if (on_ns < 950)
			return ITE_TXMPW_C;

		else if (on_ns < 1080)
			return ITE_TXMPW_D;

		else
			return ITE_TXMPW_E;
	} else {
		if (on_ns < 6500)
			return ITE_TXMPW_A;

		else if (on_ns < 7850)
			return ITE_TXMPW_B;

		else if (on_ns < 9650)
			return ITE_TXMPW_C;

		else if (on_ns < 11950)
			return ITE_TXMPW_D;

		else
			return ITE_TXMPW_E;
	}
}

 
static void ite_decode_bytes(struct ite_dev *dev, const u8 * data, int
			     length)
{
	unsigned long *ldata;
	unsigned int next_one, next_zero, size;
	struct ir_raw_event ev = {};

	if (length == 0)
		return;

	ldata = (unsigned long *)data;
	size = length << 3;
	next_one = find_next_bit_le(ldata, size, 0);
	if (next_one > 0) {
		ev.pulse = true;
		ev.duration = ITE_BITS_TO_US(next_one, sample_period);
		ir_raw_event_store_with_filter(dev->rdev, &ev);
	}

	while (next_one < size) {
		next_zero = find_next_zero_bit_le(ldata, size, next_one + 1);
		ev.pulse = false;
		ev.duration = ITE_BITS_TO_US(next_zero - next_one, sample_period);
		ir_raw_event_store_with_filter(dev->rdev, &ev);

		if (next_zero < size) {
			next_one = find_next_bit_le(ldata, size, next_zero + 1);
			ev.pulse = true;
			ev.duration = ITE_BITS_TO_US(next_one - next_zero,
						     sample_period);
			ir_raw_event_store_with_filter(dev->rdev, &ev);
		} else
			next_one = size;
	}

	ir_raw_event_handle(dev->rdev);

	dev_dbg(&dev->rdev->dev, "decoded %d bytes\n", length);
}

 
static void ite_set_carrier_params(struct ite_dev *dev)
{
	unsigned int freq, low_freq, high_freq;
	int allowance;
	bool use_demodulator;
	bool for_tx = dev->transmitting;

	if (for_tx) {
		 
		freq = dev->tx_carrier_freq;
		allowance = ITE_RXDCR_DEFAULT;
		use_demodulator = false;
	} else {
		low_freq = dev->rx_low_carrier_freq;
		high_freq = dev->rx_high_carrier_freq;

		if (low_freq == 0) {
			 
			freq = ITE_DEFAULT_CARRIER_FREQ;
			allowance = ITE_RXDCR_DEFAULT;
			use_demodulator = false;
		} else {
			 
			freq = (low_freq + high_freq) / 2;

			 
			allowance =
			    DIV_ROUND_CLOSEST(10000 * (high_freq - low_freq),
					      ITE_RXDCR_PER_10000_STEP
					      * (high_freq + low_freq));

			if (allowance < 1)
				allowance = 1;

			if (allowance > ITE_RXDCR_MAX)
				allowance = ITE_RXDCR_MAX;

			use_demodulator = true;
		}
	}

	 
	dev->params->set_carrier_params(dev, ite_is_high_carrier_freq(freq),
		 use_demodulator, ite_get_carrier_freq_bits(freq), allowance,
		 ite_get_pulse_width_bits(freq, dev->tx_duty_cycle));
}

 
static irqreturn_t ite_cir_isr(int irq, void *data)
{
	struct ite_dev *dev = data;
	irqreturn_t ret = IRQ_RETVAL(IRQ_NONE);
	u8 rx_buf[ITE_RX_FIFO_LEN];
	int rx_bytes;
	int iflags;

	 
	spin_lock(&dev->lock);

	 
	iflags = dev->params->get_irq_causes(dev);

	 
	if (iflags & ITE_IRQ_RX_FIFO_OVERRUN) {
		dev_warn(&dev->rdev->dev, "receive overflow\n");
		ir_raw_event_overflow(dev->rdev);
	}

	 
	if (iflags & (ITE_IRQ_RX_FIFO | ITE_IRQ_RX_FIFO_OVERRUN)) {
		 
		rx_bytes = dev->params->get_rx_bytes(dev, rx_buf,
						    ITE_RX_FIFO_LEN);

		dev_dbg(&dev->rdev->dev, "interrupt %d RX bytes\n", rx_bytes);

		if (rx_bytes > 0) {
			 
			spin_unlock(&dev->lock);

			 
			ite_decode_bytes(dev, rx_buf, rx_bytes);

			 
			spin_lock(&dev->lock);

			 
			ret = IRQ_RETVAL(IRQ_HANDLED);
		}
	} else if (iflags & ITE_IRQ_TX_FIFO) {
		 
		dev_dbg(&dev->rdev->dev, "interrupt TX FIFO\n");

		 
		wake_up_interruptible(&dev->tx_queue);

		 
		ret = IRQ_RETVAL(IRQ_HANDLED);
	}

	 
	spin_unlock(&dev->lock);

	return ret;
}

 
static int ite_set_rx_carrier_range(struct rc_dev *rcdev, u32 carrier_low, u32
				    carrier_high)
{
	unsigned long flags;
	struct ite_dev *dev = rcdev->priv;

	spin_lock_irqsave(&dev->lock, flags);
	dev->rx_low_carrier_freq = carrier_low;
	dev->rx_high_carrier_freq = carrier_high;
	ite_set_carrier_params(dev);
	spin_unlock_irqrestore(&dev->lock, flags);

	return 0;
}

 
static int ite_set_tx_carrier(struct rc_dev *rcdev, u32 carrier)
{
	unsigned long flags;
	struct ite_dev *dev = rcdev->priv;

	spin_lock_irqsave(&dev->lock, flags);
	dev->tx_carrier_freq = carrier;
	ite_set_carrier_params(dev);
	spin_unlock_irqrestore(&dev->lock, flags);

	return 0;
}

 
static int ite_set_tx_duty_cycle(struct rc_dev *rcdev, u32 duty_cycle)
{
	unsigned long flags;
	struct ite_dev *dev = rcdev->priv;

	spin_lock_irqsave(&dev->lock, flags);
	dev->tx_duty_cycle = duty_cycle;
	ite_set_carrier_params(dev);
	spin_unlock_irqrestore(&dev->lock, flags);

	return 0;
}

 
static int ite_tx_ir(struct rc_dev *rcdev, unsigned *txbuf, unsigned n)
{
	unsigned long flags;
	struct ite_dev *dev = rcdev->priv;
	bool is_pulse = false;
	int remaining_us, fifo_avail, fifo_remaining, last_idx = 0;
	int max_rle_us, next_rle_us;
	int ret = n;
	u8 last_sent[ITE_TX_FIFO_LEN];
	u8 val;

	 
	memset(last_sent, 0, sizeof(last_sent));

	spin_lock_irqsave(&dev->lock, flags);

	 
	dev->transmitting = true;

	 
	ite_set_carrier_params(dev);

	 
	max_rle_us =
	    (ITE_BAUDRATE_DIVISOR * sample_period *
	     ITE_TX_MAX_RLE) / 1000;

	 
	dev->params->disable_rx(dev);

	 
	fifo_avail = ITE_TX_FIFO_LEN - dev->params->get_tx_used_slots(dev);

	while (n > 0) {
		 
		is_pulse = !is_pulse;
		remaining_us = *(txbuf++);
		n--;

		dev_dbg(&dev->rdev->dev, "%s: %d\n",
			is_pulse ? "pulse" : "space", remaining_us);

		 
		while (remaining_us > 0) {
			if (remaining_us > max_rle_us)
				next_rle_us = max_rle_us;

			else
				next_rle_us = remaining_us;

			remaining_us -= next_rle_us;

			 
			val = (ITE_TX_MAX_RLE * next_rle_us) / max_rle_us;

			 
			last_sent[last_idx++] = val;
			last_idx &= (ITE_TX_FIFO_LEN);

			 
			val = (val - 1) & ITE_TX_RLE_MASK;

			 
			if (is_pulse)
				val |= ITE_TX_PULSE;

			else
				val |= ITE_TX_SPACE;

			 
			if (fifo_avail <= 0)
				fifo_avail = ITE_TX_FIFO_LEN - dev->params->get_tx_used_slots(dev);

			 
			if (fifo_avail <= 0) {
				 
				dev->params->enable_tx_interrupt(dev);

				 
				spin_unlock_irqrestore(&dev->lock, flags);

				 
				wait_event_interruptible(dev->tx_queue,
					(fifo_avail = ITE_TX_FIFO_LEN - dev->params->get_tx_used_slots(dev)) >= 8);

				 
				spin_lock_irqsave(&dev->lock, flags);

				 
				dev->params->disable_tx_interrupt(dev);
			}

			 
			dev->params->put_tx_byte(dev, val);
			fifo_avail--;
		}
	}

	 
	fifo_remaining = dev->params->get_tx_used_slots(dev);
	remaining_us = 0;
	while (fifo_remaining > 0) {
		fifo_remaining--;
		last_idx--;
		last_idx &= (ITE_TX_FIFO_LEN - 1);
		remaining_us += last_sent[last_idx];
	}
	remaining_us = (remaining_us * max_rle_us) / (ITE_TX_MAX_RLE);

	 
	spin_unlock_irqrestore(&dev->lock, flags);

	 
	mdelay(DIV_ROUND_UP(remaining_us, 1000));

	 
	spin_lock_irqsave(&dev->lock, flags);

	 
	dev->transmitting = false;

	 
	ite_set_carrier_params(dev);

	 
	dev->params->enable_rx(dev);

	 
	wake_up_interruptible(&dev->tx_ended);

	spin_unlock_irqrestore(&dev->lock, flags);

	return ret;
}

 
static void ite_s_idle(struct rc_dev *rcdev, bool enable)
{
	unsigned long flags;
	struct ite_dev *dev = rcdev->priv;

	if (enable) {
		spin_lock_irqsave(&dev->lock, flags);
		dev->params->idle_rx(dev);
		spin_unlock_irqrestore(&dev->lock, flags);
	}
}


 

 
static int it87_get_irq_causes(struct ite_dev *dev)
{
	u8 iflags;
	int ret = 0;

	 
	iflags = inb(dev->cir_addr + IT87_IIR) & IT87_II;

	switch (iflags) {
	case IT87_II_RXDS:
		ret = ITE_IRQ_RX_FIFO;
		break;
	case IT87_II_RXFO:
		ret = ITE_IRQ_RX_FIFO_OVERRUN;
		break;
	case IT87_II_TXLDL:
		ret = ITE_IRQ_TX_FIFO;
		break;
	}

	return ret;
}

 
static void it87_set_carrier_params(struct ite_dev *dev, bool high_freq,
				    bool use_demodulator,
				    u8 carrier_freq_bits, u8 allowance_bits,
				    u8 pulse_width_bits)
{
	u8 val;

	 
	val = inb(dev->cir_addr + IT87_RCR)
		& ~(IT87_HCFS | IT87_RXEND | IT87_RXDCR);

	if (high_freq)
		val |= IT87_HCFS;

	if (use_demodulator)
		val |= IT87_RXEND;

	val |= allowance_bits;

	outb(val, dev->cir_addr + IT87_RCR);

	 
	outb((carrier_freq_bits << IT87_CFQ_SHIFT) | pulse_width_bits,
		dev->cir_addr + IT87_TCR2);
}

 
static int it87_get_rx_bytes(struct ite_dev *dev, u8 * buf, int buf_size)
{
	int fifo, read = 0;

	 
	fifo = inb(dev->cir_addr + IT87_RSR) & IT87_RXFBC;

	while (fifo > 0 && buf_size > 0) {
		*(buf++) = inb(dev->cir_addr + IT87_DR);
		fifo--;
		read++;
		buf_size--;
	}

	return read;
}

 
static int it87_get_tx_used_slots(struct ite_dev *dev)
{
	return inb(dev->cir_addr + IT87_TSR) & IT87_TXFBC;
}

 
static void it87_put_tx_byte(struct ite_dev *dev, u8 value)
{
	outb(value, dev->cir_addr + IT87_DR);
}

 
static void it87_idle_rx(struct ite_dev *dev)
{
	 
	outb(inb(dev->cir_addr + IT87_RCR) | IT87_RXACT,
		dev->cir_addr + IT87_RCR);

	 
	outb(inb(dev->cir_addr + IT87_TCR1) | IT87_FIFOCLR,
		dev->cir_addr + IT87_TCR1);
}

 
static void it87_disable_rx(struct ite_dev *dev)
{
	 
	outb(inb(dev->cir_addr + IT87_IER) & ~(IT87_RDAIE | IT87_RFOIE),
		dev->cir_addr + IT87_IER);

	 
	outb(inb(dev->cir_addr + IT87_RCR) & ~IT87_RXEN,
		dev->cir_addr + IT87_RCR);

	 
	it87_idle_rx(dev);
}

 
static void it87_enable_rx(struct ite_dev *dev)
{
	 
	outb(inb(dev->cir_addr + IT87_RCR) | IT87_RXEN,
		dev->cir_addr + IT87_RCR);

	 
	it87_idle_rx(dev);

	 
	outb(inb(dev->cir_addr + IT87_IER) | IT87_RDAIE | IT87_RFOIE | IT87_IEC,
		dev->cir_addr + IT87_IER);
}

 
static void it87_disable_tx_interrupt(struct ite_dev *dev)
{
	 
	outb(inb(dev->cir_addr + IT87_IER) & ~IT87_TLDLIE,
		dev->cir_addr + IT87_IER);
}

 
static void it87_enable_tx_interrupt(struct ite_dev *dev)
{
	 
	outb(inb(dev->cir_addr + IT87_IER) | IT87_TLDLIE | IT87_IEC,
		dev->cir_addr + IT87_IER);
}

 
static void it87_disable(struct ite_dev *dev)
{
	 
	outb(inb(dev->cir_addr + IT87_IER) &
		~(IT87_IEC | IT87_RFOIE | IT87_RDAIE | IT87_TLDLIE),
		dev->cir_addr + IT87_IER);

	 
	it87_disable_rx(dev);

	 
	outb(IT87_FIFOCLR | inb(dev->cir_addr + IT87_TCR1),
		dev->cir_addr + IT87_TCR1);
}

 
static void it87_init_hardware(struct ite_dev *dev)
{
	 
	outb((inb(dev->cir_addr + IT87_IER) &
		~(IT87_IEC | IT87_RFOIE | IT87_RDAIE | IT87_TLDLIE)) | IT87_BR,
		dev->cir_addr + IT87_IER);

	 
	outb(ITE_BAUDRATE_DIVISOR & 0xff, dev->cir_addr + IT87_BDLR);
	outb((ITE_BAUDRATE_DIVISOR >> 8) & 0xff, dev->cir_addr + IT87_BDHR);

	 
	outb(inb(dev->cir_addr + IT87_IER) & ~IT87_BR,
		dev->cir_addr + IT87_IER);

	 
	outb(ITE_RXDCR_DEFAULT, dev->cir_addr + IT87_RCR);

	 
	outb(IT87_TXMPM_DEFAULT | IT87_TXENDF | IT87_TXRLE
		| IT87_FIFOTL_DEFAULT | IT87_FIFOCLR,
		dev->cir_addr + IT87_TCR1);

	 
	ite_set_carrier_params(dev);
}

 

 
static int it8708_get_irq_causes(struct ite_dev *dev)
{
	u8 iflags;
	int ret = 0;

	 
	iflags = inb(dev->cir_addr + IT8708_C0IIR);

	if (iflags & IT85_TLDLI)
		ret |= ITE_IRQ_TX_FIFO;
	if (iflags & IT85_RDAI)
		ret |= ITE_IRQ_RX_FIFO;
	if (iflags & IT85_RFOI)
		ret |= ITE_IRQ_RX_FIFO_OVERRUN;

	return ret;
}

 
static void it8708_set_carrier_params(struct ite_dev *dev, bool high_freq,
				      bool use_demodulator,
				      u8 carrier_freq_bits, u8 allowance_bits,
				      u8 pulse_width_bits)
{
	u8 val;

	 
	outb(inb(dev->cir_addr + IT8708_BANKSEL) | IT8708_HRAE,
		dev->cir_addr + IT8708_BANKSEL);

	val = (inb(dev->cir_addr + IT8708_C0CFR)
		& ~(IT85_HCFS | IT85_CFQ)) | carrier_freq_bits;

	if (high_freq)
		val |= IT85_HCFS;

	outb(val, dev->cir_addr + IT8708_C0CFR);

	outb(inb(dev->cir_addr + IT8708_BANKSEL) & ~IT8708_HRAE,
		   dev->cir_addr + IT8708_BANKSEL);

	 
	val = inb(dev->cir_addr + IT8708_C0RCR)
		& ~(IT85_RXEND | IT85_RXDCR);

	if (use_demodulator)
		val |= IT85_RXEND;

	val |= allowance_bits;

	outb(val, dev->cir_addr + IT8708_C0RCR);

	 
	val = inb(dev->cir_addr + IT8708_C0TCR) & ~IT85_TXMPW;
	val |= pulse_width_bits;
	outb(val, dev->cir_addr + IT8708_C0TCR);
}

 
static int it8708_get_rx_bytes(struct ite_dev *dev, u8 * buf, int buf_size)
{
	int fifo, read = 0;

	 
	fifo = inb(dev->cir_addr + IT8708_C0RFSR) & IT85_RXFBC;

	while (fifo > 0 && buf_size > 0) {
		*(buf++) = inb(dev->cir_addr + IT8708_C0DR);
		fifo--;
		read++;
		buf_size--;
	}

	return read;
}

 
static int it8708_get_tx_used_slots(struct ite_dev *dev)
{
	return inb(dev->cir_addr + IT8708_C0TFSR) & IT85_TXFBC;
}

 
static void it8708_put_tx_byte(struct ite_dev *dev, u8 value)
{
	outb(value, dev->cir_addr + IT8708_C0DR);
}

 
static void it8708_idle_rx(struct ite_dev *dev)
{
	 
	outb(inb(dev->cir_addr + IT8708_C0RCR) | IT85_RXACT,
		dev->cir_addr + IT8708_C0RCR);

	 
	outb(inb(dev->cir_addr + IT8708_C0MSTCR) | IT85_FIFOCLR,
		dev->cir_addr + IT8708_C0MSTCR);
}

 
static void it8708_disable_rx(struct ite_dev *dev)
{
	 
	outb(inb(dev->cir_addr + IT8708_C0IER) &
		~(IT85_RDAIE | IT85_RFOIE),
		dev->cir_addr + IT8708_C0IER);

	 
	outb(inb(dev->cir_addr + IT8708_C0RCR) & ~IT85_RXEN,
		dev->cir_addr + IT8708_C0RCR);

	 
	it8708_idle_rx(dev);
}

 
static void it8708_enable_rx(struct ite_dev *dev)
{
	 
	outb(inb(dev->cir_addr + IT8708_C0RCR) | IT85_RXEN,
		dev->cir_addr + IT8708_C0RCR);

	 
	it8708_idle_rx(dev);

	 
	outb(inb(dev->cir_addr + IT8708_C0IER)
		|IT85_RDAIE | IT85_RFOIE | IT85_IEC,
		dev->cir_addr + IT8708_C0IER);
}

 
static void it8708_disable_tx_interrupt(struct ite_dev *dev)
{
	 
	outb(inb(dev->cir_addr + IT8708_C0IER) & ~IT85_TLDLIE,
		dev->cir_addr + IT8708_C0IER);
}

 
static void it8708_enable_tx_interrupt(struct ite_dev *dev)
{
	 
	outb(inb(dev->cir_addr + IT8708_C0IER)
		|IT85_TLDLIE | IT85_IEC,
		dev->cir_addr + IT8708_C0IER);
}

 
static void it8708_disable(struct ite_dev *dev)
{
	 
	outb(inb(dev->cir_addr + IT8708_C0IER) &
		~(IT85_IEC | IT85_RFOIE | IT85_RDAIE | IT85_TLDLIE),
		dev->cir_addr + IT8708_C0IER);

	 
	it8708_disable_rx(dev);

	 
	outb(IT85_FIFOCLR | inb(dev->cir_addr + IT8708_C0MSTCR),
		dev->cir_addr + IT8708_C0MSTCR);
}

 
static void it8708_init_hardware(struct ite_dev *dev)
{
	 
	outb(inb(dev->cir_addr + IT8708_C0IER) &
		~(IT85_IEC | IT85_RFOIE | IT85_RDAIE | IT85_TLDLIE),
		dev->cir_addr + IT8708_C0IER);

	 
	outb(inb(dev->cir_addr + IT8708_BANKSEL) | IT8708_HRAE,
		dev->cir_addr + IT8708_BANKSEL);

	outb(ITE_BAUDRATE_DIVISOR & 0xff, dev->cir_addr + IT8708_C0BDLR);
	outb((ITE_BAUDRATE_DIVISOR >> 8) & 0xff,
		   dev->cir_addr + IT8708_C0BDHR);

	outb(inb(dev->cir_addr + IT8708_BANKSEL) & ~IT8708_HRAE,
		   dev->cir_addr + IT8708_BANKSEL);

	 
	outb((inb(dev->cir_addr + IT8708_C0MSTCR) &
			~(IT85_ILSEL | IT85_ILE | IT85_FIFOTL |
			  IT85_FIFOCLR | IT85_RESET)) |
		       IT85_FIFOTL_DEFAULT,
		       dev->cir_addr + IT8708_C0MSTCR);

	 
	outb((inb(dev->cir_addr + IT8708_C0RCR) &
			~(IT85_RXEN | IT85_RDWOS | IT85_RXEND |
			  IT85_RXACT | IT85_RXDCR)) |
		       ITE_RXDCR_DEFAULT,
		       dev->cir_addr + IT8708_C0RCR);

	 
	outb((inb(dev->cir_addr + IT8708_C0TCR) &
			~(IT85_TXMPM | IT85_TXMPW))
		       |IT85_TXRLE | IT85_TXENDF |
		       IT85_TXMPM_DEFAULT | IT85_TXMPW_DEFAULT,
		       dev->cir_addr + IT8708_C0TCR);

	 
	ite_set_carrier_params(dev);
}

 

 
static inline u8 it8709_rm(struct ite_dev *dev, int index)
{
	outb(index, dev->cir_addr + IT8709_RAM_IDX);
	return inb(dev->cir_addr + IT8709_RAM_VAL);
}

 
static inline void it8709_wm(struct ite_dev *dev, u8 val, int index)
{
	outb(index, dev->cir_addr + IT8709_RAM_IDX);
	outb(val, dev->cir_addr + IT8709_RAM_VAL);
}

static void it8709_wait(struct ite_dev *dev)
{
	int i = 0;
	 
	for (i = 0; i < 15000; i++) {
		udelay(2);
		if (it8709_rm(dev, IT8709_MODE) == IT8709_IDLE)
			break;
	}
}

 
static u8 it8709_rr(struct ite_dev *dev, int index)
{
	 
	it8709_wait(dev);
	it8709_wm(dev, index, IT8709_REG_IDX);
	it8709_wm(dev, IT8709_READ, IT8709_MODE);

	 
	it8709_wait(dev);

	 
	return it8709_rm(dev, IT8709_REG_VAL);
}

 
static void it8709_wr(struct ite_dev *dev, u8 val, int index)
{
	 
	it8709_wait(dev);
	it8709_wm(dev, val, IT8709_REG_VAL);
	it8709_wm(dev, index, IT8709_REG_IDX);
	it8709_wm(dev, IT8709_WRITE, IT8709_MODE);
}

 
static int it8709_get_irq_causes(struct ite_dev *dev)
{
	u8 iflags;
	int ret = 0;

	 
	iflags = it8709_rm(dev, IT8709_IIR);

	if (iflags & IT85_TLDLI)
		ret |= ITE_IRQ_TX_FIFO;
	if (iflags & IT85_RDAI)
		ret |= ITE_IRQ_RX_FIFO;
	if (iflags & IT85_RFOI)
		ret |= ITE_IRQ_RX_FIFO_OVERRUN;

	return ret;
}

 
static void it8709_set_carrier_params(struct ite_dev *dev, bool high_freq,
				      bool use_demodulator,
				      u8 carrier_freq_bits, u8 allowance_bits,
				      u8 pulse_width_bits)
{
	u8 val;

	val = (it8709_rr(dev, IT85_C0CFR)
		     &~(IT85_HCFS | IT85_CFQ)) |
	    carrier_freq_bits;

	if (high_freq)
		val |= IT85_HCFS;

	it8709_wr(dev, val, IT85_C0CFR);

	 
	val = it8709_rr(dev, IT85_C0RCR)
		& ~(IT85_RXEND | IT85_RXDCR);

	if (use_demodulator)
		val |= IT85_RXEND;

	val |= allowance_bits;

	it8709_wr(dev, val, IT85_C0RCR);

	 
	val = it8709_rr(dev, IT85_C0TCR) & ~IT85_TXMPW;
	val |= pulse_width_bits;
	it8709_wr(dev, val, IT85_C0TCR);
}

 
static int it8709_get_rx_bytes(struct ite_dev *dev, u8 * buf, int buf_size)
{
	int fifo, read = 0;

	 
	fifo = it8709_rm(dev, IT8709_RFSR) & IT85_RXFBC;

	while (fifo > 0 && buf_size > 0) {
		*(buf++) = it8709_rm(dev, IT8709_FIFO + read);
		fifo--;
		read++;
		buf_size--;
	}

	 
	it8709_wm(dev, 0, IT8709_RFSR);

	return read;
}

 
static int it8709_get_tx_used_slots(struct ite_dev *dev)
{
	return it8709_rr(dev, IT85_C0TFSR) & IT85_TXFBC;
}

 
static void it8709_put_tx_byte(struct ite_dev *dev, u8 value)
{
	it8709_wr(dev, value, IT85_C0DR);
}

 
static void it8709_idle_rx(struct ite_dev *dev)
{
	 
	it8709_wr(dev, it8709_rr(dev, IT85_C0RCR) | IT85_RXACT,
			    IT85_C0RCR);

	 
	it8709_wr(dev, it8709_rr(dev, IT85_C0MSTCR) | IT85_FIFOCLR,
			    IT85_C0MSTCR);
}

 
static void it8709_disable_rx(struct ite_dev *dev)
{
	 
	it8709_wr(dev, it8709_rr(dev, IT85_C0IER) &
			    ~(IT85_RDAIE | IT85_RFOIE),
			    IT85_C0IER);

	 
	it8709_wr(dev, it8709_rr(dev, IT85_C0RCR) & ~IT85_RXEN,
			    IT85_C0RCR);

	 
	it8709_idle_rx(dev);
}

 
static void it8709_enable_rx(struct ite_dev *dev)
{
	 
	it8709_wr(dev, it8709_rr(dev, IT85_C0RCR) | IT85_RXEN,
			    IT85_C0RCR);

	 
	it8709_idle_rx(dev);

	 
	it8709_wr(dev, it8709_rr(dev, IT85_C0IER)
			    |IT85_RDAIE | IT85_RFOIE | IT85_IEC,
			    IT85_C0IER);
}

 
static void it8709_disable_tx_interrupt(struct ite_dev *dev)
{
	 
	it8709_wr(dev, it8709_rr(dev, IT85_C0IER) & ~IT85_TLDLIE,
			    IT85_C0IER);
}

 
static void it8709_enable_tx_interrupt(struct ite_dev *dev)
{
	 
	it8709_wr(dev, it8709_rr(dev, IT85_C0IER)
			    |IT85_TLDLIE | IT85_IEC,
			    IT85_C0IER);
}

 
static void it8709_disable(struct ite_dev *dev)
{
	 
	it8709_wr(dev, it8709_rr(dev, IT85_C0IER) &
			~(IT85_IEC | IT85_RFOIE | IT85_RDAIE | IT85_TLDLIE),
		  IT85_C0IER);

	 
	it8709_disable_rx(dev);

	 
	it8709_wr(dev, IT85_FIFOCLR | it8709_rr(dev, IT85_C0MSTCR),
			    IT85_C0MSTCR);
}

 
static void it8709_init_hardware(struct ite_dev *dev)
{
	 
	it8709_wr(dev, it8709_rr(dev, IT85_C0IER) &
			~(IT85_IEC | IT85_RFOIE | IT85_RDAIE | IT85_TLDLIE),
		  IT85_C0IER);

	 
	it8709_wr(dev, ITE_BAUDRATE_DIVISOR & 0xff, IT85_C0BDLR);
	it8709_wr(dev, (ITE_BAUDRATE_DIVISOR >> 8) & 0xff,
			IT85_C0BDHR);

	 
	it8709_wr(dev, (it8709_rr(dev, IT85_C0MSTCR) &
			~(IT85_ILSEL | IT85_ILE | IT85_FIFOTL
			  | IT85_FIFOCLR | IT85_RESET)) | IT85_FIFOTL_DEFAULT,
		  IT85_C0MSTCR);

	 
	it8709_wr(dev, (it8709_rr(dev, IT85_C0RCR) &
			~(IT85_RXEN | IT85_RDWOS | IT85_RXEND | IT85_RXACT
			  | IT85_RXDCR)) | ITE_RXDCR_DEFAULT,
		  IT85_C0RCR);

	 
	it8709_wr(dev, (it8709_rr(dev, IT85_C0TCR) & ~(IT85_TXMPM | IT85_TXMPW))
			| IT85_TXRLE | IT85_TXENDF | IT85_TXMPM_DEFAULT
			| IT85_TXMPW_DEFAULT,
		  IT85_C0TCR);

	 
	ite_set_carrier_params(dev);
}


 

 
static int ite_open(struct rc_dev *rcdev)
{
	struct ite_dev *dev = rcdev->priv;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);

	 
	dev->params->enable_rx(dev);

	spin_unlock_irqrestore(&dev->lock, flags);

	return 0;
}

 
static void ite_close(struct rc_dev *rcdev)
{
	struct ite_dev *dev = rcdev->priv;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);

	 
	spin_unlock_irqrestore(&dev->lock, flags);
	wait_event_interruptible(dev->tx_ended, !dev->transmitting);
	spin_lock_irqsave(&dev->lock, flags);

	dev->params->disable(dev);

	spin_unlock_irqrestore(&dev->lock, flags);
}

 
static const struct ite_dev_params ite_dev_descs[] = {
	{	 
	       .model = "ITE8704 CIR transceiver",
	       .io_region_size = IT87_IOREG_LENGTH,
	       .io_rsrc_no = 0,

		 
	       .get_irq_causes = it87_get_irq_causes,
	       .enable_rx = it87_enable_rx,
	       .idle_rx = it87_idle_rx,
	       .disable_rx = it87_idle_rx,
	       .get_rx_bytes = it87_get_rx_bytes,
	       .enable_tx_interrupt = it87_enable_tx_interrupt,
	       .disable_tx_interrupt = it87_disable_tx_interrupt,
	       .get_tx_used_slots = it87_get_tx_used_slots,
	       .put_tx_byte = it87_put_tx_byte,
	       .disable = it87_disable,
	       .init_hardware = it87_init_hardware,
	       .set_carrier_params = it87_set_carrier_params,
	       },
	{	 
	       .model = "ITE8713 CIR transceiver",
	       .io_region_size = IT87_IOREG_LENGTH,
	       .io_rsrc_no = 0,

		 
	       .get_irq_causes = it87_get_irq_causes,
	       .enable_rx = it87_enable_rx,
	       .idle_rx = it87_idle_rx,
	       .disable_rx = it87_idle_rx,
	       .get_rx_bytes = it87_get_rx_bytes,
	       .enable_tx_interrupt = it87_enable_tx_interrupt,
	       .disable_tx_interrupt = it87_disable_tx_interrupt,
	       .get_tx_used_slots = it87_get_tx_used_slots,
	       .put_tx_byte = it87_put_tx_byte,
	       .disable = it87_disable,
	       .init_hardware = it87_init_hardware,
	       .set_carrier_params = it87_set_carrier_params,
	       },
	{	 
	       .model = "ITE8708 CIR transceiver",
	       .io_region_size = IT8708_IOREG_LENGTH,
	       .io_rsrc_no = 0,

		 
	       .get_irq_causes = it8708_get_irq_causes,
	       .enable_rx = it8708_enable_rx,
	       .idle_rx = it8708_idle_rx,
	       .disable_rx = it8708_idle_rx,
	       .get_rx_bytes = it8708_get_rx_bytes,
	       .enable_tx_interrupt = it8708_enable_tx_interrupt,
	       .disable_tx_interrupt =
	       it8708_disable_tx_interrupt,
	       .get_tx_used_slots = it8708_get_tx_used_slots,
	       .put_tx_byte = it8708_put_tx_byte,
	       .disable = it8708_disable,
	       .init_hardware = it8708_init_hardware,
	       .set_carrier_params = it8708_set_carrier_params,
	       },
	{	 
	       .model = "ITE8709 CIR transceiver",
	       .io_region_size = IT8709_IOREG_LENGTH,
	       .io_rsrc_no = 2,

		 
	       .get_irq_causes = it8709_get_irq_causes,
	       .enable_rx = it8709_enable_rx,
	       .idle_rx = it8709_idle_rx,
	       .disable_rx = it8709_idle_rx,
	       .get_rx_bytes = it8709_get_rx_bytes,
	       .enable_tx_interrupt = it8709_enable_tx_interrupt,
	       .disable_tx_interrupt =
	       it8709_disable_tx_interrupt,
	       .get_tx_used_slots = it8709_get_tx_used_slots,
	       .put_tx_byte = it8709_put_tx_byte,
	       .disable = it8709_disable,
	       .init_hardware = it8709_init_hardware,
	       .set_carrier_params = it8709_set_carrier_params,
	       },
};

static const struct pnp_device_id ite_ids[] = {
	{"ITE8704", 0},		 
	{"ITE8713", 1},		 
	{"ITE8708", 2},		 
	{"ITE8709", 3},		 
	{"", 0},
};

 
static int ite_probe(struct pnp_dev *pdev, const struct pnp_device_id
		     *dev_id)
{
	const struct ite_dev_params *dev_desc = NULL;
	struct ite_dev *itdev = NULL;
	struct rc_dev *rdev = NULL;
	int ret = -ENOMEM;
	int model_no;
	int io_rsrc_no;

	itdev = kzalloc(sizeof(struct ite_dev), GFP_KERNEL);
	if (!itdev)
		return ret;

	 
	rdev = rc_allocate_device(RC_DRIVER_IR_RAW);
	if (!rdev)
		goto exit_free_dev_rdev;
	itdev->rdev = rdev;

	ret = -ENODEV;

	 
	model_no = (int)dev_id->driver_data;
	dev_dbg(&pdev->dev, "Auto-detected model: %s\n",
		ite_dev_descs[model_no].model);

	if (model_number >= 0 && model_number < ARRAY_SIZE(ite_dev_descs)) {
		model_no = model_number;
		dev_info(&pdev->dev, "model has been forced to: %s",
			 ite_dev_descs[model_no].model);
	}

	 
	dev_desc = &ite_dev_descs[model_no];
	io_rsrc_no = dev_desc->io_rsrc_no;

	 
	if (!pnp_port_valid(pdev, io_rsrc_no) ||
	    pnp_port_len(pdev, io_rsrc_no) < dev_desc->io_region_size) {
		dev_err(&pdev->dev, "IR PNP Port not valid!\n");
		goto exit_free_dev_rdev;
	}

	if (!pnp_irq_valid(pdev, 0)) {
		dev_err(&pdev->dev, "PNP IRQ not valid!\n");
		goto exit_free_dev_rdev;
	}

	 
	itdev->cir_addr = pnp_port_start(pdev, io_rsrc_no);
	itdev->cir_irq = pnp_irq(pdev, 0);

	 
	spin_lock_init(&itdev->lock);

	 
	pnp_set_drvdata(pdev, itdev);
	itdev->pdev = pdev;

	 
	init_waitqueue_head(&itdev->tx_queue);
	init_waitqueue_head(&itdev->tx_ended);

	 
	itdev->params = dev_desc;

	 
	itdev->tx_duty_cycle = 33;
	itdev->tx_carrier_freq = ITE_DEFAULT_CARRIER_FREQ;
	itdev->params->init_hardware(itdev);

	 
	rdev->priv = itdev;
	rdev->dev.parent = &pdev->dev;
	rdev->allowed_protocols = RC_PROTO_BIT_ALL_IR_DECODER;
	rdev->open = ite_open;
	rdev->close = ite_close;
	rdev->s_idle = ite_s_idle;
	rdev->s_rx_carrier_range = ite_set_rx_carrier_range;
	 
	rdev->min_timeout = 17 * 8 * ITE_BAUDRATE_DIVISOR *
			    sample_period / 1000;
	rdev->timeout = IR_DEFAULT_TIMEOUT;
	rdev->max_timeout = 10 * IR_DEFAULT_TIMEOUT;
	rdev->rx_resolution = ITE_BAUDRATE_DIVISOR * sample_period / 1000;
	rdev->tx_resolution = ITE_BAUDRATE_DIVISOR * sample_period / 1000;

	 
	rdev->tx_ir = ite_tx_ir;
	rdev->s_tx_carrier = ite_set_tx_carrier;
	rdev->s_tx_duty_cycle = ite_set_tx_duty_cycle;

	rdev->device_name = dev_desc->model;
	rdev->input_id.bustype = BUS_HOST;
	rdev->input_id.vendor = PCI_VENDOR_ID_ITE;
	rdev->input_id.product = 0;
	rdev->input_id.version = 0;
	rdev->driver_name = ITE_DRIVER_NAME;
	rdev->map_name = RC_MAP_RC6_MCE;

	ret = rc_register_device(rdev);
	if (ret)
		goto exit_free_dev_rdev;

	ret = -EBUSY;
	 
	if (!request_region(itdev->cir_addr,
				dev_desc->io_region_size, ITE_DRIVER_NAME))
		goto exit_unregister_device;

	if (request_irq(itdev->cir_irq, ite_cir_isr, IRQF_SHARED,
			ITE_DRIVER_NAME, (void *)itdev))
		goto exit_release_cir_addr;

	return 0;

exit_release_cir_addr:
	release_region(itdev->cir_addr, itdev->params->io_region_size);
exit_unregister_device:
	rc_unregister_device(rdev);
	rdev = NULL;
exit_free_dev_rdev:
	rc_free_device(rdev);
	kfree(itdev);

	return ret;
}

static void ite_remove(struct pnp_dev *pdev)
{
	struct ite_dev *dev = pnp_get_drvdata(pdev);
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);

	 
	dev->params->disable(dev);

	spin_unlock_irqrestore(&dev->lock, flags);

	 
	free_irq(dev->cir_irq, dev);
	release_region(dev->cir_addr, dev->params->io_region_size);

	rc_unregister_device(dev->rdev);

	kfree(dev);
}

static int ite_suspend(struct pnp_dev *pdev, pm_message_t state)
{
	struct ite_dev *dev = pnp_get_drvdata(pdev);
	unsigned long flags;

	 
	wait_event_interruptible(dev->tx_ended, !dev->transmitting);

	spin_lock_irqsave(&dev->lock, flags);

	 
	dev->params->disable(dev);

	spin_unlock_irqrestore(&dev->lock, flags);

	return 0;
}

static int ite_resume(struct pnp_dev *pdev)
{
	struct ite_dev *dev = pnp_get_drvdata(pdev);
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);

	 
	dev->params->init_hardware(dev);
	 
	dev->params->enable_rx(dev);

	spin_unlock_irqrestore(&dev->lock, flags);

	return 0;
}

static void ite_shutdown(struct pnp_dev *pdev)
{
	struct ite_dev *dev = pnp_get_drvdata(pdev);
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);

	 
	dev->params->disable(dev);

	spin_unlock_irqrestore(&dev->lock, flags);
}

static struct pnp_driver ite_driver = {
	.name		= ITE_DRIVER_NAME,
	.id_table	= ite_ids,
	.probe		= ite_probe,
	.remove		= ite_remove,
	.suspend	= ite_suspend,
	.resume		= ite_resume,
	.shutdown	= ite_shutdown,
};

MODULE_DEVICE_TABLE(pnp, ite_ids);
MODULE_DESCRIPTION("ITE Tech Inc. IT8712F/ITE8512F CIR driver");

MODULE_AUTHOR("Juan J. Garcia de Soria <skandalfo@gmail.com>");
MODULE_LICENSE("GPL");

module_pnp_driver(ite_driver);
