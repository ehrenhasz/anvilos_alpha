
 

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/firmware.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/poll.h>

#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/signal.h>
#include <linux/ioctl.h>
#include <linux/of.h>
#include <linux/serdev.h>
#include <linux/skbuff.h>
#include <linux/ti_wilink_st.h>
#include <linux/clk.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include <linux/gpio/consumer.h>
#include <linux/nvmem-consumer.h>

#include "hci_uart.h"

 
#define HCI_VS_WRITE_BD_ADDR			0xfc06
#define HCI_VS_UPDATE_UART_HCI_BAUDRATE		0xff36

 
#define HCILL_GO_TO_SLEEP_IND	0x30
#define HCILL_GO_TO_SLEEP_ACK	0x31
#define HCILL_WAKE_UP_IND	0x32
#define HCILL_WAKE_UP_ACK	0x33

 
enum hcill_states_e {
	HCILL_ASLEEP,
	HCILL_ASLEEP_TO_AWAKE,
	HCILL_AWAKE,
	HCILL_AWAKE_TO_ASLEEP
};

struct ll_device {
	struct hci_uart hu;
	struct serdev_device *serdev;
	struct gpio_desc *enable_gpio;
	struct clk *ext_clk;
	bdaddr_t bdaddr;
};

struct ll_struct {
	struct sk_buff *rx_skb;
	struct sk_buff_head txq;
	spinlock_t hcill_lock;		 
	unsigned long hcill_state;	 
	struct sk_buff_head tx_wait_q;	 
};

 
static int send_hcill_cmd(u8 cmd, struct hci_uart *hu)
{
	int err = 0;
	struct sk_buff *skb = NULL;
	struct ll_struct *ll = hu->priv;

	BT_DBG("hu %p cmd 0x%x", hu, cmd);

	 
	skb = bt_skb_alloc(1, GFP_ATOMIC);
	if (!skb) {
		BT_ERR("cannot allocate memory for HCILL packet");
		err = -ENOMEM;
		goto out;
	}

	 
	skb_put_u8(skb, cmd);

	 
	skb_queue_tail(&ll->txq, skb);
out:
	return err;
}

 
static int ll_open(struct hci_uart *hu)
{
	struct ll_struct *ll;

	BT_DBG("hu %p", hu);

	ll = kzalloc(sizeof(*ll), GFP_KERNEL);
	if (!ll)
		return -ENOMEM;

	skb_queue_head_init(&ll->txq);
	skb_queue_head_init(&ll->tx_wait_q);
	spin_lock_init(&ll->hcill_lock);

	ll->hcill_state = HCILL_AWAKE;

	hu->priv = ll;

	if (hu->serdev) {
		struct ll_device *lldev = serdev_device_get_drvdata(hu->serdev);

		if (!IS_ERR(lldev->ext_clk))
			clk_prepare_enable(lldev->ext_clk);
	}

	return 0;
}

 
static int ll_flush(struct hci_uart *hu)
{
	struct ll_struct *ll = hu->priv;

	BT_DBG("hu %p", hu);

	skb_queue_purge(&ll->tx_wait_q);
	skb_queue_purge(&ll->txq);

	return 0;
}

 
static int ll_close(struct hci_uart *hu)
{
	struct ll_struct *ll = hu->priv;

	BT_DBG("hu %p", hu);

	skb_queue_purge(&ll->tx_wait_q);
	skb_queue_purge(&ll->txq);

	kfree_skb(ll->rx_skb);

	if (hu->serdev) {
		struct ll_device *lldev = serdev_device_get_drvdata(hu->serdev);

		gpiod_set_value_cansleep(lldev->enable_gpio, 0);

		clk_disable_unprepare(lldev->ext_clk);
	}

	hu->priv = NULL;

	kfree(ll);

	return 0;
}

 
static void __ll_do_awake(struct ll_struct *ll)
{
	struct sk_buff *skb = NULL;

	while ((skb = skb_dequeue(&ll->tx_wait_q)))
		skb_queue_tail(&ll->txq, skb);

	ll->hcill_state = HCILL_AWAKE;
}

 
static void ll_device_want_to_wakeup(struct hci_uart *hu)
{
	unsigned long flags;
	struct ll_struct *ll = hu->priv;

	BT_DBG("hu %p", hu);

	 
	spin_lock_irqsave(&ll->hcill_lock, flags);

	switch (ll->hcill_state) {
	case HCILL_ASLEEP_TO_AWAKE:
		 
		BT_DBG("dual wake-up-indication");
		fallthrough;
	case HCILL_ASLEEP:
		 
		if (send_hcill_cmd(HCILL_WAKE_UP_ACK, hu) < 0) {
			BT_ERR("cannot acknowledge device wake up");
			goto out;
		}
		break;
	default:
		 
		BT_ERR("received HCILL_WAKE_UP_IND in state %ld",
		       ll->hcill_state);
		break;
	}

	 
	__ll_do_awake(ll);

out:
	spin_unlock_irqrestore(&ll->hcill_lock, flags);

	 
	hci_uart_tx_wakeup(hu);
}

 
static void ll_device_want_to_sleep(struct hci_uart *hu)
{
	unsigned long flags;
	struct ll_struct *ll = hu->priv;

	BT_DBG("hu %p", hu);

	 
	spin_lock_irqsave(&ll->hcill_lock, flags);

	 
	if (ll->hcill_state != HCILL_AWAKE)
		BT_ERR("ERR: HCILL_GO_TO_SLEEP_IND in state %ld",
		       ll->hcill_state);

	 
	if (send_hcill_cmd(HCILL_GO_TO_SLEEP_ACK, hu) < 0) {
		BT_ERR("cannot acknowledge device sleep");
		goto out;
	}

	 
	ll->hcill_state = HCILL_ASLEEP;

out:
	spin_unlock_irqrestore(&ll->hcill_lock, flags);

	 
	hci_uart_tx_wakeup(hu);
}

 
static void ll_device_woke_up(struct hci_uart *hu)
{
	unsigned long flags;
	struct ll_struct *ll = hu->priv;

	BT_DBG("hu %p", hu);

	 
	spin_lock_irqsave(&ll->hcill_lock, flags);

	 
	if (ll->hcill_state != HCILL_ASLEEP_TO_AWAKE)
		BT_ERR("received HCILL_WAKE_UP_ACK in state %ld",
		       ll->hcill_state);

	 
	__ll_do_awake(ll);

	spin_unlock_irqrestore(&ll->hcill_lock, flags);

	 
	hci_uart_tx_wakeup(hu);
}

 
 
