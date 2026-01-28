


#ifndef _IGC_REGS_H_
#define _IGC_REGS_H_


#define IGC_CTRL		0x00000  
#define IGC_STATUS		0x00008  
#define IGC_EECD		0x00010  
#define IGC_CTRL_EXT		0x00018  
#define IGC_MDIC		0x00020  
#define IGC_CONNSW		0x00034  
#define IGC_VET			0x00038  
#define IGC_I225_PHPM		0x00E14  
#define IGC_GPHY_VERSION	0x0001E  


#define IGC_RXPBS		0x02404  
#define IGC_TXPBS		0x03404  


#define IGC_EERD		0x12014  
#define IGC_EEWR		0x12018  


#define IGC_FCAL		0x00028  
#define IGC_FCAH		0x0002C  
#define IGC_FCT			0x00030  
#define IGC_FCTTV		0x00170  
#define IGC_FCRTL		0x02160  
#define IGC_FCRTH		0x02168  
#define IGC_FCRTV		0x02460  


#define IGC_SW_FW_SYNC		0x05B5C  
#define IGC_SWSM		0x05B50  
#define IGC_FWSM		0x05B54  


#define IGC_FACTPS		0x05B30


#define IGC_EICR		0x01580  
#define IGC_EICS		0x01520  
#define IGC_EIMS		0x01524  
#define IGC_EIMC		0x01528  
#define IGC_EIAC		0x0152C  
#define IGC_EIAM		0x01530  
#define IGC_ICR			0x01500  
#define IGC_ICS			0x01504  
#define IGC_IMS			0x01508  
#define IGC_IMC			0x0150C  
#define IGC_IAM			0x01510  

#define IGC_EITR(_n)		(0x01680 + (0x4 * (_n)))

#define IGC_IVAR0		0x01700
#define IGC_IVAR_MISC		0x01740  
#define IGC_GPIE		0x01514  


#define IGC_MRQC		0x05818 


#define IGC_ETQF(_n)		(0x05CB0 + (4 * (_n))) 
#define IGC_FHFT(_n)		(0x09000 + (256 * (_n))) 
#define IGC_FHFT_EXT(_n)	(0x09A00 + (256 * (_n))) 
#define IGC_FHFTSL		0x05804 


#define IGC_ETQF_FILTER_ENABLE	BIT(26)
#define IGC_ETQF_QUEUE_ENABLE	BIT(31)
#define IGC_ETQF_QUEUE_SHIFT	16
#define IGC_ETQF_QUEUE_MASK	0x00070000
#define IGC_ETQF_ETYPE_MASK	0x0000FFFF


#define IGC_FHFT_LENGTH_MASK	GENMASK(7, 0)
#define IGC_FHFT_QUEUE_SHIFT	8
#define IGC_FHFT_QUEUE_MASK	GENMASK(10, 8)
#define IGC_FHFT_PRIO_SHIFT	16
#define IGC_FHFT_PRIO_MASK	GENMASK(18, 16)
#define IGC_FHFT_IMM_INT	BIT(24)
#define IGC_FHFT_DROP		BIT(25)


#define IGC_FHFTSL_FTSL_SHIFT	0
#define IGC_FHFTSL_FTSL_MASK	GENMASK(1, 0)


#define IGC_RETA(_i)		(0x05C00 + ((_i) * 4))

#define IGC_RSSRK(_i)		(0x05C80 + ((_i) * 4))


#define IGC_RCTL		0x00100  
#define IGC_SRRCTL(_n)		(0x0C00C + ((_n) * 0x40))
#define IGC_PSRTYPE(_i)		(0x05480 + ((_i) * 4))
#define IGC_RDBAL(_n)		(0x0C000 + ((_n) * 0x40))
#define IGC_RDBAH(_n)		(0x0C004 + ((_n) * 0x40))
#define IGC_RDLEN(_n)		(0x0C008 + ((_n) * 0x40))
#define IGC_RDH(_n)		(0x0C010 + ((_n) * 0x40))
#define IGC_RDT(_n)		(0x0C018 + ((_n) * 0x40))
#define IGC_RXDCTL(_n)		(0x0C028 + ((_n) * 0x40))
#define IGC_RQDPC(_n)		(0x0C030 + ((_n) * 0x40))
#define IGC_RXCSUM		0x05000  
#define IGC_RLPML		0x05004  
#define IGC_RFCTL		0x05008  
#define IGC_MTA			0x05200  
#define IGC_RA			0x05400  
#define IGC_UTA			0x0A000  
#define IGC_RAL(_n)		(0x05400 + ((_n) * 0x08))
#define IGC_RAH(_n)		(0x05404 + ((_n) * 0x08))
#define IGC_VLANPQF		0x055B0  


