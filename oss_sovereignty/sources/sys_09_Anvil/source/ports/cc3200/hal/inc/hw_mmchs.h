


































#ifndef __HW_MMCHS_H__
#define __HW_MMCHS_H__






#define MMCHS_O_HL_REV          0x00000000  
                                            
                                            
#define MMCHS_O_HL_HWINFO       0x00000004  
                                            
                                            
                                            
                                            
                                            
#define MMCHS_O_HL_SYSCONFIG    0x00000010  
#define MMCHS_O_SYSCONFIG       0x00000110  
                                            
                                            
                                            
#define MMCHS_O_SYSSTATUS       0x00000114  
                                            
                                            
                                            
                                            
#define MMCHS_O_CSRE            0x00000124  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_O_SYSTEST         0x00000128  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_O_CON             0x0000012C  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_O_PWCNT           0x00000130  
                                            
                                            
                                            
                                            
                                            
#define MMCHS_O_BLK             0x00000204  
                                            
                                            
                                            
                                            
                                            
#define MMCHS_O_ARG             0x00000208  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_O_CMD             0x0000020C  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_O_RSP10           0x00000210  
                                            
                                            
                                            
                                            
#define MMCHS_O_RSP32           0x00000214  
                                            
                                            
                                            
#define MMCHS_O_RSP54           0x00000218  
                                            
                                            
                                            
#define MMCHS_O_RSP76           0x0000021C  
                                            
                                            
                                            
#define MMCHS_O_DATA            0x00000220  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_O_PSTATE          0x00000224  
                                            
                                            
                                            
#define MMCHS_O_HCTL            0x00000228  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_O_SYSCTL          0x0000022C  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_O_STAT            0x00000230  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_O_IE              0x00000234  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_O_ISE             0x00000238  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_O_AC12            0x0000023C  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_O_CAPA            0x00000240  
                                            
                                            
                                            
#define MMCHS_O_CUR_CAPA        0x00000248  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_O_FE              0x00000250  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_O_ADMAES          0x00000254  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_O_ADMASAL         0x00000258  
#define MMCHS_O_REV             0x000002FC  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            








#define MMCHS_HL_REV_SCHEME_M   0xC0000000
#define MMCHS_HL_REV_SCHEME_S   30
#define MMCHS_HL_REV_FUNC_M     0x0FFF0000  
                                            
                                            
                                            
                                            
                                            
#define MMCHS_HL_REV_FUNC_S     16
#define MMCHS_HL_REV_R_RTL_M    0x0000F800  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_HL_REV_R_RTL_S    11
#define MMCHS_HL_REV_X_MAJOR_M  0x00000700  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_HL_REV_X_MAJOR_S  8
#define MMCHS_HL_REV_CUSTOM_M   0x000000C0
#define MMCHS_HL_REV_CUSTOM_S   6
#define MMCHS_HL_REV_Y_MINOR_M  0x0000003F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_HL_REV_Y_MINOR_S  0





#define MMCHS_HL_HWINFO_RETMODE 0x00000040
#define MMCHS_HL_HWINFO_MEM_SIZE_M \
                                0x0000003C

#define MMCHS_HL_HWINFO_MEM_SIZE_S 2
#define MMCHS_HL_HWINFO_MERGE_MEM \
                                0x00000002

#define MMCHS_HL_HWINFO_MADMA_EN \
                                0x00000001







#define MMCHS_HL_SYSCONFIG_STANDBYMODE_M \
                                0x00000030  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define MMCHS_HL_SYSCONFIG_STANDBYMODE_S 4
#define MMCHS_HL_SYSCONFIG_IDLEMODE_M \
                                0x0000000C  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define MMCHS_HL_SYSCONFIG_IDLEMODE_S 2
#define MMCHS_HL_SYSCONFIG_FREEEMU \
                                0x00000002  
                                            
                                            
                                            
                                            
                                            
                                            

#define MMCHS_HL_SYSCONFIG_SOFTRESET \
                                0x00000001






