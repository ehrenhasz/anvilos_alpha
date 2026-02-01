
 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/wireless.h>
#include <linux/netdevice.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <asm/byteorder.h>
#include <linux/usb.h>
#include <linux/bitops.h>

#include "p80211types.h"
#include "p80211hdr.h"
#include "p80211mgmt.h"
#include "p80211conv.h"
#include "p80211msg.h"
#include "p80211netdev.h"
#include "p80211metadef.h"
#include "p80211metastruct.h"
#include "hfa384x.h"
#include "prism2mgmt.h"

#define MIB_TMP_MAXLEN    200	 

#define  F_STA        0x1	 
#define  F_READ       0x2	 
#define  F_WRITE      0x4	 

struct mibrec {
	u32 did;
	u16 flag;
	u16 parm1;
	u16 parm2;
	u16 parm3;
	int (*func)(struct mibrec *mib,
		    int isget,
		    struct wlandevice *wlandev,
		    struct hfa384x *hw,
		    struct p80211msg_dot11req_mibset *msg, void *data);
};

static int prism2mib_bytearea2pstr(struct mibrec *mib,
				   int isget,
				   struct wlandevice *wlandev,
				   struct hfa384x *hw,
				   struct p80211msg_dot11req_mibset *msg,
				   void *data);

static int prism2mib_uint32(struct mibrec *mib,
			    int isget,
			    struct wlandevice *wlandev,
			    struct hfa384x *hw,
			    struct p80211msg_dot11req_mibset *msg, void *data);

static int prism2mib_flag(struct mibrec *mib,
			  int isget,
			  struct wlandevice *wlandev,
			  struct hfa384x *hw,
			  struct p80211msg_dot11req_mibset *msg, void *data);

static int prism2mib_wepdefaultkey(struct mibrec *mib,
				   int isget,
				   struct wlandevice *wlandev,
				   struct hfa384x *hw,
				   struct p80211msg_dot11req_mibset *msg,
				   void *data);

static int prism2mib_privacyinvoked(struct mibrec *mib,
				    int isget,
				    struct wlandevice *wlandev,
				    struct hfa384x *hw,
				    struct p80211msg_dot11req_mibset *msg,
				    void *data);

static int
prism2mib_fragmentationthreshold(struct mibrec *mib,
				 int isget,
				 struct wlandevice *wlandev,
				 struct hfa384x *hw,
				 struct p80211msg_dot11req_mibset *msg,
				 void *data);

static int prism2mib_priv(struct mibrec *mib,
			  int isget,
			  struct wlandevice *wlandev,
			  struct hfa384x *hw,
			  struct p80211msg_dot11req_mibset *msg, void *data);

static struct mibrec mibtab[] = {
	 
	{didmib_dot11smt_wepdefaultkeystable_key(1),
	 F_STA | F_WRITE,
	 HFA384x_RID_CNFWEPDEFAULTKEY0, 0, 0,
	 prism2mib_wepdefaultkey},
	{didmib_dot11smt_wepdefaultkeystable_key(2),
	 F_STA | F_WRITE,
	 HFA384x_RID_CNFWEPDEFAULTKEY1, 0, 0,
	 prism2mib_wepdefaultkey},
	{didmib_dot11smt_wepdefaultkeystable_key(3),
	 F_STA | F_WRITE,
	 HFA384x_RID_CNFWEPDEFAULTKEY2, 0, 0,
	 prism2mib_wepdefaultkey},
	{didmib_dot11smt_wepdefaultkeystable_key(4),
	 F_STA | F_WRITE,
	 HFA384x_RID_CNFWEPDEFAULTKEY3, 0, 0,
	 prism2mib_wepdefaultkey},
	{DIDMIB_DOT11SMT_PRIVACYTABLE_PRIVACYINVOKED,
	 F_STA | F_READ | F_WRITE,
	 HFA384x_RID_CNFWEPFLAGS, HFA384x_WEPFLAGS_PRIVINVOKED, 0,
	 prism2mib_privacyinvoked},
	{DIDMIB_DOT11SMT_PRIVACYTABLE_WEPDEFAULTKEYID,
	 F_STA | F_READ | F_WRITE,
	 HFA384x_RID_CNFWEPDEFAULTKEYID, 0, 0,
	 prism2mib_uint32},
	{DIDMIB_DOT11SMT_PRIVACYTABLE_EXCLUDEUNENCRYPTED,
	 F_STA | F_READ | F_WRITE,
	 HFA384x_RID_CNFWEPFLAGS, HFA384x_WEPFLAGS_EXCLUDE, 0,
	 prism2mib_flag},

	 

