
 

#include <linux/device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/usb.h>
#include <linux/usb/input.h>
#include <linux/pm_wakeup.h>
#include <media/rc-core.h>

#define DRIVER_VERSION	"1.95"
#define DRIVER_AUTHOR	"Jarod Wilson <jarod@redhat.com>"
#define DRIVER_DESC	"Windows Media Center Ed. eHome Infrared Transceiver " \
			"device driver"
#define DRIVER_NAME	"mceusb"

#define USB_TX_TIMEOUT		1000  
#define USB_CTRL_MSG_SZ		2   
#define MCE_G1_INIT_MSGS	40  

 
#define MCE_IRBUF_SIZE		128   
#define MCE_TIME_UNIT		50    
#define MCE_PACKET_SIZE		31    
#define MCE_IRDATA_HEADER	(0x80 + MCE_PACKET_SIZE - 1)
				      
#define MCE_IRDATA_TRAILER	0x80  
#define MCE_MAX_CHANNELS	2     
#define MCE_DEFAULT_TX_MASK	0x03  
#define MCE_PULSE_BIT		0x80  
#define MCE_PULSE_MASK		0x7f  
#define MCE_MAX_PULSE_LENGTH	0x7f  

 
#define MCE_CMD			0x1f
#define MCE_PORT_IR		0x4	 
#define MCE_PORT_SYS		0x7	 
#define MCE_PORT_SER		0x6	 
#define MCE_PORT_MASK		0xe0	 

 
#define MCE_CMD_PORT_IR		0x9f	 
#define MCE_CMD_PORT_SYS	0xff	 

 
#define MCE_CMD_RESET		0xfe	 
#define MCE_CMD_RESUME		0xaa	 
#define MCE_CMD_SETIRCFS	0x06	 
#define MCE_CMD_SETIRTIMEOUT	0x0c	 
#define MCE_CMD_SETIRTXPORTS	0x08	 
#define MCE_CMD_SETIRRXPORTEN	0x14	 
#define MCE_CMD_FLASHLED	0x23	 

 
#define MCE_CMD_GETIRCFS	0x07	 
#define MCE_CMD_GETIRTIMEOUT	0x0d	 
#define MCE_CMD_GETIRTXPORTS	0x13	 
#define MCE_CMD_GETIRRXPORTEN	0x15	 
#define MCE_CMD_GETPORTSTATUS	0x11	 
#define MCE_CMD_GETIRNUMPORTS	0x16	 
#define MCE_CMD_GETWAKESOURCE	0x17	 
#define MCE_CMD_GETEMVER	0x22	 
#define MCE_CMD_GETDEVDETAILS	0x21	 
#define MCE_CMD_GETWAKESUPPORT	0x20	 
#define MCE_CMD_GETWAKEVERSION	0x18	 

 
#define MCE_CMD_NOP		0xff	 

 
#define MCE_RSP_EQIRCFS		0x06	 
#define MCE_RSP_EQIRTIMEOUT	0x0c	 
#define MCE_RSP_GETWAKESOURCE	0x17	 
#define MCE_RSP_EQIRTXPORTS	0x08	 
#define MCE_RSP_EQIRRXPORTEN	0x14	 
#define MCE_RSP_GETPORTSTATUS	0x11	 
#define MCE_RSP_EQIRRXCFCNT	0x15	 
#define MCE_RSP_EQIRNUMPORTS	0x16	 
#define MCE_RSP_EQWAKESUPPORT	0x20	 
#define MCE_RSP_EQWAKEVERSION	0x18	 
#define MCE_RSP_EQDEVDETAILS	0x21	 
#define MCE_RSP_EQEMVER		0x22	 
#define MCE_RSP_FLASHLED	0x23	 

 
#define MCE_RSP_CMD_ILLEGAL	0xfe	 
#define MCE_RSP_TX_TIMEOUT	0x81	 

 
#define MCE_CMD_SIG_END		0x01	 
#define MCE_CMD_PING		0x03	 
#define MCE_CMD_UNKNOWN		0x04	 
#define MCE_CMD_UNKNOWN2	0x05	 
#define MCE_CMD_UNKNOWN3	0x09	 
#define MCE_CMD_UNKNOWN4	0x0a	 
#define MCE_CMD_G_REVISION	0x0b	 
#define MCE_CMD_UNKNOWN5	0x0e	 
#define MCE_CMD_UNKNOWN6	0x0f	 
#define MCE_CMD_UNKNOWN8	0x19	 
#define MCE_CMD_UNKNOWN9	0x1b	 
#define MCE_CMD_NULL		0x00	 

 
#define MCE_COMMAND_IRDATA	0x80
#define MCE_PACKET_LENGTH_MASK	0x1f  

#define VENDOR_PHILIPS		0x0471
#define VENDOR_SMK		0x0609
#define VENDOR_TATUNG		0x1460
#define VENDOR_GATEWAY		0x107b
#define VENDOR_SHUTTLE		0x1308
#define VENDOR_SHUTTLE2		0x051c
#define VENDOR_MITSUMI		0x03ee
#define VENDOR_TOPSEED		0x1784
#define VENDOR_RICAVISION	0x179d
#define VENDOR_ITRON		0x195d
#define VENDOR_FIC		0x1509
#define VENDOR_LG		0x043e
#define VENDOR_MICROSOFT	0x045e
#define VENDOR_FORMOSA		0x147a
#define VENDOR_FINTEK		0x1934
#define VENDOR_PINNACLE		0x2304
#define VENDOR_ECS		0x1019
#define VENDOR_WISTRON		0x0fb8
#define VENDOR_COMPRO		0x185b
#define VENDOR_NORTHSTAR	0x04eb
#define VENDOR_REALTEK		0x0bda
#define VENDOR_TIVO		0x105a
#define VENDOR_CONEXANT		0x0572
#define VENDOR_TWISTEDMELON	0x2596
#define VENDOR_HAUPPAUGE	0x2040
#define VENDOR_PCTV		0x2013
#define VENDOR_ADAPTEC		0x03f3

enum mceusb_model_type {
	MCE_GEN2 = 0,		 
	MCE_GEN1,
	MCE_GEN3,
	MCE_GEN3_BROKEN_IRTIMEOUT,
	MCE_GEN2_TX_INV,
	MCE_GEN2_TX_INV_RX_GOOD,
	POLARIS_EVK,
	CX_HYBRID_TV,
	MULTIFUNCTION,
	TIVO_KIT,
	MCE_GEN2_NO_TX,
	HAUPPAUGE_CX_HYBRID_TV,
	EVROMEDIA_FULL_HYBRID_FULLHD,
	ASTROMETA_T2HYBRID,
};

struct mceusb_model {
	u32 mce_gen1:1;
	u32 mce_gen2:1;
	u32 mce_gen3:1;
	u32 tx_mask_normal:1;
	u32 no_tx:1;
	u32 broken_irtimeout:1;
	 
	u32 rx2;

	int ir_intfnum;

	const char *rc_map;	 
	const char *name;	 
};

