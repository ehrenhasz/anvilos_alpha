#ifndef _ASM_S390_CPU_MF_H
#define _ASM_S390_CPU_MF_H
#include <linux/errno.h>
#include <asm/asm-extable.h>
#include <asm/facility.h>
asm(".include \"asm/cpu_mf-insn.h\"\n");
#define CPU_MF_INT_SF_IAE	(1 << 31)	 
#define CPU_MF_INT_SF_ISE	(1 << 30)	 
#define CPU_MF_INT_SF_PRA	(1 << 29)	 
#define CPU_MF_INT_SF_SACA	(1 << 23)	 
#define CPU_MF_INT_SF_LSDA	(1 << 22)	 
#define CPU_MF_INT_CF_MTDA	(1 << 15)	 
#define CPU_MF_INT_CF_CACA	(1 <<  7)	 
#define CPU_MF_INT_CF_LCDA	(1 <<  6)	 
#define CPU_MF_INT_CF_MASK	(CPU_MF_INT_CF_MTDA|CPU_MF_INT_CF_CACA| \
				 CPU_MF_INT_CF_LCDA)
#define CPU_MF_INT_SF_MASK	(CPU_MF_INT_SF_IAE|CPU_MF_INT_SF_ISE|	\
				 CPU_MF_INT_SF_PRA|CPU_MF_INT_SF_SACA|	\
				 CPU_MF_INT_SF_LSDA)
