
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/fs.h>
#include <linux/hdlc.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/soc/ixp4xx/npe.h>
#include <linux/soc/ixp4xx/qmgr.h>
#include <linux/soc/ixp4xx/cpu.h>

 
#define IXP4XX_TIMER_FREQ	66666000

#define DEBUG_DESC		0
#define DEBUG_RX		0
#define DEBUG_TX		0
#define DEBUG_PKT_BYTES		0
#define DEBUG_CLOSE		0

#define DRV_NAME		"ixp4xx_hss"

#define PKT_EXTRA_FLAGS		0  
#define PKT_NUM_PIPES		1  
#define PKT_PIPE_FIFO_SIZEW	4  

#define RX_DESCS		16  
#define TX_DESCS		16  

#define POOL_ALLOC_SIZE		(sizeof(struct desc) * (RX_DESCS + TX_DESCS))
#define RX_SIZE			(HDLC_MAX_MRU + 4)  
#define MAX_CLOSE_WAIT		1000  
#define HSS_COUNT		2
#define FRAME_SIZE		256  
#define FRAME_OFFSET		0
#define MAX_CHANNELS		(FRAME_SIZE / 8)

#define NAPI_WEIGHT		16

 
#define HSS0_PKT_RX_QUEUE	13	 
#define HSS0_PKT_TX0_QUEUE	14	 
#define HSS0_PKT_TX1_QUEUE	15
#define HSS0_PKT_TX2_QUEUE	16
#define HSS0_PKT_TX3_QUEUE	17
#define HSS0_PKT_RXFREE0_QUEUE	18	 
#define HSS0_PKT_RXFREE1_QUEUE	19
#define HSS0_PKT_RXFREE2_QUEUE	20
#define HSS0_PKT_RXFREE3_QUEUE	21
#define HSS0_PKT_TXDONE_QUEUE	22	 

#define HSS1_PKT_RX_QUEUE	0
#define HSS1_PKT_TX0_QUEUE	5
#define HSS1_PKT_TX1_QUEUE	6
#define HSS1_PKT_TX2_QUEUE	7
#define HSS1_PKT_TX3_QUEUE	8
#define HSS1_PKT_RXFREE0_QUEUE	1
#define HSS1_PKT_RXFREE1_QUEUE	2
#define HSS1_PKT_RXFREE2_QUEUE	3
#define HSS1_PKT_RXFREE3_QUEUE	4
#define HSS1_PKT_TXDONE_QUEUE	9

#define NPE_PKT_MODE_HDLC		0
#define NPE_PKT_MODE_RAW		1
#define NPE_PKT_MODE_56KMODE		2
#define NPE_PKT_MODE_56KENDIAN_MSB	4

 
#define PKT_HDLC_IDLE_ONES		0x1  
#define PKT_HDLC_CRC_32			0x2  
#define PKT_HDLC_MSB_ENDIAN		0x4  

 
 
#define PCR_FRM_SYNC_ACTIVE_HIGH	0x40000000
#define PCR_FRM_SYNC_FALLINGEDGE	0x80000000
#define PCR_FRM_SYNC_RISINGEDGE		0xC0000000

 
#define PCR_FRM_SYNC_OUTPUT_FALLING	0x20000000
#define PCR_FRM_SYNC_OUTPUT_RISING	0x30000000

 
#define PCR_FCLK_EDGE_RISING		0x08000000
#define PCR_DCLK_EDGE_RISING		0x04000000

 
#define PCR_SYNC_CLK_DIR_OUTPUT		0x02000000

 
#define PCR_FRM_PULSE_DISABLED		0x01000000

  
#define PCR_HALF_CLK_RATE		0x00200000

 
#define PCR_DATA_POLARITY_INVERT	0x00100000

 
#define PCR_MSB_ENDIAN			0x00080000

 
#define PCR_TX_PINS_OPEN_DRAIN		0x00040000

 
#define PCR_SOF_NO_FBIT			0x00020000

 
#define PCR_TX_DATA_ENABLE		0x00010000

 
#define PCR_TX_V56K_HIGH		0x00002000
#define PCR_TX_V56K_HIGH_IMP		0x00004000

 
#define PCR_TX_UNASS_HIGH		0x00000800
#define PCR_TX_UNASS_HIGH_IMP		0x00001000

 
#define PCR_TX_FB_HIGH_IMP		0x00000400

 
#define PCR_TX_56KE_BIT_0_UNUSED	0x00000200

 
#define PCR_TX_56KS_56K_DATA		0x00000100

 
 
#define CCR_NPE_HFIFO_2_HDLC		0x04000000
#define CCR_NPE_HFIFO_3_OR_4HDLC	0x08000000

 
#define CCR_LOOPBACK			0x02000000

 
#define CCR_SECOND_HSS			0x01000000

 
#define CLK42X_SPEED_EXP	((0x3FF << 22) | (2 << 12) |   15)  

#define CLK42X_SPEED_512KHZ	((130 << 22) | (2 << 12) |   15)
#define CLK42X_SPEED_1536KHZ	((43 << 22) | (18 << 12) |   47)
#define CLK42X_SPEED_1544KHZ	((43 << 22) | (33 << 12) |  192)
#define CLK42X_SPEED_2048KHZ	((32 << 22) | (34 << 12) |   63)
#define CLK42X_SPEED_4096KHZ	((16 << 22) | (34 << 12) |  127)
#define CLK42X_SPEED_8192KHZ	((8 << 22) | (34 << 12) |  255)

#define CLK46X_SPEED_512KHZ	((130 << 22) | (24 << 12) |  127)
#define CLK46X_SPEED_1536KHZ	((43 << 22) | (152 << 12) |  383)
#define CLK46X_SPEED_1544KHZ	((43 << 22) | (66 << 12) |  385)
#define CLK46X_SPEED_2048KHZ	((32 << 22) | (280 << 12) |  511)
#define CLK46X_SPEED_4096KHZ	((16 << 22) | (280 << 12) | 1023)
#define CLK46X_SPEED_8192KHZ	((8 << 22) | (280 << 12) | 2047)

 

 
#define TDMMAP_UNASSIGNED	0
#define TDMMAP_HDLC		1	 
#define TDMMAP_VOICE56K		2	 
#define TDMMAP_VOICE64K		3	 

 
#define HSS_CONFIG_TX_PCR	0x00  
#define HSS_CONFIG_RX_PCR	0x04
#define HSS_CONFIG_CORE_CR	0x08  
#define HSS_CONFIG_CLOCK_CR	0x0C  
#define HSS_CONFIG_TX_FCR	0x10  
#define HSS_CONFIG_RX_FCR	0x14
#define HSS_CONFIG_TX_LUT	0x18  
#define HSS_CONFIG_RX_LUT	0x38

 
 
