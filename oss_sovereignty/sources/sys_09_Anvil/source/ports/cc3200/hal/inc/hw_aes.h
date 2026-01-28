


































#ifndef __HW_AES_H__
#define __HW_AES_H__






#define AES_O_KEY2_6          0x00000000  
                                            
#define AES_O_KEY2_7          0x00000004  
                                            
#define AES_O_KEY2_4          0x00000008  
                                            
#define AES_O_KEY2_5          0x0000000C  
                                            
#define AES_O_KEY2_2          0x00000010  
                                            
#define AES_O_KEY2_3          0x00000014  
                                            
                                            
#define AES_O_KEY2_0          0x00000018  
                                            
#define AES_O_KEY2_1          0x0000001C  
                                            
#define AES_O_KEY1_6          0x00000020  
#define AES_O_KEY1_7          0x00000024  
#define AES_O_KEY1_4          0x00000028  
#define AES_O_KEY1_5          0x0000002C  
#define AES_O_KEY1_2          0x00000030  
#define AES_O_KEY1_3          0x00000034  
#define AES_O_KEY1_0          0x00000038  
#define AES_O_KEY1_1          0x0000003C  
#define AES_O_IV_IN_0         0x00000040  
                                            
#define AES_O_IV_IN_1         0x00000044  
#define AES_O_IV_IN_2         0x00000048  
#define AES_O_IV_IN_3         0x0000004C  
                                            
#define AES_O_CTRL            0x00000050  
                                            
#define AES_O_C_LENGTH_0      0x00000054  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define AES_O_C_LENGTH_1      0x00000058  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define AES_O_AUTH_LENGTH     0x0000005C  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define AES_O_DATA_IN_0       0x00000060  
                                            
#define AES_O_DATA_IN_1       0x00000064  
                                            
#define AES_O_DATA_IN_2       0x00000068  
                                            
#define AES_O_DATA_IN_3       0x0000006C  
                                            
#define AES_O_TAG_OUT_0       0x00000070
#define AES_O_TAG_OUT_1       0x00000074
#define AES_O_TAG_OUT_2       0x00000078
#define AES_O_TAG_OUT_3       0x0000007C
#define AES_O_REVISION        0x00000080  
#define AES_O_SYSCONFIG       0x00000084  
                                            
                                            
                                            
#define AES_O_SYSSTATUS       0x00000088
#define AES_O_IRQSTATUS       0x0000008C  
                                            
                                            
                                            
#define AES_O_IRQENABLE       0x00000090  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            








#define AES_KEY2_6_KEY_M      0xFFFFFFFF  
#define AES_KEY2_6_KEY_S      0





#define AES_KEY2_7_KEY_M      0xFFFFFFFF  
#define AES_KEY2_7_KEY_S      0





#define AES_KEY2_4_KEY_M      0xFFFFFFFF  
#define AES_KEY2_4_KEY_S      0





#define AES_KEY2_5_KEY_M      0xFFFFFFFF  
#define AES_KEY2_5_KEY_S      0





#define AES_KEY2_2_KEY_M      0xFFFFFFFF  
#define AES_KEY2_2_KEY_S      0





#define AES_KEY2_3_KEY_M      0xFFFFFFFF  
#define AES_KEY2_3_KEY_S      0





#define AES_KEY2_0_KEY_M      0xFFFFFFFF  
#define AES_KEY2_0_KEY_S      0





#define AES_KEY2_1_KEY_M      0xFFFFFFFF  
#define AES_KEY2_1_KEY_S      0





#define AES_KEY1_6_KEY_M      0xFFFFFFFF  
#define AES_KEY1_6_KEY_S      0





#define AES_KEY1_7_KEY_M      0xFFFFFFFF  
#define AES_KEY1_7_KEY_S      0





#define AES_KEY1_4_KEY_M      0xFFFFFFFF  
#define AES_KEY1_4_KEY_S      0





#define AES_KEY1_5_KEY_M      0xFFFFFFFF  
#define AES_KEY1_5_KEY_S      0





#define AES_KEY1_2_KEY_M      0xFFFFFFFF  
#define AES_KEY1_2_KEY_S      0





#define AES_KEY1_3_KEY_M      0xFFFFFFFF  
#define AES_KEY1_3_KEY_S      0





#define AES_KEY1_0_KEY_M      0xFFFFFFFF  
#define AES_KEY1_0_KEY_S      0





#define AES_KEY1_1_KEY_M      0xFFFFFFFF  
#define AES_KEY1_1_KEY_S      0





#define AES_IV_IN_0_DATA_M    0xFFFFFFFF  
#define AES_IV_IN_0_DATA_S    0





#define AES_IV_IN_1_DATA_M    0xFFFFFFFF  
#define AES_IV_IN_1_DATA_S    0





#define AES_IV_IN_2_DATA_M    0xFFFFFFFF  
#define AES_IV_IN_2_DATA_S    0





#define AES_IV_IN_3_DATA_M    0xFFFFFFFF  
#define AES_IV_IN_3_DATA_S    0





#define AES_CTRL_CONTEXT_READY \
                                0x80000000  
                                            
                                            
                                            
                                            

#define AES_CTRL_SVCTXTRDY \
                                0x40000000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define AES_CTRL_SAVE_CONTEXT 0x20000000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define AES_CTRL_CCM_M      0x01C00000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define AES_CTRL_CCM_S      22
