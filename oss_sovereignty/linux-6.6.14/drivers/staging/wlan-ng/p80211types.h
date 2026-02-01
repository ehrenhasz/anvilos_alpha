 
 

#ifndef _P80211TYPES_H
#define _P80211TYPES_H

 
 
 

 
#define P80211_MIB_CAT_DOT11SMT		1
#define P80211_MIB_CAT_DOT11MAC		2
#define P80211_MIB_CAT_DOT11PHY		3

#define P80211SEC_DOT11SMT		P80211_MIB_CAT_DOT11SMT
#define P80211SEC_DOT11MAC		P80211_MIB_CAT_DOT11MAC
#define P80211SEC_DOT11PHY		P80211_MIB_CAT_DOT11PHY

 
#define P80211_MSG_CAT_DOT11REQ		1
#define P80211_MSG_CAT_DOT11IND		2

 
 
 
 

 

#define P80211ENUM_truth_false			0
#define P80211ENUM_truth_true			1
#define P80211ENUM_ifstate_disable		0
#define P80211ENUM_ifstate_fwload		1
#define P80211ENUM_ifstate_enable		2
#define P80211ENUM_bsstype_infrastructure	1
#define P80211ENUM_bsstype_independent		2
#define P80211ENUM_bsstype_any			3
#define P80211ENUM_authalg_opensystem		1
#define P80211ENUM_authalg_sharedkey		2
#define P80211ENUM_scantype_active		1
#define P80211ENUM_resultcode_success		1
#define P80211ENUM_resultcode_invalid_parameters	2
#define P80211ENUM_resultcode_not_supported	3
#define P80211ENUM_resultcode_refused		6
#define P80211ENUM_resultcode_cant_set_readonly_mib	10
#define P80211ENUM_resultcode_implementation_failure	11
#define P80211ENUM_resultcode_cant_get_writeonly_mib	12
#define P80211ENUM_status_successful		0
#define P80211ENUM_status_unspec_failure	1
#define P80211ENUM_status_ap_full		17
#define P80211ENUM_msgitem_status_data_ok		0
#define P80211ENUM_msgitem_status_no_value		1

 
 

#define MAXLEN_PSTR6		(6)	 
#define MAXLEN_PSTR14		(14)	 
#define MAXLEN_PSTR32		(32)	 
#define MAXLEN_PSTR255		(255)	 
#define MAXLEN_MIBATTRIBUTE	(392)	 
					 
					 
					 

 

#define P80211DID_LSB_SECTION		(0)
#define P80211DID_LSB_GROUP		(6)
#define P80211DID_LSB_ITEM		(12)
#define P80211DID_LSB_INDEX		(18)
#define P80211DID_LSB_ISTABLE		(26)
#define P80211DID_LSB_ACCESS		(27)

#define P80211DID_MASK_SECTION		(0x0000003fUL)
#define P80211DID_MASK_GROUP		(0x0000003fUL)
#define P80211DID_MASK_ITEM		(0x0000003fUL)
#define P80211DID_MASK_INDEX		(0x000000ffUL)
#define P80211DID_MASK_ISTABLE		(0x00000001UL)
#define P80211DID_MASK_ACCESS		(0x00000003UL)

#define P80211DID_MK(a, m, l)	((((u32)(a)) & (m)) << (l))

#define P80211DID_MKSECTION(a)	P80211DID_MK(a, \
					P80211DID_MASK_SECTION, \
					P80211DID_LSB_SECTION)
#define P80211DID_MKGROUP(a)	P80211DID_MK(a, \
					P80211DID_MASK_GROUP, \
					P80211DID_LSB_GROUP)
#define P80211DID_MKITEM(a)	P80211DID_MK(a, \
					P80211DID_MASK_ITEM, \
					P80211DID_LSB_ITEM)
#define P80211DID_MKINDEX(a)	P80211DID_MK(a, \
					P80211DID_MASK_INDEX, \
					P80211DID_LSB_INDEX)
#define P80211DID_MKISTABLE(a)	P80211DID_MK(a, \
					P80211DID_MASK_ISTABLE, \
					P80211DID_LSB_ISTABLE)

#define P80211DID_MKID(s, g, i, n, t, a)	(P80211DID_MKSECTION(s) | \
					P80211DID_MKGROUP(g) | \
					P80211DID_MKITEM(i) | \
					P80211DID_MKINDEX(n) | \
					P80211DID_MKISTABLE(t) | \
					(a))

#define P80211DID_GET(a, m, l)	((((u32)(a)) >> (l)) & (m))

#define P80211DID_SECTION(a)	P80211DID_GET(a, \
					P80211DID_MASK_SECTION, \
					P80211DID_LSB_SECTION)
#define P80211DID_GROUP(a)	P80211DID_GET(a, \
					P80211DID_MASK_GROUP, \
					P80211DID_LSB_GROUP)
#define P80211DID_ITEM(a)	P80211DID_GET(a, \
					P80211DID_MASK_ITEM, \
					P80211DID_LSB_ITEM)
#define P80211DID_INDEX(a)	P80211DID_GET(a, \
					P80211DID_MASK_INDEX, \
					P80211DID_LSB_INDEX)
#define P80211DID_ISTABLE(a)	P80211DID_GET(a, \
					P80211DID_MASK_ISTABLE, \
					P80211DID_LSB_ISTABLE)
#define P80211DID_ACCESS(a)	P80211DID_GET(a, \
					P80211DID_MASK_ACCESS, \
					P80211DID_LSB_ACCESS)

 
 
 

 
struct p80211pstr {
	u8 len;
} __packed;

struct p80211pstrd {
	u8 len;
	u8 data[];
} __packed;

 
struct p80211pstr255 {
	u8 len;
	u8 data[MAXLEN_PSTR255];
} __packed;

 
struct p80211pstr6 {
	u8 len;
	u8 data[MAXLEN_PSTR6];
} __packed;

 
struct p80211pstr14 {
	u8 len;
	u8 data[MAXLEN_PSTR14];
} __packed;

 
struct p80211pstr32 {
	u8 len;
	u8 data[MAXLEN_PSTR32];
} __packed;

 
struct p80211item {
	u32 did;
	u16 status;
	u16 len;
} __packed;

 
struct p80211itemd {
	u32 did;
	u16 status;
	u16 len;
	u8 data[];
} __packed;

 
struct p80211item_uint32 {
	u32 did;
	u16 status;
	u16 len;
	u32 data;
} __packed;

 
struct p80211item_pstr6 {
	u32 did;
	u16 status;
	u16 len;
	struct p80211pstr6 data;
} __packed;

 
struct p80211item_pstr14 {
	u32 did;
	u16 status;
	u16 len;
	struct p80211pstr14 data;
} __packed;

 
struct p80211item_pstr32 {
	u32 did;
	u16 status;
	u16 len;
	struct p80211pstr32 data;
} __packed;

 
struct p80211item_pstr255 {
	u32 did;
	u16 status;
	u16 len;
	struct p80211pstr255 data;
} __packed;

 
struct p80211item_unk392 {
	u32 did;
	u16 status;
	u16 len;
	u8 data[MAXLEN_MIBATTRIBUTE];
} __packed;

 
struct p80211item_unk1024 {
	u32 did;
	u16 status;
	u16 len;
	u8 data[1024];
} __packed;

 
struct p80211item_unk4096 {
	u32 did;
	u16 status;
	u16 len;
	u8 data[4096];
} __packed;

#endif  
