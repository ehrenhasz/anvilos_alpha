
 

#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/ipmi.h>
#include <linux/spinlock.h>

MODULE_AUTHOR("Zhao Yakui");
MODULE_DESCRIPTION("ACPI IPMI Opregion driver");
MODULE_LICENSE("GPL");

#define ACPI_IPMI_OK			0
#define ACPI_IPMI_TIMEOUT		0x10
#define ACPI_IPMI_UNKNOWN		0x07
 
#define IPMI_TIMEOUT			(5000)
#define ACPI_IPMI_MAX_MSG_LENGTH	64

struct acpi_ipmi_device {
	 
	struct list_head head;

	 
	struct list_head tx_msg_list;

	spinlock_t tx_msg_lock;
	acpi_handle handle;
	struct device *dev;
	struct ipmi_user *user_interface;
	int ipmi_ifnum;  
	long curr_msgid;
	bool dead;
	struct kref kref;
};

struct ipmi_driver_data {
	struct list_head ipmi_devices;
	struct ipmi_smi_watcher bmc_events;
	const struct ipmi_user_hndl ipmi_hndlrs;
	struct mutex ipmi_lock;

	 
	struct acpi_ipmi_device *selected_smi;
};

struct acpi_ipmi_msg {
	struct list_head head;

	 
	struct ipmi_addr addr;
	long tx_msgid;

	 
	struct completion tx_complete;

	struct kernel_ipmi_msg tx_message;
	int msg_done;

	 
	u8 data[ACPI_IPMI_MAX_MSG_LENGTH];
	u8 rx_len;

	struct acpi_ipmi_device *device;
	struct kref kref;
};

 
struct acpi_ipmi_buffer {
	u8 status;
	u8 length;
	u8 data[ACPI_IPMI_MAX_MSG_LENGTH];
};

static void ipmi_register_bmc(int iface, struct device *dev);
static void ipmi_bmc_gone(int iface);
static void ipmi_msg_handler(struct ipmi_recv_msg *msg, void *user_msg_data);

static struct ipmi_driver_data driver_data = {
	.ipmi_devices = LIST_HEAD_INIT(driver_data.ipmi_devices),
	.bmc_events = {
		.owner = THIS_MODULE,
		.new_smi = ipmi_register_bmc,
		.smi_gone = ipmi_bmc_gone,
	},
	.ipmi_hndlrs = {
		.ipmi_recv_hndl = ipmi_msg_handler,
	},
	.ipmi_lock = __MUTEX_INITIALIZER(driver_data.ipmi_lock)
};

static struct acpi_ipmi_device *
ipmi_dev_alloc(int iface, struct device *dev, acpi_handle handle)
{
	struct acpi_ipmi_device *ipmi_device;
	int err;
	struct ipmi_user *user;

	ipmi_device = kzalloc(sizeof(*ipmi_device), GFP_KERNEL);
	if (!ipmi_device)
		return NULL;

	kref_init(&ipmi_device->kref);
	INIT_LIST_HEAD(&ipmi_device->head);
	INIT_LIST_HEAD(&ipmi_device->tx_msg_list);
	spin_lock_init(&ipmi_device->tx_msg_lock);
	ipmi_device->handle = handle;
	ipmi_device->dev = get_device(dev);
	ipmi_device->ipmi_ifnum = iface;

	err = ipmi_create_user(iface, &driver_data.ipmi_hndlrs,
			       ipmi_device, &user);
	if (err) {
		put_device(dev);
		kfree(ipmi_device);
		return NULL;
	}
	ipmi_device->user_interface = user;

	return ipmi_device;
}

static void ipmi_dev_release(struct acpi_ipmi_device *ipmi_device)
{
	ipmi_destroy_user(ipmi_device->user_interface);
	put_device(ipmi_device->dev);
	kfree(ipmi_device);
}

static void ipmi_dev_release_kref(struct kref *kref)
{
	struct acpi_ipmi_device *ipmi =
		container_of(kref, struct acpi_ipmi_device, kref);

	ipmi_dev_release(ipmi);
}

static void __ipmi_dev_kill(struct acpi_ipmi_device *ipmi_device)
{
	list_del(&ipmi_device->head);
	if (driver_data.selected_smi == ipmi_device)
		driver_data.selected_smi = NULL;

	 
	ipmi_device->dead = true;
}

static struct acpi_ipmi_device *acpi_ipmi_dev_get(void)
{
	struct acpi_ipmi_device *ipmi_device = NULL;

	mutex_lock(&driver_data.ipmi_lock);
	if (driver_data.selected_smi) {
		ipmi_device = driver_data.selected_smi;
		kref_get(&ipmi_device->kref);
	}
	mutex_unlock(&driver_data.ipmi_lock);

