


































#ifndef __HW_MCSPI_H__
#define __HW_MCSPI_H__






#define MCSPI_O_HL_REV          0x00000000  
                                            
                                            
#define MCSPI_O_HL_HWINFO       0x00000004  
                                            
                                            
                                            
                                            
                                            
#define MCSPI_O_HL_SYSCONFIG    0x00000010  
                                            
#define MCSPI_O_REVISION        0x00000100  
                                            
                                            
#define MCSPI_O_SYSCONFIG       0x00000110  
                                            
                                            
                                            
#define MCSPI_O_SYSSTATUS       0x00000114  
                                            
                                            
                                            
                                            
#define MCSPI_O_IRQSTATUS       0x00000118  
                                            
                                            
                                            
                                            
#define MCSPI_O_IRQENABLE       0x0000011C  
                                            
                                            
                                            
                                            
#define MCSPI_O_WAKEUPENABLE    0x00000120  
                                            
                                            
                                            
                                            
#define MCSPI_O_SYST            0x00000124  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_O_MODULCTRL       0x00000128  
                                            
                                            
                                            
#define MCSPI_O_CH0CONF         0x0000012C  
                                            
                                            
#define MCSPI_O_CH0STAT         0x00000130  
                                            
                                            
                                            
#define MCSPI_O_CH0CTRL         0x00000134  
                                            
                                            
#define MCSPI_O_TX0             0x00000138  
                                            
                                            
                                            
                                            
#define MCSPI_O_RX0             0x0000013C  
                                            
                                            
                                            
                                            
#define MCSPI_O_CH1CONF         0x00000140  
                                            
                                            
#define MCSPI_O_CH1STAT         0x00000144  
                                            
                                            
                                            
#define MCSPI_O_CH1CTRL         0x00000148  
                                            
                                            
#define MCSPI_O_TX1             0x0000014C  
                                            
                                            
                                            
                                            
#define MCSPI_O_RX1             0x00000150  
                                            
                                            
                                            
                                            
#define MCSPI_O_CH2CONF         0x00000154  
                                            
                                            
#define MCSPI_O_CH2STAT         0x00000158  
                                            
                                            
                                            
#define MCSPI_O_CH2CTRL         0x0000015C  
                                            
                                            
#define MCSPI_O_TX2             0x00000160  
                                            
                                            
                                            
                                            
#define MCSPI_O_RX2             0x00000164  
                                            
                                            
                                            
                                            
#define MCSPI_O_CH3CONF         0x00000168  
                                            
                                            
#define MCSPI_O_CH3STAT         0x0000016C  
                                            
                                            
                                            
#define MCSPI_O_CH3CTRL         0x00000170  
                                            
                                            
#define MCSPI_O_TX3             0x00000174  
                                            
                                            
                                            
                                            
#define MCSPI_O_RX3             0x00000178  
                                            
                                            
                                            
                                            
#define MCSPI_O_XFERLEVEL       0x0000017C  
                                            
                                            
                                            
#define MCSPI_O_DAFTX           0x00000180  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_O_DAFRX           0x000001A0  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            








#define MCSPI_HL_REV_SCHEME_M   0xC0000000
#define MCSPI_HL_REV_SCHEME_S   30
#define MCSPI_HL_REV_RSVD_M     0x30000000  
                                            
                                            
#define MCSPI_HL_REV_RSVD_S     28
#define MCSPI_HL_REV_FUNC_M     0x0FFF0000  
                                            
                                            
                                            
                                            
                                            
#define MCSPI_HL_REV_FUNC_S     16
#define MCSPI_HL_REV_R_RTL_M    0x0000F800  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_HL_REV_R_RTL_S    11
#define MCSPI_HL_REV_X_MAJOR_M  0x00000700  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_HL_REV_X_MAJOR_S  8
#define MCSPI_HL_REV_CUSTOM_M   0x000000C0
#define MCSPI_HL_REV_CUSTOM_S   6
#define MCSPI_HL_REV_Y_MINOR_M  0x0000003F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_HL_REV_Y_MINOR_S  0





#define MCSPI_HL_HWINFO_RETMODE 0x00000040
#define MCSPI_HL_HWINFO_FFNBYTE_M \
                                0x0000003E

#define MCSPI_HL_HWINFO_FFNBYTE_S 1
#define MCSPI_HL_HWINFO_USEFIFO 0x00000001






