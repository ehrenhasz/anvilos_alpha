 
 

#ifndef __OCTEON_HCD_H__
#define __OCTEON_HCD_H__

#include <asm/bitfield.h>

#define CVMX_USBCXBASE 0x00016F0010000000ull
#define CVMX_USBCXREG1(reg, bid) \
	(CVMX_ADD_IO_SEG(CVMX_USBCXBASE | reg) + \
	 ((bid) & 1) * 0x100000000000ull)
#define CVMX_USBCXREG2(reg, bid, off) \
	(CVMX_ADD_IO_SEG(CVMX_USBCXBASE | reg) + \
	 (((off) & 7) + ((bid) & 1) * 0x8000000000ull) * 32)

#define CVMX_USBCX_GAHBCFG(bid)		CVMX_USBCXREG1(0x008, bid)
#define CVMX_USBCX_GHWCFG3(bid)		CVMX_USBCXREG1(0x04c, bid)
#define CVMX_USBCX_GINTMSK(bid)		CVMX_USBCXREG1(0x018, bid)
#define CVMX_USBCX_GINTSTS(bid)		CVMX_USBCXREG1(0x014, bid)
#define CVMX_USBCX_GNPTXFSIZ(bid)	CVMX_USBCXREG1(0x028, bid)
#define CVMX_USBCX_GNPTXSTS(bid)	CVMX_USBCXREG1(0x02c, bid)
#define CVMX_USBCX_GOTGCTL(bid)		CVMX_USBCXREG1(0x000, bid)
#define CVMX_USBCX_GRSTCTL(bid)		CVMX_USBCXREG1(0x010, bid)
#define CVMX_USBCX_GRXFSIZ(bid)		CVMX_USBCXREG1(0x024, bid)
#define CVMX_USBCX_GRXSTSPH(bid)	CVMX_USBCXREG1(0x020, bid)
#define CVMX_USBCX_GUSBCFG(bid)		CVMX_USBCXREG1(0x00c, bid)
#define CVMX_USBCX_HAINT(bid)		CVMX_USBCXREG1(0x414, bid)
#define CVMX_USBCX_HAINTMSK(bid)	CVMX_USBCXREG1(0x418, bid)
#define CVMX_USBCX_HCCHARX(off, bid)	CVMX_USBCXREG2(0x500, bid, off)
#define CVMX_USBCX_HCFG(bid)		CVMX_USBCXREG1(0x400, bid)
#define CVMX_USBCX_HCINTMSKX(off, bid)	CVMX_USBCXREG2(0x50c, bid, off)
#define CVMX_USBCX_HCINTX(off, bid)	CVMX_USBCXREG2(0x508, bid, off)
#define CVMX_USBCX_HCSPLTX(off, bid)	CVMX_USBCXREG2(0x504, bid, off)
#define CVMX_USBCX_HCTSIZX(off, bid)	CVMX_USBCXREG2(0x510, bid, off)
#define CVMX_USBCX_HFIR(bid)		CVMX_USBCXREG1(0x404, bid)
#define CVMX_USBCX_HFNUM(bid)		CVMX_USBCXREG1(0x408, bid)
#define CVMX_USBCX_HPRT(bid)		CVMX_USBCXREG1(0x440, bid)
#define CVMX_USBCX_HPTXFSIZ(bid)	CVMX_USBCXREG1(0x100, bid)
#define CVMX_USBCX_HPTXSTS(bid)		CVMX_USBCXREG1(0x410, bid)

#define CVMX_USBNXBID1(bid) (((bid) & 1) * 0x10000000ull)
#define CVMX_USBNXBID2(bid) (((bid) & 1) * 0x100000000000ull)

#define CVMX_USBNXREG1(reg, bid) \
	(CVMX_ADD_IO_SEG(0x0001180068000000ull | reg) + CVMX_USBNXBID1(bid))
#define CVMX_USBNXREG2(reg, bid) \
	(CVMX_ADD_IO_SEG(0x00016F0000000000ull | reg) + CVMX_USBNXBID2(bid))

#define CVMX_USBNX_CLK_CTL(bid)		CVMX_USBNXREG1(0x10, bid)
#define CVMX_USBNX_DMA0_INB_CHN0(bid)	CVMX_USBNXREG2(0x818, bid)
#define CVMX_USBNX_DMA0_OUTB_CHN0(bid)	CVMX_USBNXREG2(0x858, bid)
#define CVMX_USBNX_USBP_CTL_STATUS(bid)	CVMX_USBNXREG1(0x18, bid)

 
union cvmx_usbcx_gahbcfg {
	u32 u32;
	 