#define IGC_TCTL		0x00400  
#define IGC_TIPG		0x00410  
#define IGC_TDBAL(_n)		(0x0E000 + ((_n) * 0x40))
#define IGC_TDBAH(_n)		(0x0E004 + ((_n) * 0x40))
#define IGC_TDLEN(_n)		(0x0E008 + ((_n) * 0x40))
#define IGC_TDH(_n)		(0x0E010 + ((_n) * 0x40))
#define IGC_TDT(_n)		(0x0E018 + ((_n) * 0x40))
#define IGC_TXDCTL(_n)		(0x0E028 + ((_n) * 0x40))


#define IGC_MMDAC		13 
#define IGC_MMDAAD		14 


#define IGC_CRCERRS	0x04000  
#define IGC_ALGNERRC	0x04004  
#define IGC_RXERRC	0x0400C  
#define IGC_MPC		0x04010  
#define IGC_SCC		0x04014  
#define IGC_ECOL	0x04018  
#define IGC_MCC		0x0401C  
#define IGC_LATECOL	0x04020  
#define IGC_COLC	0x04028  
#define IGC_RERC	0x0402C  
#define IGC_DC		0x04030  
#define IGC_TNCRS	0x04034  
#define IGC_HTDPMC	0x0403C  
#define IGC_RLEC	0x04040  
#define IGC_XONRXC	0x04048  
#define IGC_XONTXC	0x0404C  
#define IGC_XOFFRXC	0x04050  
#define IGC_XOFFTXC	0x04054  
#define IGC_FCRUC	0x04058  
#define IGC_PRC64	0x0405C  
#define IGC_PRC127	0x04060  
#define IGC_PRC255	0x04064  
#define IGC_PRC511	0x04068  
#define IGC_PRC1023	0x0406C  
#define IGC_PRC1522	0x04070  
#define IGC_GPRC	0x04074  
#define IGC_BPRC	0x04078  
#define IGC_MPRC	0x0407C  
#define IGC_GPTC	0x04080  
#define IGC_GORCL	0x04088  
#define IGC_GORCH	0x0408C  
#define IGC_GOTCL	0x04090  
#define IGC_GOTCH	0x04094  
#define IGC_RNBC	0x040A0  
#define IGC_RUC		0x040A4  
#define IGC_RFC		0x040A8  
#define IGC_ROC		0x040AC  
#define IGC_RJC		0x040B0  
#define IGC_MGTPRC	0x040B4  
#define IGC_MGTPDC	0x040B8  
#define IGC_MGTPTC	0x040BC  
#define IGC_TORL	0x040C0  
#define IGC_TORH	0x040C4  
#define IGC_TOTL	0x040C8  
#define IGC_TOTH	0x040CC  
#define IGC_TPR		0x040D0  
#define IGC_TPT		0x040D4  
#define IGC_PTC64	0x040D8  
#define IGC_PTC127	0x040DC  
#define IGC_PTC255	0x040E0  
#define IGC_PTC511	0x040E4  
#define IGC_PTC1023	0x040E8  
#define IGC_PTC1522	0x040EC  
#define IGC_MPTC	0x040F0  
#define IGC_BPTC	0x040F4  
#define IGC_TSCTC	0x040F8  
#define IGC_IAC		0x04100  
#define IGC_RPTHC	0x04104  
#define IGC_TLPIC	0x04148  
#define IGC_RLPIC	0x0414C  
#define IGC_HGPTC	0x04118  
#define IGC_RXDMTC	0x04120  
#define IGC_HGORCL	0x04128  
#define IGC_HGORCH	0x0412C  
#define IGC_HGOTCL	0x04130  
#define IGC_HGOTCH	0x04134  
#define IGC_LENERRS	0x04138  


#define IGC_TSICR	0x0B66C  
#define IGC_TSIM	0x0B674  
#define IGC_TSAUXC	0x0B640  
#define IGC_TSYNCRXCTL	0x0B620  
#define IGC_TSYNCTXCTL	0x0B614  
#define IGC_TSYNCRXCFG	0x05F50  
#define IGC_TSSDP	0x0003C  
#define IGC_TRGTTIML0	0x0B644 
#define IGC_TRGTTIMH0	0x0B648 
#define IGC_TRGTTIML1	0x0B64C 
#define IGC_TRGTTIMH1	0x0B650 
#define IGC_FREQOUT0	0x0B654 
#define IGC_FREQOUT1	0x0B658 
#define IGC_AUXSTMPL0	0x0B65C 
#define IGC_AUXSTMPH0	0x0B660 
#define IGC_AUXSTMPL1	0x0B664 
#define IGC_AUXSTMPH1	0x0B668 

