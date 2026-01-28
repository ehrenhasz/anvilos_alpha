#ifndef _ASM_POWERPC_REG_H
#define _ASM_POWERPC_REG_H
#ifdef __KERNEL__
#include <linux/stringify.h>
#include <linux/const.h>
#include <asm/cputable.h>
#include <asm/asm-const.h>
#include <asm/feature-fixups.h>
#ifdef CONFIG_BOOKE_OR_40x
#include <asm/reg_booke.h>
#endif
#ifdef CONFIG_FSL_EMB_PERFMON
#include <asm/reg_fsl_emb.h>
#endif
#include <asm/reg_8xx.h>
#define MSR_SF_LG	63               
#define MSR_HV_LG 	60               
#define MSR_TS_T_LG	34		 
#define MSR_TS_S_LG	33		 
#define MSR_TS_LG	33		 
#define MSR_TM_LG	32		 
#define MSR_VEC_LG	25	         
#define MSR_VSX_LG	23		 
#define MSR_S_LG	22		 
#define MSR_POW_LG	18		 
#define MSR_WE_LG	18		 
#define MSR_TGPR_LG	17		 
#define MSR_CE_LG	17		 
#define MSR_ILE_LG	16		 
#define MSR_EE_LG	15		 
#define MSR_PR_LG	14		 
#define MSR_FP_LG	13		 
#define MSR_ME_LG	12		 
#define MSR_FE0_LG	11		 
#define MSR_SE_LG	10		 
#define MSR_BE_LG	9		 
#define MSR_DE_LG	9 		 
#define MSR_FE1_LG	8		 
#define MSR_IP_LG	6		 
#define MSR_IR_LG	5 		 
#define MSR_DR_LG	4 		 
#define MSR_PE_LG	3		 
#define MSR_PX_LG	2		 
#define MSR_PMM_LG	2		 
#define MSR_RI_LG	1		 
#define MSR_LE_LG	0 		 
#ifdef __ASSEMBLY__
#define __MASK(X)	(1<<(X))
#else
#define __MASK(X)	(1UL<<(X))
#endif
#ifdef CONFIG_PPC64
#define MSR_SF		__MASK(MSR_SF_LG)	 
#define MSR_HV 		__MASK(MSR_HV_LG)	 
#define MSR_S		__MASK(MSR_S_LG)	 
#else
#define MSR_SF		0
#define MSR_HV		0
#define MSR_S		0
#endif
#ifndef MSR_SPE
#define MSR_SPE 	0
#endif
#define MSR_VEC		__MASK(MSR_VEC_LG)	 
#define MSR_VSX		__MASK(MSR_VSX_LG)	 
#define MSR_POW		__MASK(MSR_POW_LG)	 
#define MSR_WE		__MASK(MSR_WE_LG)	 
#define MSR_TGPR	__MASK(MSR_TGPR_LG)	 
#define MSR_CE		__MASK(MSR_CE_LG)	 
#define MSR_ILE		__MASK(MSR_ILE_LG)	 
#define MSR_EE		__MASK(MSR_EE_LG)	 
#define MSR_PR		__MASK(MSR_PR_LG)	 
#define MSR_FP		__MASK(MSR_FP_LG)	 
#define MSR_ME		__MASK(MSR_ME_LG)	 
#define MSR_FE0		__MASK(MSR_FE0_LG)	 
#define MSR_SE		__MASK(MSR_SE_LG)	 
#define MSR_BE		__MASK(MSR_BE_LG)	 
#define MSR_DE		__MASK(MSR_DE_LG)	 
#define MSR_FE1		__MASK(MSR_FE1_LG)	 
#define MSR_IP		__MASK(MSR_IP_LG)	 
#define MSR_IR		__MASK(MSR_IR_LG)	 
#define MSR_DR		__MASK(MSR_DR_LG)	 
#define MSR_PE		__MASK(MSR_PE_LG)	 
#define MSR_PX		__MASK(MSR_PX_LG)	 
#ifndef MSR_PMM
#define MSR_PMM		__MASK(MSR_PMM_LG)	 
#endif
#define MSR_RI		__MASK(MSR_RI_LG)	 
#define MSR_LE		__MASK(MSR_LE_LG)	 
#define MSR_TM		__MASK(MSR_TM_LG)	 
#define MSR_TS_N	0			 
#define MSR_TS_S	__MASK(MSR_TS_S_LG)	 
#define MSR_TS_T	__MASK(MSR_TS_T_LG)	 
#define MSR_TS_MASK	(MSR_TS_T | MSR_TS_S)    
#define MSR_TM_RESV(x) (((x) & MSR_TS_MASK) == MSR_TS_MASK)  
#define MSR_TM_TRANSACTIONAL(x)	(((x) & MSR_TS_MASK) == MSR_TS_T)
#define MSR_TM_SUSPENDED(x)	(((x) & MSR_TS_MASK) == MSR_TS_S)
#ifdef CONFIG_PPC_TRANSACTIONAL_MEM
#define MSR_TM_ACTIVE(x) (((x) & MSR_TS_MASK) != 0)  
#else
#define MSR_TM_ACTIVE(x) ((void)(x), 0)
#endif
#if defined(CONFIG_PPC_BOOK3S_64)
#define MSR_64BIT	MSR_SF
#define __MSR		(MSR_ME | MSR_RI | MSR_IR | MSR_DR | MSR_HV)
#ifdef __BIG_ENDIAN__
#define MSR_		__MSR
#define MSR_IDLE	(MSR_ME | MSR_SF | MSR_HV)
#else
#define MSR_		(__MSR | MSR_LE)
#define MSR_IDLE	(MSR_ME | MSR_SF | MSR_HV | MSR_LE)
#endif
#define MSR_KERNEL	(MSR_ | MSR_64BIT)
#define MSR_USER32	(MSR_ | MSR_PR | MSR_EE)
#define MSR_USER64	(MSR_USER32 | MSR_64BIT)
#elif defined(CONFIG_PPC_BOOK3S_32) || defined(CONFIG_PPC_8xx)
#define MSR_KERNEL	(MSR_ME|MSR_RI|MSR_IR|MSR_DR)
#define MSR_USER	(MSR_KERNEL|MSR_PR|MSR_EE)
#endif
#ifndef MSR_64BIT
#define MSR_64BIT	0
#endif
#define CR0_SHIFT	28
#define CR0_MASK	0xF
#define CR0_TBEGIN_FAILURE	(0x2 << 28)  
#define PSSCR_RL_MASK		0x0000000F  
#define PSSCR_MTL_MASK		0x000000F0  
#define PSSCR_TR_MASK		0x00000300  
#define PSSCR_PSLL_MASK		0x000F0000  
#define PSSCR_EC		0x00100000  
#define PSSCR_ESL		0x00200000  
#define PSSCR_SD		0x00400000  
#define PSSCR_PLS	0xf000000000000000  
#define PSSCR_PLS_SHIFT	60
#define PSSCR_GUEST_VIS	0xf0000000000003ffUL  
#define PSSCR_FAKE_SUSPEND	0x00000400  
#define PSSCR_FAKE_SUSPEND_LG	10	    
#define FPSCR_FX	0x80000000	 
#define FPSCR_FEX	0x40000000	 
#define FPSCR_VX	0x20000000	 
#define FPSCR_OX	0x10000000	 
#define FPSCR_UX	0x08000000	 
#define FPSCR_ZX	0x04000000	 
#define FPSCR_XX	0x02000000	 
#define FPSCR_VXSNAN	0x01000000	 
#define FPSCR_VXISI	0x00800000	 
#define FPSCR_VXIDI	0x00400000	 
#define FPSCR_VXZDZ	0x00200000	 
#define FPSCR_VXIMZ	0x00100000	 
#define FPSCR_VXVC	0x00080000	 
#define FPSCR_FR	0x00040000	 
#define FPSCR_FI	0x00020000	 
#define FPSCR_FPRF	0x0001f000	 
#define FPSCR_FPCC	0x0000f000	 
#define FPSCR_VXSOFT	0x00000400	 
#define FPSCR_VXSQRT	0x00000200	 
#define FPSCR_VXCVI	0x00000100	 
#define FPSCR_VE	0x00000080	 
#define FPSCR_OE	0x00000040	 
#define FPSCR_UE	0x00000020	 
#define FPSCR_ZE	0x00000010	 
#define FPSCR_XE	0x00000008	 
#define FPSCR_NI	0x00000004	 
#define FPSCR_RN	0x00000003	 
#define SPEFSCR_SOVH	0x80000000	 
#define SPEFSCR_OVH	0x40000000	 
#define SPEFSCR_FGH	0x20000000	 
#define SPEFSCR_FXH	0x10000000	 
#define SPEFSCR_FINVH	0x08000000	 
#define SPEFSCR_FDBZH	0x04000000	 
#define SPEFSCR_FUNFH	0x02000000	 
#define SPEFSCR_FOVFH	0x01000000	 
#define SPEFSCR_FINXS	0x00200000	 
#define SPEFSCR_FINVS	0x00100000	 
#define SPEFSCR_FDBZS	0x00080000	 
#define SPEFSCR_FUNFS	0x00040000	 
#define SPEFSCR_FOVFS	0x00020000	 
#define SPEFSCR_MODE	0x00010000	 
#define SPEFSCR_SOV	0x00008000	 
#define SPEFSCR_OV	0x00004000	 
#define SPEFSCR_FG	0x00002000	 
#define SPEFSCR_FX	0x00001000	 
#define SPEFSCR_FINV	0x00000800	 
#define SPEFSCR_FDBZ	0x00000400	 
#define SPEFSCR_FUNF	0x00000200	 
#define SPEFSCR_FOVF	0x00000100	 
#define SPEFSCR_FINXE	0x00000040	 
#define SPEFSCR_FINVE	0x00000020	 
#define SPEFSCR_FDBZE	0x00000010	 
#define SPEFSCR_FUNFE	0x00000008	 
#define SPEFSCR_FOVFE	0x00000004	 
#define SPEFSCR_FRMC 	0x00000003	 
#ifdef CONFIG_40x
#define SPRN_PID	0x3B1	 
#else
#define SPRN_PID	0x030	 
#ifdef CONFIG_BOOKE
#define SPRN_PID0	SPRN_PID 
#endif
#endif
#define SPRN_CTR	0x009	 
#define SPRN_DSCR	0x11
#define SPRN_CFAR	0x1c	 
#define SPRN_AMR	0x1d	 
#define SPRN_UAMOR	0x9d	 
#define SPRN_AMOR	0x15d	 
#define SPRN_ACOP	0x1F	 
#define SPRN_TFIAR	0x81	 
#define SPRN_TEXASR	0x82	 
#define SPRN_TEXASRU	0x83	 
#define TEXASR_FC_LG	(63 - 7)	 
#define TEXASR_AB_LG	(63 - 31)	 
#define TEXASR_SU_LG	(63 - 32)	 
#define TEXASR_HV_LG	(63 - 34)	 
#define TEXASR_PR_LG	(63 - 35)	 
#define TEXASR_FS_LG	(63 - 36)	 
#define TEXASR_EX_LG	(63 - 37)	 
#define TEXASR_ROT_LG	(63 - 38)	 
#define   TEXASR_ABORT	__MASK(TEXASR_AB_LG)  
#define   TEXASR_SUSP	__MASK(TEXASR_SU_LG)  
#define   TEXASR_HV	__MASK(TEXASR_HV_LG)  
#define   TEXASR_PR	__MASK(TEXASR_PR_LG)  
#define   TEXASR_FS	__MASK(TEXASR_FS_LG)  
#define   TEXASR_EXACT	__MASK(TEXASR_EX_LG)  
#define   TEXASR_ROT	__MASK(TEXASR_ROT_LG)
#define   TEXASR_FC	(ASM_CONST(0xFF) << TEXASR_FC_LG)
#define SPRN_TFHAR	0x80	 
#define SPRN_TIDR	144	 
#define SPRN_CTRLF	0x088
#define SPRN_CTRLT	0x098
#define   CTRL_CT	0xc0000000	 
#define   CTRL_CT0	0x80000000	 
#define   CTRL_CT1	0x40000000	 
#define   CTRL_TE	0x00c00000	 
#define   CTRL_RUNLATCH	0x1
#define SPRN_DAWR0	0xB4
#define SPRN_DAWR1	0xB5
#define SPRN_RPR	0xBA	 
#define SPRN_CIABR	0xBB
#define   CIABR_PRIV		0x3
#define   CIABR_PRIV_USER	1
#define   CIABR_PRIV_SUPER	2
#define   CIABR_PRIV_HYPER	3
#define SPRN_DAWRX0	0xBC
#define SPRN_DAWRX1	0xBD
#define   DAWRX_USER	__MASK(0)
#define   DAWRX_KERNEL	__MASK(1)
#define   DAWRX_HYP	__MASK(2)
#define   DAWRX_WTI	__MASK(3)
#define   DAWRX_WT	__MASK(4)
#define   DAWRX_DR	__MASK(5)
#define   DAWRX_DW	__MASK(6)
#define SPRN_DABR	0x3F5	 
#define SPRN_DABR2	0x13D	 
#define SPRN_DABRX	0x3F7	 
#define   DABRX_USER	__MASK(0)
#define   DABRX_KERNEL	__MASK(1)
#define   DABRX_HYP	__MASK(2)
#define   DABRX_BTI	__MASK(3)
#define   DABRX_ALL     (DABRX_BTI | DABRX_HYP | DABRX_KERNEL | DABRX_USER)
#define SPRN_DAR	0x013	 
#define SPRN_DBCR	0x136	 
#define SPRN_DSISR	0x012	 
#define   DSISR_BAD_DIRECT_ST	0x80000000  
#define   DSISR_NOHPTE		0x40000000  
#define   DSISR_ATTR_CONFLICT	0x20000000  
#define   DSISR_NOEXEC_OR_G	0x10000000  
#define   DSISR_PROTFAULT	0x08000000  
#define   DSISR_BADACCESS	0x04000000  
#define   DSISR_ISSTORE		0x02000000  
#define   DSISR_DABRMATCH	0x00400000  
#define   DSISR_NOSEGMENT	0x00200000  
#define   DSISR_KEYFAULT	0x00200000  
#define   DSISR_BAD_EXT_CTRL	0x00100000  
#define   DSISR_UNSUPP_MMU	0x00080000  
#define   DSISR_SET_RC		0x00040000  
#define   DSISR_PRTABLE_FAULT   0x00020000  
#define   DSISR_ICSWX_NO_CT     0x00004000  
#define   DSISR_BAD_COPYPASTE   0x00000008  
#define   DSISR_BAD_AMO		0x00000004  
#define   DSISR_BAD_CI_LDST	0x00000002  
#define   DSISR_BAD_FAULT_32S	(DSISR_BAD_DIRECT_ST	| \
				 DSISR_BADACCESS	| \
				 DSISR_BAD_EXT_CTRL)
