#ifndef _ASM_S390_AP_H_
#define _ASM_S390_AP_H_
#include <linux/io.h>
#include <asm/asm-extable.h>
typedef unsigned int ap_qid_t;
#define AP_MKQID(_card, _queue) (((_card) & 0xff) << 8 | ((_queue) & 0xff))
#define AP_QID_CARD(_qid) (((_qid) >> 8) & 0xff)
#define AP_QID_QUEUE(_qid) ((_qid) & 0xff)
struct ap_queue_status {
	unsigned int queue_empty	: 1;
	unsigned int replies_waiting	: 1;
	unsigned int queue_full		: 1;
	unsigned int			: 3;
	unsigned int async		: 1;
	unsigned int irq_enabled	: 1;
	unsigned int response_code	: 8;
	unsigned int			: 16;
};
union ap_queue_status_reg {
	unsigned long value;
	struct {
		u32 _pad;
		struct ap_queue_status status;
	};
};
static inline bool ap_instructions_available(void)
{
	unsigned long reg0 = AP_MKQID(0, 0);
	unsigned long reg1 = 0;
	asm volatile(
		"	lgr	0,%[reg0]\n"		 
		"	lghi	1,0\n"			 
		"	lghi	2,0\n"			 
		"	.insn	rre,0xb2af0000,0,0\n"	 
		"0:	la	%[reg1],1\n"		 
		"1:\n"
		EX_TABLE(0b, 1b)
		: [reg1] "+&d" (reg1)
		: [reg0] "d" (reg0)
		: "cc", "0", "1", "2");
	return reg1 != 0;
}
struct ap_tapq_gr2 {
	union {
		unsigned long value;
		struct {
			unsigned int fac    : 32;  
			unsigned int apinfo : 32;  
		};
		struct {
			unsigned int s	   :  1;  
			unsigned int m	   :  1;  
			unsigned int c	   :  1;  
			unsigned int mode  :  3;
			unsigned int n	   :  1;  
			unsigned int	   :  1;
			unsigned int class :  8;
			unsigned int bs	   :  2;  
			unsigned int	   : 14;
			unsigned int at	   :  8;  
			unsigned int nd	   :  8;  
			unsigned int	   :  4;
			unsigned int ml	   :  4;  
			unsigned int	   :  4;
			unsigned int qd	   :  4;  
		};
	};
};
#define AP_BS_Q_USABLE		      0
#define AP_BS_Q_USABLE_NO_SECURE_KEY  1
#define AP_BS_Q_AVAIL_FOR_BINDING     2
#define AP_BS_Q_UNUSABLE	      3
static inline struct ap_queue_status ap_tapq(ap_qid_t qid, struct ap_tapq_gr2 *info)
{
	union ap_queue_status_reg reg1;
	unsigned long reg2;
	asm volatile(
		"	lgr	0,%[qid]\n"		 
		"	lghi	2,0\n"			 
		"	.insn	rre,0xb2af0000,0,0\n"	 
		"	lgr	%[reg1],1\n"		 
		"	lgr	%[reg2],2\n"		 
		: [reg1] "=&d" (reg1.value), [reg2] "=&d" (reg2)
		: [qid] "d" (qid)
		: "cc", "0", "1", "2");
	if (info)
		info->value = reg2;
	return reg1.status;
}
static inline struct ap_queue_status ap_test_queue(ap_qid_t qid, int tbit,
						   struct ap_tapq_gr2 *info)
{
	if (tbit)
		qid |= 1UL << 23;  
	return ap_tapq(qid, info);
}
static inline struct ap_queue_status ap_rapq(ap_qid_t qid, int fbit)
{
	unsigned long reg0 = qid | (1UL << 24);   
	union ap_queue_status_reg reg1;
	if (fbit)
		reg0 |= 1UL << 22;
	asm volatile(
		"	lgr	0,%[reg0]\n"		 
		"	.insn	rre,0xb2af0000,0,0\n"	 
		"	lgr	%[reg1],1\n"		 
		: [reg1] "=&d" (reg1.value)
		: [reg0] "d" (reg0)
		: "cc", "0", "1");
	return reg1.status;
}
static inline struct ap_queue_status ap_zapq(ap_qid_t qid, int fbit)
{
	unsigned long reg0 = qid | (2UL << 24);   
	union ap_queue_status_reg reg1;
	if (fbit)
		reg0 |= 1UL << 22;
	asm volatile(
		"	lgr	0,%[reg0]\n"		 
		"	.insn	rre,0xb2af0000,0,0\n"	 
		"	lgr	%[reg1],1\n"		 
		: [reg1] "=&d" (reg1.value)
		: [reg0] "d" (reg0)
		: "cc", "0", "1");
	return reg1.status;
}
struct ap_config_info {
	unsigned int apsc	 : 1;	 
	unsigned int apxa	 : 1;	 
	unsigned int qact	 : 1;	 
	unsigned int rc8a	 : 1;	 
	unsigned int		 : 4;
	unsigned int apsb	 : 1;	 
	unsigned int		 : 23;
	unsigned char na;		 
	unsigned char nd;		 
	unsigned char _reserved0[10];
	unsigned int apm[8];		 
	unsigned int aqm[8];		 
	unsigned int adm[8];		 
	unsigned char _reserved1[16];
} __aligned(8);
static inline int ap_qci(struct ap_config_info *config)
{
	unsigned long reg0 = 4UL << 24;   
	unsigned long reg1 = -EOPNOTSUPP;
	struct ap_config_info *reg2 = config;
	asm volatile(
		"	lgr	0,%[reg0]\n"		 
		"	lgr	2,%[reg2]\n"		 
		"	.insn	rre,0xb2af0000,0,0\n"	 
		"0:	la	%[reg1],0\n"		 
		"1:\n"
		EX_TABLE(0b, 1b)
		: [reg1] "+&d" (reg1)
		: [reg0] "d" (reg0), [reg2] "d" (reg2)
		: "cc", "memory", "0", "2");
	return reg1;
}
union ap_qirq_ctrl {
	unsigned long value;
	struct {
		unsigned int	   : 8;
		unsigned int zone  : 8;	 
		unsigned int ir	   : 1;	 
		unsigned int	   : 4;
		unsigned int gisc  : 3;	 
		unsigned int	   : 6;
		unsigned int gf	   : 2;	 
		unsigned int	   : 1;
		unsigned int gisa  : 27;	 
		unsigned int	   : 1;
		unsigned int isc   : 3;	 
	};
};
static inline struct ap_queue_status ap_aqic(ap_qid_t qid,
					     union ap_qirq_ctrl qirqctrl,
					     phys_addr_t pa_ind)
{
	unsigned long reg0 = qid | (3UL << 24);   
	union ap_queue_status_reg reg1;
	unsigned long reg2 = pa_ind;
	reg1.value = qirqctrl.value;
	asm volatile(
		"	lgr	0,%[reg0]\n"		 
		"	lgr	1,%[reg1]\n"		 
		"	lgr	2,%[reg2]\n"		 
		"	.insn	rre,0xb2af0000,0,0\n"	 
		"	lgr	%[reg1],1\n"		 
		: [reg1] "+&d" (reg1.value)
		: [reg0] "d" (reg0), [reg2] "d" (reg2)
		: "cc", "memory", "0", "1", "2");
	return reg1.status;
}
union ap_qact_ap_info {
	unsigned long val;
	struct {
		unsigned int	  : 3;
		unsigned int mode : 3;
		unsigned int	  : 26;
		unsigned int cat  : 8;
		unsigned int	  : 8;
		unsigned char ver[2];
	};
};
static inline struct ap_queue_status ap_qact(ap_qid_t qid, int ifbit,
					     union ap_qact_ap_info *apinfo)
{
	unsigned long reg0 = qid | (5UL << 24) | ((ifbit & 0x01) << 22);
	union ap_queue_status_reg reg1;
	unsigned long reg2;
	reg1.value = apinfo->val;
	asm volatile(
		"	lgr	0,%[reg0]\n"		 
		"	lgr	1,%[reg1]\n"		 
		"	.insn	rre,0xb2af0000,0,0\n"	 
		"	lgr	%[reg1],1\n"		 
		"	lgr	%[reg2],2\n"		 
		: [reg1] "+&d" (reg1.value), [reg2] "=&d" (reg2)
		: [reg0] "d" (reg0)
		: "cc", "0", "1", "2");
	apinfo->val = reg2;
	return reg1.status;
}
static inline struct ap_queue_status ap_bapq(ap_qid_t qid)
{
	unsigned long reg0 = qid | (7UL << 24);   
	union ap_queue_status_reg reg1;
	asm volatile(
		"	lgr	0,%[reg0]\n"		 
		"	.insn	rre,0xb2af0000,0,0\n"	 
		"	lgr	%[reg1],1\n"		 
		: [reg1] "=&d" (reg1.value)
		: [reg0] "d" (reg0)
		: "cc", "0", "1");
	return reg1.status;
}
static inline struct ap_queue_status ap_aapq(ap_qid_t qid, unsigned int sec_idx)
{
	unsigned long reg0 = qid | (8UL << 24);   
	unsigned long reg2 = sec_idx;
	union ap_queue_status_reg reg1;
	asm volatile(
		"	lgr	0,%[reg0]\n"		 
		"	lgr	2,%[reg2]\n"		 
		"	.insn	rre,0xb2af0000,0,0\n"	 
		"	lgr	%[reg1],1\n"		 
		: [reg1] "=&d" (reg1.value)
		: [reg0] "d" (reg0), [reg2] "d" (reg2)
		: "cc", "0", "1", "2");
	return reg1.status;
}
static inline struct ap_queue_status ap_nqap(ap_qid_t qid,
					     unsigned long long psmid,
					     void *msg, size_t length)
{
	unsigned long reg0 = qid | 0x40000000UL;   
	union register_pair nqap_r1, nqap_r2;
	union ap_queue_status_reg reg1;
	nqap_r1.even = (unsigned int)(psmid >> 32);
	nqap_r1.odd  = psmid & 0xffffffff;
	nqap_r2.even = (unsigned long)msg;
	nqap_r2.odd  = (unsigned long)length;
	asm volatile (
		"	lgr	0,%[reg0]\n"   
		"0:	.insn	rre,0xb2ad0000,%[nqap_r1],%[nqap_r2]\n"
		"	brc	2,0b\n"        
		"	lgr	%[reg1],1\n"   
		: [reg0] "+&d" (reg0), [reg1] "=&d" (reg1.value),
		  [nqap_r2] "+&d" (nqap_r2.pair)
		: [nqap_r1] "d" (nqap_r1.pair)
		: "cc", "memory", "0", "1");
	return reg1.status;
}
static inline struct ap_queue_status ap_dqap(ap_qid_t qid,
					     unsigned long *psmid,
					     void *msg, size_t msglen,
					     size_t *length,
					     size_t *reslength,
					     unsigned long *resgr0)
{
	unsigned long reg0 = resgr0 && *resgr0 ? *resgr0 : qid | 0x80000000UL;
	union ap_queue_status_reg reg1;
	unsigned long reg2;
	union register_pair rp1, rp2;
	rp1.even = 0UL;
	rp1.odd  = 0UL;
	rp2.even = (unsigned long)msg;
	rp2.odd  = (unsigned long)msglen;
	asm volatile(
		"	lgr	0,%[reg0]\n"    
		"	lghi	2,0\n"	        
		"0:	ltgr	%N[rp2],%N[rp2]\n"  
		"	jz	2f\n"	        
		"1:	.insn	rre,0xb2ae0000,%[rp1],%[rp2]\n"
		"	brc	6,0b\n"         
		"2:	lgr	%[reg0],0\n"    
		"	lgr	%[reg1],1\n"    
		"	lgr	%[reg2],2\n"    
		: [reg0] "+&d" (reg0), [reg1] "=&d" (reg1.value),
		  [reg2] "=&d" (reg2), [rp1] "+&d" (rp1.pair),
		  [rp2] "+&d" (rp2.pair)
		:
		: "cc", "memory", "0", "1", "2");
	if (reslength)
		*reslength = reg2;
	if (reg2 != 0 && rp2.odd == 0) {
		reg1.status.response_code = 0xFF;
		if (resgr0)
			*resgr0 = reg0;
	} else {
		*psmid = (rp1.even << 32) + rp1.odd;
		if (resgr0)
			*resgr0 = 0;
	}
	if (length)
		*length = msglen - rp2.odd;
	return reg1.status;
}
#if IS_ENABLED(CONFIG_ZCRYPT)
void ap_bus_cfg_chg(void);
#else
static inline void ap_bus_cfg_chg(void){}
#endif
#endif  
