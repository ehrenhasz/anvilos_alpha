
 

#include <linux/property.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include <net/bluetooth/mgmt.h>

#include "hci_request.h"
#include "hci_codec.h"
#include "hci_debugfs.h"
#include "smp.h"
#include "eir.h"
#include "msft.h"
#include "aosp.h"
#include "leds.h"

static void hci_cmd_sync_complete(struct hci_dev *hdev, u8 result, u16 opcode,
				  struct sk_buff *skb)
{
	bt_dev_dbg(hdev, "result 0x%2.2x", result);

	if (hdev->req_status != HCI_REQ_PEND)
		return;

	hdev->req_result = result;
	hdev->req_status = HCI_REQ_DONE;

	if (skb) {
		struct sock *sk = hci_skb_sk(skb);

		 
		if (sk)
			sock_put(sk);

		hdev->req_skb = skb_get(skb);
	}

	wake_up_interruptible(&hdev->req_wait_q);
}

static struct sk_buff *hci_cmd_sync_alloc(struct hci_dev *hdev, u16 opcode,
					  u32 plen, const void *param,
					  struct sock *sk)
{
	int len = HCI_COMMAND_HDR_SIZE + plen;
	struct hci_command_hdr *hdr;
	struct sk_buff *skb;

	skb = bt_skb_alloc(len, GFP_ATOMIC);
	if (!skb)
		return NULL;

	hdr = skb_put(skb, HCI_COMMAND_HDR_SIZE);
	hdr->opcode = cpu_to_le16(opcode);
	hdr->plen   = plen;

	if (plen)
		skb_put_data(skb, param, plen);

	bt_dev_dbg(hdev, "skb len %d", skb->len);

	hci_skb_pkt_type(skb) = HCI_COMMAND_PKT;
	hci_skb_opcode(skb) = opcode;

	 
	if (sk) {
		hci_skb_sk(skb) = sk;
		sock_hold(sk);
	}

	return skb;
}

static void hci_cmd_sync_add(struct hci_request *req, u16 opcode, u32 plen,
			     const void *param, u8 event, struct sock *sk)
{
	struct hci_dev *hdev = req->hdev;
	struct sk_buff *skb;

	bt_dev_dbg(hdev, "opcode 0x%4.4x plen %d", opcode, plen);

	 
	if (req->err)
		return;

	skb = hci_cmd_sync_alloc(hdev, opcode, plen, param, sk);
	if (!skb) {
		bt_dev_err(hdev, "no memory for command (opcode 0x%4.4x)",
			   opcode);
		req->err = -ENOMEM;
		return;
	}

	if (skb_queue_empty(&req->cmd_q))
		bt_cb(skb)->hci.req_flags |= HCI_REQ_START;

	hci_skb_event(skb) = event;

	skb_queue_tail(&req->cmd_q, skb);
}

static int hci_cmd_sync_run(struct hci_request *req)
{
	struct hci_dev *hdev = req->hdev;
	struct sk_buff *skb;
	unsigned long flags;

	bt_dev_dbg(hdev, "length %u", skb_queue_len(&req->cmd_q));

	 
	if (req->err) {
		skb_queue_purge(&req->cmd_q);
		return req->err;
	}

	 
	if (skb_queue_empty(&req->cmd_q))
		return -ENODATA;

	skb = skb_peek_tail(&req->cmd_q);
	bt_cb(skb)->hci.req_complete_skb = hci_cmd_sync_complete;
	bt_cb(skb)->hci.req_flags |= HCI_REQ_SKB;

	spin_lock_irqsave(&hdev->cmd_q.lock, flags);
	skb_queue_splice_tail(&req->cmd_q, &hdev->cmd_q);
	spin_unlock_irqrestore(&hdev->cmd_q.lock, flags);

	queue_work(hdev->workqueue, &hdev->cmd_work);

	return 0;
}

 
struct sk_buff *__hci_cmd_sync_sk(struct hci_dev *hdev, u16 opcode, u32 plen,
				  const void *param, u8 event, u32 timeout,
				  struct sock *sk)
{
	struct hci_request req;
	struct sk_buff *skb;
	int err = 0;

	bt_dev_dbg(hdev, "Opcode 0x%4.4x", opcode);

	hci_req_init(&req, hdev);

	hci_cmd_sync_add(&req, opcode, plen, param, event, sk);

	hdev->req_status = HCI_REQ_PEND;

	err = hci_cmd_sync_run(&req);
	if (err < 0)
		return ERR_PTR(err);

	err = wait_event_interruptible_timeout(hdev->req_wait_q,
					       hdev->req_status != HCI_REQ_PEND,
					       timeout);

	if (err == -ERESTARTSYS)
		return ERR_PTR(-EINTR);

	switch (hdev->req_status) {
	case HCI_REQ_DONE:
		err = -bt_to_errno(hdev->req_result);
		break;

	case HCI_REQ_CANCELED:
		err = -hdev->req_result;
		break;

	default:
		err = -ETIMEDOUT;
		break;
	}

	hdev->req_status = 0;
	hdev->req_result = 0;
	skb = hdev->req_skb;
	hdev->req_skb = NULL;

	bt_dev_dbg(hdev, "end: err %d", err);

	if (err < 0) {
		kfree_skb(skb);
		return ERR_PTR(err);
	}

	return skb;
}
EXPORT_SYMBOL(__hci_cmd_sync_sk);

 
struct sk_buff *__hci_cmd_sync(struct hci_dev *hdev, u16 opcode, u32 plen,
			       const void *param, u32 timeout)
{
	return __hci_cmd_sync_sk(hdev, opcode, plen, param, 0, timeout, NULL);
}
EXPORT_SYMBOL(__hci_cmd_sync);

 
struct sk_buff *hci_cmd_sync(struct hci_dev *hdev, u16 opcode, u32 plen,
			     const void *param, u32 timeout)
{
	struct sk_buff *skb;

	if (!test_bit(HCI_UP, &hdev->flags))
		return ERR_PTR(-ENETDOWN);

	bt_dev_dbg(hdev, "opcode 0x%4.4x plen %d", opcode, plen);

	hci_req_sync_lock(hdev);
	skb = __hci_cmd_sync(hdev, opcode, plen, param, timeout);
	hci_req_sync_unlock(hdev);

	return skb;
}
EXPORT_SYMBOL(hci_cmd_sync);

 
struct sk_buff *__hci_cmd_sync_ev(struct hci_dev *hdev, u16 opcode, u32 plen,
				  const void *param, u8 event, u32 timeout)
{
	return __hci_cmd_sync_sk(hdev, opcode, plen, param, event, timeout,
				 NULL);
}
EXPORT_SYMBOL(__hci_cmd_sync_ev);

 
int __hci_cmd_sync_status_sk(struct hci_dev *hdev, u16 opcode, u32 plen,
			     const void *param, u8 event, u32 timeout,
			     struct sock *sk)
{
	struct sk_buff *skb;
	u8 status;

	skb = __hci_cmd_sync_sk(hdev, opcode, plen, param, event, timeout, sk);
	if (IS_ERR(skb)) {
		if (!event)
			bt_dev_err(hdev, "Opcode 0x%4.4x failed: %ld", opcode,
				   PTR_ERR(skb));
		return PTR_ERR(skb);
	}

	 
	if (!skb)
		return 0;

	status = skb->data[0];

	kfree_skb(skb);

	return status;
}
EXPORT_SYMBOL(__hci_cmd_sync_status_sk);

int __hci_cmd_sync_status(struct hci_dev *hdev, u16 opcode, u32 plen,
			  const void *param, u32 timeout)
{
	return __hci_cmd_sync_status_sk(hdev, opcode, plen, param, 0, timeout,
					NULL);
}
EXPORT_SYMBOL(__hci_cmd_sync_status);

static void hci_cmd_sync_work(struct work_struct *work)
{
	struct hci_dev *hdev = container_of(work, struct hci_dev, cmd_sync_work);

	bt_dev_dbg(hdev, "");

	 
	while (1) {
		struct hci_cmd_sync_work_entry *entry;

		mutex_lock(&hdev->cmd_sync_work_lock);
		entry = list_first_entry_or_null(&hdev->cmd_sync_work_list,
						 struct hci_cmd_sync_work_entry,
						 list);
		if (entry)
			list_del(&entry->list);
		mutex_unlock(&hdev->cmd_sync_work_lock);

		if (!entry)
			break;

		bt_dev_dbg(hdev, "entry %p", entry);

		if (entry->func) {
			int err;

			hci_req_sync_lock(hdev);
			err = entry->func(hdev, entry->data);
			if (entry->destroy)
				entry->destroy(hdev, entry->data, err);
			hci_req_sync_unlock(hdev);
		}

		kfree(entry);
	}
}

static void hci_cmd_sync_cancel_work(struct work_struct *work)
{
	struct hci_dev *hdev = container_of(work, struct hci_dev, cmd_sync_cancel_work);

	cancel_delayed_work_sync(&hdev->cmd_timer);
	cancel_delayed_work_sync(&hdev->ncmd_timer);
	atomic_set(&hdev->cmd_cnt, 1);

	wake_up_interruptible(&hdev->req_wait_q);
}

static int hci_scan_disable_sync(struct hci_dev *hdev);
static int scan_disable_sync(struct hci_dev *hdev, void *data)
{
	return hci_scan_disable_sync(hdev);
}

static int hci_inquiry_sync(struct hci_dev *hdev, u8 length);
static int interleaved_inquiry_sync(struct hci_dev *hdev, void *data)
{
	return hci_inquiry_sync(hdev, DISCOV_INTERLEAVED_INQUIRY_LEN);
}

static void le_scan_disable(struct work_struct *work)
{
	struct hci_dev *hdev = container_of(work, struct hci_dev,
					    le_scan_disable.work);
	int status;

	bt_dev_dbg(hdev, "");
	hci_dev_lock(hdev);

	if (!hci_dev_test_flag(hdev, HCI_LE_SCAN))
		goto _return;

	cancel_delayed_work(&hdev->le_scan_restart);

	status = hci_cmd_sync_queue(hdev, scan_disable_sync, NULL, NULL);
	if (status) {
		bt_dev_err(hdev, "failed to disable LE scan: %d", status);
		goto _return;
	}

	hdev->discovery.scan_start = 0;

	 

	if (hdev->discovery.type == DISCOV_TYPE_LE)
		goto discov_stopped;

	if (hdev->discovery.type != DISCOV_TYPE_INTERLEAVED)
		goto _return;

	if (test_bit(HCI_QUIRK_SIMULTANEOUS_DISCOVERY, &hdev->quirks)) {
		if (!test_bit(HCI_INQUIRY, &hdev->flags) &&
		    hdev->discovery.state != DISCOVERY_RESOLVING)
			goto discov_stopped;

		goto _return;
	}

	status = hci_cmd_sync_queue(hdev, interleaved_inquiry_sync, NULL, NULL);
	if (status) {
		bt_dev_err(hdev, "inquiry failed: status %d", status);
		goto discov_stopped;
	}

	goto _return;

discov_stopped:
	hci_discovery_set_state(hdev, DISCOVERY_STOPPED);

_return:
	hci_dev_unlock(hdev);
}

static int hci_le_set_scan_enable_sync(struct hci_dev *hdev, u8 val,
				       u8 filter_dup);
static int hci_le_scan_restart_sync(struct hci_dev *hdev)
{
	 
	if (!hci_dev_test_flag(hdev, HCI_LE_SCAN))
		return 0;

	if (hdev->scanning_paused) {
		bt_dev_dbg(hdev, "Scanning is paused for suspend");
		return 0;
	}

	hci_le_set_scan_enable_sync(hdev, LE_SCAN_DISABLE, 0x00);
	return hci_le_set_scan_enable_sync(hdev, LE_SCAN_ENABLE,
					   LE_SCAN_FILTER_DUP_ENABLE);
}

static void le_scan_restart(struct work_struct *work)
{
	struct hci_dev *hdev = container_of(work, struct hci_dev,
					    le_scan_restart.work);
	unsigned long timeout, duration, scan_start, now;
	int status;

	bt_dev_dbg(hdev, "");

	status = hci_le_scan_restart_sync(hdev);
	if (status) {
		bt_dev_err(hdev, "failed to restart LE scan: status %d",
			   status);
		return;
	}

	hci_dev_lock(hdev);

	if (!test_bit(HCI_QUIRK_STRICT_DUPLICATE_FILTER, &hdev->quirks) ||
	    !hdev->discovery.scan_start)
		goto unlock;

	 
	duration = hdev->discovery.scan_duration;
	scan_start = hdev->discovery.scan_start;
	now = jiffies;
	if (now - scan_start <= duration) {
		int elapsed;

		if (now >= scan_start)
			elapsed = now - scan_start;
		else
			elapsed = ULONG_MAX - scan_start + now;

		timeout = duration - elapsed;
	} else {
		timeout = 0;
	}

	queue_delayed_work(hdev->req_workqueue,
			   &hdev->le_scan_disable, timeout);

unlock:
	hci_dev_unlock(hdev);
}

static int reenable_adv_sync(struct hci_dev *hdev, void *data)
{
	bt_dev_dbg(hdev, "");

	if (!hci_dev_test_flag(hdev, HCI_ADVERTISING) &&
	    list_empty(&hdev->adv_instances))
		return 0;

	if (hdev->cur_adv_instance) {
		return hci_schedule_adv_instance_sync(hdev,
						      hdev->cur_adv_instance,
						      true);
	} else {
		if (ext_adv_capable(hdev)) {
			hci_start_ext_adv_sync(hdev, 0x00);
		} else {
			hci_update_adv_data_sync(hdev, 0x00);
			hci_update_scan_rsp_data_sync(hdev, 0x00);
			hci_enable_advertising_sync(hdev);
		}
	}

	return 0;
}

static void reenable_adv(struct work_struct *work)
{
	struct hci_dev *hdev = container_of(work, struct hci_dev,
					    reenable_adv_work);
	int status;

	bt_dev_dbg(hdev, "");

	hci_dev_lock(hdev);

	status = hci_cmd_sync_queue(hdev, reenable_adv_sync, NULL, NULL);
	if (status)
		bt_dev_err(hdev, "failed to reenable ADV: %d", status);

	hci_dev_unlock(hdev);
}

static void cancel_adv_timeout(struct hci_dev *hdev)
{
	if (hdev->adv_instance_timeout) {
		hdev->adv_instance_timeout = 0;
		cancel_delayed_work(&hdev->adv_instance_expire);
	}
}

 
int hci_clear_adv_instance_sync(struct hci_dev *hdev, struct sock *sk,
				u8 instance, bool force)
{
	struct adv_info *adv_instance, *n, *next_instance = NULL;
	int err;
	u8 rem_inst;

	 
	if (!instance || hdev->cur_adv_instance == instance)
		cancel_adv_timeout(hdev);

	 
	if (instance && hdev->cur_adv_instance == instance)
		next_instance = hci_get_next_instance(hdev, instance);

	if (instance == 0x00) {
		list_for_each_entry_safe(adv_instance, n, &hdev->adv_instances,
					 list) {
			if (!(force || adv_instance->timeout))
				continue;

			rem_inst = adv_instance->instance;
			err = hci_remove_adv_instance(hdev, rem_inst);
			if (!err)
				mgmt_advertising_removed(sk, hdev, rem_inst);
		}
	} else {
		adv_instance = hci_find_adv_instance(hdev, instance);

		if (force || (adv_instance && adv_instance->timeout &&
			      !adv_instance->remaining_time)) {
			 
			if (next_instance &&
			    next_instance->instance == instance)
				next_instance = NULL;

			err = hci_remove_adv_instance(hdev, instance);
			if (!err)
				mgmt_advertising_removed(sk, hdev, instance);
		}
	}

	if (!hdev_is_powered(hdev) || hci_dev_test_flag(hdev, HCI_ADVERTISING))
		return 0;

	if (next_instance && !ext_adv_capable(hdev))
		return hci_schedule_adv_instance_sync(hdev,
						      next_instance->instance,
						      false);

	return 0;
}

static int adv_timeout_expire_sync(struct hci_dev *hdev, void *data)
{
	u8 instance = *(u8 *)data;

	kfree(data);

	hci_clear_adv_instance_sync(hdev, NULL, instance, false);

	if (list_empty(&hdev->adv_instances))
		return hci_disable_advertising_sync(hdev);

	return 0;
}

static void adv_timeout_expire(struct work_struct *work)
{
	u8 *inst_ptr;
	struct hci_dev *hdev = container_of(work, struct hci_dev,
					    adv_instance_expire.work);

	bt_dev_dbg(hdev, "");

	hci_dev_lock(hdev);

	hdev->adv_instance_timeout = 0;

	if (hdev->cur_adv_instance == 0x00)
		goto unlock;

	inst_ptr = kmalloc(1, GFP_KERNEL);
	if (!inst_ptr)
		goto unlock;

	*inst_ptr = hdev->cur_adv_instance;
	hci_cmd_sync_queue(hdev, adv_timeout_expire_sync, inst_ptr, NULL);

unlock:
	hci_dev_unlock(hdev);
}

void hci_cmd_sync_init(struct hci_dev *hdev)
{
	INIT_WORK(&hdev->cmd_sync_work, hci_cmd_sync_work);
	INIT_LIST_HEAD(&hdev->cmd_sync_work_list);
	mutex_init(&hdev->cmd_sync_work_lock);
	mutex_init(&hdev->unregister_lock);

	INIT_WORK(&hdev->cmd_sync_cancel_work, hci_cmd_sync_cancel_work);
	INIT_WORK(&hdev->reenable_adv_work, reenable_adv);
	INIT_DELAYED_WORK(&hdev->le_scan_disable, le_scan_disable);
	INIT_DELAYED_WORK(&hdev->le_scan_restart, le_scan_restart);
	INIT_DELAYED_WORK(&hdev->adv_instance_expire, adv_timeout_expire);
}

void hci_cmd_sync_clear(struct hci_dev *hdev)
{
	struct hci_cmd_sync_work_entry *entry, *tmp;

	cancel_work_sync(&hdev->cmd_sync_work);
	cancel_work_sync(&hdev->reenable_adv_work);

	mutex_lock(&hdev->cmd_sync_work_lock);
	list_for_each_entry_safe(entry, tmp, &hdev->cmd_sync_work_list, list) {
		if (entry->destroy)
			entry->destroy(hdev, entry->data, -ECANCELED);

		list_del(&entry->list);
		kfree(entry);
	}
	mutex_unlock(&hdev->cmd_sync_work_lock);
}

void __hci_cmd_sync_cancel(struct hci_dev *hdev, int err)
{
	bt_dev_dbg(hdev, "err 0x%2.2x", err);

	if (hdev->req_status == HCI_REQ_PEND) {
		hdev->req_result = err;
		hdev->req_status = HCI_REQ_CANCELED;

		cancel_delayed_work_sync(&hdev->cmd_timer);
		cancel_delayed_work_sync(&hdev->ncmd_timer);
		atomic_set(&hdev->cmd_cnt, 1);

		wake_up_interruptible(&hdev->req_wait_q);
	}
}

void hci_cmd_sync_cancel(struct hci_dev *hdev, int err)
{
	bt_dev_dbg(hdev, "err 0x%2.2x", err);

	if (hdev->req_status == HCI_REQ_PEND) {
		hdev->req_result = err;
		hdev->req_status = HCI_REQ_CANCELED;

		queue_work(hdev->workqueue, &hdev->cmd_sync_cancel_work);
	}
}
EXPORT_SYMBOL(hci_cmd_sync_cancel);

 
int hci_cmd_sync_submit(struct hci_dev *hdev, hci_cmd_sync_work_func_t func,
			void *data, hci_cmd_sync_work_destroy_t destroy)
{
	struct hci_cmd_sync_work_entry *entry;
	int err = 0;

	mutex_lock(&hdev->unregister_lock);
	if (hci_dev_test_flag(hdev, HCI_UNREGISTER)) {
		err = -ENODEV;
		goto unlock;
	}

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry) {
		err = -ENOMEM;
		goto unlock;
	}
	entry->func = func;
	entry->data = data;
	entry->destroy = destroy;

	mutex_lock(&hdev->cmd_sync_work_lock);
	list_add_tail(&entry->list, &hdev->cmd_sync_work_list);
	mutex_unlock(&hdev->cmd_sync_work_lock);

	queue_work(hdev->req_workqueue, &hdev->cmd_sync_work);

unlock:
	mutex_unlock(&hdev->unregister_lock);
	return err;
}
EXPORT_SYMBOL(hci_cmd_sync_submit);

 
int hci_cmd_sync_queue(struct hci_dev *hdev, hci_cmd_sync_work_func_t func,
		       void *data, hci_cmd_sync_work_destroy_t destroy)
{
	 
	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return -ENETDOWN;

	return hci_cmd_sync_submit(hdev, func, data, destroy);
}
EXPORT_SYMBOL(hci_cmd_sync_queue);

int hci_update_eir_sync(struct hci_dev *hdev)
{
	struct hci_cp_write_eir cp;

	bt_dev_dbg(hdev, "");

	if (!hdev_is_powered(hdev))
		return 0;

	if (!lmp_ext_inq_capable(hdev))
		return 0;

	if (!hci_dev_test_flag(hdev, HCI_SSP_ENABLED))
		return 0;

	if (hci_dev_test_flag(hdev, HCI_SERVICE_CACHE))
		return 0;

	memset(&cp, 0, sizeof(cp));

	eir_create(hdev, cp.data);

	if (memcmp(cp.data, hdev->eir, sizeof(cp.data)) == 0)
		return 0;

	memcpy(hdev->eir, cp.data, sizeof(cp.data));

	return __hci_cmd_sync_status(hdev, HCI_OP_WRITE_EIR, sizeof(cp), &cp,
				     HCI_CMD_TIMEOUT);
}

