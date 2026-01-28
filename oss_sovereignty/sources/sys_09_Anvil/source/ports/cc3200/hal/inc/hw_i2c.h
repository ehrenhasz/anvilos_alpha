


































#ifndef __HW_I2C_H__
#define __HW_I2C_H__






#define I2C_O_MSA               0x00000000
#define I2C_O_MCS               0x00000004
#define I2C_O_MDR               0x00000008
#define I2C_O_MTPR              0x0000000C
#define I2C_O_MIMR              0x00000010
#define I2C_O_MRIS              0x00000014
#define I2C_O_MMIS              0x00000018
#define I2C_O_MICR              0x0000001C
#define I2C_O_MCR               0x00000020
#define I2C_O_MCLKOCNT          0x00000024
#define I2C_O_MBMON             0x0000002C
#define I2C_O_MBLEN             0x00000030
#define I2C_O_MBCNT             0x00000034
#define I2C_O_SOAR              0x00000800
#define I2C_O_SCSR              0x00000804
#define I2C_O_SDR               0x00000808
#define I2C_O_SIMR              0x0000080C
#define I2C_O_SRIS              0x00000810
#define I2C_O_SMIS              0x00000814
#define I2C_O_SICR              0x00000818
#define I2C_O_SOAR2             0x0000081C
#define I2C_O_SACKCTL           0x00000820
#define I2C_O_FIFODATA          0x00000F00
#define I2C_O_FIFOCTL           0x00000F04
#define I2C_O_FIFOSTATUS        0x00000F08
#define I2C_O_OBSMUXSEL0        0x00000F80
#define I2C_O_OBSMUXSEL1        0x00000F84
#define I2C_O_MUXROUTE          0x00000F88
#define I2C_O_PV                0x00000FB0
#define I2C_O_PP                0x00000FC0
#define I2C_O_PC                0x00000FC4
#define I2C_O_CC                0x00000FC8








#define I2C_MSA_SA_M          0x000000FE  
#define I2C_MSA_SA_S          1
#define I2C_MSA_RS              0x00000001  





#define I2C_MCS_ACTDMARX        0x80000000  
#define I2C_MCS_ACTDMATX        0x40000000  
#define I2C_MCS_CLKTO           0x00000080  
#define I2C_MCS_BUSBSY          0x00000040  
#define I2C_MCS_IDLE            0x00000020  
#define I2C_MCS_ARBLST          0x00000010  
#define I2C_MCS_ACK             0x00000008  
#define I2C_MCS_ADRACK          0x00000004  
#define I2C_MCS_ERROR           0x00000002  
#define I2C_MCS_BUSY            0x00000001  





#define I2C_MDR_DATA_M        0x000000FF  
#define I2C_MDR_DATA_S        0





#define I2C_MTPR_HS             0x00000080  
#define I2C_MTPR_TPR_M        0x0000007F  
#define I2C_MTPR_TPR_S        0





#define I2C_MIMR_RXFFIM         0x00000800  
#define I2C_MIMR_TXFEIM         0x00000400  
                                            
#define I2C_MIMR_RXIM           0x00000200  
                                            
#define I2C_MIMR_TXIM           0x00000100  
                                            
#define I2C_MIMR_ARBLOSTIM      0x00000080  
#define I2C_MIMR_STOPIM         0x00000040  
#define I2C_MIMR_STARTIM        0x00000020  
#define I2C_MIMR_NACKIM         0x00000010  
#define I2C_MIMR_DMATXIM        0x00000008  
#define I2C_MIMR_DMARXIM        0x00000004  
#define I2C_MIMR_CLKIM          0x00000002  
#define I2C_MIMR_IM             0x00000001  





#define I2C_MRIS_RXFFRIS        0x00000800  
                                            
#define I2C_MRIS_TXFERIS        0x00000400  
                                            
#define I2C_MRIS_RXRIS          0x00000200  
                                            
#define I2C_MRIS_TXRIS          0x00000100  
                                            
#define I2C_MRIS_ARBLOSTRIS     0x00000080  
                                            
#define I2C_MRIS_STOPRIS        0x00000040  
                                            
#define I2C_MRIS_STARTRIS       0x00000020  
                                            
#define I2C_MRIS_NACKRIS        0x00000010  
                                            
#define I2C_MRIS_DMATXRIS       0x00000008  
                                            
#define I2C_MRIS_DMARXRIS       0x00000004  
#define I2C_MRIS_CLKRIS         0x00000002  
                                            
#define I2C_MRIS_RIS            0x00000001  





#define I2C_MMIS_RXFFMIS        0x00000800  
#define I2C_MMIS_TXFEMIS        0x00000400  
                                            
#define I2C_MMIS_RXMIS          0x00000200  
                                            