static const struct mceusb_model mceusb_model[] = {
	[MCE_GEN1] = {
		.mce_gen1 = 1,
		.tx_mask_normal = 1,
		.rx2 = 2,
	},
	[MCE_GEN2] = {
		.mce_gen2 = 1,
		.rx2 = 2,
	},
	[MCE_GEN2_NO_TX] = {
		.mce_gen2 = 1,
		.no_tx = 1,
	},
	[MCE_GEN2_TX_INV] = {
		.mce_gen2 = 1,
		.tx_mask_normal = 1,
		.rx2 = 1,
	},
	[MCE_GEN2_TX_INV_RX_GOOD] = {
		.mce_gen2 = 1,
		.tx_mask_normal = 1,
		.rx2 = 2,
	},
	[MCE_GEN3] = {
		.mce_gen3 = 1,
		.tx_mask_normal = 1,
		.rx2 = 2,
	},
	[MCE_GEN3_BROKEN_IRTIMEOUT] = {
		.mce_gen3 = 1,
		.tx_mask_normal = 1,
		.rx2 = 2,
		.broken_irtimeout = 1
	},
	[POLARIS_EVK] = {
		 
		.name = "Conexant Hybrid TV (cx231xx) MCE IR",
		.rx2 = 2,
	},
	[CX_HYBRID_TV] = {
		.no_tx = 1,  
		.name = "Conexant Hybrid TV (cx231xx) MCE IR",
	},
	[HAUPPAUGE_CX_HYBRID_TV] = {
		.no_tx = 1,  
		.name = "Conexant Hybrid TV (cx231xx) MCE IR no TX",
	},
	[MULTIFUNCTION] = {
		.mce_gen2 = 1,
		.ir_intfnum = 2,
		.rx2 = 2,
	},
	[TIVO_KIT] = {
		.mce_gen2 = 1,
		.rc_map = RC_MAP_TIVO,
		.rx2 = 2,
	},
	[EVROMEDIA_FULL_HYBRID_FULLHD] = {
		.name = "Evromedia USB Full Hybrid Full HD",
		.no_tx = 1,
		.rc_map = RC_MAP_MSI_DIGIVOX_III,
	},
	[ASTROMETA_T2HYBRID] = {
		.name = "Astrometa T2Hybrid",
		.no_tx = 1,
		.rc_map = RC_MAP_ASTROMETA_T2HYBRID,
	}
};

static const struct usb_device_id mceusb_dev_table[] = {
	 
	{ USB_DEVICE(VENDOR_MICROSOFT, 0x006d),
	  .driver_info = MCE_GEN1 },
	 
	{ USB_DEVICE(VENDOR_PHILIPS, 0x0608) },
	 
	{ USB_DEVICE(VENDOR_PHILIPS, 0x060c),
	  .driver_info = MCE_GEN2_TX_INV },
	 
	{ USB_DEVICE(VENDOR_PHILIPS, 0x060d) },
	 
	{ USB_DEVICE(VENDOR_PHILIPS, 0x060f) },
	 
	{ USB_DEVICE(VENDOR_PHILIPS, 0x0613) },
	 
	{ USB_DEVICE(VENDOR_PHILIPS, 0x0815) },
	 
	{ USB_DEVICE(VENDOR_PHILIPS, 0x206c) },
	 
	{ USB_DEVICE(VENDOR_PHILIPS, 0x2088) },
	 
	{ USB_DEVICE(VENDOR_PHILIPS, 0x2093),
	  .driver_info = MCE_GEN2_TX_INV },
	 
	{ USB_DEVICE(VENDOR_REALTEK, 0x0161),
	  .driver_info = MULTIFUNCTION },
	 
	{ USB_DEVICE(VENDOR_SMK, 0x031d),
	  .driver_info = MCE_GEN2_TX_INV_RX_GOOD },
	 
	{ USB_DEVICE(VENDOR_SMK, 0x0322),
	  .driver_info = MCE_GEN2_TX_INV },
	 
	{ USB_DEVICE(VENDOR_SMK, 0x0334),
	  .driver_info = MCE_GEN2_TX_INV },
	 
	{ USB_DEVICE(VENDOR_SMK, 0x0338) },
	 
	{ USB_DEVICE(VENDOR_SMK, 0x0353),
	  .driver_info = MCE_GEN2_NO_TX },
	 
	{ USB_DEVICE(VENDOR_SMK, 0x0357),
	  .driver_info = MCE_GEN2_NO_TX },
	 
	{ USB_DEVICE(VENDOR_TATUNG, 0x9150) },
	 
	{ USB_DEVICE(VENDOR_SHUTTLE, 0xc001) },
	 
	{ USB_DEVICE(VENDOR_SHUTTLE2, 0xc001) },
	 
	{ USB_DEVICE(VENDOR_GATEWAY, 0x3009) },
	 
	{ USB_DEVICE(VENDOR_MITSUMI, 0x2501) },
	 
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0001),
	  .driver_info = MCE_GEN2_TX_INV },
	 
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0006),
	  .driver_info = MCE_GEN2_TX_INV },
	 
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0007),
	  .driver_info = MCE_GEN2_TX_INV },
	 
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0008),
	  .driver_info = MCE_GEN3 },
	 
	{ USB_DEVICE(VENDOR_TOPSEED, 0x000a),
	  .driver_info = MCE_GEN2_TX_INV },
	 
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0011),
	  .driver_info = MCE_GEN3_BROKEN_IRTIMEOUT },
	 
	{ USB_DEVICE(VENDOR_RICAVISION, 0x0010) },
	 
	{ USB_DEVICE(VENDOR_ITRON, 0x7002) },
	 
	{ USB_DEVICE(VENDOR_FIC, 0x9242) },
	 
	{ USB_DEVICE(VENDOR_LG, 0x9803) },
	 
	{ USB_DEVICE(VENDOR_MICROSOFT, 0x00a0) },
	 
	{ USB_DEVICE(VENDOR_FORMOSA, 0xe015) },
	 
	{ USB_DEVICE(VENDOR_FORMOSA, 0xe016) },
	 
	{ USB_DEVICE(VENDOR_FORMOSA, 0xe017),
	  .driver_info = MCE_GEN2_NO_TX },
	 
	{ USB_DEVICE(VENDOR_FORMOSA, 0xe018) },
	 
	{ USB_DEVICE(VENDOR_FORMOSA, 0xe03a) },
	 
	{ USB_DEVICE(VENDOR_FORMOSA, 0xe03c) },
	 
	{ USB_DEVICE(VENDOR_FORMOSA, 0xe03e) },
	 
	{ USB_DEVICE(VENDOR_FORMOSA, 0xe042) },
	 
	{ USB_DEVICE(VENDOR_FINTEK, 0x5168),
	  .driver_info = MCE_GEN2_TX_INV },
	 
	{ USB_DEVICE(VENDOR_FINTEK, 0x0602) },
	 
	{ USB_DEVICE(VENDOR_FINTEK, 0x0702) },
	 
	{ USB_DEVICE(VENDOR_PINNACLE, 0x0225),
	  .driver_info = MCE_GEN3 },
	 
	{ USB_DEVICE(VENDOR_ECS, 0x0f38) },
	 
	{ USB_DEVICE(VENDOR_WISTRON, 0x0002) },
	 
	{ USB_DEVICE(VENDOR_COMPRO, 0x3020) },
	 
	{ USB_DEVICE(VENDOR_COMPRO, 0x3082) },
	 
	{ USB_DEVICE(VENDOR_NORTHSTAR, 0xe004) },
	 
	{ USB_DEVICE(VENDOR_TIVO, 0x2000),
	  .driver_info = TIVO_KIT },
	 
	{ USB_DEVICE(VENDOR_CONEXANT, 0x58a1),
	  .driver_info = POLARIS_EVK },
	 
	{ USB_DEVICE(VENDOR_CONEXANT, 0x58a5),
	  .driver_info = CX_HYBRID_TV },
	 
	{ USB_DEVICE(VENDOR_TWISTEDMELON, 0x8008) },
	 
	{ USB_DEVICE(VENDOR_TWISTEDMELON, 0x8016) },
	 
	{ USB_DEVICE(VENDOR_TWISTEDMELON, 0x8042) },
	 
	{ USB_DEVICE(VENDOR_HAUPPAUGE, 0xb130),
	  .driver_info = HAUPPAUGE_CX_HYBRID_TV },
	{ USB_DEVICE(VENDOR_HAUPPAUGE, 0xb131),
	  .driver_info = HAUPPAUGE_CX_HYBRID_TV },
	{ USB_DEVICE(VENDOR_HAUPPAUGE, 0xb138),
	  .driver_info = HAUPPAUGE_CX_HYBRID_TV },
	{ USB_DEVICE(VENDOR_HAUPPAUGE, 0xb139),
	  .driver_info = HAUPPAUGE_CX_HYBRID_TV },
	 
	{ USB_DEVICE(VENDOR_HAUPPAUGE, 0xb151),
	  .driver_info = HAUPPAUGE_CX_HYBRID_TV },
	 
	{ USB_DEVICE(VENDOR_HAUPPAUGE, 0xb123),
	  .driver_info = HAUPPAUGE_CX_HYBRID_TV },
	 
	{ USB_DEVICE(VENDOR_HAUPPAUGE, 0xb150),
	  .driver_info = HAUPPAUGE_CX_HYBRID_TV },
	{ USB_DEVICE(VENDOR_PCTV, 0x0259),
	  .driver_info = HAUPPAUGE_CX_HYBRID_TV },
	{ USB_DEVICE(VENDOR_PCTV, 0x025e),
	  .driver_info = HAUPPAUGE_CX_HYBRID_TV },
	 
	{ USB_DEVICE(VENDOR_ADAPTEC, 0x0094) },
	 
	{ USB_DEVICE(0x1b80, 0xd3b2),
	  .driver_info = EVROMEDIA_FULL_HYBRID_FULLHD },
	 
	{ USB_DEVICE(0x15f4, 0x0135),
	  .driver_info = ASTROMETA_T2HYBRID },

	 
	{ }
};

 
struct mceusb_dev {
	 