#define MCSPI_HL_SYSCONFIG_IDLEMODE_M \
                                0x0000000C  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define MCSPI_HL_SYSCONFIG_IDLEMODE_S 2
#define MCSPI_HL_SYSCONFIG_FREEEMU \
                                0x00000002  
                                            
                                            
                                            
                                            

#define MCSPI_HL_SYSCONFIG_SOFTRESET \
                                0x00000001






#define MCSPI_REVISION_REV_M    0x000000FF  
                                            
                                            
#define MCSPI_REVISION_REV_S    0





#define MCSPI_SYSCONFIG_CLOCKACTIVITY_M \
                                0x00000300  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define MCSPI_SYSCONFIG_CLOCKACTIVITY_S 8
#define MCSPI_SYSCONFIG_SIDLEMODE_M \
                                0x00000018  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define MCSPI_SYSCONFIG_SIDLEMODE_S 3
#define MCSPI_SYSCONFIG_ENAWAKEUP \
                                0x00000004  
                                            
                                            

#define MCSPI_SYSCONFIG_SOFTRESET \
                                0x00000002  
                                            
                                            
                                            
                                            
                                            

#define MCSPI_SYSCONFIG_AUTOIDLE \
                                0x00000001  
                                            
                                            
                                            
                                            
                                            






#define MCSPI_SYSSTATUS_RESETDONE \
                                0x00000001






#define MCSPI_IRQSTATUS_EOW     0x00020000
#define MCSPI_IRQSTATUS_WKS     0x00010000
#define MCSPI_IRQSTATUS_RX3_FULL \
                                0x00004000

#define MCSPI_IRQSTATUS_TX3_UNDERFLOW \
                                0x00002000

#define MCSPI_IRQSTATUS_TX3_EMPTY \
                                0x00001000

#define MCSPI_IRQSTATUS_RX2_FULL \
                                0x00000400

#define MCSPI_IRQSTATUS_TX2_UNDERFLOW \
                                0x00000200

#define MCSPI_IRQSTATUS_TX2_EMPTY \
                                0x00000100

#define MCSPI_IRQSTATUS_RX1_FULL \
                                0x00000040

#define MCSPI_IRQSTATUS_TX1_UNDERFLOW \
                                0x00000020

#define MCSPI_IRQSTATUS_TX1_EMPTY \
                                0x00000010

#define MCSPI_IRQSTATUS_RX0_OVERFLOW \
                                0x00000008

#define MCSPI_IRQSTATUS_RX0_FULL \
                                0x00000004

#define MCSPI_IRQSTATUS_TX0_UNDERFLOW \
                                0x00000002

#define MCSPI_IRQSTATUS_TX0_EMPTY \
                                0x00000001






#define MCSPI_IRQENABLE_EOW_ENABLE \
                                0x00020000  
                                            
                                            

#define MCSPI_IRQENABLE_WKE     0x00010000  
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_IRQENABLE_RX3_FULL_ENABLE \
                                0x00004000  
                                            
                                            

#define MCSPI_IRQENABLE_TX3_UNDERFLOW_ENABLE \
                                0x00002000  
                                            
                                            
                                            

#define MCSPI_IRQENABLE_TX3_EMPTY_ENABLE \
                                0x00001000  
                                            
                                            

#define MCSPI_IRQENABLE_RX2_FULL_ENABLE \
                                0x00000400  
                                            
                                            

#define MCSPI_IRQENABLE_TX2_UNDERFLOW_ENABLE \
                                0x00000200  
                                            
                                            
                                            

#define MCSPI_IRQENABLE_TX2_EMPTY_ENABLE \
                                0x00000100  
                                            
                                            
                                            

#define MCSPI_IRQENABLE_RX1_FULL_ENABLE \
                                0x00000040  
                                            
                                            

#define MCSPI_IRQENABLE_TX1_UNDERFLOW_ENABLE \
                                0x00000020  
                                            
                                            
                                            

#define MCSPI_IRQENABLE_TX1_EMPTY_ENABLE \
                                0x00000010  
                                            
                                            
                                            

#define MCSPI_IRQENABLE_RX0_OVERFLOW_ENABLE \
                                0x00000008  
                                            
                                            
                                            

#define MCSPI_IRQENABLE_RX0_FULL_ENABLE \
                                0x00000004  
                                            
                                            