#define I2C_MMIS_TXMIS          0x00000100  
#define I2C_MMIS_ARBLOSTMIS     0x00000080  
#define I2C_MMIS_STOPMIS        0x00000040  
#define I2C_MMIS_STARTMIS       0x00000020  
#define I2C_MMIS_NACKMIS        0x00000010  
#define I2C_MMIS_DMATXMIS       0x00000008  
#define I2C_MMIS_DMARXMIS       0x00000004  
#define I2C_MMIS_CLKMIS         0x00000002  
                                            
#define I2C_MMIS_MIS            0x00000001  





#define I2C_MICR_RXFFIC         0x00000800  
                                            
#define I2C_MICR_TXFEIC         0x00000400  
                                            
#define I2C_MICR_RXIC           0x00000200  
                                            
#define I2C_MICR_TXIC           0x00000100  
                                            
#define I2C_MICR_ARBLOSTIC      0x00000080  
#define I2C_MICR_STOPIC         0x00000040  
#define I2C_MICR_STARTIC        0x00000020  
#define I2C_MICR_NACKIC         0x00000010  
                                            
#define I2C_MICR_DMATXIC        0x00000008  
#define I2C_MICR_DMARXIC        0x00000004  
#define I2C_MICR_CLKIC          0x00000002  
#define I2C_MICR_IC             0x00000001  





#define I2C_MCR_MMD             0x00000040  
#define I2C_MCR_SFE             0x00000020  
#define I2C_MCR_MFE             0x00000010  
#define I2C_MCR_LPBK            0x00000001  





#define I2C_MCLKOCNT_CNTL_M   0x000000FF  
#define I2C_MCLKOCNT_CNTL_S   0





#define I2C_MBMON_SDA           0x00000002  
#define I2C_MBMON_SCL           0x00000001  





#define I2C_MBLEN_CNTL_M      0x000000FF  
#define I2C_MBLEN_CNTL_S      0





#define I2C_MBCNT_CNTL_M      0x000000FF  
#define I2C_MBCNT_CNTL_S      0





#define I2C_SOAR_OAR_M        0x0000007F  
#define I2C_SOAR_OAR_S        0





#define I2C_SCSR_ACTDMARX       0x80000000  
#define I2C_SCSR_ACTDMATX       0x40000000  
#define I2C_SCSR_QCMDRW         0x00000020  
#define I2C_SCSR_QCMDST         0x00000010  
#define I2C_SCSR_OAR2SEL        0x00000008  
#define I2C_SCSR_FBR            0x00000004  
#define I2C_SCSR_TREQ           0x00000002  
#define I2C_SCSR_DA             0x00000001  





#define I2C_SDR_DATA_M        0x000000FF  
#define I2C_SDR_DATA_S        0





#define I2C_SIMR_IM             0x00000100  
#define I2C_SIMR_TXFEIM         0x00000080  
                                            
#define I2C_SIMR_RXIM           0x00000040  
                                            
#define I2C_SIMR_TXIM           0x00000020  
                                            
#define I2C_SIMR_DMATXIM        0x00000010  
#define I2C_SIMR_DMARXIM        0x00000008  
#define I2C_SIMR_STOPIM         0x00000004  
#define I2C_SIMR_STARTIM        0x00000002  
#define I2C_SIMR_DATAIM         0x00000001  





#define I2C_SRIS_RIS            0x00000100  
#define I2C_SRIS_TXFERIS        0x00000080  
                                            
#define I2C_SRIS_RXRIS          0x00000040  
                                            
#define I2C_SRIS_TXRIS          0x00000020  
                                            
#define I2C_SRIS_DMATXRIS       0x00000010  
                                            
#define I2C_SRIS_DMARXRIS       0x00000008  
#define I2C_SRIS_STOPRIS        0x00000004  
                                            
#define I2C_SRIS_STARTRIS       0x00000002  
                                            
#define I2C_SRIS_DATARIS        0x00000001  





#define I2C_SMIS_RXFFMIS        0x00000100  
#define I2C_SMIS_TXFEMIS        0x00000080  
                                            
#define I2C_SMIS_RXMIS          0x00000040  
                                            
#define I2C_SMIS_TXMIS          0x00000020  
                                            
#define I2C_SMIS_DMATXMIS       0x00000010  
                                            
#define I2C_SMIS_DMARXMIS       0x00000008  
                                            
#define I2C_SMIS_STOPMIS        0x00000004  
                                            
#define I2C_SMIS_STARTMIS       0x00000002  
                                            
#define I2C_SMIS_DATAMIS        0x00000001  





#define I2C_SICR_RXFFIC         0x00000100  
#define I2C_SICR_TXFEIC         0x00000080  
                                            
