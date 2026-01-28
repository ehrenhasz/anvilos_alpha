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
#define XTHAL_SAS_ANYABI	0x0070	 
#define XTHAL_SAS_ALL	0xFFFF	 
#define XTHAL_SAS3(optie,ccuse,abi)	( ((optie) & XTHAL_SAS_ANYOT)  \
					| ((ccuse) & XTHAL_SAS_ANYCC)  \
					| ((abi)   & XTHAL_SAS_ANYABI) )
    .macro xchal_ncp_store  ptr at1 at2 at3 at4  continue=0 ofs=-1 select=XTHAL_SAS_ALL alloc=0
	xchal_sa_start	\continue, \ofs
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
	xchal_sa_align	\ptr, 0, 1004, 4, 4
	rsr.SCOMPARE1	\at1		 
	s32i	\at1, \ptr, .Lxchal_ofs_+0
	rsr.M0	\at1		 
	s32i	\at1, \ptr, .Lxchal_ofs_+4
	rsr.M1	\at1		 
	s32i	\at1, \ptr, .Lxchal_ofs_+8
	rsr.M2	\at1		 
	s32i	\at1, \ptr, .Lxchal_ofs_+12
	rsr.M3	\at1		 
	s32i	\at1, \ptr, .Lxchal_ofs_+16
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 20
	.elseif ((XTHAL_SAS_OPT | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~(\alloc)) == 0
	xchal_sa_align	\ptr, 0, 1004, 4, 4
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 20
	.endif
    .endm	 
    .macro xchal_ncp_load  ptr at1 at2 at3 at4  continue=0 ofs=-1 select=XTHAL_SAS_ALL alloc=0
	xchal_sa_start	\continue, \ofs
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
	xchal_sa_align	\ptr, 0, 1004, 4, 4
	l32i	\at1, \ptr, .Lxchal_ofs_+0
	wsr.SCOMPARE1	\at1		 
	l32i	\at1, \ptr, .Lxchal_ofs_+4
	wsr.M0	\at1		 
	l32i	\at1, \ptr, .Lxchal_ofs_+8
	wsr.M1	\at1		 
	l32i	\at1, \ptr, .Lxchal_ofs_+12
	wsr.M2	\at1		 
	l32i	\at1, \ptr, .Lxchal_ofs_+16
	wsr.M3	\at1		 
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 20
	.elseif ((XTHAL_SAS_OPT | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~(\alloc)) == 0
	xchal_sa_align	\ptr, 0, 1004, 4, 4
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 20
	.endif
    .endm	 
#define XCHAL_NCP_NUM_ATMPS	1
#define XCHAL_SA_NUM_ATMPS	1
#endif  
