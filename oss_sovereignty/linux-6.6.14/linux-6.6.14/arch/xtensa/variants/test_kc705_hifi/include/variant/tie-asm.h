#ifndef _XTENSA_CORE_TIE_ASM_H
#define _XTENSA_CORE_TIE_ASM_H
#define XTHAL_SAS_TIE	0x0001	 
#define XTHAL_SAS_OPT	0x0002	 
#define XTHAL_SAS_ANYOT	0x0003	 
#define XTHAL_SAS_NOCC	0x0004	 
#define XTHAL_SAS_CC	0x0008	 
#define XTHAL_SAS_ANYCC	0x000C	 
#define XTHAL_SAS_CALR	0x0010	 
#define XTHAL_SAS_CALE	0x0020	 
#define XTHAL_SAS_GLOB	0x0040	 
#define XTHAL_SAS_ANYABI 0x0070	 
#define XTHAL_SAS_ALL	0xFFFF	 
#define XTHAL_SAS3(optie,ccuse,abi)	( ((optie) & XTHAL_SAS_ANYOT)  \
					| ((ccuse) & XTHAL_SAS_ANYCC)  \
					| ((abi)   & XTHAL_SAS_ANYABI) )
    .macro xchal_ncp_store  ptr at1 at2 at3 at4  continue=0 ofs=-1 select=XTHAL_SAS_ALL alloc=0
	xchal_sa_start	\continue, \ofs
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_CC | XTHAL_SAS_GLOB) & ~(\select)
	xchal_sa_align	\ptr, 0, 1020, 4, 4
	rur.THREADPTR	\at1		 
	s32i	\at1, \ptr, .Lxchal_ofs_+0
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 4
	.elseif ((XTHAL_SAS_OPT | XTHAL_SAS_CC | XTHAL_SAS_GLOB) & ~(\alloc)) == 0
	xchal_sa_align	\ptr, 0, 1020, 4, 4
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 4
	.endif
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_CC | XTHAL_SAS_CALR) & ~(\select)
	xchal_sa_align	\ptr, 0, 1016, 4, 4
	rsr.ACCLO	\at1		 
	s32i	\at1, \ptr, .Lxchal_ofs_+0
	rsr.ACCHI	\at1		 
	s32i	\at1, \ptr, .Lxchal_ofs_+4
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 8
	.elseif ((XTHAL_SAS_OPT | XTHAL_SAS_CC | XTHAL_SAS_CALR) & ~(\alloc)) == 0
	xchal_sa_align	\ptr, 0, 1016, 4, 4
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 8
	.endif
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~(\select)
	xchal_sa_align	\ptr, 0, 1000, 4, 4
	rsr.M0	\at1		 
	s32i	\at1, \ptr, .Lxchal_ofs_+0
	rsr.M1	\at1		 
	s32i	\at1, \ptr, .Lxchal_ofs_+4
	rsr.M2	\at1		 
	s32i	\at1, \ptr, .Lxchal_ofs_+8
	rsr.M3	\at1		 
	s32i	\at1, \ptr, .Lxchal_ofs_+12
	rsr.BR	\at1		 
	s32i	\at1, \ptr, .Lxchal_ofs_+16
	rsr.SCOMPARE1	\at1		 
	s32i	\at1, \ptr, .Lxchal_ofs_+20
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 24
	.elseif ((XTHAL_SAS_OPT | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~(\alloc)) == 0
	xchal_sa_align	\ptr, 0, 1000, 4, 4
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 24
	.endif
    .endm	 
    .macro xchal_ncp_load  ptr at1 at2 at3 at4  continue=0 ofs=-1 select=XTHAL_SAS_ALL alloc=0
	xchal_sa_start	\continue, \ofs
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_CC | XTHAL_SAS_GLOB) & ~(\select)
	xchal_sa_align	\ptr, 0, 1020, 4, 4
	l32i	\at1, \ptr, .Lxchal_ofs_+0
	wur.THREADPTR	\at1		 
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 4
	.elseif ((XTHAL_SAS_OPT | XTHAL_SAS_CC | XTHAL_SAS_GLOB) & ~(\alloc)) == 0
	xchal_sa_align	\ptr, 0, 1020, 4, 4
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 4
	.endif
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_CC | XTHAL_SAS_CALR) & ~(\select)
	xchal_sa_align	\ptr, 0, 1016, 4, 4
	l32i	\at1, \ptr, .Lxchal_ofs_+0
	wsr.ACCLO	\at1		 
	l32i	\at1, \ptr, .Lxchal_ofs_+4
	wsr.ACCHI	\at1		 
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 8
	.elseif ((XTHAL_SAS_OPT | XTHAL_SAS_CC | XTHAL_SAS_CALR) & ~(\alloc)) == 0
	xchal_sa_align	\ptr, 0, 1016, 4, 4
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 8
	.endif
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~(\select)
	xchal_sa_align	\ptr, 0, 1000, 4, 4
	l32i	\at1, \ptr, .Lxchal_ofs_+0
	wsr.M0	\at1		 
	l32i	\at1, \ptr, .Lxchal_ofs_+4
	wsr.M1	\at1		 
	l32i	\at1, \ptr, .Lxchal_ofs_+8
	wsr.M2	\at1		 
	l32i	\at1, \ptr, .Lxchal_ofs_+12
	wsr.M3	\at1		 
	l32i	\at1, \ptr, .Lxchal_ofs_+16
	wsr.BR	\at1		 
	l32i	\at1, \ptr, .Lxchal_ofs_+20
	wsr.SCOMPARE1	\at1		 
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 24
	.elseif ((XTHAL_SAS_OPT | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~(\alloc)) == 0
	xchal_sa_align	\ptr, 0, 1000, 4, 4
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 24
	.endif
    .endm	 
#define XCHAL_NCP_NUM_ATMPS	1
#define xchal_cp_AudioEngineLX_store	xchal_cp1_store
    .macro	xchal_cp1_store  ptr at1 at2 at3 at4  continue=0 ofs=-1 select=XTHAL_SAS_ALL alloc=0
	xchal_sa_start \continue, \ofs
	.ifeq (XTHAL_SAS_TIE | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~(\select)
	xchal_sa_align	\ptr, 0, 0, 8, 8
	rur.AE_OVF_SAR	\at1		 
	s32i	\at1, \ptr, .Lxchal_ofs_+0
	rur.AE_BITHEAD	\at1		 
	s32i	\at1, \ptr, .Lxchal_ofs_+4
	rur.AE_TS_FTS_BU_BP	\at1		 
	s32i	\at1, \ptr, .Lxchal_ofs_+8
	rur.AE_CW_SD_NO	\at1		 
	s32i	\at1, \ptr, .Lxchal_ofs_+12
	rur.AE_CBEGIN0	\at1		 
	s32i	\at1, \ptr, .Lxchal_ofs_+16
	rur.AE_CEND0	\at1		 
	s32i	\at1, \ptr, .Lxchal_ofs_+20
	AE_S64.I	aed0, \ptr, .Lxchal_ofs_+24
	AE_S64.I	aed1, \ptr, .Lxchal_ofs_+32
	AE_S64.I	aed2, \ptr, .Lxchal_ofs_+40
	AE_S64.I	aed3, \ptr, .Lxchal_ofs_+48
	AE_S64.I	aed4, \ptr, .Lxchal_ofs_+56
	addi	\ptr, \ptr, 64
	AE_S64.I	aed5, \ptr, .Lxchal_ofs_+0
	AE_S64.I	aed6, \ptr, .Lxchal_ofs_+8
	AE_S64.I	aed7, \ptr, .Lxchal_ofs_+16
	AE_S64.I	aed8, \ptr, .Lxchal_ofs_+24
	AE_S64.I	aed9, \ptr, .Lxchal_ofs_+32
	AE_S64.I	aed10, \ptr, .Lxchal_ofs_+40
	AE_S64.I	aed11, \ptr, .Lxchal_ofs_+48
	AE_S64.I	aed12, \ptr, .Lxchal_ofs_+56
	addi	\ptr, \ptr, 64
	AE_S64.I	aed13, \ptr, .Lxchal_ofs_+0
	AE_S64.I	aed14, \ptr, .Lxchal_ofs_+8
	AE_S64.I	aed15, \ptr, .Lxchal_ofs_+16
	AE_SALIGN64.I	u0, \ptr, .Lxchal_ofs_+24
	AE_SALIGN64.I	u1, \ptr, .Lxchal_ofs_+32
	AE_SALIGN64.I	u2, \ptr, .Lxchal_ofs_+40
	AE_SALIGN64.I	u3, \ptr, .Lxchal_ofs_+48
	.set	.Lxchal_pofs_, .Lxchal_pofs_ + 128
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 56
	.elseif ((XTHAL_SAS_TIE | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~(\alloc)) == 0
	xchal_sa_align	\ptr, 0, 0, 8, 8
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 184
	.endif
    .endm	 
#define xchal_cp_AudioEngineLX_load	xchal_cp1_load
    .macro	xchal_cp1_load  ptr at1 at2 at3 at4  continue=0 ofs=-1 select=XTHAL_SAS_ALL alloc=0
	xchal_sa_start \continue, \ofs
	.ifeq (XTHAL_SAS_TIE | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~(\select)
	xchal_sa_align	\ptr, 0, 0, 8, 8
	l32i	\at1, \ptr, .Lxchal_ofs_+0
	wur.AE_OVF_SAR	\at1		 
	l32i	\at1, \ptr, .Lxchal_ofs_+4
	wur.AE_BITHEAD	\at1		 
	l32i	\at1, \ptr, .Lxchal_ofs_+8
	wur.AE_TS_FTS_BU_BP	\at1		 
	l32i	\at1, \ptr, .Lxchal_ofs_+12
	wur.AE_CW_SD_NO	\at1		 
	l32i	\at1, \ptr, .Lxchal_ofs_+16
	wur.AE_CBEGIN0	\at1		 
	l32i	\at1, \ptr, .Lxchal_ofs_+20
	wur.AE_CEND0	\at1		 
	AE_L64.I	aed0, \ptr, .Lxchal_ofs_+24
	AE_L64.I	aed1, \ptr, .Lxchal_ofs_+32
	AE_L64.I	aed2, \ptr, .Lxchal_ofs_+40
	AE_L64.I	aed3, \ptr, .Lxchal_ofs_+48
	AE_L64.I	aed4, \ptr, .Lxchal_ofs_+56
	addi	\ptr, \ptr, 64
	AE_L64.I	aed5, \ptr, .Lxchal_ofs_+0
	AE_L64.I	aed6, \ptr, .Lxchal_ofs_+8
	AE_L64.I	aed7, \ptr, .Lxchal_ofs_+16
	AE_L64.I	aed8, \ptr, .Lxchal_ofs_+24
	AE_L64.I	aed9, \ptr, .Lxchal_ofs_+32
	AE_L64.I	aed10, \ptr, .Lxchal_ofs_+40
	AE_L64.I	aed11, \ptr, .Lxchal_ofs_+48
	AE_L64.I	aed12, \ptr, .Lxchal_ofs_+56
	addi	\ptr, \ptr, 64
	AE_L64.I	aed13, \ptr, .Lxchal_ofs_+0
	AE_L64.I	aed14, \ptr, .Lxchal_ofs_+8
	AE_L64.I	aed15, \ptr, .Lxchal_ofs_+16
	AE_LALIGN64.I	u0, \ptr, .Lxchal_ofs_+24
	AE_LALIGN64.I	u1, \ptr, .Lxchal_ofs_+32
	AE_LALIGN64.I	u2, \ptr, .Lxchal_ofs_+40
	AE_LALIGN64.I	u3, \ptr, .Lxchal_ofs_+48
	.set	.Lxchal_pofs_, .Lxchal_pofs_ + 128
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 56
	.elseif ((XTHAL_SAS_TIE | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~(\alloc)) == 0
	xchal_sa_align	\ptr, 0, 0, 8, 8
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 184
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