#define PORT_CONFIG_WRITE		0x40

 
#define PORT_CONFIG_LOAD		0x41

 
#define PORT_ERROR_READ			0x42

 
#define PKT_PIPE_FLOW_ENABLE		0x50
#define PKT_PIPE_FLOW_DISABLE		0x51
#define PKT_NUM_PIPES_WRITE		0x52
#define PKT_PIPE_FIFO_SIZEW_WRITE	0x53
#define PKT_PIPE_HDLC_CFG_WRITE		0x54
#define PKT_PIPE_IDLE_PATTERN_WRITE	0x55
#define PKT_PIPE_RX_SIZE_WRITE		0x56
#define PKT_PIPE_MODE_WRITE		0x57

 
#define ERR_SHUTDOWN		1  
#define ERR_HDLC_ALIGN		2  
#define ERR_HDLC_FCS		3  
#define ERR_RXFREE_Q_EMPTY	4  
#define ERR_HDLC_TOO_LONG	5  
#define ERR_HDLC_ABORT		6  
#define ERR_DISCONNECTING	7  

#ifdef __ARMEB__
typedef struct sk_buff buffer_t;
#define free_buffer dev_kfree_skb
#define free_buffer_irq dev_consume_skb_irq
#else
typedef void buffer_t;
#define free_buffer kfree
#define free_buffer_irq kfree
#endif

struct port {
	struct device *dev;
	struct npe *npe;
	unsigned int txreadyq;
	unsigned int rxtrigq;
	unsigned int rxfreeq;
	unsigned int rxq;
	unsigned int txq;
	unsigned int txdoneq;
	struct gpio_desc *cts;
	struct gpio_desc *rts;
	struct gpio_desc *dcd;
	struct gpio_desc *dtr;
	struct gpio_desc *clk_internal;
	struct net_device *netdev;
	struct napi_struct napi;
	buffer_t *rx_buff_tab[RX_DESCS], *tx_buff_tab[TX_DESCS];
	struct desc *desc_tab;	 
	dma_addr_t desc_tab_phys;
	unsigned int id;
	unsigned int clock_type, clock_rate, loopback;
	unsigned int initialized, carrier;
	u8 hdlc_cfg;
	u32 clock_reg;
};

 
struct msg {
#ifdef __ARMEB__
	u8 cmd, unused, hss_port, index;
	union {
		struct { u8 data8a, data8b, data8c, data8d; };
		struct { u16 data16a, data16b; };
		struct { u32 data32; };
	};
#else
	u8 index, hss_port, unused, cmd;
	union {
		struct { u8 data8d, data8c, data8b, data8a; };
		struct { u16 data16b, data16a; };
		struct { u32 data32; };
	};
#endif
};

 
struct desc {
	u32 next;		 

#ifdef __ARMEB__
	u16 buf_len;		 
	u16 pkt_len;		 
	u32 data;		 
	u8 status;
	u8 error_count;
	u16 __reserved;
#else
	u16 pkt_len;		 
	u16 buf_len;		 
	u32 data;		 
	u16 __reserved;
	u8 error_count;
	u8 status;
#endif
	u32 __reserved1[4];
};

#define rx_desc_phys(port, n)	((port)->desc_tab_phys +		\
				 (n) * sizeof(struct desc))
#define rx_desc_ptr(port, n)	(&(port)->desc_tab[n])

#define tx_desc_phys(port, n)	((port)->desc_tab_phys +		\
				 ((n) + RX_DESCS) * sizeof(struct desc))
#define tx_desc_ptr(port, n)	(&(port)->desc_tab[(n) + RX_DESCS])

 

static int ports_open;
static struct dma_pool *dma_pool;
static DEFINE_SPINLOCK(npe_lock);

 

static inline struct port *dev_to_port(struct net_device *dev)
{
	return dev_to_hdlc(dev)->priv;
}

#ifndef __ARMEB__
static inline void memcpy_swab32(u32 *dest, u32 *src, int cnt)
{
	int i;

	for (i = 0; i < cnt; i++)
		dest[i] = swab32(src[i]);
}
#endif

 

static void hss_npe_send(struct port *port, struct msg *msg, const char *what)
{
	u32 *val = (u32 *)msg;

	if (npe_send_message(port->npe, msg, what)) {
		pr_crit("HSS-%i: unable to send command [%08X:%08X] to %s\n",
			port->id, val[0], val[1], npe_name(port->npe));
		BUG();
	}
}

static void hss_config_set_lut(struct port *port)
{
	struct msg msg;
	int ch;

	memset(&msg, 0, sizeof(msg));
	msg.cmd = PORT_CONFIG_WRITE;
	msg.hss_port = port->id;

	for (ch = 0; ch < MAX_CHANNELS; ch++) {
		msg.data32 >>= 2;
		msg.data32 |= TDMMAP_HDLC << 30;

		if (ch % 16 == 15) {
			msg.index = HSS_CONFIG_TX_LUT + ((ch / 4) & ~3);
			hss_npe_send(port, &msg, "HSS_SET_TX_LUT");

			msg.index += HSS_CONFIG_RX_LUT - HSS_CONFIG_TX_LUT;
			hss_npe_send(port, &msg, "HSS_SET_RX_LUT");
		}
	}
}

