















































#ifndef __HW_TIMER_H__
#define __HW_TIMER_H__






#define TIMER_O_CFG             0x00000000  
#define TIMER_O_TAMR            0x00000004  
#define TIMER_O_TBMR            0x00000008  
#define TIMER_O_CTL             0x0000000C  

#define TIMER_O_SYNC            0x00000010  

#define TIMER_O_IMR             0x00000018  
#define TIMER_O_RIS             0x0000001C  
#define TIMER_O_MIS             0x00000020  
#define TIMER_O_ICR             0x00000024  
#define TIMER_O_TAILR           0x00000028  
#define TIMER_O_TBILR           0x0000002C  
#define TIMER_O_TAMATCHR        0x00000030  
#define TIMER_O_TBMATCHR        0x00000034  
#define TIMER_O_TAPR            0x00000038  
#define TIMER_O_TBPR            0x0000003C  
#define TIMER_O_TAPMR           0x00000040  
#define TIMER_O_TBPMR           0x00000044  
#define TIMER_O_TAR             0x00000048  
#define TIMER_O_TBR             0x0000004C  
#define TIMER_O_TAV             0x00000050  
#define TIMER_O_TBV             0x00000054  
#define TIMER_O_RTCPD           0x00000058  
#define TIMER_O_TAPS            0x0000005C  
#define TIMER_O_TBPS            0x00000060  
#define TIMER_O_TAPV            0x00000064  
#define TIMER_O_TBPV            0x00000068  
#define TIMER_O_DMAEV           0x0000006C  
#define TIMER_O_PP              0x00000FC0  







#define TIMER_CFG_M             0x00000007  
#define TIMER_CFG_32_BIT_TIMER  0x00000000  
#define TIMER_CFG_32_BIT_RTC    0x00000001  
                                            
#define TIMER_CFG_16_BIT        0x00000004  
                                            
                                            







#define TIMER_TAMR_TAPLO        0x00000800  
                                            
#define TIMER_TAMR_TAMRSU       0x00000400  
                                            
#define TIMER_TAMR_TAPWMIE      0x00000200  
                                            
#define TIMER_TAMR_TAILD        0x00000100  

#define TIMER_TAMR_TASNAPS      0x00000080  
#define TIMER_TAMR_TAWOT        0x00000040  
#define TIMER_TAMR_TAMIE        0x00000020  
                                            
#define TIMER_TAMR_TACDIR       0x00000010  
#define TIMER_TAMR_TAAMS        0x00000008  
                                            
#define TIMER_TAMR_TACMR        0x00000004  
#define TIMER_TAMR_TAMR_M       0x00000003  
#define TIMER_TAMR_TAMR_1_SHOT  0x00000001  
#define TIMER_TAMR_TAMR_PERIOD  0x00000002  
#define TIMER_TAMR_TAMR_CAP     0x00000003  







#define TIMER_TBMR_TBPLO        0x00000800  
                                            
#define TIMER_TBMR_TBMRSU       0x00000400  
                                            
#define TIMER_TBMR_TBPWMIE      0x00000200  
                                            
#define TIMER_TBMR_TBILD        0x00000100  

#define TIMER_TBMR_TBSNAPS      0x00000080  
#define TIMER_TBMR_TBWOT        0x00000040  
#define TIMER_TBMR_TBMIE        0x00000020  
                                            
#define TIMER_TBMR_TBCDIR       0x00000010  
#define TIMER_TBMR_TBAMS        0x00000008  
                                            
#define TIMER_TBMR_TBCMR        0x00000004  
#define TIMER_TBMR_TBMR_M       0x00000003  
#define TIMER_TBMR_TBMR_1_SHOT  0x00000001  
#define TIMER_TBMR_TBMR_PERIOD  0x00000002  
#define TIMER_TBMR_TBMR_CAP     0x00000003  






#define TIMER_CTL_TBPWML        0x00004000  
#define TIMER_CTL_TBOTE         0x00002000  
                                            
#define TIMER_CTL_TBEVENT_M     0x00000C00  
#define TIMER_CTL_TBEVENT_POS   0x00000000  
#define TIMER_CTL_TBEVENT_NEG   0x00000400  
#define TIMER_CTL_TBEVENT_BOTH  0x00000C00  
#define TIMER_CTL_TBSTALL       0x00000200  
#define TIMER_CTL_TBEN          0x00000100  
#define TIMER_CTL_TAPWML        0x00000040  
#define TIMER_CTL_TAOTE         0x00000020  
                                            
#define TIMER_CTL_RTCEN         0x00000010  
#define TIMER_CTL_TAEVENT_M     0x0000000C  
#define TIMER_CTL_TAEVENT_POS   0x00000000  
#define TIMER_CTL_TAEVENT_NEG   0x00000004  
#define TIMER_CTL_TAEVENT_BOTH  0x0000000C  
#define TIMER_CTL_TASTALL       0x00000002  
#define TIMER_CTL_TAEN          0x00000001  