#define	  DSISR_BAD_FAULT_64S	(DSISR_BAD_FAULT_32S	| \
				 DSISR_ATTR_CONFLICT	| \
				 DSISR_UNSUPP_MMU	| \
				 DSISR_PRTABLE_FAULT	| \
				 DSISR_ICSWX_NO_CT	| \
				 DSISR_BAD_COPYPASTE	| \
				 DSISR_BAD_AMO		| \
				 DSISR_BAD_CI_LDST)
#define   DSISR_SRR1_MATCH_32S	(DSISR_NOHPTE		| \
				 DSISR_NOEXEC_OR_G	| \
				 DSISR_PROTFAULT)
#define   DSISR_SRR1_MATCH_64S	(DSISR_SRR1_MATCH_32S	| \
				 DSISR_KEYFAULT		| \
				 DSISR_UNSUPP_MMU	| \
				 DSISR_SET_RC		| \
				 DSISR_PRTABLE_FAULT)
#define SPRN_TBRL	0x10C	 
#define SPRN_TBRU	0x10D	 
#define SPRN_CIR	0x11B	 
#define SPRN_TBWL	0x11C	 
#define SPRN_TBWU	0x11D	 
#define SPRN_TBU40	0x11E	 
#define SPRN_SPURR	0x134	 
#define SPRN_HSPRG0	0x130	 
#define SPRN_HSPRG1	0x131	 
#define SPRN_HDSISR     0x132
#define SPRN_HDAR       0x133
#define SPRN_HDEC	0x136	 
#define SPRN_HIOR	0x137	 
#define SPRN_RMOR	0x138	 
#define SPRN_HRMOR	0x139	 
#define SPRN_HDEXCR_RO	0x1C7	 
#define SPRN_HASHKEYR	0x1D4	 
#define SPRN_HDEXCR	0x1D7	 
#define SPRN_DEXCR_RO	0x32C	 
#define SPRN_ASDR	0x330	 
#define SPRN_DEXCR	0x33C	 
#define   DEXCR_PR_SBHE	  0x80000000UL  
#define   DEXCR_PR_IBRTPD 0x10000000UL  
#define   DEXCR_PR_SRAPD  0x08000000UL  
#define   DEXCR_PR_NPHIE  0x04000000UL  
#define   DEXCR_INIT	DEXCR_PR_NPHIE	 
#define SPRN_IC		0x350	 
#define SPRN_VTB	0x351	 
#define SPRN_LDBAR	0x352	 
#define SPRN_PMICR	0x354    
#define SPRN_PMSR	0x355    
#define SPRN_PMMAR	0x356	 
#define SPRN_PSSCR	0x357	 
#define SPRN_PSSCR_PR	0x337	 
#define SPRN_TRIG2	0x372
#define SPRN_PMCR	0x374	 
#define SPRN_RWMR	0x375	 
#define FSCR_PREFIX_LG	13	 
#define FSCR_SCV_LG	12	 
#define FSCR_MSGP_LG	10	 
#define FSCR_TAR_LG	8	 
#define FSCR_EBB_LG	7	 
#define FSCR_TM_LG	5	 
#define FSCR_BHRB_LG	4	 
#define FSCR_PM_LG	3	 
#define FSCR_DSCR_LG	2	 
#define FSCR_VECVSX_LG	1	 
#define FSCR_FP_LG	0	 
#define SPRN_FSCR	0x099	 
#define   FSCR_PREFIX	__MASK(FSCR_PREFIX_LG)
#define   FSCR_SCV	__MASK(FSCR_SCV_LG)
#define   FSCR_TAR	__MASK(FSCR_TAR_LG)
#define   FSCR_EBB	__MASK(FSCR_EBB_LG)
#define   FSCR_DSCR	__MASK(FSCR_DSCR_LG)
#define   FSCR_INTR_CAUSE (ASM_CONST(0xFF) << 56)	 
#define SPRN_HFSCR	0xbe	 
#define   HFSCR_PREFIX	__MASK(FSCR_PREFIX_LG)
#define   HFSCR_MSGP	__MASK(FSCR_MSGP_LG)
#define   HFSCR_TAR	__MASK(FSCR_TAR_LG)
#define   HFSCR_EBB	__MASK(FSCR_EBB_LG)
#define   HFSCR_TM	__MASK(FSCR_TM_LG)
#define   HFSCR_PM	__MASK(FSCR_PM_LG)
#define   HFSCR_BHRB	__MASK(FSCR_BHRB_LG)
#define   HFSCR_DSCR	__MASK(FSCR_DSCR_LG)
#define   HFSCR_VECVSX	__MASK(FSCR_VECVSX_LG)
#define   HFSCR_FP	__MASK(FSCR_FP_LG)
#define   HFSCR_INTR_CAUSE FSCR_INTR_CAUSE
#define SPRN_TAR	0x32f	 
#define SPRN_LPCR	0x13E	 
#define   LPCR_VPM0		ASM_CONST(0x8000000000000000)
#define   LPCR_VPM1		ASM_CONST(0x4000000000000000)
#define   LPCR_ISL		ASM_CONST(0x2000000000000000)
#define   LPCR_VC_SH		61
#define   LPCR_DPFD_SH		52
#define   LPCR_DPFD		(ASM_CONST(7) << LPCR_DPFD_SH)
#define   LPCR_VRMASD_SH	47
#define   LPCR_VRMASD		(ASM_CONST(0x1f) << LPCR_VRMASD_SH)
#define   LPCR_VRMA_L		ASM_CONST(0x0008000000000000)
#define   LPCR_VRMA_LP0		ASM_CONST(0x0001000000000000)
#define   LPCR_VRMA_LP1		ASM_CONST(0x0000800000000000)
#define   LPCR_RMLS		0x1C000000	 
#define   LPCR_RMLS_SH		26
#define   LPCR_HAIL		ASM_CONST(0x0000000004000000)    
#define   LPCR_ILE		ASM_CONST(0x0000000002000000)    
#define   LPCR_AIL		ASM_CONST(0x0000000001800000)	 
#define   LPCR_AIL_0		ASM_CONST(0x0000000000000000)	 
#define   LPCR_AIL_3		ASM_CONST(0x0000000001800000)    
#define   LPCR_ONL		ASM_CONST(0x0000000000040000)	 
#define   LPCR_LD		ASM_CONST(0x0000000000020000)	 
#define   LPCR_PECE		ASM_CONST(0x000000000001f000)	 
#define     LPCR_PECEDP	ASM_CONST(0x0000000000010000)	 
#define     LPCR_PECEDH	ASM_CONST(0x0000000000008000)	 
#define     LPCR_PECE0		ASM_CONST(0x0000000000004000)	 
#define     LPCR_PECE1		ASM_CONST(0x0000000000002000)	 
#define     LPCR_PECE2		ASM_CONST(0x0000000000001000)	 
#define     LPCR_PECE_HVEE	ASM_CONST(0x0000400000000000)	 
#define   LPCR_MER		ASM_CONST(0x0000000000000800)	 
#define   LPCR_MER_SH		11
#define	  LPCR_GTSE		ASM_CONST(0x0000000000000400)  	 
#define   LPCR_TC		ASM_CONST(0x0000000000000200)	 
#define   LPCR_HEIC		ASM_CONST(0x0000000000000010)    
#define   LPCR_LPES		0x0000000c
#define   LPCR_LPES0		ASM_CONST(0x0000000000000008)       
#define   LPCR_LPES1		ASM_CONST(0x0000000000000004)       
#define   LPCR_LPES_SH		2
#define   LPCR_RMI		ASM_CONST(0x0000000000000002)       
#define   LPCR_HVICE		ASM_CONST(0x0000000000000002)       
#define   LPCR_HDICE		ASM_CONST(0x0000000000000001)       
#define   LPCR_UPRT		ASM_CONST(0x0000000000400000)       
#define   LPCR_HR		ASM_CONST(0x0000000000100000)
#ifndef SPRN_LPID
#define SPRN_LPID	0x13F	 
#endif
#define	SPRN_HMER	0x150	 
#define   HMER_DEBUG_TRIG	(1ul << (63 - 17))  
#define	SPRN_HMEER	0x151	 
#define SPRN_PCR	0x152	 
#define   PCR_VEC_DIS	(__MASK(63-0))	 
#define   PCR_VSX_DIS	(__MASK(63-1))	 
#define   PCR_TM_DIS	(__MASK(63-2))	 
#define   PCR_MMA_DIS	(__MASK(63-3))  
#define   PCR_HIGH_BITS	(PCR_MMA_DIS | PCR_VEC_DIS | PCR_VSX_DIS | PCR_TM_DIS)
#define   PCR_ARCH_300	0x10		 
#define   PCR_ARCH_207	0x8		 
#define   PCR_ARCH_206	0x4		 
#define   PCR_ARCH_205	0x2		 
#define   PCR_LOW_BITS	(PCR_ARCH_207 | PCR_ARCH_206 | PCR_ARCH_205 | PCR_ARCH_300)
#define   PCR_MASK	~(PCR_HIGH_BITS | PCR_LOW_BITS)	 
#define	SPRN_HEIR	0x153	 
#define SPRN_TLBINDEXR	0x154	 
#define SPRN_TLBVPNR	0x155	 
#define SPRN_TLBRPNR	0x156	 
#define SPRN_TLBLPIDR	0x157	 
#define SPRN_DBAT0L	0x219	 
#define SPRN_DBAT0U	0x218	 
#define SPRN_DBAT1L	0x21B	 
#define SPRN_DBAT1U	0x21A	 
#define SPRN_DBAT2L	0x21D	 
#define SPRN_DBAT2U	0x21C	 
#define SPRN_DBAT3L	0x21F	 
#define SPRN_DBAT3U	0x21E	 
#define SPRN_DBAT4L	0x239	 
#define SPRN_DBAT4U	0x238	 
#define SPRN_DBAT5L	0x23B	 
#define SPRN_DBAT5U	0x23A	 
#define SPRN_DBAT6L	0x23D	 
#define SPRN_DBAT6U	0x23C	 
#define SPRN_DBAT7L	0x23F	 
#define SPRN_DBAT7U	0x23E	 
#define SPRN_PPR	0x380	 
#define SPRN_TSCR	0x399	 
#define SPRN_DEC	0x016		 
#define SPRN_PIT	0x3DB		 
#define SPRN_DER	0x095		 
#define DER_RSTE	0x40000000	 
#define DER_CHSTPE	0x20000000	 
#define DER_MCIE	0x10000000	 
#define DER_EXTIE	0x02000000	 
#define DER_ALIE	0x01000000	 
#define DER_PRIE	0x00800000	 
#define DER_FPUVIE	0x00400000	 
#define DER_DECIE	0x00200000	 
#define DER_SYSIE	0x00040000	 
#define DER_TRE		0x00020000	 
#define DER_SEIE	0x00004000	 
#define DER_ITLBMSE	0x00002000	 
#define DER_ITLBERE	0x00001000	 
#define DER_DTLBMSE	0x00000800	 
#define DER_DTLBERE	0x00000400	 
#define DER_LBRKE	0x00000008	 
#define DER_IBRKE	0x00000004	 
#define DER_EBRKE	0x00000002	 
#define DER_DPIE	0x00000001	 
#define SPRN_DMISS	0x3D0		 
#define SPRN_DHDES	0x0B1		 
#define SPRN_DPDES	0x0B0		 
#define SPRN_EAR	0x11A		 
#define SPRN_HASH1	0x3D2		 
#define SPRN_HASH2	0x3D3		 
#define SPRN_HID0	0x3F0		 
#define HID0_HDICE_SH	(63 - 23)	 
#define HID0_EMCP	(1<<31)		 
#define HID0_EBA	(1<<29)		 
#define HID0_EBD	(1<<28)		 
#define HID0_SBCLK	(1<<27)
#define HID0_EICE	(1<<26)
#define HID0_TBEN	(1<<26)		 
#define HID0_ECLK	(1<<25)
#define HID0_PAR	(1<<24)
#define HID0_STEN	(1<<24)		 
#define HID0_HIGH_BAT	(1<<23)		 
#define HID0_DOZE	(1<<23)
#define HID0_NAP	(1<<22)
#define HID0_SLEEP	(1<<21)
#define HID0_DPM	(1<<20)
#define HID0_BHTCLR	(1<<18)		 
#define HID0_XAEN	(1<<17)		 
#define HID0_NHR	(1<<16)		 
#define HID0_ICE	(1<<15)		 
#define HID0_DCE	(1<<14)		 
#define HID0_ILOCK	(1<<13)		 
#define HID0_DLOCK	(1<<12)		 
#define HID0_ICFI	(1<<11)		 
#define HID0_DCI	(1<<10)		 
#define HID0_SPD	(1<<9)		 
#define HID0_DAPUEN	(1<<8)		 
#define HID0_SGE	(1<<7)		 
#define HID0_SIED	(1<<7)		 
#define HID0_DCFA	(1<<6)		 
#define HID0_LRSTK	(1<<4)		 
#define HID0_BTIC	(1<<5)		 
#define HID0_ABE	(1<<3)		 
#define HID0_FOLD	(1<<3)		 
#define HID0_BHTE	(1<<2)		 
#define HID0_BTCD	(1<<1)		 
#define HID0_NOPDST	(1<<1)		 
#define HID0_NOPTI	(1<<0)		 
#define HID0_POWER8_4LPARMODE	__MASK(61)
#define HID0_POWER8_2LPARMODE	__MASK(57)
#define HID0_POWER8_1TO2LPAR	__MASK(52)
#define HID0_POWER8_1TO4LPAR	__MASK(51)
#define HID0_POWER8_DYNLPARDIS	__MASK(48)
#define HID0_POWER9_RADIX	__MASK(63 - 8)
#define SPRN_HID1	0x3F1		 
#ifdef CONFIG_PPC_BOOK3S_32
#define HID1_EMCP	(1<<31)		 
#define HID1_DFS	(1<<22)		 
#define HID1_PC0	(1<<16)		 
#define HID1_PC1	(1<<15)		 
#define HID1_PC2	(1<<14)		 
#define HID1_PC3	(1<<13)		 
#define HID1_SYNCBE	(1<<11)		 
#define HID1_ABE	(1<<10)		 
#define HID1_PS		(1<<16)		 
#endif
#define SPRN_HID2	0x3F8		 
#define SPRN_HID2_GEKKO	0x398		 
#define SPRN_IABR	0x3F2	 
#define SPRN_IABR2	0x3FA		 
#define SPRN_IBCR	0x135		 
#define SPRN_IAMR	0x03D		 
#define SPRN_HID4	0x3F4		 
#define  HID4_LPES0	 (1ul << (63-0))  
#define	 HID4_RMLS2_SH	 (63 - 2)	 
#define	 HID4_LPID5_SH	 (63 - 6)	 
#define	 HID4_RMOR_SH	 (63 - 22)	 
#define  HID4_RMOR	 (0xFFFFul << HID4_RMOR_SH)
#define  HID4_LPES1	 (1 << (63-57))	 
#define  HID4_RMLS0_SH	 (63 - 58)	 
#define	 HID4_LPID1_SH	 0		 
#define SPRN_HID4_GEKKO	0x3F3		 
#define SPRN_HID5	0x3F6		 
#define SPRN_HID6	0x3F9	 
#define   HID6_LB	(0x0F<<12)  
#define   HID6_DLP	(1<<20)	 
#define SPRN_TSC_CELL	0x399	 
#define   TSC_CELL_DEC_ENABLE_0	0x400000  
#define   TSC_CELL_DEC_ENABLE_1	0x200000  
#define   TSC_CELL_EE_ENABLE	0x100000  
#define   TSC_CELL_EE_BOOST	0x080000  
#define SPRN_TSC 	0x3FD	 
#define SPRN_TST 	0x3FC	 
#if !defined(SPRN_IAC1) && !defined(SPRN_IAC2)
#define SPRN_IAC1	0x3F4		 
#define SPRN_IAC2	0x3F5		 
#endif
#define SPRN_IBAT0L	0x211		 
#define SPRN_IBAT0U	0x210		 
#define SPRN_IBAT1L	0x213		 
#define SPRN_IBAT1U	0x212		 
#define SPRN_IBAT2L	0x215		 
#define SPRN_IBAT2U	0x214		 
#define SPRN_IBAT3L	0x217		 
#define SPRN_IBAT3U	0x216		 
#define SPRN_IBAT4L	0x231		 
#define SPRN_IBAT4U	0x230		 
#define SPRN_IBAT5L	0x233		 
#define SPRN_IBAT5U	0x232		 
#define SPRN_IBAT6L	0x235		 
#define SPRN_IBAT6U	0x234		 
#define SPRN_IBAT7L	0x237		 
#define SPRN_IBAT7U	0x236		 
#define SPRN_ICMP	0x3D5		 
#define SPRN_ICTC	0x3FB	 
#ifndef SPRN_ICTRL
#define SPRN_ICTRL	0x3F3	 
#endif
#define ICTRL_EICE	0x08000000	 
#define ICTRL_EDC	0x04000000	 
#define ICTRL_EICP	0x00000100	 
#define SPRN_IMISS	0x3D4		 
#define SPRN_IMMR	0x27E		 
#define SPRN_L2CR	0x3F9		 
#define SPRN_L2CR2	0x3f8
#define L2CR_L2E		0x80000000	 
#define L2CR_L2PE		0x40000000	 
#define L2CR_L2SIZ_MASK		0x30000000	 
#define L2CR_L2SIZ_256KB	0x10000000	 
#define L2CR_L2SIZ_512KB	0x20000000	 
#define L2CR_L2SIZ_1MB		0x30000000	 
#define L2CR_L2CLK_MASK		0x0e000000	 
#define L2CR_L2CLK_DISABLED	0x00000000	 
#define L2CR_L2CLK_DIV1		0x02000000	 
#define L2CR_L2CLK_DIV1_5	0x04000000	 
#define L2CR_L2CLK_DIV2		0x08000000	 
#define L2CR_L2CLK_DIV2_5	0x0a000000	 
#define L2CR_L2CLK_DIV3		0x0c000000	 
#define L2CR_L2RAM_MASK		0x01800000	 
#define L2CR_L2RAM_FLOW		0x00000000	 
#define L2CR_L2RAM_PIPE		0x01000000	 
#define L2CR_L2RAM_PIPE_LW	0x01800000	 
#define L2CR_L2DO		0x00400000	 
#define L2CR_L2I		0x00200000	 
#define L2CR_L2CTL		0x00100000	 
#define L2CR_L2WT		0x00080000	 
#define L2CR_L2TS		0x00040000	 
#define L2CR_L2OH_MASK		0x00030000	 
#define L2CR_L2OH_0_5		0x00000000	 
#define L2CR_L2OH_1_0		0x00010000	 
#define L2CR_L2SL		0x00008000	 
#define L2CR_L2DF		0x00004000	 
#define L2CR_L2BYP		0x00002000	 
#define L2CR_L2IP		0x00000001	 
#define L2CR_L2IO_745x		0x00100000	 
#define L2CR_L2DO_745x		0x00010000	 
#define L2CR_L2REP_745x		0x00001000	 
#define L2CR_L2HWF_745x		0x00000800	 
#define SPRN_L3CR		0x3FA	 
#define L3CR_L3E		0x80000000	 
#define L3CR_L3PE		0x40000000	 
#define L3CR_L3APE		0x20000000	 
#define L3CR_L3SIZ		0x10000000	 
#define L3CR_L3CLKEN		0x08000000	 
#define L3CR_L3RES		0x04000000	 
#define L3CR_L3CLKDIV		0x03800000	 
#define L3CR_L3IO		0x00400000	 
#define L3CR_L3SPO		0x00040000	 
#define L3CR_L3CKSP		0x00030000	 
#define L3CR_L3PSP		0x0000e000	 
#define L3CR_L3REP		0x00001000	 
#define L3CR_L3HWF		0x00000800	 
#define L3CR_L3I		0x00000400	 
#define L3CR_L3RT		0x00000300	 
#define L3CR_L3NIRCA		0x00000080	 
#define L3CR_L3DO		0x00000040	 
#define L3CR_PMEN		0x00000004	 
#define L3CR_PMSIZ		0x00000001	 
#define SPRN_MSSCR0	0x3f6	 
#define SPRN_MSSSR0	0x3f7	 
#define SPRN_LDSTCR	0x3f8	 
#define SPRN_LDSTDB	0x3f4	 
#define SPRN_LR		0x008	 
#ifndef SPRN_PIR
#define SPRN_PIR	0x3FF	 
#endif
#define SPRN_TIR	0x1BE	 
#define SPRN_PTCR	0x1D0	 
#define SPRN_PSPB	0x09F	 
#define SPRN_PTEHI	0x3D5	 
#define SPRN_PTELO	0x3D6	 
#define SPRN_PURR	0x135	 
#define SPRN_PVR	0x11F	 
#define SPRN_RPA	0x3D6	 
#define SPRN_SDA	0x3BF	 
#define SPRN_SDR1	0x019	 
#define SPRN_ASR	0x118    
#define SPRN_SIA	0x3BB	 
#define SPRN_SPRG0	0x110	 
#define SPRN_SPRG1	0x111	 
#define SPRN_SPRG2	0x112	 
#define SPRN_SPRG3	0x113	 
#define SPRN_USPRG3	0x103	 
#define SPRN_SPRG4	0x114	 
#define SPRN_USPRG4	0x104	 
#define SPRN_SPRG5	0x115	 
#define SPRN_USPRG5	0x105	 
#define SPRN_SPRG6	0x116	 
#define SPRN_USPRG6	0x106	 
#define SPRN_SPRG7	0x117	 
#define SPRN_USPRG7	0x107	 
#define SPRN_SRR0	0x01A	 
#define SPRN_SRR1	0x01B	 
#ifdef CONFIG_PPC_BOOK3S
#define   SRR1_MSR_BITS		(~0x783f0000UL)
#endif
#define   SRR1_ISI_NOPT		0x40000000  
#define   SRR1_ISI_N_G_OR_CIP	0x10000000  
#define   SRR1_ISI_PROT		0x08000000  
#define   SRR1_WAKEMASK		0x00380000  
#define   SRR1_WAKEMASK_P8	0x003c0000  
#define   SRR1_WAKEMCE_RESVD	0x003c0000  
#define   SRR1_WAKESYSERR	0x00300000  
#define   SRR1_WAKEEE		0x00200000  
#define   SRR1_WAKEHVI		0x00240000  
#define   SRR1_WAKEMT		0x00280000  
#define	  SRR1_WAKEHMI		0x00280000  
#define   SRR1_WAKEDEC		0x00180000  
#define   SRR1_WAKEDBELL	0x00140000  
#define   SRR1_WAKETHERM	0x00100000  
#define	  SRR1_WAKERESET	0x00100000  
#define   SRR1_WAKEHDBELL	0x000c0000  
#define	  SRR1_WAKESTATE	0x00030000  
#define	  SRR1_WS_HVLOSS	0x00030000  
#define	  SRR1_WS_GPRLOSS	0x00020000  
#define	  SRR1_WS_NOLOSS	0x00010000  
#define   SRR1_PROGTM		0x00200000  
#define   SRR1_PROGFPE		0x00100000  
#define   SRR1_PROGILL		0x00080000  
#define   SRR1_PROGPRIV		0x00040000  
#define   SRR1_PROGTRAP		0x00020000  
#define   SRR1_PROGADDR		0x00010000  
#define   SRR1_MCE_MCP		0x00080000  
#define   SRR1_BOUNDARY		0x10000000  
#define   SRR1_PREFIXED		0x20000000  
#define SPRN_HSRR0	0x13A	 
#define SPRN_HSRR1	0x13B	 
#define   HSRR1_DENORM		0x00100000  
#define   HSRR1_HISI_WRITE	0x00010000  
#define SPRN_TBCTL	0x35f	 
#define   TBCTL_FREEZE		0x0000000000000000ull  
#define   TBCTL_RESTART		0x0000000100000000ull  
#define   TBCTL_UPDATE_UPPER	0x0000000200000000ull  
#define   TBCTL_UPDATE_LOWER	0x0000000300000000ull  
#ifndef SPRN_SVR
#define SPRN_SVR	0x11E	 
#endif
#define SPRN_THRM1	0x3FC		 
#define THRM1_TIN	(1 << 31)
#define THRM1_TIV	(1 << 30)
#define THRM1_THRES(x)	((x&0x7f)<<23)
#define THRM3_SITV(x)	((x & 0x1fff) << 1)
#define THRM1_TID	(1<<2)
#define THRM1_TIE	(1<<1)
#define THRM1_V		(1<<0)
#define SPRN_THRM2	0x3FD		 
#define SPRN_THRM3	0x3FE		 
#define THRM3_E		(1<<0)
#define SPRN_TLBMISS	0x3D4		 
#define SPRN_UMMCR0	0x3A8	 
#define SPRN_UMMCR1	0x3AC	 
#define SPRN_UPMC1	0x3A9	 
#define SPRN_UPMC2	0x3AA	 
#define SPRN_UPMC3	0x3AD	 
#define SPRN_UPMC4	0x3AE	 
#define SPRN_USIA	0x3AB	 
#define SPRN_VRSAVE	0x100	 
#define SPRN_XER	0x001	 
#define SPRN_MMCR0_GEKKO 0x3B8  
#define SPRN_MMCR1_GEKKO 0x3BC  
#define SPRN_PMC1_GEKKO  0x3B9  
#define SPRN_PMC2_GEKKO  0x3BA  
#define SPRN_PMC3_GEKKO  0x3BD  
#define SPRN_PMC4_GEKKO  0x3BE  
#define SPRN_WPAR_GEKKO  0x399  
#define SPRN_SCOMC	0x114	 
#define SPRN_SCOMD	0x115	 
#ifdef CONFIG_PPC64
#define SPRN_MMCR0	795
#define   MMCR0_FC	0x80000000UL  
#define   MMCR0_FCS	0x40000000UL  
#define   MMCR0_KERNEL_DISABLE MMCR0_FCS
#define   MMCR0_FCP	0x20000000UL  
#define   MMCR0_PROBLEM_DISABLE MMCR0_FCP
#define   MMCR0_FCM1	0x10000000UL  
#define   MMCR0_FCM0	0x08000000UL  
#define   MMCR0_PMXE	ASM_CONST(0x04000000)  
#define   MMCR0_FCECE	ASM_CONST(0x02000000)  
#define   MMCR0_TBEE	0x00400000UL  
#define   MMCR0_BHRBA	0x00200000UL  
#define   MMCR0_EBE	0x00100000UL  
#define   MMCR0_PMCC	0x000c0000UL  
#define   MMCR0_PMCCEXT	ASM_CONST(0x00000200)  
#define   MMCR0_PMCC_U6	0x00080000UL  
#define   MMCR0_PMC1CE	0x00008000UL  
#define   MMCR0_PMCjCE	ASM_CONST(0x00004000)  
#define   MMCR0_TRIGGER	0x00002000UL  
#define   MMCR0_PMAO_SYNC ASM_CONST(0x00000800)  
#define   MMCR0_C56RUN	ASM_CONST(0x00000100)  
#define   MMCR0_PMAO	ASM_CONST(0x00000080)
#define   MMCR0_SHRFC	0x00000040UL  
#define   MMCR0_FC56	0x00000010UL  
#define   MMCR0_FCTI	0x00000008UL  
#define   MMCR0_FCTA	0x00000004UL  
#define   MMCR0_FCWAIT	0x00000002UL  
#define   MMCR0_FCHV	0x00000001UL  
#define SPRN_MMCR1	798
#define SPRN_MMCR2	785
#define SPRN_MMCR3	754
#define SPRN_UMMCR2	769
#define SPRN_UMMCR3	738
#define SPRN_MMCRA	0x312
#define   MMCRA_SDSYNC	0x80000000UL  
#define   MMCRA_SDAR_DCACHE_MISS 0x40000000UL
#define   MMCRA_SDAR_ERAT_MISS   0x20000000UL
#define   MMCRA_SIHV	0x10000000UL  
#define   MMCRA_SIPR	0x08000000UL  
#define   MMCRA_SLOT	0x07000000UL  
#define   MMCRA_SLOT_SHIFT	24
#define   MMCRA_SAMPLE_ENABLE 0x00000001UL  
#define   MMCRA_BHRB_DISABLE  _UL(0x2000000000)  
#define   POWER6_MMCRA_SDSYNC 0x0000080000000000ULL	 
#define   POWER6_MMCRA_SIHV   0x0000040000000000ULL
#define   POWER6_MMCRA_SIPR   0x0000020000000000ULL
#define   POWER6_MMCRA_THRM	0x00000020UL
#define   POWER6_MMCRA_OTHER	0x0000000EUL
#define   POWER7P_MMCRA_SIAR_VALID 0x10000000	 
#define   POWER7P_MMCRA_SDAR_VALID 0x08000000	 
#define SPRN_MMCRH	316	 
#define SPRN_MMCRS	894	 
#define SPRN_MMCRC	851	 
#define SPRN_EBBHR	804	 
#define SPRN_EBBRR	805	 
#define SPRN_BESCR	806	 
#define   BESCR_GE	0x8000000000000000ULL  
#define SPRN_WORT	895	 
#define SPRN_WORC	863	 
#define SPRN_PMC1	787
#define SPRN_PMC2	788
#define SPRN_PMC3	789
#define SPRN_PMC4	790
#define SPRN_PMC5	791
#define SPRN_PMC6	792
#define SPRN_PMC7	793
#define SPRN_PMC8	794
#define SPRN_SIER	784
#define   SIER_SIPR		0x2000000	 
#define   SIER_SIHV		0x1000000	 
#define   SIER_SIAR_VALID	0x0400000	 
#define   SIER_SDAR_VALID	0x0200000	 
#define SPRN_SIER2	752
#define SPRN_SIER3	753
#define SPRN_USIER2	736
#define SPRN_USIER3	737
#define SPRN_SIAR	796
#define SPRN_SDAR	797
#define SPRN_TACR	888
#define SPRN_TCSCR	889
#define SPRN_CSIGR	890
#define SPRN_SPMC1	892
#define SPRN_SPMC2	893
#define MMCR0_USER_MASK	(MMCR0_FC | MMCR0_PMXE | MMCR0_PMAO)
#define MMCR2_USER_MASK	0x4020100804020000UL  
#define SIER_USER_MASK	0x7fffffUL
#define SPRN_PA6T_MMCR0 795
#define   PA6T_MMCR0_EN0	0x0000000000000001UL
#define   PA6T_MMCR0_EN1	0x0000000000000002UL
#define   PA6T_MMCR0_EN2	0x0000000000000004UL
#define   PA6T_MMCR0_EN3	0x0000000000000008UL
#define   PA6T_MMCR0_EN4	0x0000000000000010UL
#define   PA6T_MMCR0_EN5	0x0000000000000020UL
#define   PA6T_MMCR0_SUPEN	0x0000000000000040UL
#define   PA6T_MMCR0_PREN	0x0000000000000080UL
#define   PA6T_MMCR0_HYPEN	0x0000000000000100UL
#define   PA6T_MMCR0_FCM0	0x0000000000000200UL
#define   PA6T_MMCR0_FCM1	0x0000000000000400UL
#define   PA6T_MMCR0_INTGEN	0x0000000000000800UL
#define   PA6T_MMCR0_INTEN0	0x0000000000001000UL
#define   PA6T_MMCR0_INTEN1	0x0000000000002000UL
#define   PA6T_MMCR0_INTEN2	0x0000000000004000UL
#define   PA6T_MMCR0_INTEN3	0x0000000000008000UL
#define   PA6T_MMCR0_INTEN4	0x0000000000010000UL
#define   PA6T_MMCR0_INTEN5	0x0000000000020000UL
#define   PA6T_MMCR0_DISCNT	0x0000000000040000UL
#define   PA6T_MMCR0_UOP	0x0000000000080000UL
#define   PA6T_MMCR0_TRG	0x0000000000100000UL
#define   PA6T_MMCR0_TRGEN	0x0000000000200000UL
#define   PA6T_MMCR0_TRGREG	0x0000000001600000UL
#define   PA6T_MMCR0_SIARLOG	0x0000000002000000UL
#define   PA6T_MMCR0_SDARLOG	0x0000000004000000UL
#define   PA6T_MMCR0_PROEN	0x0000000008000000UL
#define   PA6T_MMCR0_PROLOG	0x0000000010000000UL
#define   PA6T_MMCR0_DAMEN2	0x0000000020000000UL
#define   PA6T_MMCR0_DAMEN3	0x0000000040000000UL
#define   PA6T_MMCR0_DAMEN4	0x0000000080000000UL
#define   PA6T_MMCR0_DAMEN5	0x0000000100000000UL
#define   PA6T_MMCR0_DAMSEL2	0x0000000200000000UL
#define   PA6T_MMCR0_DAMSEL3	0x0000000400000000UL
#define   PA6T_MMCR0_DAMSEL4	0x0000000800000000UL
#define   PA6T_MMCR0_DAMSEL5	0x0000001000000000UL
#define   PA6T_MMCR0_HANDDIS	0x0000002000000000UL
#define   PA6T_MMCR0_PCTEN	0x0000004000000000UL
#define   PA6T_MMCR0_SOCEN	0x0000008000000000UL
#define   PA6T_MMCR0_SOCMOD	0x0000010000000000UL
#define SPRN_PA6T_MMCR1 798
#define   PA6T_MMCR1_ES2	0x00000000000000ffUL
#define   PA6T_MMCR1_ES3	0x000000000000ff00UL
#define   PA6T_MMCR1_ES4	0x0000000000ff0000UL
#define   PA6T_MMCR1_ES5	0x00000000ff000000UL
#define SPRN_PA6T_UPMC0 771	 
#define SPRN_PA6T_UPMC1 772	 
#define SPRN_PA6T_UPMC2 773
#define SPRN_PA6T_UPMC3 774
#define SPRN_PA6T_UPMC4 775
#define SPRN_PA6T_UPMC5 776
#define SPRN_PA6T_UMMCR0 779	 
#define SPRN_PA6T_SIAR	780	 
#define SPRN_PA6T_UMMCR1 782	 
#define SPRN_PA6T_SIER	785	 
#define SPRN_PA6T_PMC0	787
#define SPRN_PA6T_PMC1	788
#define SPRN_PA6T_PMC2	789
#define SPRN_PA6T_PMC3	790
#define SPRN_PA6T_PMC4	791
#define SPRN_PA6T_PMC5	792
#define SPRN_PA6T_TSR0	793	 
#define SPRN_PA6T_TSR1	794	 
#define SPRN_PA6T_TSR2	799	 
#define SPRN_PA6T_TSR3	784	 
#define SPRN_PA6T_IER	981	 
#define SPRN_PA6T_DER	982	 
#define SPRN_PA6T_BER	862	 
#define SPRN_PA6T_MER	849	 
#define SPRN_PA6T_IMA0	880	 
#define SPRN_PA6T_IMA1	881	 
#define SPRN_PA6T_IMA2	882
#define SPRN_PA6T_IMA3	883
#define SPRN_PA6T_IMA4	884
#define SPRN_PA6T_IMA5	885
#define SPRN_PA6T_IMA6	886
#define SPRN_PA6T_IMA7	887
#define SPRN_PA6T_IMA8	888
#define SPRN_PA6T_IMA9	889
#define SPRN_PA6T_BTCR	978	 
#define SPRN_PA6T_IMAAT	979	 
#define SPRN_PA6T_PCCR	1019	 
#define SPRN_BKMK	1020	 
#define SPRN_PA6T_RPCCR	1021	 
#else  
#define SPRN_MMCR0	952	 
#define   MMCR0_FC	0x80000000UL  
#define   MMCR0_FCS	0x40000000UL  
#define   MMCR0_FCP	0x20000000UL  
#define   MMCR0_FCM1	0x10000000UL  
#define   MMCR0_FCM0	0x08000000UL  
#define   MMCR0_PMXE	0x04000000UL  
#define   MMCR0_FCECE	0x02000000UL  
#define   MMCR0_TBEE	0x00400000UL  
#define   MMCR0_PMC1CE	0x00008000UL  
#define   MMCR0_PMCnCE	0x00004000UL  
#define   MMCR0_TRIGGER	0x00002000UL  
#define   MMCR0_PMC1SEL	0x00001fc0UL  
#define   MMCR0_PMC2SEL	0x0000003fUL  
#define SPRN_MMCR1	956
#define   MMCR1_PMC3SEL	0xf8000000UL  
#define   MMCR1_PMC4SEL	0x07c00000UL  
#define   MMCR1_PMC5SEL	0x003e0000UL  
#define   MMCR1_PMC6SEL 0x0001f800UL  
#define SPRN_MMCR2	944
#define SPRN_PMC1	953	 
#define SPRN_PMC2	954	 
#define SPRN_PMC3	957	 
#define SPRN_PMC4	958	 
#define SPRN_PMC5	945	 
#define SPRN_PMC6	946	 
#define SPRN_SIAR	955	 
#define MMCR0_PMC1_CYCLES	(1 << 7)
#define MMCR0_PMC1_ICACHEMISS	(5 << 7)
#define MMCR0_PMC1_DTLB		(6 << 7)
#define MMCR0_PMC2_DCACHEMISS	0x6
#define MMCR0_PMC2_CYCLES	0x1
#define MMCR0_PMC2_ITLB		0x7
#define MMCR0_PMC2_LOADMISSTIME	0x5
#endif
#ifdef CONFIG_PPC64
#define SPRN_SPRG_PACA 		SPRN_SPRG1
#else
#define SPRN_SPRG_THREAD 	SPRN_SPRG3
#endif
#ifdef CONFIG_PPC_BOOK3S_64
#define SPRN_SPRG_SCRATCH0	SPRN_SPRG2
#define SPRN_SPRG_HPACA		SPRN_HSPRG0
#define SPRN_SPRG_HSCRATCH0	SPRN_HSPRG1
#define SPRN_SPRG_VDSO_READ	SPRN_USPRG3
#define SPRN_SPRG_VDSO_WRITE	SPRN_SPRG3
#define GET_PACA(rX)					\
	BEGIN_FTR_SECTION_NESTED(66);			\
	mfspr	rX,SPRN_SPRG_PACA;			\
	FTR_SECTION_ELSE_NESTED(66);			\
	mfspr	rX,SPRN_SPRG_HPACA;			\
	ALT_FTR_SECTION_END_NESTED_IFCLR(CPU_FTR_HVMODE, 66)
