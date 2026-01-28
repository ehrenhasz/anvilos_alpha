#ifndef __INTEL_TH_STH_H__
#define __INTEL_TH_STH_H__
enum {
	REG_STH_STHCAP0		= 0x0000,  
	REG_STH_STHCAP1		= 0x0004,  
	REG_STH_TRIG		= 0x0008,  
	REG_STH_TRIG_TS		= 0x000c,  
	REG_STH_XSYNC		= 0x0010,  
	REG_STH_XSYNC_TS	= 0x0014,  
	REG_STH_GERR		= 0x0018,  
};
struct intel_th_channel {
	u64	Dn;
	u64	DnM;
	u64	DnTS;
	u64	DnMTS;
	u64	USER;
	u64	USER_TS;
	u32	FLAG;
	u32	FLAG_TS;
	u32	MERR;
	u32	__unused;
} __packed;
#endif  