static u8 get_service_classes(struct hci_dev *hdev)
{
	struct bt_uuid *uuid;
	u8 val = 0;

	list_for_each_entry(uuid, &hdev->uuids, list)
		val |= uuid->svc_hint;

	return val;
}

int hci_update_class_sync(struct hci_dev *hdev)
{
	u8 cod[3];

	bt_dev_dbg(hdev, "");

	if (!hdev_is_powered(hdev))
		return 0;

	if (!hci_dev_test_flag(hdev, HCI_BREDR_ENABLED))
		return 0;

	if (hci_dev_test_flag(hdev, HCI_SERVICE_CACHE))
		return 0;

	cod[0] = hdev->minor_class;
	cod[1] = hdev->major_class;
	cod[2] = get_service_classes(hdev);

	if (hci_dev_test_flag(hdev, HCI_LIMITED_DISCOVERABLE))
		cod[1] |= 0x20;

	if (memcmp(cod, hdev->dev_class, 3) == 0)
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_WRITE_CLASS_OF_DEV,
				     sizeof(cod), cod, HCI_CMD_TIMEOUT);
}

static bool is_advertising_allowed(struct hci_dev *hdev, bool connectable)
{
	 
	if (hci_conn_num(hdev, LE_LINK) == 0)
		return true;

	 
	if (hdev->conn_hash.le_num_peripheral > 0) {
		 
		if (!connectable && !(hdev->le_states[2] & 0x10))
			return false;

		 
		if (connectable && (!(hdev->le_states[4] & 0x40) ||
				    !(hdev->le_states[2] & 0x20)))
			return false;
	}

	 
	if (hci_conn_num(hdev, LE_LINK) != hdev->conn_hash.le_num_peripheral) {
		 
		if (!connectable && !(hdev->le_states[2] & 0x02))
			return false;

		 
		if (connectable && (!(hdev->le_states[4] & 0x08) ||
				    !(hdev->le_states[2] & 0x08)))
			return false;
	}

	return true;
}

static bool adv_use_rpa(struct hci_dev *hdev, uint32_t flags)
{
	 
	if (!hci_dev_test_flag(hdev, HCI_PRIVACY))
		return false;

	 
	if (!hci_dev_test_flag(hdev, HCI_LIMITED_PRIVACY))
		return true;

	 
	if ((flags & MGMT_ADV_FLAG_DISCOV) &&
	    hci_dev_test_flag(hdev, HCI_BONDABLE))
		return false;

	 
	return true;
}

static int hci_set_random_addr_sync(struct hci_dev *hdev, bdaddr_t *rpa)
{
	 
	if (hci_dev_test_flag(hdev, HCI_LE_ADV) ||
	    hci_lookup_le_connect(hdev)) {
		bt_dev_dbg(hdev, "Deferring random address update");
		hci_dev_set_flag(hdev, HCI_RPA_EXPIRED);
		return 0;
	}

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_RANDOM_ADDR,
				     6, rpa, HCI_CMD_TIMEOUT);
}

int hci_update_random_address_sync(struct hci_dev *hdev, bool require_privacy,
				   bool rpa, u8 *own_addr_type)
{
	int err;

	 
	if (rpa) {
		 
		if (use_ll_privacy(hdev))
			*own_addr_type = ADDR_LE_DEV_RANDOM_RESOLVED;
		else
			*own_addr_type = ADDR_LE_DEV_RANDOM;

		 
		if (rpa_valid(hdev))
			return 0;

		err = smp_generate_rpa(hdev, hdev->irk, &hdev->rpa);
		if (err < 0) {
			bt_dev_err(hdev, "failed to generate new RPA");
			return err;
		}

		err = hci_set_random_addr_sync(hdev, &hdev->rpa);
		if (err)
			return err;

		return 0;
	}

	 
	if (require_privacy) {
		bdaddr_t nrpa;

		while (true) {
			 
			get_random_bytes(&nrpa, 6);
			nrpa.b[5] &= 0x3f;

			 
			if (bacmp(&hdev->bdaddr, &nrpa))
				break;
		}

		*own_addr_type = ADDR_LE_DEV_RANDOM;

		return hci_set_random_addr_sync(hdev, &nrpa);
	}

	 
	if (hci_dev_test_flag(hdev, HCI_FORCE_STATIC_ADDR) ||
	    !bacmp(&hdev->bdaddr, BDADDR_ANY) ||
	    (!hci_dev_test_flag(hdev, HCI_BREDR_ENABLED) &&
	     bacmp(&hdev->static_addr, BDADDR_ANY))) {
		*own_addr_type = ADDR_LE_DEV_RANDOM;
		if (bacmp(&hdev->static_addr, &hdev->random_addr))
			return hci_set_random_addr_sync(hdev,
							&hdev->static_addr);
		return 0;
	}

	 
	*own_addr_type = ADDR_LE_DEV_PUBLIC;

	return 0;
}

static int hci_disable_ext_adv_instance_sync(struct hci_dev *hdev, u8 instance)
{
	struct hci_cp_le_set_ext_adv_enable *cp;
	struct hci_cp_ext_adv_set *set;
	u8 data[sizeof(*cp) + sizeof(*set) * 1];
	u8 size;

	 
	if (instance > 0) {
		struct adv_info *adv;

		adv = hci_find_adv_instance(hdev, instance);
		if (!adv)
			return -EINVAL;

		 
		if (!adv->enabled)
			return 0;
	}

	memset(data, 0, sizeof(data));

	cp = (void *)data;
	set = (void *)cp->data;

	 
	cp->num_of_sets = !!instance;
	cp->enable = 0x00;

	set->handle = instance;

	size = sizeof(*cp) + sizeof(*set) * cp->num_of_sets;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_EXT_ADV_ENABLE,
				     size, data, HCI_CMD_TIMEOUT);
}

static int hci_set_adv_set_random_addr_sync(struct hci_dev *hdev, u8 instance,
					    bdaddr_t *random_addr)
{
	struct hci_cp_le_set_adv_set_rand_addr cp;
	int err;

	if (!instance) {
		 
		err = hci_set_random_addr_sync(hdev, random_addr);
		if (err)
			return err;
	}

	memset(&cp, 0, sizeof(cp));

	cp.handle = instance;
	bacpy(&cp.bdaddr, random_addr);

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_ADV_SET_RAND_ADDR,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

int hci_setup_ext_adv_instance_sync(struct hci_dev *hdev, u8 instance)
{
	struct hci_cp_le_set_ext_adv_params cp;
	bool connectable;
	u32 flags;
	bdaddr_t random_addr;
	u8 own_addr_type;
	int err;
	struct adv_info *adv;
	bool secondary_adv;

	if (instance > 0) {
		adv = hci_find_adv_instance(hdev, instance);
		if (!adv)
			return -EINVAL;
	} else {
		adv = NULL;
	}

	 
	if (adv && !adv->pending) {
		err = hci_disable_ext_adv_instance_sync(hdev, instance);
		if (err)
			return err;
	}

	flags = hci_adv_instance_flags(hdev, instance);

	 
	connectable = (flags & MGMT_ADV_FLAG_CONNECTABLE) ||
		      mgmt_get_connectable(hdev);

	if (!is_advertising_allowed(hdev, connectable))
		return -EPERM;

	 
	err = hci_get_random_address(hdev, !connectable,
				     adv_use_rpa(hdev, flags), adv,
				     &own_addr_type, &random_addr);
	if (err < 0)
		return err;

	memset(&cp, 0, sizeof(cp));

	if (adv) {
		hci_cpu_to_le24(adv->min_interval, cp.min_interval);
		hci_cpu_to_le24(adv->max_interval, cp.max_interval);
		cp.tx_power = adv->tx_power;
	} else {
		hci_cpu_to_le24(hdev->le_adv_min_interval, cp.min_interval);
		hci_cpu_to_le24(hdev->le_adv_max_interval, cp.max_interval);
		cp.tx_power = HCI_ADV_TX_POWER_NO_PREFERENCE;
	}

	secondary_adv = (flags & MGMT_ADV_FLAG_SEC_MASK);

	if (connectable) {
		if (secondary_adv)
			cp.evt_properties = cpu_to_le16(LE_EXT_ADV_CONN_IND);
		else
			cp.evt_properties = cpu_to_le16(LE_LEGACY_ADV_IND);
	} else if (hci_adv_instance_is_scannable(hdev, instance) ||
		   (flags & MGMT_ADV_PARAM_SCAN_RSP)) {
		if (secondary_adv)
			cp.evt_properties = cpu_to_le16(LE_EXT_ADV_SCAN_IND);
		else
			cp.evt_properties = cpu_to_le16(LE_LEGACY_ADV_SCAN_IND);
	} else {
		if (secondary_adv)
			cp.evt_properties = cpu_to_le16(LE_EXT_ADV_NON_CONN_IND);
		else
			cp.evt_properties = cpu_to_le16(LE_LEGACY_NONCONN_IND);
	}

	 
	if (own_addr_type == ADDR_LE_DEV_RANDOM_RESOLVED)
		hci_copy_identity_address(hdev, &cp.peer_addr,
					  &cp.peer_addr_type);

	cp.own_addr_type = own_addr_type;
	cp.channel_map = hdev->le_adv_channel_map;
	cp.handle = instance;

	if (flags & MGMT_ADV_FLAG_SEC_2M) {
		cp.primary_phy = HCI_ADV_PHY_1M;
		cp.secondary_phy = HCI_ADV_PHY_2M;
	} else if (flags & MGMT_ADV_FLAG_SEC_CODED) {
		cp.primary_phy = HCI_ADV_PHY_CODED;
		cp.secondary_phy = HCI_ADV_PHY_CODED;
	} else {
		 
		cp.primary_phy = HCI_ADV_PHY_1M;
		cp.secondary_phy = HCI_ADV_PHY_1M;
	}

	err = __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_EXT_ADV_PARAMS,
				    sizeof(cp), &cp, HCI_CMD_TIMEOUT);
	if (err)
		return err;

	if ((own_addr_type == ADDR_LE_DEV_RANDOM ||
	     own_addr_type == ADDR_LE_DEV_RANDOM_RESOLVED) &&
	    bacmp(&random_addr, BDADDR_ANY)) {
		 
		if (adv) {
			if (!bacmp(&random_addr, &adv->random_addr))
				return 0;
		} else {
			if (!bacmp(&random_addr, &hdev->random_addr))
				return 0;
		}

		return hci_set_adv_set_random_addr_sync(hdev, instance,
							&random_addr);
	}

	return 0;
}

static int hci_set_ext_scan_rsp_data_sync(struct hci_dev *hdev, u8 instance)
{
	struct {
		struct hci_cp_le_set_ext_scan_rsp_data cp;
		u8 data[HCI_MAX_EXT_AD_LENGTH];
	} pdu;
	u8 len;
	struct adv_info *adv = NULL;
	int err;

	memset(&pdu, 0, sizeof(pdu));

	if (instance) {
		adv = hci_find_adv_instance(hdev, instance);
		if (!adv || !adv->scan_rsp_changed)
			return 0;
	}

	len = eir_create_scan_rsp(hdev, instance, pdu.data);

	pdu.cp.handle = instance;
	pdu.cp.length = len;
	pdu.cp.operation = LE_SET_ADV_DATA_OP_COMPLETE;
	pdu.cp.frag_pref = LE_SET_ADV_DATA_NO_FRAG;

	err = __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_EXT_SCAN_RSP_DATA,
				    sizeof(pdu.cp) + len, &pdu.cp,
				    HCI_CMD_TIMEOUT);
	if (err)
		return err;

	if (adv) {
		adv->scan_rsp_changed = false;
	} else {
		memcpy(hdev->scan_rsp_data, pdu.data, len);
		hdev->scan_rsp_data_len = len;
	}

	return 0;
}

static int __hci_set_scan_rsp_data_sync(struct hci_dev *hdev, u8 instance)
{
	struct hci_cp_le_set_scan_rsp_data cp;
	u8 len;

	memset(&cp, 0, sizeof(cp));

	len = eir_create_scan_rsp(hdev, instance, cp.data);

	if (hdev->scan_rsp_data_len == len &&
	    !memcmp(cp.data, hdev->scan_rsp_data, len))
		return 0;

	memcpy(hdev->scan_rsp_data, cp.data, sizeof(cp.data));
	hdev->scan_rsp_data_len = len;

	cp.length = len;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_SCAN_RSP_DATA,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

int hci_update_scan_rsp_data_sync(struct hci_dev *hdev, u8 instance)
{
	if (!hci_dev_test_flag(hdev, HCI_LE_ENABLED))
		return 0;

	if (ext_adv_capable(hdev))
		return hci_set_ext_scan_rsp_data_sync(hdev, instance);

	return __hci_set_scan_rsp_data_sync(hdev, instance);
}

int hci_enable_ext_advertising_sync(struct hci_dev *hdev, u8 instance)
{
	struct hci_cp_le_set_ext_adv_enable *cp;
	struct hci_cp_ext_adv_set *set;
	u8 data[sizeof(*cp) + sizeof(*set) * 1];
	struct adv_info *adv;

	if (instance > 0) {
		adv = hci_find_adv_instance(hdev, instance);
		if (!adv)
			return -EINVAL;
		 
		if (adv->enabled)
			return 0;
	} else {
		adv = NULL;
	}

	cp = (void *)data;
	set = (void *)cp->data;

	memset(cp, 0, sizeof(*cp));

	cp->enable = 0x01;
	cp->num_of_sets = 0x01;

	memset(set, 0, sizeof(*set));

	set->handle = instance;

	 
	if (adv && adv->timeout) {
		u16 duration = adv->timeout * MSEC_PER_SEC;

		 
		set->duration = cpu_to_le16(duration / 10);
	}

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_EXT_ADV_ENABLE,
				     sizeof(*cp) +
				     sizeof(*set) * cp->num_of_sets,
				     data, HCI_CMD_TIMEOUT);
}

int hci_start_ext_adv_sync(struct hci_dev *hdev, u8 instance)
{
	int err;

	err = hci_setup_ext_adv_instance_sync(hdev, instance);
	if (err)
		return err;

	err = hci_set_ext_scan_rsp_data_sync(hdev, instance);
	if (err)
		return err;

	return hci_enable_ext_advertising_sync(hdev, instance);
}

static int hci_disable_per_advertising_sync(struct hci_dev *hdev, u8 instance)
{
	struct hci_cp_le_set_per_adv_enable cp;
	struct adv_info *adv = NULL;

	 
	adv = hci_find_adv_instance(hdev, instance);
	if (!adv || !adv->periodic || !adv->enabled)
		return 0;

	memset(&cp, 0, sizeof(cp));

	cp.enable = 0x00;
	cp.handle = instance;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_PER_ADV_ENABLE,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

static int hci_set_per_adv_params_sync(struct hci_dev *hdev, u8 instance,
				       u16 min_interval, u16 max_interval)
{
	struct hci_cp_le_set_per_adv_params cp;

	memset(&cp, 0, sizeof(cp));

	if (!min_interval)
		min_interval = DISCOV_LE_PER_ADV_INT_MIN;

	if (!max_interval)
		max_interval = DISCOV_LE_PER_ADV_INT_MAX;

	cp.handle = instance;
	cp.min_interval = cpu_to_le16(min_interval);
	cp.max_interval = cpu_to_le16(max_interval);
	cp.periodic_properties = 0x0000;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_PER_ADV_PARAMS,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

static int hci_set_per_adv_data_sync(struct hci_dev *hdev, u8 instance)
{
	struct {
		struct hci_cp_le_set_per_adv_data cp;
		u8 data[HCI_MAX_PER_AD_LENGTH];
	} pdu;
	u8 len;

	memset(&pdu, 0, sizeof(pdu));

	if (instance) {
		struct adv_info *adv = hci_find_adv_instance(hdev, instance);

		if (!adv || !adv->periodic)
			return 0;
	}

	len = eir_create_per_adv_data(hdev, instance, pdu.data);

	pdu.cp.length = len;
	pdu.cp.handle = instance;
	pdu.cp.operation = LE_SET_ADV_DATA_OP_COMPLETE;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_PER_ADV_DATA,
				     sizeof(pdu.cp) + len, &pdu,
				     HCI_CMD_TIMEOUT);
}

static int hci_enable_per_advertising_sync(struct hci_dev *hdev, u8 instance)
{
	struct hci_cp_le_set_per_adv_enable cp;
	struct adv_info *adv = NULL;

	 
	adv = hci_find_adv_instance(hdev, instance);
	if (adv && adv->periodic && adv->enabled)
		return 0;

	memset(&cp, 0, sizeof(cp));

	cp.enable = 0x01;
	cp.handle = instance;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_PER_ADV_ENABLE,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

 
static int hci_adv_bcast_annoucement(struct hci_dev *hdev, struct adv_info *adv)
{
	u8 bid[3];
	u8 ad[4 + 3];

	 
	if (!adv)
		return 0;

	 
	if (!eir_get_service_data(adv->per_adv_data, adv->per_adv_data_len,
				  0x1851, NULL))
		return 0;

	 
	if (eir_get_service_data(adv->adv_data, adv->adv_data_len, 0x1852,
				 NULL))
		return 0;

	 
	get_random_bytes(bid, sizeof(bid));
	eir_append_service_data(ad, 0, 0x1852, bid, sizeof(bid));
	hci_set_adv_instance_data(hdev, adv->instance, sizeof(ad), ad, 0, NULL);

	return hci_update_adv_data_sync(hdev, adv->instance);
}

int hci_start_per_adv_sync(struct hci_dev *hdev, u8 instance, u8 data_len,
			   u8 *data, u32 flags, u16 min_interval,
			   u16 max_interval, u16 sync_interval)
{
	struct adv_info *adv = NULL;
	int err;
	bool added = false;

	hci_disable_per_advertising_sync(hdev, instance);

	if (instance) {
		adv = hci_find_adv_instance(hdev, instance);
		 
		if (!adv) {
			adv = hci_add_per_instance(hdev, instance, flags,
						   data_len, data,
						   sync_interval,
						   sync_interval);
			if (IS_ERR(adv))
				return PTR_ERR(adv);
			adv->pending = false;
			added = true;
		}
	}

	 
	err = hci_start_ext_adv_sync(hdev, instance);
	if (err < 0)
		goto fail;

	err = hci_adv_bcast_annoucement(hdev, adv);
	if (err < 0)
		goto fail;

	err = hci_set_per_adv_params_sync(hdev, instance, min_interval,
					  max_interval);
	if (err < 0)
		goto fail;

	err = hci_set_per_adv_data_sync(hdev, instance);
	if (err < 0)
		goto fail;

	err = hci_enable_per_advertising_sync(hdev, instance);
	if (err < 0)
		goto fail;

	return 0;

fail:
	if (added)
		hci_remove_adv_instance(hdev, instance);

	return err;
}

static int hci_start_adv_sync(struct hci_dev *hdev, u8 instance)
{
	int err;

	if (ext_adv_capable(hdev))
		return hci_start_ext_adv_sync(hdev, instance);

	err = hci_update_adv_data_sync(hdev, instance);
	if (err)
		return err;

	err = hci_update_scan_rsp_data_sync(hdev, instance);
	if (err)
		return err;

	return hci_enable_advertising_sync(hdev);
}

int hci_enable_advertising_sync(struct hci_dev *hdev)
{
	struct adv_info *adv_instance;
	struct hci_cp_le_set_adv_param cp;
	u8 own_addr_type, enable = 0x01;
	bool connectable;
	u16 adv_min_interval, adv_max_interval;
	u32 flags;
	u8 status;

	if (ext_adv_capable(hdev))
		return hci_enable_ext_advertising_sync(hdev,
						       hdev->cur_adv_instance);

	flags = hci_adv_instance_flags(hdev, hdev->cur_adv_instance);
	adv_instance = hci_find_adv_instance(hdev, hdev->cur_adv_instance);

	 
	connectable = (flags & MGMT_ADV_FLAG_CONNECTABLE) ||
		      mgmt_get_connectable(hdev);

	if (!is_advertising_allowed(hdev, connectable))
		return -EINVAL;

	status = hci_disable_advertising_sync(hdev);
	if (status)
		return status;

	 
	hci_dev_clear_flag(hdev, HCI_LE_ADV);

	 
	status = hci_update_random_address_sync(hdev, !connectable,
						adv_use_rpa(hdev, flags),
						&own_addr_type);
	if (status)
		return status;

	memset(&cp, 0, sizeof(cp));

	if (adv_instance) {
		adv_min_interval = adv_instance->min_interval;
		adv_max_interval = adv_instance->max_interval;
	} else {
		adv_min_interval = hdev->le_adv_min_interval;
		adv_max_interval = hdev->le_adv_max_interval;
	}

	if (connectable) {
		cp.type = LE_ADV_IND;
	} else {
		if (hci_adv_instance_is_scannable(hdev, hdev->cur_adv_instance))
			cp.type = LE_ADV_SCAN_IND;
		else
			cp.type = LE_ADV_NONCONN_IND;

		if (!hci_dev_test_flag(hdev, HCI_DISCOVERABLE) ||
		    hci_dev_test_flag(hdev, HCI_LIMITED_DISCOVERABLE)) {
			adv_min_interval = DISCOV_LE_FAST_ADV_INT_MIN;
			adv_max_interval = DISCOV_LE_FAST_ADV_INT_MAX;
		}
	}

	cp.min_interval = cpu_to_le16(adv_min_interval);
	cp.max_interval = cpu_to_le16(adv_max_interval);
	cp.own_address_type = own_addr_type;
	cp.channel_map = hdev->le_adv_channel_map;

	status = __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_ADV_PARAM,
				       sizeof(cp), &cp, HCI_CMD_TIMEOUT);
	if (status)
		return status;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_ADV_ENABLE,
				     sizeof(enable), &enable, HCI_CMD_TIMEOUT);
}