#define CPU_MF_SF_RIBM_NOTAV	0x1		 
static inline int cpum_cf_avail(void)
{
	return test_facility(40) && test_facility(67);
}
static inline int cpum_sf_avail(void)
{
	return test_facility(40) && test_facility(68);
}
struct cpumf_ctr_info {
	u16   cfvn;
	u16   auth_ctl;
	u16   enable_ctl;
	u16   act_ctl;
	u16   max_cpu;
	u16   csvn;
	u16   max_cg;
	u16   reserved1;
	u32   reserved2[12];
} __packed;
struct hws_qsi_info_block {	     
	unsigned int b0_13:14;	     
	unsigned int as:1;	     
	unsigned int ad:1;	     
	unsigned int b16_21:6;	     
	unsigned int es:1;	     
	unsigned int ed:1;	     
	unsigned int b24_29:6;	     
	unsigned int cs:1;	     
	unsigned int cd:1;	     
	unsigned int bsdes:16;	     
	unsigned int dsdes:16;	     
	unsigned long min_sampl_rate;  
	unsigned long max_sampl_rate;  
	unsigned long tear;	     
	unsigned long dear;	     
	unsigned int rsvrd0:24;	     
	unsigned int ribm:8;	     
	unsigned int cpu_speed;      
	unsigned long long rsvrd1;   
	unsigned long long rsvrd2;   
} __packed;
struct hws_lsctl_request_block {
	unsigned int s:1;	     
	unsigned int h:1;	     
	unsigned long long b2_53:52; 
	unsigned int es:1;	     
	unsigned int ed:1;	     
	unsigned int b56_61:6;	     
	unsigned int cs:1;	     
	unsigned int cd:1;	     
	unsigned long interval;      
	unsigned long tear;	     
	unsigned long dear;	     
	unsigned long rsvrd1;	     
	unsigned long rsvrd2;	     
	unsigned long rsvrd3;	     
	unsigned long rsvrd4;	     
} __packed;
struct hws_basic_entry {
	unsigned int def:16;	     
	unsigned int R:4;	     
	unsigned int U:4;	     
	unsigned int z:2;	     
	unsigned int T:1;	     
	unsigned int W:1;	     
	unsigned int P:1;	     
	unsigned int AS:2;	     
	unsigned int I:1;	     
	unsigned int CL:2;	     
	unsigned int H:1;	     
	unsigned int LS:1;	     
	unsigned int:12;
	unsigned int prim_asn:16;    
	unsigned long long ia;	     
	unsigned long long gpp;      
	unsigned long long hpp;      
} __packed;
struct hws_diag_entry {
	unsigned int def:16;	     
	unsigned int R:15;	     
	unsigned int I:1;	     
	u8	     data[];	     
} __packed;
struct hws_combined_entry {
	struct hws_basic_entry	basic;	 
	struct hws_diag_entry	diag;	 
} __packed;
union hws_trailer_header {
	struct {
		unsigned int f:1;	 
		unsigned int a:1;	 
		unsigned int t:1;	 
		unsigned int :29;	 
		unsigned int bsdes:16;	 
		unsigned int dsdes:16;	 
		unsigned long long overflow;  
	};
	u128 val;
};
struct hws_trailer_entry {
	union hws_trailer_header header;  
	unsigned char timestamp[16];	  
	unsigned long long reserved1;	  
	unsigned long long reserved2;	  
	union {				  
		struct {
			unsigned int clock_base:1;  
			unsigned long long progusage1:63;
			unsigned long long progusage2;
		};
		unsigned long long progusage[2];
	};
} __packed;
static inline void lpp(void *pp)
{
	asm volatile("lpp 0(%0)\n" :: "a" (pp) : "memory");
}
static inline int qctri(struct cpumf_ctr_info *info)
{
	int rc = -EINVAL;
	asm volatile (
		"0:	qctri	%1\n"
		"1:	lhi	%0,0\n"
		"2:\n"
		EX_TABLE(1b, 2b)
		: "+d" (rc), "=Q" (*info));
	return rc;
}
static inline int lcctl(u64 ctl)
{
	int cc;
	asm volatile (
		"	lcctl	%1\n"
		"	ipm	%0\n"
		"	srl	%0,28\n"
		: "=d" (cc) : "Q" (ctl) : "cc");
	return cc;
}
static inline int __ecctr(u64 ctr, u64 *content)
{
	u64 _content;
	int cc;
	asm volatile (
		"	ecctr	%0,%2\n"
		"	ipm	%1\n"
		"	srl	%1,28\n"
		: "=d" (_content), "=d" (cc) : "d" (ctr) : "cc");
	*content = _content;
	return cc;
}
static inline int ecctr(u64 ctr, u64 *val)
{
	u64 content;
	int cc;
	cc = __ecctr(ctr, &content);
	if (!cc)
		*val = content;
	return cc;
}
enum stcctm_ctr_set {
	EXTENDED = 0,
	BASIC = 1,
	PROBLEM_STATE = 2,
	CRYPTO_ACTIVITY = 3,
	MT_DIAG = 5,
	MT_DIAG_CLEARING = 9,	 
};
static __always_inline int stcctm(enum stcctm_ctr_set set, u64 range, u64 *dest)
{
	int cc;
	asm volatile (
		"	STCCTM	%2,%3,%1\n"
		"	ipm	%0\n"
		"	srl	%0,28\n"
		: "=d" (cc)
		: "Q" (*dest), "d" (range), "i" (set)
		: "cc", "memory");
	return cc;
}
static inline int qsi(struct hws_qsi_info_block *info)
{
	int cc = 1;
	asm volatile(
		"0:	qsi	%1\n"
		"1:	lhi	%0,0\n"
		"2:\n"
		EX_TABLE(0b, 2b) EX_TABLE(1b, 2b)
		: "+d" (cc), "+Q" (*info));
	return cc ? -EINVAL : 0;
}
static inline int lsctl(struct hws_lsctl_request_block *req)
{
	int cc;
	cc = 1;
	asm volatile(
		"0:	lsctl	0(%1)\n"
		"1:	ipm	%0\n"
		"	srl	%0,28\n"
		"2:\n"
		EX_TABLE(0b, 2b) EX_TABLE(1b, 2b)
		: "+d" (cc), "+a" (req)
		: "m" (*req)
		: "cc", "memory");
	return cc ? -EINVAL : 0;
}
#endif  