#define I2C_SICR_RXIC           0x00000040  
#define I2C_SICR_TXIC           0x00000020  
#define I2C_SICR_DMATXIC        0x00000010  
#define I2C_SICR_DMARXIC        0x00000008  
#define I2C_SICR_STOPIC         0x00000004  
#define I2C_SICR_STARTIC        0x00000002  
#define I2C_SICR_DATAIC         0x00000001  





#define I2C_SOAR2_OAR2EN        0x00000080  
#define I2C_SOAR2_OAR2_M      0x0000007F  
#define I2C_SOAR2_OAR2_S      0





#define I2C_SACKCTL_ACKOVAL     0x00000002  
#define I2C_SACKCTL_ACKOEN      0x00000001  





#define I2C_FIFODATA_DATA_M   0x000000FF  
#define I2C_FIFODATA_DATA_S   0





#define I2C_FIFOCTL_RXASGNMT    0x80000000  
#define I2C_FIFOCTL_RXFLUSH     0x40000000  
#define I2C_FIFOCTL_DMARXENA    0x20000000  
#define I2C_FIFOCTL_RXTRIG_M  0x00070000  
#define I2C_FIFOCTL_RXTRIG_S  16
#define I2C_FIFOCTL_TXASGNMT    0x00008000  
#define I2C_FIFOCTL_TXFLUSH     0x00004000  
#define I2C_FIFOCTL_DMATXENA    0x00002000  
#define I2C_FIFOCTL_TXTRIG_M  0x00000007  
#define I2C_FIFOCTL_TXTRIG_S  0





#define I2C_FIFOSTATUS_RXABVTRIG \
                                0x00040000  

#define I2C_FIFOSTATUS_RXFF     0x00020000  
#define I2C_FIFOSTATUS_RXFE     0x00010000  
#define I2C_FIFOSTATUS_TXBLWTRIG \
                                0x00000004  

#define I2C_FIFOSTATUS_TXFF     0x00000002  
#define I2C_FIFOSTATUS_TXFE     0x00000001  





#define I2C_OBSMUXSEL0_LN3_M  0x07000000  
#define I2C_OBSMUXSEL0_LN3_S  24
#define I2C_OBSMUXSEL0_LN2_M  0x00070000  
#define I2C_OBSMUXSEL0_LN2_S  16
#define I2C_OBSMUXSEL0_LN1_M  0x00000700  
#define I2C_OBSMUXSEL0_LN1_S  8
#define I2C_OBSMUXSEL0_LN0_M  0x00000007  
#define I2C_OBSMUXSEL0_LN0_S  0





#define I2C_OBSMUXSEL1_LN7_M  0x07000000  
#define I2C_OBSMUXSEL1_LN7_S  24
#define I2C_OBSMUXSEL1_LN6_M  0x00070000  
#define I2C_OBSMUXSEL1_LN6_S  16
#define I2C_OBSMUXSEL1_LN5_M  0x00000700  
#define I2C_OBSMUXSEL1_LN5_S  8
#define I2C_OBSMUXSEL1_LN4_M  0x00000007  
#define I2C_OBSMUXSEL1_LN4_S  0





#define I2C_MUXROUTE_LN7ROUTE_M \
                                0x70000000  
                                            
                                            

#define I2C_MUXROUTE_LN7ROUTE_S 28
#define I2C_MUXROUTE_LN6ROUTE_M \
                                0x07000000  
                                            
                                            

#define I2C_MUXROUTE_LN6ROUTE_S 24
#define I2C_MUXROUTE_LN5ROUTE_M \
                                0x00700000  
                                            
                                            

#define I2C_MUXROUTE_LN5ROUTE_S 20
#define I2C_MUXROUTE_LN4ROUTE_M \
                                0x00070000  
                                            
                                            

#define I2C_MUXROUTE_LN4ROUTE_S 16
#define I2C_MUXROUTE_LN3ROUTE_M \
                                0x00007000  
                                            
                                            

#define I2C_MUXROUTE_LN3ROUTE_S 12
#define I2C_MUXROUTE_LN2ROUTE_M \
                                0x00000700  
                                            
                                            

#define I2C_MUXROUTE_LN2ROUTE_S 8
#define I2C_MUXROUTE_LN1ROUTE_M \
                                0x00000070  
                                            
                                            

#define I2C_MUXROUTE_LN1ROUTE_S 4
#define I2C_MUXROUTE_LN0ROUTE_M \
                                0x00000007  
                                            
                                            

#define I2C_MUXROUTE_LN0ROUTE_S 0





#define I2C_PV_MAJOR_M        0x0000FF00  
#define I2C_PV_MAJOR_S        8
#define I2C_PV_MINOR_M        0x000000FF  
#define I2C_PV_MINOR_S        0





#define I2C_PP_HS               0x00000001  





#define I2C_PC_HS               0x00000001  








#endif 