	struct rc_dev *rc;

	 
	bool carrier_report_enabled;
	bool wideband_rx_enabled;	 

	 
	struct device *dev;

	 
	struct usb_device *usbdev;
	struct usb_interface *usbintf;
	struct urb *urb_in;
	unsigned int pipe_in;
	struct usb_endpoint_descriptor *usb_ep_out;
	unsigned int pipe_out;

	 
	unsigned char *buf_in;
	unsigned int len_in;
	dma_addr_t dma_in;

	enum {
		CMD_HEADER = 0,
		SUBCMD,
		CMD_DATA,
		PARSE_IRDATA,
	} parser_state;

	u8 cmd, rem;		 

	struct {
		u32 connected:1;
		u32 tx_mask_normal:1;
		u32 microsoft_gen1:1;
		u32 no_tx:1;
		u32 rx2;
	} flags;

	 
	u32 carrier;
	unsigned char tx_mask;

	char name[128];
	char phys[64];
	enum mceusb_model_type model;

	bool need_reset;	 
	u8 emver;		 
	u8 num_txports;		 
	u8 num_rxports;		 
	u8 txports_cabled;	 
	u8 rxports_active;	 
	bool learning_active;	 

	 
	u32 pulse_tunit;	 
	u32 pulse_count;	 

	 
	struct work_struct kevent;
	unsigned long kevent_flags;
#		define EVENT_TX_HALT	0
#		define EVENT_RX_HALT	1
#		define EVENT_RST_PEND	31
};

 
static char DEVICE_RESUME[]	= {MCE_CMD_NULL, MCE_CMD_PORT_SYS,
				   MCE_CMD_RESUME};
static char GET_REVISION[]	= {MCE_CMD_PORT_SYS, MCE_CMD_G_REVISION};
static char GET_EMVER[]		= {MCE_CMD_PORT_SYS, MCE_CMD_GETEMVER};
static char GET_WAKEVERSION[]	= {MCE_CMD_PORT_SYS, MCE_CMD_GETWAKEVERSION};
static char FLASH_LED[]		= {MCE_CMD_PORT_SYS, MCE_CMD_FLASHLED};
static char GET_UNKNOWN2[]	= {MCE_CMD_PORT_IR, MCE_CMD_UNKNOWN2};
static char GET_CARRIER_FREQ[]	= {MCE_CMD_PORT_IR, MCE_CMD_GETIRCFS};
static char GET_RX_TIMEOUT[]	= {MCE_CMD_PORT_IR, MCE_CMD_GETIRTIMEOUT};
static char GET_NUM_PORTS[]	= {MCE_CMD_PORT_IR, MCE_CMD_GETIRNUMPORTS};
static char GET_TX_BITMASK[]	= {MCE_CMD_PORT_IR, MCE_CMD_GETIRTXPORTS};
static char GET_RX_SENSOR[]	= {MCE_CMD_PORT_IR, MCE_CMD_GETIRRXPORTEN};
 
 

static int mceusb_cmd_datasize(u8 cmd, u8 subcmd)
{
	int datasize = 0;

	switch (cmd) {
	case MCE_CMD_NULL:
		if (subcmd == MCE_CMD_PORT_SYS)
			datasize = 1;
		break;
	case MCE_CMD_PORT_SYS:
		switch (subcmd) {
		case MCE_RSP_GETPORTSTATUS:
			datasize = 5;
			break;
		case MCE_RSP_EQWAKEVERSION:
			datasize = 4;
			break;
		case MCE_CMD_G_REVISION:
			datasize = 4;
			break;
		case MCE_RSP_EQWAKESUPPORT:
		case MCE_RSP_GETWAKESOURCE:
		case MCE_RSP_EQDEVDETAILS:
		case MCE_RSP_EQEMVER:
			datasize = 1;
			break;
		}
		break;
	case MCE_CMD_PORT_IR:
		switch (subcmd) {
		case MCE_CMD_UNKNOWN:
		case MCE_RSP_EQIRCFS:
		case MCE_RSP_EQIRTIMEOUT:
		case MCE_RSP_EQIRRXCFCNT:
		case MCE_RSP_EQIRNUMPORTS:
			datasize = 2;
			break;
		case MCE_CMD_SIG_END:
		case MCE_RSP_EQIRTXPORTS:
		case MCE_RSP_EQIRRXPORTEN:
			datasize = 1;
			break;
		}
	}
	return datasize;
}