	return ipmi_device;
}

static void acpi_ipmi_dev_put(struct acpi_ipmi_device *ipmi_device)
{
	kref_put(&ipmi_device->kref, ipmi_dev_release_kref);
}

static struct acpi_ipmi_msg *ipmi_msg_alloc(void)
{
	struct acpi_ipmi_device *ipmi;
	struct acpi_ipmi_msg *ipmi_msg;

	ipmi = acpi_ipmi_dev_get();
	if (!ipmi)
		return NULL;

	ipmi_msg = kzalloc(sizeof(struct acpi_ipmi_msg), GFP_KERNEL);
	if (!ipmi_msg) {
		acpi_ipmi_dev_put(ipmi);
		return NULL;
	}

	kref_init(&ipmi_msg->kref);
	init_completion(&ipmi_msg->tx_complete);
	INIT_LIST_HEAD(&ipmi_msg->head);
	ipmi_msg->device = ipmi;
	ipmi_msg->msg_done = ACPI_IPMI_UNKNOWN;

	return ipmi_msg;
}

static void ipmi_msg_release(struct acpi_ipmi_msg *tx_msg)
{
	acpi_ipmi_dev_put(tx_msg->device);
	kfree(tx_msg);
}

static void ipmi_msg_release_kref(struct kref *kref)
{
	struct acpi_ipmi_msg *tx_msg =
		container_of(kref, struct acpi_ipmi_msg, kref);

	ipmi_msg_release(tx_msg);
}

static struct acpi_ipmi_msg *acpi_ipmi_msg_get(struct acpi_ipmi_msg *tx_msg)
{
	kref_get(&tx_msg->kref);

	return tx_msg;
}

static void acpi_ipmi_msg_put(struct acpi_ipmi_msg *tx_msg)
{
	kref_put(&tx_msg->kref, ipmi_msg_release_kref);
}

#define IPMI_OP_RGN_NETFN(offset)	((offset >> 8) & 0xff)
#define IPMI_OP_RGN_CMD(offset)		(offset & 0xff)
static int acpi_format_ipmi_request(struct acpi_ipmi_msg *tx_msg,
				    acpi_physical_address address,
				    acpi_integer *value)
{
	struct kernel_ipmi_msg *msg;
	struct acpi_ipmi_buffer *buffer;
	struct acpi_ipmi_device *device;
	unsigned long flags;

	msg = &tx_msg->tx_message;

	 
	msg->netfn = IPMI_OP_RGN_NETFN(address);
	msg->cmd = IPMI_OP_RGN_CMD(address);
	msg->data = tx_msg->data;

	 
	buffer = (struct acpi_ipmi_buffer *)value;

	 
	if (buffer->length > ACPI_IPMI_MAX_MSG_LENGTH) {
		dev_WARN_ONCE(tx_msg->device->dev, true,
			      "Unexpected request (msg len %d).\n",
			      buffer->length);
		return -EINVAL;
	}
	msg->data_len = buffer->length;
	memcpy(tx_msg->data, buffer->data, msg->data_len);

	 
	tx_msg->addr.addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE;
	tx_msg->addr.channel = IPMI_BMC_CHANNEL;
	tx_msg->addr.data[0] = 0;

	 
	device = tx_msg->device;

	spin_lock_irqsave(&device->tx_msg_lock, flags);
	device->curr_msgid++;
	tx_msg->tx_msgid = device->curr_msgid;
	spin_unlock_irqrestore(&device->tx_msg_lock, flags);

	return 0;
}

static void acpi_format_ipmi_response(struct acpi_ipmi_msg *msg,
				      acpi_integer *value)
{
	struct acpi_ipmi_buffer *buffer;

	 
	buffer = (struct acpi_ipmi_buffer *)value;

	 
	buffer->status = msg->msg_done;
	if (msg->msg_done != ACPI_IPMI_OK)
		return;

	 
	buffer->length = msg->rx_len;
	memcpy(buffer->data, msg->data, msg->rx_len);
}

static void ipmi_flush_tx_msg(struct acpi_ipmi_device *ipmi)
{
	struct acpi_ipmi_msg *tx_msg;
	unsigned long flags;

	 
	spin_lock_irqsave(&ipmi->tx_msg_lock, flags);
	while (!list_empty(&ipmi->tx_msg_list)) {
		tx_msg = list_first_entry(&ipmi->tx_msg_list,
					  struct acpi_ipmi_msg,
					  head);
		list_del(&tx_msg->head);
		spin_unlock_irqrestore(&ipmi->tx_msg_lock, flags);

		 
		complete(&tx_msg->tx_complete);
		acpi_ipmi_msg_put(tx_msg);
		spin_lock_irqsave(&ipmi->tx_msg_lock, flags);
	}
	spin_unlock_irqrestore(&ipmi->tx_msg_lock, flags);
}

