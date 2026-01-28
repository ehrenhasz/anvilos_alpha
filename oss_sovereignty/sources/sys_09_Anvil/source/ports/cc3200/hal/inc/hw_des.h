


































#ifndef __HW_DES_H__
#define __HW_DES_H__






#define DES_O_KEY3_L          0x00000000  
#define DES_O_KEY3_H          0x00000004  
#define DES_O_KEY2_L          0x00000008  
#define DES_O_KEY2_H          0x0000000C  
#define DES_O_KEY1_L          0x00000010  
                                            
#define DES_O_KEY1_H          0x00000014  
                                            
#define DES_O_IV_L            0x00000018  
#define DES_O_IV_H            0x0000001C  
#define DES_O_CTRL            0x00000020
#define DES_O_LENGTH          0x00000024  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define DES_O_DATA_L          0x00000028  
                                            
#define DES_O_DATA_H          0x0000002C  
                                            
#define DES_O_REVISION        0x00000030
#define DES_O_SYSCONFIG       0x00000034
#define DES_O_SYSSTATUS       0x00000038
#define DES_O_IRQSTATUS       0x0000003C  
                                            
                                            
                                            
#define DES_O_IRQENABLE       0x00000040  
                                            
                                            
                                            
                                            
                                            
                                            








#define DES_KEY3_L_KEY3_L_M   0xFFFFFFFF  
#define DES_KEY3_L_KEY3_L_S   0





#define DES_KEY3_H_KEY3_H_M   0xFFFFFFFF  
#define DES_KEY3_H_KEY3_H_S   0





#define DES_KEY2_L_KEY2_L_M   0xFFFFFFFF  
#define DES_KEY2_L_KEY2_L_S   0





#define DES_KEY2_H_KEY2_H_M   0xFFFFFFFF  
#define DES_KEY2_H_KEY2_H_S   0





#define DES_KEY1_L_KEY1_L_M   0xFFFFFFFF  
#define DES_KEY1_L_KEY1_L_S   0





#define DES_KEY1_H_KEY1_H_M   0xFFFFFFFF  
#define DES_KEY1_H_KEY1_H_S   0





#define DES_IV_L_IV_L_M       0xFFFFFFFF  
                                            
#define DES_IV_L_IV_L_S       0





#define DES_IV_H_IV_H_M       0xFFFFFFFF  
                                            
#define DES_IV_H_IV_H_S       0





#define DES_CTRL_CONTEXT      0x80000000  
                                            
                                            
                                            
                                            
#define DES_CTRL_MODE_M       0x00000030  
                                            
                                            
#define DES_CTRL_MODE_S       4
#define DES_CTRL_TDES         0x00000008  
                                            
                                            
#define DES_CTRL_DIRECTION    0x00000004  
                                            
                                            
#define DES_CTRL_INPUT_READY  0x00000002  
                                            
#define DES_CTRL_OUTPUT_READY 0x00000001  
                                            





#define DES_LENGTH_LENGTH_M   0xFFFFFFFF
#define DES_LENGTH_LENGTH_S   0





#define DES_DATA_L_DATA_L_M   0xFFFFFFFF  
#define DES_DATA_L_DATA_L_S   0





#define DES_DATA_H_DATA_H_M   0xFFFFFFFF  
#define DES_DATA_H_DATA_H_S   0





#define DES_REVISION_SCHEME_M 0xC0000000
#define DES_REVISION_SCHEME_S 30
#define DES_REVISION_FUNC_M   0x0FFF0000  
                                            
                                            
                                            
                                            
                                            
#define DES_REVISION_FUNC_S   16
#define DES_REVISION_R_RTL_M  0x0000F800  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define DES_REVISION_R_RTL_S  11
#define DES_REVISION_X_MAJOR_M \
                                0x00000700  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define DES_REVISION_X_MAJOR_S 8
#define DES_REVISION_CUSTOM_M 0x000000C0
#define DES_REVISION_CUSTOM_S 6
#define DES_REVISION_Y_MINOR_M \
                                0x0000003F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define DES_REVISION_Y_MINOR_S 0





#define DES_SYSCONFIG_DMA_REQ_CONTEXT_IN_EN \
                                0x00000080  
                                            
                                            

#define DES_SYSCONFIG_DMA_REQ_DATA_OUT_EN \
                                0x00000040  
                                            
                                            

#define DES_SYSCONFIG_DMA_REQ_DATA_IN_EN \
                                0x00000020  
                                            
                                            






#define DES_SYSSTATUS_RESETDONE \
                                0x00000001






#define DES_IRQSTATUS_DATA_OUT \
                                0x00000004  
                                            
                                            

#define DES_IRQSTATUS_DATA_IN 0x00000002  
                                            
                                            
#define DES_IRQSTATUS_CONTEX_IN \
                                0x00000001  
                                            
                                            






#define DES_IRQENABLE_M_DATA_OUT \
                                0x00000004  
                                            
                                            

#define DES_IRQENABLE_M_DATA_IN \
                                0x00000002  
                                            
                                            

#define DES_IRQENABLE_M_CONTEX_IN \
                                0x00000001  
                                            
                                            




#endif 