static void mceusb_dev_printdata(struct mceusb_dev *ir, u8 *buf, int buf_len,
				 int offset, int len, bool out)
{
#if defined(DEBUG) || defined(CONFIG_DYNAMIC_DEBUG)
	char *inout;
	u8 cmd, subcmd, *data;
	struct device *dev = ir->dev;
	u32 carrier, period;

	if (offset < 0 || offset >= buf_len)
		return;

	dev_dbg(dev, "%cx data[%d]: %*ph (len=%d sz=%d)",
		(out ? 't' : 'r'), offset,
		min(len, buf_len - offset), buf + offset, len, buf_len);

	inout = out ? "Request" : "Got";

	cmd    = buf[offset];
	subcmd = (offset + 1 < buf_len) ? buf[offset + 1] : 0;
	data   = &buf[offset] + 2;

	 
	if (ir->flags.microsoft_gen1 && !out && !offset) {
		dev_dbg(dev, "MCE gen 1 header");
		return;
	}

	 
	if (cmd != MCE_CMD_PORT_IR &&
	    (cmd & MCE_PORT_MASK) == MCE_COMMAND_IRDATA) {
		if (cmd == MCE_IRDATA_TRAILER)
			dev_dbg(dev, "End of raw IR data");
		else
			dev_dbg(dev, "Raw IR data, %d pulse/space samples",
				cmd & MCE_PACKET_LENGTH_MASK);
		return;
	}

	 
	if (offset + len > buf_len)
		return;

	 
	switch (cmd) {
	case MCE_CMD_NULL:
		if (subcmd == MCE_CMD_NULL)
			break;
		if ((subcmd == MCE_CMD_PORT_SYS) &&
		    (data[0] == MCE_CMD_RESUME))
			dev_dbg(dev, "Device resume requested");
		else
			dev_dbg(dev, "Unknown command 0x%02x 0x%02x",
				 cmd, subcmd);
		break;
	case MCE_CMD_PORT_SYS:
		switch (subcmd) {
		case MCE_RSP_EQEMVER:
			if (!out)
				dev_dbg(dev, "Emulator interface version %x",
					 data[0]);
			break;
		case MCE_CMD_G_REVISION:
			if (len == 2)
				dev_dbg(dev, "Get hw/sw rev?");
			else
				dev_dbg(dev, "hw/sw rev %*ph",
					4, &buf[offset + 2]);
			break;
		case MCE_CMD_RESUME:
			dev_dbg(dev, "Device resume requested");
			break;
		case MCE_RSP_CMD_ILLEGAL:
			dev_dbg(dev, "Illegal PORT_SYS command");
			break;
		case MCE_RSP_EQWAKEVERSION:
			if (!out)
				dev_dbg(dev, "Wake version, proto: 0x%02x, payload: 0x%02x, address: 0x%02x, version: 0x%02x",
					data[0], data[1], data[2], data[3]);
			break;
		case MCE_RSP_GETPORTSTATUS:
			if (!out)
				 
				dev_dbg(dev, "TX port %d: blaster is%s connected",
					 data[0] + 1, data[3] ? " not" : "");
			break;
		case MCE_CMD_FLASHLED:
			dev_dbg(dev, "Attempting to flash LED");
			break;
		default:
			dev_dbg(dev, "Unknown command 0x%02x 0x%02x",
				 cmd, subcmd);
			break;
		}
		break;
	case MCE_CMD_PORT_IR:
		switch (subcmd) {
		case MCE_CMD_SIG_END:
			dev_dbg(dev, "End of signal");
			break;
		case MCE_CMD_PING:
			dev_dbg(dev, "Ping");
			break;
		case MCE_CMD_UNKNOWN:
			dev_dbg(dev, "Resp to 9f 05 of 0x%02x 0x%02x",
				data[0], data[1]);
			break;
		case MCE_RSP_EQIRCFS:
			if (!data[0] && !data[1]) {
				dev_dbg(dev, "%s: no carrier", inout);
				break;
			}
			
			if (data[0] > 8)
				break;
			period = DIV_ROUND_CLOSEST((1U << data[0] * 2) *
						   (data[1] + 1), 10);
			if (!period)
				break;
			carrier = USEC_PER_SEC / period;
			dev_dbg(dev, "%s carrier of %u Hz (period %uus)",
				 inout, carrier, period);
			break;
		case MCE_CMD_GETIRCFS:
			dev_dbg(dev, "Get carrier mode and freq");
			break;
		case MCE_RSP_EQIRTXPORTS:
			dev_dbg(dev, "%s transmit blaster mask of 0x%02x",
				 inout, data[0]);
			break;
		case MCE_RSP_EQIRTIMEOUT:
			 
			period = ((data[0] << 8) | data[1]) *
				  MCE_TIME_UNIT / 1000;
			dev_dbg(dev, "%s receive timeout of %d ms",
				 inout, period);
			break;
		case MCE_CMD_GETIRTIMEOUT:
			dev_dbg(dev, "Get receive timeout");
			break;
		case MCE_CMD_GETIRTXPORTS:
			dev_dbg(dev, "Get transmit blaster mask");
			break;
		case MCE_RSP_EQIRRXPORTEN:
			dev_dbg(dev, "%s %s-range receive sensor in use",
				 inout, data[0] == 0x02 ? "short" : "long");
			break;
		case MCE_CMD_GETIRRXPORTEN:
		 
			if (out)
				dev_dbg(dev, "Get receive sensor");
			else
				dev_dbg(dev, "RX carrier cycle count: %d",
					((data[0] << 8) | data[1]));
			break;
		case MCE_RSP_EQIRNUMPORTS:
			if (out)
				break;
			dev_dbg(dev, "Num TX ports: %x, num RX ports: %x",
				data[0], data[1]);
			break;
		case MCE_RSP_CMD_ILLEGAL:
			dev_dbg(dev, "Illegal PORT_IR command");
			break;
		case MCE_RSP_TX_TIMEOUT:
			dev_dbg(dev, "IR TX timeout (TX buffer underrun)");
			break;
		default:
			dev_dbg(dev, "Unknown command 0x%02x 0x%02x",
				 cmd, subcmd);
			break;
		}
		break;
	default:
		break;
	}
#endif
}

 
static void mceusb_defer_kevent(struct mceusb_dev *ir, int kevent)
{
	set_bit(kevent, &ir->kevent_flags);

	if (test_bit(EVENT_RST_PEND, &ir->kevent_flags)) {
		dev_dbg(ir->dev, "kevent %d dropped pending USB Reset Device",
			kevent);
		return;
	}

	if (!schedule_work(&ir->kevent))
		dev_dbg(ir->dev, "kevent %d already scheduled", kevent);
	else
		dev_dbg(ir->dev, "kevent %d scheduled", kevent);
}

static void mce_write_callback(struct urb *urb)
{
	if (!urb)
		return;

	complete(urb->context);
}

 
static int mce_write(struct mceusb_dev *ir, u8 *data, int size)
{
	int ret;
	struct urb *urb;
	struct device *dev = ir->dev;
	unsigned char *buf_out;
	struct completion tx_done;
	unsigned long expire;
	unsigned long ret_wait;

	mceusb_dev_printdata(ir, data, size, 0, size, true);

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (unlikely(!urb)) {
		dev_err(dev, "Error: mce write couldn't allocate urb");
		return -ENOMEM;
	}

	buf_out = kmalloc(size, GFP_KERNEL);
	if (!buf_out) {
		usb_free_urb(urb);
		return -ENOMEM;
	}

	init_completion(&tx_done);

	 
	if (usb_endpoint_xfer_int(ir->usb_ep_out))
		usb_fill_int_urb(urb, ir->usbdev, ir->pipe_out,
				 buf_out, size, mce_write_callback, &tx_done,
				 ir->usb_ep_out->bInterval);
	else
		usb_fill_bulk_urb(urb, ir->usbdev, ir->pipe_out,
				  buf_out, size, mce_write_callback, &tx_done);
	memcpy(buf_out, data, size);

	ret = usb_submit_urb(urb, GFP_KERNEL);
	if (ret) {
		dev_err(dev, "Error: mce write submit urb error = %d", ret);
		kfree(buf_out);
		usb_free_urb(urb);
		return ret;
	}

	expire = msecs_to_jiffies(USB_TX_TIMEOUT);
	ret_wait = wait_for_completion_timeout(&tx_done, expire);
	if (!ret_wait) {
		dev_err(dev, "Error: mce write timed out (expire = %lu (%dms))",
			expire, USB_TX_TIMEOUT);
		usb_kill_urb(urb);
		ret = (urb->status == -ENOENT ? -ETIMEDOUT : urb->status);
	} else {
		ret = urb->status;
	}
	if (ret >= 0)
		ret = urb->actual_length;	 

	switch (urb->status) {
	 
	case 0:
		break;

	case -ECONNRESET:
	case -ENOENT:
	case -EILSEQ:
	case -ESHUTDOWN:
		break;

	case -EPIPE:
		dev_err(ir->dev, "Error: mce write urb status = %d (TX HALT)",
			urb->status);
		mceusb_defer_kevent(ir, EVENT_TX_HALT);
		break;

	default:
		dev_err(ir->dev, "Error: mce write urb status = %d",
			urb->status);
		break;
	}

	dev_dbg(dev, "tx done status = %d (wait = %lu, expire = %lu (%dms), urb->actual_length = %d, urb->status = %d)",
		ret, ret_wait, expire, USB_TX_TIMEOUT,
		urb->actual_length, urb->status);

	kfree(buf_out);
	usb_free_urb(urb);

	return ret;
}

