
 
#include <linux/device.h>
#include <linux/ethtool.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/usb.h>

#include <linux/can.h>
#include <linux/can/dev.h>
#include <linux/can/error.h>

#include "pcan_usb_core.h"

MODULE_AUTHOR("Stephane Grosjean <s.grosjean@peak-system.com>");
MODULE_DESCRIPTION("CAN driver for PEAK-System USB adapters");
MODULE_LICENSE("GPL v2");

 
static const struct usb_device_id peak_usb_table[] = {
	{
		USB_DEVICE(PCAN_USB_VENDOR_ID, PCAN_USB_PRODUCT_ID),
		.driver_info = (kernel_ulong_t)&pcan_usb,
	}, {
		USB_DEVICE(PCAN_USB_VENDOR_ID, PCAN_USBPRO_PRODUCT_ID),
		.driver_info = (kernel_ulong_t)&pcan_usb_pro,
	}, {
		USB_DEVICE(PCAN_USB_VENDOR_ID, PCAN_USBFD_PRODUCT_ID),
		.driver_info = (kernel_ulong_t)&pcan_usb_fd,
	}, {
		USB_DEVICE(PCAN_USB_VENDOR_ID, PCAN_USBPROFD_PRODUCT_ID),
		.driver_info = (kernel_ulong_t)&pcan_usb_pro_fd,
	}, {
		USB_DEVICE(PCAN_USB_VENDOR_ID, PCAN_USBCHIP_PRODUCT_ID),
		.driver_info = (kernel_ulong_t)&pcan_usb_chip,
	}, {
		USB_DEVICE(PCAN_USB_VENDOR_ID, PCAN_USBX6_PRODUCT_ID),
		.driver_info = (kernel_ulong_t)&pcan_usb_x6,
	}, {
		 
	}
};

MODULE_DEVICE_TABLE(usb, peak_usb_table);

static ssize_t can_channel_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct net_device *netdev = to_net_dev(dev);
	struct peak_usb_device *peak_dev = netdev_priv(netdev);

	return sysfs_emit(buf, "%08X\n", peak_dev->can_channel_id);
}
static DEVICE_ATTR_RO(can_channel_id);

 
static struct attribute *peak_usb_sysfs_attrs[] = {
	&dev_attr_can_channel_id.attr,
	NULL,
};

