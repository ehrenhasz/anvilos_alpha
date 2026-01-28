


































#ifndef __HW_SHAMD5_H__
#define __HW_SHAMD5_H__






#define SHAMD5_O_ODIGEST_A       0x00000000  
                                            
                                            
                                            
                                            
                                            
#define SHAMD5_O_ODIGEST_B       0x00000004  
                                            
                                            
                                            
                                            
                                            
#define SHAMD5_O_ODIGEST_C       0x00000008  
                                            
                                            
                                            
                                            
                                            
#define SHAMD5_O_ODIGEST_D       0x0000000C  
                                            
                                            
                                            
                                            
                                            
#define SHAMD5_O_ODIGEST_E       0x00000010  
                                            
                                            
                                            
                                            
#define SHAMD5_O_ODIGEST_F       0x00000014  
                                            
                                            
                                            
#define SHAMD5_O_ODIGEST_G       0x00000018  
                                            
                                            
                                            
#define SHAMD5_O_ODIGEST_H       0x0000001C  
                                            
                                            
                                            
#define SHAMD5_O_IDIGEST_A       0x00000020  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define SHAMD5_O_IDIGEST_B       0x00000024  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define SHAMD5_O_IDIGEST_C       0x00000028  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define SHAMD5_O_IDIGEST_D       0x0000002C  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define SHAMD5_O_IDIGEST_E       0x00000030  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define SHAMD5_O_IDIGEST_F       0x00000034  
                                            
                                            
                                            
                                            
                                            
                                            
#define SHAMD5_O_IDIGEST_G       0x00000038  
                                            
                                            
                                            
                                            
                                            
                                            
#define SHAMD5_O_IDIGEST_H       0x0000003C  
                                            
                                            
                                            
                                            
                                            
#define SHAMD5_O_DIGEST_COUNT    0x00000040  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define SHAMD5_O_MODE            0x00000044  
#define SHAMD5_O_LENGTH          0x00000048  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define SHAMD5_O_DATA0_IN        0x00000080  
#define SHAMD5_O_DATA1_IN        0x00000084  
#define SHAMD5_O_DATA2_IN        0x00000088  
#define SHAMD5_O_DATA3_IN        0x0000008C  
#define SHAMD5_O_DATA4_IN        0x00000090  
#define SHAMD5_O_DATA5_IN        0x00000094  
#define SHAMD5_O_DATA6_IN        0x00000098  
#define SHAMD5_O_DATA7_IN        0x0000009C  
#define SHAMD5_O_DATA8_IN        0x000000A0  
#define SHAMD5_O_DATA9_IN        0x000000A4  
#define SHAMD5_O_DATA10_IN       0x000000A8  
#define SHAMD5_O_DATA11_IN       0x000000AC  
#define SHAMD5_O_DATA12_IN       0x000000B0  
#define SHAMD5_O_DATA13_IN       0x000000B4  
#define SHAMD5_O_DATA14_IN       0x000000B8  
#define SHAMD5_O_DATA15_IN       0x000000BC  
#define SHAMD5_O_REVISION        0x00000100  
#define SHAMD5_O_SYSCONFIG       0x00000110  
#define SHAMD5_O_SYSSTATUS       0x00000114  
#define SHAMD5_O_IRQSTATUS       0x00000118  
#define SHAMD5_O_IRQENABLE       0x0000011C  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define SHAMD5_O_HASH512_ODIGEST_A \
                                0x00000200

#define SHAMD5_O_HASH512_ODIGEST_B \
                                0x00000204

#define SHAMD5_O_HASH512_ODIGEST_C \
                                0x00000208

#define SHAMD5_O_HASH512_ODIGEST_D \
                                0x0000020C

#define SHAMD5_O_HASH512_ODIGEST_E \
                                0x00000210

#define SHAMD5_O_HASH512_ODIGEST_F \
                                0x00000214

#define SHAMD5_O_HASH512_ODIGEST_G \
                                0x00000218

#define SHAMD5_O_HASH512_ODIGEST_H \
                                0x0000021C

#define SHAMD5_O_HASH512_ODIGEST_I \
                                0x00000220

#define SHAMD5_O_HASH512_ODIGEST_J \
                                0x00000224

#define SHAMD5_O_HASH512_ODIGEST_K \
                                0x00000228

#define SHAMD5_O_HASH512_ODIGEST_L \
                                0x0000022C

#define SHAMD5_O_HASH512_ODIGEST_M \
                                0x00000230

#define SHAMD5_O_HASH512_ODIGEST_N \
                                0x00000234

#define SHAMD5_O_HASH512_ODIGEST_O \
                                0x00000238