	struct cvmx_usbcx_gahbcfg_s {
		__BITFIELD_FIELD(u32 reserved_9_31	: 23,
		__BITFIELD_FIELD(u32 ptxfemplvl		: 1,
		__BITFIELD_FIELD(u32 nptxfemplvl	: 1,
		__BITFIELD_FIELD(u32 reserved_6_6	: 1,
		__BITFIELD_FIELD(u32 dmaen		: 1,
		__BITFIELD_FIELD(u32 hbstlen		: 4,
		__BITFIELD_FIELD(u32 glblintrmsk	: 1,
		;)))))))
	} s;
};

 
union cvmx_usbcx_ghwcfg3 {
	u32 u32;
	 
	struct cvmx_usbcx_ghwcfg3_s {
		__BITFIELD_FIELD(u32 dfifodepth				: 16,
		__BITFIELD_FIELD(u32 reserved_13_15			: 3,
		__BITFIELD_FIELD(u32 ahbphysync				: 1,
		__BITFIELD_FIELD(u32 rsttype				: 1,
		__BITFIELD_FIELD(u32 optfeature				: 1,
		__BITFIELD_FIELD(u32 vendor_control_interface_support	: 1,
		__BITFIELD_FIELD(u32 i2c_selection			: 1,
		__BITFIELD_FIELD(u32 otgen				: 1,
		__BITFIELD_FIELD(u32 pktsizewidth			: 3,
		__BITFIELD_FIELD(u32 xfersizewidth			: 4,
		;))))))))))
	} s;
};

 
union cvmx_usbcx_gintmsk {
	u32 u32;
	 
	struct cvmx_usbcx_gintmsk_s {
		__BITFIELD_FIELD(u32 wkupintmsk		: 1,
		__BITFIELD_FIELD(u32 sessreqintmsk	: 1,
		__BITFIELD_FIELD(u32 disconnintmsk	: 1,
		__BITFIELD_FIELD(u32 conidstschngmsk	: 1,
		__BITFIELD_FIELD(u32 reserved_27_27	: 1,
		__BITFIELD_FIELD(u32 ptxfempmsk		: 1,
		__BITFIELD_FIELD(u32 hchintmsk		: 1,
		__BITFIELD_FIELD(u32 prtintmsk		: 1,
		__BITFIELD_FIELD(u32 reserved_23_23	: 1,
		__BITFIELD_FIELD(u32 fetsuspmsk		: 1,
		__BITFIELD_FIELD(u32 incomplpmsk	: 1,
		__BITFIELD_FIELD(u32 incompisoinmsk	: 1,
		__BITFIELD_FIELD(u32 oepintmsk		: 1,
		__BITFIELD_FIELD(u32 inepintmsk		: 1,
		__BITFIELD_FIELD(u32 epmismsk		: 1,
		__BITFIELD_FIELD(u32 reserved_16_16	: 1,
		__BITFIELD_FIELD(u32 eopfmsk		: 1,
		__BITFIELD_FIELD(u32 isooutdropmsk	: 1,
		__BITFIELD_FIELD(u32 enumdonemsk	: 1,
		__BITFIELD_FIELD(u32 usbrstmsk		: 1,
		__BITFIELD_FIELD(u32 usbsuspmsk		: 1,
		__BITFIELD_FIELD(u32 erlysuspmsk	: 1,
		__BITFIELD_FIELD(u32 i2cint		: 1,
		__BITFIELD_FIELD(u32 ulpickintmsk	: 1,
		__BITFIELD_FIELD(u32 goutnakeffmsk	: 1,
		__BITFIELD_FIELD(u32 ginnakeffmsk	: 1,
		__BITFIELD_FIELD(u32 nptxfempmsk	: 1,
		__BITFIELD_FIELD(u32 rxflvlmsk		: 1,
		__BITFIELD_FIELD(u32 sofmsk		: 1,
		__BITFIELD_FIELD(u32 otgintmsk		: 1,
		__BITFIELD_FIELD(u32 modemismsk		: 1,
		__BITFIELD_FIELD(u32 reserved_0_0	: 1,
		;))))))))))))))))))))))))))))))))
	} s;
};

 
union cvmx_usbcx_gintsts {
	u32 u32;
	 