static void hss_config(struct port *port)
{
	struct msg msg;

	memset(&msg, 0, sizeof(msg));
	msg.cmd = PORT_CONFIG_WRITE;
	msg.hss_port = port->id;
	msg.index = HSS_CONFIG_TX_PCR;
	msg.data32 = PCR_FRM_PULSE_DISABLED | PCR_MSB_ENDIAN |
		PCR_TX_DATA_ENABLE | PCR_SOF_NO_FBIT;
	if (port->clock_type == CLOCK_INT)
		msg.data32 |= PCR_SYNC_CLK_DIR_OUTPUT;
	hss_npe_send(port, &msg, "HSS_SET_TX_PCR");

	msg.index = HSS_CONFIG_RX_PCR;
	msg.data32 ^= PCR_TX_DATA_ENABLE | PCR_DCLK_EDGE_RISING;
	hss_npe_send(port, &msg, "HSS_SET_RX_PCR");

	memset(&msg, 0, sizeof(msg));
	msg.cmd = PORT_CONFIG_WRITE;
	msg.hss_port = port->id;
	msg.index = HSS_CONFIG_CORE_CR;
	msg.data32 = (port->loopback ? CCR_LOOPBACK : 0) |
		(port->id ? CCR_SECOND_HSS : 0);
	hss_npe_send(port, &msg, "HSS_SET_CORE_CR");

	memset(&msg, 0, sizeof(msg));
	msg.cmd = PORT_CONFIG_WRITE;
	msg.hss_port = port->id;
	msg.index = HSS_CONFIG_CLOCK_CR;
	msg.data32 = port->clock_reg;
	hss_npe_send(port, &msg, "HSS_SET_CLOCK_CR");

	memset(&msg, 0, sizeof(msg));
	msg.cmd = PORT_CONFIG_WRITE;
	msg.hss_port = port->id;
	msg.index = HSS_CONFIG_TX_FCR;
	msg.data16a = FRAME_OFFSET;
	msg.data16b = FRAME_SIZE - 1;
	hss_npe_send(port, &msg, "HSS_SET_TX_FCR");

	memset(&msg, 0, sizeof(msg));
	msg.cmd = PORT_CONFIG_WRITE;
	msg.hss_port = port->id;
	msg.index = HSS_CONFIG_RX_FCR;
	msg.data16a = FRAME_OFFSET;
	msg.data16b = FRAME_SIZE - 1;
	hss_npe_send(port, &msg, "HSS_SET_RX_FCR");

	hss_config_set_lut(port);

	memset(&msg, 0, sizeof(msg));
	msg.cmd = PORT_CONFIG_LOAD;
	msg.hss_port = port->id;
	hss_npe_send(port, &msg, "HSS_LOAD_CONFIG");

	if (npe_recv_message(port->npe, &msg, "HSS_LOAD_CONFIG") ||
	     
	    msg.cmd != PORT_CONFIG_LOAD || msg.data32) {
		pr_crit("HSS-%i: HSS_LOAD_CONFIG failed\n", port->id);
		BUG();
	}

	 
	npe_recv_message(port->npe, &msg, "FLUSH_IT");
}

static void hss_set_hdlc_cfg(struct port *port)
{
	struct msg msg;

	memset(&msg, 0, sizeof(msg));
	msg.cmd = PKT_PIPE_HDLC_CFG_WRITE;
	msg.hss_port = port->id;
	msg.data8a = port->hdlc_cfg;  
	msg.data8b = port->hdlc_cfg | (PKT_EXTRA_FLAGS << 3);  
	hss_npe_send(port, &msg, "HSS_SET_HDLC_CFG");
}

static u32 hss_get_status(struct port *port)
{
	struct msg msg;

	memset(&msg, 0, sizeof(msg));
	msg.cmd = PORT_ERROR_READ;
	msg.hss_port = port->id;
	hss_npe_send(port, &msg, "PORT_ERROR_READ");
	if (npe_recv_message(port->npe, &msg, "PORT_ERROR_READ")) {
		pr_crit("HSS-%i: unable to read HSS status\n", port->id);
		BUG();
	}

	return msg.data32;
}

static void hss_start_hdlc(struct port *port)
{
	struct msg msg;

	memset(&msg, 0, sizeof(msg));
	msg.cmd = PKT_PIPE_FLOW_ENABLE;
	msg.hss_port = port->id;
	msg.data32 = 0;
	hss_npe_send(port, &msg, "HSS_ENABLE_PKT_PIPE");
}

static void hss_stop_hdlc(struct port *port)
{
	struct msg msg;

	memset(&msg, 0, sizeof(msg));
	msg.cmd = PKT_PIPE_FLOW_DISABLE;
	msg.hss_port = port->id;
	hss_npe_send(port, &msg, "HSS_DISABLE_PKT_PIPE");
	hss_get_status(port);  
}

static int hss_load_firmware(struct port *port)
{
	struct msg msg;
	int err;

	if (port->initialized)
		return 0;

	if (!npe_running(port->npe)) {
		err = npe_load_firmware(port->npe, npe_name(port->npe),
					port->dev);
		if (err)
			return err;
	}

	 
	memset(&msg, 0, sizeof(msg));
	msg.cmd = PKT_NUM_PIPES_WRITE;
	msg.hss_port = port->id;
	msg.data8a = PKT_NUM_PIPES;
	hss_npe_send(port, &msg, "HSS_SET_PKT_PIPES");

	msg.cmd = PKT_PIPE_FIFO_SIZEW_WRITE;
	msg.data8a = PKT_PIPE_FIFO_SIZEW;
	hss_npe_send(port, &msg, "HSS_SET_PKT_FIFO");

	msg.cmd = PKT_PIPE_MODE_WRITE;
	msg.data8a = NPE_PKT_MODE_HDLC;
	 
	 
	hss_npe_send(port, &msg, "HSS_SET_PKT_MODE");

	msg.cmd = PKT_PIPE_RX_SIZE_WRITE;
	msg.data16a = HDLC_MAX_MRU;  
	hss_npe_send(port, &msg, "HSS_SET_PKT_RX_SIZE");

	msg.cmd = PKT_PIPE_IDLE_PATTERN_WRITE;
	msg.data32 = 0x7F7F7F7F;  
	hss_npe_send(port, &msg, "HSS_SET_PKT_IDLE");

	port->initialized = 1;
	return 0;
}

 

static inline void debug_pkt(struct net_device *dev, const char *func,
			     u8 *data, int len)
{
#if DEBUG_PKT_BYTES
	int i;

	printk(KERN_DEBUG "%s: %s(%i)", dev->name, func, len);
	for (i = 0; i < len; i++) {
		if (i >= DEBUG_PKT_BYTES)
			break;
		printk("%s%02X", !(i % 4) ? " " : "", data[i]);
	}
	printk("\n");
#endif
}