#define SHAMD5_O_HASH512_ODIGEST_P \
                                0x0000023C

#define SHAMD5_O_HASH512_IDIGEST_A \
                                0x00000240

#define SHAMD5_O_HASH512_IDIGEST_B \
                                0x00000244

#define SHAMD5_O_HASH512_IDIGEST_C \
                                0x00000248

#define SHAMD5_O_HASH512_IDIGEST_D \
                                0x0000024C

#define SHAMD5_O_HASH512_IDIGEST_E \
                                0x00000250

#define SHAMD5_O_HASH512_IDIGEST_F \
                                0x00000254

#define SHAMD5_O_HASH512_IDIGEST_G \
                                0x00000258

#define SHAMD5_O_HASH512_IDIGEST_H \
                                0x0000025C

#define SHAMD5_O_HASH512_IDIGEST_I \
                                0x00000260

#define SHAMD5_O_HASH512_IDIGEST_J \
                                0x00000264

#define SHAMD5_O_HASH512_IDIGEST_K \
                                0x00000268

#define SHAMD5_O_HASH512_IDIGEST_L \
                                0x0000026C

#define SHAMD5_O_HASH512_IDIGEST_M \
                                0x00000270

#define SHAMD5_O_HASH512_IDIGEST_N \
                                0x00000274

#define SHAMD5_O_HASH512_IDIGEST_O \
                                0x00000278

#define SHAMD5_O_HASH512_IDIGEST_P \
                                0x0000027C

#define SHAMD5_O_HASH512_DIGEST_COUNT \
                                0x00000280

#define SHAMD5_O_HASH512_MODE    0x00000284
#define SHAMD5_O_HASH512_LENGTH  0x00000288








#define SHAMD5_ODIGEST_A_DATA_M  0xFFFFFFFF  
#define SHAMD5_ODIGEST_A_DATA_S  0





#define SHAMD5_ODIGEST_B_DATA_M  0xFFFFFFFF  
#define SHAMD5_ODIGEST_B_DATA_S  0





#define SHAMD5_ODIGEST_C_DATA_M  0xFFFFFFFF  
#define SHAMD5_ODIGEST_C_DATA_S  0





#define SHAMD5_ODIGEST_D_DATA_M  0xFFFFFFFF  
#define SHAMD5_ODIGEST_D_DATA_S  0





#define SHAMD5_ODIGEST_E_DATA_M  0xFFFFFFFF  
#define SHAMD5_ODIGEST_E_DATA_S  0





#define SHAMD5_ODIGEST_F_DATA_M  0xFFFFFFFF  
#define SHAMD5_ODIGEST_F_DATA_S  0





#define SHAMD5_ODIGEST_G_DATA_M  0xFFFFFFFF  
#define SHAMD5_ODIGEST_G_DATA_S  0





#define SHAMD5_ODIGEST_H_DATA_M  0xFFFFFFFF  
#define SHAMD5_ODIGEST_H_DATA_S  0





#define SHAMD5_IDIGEST_A_DATA_M  0xFFFFFFFF  
#define SHAMD5_IDIGEST_A_DATA_S  0





#define SHAMD5_IDIGEST_B_DATA_M  0xFFFFFFFF  
#define SHAMD5_IDIGEST_B_DATA_S  0





#define SHAMD5_IDIGEST_C_DATA_M  0xFFFFFFFF  
#define SHAMD5_IDIGEST_C_DATA_S  0





#define SHAMD5_IDIGEST_D_DATA_M  0xFFFFFFFF  
#define SHAMD5_IDIGEST_D_DATA_S  0





#define SHAMD5_IDIGEST_E_DATA_M  0xFFFFFFFF  
#define SHAMD5_IDIGEST_E_DATA_S  0





#define SHAMD5_IDIGEST_F_DATA_M  0xFFFFFFFF  
#define SHAMD5_IDIGEST_F_DATA_S  0





#define SHAMD5_IDIGEST_G_DATA_M  0xFFFFFFFF  
#define SHAMD5_IDIGEST_G_DATA_S  0





#define SHAMD5_IDIGEST_H_DATA_M  0xFFFFFFFF  
#define SHAMD5_IDIGEST_H_DATA_S  0






#define SHAMD5_DIGEST_COUNT_DATA_M \
                                0xFFFFFFFF  

#define SHAMD5_DIGEST_COUNT_DATA_S 0