	{DIDMIB_DOT11MAC_OPERATIONTABLE_MACADDRESS,
	 F_STA | F_READ | F_WRITE,
	 HFA384x_RID_CNFOWNMACADDR, HFA384x_RID_CNFOWNMACADDR_LEN, 0,
	 prism2mib_bytearea2pstr},
	{DIDMIB_DOT11MAC_OPERATIONTABLE_RTSTHRESHOLD,
	 F_STA | F_READ | F_WRITE,
	 HFA384x_RID_RTSTHRESH, 0, 0,
	 prism2mib_uint32},
	{DIDMIB_DOT11MAC_OPERATIONTABLE_SHORTRETRYLIMIT,
	 F_STA | F_READ,
	 HFA384x_RID_SHORTRETRYLIMIT, 0, 0,
	 prism2mib_uint32},
	{DIDMIB_DOT11MAC_OPERATIONTABLE_LONGRETRYLIMIT,
	 F_STA | F_READ,
	 HFA384x_RID_LONGRETRYLIMIT, 0, 0,
	 prism2mib_uint32},
	{DIDMIB_DOT11MAC_OPERATIONTABLE_FRAGMENTATIONTHRESHOLD,
	 F_STA | F_READ | F_WRITE,
	 HFA384x_RID_FRAGTHRESH, 0, 0,
	 prism2mib_fragmentationthreshold},
	{DIDMIB_DOT11MAC_OPERATIONTABLE_MAXTRANSMITMSDULIFETIME,
	 F_STA | F_READ,
	 HFA384x_RID_MAXTXLIFETIME, 0, 0,
	 prism2mib_uint32},

	 

	{DIDMIB_DOT11PHY_DSSSTABLE_CURRENTCHANNEL,
	 F_STA | F_READ,
	 HFA384x_RID_CURRENTCHANNEL, 0, 0,
	 prism2mib_uint32},
	{DIDMIB_DOT11PHY_TXPOWERTABLE_CURRENTTXPOWERLEVEL,
	 F_STA | F_READ | F_WRITE,
	 HFA384x_RID_TXPOWERMAX, 0, 0,
	 prism2mib_uint32},

	 

	{DIDMIB_P2_STATIC_CNFPORTTYPE,
	 F_STA | F_READ | F_WRITE,
	 HFA384x_RID_CNFPORTTYPE, 0, 0,
	 prism2mib_uint32},

	 

	{DIDMIB_P2_MAC_CURRENTTXRATE,
	 F_STA | F_READ,
	 HFA384x_RID_CURRENTTXRATE, 0, 0,
	 prism2mib_uint32},

	 
	{DIDMIB_LNX_CONFIGTABLE_RSNAIE,
	 F_STA | F_READ | F_WRITE,
	 HFA384x_RID_CNFWPADATA, 0, 0,
	 prism2mib_priv},
	{0, 0, 0, 0, 0, NULL}
};

 

