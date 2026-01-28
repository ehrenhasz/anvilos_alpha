


































#ifndef __HW_WDT_H__
#define __HW_WDT_H__






#define WDT_O_LOAD              0x00000000
#define WDT_O_VALUE             0x00000004
#define WDT_O_CTL               0x00000008
#define WDT_O_ICR               0x0000000C
#define WDT_O_RIS               0x00000010
#define WDT_O_MIS               0x00000014
#define WDT_O_TEST              0x00000418
#define WDT_O_LOCK              0x00000C00








#define WDT_LOAD_M            0xFFFFFFFF  
#define WDT_LOAD_S            0





#define WDT_VALUE_M           0xFFFFFFFF  
#define WDT_VALUE_S           0





#define WDT_CTL_WRC             0x80000000  
#define WDT_CTL_INTTYPE         0x00000004  
#define WDT_CTL_RESEN           0x00000002  
                                            
                                            
                                            
#define WDT_CTL_INTEN           0x00000001  





#define WDT_ICR_M             0xFFFFFFFF  
#define WDT_ICR_S             0





#define WDT_RIS_WDTRIS          0x00000001  





#define WDT_MIS_WDTMIS          0x00000001  





#define WDT_TEST_STALL_EN_M     0x00000C00  
#define WDT_TEST_STALL_EN_S     10
#define WDT_TEST_STALL          0x00000100  





#define WDT_LOCK_M            0xFFFFFFFF  
#define WDT_LOCK_S            0
#define WDT_LOCK_UNLOCKED     0x00000000  
#define WDT_LOCK_LOCKED       0x00000001  
#define WDT_LOCK_UNLOCK       0x1ACCE551  







#define WDT_INT_TIMEOUT         0x00000001  





#endif 
