
 

#ifndef BRCMFMAC_BUS_H
#define BRCMFMAC_BUS_H

#include <linux/kernel.h>
#include <linux/firmware.h>
#include <linux/device.h>
#include "debug.h"

 
#define BRCMF_H2D_MSGRING_CONTROL_SUBMIT	0
#define BRCMF_H2D_MSGRING_RXPOST_SUBMIT		1
#define BRCMF_H2D_MSGRING_FLOWRING_IDSTART	2
#define BRCMF_D2H_MSGRING_CONTROL_COMPLETE	2
#define BRCMF_D2H_MSGRING_TX_COMPLETE		3
#define BRCMF_D2H_MSGRING_RX_COMPLETE		4


#define BRCMF_NROF_H2D_COMMON_MSGRINGS		2
#define BRCMF_NROF_D2H_COMMON_MSGRINGS		3
#define BRCMF_NROF_COMMON_MSGRINGS	(BRCMF_NROF_H2D_COMMON_MSGRINGS + \
					 BRCMF_NROF_D2H_COMMON_MSGRINGS)

 
#define BRCMF_CONSOLE	10

 
#define MAX_CONSOLE_INTERVAL	(5 * 60)

enum brcmf_fwvendor {
	BRCMF_FWVENDOR_WCC,
	BRCMF_FWVENDOR_CYW,
	BRCMF_FWVENDOR_BCA,
	 
	BRCMF_FWVENDOR_NUM,
	BRCMF_FWVENDOR_INVALID
};

 
enum brcmf_bus_state {
	BRCMF_BUS_DOWN,		 
	BRCMF_BUS_UP		 
};

 
enum brcmf_bus_protocol_type {
	BRCMF_PROTO_BCDC,
	BRCMF_PROTO_MSGBUF
};

 
enum brcmf_blob_type {
	BRCMF_BLOB_CLM,
	BRCMF_BLOB_TXCAP,
};

struct brcmf_mp_device;

struct brcmf_bus_dcmd {
	char *name;
	char *param;
	int param_len;
	struct list_head list;
};

 
struct brcmf_bus_ops {
	int (*preinit)(struct device *dev);
	void (*stop)(struct device *dev);
	int (*txdata)(struct device *dev, struct sk_buff *skb);
	int (*txctl)(struct device *dev, unsigned char *msg, uint len);
	int (*rxctl)(struct device *dev, unsigned char *msg, uint len);
	struct pktq * (*gettxq)(struct device *dev);
	void (*wowl_config)(struct device *dev, bool enabled);
	size_t (*get_ramsize)(struct device *dev);
	int (*get_memdump)(struct device *dev, void *data, size_t len);
	int (*get_blob)(struct device *dev, const struct firmware **fw,
			enum brcmf_blob_type type);
	void (*debugfs_create)(struct device *dev);
	int (*reset)(struct device *dev);
	void (*remove)(struct device *dev);
};


 
struct brcmf_bus_msgbuf {
	struct brcmf_commonring *commonrings[BRCMF_NROF_COMMON_MSGRINGS];
	struct brcmf_commonring **flowrings;
	u32 rx_dataoffset;
	u32 max_rxbufpost;
	u16 max_flowrings;
	u16 max_submissionrings;
	u16 max_completionrings;
};


 
struct brcmf_bus_stats {
	atomic_t pktcowed;
	atomic_t pktcow_failed;
};

 
struct brcmf_bus {
	union {
		struct brcmf_sdio_dev *sdio;
		struct brcmf_usbdev *usb;
		struct brcmf_pciedev *pcie;
	} bus_priv;
	enum brcmf_bus_protocol_type proto_type;
	struct device *dev;
	struct brcmf_pub *drvr;
	enum brcmf_bus_state state;
	struct brcmf_bus_stats stats;
	uint maxctl;
	u32 chip;
	u32 chiprev;
	enum brcmf_fwvendor fwvid;
	bool always_use_fws_queue;
	bool wowl_supported;

	const struct brcmf_bus_ops *ops;
	struct brcmf_bus_msgbuf *msgbuf;

	struct list_head list;
};

 
static inline int brcmf_bus_preinit(struct brcmf_bus *bus)
{
	if (!bus->ops->preinit)
		return 0;
	return bus->ops->preinit(bus->dev);
}