#define MCSPI_IRQENABLE_TX0_UNDERFLOW_ENABLE \
                                0x00000002  
                                            
                                            
                                            

#define MCSPI_IRQENABLE_TX0_EMPTY_ENABLE \
                                0x00000001  
                                            
                                            
                                            







#define MCSPI_WAKEUPENABLE_WKEN 0x00000001  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            





#define MCSPI_SYST_SSB          0x00000800  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_SYST_SPIENDIR     0x00000400  
                                            
                                            
                                            
#define MCSPI_SYST_SPIDATDIR1   0x00000200  
                                            
#define MCSPI_SYST_SPIDATDIR0   0x00000100  
                                            
#define MCSPI_SYST_WAKD         0x00000080  
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_SYST_SPICLK       0x00000040  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_SYST_SPIDAT_1     0x00000020  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_SYST_SPIDAT_0     0x00000010  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_SYST_SPIEN_3      0x00000008  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_SYST_SPIEN_2      0x00000004  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_SYST_SPIEN_1      0x00000002  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_SYST_SPIEN_0      0x00000001  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            





#define MCSPI_MODULCTRL_FDAA    0x00000100  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_MODULCTRL_MOA     0x00000080  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_MODULCTRL_INITDLY_M \
                                0x00000070  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define MCSPI_MODULCTRL_INITDLY_S 4
#define MCSPI_MODULCTRL_SYSTEM_TEST \
                                0x00000008  
                                            
                                            

#define MCSPI_MODULCTRL_MS      0x00000004  
                                            
                                            
                                            
                                            
#define MCSPI_MODULCTRL_PIN34   0x00000002  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_MODULCTRL_SINGLE  0x00000001  
                                            
                                            
                                            
                                            
                                            
                                            





#define MCSPI_CH0CONF_CLKG      0x20000000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH0CONF_FFER      0x10000000  
                                            
                                            
                                            
                                            
#define MCSPI_CH0CONF_FFEW      0x08000000  
                                            
                                            
                                            
                                            
#define MCSPI_CH0CONF_TCS0_M    0x06000000  
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH0CONF_TCS0_S    25
#define MCSPI_CH0CONF_SBPOL     0x01000000  
                                            
                                            
                                            
#define MCSPI_CH0CONF_SBE       0x00800000  
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH0CONF_SPIENSLV_M \
                                0x00600000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define MCSPI_CH0CONF_SPIENSLV_S 21
#define MCSPI_CH0CONF_FORCE     0x00100000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH0CONF_TURBO     0x00080000  
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH0CONF_IS        0x00040000  
                                            
                                            
                                            
                                            
#define MCSPI_CH0CONF_DPE1      0x00020000  
                                            
                                            
                                            
                                            
#define MCSPI_CH0CONF_DPE0      0x00010000  
                                            
                                            
                                            
                                            
#define MCSPI_CH0CONF_DMAR      0x00008000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH0CONF_DMAW      0x00004000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH0CONF_TRM_M     0x00003000  
                                            
                                            
                                            
#define MCSPI_CH0CONF_TRM_S     12
#define MCSPI_CH0CONF_WL_M      0x00000F80  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH0CONF_WL_S      7
#define MCSPI_CH0CONF_EPOL      0x00000040  
                                            
                                            
                                            
#define MCSPI_CH0CONF_CLKD_M    0x0000003C  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH0CONF_CLKD_S    2
#define MCSPI_CH0CONF_POL       0x00000002  
                                            
                                            
                                            
#define MCSPI_CH0CONF_PHA       0x00000001  
                                            
                                            
                                            





#define MCSPI_CH0STAT_RXFFF     0x00000040
#define MCSPI_CH0STAT_RXFFE     0x00000020
#define MCSPI_CH0STAT_TXFFF     0x00000010
#define MCSPI_CH0STAT_TXFFE     0x00000008
#define MCSPI_CH0STAT_EOT       0x00000004
#define MCSPI_CH0STAT_TXS       0x00000002
#define MCSPI_CH0STAT_RXS       0x00000001





#define MCSPI_CH0CTRL_EXTCLK_M  0x0000FF00  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH0CTRL_EXTCLK_S  8
#define MCSPI_CH0CTRL_EN        0x00000001  
                                            
                                            