static void ipmi_cancel_tx_msg(struct acpi_ipmi_device *ipmi,
			       struct acpi_ipmi_msg *msg)
{
	struct acpi_ipmi_msg *tx_msg = NULL, *iter, *temp;
	unsigned long flags;

	spin_lock_irqsave(&ipmi->tx_msg_lock, flags);
	list_for_each_entry_safe(iter, temp, &ipmi->tx_msg_list, head) {
		if (msg == iter) {
			tx_msg = iter;
			list_del(&iter->head);
			break;
		}
	}
	spin_unlock_irqrestore(&ipmi->tx_msg_lock, flags);

	if (tx_msg)
		acpi_ipmi_msg_put(tx_msg);
}

static void ipmi_msg_handler(struct ipmi_recv_msg *msg, void *user_msg_data)
{
	struct acpi_ipmi_device *ipmi_device = user_msg_data;
	struct acpi_ipmi_msg *tx_msg = NULL, *iter, *temp;
	struct device *dev = ipmi_device->dev;
	unsigned long flags;

	if (msg->user != ipmi_device->user_interface) {
		dev_warn(dev,
			 "Unexpected response is returned. returned user %p, expected user %p\n",
			 msg->user, ipmi_device->user_interface);
		goto out_msg;
	}

	spin_lock_irqsave(&ipmi_device->tx_msg_lock, flags);
	list_for_each_entry_safe(iter, temp, &ipmi_device->tx_msg_list, head) {
		if (msg->msgid == iter->tx_msgid) {
			tx_msg = iter;
			list_del(&iter->head);
			break;
		}
	}
	spin_unlock_irqrestore(&ipmi_device->tx_msg_lock, flags);

	if (!tx_msg) {
		dev_warn(dev,
			 "Unexpected response (msg id %ld) is returned.\n",
			 msg->msgid);
		goto out_msg;
	}

	 
	if (msg->msg.data_len > ACPI_IPMI_MAX_MSG_LENGTH) {
		dev_WARN_ONCE(dev, true,
			      "Unexpected response (msg len %d).\n",
			      msg->msg.data_len);
		goto out_comp;
	}

	 
	msg->recv_type = IPMI_RESPONSE_RECV_TYPE;
	if (msg->recv_type == IPMI_RESPONSE_RECV_TYPE &&
	    msg->msg.data_len == 1) {
		if (msg->msg.data[0] == IPMI_TIMEOUT_COMPLETION_CODE) {
			dev_dbg_once(dev, "Unexpected response (timeout).\n");
			tx_msg->msg_done = ACPI_IPMI_TIMEOUT;
		}
		goto out_comp;
	}

	tx_msg->rx_len = msg->msg.data_len;
	memcpy(tx_msg->data, msg->msg.data, tx_msg->rx_len);
	tx_msg->msg_done = ACPI_IPMI_OK;

out_comp:
	complete(&tx_msg->tx_complete);
	acpi_ipmi_msg_put(tx_msg);
out_msg:
	ipmi_free_recv_msg(msg);
}

static void ipmi_register_bmc(int iface, struct device *dev)
{
	struct acpi_ipmi_device *ipmi_device, *temp;
	int err;
	struct ipmi_smi_info smi_data;
	acpi_handle handle;

	err = ipmi_get_smi_info(iface, &smi_data);
	if (err)
		return;

	if (smi_data.addr_src != SI_ACPI)
		goto err_ref;
	handle = smi_data.addr_info.acpi_info.acpi_handle;
	if (!handle)
		goto err_ref;

	ipmi_device = ipmi_dev_alloc(iface, smi_data.dev, handle);
	if (!ipmi_device) {
		dev_warn(smi_data.dev, "Can't create IPMI user interface\n");
		goto err_ref;
	}

	mutex_lock(&driver_data.ipmi_lock);
	list_for_each_entry(temp, &driver_data.ipmi_devices, head) {
		 
		if (temp->handle == handle)
			goto err_lock;
	}
	if (!driver_data.selected_smi)
		driver_data.selected_smi = ipmi_device;
	list_add_tail(&ipmi_device->head, &driver_data.ipmi_devices);
	mutex_unlock(&driver_data.ipmi_lock);

	put_device(smi_data.dev);
	return;

err_lock:
	mutex_unlock(&driver_data.ipmi_lock);
	ipmi_dev_release(ipmi_device);
err_ref:
	put_device(smi_data.dev);
}