static void mce_command_out(struct mceusb_dev *ir, u8 *data, int size)
{
	int rsize = sizeof(DEVICE_RESUME);

	if (ir->need_reset) {
		ir->need_reset = false;
		mce_write(ir, DEVICE_RESUME, rsize);
		msleep(10);
	}

	mce_write(ir, data, size);
	msleep(10);
}

 
static int mceusb_tx_ir(struct rc_dev *dev, unsigned *txbuf, unsigned count)
{
	struct mceusb_dev *ir = dev->priv;
	u8 cmdbuf[3] = { MCE_CMD_PORT_IR, MCE_CMD_SETIRTXPORTS, 0x00 };
	u8 irbuf[MCE_IRBUF_SIZE];
	int ircount = 0;
	unsigned int irsample;
	int i, length, ret;

	 
	cmdbuf[2] = ir->tx_mask;
	mce_command_out(ir, cmdbuf, sizeof(cmdbuf));

	 
	for (i = 0; i < count; i++) {
		irsample = txbuf[i] / MCE_TIME_UNIT;

		 
		while (irsample > 0) {
			 
			if (ircount % MCE_PACKET_SIZE == 0) {
				 
				if (ircount >= MCE_IRBUF_SIZE - 1) {
					 
					ret = mce_write(ir, irbuf, ircount);
					if (ret < 0)
						return ret;
					ircount = 0;
				}
				irbuf[ircount++] = MCE_IRDATA_HEADER;
			}

			 
			if (irsample <= MCE_MAX_PULSE_LENGTH) {
				irbuf[ircount] = irsample;
				irsample = 0;
			} else {
				irbuf[ircount] = MCE_MAX_PULSE_LENGTH;
				irsample -= MCE_MAX_PULSE_LENGTH;
			}
			 
			irbuf[ircount] |= (i & 1 ? 0 : MCE_PULSE_BIT);
			ircount++;

			 
			if (ircount >= MCE_IRBUF_SIZE) {
				 
				length = ircount % MCE_PACKET_SIZE;
				if (length > 0)
					irbuf[ircount - length] -=
						MCE_PACKET_SIZE - length;
				 
				ret = mce_write(ir, irbuf, ircount);
				if (ret < 0)
					return ret;
				ircount = 0;
			}
		}
	}  

	 
	length = ircount % MCE_PACKET_SIZE;
	if (length > 0)
		irbuf[ircount - length] -= MCE_PACKET_SIZE - length;

	 
	irbuf[ircount++] = MCE_IRDATA_TRAILER;

	 
	ret = mce_write(ir, irbuf, ircount);
	if (ret < 0)
		return ret;

	return count;
}

 
static int mceusb_set_tx_mask(struct rc_dev *dev, u32 mask)
{
	struct mceusb_dev *ir = dev->priv;

	 
	int emitters = ir->num_txports ? ir->num_txports : 2;

	if (mask >= (1 << emitters))
		return emitters;

	if (ir->flags.tx_mask_normal)
		ir->tx_mask = mask;
	else
		ir->tx_mask = (mask != MCE_DEFAULT_TX_MASK ?
				mask ^ MCE_DEFAULT_TX_MASK : mask) << 1;

	return 0;
}

 
static int mceusb_set_tx_carrier(struct rc_dev *dev, u32 carrier)
{
	struct mceusb_dev *ir = dev->priv;
	int clk = 10000000;
	int prescaler = 0, divisor = 0;
	unsigned char cmdbuf[4] = { MCE_CMD_PORT_IR,
				    MCE_CMD_SETIRCFS, 0x00, 0x00 };

	 
	if (ir->carrier != carrier) {

		if (carrier == 0) {
			ir->carrier = carrier;
			cmdbuf[2] = MCE_CMD_SIG_END;
			cmdbuf[3] = MCE_IRDATA_TRAILER;
			dev_dbg(ir->dev, "disabling carrier modulation");
			mce_command_out(ir, cmdbuf, sizeof(cmdbuf));
			return 0;
		}

		for (prescaler = 0; prescaler < 4; ++prescaler) {
			divisor = (clk >> (2 * prescaler)) / carrier;
			if (divisor <= 0xff) {
				ir->carrier = carrier;
				cmdbuf[2] = prescaler;
				cmdbuf[3] = divisor;
				dev_dbg(ir->dev, "requesting %u HZ carrier",
								carrier);

				 
				mce_command_out(ir, cmdbuf, sizeof(cmdbuf));
				return 0;
			}
		}

		return -EINVAL;

	}

	return 0;
}

static int mceusb_set_timeout(struct rc_dev *dev, unsigned int timeout)
{
	u8 cmdbuf[4] = { MCE_CMD_PORT_IR, MCE_CMD_SETIRTIMEOUT, 0, 0 };
	struct mceusb_dev *ir = dev->priv;
	unsigned int units;

	units = DIV_ROUND_UP(timeout, MCE_TIME_UNIT);

	cmdbuf[2] = units >> 8;
	cmdbuf[3] = units;

	mce_command_out(ir, cmdbuf, sizeof(cmdbuf));

	 
	mce_command_out(ir, GET_RX_TIMEOUT, sizeof(GET_RX_TIMEOUT));

	return 0;
}

 
static int mceusb_set_rx_wideband(struct rc_dev *dev, int enable)
{
	struct mceusb_dev *ir = dev->priv;
	unsigned char cmdbuf[3] = { MCE_CMD_PORT_IR,
				    MCE_CMD_SETIRRXPORTEN, 0x00 };

	dev_dbg(ir->dev, "select %s-range receive sensor",
		enable ? "short" : "long");
	if (enable) {
		ir->wideband_rx_enabled = true;
		cmdbuf[2] = 2;	 
	} else {
		ir->wideband_rx_enabled = false;
		cmdbuf[2] = 1;	 
	}
	mce_command_out(ir, cmdbuf, sizeof(cmdbuf));
	 

	return 0;
}

 
static int mceusb_set_rx_carrier_report(struct rc_dev *dev, int enable)
{
	struct mceusb_dev *ir = dev->priv;
	unsigned char cmdbuf[3] = { MCE_CMD_PORT_IR,
				    MCE_CMD_SETIRRXPORTEN, 0x00 };

	dev_dbg(ir->dev, "%s short-range receiver carrier reporting",
		enable ? "enable" : "disable");
	if (enable) {
		ir->carrier_report_enabled = true;
		if (!ir->learning_active) {
			cmdbuf[2] = 2;	 
			mce_command_out(ir, cmdbuf, sizeof(cmdbuf));
		}
	} else {
		ir->carrier_report_enabled = false;
		 
		if (ir->learning_active && !ir->wideband_rx_enabled) {
			cmdbuf[2] = 1;	 
			mce_command_out(ir, cmdbuf, sizeof(cmdbuf));
		}
	}

	return 0;
}

 
static void mceusb_handle_command(struct mceusb_dev *ir, u8 *buf_in)
{
	u8 cmd = buf_in[0];
	u8 subcmd = buf_in[1];
	u8 *hi = &buf_in[2];		 
	u8 *lo = &buf_in[3];		 
	struct ir_raw_event rawir = {};
	u32 carrier_cycles;
	u32 cycles_fix;

	if (cmd == MCE_CMD_PORT_SYS) {
		switch (subcmd) {
		 
		case MCE_RSP_GETPORTSTATUS:
			if (buf_in[5] == 0 && *hi < 8)
				ir->txports_cabled |= 1 << *hi;
			break;

		 
		case MCE_RSP_EQEMVER:
			ir->emver = *hi;
			break;

		 
		case MCE_RSP_CMD_ILLEGAL:
			ir->need_reset = true;
			break;

		default:
			break;
		}

		return;
	}

	if (cmd != MCE_CMD_PORT_IR)
		return;

	switch (subcmd) {
	 
	case MCE_RSP_EQIRTIMEOUT:
		ir->rc->timeout = (*hi << 8 | *lo) * MCE_TIME_UNIT;
		break;
	case MCE_RSP_EQIRNUMPORTS:
		ir->num_txports = *hi;
		ir->num_rxports = *lo;
		break;
	case MCE_RSP_EQIRRXCFCNT:
		 
		if (ir->carrier_report_enabled && ir->learning_active &&
		    ir->pulse_tunit > 0) {
			carrier_cycles = (*hi << 8 | *lo);
			 
			cycles_fix = ir->flags.rx2 == 2 ? ir->pulse_count : 0;
			rawir.carrier_report = 1;
			rawir.carrier = (1000000u / MCE_TIME_UNIT) *
					(carrier_cycles + cycles_fix) /
					ir->pulse_tunit;
			dev_dbg(ir->dev, "RX carrier frequency %u Hz (pulse count = %u, cycles = %u, duration = %u, rx2 = %u)",
				rawir.carrier, ir->pulse_count, carrier_cycles,
				ir->pulse_tunit, ir->flags.rx2);
			ir_raw_event_store(ir->rc, &rawir);
		}
		break;

	 
	case MCE_RSP_EQIRTXPORTS:
		ir->tx_mask = *hi;
		break;
	case MCE_RSP_EQIRRXPORTEN:
		ir->learning_active = ((*hi & 0x02) == 0x02);
		if (ir->rxports_active != *hi) {
			dev_info(ir->dev, "%s-range (0x%x) receiver active",
				 ir->learning_active ? "short" : "long", *hi);
			ir->rxports_active = *hi;
		}
		break;

	 
	case MCE_RSP_CMD_ILLEGAL:
	case MCE_RSP_TX_TIMEOUT:
		ir->need_reset = true;
		break;

	default:
		break;
	}
}