#define MMCHS_SYSCONFIG_STANDBYMODE_M \
                                0x00003000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define MMCHS_SYSCONFIG_STANDBYMODE_S 12
#define MMCHS_SYSCONFIG_CLOCKACTIVITY_M \
                                0x00000300  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define MMCHS_SYSCONFIG_CLOCKACTIVITY_S 8
#define MMCHS_SYSCONFIG_SIDLEMODE_M \
                                0x00000018  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define MMCHS_SYSCONFIG_SIDLEMODE_S 3
#define MMCHS_SYSCONFIG_ENAWAKEUP \
                                0x00000004  
                                            
                                            

#define MMCHS_SYSCONFIG_SOFTRESET \
                                0x00000002

#define MMCHS_SYSCONFIG_AUTOIDLE \
                                0x00000001  
                                            
                                            
                                            
                                            






#define MMCHS_SYSSTATUS_RESETDONE \
                                0x00000001






#define MMCHS_CSRE_CSRE_M       0xFFFFFFFF  
#define MMCHS_CSRE_CSRE_S       0





#define MMCHS_SYSTEST_OBI       0x00010000
#define MMCHS_SYSTEST_SDCD      0x00008000
#define MMCHS_SYSTEST_SDWP      0x00004000
#define MMCHS_SYSTEST_WAKD      0x00002000
#define MMCHS_SYSTEST_SSB       0x00001000
#define MMCHS_SYSTEST_D7D       0x00000800
#define MMCHS_SYSTEST_D6D       0x00000400
#define MMCHS_SYSTEST_D5D       0x00000200
#define MMCHS_SYSTEST_D4D       0x00000100
#define MMCHS_SYSTEST_D3D       0x00000080
#define MMCHS_SYSTEST_D2D       0x00000040
#define MMCHS_SYSTEST_D1D       0x00000020
#define MMCHS_SYSTEST_D0D       0x00000010
#define MMCHS_SYSTEST_DDIR      0x00000008
#define MMCHS_SYSTEST_CDAT      0x00000004
#define MMCHS_SYSTEST_CDIR      0x00000002
#define MMCHS_SYSTEST_MCKD      0x00000001





#define MMCHS_CON_SDMA_LNE      0x00200000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CON_DMA_MNS       0x00100000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CON_DDR           0x00080000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CON_BOOT_CF0      0x00040000
#define MMCHS_CON_BOOT_ACK      0x00020000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CON_CLKEXTFREE    0x00010000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CON_PADEN         0x00008000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CON_OBIE          0x00004000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CON_OBIP          0x00002000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CON_CEATA         0x00001000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CON_CTPL          0x00000800  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CON_DVAL_M        0x00000600  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CON_DVAL_S        9
#define MMCHS_CON_WPP           0x00000100  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CON_CDP           0x00000080  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CON_MIT           0x00000040  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CON_DW8           0x00000020  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CON_MODE          0x00000010  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CON_STR           0x00000008  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CON_HR            0x00000004  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CON_INIT          0x00000002  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CON_OD            0x00000001  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            





#define MMCHS_PWCNT_PWRCNT_M    0x0000FFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_PWCNT_PWRCNT_S    0





#define MMCHS_BLK_NBLK_M        0xFFFF0000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_BLK_NBLK_S        16
#define MMCHS_BLK_BLEN_M        0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_BLK_BLEN_S        0





#define MMCHS_ARG_ARG_M         0xFFFFFFFF  
#define MMCHS_ARG_ARG_S         0





#define MMCHS_CMD_INDX_M        0x3F000000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CMD_INDX_S        24
#define MMCHS_CMD_CMD_TYPE_M    0x00C00000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CMD_CMD_TYPE_S    22
#define MMCHS_CMD_DP            0x00200000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CMD_CICE          0x00100000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CMD_CCCE          0x00080000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CMD_RSP_TYPE_M    0x00030000  
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CMD_RSP_TYPE_S    16
#define MMCHS_CMD_MSBS          0x00000020  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CMD_DDIR          0x00000010  
                                            
                                            
                                            
                                            
#define MMCHS_CMD_ACEN          0x00000004  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CMD_BCE           0x00000002  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_CMD_DE            0x00000001  
                                            
                                            
                                            