	struct cvmx_usbcx_gintsts_s {
		__BITFIELD_FIELD(u32 wkupint		: 1,
		__BITFIELD_FIELD(u32 sessreqint		: 1,
		__BITFIELD_FIELD(u32 disconnint		: 1,
		__BITFIELD_FIELD(u32 conidstschng	: 1,
		__BITFIELD_FIELD(u32 reserved_27_27	: 1,
		__BITFIELD_FIELD(u32 ptxfemp		: 1,
		__BITFIELD_FIELD(u32 hchint		: 1,
		__BITFIELD_FIELD(u32 prtint		: 1,
		__BITFIELD_FIELD(u32 reserved_23_23	: 1,
		__BITFIELD_FIELD(u32 fetsusp		: 1,
		__BITFIELD_FIELD(u32 incomplp		: 1,
		__BITFIELD_FIELD(u32 incompisoin	: 1,
		__BITFIELD_FIELD(u32 oepint		: 1,
		__BITFIELD_FIELD(u32 iepint		: 1,
		__BITFIELD_FIELD(u32 epmis		: 1,
		__BITFIELD_FIELD(u32 reserved_16_16	: 1,
		__BITFIELD_FIELD(u32 eopf		: 1,
		__BITFIELD_FIELD(u32 isooutdrop		: 1,
		__BITFIELD_FIELD(u32 enumdone		: 1,
		__BITFIELD_FIELD(u32 usbrst		: 1,
		__BITFIELD_FIELD(u32 usbsusp		: 1,
		__BITFIELD_FIELD(u32 erlysusp		: 1,
		__BITFIELD_FIELD(u32 i2cint		: 1,
		__BITFIELD_FIELD(u32 ulpickint		: 1,
		__BITFIELD_FIELD(u32 goutnakeff		: 1,
		__BITFIELD_FIELD(u32 ginnakeff		: 1,
		__BITFIELD_FIELD(u32 nptxfemp		: 1,
		__BITFIELD_FIELD(u32 rxflvl		: 1,
		__BITFIELD_FIELD(u32 sof		: 1,
		__BITFIELD_FIELD(u32 otgint		: 1,
		__BITFIELD_FIELD(u32 modemis		: 1,
		__BITFIELD_FIELD(u32 curmod		: 1,
		;))))))))))))))))))))))))))))))))
	} s;
};

 
union cvmx_usbcx_gnptxfsiz {
	u32 u32;
	 
	struct cvmx_usbcx_gnptxfsiz_s {
		__BITFIELD_FIELD(u32 nptxfdep		: 16,
		__BITFIELD_FIELD(u32 nptxfstaddr	: 16,
		;))
	} s;
};

 
union cvmx_usbcx_gnptxsts {
	u32 u32;
	 
	struct cvmx_usbcx_gnptxsts_s {
		__BITFIELD_FIELD(u32 reserved_31_31	: 1,
		__BITFIELD_FIELD(u32 nptxqtop		: 7,
		__BITFIELD_FIELD(u32 nptxqspcavail	: 8,
		__BITFIELD_FIELD(u32 nptxfspcavail	: 16,
		;))))
	} s;
};

 
union cvmx_usbcx_grstctl {
	u32 u32;
	 
	struct cvmx_usbcx_grstctl_s {
		__BITFIELD_FIELD(u32 ahbidle		: 1,
		__BITFIELD_FIELD(u32 dmareq		: 1,
		__BITFIELD_FIELD(u32 reserved_11_29	: 19,
		__BITFIELD_FIELD(u32 txfnum		: 5,
		__BITFIELD_FIELD(u32 txfflsh		: 1,
		__BITFIELD_FIELD(u32 rxfflsh		: 1,
		__BITFIELD_FIELD(u32 intknqflsh		: 1,
		__BITFIELD_FIELD(u32 frmcntrrst		: 1,
		__BITFIELD_FIELD(u32 hsftrst		: 1,
		__BITFIELD_FIELD(u32 csftrst		: 1,
		;))))))))))
	} s;
};

 
union cvmx_usbcx_grxfsiz {
	u32 u32;
	 
	struct cvmx_usbcx_grxfsiz_s {
		__BITFIELD_FIELD(u32 reserved_16_31	: 16,
		__BITFIELD_FIELD(u32 rxfdep		: 16,
		;))
	} s;
};

 
union cvmx_usbcx_grxstsph {
	u32 u32;
	 
