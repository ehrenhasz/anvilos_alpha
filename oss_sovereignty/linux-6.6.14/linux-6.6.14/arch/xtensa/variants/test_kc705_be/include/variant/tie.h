#ifndef _XTENSA_CORE_TIE_H
#define _XTENSA_CORE_TIE_H
#define XCHAL_CP_NUM			2	 
#define XCHAL_CP_MAX			8	 
#define XCHAL_CP_MASK			0x82	 
#define XCHAL_CP_PORT_MASK		0x80	 
#define XCHAL_CP1_NAME			"AudioEngineLX"
#define XCHAL_CP1_IDENT			AudioEngineLX
#define XCHAL_CP1_SA_SIZE		120	 
#define XCHAL_CP1_SA_ALIGN		8	 
#define XCHAL_CP_ID_AUDIOENGINELX   	1	 
#define XCHAL_CP7_NAME			"XTIOP"
#define XCHAL_CP7_IDENT			XTIOP
#define XCHAL_CP7_SA_SIZE		0	 
#define XCHAL_CP7_SA_ALIGN		1	 
#define XCHAL_CP_ID_XTIOP           	7	 
#define XCHAL_CP0_SA_SIZE		0
#define XCHAL_CP0_SA_ALIGN		1
#define XCHAL_CP2_SA_SIZE		0
#define XCHAL_CP2_SA_ALIGN		1
#define XCHAL_CP3_SA_SIZE		0
#define XCHAL_CP3_SA_ALIGN		1
#define XCHAL_CP4_SA_SIZE		0
#define XCHAL_CP4_SA_ALIGN		1
#define XCHAL_CP5_SA_SIZE		0
#define XCHAL_CP5_SA_ALIGN		1
#define XCHAL_CP6_SA_SIZE		0
#define XCHAL_CP6_SA_ALIGN		1
#define XCHAL_NCP_SA_SIZE		36
#define XCHAL_NCP_SA_ALIGN		4
#define XCHAL_TOTAL_SA_SIZE		160	 
#define XCHAL_TOTAL_SA_ALIGN		8	 
#define XCHAL_NCP_SA_NUM	9
#define XCHAL_NCP_SA_LIST(s)	\
 XCHAL_SA_REG(s,1,2,1,1,      threadptr, 4, 4, 4,0x03E7,  ur,231, 32,0,0,0) \
 XCHAL_SA_REG(s,1,0,0,1,          acclo, 4, 4, 4,0x0210,  sr,16 , 32,0,0,0) \
 XCHAL_SA_REG(s,1,0,0,1,          acchi, 4, 4, 4,0x0211,  sr,17 ,  8,0,0,0) \
 XCHAL_SA_REG(s,0,0,0,1,             br, 4, 4, 4,0x0204,  sr,4  , 16,0,0,0) \
 XCHAL_SA_REG(s,0,0,0,1,      scompare1, 4, 4, 4,0x020C,  sr,12 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,0,1,             m0, 4, 4, 4,0x0220,  sr,32 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,0,1,             m1, 4, 4, 4,0x0221,  sr,33 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,0,1,             m2, 4, 4, 4,0x0222,  sr,34 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,0,1,             m3, 4, 4, 4,0x0223,  sr,35 , 32,0,0,0)
#define XCHAL_CP0_SA_NUM	0
#define XCHAL_CP0_SA_LIST(s)	 
#define XCHAL_CP1_SA_NUM	18
#define XCHAL_CP1_SA_LIST(s)	\
 XCHAL_SA_REG(s,0,0,1,0,     ae_ovf_sar, 8, 4, 4,0x03F0,  ur,240,  7,0,0,0) \
 XCHAL_SA_REG(s,0,0,1,0,     ae_bithead, 4, 4, 4,0x03F1,  ur,241, 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,1,0,ae_ts_fts_bu_bp, 4, 4, 4,0x03F2,  ur,242, 16,0,0,0) \
 XCHAL_SA_REG(s,0,0,1,0,       ae_sd_no, 4, 4, 4,0x03F3,  ur,243, 28,0,0,0) \
 XCHAL_SA_REG(s,0,0,1,0,     ae_cbegin0, 4, 4, 4,0x03F6,  ur,246, 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,1,0,       ae_cend0, 4, 4, 4,0x03F7,  ur,247, 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           aep0, 8, 8, 8,0x0060, aep,0  , 48,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           aep1, 8, 8, 8,0x0061, aep,1  , 48,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           aep2, 8, 8, 8,0x0062, aep,2  , 48,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           aep3, 8, 8, 8,0x0063, aep,3  , 48,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           aep4, 8, 8, 8,0x0064, aep,4  , 48,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           aep5, 8, 8, 8,0x0065, aep,5  , 48,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           aep6, 8, 8, 8,0x0066, aep,6  , 48,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           aep7, 8, 8, 8,0x0067, aep,7  , 48,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           aeq0, 8, 8, 8,0x0068, aeq,0  , 56,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           aeq1, 8, 8, 8,0x0069, aeq,1  , 56,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           aeq2, 8, 8, 8,0x006A, aeq,2  , 56,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           aeq3, 8, 8, 8,0x006B, aeq,3  , 56,0,0,0)
#define XCHAL_CP2_SA_NUM	0
#define XCHAL_CP2_SA_LIST(s)	 
#define XCHAL_CP3_SA_NUM	0
#define XCHAL_CP3_SA_LIST(s)	 
#define XCHAL_CP4_SA_NUM	0
#define XCHAL_CP4_SA_LIST(s)	 
#define XCHAL_CP5_SA_NUM	0
#define XCHAL_CP5_SA_LIST(s)	 
#define XCHAL_CP6_SA_NUM	0
#define XCHAL_CP6_SA_LIST(s)	 
#define XCHAL_CP7_SA_NUM	0
#define XCHAL_CP7_SA_LIST(s)	 
#define XCHAL_OP0_FORMAT_LENGTHS	3,3,3,3,3,3,3,3,2,2,2,2,2,2,3,8
#define XCHAL_BYTE0_FORMAT_LENGTHS	\
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,\
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,\
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,\
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,\
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,\
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,\
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,\
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
#endif  