#define SET_PACA(rX)					\
	BEGIN_FTR_SECTION_NESTED(66);			\
	mtspr	SPRN_SPRG_PACA,rX;			\
	FTR_SECTION_ELSE_NESTED(66);			\
	mtspr	SPRN_SPRG_HPACA,rX;			\
	ALT_FTR_SECTION_END_NESTED_IFCLR(CPU_FTR_HVMODE, 66)
#define GET_SCRATCH0(rX)				\
	BEGIN_FTR_SECTION_NESTED(66);			\
	mfspr	rX,SPRN_SPRG_SCRATCH0;			\
	FTR_SECTION_ELSE_NESTED(66);			\
	mfspr	rX,SPRN_SPRG_HSCRATCH0;			\
	ALT_FTR_SECTION_END_NESTED_IFCLR(CPU_FTR_HVMODE, 66)
#define SET_SCRATCH0(rX)				\
	BEGIN_FTR_SECTION_NESTED(66);			\
	mtspr	SPRN_SPRG_SCRATCH0,rX;			\
	FTR_SECTION_ELSE_NESTED(66);			\
	mtspr	SPRN_SPRG_HSCRATCH0,rX;			\
	ALT_FTR_SECTION_END_NESTED_IFCLR(CPU_FTR_HVMODE, 66)