static int ll_enqueue(struct hci_uart *hu, struct sk_buff *skb)
{
	unsigned long flags = 0;
	struct ll_struct *ll = hu->priv;

	BT_DBG("hu %p skb %p", hu, skb);

	 
	memcpy(skb_push(skb, 1), &hci_skb_pkt_type(skb), 1);

	 
	spin_lock_irqsave(&ll->hcill_lock, flags);

	 
	switch (ll->hcill_state) {
	case HCILL_AWAKE:
		BT_DBG("device awake, sending normally");
		skb_queue_tail(&ll->txq, skb);
		break;
	case HCILL_ASLEEP:
		BT_DBG("device asleep, waking up and queueing packet");
		 
		skb_queue_tail(&ll->tx_wait_q, skb);
		 
		if (send_hcill_cmd(HCILL_WAKE_UP_IND, hu) < 0) {
			BT_ERR("cannot wake up device");
			break;
		}
		ll->hcill_state = HCILL_ASLEEP_TO_AWAKE;
		break;
	case HCILL_ASLEEP_TO_AWAKE:
		BT_DBG("device waking up, queueing packet");
		 
		skb_queue_tail(&ll->tx_wait_q, skb);
		break;
	default:
		BT_ERR("illegal hcill state: %ld (losing packet)",
		       ll->hcill_state);
		dev_kfree_skb_irq(skb);
		break;
	}

	spin_unlock_irqrestore(&ll->hcill_lock, flags);

	return 0;
}

static int ll_recv_frame(struct hci_dev *hdev, struct sk_buff *skb)
{
	struct hci_uart *hu = hci_get_drvdata(hdev);
	struct ll_struct *ll = hu->priv;

	switch (hci_skb_pkt_type(skb)) {
	case HCILL_GO_TO_SLEEP_IND:
		BT_DBG("HCILL_GO_TO_SLEEP_IND packet");
		ll_device_want_to_sleep(hu);
		break;
	case HCILL_GO_TO_SLEEP_ACK:
		 
		bt_dev_err(hdev, "received HCILL_GO_TO_SLEEP_ACK in state %ld",
			   ll->hcill_state);
		break;
	case HCILL_WAKE_UP_IND:
		BT_DBG("HCILL_WAKE_UP_IND packet");
		ll_device_want_to_wakeup(hu);
		break;
	case HCILL_WAKE_UP_ACK:
		BT_DBG("HCILL_WAKE_UP_ACK packet");
		ll_device_woke_up(hu);
		break;
	}

	kfree_skb(skb);
	return 0;
}