static void mceusb_process_ir_data(struct mceusb_dev *ir, int buf_len)
{
	struct ir_raw_event rawir = {};
	bool event = false;
	int i = 0;

	 
	if (ir->flags.microsoft_gen1)
		i = 2;

	 
	if (buf_len <= i)
		return;

	for (; i < buf_len; i++) {
		switch (ir->parser_state) {
		case SUBCMD:
			ir->rem = mceusb_cmd_datasize(ir->cmd, ir->buf_in[i]);
			mceusb_dev_printdata(ir, ir->buf_in, buf_len, i - 1,
					     ir->rem + 2, false);
			if (i + ir->rem < buf_len)
				mceusb_handle_command(ir, &ir->buf_in[i - 1]);
			ir->parser_state = CMD_DATA;
			break;
		case PARSE_IRDATA:
			ir->rem--;
			rawir.pulse = ((ir->buf_in[i] & MCE_PULSE_BIT) != 0);
			rawir.duration = (ir->buf_in[i] & MCE_PULSE_MASK);
			if (unlikely(!rawir.duration)) {
				dev_dbg(ir->dev, "nonsensical irdata %02x with duration 0",
					ir->buf_in[i]);
				break;
			}
			if (rawir.pulse) {
				ir->pulse_tunit += rawir.duration;
				ir->pulse_count++;
			}
			rawir.duration *= MCE_TIME_UNIT;

			dev_dbg(ir->dev, "Storing %s %u us (%02x)",
				rawir.pulse ? "pulse" : "space",
				rawir.duration,	ir->buf_in[i]);

			if (ir_raw_event_store_with_filter(ir->rc, &rawir))
				event = true;
			break;
		case CMD_DATA:
			ir->rem--;
			break;
		case CMD_HEADER:
			ir->cmd = ir->buf_in[i];
			if ((ir->cmd == MCE_CMD_PORT_IR) ||
			    ((ir->cmd & MCE_PORT_MASK) !=
			     MCE_COMMAND_IRDATA)) {
				 
				ir->parser_state = SUBCMD;
				continue;
			}
			 
			ir->rem = (ir->cmd & MCE_PACKET_LENGTH_MASK);
			mceusb_dev_printdata(ir, ir->buf_in, buf_len,
					     i, ir->rem + 1, false);
			if (ir->rem) {
				ir->parser_state = PARSE_IRDATA;
			} else {
				struct ir_raw_event ev = {
					.timeout = 1,
					.duration = ir->rc->timeout
				};

				if (ir_raw_event_store_with_filter(ir->rc,
								   &ev))
					event = true;
				ir->pulse_tunit = 0;
				ir->pulse_count = 0;
			}
			break;
		}

		if (ir->parser_state != CMD_HEADER && !ir->rem)
			ir->parser_state = CMD_HEADER;
	}

	 
	if (ir->parser_state != PARSE_IRDATA || !ir->rem)
		ir->parser_state = CMD_HEADER;

	if (event) {
		dev_dbg(ir->dev, "processed IR data");
		ir_raw_event_handle(ir->rc);
	}
}

static void mceusb_dev_recv(struct urb *urb)
{
	struct mceusb_dev *ir;

	if (!urb)
		return;

	ir = urb->context;
	if (!ir) {
		usb_unlink_urb(urb);
		return;
	}

	switch (urb->status) {
	 
	case 0:
		mceusb_process_ir_data(ir, urb->actual_length);
		break;

	case -ECONNRESET:
	case -ENOENT:
	case -EILSEQ:
	case -EPROTO:
	case -ESHUTDOWN:
		usb_unlink_urb(urb);
		return;

	case -EPIPE:
		dev_err(ir->dev, "Error: urb status = %d (RX HALT)",
			urb->status);
		mceusb_defer_kevent(ir, EVENT_RX_HALT);
		return;

	default:
		dev_err(ir->dev, "Error: urb status = %d", urb->status);
		break;
	}

	usb_submit_urb(urb, GFP_ATOMIC);
}

static void mceusb_get_emulator_version(struct mceusb_dev *ir)
{
	 
	ir->emver = 1;
	mce_command_out(ir, GET_EMVER, sizeof(GET_EMVER));
}

static void mceusb_gen1_init(struct mceusb_dev *ir)
{
	int ret;
	struct device *dev = ir->dev;
	char data[USB_CTRL_MSG_SZ];

	 
	ret = usb_control_msg_recv(ir->usbdev, 0, USB_REQ_SET_ADDRESS,
				   USB_DIR_IN | USB_TYPE_VENDOR,
				   0, 0, data, USB_CTRL_MSG_SZ, 3000,
				   GFP_KERNEL);
	dev_dbg(dev, "set address - ret = %d", ret);
	dev_dbg(dev, "set address - data[0] = %d, data[1] = %d",
						data[0], data[1]);

	 
	ret = usb_control_msg_send(ir->usbdev, 0,
				   USB_REQ_SET_FEATURE, USB_TYPE_VENDOR,
				   0xc04e, 0x0000, NULL, 0, 3000, GFP_KERNEL);

	dev_dbg(dev, "set feature - ret = %d", ret);

	 
	ret = usb_control_msg_send(ir->usbdev, 0,
				   4, USB_TYPE_VENDOR,
				   0x0808, 0x0000, NULL, 0, 3000, GFP_KERNEL);
	dev_dbg(dev, "set char length - retB = %d", ret);

	 
	ret = usb_control_msg_send(ir->usbdev, 0,
				   2, USB_TYPE_VENDOR,
				   0x0000, 0x0100, NULL, 0, 3000, GFP_KERNEL);
	dev_dbg(dev, "set handshake  - retC = %d", ret);

	 
	mce_command_out(ir, DEVICE_RESUME, sizeof(DEVICE_RESUME));

	 
	mce_command_out(ir, GET_REVISION, sizeof(GET_REVISION));
}