#else  
#define GET_SCRATCH0(rX)	mfspr	rX,SPRN_SPRG_SCRATCH0
#define SET_SCRATCH0(rX)	mtspr	SPRN_SPRG_SCRATCH0,rX
#endif
#ifdef CONFIG_PPC_BOOK3E_64
#define SPRN_SPRG_MC_SCRATCH	SPRN_SPRG8
#define SPRN_SPRG_CRIT_SCRATCH	SPRN_SPRG3
#define SPRN_SPRG_DBG_SCRATCH	SPRN_SPRG9
#define SPRN_SPRG_TLB_EXFRAME	SPRN_SPRG2
#define SPRN_SPRG_TLB_SCRATCH	SPRN_SPRG6
#define SPRN_SPRG_GEN_SCRATCH	SPRN_SPRG0
#define SPRN_SPRG_GDBELL_SCRATCH SPRN_SPRG_GEN_SCRATCH
#define SPRN_SPRG_VDSO_READ	SPRN_USPRG7
#define SPRN_SPRG_VDSO_WRITE	SPRN_SPRG7
#define SET_PACA(rX)	mtspr	SPRN_SPRG_PACA,rX
#define GET_PACA(rX)	mfspr	rX,SPRN_SPRG_PACA
#endif
#ifdef CONFIG_PPC_BOOK3S_32
#define SPRN_SPRG_SCRATCH0	SPRN_SPRG0
#define SPRN_SPRG_SCRATCH1	SPRN_SPRG1
#define SPRN_SPRG_SCRATCH2	SPRN_SPRG2
#define SPRN_SPRG_603_LRU	SPRN_SPRG4
#endif
#ifdef CONFIG_40x
#define SPRN_SPRG_SCRATCH0	SPRN_SPRG0
#define SPRN_SPRG_SCRATCH1	SPRN_SPRG1
#define SPRN_SPRG_SCRATCH2	SPRN_SPRG2
#define SPRN_SPRG_SCRATCH3	SPRN_SPRG4
#define SPRN_SPRG_SCRATCH4	SPRN_SPRG5
#define SPRN_SPRG_SCRATCH5	SPRN_SPRG6
#define SPRN_SPRG_SCRATCH6	SPRN_SPRG7
#endif
#ifdef CONFIG_BOOKE
#define SPRN_SPRG_RSCRATCH0	SPRN_SPRG0
#define SPRN_SPRG_WSCRATCH0	SPRN_SPRG0
#define SPRN_SPRG_RSCRATCH1	SPRN_SPRG1
#define SPRN_SPRG_WSCRATCH1	SPRN_SPRG1
#define SPRN_SPRG_RSCRATCH_CRIT	SPRN_SPRG2
#define SPRN_SPRG_WSCRATCH_CRIT	SPRN_SPRG2
#define SPRN_SPRG_RSCRATCH2	SPRN_SPRG4R
#define SPRN_SPRG_WSCRATCH2	SPRN_SPRG4W
#define SPRN_SPRG_RSCRATCH3	SPRN_SPRG5R
#define SPRN_SPRG_WSCRATCH3	SPRN_SPRG5W
#define SPRN_SPRG_RSCRATCH_MC	SPRN_SPRG1
#define SPRN_SPRG_WSCRATCH_MC	SPRN_SPRG1
#define SPRN_SPRG_RSCRATCH4	SPRN_SPRG7R
#define SPRN_SPRG_WSCRATCH4	SPRN_SPRG7W
#define SPRN_SPRG_RSCRATCH_DBG	SPRN_SPRG9
#define SPRN_SPRG_WSCRATCH_DBG	SPRN_SPRG9
#endif
#ifdef CONFIG_PPC_8xx
#define SPRN_SPRG_SCRATCH0	SPRN_SPRG0
#define SPRN_SPRG_SCRATCH1	SPRN_SPRG1
#define SPRN_SPRG_SCRATCH2	SPRN_SPRG2
#endif
#ifdef CONFIG_PPC64
#define MTFSF_L(REG) \
	.long (0xfc00058e | ((0xff) << 17) | ((REG) << 11) | (1 << 25))