#define TIMER_SYNC_SYNC11_M     0x00C00000  
#define TIMER_SYNC_SYNC11_TA    0x00400000  
                                            
#define TIMER_SYNC_SYNC11_TB    0x00800000  
                                            
#define TIMER_SYNC_SYNC11_TATB  0x00C00000  
                                            
                                            
#define TIMER_SYNC_SYNC10_M     0x00300000  
#define TIMER_SYNC_SYNC10_TA    0x00100000  
                                            
#define TIMER_SYNC_SYNC10_TB    0x00200000  
                                            
#define TIMER_SYNC_SYNC10_TATB  0x00300000  
                                            
                                            
#define TIMER_SYNC_SYNC9_M      0x000C0000  
#define TIMER_SYNC_SYNC9_TA     0x00040000  
                                            
#define TIMER_SYNC_SYNC9_TB     0x00080000  
                                            
#define TIMER_SYNC_SYNC9_TATB   0x000C0000  
                                            
                                            
#define TIMER_SYNC_SYNC8_M      0x00030000  
#define TIMER_SYNC_SYNC8_TA     0x00010000  
                                            
#define TIMER_SYNC_SYNC8_TB     0x00020000  
                                            
#define TIMER_SYNC_SYNC8_TATB   0x00030000  
                                            
                                            
#define TIMER_SYNC_SYNC7_M      0x0000C000  
#define TIMER_SYNC_SYNC7_TA     0x00004000  
                                            
#define TIMER_SYNC_SYNC7_TB     0x00008000  
                                            
#define TIMER_SYNC_SYNC7_TATB   0x0000C000  
                                            
                                            
#define TIMER_SYNC_SYNC6_M      0x00003000  
#define TIMER_SYNC_SYNC6_TA     0x00001000  
                                            
#define TIMER_SYNC_SYNC6_TB     0x00002000  
                                            
#define TIMER_SYNC_SYNC6_TATB   0x00003000  
                                            
                                            
#define TIMER_SYNC_SYNC5_M      0x00000C00  
#define TIMER_SYNC_SYNC5_TA     0x00000400  
                                            
#define TIMER_SYNC_SYNC5_TB     0x00000800  
                                            
#define TIMER_SYNC_SYNC5_TATB   0x00000C00  
                                            
                                            
#define TIMER_SYNC_SYNC4_M      0x00000300  
#define TIMER_SYNC_SYNC4_TA     0x00000100  
                                            
#define TIMER_SYNC_SYNC4_TB     0x00000200  
                                            
#define TIMER_SYNC_SYNC4_TATB   0x00000300  
                                            
                                            
#define TIMER_SYNC_SYNC3_M      0x000000C0  
#define TIMER_SYNC_SYNC3_TA     0x00000040  
                                            
#define TIMER_SYNC_SYNC3_TB     0x00000080  
                                            
#define TIMER_SYNC_SYNC3_TATB   0x000000C0  
                                            
                                            
#define TIMER_SYNC_SYNC2_M      0x00000030  
#define TIMER_SYNC_SYNC2_TA     0x00000010  
                                            
#define TIMER_SYNC_SYNC2_TB     0x00000020  
                                            
#define TIMER_SYNC_SYNC2_TATB   0x00000030  
                                            
                                            
#define TIMER_SYNC_SYNC1_M      0x0000000C  
#define TIMER_SYNC_SYNC1_TA     0x00000004  
                                            
#define TIMER_SYNC_SYNC1_TB     0x00000008  
                                            
#define TIMER_SYNC_SYNC1_TATB   0x0000000C  
                                            
                                            
#define TIMER_SYNC_SYNC0_M      0x00000003  
#define TIMER_SYNC_SYNC0_TA     0x00000001  
                                            
#define TIMER_SYNC_SYNC0_TB     0x00000002  
                                            
#define TIMER_SYNC_SYNC0_TATB   0x00000003  
                                            
                                            








#define TIMER_IMR_WUEIM         0x00010000  
                                            

#define TIMER_IMR_TBMIM         0x00000800  
                                            
#define TIMER_IMR_CBEIM         0x00000400  
                                            
#define TIMER_IMR_CBMIM         0x00000200  
                                            
#define TIMER_IMR_TBTOIM        0x00000100  
                                            
#define TIMER_IMR_TAMIM         0x00000010  
                                            
#define TIMER_IMR_RTCIM         0x00000008  
#define TIMER_IMR_CAEIM         0x00000004  
                                            
#define TIMER_IMR_CAMIM         0x00000002  
                                            
#define TIMER_IMR_TATOIM        0x00000001  
                                            







#define TIMER_RIS_WUERIS        0x00010000  
                                            