static inline void debug_desc(u32 phys, struct desc *desc)
{
#if DEBUG_DESC
	printk(KERN_DEBUG "%X: %X %3X %3X %08X %X %X\n",
	       phys, desc->next, desc->buf_len, desc->pkt_len,
	       desc->data, desc->status, desc->error_count);
#endif
}

static inline int queue_get_desc(unsigned int queue, struct port *port,
				 int is_tx)
{
	u32 phys, tab_phys, n_desc;
	struct desc *tab;

	phys = qmgr_get_entry(queue);
	if (!phys)
		return -1;

	BUG_ON(phys & 0x1F);
	tab_phys = is_tx ? tx_desc_phys(port, 0) : rx_desc_phys(port, 0);
	tab = is_tx ? tx_desc_ptr(port, 0) : rx_desc_ptr(port, 0);
	n_desc = (phys - tab_phys) / sizeof(struct desc);
	BUG_ON(n_desc >= (is_tx ? TX_DESCS : RX_DESCS));
	debug_desc(phys, &tab[n_desc]);
	BUG_ON(tab[n_desc].next);
	return n_desc;
}

static inline void queue_put_desc(unsigned int queue, u32 phys,
				  struct desc *desc)
{
	debug_desc(phys, desc);
	BUG_ON(phys & 0x1F);
	qmgr_put_entry(queue, phys);
	 
}

static inline void dma_unmap_tx(struct port *port, struct desc *desc)
{
#ifdef __ARMEB__
	dma_unmap_single(&port->netdev->dev, desc->data,
			 desc->buf_len, DMA_TO_DEVICE);
#else
	dma_unmap_single(&port->netdev->dev, desc->data & ~3,
			 ALIGN((desc->data & 3) + desc->buf_len, 4),
			 DMA_TO_DEVICE);
#endif
}

static void hss_hdlc_set_carrier(void *pdev, int carrier)
{
	struct net_device *netdev = pdev;
	struct port *port = dev_to_port(netdev);
	unsigned long flags;

	spin_lock_irqsave(&npe_lock, flags);
	port->carrier = carrier;
	if (!port->loopback) {
		if (carrier)
			netif_carrier_on(netdev);
		else
			netif_carrier_off(netdev);
	}
	spin_unlock_irqrestore(&npe_lock, flags);
}

static void hss_hdlc_rx_irq(void *pdev)
{
	struct net_device *dev = pdev;
	struct port *port = dev_to_port(dev);

#if DEBUG_RX
	printk(KERN_DEBUG "%s: hss_hdlc_rx_irq\n", dev->name);
#endif
	qmgr_disable_irq(port->rxq);
	napi_schedule(&port->napi);
}

static int hss_hdlc_poll(struct napi_struct *napi, int budget)
{
	struct port *port = container_of(napi, struct port, napi);
	struct net_device *dev = port->netdev;
	unsigned int rxq = port->rxq;
	unsigned int rxfreeq = port->rxfreeq;
	int received = 0;

#if DEBUG_RX
	printk(KERN_DEBUG "%s: hss_hdlc_poll\n", dev->name);
#endif

	while (received < budget) {
		struct sk_buff *skb;
		struct desc *desc;
		int n;
#ifdef __ARMEB__
		struct sk_buff *temp;
		u32 phys;
#endif

		n = queue_get_desc(rxq, port, 0);
		if (n < 0) {
#if DEBUG_RX
			printk(KERN_DEBUG "%s: hss_hdlc_poll"
			       " napi_complete\n", dev->name);
#endif
			napi_complete(napi);
			qmgr_enable_irq(rxq);
			if (!qmgr_stat_empty(rxq) &&
			    napi_reschedule(napi)) {
#if DEBUG_RX
				printk(KERN_DEBUG "%s: hss_hdlc_poll"
				       " napi_reschedule succeeded\n",
				       dev->name);
#endif
				qmgr_disable_irq(rxq);
				continue;
			}
#if DEBUG_RX
			printk(KERN_DEBUG "%s: hss_hdlc_poll all done\n",
			       dev->name);
#endif
			return received;  
		}

		desc = rx_desc_ptr(port, n);
#if 0  
		if (desc->error_count)
			printk(KERN_DEBUG "%s: hss_hdlc_poll status 0x%02X"
			       " errors %u\n", dev->name, desc->status,
			       desc->error_count);
#endif
		skb = NULL;
		switch (desc->status) {
		case 0:
#ifdef __ARMEB__
			skb = netdev_alloc_skb(dev, RX_SIZE);
			if (skb) {
				phys = dma_map_single(&dev->dev, skb->data,
						      RX_SIZE,
						      DMA_FROM_DEVICE);
				if (dma_mapping_error(&dev->dev, phys)) {
					dev_kfree_skb(skb);
					skb = NULL;
				}
			}
#else
			skb = netdev_alloc_skb(dev, desc->pkt_len);
#endif
			if (!skb)
				dev->stats.rx_dropped++;
			break;
		case ERR_HDLC_ALIGN:
		case ERR_HDLC_ABORT:
			dev->stats.rx_frame_errors++;
			dev->stats.rx_errors++;
			break;
		case ERR_HDLC_FCS:
			dev->stats.rx_crc_errors++;
			dev->stats.rx_errors++;
			break;
		case ERR_HDLC_TOO_LONG:
			dev->stats.rx_length_errors++;
			dev->stats.rx_errors++;
			break;
		default:	 
			netdev_err(dev, "hss_hdlc_poll: status 0x%02X errors %u\n",
				   desc->status, desc->error_count);
			dev->stats.rx_errors++;
		}

		if (!skb) {
			 
			desc->buf_len = RX_SIZE;
			desc->pkt_len = desc->status = 0;
			queue_put_desc(rxfreeq, rx_desc_phys(port, n), desc);
			continue;
		}

		 
#ifdef __ARMEB__
		temp = skb;
		skb = port->rx_buff_tab[n];
		dma_unmap_single(&dev->dev, desc->data,
				 RX_SIZE, DMA_FROM_DEVICE);
#else
		dma_sync_single_for_cpu(&dev->dev, desc->data,
					RX_SIZE, DMA_FROM_DEVICE);
		memcpy_swab32((u32 *)skb->data, (u32 *)port->rx_buff_tab[n],
			      ALIGN(desc->pkt_len, 4) / 4);
#endif
		skb_put(skb, desc->pkt_len);

		debug_pkt(dev, "hss_hdlc_poll", skb->data, skb->len);

		skb->protocol = hdlc_type_trans(skb, dev);
		dev->stats.rx_packets++;
		dev->stats.rx_bytes += skb->len;
		netif_receive_skb(skb);

		 
#ifdef __ARMEB__
		port->rx_buff_tab[n] = temp;
		desc->data = phys;
#endif
		desc->buf_len = RX_SIZE;
		desc->pkt_len = 0;
		queue_put_desc(rxfreeq, rx_desc_phys(port, n), desc);
		received++;
	}
#if DEBUG_RX
	printk(KERN_DEBUG "hss_hdlc_poll: end, not all work done\n");
#endif
	return received;	 
}