static void mceusb_gen2_init(struct mceusb_dev *ir)
{
	 
	mce_command_out(ir, DEVICE_RESUME, sizeof(DEVICE_RESUME));

	 
	mce_command_out(ir, GET_WAKEVERSION, sizeof(GET_WAKEVERSION));

	 
	mce_command_out(ir, GET_UNKNOWN2, sizeof(GET_UNKNOWN2));
}

static void mceusb_get_parameters(struct mceusb_dev *ir)
{
	int i;
	unsigned char cmdbuf[3] = { MCE_CMD_PORT_SYS,
				    MCE_CMD_GETPORTSTATUS, 0x00 };

	 
	ir->num_txports = 2;
	ir->num_rxports = 2;

	 
	mce_command_out(ir, GET_NUM_PORTS, sizeof(GET_NUM_PORTS));

	 
	mce_command_out(ir, GET_CARRIER_FREQ, sizeof(GET_CARRIER_FREQ));

	if (ir->num_txports && !ir->flags.no_tx)
		 
		mce_command_out(ir, GET_TX_BITMASK, sizeof(GET_TX_BITMASK));

	 
	mce_command_out(ir, GET_RX_TIMEOUT, sizeof(GET_RX_TIMEOUT));

	 
	mce_command_out(ir, GET_RX_SENSOR, sizeof(GET_RX_SENSOR));

	for (i = 0; i < ir->num_txports; i++) {
		cmdbuf[2] = i;
		mce_command_out(ir, cmdbuf, sizeof(cmdbuf));
	}
}

static void mceusb_flash_led(struct mceusb_dev *ir)
{
	if (ir->emver < 2)
		return;

	mce_command_out(ir, FLASH_LED, sizeof(FLASH_LED));
}

 
static void mceusb_deferred_kevent(struct work_struct *work)
{
	struct mceusb_dev *ir =
		container_of(work, struct mceusb_dev, kevent);
	int status;

	dev_err(ir->dev, "kevent handler called (flags 0x%lx)",
		ir->kevent_flags);

	if (test_bit(EVENT_RST_PEND, &ir->kevent_flags)) {
		dev_err(ir->dev, "kevent handler canceled pending USB Reset Device");
		return;
	}

	if (test_bit(EVENT_RX_HALT, &ir->kevent_flags)) {
		usb_unlink_urb(ir->urb_in);
		status = usb_clear_halt(ir->usbdev, ir->pipe_in);
		dev_err(ir->dev, "rx clear halt status = %d", status);
		if (status < 0) {
			 
			dev_err(ir->dev,
				"stuck RX HALT state requires USB Reset Device to clear");
			usb_queue_reset_device(ir->usbintf);
			set_bit(EVENT_RST_PEND, &ir->kevent_flags);
			clear_bit(EVENT_RX_HALT, &ir->kevent_flags);

			 
			clear_bit(EVENT_TX_HALT, &ir->kevent_flags);
			return;
		}
		clear_bit(EVENT_RX_HALT, &ir->kevent_flags);
		status = usb_submit_urb(ir->urb_in, GFP_KERNEL);
		if (status < 0) {
			dev_err(ir->dev, "rx unhalt submit urb error = %d",
				status);
		}
	}

	if (test_bit(EVENT_TX_HALT, &ir->kevent_flags)) {
		status = usb_clear_halt(ir->usbdev, ir->pipe_out);
		dev_err(ir->dev, "tx clear halt status = %d", status);
		if (status < 0) {
			 
			dev_err(ir->dev,
				"stuck TX HALT state requires USB Reset Device to clear");
			usb_queue_reset_device(ir->usbintf);
			set_bit(EVENT_RST_PEND, &ir->kevent_flags);
			clear_bit(EVENT_TX_HALT, &ir->kevent_flags);

			 
			clear_bit(EVENT_RX_HALT, &ir->kevent_flags);
			return;
		}
		clear_bit(EVENT_TX_HALT, &ir->kevent_flags);
	}
}

static struct rc_dev *mceusb_init_rc_dev(struct mceusb_dev *ir)
{
	struct usb_device *udev = ir->usbdev;
	struct device *dev = ir->dev;
	struct rc_dev *rc;
	int ret;

	rc = rc_allocate_device(RC_DRIVER_IR_RAW);
	if (!rc) {
		dev_err(dev, "remote dev allocation failed");
		goto out;
	}

	snprintf(ir->name, sizeof(ir->name), "%s (%04x:%04x)",
		 mceusb_model[ir->model].name ?
			mceusb_model[ir->model].name :
			"Media Center Ed. eHome Infrared Remote Transceiver",
		 le16_to_cpu(ir->usbdev->descriptor.idVendor),
		 le16_to_cpu(ir->usbdev->descriptor.idProduct));

	usb_make_path(ir->usbdev, ir->phys, sizeof(ir->phys));

	rc->device_name = ir->name;
	rc->input_phys = ir->phys;
	usb_to_input_id(ir->usbdev, &rc->input_id);
	rc->dev.parent = dev;
	rc->priv = ir;
	rc->allowed_protocols = RC_PROTO_BIT_ALL_IR_DECODER;
	rc->rx_resolution = MCE_TIME_UNIT;
	rc->min_timeout = MCE_TIME_UNIT;
	rc->timeout = MS_TO_US(100);
	if (!mceusb_model[ir->model].broken_irtimeout) {
		rc->s_timeout = mceusb_set_timeout;
		rc->max_timeout = 10 * IR_DEFAULT_TIMEOUT;
	} else {
		 
		rc->max_timeout = rc->timeout;
	}
	if (!ir->flags.no_tx) {
		rc->s_tx_mask = mceusb_set_tx_mask;
		rc->s_tx_carrier = mceusb_set_tx_carrier;
		rc->tx_ir = mceusb_tx_ir;
	}
	if (ir->flags.rx2 > 0) {
		rc->s_wideband_receiver = mceusb_set_rx_wideband;
		rc->s_carrier_report = mceusb_set_rx_carrier_report;
	}
	rc->driver_name = DRIVER_NAME;

	switch (le16_to_cpu(udev->descriptor.idVendor)) {
	case VENDOR_HAUPPAUGE:
		rc->map_name = RC_MAP_HAUPPAUGE;
		break;
	case VENDOR_PCTV:
		rc->map_name = RC_MAP_PINNACLE_PCTV_HD;
		break;
	default:
		rc->map_name = RC_MAP_RC6_MCE;
	}
	if (mceusb_model[ir->model].rc_map)
		rc->map_name = mceusb_model[ir->model].rc_map;

	ret = rc_register_device(rc);
	if (ret < 0) {
		dev_err(dev, "remote dev registration failed");
		goto out;
	}

	return rc;

out:
	rc_free_device(rc);
	return NULL;
}