#define AES_CTRL_CCM_L_M      0x00380000  
                                            
                                            
                                            
                                            
                                            
                                            
#define AES_CTRL_CCM_L_S      19
#define AES_CTRL_CCM          0x00040000  
                                            
                                            
                                            
                                            
                                            
#define AES_CTRL_GCM_M        0x00030000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define AES_CTRL_GCM_S        16
#define AES_CTRL_CBCMAC       0x00008000  
                                            
                                            
                                            
#define AES_CTRL_F9           0x00004000  
                                            
                                            
                                            
#define AES_CTRL_F8           0x00002000  
                                            
                                            
                                            
#define AES_CTRL_XTS_M        0x00001800  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define AES_CTRL_XTS_S        11
#define AES_CTRL_CFB          0x00000400  
                                            
                                            
                                            
#define AES_CTRL_ICM          0x00000200  
                                            
                                            
                                            
                                            
#define AES_CTRL_CTR_WIDTH_M  0x00000180  
                                            
                                            
                                            
                                            
#define AES_CTRL_CTR_WIDTH_S  7
#define AES_CTRL_CTR          0x00000040  
                                            
                                            
                                            
                                            
#define AES_CTRL_MODE         0x00000020  
                                            
#define AES_CTRL_KEY_SIZE_M   0x00000018  
                                            
                                            
#define AES_CTRL_KEY_SIZE_S   3
#define AES_CTRL_DIRECTION    0x00000004  
                                            
                                            
                                            
                                            
                                            
#define AES_CTRL_INPUT_READY  0x00000002  
                                            
                                            
                                            
                                            
#define AES_CTRL_OUTPUT_READY 0x00000001  
                                            
                                            
                                            












#define AES_C_LENGTH_1_LENGTH_M \
                                0x1FFFFFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define AES_C_LENGTH_1_LENGTH_S 0






#define AES_AUTH_LENGTH_AUTH_M \
                                0xFFFFFFFF  

#define AES_AUTH_LENGTH_AUTH_S 0





#define AES_DATA_IN_0_DATA_M  0xFFFFFFFF  
#define AES_DATA_IN_0_DATA_S  0





#define AES_DATA_IN_1_DATA_M  0xFFFFFFFF  
#define AES_DATA_IN_1_DATA_S  0





#define AES_DATA_IN_2_DATA_M  0xFFFFFFFF  
#define AES_DATA_IN_2_DATA_S  0





#define AES_DATA_IN_3_DATA_M  0xFFFFFFFF  
#define AES_DATA_IN_3_DATA_S  0





#define AES_TAG_OUT_0_HASH_M  0xFFFFFFFF  
#define AES_TAG_OUT_0_HASH_S  0





#define AES_TAG_OUT_1_HASH_M  0xFFFFFFFF  
#define AES_TAG_OUT_1_HASH_S  0





#define AES_TAG_OUT_2_HASH_M  0xFFFFFFFF  
#define AES_TAG_OUT_2_HASH_S  0





#define AES_TAG_OUT_3_HASH_M  0xFFFFFFFF  
#define AES_TAG_OUT_3_HASH_S  0





#define AES_REVISION_SCHEME_M 0xC0000000
#define AES_REVISION_SCHEME_S 30
#define AES_REVISION_FUNC_M   0x0FFF0000  
                                            
                                            
                                            
                                            
                                            
#define AES_REVISION_FUNC_S   16
#define AES_REVISION_R_RTL_M  0x0000F800  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define AES_REVISION_R_RTL_S  11
#define AES_REVISION_X_MAJOR_M \
                                0x00000700  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define AES_REVISION_X_MAJOR_S 8
#define AES_REVISION_CUSTOM_M 0x000000C0
#define AES_REVISION_CUSTOM_S 6
#define AES_REVISION_Y_MINOR_M \
                                0x0000003F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define AES_REVISION_Y_MINOR_S 0





#define AES_SYSCONFIG_MACONTEXT_OUT_ON_DATA_OUT \
                                0x00000200  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define AES_SYSCONFIG_DMA_REQ_CONTEXT_OUT_EN \
                                0x00000100  
                                            
                                            
                                            
                                            

#define AES_SYSCONFIG_DMA_REQ_CONTEXT_IN_EN \
                                0x00000080  
                                            
                                            

#define AES_SYSCONFIG_DMA_REQ_DATA_OUT_EN \
                                0x00000040  
                                            
                                            

#define AES_SYSCONFIG_DMA_REQ_DATA_IN_EN \
                                0x00000020  
                                            
                                            






#define AES_SYSSTATUS_RESETDONE \
                                0x00000001






#define AES_IRQSTATUS_CONTEXT_OUT \
                                0x00000008  
                                            
                                            
                                            

#define AES_IRQSTATUS_DATA_OUT \
                                0x00000004  
                                            
                                            

#define AES_IRQSTATUS_DATA_IN 0x00000002  
                                            
                                            
#define AES_IRQSTATUS_CONTEX_IN \
                                0x00000001  
                                            
                                            






#define AES_IRQENABLE_CONTEXT_OUT \
                                0x00000008  
                                            
                                            
                                            

#define AES_IRQENABLE_DATA_OUT \
                                0x00000004  
                                            
                                            

#define AES_IRQENABLE_DATA_IN 0x00000002  
                                            
                                            
#define AES_IRQENABLE_CONTEX_IN \
                                0x00000001  
                                            
                                            




#endif 