#define MMCHS_RSP10_RSP1_M      0xFFFF0000  
#define MMCHS_RSP10_RSP1_S      16
#define MMCHS_RSP10_RSP0_M      0x0000FFFF  
#define MMCHS_RSP10_RSP0_S      0





#define MMCHS_RSP32_RSP3_M      0xFFFF0000  
#define MMCHS_RSP32_RSP3_S      16
#define MMCHS_RSP32_RSP2_M      0x0000FFFF  
#define MMCHS_RSP32_RSP2_S      0





#define MMCHS_RSP54_RSP5_M      0xFFFF0000  
#define MMCHS_RSP54_RSP5_S      16
#define MMCHS_RSP54_RSP4_M      0x0000FFFF  
#define MMCHS_RSP54_RSP4_S      0





#define MMCHS_RSP76_RSP7_M      0xFFFF0000  
#define MMCHS_RSP76_RSP7_S      16
#define MMCHS_RSP76_RSP6_M      0x0000FFFF  
#define MMCHS_RSP76_RSP6_S      0





#define MMCHS_DATA_DATA_M       0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_DATA_DATA_S       0





#define MMCHS_PSTATE_CLEV       0x01000000
#define MMCHS_PSTATE_DLEV_M     0x00F00000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_PSTATE_DLEV_S     20
#define MMCHS_PSTATE_WP         0x00080000
#define MMCHS_PSTATE_CDPL       0x00040000
#define MMCHS_PSTATE_CSS        0x00020000
#define MMCHS_PSTATE_CINS       0x00010000
#define MMCHS_PSTATE_BRE        0x00000800
#define MMCHS_PSTATE_BWE        0x00000400
#define MMCHS_PSTATE_RTA        0x00000200
#define MMCHS_PSTATE_WTA        0x00000100
#define MMCHS_PSTATE_DLA        0x00000004
#define MMCHS_PSTATE_DATI       0x00000002
#define MMCHS_PSTATE_CMDI       0x00000001





#define MMCHS_HCTL_OBWE         0x08000000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_HCTL_REM          0x04000000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_HCTL_INS          0x02000000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_HCTL_IWE          0x01000000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_HCTL_IBG          0x00080000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_HCTL_RWC          0x00040000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_HCTL_CR           0x00020000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_HCTL_SBGR         0x00010000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_HCTL_SDVS_M       0x00000E00  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_HCTL_SDVS_S       9
#define MMCHS_HCTL_SDBP         0x00000100  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_HCTL_CDSS         0x00000080  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_HCTL_CDTL         0x00000040  
                                            
                                            
                                            
                                            
#define MMCHS_HCTL_DMAS_M       0x00000018  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_HCTL_DMAS_S       3
#define MMCHS_HCTL_HSPE         0x00000004  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_HCTL_DTW          0x00000002  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            





#define MMCHS_SYSCTL_SRD        0x04000000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_SYSCTL_SRC        0x02000000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_SYSCTL_SRA        0x01000000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_SYSCTL_DTO_M      0x000F0000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_SYSCTL_DTO_S      16
#define MMCHS_SYSCTL_CLKD_M     0x0000FFC0  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_SYSCTL_CLKD_S     6
#define MMCHS_SYSCTL_CEN        0x00000004  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_SYSCTL_ICS        0x00000002
#define MMCHS_SYSCTL_ICE        0x00000001  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            