#define IGC_IMIR(_i)	(0x05A80 + ((_i) * 4))  
#define IGC_IMIREXT(_i)	(0x05AA0 + ((_i) * 4))  

#define IGC_FTQF(_n)	(0x059E0 + (4 * (_n)))  


#define IGC_TQAVCTRL		0x3570
#define IGC_TXQCTL(_n)		(0x3344 + 0x4 * (_n))
#define IGC_GTXOFFSET		0x3310
#define IGC_BASET_L		0x3314
#define IGC_BASET_H		0x3318
#define IGC_QBVCYCLET		0x331C
#define IGC_QBVCYCLET_S		0x3320

#define IGC_STQT(_n)		(0x3324 + 0x4 * (_n))
#define IGC_ENDQT(_n)		(0x3334 + 0x4 * (_n))
#define IGC_DTXMXPKTSZ		0x355C

#define IGC_TQAVCC(_n)		(0x3004 + ((_n) * 0x40))
#define IGC_TQAVHC(_n)		(0x300C + ((_n) * 0x40))


#define IGC_SYSTIML	0x0B600  
#define IGC_SYSTIMH	0x0B604  
#define IGC_SYSTIMR	0x0B6F8  
#define IGC_TIMINCA	0x0B608  


#define IGC_TXSTMPL_0		0x0B618
#define IGC_TXSTMPL_1		0x0B698
#define IGC_TXSTMPL_2		0x0B6B8
#define IGC_TXSTMPL_3		0x0B6D8


#define IGC_TXSTMPH_0		0x0B61C
#define IGC_TXSTMPH_1		0x0B69C
#define IGC_TXSTMPH_2		0x0B6BC
#define IGC_TXSTMPH_3		0x0B6DC

#define IGC_TXSTMPL	0x0B618  
#define IGC_TXSTMPH	0x0B61C  

#define IGC_TIMADJ	0x0B60C  


#define IGC_PTM_CTRL		0x12540  
#define IGC_PTM_STAT		0x12544  
#define IGC_PTM_CYCLE_CTRL	0x1254C  


#define IGC_PTM_T1_TIM0_L	0x12558  
#define IGC_PTM_T1_TIM0_H	0x1255C  

#define IGC_PTM_CURR_T2_L	0x1258C  
#define IGC_PTM_CURR_T2_H	0x12590  
#define IGC_PTM_PREV_T2_L	0x12584  
#define IGC_PTM_PREV_T2_H	0x12588  
#define IGC_PTM_PREV_T4M1	0x12578  
#define IGC_PTM_CURR_T4M1	0x1257C  
#define IGC_PTM_PREV_T3M2	0x12580  
#define IGC_PTM_TDELAY		0x12594  

#define IGC_PCIE_DIG_DELAY	0x12550  
#define IGC_PCIE_PHY_DELAY	0x12554  


#define IGC_MANC	0x05820  


#define IGC_SRWR	0x12018


#define IGC_WUC		0x05800  
#define IGC_WUFC	0x05808  
#define IGC_WUS		0x05810  
#define IGC_WUPL	0x05900  
#define IGC_WUFC_EXT	0x0580C  


#define IGC_WUPM_REG(_i)	(0x05A00 + ((_i) * 4))


#define IGC_EEER	0x0E30 
#define IGC_IPCNFG	0x0E38 
#define IGC_EEE_SU	0x0E34 


#define IGC_LTRC	0x01A0 
#define IGC_LTRMINV	0x5BB0 
#define IGC_LTRMAXV	0x5BB4 


struct igc_hw;
u32 igc_rd32(struct igc_hw *hw, u32 reg);


#define wr32(reg, val) \
do { \
	u8 __iomem *hw_addr = READ_ONCE((hw)->hw_addr); \
	if (!IGC_REMOVED(hw_addr)) \
		writel((val), &hw_addr[(reg)]); \
} while (0)

#define rd32(reg) (igc_rd32(hw, reg))

#define wrfl() ((void)rd32(IGC_STATUS))

#define array_wr32(reg, offset, value) \
	wr32((reg) + ((offset) << 2), (value))

#define array_rd32(reg, offset) (igc_rd32(hw, (reg) + ((offset) << 2)))

#define IGC_REMOVED(h) unlikely(!(h))

#endif