#define TIMER_RIS_TBMRIS        0x00000800  
                                            
#define TIMER_RIS_CBERIS        0x00000400  
                                            
#define TIMER_RIS_CBMRIS        0x00000200  
                                            
#define TIMER_RIS_TBTORIS       0x00000100  
                                            
#define TIMER_RIS_TAMRIS        0x00000010  
                                            
#define TIMER_RIS_RTCRIS        0x00000008  
#define TIMER_RIS_CAERIS        0x00000004  
                                            
#define TIMER_RIS_CAMRIS        0x00000002  
                                            
#define TIMER_RIS_TATORIS       0x00000001  
                                            







#define TIMER_MIS_WUEMIS        0x00010000  
                                            

#define TIMER_MIS_TBMMIS        0x00000800  
                                            
#define TIMER_MIS_CBEMIS        0x00000400  
                                            
#define TIMER_MIS_CBMMIS        0x00000200  
                                            
#define TIMER_MIS_TBTOMIS       0x00000100  
                                            
#define TIMER_MIS_TAMMIS        0x00000010  
                                            
#define TIMER_MIS_RTCMIS        0x00000008  
#define TIMER_MIS_CAEMIS        0x00000004  
                                            
#define TIMER_MIS_CAMMIS        0x00000002  
                                            
#define TIMER_MIS_TATOMIS       0x00000001  
                                            







#define TIMER_ICR_WUECINT       0x00010000  
                                            

#define TIMER_ICR_TBMCINT       0x00000800  
                                            
#define TIMER_ICR_CBECINT       0x00000400  
                                            
#define TIMER_ICR_CBMCINT       0x00000200  
                                            
#define TIMER_ICR_TBTOCINT      0x00000100  
                                            
#define TIMER_ICR_TAMCINT       0x00000010  
                                            
#define TIMER_ICR_RTCCINT       0x00000008  
#define TIMER_ICR_CAECINT       0x00000004  
                                            
#define TIMER_ICR_CAMCINT       0x00000002  
                                            
#define TIMER_ICR_TATOCINT      0x00000001  
                                            







#define TIMER_TAILR_M           0xFFFFFFFF  
                                            

#define TIMER_TAILR_TAILRH_M    0xFFFF0000  
                                            
#define TIMER_TAILR_TAILRL_M    0x0000FFFF  
                                            
#define TIMER_TAILR_TAILRH_S    16
#define TIMER_TAILR_TAILRL_S    0

#define TIMER_TAILR_S           0








#define TIMER_TBILR_M           0xFFFFFFFF  
                                            

#define TIMER_TBILR_TBILRL_M    0x0000FFFF  
                                            
#define TIMER_TBILR_TBILRL_S    0

#define TIMER_TBILR_S           0









#define TIMER_TAMATCHR_TAMR_M   0xFFFFFFFF  

#define TIMER_TAMATCHR_TAMRH_M  0xFFFF0000  
#define TIMER_TAMATCHR_TAMRL_M  0x0000FFFF  
#define TIMER_TAMATCHR_TAMRH_S  16
#define TIMER_TAMATCHR_TAMRL_S  0

#define TIMER_TAMATCHR_TAMR_S   0









#define TIMER_TBMATCHR_TBMR_M   0xFFFFFFFF  

#define TIMER_TBMATCHR_TBMRL_M  0x0000FFFF  

#define TIMER_TBMATCHR_TBMR_S   0

#define TIMER_TBMATCHR_TBMRL_S  0







#define TIMER_TAPR_TAPSRH_M     0x0000FF00  

#define TIMER_TAPR_TAPSR_M      0x000000FF  

#define TIMER_TAPR_TAPSRH_S     8

#define TIMER_TAPR_TAPSR_S      0







#define TIMER_TBPR_TBPSRH_M     0x0000FF00  

#define TIMER_TBPR_TBPSR_M      0x000000FF  

#define TIMER_TBPR_TBPSRH_S     8

#define TIMER_TBPR_TBPSR_S      0







#define TIMER_TAPMR_TAPSMRH_M   0x0000FF00  
                                            

#define TIMER_TAPMR_TAPSMR_M    0x000000FF  

#define TIMER_TAPMR_TAPSMRH_S   8

#define TIMER_TAPMR_TAPSMR_S    0







#define TIMER_TBPMR_TBPSMRH_M   0x0000FF00  
                                            

#define TIMER_TBPMR_TBPSMR_M    0x000000FF  

#define TIMER_TBPMR_TBPSMRH_S   8

#define TIMER_TBPMR_TBPSMR_S    0







#define TIMER_TAR_M             0xFFFFFFFF  

#define TIMER_TAR_TARH_M        0xFFFF0000  
#define TIMER_TAR_TARL_M        0x0000FFFF  
#define TIMER_TAR_TARH_S        16
#define TIMER_TAR_TARL_S        0

