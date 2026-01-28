


































#ifndef __HW_GPIO_H__
#define __HW_GPIO_H__






#define GPIO_O_GPIO_DATA        0x00000000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_DIR         0x00000400  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_IS          0x00000404  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_IBE         0x00000408  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_IEV         0x0000040C  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_IM          0x00000410  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_RIS         0x00000414  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_MIS         0x00000418  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_ICR         0x0000041C  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_AFSEL       0x00000420  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_DR2R        0x00000500  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_DR4R        0x00000504  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_DR8R        0x00000508  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_ODR         0x0000050C  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_PUR         0x00000510  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_PDR         0x00000514  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_SLR         0x00000518  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_DEN         0x0000051C  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_LOCK        0x00000520  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_CR          0x00000524  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_AMSEL       0x00000528  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_PCTL        0x0000052C  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_ADCCTL      0x00000530  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_DMACTL      0x00000534  
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_SI          0x00000538  
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_PERIPHID4   0x00000FD0  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_PERIPHID5   0x00000FD4  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_PERIPHID6   0x00000FD8  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_PERIPHID7   0x00000FDC  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_PERIPHID0   0x00000FE0  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_PERIPHID1   0x00000FE4  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_PERIPHID2   0x00000FE8  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_PERIPHID3   0x00000FEC  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_PCELLID0    0x00000FF0  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_PCELLID1    0x00000FF4  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_PCELLID2    0x00000FF8  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_O_GPIO_PCELLID3    0x00000FFC  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            








#define GPIO_GPIO_DATA_DATA_M   0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_DATA_DATA_S   0





#define GPIO_GPIO_DIR_DIR_M     0x000000FF  
                                            
                                            
                                            
#define GPIO_GPIO_DIR_DIR_S     0





#define GPIO_GPIO_IS_IS_M       0x000000FF  
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_IS_IS_S       0





#define GPIO_GPIO_IBE_IBE_M     0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_IBE_IBE_S     0





#define GPIO_GPIO_IEV_IEV_M     0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_IEV_IEV_S     0





#define GPIO_GPIO_IM_IME_M      0x000000FF  
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_IM_IME_S      0





#define GPIO_GPIO_RIS_RIS_M     0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_RIS_RIS_S     0





#define GPIO_GPIO_MIS_MIS_M     0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_MIS_MIS_S     0





#define GPIO_GPIO_ICR_IC_M      0x000000FF  
                                            
                                            
                                            
                                            
#define GPIO_GPIO_ICR_IC_S      0










#define GPIO_GPIO_DR2R_DRV2_M   0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_DR2R_DRV2_S   0





#define GPIO_GPIO_DR4R_DRV4_M   0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_DR4R_DRV4_S   0





#define GPIO_GPIO_DR8R_DRV8_M   0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_DR8R_DRV8_S   0





#define GPIO_GPIO_ODR_ODE_M     0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_ODR_ODE_S     0





#define GPIO_GPIO_PUR_PUE_M     0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_PUR_PUE_S     0





#define GPIO_GPIO_PDR_PDE_M     0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_PDR_PDE_S     0





#define GPIO_GPIO_SLR_SRL_M     0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_SLR_SRL_S     0





#define GPIO_GPIO_DEN_DEN_M     0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_DEN_DEN_S     0





#define GPIO_GPIO_LOCK_LOCK_M   0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_LOCK_LOCK_S   0





#define GPIO_GPIO_CR_CR_M       0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_CR_CR_S       0





#define GPIO_GPIO_AMSEL_GPIO_AMSEL_M \
                                0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPIO_GPIO_AMSEL_GPIO_AMSEL_S 0





#define GPIO_GPIO_PCTL_PMC7_M   0xF0000000  
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_PCTL_PMC7_S   28
#define GPIO_GPIO_PCTL_PMC6_M   0x0F000000  
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_PCTL_PMC6_S   24
#define GPIO_GPIO_PCTL_PMC5_M   0x00F00000  
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_PCTL_PMC5_S   20
#define GPIO_GPIO_PCTL_PMC4_M   0x000F0000  
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_PCTL_PMC4_S   16
#define GPIO_GPIO_PCTL_PMC3_M   0x0000F000  
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_PCTL_PMC3_S   12
#define GPIO_GPIO_PCTL_PMC1_M   0x00000F00  
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_PCTL_PMC1_S   8
#define GPIO_GPIO_PCTL_PMC2_M   0x000000F0  
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_PCTL_PMC2_S   4
#define GPIO_GPIO_PCTL_PMC0_M   0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            
#define GPIO_GPIO_PCTL_PMC0_S   0






#define GPIO_GPIO_ADCCTL_ADCEN_M \
                                0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPIO_GPIO_ADCCTL_ADCEN_S 0






#define GPIO_GPIO_DMACTL_DMAEN_M \
                                0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPIO_GPIO_DMACTL_DMAEN_S 0





#define GPIO_GPIO_SI_SUM        0x00000001  
                                            
                                            
                                            
                                            






#define GPIO_GPIO_PERIPHID4_PID4_M \
                                0x000000FF  
                                            
                                            

#define GPIO_GPIO_PERIPHID4_PID4_S 0






#define GPIO_GPIO_PERIPHID5_PID5_M \
                                0x000000FF  
                                            
                                            

#define GPIO_GPIO_PERIPHID5_PID5_S 0






#define GPIO_GPIO_PERIPHID6_PID6_M \
                                0x000000FF  
                                            
                                            

#define GPIO_GPIO_PERIPHID6_PID6_S 0






#define GPIO_GPIO_PERIPHID7_PID7_M \
                                0x000000FF  
                                            
                                            

#define GPIO_GPIO_PERIPHID7_PID7_S 0






#define GPIO_GPIO_PERIPHID0_PID0_M \
                                0x000000FF  
                                            
                                            
                                            
                                            

#define GPIO_GPIO_PERIPHID0_PID0_S 0






#define GPIO_GPIO_PERIPHID1_PID1_M \
                                0x000000FF  
                                            
                                            
                                            

#define GPIO_GPIO_PERIPHID1_PID1_S 0






#define GPIO_GPIO_PERIPHID2_PID2_M \
                                0x000000FF  
                                            
                                            
                                            
                                            

#define GPIO_GPIO_PERIPHID2_PID2_S 0






#define GPIO_GPIO_PERIPHID3_PID3_M \
                                0x000000FF  
                                            
                                            
                                            
                                            

#define GPIO_GPIO_PERIPHID3_PID3_S 0






#define GPIO_GPIO_PCELLID0_CID0_M \
                                0x000000FF  
                                            
                                            
                                            
                                            

#define GPIO_GPIO_PCELLID0_CID0_S 0






#define GPIO_GPIO_PCELLID1_CID1_M \
                                0x000000FF  
                                            
                                            
                                            
                                            

#define GPIO_GPIO_PCELLID1_CID1_S 0






#define GPIO_GPIO_PCELLID2_CID2_M \
                                0x000000FF  
                                            
                                            
                                            
                                            

#define GPIO_GPIO_PCELLID2_CID2_S 0






#define GPIO_GPIO_PCELLID3_CID3_M \
                                0x000000FF  
                                            
                                            
                                            
                                            

#define GPIO_GPIO_PCELLID3_CID3_S 0



#endif 