static const struct attribute_group peak_usb_sysfs_group = {
	.name	= "peak_usb",
	.attrs	= peak_usb_sysfs_attrs,
};

 
#define DUMP_WIDTH	16
void pcan_dump_mem(const char *prompt, const void *p, int l)
{
	pr_info("%s dumping %s (%d bytes):\n",
		PCAN_USB_DRIVER_NAME, prompt ? prompt : "memory", l);
	print_hex_dump(KERN_INFO, PCAN_USB_DRIVER_NAME " ", DUMP_PREFIX_NONE,
		       DUMP_WIDTH, 1, p, l, false);
}

 
void peak_usb_init_time_ref(struct peak_time_ref *time_ref,
			    const struct peak_usb_adapter *adapter)
{
	if (time_ref) {
		memset(time_ref, 0, sizeof(struct peak_time_ref));
		time_ref->adapter = adapter;
	}
}

 
void peak_usb_update_ts_now(struct peak_time_ref *time_ref, u32 ts_now)
{
	time_ref->ts_dev_2 = ts_now;

	 
	if (ktime_to_ns(time_ref->tv_host) > 0) {
		u32 delta_ts = time_ref->ts_dev_2 - time_ref->ts_dev_1;

		if (time_ref->ts_dev_2 < time_ref->ts_dev_1)
			delta_ts &= (1 << time_ref->adapter->ts_used_bits) - 1;

		time_ref->ts_total += delta_ts;
	}
}

 
void peak_usb_set_ts_now(struct peak_time_ref *time_ref, u32 ts_now)
{
	if (ktime_to_ns(time_ref->tv_host_0) == 0) {
		 
		time_ref->tv_host_0 = ktime_get();
		time_ref->tv_host = ktime_set(0, 0);
	} else {
		 
		if (ktime_to_ns(time_ref->tv_host)) {
			ktime_t delta = ktime_sub(time_ref->tv_host,
						  time_ref->tv_host_0);
			if (ktime_to_ns(delta) > (4200ull * NSEC_PER_SEC)) {
				time_ref->tv_host_0 = time_ref->tv_host;
				time_ref->ts_total = 0;
			}
		}

		time_ref->tv_host = ktime_get();
		time_ref->tick_count++;
	}

	time_ref->ts_dev_1 = time_ref->ts_dev_2;
	peak_usb_update_ts_now(time_ref, ts_now);
}

 
void peak_usb_get_ts_time(struct peak_time_ref *time_ref, u32 ts, ktime_t *time)
{
	 
	if (ktime_to_ns(time_ref->tv_host)) {
		u64 delta_us;
		s64 delta_ts = 0;

		 
		if (time_ref->ts_dev_1 < time_ref->ts_dev_2) {
			 
			if (ts < time_ref->ts_dev_1)
				delta_ts = BIT_ULL(time_ref->adapter->ts_used_bits);

		 
		} else if (time_ref->ts_dev_1 < ts) {
			delta_ts = -BIT_ULL(time_ref->adapter->ts_used_bits);
		}

		 
		delta_ts += (signed int)(ts - time_ref->ts_dev_2);

		 
		delta_ts += time_ref->ts_total;

		 
		delta_us = delta_ts * time_ref->adapter->us_per_ts_scale;
		delta_us >>= time_ref->adapter->us_per_ts_shift;

		*time = ktime_add_us(time_ref->tv_host_0, delta_us);
	} else {
		*time = ktime_get();
	}
}

 
int peak_usb_netif_rx_64(struct sk_buff *skb, u32 ts_low, u32 ts_high)
{
	struct skb_shared_hwtstamps *hwts = skb_hwtstamps(skb);
	u64 ns_ts;

	ns_ts = (u64)ts_high << 32 | ts_low;
	ns_ts *= NSEC_PER_USEC;
	hwts->hwtstamp = ns_to_ktime(ns_ts);

	return netif_rx(skb);
}

 
static void peak_usb_read_bulk_callback(struct urb *urb)
{
	struct peak_usb_device *dev = urb->context;
	struct net_device *netdev;
	int err;

	netdev = dev->netdev;

	if (!netif_device_present(netdev))
		return;

	 
	switch (urb->status) {
	case 0:
		 
		break;

	case -EILSEQ:
	case -ENOENT:
	case -ECONNRESET:
	case -ESHUTDOWN:
		return;

	default:
		if (net_ratelimit())
			netdev_err(netdev,
				   "Rx urb aborted (%d)\n", urb->status);
		goto resubmit_urb;
	}

	 
	if ((urb->actual_length > 0) && (dev->adapter->dev_decode_buf)) {
		 
		if (dev->state & PCAN_USB_STATE_STARTED) {
			err = dev->adapter->dev_decode_buf(dev, urb);
			if (err)
				pcan_dump_mem("received usb message",
					      urb->transfer_buffer,
					      urb->transfer_buffer_length);
		}
	}

resubmit_urb:
	usb_fill_bulk_urb(urb, dev->udev,
		usb_rcvbulkpipe(dev->udev, dev->ep_msg_in),
		urb->transfer_buffer, dev->adapter->rx_buffer_size,
		peak_usb_read_bulk_callback, dev);

	usb_anchor_urb(urb, &dev->rx_submitted);
	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (!err)
		return;

	usb_unanchor_urb(urb);

	if (err == -ENODEV)
		netif_device_detach(netdev);
	else
		netdev_err(netdev, "failed resubmitting read bulk urb: %d\n",
			   err);
}

 
static void peak_usb_write_bulk_callback(struct urb *urb)
{
	struct peak_tx_urb_context *context = urb->context;
	struct peak_usb_device *dev;
	struct net_device *netdev;
	int tx_bytes;

	BUG_ON(!context);

	dev = context->dev;
	netdev = dev->netdev;

	atomic_dec(&dev->active_tx_urbs);

	if (!netif_device_present(netdev))
		return;

	 
	switch (urb->status) {
	case 0:
		 
		netif_trans_update(netdev);
		break;

	case -EPROTO:
	case -ENOENT:
	case -ECONNRESET:
	case -ESHUTDOWN:
		break;

	default:
		if (net_ratelimit())
			netdev_err(netdev, "Tx urb aborted (%d)\n",
				   urb->status);
		break;
	}

	 
	tx_bytes = can_get_echo_skb(netdev, context->echo_index, NULL);
	context->echo_index = PCAN_USB_MAX_TX_URBS;

	if (!urb->status) {
		 
		netdev->stats.tx_packets++;
		netdev->stats.tx_bytes += tx_bytes;

		 
		netif_wake_queue(netdev);
	}
}

 
static netdev_tx_t peak_usb_ndo_start_xmit(struct sk_buff *skb,
					   struct net_device *netdev)
{
	struct peak_usb_device *dev = netdev_priv(netdev);
	struct peak_tx_urb_context *context = NULL;
	struct net_device_stats *stats = &netdev->stats;
	struct urb *urb;
	u8 *obuf;
	int i, err;
	size_t size = dev->adapter->tx_buffer_size;