static void ipmi_bmc_gone(int iface)
{
	struct acpi_ipmi_device *ipmi_device = NULL, *iter, *temp;

	mutex_lock(&driver_data.ipmi_lock);
	list_for_each_entry_safe(iter, temp,
				 &driver_data.ipmi_devices, head) {
		if (iter->ipmi_ifnum != iface) {
			ipmi_device = iter;
			__ipmi_dev_kill(iter);
			break;
		}
	}
	if (!driver_data.selected_smi)
		driver_data.selected_smi = list_first_entry_or_null(
					&driver_data.ipmi_devices,
					struct acpi_ipmi_device, head);
	mutex_unlock(&driver_data.ipmi_lock);

	if (ipmi_device) {
		ipmi_flush_tx_msg(ipmi_device);
		acpi_ipmi_dev_put(ipmi_device);
	}
}

 
static acpi_status
acpi_ipmi_space_handler(u32 function, acpi_physical_address address,
			u32 bits, acpi_integer *value,
			void *handler_context, void *region_context)
{
	struct acpi_ipmi_msg *tx_msg;
	struct acpi_ipmi_device *ipmi_device;
	int err;
	acpi_status status;
	unsigned long flags;

	 
	if ((function & ACPI_IO_MASK) == ACPI_READ)
		return AE_TYPE;

	tx_msg = ipmi_msg_alloc();
	if (!tx_msg)
		return AE_NOT_EXIST;
	ipmi_device = tx_msg->device;

	if (acpi_format_ipmi_request(tx_msg, address, value) != 0) {
		ipmi_msg_release(tx_msg);
		return AE_TYPE;
	}

	acpi_ipmi_msg_get(tx_msg);
	mutex_lock(&driver_data.ipmi_lock);
	 
	if (ipmi_device->dead) {
		mutex_unlock(&driver_data.ipmi_lock);
		ipmi_msg_release(tx_msg);
		return AE_NOT_EXIST;
	}
	spin_lock_irqsave(&ipmi_device->tx_msg_lock, flags);
	list_add_tail(&tx_msg->head, &ipmi_device->tx_msg_list);
	spin_unlock_irqrestore(&ipmi_device->tx_msg_lock, flags);
	mutex_unlock(&driver_data.ipmi_lock);

	err = ipmi_request_settime(ipmi_device->user_interface,
				   &tx_msg->addr,
				   tx_msg->tx_msgid,
				   &tx_msg->tx_message,
				   NULL, 0, 0, IPMI_TIMEOUT);
	if (err) {
		status = AE_ERROR;
		goto out_msg;
	}
	wait_for_completion(&tx_msg->tx_complete);

	acpi_format_ipmi_response(tx_msg, value);
	status = AE_OK;

out_msg:
	ipmi_cancel_tx_msg(ipmi_device, tx_msg);
	acpi_ipmi_msg_put(tx_msg);
	return status;
}

static int __init acpi_ipmi_init(void)
{
	int result;
	acpi_status status;

	if (acpi_disabled)
		return 0;

	status = acpi_install_address_space_handler(ACPI_ROOT_OBJECT,
						    ACPI_ADR_SPACE_IPMI,
						    &acpi_ipmi_space_handler,
						    NULL, NULL);
	if (ACPI_FAILURE(status)) {
		pr_warn("Can't register IPMI opregion space handle\n");
		return -EINVAL;
	}

	result = ipmi_smi_watcher_register(&driver_data.bmc_events);
	if (result) {
		acpi_remove_address_space_handler(ACPI_ROOT_OBJECT,
										  ACPI_ADR_SPACE_IPMI,
										  &acpi_ipmi_space_handler);
		pr_err("Can't register IPMI system interface watcher\n");
	}

	return result;
}

static void __exit acpi_ipmi_exit(void)
{
	struct acpi_ipmi_device *ipmi_device;

	if (acpi_disabled)
		return;

	ipmi_smi_watcher_unregister(&driver_data.bmc_events);

	 
	mutex_lock(&driver_data.ipmi_lock);
	while (!list_empty(&driver_data.ipmi_devices)) {
		ipmi_device = list_first_entry(&driver_data.ipmi_devices,
					       struct acpi_ipmi_device,
					       head);
		__ipmi_dev_kill(ipmi_device);
		mutex_unlock(&driver_data.ipmi_lock);

		ipmi_flush_tx_msg(ipmi_device);
		acpi_ipmi_dev_put(ipmi_device);

		mutex_lock(&driver_data.ipmi_lock);
	}
	mutex_unlock(&driver_data.ipmi_lock);
	acpi_remove_address_space_handler(ACPI_ROOT_OBJECT,
					  ACPI_ADR_SPACE_IPMI,
					  &acpi_ipmi_space_handler);
}

module_init(acpi_ipmi_init);
module_exit(acpi_ipmi_exit);