int prism2mgmt_mibset_mibget(struct wlandevice *wlandev, void *msgp)
{
	struct hfa384x *hw = wlandev->priv;
	int result, isget;
	struct mibrec *mib;

	u16 which;

	struct p80211msg_dot11req_mibset *msg = msgp;
	struct p80211itemd *mibitem;

	msg->resultcode.status = P80211ENUM_msgitem_status_data_ok;
	msg->resultcode.data = P80211ENUM_resultcode_success;

	 

	which = F_STA;

	 

	mibitem = (struct p80211itemd *)msg->mibattribute.data;

	for (mib = mibtab; mib->did != 0; mib++)
		if (mib->did == mibitem->did && (mib->flag & which))
			break;

	if (mib->did == 0) {
		msg->resultcode.data = P80211ENUM_resultcode_not_supported;
		goto done;
	}

	 

	isget = (msg->msgcode == DIDMSG_DOT11REQ_MIBGET);

	if (isget) {
		if (!(mib->flag & F_READ)) {
			msg->resultcode.data =
			    P80211ENUM_resultcode_cant_get_writeonly_mib;
			goto done;
		}
	} else {
		if (!(mib->flag & F_WRITE)) {
			msg->resultcode.data =
			    P80211ENUM_resultcode_cant_set_readonly_mib;
			goto done;
		}
	}

	 

	result = mib->func(mib, isget, wlandev, hw, msg, (void *)mibitem->data);

	if (msg->resultcode.data == P80211ENUM_resultcode_success) {
		if (result != 0) {
			pr_debug("get/set failure, result=%d\n", result);
			msg->resultcode.data =
			    P80211ENUM_resultcode_implementation_failure;
		} else {
			if (isget) {
				msg->mibattribute.status =
				    P80211ENUM_msgitem_status_data_ok;
				mibitem->status =
				    P80211ENUM_msgitem_status_data_ok;
			}
		}
	}

done:
	return 0;
}

 

static int prism2mib_bytearea2pstr(struct mibrec *mib,
				   int isget,
				   struct wlandevice *wlandev,
				   struct hfa384x *hw,
				   struct p80211msg_dot11req_mibset *msg,
				   void *data)
{
	int result;
	struct p80211pstrd *pstr = data;
	u8 bytebuf[MIB_TMP_MAXLEN];

	if (isget) {
		result =
		    hfa384x_drvr_getconfig(hw, mib->parm1, bytebuf, mib->parm2);
		prism2mgmt_bytearea2pstr(bytebuf, pstr, mib->parm2);
	} else {
		memset(bytebuf, 0, mib->parm2);
		memcpy(bytebuf, pstr->data, pstr->len);
		result =
		    hfa384x_drvr_setconfig(hw, mib->parm1, bytebuf, mib->parm2);
	}

	return result;
}

 

static int prism2mib_uint32(struct mibrec *mib,
			    int isget,
			    struct wlandevice *wlandev,
			    struct hfa384x *hw,
			    struct p80211msg_dot11req_mibset *msg, void *data)
{
	int result;
	u32 *uint32 = data;
	u8 bytebuf[MIB_TMP_MAXLEN];
	u16 *wordbuf = (u16 *)bytebuf;

	if (isget) {
		result = hfa384x_drvr_getconfig16(hw, mib->parm1, wordbuf);
		*uint32 = *wordbuf;
	} else {
		*wordbuf = *uint32;
		result = hfa384x_drvr_setconfig16(hw, mib->parm1, *wordbuf);
	}

	return result;
}

 

static int prism2mib_flag(struct mibrec *mib,
			  int isget,
			  struct wlandevice *wlandev,
			  struct hfa384x *hw,
			  struct p80211msg_dot11req_mibset *msg, void *data)
{
	int result;
	u32 *uint32 = data;
	u8 bytebuf[MIB_TMP_MAXLEN];
	u16 *wordbuf = (u16 *)bytebuf;
	u32 flags;

	result = hfa384x_drvr_getconfig16(hw, mib->parm1, wordbuf);
	if (result == 0) {
		flags = *wordbuf;
		if (isget) {
			*uint32 = (flags & mib->parm2) ?
			    P80211ENUM_truth_true : P80211ENUM_truth_false;
		} else {
			if ((*uint32) == P80211ENUM_truth_true)
				flags |= mib->parm2;
			else
				flags &= ~mib->parm2;
			*wordbuf = flags;
			result =
			    hfa384x_drvr_setconfig16(hw, mib->parm1, *wordbuf);
		}
	}

	return result;
}

 