	if (can_dev_dropped_skb(netdev, skb))
		return NETDEV_TX_OK;

	for (i = 0; i < PCAN_USB_MAX_TX_URBS; i++)
		if (dev->tx_contexts[i].echo_index == PCAN_USB_MAX_TX_URBS) {
			context = dev->tx_contexts + i;
			break;
		}

	if (!context) {
		 
		return NETDEV_TX_BUSY;
	}

	urb = context->urb;
	obuf = urb->transfer_buffer;

	err = dev->adapter->dev_encode_msg(dev, skb, obuf, &size);
	if (err) {
		if (net_ratelimit())
			netdev_err(netdev, "packet dropped\n");
		dev_kfree_skb(skb);
		stats->tx_dropped++;
		return NETDEV_TX_OK;
	}

	context->echo_index = i;

	usb_anchor_urb(urb, &dev->tx_submitted);

	can_put_echo_skb(skb, netdev, context->echo_index, 0);

	atomic_inc(&dev->active_tx_urbs);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err) {
		can_free_echo_skb(netdev, context->echo_index, NULL);

		usb_unanchor_urb(urb);

		 
		context->echo_index = PCAN_USB_MAX_TX_URBS;

		atomic_dec(&dev->active_tx_urbs);

		switch (err) {
		case -ENODEV:
			netif_device_detach(netdev);
			break;
		default:
			netdev_warn(netdev, "tx urb submitting failed err=%d\n",
				    err);
			fallthrough;
		case -ENOENT:
			 
			stats->tx_dropped++;
		}
	} else {
		netif_trans_update(netdev);

		 
		if (atomic_read(&dev->active_tx_urbs) >= PCAN_USB_MAX_TX_URBS)
			netif_stop_queue(netdev);
	}

	return NETDEV_TX_OK;
}

 
static int peak_usb_start(struct peak_usb_device *dev)
{
	struct net_device *netdev = dev->netdev;
	int err, i;

	for (i = 0; i < PCAN_USB_MAX_RX_URBS; i++) {
		struct urb *urb;
		u8 *buf;

		 
		urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!urb) {
			err = -ENOMEM;
			break;
		}

		buf = kmalloc(dev->adapter->rx_buffer_size, GFP_KERNEL);
		if (!buf) {
			usb_free_urb(urb);
			err = -ENOMEM;
			break;
		}

		usb_fill_bulk_urb(urb, dev->udev,
			usb_rcvbulkpipe(dev->udev, dev->ep_msg_in),
			buf, dev->adapter->rx_buffer_size,
			peak_usb_read_bulk_callback, dev);

		 
		urb->transfer_flags |= URB_FREE_BUFFER;
		usb_anchor_urb(urb, &dev->rx_submitted);

		err = usb_submit_urb(urb, GFP_KERNEL);
		if (err) {
			if (err == -ENODEV)
				netif_device_detach(dev->netdev);

			usb_unanchor_urb(urb);
			kfree(buf);
			usb_free_urb(urb);
			break;
		}

		 
		usb_free_urb(urb);
	}

	 
	if (i < PCAN_USB_MAX_RX_URBS) {
		if (i == 0) {
			netdev_err(netdev, "couldn't setup any rx URB\n");
			return err;
		}

		netdev_warn(netdev, "rx performance may be slow\n");
	}

	 
	for (i = 0; i < PCAN_USB_MAX_TX_URBS; i++) {
		struct peak_tx_urb_context *context;
		struct urb *urb;
		u8 *buf;

		 
		urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!urb) {
			err = -ENOMEM;
			break;
		}

		buf = kmalloc(dev->adapter->tx_buffer_size, GFP_KERNEL);
		if (!buf) {
			usb_free_urb(urb);
			err = -ENOMEM;
			break;
		}

		context = dev->tx_contexts + i;
		context->dev = dev;
		context->urb = urb;

		usb_fill_bulk_urb(urb, dev->udev,
			usb_sndbulkpipe(dev->udev, dev->ep_msg_out),
			buf, dev->adapter->tx_buffer_size,
			peak_usb_write_bulk_callback, context);

		 
		urb->transfer_flags |= URB_FREE_BUFFER;
	}

	 
	if (i < PCAN_USB_MAX_TX_URBS) {
		if (i == 0) {
			netdev_err(netdev, "couldn't setup any tx URB\n");
			goto err_tx;
		}

		netdev_warn(netdev, "tx performance may be slow\n");
	}

	if (dev->adapter->dev_start) {
		err = dev->adapter->dev_start(dev);
		if (err)
			goto err_adapter;
	}

	dev->state |= PCAN_USB_STATE_STARTED;

	 
	if (dev->adapter->dev_set_bus) {
		err = dev->adapter->dev_set_bus(dev, 1);
		if (err)
			goto err_adapter;
	}

	dev->can.state = CAN_STATE_ERROR_ACTIVE;

	return 0;