	struct cvmx_usbcx_grxstsph_s {
		__BITFIELD_FIELD(u32 reserved_21_31	: 11,
		__BITFIELD_FIELD(u32 pktsts		: 4,
		__BITFIELD_FIELD(u32 dpid		: 2,
		__BITFIELD_FIELD(u32 bcnt		: 11,
		__BITFIELD_FIELD(u32 chnum		: 4,
		;)))))
	} s;
};

 
union cvmx_usbcx_gusbcfg {
	u32 u32;
	 
	struct cvmx_usbcx_gusbcfg_s {
		__BITFIELD_FIELD(u32 reserved_17_31	: 15,
		__BITFIELD_FIELD(u32 otgi2csel		: 1,
		__BITFIELD_FIELD(u32 phylpwrclksel	: 1,
		__BITFIELD_FIELD(u32 reserved_14_14	: 1,
		__BITFIELD_FIELD(u32 usbtrdtim		: 4,
		__BITFIELD_FIELD(u32 hnpcap		: 1,
		__BITFIELD_FIELD(u32 srpcap		: 1,
		__BITFIELD_FIELD(u32 ddrsel		: 1,
		__BITFIELD_FIELD(u32 physel		: 1,
		__BITFIELD_FIELD(u32 fsintf		: 1,
		__BITFIELD_FIELD(u32 ulpi_utmi_sel	: 1,
		__BITFIELD_FIELD(u32 phyif		: 1,
		__BITFIELD_FIELD(u32 toutcal		: 3,
		;)))))))))))))
	} s;
};

 
union cvmx_usbcx_haint {
	u32 u32;
	 
	struct cvmx_usbcx_haint_s {
		__BITFIELD_FIELD(u32 reserved_16_31	: 16,
		__BITFIELD_FIELD(u32 haint		: 16,
		;))
	} s;
};

 
union cvmx_usbcx_haintmsk {
	u32 u32;
	 
	struct cvmx_usbcx_haintmsk_s {
		__BITFIELD_FIELD(u32 reserved_16_31	: 16,
		__BITFIELD_FIELD(u32 haintmsk		: 16,
		;))
	} s;
};

 
union cvmx_usbcx_hccharx {
	u32 u32;
	 
	struct cvmx_usbcx_hccharx_s {
		__BITFIELD_FIELD(u32 chena		: 1,
		__BITFIELD_FIELD(u32 chdis		: 1,
		__BITFIELD_FIELD(u32 oddfrm		: 1,
		__BITFIELD_FIELD(u32 devaddr		: 7,
		__BITFIELD_FIELD(u32 ec			: 2,
		__BITFIELD_FIELD(u32 eptype		: 2,
		__BITFIELD_FIELD(u32 lspddev		: 1,
		__BITFIELD_FIELD(u32 reserved_16_16	: 1,
		__BITFIELD_FIELD(u32 epdir		: 1,
		__BITFIELD_FIELD(u32 epnum		: 4,
		__BITFIELD_FIELD(u32 mps		: 11,
		;)))))))))))
	} s;
};

 
union cvmx_usbcx_hcfg {
	u32 u32;
	 
	struct cvmx_usbcx_hcfg_s {
		__BITFIELD_FIELD(u32 reserved_3_31	: 29,
		__BITFIELD_FIELD(u32 fslssupp		: 1,
		__BITFIELD_FIELD(u32 fslspclksel	: 2,
		;)))
	} s;
};

 
union cvmx_usbcx_hcintx {
	u32 u32;
	 
	struct cvmx_usbcx_hcintx_s {
		__BITFIELD_FIELD(u32 reserved_11_31	: 21,
		__BITFIELD_FIELD(u32 datatglerr		: 1,
		__BITFIELD_FIELD(u32 frmovrun		: 1,
		__BITFIELD_FIELD(u32 bblerr		: 1,
		__BITFIELD_FIELD(u32 xacterr		: 1,
		__BITFIELD_FIELD(u32 nyet		: 1,
		__BITFIELD_FIELD(u32 ack		: 1,
		__BITFIELD_FIELD(u32 nak		: 1,
		__BITFIELD_FIELD(u32 stall		: 1,
		__BITFIELD_FIELD(u32 ahberr		: 1,
		__BITFIELD_FIELD(u32 chhltd		: 1,
		__BITFIELD_FIELD(u32 xfercompl		: 1,
		;))))))))))))
	} s;
};

 
union cvmx_usbcx_hcintmskx {
	u32 u32;
	 