static void hss_hdlc_txdone_irq(void *pdev)
{
	struct net_device *dev = pdev;
	struct port *port = dev_to_port(dev);
	int n_desc;

#if DEBUG_TX
	printk(KERN_DEBUG DRV_NAME ": hss_hdlc_txdone_irq\n");
#endif
	while ((n_desc = queue_get_desc(port->txdoneq,
					port, 1)) >= 0) {
		struct desc *desc;
		int start;

		desc = tx_desc_ptr(port, n_desc);

		dev->stats.tx_packets++;
		dev->stats.tx_bytes += desc->pkt_len;

		dma_unmap_tx(port, desc);
#if DEBUG_TX
		printk(KERN_DEBUG "%s: hss_hdlc_txdone_irq free %p\n",
		       dev->name, port->tx_buff_tab[n_desc]);
#endif
		free_buffer_irq(port->tx_buff_tab[n_desc]);
		port->tx_buff_tab[n_desc] = NULL;

		start = qmgr_stat_below_low_watermark(port->txreadyq);
		queue_put_desc(port->txreadyq,
			       tx_desc_phys(port, n_desc), desc);
		if (start) {  
#if DEBUG_TX
			printk(KERN_DEBUG "%s: hss_hdlc_txdone_irq xmit"
			       " ready\n", dev->name);
#endif
			netif_wake_queue(dev);
		}
	}
}

static int hss_hdlc_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct port *port = dev_to_port(dev);
	unsigned int txreadyq = port->txreadyq;
	int len, offset, bytes, n;
	void *mem;
	u32 phys;
	struct desc *desc;

#if DEBUG_TX
	printk(KERN_DEBUG "%s: hss_hdlc_xmit\n", dev->name);
#endif

	if (unlikely(skb->len > HDLC_MAX_MRU)) {
		dev_kfree_skb(skb);
		dev->stats.tx_errors++;
		return NETDEV_TX_OK;
	}

	debug_pkt(dev, "hss_hdlc_xmit", skb->data, skb->len);

	len = skb->len;
#ifdef __ARMEB__
	offset = 0;  
	bytes = len;
	mem = skb->data;