err_adapter:
	if (err == -ENODEV)
		netif_device_detach(dev->netdev);

	netdev_warn(netdev, "couldn't submit control: %d\n", err);

	for (i = 0; i < PCAN_USB_MAX_TX_URBS; i++) {
		usb_free_urb(dev->tx_contexts[i].urb);
		dev->tx_contexts[i].urb = NULL;
	}
err_tx:
	usb_kill_anchored_urbs(&dev->rx_submitted);

	return err;
}

 
static int peak_usb_ndo_open(struct net_device *netdev)
{
	struct peak_usb_device *dev = netdev_priv(netdev);
	int err;

	 
	err = open_candev(netdev);
	if (err)
		return err;

	 
	err = peak_usb_start(dev);
	if (err) {
		netdev_err(netdev, "couldn't start device: %d\n", err);
		close_candev(netdev);
		return err;
	}

	netif_start_queue(netdev);

	return 0;
}

 
static void peak_usb_unlink_all_urbs(struct peak_usb_device *dev)
{
	int i;

	 
	usb_kill_anchored_urbs(&dev->rx_submitted);

	 
	for (i = 0; i < PCAN_USB_MAX_TX_URBS; i++) {
		struct urb *urb = dev->tx_contexts[i].urb;

		if (!urb ||
		    dev->tx_contexts[i].echo_index != PCAN_USB_MAX_TX_URBS) {
			 
			continue;
		}

		usb_free_urb(urb);
		dev->tx_contexts[i].urb = NULL;
	}

	 
	usb_kill_anchored_urbs(&dev->tx_submitted);
	atomic_set(&dev->active_tx_urbs, 0);
}

 
static int peak_usb_ndo_stop(struct net_device *netdev)
{
	struct peak_usb_device *dev = netdev_priv(netdev);

	dev->state &= ~PCAN_USB_STATE_STARTED;
	netif_stop_queue(netdev);

	close_candev(netdev);

	dev->can.state = CAN_STATE_STOPPED;

	 
	peak_usb_unlink_all_urbs(dev);

	if (dev->adapter->dev_stop)
		dev->adapter->dev_stop(dev);

	 
	if (dev->adapter->dev_set_bus) {
		int err = dev->adapter->dev_set_bus(dev, 0);

		if (err)
			return err;
	}

	return 0;
}

 
void peak_usb_restart_complete(struct peak_usb_device *dev)
{
	 
	dev->can.state = CAN_STATE_ERROR_ACTIVE;

	 
	netif_wake_queue(dev->netdev);
}