#define LL_RECV_SLEEP_IND \
	.type = HCILL_GO_TO_SLEEP_IND, \
	.hlen = 0, \
	.loff = 0, \
	.lsize = 0, \
	.maxlen = 0

#define LL_RECV_SLEEP_ACK \
	.type = HCILL_GO_TO_SLEEP_ACK, \
	.hlen = 0, \
	.loff = 0, \
	.lsize = 0, \
	.maxlen = 0

#define LL_RECV_WAKE_IND \
	.type = HCILL_WAKE_UP_IND, \
	.hlen = 0, \
	.loff = 0, \
	.lsize = 0, \
	.maxlen = 0

#define LL_RECV_WAKE_ACK \
	.type = HCILL_WAKE_UP_ACK, \
	.hlen = 0, \
	.loff = 0, \
	.lsize = 0, \
	.maxlen = 0

static const struct h4_recv_pkt ll_recv_pkts[] = {
	{ H4_RECV_ACL,       .recv = hci_recv_frame },
	{ H4_RECV_SCO,       .recv = hci_recv_frame },
	{ H4_RECV_EVENT,     .recv = hci_recv_frame },
	{ LL_RECV_SLEEP_IND, .recv = ll_recv_frame  },
	{ LL_RECV_SLEEP_ACK, .recv = ll_recv_frame  },
	{ LL_RECV_WAKE_IND,  .recv = ll_recv_frame  },
	{ LL_RECV_WAKE_ACK,  .recv = ll_recv_frame  },
};

 
static int ll_recv(struct hci_uart *hu, const void *data, int count)
{
	struct ll_struct *ll = hu->priv;

	if (!test_bit(HCI_UART_REGISTERED, &hu->flags))
		return -EUNATCH;

	ll->rx_skb = h4_recv_buf(hu->hdev, ll->rx_skb, data, count,
				 ll_recv_pkts, ARRAY_SIZE(ll_recv_pkts));
	if (IS_ERR(ll->rx_skb)) {
		int err = PTR_ERR(ll->rx_skb);
		bt_dev_err(hu->hdev, "Frame reassembly failed (%d)", err);
		ll->rx_skb = NULL;
		return err;
	}

	return count;
}

static struct sk_buff *ll_dequeue(struct hci_uart *hu)
{
	struct ll_struct *ll = hu->priv;

	return skb_dequeue(&ll->txq);
}

#if IS_ENABLED(CONFIG_SERIAL_DEV_BUS)
static int read_local_version(struct hci_dev *hdev)
{
	int err = 0;
	unsigned short version = 0;
	struct sk_buff *skb;
	struct hci_rp_read_local_version *ver;

	skb = __hci_cmd_sync(hdev, HCI_OP_READ_LOCAL_VERSION, 0, NULL,
			     HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		bt_dev_err(hdev, "Reading TI version information failed (%ld)",
			   PTR_ERR(skb));
		return PTR_ERR(skb);
	}
	if (skb->len != sizeof(*ver)) {
		err = -EILSEQ;
		goto out;
	}

	ver = (struct hci_rp_read_local_version *)skb->data;
	if (le16_to_cpu(ver->manufacturer) != 13) {
		err = -ENODEV;
		goto out;
	}

	version = le16_to_cpu(ver->lmp_subver);

out:
	if (err)
		bt_dev_err(hdev, "Failed to read TI version info: %d", err);
	kfree_skb(skb);
	return err ? err : version;
}

static int send_command_from_firmware(struct ll_device *lldev,
				      struct hci_command *cmd)
{
	struct sk_buff *skb;

	if (cmd->opcode == HCI_VS_UPDATE_UART_HCI_BAUDRATE) {
		 
		bt_dev_warn(lldev->hu.hdev,
			    "change remote baud rate command in firmware");
		return 0;
	}
	if (cmd->prefix != 1)
		bt_dev_dbg(lldev->hu.hdev, "command type %d", cmd->prefix);