#else
	offset = (int)skb->data & 3;  
	bytes = ALIGN(offset + len, 4);
	mem = kmalloc(bytes, GFP_ATOMIC);
	if (!mem) {
		dev_kfree_skb(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}
	memcpy_swab32(mem, (u32 *)((uintptr_t)skb->data & ~3), bytes / 4);
	dev_kfree_skb(skb);
#endif

	phys = dma_map_single(&dev->dev, mem, bytes, DMA_TO_DEVICE);
	if (dma_mapping_error(&dev->dev, phys)) {
#ifdef __ARMEB__
		dev_kfree_skb(skb);
#else
		kfree(mem);
#endif
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	n = queue_get_desc(txreadyq, port, 1);
	BUG_ON(n < 0);
	desc = tx_desc_ptr(port, n);

#ifdef __ARMEB__
	port->tx_buff_tab[n] = skb;
#else
	port->tx_buff_tab[n] = mem;
#endif
	desc->data = phys + offset;
	desc->buf_len = desc->pkt_len = len;

	wmb();
	queue_put_desc(port->txq, tx_desc_phys(port, n), desc);

	if (qmgr_stat_below_low_watermark(txreadyq)) {  
#if DEBUG_TX
		printk(KERN_DEBUG "%s: hss_hdlc_xmit queue full\n", dev->name);
#endif
		netif_stop_queue(dev);
		 
		if (!qmgr_stat_below_low_watermark(txreadyq)) {
#if DEBUG_TX
			printk(KERN_DEBUG "%s: hss_hdlc_xmit ready again\n",
			       dev->name);
#endif
			netif_wake_queue(dev);
		}
	}

#if DEBUG_TX
	printk(KERN_DEBUG "%s: hss_hdlc_xmit end\n", dev->name);
#endif
	return NETDEV_TX_OK;
}

static int request_hdlc_queues(struct port *port)
{
	int err;

	err = qmgr_request_queue(port->rxfreeq, RX_DESCS, 0, 0,
				 "%s:RX-free", port->netdev->name);
	if (err)
		return err;

	err = qmgr_request_queue(port->rxq, RX_DESCS, 0, 0,
				 "%s:RX", port->netdev->name);
	if (err)
		goto rel_rxfree;

	err = qmgr_request_queue(port->txq, TX_DESCS, 0, 0,
				 "%s:TX", port->netdev->name);
	if (err)
		goto rel_rx;

	err = qmgr_request_queue(port->txreadyq, TX_DESCS, 0, 0,
				 "%s:TX-ready", port->netdev->name);
	if (err)
		goto rel_tx;

	err = qmgr_request_queue(port->txdoneq, TX_DESCS, 0, 0,
				 "%s:TX-done", port->netdev->name);
	if (err)
		goto rel_txready;
	return 0;

rel_txready:
	qmgr_release_queue(port->txreadyq);
rel_tx:
	qmgr_release_queue(port->txq);
rel_rx:
	qmgr_release_queue(port->rxq);
rel_rxfree:
	qmgr_release_queue(port->rxfreeq);
	printk(KERN_DEBUG "%s: unable to request hardware queues\n",
	       port->netdev->name);
	return err;
}

static void release_hdlc_queues(struct port *port)
{
	qmgr_release_queue(port->rxfreeq);
	qmgr_release_queue(port->rxq);
	qmgr_release_queue(port->txdoneq);
	qmgr_release_queue(port->txq);
	qmgr_release_queue(port->txreadyq);
}

static int init_hdlc_queues(struct port *port)
{
	int i;

	if (!ports_open) {
		dma_pool = dma_pool_create(DRV_NAME, &port->netdev->dev,
					   POOL_ALLOC_SIZE, 32, 0);
		if (!dma_pool)
			return -ENOMEM;
	}

	port->desc_tab = dma_pool_zalloc(dma_pool, GFP_KERNEL,
					&port->desc_tab_phys);
	if (!port->desc_tab)
		return -ENOMEM;
	memset(port->rx_buff_tab, 0, sizeof(port->rx_buff_tab));  
	memset(port->tx_buff_tab, 0, sizeof(port->tx_buff_tab));

	 
	for (i = 0; i < RX_DESCS; i++) {
		struct desc *desc = rx_desc_ptr(port, i);
		buffer_t *buff;
		void *data;
#ifdef __ARMEB__
		buff = netdev_alloc_skb(port->netdev, RX_SIZE);
		if (!buff)
			return -ENOMEM;
		data = buff->data;
#else
		buff = kmalloc(RX_SIZE, GFP_KERNEL);
		if (!buff)
			return -ENOMEM;
		data = buff;
#endif
		desc->buf_len = RX_SIZE;
		desc->data = dma_map_single(&port->netdev->dev, data,
					    RX_SIZE, DMA_FROM_DEVICE);
		if (dma_mapping_error(&port->netdev->dev, desc->data)) {
			free_buffer(buff);
			return -EIO;
		}
		port->rx_buff_tab[i] = buff;
	}

	return 0;
}

static void destroy_hdlc_queues(struct port *port)
{
	int i;

	if (port->desc_tab) {
		for (i = 0; i < RX_DESCS; i++) {
			struct desc *desc = rx_desc_ptr(port, i);
			buffer_t *buff = port->rx_buff_tab[i];

			if (buff) {
				dma_unmap_single(&port->netdev->dev,
						 desc->data, RX_SIZE,
						 DMA_FROM_DEVICE);
				free_buffer(buff);
			}
		}
		for (i = 0; i < TX_DESCS; i++) {
			struct desc *desc = tx_desc_ptr(port, i);
			buffer_t *buff = port->tx_buff_tab[i];

			if (buff) {
				dma_unmap_tx(port, desc);
				free_buffer(buff);
			}
		}
		dma_pool_free(dma_pool, port->desc_tab, port->desc_tab_phys);
		port->desc_tab = NULL;
	}

	if (!ports_open && dma_pool) {
		dma_pool_destroy(dma_pool);
		dma_pool = NULL;
	}
}

static irqreturn_t hss_hdlc_dcd_irq(int irq, void *data)
{
	struct net_device *dev = data;
	struct port *port = dev_to_port(dev);
	int val;

	val = gpiod_get_value(port->dcd);
	hss_hdlc_set_carrier(dev, val);

	return IRQ_HANDLED;
}

static int hss_hdlc_open(struct net_device *dev)
{
	struct port *port = dev_to_port(dev);
	unsigned long flags;
	int i, err = 0;
	int val;

	err = hdlc_open(dev);
	if (err)
		return err;

	err = hss_load_firmware(port);
	if (err)
		goto err_hdlc_close;

	err = request_hdlc_queues(port);
	if (err)
		goto err_hdlc_close;

	err = init_hdlc_queues(port);
	if (err)
		goto err_destroy_queues;

	spin_lock_irqsave(&npe_lock, flags);

	 
	val = gpiod_get_value(port->dcd);
	hss_hdlc_set_carrier(dev, val);

	 
	err = request_irq(gpiod_to_irq(port->dcd), hss_hdlc_dcd_irq, 0, "IXP4xx HSS", dev);
	if (err) {
		dev_err(&dev->dev, "ixp4xx_hss: failed to request DCD IRQ (%i)\n", err);
		goto err_unlock;
	}

	 
	gpiod_set_value(port->dtr, 1);
	gpiod_set_value(port->rts, 1);

	spin_unlock_irqrestore(&npe_lock, flags);

	 
	for (i = 0; i < TX_DESCS; i++)
		queue_put_desc(port->txreadyq,
			       tx_desc_phys(port, i), tx_desc_ptr(port, i));

	for (i = 0; i < RX_DESCS; i++)
		queue_put_desc(port->rxfreeq,
			       rx_desc_phys(port, i), rx_desc_ptr(port, i));

	napi_enable(&port->napi);
	netif_start_queue(dev);

	qmgr_set_irq(port->rxq, QUEUE_IRQ_SRC_NOT_EMPTY,
		     hss_hdlc_rx_irq, dev);

	qmgr_set_irq(port->txdoneq, QUEUE_IRQ_SRC_NOT_EMPTY,
		     hss_hdlc_txdone_irq, dev);
	qmgr_enable_irq(port->txdoneq);

	ports_open++;

	hss_set_hdlc_cfg(port);
	hss_config(port);

	hss_start_hdlc(port);

	 
	napi_schedule(&port->napi);
	return 0;

err_unlock:
	spin_unlock_irqrestore(&npe_lock, flags);
err_destroy_queues:
	destroy_hdlc_queues(port);
	release_hdlc_queues(port);
err_hdlc_close:
	hdlc_close(dev);
	return err;
}

static int hss_hdlc_close(struct net_device *dev)
{
	struct port *port = dev_to_port(dev);
	unsigned long flags;
	int i, buffs = RX_DESCS;  

	spin_lock_irqsave(&npe_lock, flags);
	ports_open--;
	qmgr_disable_irq(port->rxq);
	netif_stop_queue(dev);
	napi_disable(&port->napi);

	hss_stop_hdlc(port);

	while (queue_get_desc(port->rxfreeq, port, 0) >= 0)
		buffs--;
	while (queue_get_desc(port->rxq, port, 0) >= 0)
		buffs--;

	if (buffs)
		netdev_crit(dev, "unable to drain RX queue, %i buffer(s) left in NPE\n",
			    buffs);

	buffs = TX_DESCS;
	while (queue_get_desc(port->txq, port, 1) >= 0)
		buffs--;  

	i = 0;
	do {
		while (queue_get_desc(port->txreadyq, port, 1) >= 0)
			buffs--;
		if (!buffs)
			break;
	} while (++i < MAX_CLOSE_WAIT);

	if (buffs)
		netdev_crit(dev, "unable to drain TX queue, %i buffer(s) left in NPE\n",
			    buffs);
#if DEBUG_CLOSE
	if (!buffs)
		printk(KERN_DEBUG "Draining TX queues took %i cycles\n", i);
#endif
	qmgr_disable_irq(port->txdoneq);

	free_irq(gpiod_to_irq(port->dcd), dev);
	 
	gpiod_set_value(port->dtr, 0);
	gpiod_set_value(port->rts, 0);
	spin_unlock_irqrestore(&npe_lock, flags);

	destroy_hdlc_queues(port);
	release_hdlc_queues(port);
	hdlc_close(dev);
	return 0;
}

static int hss_hdlc_attach(struct net_device *dev, unsigned short encoding,
			   unsigned short parity)
{
	struct port *port = dev_to_port(dev);

	if (encoding != ENCODING_NRZ)
		return -EINVAL;

	switch (parity) {
	case PARITY_CRC16_PR1_CCITT:
		port->hdlc_cfg = 0;
		return 0;

	case PARITY_CRC32_PR1_CCITT:
		port->hdlc_cfg = PKT_HDLC_CRC_32;
		return 0;

	default:
		return -EINVAL;
	}
}

static u32 check_clock(u32 timer_freq, u32 rate, u32 a, u32 b, u32 c,
		       u32 *best, u32 *best_diff, u32 *reg)
{
	 
	u64 new_rate;
	u32 new_diff;

	new_rate = timer_freq * (u64)(c + 1);
	do_div(new_rate, a * (c + 1) + b + 1);
	new_diff = abs((u32)new_rate - rate);

	if (new_diff < *best_diff) {
		*best = new_rate;
		*best_diff = new_diff;
		*reg = (a << 22) | (b << 12) | c;
	}
	return new_diff;
}

static void find_best_clock(u32 timer_freq, u32 rate, u32 *best, u32 *reg)
{
	u32 a, b, diff = 0xFFFFFFFF;

	a = timer_freq / rate;

	if (a > 0x3FF) {  
		check_clock(timer_freq, rate, 0x3FF, 1, 1, best, &diff, reg);
		return;
	}
	if (a == 0) {  
		a = 1;  
		rate = timer_freq;
	}

	if (rate * a == timer_freq) {  
		check_clock(timer_freq, rate, a - 1, 1, 1, best, &diff, reg);
		return;
	}

	for (b = 0; b < 0x400; b++) {
		u64 c = (b + 1) * (u64)rate;

		do_div(c, timer_freq - rate * a);
		c--;
		if (c >= 0xFFF) {  
			if (b == 0 &&  
			    !check_clock(timer_freq, rate, a - 1, 1, 1, best,
					 &diff, reg))
				return;
			check_clock(timer_freq, rate, a, b, 0xFFF, best,
				    &diff, reg);
			return;
		}
		if (!check_clock(timer_freq, rate, a, b, c, best, &diff, reg))
			return;
		if (!check_clock(timer_freq, rate, a, b, c + 1, best, &diff,
				 reg))
			return;
	}
}

static int hss_hdlc_set_clock(struct port *port, unsigned int clock_type)
{
	switch (clock_type) {
	case CLOCK_DEFAULT:
	case CLOCK_EXT:
		gpiod_set_value(port->clk_internal, 0);
		return CLOCK_EXT;
	case CLOCK_INT:
		gpiod_set_value(port->clk_internal, 1);
		return CLOCK_INT;
	default:
		return -EINVAL;
	}
}

static int hss_hdlc_ioctl(struct net_device *dev, struct if_settings *ifs)
{
	const size_t size = sizeof(sync_serial_settings);
	sync_serial_settings new_line;
	sync_serial_settings __user *line = ifs->ifs_ifsu.sync;
	struct port *port = dev_to_port(dev);
	unsigned long flags;
	int clk;

	switch (ifs->type) {
	case IF_GET_IFACE:
		ifs->type = IF_IFACE_V35;
		if (ifs->size < size) {
			ifs->size = size;  
			return -ENOBUFS;
		}
		memset(&new_line, 0, sizeof(new_line));
		new_line.clock_type = port->clock_type;
		new_line.clock_rate = port->clock_rate;
		new_line.loopback = port->loopback;
		if (copy_to_user(line, &new_line, size))
			return -EFAULT;
		return 0;

	case IF_IFACE_SYNC_SERIAL:
	case IF_IFACE_V35:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		if (copy_from_user(&new_line, line, size))
			return -EFAULT;

		clk = new_line.clock_type;
		hss_hdlc_set_clock(port, clk);

		if (clk != CLOCK_EXT && clk != CLOCK_INT)
			return -EINVAL;	 

		if (new_line.loopback != 0 && new_line.loopback != 1)
			return -EINVAL;

		port->clock_type = clk;  
		if (clk == CLOCK_INT) {
			find_best_clock(IXP4XX_TIMER_FREQ,
					new_line.clock_rate,
					&port->clock_rate, &port->clock_reg);
		} else {
			port->clock_rate = 0;
			port->clock_reg = CLK42X_SPEED_2048KHZ;
		}
		port->loopback = new_line.loopback;

		spin_lock_irqsave(&npe_lock, flags);

		if (dev->flags & IFF_UP)
			hss_config(port);

		if (port->loopback || port->carrier)
			netif_carrier_on(port->netdev);
		else
			netif_carrier_off(port->netdev);
		spin_unlock_irqrestore(&npe_lock, flags);

		return 0;

	default:
		return hdlc_ioctl(dev, ifs);
	}
}

 

static const struct net_device_ops hss_hdlc_ops = {
	.ndo_open       = hss_hdlc_open,
	.ndo_stop       = hss_hdlc_close,
	.ndo_start_xmit = hdlc_start_xmit,
	.ndo_siocwandev = hss_hdlc_ioctl,
};

static int ixp4xx_hss_probe(struct platform_device *pdev)
{
	struct of_phandle_args queue_spec;
	struct of_phandle_args npe_spec;
	struct device *dev = &pdev->dev;
	struct net_device *ndev;
	struct device_node *np;
	struct regmap *rmap;
	struct port *port;
	hdlc_device *hdlc;
	int err;
	u32 val;

	 
	rmap = syscon_regmap_lookup_by_compatible("syscon");
	if (IS_ERR(rmap))
		return dev_err_probe(dev, PTR_ERR(rmap),
				     "failed to look up syscon\n");

	val = cpu_ixp4xx_features(rmap);

	if ((val & (IXP4XX_FEATURE_HDLC | IXP4XX_FEATURE_HSS)) !=
	    (IXP4XX_FEATURE_HDLC | IXP4XX_FEATURE_HSS)) {
		dev_err(dev, "HDLC and HSS feature unavailable in platform\n");
		return -ENODEV;
	}

	np = dev->of_node;

	port = devm_kzalloc(dev, sizeof(*port), GFP_KERNEL);
	if (!port)
		return -ENOMEM;

	err = of_parse_phandle_with_fixed_args(np, "intel,npe-handle", 1, 0,
					       &npe_spec);
	if (err)
		return dev_err_probe(dev, err, "no NPE engine specified\n");
	 
	port->npe = npe_request(npe_spec.args[0] << 4);
	if (!port->npe) {
		dev_err(dev, "unable to obtain NPE instance\n");
		return -ENODEV;
	}

	 
	err = of_parse_phandle_with_fixed_args(np, "intek,queue-chl-txready", 1, 0,
					       &queue_spec);
	if (err)
		return dev_err_probe(dev, err, "no txready queue phandle\n");
	port->txreadyq = queue_spec.args[0];
	 
	err = of_parse_phandle_with_fixed_args(np, "intek,queue-chl-rxtrig", 1, 0,
					       &queue_spec);
	if (err)
		return dev_err_probe(dev, err, "no rxtrig queue phandle\n");
	port->rxtrigq = queue_spec.args[0];
	 
	err = of_parse_phandle_with_fixed_args(np, "intek,queue-pkt-rx", 1, 0,
					       &queue_spec);
	if (err)
		return dev_err_probe(dev, err, "no RX queue phandle\n");
	port->rxq = queue_spec.args[0];
	 
	err = of_parse_phandle_with_fixed_args(np, "intek,queue-pkt-tx", 1, 0,
					       &queue_spec);
	if (err)
		return dev_err_probe(dev, err, "no RX queue phandle\n");
	port->txq = queue_spec.args[0];
	 
	err = of_parse_phandle_with_fixed_args(np, "intek,queue-pkt-rxfree", 1, 0,
					       &queue_spec);
	if (err)
		return dev_err_probe(dev, err, "no RX free queue phandle\n");
	port->rxfreeq = queue_spec.args[0];
	 
	err = of_parse_phandle_with_fixed_args(np, "intek,queue-pkt-txdone", 1, 0,
					       &queue_spec);
	if (err)
		return dev_err_probe(dev, err, "no TX done queue phandle\n");
	port->txdoneq = queue_spec.args[0];

	 
	port->cts = devm_gpiod_get(dev, "cts", GPIOD_OUT_LOW);
	if (IS_ERR(port->cts))
		return dev_err_probe(dev, PTR_ERR(port->cts), "unable to get CTS GPIO\n");
	port->rts = devm_gpiod_get(dev, "rts", GPIOD_OUT_LOW);
	if (IS_ERR(port->rts))
		return dev_err_probe(dev, PTR_ERR(port->rts), "unable to get RTS GPIO\n");
	port->dcd = devm_gpiod_get(dev, "dcd", GPIOD_IN);
	if (IS_ERR(port->dcd))
		return dev_err_probe(dev, PTR_ERR(port->dcd), "unable to get DCD GPIO\n");
	port->dtr = devm_gpiod_get(dev, "dtr", GPIOD_OUT_LOW);
	if (IS_ERR(port->dtr))
		return dev_err_probe(dev, PTR_ERR(port->dtr), "unable to get DTR GPIO\n");
	port->clk_internal = devm_gpiod_get(dev, "clk-internal", GPIOD_OUT_LOW);
	if (IS_ERR(port->clk_internal))
		return dev_err_probe(dev, PTR_ERR(port->clk_internal),
				     "unable to get CLK internal GPIO\n");

	ndev = alloc_hdlcdev(port);
	port->netdev = alloc_hdlcdev(port);
	if (!port->netdev) {
		err = -ENOMEM;
		goto err_plat;
	}

	SET_NETDEV_DEV(ndev, &pdev->dev);
	hdlc = dev_to_hdlc(ndev);
	hdlc->attach = hss_hdlc_attach;
	hdlc->xmit = hss_hdlc_xmit;
	ndev->netdev_ops = &hss_hdlc_ops;
	ndev->tx_queue_len = 100;
	port->clock_type = CLOCK_EXT;
	port->clock_rate = 0;
	port->clock_reg = CLK42X_SPEED_2048KHZ;
	port->id = pdev->id;
	port->dev = &pdev->dev;
	netif_napi_add_weight(ndev, &port->napi, hss_hdlc_poll, NAPI_WEIGHT);

	err = register_hdlc_device(ndev);
	if (err)
		goto err_free_netdev;

	platform_set_drvdata(pdev, port);

	netdev_info(ndev, "initialized\n");
	return 0;

err_free_netdev:
	free_netdev(ndev);
err_plat:
	npe_release(port->npe);
	return err;
}

static int ixp4xx_hss_remove(struct platform_device *pdev)
{
	struct port *port = platform_get_drvdata(pdev);

	unregister_hdlc_device(port->netdev);
	free_netdev(port->netdev);
	npe_release(port->npe);
	return 0;
}

static struct platform_driver ixp4xx_hss_driver = {
	.driver.name	= DRV_NAME,
	.probe		= ixp4xx_hss_probe,
	.remove		= ixp4xx_hss_remove,
};
module_platform_driver(ixp4xx_hss_driver);

MODULE_AUTHOR("Krzysztof Halasa");
MODULE_DESCRIPTION("Intel IXP4xx HSS driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:ixp4xx_hss");