#define MMCHS_STAT_BADA         0x20000000
#define MMCHS_STAT_CERR         0x10000000
#define MMCHS_STAT_ADMAE        0x02000000
#define MMCHS_STAT_ACE          0x01000000
#define MMCHS_STAT_DEB          0x00400000
#define MMCHS_STAT_DCRC         0x00200000
#define MMCHS_STAT_DTO          0x00100000
#define MMCHS_STAT_CIE          0x00080000
#define MMCHS_STAT_CEB          0x00040000
#define MMCHS_STAT_CCRC         0x00020000
#define MMCHS_STAT_CTO          0x00010000
#define MMCHS_STAT_ERRI         0x00008000
#define MMCHS_STAT_BSR          0x00000400
#define MMCHS_STAT_OBI          0x00000200
#define MMCHS_STAT_CIRQ         0x00000100
#define MMCHS_STAT_CREM         0x00000080
#define MMCHS_STAT_CINS         0x00000040
#define MMCHS_STAT_BRR          0x00000020
#define MMCHS_STAT_BWR          0x00000010
#define MMCHS_STAT_DMA          0x00000008
#define MMCHS_STAT_BGE          0x00000004
#define MMCHS_STAT_TC           0x00000002
#define MMCHS_STAT_CC           0x00000001





#define MMCHS_IE_BADA_ENABLE    0x20000000  
                                            
                                            
#define MMCHS_IE_CERR_ENABLE    0x10000000  
                                            
#define MMCHS_IE_ADMAE_ENABLE   0x02000000  
                                            
#define MMCHS_IE_ACE_ENABLE     0x01000000  
                                            
#define MMCHS_IE_DEB_ENABLE     0x00400000  
                                            
#define MMCHS_IE_DCRC_ENABLE    0x00200000  
                                            
#define MMCHS_IE_DTO_ENABLE     0x00100000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_IE_CIE_ENABLE     0x00080000  
                                            
#define MMCHS_IE_CEB_ENABLE     0x00040000  
                                            
#define MMCHS_IE_CCRC_ENABLE    0x00020000  
                                            
#define MMCHS_IE_CTO_ENABLE     0x00010000  
                                            
#define MMCHS_IE_NULL           0x00008000  
                                            
                                            
                                            
                                            
#define MMCHS_IE_BSR_ENABLE     0x00000400  
                                            
                                            
                                            
#define MMCHS_IE_OBI_ENABLE     0x00000200  
                                            
                                            
                                            
#define MMCHS_IE_CIRQ_ENABLE    0x00000100  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_IE_CREM_ENABLE    0x00000080  
                                            
#define MMCHS_IE_CINS_ENABLE    0x00000040  
                                            
#define MMCHS_IE_BRR_ENABLE     0x00000020  
                                            
#define MMCHS_IE_BWR_ENABLE     0x00000010  
                                            
#define MMCHS_IE_DMA_ENABLE     0x00000008  
                                            
#define MMCHS_IE_BGE_ENABLE     0x00000004  
                                            
#define MMCHS_IE_TC_ENABLE      0x00000002  
                                            
#define MMCHS_IE_CC_ENABLE      0x00000001  
                                            





#define MMCHS_ISE_BADA_SIGEN    0x20000000  
                                            
#define MMCHS_ISE_CERR_SIGEN    0x10000000  
                                            
#define MMCHS_ISE_ADMAE_SIGEN   0x02000000  
                                            
#define MMCHS_ISE_ACE_SIGEN     0x01000000  
                                            
#define MMCHS_ISE_DEB_SIGEN     0x00400000  
                                            
#define MMCHS_ISE_DCRC_SIGEN    0x00200000  
                                            
#define MMCHS_ISE_DTO_SIGEN     0x00100000  
                                            
#define MMCHS_ISE_CIE_SIGEN     0x00080000  
                                            
#define MMCHS_ISE_CEB_SIGEN     0x00040000  
                                            
#define MMCHS_ISE_CCRC_SIGEN    0x00020000  
                                            
#define MMCHS_ISE_CTO_SIGEN     0x00010000  
                                            
#define MMCHS_ISE_NULL          0x00008000  
                                            
                                            
                                            
                                            
#define MMCHS_ISE_BSR_SIGEN     0x00000400  
                                            
                                            
                                            
                                            
#define MMCHS_ISE_OBI_SIGEN     0x00000200  
                                            
                                            
                                            
                                            
#define MMCHS_ISE_CIRQ_SIGEN    0x00000100  
                                            
#define MMCHS_ISE_CREM_SIGEN    0x00000080  
                                            