static int enable_advertising_sync(struct hci_dev *hdev, void *data)
{
	return hci_enable_advertising_sync(hdev);
}

int hci_enable_advertising(struct hci_dev *hdev)
{
	if (!hci_dev_test_flag(hdev, HCI_ADVERTISING) &&
	    list_empty(&hdev->adv_instances))
		return 0;

	return hci_cmd_sync_queue(hdev, enable_advertising_sync, NULL, NULL);
}

int hci_remove_ext_adv_instance_sync(struct hci_dev *hdev, u8 instance,
				     struct sock *sk)
{
	int err;

	if (!ext_adv_capable(hdev))
		return 0;

	err = hci_disable_ext_adv_instance_sync(hdev, instance);
	if (err)
		return err;

	 
	if (instance > 0 && !hci_find_adv_instance(hdev, instance))
		return -EINVAL;

	return __hci_cmd_sync_status_sk(hdev, HCI_OP_LE_REMOVE_ADV_SET,
					sizeof(instance), &instance, 0,
					HCI_CMD_TIMEOUT, sk);
}

static int remove_ext_adv_sync(struct hci_dev *hdev, void *data)
{
	struct adv_info *adv = data;
	u8 instance = 0;

	if (adv)
		instance = adv->instance;

	return hci_remove_ext_adv_instance_sync(hdev, instance, NULL);
}

int hci_remove_ext_adv_instance(struct hci_dev *hdev, u8 instance)
{
	struct adv_info *adv = NULL;

	if (instance) {
		adv = hci_find_adv_instance(hdev, instance);
		if (!adv)
			return -EINVAL;
	}

	return hci_cmd_sync_queue(hdev, remove_ext_adv_sync, adv, NULL);
}

int hci_le_terminate_big_sync(struct hci_dev *hdev, u8 handle, u8 reason)
{
	struct hci_cp_le_term_big cp;

	memset(&cp, 0, sizeof(cp));
	cp.handle = handle;
	cp.reason = reason;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_TERM_BIG,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

static int hci_set_ext_adv_data_sync(struct hci_dev *hdev, u8 instance)
{
	struct {
		struct hci_cp_le_set_ext_adv_data cp;
		u8 data[HCI_MAX_EXT_AD_LENGTH];
	} pdu;
	u8 len;
	struct adv_info *adv = NULL;
	int err;

	memset(&pdu, 0, sizeof(pdu));

	if (instance) {
		adv = hci_find_adv_instance(hdev, instance);
		if (!adv || !adv->adv_data_changed)
			return 0;
	}

	len = eir_create_adv_data(hdev, instance, pdu.data);

	pdu.cp.length = len;
	pdu.cp.handle = instance;
	pdu.cp.operation = LE_SET_ADV_DATA_OP_COMPLETE;
	pdu.cp.frag_pref = LE_SET_ADV_DATA_NO_FRAG;

	err = __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_EXT_ADV_DATA,
				    sizeof(pdu.cp) + len, &pdu.cp,
				    HCI_CMD_TIMEOUT);
	if (err)
		return err;

	 
	if (adv) {
		adv->adv_data_changed = false;
	} else {
		memcpy(hdev->adv_data, pdu.data, len);
		hdev->adv_data_len = len;
	}

	return 0;
}