	struct cvmx_usbcx_hcintmskx_s {
		__BITFIELD_FIELD(u32 reserved_11_31		: 21,
		__BITFIELD_FIELD(u32 datatglerrmsk		: 1,
		__BITFIELD_FIELD(u32 frmovrunmsk		: 1,
		__BITFIELD_FIELD(u32 bblerrmsk			: 1,
		__BITFIELD_FIELD(u32 xacterrmsk			: 1,
		__BITFIELD_FIELD(u32 nyetmsk			: 1,
		__BITFIELD_FIELD(u32 ackmsk			: 1,
		__BITFIELD_FIELD(u32 nakmsk			: 1,
		__BITFIELD_FIELD(u32 stallmsk			: 1,
		__BITFIELD_FIELD(u32 ahberrmsk			: 1,
		__BITFIELD_FIELD(u32 chhltdmsk			: 1,
		__BITFIELD_FIELD(u32 xfercomplmsk		: 1,
		;))))))))))))
	} s;
};

 
union cvmx_usbcx_hcspltx {
	u32 u32;
	 
	struct cvmx_usbcx_hcspltx_s {
		__BITFIELD_FIELD(u32 spltena			: 1,
		__BITFIELD_FIELD(u32 reserved_17_30		: 14,
		__BITFIELD_FIELD(u32 compsplt			: 1,
		__BITFIELD_FIELD(u32 xactpos			: 2,
		__BITFIELD_FIELD(u32 hubaddr			: 7,
		__BITFIELD_FIELD(u32 prtaddr			: 7,
		;))))))
	} s;
};

 
union cvmx_usbcx_hctsizx {
	u32 u32;
	 
	struct cvmx_usbcx_hctsizx_s {
		__BITFIELD_FIELD(u32 dopng		: 1,
		__BITFIELD_FIELD(u32 pid		: 2,
		__BITFIELD_FIELD(u32 pktcnt		: 10,
		__BITFIELD_FIELD(u32 xfersize		: 19,
		;))))
	} s;
};

 
union cvmx_usbcx_hfir {
	u32 u32;
	 
	struct cvmx_usbcx_hfir_s {
		__BITFIELD_FIELD(u32 reserved_16_31		: 16,
		__BITFIELD_FIELD(u32 frint			: 16,
		;))
	} s;
};

 
union cvmx_usbcx_hfnum {
	u32 u32;
	 
	struct cvmx_usbcx_hfnum_s {
		__BITFIELD_FIELD(u32 frrem		: 16,
		__BITFIELD_FIELD(u32 frnum		: 16,
		;))
	} s;
};

 
union cvmx_usbcx_hprt {
	u32 u32;
	 
	struct cvmx_usbcx_hprt_s {
		__BITFIELD_FIELD(u32 reserved_19_31	: 13,
		__BITFIELD_FIELD(u32 prtspd		: 2,
		__BITFIELD_FIELD(u32 prttstctl		: 4,
		__BITFIELD_FIELD(u32 prtpwr		: 1,
		__BITFIELD_FIELD(u32 prtlnsts		: 2,
		__BITFIELD_FIELD(u32 reserved_9_9	: 1,
		__BITFIELD_FIELD(u32 prtrst		: 1,
		__BITFIELD_FIELD(u32 prtsusp		: 1,
		__BITFIELD_FIELD(u32 prtres		: 1,
		__BITFIELD_FIELD(u32 prtovrcurrchng	: 1,
		__BITFIELD_FIELD(u32 prtovrcurract	: 1,
		__BITFIELD_FIELD(u32 prtenchng		: 1,
		__BITFIELD_FIELD(u32 prtena		: 1,
		__BITFIELD_FIELD(u32 prtconndet		: 1,
		__BITFIELD_FIELD(u32 prtconnsts		: 1,
		;)))))))))))))))
	} s;
};

 
union cvmx_usbcx_hptxfsiz {
	u32 u32;
	 
	struct cvmx_usbcx_hptxfsiz_s {
		__BITFIELD_FIELD(u32 ptxfsize	: 16,
		__BITFIELD_FIELD(u32 ptxfstaddr	: 16,
		;))
	} s;
};

 
union cvmx_usbcx_hptxsts {
	u32 u32;
	 