#else
#define MTFSF_L(REG)	mtfsf	0xff, (REG)
#endif
#define PVR_VER(pvr)	(((pvr) >>  16) & 0xFFFF)	 
#define PVR_REV(pvr)	(((pvr) >>   0) & 0xFFFF)	 
#define pvr_version_is(pvr)	(PVR_VER(mfspr(SPRN_PVR)) == (pvr))
#define PVR_FAM(pvr)	(((pvr) >> 20) & 0xFFF)	 
#define PVR_MEM(pvr)	(((pvr) >> 16) & 0xF)	 
#define PVR_CORE(pvr)	(((pvr) >> 12) & 0xF)	 
#define PVR_CFG(pvr)	(((pvr) >>  8) & 0xF)	 
#define PVR_MAJ(pvr)	(((pvr) >>  4) & 0xF)	 
#define PVR_MIN(pvr)	(((pvr) >>  0) & 0xF)	 
#define PVR_403GA	0x00200000
#define PVR_403GB	0x00200100
#define PVR_403GC	0x00200200
#define PVR_403GCX	0x00201400
#define PVR_405GP	0x40110000
#define PVR_476		0x11a52000
#define PVR_476FPE	0x7ff50000
#define PVR_STB03XXX	0x40310000
#define PVR_NP405H	0x41410000
#define PVR_NP405L	0x41610000
#define PVR_601		0x00010000
#define PVR_602		0x00050000
#define PVR_603		0x00030000
#define PVR_603e	0x00060000
#define PVR_603ev	0x00070000
#define PVR_603r	0x00071000
#define PVR_604		0x00040000
#define PVR_604e	0x00090000
#define PVR_604r	0x000A0000
#define PVR_620		0x00140000
#define PVR_740		0x00080000
#define PVR_750		PVR_740
#define PVR_740P	0x10080000
#define PVR_750P	PVR_740P
#define PVR_7400	0x000C0000
#define PVR_7410	0x800C0000
#define PVR_7450	0x80000000
#define PVR_8540	0x80200000
#define PVR_8560	0x80200000
#define PVR_VER_E500V1	0x8020
#define PVR_VER_E500V2	0x8021
#define PVR_VER_E500MC	0x8023
#define PVR_VER_E5500	0x8024
#define PVR_VER_E6500	0x8040
#define PVR_VER_7450	0x8000
#define PVR_VER_7455	0x8001
#define PVR_VER_7447	0x8002
#define PVR_VER_7447A	0x8003
#define PVR_VER_7448	0x8004
#define PVR_8xx		0x00500000
#define PVR_8240	0x00810100
#define PVR_8245	0x80811014
#define PVR_8260	PVR_8240
#define PVR_476_ISS	0x00052000
#define PVR_NORTHSTAR	0x0033
#define PVR_PULSAR	0x0034
#define PVR_POWER4	0x0035
#define PVR_ICESTAR	0x0036
#define PVR_SSTAR	0x0037
#define PVR_POWER4p	0x0038
#define PVR_970		0x0039
#define PVR_POWER5	0x003A
#define PVR_POWER5p	0x003B
#define PVR_970FX	0x003C
#define PVR_POWER6	0x003E
#define PVR_POWER7	0x003F
#define PVR_630		0x0040
#define PVR_630p	0x0041
#define PVR_970MP	0x0044
#define PVR_970GX	0x0045
#define PVR_POWER7p	0x004A
#define PVR_POWER8E	0x004B
#define PVR_POWER8NVL	0x004C
#define PVR_POWER8	0x004D
#define PVR_POWER9	0x004E
#define PVR_POWER10	0x0080
#define PVR_BE		0x0070
#define PVR_PA6T	0x0090
#define PVR_ARCH_204	0x0f000001
#define PVR_ARCH_205	0x0f000002
#define PVR_ARCH_206	0x0f000003
#define PVR_ARCH_206p	0x0f100003
#define PVR_ARCH_207	0x0f000004
#define PVR_ARCH_300	0x0f000005
#define PVR_ARCH_31	0x0f000006
#ifndef __ASSEMBLY__
#if defined(CONFIG_PPC64) || defined(__CHECKER__)
typedef struct {
	u32 val;
#ifdef CONFIG_PPC64
	u32 suffix;
#endif
} __packed ppc_inst_t;
#else
typedef u32 ppc_inst_t;
#endif
#define mfmsr()		({unsigned long rval; \
			asm volatile("mfmsr %0" : "=r" (rval) : \
						: "memory"); rval;})