#define SHAMD5_MODE_HMAC_OUTER_HASH \
                                0x00000080  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define SHAMD5_MODE_HMAC_KEY_PROC \
                                0x00000020  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define SHAMD5_MODE_CLOSE_HASH   0x00000010  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define SHAMD5_MODE_ALGO_CONSTANT \
                                0x00000008  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define SHAMD5_MODE_ALGO_M       0x00000006  
                                            
                                            
                                            
                                            
                                            
#define SHAMD5_MODE_ALGO_S       1





#define SHAMD5_LENGTH_DATA_M     0xFFFFFFFF  
#define SHAMD5_LENGTH_DATA_S     0





#define SHAMD5_DATA0_IN_DATA0_IN_M \
                                0xFFFFFFFF  

#define SHAMD5_DATA0_IN_DATA0_IN_S 0





#define SHAMD5_DATA1_IN_DATA1_IN_M \
                                0xFFFFFFFF  

#define SHAMD5_DATA1_IN_DATA1_IN_S 0





#define SHAMD5_DATA2_IN_DATA2_IN_M \
                                0xFFFFFFFF  

#define SHAMD5_DATA2_IN_DATA2_IN_S 0





#define SHAMD5_DATA3_IN_DATA3_IN_M \
                                0xFFFFFFFF  

#define SHAMD5_DATA3_IN_DATA3_IN_S 0





#define SHAMD5_DATA4_IN_DATA4_IN_M \
                                0xFFFFFFFF  

#define SHAMD5_DATA4_IN_DATA4_IN_S 0





#define SHAMD5_DATA5_IN_DATA5_IN_M \
                                0xFFFFFFFF  

#define SHAMD5_DATA5_IN_DATA5_IN_S 0





#define SHAMD5_DATA6_IN_DATA6_IN_M \
                                0xFFFFFFFF  

#define SHAMD5_DATA6_IN_DATA6_IN_S 0





#define SHAMD5_DATA7_IN_DATA7_IN_M \
                                0xFFFFFFFF  

#define SHAMD5_DATA7_IN_DATA7_IN_S 0





#define SHAMD5_DATA8_IN_DATA8_IN_M \
                                0xFFFFFFFF  

#define SHAMD5_DATA8_IN_DATA8_IN_S 0





#define SHAMD5_DATA9_IN_DATA9_IN_M \
                                0xFFFFFFFF  

#define SHAMD5_DATA9_IN_DATA9_IN_S 0





#define SHAMD5_DATA10_IN_DATA10_IN_M \
                                0xFFFFFFFF  

#define SHAMD5_DATA10_IN_DATA10_IN_S 0





#define SHAMD5_DATA11_IN_DATA11_IN_M \
                                0xFFFFFFFF  

#define SHAMD5_DATA11_IN_DATA11_IN_S 0





#define SHAMD5_DATA12_IN_DATA12_IN_M \
                                0xFFFFFFFF  

#define SHAMD5_DATA12_IN_DATA12_IN_S 0





#define SHAMD5_DATA13_IN_DATA13_IN_M \
                                0xFFFFFFFF  

#define SHAMD5_DATA13_IN_DATA13_IN_S 0





#define SHAMD5_DATA14_IN_DATA14_IN_M \
                                0xFFFFFFFF  

#define SHAMD5_DATA14_IN_DATA14_IN_S 0





#define SHAMD5_DATA15_IN_DATA15_IN_M \
                                0xFFFFFFFF  

#define SHAMD5_DATA15_IN_DATA15_IN_S 0





#define SHAMD5_REVISION_SCHEME_M 0xC0000000
#define SHAMD5_REVISION_SCHEME_S 30
#define SHAMD5_REVISION_FUNC_M   0x0FFF0000  
                                            
                                            
                                            
                                            
                                            
#define SHAMD5_REVISION_FUNC_S   16
#define SHAMD5_REVISION_R_RTL_M  0x0000F800  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define SHAMD5_REVISION_R_RTL_S  11
#define SHAMD5_REVISION_X_MAJOR_M \
                                0x00000700  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define SHAMD5_REVISION_X_MAJOR_S 8
#define SHAMD5_REVISION_CUSTOM_M 0x000000C0
#define SHAMD5_REVISION_CUSTOM_S 6
#define SHAMD5_REVISION_Y_MINOR_M \
                                0x0000003F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define SHAMD5_REVISION_Y_MINOR_S 0





#define SHAMD5_SYSCONFIG_PADVANCED \
                                0x00000080  
                                            
                                            
                                            

#define SHAMD5_SYSCONFIG_PCONT_SWT \
                                0x00000040  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define SHAMD5_SYSCONFIG_PDMA_EN 0x00000008
#define SHAMD5_SYSCONFIG_PIT_EN  0x00000004





#define SHAMD5_SYSSTATUS_RESETDONE \
                                0x00000001  