	struct cvmx_usbcx_hptxsts_s {
		__BITFIELD_FIELD(u32 ptxqtop		: 8,
		__BITFIELD_FIELD(u32 ptxqspcavail	: 8,
		__BITFIELD_FIELD(u32 ptxfspcavail	: 16,
		;)))
	} s;
};

 
union cvmx_usbnx_clk_ctl {
	u64 u64;
	 
	struct cvmx_usbnx_clk_ctl_s {
		__BITFIELD_FIELD(u64 reserved_20_63	: 44,
		__BITFIELD_FIELD(u64 divide2		: 2,
		__BITFIELD_FIELD(u64 hclk_rst		: 1,
		__BITFIELD_FIELD(u64 p_x_on		: 1,
		__BITFIELD_FIELD(u64 p_rtype		: 2,
		__BITFIELD_FIELD(u64 p_com_on		: 1,
		__BITFIELD_FIELD(u64 p_c_sel		: 2,
		__BITFIELD_FIELD(u64 cdiv_byp		: 1,
		__BITFIELD_FIELD(u64 sd_mode		: 2,
		__BITFIELD_FIELD(u64 s_bist		: 1,
		__BITFIELD_FIELD(u64 por		: 1,
		__BITFIELD_FIELD(u64 enable		: 1,
		__BITFIELD_FIELD(u64 prst		: 1,
		__BITFIELD_FIELD(u64 hrst		: 1,
		__BITFIELD_FIELD(u64 divide		: 3,
		;)))))))))))))))
	} s;
};

 
union cvmx_usbnx_usbp_ctl_status {
	u64 u64;
	 
	struct cvmx_usbnx_usbp_ctl_status_s {
		__BITFIELD_FIELD(u64 txrisetune		: 1,
		__BITFIELD_FIELD(u64 txvreftune		: 4,
		__BITFIELD_FIELD(u64 txfslstune		: 4,
		__BITFIELD_FIELD(u64 txhsxvtune		: 2,
		__BITFIELD_FIELD(u64 sqrxtune		: 3,
		__BITFIELD_FIELD(u64 compdistune	: 3,
		__BITFIELD_FIELD(u64 otgtune		: 3,
		__BITFIELD_FIELD(u64 otgdisable		: 1,
		__BITFIELD_FIELD(u64 portreset		: 1,
		__BITFIELD_FIELD(u64 drvvbus		: 1,
		__BITFIELD_FIELD(u64 lsbist		: 1,
		__BITFIELD_FIELD(u64 fsbist		: 1,
		__BITFIELD_FIELD(u64 hsbist		: 1,
		__BITFIELD_FIELD(u64 bist_done		: 1,
		__BITFIELD_FIELD(u64 bist_err		: 1,
		__BITFIELD_FIELD(u64 tdata_out		: 4,
		__BITFIELD_FIELD(u64 siddq		: 1,
		__BITFIELD_FIELD(u64 txpreemphasistune	: 1,
		__BITFIELD_FIELD(u64 dma_bmode		: 1,
		__BITFIELD_FIELD(u64 usbc_end		: 1,
		__BITFIELD_FIELD(u64 usbp_bist		: 1,
		__BITFIELD_FIELD(u64 tclk		: 1,
		__BITFIELD_FIELD(u64 dp_pulld		: 1,
		__BITFIELD_FIELD(u64 dm_pulld		: 1,
		__BITFIELD_FIELD(u64 hst_mode		: 1,
		__BITFIELD_FIELD(u64 tuning		: 4,
		__BITFIELD_FIELD(u64 tx_bs_enh		: 1,
		__BITFIELD_FIELD(u64 tx_bs_en		: 1,
		__BITFIELD_FIELD(u64 loop_enb		: 1,
		__BITFIELD_FIELD(u64 vtest_enb		: 1,
		__BITFIELD_FIELD(u64 bist_enb		: 1,
		__BITFIELD_FIELD(u64 tdata_sel		: 1,
		__BITFIELD_FIELD(u64 taddr_in		: 4,
		__BITFIELD_FIELD(u64 tdata_in		: 8,
		__BITFIELD_FIELD(u64 ate_reset		: 1,
		;)))))))))))))))))))))))))))))))))))
	} s;
};

#endif  