#ifdef CONFIG_PPC_BOOK3S_64
#define __mtmsrd(v, l)	asm volatile("mtmsrd %0," __stringify(l) \
				     : : "r" (v) : "memory")
#define mtmsr(v)	__mtmsrd((v), 0)
#define __MTMSR		"mtmsrd"
#else
#define mtmsr(v)	asm volatile("mtmsr %0" : \
				     : "r" ((unsigned long)(v)) \
				     : "memory")
#define __mtmsrd(v, l)	BUILD_BUG()
#define __MTMSR		"mtmsr"
#endif
static inline void mtmsr_isync(unsigned long val)
{
	asm volatile(__MTMSR " %0; " ASM_FTR_IFCLR("isync", "nop", %1) : :
			"r" (val), "i" (CPU_FTR_ARCH_206) : "memory");
}
#define mfspr(rn)	({unsigned long rval; \
			asm volatile("mfspr %0," __stringify(rn) \
				: "=r" (rval)); rval;})
#define mtspr(rn, v)	asm volatile("mtspr " __stringify(rn) ",%0" : \
				     : "r" ((unsigned long)(v)) \
				     : "memory")
#define wrtspr(rn)	asm volatile("mtspr " __stringify(rn) ",2" : : : "memory")
static inline void wrtee(unsigned long val)
{
	if (__builtin_constant_p(val))
		asm volatile("wrteei %0" : : "i" ((val & MSR_EE) ? 1 : 0) : "memory");
	else
		asm volatile("wrtee %0" : : "r" (val) : "memory");
}
extern unsigned long msr_check_and_set(unsigned long bits);
extern bool strict_msr_control;
extern void __msr_check_and_clear(unsigned long bits);
static inline void msr_check_and_clear(unsigned long bits)
{
	if (strict_msr_control)
		__msr_check_and_clear(bits);
}
#ifdef CONFIG_PPC32
static inline u32 mfsr(u32 idx)
{
	u32 val;
	if (__builtin_constant_p(idx))
		asm volatile("mfsr %0, %1" : "=r" (val): "i" (idx >> 28));
	else
		asm volatile("mfsrin %0, %1" : "=r" (val): "r" (idx));
	return val;
}
static inline void mtsr(u32 val, u32 idx)
{
	if (__builtin_constant_p(idx))
		asm volatile("mtsr %1, %0" : : "r" (val), "i" (idx >> 28));
	else
		asm volatile("mtsrin %0, %1" : : "r" (val), "r" (idx));
}
#endif
extern unsigned long current_stack_frame(void);
register unsigned long current_stack_pointer asm("r1");
extern unsigned long scom970_read(unsigned int address);
extern void scom970_write(unsigned int address, unsigned long value);
struct pt_regs;
extern void ppc_save_regs(struct pt_regs *regs);
#endif  
#endif  
#endif  
