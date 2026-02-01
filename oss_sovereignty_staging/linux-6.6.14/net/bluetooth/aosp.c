
 

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include "aosp.h"

 
struct aosp_rp_le_get_vendor_capa {
	 
	__u8	status;
	__u8	max_advt_instances;
	__u8	offloaded_resolution_of_private_address;
	__le16	total_scan_results_storage;
	__u8	max_irk_list_sz;
	__u8	filtering_support;
	__u8	max_filter;
	__u8	activity_energy_info_support;
	__le16	version_supported;
	__le16	total_num_of_advt_tracked;
	__u8	extended_scan_support;
	__u8	debug_logging_supported;
	 
	__u8	le_address_generation_offloading_support;
	 
	__le32	a2dp_source_offload_capability_mask;
	__u8	bluetooth_quality_report_support;
	 
	__le32	dynamic_audio_buffer_support;
} __packed;

#define VENDOR_CAPA_BASE_SIZE		15
#define VENDOR_CAPA_0_98_SIZE		21

void aosp_do_open(struct hci_dev *hdev)
{
	struct sk_buff *skb;
	struct aosp_rp_le_get_vendor_capa *rp;
	u16 version_supported;

	if (!hdev->aosp_capable)
		return;

	bt_dev_dbg(hdev, "Initialize AOSP extension");

	 
	skb = __hci_cmd_sync(hdev, hci_opcode_pack(0x3f, 0x153), 0, NULL,
			     HCI_CMD_TIMEOUT);
	if (IS_ERR_OR_NULL(skb)) {
		if (!skb)
			skb = ERR_PTR(-EIO);

		bt_dev_err(hdev, "AOSP get vendor capabilities (%ld)",
			   PTR_ERR(skb));
		return;
	}

	 
	if (skb->len < VENDOR_CAPA_BASE_SIZE)
		goto length_error;

	rp = (struct aosp_rp_le_get_vendor_capa *)skb->data;

	version_supported = le16_to_cpu(rp->version_supported);
	 
	bt_dev_info(hdev, "AOSP extensions version v%u.%02u",
		    version_supported >> 8, version_supported & 0xff);

	 
	if (version_supported < 95) {
		bt_dev_warn(hdev, "AOSP capabilities version %u too old",
			    version_supported);
		goto done;
	}

	if (version_supported < 98) {
		bt_dev_warn(hdev, "AOSP quality report is not supported");
		goto done;
	}

	if (skb->len < VENDOR_CAPA_0_98_SIZE)
		goto length_error;

	 
	if (rp->bluetooth_quality_report_support) {
		hdev->aosp_quality_report = true;
		bt_dev_info(hdev, "AOSP quality report is supported");
	}

	goto done;

length_error:
	bt_dev_err(hdev, "AOSP capabilities length %d too short", skb->len);

done:
	kfree_skb(skb);
}

void aosp_do_close(struct hci_dev *hdev)
{
	if (!hdev->aosp_capable)
		return;

	bt_dev_dbg(hdev, "Cleanup of AOSP extension");
}

 
#define BQR_OPCODE			hci_opcode_pack(0x3f, 0x015e)

 
#define REPORT_ACTION_ADD		0x00
#define REPORT_ACTION_DELETE		0x01
#define REPORT_ACTION_CLEAR		0x02

 
#define QUALITY_MONITORING		BIT(0)
#define APPRAOCHING_LSTO		BIT(1)
#define A2DP_AUDIO_CHOPPY		BIT(2)
#define SCO_VOICE_CHOPPY		BIT(3)

#define DEFAULT_BQR_EVENT_MASK	(QUALITY_MONITORING | APPRAOCHING_LSTO | \
				 A2DP_AUDIO_CHOPPY | SCO_VOICE_CHOPPY)

 
#define DEFALUT_REPORT_INTERVAL_MS	5000

struct aosp_bqr_cp {
	__u8	report_action;
	__u32	event_mask;
	__u16	min_report_interval;
} __packed;

static int enable_quality_report(struct hci_dev *hdev)
{
	struct sk_buff *skb;
	struct aosp_bqr_cp cp;

	cp.report_action = REPORT_ACTION_ADD;
	cp.event_mask = DEFAULT_BQR_EVENT_MASK;
	cp.min_report_interval = DEFALUT_REPORT_INTERVAL_MS;

	skb = __hci_cmd_sync(hdev, BQR_OPCODE, sizeof(cp), &cp,
			     HCI_CMD_TIMEOUT);
	if (IS_ERR_OR_NULL(skb)) {
		if (!skb)
			skb = ERR_PTR(-EIO);

		bt_dev_err(hdev, "Enabling Android BQR failed (%ld)",
			   PTR_ERR(skb));
		return PTR_ERR(skb);
	}

	kfree_skb(skb);
	return 0;
}

static int disable_quality_report(struct hci_dev *hdev)
{
	struct sk_buff *skb;
	struct aosp_bqr_cp cp = { 0 };

	cp.report_action = REPORT_ACTION_CLEAR;

	skb = __hci_cmd_sync(hdev, BQR_OPCODE, sizeof(cp), &cp,
			     HCI_CMD_TIMEOUT);
	if (IS_ERR_OR_NULL(skb)) {
		if (!skb)
			skb = ERR_PTR(-EIO);

		bt_dev_err(hdev, "Disabling Android BQR failed (%ld)",
			   PTR_ERR(skb));
		return PTR_ERR(skb);
	}

	kfree_skb(skb);
	return 0;
}

bool aosp_has_quality_report(struct hci_dev *hdev)
{
	return hdev->aosp_quality_report;
}

int aosp_set_quality_report(struct hci_dev *hdev, bool enable)
{
	if (!aosp_has_quality_report(hdev))
		return -EOPNOTSUPP;

	bt_dev_dbg(hdev, "quality report enable %d", enable);

	 
	if (enable)
		return enable_quality_report(hdev);
	else
		return disable_quality_report(hdev);
}
