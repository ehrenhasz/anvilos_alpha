#ifndef _XTENSA_CORE_TIE_ASM_H
#define _XTENSA_CORE_TIE_ASM_H
#define XTHAL_SAS_TIE	0x0001	 
#define XTHAL_SAS_OPT	0x0002	 
#define XTHAL_SAS_NOCC	0x0004	 
#define XTHAL_SAS_CC	0x0008	 
#define XTHAL_SAS_CALR	0x0010	 
#define XTHAL_SAS_CALE	0x0020	 
#define XTHAL_SAS_GLOB	0x0040	 
#define XTHAL_SAS_ALL	0xFFFF	 
	.macro xchal_ncp_store  ptr at1 at2 at3 at4  continue=0 ofs=-1 select=XTHAL_SAS_ALL
	xchal_sa_start	\continue, \ofs
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~\select
	xchal_sa_align	\ptr, 0, 1024-4, 4, 4
	rsr	\at1, BR		 
	s32i	\at1, \ptr, .Lxchal_ofs_ + 0
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 4
	.endif
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~\select
	xchal_sa_align	\ptr, 0, 1024-4, 4, 4
	rsr	\at1, SCOMPARE1		 
	s32i	\at1, \ptr, .Lxchal_ofs_ + 0
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 4
	.endif
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_CC | XTHAL_SAS_GLOB) & ~\select
	xchal_sa_align	\ptr, 0, 1024-4, 4, 4
	rur	\at1, THREADPTR		 
	s32i	\at1, \ptr, .Lxchal_ofs_ + 0
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 4
	.endif
	.endm	 
	.macro xchal_ncp_load  ptr at1 at2 at3 at4  continue=0 ofs=-1 select=XTHAL_SAS_ALL
	xchal_sa_start	\continue, \ofs
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~\select
	xchal_sa_align	\ptr, 0, 1024-4, 4, 4
	l32i	\at1, \ptr, .Lxchal_ofs_ + 0
	wsr	\at1, BR		 
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 4
	.endif
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~\select
	xchal_sa_align	\ptr, 0, 1024-4, 4, 4
	l32i	\at1, \ptr, .Lxchal_ofs_ + 0
	wsr	\at1, SCOMPARE1		 
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 4
	.endif
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_CC | XTHAL_SAS_GLOB) & ~\select
	xchal_sa_align	\ptr, 0, 1024-4, 4, 4
	l32i	\at1, \ptr, .Lxchal_ofs_ + 0
	wur	\at1, THREADPTR		 
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 4
	.endif
	.endm	 
#define XCHAL_NCP_NUM_ATMPS	1
#define xchal_cp_AudioEngineLX_store	xchal_cp1_store
	.macro	xchal_cp1_store  ptr at1 at2 at3 at4  continue=0 ofs=-1 select=XTHAL_SAS_ALL
	xchal_sa_start \continue, \ofs
	.ifeq (XTHAL_SAS_TIE | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~\select
	xchal_sa_align	\ptr, 0, 0, 1, 8
	rur240	\at1		 
	s32i	\at1, \ptr, 0
	rur241	\at1		 
	s32i	\at1, \ptr, 4
	rur242	\at1		 
	s32i	\at1, \ptr, 8
	rur243	\at1		 
	s32i	\at1, \ptr, 12
	AE_SP24X2S.I aep0, \ptr,  16
	AE_SP24X2S.I aep1, \ptr,  24
	AE_SP24X2S.I aep2, \ptr,  32
	AE_SP24X2S.I aep3, \ptr,  40
	AE_SP24X2S.I aep4, \ptr,  48
	AE_SP24X2S.I aep5, \ptr,  56
	addi	\ptr, \ptr, 64
	AE_SP24X2S.I aep6, \ptr,  0
	AE_SP24X2S.I aep7, \ptr,  8
	AE_SQ56S.I aeq0, \ptr,  16
	AE_SQ56S.I aeq1, \ptr,  24
	AE_SQ56S.I aeq2, \ptr,  32
	AE_SQ56S.I aeq3, \ptr,  40
	.set	.Lxchal_pofs_, .Lxchal_pofs_ + 64
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 112
	.endif
	.endm	 
#define xchal_cp_AudioEngineLX_load	xchal_cp1_load
	.macro	xchal_cp1_load  ptr at1 at2 at3 at4  continue=0 ofs=-1 select=XTHAL_SAS_ALL
	xchal_sa_start \continue, \ofs
	.ifeq (XTHAL_SAS_TIE | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~\select
	xchal_sa_align	\ptr, 0, 0, 1, 8
	l32i	\at1, \ptr, 0
	wur240	\at1		 
	l32i	\at1, \ptr, 4
	wur241	\at1		 
	l32i	\at1, \ptr, 8
	wur242	\at1		 
	l32i	\at1, \ptr, 12
	wur243	\at1		 
	addi	\ptr, \ptr, 80
	AE_LQ56.I aeq0, \ptr,  0
	AE_LQ56.I aeq1, \ptr,  8
	AE_LQ56.I aeq2, \ptr,  16
	AE_LQ56.I aeq3, \ptr,  24
	AE_LP24X2.I aep0, \ptr,  -64
	AE_LP24X2.I aep1, \ptr,  -56
	AE_LP24X2.I aep2, \ptr,  -48
	AE_LP24X2.I aep3, \ptr,  -40
	AE_LP24X2.I aep4, \ptr,  -32
	AE_LP24X2.I aep5, \ptr,  -24
	AE_LP24X2.I aep6, \ptr,  -16
	AE_LP24X2.I aep7, \ptr,  -8
	.set	.Lxchal_pofs_, .Lxchal_pofs_ + 80
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 112
	.endif
	.endm	 
#define XCHAL_CP1_NUM_ATMPS	1
#define XCHAL_SA_NUM_ATMPS	1
	.macro xchal_cp0_store	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp0_load	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp2_store	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp2_load	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp3_store	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp3_load	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp4_store	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp4_load	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp5_store	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp5_load	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp6_store	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp6_load	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp7_store	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp7_load	p a b c d continue=0 ofs=-1 select=-1 ; .endm
#endif  
