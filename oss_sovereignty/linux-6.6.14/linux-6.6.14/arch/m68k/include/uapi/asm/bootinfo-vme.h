#ifndef _UAPI_ASM_M68K_BOOTINFO_VME_H
#define _UAPI_ASM_M68K_BOOTINFO_VME_H
#include <linux/types.h>
#define BI_VME_TYPE		0x8000	 
#define BI_VME_BRDINFO		0x8001	 
#define VME_TYPE_TP34V		0x0034	 
#define VME_TYPE_MVME147	0x0147	 
#define VME_TYPE_MVME162	0x0162	 
#define VME_TYPE_MVME166	0x0166	 
#define VME_TYPE_MVME167	0x0167	 
#define VME_TYPE_MVME172	0x0172	 
#define VME_TYPE_MVME177	0x0177	 
#define VME_TYPE_BVME4000	0x4000	 
#define VME_TYPE_BVME6000	0x6000	 
#ifndef __ASSEMBLY__
typedef struct {
	char	bdid[4];
	__u8	rev, mth, day, yr;
	__be16	size, reserved;
	__be16	brdno;
	char	brdsuffix[2];
	__be32	options;
	__be16	clun, dlun, ctype, dnum;
	__be32	option2;
} t_bdid, *p_bdid;
#endif  
#define MVME147_BOOTI_VERSION	MK_BI_VERSION(2, 0)
#define MVME16x_BOOTI_VERSION	MK_BI_VERSION(2, 0)
#define BVME6000_BOOTI_VERSION	MK_BI_VERSION(2, 0)
#endif  