#define SHAMD5_IRQSTATUS_CONTEXT_READY \
                                0x00000008  
                                            
                                            
                                            

#define SHAMD5_IRQSTATUS_PARTHASH_READY \
                                0x00000004  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define SHAMD5_IRQSTATUS_INPUT_READY \
                                0x00000002  
                                            
                                            

#define SHAMD5_IRQSTATUS_OUTPUT_READY \
                                0x00000001  
                                            
                                            
                                            






#define SHAMD5_IRQENABLE_M_CONTEXT_READY \
                                0x00000008  

#define SHAMD5_IRQENABLE_M_PARTHASH_READY \
                                0x00000004  

#define SHAMD5_IRQENABLE_M_INPUT_READY \
                                0x00000002  

#define SHAMD5_IRQENABLE_M_OUTPUT_READY \
                                0x00000001  







#define SHAMD5_HASH512_ODIGEST_A_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_ODIGEST_A_DATA_S 0






#define SHAMD5_HASH512_ODIGEST_B_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_ODIGEST_B_DATA_S 0






#define SHAMD5_HASH512_ODIGEST_C_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_ODIGEST_C_DATA_S 0






#define SHAMD5_HASH512_ODIGEST_D_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_ODIGEST_D_DATA_S 0






#define SHAMD5_HASH512_ODIGEST_E_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_ODIGEST_E_DATA_S 0






#define SHAMD5_HASH512_ODIGEST_F_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_ODIGEST_F_DATA_S 0






#define SHAMD5_HASH512_ODIGEST_G_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_ODIGEST_G_DATA_S 0






#define SHAMD5_HASH512_ODIGEST_H_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_ODIGEST_H_DATA_S 0






#define SHAMD5_HASH512_ODIGEST_I_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_ODIGEST_I_DATA_S 0






#define SHAMD5_HASH512_ODIGEST_J_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_ODIGEST_J_DATA_S 0






#define SHAMD5_HASH512_ODIGEST_K_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_ODIGEST_K_DATA_S 0






#define SHAMD5_HASH512_ODIGEST_L_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_ODIGEST_L_DATA_S 0






#define SHAMD5_HASH512_ODIGEST_M_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_ODIGEST_M_DATA_S 0






#define SHAMD5_HASH512_ODIGEST_N_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_ODIGEST_N_DATA_S 0






#define SHAMD5_HASH512_ODIGEST_O_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_ODIGEST_O_DATA_S 0






#define SHAMD5_HASH512_ODIGEST_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_ODIGEST_DATA_S 0






#define SHAMD5_HASH512_IDIGEST_A_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_IDIGEST_A_DATA_S 0






#define SHAMD5_HASH512_IDIGEST_B_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_IDIGEST_B_DATA_S 0






#define SHAMD5_HASH512_IDIGEST_C_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_IDIGEST_C_DATA_S 0






#define SHAMD5_HASH512_IDIGEST_D_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_IDIGEST_D_DATA_S 0






#define SHAMD5_HASH512_IDIGEST_E_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_IDIGEST_E_DATA_S 0






#define SHAMD5_HASH512_IDIGEST_F_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_IDIGEST_F_DATA_S 0






#define SHAMD5_HASH512_IDIGEST_G_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_IDIGEST_G_DATA_S 0






#define SHAMD5_HASH512_IDIGEST_H_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_IDIGEST_H_DATA_S 0






#define SHAMD5_HASH512_IDIGEST_I_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_IDIGEST_I_DATA_S 0






#define SHAMD5_HASH512_IDIGEST_J_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_IDIGEST_J_DATA_S 0






#define SHAMD5_HASH512_IDIGEST_K_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_IDIGEST_K_DATA_S 0






#define SHAMD5_HASH512_IDIGEST_L_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_IDIGEST_L_DATA_S 0






#define SHAMD5_HASH512_IDIGEST_M_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_IDIGEST_M_DATA_S 0






#define SHAMD5_HASH512_IDIGEST_N_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_IDIGEST_N_DATA_S 0






#define SHAMD5_HASH512_IDIGEST_O_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_IDIGEST_O_DATA_S 0






#define SHAMD5_HASH512_IDIGEST_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_IDIGEST_DATA_S 0






#define SHAMD5_HASH512_DIGEST_COUNT_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_DIGEST_COUNT_DATA_S 0






#define SHAMD5_HASH512_MODE_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_MODE_DATA_S 0






#define SHAMD5_HASH512_LENGTH_DATA_M \
                                0xFFFFFFFF

#define SHAMD5_HASH512_LENGTH_DATA_S 0



#endif 