void peak_usb_async_complete(struct urb *urb)
{
	kfree(urb->transfer_buffer);
	usb_free_urb(urb);
}

 
static int peak_usb_restart(struct peak_usb_device *dev)
{
	struct urb *urb;
	int err;
	u8 *buf;

	 
	if (!dev->adapter->dev_restart_async) {
		peak_usb_restart_complete(dev);
		return 0;
	}

	 
	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb)
		return -ENOMEM;

	 
	buf = kmalloc(PCAN_USB_MAX_CMD_LEN, GFP_ATOMIC);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}

	 
	err = dev->adapter->dev_restart_async(dev, urb, buf);
	if (!err)
		return 0;

	kfree(buf);
	usb_free_urb(urb);

	return err;
}

 
static int peak_usb_set_mode(struct net_device *netdev, enum can_mode mode)
{
	struct peak_usb_device *dev = netdev_priv(netdev);
	int err = 0;

	switch (mode) {
	case CAN_MODE_START:
		err = peak_usb_restart(dev);
		if (err)
			netdev_err(netdev, "couldn't start device (err %d)\n",
				   err);
		break;

	default:
		return -EOPNOTSUPP;
	}

	return err;
}

 
static int peak_usb_set_bittiming(struct net_device *netdev)
{
	struct peak_usb_device *dev = netdev_priv(netdev);
	const struct peak_usb_adapter *pa = dev->adapter;

	if (pa->dev_set_bittiming) {
		struct can_bittiming *bt = &dev->can.bittiming;
		int err = pa->dev_set_bittiming(dev, bt);

		if (err)
			netdev_info(netdev, "couldn't set bitrate (err %d)\n",
				    err);
		return err;
	}

	return 0;
}

 
static int peak_usb_set_data_bittiming(struct net_device *netdev)
{
	struct peak_usb_device *dev = netdev_priv(netdev);
	const struct peak_usb_adapter *pa = dev->adapter;

	if (pa->dev_set_data_bittiming) {
		struct can_bittiming *bt = &dev->can.data_bittiming;
		int err = pa->dev_set_data_bittiming(dev, bt);

		if (err)
			netdev_info(netdev,
				    "couldn't set data bitrate (err %d)\n",
				    err);

		return err;
	}

	return 0;
}

static int peak_eth_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
	struct hwtstamp_config hwts_cfg = { 0 };

	switch (cmd) {
	case SIOCSHWTSTAMP:  
		if (copy_from_user(&hwts_cfg, ifr->ifr_data, sizeof(hwts_cfg)))
			return -EFAULT;
		if (hwts_cfg.tx_type == HWTSTAMP_TX_OFF &&
		    hwts_cfg.rx_filter == HWTSTAMP_FILTER_ALL)
			return 0;
		return -ERANGE;

	case SIOCGHWTSTAMP:  
		hwts_cfg.tx_type = HWTSTAMP_TX_OFF;
		hwts_cfg.rx_filter = HWTSTAMP_FILTER_ALL;
		if (copy_to_user(ifr->ifr_data, &hwts_cfg, sizeof(hwts_cfg)))
			return -EFAULT;
		return 0;

	default:
		return -EOPNOTSUPP;
	}
}

static const struct net_device_ops peak_usb_netdev_ops = {
	.ndo_open = peak_usb_ndo_open,
	.ndo_stop = peak_usb_ndo_stop,
	.ndo_eth_ioctl = peak_eth_ioctl,
	.ndo_start_xmit = peak_usb_ndo_start_xmit,
	.ndo_change_mtu = can_change_mtu,
};

 
int peak_usb_get_eeprom_len(struct net_device *netdev)
{
	return sizeof(u32);
}

 
int peak_usb_get_eeprom(struct net_device *netdev,
			struct ethtool_eeprom *eeprom, u8 *data)
{
	struct peak_usb_device *dev = netdev_priv(netdev);
	u32 ch_id;
	__le32 ch_id_le;
	int err;

	err = dev->adapter->dev_get_can_channel_id(dev, &ch_id);
	if (err)
		return err;

	 
	ch_id_le = cpu_to_le32(ch_id);
	memcpy(data, (u8 *)&ch_id_le + eeprom->offset, eeprom->len);

	 
	dev->can_channel_id = ch_id;
	return err;
}

 
int peak_usb_set_eeprom(struct net_device *netdev,
			struct ethtool_eeprom *eeprom, u8 *data)
{
	struct peak_usb_device *dev = netdev_priv(netdev);
	u32 ch_id;
	__le32 ch_id_le;
	int err;

	 
	err = dev->adapter->dev_get_can_channel_id(dev, &ch_id);
	if (err) {
		netdev_err(netdev, "Failed to init CAN channel id (err %d)\n", err);
		return err;
	}

	 
	ch_id_le = cpu_to_le32(ch_id);
	memcpy((u8 *)&ch_id_le + eeprom->offset, data, eeprom->len);
	ch_id = le32_to_cpu(ch_id_le);

	 
	err = dev->adapter->dev_set_can_channel_id(dev, ch_id);
	if (err) {
		netdev_err(netdev, "Failed to write new CAN channel id (err %d)\n",
			   err);
		return err;
	}

	 
	dev->can_channel_id = ch_id;