#define MMCHS_ISE_CINS_SIGEN    0x00000040  
                                            
#define MMCHS_ISE_BRR_SIGEN     0x00000020  
                                            
#define MMCHS_ISE_BWR_SIGEN     0x00000010  
                                            
#define MMCHS_ISE_DMA_SIGEN     0x00000008  
                                            
#define MMCHS_ISE_BGE_SIGEN     0x00000004  
                                            
#define MMCHS_ISE_TC_SIGEN      0x00000002  
                                            
#define MMCHS_ISE_CC_SIGEN      0x00000001  
                                            





#define MMCHS_AC12_CNI          0x00000080
#define MMCHS_AC12_ACIE         0x00000010
#define MMCHS_AC12_ACEB         0x00000008
#define MMCHS_AC12_ACCE         0x00000004
#define MMCHS_AC12_ACTO         0x00000002
#define MMCHS_AC12_ACNE         0x00000001





#define MMCHS_CAPA_BIT64        0x10000000
#define MMCHS_CAPA_VS18         0x04000000
#define MMCHS_CAPA_VS30         0x02000000
#define MMCHS_CAPA_VS33         0x01000000
#define MMCHS_CAPA_SRS          0x00800000
#define MMCHS_CAPA_DS           0x00400000
#define MMCHS_CAPA_HSS          0x00200000
#define MMCHS_CAPA_AD2S         0x00080000
#define MMCHS_CAPA_MBL_M        0x00030000
#define MMCHS_CAPA_MBL_S        16
#define MMCHS_CAPA_BCF_M        0x00003F00
#define MMCHS_CAPA_BCF_S        8
#define MMCHS_CAPA_TCU          0x00000080
#define MMCHS_CAPA_TCF_M        0x0000003F
#define MMCHS_CAPA_TCF_S        0





#define MMCHS_CUR_CAPA_CUR_1V8_M \
                                0x00FF0000

#define MMCHS_CUR_CAPA_CUR_1V8_S 16
#define MMCHS_CUR_CAPA_CUR_3V0_M \
                                0x0000FF00

#define MMCHS_CUR_CAPA_CUR_3V0_S 8
#define MMCHS_CUR_CAPA_CUR_3V3_M \
                                0x000000FF

#define MMCHS_CUR_CAPA_CUR_3V3_S 0





#define MMCHS_FE_FE_BADA        0x20000000
#define MMCHS_FE_FE_CERR        0x10000000
#define MMCHS_FE_FE_ADMAE       0x02000000
#define MMCHS_FE_FE_ACE         0x01000000
#define MMCHS_FE_FE_DEB         0x00400000
#define MMCHS_FE_FE_DCRC        0x00200000
#define MMCHS_FE_FE_DTO         0x00100000
#define MMCHS_FE_FE_CIE         0x00080000
#define MMCHS_FE_FE_CEB         0x00040000
#define MMCHS_FE_FE_CCRC        0x00020000
#define MMCHS_FE_FE_CTO         0x00010000
#define MMCHS_FE_FE_CNI         0x00000080
#define MMCHS_FE_FE_ACIE        0x00000010
#define MMCHS_FE_FE_ACEB        0x00000008
#define MMCHS_FE_FE_ACCE        0x00000004
#define MMCHS_FE_FE_ACTO        0x00000002
#define MMCHS_FE_FE_ACNE        0x00000001





#define MMCHS_ADMAES_LME        0x00000004  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_ADMAES_AES_M      0x00000003  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MMCHS_ADMAES_AES_S      0





#define MMCHS_ADMASAL_ADMA_A32B_M \
                                0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define MMCHS_ADMASAL_ADMA_A32B_S 0





#define MMCHS_REV_VREV_M        0xFF000000  
                                            
                                            
                                            
#define MMCHS_REV_VREV_S        24
#define MMCHS_REV_SREV_M        0x00FF0000
#define MMCHS_REV_SREV_S        16
#define MMCHS_REV_SIS           0x00000001  
                                            
                                            
                                            
                                            
                                            
                                            
                                            



#endif 
