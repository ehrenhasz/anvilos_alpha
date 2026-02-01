 
 

#ifndef __LOCAL_ST_NCI_H_
#define __LOCAL_ST_NCI_H_

#include "ndlc.h"

 
#define ST_NCI_RUNNING			1

#define ST_NCI_CORE_PROP                0x01
#define ST_NCI_SET_NFC_MODE             0x02

 
#define ST_NCI_ESE_MAX_LENGTH  33

#define ST_NCI_DEVICE_MGNT_GATE		0x01

#define ST_NCI_VENDOR_OUI 0x0080E1  
#define ST_NCI_FACTORY_MODE 2

struct nci_mode_set_cmd {
	u8 cmd_type;
	u8 mode;
} __packed;

struct nci_mode_set_rsp {
	u8 status;
} __packed;

struct st_nci_se_status {
	bool is_ese_present;
	bool is_uicc_present;
};

struct st_nci_se_info {
	struct st_nci_se_status *se_status;
	u8 atr[ST_NCI_ESE_MAX_LENGTH];
	struct completion req_completion;

	struct timer_list bwi_timer;
	int wt_timeout;  
	bool bwi_active;

	struct timer_list se_active_timer;
	bool se_active;

	bool xch_error;

	se_io_cb_t cb;
	void *cb_context;
};

 
enum nfc_vendor_cmds {
	FACTORY_MODE,
	HCI_CLEAR_ALL_PIPES,
	HCI_DM_PUT_DATA,
	HCI_DM_UPDATE_AID,
	HCI_DM_GET_INFO,
	HCI_DM_GET_DATA,
	HCI_DM_DIRECT_LOAD,
	HCI_DM_RESET,
	HCI_GET_PARAM,
	HCI_DM_FIELD_GENERATOR,
	LOOPBACK,
	HCI_DM_FWUPD_START,
	HCI_DM_FWUPD_END,
	HCI_DM_VDC_MEASUREMENT_VALUE,
	HCI_DM_VDC_VALUE_COMPARISON,
	MANUFACTURER_SPECIFIC,
};

struct st_nci_info {
	struct llt_ndlc *ndlc;
	unsigned long flags;

	struct st_nci_se_info se_info;
};

void st_nci_remove(struct nci_dev *ndev);
int st_nci_probe(struct llt_ndlc *ndlc, int phy_headroom,
		 int phy_tailroom, struct st_nci_se_status *se_status);

int st_nci_se_init(struct nci_dev *ndev, struct st_nci_se_status *se_status);
void st_nci_se_deinit(struct nci_dev *ndev);

int st_nci_discover_se(struct nci_dev *ndev);
int st_nci_enable_se(struct nci_dev *ndev, u32 se_idx);
int st_nci_disable_se(struct nci_dev *ndev, u32 se_idx);
int st_nci_se_io(struct nci_dev *ndev, u32 se_idx,
				u8 *apdu, size_t apdu_length,
				se_io_cb_t cb, void *cb_context);
int st_nci_hci_load_session(struct nci_dev *ndev);
void st_nci_hci_event_received(struct nci_dev *ndev, u8 pipe,
					u8 event, struct sk_buff *skb);
void st_nci_hci_cmd_received(struct nci_dev *ndev, u8 pipe, u8 cmd,
						struct sk_buff *skb);

int st_nci_vendor_cmds_init(struct nci_dev *ndev);

#endif  