static int hci_set_adv_data_sync(struct hci_dev *hdev, u8 instance)
{
	struct hci_cp_le_set_adv_data cp;
	u8 len;

	memset(&cp, 0, sizeof(cp));

	len = eir_create_adv_data(hdev, instance, cp.data);

	 
	if (hdev->adv_data_len == len &&
	    memcmp(cp.data, hdev->adv_data, len) == 0)
		return 0;

	memcpy(hdev->adv_data, cp.data, sizeof(cp.data));
	hdev->adv_data_len = len;

	cp.length = len;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_ADV_DATA,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

int hci_update_adv_data_sync(struct hci_dev *hdev, u8 instance)
{
	if (!hci_dev_test_flag(hdev, HCI_LE_ENABLED))
		return 0;

	if (ext_adv_capable(hdev))
		return hci_set_ext_adv_data_sync(hdev, instance);

	return hci_set_adv_data_sync(hdev, instance);
}

int hci_schedule_adv_instance_sync(struct hci_dev *hdev, u8 instance,
				   bool force)
{
	struct adv_info *adv = NULL;
	u16 timeout;

	if (hci_dev_test_flag(hdev, HCI_ADVERTISING) && !ext_adv_capable(hdev))
		return -EPERM;

	if (hdev->adv_instance_timeout)
		return -EBUSY;

	adv = hci_find_adv_instance(hdev, instance);
	if (!adv)
		return -ENOENT;

	 
	if (adv->timeout == 0 || adv->duration <= adv->remaining_time)
		timeout = adv->duration;
	else
		timeout = adv->remaining_time;

	 
	if (adv->timeout)
		adv->remaining_time = adv->remaining_time - timeout;

	 
	if (!ext_adv_capable(hdev)) {
		hdev->adv_instance_timeout = timeout;
		queue_delayed_work(hdev->req_workqueue,
				   &hdev->adv_instance_expire,
				   msecs_to_jiffies(timeout * 1000));
	}

	 
	if (!force && hdev->cur_adv_instance == instance &&
	    hci_dev_test_flag(hdev, HCI_LE_ADV))
		return 0;

	hdev->cur_adv_instance = instance;

	return hci_start_adv_sync(hdev, instance);
}

static int hci_clear_adv_sets_sync(struct hci_dev *hdev, struct sock *sk)
{
	int err;

	if (!ext_adv_capable(hdev))
		return 0;

	 
	err = hci_disable_ext_adv_instance_sync(hdev, 0x00);
	if (err)
		return err;

	return __hci_cmd_sync_status_sk(hdev, HCI_OP_LE_CLEAR_ADV_SETS,
					0, NULL, 0, HCI_CMD_TIMEOUT, sk);
}

static int hci_clear_adv_sync(struct hci_dev *hdev, struct sock *sk, bool force)
{
	struct adv_info *adv, *n;
	int err = 0;

	if (ext_adv_capable(hdev))
		 
		err = hci_clear_adv_sets_sync(hdev, sk);
	if (ext_adv_capable(hdev))
		return err;

	 
	hci_dev_lock(hdev);

	 
	list_for_each_entry_safe(adv, n, &hdev->adv_instances, list) {
		u8 instance = adv->instance;
		int err;

		if (!(force || adv->timeout))
			continue;

		err = hci_remove_adv_instance(hdev, instance);
		if (!err)
			mgmt_advertising_removed(sk, hdev, instance);
	}

	hci_dev_unlock(hdev);

	return 0;
}

static int hci_remove_adv_sync(struct hci_dev *hdev, u8 instance,
			       struct sock *sk)
{
	int err = 0;

	 
	if (ext_adv_capable(hdev))
		err = hci_remove_ext_adv_instance_sync(hdev, instance, sk);
	if (ext_adv_capable(hdev))
		return err;

	 
	hci_dev_lock(hdev);

	err = hci_remove_adv_instance(hdev, instance);
	if (!err)
		mgmt_advertising_removed(sk, hdev, instance);

	hci_dev_unlock(hdev);

	return err;
}

 
int hci_remove_advertising_sync(struct hci_dev *hdev, struct sock *sk,
				u8 instance, bool force)
{
	struct adv_info *next = NULL;
	int err;

	 
	if (!instance || hdev->cur_adv_instance == instance)
		cancel_adv_timeout(hdev);

	 
	if (hdev->cur_adv_instance == instance)
		next = hci_get_next_instance(hdev, instance);

	if (!instance) {
		err = hci_clear_adv_sync(hdev, sk, force);
		if (err)
			return err;
	} else {
		struct adv_info *adv = hci_find_adv_instance(hdev, instance);

		if (force || (adv && adv->timeout && !adv->remaining_time)) {
			 
			if (next && next->instance == instance)
				next = NULL;

			err = hci_remove_adv_sync(hdev, instance, sk);
			if (err)
				return err;
		}
	}

	if (!hdev_is_powered(hdev) || hci_dev_test_flag(hdev, HCI_ADVERTISING))
		return 0;

	if (next && !ext_adv_capable(hdev))
		hci_schedule_adv_instance_sync(hdev, next->instance, false);

	return 0;
}

int hci_read_rssi_sync(struct hci_dev *hdev, __le16 handle)
{
	struct hci_cp_read_rssi cp;

	cp.handle = handle;
	return __hci_cmd_sync_status(hdev, HCI_OP_READ_RSSI,
					sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

int hci_read_clock_sync(struct hci_dev *hdev, struct hci_cp_read_clock *cp)
{
	return __hci_cmd_sync_status(hdev, HCI_OP_READ_CLOCK,
					sizeof(*cp), cp, HCI_CMD_TIMEOUT);
}

int hci_read_tx_power_sync(struct hci_dev *hdev, __le16 handle, u8 type)
{
	struct hci_cp_read_tx_power cp;

	cp.handle = handle;
	cp.type = type;
	return __hci_cmd_sync_status(hdev, HCI_OP_READ_TX_POWER,
					sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

int hci_disable_advertising_sync(struct hci_dev *hdev)
{
	u8 enable = 0x00;
	int err = 0;

	 
	if (!hci_dev_test_flag(hdev, HCI_LE_ADV))
		return 0;

	if (ext_adv_capable(hdev))
		err = hci_disable_ext_adv_instance_sync(hdev, 0x00);
	if (ext_adv_capable(hdev))
		return err;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_ADV_ENABLE,
				     sizeof(enable), &enable, HCI_CMD_TIMEOUT);
}

static int hci_le_set_ext_scan_enable_sync(struct hci_dev *hdev, u8 val,
					   u8 filter_dup)
{
	struct hci_cp_le_set_ext_scan_enable cp;

	memset(&cp, 0, sizeof(cp));
	cp.enable = val;

	if (hci_dev_test_flag(hdev, HCI_MESH))
		cp.filter_dup = LE_SCAN_FILTER_DUP_DISABLE;
	else
		cp.filter_dup = filter_dup;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_EXT_SCAN_ENABLE,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

static int hci_le_set_scan_enable_sync(struct hci_dev *hdev, u8 val,
				       u8 filter_dup)
{
	struct hci_cp_le_set_scan_enable cp;

	if (use_ext_scan(hdev))
		return hci_le_set_ext_scan_enable_sync(hdev, val, filter_dup);

	memset(&cp, 0, sizeof(cp));
	cp.enable = val;

	if (val && hci_dev_test_flag(hdev, HCI_MESH))
		cp.filter_dup = LE_SCAN_FILTER_DUP_DISABLE;
	else
		cp.filter_dup = filter_dup;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_SCAN_ENABLE,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

static int hci_le_set_addr_resolution_enable_sync(struct hci_dev *hdev, u8 val)
{
	if (!use_ll_privacy(hdev))
		return 0;

	 
	if (val == hci_dev_test_flag(hdev, HCI_LL_RPA_RESOLUTION))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_ADDR_RESOLV_ENABLE,
				     sizeof(val), &val, HCI_CMD_TIMEOUT);
}

static int hci_scan_disable_sync(struct hci_dev *hdev)
{
	int err;

	 
	if (!hci_dev_test_flag(hdev, HCI_LE_SCAN))
		return 0;

	if (hdev->scanning_paused) {
		bt_dev_dbg(hdev, "Scanning is paused for suspend");
		return 0;
	}

	err = hci_le_set_scan_enable_sync(hdev, LE_SCAN_DISABLE, 0x00);
	if (err) {
		bt_dev_err(hdev, "Unable to disable scanning: %d", err);
		return err;
	}

	return err;
}

static bool scan_use_rpa(struct hci_dev *hdev)
{
	return hci_dev_test_flag(hdev, HCI_PRIVACY);
}

static void hci_start_interleave_scan(struct hci_dev *hdev)
{
	hdev->interleave_scan_state = INTERLEAVE_SCAN_NO_FILTER;
	queue_delayed_work(hdev->req_workqueue,
			   &hdev->interleave_scan, 0);
}

static bool is_interleave_scanning(struct hci_dev *hdev)
{
	return hdev->interleave_scan_state != INTERLEAVE_SCAN_NONE;
}

static void cancel_interleave_scan(struct hci_dev *hdev)
{
	bt_dev_dbg(hdev, "cancelling interleave scan");

	cancel_delayed_work_sync(&hdev->interleave_scan);

	hdev->interleave_scan_state = INTERLEAVE_SCAN_NONE;
}

 
static bool hci_update_interleaved_scan_sync(struct hci_dev *hdev)
{
	 
	bool use_interleaving = hci_is_adv_monitoring(hdev) &&
				!(list_empty(&hdev->pend_le_conns) &&
				  list_empty(&hdev->pend_le_reports)) &&
				hci_get_adv_monitor_offload_ext(hdev) ==
				    HCI_ADV_MONITOR_EXT_NONE;
	bool is_interleaving = is_interleave_scanning(hdev);

	if (use_interleaving && !is_interleaving) {
		hci_start_interleave_scan(hdev);
		bt_dev_dbg(hdev, "starting interleave scan");
		return true;
	}

	if (!use_interleaving && is_interleaving)
		cancel_interleave_scan(hdev);

	return false;
}

 
static int hci_le_del_resolve_list_sync(struct hci_dev *hdev,
					bdaddr_t *bdaddr, u8 bdaddr_type)
{
	struct hci_cp_le_del_from_resolv_list cp;
	struct bdaddr_list_with_irk *entry;

	if (!use_ll_privacy(hdev))
		return 0;

	 
	entry = hci_bdaddr_list_lookup_with_irk(&hdev->le_resolv_list, bdaddr,
						bdaddr_type);
	if (!entry)
		return 0;

	cp.bdaddr_type = bdaddr_type;
	bacpy(&cp.bdaddr, bdaddr);

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_DEL_FROM_RESOLV_LIST,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

static int hci_le_del_accept_list_sync(struct hci_dev *hdev,
				       bdaddr_t *bdaddr, u8 bdaddr_type)
{
	struct hci_cp_le_del_from_accept_list cp;
	int err;

	 
	if (!hci_bdaddr_list_lookup(&hdev->le_accept_list, bdaddr, bdaddr_type))
		return 0;

	cp.bdaddr_type = bdaddr_type;
	bacpy(&cp.bdaddr, bdaddr);

	 
	hci_le_del_resolve_list_sync(hdev, &cp.bdaddr, cp.bdaddr_type);

	err = __hci_cmd_sync_status(hdev, HCI_OP_LE_DEL_FROM_ACCEPT_LIST,
				    sizeof(cp), &cp, HCI_CMD_TIMEOUT);
	if (err) {
		bt_dev_err(hdev, "Unable to remove from allow list: %d", err);
		return err;
	}

	bt_dev_dbg(hdev, "Remove %pMR (0x%x) from allow list", &cp.bdaddr,
		   cp.bdaddr_type);

	return 0;
}

struct conn_params {
	bdaddr_t addr;
	u8 addr_type;
	hci_conn_flags_t flags;
	u8 privacy_mode;
};

 
static int hci_le_add_resolve_list_sync(struct hci_dev *hdev,
					struct conn_params *params)
{
	struct hci_cp_le_add_to_resolv_list cp;
	struct smp_irk *irk;
	struct bdaddr_list_with_irk *entry;
	struct hci_conn_params *p;

	if (!use_ll_privacy(hdev))
		return 0;

	 
	if (!params) {
		if (!hci_dev_test_flag(hdev, HCI_PRIVACY))
			return 0;

		hci_copy_identity_address(hdev, &cp.bdaddr, &cp.bdaddr_type);
		memcpy(cp.peer_irk, hdev->irk, 16);
		goto done;
	}

	irk = hci_find_irk_by_addr(hdev, &params->addr, params->addr_type);
	if (!irk)
		return 0;

	 
	entry = hci_bdaddr_list_lookup_with_irk(&hdev->le_resolv_list,
						&params->addr,
						params->addr_type);
	if (entry)
		return 0;

	cp.bdaddr_type = params->addr_type;
	bacpy(&cp.bdaddr, &params->addr);
	memcpy(cp.peer_irk, irk->val, 16);

	 
	params->privacy_mode = HCI_NETWORK_PRIVACY;

	rcu_read_lock();
	p = hci_pend_le_action_lookup(&hdev->pend_le_conns,
				      &params->addr, params->addr_type);
	if (!p)
		p = hci_pend_le_action_lookup(&hdev->pend_le_reports,
					      &params->addr, params->addr_type);
	if (p)
		WRITE_ONCE(p->privacy_mode, HCI_NETWORK_PRIVACY);
	rcu_read_unlock();

done:
	if (hci_dev_test_flag(hdev, HCI_PRIVACY))
		memcpy(cp.local_irk, hdev->irk, 16);
	else
		memset(cp.local_irk, 0, 16);

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_ADD_TO_RESOLV_LIST,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

 
static int hci_le_set_privacy_mode_sync(struct hci_dev *hdev,
					struct conn_params *params)
{
	struct hci_cp_le_set_privacy_mode cp;
	struct smp_irk *irk;

	 
	if (params->privacy_mode == HCI_DEVICE_PRIVACY)
		return 0;

	 
	if (!(params->flags & HCI_CONN_FLAG_DEVICE_PRIVACY))
		return 0;

	irk = hci_find_irk_by_addr(hdev, &params->addr, params->addr_type);
	if (!irk)
		return 0;

	memset(&cp, 0, sizeof(cp));
	cp.bdaddr_type = irk->addr_type;
	bacpy(&cp.bdaddr, &irk->bdaddr);
	cp.mode = HCI_DEVICE_PRIVACY;

	 

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_PRIVACY_MODE,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

 
static int hci_le_add_accept_list_sync(struct hci_dev *hdev,
				       struct conn_params *params,
				       u8 *num_entries)
{
	struct hci_cp_le_add_to_accept_list cp;
	int err;

	 
	if (hdev->suspended &&
	    !(params->flags & HCI_CONN_FLAG_REMOTE_WAKEUP))
		return 0;

	 
	if (*num_entries >= hdev->le_accept_list_size)
		return -ENOSPC;

	 
	if (!use_ll_privacy(hdev) &&
	    hci_find_irk_by_addr(hdev, &params->addr, params->addr_type))
		return -EINVAL;

	 
	err = hci_le_add_resolve_list_sync(hdev, params);
	if (err) {
		bt_dev_err(hdev, "Unable to add to resolve list: %d", err);
		return err;
	}

	 
	err = hci_le_set_privacy_mode_sync(hdev, params);
	if (err) {
		bt_dev_err(hdev, "Unable to set privacy mode: %d", err);
		return err;
	}

	 
	if (hci_bdaddr_list_lookup(&hdev->le_accept_list, &params->addr,
				   params->addr_type))
		return 0;

	*num_entries += 1;
	cp.bdaddr_type = params->addr_type;
	bacpy(&cp.bdaddr, &params->addr);

	err = __hci_cmd_sync_status(hdev, HCI_OP_LE_ADD_TO_ACCEPT_LIST,
				    sizeof(cp), &cp, HCI_CMD_TIMEOUT);
	if (err) {
		bt_dev_err(hdev, "Unable to add to allow list: %d", err);
		 
		hci_le_del_resolve_list_sync(hdev, &cp.bdaddr, cp.bdaddr_type);
		return err;
	}

	bt_dev_dbg(hdev, "Add %pMR (0x%x) to allow list", &cp.bdaddr,
		   cp.bdaddr_type);

	return 0;
}

 
static int hci_pause_advertising_sync(struct hci_dev *hdev)
{
	int err;
	int old_state;

	 
	if (hdev->advertising_paused)
		return 0;

	bt_dev_dbg(hdev, "Pausing directed advertising");

	 
	old_state = hci_dev_test_flag(hdev, HCI_ADVERTISING);
	if (old_state) {
		 
		hci_dev_clear_flag(hdev, HCI_LIMITED_DISCOVERABLE);
		hci_dev_clear_flag(hdev, HCI_DISCOVERABLE);
		hdev->discov_timeout = 0;
	}

	bt_dev_dbg(hdev, "Pausing advertising instances");

	 
	err = hci_disable_advertising_sync(hdev);
	if (err)
		return err;

	 
	if (!ext_adv_capable(hdev))
		cancel_adv_timeout(hdev);

	hdev->advertising_paused = true;
	hdev->advertising_old_state = old_state;

	return 0;
}

 
static int hci_resume_advertising_sync(struct hci_dev *hdev)
{
	struct adv_info *adv, *tmp;
	int err;

	 
	if (!hdev->advertising_paused)
		return 0;

	 
	hdev->advertising_paused = false;
	if (hdev->advertising_old_state) {
		hci_dev_set_flag(hdev, HCI_ADVERTISING);
		hdev->advertising_old_state = 0;
	}

	bt_dev_dbg(hdev, "Resuming advertising instances");

	if (ext_adv_capable(hdev)) {
		 
		list_for_each_entry_safe(adv, tmp, &hdev->adv_instances, list) {
			err = hci_enable_ext_advertising_sync(hdev,
							      adv->instance);
			if (!err)
				continue;

			 
			hci_remove_ext_adv_instance_sync(hdev, adv->instance,
							 NULL);
		}
	} else {
		 
		err = hci_schedule_adv_instance_sync(hdev,
						     hdev->cur_adv_instance,
						     true);
	}

	hdev->advertising_paused = false;

	return err;
}

static int hci_pause_addr_resolution(struct hci_dev *hdev)
{
	int err;

	if (!use_ll_privacy(hdev))
		return 0;

	if (!hci_dev_test_flag(hdev, HCI_LL_RPA_RESOLUTION))
		return 0;

	 
	if (hci_dev_test_flag(hdev, HCI_LE_SCAN) ||
	    hci_lookup_le_connect(hdev)) {
		bt_dev_err(hdev, "Command not allowed when scan/LE connect");
		return -EPERM;
	}

	 
	err = hci_pause_advertising_sync(hdev);
	if (err) {
		bt_dev_err(hdev, "Pause advertising failed: %d", err);
		return err;
	}

	err = hci_le_set_addr_resolution_enable_sync(hdev, 0x00);
	if (err)
		bt_dev_err(hdev, "Unable to disable Address Resolution: %d",
			   err);

	 
	if (!err && scan_use_rpa(hdev))
		return 0;

	hci_resume_advertising_sync(hdev);
	return err;
}

struct sk_buff *hci_read_local_oob_data_sync(struct hci_dev *hdev,
					     bool extended, struct sock *sk)
{
	u16 opcode = extended ? HCI_OP_READ_LOCAL_OOB_EXT_DATA :
					HCI_OP_READ_LOCAL_OOB_DATA;

	return __hci_cmd_sync_sk(hdev, opcode, 0, NULL, 0, HCI_CMD_TIMEOUT, sk);
}

static struct conn_params *conn_params_copy(struct list_head *list, size_t *n)
{
	struct hci_conn_params *params;
	struct conn_params *p;
	size_t i;

	rcu_read_lock();

	i = 0;
	list_for_each_entry_rcu(params, list, action)
		++i;
	*n = i;

	rcu_read_unlock();

	p = kvcalloc(*n, sizeof(struct conn_params), GFP_KERNEL);
	if (!p)
		return NULL;

	rcu_read_lock();

	i = 0;
	list_for_each_entry_rcu(params, list, action) {
		 
		if (i >= *n)
			break;

		 
		bacpy(&p[i].addr, &params->addr);
		p[i].addr_type = params->addr_type;
		p[i].flags = READ_ONCE(params->flags);
		p[i].privacy_mode = READ_ONCE(params->privacy_mode);
		++i;
	}

	rcu_read_unlock();

	*n = i;
	return p;
}

 
static u8 hci_update_accept_list_sync(struct hci_dev *hdev)
{
	struct conn_params *params;
	struct bdaddr_list *b, *t;
	u8 num_entries = 0;
	bool pend_conn, pend_report;
	u8 filter_policy;
	size_t i, n;
	int err;

	 
	if (use_ll_privacy(hdev)) {
		err = hci_pause_advertising_sync(hdev);
		if (err) {
			bt_dev_err(hdev, "pause advertising failed: %d", err);
			return 0x00;
		}
	}

	 
	err = hci_le_set_addr_resolution_enable_sync(hdev, 0x00);
	if (err) {
		bt_dev_err(hdev, "Unable to disable LL privacy: %d", err);
		goto done;
	}

	 
	list_for_each_entry_safe(b, t, &hdev->le_accept_list, list) {
		if (hci_conn_hash_lookup_le(hdev, &b->bdaddr, b->bdaddr_type))
			continue;

		 
		pend_conn = hci_pend_le_action_lookup(&hdev->pend_le_conns,
						      &b->bdaddr,
						      b->bdaddr_type);
		pend_report = hci_pend_le_action_lookup(&hdev->pend_le_reports,
							&b->bdaddr,
							b->bdaddr_type);

		 
		if (!pend_conn && !pend_report) {
			hci_le_del_accept_list_sync(hdev, &b->bdaddr,
						    b->bdaddr_type);
			continue;
		}

		num_entries++;
	}

	 

	params = conn_params_copy(&hdev->pend_le_conns, &n);
	if (!params) {
		err = -ENOMEM;
		goto done;
	}

	for (i = 0; i < n; ++i) {
		err = hci_le_add_accept_list_sync(hdev, &params[i],
						  &num_entries);
		if (err) {
			kvfree(params);
			goto done;
		}
	}

	kvfree(params);

	 

	params = conn_params_copy(&hdev->pend_le_reports, &n);
	if (!params) {
		err = -ENOMEM;
		goto done;
	}

	for (i = 0; i < n; ++i) {
		err = hci_le_add_accept_list_sync(hdev, &params[i],
						  &num_entries);
		if (err) {
			kvfree(params);
			goto done;
		}
	}

	kvfree(params);

	 
	if (!idr_is_empty(&hdev->adv_monitors_idr) && !hdev->suspended &&
	    hci_get_adv_monitor_offload_ext(hdev) == HCI_ADV_MONITOR_EXT_NONE &&
	    hdev->interleave_scan_state != INTERLEAVE_SCAN_ALLOWLIST)
		err = -EINVAL;

done:
	filter_policy = err ? 0x00 : 0x01;

	 
	err = hci_le_set_addr_resolution_enable_sync(hdev, 0x01);
	if (err)
		bt_dev_err(hdev, "Unable to enable LL privacy: %d", err);

	 
	if (use_ll_privacy(hdev))
		hci_resume_advertising_sync(hdev);

	 
	return filter_policy;
}

static int hci_le_set_ext_scan_param_sync(struct hci_dev *hdev, u8 type,
					  u16 interval, u16 window,
					  u8 own_addr_type, u8 filter_policy)
{
	struct hci_cp_le_set_ext_scan_params *cp;
	struct hci_cp_le_scan_phy_params *phy;
	u8 data[sizeof(*cp) + sizeof(*phy) * 2];
	u8 num_phy = 0;

	cp = (void *)data;
	phy = (void *)cp->data;

	memset(data, 0, sizeof(data));

	cp->own_addr_type = own_addr_type;
	cp->filter_policy = filter_policy;

	if (scan_1m(hdev) || scan_2m(hdev)) {
		cp->scanning_phys |= LE_SCAN_PHY_1M;

		phy->type = type;
		phy->interval = cpu_to_le16(interval);
		phy->window = cpu_to_le16(window);

		num_phy++;
		phy++;
	}

	if (scan_coded(hdev)) {
		cp->scanning_phys |= LE_SCAN_PHY_CODED;

		phy->type = type;
		phy->interval = cpu_to_le16(interval);
		phy->window = cpu_to_le16(window);

		num_phy++;
		phy++;
	}

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_EXT_SCAN_PARAMS,
				     sizeof(*cp) + sizeof(*phy) * num_phy,
				     data, HCI_CMD_TIMEOUT);
}

static int hci_le_set_scan_param_sync(struct hci_dev *hdev, u8 type,
				      u16 interval, u16 window,
				      u8 own_addr_type, u8 filter_policy)
{
	struct hci_cp_le_set_scan_param cp;

	if (use_ext_scan(hdev))
		return hci_le_set_ext_scan_param_sync(hdev, type, interval,
						      window, own_addr_type,
						      filter_policy);

	memset(&cp, 0, sizeof(cp));
	cp.type = type;
	cp.interval = cpu_to_le16(interval);
	cp.window = cpu_to_le16(window);
	cp.own_address_type = own_addr_type;
	cp.filter_policy = filter_policy;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_SCAN_PARAM,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

static int hci_start_scan_sync(struct hci_dev *hdev, u8 type, u16 interval,
			       u16 window, u8 own_addr_type, u8 filter_policy,
			       u8 filter_dup)
{
	int err;

	if (hdev->scanning_paused) {
		bt_dev_dbg(hdev, "Scanning is paused for suspend");
		return 0;
	}

	err = hci_le_set_scan_param_sync(hdev, type, interval, window,
					 own_addr_type, filter_policy);
	if (err)
		return err;

	return hci_le_set_scan_enable_sync(hdev, LE_SCAN_ENABLE, filter_dup);
}

static int hci_passive_scan_sync(struct hci_dev *hdev)
{
	u8 own_addr_type;
	u8 filter_policy;
	u16 window, interval;
	u8 filter_dups = LE_SCAN_FILTER_DUP_ENABLE;
	int err;

	if (hdev->scanning_paused) {
		bt_dev_dbg(hdev, "Scanning is paused for suspend");
		return 0;
	}

	err = hci_scan_disable_sync(hdev);
	if (err) {
		bt_dev_err(hdev, "disable scanning failed: %d", err);
		return err;
	}

	 
	if (hci_update_random_address_sync(hdev, false, scan_use_rpa(hdev),
					   &own_addr_type))
		return 0;

	if (hdev->enable_advmon_interleave_scan &&
	    hci_update_interleaved_scan_sync(hdev))
		return 0;

	bt_dev_dbg(hdev, "interleave state %d", hdev->interleave_scan_state);

	 
	filter_policy = hci_update_accept_list_sync(hdev);

	 
	if (hci_dev_test_flag(hdev, HCI_PRIVACY) &&
	    (hdev->le_features[0] & HCI_LE_EXT_SCAN_POLICY))
		filter_policy |= 0x02;

	if (hdev->suspended) {
		window = hdev->le_scan_window_suspend;
		interval = hdev->le_scan_int_suspend;
	} else if (hci_is_le_conn_scanning(hdev)) {
		window = hdev->le_scan_window_connect;
		interval = hdev->le_scan_int_connect;
	} else if (hci_is_adv_monitoring(hdev)) {
		window = hdev->le_scan_window_adv_monitor;
		interval = hdev->le_scan_int_adv_monitor;
	} else {
		window = hdev->le_scan_window;
		interval = hdev->le_scan_interval;
	}

	 
	if (hci_dev_test_flag(hdev, HCI_MESH)) {
		filter_policy = 0;
		filter_dups = LE_SCAN_FILTER_DUP_DISABLE;
	}

	bt_dev_dbg(hdev, "LE passive scan with acceptlist = %d", filter_policy);

	return hci_start_scan_sync(hdev, LE_SCAN_PASSIVE, interval, window,
				   own_addr_type, filter_policy, filter_dups);
}

 
int hci_update_passive_scan_sync(struct hci_dev *hdev)
{
	int err;

	if (!test_bit(HCI_UP, &hdev->flags) ||
	    test_bit(HCI_INIT, &hdev->flags) ||
	    hci_dev_test_flag(hdev, HCI_SETUP) ||
	    hci_dev_test_flag(hdev, HCI_CONFIG) ||
	    hci_dev_test_flag(hdev, HCI_AUTO_OFF) ||
	    hci_dev_test_flag(hdev, HCI_UNREGISTER))
		return 0;

	 
	if (!hci_dev_test_flag(hdev, HCI_LE_ENABLED))
		return 0;

	 
	if (hdev->discovery.state != DISCOVERY_STOPPED)
		return 0;

	 
	hci_discovery_filter_clear(hdev);

	bt_dev_dbg(hdev, "ADV monitoring is %s",
		   hci_is_adv_monitoring(hdev) ? "on" : "off");

	if (!hci_dev_test_flag(hdev, HCI_MESH) &&
	    list_empty(&hdev->pend_le_conns) &&
	    list_empty(&hdev->pend_le_reports) &&
	    !hci_is_adv_monitoring(hdev) &&
	    !hci_dev_test_flag(hdev, HCI_PA_SYNC)) {
		 

		bt_dev_dbg(hdev, "stopping background scanning");

		err = hci_scan_disable_sync(hdev);
		if (err)
			bt_dev_err(hdev, "stop background scanning failed: %d",
				   err);
	} else {
		 

		 
		if (hci_lookup_le_connect(hdev))
			return 0;

		bt_dev_dbg(hdev, "start background scanning");

		err = hci_passive_scan_sync(hdev);
		if (err)
			bt_dev_err(hdev, "start background scanning failed: %d",
				   err);
	}

	return err;
}

static int update_scan_sync(struct hci_dev *hdev, void *data)
{
	return hci_update_scan_sync(hdev);
}

int hci_update_scan(struct hci_dev *hdev)
{
	return hci_cmd_sync_queue(hdev, update_scan_sync, NULL, NULL);
}

static int update_passive_scan_sync(struct hci_dev *hdev, void *data)
{
	return hci_update_passive_scan_sync(hdev);
}

int hci_update_passive_scan(struct hci_dev *hdev)
{
	 
	if (!test_bit(HCI_UP, &hdev->flags) ||
	    test_bit(HCI_INIT, &hdev->flags) ||
	    hci_dev_test_flag(hdev, HCI_SETUP) ||
	    hci_dev_test_flag(hdev, HCI_CONFIG) ||
	    hci_dev_test_flag(hdev, HCI_AUTO_OFF) ||
	    hci_dev_test_flag(hdev, HCI_UNREGISTER))
		return 0;

	return hci_cmd_sync_queue(hdev, update_passive_scan_sync, NULL, NULL);
}

int hci_write_sc_support_sync(struct hci_dev *hdev, u8 val)
{
	int err;

	if (!bredr_sc_enabled(hdev) || lmp_host_sc_capable(hdev))
		return 0;

	err = __hci_cmd_sync_status(hdev, HCI_OP_WRITE_SC_SUPPORT,
				    sizeof(val), &val, HCI_CMD_TIMEOUT);

	if (!err) {
		if (val) {
			hdev->features[1][0] |= LMP_HOST_SC;
			hci_dev_set_flag(hdev, HCI_SC_ENABLED);
		} else {
			hdev->features[1][0] &= ~LMP_HOST_SC;
			hci_dev_clear_flag(hdev, HCI_SC_ENABLED);
		}
	}

	return err;
}

int hci_write_ssp_mode_sync(struct hci_dev *hdev, u8 mode)
{
	int err;

	if (!hci_dev_test_flag(hdev, HCI_SSP_ENABLED) ||
	    lmp_host_ssp_capable(hdev))
		return 0;

	if (!mode && hci_dev_test_flag(hdev, HCI_USE_DEBUG_KEYS)) {
		__hci_cmd_sync_status(hdev, HCI_OP_WRITE_SSP_DEBUG_MODE,
				      sizeof(mode), &mode, HCI_CMD_TIMEOUT);
	}

	err = __hci_cmd_sync_status(hdev, HCI_OP_WRITE_SSP_MODE,
				    sizeof(mode), &mode, HCI_CMD_TIMEOUT);
	if (err)
		return err;

	return hci_write_sc_support_sync(hdev, 0x01);
}

int hci_write_le_host_supported_sync(struct hci_dev *hdev, u8 le, u8 simul)
{
	struct hci_cp_write_le_host_supported cp;

	if (!hci_dev_test_flag(hdev, HCI_LE_ENABLED) ||
	    !lmp_bredr_capable(hdev))
		return 0;

	 
	if (le == lmp_host_le_capable(hdev) &&
	    simul == lmp_host_le_br_capable(hdev))
		return 0;

	memset(&cp, 0, sizeof(cp));

	cp.le = le;
	cp.simul = simul;

	return __hci_cmd_sync_status(hdev, HCI_OP_WRITE_LE_HOST_SUPPORTED,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

static int hci_powered_update_adv_sync(struct hci_dev *hdev)
{
	struct adv_info *adv, *tmp;
	int err;

	if (!hci_dev_test_flag(hdev, HCI_LE_ENABLED))
		return 0;

	 
	if (!hci_dev_test_flag(hdev, HCI_LL_RPA_RESOLUTION)) {
		hci_le_add_resolve_list_sync(hdev, NULL);
		hci_le_set_addr_resolution_enable_sync(hdev, 0x01);
	}

	 
	if (hci_dev_test_flag(hdev, HCI_ADVERTISING) ||
	    list_empty(&hdev->adv_instances)) {
		if (ext_adv_capable(hdev)) {
			err = hci_setup_ext_adv_instance_sync(hdev, 0x00);
			if (!err)
				hci_update_scan_rsp_data_sync(hdev, 0x00);
		} else {
			err = hci_update_adv_data_sync(hdev, 0x00);
			if (!err)
				hci_update_scan_rsp_data_sync(hdev, 0x00);
		}

		if (hci_dev_test_flag(hdev, HCI_ADVERTISING))
			hci_enable_advertising_sync(hdev);
	}

	 
	list_for_each_entry_safe(adv, tmp, &hdev->adv_instances, list)
		hci_schedule_adv_instance_sync(hdev, adv->instance, true);

	return 0;
}

static int hci_write_auth_enable_sync(struct hci_dev *hdev)
{
	u8 link_sec;

	link_sec = hci_dev_test_flag(hdev, HCI_LINK_SECURITY);
	if (link_sec == test_bit(HCI_AUTH, &hdev->flags))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_WRITE_AUTH_ENABLE,
				     sizeof(link_sec), &link_sec,
				     HCI_CMD_TIMEOUT);
}

int hci_write_fast_connectable_sync(struct hci_dev *hdev, bool enable)
{
	struct hci_cp_write_page_scan_activity cp;
	u8 type;
	int err = 0;

	if (!hci_dev_test_flag(hdev, HCI_BREDR_ENABLED))
		return 0;

	if (hdev->hci_ver < BLUETOOTH_VER_1_2)
		return 0;

	memset(&cp, 0, sizeof(cp));

	if (enable) {
		type = PAGE_SCAN_TYPE_INTERLACED;

		 
		cp.interval = cpu_to_le16(0x0100);
	} else {
		type = hdev->def_page_scan_type;
		cp.interval = cpu_to_le16(hdev->def_page_scan_int);
	}

	cp.window = cpu_to_le16(hdev->def_page_scan_window);

	if (__cpu_to_le16(hdev->page_scan_interval) != cp.interval ||
	    __cpu_to_le16(hdev->page_scan_window) != cp.window) {
		err = __hci_cmd_sync_status(hdev,
					    HCI_OP_WRITE_PAGE_SCAN_ACTIVITY,
					    sizeof(cp), &cp, HCI_CMD_TIMEOUT);
		if (err)
			return err;
	}

	if (hdev->page_scan_type != type)
		err = __hci_cmd_sync_status(hdev,
					    HCI_OP_WRITE_PAGE_SCAN_TYPE,
					    sizeof(type), &type,
					    HCI_CMD_TIMEOUT);

	return err;
}

static bool disconnected_accept_list_entries(struct hci_dev *hdev)
{
	struct bdaddr_list *b;

	list_for_each_entry(b, &hdev->accept_list, list) {
		struct hci_conn *conn;

		conn = hci_conn_hash_lookup_ba(hdev, ACL_LINK, &b->bdaddr);
		if (!conn)
			return true;

		if (conn->state != BT_CONNECTED && conn->state != BT_CONFIG)
			return true;
	}

	return false;
}

static int hci_write_scan_enable_sync(struct hci_dev *hdev, u8 val)
{
	return __hci_cmd_sync_status(hdev, HCI_OP_WRITE_SCAN_ENABLE,
					    sizeof(val), &val,
					    HCI_CMD_TIMEOUT);
}

int hci_update_scan_sync(struct hci_dev *hdev)
{
	u8 scan;

	if (!hci_dev_test_flag(hdev, HCI_BREDR_ENABLED))
		return 0;

	if (!hdev_is_powered(hdev))
		return 0;

	if (mgmt_powering_down(hdev))
		return 0;

	if (hdev->scanning_paused)
		return 0;

	if (hci_dev_test_flag(hdev, HCI_CONNECTABLE) ||
	    disconnected_accept_list_entries(hdev))
		scan = SCAN_PAGE;
	else
		scan = SCAN_DISABLED;

	if (hci_dev_test_flag(hdev, HCI_DISCOVERABLE))
		scan |= SCAN_INQUIRY;

	if (test_bit(HCI_PSCAN, &hdev->flags) == !!(scan & SCAN_PAGE) &&
	    test_bit(HCI_ISCAN, &hdev->flags) == !!(scan & SCAN_INQUIRY))
		return 0;

	return hci_write_scan_enable_sync(hdev, scan);
}

int hci_update_name_sync(struct hci_dev *hdev)
{
	struct hci_cp_write_local_name cp;

	memset(&cp, 0, sizeof(cp));

	memcpy(cp.name, hdev->dev_name, sizeof(cp.name));

	return __hci_cmd_sync_status(hdev, HCI_OP_WRITE_LOCAL_NAME,
					    sizeof(cp), &cp,
					    HCI_CMD_TIMEOUT);
}

 
int hci_powered_update_sync(struct hci_dev *hdev)
{
	int err;

	 
	smp_register(hdev);

	err = hci_write_ssp_mode_sync(hdev, 0x01);
	if (err)
		return err;

	err = hci_write_le_host_supported_sync(hdev, 0x01, 0x00);
	if (err)
		return err;

	err = hci_powered_update_adv_sync(hdev);
	if (err)
		return err;

	err = hci_write_auth_enable_sync(hdev);
	if (err)
		return err;

	if (lmp_bredr_capable(hdev)) {
		if (hci_dev_test_flag(hdev, HCI_FAST_CONNECTABLE))
			hci_write_fast_connectable_sync(hdev, true);
		else
			hci_write_fast_connectable_sync(hdev, false);
		hci_update_scan_sync(hdev);
		hci_update_class_sync(hdev);
		hci_update_name_sync(hdev);
		hci_update_eir_sync(hdev);
	}

	 
	if (hci_dev_test_flag(hdev, HCI_FORCE_STATIC_ADDR) ||
	    (!bacmp(&hdev->bdaddr, BDADDR_ANY) &&
	    !hci_dev_test_flag(hdev, HCI_BREDR_ENABLED))) {
		if (bacmp(&hdev->static_addr, BDADDR_ANY))
			return hci_set_random_addr_sync(hdev,
							&hdev->static_addr);
	}

	return 0;
}

 
static void hci_dev_get_bd_addr_from_property(struct hci_dev *hdev)
{
	struct fwnode_handle *fwnode = dev_fwnode(hdev->dev.parent);
	bdaddr_t ba;
	int ret;

	ret = fwnode_property_read_u8_array(fwnode, "local-bd-address",
					    (u8 *)&ba, sizeof(ba));
	if (ret < 0 || !bacmp(&ba, BDADDR_ANY))
		return;

	bacpy(&hdev->public_addr, &ba);
}

struct hci_init_stage {
	int (*func)(struct hci_dev *hdev);
};

 
static int hci_init_stage_sync(struct hci_dev *hdev,
			       const struct hci_init_stage *stage)
{
	size_t i;

	for (i = 0; stage[i].func; i++) {
		int err;

		err = stage[i].func(hdev);
		if (err)
			return err;
	}

	return 0;
}

 
static int hci_read_local_version_sync(struct hci_dev *hdev)
{
	return __hci_cmd_sync_status(hdev, HCI_OP_READ_LOCAL_VERSION,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static int hci_read_bd_addr_sync(struct hci_dev *hdev)
{
	return __hci_cmd_sync_status(hdev, HCI_OP_READ_BD_ADDR,
				     0, NULL, HCI_CMD_TIMEOUT);
}

#define HCI_INIT(_func) \
{ \
	.func = _func, \
}

static const struct hci_init_stage hci_init0[] = {
	 
	HCI_INIT(hci_read_local_version_sync),
	 
	HCI_INIT(hci_read_bd_addr_sync),
	{}
};

int hci_reset_sync(struct hci_dev *hdev)
{
	int err;

	set_bit(HCI_RESET, &hdev->flags);

	err = __hci_cmd_sync_status(hdev, HCI_OP_RESET, 0, NULL,
				    HCI_CMD_TIMEOUT);
	if (err)
		return err;

	return 0;
}

static int hci_init0_sync(struct hci_dev *hdev)
{
	int err;

	bt_dev_dbg(hdev, "");

	 
	if (!test_bit(HCI_QUIRK_RESET_ON_CLOSE, &hdev->quirks)) {
		err = hci_reset_sync(hdev);
		if (err)
			return err;
	}

	return hci_init_stage_sync(hdev, hci_init0);
}

static int hci_unconf_init_sync(struct hci_dev *hdev)
{
	int err;

	if (test_bit(HCI_QUIRK_RAW_DEVICE, &hdev->quirks))
		return 0;

	err = hci_init0_sync(hdev);
	if (err < 0)
		return err;

	if (hci_dev_test_flag(hdev, HCI_SETUP))
		hci_debugfs_create_basic(hdev);

	return 0;
}

 
static int hci_read_local_features_sync(struct hci_dev *hdev)
{
	  
	if (hdev->dev_type == HCI_AMP && !(hdev->commands[14] & 0x20))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_READ_LOCAL_FEATURES,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static const struct hci_init_stage br_init1[] = {
	 
	HCI_INIT(hci_read_local_features_sync),
	 
	HCI_INIT(hci_read_local_version_sync),
	 
	HCI_INIT(hci_read_bd_addr_sync),
	{}
};

 
static int hci_read_local_cmds_sync(struct hci_dev *hdev)
{
	 
	if (hdev->hci_ver > BLUETOOTH_VER_1_1 &&
	    !test_bit(HCI_QUIRK_BROKEN_LOCAL_COMMANDS, &hdev->quirks))
		return __hci_cmd_sync_status(hdev, HCI_OP_READ_LOCAL_COMMANDS,
					     0, NULL, HCI_CMD_TIMEOUT);

	return 0;
}

 
static int hci_read_local_amp_info_sync(struct hci_dev *hdev)
{
	return __hci_cmd_sync_status(hdev, HCI_OP_READ_LOCAL_AMP_INFO,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static int hci_read_data_block_size_sync(struct hci_dev *hdev)
{
	return __hci_cmd_sync_status(hdev, HCI_OP_READ_DATA_BLOCK_SIZE,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static int hci_read_flow_control_mode_sync(struct hci_dev *hdev)
{
	return __hci_cmd_sync_status(hdev, HCI_OP_READ_FLOW_CONTROL_MODE,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static int hci_read_location_data_sync(struct hci_dev *hdev)
{
	return __hci_cmd_sync_status(hdev, HCI_OP_READ_LOCATION_DATA,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static const struct hci_init_stage amp_init1[] = {
	 
	HCI_INIT(hci_read_local_version_sync),
	 
	HCI_INIT(hci_read_local_cmds_sync),
	 
	HCI_INIT(hci_read_local_amp_info_sync),
	 
	HCI_INIT(hci_read_data_block_size_sync),
	 
	HCI_INIT(hci_read_flow_control_mode_sync),
	 
	HCI_INIT(hci_read_location_data_sync),
	{}
};

static int hci_init1_sync(struct hci_dev *hdev)
{
	int err;

	bt_dev_dbg(hdev, "");

	 
	if (!test_bit(HCI_QUIRK_RESET_ON_CLOSE, &hdev->quirks)) {
		err = hci_reset_sync(hdev);
		if (err)
			return err;
	}

	switch (hdev->dev_type) {
	case HCI_PRIMARY:
		hdev->flow_ctl_mode = HCI_FLOW_CTL_MODE_PACKET_BASED;
		return hci_init_stage_sync(hdev, br_init1);
	case HCI_AMP:
		hdev->flow_ctl_mode = HCI_FLOW_CTL_MODE_BLOCK_BASED;
		return hci_init_stage_sync(hdev, amp_init1);
	default:
		bt_dev_err(hdev, "Unknown device type %d", hdev->dev_type);
		break;
	}

	return 0;
}

 
static const struct hci_init_stage amp_init2[] = {
	 
	HCI_INIT(hci_read_local_features_sync),
	{}
};

 
static int hci_read_buffer_size_sync(struct hci_dev *hdev)
{
	return __hci_cmd_sync_status(hdev, HCI_OP_READ_BUFFER_SIZE,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static int hci_read_dev_class_sync(struct hci_dev *hdev)
{
	return __hci_cmd_sync_status(hdev, HCI_OP_READ_CLASS_OF_DEV,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static int hci_read_local_name_sync(struct hci_dev *hdev)
{
	return __hci_cmd_sync_status(hdev, HCI_OP_READ_LOCAL_NAME,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static int hci_read_voice_setting_sync(struct hci_dev *hdev)
{
	return __hci_cmd_sync_status(hdev, HCI_OP_READ_VOICE_SETTING,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static int hci_read_num_supported_iac_sync(struct hci_dev *hdev)
{
	return __hci_cmd_sync_status(hdev, HCI_OP_READ_NUM_SUPPORTED_IAC,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static int hci_read_current_iac_lap_sync(struct hci_dev *hdev)
{
	return __hci_cmd_sync_status(hdev, HCI_OP_READ_CURRENT_IAC_LAP,
				     0, NULL, HCI_CMD_TIMEOUT);
}

static int hci_set_event_filter_sync(struct hci_dev *hdev, u8 flt_type,
				     u8 cond_type, bdaddr_t *bdaddr,
				     u8 auto_accept)
{
	struct hci_cp_set_event_filter cp;

	if (!hci_dev_test_flag(hdev, HCI_BREDR_ENABLED))
		return 0;

	if (test_bit(HCI_QUIRK_BROKEN_FILTER_CLEAR_ALL, &hdev->quirks))
		return 0;

	memset(&cp, 0, sizeof(cp));
	cp.flt_type = flt_type;

	if (flt_type != HCI_FLT_CLEAR_ALL) {
		cp.cond_type = cond_type;
		bacpy(&cp.addr_conn_flt.bdaddr, bdaddr);
		cp.addr_conn_flt.auto_accept = auto_accept;
	}

	return __hci_cmd_sync_status(hdev, HCI_OP_SET_EVENT_FLT,
				     flt_type == HCI_FLT_CLEAR_ALL ?
				     sizeof(cp.flt_type) : sizeof(cp), &cp,
				     HCI_CMD_TIMEOUT);
}

static int hci_clear_event_filter_sync(struct hci_dev *hdev)
{
	if (!hci_dev_test_flag(hdev, HCI_EVENT_FILTER_CONFIGURED))
		return 0;

	 
	if (test_bit(HCI_QUIRK_BROKEN_FILTER_CLEAR_ALL, &hdev->quirks))
		return 0;

	return hci_set_event_filter_sync(hdev, HCI_FLT_CLEAR_ALL, 0x00,
					 BDADDR_ANY, 0x00);
}

 
static int hci_write_ca_timeout_sync(struct hci_dev *hdev)
{
	__le16 param = cpu_to_le16(0x7d00);

	return __hci_cmd_sync_status(hdev, HCI_OP_WRITE_CA_TIMEOUT,
				     sizeof(param), &param, HCI_CMD_TIMEOUT);
}

 
static const struct hci_init_stage br_init2[] = {
	 
	HCI_INIT(hci_read_buffer_size_sync),
	 
	HCI_INIT(hci_read_dev_class_sync),
	 
	HCI_INIT(hci_read_local_name_sync),
	 
	HCI_INIT(hci_read_voice_setting_sync),
	 
	HCI_INIT(hci_read_num_supported_iac_sync),
	 
	HCI_INIT(hci_read_current_iac_lap_sync),
	 
	HCI_INIT(hci_clear_event_filter_sync),
	 
	HCI_INIT(hci_write_ca_timeout_sync),
	{}
};

static int hci_write_ssp_mode_1_sync(struct hci_dev *hdev)
{
	u8 mode = 0x01;

	if (!lmp_ssp_capable(hdev) || !hci_dev_test_flag(hdev, HCI_SSP_ENABLED))
		return 0;

	 
	hdev->max_page = 0x01;

	return __hci_cmd_sync_status(hdev, HCI_OP_WRITE_SSP_MODE,
				     sizeof(mode), &mode, HCI_CMD_TIMEOUT);
}

static int hci_write_eir_sync(struct hci_dev *hdev)
{
	struct hci_cp_write_eir cp;

	if (!lmp_ssp_capable(hdev) || hci_dev_test_flag(hdev, HCI_SSP_ENABLED))
		return 0;

	memset(hdev->eir, 0, sizeof(hdev->eir));
	memset(&cp, 0, sizeof(cp));

	return __hci_cmd_sync_status(hdev, HCI_OP_WRITE_EIR, sizeof(cp), &cp,
				     HCI_CMD_TIMEOUT);
}

static int hci_write_inquiry_mode_sync(struct hci_dev *hdev)
{
	u8 mode;

	if (!lmp_inq_rssi_capable(hdev) &&
	    !test_bit(HCI_QUIRK_FIXUP_INQUIRY_MODE, &hdev->quirks))
		return 0;

	 
	mode = lmp_ext_inq_capable(hdev) ? 0x02 : 0x01;

	return __hci_cmd_sync_status(hdev, HCI_OP_WRITE_INQUIRY_MODE,
				     sizeof(mode), &mode, HCI_CMD_TIMEOUT);
}

static int hci_read_inq_rsp_tx_power_sync(struct hci_dev *hdev)
{
	if (!lmp_inq_tx_pwr_capable(hdev))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_READ_INQ_RSP_TX_POWER,
				     0, NULL, HCI_CMD_TIMEOUT);
}

static int hci_read_local_ext_features_sync(struct hci_dev *hdev, u8 page)
{
	struct hci_cp_read_local_ext_features cp;

	if (!lmp_ext_feat_capable(hdev))
		return 0;

	memset(&cp, 0, sizeof(cp));
	cp.page = page;

	return __hci_cmd_sync_status(hdev, HCI_OP_READ_LOCAL_EXT_FEATURES,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

static int hci_read_local_ext_features_1_sync(struct hci_dev *hdev)
{
	return hci_read_local_ext_features_sync(hdev, 0x01);
}

 
static const struct hci_init_stage hci_init2[] = {
	 
	HCI_INIT(hci_read_local_cmds_sync),
	 
	HCI_INIT(hci_write_ssp_mode_1_sync),
	 
	HCI_INIT(hci_write_eir_sync),
	 
	HCI_INIT(hci_write_inquiry_mode_sync),
	 
	HCI_INIT(hci_read_inq_rsp_tx_power_sync),
	 
	HCI_INIT(hci_read_local_ext_features_1_sync),
	 
	HCI_INIT(hci_write_auth_enable_sync),
	{}
};

 
static int hci_le_read_buffer_size_sync(struct hci_dev *hdev)
{
	 
	if (iso_capable(hdev) && hdev->commands[41] & 0x20)
		return __hci_cmd_sync_status(hdev,
					     HCI_OP_LE_READ_BUFFER_SIZE_V2,
					     0, NULL, HCI_CMD_TIMEOUT);

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_READ_BUFFER_SIZE,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static int hci_le_read_local_features_sync(struct hci_dev *hdev)
{
	return __hci_cmd_sync_status(hdev, HCI_OP_LE_READ_LOCAL_FEATURES,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static int hci_le_read_supported_states_sync(struct hci_dev *hdev)
{
	return __hci_cmd_sync_status(hdev, HCI_OP_LE_READ_SUPPORTED_STATES,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static const struct hci_init_stage le_init2[] = {
	 
	HCI_INIT(hci_le_read_local_features_sync),
	 
	HCI_INIT(hci_le_read_buffer_size_sync),
	 
	HCI_INIT(hci_le_read_supported_states_sync),
	{}
};

static int hci_init2_sync(struct hci_dev *hdev)
{
	int err;

	bt_dev_dbg(hdev, "");

	if (hdev->dev_type == HCI_AMP)
		return hci_init_stage_sync(hdev, amp_init2);

	err = hci_init_stage_sync(hdev, hci_init2);
	if (err)
		return err;

	if (lmp_bredr_capable(hdev)) {
		err = hci_init_stage_sync(hdev, br_init2);
		if (err)
			return err;
	} else {
		hci_dev_clear_flag(hdev, HCI_BREDR_ENABLED);
	}

	if (lmp_le_capable(hdev)) {
		err = hci_init_stage_sync(hdev, le_init2);
		if (err)
			return err;
		 
		if (!lmp_bredr_capable(hdev))
			hci_dev_set_flag(hdev, HCI_LE_ENABLED);
	}

	return 0;
}

static int hci_set_event_mask_sync(struct hci_dev *hdev)
{
	 
	u8 events[8] = { 0xff, 0xff, 0xfb, 0xff, 0x00, 0x00, 0x00, 0x00 };

	 
	if (hdev->hci_ver < BLUETOOTH_VER_1_2)
		return 0;

	if (lmp_bredr_capable(hdev)) {
		events[4] |= 0x01;  

		 
		if (hdev->suspended)
			events[0] &= 0xef;
	} else {
		 
		memset(events, 0, sizeof(events));
		events[1] |= 0x20;  
		events[1] |= 0x40;  
		events[1] |= 0x80;  

		 
		if (hdev->commands[0] & 0x20) {
			 
			if (!hdev->suspended)
				events[0] |= 0x10;  
			events[2] |= 0x04;  
			events[3] |= 0x02;  
		}

		 
		if (hdev->commands[2] & 0x80)
			events[1] |= 0x08;  

		if (hdev->le_features[0] & HCI_LE_ENCRYPTION) {
			events[0] |= 0x80;  
			events[5] |= 0x80;  
		}
	}

	if (lmp_inq_rssi_capable(hdev) ||
	    test_bit(HCI_QUIRK_FIXUP_INQUIRY_MODE, &hdev->quirks))
		events[4] |= 0x02;  

	if (lmp_ext_feat_capable(hdev))
		events[4] |= 0x04;  

	if (lmp_esco_capable(hdev)) {
		events[5] |= 0x08;  
		events[5] |= 0x10;  
	}

	if (lmp_sniffsubr_capable(hdev))
		events[5] |= 0x20;  

	if (lmp_pause_enc_capable(hdev))
		events[5] |= 0x80;  

	if (lmp_ext_inq_capable(hdev))
		events[5] |= 0x40;  

	if (lmp_no_flush_capable(hdev))
		events[7] |= 0x01;  

	if (lmp_lsto_capable(hdev))
		events[6] |= 0x80;  

	if (lmp_ssp_capable(hdev)) {
		events[6] |= 0x01;	 
		events[6] |= 0x02;	 
		events[6] |= 0x04;	 
		events[6] |= 0x08;	 
		events[6] |= 0x10;	 
		events[6] |= 0x20;	 
		events[7] |= 0x04;	 
		events[7] |= 0x08;	 
		events[7] |= 0x10;	 
	}

	if (lmp_le_capable(hdev))
		events[7] |= 0x20;	 

	return __hci_cmd_sync_status(hdev, HCI_OP_SET_EVENT_MASK,
				     sizeof(events), events, HCI_CMD_TIMEOUT);
}

static int hci_read_stored_link_key_sync(struct hci_dev *hdev)
{
	struct hci_cp_read_stored_link_key cp;

	if (!(hdev->commands[6] & 0x20) ||
	    test_bit(HCI_QUIRK_BROKEN_STORED_LINK_KEY, &hdev->quirks))
		return 0;

	memset(&cp, 0, sizeof(cp));
	bacpy(&cp.bdaddr, BDADDR_ANY);
	cp.read_all = 0x01;

	return __hci_cmd_sync_status(hdev, HCI_OP_READ_STORED_LINK_KEY,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

static int hci_setup_link_policy_sync(struct hci_dev *hdev)
{
	struct hci_cp_write_def_link_policy cp;
	u16 link_policy = 0;

	if (!(hdev->commands[5] & 0x10))
		return 0;

	memset(&cp, 0, sizeof(cp));

	if (lmp_rswitch_capable(hdev))
		link_policy |= HCI_LP_RSWITCH;
	if (lmp_hold_capable(hdev))
		link_policy |= HCI_LP_HOLD;
	if (lmp_sniff_capable(hdev))
		link_policy |= HCI_LP_SNIFF;
	if (lmp_park_capable(hdev))
		link_policy |= HCI_LP_PARK;

	cp.policy = cpu_to_le16(link_policy);

	return __hci_cmd_sync_status(hdev, HCI_OP_WRITE_DEF_LINK_POLICY,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

static int hci_read_page_scan_activity_sync(struct hci_dev *hdev)
{
	if (!(hdev->commands[8] & 0x01))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_READ_PAGE_SCAN_ACTIVITY,
				     0, NULL, HCI_CMD_TIMEOUT);
}

static int hci_read_def_err_data_reporting_sync(struct hci_dev *hdev)
{
	if (!(hdev->commands[18] & 0x04) ||
	    !(hdev->features[0][6] & LMP_ERR_DATA_REPORTING) ||
	    test_bit(HCI_QUIRK_BROKEN_ERR_DATA_REPORTING, &hdev->quirks))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_READ_DEF_ERR_DATA_REPORTING,
				     0, NULL, HCI_CMD_TIMEOUT);
}

static int hci_read_page_scan_type_sync(struct hci_dev *hdev)
{
	 
	if (!(hdev->commands[13] & 0x01))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_READ_PAGE_SCAN_TYPE,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static int hci_read_local_ext_features_all_sync(struct hci_dev *hdev)
{
	u8 page;
	int err;

	if (!lmp_ext_feat_capable(hdev))
		return 0;

	for (page = 2; page < HCI_MAX_PAGES && page <= hdev->max_page;
	     page++) {
		err = hci_read_local_ext_features_sync(hdev, page);
		if (err)
			return err;
	}

	return 0;
}

 
static const struct hci_init_stage hci_init3[] = {
	 
	HCI_INIT(hci_set_event_mask_sync),
	 
	HCI_INIT(hci_read_stored_link_key_sync),
	 
	HCI_INIT(hci_setup_link_policy_sync),
	 
	HCI_INIT(hci_read_page_scan_activity_sync),
	 
	HCI_INIT(hci_read_def_err_data_reporting_sync),
	 
	HCI_INIT(hci_read_page_scan_type_sync),
	 
	HCI_INIT(hci_read_local_ext_features_all_sync),
	{}
};

static int hci_le_set_event_mask_sync(struct hci_dev *hdev)
{
	u8 events[8];

	if (!lmp_le_capable(hdev))
		return 0;

	memset(events, 0, sizeof(events));

	if (hdev->le_features[0] & HCI_LE_ENCRYPTION)
		events[0] |= 0x10;	 

	 
	if (hdev->le_features[0] & HCI_LE_CONN_PARAM_REQ_PROC)
		 
		events[0] |= 0x20;

	 
	if (hdev->le_features[0] & HCI_LE_DATA_LEN_EXT)
		events[0] |= 0x40;	 

	 
	if (use_enhanced_conn_complete(hdev))
		events[1] |= 0x02;	 

	 
	if (hdev->le_features[0] & HCI_LE_EXT_SCAN_POLICY)
		events[1] |= 0x04;	 

	 
	if (hdev->le_features[1] & HCI_LE_CHAN_SEL_ALG2)
		events[2] |= 0x08;	 

	 
	if (hdev->commands[26] & 0x08)
		events[0] |= 0x02;	 

	 
	if (hdev->commands[26] & 0x10)
		events[0] |= 0x01;	 

	 
	if (hdev->commands[27] & 0x04)
		events[0] |= 0x04;	 

	 
	if (hdev->commands[27] & 0x20)
		 
		events[0] |= 0x08;

	 
	if (hdev->commands[34] & 0x02)
		 
		events[0] |= 0x80;

	 
	if (hdev->commands[34] & 0x04)
		events[1] |= 0x01;	 

	 
	if (hdev->commands[35] & (0x20 | 0x40))
		events[1] |= 0x08;         

	 
	if (use_ext_scan(hdev))
		events[1] |= 0x10;	 

	 
	if (ext_adv_capable(hdev))
		events[2] |= 0x02;	 

	if (cis_capable(hdev)) {
		events[3] |= 0x01;	 
		if (cis_peripheral_capable(hdev))
			events[3] |= 0x02;  
	}

	if (bis_capable(hdev)) {
		events[1] |= 0x20;	 
		events[1] |= 0x40;	 
		events[3] |= 0x04;	 
		events[3] |= 0x08;	 
		events[3] |= 0x10;	 
		events[3] |= 0x20;	 
		events[4] |= 0x02;	 
	}

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_EVENT_MASK,
				     sizeof(events), events, HCI_CMD_TIMEOUT);
}

 
static int hci_le_read_adv_tx_power_sync(struct hci_dev *hdev)
{
	if ((hdev->commands[25] & 0x40) && !ext_adv_capable(hdev)) {
		 
		return __hci_cmd_sync_status(hdev,
					       HCI_OP_LE_READ_ADV_TX_POWER,
					       0, NULL, HCI_CMD_TIMEOUT);
	}

	return 0;
}

 
static int hci_le_read_tx_power_sync(struct hci_dev *hdev)
{
	if (!(hdev->commands[38] & 0x80) ||
	    test_bit(HCI_QUIRK_BROKEN_READ_TRANSMIT_POWER, &hdev->quirks))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_READ_TRANSMIT_POWER,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static int hci_le_read_accept_list_size_sync(struct hci_dev *hdev)
{
	if (!(hdev->commands[26] & 0x40))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_READ_ACCEPT_LIST_SIZE,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static int hci_le_clear_accept_list_sync(struct hci_dev *hdev)
{
	if (!(hdev->commands[26] & 0x80))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_CLEAR_ACCEPT_LIST, 0, NULL,
				     HCI_CMD_TIMEOUT);
}

 
static int hci_le_read_resolv_list_size_sync(struct hci_dev *hdev)
{
	if (!(hdev->commands[34] & 0x40))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_READ_RESOLV_LIST_SIZE,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static int hci_le_clear_resolv_list_sync(struct hci_dev *hdev)
{
	if (!(hdev->commands[34] & 0x20))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_CLEAR_RESOLV_LIST, 0, NULL,
				     HCI_CMD_TIMEOUT);
}

 
static int hci_le_set_rpa_timeout_sync(struct hci_dev *hdev)
{
	__le16 timeout = cpu_to_le16(hdev->rpa_timeout);

	if (!(hdev->commands[35] & 0x04) ||
	    test_bit(HCI_QUIRK_BROKEN_SET_RPA_TIMEOUT, &hdev->quirks))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_RPA_TIMEOUT,
				     sizeof(timeout), &timeout,
				     HCI_CMD_TIMEOUT);
}

 
static int hci_le_read_max_data_len_sync(struct hci_dev *hdev)
{
	if (!(hdev->le_features[0] & HCI_LE_DATA_LEN_EXT))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_READ_MAX_DATA_LEN, 0, NULL,
				     HCI_CMD_TIMEOUT);
}

 
static int hci_le_read_def_data_len_sync(struct hci_dev *hdev)
{
	if (!(hdev->le_features[0] & HCI_LE_DATA_LEN_EXT))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_READ_DEF_DATA_LEN, 0, NULL,
				     HCI_CMD_TIMEOUT);
}

 
static int hci_le_read_num_support_adv_sets_sync(struct hci_dev *hdev)
{
	if (!ext_adv_capable(hdev))
		return 0;

	return __hci_cmd_sync_status(hdev,
				     HCI_OP_LE_READ_NUM_SUPPORTED_ADV_SETS,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static int hci_set_le_support_sync(struct hci_dev *hdev)
{
	struct hci_cp_write_le_host_supported cp;

	 
	if (!lmp_bredr_capable(hdev))
		return 0;

	memset(&cp, 0, sizeof(cp));

	if (hci_dev_test_flag(hdev, HCI_LE_ENABLED)) {
		cp.le = 0x01;
		cp.simul = 0x00;
	}

	if (cp.le == lmp_host_le_capable(hdev))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_WRITE_LE_HOST_SUPPORTED,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

 
static int hci_le_set_host_feature_sync(struct hci_dev *hdev)
{
	struct hci_cp_le_set_host_feature cp;

	if (!iso_capable(hdev))
		return 0;

	memset(&cp, 0, sizeof(cp));

	 
	cp.bit_number = 32;
	cp.bit_value = 1;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_HOST_FEATURE,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

 
static const struct hci_init_stage le_init3[] = {
	 
	HCI_INIT(hci_le_set_event_mask_sync),
	 
	HCI_INIT(hci_le_read_adv_tx_power_sync),
	 
	HCI_INIT(hci_le_read_tx_power_sync),
	 
	HCI_INIT(hci_le_read_accept_list_size_sync),
	 
	HCI_INIT(hci_le_clear_accept_list_sync),
	 
	HCI_INIT(hci_le_read_resolv_list_size_sync),
	 
	HCI_INIT(hci_le_clear_resolv_list_sync),
	 
	HCI_INIT(hci_le_set_rpa_timeout_sync),
	 
	HCI_INIT(hci_le_read_max_data_len_sync),
	 
	HCI_INIT(hci_le_read_def_data_len_sync),
	 
	HCI_INIT(hci_le_read_num_support_adv_sets_sync),
	 
	HCI_INIT(hci_set_le_support_sync),
	 
	HCI_INIT(hci_le_set_host_feature_sync),
	{}
};

static int hci_init3_sync(struct hci_dev *hdev)
{
	int err;

	bt_dev_dbg(hdev, "");

	err = hci_init_stage_sync(hdev, hci_init3);
	if (err)
		return err;

	if (lmp_le_capable(hdev))
		return hci_init_stage_sync(hdev, le_init3);

	return 0;
}

static int hci_delete_stored_link_key_sync(struct hci_dev *hdev)
{
	struct hci_cp_delete_stored_link_key cp;

	 
	if (!(hdev->commands[6] & 0x80) ||
	    test_bit(HCI_QUIRK_BROKEN_STORED_LINK_KEY, &hdev->quirks))
		return 0;

	memset(&cp, 0, sizeof(cp));
	bacpy(&cp.bdaddr, BDADDR_ANY);
	cp.delete_all = 0x01;

	return __hci_cmd_sync_status(hdev, HCI_OP_DELETE_STORED_LINK_KEY,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

static int hci_set_event_mask_page_2_sync(struct hci_dev *hdev)
{
	u8 events[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	bool changed = false;

	 
	if (!(hdev->commands[22] & 0x04))
		return 0;

	 
	if (lmp_cpb_central_capable(hdev)) {
		events[1] |= 0x40;	 
		events[1] |= 0x80;	 
		events[2] |= 0x08;	 
		events[2] |= 0x20;	 
		changed = true;
	}

	 
	if (lmp_cpb_peripheral_capable(hdev)) {
		events[2] |= 0x01;	 
		events[2] |= 0x02;	 
		events[2] |= 0x04;	 
		events[2] |= 0x10;	 
		changed = true;
	}

	 
	if (lmp_ping_capable(hdev) || hdev->le_features[0] & HCI_LE_PING) {
		events[2] |= 0x80;
		changed = true;
	}

	 
	if (!changed)
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_SET_EVENT_MASK_PAGE_2,
				     sizeof(events), events, HCI_CMD_TIMEOUT);
}

 
static int hci_read_local_codecs_sync(struct hci_dev *hdev)
{
	if (hdev->commands[45] & 0x04)
		hci_read_supported_codecs_v2(hdev);
	else if (hdev->commands[29] & 0x20)
		hci_read_supported_codecs(hdev);

	return 0;
}

 
static int hci_read_local_pairing_opts_sync(struct hci_dev *hdev)
{
	if (!(hdev->commands[41] & 0x08))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_READ_LOCAL_PAIRING_OPTS,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static int hci_get_mws_transport_config_sync(struct hci_dev *hdev)
{
	if (!mws_transport_config_capable(hdev))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_GET_MWS_TRANSPORT_CONFIG,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static int hci_read_sync_train_params_sync(struct hci_dev *hdev)
{
	if (!lmp_sync_train_capable(hdev))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_READ_SYNC_TRAIN_PARAMS,
				     0, NULL, HCI_CMD_TIMEOUT);
}

 
static int hci_write_sc_support_1_sync(struct hci_dev *hdev)
{
	u8 support = 0x01;

	if (!hci_dev_test_flag(hdev, HCI_SSP_ENABLED) ||
	    !bredr_sc_enabled(hdev))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_WRITE_SC_SUPPORT,
				     sizeof(support), &support,
				     HCI_CMD_TIMEOUT);
}

 
static int hci_set_err_data_report_sync(struct hci_dev *hdev)
{
	struct hci_cp_write_def_err_data_reporting cp;
	bool enabled = hci_dev_test_flag(hdev, HCI_WIDEBAND_SPEECH_ENABLED);

	if (!(hdev->commands[18] & 0x08) ||
	    !(hdev->features[0][6] & LMP_ERR_DATA_REPORTING) ||
	    test_bit(HCI_QUIRK_BROKEN_ERR_DATA_REPORTING, &hdev->quirks))
		return 0;

	if (enabled == hdev->err_data_reporting)
		return 0;

	memset(&cp, 0, sizeof(cp));
	cp.err_data_reporting = enabled ? ERR_DATA_REPORTING_ENABLED :
				ERR_DATA_REPORTING_DISABLED;

	return __hci_cmd_sync_status(hdev, HCI_OP_WRITE_DEF_ERR_DATA_REPORTING,
				    sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

static const struct hci_init_stage hci_init4[] = {
	  
	HCI_INIT(hci_delete_stored_link_key_sync),
	 
	HCI_INIT(hci_set_event_mask_page_2_sync),
	 
	HCI_INIT(hci_read_local_codecs_sync),
	  
	HCI_INIT(hci_read_local_pairing_opts_sync),
	  
	HCI_INIT(hci_get_mws_transport_config_sync),
	  
	HCI_INIT(hci_read_sync_train_params_sync),
	 
	HCI_INIT(hci_write_sc_support_1_sync),
	 
	HCI_INIT(hci_set_err_data_report_sync),
	{}
};

 
static int hci_le_set_write_def_data_len_sync(struct hci_dev *hdev)
{
	struct hci_cp_le_write_def_data_len cp;

	if (!(hdev->le_features[0] & HCI_LE_DATA_LEN_EXT))
		return 0;

	memset(&cp, 0, sizeof(cp));
	cp.tx_len = cpu_to_le16(hdev->le_max_tx_len);
	cp.tx_time = cpu_to_le16(hdev->le_max_tx_time);

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_WRITE_DEF_DATA_LEN,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

 
static int hci_le_set_default_phy_sync(struct hci_dev *hdev)
{
	struct hci_cp_le_set_default_phy cp;

	if (!(hdev->commands[35] & 0x20)) {
		 
		hdev->le_tx_def_phys = HCI_LE_SET_PHY_1M;
		hdev->le_rx_def_phys = HCI_LE_SET_PHY_1M;
		return 0;
	}

	memset(&cp, 0, sizeof(cp));
	cp.all_phys = 0x00;
	cp.tx_phys = HCI_LE_SET_PHY_1M;
	cp.rx_phys = HCI_LE_SET_PHY_1M;

	 
	if (le_2m_capable(hdev)) {
		cp.tx_phys |= HCI_LE_SET_PHY_2M;
		cp.rx_phys |= HCI_LE_SET_PHY_2M;
	}

	 
	if (le_coded_capable(hdev)) {
		cp.tx_phys |= HCI_LE_SET_PHY_CODED;
		cp.rx_phys |= HCI_LE_SET_PHY_CODED;
	}

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_DEFAULT_PHY,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

static const struct hci_init_stage le_init4[] = {
	 
	HCI_INIT(hci_le_set_write_def_data_len_sync),
	 
	HCI_INIT(hci_le_set_default_phy_sync),
	{}
};

static int hci_init4_sync(struct hci_dev *hdev)
{
	int err;

	bt_dev_dbg(hdev, "");

	err = hci_init_stage_sync(hdev, hci_init4);
	if (err)
		return err;

	if (lmp_le_capable(hdev))
		return hci_init_stage_sync(hdev, le_init4);

	return 0;
}

static int hci_init_sync(struct hci_dev *hdev)
{
	int err;

	err = hci_init1_sync(hdev);
	if (err < 0)
		return err;

	if (hci_dev_test_flag(hdev, HCI_SETUP))
		hci_debugfs_create_basic(hdev);

	err = hci_init2_sync(hdev);
	if (err < 0)
		return err;

	 
	if (hdev->dev_type != HCI_PRIMARY)
		return 0;

	err = hci_init3_sync(hdev);
	if (err < 0)
		return err;

	err = hci_init4_sync(hdev);
	if (err < 0)
		return err;

	 
	if (!hci_dev_test_flag(hdev, HCI_SETUP) &&
	    !hci_dev_test_flag(hdev, HCI_CONFIG))
		return 0;

	if (hci_dev_test_and_set_flag(hdev, HCI_DEBUGFS_CREATED))
		return 0;

	hci_debugfs_create_common(hdev);

	if (lmp_bredr_capable(hdev))
		hci_debugfs_create_bredr(hdev);

	if (lmp_le_capable(hdev))
		hci_debugfs_create_le(hdev);

	return 0;
}

#define HCI_QUIRK_BROKEN(_quirk, _desc) { HCI_QUIRK_BROKEN_##_quirk, _desc }

static const struct {
	unsigned long quirk;
	const char *desc;
} hci_broken_table[] = {
	HCI_QUIRK_BROKEN(LOCAL_COMMANDS,
			 "HCI Read Local Supported Commands not supported"),
	HCI_QUIRK_BROKEN(STORED_LINK_KEY,
			 "HCI Delete Stored Link Key command is advertised, "
			 "but not supported."),
	HCI_QUIRK_BROKEN(ERR_DATA_REPORTING,
			 "HCI Read Default Erroneous Data Reporting command is "
			 "advertised, but not supported."),
	HCI_QUIRK_BROKEN(READ_TRANSMIT_POWER,
			 "HCI Read Transmit Power Level command is advertised, "
			 "but not supported."),
	HCI_QUIRK_BROKEN(FILTER_CLEAR_ALL,
			 "HCI Set Event Filter command not supported."),
	HCI_QUIRK_BROKEN(ENHANCED_SETUP_SYNC_CONN,
			 "HCI Enhanced Setup Synchronous Connection command is "
			 "advertised, but not supported."),
	HCI_QUIRK_BROKEN(SET_RPA_TIMEOUT,
			 "HCI LE Set Random Private Address Timeout command is "
			 "advertised, but not supported."),
	HCI_QUIRK_BROKEN(LE_CODED,
			 "HCI LE Coded PHY feature bit is set, "
			 "but its usage is not supported.")
};

 
static int hci_dev_setup_sync(struct hci_dev *hdev)
{
	int ret = 0;
	bool invalid_bdaddr;
	size_t i;

	if (!hci_dev_test_flag(hdev, HCI_SETUP) &&
	    !test_bit(HCI_QUIRK_NON_PERSISTENT_SETUP, &hdev->quirks))
		return 0;

	bt_dev_dbg(hdev, "");

	hci_sock_dev_event(hdev, HCI_DEV_SETUP);

	if (hdev->setup)
		ret = hdev->setup(hdev);

	for (i = 0; i < ARRAY_SIZE(hci_broken_table); i++) {
		if (test_bit(hci_broken_table[i].quirk, &hdev->quirks))
			bt_dev_warn(hdev, "%s", hci_broken_table[i].desc);
	}

	 
	invalid_bdaddr = test_bit(HCI_QUIRK_INVALID_BDADDR, &hdev->quirks) ||
			 test_bit(HCI_QUIRK_USE_BDADDR_PROPERTY, &hdev->quirks);
	if (!ret) {
		if (test_bit(HCI_QUIRK_USE_BDADDR_PROPERTY, &hdev->quirks) &&
		    !bacmp(&hdev->public_addr, BDADDR_ANY))
			hci_dev_get_bd_addr_from_property(hdev);

		if (invalid_bdaddr && bacmp(&hdev->public_addr, BDADDR_ANY) &&
		    hdev->set_bdaddr) {
			ret = hdev->set_bdaddr(hdev, &hdev->public_addr);
			if (!ret)
				invalid_bdaddr = false;
		}
	}

	 
	if (test_bit(HCI_QUIRK_EXTERNAL_CONFIG, &hdev->quirks) ||
	    invalid_bdaddr)
		hci_dev_set_flag(hdev, HCI_UNCONFIGURED);

	 
	if (hci_dev_test_flag(hdev, HCI_UNCONFIGURED))
		return hci_unconf_init_sync(hdev);

	return ret;
}

 
static int hci_dev_init_sync(struct hci_dev *hdev)
{
	int ret;

	bt_dev_dbg(hdev, "");

	atomic_set(&hdev->cmd_cnt, 1);
	set_bit(HCI_INIT, &hdev->flags);

	ret = hci_dev_setup_sync(hdev);

	if (hci_dev_test_flag(hdev, HCI_CONFIG)) {
		 
		if (bacmp(&hdev->public_addr, BDADDR_ANY) &&
		    hdev->set_bdaddr)
			ret = hdev->set_bdaddr(hdev, &hdev->public_addr);
		else
			ret = -EADDRNOTAVAIL;
	}

	if (!ret) {
		if (!hci_dev_test_flag(hdev, HCI_UNCONFIGURED) &&
		    !hci_dev_test_flag(hdev, HCI_USER_CHANNEL)) {
			ret = hci_init_sync(hdev);
			if (!ret && hdev->post_init)
				ret = hdev->post_init(hdev);
		}
	}

	 
	if (test_bit(HCI_QUIRK_NON_PERSISTENT_DIAG, &hdev->quirks) &&
	    !hci_dev_test_flag(hdev, HCI_USER_CHANNEL) &&
	    hci_dev_test_flag(hdev, HCI_VENDOR_DIAG) && hdev->set_diag)
		ret = hdev->set_diag(hdev, true);

	if (!hci_dev_test_flag(hdev, HCI_USER_CHANNEL)) {
		msft_do_open(hdev);
		aosp_do_open(hdev);
	}

	clear_bit(HCI_INIT, &hdev->flags);

	return ret;
}

int hci_dev_open_sync(struct hci_dev *hdev)
{
	int ret;

	bt_dev_dbg(hdev, "");

	if (hci_dev_test_flag(hdev, HCI_UNREGISTER)) {
		ret = -ENODEV;
		goto done;
	}

	if (!hci_dev_test_flag(hdev, HCI_SETUP) &&
	    !hci_dev_test_flag(hdev, HCI_CONFIG)) {
		 
		if (hci_dev_test_flag(hdev, HCI_RFKILLED)) {
			ret = -ERFKILL;
			goto done;
		}

		 
		if (!hci_dev_test_flag(hdev, HCI_USER_CHANNEL) &&
		    hdev->dev_type == HCI_PRIMARY &&
		    !bacmp(&hdev->bdaddr, BDADDR_ANY) &&
		    !bacmp(&hdev->static_addr, BDADDR_ANY)) {
			ret = -EADDRNOTAVAIL;
			goto done;
		}
	}

	if (test_bit(HCI_UP, &hdev->flags)) {
		ret = -EALREADY;
		goto done;
	}

	if (hdev->open(hdev)) {
		ret = -EIO;
		goto done;
	}

	hci_devcd_reset(hdev);

	set_bit(HCI_RUNNING, &hdev->flags);
	hci_sock_dev_event(hdev, HCI_DEV_OPEN);

	ret = hci_dev_init_sync(hdev);
	if (!ret) {
		hci_dev_hold(hdev);
		hci_dev_set_flag(hdev, HCI_RPA_EXPIRED);
		hci_adv_instances_set_rpa_expired(hdev, true);
		set_bit(HCI_UP, &hdev->flags);
		hci_sock_dev_event(hdev, HCI_DEV_UP);
		hci_leds_update_powered(hdev, true);
		if (!hci_dev_test_flag(hdev, HCI_SETUP) &&
		    !hci_dev_test_flag(hdev, HCI_CONFIG) &&
		    !hci_dev_test_flag(hdev, HCI_UNCONFIGURED) &&
		    !hci_dev_test_flag(hdev, HCI_USER_CHANNEL) &&
		    hci_dev_test_flag(hdev, HCI_MGMT) &&
		    hdev->dev_type == HCI_PRIMARY) {
			ret = hci_powered_update_sync(hdev);
			mgmt_power_on(hdev, ret);
		}
	} else {
		 
		flush_work(&hdev->tx_work);

		 
		flush_work(&hdev->rx_work);
		flush_work(&hdev->cmd_work);

		skb_queue_purge(&hdev->cmd_q);
		skb_queue_purge(&hdev->rx_q);

		if (hdev->flush)
			hdev->flush(hdev);

		if (hdev->sent_cmd) {
			cancel_delayed_work_sync(&hdev->cmd_timer);
			kfree_skb(hdev->sent_cmd);
			hdev->sent_cmd = NULL;
		}

		clear_bit(HCI_RUNNING, &hdev->flags);
		hci_sock_dev_event(hdev, HCI_DEV_CLOSE);

		hdev->close(hdev);
		hdev->flags &= BIT(HCI_RAW);
	}

done:
	return ret;
}

 
static void hci_pend_le_actions_clear(struct hci_dev *hdev)
{
	struct hci_conn_params *p;

	list_for_each_entry(p, &hdev->le_conn_params, list) {
		hci_pend_le_list_del_init(p);
		if (p->conn) {
			hci_conn_drop(p->conn);
			hci_conn_put(p->conn);
			p->conn = NULL;
		}
	}

	BT_DBG("All LE pending actions cleared");
}

static int hci_dev_shutdown(struct hci_dev *hdev)
{
	int err = 0;
	 
	bool was_userchannel =
		hci_dev_test_and_clear_flag(hdev, HCI_USER_CHANNEL);

	if (!hci_dev_test_flag(hdev, HCI_UNREGISTER) &&
	    test_bit(HCI_UP, &hdev->flags)) {
		 
		if (hdev->shutdown)
			err = hdev->shutdown(hdev);
	}

	if (was_userchannel)
		hci_dev_set_flag(hdev, HCI_USER_CHANNEL);

	return err;
}

int hci_dev_close_sync(struct hci_dev *hdev)
{
	bool auto_off;
	int err = 0;

	bt_dev_dbg(hdev, "");

	cancel_delayed_work(&hdev->power_off);
	cancel_delayed_work(&hdev->ncmd_timer);
	cancel_delayed_work(&hdev->le_scan_disable);
	cancel_delayed_work(&hdev->le_scan_restart);

	hci_request_cancel_all(hdev);

	if (hdev->adv_instance_timeout) {
		cancel_delayed_work_sync(&hdev->adv_instance_expire);
		hdev->adv_instance_timeout = 0;
	}

	err = hci_dev_shutdown(hdev);

	if (!test_and_clear_bit(HCI_UP, &hdev->flags)) {
		cancel_delayed_work_sync(&hdev->cmd_timer);
		return err;
	}

	hci_leds_update_powered(hdev, false);

	 
	flush_work(&hdev->tx_work);
	flush_work(&hdev->rx_work);

	if (hdev->discov_timeout > 0) {
		hdev->discov_timeout = 0;
		hci_dev_clear_flag(hdev, HCI_DISCOVERABLE);
		hci_dev_clear_flag(hdev, HCI_LIMITED_DISCOVERABLE);
	}

	if (hci_dev_test_and_clear_flag(hdev, HCI_SERVICE_CACHE))
		cancel_delayed_work(&hdev->service_cache);

	if (hci_dev_test_flag(hdev, HCI_MGMT)) {
		struct adv_info *adv_instance;

		cancel_delayed_work_sync(&hdev->rpa_expired);

		list_for_each_entry(adv_instance, &hdev->adv_instances, list)
			cancel_delayed_work_sync(&adv_instance->rpa_expired_cb);
	}

	 
	drain_workqueue(hdev->workqueue);

	hci_dev_lock(hdev);

	hci_discovery_set_state(hdev, DISCOVERY_STOPPED);

	auto_off = hci_dev_test_and_clear_flag(hdev, HCI_AUTO_OFF);

	if (!auto_off && hdev->dev_type == HCI_PRIMARY &&
	    !hci_dev_test_flag(hdev, HCI_USER_CHANNEL) &&
	    hci_dev_test_flag(hdev, HCI_MGMT))
		__mgmt_power_off(hdev);

	hci_inquiry_cache_flush(hdev);
	hci_pend_le_actions_clear(hdev);
	hci_conn_hash_flush(hdev);
	 
	smp_unregister(hdev);
	hci_dev_unlock(hdev);

	hci_sock_dev_event(hdev, HCI_DEV_DOWN);

	if (!hci_dev_test_flag(hdev, HCI_USER_CHANNEL)) {
		aosp_do_close(hdev);
		msft_do_close(hdev);
	}

	if (hdev->flush)
		hdev->flush(hdev);

	 
	skb_queue_purge(&hdev->cmd_q);
	atomic_set(&hdev->cmd_cnt, 1);
	if (test_bit(HCI_QUIRK_RESET_ON_CLOSE, &hdev->quirks) &&
	    !auto_off && !hci_dev_test_flag(hdev, HCI_UNCONFIGURED)) {
		set_bit(HCI_INIT, &hdev->flags);
		hci_reset_sync(hdev);
		clear_bit(HCI_INIT, &hdev->flags);
	}

	 
	flush_work(&hdev->cmd_work);

	 
	skb_queue_purge(&hdev->rx_q);
	skb_queue_purge(&hdev->cmd_q);
	skb_queue_purge(&hdev->raw_q);

	 
	if (hdev->sent_cmd) {
		cancel_delayed_work_sync(&hdev->cmd_timer);
		kfree_skb(hdev->sent_cmd);
		hdev->sent_cmd = NULL;
	}

	clear_bit(HCI_RUNNING, &hdev->flags);
	hci_sock_dev_event(hdev, HCI_DEV_CLOSE);

	 
	hdev->close(hdev);

	 
	hdev->flags &= BIT(HCI_RAW);
	hci_dev_clear_volatile_flags(hdev);

	 
	hdev->amp_status = AMP_STATUS_POWERED_DOWN;

	memset(hdev->eir, 0, sizeof(hdev->eir));
	memset(hdev->dev_class, 0, sizeof(hdev->dev_class));
	bacpy(&hdev->random_addr, BDADDR_ANY);
	hci_codec_list_clear(&hdev->local_codecs);

	hci_dev_put(hdev);
	return err;
}

 
static int hci_power_on_sync(struct hci_dev *hdev)
{
	int err;

	if (test_bit(HCI_UP, &hdev->flags) &&
	    hci_dev_test_flag(hdev, HCI_MGMT) &&
	    hci_dev_test_and_clear_flag(hdev, HCI_AUTO_OFF)) {
		cancel_delayed_work(&hdev->power_off);
		return hci_powered_update_sync(hdev);
	}

	err = hci_dev_open_sync(hdev);
	if (err < 0)
		return err;

	 
	if (hci_dev_test_flag(hdev, HCI_RFKILLED) ||
	    hci_dev_test_flag(hdev, HCI_UNCONFIGURED) ||
	    (hdev->dev_type == HCI_PRIMARY &&
	     !bacmp(&hdev->bdaddr, BDADDR_ANY) &&
	     !bacmp(&hdev->static_addr, BDADDR_ANY))) {
		hci_dev_clear_flag(hdev, HCI_AUTO_OFF);
		hci_dev_close_sync(hdev);
	} else if (hci_dev_test_flag(hdev, HCI_AUTO_OFF)) {
		queue_delayed_work(hdev->req_workqueue, &hdev->power_off,
				   HCI_AUTO_OFF_TIMEOUT);
	}

	if (hci_dev_test_and_clear_flag(hdev, HCI_SETUP)) {
		 
		if (hci_dev_test_flag(hdev, HCI_UNCONFIGURED))
			set_bit(HCI_RAW, &hdev->flags);

		 
		mgmt_index_added(hdev);
	} else if (hci_dev_test_and_clear_flag(hdev, HCI_CONFIG)) {
		 
		if (!hci_dev_test_flag(hdev, HCI_UNCONFIGURED))
			clear_bit(HCI_RAW, &hdev->flags);

		 
		mgmt_index_added(hdev);
	}

	return 0;
}

static int hci_remote_name_cancel_sync(struct hci_dev *hdev, bdaddr_t *addr)
{
	struct hci_cp_remote_name_req_cancel cp;

	memset(&cp, 0, sizeof(cp));
	bacpy(&cp.bdaddr, addr);

	return __hci_cmd_sync_status(hdev, HCI_OP_REMOTE_NAME_REQ_CANCEL,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

int hci_stop_discovery_sync(struct hci_dev *hdev)
{
	struct discovery_state *d = &hdev->discovery;
	struct inquiry_entry *e;
	int err;

	bt_dev_dbg(hdev, "state %u", hdev->discovery.state);

	if (d->state == DISCOVERY_FINDING || d->state == DISCOVERY_STOPPING) {
		if (test_bit(HCI_INQUIRY, &hdev->flags)) {
			err = __hci_cmd_sync_status(hdev, HCI_OP_INQUIRY_CANCEL,
						    0, NULL, HCI_CMD_TIMEOUT);
			if (err)
				return err;
		}

		if (hci_dev_test_flag(hdev, HCI_LE_SCAN)) {
			cancel_delayed_work(&hdev->le_scan_disable);
			cancel_delayed_work(&hdev->le_scan_restart);

			err = hci_scan_disable_sync(hdev);
			if (err)
				return err;
		}

	} else {
		err = hci_scan_disable_sync(hdev);
		if (err)
			return err;
	}

	 
	if (use_ll_privacy(hdev))
		hci_resume_advertising_sync(hdev);

	 
	if (d->type == DISCOV_TYPE_LE)
		return 0;

	if (d->state == DISCOVERY_RESOLVING || d->state == DISCOVERY_STOPPING) {
		e = hci_inquiry_cache_lookup_resolve(hdev, BDADDR_ANY,
						     NAME_PENDING);
		if (!e)
			return 0;

		return hci_remote_name_cancel_sync(hdev, &e->data.bdaddr);
	}

	return 0;
}

static int hci_disconnect_phy_link_sync(struct hci_dev *hdev, u16 handle,
					u8 reason)
{
	struct hci_cp_disconn_phy_link cp;

	memset(&cp, 0, sizeof(cp));
	cp.phy_handle = HCI_PHY_HANDLE(handle);
	cp.reason = reason;

	return __hci_cmd_sync_status(hdev, HCI_OP_DISCONN_PHY_LINK,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

static int hci_disconnect_sync(struct hci_dev *hdev, struct hci_conn *conn,
			       u8 reason)
{
	struct hci_cp_disconnect cp;

	if (conn->type == AMP_LINK)
		return hci_disconnect_phy_link_sync(hdev, conn->handle, reason);

	memset(&cp, 0, sizeof(cp));
	cp.handle = cpu_to_le16(conn->handle);
	cp.reason = reason;

	 
	if (reason != HCI_ERROR_REMOTE_POWER_OFF)
		return __hci_cmd_sync_status_sk(hdev, HCI_OP_DISCONNECT,
						sizeof(cp), &cp,
						HCI_EV_DISCONN_COMPLETE,
						HCI_CMD_TIMEOUT, NULL);

	return __hci_cmd_sync_status(hdev, HCI_OP_DISCONNECT, sizeof(cp), &cp,
				     HCI_CMD_TIMEOUT);
}

static int hci_le_connect_cancel_sync(struct hci_dev *hdev,
				      struct hci_conn *conn, u8 reason)
{
	 
	if (test_bit(HCI_CONN_SCANNING, &conn->flags))
		return reason;

	if (conn->role == HCI_ROLE_SLAVE ||
	    test_and_set_bit(HCI_CONN_CANCEL, &conn->flags))
		return 0;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_CREATE_CONN_CANCEL,
				     0, NULL, HCI_CMD_TIMEOUT);
}

static int hci_connect_cancel_sync(struct hci_dev *hdev, struct hci_conn *conn,
				   u8 reason)
{
	if (conn->type == LE_LINK)
		return hci_le_connect_cancel_sync(hdev, conn, reason);

	if (conn->type == ISO_LINK) {
		 
		if (test_bit(HCI_CONN_CREATE_CIS, &conn->flags))
			return hci_disconnect_sync(hdev, conn, reason);

		 
		if (bacmp(&conn->dst, BDADDR_ANY))
			return HCI_ERROR_LOCAL_HOST_TERM;

		 
		return 0;
	}

	if (hdev->hci_ver < BLUETOOTH_VER_1_2)
		return 0;

	 
	if (reason != HCI_ERROR_REMOTE_POWER_OFF)
		return __hci_cmd_sync_status_sk(hdev, HCI_OP_CREATE_CONN_CANCEL,
						6, &conn->dst,
						HCI_EV_CONN_COMPLETE,
						HCI_CMD_TIMEOUT, NULL);

	return __hci_cmd_sync_status(hdev, HCI_OP_CREATE_CONN_CANCEL,
				     6, &conn->dst, HCI_CMD_TIMEOUT);
}

static int hci_reject_sco_sync(struct hci_dev *hdev, struct hci_conn *conn,
			       u8 reason)
{
	struct hci_cp_reject_sync_conn_req cp;

	memset(&cp, 0, sizeof(cp));
	bacpy(&cp.bdaddr, &conn->dst);
	cp.reason = reason;

	 
	if (reason < 0x0d || reason > 0x0f)
		cp.reason = HCI_ERROR_REJ_LIMITED_RESOURCES;

	return __hci_cmd_sync_status(hdev, HCI_OP_REJECT_SYNC_CONN_REQ,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

static int hci_le_reject_cis_sync(struct hci_dev *hdev, struct hci_conn *conn,
				  u8 reason)
{
	struct hci_cp_le_reject_cis cp;

	memset(&cp, 0, sizeof(cp));
	cp.handle = cpu_to_le16(conn->handle);
	cp.reason = reason;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_REJECT_CIS,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

static int hci_reject_conn_sync(struct hci_dev *hdev, struct hci_conn *conn,
				u8 reason)
{
	struct hci_cp_reject_conn_req cp;

	if (conn->type == ISO_LINK)
		return hci_le_reject_cis_sync(hdev, conn, reason);

	if (conn->type == SCO_LINK || conn->type == ESCO_LINK)
		return hci_reject_sco_sync(hdev, conn, reason);

	memset(&cp, 0, sizeof(cp));
	bacpy(&cp.bdaddr, &conn->dst);
	cp.reason = reason;

	return __hci_cmd_sync_status(hdev, HCI_OP_REJECT_CONN_REQ,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

int hci_abort_conn_sync(struct hci_dev *hdev, struct hci_conn *conn, u8 reason)
{
	int err = 0;
	u16 handle = conn->handle;
	bool disconnect = false;
	struct hci_conn *c;

	switch (conn->state) {
	case BT_CONNECTED:
	case BT_CONFIG:
		err = hci_disconnect_sync(hdev, conn, reason);
		break;
	case BT_CONNECT:
		err = hci_connect_cancel_sync(hdev, conn, reason);
		break;
	case BT_CONNECT2:
		err = hci_reject_conn_sync(hdev, conn, reason);
		break;
	case BT_OPEN:
		hci_dev_lock(hdev);

		 
		if (test_and_clear_bit(HCI_CONN_BIG_SYNC_FAILED, &conn->flags) ||
		    test_and_clear_bit(HCI_CONN_PA_SYNC_FAILED, &conn->flags)) {
			hci_conn_failed(conn, reason);
		} else if (test_bit(HCI_CONN_PA_SYNC, &conn->flags) ||
			   test_bit(HCI_CONN_BIG_SYNC, &conn->flags)) {
			conn->state = BT_CLOSED;
			hci_disconn_cfm(conn, reason);
			hci_conn_del(conn);
		}

		hci_dev_unlock(hdev);
		return 0;
	case BT_BOUND:
		break;
	default:
		disconnect = true;
		break;
	}

	hci_dev_lock(hdev);

	 
	c = hci_conn_hash_lookup_handle(hdev, handle);
	if (!c || c != conn) {
		err = 0;
		goto unlock;
	}

	 
	if (disconnect) {
		conn->state = BT_CLOSED;
		hci_disconn_cfm(conn, reason);
		hci_conn_del(conn);
	} else {
		hci_conn_failed(conn, reason);
	}

unlock:
	hci_dev_unlock(hdev);
	return err;
}

static int hci_disconnect_all_sync(struct hci_dev *hdev, u8 reason)
{
	struct list_head *head = &hdev->conn_hash.list;
	struct hci_conn *conn;

	rcu_read_lock();
	while ((conn = list_first_or_null_rcu(head, struct hci_conn, list))) {
		 
		conn = hci_conn_get(conn);
		rcu_read_unlock();
		 
		hci_abort_conn_sync(hdev, conn, reason);
		hci_conn_put(conn);
		rcu_read_lock();
	}
	rcu_read_unlock();

	return 0;
}

 
static int hci_power_off_sync(struct hci_dev *hdev)
{
	int err;

	 
	if (!test_bit(HCI_UP, &hdev->flags))
		return 0;

	if (test_bit(HCI_ISCAN, &hdev->flags) ||
	    test_bit(HCI_PSCAN, &hdev->flags)) {
		err = hci_write_scan_enable_sync(hdev, 0x00);
		if (err)
			return err;
	}

	err = hci_clear_adv_sync(hdev, NULL, false);
	if (err)
		return err;

	err = hci_stop_discovery_sync(hdev);
	if (err)
		return err;

	 
	err = hci_disconnect_all_sync(hdev, HCI_ERROR_REMOTE_POWER_OFF);
	if (err)
		return err;

	return hci_dev_close_sync(hdev);
}

int hci_set_powered_sync(struct hci_dev *hdev, u8 val)
{
	if (val)
		return hci_power_on_sync(hdev);

	return hci_power_off_sync(hdev);
}

static int hci_write_iac_sync(struct hci_dev *hdev)
{
	struct hci_cp_write_current_iac_lap cp;

	if (!hci_dev_test_flag(hdev, HCI_DISCOVERABLE))
		return 0;

	memset(&cp, 0, sizeof(cp));

	if (hci_dev_test_flag(hdev, HCI_LIMITED_DISCOVERABLE)) {
		 
		cp.num_iac = min_t(u8, hdev->num_iac, 2);
		cp.iac_lap[0] = 0x00;	 
		cp.iac_lap[1] = 0x8b;
		cp.iac_lap[2] = 0x9e;
		cp.iac_lap[3] = 0x33;	 
		cp.iac_lap[4] = 0x8b;
		cp.iac_lap[5] = 0x9e;
	} else {
		 
		cp.num_iac = 1;
		cp.iac_lap[0] = 0x33;	 
		cp.iac_lap[1] = 0x8b;
		cp.iac_lap[2] = 0x9e;
	}

	return __hci_cmd_sync_status(hdev, HCI_OP_WRITE_CURRENT_IAC_LAP,
				     (cp.num_iac * 3) + 1, &cp,
				     HCI_CMD_TIMEOUT);
}

int hci_update_discoverable_sync(struct hci_dev *hdev)
{
	int err = 0;

	if (hci_dev_test_flag(hdev, HCI_BREDR_ENABLED)) {
		err = hci_write_iac_sync(hdev);
		if (err)
			return err;

		err = hci_update_scan_sync(hdev);
		if (err)
			return err;

		err = hci_update_class_sync(hdev);
		if (err)
			return err;
	}

	 
	if (hci_dev_test_flag(hdev, HCI_ADVERTISING)) {
		err = hci_update_adv_data_sync(hdev, 0x00);
		if (err)
			return err;

		 
		if (hci_dev_test_flag(hdev, HCI_LIMITED_PRIVACY)) {
			if (ext_adv_capable(hdev))
				err = hci_start_ext_adv_sync(hdev, 0x00);
			else
				err = hci_enable_advertising_sync(hdev);
		}
	}

	return err;
}

static int update_discoverable_sync(struct hci_dev *hdev, void *data)
{
	return hci_update_discoverable_sync(hdev);
}

int hci_update_discoverable(struct hci_dev *hdev)
{
	 
	if (hdev_is_powered(hdev) &&
	    hci_dev_test_flag(hdev, HCI_ADVERTISING) &&
	    hci_dev_test_flag(hdev, HCI_DISCOVERABLE) &&
	    hci_dev_test_flag(hdev, HCI_LIMITED_PRIVACY))
		return hci_cmd_sync_queue(hdev, update_discoverable_sync, NULL,
					  NULL);

	return 0;
}

int hci_update_connectable_sync(struct hci_dev *hdev)
{
	int err;

	err = hci_update_scan_sync(hdev);
	if (err)
		return err;

	 
	if (!hci_dev_test_flag(hdev, HCI_BREDR_ENABLED))
		err = hci_update_adv_data_sync(hdev, hdev->cur_adv_instance);

	 
	if (hci_dev_test_flag(hdev, HCI_ADVERTISING) ||
	    !list_empty(&hdev->adv_instances)) {
		if (ext_adv_capable(hdev))
			err = hci_start_ext_adv_sync(hdev,
						     hdev->cur_adv_instance);
		else
			err = hci_enable_advertising_sync(hdev);

		if (err)
			return err;
	}

	return hci_update_passive_scan_sync(hdev);
}

static int hci_inquiry_sync(struct hci_dev *hdev, u8 length)
{
	const u8 giac[3] = { 0x33, 0x8b, 0x9e };
	const u8 liac[3] = { 0x00, 0x8b, 0x9e };
	struct hci_cp_inquiry cp;

	bt_dev_dbg(hdev, "");

	if (hci_dev_test_flag(hdev, HCI_INQUIRY))
		return 0;

	hci_dev_lock(hdev);
	hci_inquiry_cache_flush(hdev);
	hci_dev_unlock(hdev);

	memset(&cp, 0, sizeof(cp));

	if (hdev->discovery.limited)
		memcpy(&cp.lap, liac, sizeof(cp.lap));
	else
		memcpy(&cp.lap, giac, sizeof(cp.lap));

	cp.length = length;

	return __hci_cmd_sync_status(hdev, HCI_OP_INQUIRY,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

static int hci_active_scan_sync(struct hci_dev *hdev, uint16_t interval)
{
	u8 own_addr_type;
	 
	u8 filter_policy = 0x00;
	 
	u8 filter_dup = LE_SCAN_FILTER_DUP_ENABLE;
	int err;

	bt_dev_dbg(hdev, "");

	 
	err = hci_scan_disable_sync(hdev);
	if (err) {
		bt_dev_err(hdev, "Unable to disable scanning: %d", err);
		return err;
	}

	cancel_interleave_scan(hdev);

	 
	err = hci_pause_addr_resolution(hdev);
	if (err)
		goto failed;

	 
	err = hci_update_random_address_sync(hdev, true, scan_use_rpa(hdev),
					     &own_addr_type);
	if (err < 0)
		own_addr_type = ADDR_LE_DEV_PUBLIC;

	if (hci_is_adv_monitoring(hdev)) {
		 
		filter_dup = LE_SCAN_FILTER_DUP_DISABLE;
	}

	err = hci_start_scan_sync(hdev, LE_SCAN_ACTIVE, interval,
				  hdev->le_scan_window_discovery,
				  own_addr_type, filter_policy, filter_dup);
	if (!err)
		return err;

failed:
	 
	if (use_ll_privacy(hdev))
		hci_resume_advertising_sync(hdev);

	 
	hci_update_passive_scan_sync(hdev);
	return err;
}

static int hci_start_interleaved_discovery_sync(struct hci_dev *hdev)
{
	int err;

	bt_dev_dbg(hdev, "");

	err = hci_active_scan_sync(hdev, hdev->le_scan_int_discovery * 2);
	if (err)
		return err;

	return hci_inquiry_sync(hdev, DISCOV_BREDR_INQUIRY_LEN);
}

int hci_start_discovery_sync(struct hci_dev *hdev)
{
	unsigned long timeout;
	int err;

	bt_dev_dbg(hdev, "type %u", hdev->discovery.type);

	switch (hdev->discovery.type) {
	case DISCOV_TYPE_BREDR:
		return hci_inquiry_sync(hdev, DISCOV_BREDR_INQUIRY_LEN);
	case DISCOV_TYPE_INTERLEAVED:
		 
		if (test_bit(HCI_QUIRK_SIMULTANEOUS_DISCOVERY,
			     &hdev->quirks)) {
			timeout = msecs_to_jiffies(DISCOV_LE_TIMEOUT);
			 
			err = hci_start_interleaved_discovery_sync(hdev);
			break;
		}

		timeout = msecs_to_jiffies(hdev->discov_interleaved_timeout);
		err = hci_active_scan_sync(hdev, hdev->le_scan_int_discovery);
		break;
	case DISCOV_TYPE_LE:
		timeout = msecs_to_jiffies(DISCOV_LE_TIMEOUT);
		err = hci_active_scan_sync(hdev, hdev->le_scan_int_discovery);
		break;
	default:
		return -EINVAL;
	}

	if (err)
		return err;

	bt_dev_dbg(hdev, "timeout %u ms", jiffies_to_msecs(timeout));

	 
	if (test_bit(HCI_QUIRK_STRICT_DUPLICATE_FILTER, &hdev->quirks) &&
	    hdev->discovery.result_filtering) {
		hdev->discovery.scan_start = jiffies;
		hdev->discovery.scan_duration = timeout;
	}

	queue_delayed_work(hdev->req_workqueue, &hdev->le_scan_disable,
			   timeout);
	return 0;
}

static void hci_suspend_monitor_sync(struct hci_dev *hdev)
{
	switch (hci_get_adv_monitor_offload_ext(hdev)) {
	case HCI_ADV_MONITOR_EXT_MSFT:
		msft_suspend_sync(hdev);
		break;
	default:
		return;
	}
}

 
static int hci_pause_discovery_sync(struct hci_dev *hdev)
{
	int old_state = hdev->discovery.state;
	int err;

	 
	if (old_state == DISCOVERY_STOPPED || old_state == DISCOVERY_STOPPING ||
	    hdev->discovery_paused)
		return 0;

	hci_discovery_set_state(hdev, DISCOVERY_STOPPING);
	err = hci_stop_discovery_sync(hdev);
	if (err)
		return err;

	hdev->discovery_paused = true;
	hdev->discovery_old_state = old_state;
	hci_discovery_set_state(hdev, DISCOVERY_STOPPED);

	return 0;
}

static int hci_update_event_filter_sync(struct hci_dev *hdev)
{
	struct bdaddr_list_with_flags *b;
	u8 scan = SCAN_DISABLED;
	bool scanning = test_bit(HCI_PSCAN, &hdev->flags);
	int err;

	if (!hci_dev_test_flag(hdev, HCI_BREDR_ENABLED))
		return 0;

	 
	if (test_bit(HCI_QUIRK_BROKEN_FILTER_CLEAR_ALL, &hdev->quirks))
		return 0;

	 
	hci_clear_event_filter_sync(hdev);

	list_for_each_entry(b, &hdev->accept_list, list) {
		if (!(b->flags & HCI_CONN_FLAG_REMOTE_WAKEUP))
			continue;

		bt_dev_dbg(hdev, "Adding event filters for %pMR", &b->bdaddr);

		err =  hci_set_event_filter_sync(hdev, HCI_FLT_CONN_SETUP,
						 HCI_CONN_SETUP_ALLOW_BDADDR,
						 &b->bdaddr,
						 HCI_CONN_SETUP_AUTO_ON);
		if (err)
			bt_dev_dbg(hdev, "Failed to set event filter for %pMR",
				   &b->bdaddr);
		else
			scan = SCAN_PAGE;
	}

	if (scan && !scanning)
		hci_write_scan_enable_sync(hdev, scan);
	else if (!scan && scanning)
		hci_write_scan_enable_sync(hdev, scan);

	return 0;
}

 
static int hci_pause_scan_sync(struct hci_dev *hdev)
{
	if (hdev->scanning_paused)
		return 0;

	 
	if (test_bit(HCI_PSCAN, &hdev->flags))
		hci_write_scan_enable_sync(hdev, SCAN_DISABLED);

	hci_scan_disable_sync(hdev);

	hdev->scanning_paused = true;

	return 0;
}

 
int hci_suspend_sync(struct hci_dev *hdev)
{
	int err;

	 
	if (hdev->suspended)
		return 0;

	 
	hdev->suspended = true;

	 
	hci_pause_discovery_sync(hdev);

	 
	hci_pause_advertising_sync(hdev);

	 
	hci_suspend_monitor_sync(hdev);

	 
	hci_pause_scan_sync(hdev);

	if (hci_conn_count(hdev)) {
		 
		err = hci_disconnect_all_sync(hdev, HCI_ERROR_REMOTE_POWER_OFF);
		if (err) {
			 
			hdev->suspend_state = BT_RUNNING;
			hci_resume_sync(hdev);
			return err;
		}

		 
		hci_set_event_mask_sync(hdev);
	}

	 
	if (!hdev->wakeup || !hdev->wakeup(hdev)) {
		hdev->suspend_state = BT_SUSPEND_DISCONNECT;
		return 0;
	}

	 
	hdev->scanning_paused = false;

	 
	hci_update_event_filter_sync(hdev);

	 
	hci_update_passive_scan_sync(hdev);

	 
	hdev->scanning_paused = true;

	hdev->suspend_state = BT_SUSPEND_CONFIGURE_WAKE;

	return 0;
}

 
static int hci_resume_discovery_sync(struct hci_dev *hdev)
{
	int err;

	 
	if (!hdev->discovery_paused)
		return 0;

	hdev->discovery_paused = false;

	hci_discovery_set_state(hdev, DISCOVERY_STARTING);

	err = hci_start_discovery_sync(hdev);

	hci_discovery_set_state(hdev, err ? DISCOVERY_STOPPED :
				DISCOVERY_FINDING);

	return err;
}

static void hci_resume_monitor_sync(struct hci_dev *hdev)
{
	switch (hci_get_adv_monitor_offload_ext(hdev)) {
	case HCI_ADV_MONITOR_EXT_MSFT:
		msft_resume_sync(hdev);
		break;
	default:
		return;
	}
}

 
static int hci_resume_scan_sync(struct hci_dev *hdev)
{
	if (!hdev->scanning_paused)
		return 0;

	hdev->scanning_paused = false;

	hci_update_scan_sync(hdev);

	 
	hci_update_passive_scan_sync(hdev);

	return 0;
}

 
int hci_resume_sync(struct hci_dev *hdev)
{
	 
	if (!hdev->suspended)
		return 0;

	hdev->suspended = false;

	 
	hci_set_event_mask_sync(hdev);

	 
	hci_clear_event_filter_sync(hdev);

	 
	hci_resume_scan_sync(hdev);

	 
	hci_resume_monitor_sync(hdev);

	 
	hci_resume_advertising_sync(hdev);

	 
	hci_resume_discovery_sync(hdev);

	return 0;
}

static bool conn_use_rpa(struct hci_conn *conn)
{
	struct hci_dev *hdev = conn->hdev;

	return hci_dev_test_flag(hdev, HCI_PRIVACY);
}

static int hci_le_ext_directed_advertising_sync(struct hci_dev *hdev,
						struct hci_conn *conn)
{
	struct hci_cp_le_set_ext_adv_params cp;
	int err;
	bdaddr_t random_addr;
	u8 own_addr_type;

	err = hci_update_random_address_sync(hdev, false, conn_use_rpa(conn),
					     &own_addr_type);
	if (err)
		return err;

	 
	err = hci_get_random_address(hdev, false, conn_use_rpa(conn), NULL,
				     &own_addr_type, &random_addr);
	if (err)
		return err;

	memset(&cp, 0, sizeof(cp));

	cp.evt_properties = cpu_to_le16(LE_LEGACY_ADV_DIRECT_IND);
	cp.channel_map = hdev->le_adv_channel_map;
	cp.tx_power = HCI_TX_POWER_INVALID;
	cp.primary_phy = HCI_ADV_PHY_1M;
	cp.secondary_phy = HCI_ADV_PHY_1M;
	cp.handle = 0x00;  
	cp.own_addr_type = own_addr_type;
	cp.peer_addr_type = conn->dst_type;
	bacpy(&cp.peer_addr, &conn->dst);

	 
	err = hci_remove_ext_adv_instance_sync(hdev, cp.handle, NULL);
	if (err)
		return err;

	err = __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_EXT_ADV_PARAMS,
				    sizeof(cp), &cp, HCI_CMD_TIMEOUT);
	if (err)
		return err;

	 
	if (own_addr_type == ADDR_LE_DEV_RANDOM &&
	    bacmp(&random_addr, BDADDR_ANY) &&
	    bacmp(&random_addr, &hdev->random_addr)) {
		err = hci_set_adv_set_random_addr_sync(hdev, 0x00,
						       &random_addr);
		if (err)
			return err;
	}

	return hci_enable_ext_advertising_sync(hdev, 0x00);
}

static int hci_le_directed_advertising_sync(struct hci_dev *hdev,
					    struct hci_conn *conn)
{
	struct hci_cp_le_set_adv_param cp;
	u8 status;
	u8 own_addr_type;
	u8 enable;

	if (ext_adv_capable(hdev))
		return hci_le_ext_directed_advertising_sync(hdev, conn);

	 
	hci_dev_clear_flag(hdev, HCI_LE_ADV);

	 
	status = hci_update_random_address_sync(hdev, false, conn_use_rpa(conn),
						&own_addr_type);
	if (status)
		return status;

	memset(&cp, 0, sizeof(cp));

	 
	cp.min_interval = cpu_to_le16(0x0020);
	cp.max_interval = cpu_to_le16(0x0020);

	cp.type = LE_ADV_DIRECT_IND;
	cp.own_address_type = own_addr_type;
	cp.direct_addr_type = conn->dst_type;
	bacpy(&cp.direct_addr, &conn->dst);
	cp.channel_map = hdev->le_adv_channel_map;

	status = __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_ADV_PARAM,
				       sizeof(cp), &cp, HCI_CMD_TIMEOUT);
	if (status)
		return status;

	enable = 0x01;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_ADV_ENABLE,
				     sizeof(enable), &enable, HCI_CMD_TIMEOUT);
}

static void set_ext_conn_params(struct hci_conn *conn,
				struct hci_cp_le_ext_conn_param *p)
{
	struct hci_dev *hdev = conn->hdev;

	memset(p, 0, sizeof(*p));

	p->scan_interval = cpu_to_le16(hdev->le_scan_int_connect);
	p->scan_window = cpu_to_le16(hdev->le_scan_window_connect);
	p->conn_interval_min = cpu_to_le16(conn->le_conn_min_interval);
	p->conn_interval_max = cpu_to_le16(conn->le_conn_max_interval);
	p->conn_latency = cpu_to_le16(conn->le_conn_latency);
	p->supervision_timeout = cpu_to_le16(conn->le_supv_timeout);
	p->min_ce_len = cpu_to_le16(0x0000);
	p->max_ce_len = cpu_to_le16(0x0000);
}

static int hci_le_ext_create_conn_sync(struct hci_dev *hdev,
				       struct hci_conn *conn, u8 own_addr_type)
{
	struct hci_cp_le_ext_create_conn *cp;
	struct hci_cp_le_ext_conn_param *p;
	u8 data[sizeof(*cp) + sizeof(*p) * 3];
	u32 plen;

	cp = (void *)data;
	p = (void *)cp->data;

	memset(cp, 0, sizeof(*cp));

	bacpy(&cp->peer_addr, &conn->dst);
	cp->peer_addr_type = conn->dst_type;
	cp->own_addr_type = own_addr_type;

	plen = sizeof(*cp);

	if (scan_1m(hdev)) {
		cp->phys |= LE_SCAN_PHY_1M;
		set_ext_conn_params(conn, p);

		p++;
		plen += sizeof(*p);
	}

	if (scan_2m(hdev)) {
		cp->phys |= LE_SCAN_PHY_2M;
		set_ext_conn_params(conn, p);

		p++;
		plen += sizeof(*p);
	}

	if (scan_coded(hdev)) {
		cp->phys |= LE_SCAN_PHY_CODED;
		set_ext_conn_params(conn, p);

		plen += sizeof(*p);
	}

	return __hci_cmd_sync_status_sk(hdev, HCI_OP_LE_EXT_CREATE_CONN,
					plen, data,
					HCI_EV_LE_ENHANCED_CONN_COMPLETE,
					conn->conn_timeout, NULL);
}

int hci_le_create_conn_sync(struct hci_dev *hdev, struct hci_conn *conn)
{
	struct hci_cp_le_create_conn cp;
	struct hci_conn_params *params;
	u8 own_addr_type;
	int err;

	 
	if (conn->role == HCI_ROLE_SLAVE) {
		 
		if (hci_dev_test_flag(hdev, HCI_LE_SCAN) &&
		    hdev->le_scan_type == LE_SCAN_ACTIVE &&
		    !hci_dev_test_flag(hdev, HCI_LE_SIMULTANEOUS_ROLES)) {
			hci_conn_del(conn);
			return -EBUSY;
		}

		 
		hci_pause_advertising_sync(hdev);

		err = hci_le_directed_advertising_sync(hdev, conn);
		goto done;
	}

	 
	if (!hci_dev_test_flag(hdev, HCI_LE_SIMULTANEOUS_ROLES))
		hci_pause_advertising_sync(hdev);

	params = hci_conn_params_lookup(hdev, &conn->dst, conn->dst_type);
	if (params) {
		conn->le_conn_min_interval = params->conn_min_interval;
		conn->le_conn_max_interval = params->conn_max_interval;
		conn->le_conn_latency = params->conn_latency;
		conn->le_supv_timeout = params->supervision_timeout;
	} else {
		conn->le_conn_min_interval = hdev->le_conn_min_interval;
		conn->le_conn_max_interval = hdev->le_conn_max_interval;
		conn->le_conn_latency = hdev->le_conn_latency;
		conn->le_supv_timeout = hdev->le_supv_timeout;
	}

	 
	if (hci_dev_test_flag(hdev, HCI_LE_SCAN)) {
		hci_scan_disable_sync(hdev);
		hci_dev_set_flag(hdev, HCI_LE_SCAN_INTERRUPTED);
	}

	 
	err = hci_update_random_address_sync(hdev, false, conn_use_rpa(conn),
					     &own_addr_type);
	if (err)
		goto done;

	if (use_ext_conn(hdev)) {
		err = hci_le_ext_create_conn_sync(hdev, conn, own_addr_type);
		goto done;
	}

	memset(&cp, 0, sizeof(cp));

	cp.scan_interval = cpu_to_le16(hdev->le_scan_int_connect);
	cp.scan_window = cpu_to_le16(hdev->le_scan_window_connect);

	bacpy(&cp.peer_addr, &conn->dst);
	cp.peer_addr_type = conn->dst_type;
	cp.own_address_type = own_addr_type;
	cp.conn_interval_min = cpu_to_le16(conn->le_conn_min_interval);
	cp.conn_interval_max = cpu_to_le16(conn->le_conn_max_interval);
	cp.conn_latency = cpu_to_le16(conn->le_conn_latency);
	cp.supervision_timeout = cpu_to_le16(conn->le_supv_timeout);
	cp.min_ce_len = cpu_to_le16(0x0000);
	cp.max_ce_len = cpu_to_le16(0x0000);

	 
	err = __hci_cmd_sync_status_sk(hdev, HCI_OP_LE_CREATE_CONN,
				       sizeof(cp), &cp,
				       use_enhanced_conn_complete(hdev) ?
				       HCI_EV_LE_ENHANCED_CONN_COMPLETE :
				       HCI_EV_LE_CONN_COMPLETE,
				       conn->conn_timeout, NULL);

done:
	if (err == -ETIMEDOUT)
		hci_le_connect_cancel_sync(hdev, conn, 0x00);

	 
	hci_resume_advertising_sync(hdev);
	return err;
}

int hci_le_create_cis_sync(struct hci_dev *hdev)
{
	struct {
		struct hci_cp_le_create_cis cp;
		struct hci_cis cis[0x1f];
	} cmd;
	struct hci_conn *conn;
	u8 cig = BT_ISO_QOS_CIG_UNSET;

	 

	memset(&cmd, 0, sizeof(cmd));

	hci_dev_lock(hdev);

	rcu_read_lock();

	 
	list_for_each_entry_rcu(conn, &hdev->conn_hash.list, list) {
		if (test_bit(HCI_CONN_CREATE_CIS, &conn->flags))
			goto done;
	}

	 
	list_for_each_entry_rcu(conn, &hdev->conn_hash.list, list) {
		struct hci_conn *link;

		if (hci_conn_check_create_cis(conn))
			continue;

		cig = conn->iso_qos.ucast.cig;

		list_for_each_entry_rcu(link, &hdev->conn_hash.list, list) {
			if (hci_conn_check_create_cis(link) > 0 &&
			    link->iso_qos.ucast.cig == cig &&
			    link->state != BT_CONNECTED) {
				cig = BT_ISO_QOS_CIG_UNSET;
				break;
			}
		}

		if (cig != BT_ISO_QOS_CIG_UNSET)
			break;
	}

	if (cig == BT_ISO_QOS_CIG_UNSET)
		goto done;

	list_for_each_entry_rcu(conn, &hdev->conn_hash.list, list) {
		struct hci_cis *cis = &cmd.cis[cmd.cp.num_cis];

		if (hci_conn_check_create_cis(conn) ||
		    conn->iso_qos.ucast.cig != cig)
			continue;

		set_bit(HCI_CONN_CREATE_CIS, &conn->flags);
		cis->acl_handle = cpu_to_le16(conn->parent->handle);
		cis->cis_handle = cpu_to_le16(conn->handle);
		cmd.cp.num_cis++;

		if (cmd.cp.num_cis >= ARRAY_SIZE(cmd.cis))
			break;
	}

done:
	rcu_read_unlock();

	hci_dev_unlock(hdev);

	if (!cmd.cp.num_cis)
		return 0;

	 
	return __hci_cmd_sync_status_sk(hdev, HCI_OP_LE_CREATE_CIS,
					sizeof(cmd.cp) + sizeof(cmd.cis[0]) *
					cmd.cp.num_cis, &cmd,
					HCI_EVT_LE_CIS_ESTABLISHED,
					conn->conn_timeout, NULL);
}

int hci_le_remove_cig_sync(struct hci_dev *hdev, u8 handle)
{
	struct hci_cp_le_remove_cig cp;

	memset(&cp, 0, sizeof(cp));
	cp.cig_id = handle;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_REMOVE_CIG, sizeof(cp),
				     &cp, HCI_CMD_TIMEOUT);
}

int hci_le_big_terminate_sync(struct hci_dev *hdev, u8 handle)
{
	struct hci_cp_le_big_term_sync cp;

	memset(&cp, 0, sizeof(cp));
	cp.handle = handle;

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_BIG_TERM_SYNC,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

int hci_le_pa_terminate_sync(struct hci_dev *hdev, u16 handle)
{
	struct hci_cp_le_pa_term_sync cp;

	memset(&cp, 0, sizeof(cp));
	cp.handle = cpu_to_le16(handle);

	return __hci_cmd_sync_status(hdev, HCI_OP_LE_PA_TERM_SYNC,
				     sizeof(cp), &cp, HCI_CMD_TIMEOUT);
}

int hci_get_random_address(struct hci_dev *hdev, bool require_privacy,
			   bool use_rpa, struct adv_info *adv_instance,
			   u8 *own_addr_type, bdaddr_t *rand_addr)
{
	int err;

	bacpy(rand_addr, BDADDR_ANY);

	 
	if (use_rpa) {
		 
		if (use_ll_privacy(hdev))
			*own_addr_type = ADDR_LE_DEV_RANDOM_RESOLVED;
		else
			*own_addr_type = ADDR_LE_DEV_RANDOM;

		if (adv_instance) {
			if (adv_rpa_valid(adv_instance))
				return 0;
		} else {
			if (rpa_valid(hdev))
				return 0;
		}

		err = smp_generate_rpa(hdev, hdev->irk, &hdev->rpa);
		if (err < 0) {
			bt_dev_err(hdev, "failed to generate new RPA");
			return err;
		}

		bacpy(rand_addr, &hdev->rpa);

		return 0;
	}

	 
	if (require_privacy) {
		bdaddr_t nrpa;

		while (true) {
			 
			get_random_bytes(&nrpa, 6);
			nrpa.b[5] &= 0x3f;

			 
			if (bacmp(&hdev->bdaddr, &nrpa))
				break;
		}

		*own_addr_type = ADDR_LE_DEV_RANDOM;
		bacpy(rand_addr, &nrpa);

		return 0;
	}

	 
	*own_addr_type = ADDR_LE_DEV_PUBLIC;

	return 0;
}

static int _update_adv_data_sync(struct hci_dev *hdev, void *data)
{
	u8 instance = PTR_UINT(data);

	return hci_update_adv_data_sync(hdev, instance);
}

int hci_update_adv_data(struct hci_dev *hdev, u8 instance)
{
	return hci_cmd_sync_queue(hdev, _update_adv_data_sync,
				  UINT_PTR(instance), NULL);
}