	skb = __hci_cmd_sync(lldev->hu.hdev, cmd->opcode, cmd->plen,
			     &cmd->speed, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		bt_dev_err(lldev->hu.hdev, "send command failed");
		return PTR_ERR(skb);
	}
	kfree_skb(skb);
	return 0;
}

 
static int download_firmware(struct ll_device *lldev)
{
	unsigned short chip, min_ver, maj_ver;
	int version, err, len;
	unsigned char *ptr, *action_ptr;
	unsigned char bts_scr_name[40];	 
	const struct firmware *fw;
	struct hci_command *cmd;

	version = read_local_version(lldev->hu.hdev);
	if (version < 0)
		return version;

	chip = (version & 0x7C00) >> 10;
	min_ver = (version & 0x007F);
	maj_ver = (version & 0x0380) >> 7;
	if (version & 0x8000)
		maj_ver |= 0x0008;

	snprintf(bts_scr_name, sizeof(bts_scr_name),
		 "ti-connectivity/TIInit_%d.%d.%d.bts",
		 chip, maj_ver, min_ver);

	err = request_firmware(&fw, bts_scr_name, &lldev->serdev->dev);
	if (err || !fw->data || !fw->size) {
		bt_dev_err(lldev->hu.hdev, "request_firmware failed(errno %d) for %s",
			   err, bts_scr_name);
		return -EINVAL;
	}
	ptr = (void *)fw->data;
	len = fw->size;
	 
	ptr += sizeof(struct bts_header);
	len -= sizeof(struct bts_header);

	while (len > 0 && ptr) {
		bt_dev_dbg(lldev->hu.hdev, " action size %d, type %d ",
			   ((struct bts_action *)ptr)->size,
			   ((struct bts_action *)ptr)->type);

		action_ptr = &(((struct bts_action *)ptr)->data[0]);

		switch (((struct bts_action *)ptr)->type) {
		case ACTION_SEND_COMMAND:	 
			bt_dev_dbg(lldev->hu.hdev, "S");
			cmd = (struct hci_command *)action_ptr;
			err = send_command_from_firmware(lldev, cmd);
			if (err)
				goto out_rel_fw;
			break;
		case ACTION_WAIT_EVENT:   
			 
			bt_dev_dbg(lldev->hu.hdev, "W");
			break;
		case ACTION_DELAY:	 
			bt_dev_info(lldev->hu.hdev, "sleep command in scr");
			msleep(((struct bts_action_delay *)action_ptr)->msec);
			break;
		}
		len -= (sizeof(struct bts_action) +
			((struct bts_action *)ptr)->size);
		ptr += sizeof(struct bts_action) +
			((struct bts_action *)ptr)->size;
	}

out_rel_fw:
	 
	release_firmware(fw);
	return err;
}

static int ll_set_bdaddr(struct hci_dev *hdev, const bdaddr_t *bdaddr)
{
	bdaddr_t bdaddr_swapped;
	struct sk_buff *skb;

	 
	baswap(&bdaddr_swapped, bdaddr);
	skb = __hci_cmd_sync(hdev, HCI_VS_WRITE_BD_ADDR, sizeof(bdaddr_t),
			     &bdaddr_swapped, HCI_INIT_TIMEOUT);
	if (!IS_ERR(skb))
		kfree_skb(skb);

	return PTR_ERR_OR_ZERO(skb);
}

static int ll_setup(struct hci_uart *hu)
{
	int err, retry = 3;
	struct ll_device *lldev;
	struct serdev_device *serdev = hu->serdev;
	u32 speed;

	if (!serdev)
		return 0;

	lldev = serdev_device_get_drvdata(serdev);

	hu->hdev->set_bdaddr = ll_set_bdaddr;

	serdev_device_set_flow_control(serdev, true);

	do {
		 
		gpiod_set_value_cansleep(lldev->enable_gpio, 0);
		msleep(5);
		gpiod_set_value_cansleep(lldev->enable_gpio, 1);
		mdelay(100);
		err = serdev_device_wait_for_cts(serdev, true, 200);
		if (err) {
			bt_dev_err(hu->hdev, "Failed to get CTS");
			return err;
		}

		err = download_firmware(lldev);
		if (!err)
			break;

		 
		bt_dev_err(hu->hdev, "download firmware failed, retrying...");
	} while (retry--);

	if (err)
		return err;

	 
	if (!bacmp(&lldev->bdaddr, BDADDR_NONE)) {
		 
		set_bit(HCI_QUIRK_INVALID_BDADDR, &hu->hdev->quirks);
	} else if (bacmp(&lldev->bdaddr, BDADDR_ANY)) {
		err = ll_set_bdaddr(hu->hdev, &lldev->bdaddr);
		if (err)
			set_bit(HCI_QUIRK_INVALID_BDADDR, &hu->hdev->quirks);
	}

	 
	if (hu->oper_speed)
		speed = hu->oper_speed;
	else if (hu->proto->oper_speed)
		speed = hu->proto->oper_speed;
	else
		speed = 0;

	if (speed) {
		__le32 speed_le = cpu_to_le32(speed);
		struct sk_buff *skb;

		skb = __hci_cmd_sync(hu->hdev, HCI_VS_UPDATE_UART_HCI_BAUDRATE,
				     sizeof(speed_le), &speed_le,
				     HCI_INIT_TIMEOUT);
		if (!IS_ERR(skb)) {
			kfree_skb(skb);
			serdev_device_set_baudrate(serdev, speed);
		}
	}

	return 0;
}