#define MCSPI_TX0_TDATA_M       0xFFFFFFFF  
#define MCSPI_TX0_TDATA_S       0





#define MCSPI_RX0_RDATA_M       0xFFFFFFFF  
#define MCSPI_RX0_RDATA_S       0





#define MCSPI_CH1CONF_CLKG      0x20000000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH1CONF_FFER      0x10000000  
                                            
                                            
                                            
                                            
#define MCSPI_CH1CONF_FFEW      0x08000000  
                                            
                                            
                                            
                                            
#define MCSPI_CH1CONF_TCS1_M    0x06000000  
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH1CONF_TCS1_S    25
#define MCSPI_CH1CONF_SBPOL     0x01000000  
                                            
                                            
                                            
#define MCSPI_CH1CONF_SBE       0x00800000  
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH1CONF_FORCE     0x00100000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH1CONF_TURBO     0x00080000  
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH1CONF_IS        0x00040000  
                                            
                                            
                                            
                                            
#define MCSPI_CH1CONF_DPE1      0x00020000  
                                            
                                            
                                            
                                            
#define MCSPI_CH1CONF_DPE0      0x00010000  
                                            
                                            
                                            
                                            
#define MCSPI_CH1CONF_DMAR      0x00008000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH1CONF_DMAW      0x00004000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH1CONF_TRM_M     0x00003000  
                                            
                                            
                                            
#define MCSPI_CH1CONF_TRM_S     12
#define MCSPI_CH1CONF_WL_M      0x00000F80  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH1CONF_WL_S      7
#define MCSPI_CH1CONF_EPOL      0x00000040  
                                            
                                            
                                            
#define MCSPI_CH1CONF_CLKD_M    0x0000003C  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH1CONF_CLKD_S    2
#define MCSPI_CH1CONF_POL       0x00000002  
                                            
                                            
                                            
#define MCSPI_CH1CONF_PHA       0x00000001  
                                            
                                            
                                            





#define MCSPI_CH1STAT_RXFFF     0x00000040
#define MCSPI_CH1STAT_RXFFE     0x00000020
#define MCSPI_CH1STAT_TXFFF     0x00000010
#define MCSPI_CH1STAT_TXFFE     0x00000008
#define MCSPI_CH1STAT_EOT       0x00000004
#define MCSPI_CH1STAT_TXS       0x00000002
#define MCSPI_CH1STAT_RXS       0x00000001





#define MCSPI_CH1CTRL_EXTCLK_M  0x0000FF00  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH1CTRL_EXTCLK_S  8
#define MCSPI_CH1CTRL_EN        0x00000001  
                                            
                                            





#define MCSPI_TX1_TDATA_M       0xFFFFFFFF  
#define MCSPI_TX1_TDATA_S       0





#define MCSPI_RX1_RDATA_M       0xFFFFFFFF  
#define MCSPI_RX1_RDATA_S       0





#define MCSPI_CH2CONF_CLKG      0x20000000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH2CONF_FFER      0x10000000  
                                            
                                            
                                            
                                            
#define MCSPI_CH2CONF_FFEW      0x08000000  
                                            
                                            
                                            
                                            
#define MCSPI_CH2CONF_TCS2_M    0x06000000  
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH2CONF_TCS2_S    25
#define MCSPI_CH2CONF_SBPOL     0x01000000  
                                            
                                            
                                            
#define MCSPI_CH2CONF_SBE       0x00800000  
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH2CONF_FORCE     0x00100000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH2CONF_TURBO     0x00080000  
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH2CONF_IS        0x00040000  
                                            
                                            
                                            
                                            
#define MCSPI_CH2CONF_DPE1      0x00020000  
                                            
                                            
                                            
                                            
#define MCSPI_CH2CONF_DPE0      0x00010000  
                                            
                                            
                                            
                                            
#define MCSPI_CH2CONF_DMAR      0x00008000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH2CONF_DMAW      0x00004000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH2CONF_TRM_M     0x00003000  
                                            
                                            
                                            
#define MCSPI_CH2CONF_TRM_S     12
#define MCSPI_CH2CONF_WL_M      0x00000F80  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH2CONF_WL_S      7
#define MCSPI_CH2CONF_EPOL      0x00000040  
                                            
                                            
                                            
#define MCSPI_CH2CONF_CLKD_M    0x0000003C  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH2CONF_CLKD_S    2
#define MCSPI_CH2CONF_POL       0x00000002  
                                            
                                            
                                            