static int mceusb_dev_probe(struct usb_interface *intf,
			    const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	struct usb_host_interface *idesc;
	struct usb_endpoint_descriptor *ep = NULL;
	struct usb_endpoint_descriptor *ep_in = NULL;
	struct usb_endpoint_descriptor *ep_out = NULL;
	struct mceusb_dev *ir = NULL;
	int pipe, maxp, i, res;
	char buf[63], name[128] = "";
	enum mceusb_model_type model = id->driver_info;
	bool is_gen3;
	bool is_microsoft_gen1;
	bool tx_mask_normal;
	int ir_intfnum;

	dev_dbg(&intf->dev, "%s called", __func__);

	idesc  = intf->cur_altsetting;

	is_gen3 = mceusb_model[model].mce_gen3;
	is_microsoft_gen1 = mceusb_model[model].mce_gen1;
	tx_mask_normal = mceusb_model[model].tx_mask_normal;
	ir_intfnum = mceusb_model[model].ir_intfnum;

	 
	if (idesc->desc.bInterfaceNumber != ir_intfnum)
		return -ENODEV;

	 
	for (i = 0; i < idesc->desc.bNumEndpoints; ++i) {
		ep = &idesc->endpoint[i].desc;

		if (ep_in == NULL) {
			if (usb_endpoint_is_bulk_in(ep)) {
				ep_in = ep;
				dev_dbg(&intf->dev, "acceptable bulk inbound endpoint found\n");
			} else if (usb_endpoint_is_int_in(ep)) {
				ep_in = ep;
				ep_in->bInterval = 1;
				dev_dbg(&intf->dev, "acceptable interrupt inbound endpoint found\n");
			}
		}

		if (ep_out == NULL) {
			if (usb_endpoint_is_bulk_out(ep)) {
				ep_out = ep;
				dev_dbg(&intf->dev, "acceptable bulk outbound endpoint found\n");
			} else if (usb_endpoint_is_int_out(ep)) {
				ep_out = ep;
				ep_out->bInterval = 1;
				dev_dbg(&intf->dev, "acceptable interrupt outbound endpoint found\n");
			}
		}
	}
	if (!ep_in || !ep_out) {
		dev_dbg(&intf->dev, "required endpoints not found\n");
		return -ENODEV;
	}

	if (usb_endpoint_xfer_int(ep_in))
		pipe = usb_rcvintpipe(dev, ep_in->bEndpointAddress);
	else
		pipe = usb_rcvbulkpipe(dev, ep_in->bEndpointAddress);
	maxp = usb_maxpacket(dev, pipe);

	ir = kzalloc(sizeof(struct mceusb_dev), GFP_KERNEL);
	if (!ir)
		goto mem_alloc_fail;

	ir->pipe_in = pipe;
	ir->buf_in = usb_alloc_coherent(dev, maxp, GFP_KERNEL, &ir->dma_in);
	if (!ir->buf_in)
		goto buf_in_alloc_fail;

	ir->urb_in = usb_alloc_urb(0, GFP_KERNEL);
	if (!ir->urb_in)
		goto urb_in_alloc_fail;

	ir->usbintf = intf;
	ir->usbdev = usb_get_dev(dev);
	ir->dev = &intf->dev;
	ir->len_in = maxp;
	ir->flags.microsoft_gen1 = is_microsoft_gen1;
	ir->flags.tx_mask_normal = tx_mask_normal;
	ir->flags.no_tx = mceusb_model[model].no_tx;
	ir->flags.rx2 = mceusb_model[model].rx2;
	ir->model = model;

	 
	ir->usb_ep_out = ep_out;
	if (usb_endpoint_xfer_int(ep_out))
		ir->pipe_out = usb_sndintpipe(ir->usbdev,
					      ep_out->bEndpointAddress);
	else
		ir->pipe_out = usb_sndbulkpipe(ir->usbdev,
					       ep_out->bEndpointAddress);

	if (dev->descriptor.iManufacturer
	    && usb_string(dev, dev->descriptor.iManufacturer,
			  buf, sizeof(buf)) > 0)
		strscpy(name, buf, sizeof(name));
	if (dev->descriptor.iProduct
	    && usb_string(dev, dev->descriptor.iProduct,
			  buf, sizeof(buf)) > 0)
		snprintf(name + strlen(name), sizeof(name) - strlen(name),
			 " %s", buf);

	 
	INIT_WORK(&ir->kevent, mceusb_deferred_kevent);

	ir->rc = mceusb_init_rc_dev(ir);
	if (!ir->rc)
		goto rc_dev_fail;

	 
	if (usb_endpoint_xfer_int(ep_in))
		usb_fill_int_urb(ir->urb_in, dev, pipe, ir->buf_in, maxp,
				 mceusb_dev_recv, ir, ep_in->bInterval);
	else
		usb_fill_bulk_urb(ir->urb_in, dev, pipe, ir->buf_in, maxp,
				  mceusb_dev_recv, ir);

	ir->urb_in->transfer_dma = ir->dma_in;
	ir->urb_in->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	 
	dev_dbg(&intf->dev, "Flushing receive buffers");
	res = usb_submit_urb(ir->urb_in, GFP_KERNEL);
	if (res)
		dev_err(&intf->dev, "failed to flush buffers: %d", res);

	 
	mceusb_get_emulator_version(ir);

	 
	if (ir->flags.microsoft_gen1)
		mceusb_gen1_init(ir);
	else if (!is_gen3)
		mceusb_gen2_init(ir);

	mceusb_get_parameters(ir);

	mceusb_flash_led(ir);

	if (!ir->flags.no_tx)
		mceusb_set_tx_mask(ir->rc, MCE_DEFAULT_TX_MASK);

	usb_set_intfdata(intf, ir);

	 
	device_set_wakeup_capable(ir->dev, true);
	device_set_wakeup_enable(ir->dev, true);

	dev_info(&intf->dev, "Registered %s with mce emulator interface version %x",
		name, ir->emver);
	dev_info(&intf->dev, "%x tx ports (0x%x cabled) and %x rx sensors (0x%x active)",
		 ir->num_txports, ir->txports_cabled,
		 ir->num_rxports, ir->rxports_active);

	return 0;

	 
rc_dev_fail:
	cancel_work_sync(&ir->kevent);
	usb_put_dev(ir->usbdev);
	usb_kill_urb(ir->urb_in);
	usb_free_urb(ir->urb_in);
urb_in_alloc_fail:
	usb_free_coherent(dev, maxp, ir->buf_in, ir->dma_in);
buf_in_alloc_fail:
	kfree(ir);
mem_alloc_fail:
	dev_err(&intf->dev, "%s: device setup failed!", __func__);

	return -ENOMEM;
}


static void mceusb_dev_disconnect(struct usb_interface *intf)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	struct mceusb_dev *ir = usb_get_intfdata(intf);

	dev_dbg(&intf->dev, "%s called", __func__);

	usb_set_intfdata(intf, NULL);

	if (!ir)
		return;

	ir->usbdev = NULL;
	cancel_work_sync(&ir->kevent);
	rc_unregister_device(ir->rc);
	usb_kill_urb(ir->urb_in);
	usb_free_urb(ir->urb_in);
	usb_free_coherent(dev, ir->len_in, ir->buf_in, ir->dma_in);
	usb_put_dev(dev);

	kfree(ir);
}

static int mceusb_dev_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct mceusb_dev *ir = usb_get_intfdata(intf);
	dev_info(ir->dev, "suspend");
	usb_kill_urb(ir->urb_in);
	return 0;
}

static int mceusb_dev_resume(struct usb_interface *intf)
{
	struct mceusb_dev *ir = usb_get_intfdata(intf);
	dev_info(ir->dev, "resume");
	if (usb_submit_urb(ir->urb_in, GFP_ATOMIC))
		return -EIO;
	return 0;
}

static struct usb_driver mceusb_dev_driver = {
	.name =		DRIVER_NAME,
	.probe =	mceusb_dev_probe,
	.disconnect =	mceusb_dev_disconnect,
	.suspend =	mceusb_dev_suspend,
	.resume =	mceusb_dev_resume,
	.reset_resume =	mceusb_dev_resume,
	.id_table =	mceusb_dev_table
};

module_usb_driver(mceusb_dev_driver);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(usb, mceusb_dev_table);