static int prism2mib_wepdefaultkey(struct mibrec *mib,
				   int isget,
				   struct wlandevice *wlandev,
				   struct hfa384x *hw,
				   struct p80211msg_dot11req_mibset *msg,
				   void *data)
{
	int result;
	struct p80211pstrd *pstr = data;
	u8 bytebuf[MIB_TMP_MAXLEN];
	u16 len;

	if (isget) {
		result = 0;	 
	} else {
		len = (pstr->len > 5) ? HFA384x_RID_CNFWEP128DEFAULTKEY_LEN :
		    HFA384x_RID_CNFWEPDEFAULTKEY_LEN;
		memset(bytebuf, 0, len);
		memcpy(bytebuf, pstr->data, pstr->len);
		result = hfa384x_drvr_setconfig(hw, mib->parm1, bytebuf, len);
	}

	return result;
}

 

static int prism2mib_privacyinvoked(struct mibrec *mib,
				    int isget,
				    struct wlandevice *wlandev,
				    struct hfa384x *hw,
				    struct p80211msg_dot11req_mibset *msg,
				    void *data)
{
	if (wlandev->hostwep & HOSTWEP_DECRYPT) {
		if (wlandev->hostwep & HOSTWEP_DECRYPT)
			mib->parm2 |= HFA384x_WEPFLAGS_DISABLE_RXCRYPT;
		if (wlandev->hostwep & HOSTWEP_ENCRYPT)
			mib->parm2 |= HFA384x_WEPFLAGS_DISABLE_TXCRYPT;
	}

	return prism2mib_flag(mib, isget, wlandev, hw, msg, data);
}

 

static int
prism2mib_fragmentationthreshold(struct mibrec *mib,
				 int isget,
				 struct wlandevice *wlandev,
				 struct hfa384x *hw,
				 struct p80211msg_dot11req_mibset *msg,
				 void *data)
{
	u32 *uint32 = data;

	if (!isget)
		if ((*uint32) % 2) {
			netdev_warn(wlandev->netdev,
				    "Attempt to set odd number FragmentationThreshold\n");
			msg->resultcode.data =
			    P80211ENUM_resultcode_not_supported;
			return 0;
		}

	return prism2mib_uint32(mib, isget, wlandev, hw, msg, data);
}

 

static int prism2mib_priv(struct mibrec *mib,
			  int isget,
			  struct wlandevice *wlandev,
			  struct hfa384x *hw,
			  struct p80211msg_dot11req_mibset *msg, void *data)
{
	struct p80211pstrd *pstr = data;

	switch (mib->did) {
	case DIDMIB_LNX_CONFIGTABLE_RSNAIE: {
		 
		struct hfa384x_wpa_data wpa;

		if (isget) {
			hfa384x_drvr_getconfig(hw,
					       HFA384x_RID_CNFWPADATA,
					       (u8 *)&wpa,
					       sizeof(wpa));
			pstr->len = 0;
		} else {
			wpa.datalen = 0;

			hfa384x_drvr_setconfig(hw,
					       HFA384x_RID_CNFWPADATA,
					       (u8 *)&wpa,
					       sizeof(wpa));
		}
		break;
	}
	default:
		netdev_err(wlandev->netdev, "Unhandled DID 0x%08x\n", mib->did);
	}

	return 0;
}

 

void prism2mgmt_pstr2bytestr(struct hfa384x_bytestr *bytestr,
			     struct p80211pstrd *pstr)
{
	bytestr->len = cpu_to_le16((u16)(pstr->len));
	memcpy(bytestr->data, pstr->data, pstr->len);
}

 

void prism2mgmt_bytestr2pstr(struct hfa384x_bytestr *bytestr,
			     struct p80211pstrd *pstr)
{
	pstr->len = (u8)(le16_to_cpu(bytestr->len));
	memcpy(pstr->data, bytestr->data, pstr->len);
}

 

void prism2mgmt_bytearea2pstr(u8 *bytearea, struct p80211pstrd *pstr, int len)
{
	pstr->len = (u8)len;
	memcpy(pstr->data, bytearea, len);
}