#define MCSPI_CH2CONF_PHA       0x00000001  
                                            
                                            
                                            





#define MCSPI_CH2STAT_RXFFF     0x00000040
#define MCSPI_CH2STAT_RXFFE     0x00000020
#define MCSPI_CH2STAT_TXFFF     0x00000010
#define MCSPI_CH2STAT_TXFFE     0x00000008
#define MCSPI_CH2STAT_EOT       0x00000004
#define MCSPI_CH2STAT_TXS       0x00000002
#define MCSPI_CH2STAT_RXS       0x00000001





#define MCSPI_CH2CTRL_EXTCLK_M  0x0000FF00  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH2CTRL_EXTCLK_S  8
#define MCSPI_CH2CTRL_EN        0x00000001  
                                            
                                            





#define MCSPI_TX2_TDATA_M       0xFFFFFFFF  
#define MCSPI_TX2_TDATA_S       0





#define MCSPI_RX2_RDATA_M       0xFFFFFFFF  
#define MCSPI_RX2_RDATA_S       0





#define MCSPI_CH3CONF_CLKG      0x20000000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH3CONF_FFER      0x10000000  
                                            
                                            
                                            
                                            
#define MCSPI_CH3CONF_FFEW      0x08000000  
                                            
                                            
                                            
                                            
#define MCSPI_CH3CONF_TCS3_M    0x06000000  
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH3CONF_TCS3_S    25
#define MCSPI_CH3CONF_SBPOL     0x01000000  
                                            
                                            
                                            
#define MCSPI_CH3CONF_SBE       0x00800000  
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH3CONF_FORCE     0x00100000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH3CONF_TURBO     0x00080000  
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH3CONF_IS        0x00040000  
                                            
                                            
                                            
                                            
#define MCSPI_CH3CONF_DPE1      0x00020000  
                                            
                                            
                                            
                                            
#define MCSPI_CH3CONF_DPE0      0x00010000  
                                            
                                            
                                            
                                            
#define MCSPI_CH3CONF_DMAR      0x00008000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH3CONF_DMAW      0x00004000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH3CONF_TRM_M     0x00003000  
                                            
                                            
                                            
#define MCSPI_CH3CONF_TRM_S     12
#define MCSPI_CH3CONF_WL_M      0x00000F80  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH3CONF_WL_S      7
#define MCSPI_CH3CONF_EPOL      0x00000040  
                                            
                                            
                                            
#define MCSPI_CH3CONF_CLKD_M    0x0000003C  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH3CONF_CLKD_S    2
#define MCSPI_CH3CONF_POL       0x00000002  
                                            
                                            
                                            
#define MCSPI_CH3CONF_PHA       0x00000001  
                                            
                                            
                                            





#define MCSPI_CH3STAT_RXFFF     0x00000040
#define MCSPI_CH3STAT_RXFFE     0x00000020
#define MCSPI_CH3STAT_TXFFF     0x00000010
#define MCSPI_CH3STAT_TXFFE     0x00000008
#define MCSPI_CH3STAT_EOT       0x00000004
#define MCSPI_CH3STAT_TXS       0x00000002
#define MCSPI_CH3STAT_RXS       0x00000001





#define MCSPI_CH3CTRL_EXTCLK_M  0x0000FF00  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_CH3CTRL_EXTCLK_S  8
#define MCSPI_CH3CTRL_EN        0x00000001  
                                            
                                            





#define MCSPI_TX3_TDATA_M       0xFFFFFFFF  
#define MCSPI_TX3_TDATA_S       0





#define MCSPI_RX3_RDATA_M       0xFFFFFFFF  
#define MCSPI_RX3_RDATA_S       0





#define MCSPI_XFERLEVEL_WCNT_M  0xFFFF0000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_XFERLEVEL_WCNT_S  16
#define MCSPI_XFERLEVEL_AFL_M   0x0000FF00  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_XFERLEVEL_AFL_S   8
#define MCSPI_XFERLEVEL_AEL_M   0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_XFERLEVEL_AEL_S   0





#define MCSPI_DAFTX_DAFTDATA_M  0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_DAFTX_DAFTDATA_S  0





#define MCSPI_DAFRX_DAFRDATA_M  0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCSPI_DAFRX_DAFRDATA_S  0



#endif 