#define TIMER_TAR_S             0








#define TIMER_TBR_M             0xFFFFFFFF  

#define TIMER_TBR_TBRL_M        0x00FFFFFF  
#define TIMER_TBR_TBRL_S        0

#define TIMER_TBR_S             0








#define TIMER_TAV_M             0xFFFFFFFF  

#define TIMER_TAV_TAVH_M        0xFFFF0000  
#define TIMER_TAV_TAVL_M        0x0000FFFF  
#define TIMER_TAV_TAVH_S        16
#define TIMER_TAV_TAVL_S        0

#define TIMER_TAV_S             0








#define TIMER_TBV_M             0xFFFFFFFF  

#define TIMER_TBV_TBVL_M        0x0000FFFF  
#define TIMER_TBV_TBVL_S        0

#define TIMER_TBV_S             0






#define TIMER_RTCPD_RTCPD_M     0x0000FFFF  
#define TIMER_RTCPD_RTCPD_S     0






#define TIMER_TAPS_PSS_M        0x0000FFFF  
#define TIMER_TAPS_PSS_S        0






#define TIMER_TBPS_PSS_M        0x0000FFFF  
#define TIMER_TBPS_PSS_S        0






#define TIMER_TAPV_PSV_M        0x0000FFFF  
#define TIMER_TAPV_PSV_S        0






#define TIMER_TBPV_PSV_M        0x0000FFFF  
#define TIMER_TBPV_PSV_S        0






#define TIMER_PP_SYNCCNT        0x00000020  
#define TIMER_PP_CHAIN          0x00000010  
#define TIMER_PP_SIZE_M         0x0000000F  
#define TIMER_PP_SIZE__0        0x00000000  
                                            
                                            
#define TIMER_PP_SIZE__1        0x00000001  
                                            
                                            







#ifndef DEPRECATED







#define TIMER_CFG_CFG_MSK       0x00000007  







#define TIMER_CTL_TBEVENT_MSK   0x00000C00  
#define TIMER_CTL_TAEVENT_MSK   0x0000000C  







#define TIMER_RIS_CBEMIS        0x00000400  
#define TIMER_RIS_CBMMIS        0x00000200  
#define TIMER_RIS_TBTOMIS       0x00000100  
#define TIMER_RIS_RTCMIS        0x00000008  
#define TIMER_RIS_CAEMIS        0x00000004  
#define TIMER_RIS_CAMMIS        0x00000002  
#define TIMER_RIS_TATOMIS       0x00000001  







#define TIMER_TAILR_TAILRH      0xFFFF0000  
#define TIMER_TAILR_TAILRL      0x0000FFFF  







#define TIMER_TBILR_TBILRL      0x0000FFFF  







#define TIMER_TAMATCHR_TAMRH    0xFFFF0000  
#define TIMER_TAMATCHR_TAMRL    0x0000FFFF  







#define TIMER_TBMATCHR_TBMRL    0x0000FFFF  







#define TIMER_TAR_TARH          0xFFFF0000  
#define TIMER_TAR_TARL          0x0000FFFF  







#define TIMER_TBR_TBRL          0x0000FFFF  







#define TIMER_RV_TAILR          0xFFFFFFFF  
#define TIMER_RV_TAR            0xFFFFFFFF  
#define TIMER_RV_TAMATCHR       0xFFFFFFFF  
#define TIMER_RV_TBILR          0x0000FFFF  
#define TIMER_RV_TBMATCHR       0x0000FFFF  
#define TIMER_RV_TBR            0x0000FFFF  
#define TIMER_RV_TAPR           0x00000000  
#define TIMER_RV_CFG            0x00000000  
#define TIMER_RV_TBPMR          0x00000000  
#define TIMER_RV_TAPMR          0x00000000  
#define TIMER_RV_CTL            0x00000000  
#define TIMER_RV_ICR            0x00000000  
#define TIMER_RV_TBMR           0x00000000  
#define TIMER_RV_MIS            0x00000000  
#define TIMER_RV_RIS            0x00000000  
#define TIMER_RV_TBPR           0x00000000  
#define TIMER_RV_IMR            0x00000000  
#define TIMER_RV_TAMR           0x00000000  







#define TIMER_TNMR_TNAMS        0x00000008  
#define TIMER_TNMR_TNCMR        0x00000004  
#define TIMER_TNMR_TNTMR_MSK    0x00000003  
#define TIMER_TNMR_TNTMR_1_SHOT 0x00000001  
#define TIMER_TNMR_TNTMR_PERIOD 0x00000002  
#define TIMER_TNMR_TNTMR_CAP    0x00000003  







#define TIMER_TNPR_TNPSR        0x000000FF  







#define TIMER_TNPMR_TNPSMR      0x000000FF  

#endif

#endif 