	return 0;
}

int pcan_get_ts_info(struct net_device *dev, struct ethtool_ts_info *info)
{
	info->so_timestamping =
		SOF_TIMESTAMPING_TX_SOFTWARE |
		SOF_TIMESTAMPING_RX_SOFTWARE |
		SOF_TIMESTAMPING_SOFTWARE |
		SOF_TIMESTAMPING_RX_HARDWARE |
		SOF_TIMESTAMPING_RAW_HARDWARE;
	info->phc_index = -1;
	info->tx_types = BIT(HWTSTAMP_TX_OFF);
	info->rx_filters = BIT(HWTSTAMP_FILTER_ALL);

	return 0;
}

 
static int peak_usb_create_dev(const struct peak_usb_adapter *peak_usb_adapter,
			       struct usb_interface *intf, int ctrl_idx)
{
	struct usb_device *usb_dev = interface_to_usbdev(intf);
	int sizeof_candev = peak_usb_adapter->sizeof_dev_private;
	struct peak_usb_device *dev;
	struct net_device *netdev;
	int i, err;
	u16 tmp16;

	if (sizeof_candev < sizeof(struct peak_usb_device))
		sizeof_candev = sizeof(struct peak_usb_device);

	netdev = alloc_candev(sizeof_candev, PCAN_USB_MAX_TX_URBS);
	if (!netdev) {
		dev_err(&intf->dev, "%s: couldn't alloc candev\n",
			PCAN_USB_DRIVER_NAME);
		return -ENOMEM;
	}

	dev = netdev_priv(netdev);

	 
	dev->cmd_buf = kzalloc(PCAN_USB_MAX_CMD_LEN, GFP_KERNEL);
	if (!dev->cmd_buf) {
		err = -ENOMEM;
		goto lbl_free_candev;
	}

	dev->udev = usb_dev;
	dev->netdev = netdev;
	dev->adapter = peak_usb_adapter;
	dev->ctrl_idx = ctrl_idx;
	dev->state = PCAN_USB_STATE_CONNECTED;

	dev->ep_msg_in = peak_usb_adapter->ep_msg_in;
	dev->ep_msg_out = peak_usb_adapter->ep_msg_out[ctrl_idx];

	dev->can.clock = peak_usb_adapter->clock;
	dev->can.bittiming_const = peak_usb_adapter->bittiming_const;
	dev->can.do_set_bittiming = peak_usb_set_bittiming;
	dev->can.data_bittiming_const = peak_usb_adapter->data_bittiming_const;
	dev->can.do_set_data_bittiming = peak_usb_set_data_bittiming;
	dev->can.do_set_mode = peak_usb_set_mode;
	dev->can.do_get_berr_counter = peak_usb_adapter->do_get_berr_counter;
	dev->can.ctrlmode_supported = peak_usb_adapter->ctrlmode_supported;

	netdev->netdev_ops = &peak_usb_netdev_ops;

	netdev->flags |= IFF_ECHO;  

	 
	netdev->ethtool_ops = peak_usb_adapter->ethtool_ops;

	 
	netdev->sysfs_groups[0] = &peak_usb_sysfs_group;

	init_usb_anchor(&dev->rx_submitted);

	init_usb_anchor(&dev->tx_submitted);
	atomic_set(&dev->active_tx_urbs, 0);

	for (i = 0; i < PCAN_USB_MAX_TX_URBS; i++)
		dev->tx_contexts[i].echo_index = PCAN_USB_MAX_TX_URBS;

	dev->prev_siblings = usb_get_intfdata(intf);
	usb_set_intfdata(intf, dev);

	SET_NETDEV_DEV(netdev, &intf->dev);
	netdev->dev_id = ctrl_idx;

	err = register_candev(netdev);
	if (err) {
		dev_err(&intf->dev, "couldn't register CAN device: %d\n", err);
		goto lbl_restore_intf_data;
	}