static const struct hci_uart_proto llp;

static int hci_ti_probe(struct serdev_device *serdev)
{
	struct hci_uart *hu;
	struct ll_device *lldev;
	struct nvmem_cell *bdaddr_cell;
	u32 max_speed = 3000000;

	lldev = devm_kzalloc(&serdev->dev, sizeof(struct ll_device), GFP_KERNEL);
	if (!lldev)
		return -ENOMEM;
	hu = &lldev->hu;

	serdev_device_set_drvdata(serdev, lldev);
	lldev->serdev = hu->serdev = serdev;

	lldev->enable_gpio = devm_gpiod_get_optional(&serdev->dev,
						     "enable",
						     GPIOD_OUT_LOW);
	if (IS_ERR(lldev->enable_gpio))
		return PTR_ERR(lldev->enable_gpio);

	lldev->ext_clk = devm_clk_get(&serdev->dev, "ext_clock");
	if (IS_ERR(lldev->ext_clk) && PTR_ERR(lldev->ext_clk) != -ENOENT)
		return PTR_ERR(lldev->ext_clk);

	of_property_read_u32(serdev->dev.of_node, "max-speed", &max_speed);
	hci_uart_set_speeds(hu, 115200, max_speed);

	 
	bdaddr_cell = nvmem_cell_get(&serdev->dev, "bd-address");
	if (IS_ERR(bdaddr_cell)) {
		int err = PTR_ERR(bdaddr_cell);

		if (err == -EPROBE_DEFER)
			return err;

		 
		if (err != -ENOENT && err != -ENOSYS) {
			 
			dev_warn(&serdev->dev,
				 "Failed to get \"bd-address\" nvmem cell (%d)\n",
				 err);
			bacpy(&lldev->bdaddr, BDADDR_NONE);
		}
	} else {
		bdaddr_t *bdaddr;
		size_t len;

		bdaddr = nvmem_cell_read(bdaddr_cell, &len);
		nvmem_cell_put(bdaddr_cell);
		if (IS_ERR(bdaddr)) {
			dev_err(&serdev->dev, "Failed to read nvmem bd-address\n");
			return PTR_ERR(bdaddr);
		}
		if (len != sizeof(bdaddr_t)) {
			dev_err(&serdev->dev, "Invalid nvmem bd-address length\n");
			kfree(bdaddr);
			return -EINVAL;
		}

		 
		baswap(&lldev->bdaddr, bdaddr);
		kfree(bdaddr);
	}

	return hci_uart_register_device(hu, &llp);
}

static void hci_ti_remove(struct serdev_device *serdev)
{
	struct ll_device *lldev = serdev_device_get_drvdata(serdev);

	hci_uart_unregister_device(&lldev->hu);
}

static const struct of_device_id hci_ti_of_match[] = {
	{ .compatible = "ti,cc2560" },
	{ .compatible = "ti,wl1271-st" },
	{ .compatible = "ti,wl1273-st" },
	{ .compatible = "ti,wl1281-st" },
	{ .compatible = "ti,wl1283-st" },
	{ .compatible = "ti,wl1285-st" },
	{ .compatible = "ti,wl1801-st" },
	{ .compatible = "ti,wl1805-st" },
	{ .compatible = "ti,wl1807-st" },
	{ .compatible = "ti,wl1831-st" },
	{ .compatible = "ti,wl1835-st" },
	{ .compatible = "ti,wl1837-st" },
	{},
};
MODULE_DEVICE_TABLE(of, hci_ti_of_match);

static struct serdev_device_driver hci_ti_drv = {
	.driver		= {
		.name	= "hci-ti",
		.of_match_table = hci_ti_of_match,
	},
	.probe	= hci_ti_probe,
	.remove	= hci_ti_remove,
};
#else
#define ll_setup NULL
#endif

static const struct hci_uart_proto llp = {
	.id		= HCI_UART_LL,
	.name		= "LL",
	.setup		= ll_setup,
	.open		= ll_open,
	.close		= ll_close,
	.recv		= ll_recv,
	.enqueue	= ll_enqueue,
	.dequeue	= ll_dequeue,
	.flush		= ll_flush,
};

int __init ll_init(void)
{
	serdev_device_driver_register(&hci_ti_drv);

	return hci_uart_register_proto(&llp);
}

int __exit ll_deinit(void)
{
	serdev_device_driver_unregister(&hci_ti_drv);

	return hci_uart_unregister_proto(&llp);
}
