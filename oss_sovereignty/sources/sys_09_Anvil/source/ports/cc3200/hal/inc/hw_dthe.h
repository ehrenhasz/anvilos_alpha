



































#ifndef __HW_DTHE_H__
#define __HW_DTHE_H__






#define DTHE_O_SHA_IM    0x00000810
#define DTHE_O_SHA_RIS    0x00000814
#define DTHE_O_SHA_MIS    0x00000818
#define DTHE_O_SHA_IC    0x0000081C
#define DTHE_O_AES_IM    0x00000820
#define DTHE_O_AES_RIS    0x00000824
#define DTHE_O_AES_MIS    0x00000828
#define DTHE_O_AES_IC    0x0000082C
#define DTHE_O_DES_IM    0x00000830
#define DTHE_O_DES_RIS    0x00000834
#define DTHE_O_DES_MIS    0x00000838
#define DTHE_O_DES_IC    0x0000083C
#define DTHE_O_EIP_CGCFG      0x00000A00
#define DTHE_O_EIP_CGREQ      0x00000A04
#define DTHE_O_CRC_CTRL       0x00000C00
#define DTHE_O_CRC_SEED       0x00000C10
#define DTHE_O_CRC_DIN        0x00000C14
#define DTHE_O_CRC_RSLT_PP    0x00000C18
#define DTHE_O_RAND_KEY0      0x00000F00
#define DTHE_O_RAND_KEY1      0x00000F04
#define DTHE_O_RAND_KEY2      0x00000F08
#define DTHE_O_RAND_KEY3      0x00000F0C








#define DTHE_SHAMD5_IMST_DIN  0x00000004  
                                            
                                            
                                            
#define DTHE_SHAMD5_IMST_COUT 0x00000002  
                                            
                                            
                                            
#define DTHE_SHAMD5_IMST_CIN  0x00000001  
                                            
                                            






#define DTHE_SHAMD5_IRIS_DIN  0x00000004  
#define DTHE_SHAMD5_IRIS_COUT 0x00000002  
#define DTHE_SHAMD5_IRIS_CIN  0x00000001  






#define DTHE_SHAMD5_IMIS_DIN  0x00000004  
#define DTHE_SHAMD5_IMIS_COUT 0x00000002  
#define DTHE_SHAMD5_IMIS_CIN  0x00000001  






#define DTHE_SHAMD5_ICIS_DIN  0x00000004  
                                            
#define DTHE_SHAMD5_ICIS_COUT 0x00000002  
#define DTHE_SHAMD5_ICIS_CIN  0x00000001  






#define DTHE_AES_IMST_DOUT 0x00000008  
                                            
                                            
#define DTHE_AES_IMST_DIN  0x00000004  
                                            
                                            
                                            
#define DTHE_AES_IMST_COUT 0x00000002  
                                            
                                            
                                            
#define DTHE_AES_IMST_CIN  0x00000001  
                                            
                                            






#define DTHE_AES_IRIS_DOUT 0x00000008  
#define DTHE_AES_IRIS_DIN  0x00000004  
#define DTHE_AES_IRIS_COUT 0x00000002  
#define DTHE_AES_IRIS_CIN  0x00000001  






#define DTHE_AES_IMIS_DOUT 0x00000008  
#define DTHE_AES_IMIS_DIN  0x00000004  
#define DTHE_AES_IMIS_COUT 0x00000002  
#define DTHE_AES_IMIS_CIN  0x00000001  






#define DTHE_AES_ICIS_DOUT 0x00000008  
                                            
#define DTHE_AES_ICIS_DIN  0x00000004  
                                            
#define DTHE_AES_ICIS_COUT 0x00000002  
#define DTHE_AES_ICIS_CIN  0x00000001  






#define DTHE_DES_IMST_DOUT 0x00000008  
                                            
                                            
#define DTHE_DES_IMST_DIN  0x00000004  
                                            
                                            
                                            
#define DTHE_DES_IMST_CIN  0x00000001  
                                            
                                            






#define DTHE_DES_IRIS_DOUT 0x00000008  
#define DTHE_DES_IRIS_DIN  0x00000004  
#define DTHE_DES_IRIS_CIN  0x00000001  






#define DTHE_DES_IMIS_DOUT 0x00000008  
#define DTHE_DES_IMIS_DIN  0x00000004  
#define DTHE_DES_IMIS_CIN  0x00000001  






#define DTHE_DES_ICIS_DOUT 0x00000008  
                                            
#define DTHE_DES_ICIS_DIN  0x00000004  
                                            
#define DTHE_DES_ICIS_CIN  0x00000001  






#define DTHE_EIP_CGCFG_EIP29_CFG \
                                0x00000010  
                                            
                                            
                                            

#define DTHE_EIP_CGCFG_EIP75_CFG \
                                0x00000008  
                                            
                                            
                                            

#define DTHE_EIP_CGCFG_EIP16_CFG \
                                0x00000004  
                                            
                                            
                                            

#define DTHE_EIP_CGCFG_EIP36_CFG \
                                0x00000002  
                                            
                                            
                                            

#define DTHE_EIP_CGCFG_EIP57_CFG \
                                0x00000001  
                                            
                                            
                                            







#define DTHE_EIP_CGREQ_Key_M  0xF0000000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define DTHE_EIP_CGREQ_Key_S  28
#define DTHE_EIP_CGREQ_EIP29_REQ \
                                0x00000010  
                                            

#define DTHE_EIP_CGREQ_EIP75_REQ \
                                0x00000008  
                                            

#define DTHE_EIP_CGREQ_EIP16_REQ \
                                0x00000004  
                                            

#define DTHE_EIP_CGREQ_EIP36_REQ \
                                0x00000002  
                                            

#define DTHE_EIP_CGREQ_EIP57_REQ \
                                0x00000001  
                                            






#define DTHE_CRC_CTRL_INIT_M  0x00006000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define DTHE_CRC_CTRL_INIT_S  13
#define DTHE_CRC_CTRL_SIZE    0x00001000  
                                            
#define DTHE_CRC_CTRL_OINV    0x00000200  
                                            
#define DTHE_CRC_CTRL_OBR     0x00000100  
                                            
                                            
                                            
#define DTHE_CRC_CTRL_IBR     0x00000080  
                                            
#define DTHE_CRC_CTRL_ENDIAN_M \
                                0x00000030  
                                            

#define DTHE_CRC_CTRL_ENDIAN_S 4
#define DTHE_CRC_CTRL_TYPE_M  0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define DTHE_CRC_CTRL_TYPE_S  0





#define DTHE_CRC_SEED_SEED_M  0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            
#define DTHE_CRC_SEED_SEED_S  0





#define DTHE_CRC_DIN_DATA_IN_M \
                                0xFFFFFFFF  
                                            

#define DTHE_CRC_DIN_DATA_IN_S 0






#define DTHE_CRC_RSLT_PP_RSLT_PP_M \
                                0xFFFFFFFF  
                                            

#define DTHE_CRC_RSLT_PP_RSLT_PP_S 0






#define DTHE_RAND_KEY0_KEY_M  0xFFFFFFFF  
                                            
#define DTHE_RAND_KEY0_KEY_S  0






#define DTHE_RAND_KEY1_KEY_M  0xFFFFFFFF  
                                            
#define DTHE_RAND_KEY1_KEY_S  0






#define DTHE_RAND_KEY2_KEY_M  0xFFFFFFFF  
                                            
#define DTHE_RAND_KEY2_KEY_S  0






#define DTHE_RAND_KEY3_KEY_M  0xFFFFFFFF  
                                            
#define DTHE_RAND_KEY3_KEY_S  0



#endif 