	if (dev->prev_siblings)
		(dev->prev_siblings)->next_siblings = dev;

	 
	tmp16 = le16_to_cpu(usb_dev->descriptor.bcdDevice);
	dev->device_rev = tmp16 >> 8;

	if (dev->adapter->dev_init) {
		err = dev->adapter->dev_init(dev);
		if (err)
			goto lbl_unregister_candev;
	}

	 
	if (dev->adapter->dev_set_bus) {
		err = dev->adapter->dev_set_bus(dev, 0);
		if (err)
			goto adap_dev_free;
	}

	 
	dev->adapter->dev_get_can_channel_id(dev, &dev->can_channel_id);

	netdev_info(netdev, "attached to %s channel %u (device 0x%08X)\n",
		    peak_usb_adapter->name, ctrl_idx, dev->can_channel_id);

	return 0;

adap_dev_free:
	if (dev->adapter->dev_free)
		dev->adapter->dev_free(dev);

lbl_unregister_candev:
	unregister_candev(netdev);

lbl_restore_intf_data:
	usb_set_intfdata(intf, dev->prev_siblings);
	kfree(dev->cmd_buf);

lbl_free_candev:
	free_candev(netdev);

	return err;
}

 
static void peak_usb_disconnect(struct usb_interface *intf)
{
	struct peak_usb_device *dev;
	struct peak_usb_device *dev_prev_siblings;

	 
	for (dev = usb_get_intfdata(intf); dev; dev = dev_prev_siblings) {
		struct net_device *netdev = dev->netdev;
		char name[IFNAMSIZ];

		dev_prev_siblings = dev->prev_siblings;
		dev->state &= ~PCAN_USB_STATE_CONNECTED;
		strscpy(name, netdev->name, IFNAMSIZ);

		unregister_candev(netdev);

		kfree(dev->cmd_buf);
		dev->next_siblings = NULL;
		if (dev->adapter->dev_free)
			dev->adapter->dev_free(dev);

		free_candev(netdev);
		dev_info(&intf->dev, "%s removed\n", name);
	}

	usb_set_intfdata(intf, NULL);
}

 
static int peak_usb_probe(struct usb_interface *intf,
			  const struct usb_device_id *id)
{
	const struct peak_usb_adapter *peak_usb_adapter;
	int i, err = -ENOMEM;

	 
	peak_usb_adapter = (const struct peak_usb_adapter *)id->driver_info;

	 
	if (peak_usb_adapter->intf_probe) {
		err = peak_usb_adapter->intf_probe(intf);
		if (err)
			return err;
	}

	for (i = 0; i < peak_usb_adapter->ctrl_count; i++) {
		err = peak_usb_create_dev(peak_usb_adapter, intf, i);
		if (err) {
			 
			peak_usb_disconnect(intf);
			break;
		}
	}

	return err;
}

 
static struct usb_driver peak_usb_driver = {
	.name = PCAN_USB_DRIVER_NAME,
	.disconnect = peak_usb_disconnect,
	.probe = peak_usb_probe,
	.id_table = peak_usb_table,
};

static int __init peak_usb_init(void)
{
	int err;

	 
	err = usb_register(&peak_usb_driver);
	if (err)
		pr_err("%s: usb_register failed (err %d)\n",
			PCAN_USB_DRIVER_NAME, err);

	return err;
}

static int peak_usb_do_device_exit(struct device *d, void *arg)
{
	struct usb_interface *intf = to_usb_interface(d);
	struct peak_usb_device *dev;

	 
	for (dev = usb_get_intfdata(intf); dev; dev = dev->prev_siblings) {
		struct net_device *netdev = dev->netdev;

		if (netif_device_present(netdev))
			if (dev->adapter->dev_exit)
				dev->adapter->dev_exit(dev);
	}

	return 0;
}

static void __exit peak_usb_exit(void)
{
	int err;

	 
	err = driver_for_each_device(&peak_usb_driver.drvwrap.driver, NULL,
				     NULL, peak_usb_do_device_exit);
	if (err)
		pr_err("%s: failed to stop all can devices (err %d)\n",
			PCAN_USB_DRIVER_NAME, err);

	 
	usb_deregister(&peak_usb_driver);

	pr_info("%s: PCAN-USB interfaces driver unloaded\n",
		PCAN_USB_DRIVER_NAME);
}

module_init(peak_usb_init);
module_exit(peak_usb_exit);