static inline void brcmf_bus_stop(struct brcmf_bus *bus)
{
	bus->ops->stop(bus->dev);
}

static inline int brcmf_bus_txdata(struct brcmf_bus *bus, struct sk_buff *skb)
{
	return bus->ops->txdata(bus->dev, skb);
}

static inline
int brcmf_bus_txctl(struct brcmf_bus *bus, unsigned char *msg, uint len)
{
	return bus->ops->txctl(bus->dev, msg, len);
}

static inline
int brcmf_bus_rxctl(struct brcmf_bus *bus, unsigned char *msg, uint len)
{
	return bus->ops->rxctl(bus->dev, msg, len);
}

static inline
struct pktq *brcmf_bus_gettxq(struct brcmf_bus *bus)
{
	if (!bus->ops->gettxq)
		return ERR_PTR(-ENOENT);

	return bus->ops->gettxq(bus->dev);
}

static inline
void brcmf_bus_wowl_config(struct brcmf_bus *bus, bool enabled)
{
	if (bus->ops->wowl_config)
		bus->ops->wowl_config(bus->dev, enabled);
}

static inline size_t brcmf_bus_get_ramsize(struct brcmf_bus *bus)
{
	if (!bus->ops->get_ramsize)
		return 0;

	return bus->ops->get_ramsize(bus->dev);
}

static inline
int brcmf_bus_get_memdump(struct brcmf_bus *bus, void *data, size_t len)
{
	if (!bus->ops->get_memdump)
		return -EOPNOTSUPP;

	return bus->ops->get_memdump(bus->dev, data, len);
}

static inline
int brcmf_bus_get_blob(struct brcmf_bus *bus, const struct firmware **fw,
		       enum brcmf_blob_type type)
{
	return bus->ops->get_blob(bus->dev, fw, type);
}

static inline
void brcmf_bus_debugfs_create(struct brcmf_bus *bus)
{
	if (!bus->ops->debugfs_create)
		return;

	return bus->ops->debugfs_create(bus->dev);
}

static inline
int brcmf_bus_reset(struct brcmf_bus *bus)
{
	if (!bus->ops->reset)
		return -EOPNOTSUPP;

	return bus->ops->reset(bus->dev);
}

static inline void brcmf_bus_remove(struct brcmf_bus *bus)
{
	if (!bus->ops->remove) {
		device_release_driver(bus->dev);
		return;
	}

	bus->ops->remove(bus->dev);
}

 

 
void brcmf_rx_frame(struct device *dev, struct sk_buff *rxp, bool handle_event,
		    bool inirq);
 
void brcmf_rx_event(struct device *dev, struct sk_buff *rxp);

int brcmf_alloc(struct device *dev, struct brcmf_mp_device *settings);
 
int brcmf_attach(struct device *dev);
 
void brcmf_detach(struct device *dev);
void brcmf_free(struct device *dev);
 
void brcmf_dev_reset(struct device *dev);
 
void brcmf_dev_coredump(struct device *dev);
 
void brcmf_fw_crashed(struct device *dev);

 
void brcmf_bus_change_state(struct brcmf_bus *bus, enum brcmf_bus_state state);

s32 brcmf_iovar_data_set(struct device *dev, char *name, void *data, u32 len);
void brcmf_bus_add_txhdrlen(struct device *dev, uint len);

#ifdef CONFIG_BRCMFMAC_SDIO
void brcmf_sdio_exit(void);
int brcmf_sdio_register(void);
#else
static inline void brcmf_sdio_exit(void) { }
static inline int brcmf_sdio_register(void) { return 0; }
#endif

#ifdef CONFIG_BRCMFMAC_USB
void brcmf_usb_exit(void);
int brcmf_usb_register(void);
#else
static inline void brcmf_usb_exit(void) { }
static inline int brcmf_usb_register(void) { return 0; }
#endif

#ifdef CONFIG_BRCMFMAC_PCIE
void brcmf_pcie_exit(void);
int brcmf_pcie_register(void);
#else
static inline void brcmf_pcie_exit(void) { }
static inline int brcmf_pcie_register(void) { return 0; }
#endif

#endif  
