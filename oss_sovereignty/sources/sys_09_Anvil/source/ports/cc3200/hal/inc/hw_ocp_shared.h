


































#ifndef __HW_OCP_SHARED_H__
#define __HW_OCP_SHARED_H__






#define OCP_SHARED_O_SEMAPHORE1 0x00000000
#define OCP_SHARED_O_SEMAPHORE2 0x00000004
#define OCP_SHARED_O_SEMAPHORE3 0x00000008
#define OCP_SHARED_O_SEMAPHORE4 0x0000000C
#define OCP_SHARED_O_SEMAPHORE5 0x00000010
#define OCP_SHARED_O_SEMAPHORE6 0x00000014
#define OCP_SHARED_O_SEMAPHORE7 0x00000018
#define OCP_SHARED_O_SEMAPHORE8 0x0000001C
#define OCP_SHARED_O_SEMAPHORE9 0x00000020
#define OCP_SHARED_O_SEMAPHORE10 \
                                0x00000024

#define OCP_SHARED_O_SEMAPHORE11 \
                                0x00000028

#define OCP_SHARED_O_SEMAPHORE12 \
                                0x0000002C

#define OCP_SHARED_O_IC_LOCKER_ID \
                                0x00000030

#define OCP_SHARED_O_MCU_SEMAPHORE_PEND \
                                0x00000034

#define OCP_SHARED_O_WL_SEMAPHORE_PEND \
                                0x00000038

#define OCP_SHARED_O_PLATFORM_DETECTION_RD_ONLY \
                                0x0000003C

#define OCP_SHARED_O_SEMAPHORES_STATUS_RD_ONLY \
                                0x00000040

#define OCP_SHARED_O_CC3XX_CONFIG_CTRL \
                                0x00000044

#define OCP_SHARED_O_CC3XX_SHARED_MEM_SEL_LSB \
                                0x00000048

#define OCP_SHARED_O_CC3XX_SHARED_MEM_SEL_MSB \
                                0x0000004C

#define OCP_SHARED_O_WLAN_ELP_WAKE_EN \
                                0x00000050

#define OCP_SHARED_O_DEVINIT_ROM_START_ADDR \
                                0x00000054

#define OCP_SHARED_O_DEVINIT_ROM_END_ADDR \
                                0x00000058

#define OCP_SHARED_O_SSBD_SEED  0x0000005C
#define OCP_SHARED_O_SSBD_CHK   0x00000060
#define OCP_SHARED_O_SSBD_POLY_SEL \
                                0x00000064

#define OCP_SHARED_O_SPARE_REG_0 \
                                0x00000068

#define OCP_SHARED_O_SPARE_REG_1 \
                                0x0000006C

#define OCP_SHARED_O_SPARE_REG_2 \
                                0x00000070

#define OCP_SHARED_O_SPARE_REG_3 \
                                0x00000074

#define OCP_SHARED_O_GPIO_PAD_CONFIG_0 \
                                0x000000A0

#define OCP_SHARED_O_GPIO_PAD_CONFIG_1 \
                                0x000000A4

#define OCP_SHARED_O_GPIO_PAD_CONFIG_2 \
                                0x000000A8

#define OCP_SHARED_O_GPIO_PAD_CONFIG_3 \
                                0x000000AC

#define OCP_SHARED_O_GPIO_PAD_CONFIG_4 \
                                0x000000B0

#define OCP_SHARED_O_GPIO_PAD_CONFIG_5 \
                                0x000000B4

#define OCP_SHARED_O_GPIO_PAD_CONFIG_6 \
                                0x000000B8

#define OCP_SHARED_O_GPIO_PAD_CONFIG_7 \
                                0x000000BC

#define OCP_SHARED_O_GPIO_PAD_CONFIG_8 \
                                0x000000C0

#define OCP_SHARED_O_GPIO_PAD_CONFIG_9 \
                                0x000000C4

#define OCP_SHARED_O_GPIO_PAD_CONFIG_10 \
                                0x000000C8

#define OCP_SHARED_O_GPIO_PAD_CONFIG_11 \
                                0x000000CC

#define OCP_SHARED_O_GPIO_PAD_CONFIG_12 \
                                0x000000D0

#define OCP_SHARED_O_GPIO_PAD_CONFIG_13 \
                                0x000000D4

#define OCP_SHARED_O_GPIO_PAD_CONFIG_14 \
                                0x000000D8

#define OCP_SHARED_O_GPIO_PAD_CONFIG_15 \
                                0x000000DC

#define OCP_SHARED_O_GPIO_PAD_CONFIG_16 \
                                0x000000E0

#define OCP_SHARED_O_GPIO_PAD_CONFIG_17 \
                                0x000000E4

#define OCP_SHARED_O_GPIO_PAD_CONFIG_18 \
                                0x000000E8

#define OCP_SHARED_O_GPIO_PAD_CONFIG_19 \
                                0x000000EC

#define OCP_SHARED_O_GPIO_PAD_CONFIG_20 \
                                0x000000F0

#define OCP_SHARED_O_GPIO_PAD_CONFIG_21 \
                                0x000000F4

#define OCP_SHARED_O_GPIO_PAD_CONFIG_22 \
                                0x000000F8

#define OCP_SHARED_O_GPIO_PAD_CONFIG_23 \
                                0x000000FC

#define OCP_SHARED_O_GPIO_PAD_CONFIG_24 \
                                0x00000100

#define OCP_SHARED_O_GPIO_PAD_CONFIG_25 \
                                0x00000104

#define OCP_SHARED_O_GPIO_PAD_CONFIG_26 \
                                0x00000108

#define OCP_SHARED_O_GPIO_PAD_CONFIG_27 \
                                0x0000010C

#define OCP_SHARED_O_GPIO_PAD_CONFIG_28 \
                                0x00000110

#define OCP_SHARED_O_GPIO_PAD_CONFIG_29 \
                                0x00000114

#define OCP_SHARED_O_GPIO_PAD_CONFIG_30 \
                                0x00000118

#define OCP_SHARED_O_GPIO_PAD_CONFIG_31 \
                                0x0000011C

#define OCP_SHARED_O_GPIO_PAD_CONFIG_32 \
                                0x00000120

#define OCP_SHARED_O_GPIO_PAD_CONFIG_33 \
                                0x00000124

#define OCP_SHARED_O_GPIO_PAD_CONFIG_34 \
                                0x00000128

#define OCP_SHARED_O_GPIO_PAD_CONFIG_35 \
                                0x0000012C

#define OCP_SHARED_O_GPIO_PAD_CONFIG_36 \
                                0x00000130

#define OCP_SHARED_O_GPIO_PAD_CONFIG_37 \
                                0x00000134

#define OCP_SHARED_O_GPIO_PAD_CONFIG_38 \
                                0x00000138

#define OCP_SHARED_O_GPIO_PAD_CONFIG_39 \
                                0x0000013C

#define OCP_SHARED_O_GPIO_PAD_CONFIG_40 \
                                0x00000140

#define OCP_SHARED_O_GPIO_PAD_CMN_CONFIG \
                                0x00000144  
                                            
                                            
                                            

#define OCP_SHARED_O_D2D_DEV_PAD_CMN_CONFIG \
                                0x00000148

#define OCP_SHARED_O_D2D_TOSTACK_PAD_CONF \
                                0x0000014C

#define OCP_SHARED_O_D2D_MISC_PAD_CONF \
                                0x00000150

#define OCP_SHARED_O_SOP_CONF_OVERRIDE \
                                0x00000154

#define OCP_SHARED_O_CC3XX_DEBUGSS_STATUS \
                                0x00000158

#define OCP_SHARED_O_CC3XX_DEBUGMUX_SEL \
                                0x0000015C

#define OCP_SHARED_O_ALT_PC_VAL_NW \
                                0x00000160

#define OCP_SHARED_O_ALT_PC_VAL_APPS \
                                0x00000164

#define OCP_SHARED_O_SPARE_REG_4 \
                                0x00000168

#define OCP_SHARED_O_SPARE_REG_5 \
                                0x0000016C

#define OCP_SHARED_O_SH_SPI_CS_MASK \
                                0x00000170

#define OCP_SHARED_O_CC3XX_DEVICE_TYPE \
                                0x00000174

#define OCP_SHARED_O_MEM_TOPMUXCTRL_IFORCE \
                                0x00000178

#define OCP_SHARED_O_CC3XX_DEV_PACKAGE_DETECT \
                                0x0000017C

#define OCP_SHARED_O_AUTONMS_SPICLK_SEL \
                                0x00000180

#define OCP_SHARED_O_CC3XX_DEV_PADCONF \
                                0x00000184

#define OCP_SHARED_O_SPARE_REG_8 \
                                0x00000188

#define OCP_SHARED_O_SPARE_REG_6 \
                                0x0000018C

#define OCP_SHARED_O_SPARE_REG_7 \
                                0x00000190

#define OCP_SHARED_O_APPS_WLAN_ORBIT \
                                0x00000194

#define OCP_SHARED_O_APPS_WLAN_SCRATCH_PAD \
                                0x00000198










#define OCP_SHARED_SEMAPHORE1_MEM_SEMAPHORE1_M \
                                0x00000003  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_SEMAPHORE1_MEM_SEMAPHORE1_S 0






#define OCP_SHARED_SEMAPHORE2_MEM_SEMAPHORE2_M \
                                0x00000003  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_SEMAPHORE2_MEM_SEMAPHORE2_S 0






#define OCP_SHARED_SEMAPHORE3_MEM_SEMAPHORE3_M \
                                0x00000003  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_SEMAPHORE3_MEM_SEMAPHORE3_S 0






#define OCP_SHARED_SEMAPHORE4_MEM_SEMAPHORE4_M \
                                0x00000003  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_SEMAPHORE4_MEM_SEMAPHORE4_S 0






#define OCP_SHARED_SEMAPHORE5_MEM_SEMAPHORE5_M \
                                0x00000003  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_SEMAPHORE5_MEM_SEMAPHORE5_S 0






#define OCP_SHARED_SEMAPHORE6_MEM_SEMAPHORE6_M \
                                0x00000003  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_SEMAPHORE6_MEM_SEMAPHORE6_S 0






#define OCP_SHARED_SEMAPHORE7_MEM_SEMAPHORE7_M \
                                0x00000003  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_SEMAPHORE7_MEM_SEMAPHORE7_S 0






#define OCP_SHARED_SEMAPHORE8_MEM_SEMAPHORE8_M \
                                0x00000003  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_SEMAPHORE8_MEM_SEMAPHORE8_S 0






#define OCP_SHARED_SEMAPHORE9_MEM_SEMAPHORE9_M \
                                0x00000003  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_SEMAPHORE9_MEM_SEMAPHORE9_S 0






#define OCP_SHARED_SEMAPHORE10_MEM_SEMAPHORE10_M \
                                0x00000003  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_SEMAPHORE10_MEM_SEMAPHORE10_S 0






#define OCP_SHARED_SEMAPHORE11_MEM_SEMAPHORE11_M \
                                0x00000003  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_SEMAPHORE11_MEM_SEMAPHORE11_S 0






#define OCP_SHARED_SEMAPHORE12_MEM_SEMAPHORE12_M \
                                0x00000003  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_SEMAPHORE12_MEM_SEMAPHORE12_S 0






#define OCP_SHARED_IC_LOCKER_ID_MEM_IC_LOCKER_ID_M \
                                0x00000007  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_IC_LOCKER_ID_MEM_IC_LOCKER_ID_S 0






#define OCP_SHARED_MCU_SEMAPHORE_PEND_MEM_MCU_SEMAPHORE_PEND_M \
                                0x0000FFFF  
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_MCU_SEMAPHORE_PEND_MEM_MCU_SEMAPHORE_PEND_S 0






#define OCP_SHARED_WL_SEMAPHORE_PEND_MEM_WL_SEMAPHORE_PEND_M \
                                0x0000FFFF  
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_WL_SEMAPHORE_PEND_MEM_WL_SEMAPHORE_PEND_S 0






#define OCP_SHARED_PLATFORM_DETECTION_RD_ONLY_PLATFORM_DETECTION_M \
                                0x0000FFFF  
                                            
                                            

#define OCP_SHARED_PLATFORM_DETECTION_RD_ONLY_PLATFORM_DETECTION_S 0






#define OCP_SHARED_SEMAPHORES_STATUS_RD_ONLY_SEMAPHORES_STATUS_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_SEMAPHORES_STATUS_RD_ONLY_SEMAPHORES_STATUS_S 0






#define OCP_SHARED_CC3XX_CONFIG_CTRL_MEM_IC_TO_EN \
                                0x00000010  
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_CC3XX_CONFIG_CTRL_MEM_ALT_PC_EN_APPS \
                                0x00000008  
                                            
                                            

#define OCP_SHARED_CC3XX_CONFIG_CTRL_MEM_ALT_PC_EN_NW \
                                0x00000004  
                                            
                                            

#define OCP_SHARED_CC3XX_CONFIG_CTRL_MEM_EXTEND_NW_ROM \
                                0x00000002  
                                            
                                            
                                            
                                            

#define OCP_SHARED_CC3XX_CONFIG_CTRL_MEM_WLAN_HOST_INTF_SEL \
                                0x00000001  
                                            
                                            
                                            
                                            







#define OCP_SHARED_CC3XX_SHARED_MEM_SEL_LSB_MEM_SHARED_MEM_SEL_LSB_M \
                                0x3FFFFFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_CC3XX_SHARED_MEM_SEL_LSB_MEM_SHARED_MEM_SEL_LSB_S 0






#define OCP_SHARED_CC3XX_SHARED_MEM_SEL_MSB_MEM_SHARED_MEM_SEL_MSB_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_CC3XX_SHARED_MEM_SEL_MSB_MEM_SHARED_MEM_SEL_MSB_S 0






#define OCP_SHARED_WLAN_ELP_WAKE_EN_MEM_WLAN_ELP_WAKE_EN \
                                0x00000001  
                                            
                                            







#define OCP_SHARED_DEVINIT_ROM_START_ADDR_MEM_DEVINIT_ROM_START_ADDR_M \
                                0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_DEVINIT_ROM_START_ADDR_MEM_DEVINIT_ROM_START_ADDR_S 0






#define OCP_SHARED_DEVINIT_ROM_END_ADDR_MEM_DEVINIT_ROM_END_ADDR_M \
                                0xFFFFFFFF  
                                            
                                            
                                            

#define OCP_SHARED_DEVINIT_ROM_END_ADDR_MEM_DEVINIT_ROM_END_ADDR_S 0






#define OCP_SHARED_SSBD_SEED_MEM_SSBD_SEED_M \
                                0xFFFFFFFF  
                                            
                                            
                                            

#define OCP_SHARED_SSBD_SEED_MEM_SSBD_SEED_S 0






#define OCP_SHARED_SSBD_CHK_MEM_SSBD_CHK_M \
                                0xFFFFFFFF  
                                            
                                            
                                            

#define OCP_SHARED_SSBD_CHK_MEM_SSBD_CHK_S 0






#define OCP_SHARED_SSBD_POLY_SEL_MEM_SSBD_POLY_SEL_M \
                                0x00000003  
                                            
                                            
                                            

#define OCP_SHARED_SSBD_POLY_SEL_MEM_SSBD_POLY_SEL_S 0






#define OCP_SHARED_SPARE_REG_0_MEM_SPARE_REG_0_M \
                                0xFFFFFFFF  
                                            
                                            
                                            
                                            

#define OCP_SHARED_SPARE_REG_0_MEM_SPARE_REG_0_S 0






#define OCP_SHARED_SPARE_REG_1_MEM_SPARE_REG_1_M \
                                0xFFFFFFFF  

#define OCP_SHARED_SPARE_REG_1_MEM_SPARE_REG_1_S 0






#define OCP_SHARED_SPARE_REG_2_MEM_SPARE_REG_2_M \
                                0xFFFFFFFF  

#define OCP_SHARED_SPARE_REG_2_MEM_SPARE_REG_2_S 0






#define OCP_SHARED_SPARE_REG_3_MEM_SPARE_REG_3_M \
                                0xFFFFFFFF  

#define OCP_SHARED_SPARE_REG_3_MEM_SPARE_REG_3_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_0_MEM_GPIO_PAD_CONFIG_0_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_0_MEM_GPIO_PAD_CONFIG_0_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_1_MEM_GPIO_PAD_CONFIG_1_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_1_MEM_GPIO_PAD_CONFIG_1_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_2_MEM_GPIO_PAD_CONFIG_2_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_2_MEM_GPIO_PAD_CONFIG_2_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_3_MEM_GPIO_PAD_CONFIG_3_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_3_MEM_GPIO_PAD_CONFIG_3_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_4_MEM_GPIO_PAD_CONFIG_4_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_4_MEM_GPIO_PAD_CONFIG_4_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_5_MEM_GPIO_PAD_CONFIG_5_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_5_MEM_GPIO_PAD_CONFIG_5_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_6_MEM_GPIO_PAD_CONFIG_6_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_6_MEM_GPIO_PAD_CONFIG_6_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_7_MEM_GPIO_PAD_CONFIG_7_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_7_MEM_GPIO_PAD_CONFIG_7_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_8_MEM_GPIO_PAD_CONFIG_8_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_8_MEM_GPIO_PAD_CONFIG_8_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_9_MEM_GPIO_PAD_CONFIG_9_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_9_MEM_GPIO_PAD_CONFIG_9_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_10_MEM_GPIO_PAD_CONFIG_10_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_10_MEM_GPIO_PAD_CONFIG_10_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_11_MEM_GPIO_PAD_CONFIG_11_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_11_MEM_GPIO_PAD_CONFIG_11_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_12_MEM_GPIO_PAD_CONFIG_12_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_12_MEM_GPIO_PAD_CONFIG_12_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_13_MEM_GPIO_PAD_CONFIG_13_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_13_MEM_GPIO_PAD_CONFIG_13_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_14_MEM_GPIO_PAD_CONFIG_14_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_14_MEM_GPIO_PAD_CONFIG_14_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_15_MEM_GPIO_PAD_CONFIG_15_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_15_MEM_GPIO_PAD_CONFIG_15_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_16_MEM_GPIO_PAD_CONFIG_16_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_16_MEM_GPIO_PAD_CONFIG_16_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_17_MEM_GPIO_PAD_CONFIG_17_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_17_MEM_GPIO_PAD_CONFIG_17_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_18_MEM_GPIO_PAD_CONFIG_18_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_18_MEM_GPIO_PAD_CONFIG_18_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_19_MEM_GPIO_PAD_CONFIG_19_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_19_MEM_GPIO_PAD_CONFIG_19_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_20_MEM_GPIO_PAD_CONFIG_20_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_20_MEM_GPIO_PAD_CONFIG_20_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_21_MEM_GPIO_PAD_CONFIG_21_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_21_MEM_GPIO_PAD_CONFIG_21_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_22_MEM_GPIO_PAD_CONFIG_22_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_22_MEM_GPIO_PAD_CONFIG_22_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_23_MEM_GPIO_PAD_CONFIG_23_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_23_MEM_GPIO_PAD_CONFIG_23_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_24_MEM_GPIO_PAD_CONFIG_24_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_24_MEM_GPIO_PAD_CONFIG_24_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_25_MEM_GPIO_PAD_CONFIG_25_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_25_MEM_GPIO_PAD_CONFIG_25_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_26_MEM_GPIO_PAD_CONFIG_26_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_26_MEM_GPIO_PAD_CONFIG_26_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_27_MEM_GPIO_PAD_CONFIG_27_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_27_MEM_GPIO_PAD_CONFIG_27_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_28_MEM_GPIO_PAD_CONFIG_28_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_28_MEM_GPIO_PAD_CONFIG_28_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_29_MEM_GPIO_PAD_CONFIG_29_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_29_MEM_GPIO_PAD_CONFIG_29_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_30_MEM_GPIO_PAD_CONFIG_30_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_30_MEM_GPIO_PAD_CONFIG_30_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_31_MEM_GPIO_PAD_CONFIG_31_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_31_MEM_GPIO_PAD_CONFIG_31_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_32_MEM_GPIO_PAD_CONFIG_32_M \
                                0x00000FFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_32_MEM_GPIO_PAD_CONFIG_32_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_33_MEM_GPIO_PAD_CONFIG_33_M \
                                0x0000003F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_33_MEM_GPIO_PAD_CONFIG_33_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_34_MEM_GPIO_PAD_CONFIG_34_M \
                                0x0000003F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_34_MEM_GPIO_PAD_CONFIG_34_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_35_MEM_GPIO_PAD_CONFIG_35_M \
                                0x0000003F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_35_MEM_GPIO_PAD_CONFIG_35_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_36_MEM_GPIO_PAD_CONFIG_36_M \
                                0x0000003F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_36_MEM_GPIO_PAD_CONFIG_36_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_37_MEM_GPIO_PAD_CONFIG_37_M \
                                0x0000003F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_37_MEM_GPIO_PAD_CONFIG_37_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_38_MEM_GPIO_PAD_CONFIG_38_M \
                                0x0000003F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_38_MEM_GPIO_PAD_CONFIG_38_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_39_MEM_GPIO_PAD_CONFIG_39_M \
                                0x0000003F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_39_MEM_GPIO_PAD_CONFIG_39_S 0






#define OCP_SHARED_GPIO_PAD_CONFIG_40_MEM_GPIO_PAD_CONFIG_40_M \
                                0x0007FFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CONFIG_40_MEM_GPIO_PAD_CONFIG_40_S 0






#define OCP_SHARED_GPIO_PAD_CMN_CONFIG_MEM_D2D_ISO_A_EN \
                                0x00000080  
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CMN_CONFIG_MEM_D2D_ISO_Y_EN \
                                0x00000040  
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CMN_CONFIG_MEM_PAD_JTAG_IDIEN \
                                0x00000020  
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CMN_CONFIG_MEM_PAD_HYSTVAL_M \
                                0x00000018  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CMN_CONFIG_MEM_PAD_HYSTVAL_S 3
#define OCP_SHARED_GPIO_PAD_CMN_CONFIG_MEM_PAD_HYSTEN \
                                0x00000004  
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CMN_CONFIG_MEM_PAD_IBIASEN \
                                0x00000002  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_GPIO_PAD_CMN_CONFIG_MEM_PAD_IDIEN \
                                0x00000001  
                                            
                                            
                                            
                                            







#define OCP_SHARED_D2D_DEV_PAD_CMN_CONFIG_MEM_DEV_PAD_CMN_CONF_M \
                                0x0000003F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_D2D_DEV_PAD_CMN_CONFIG_MEM_DEV_PAD_CMN_CONF_S 0






#define OCP_SHARED_D2D_TOSTACK_PAD_CONF_MEM_D2D_TOSTACK_PAD_CONF_M \
                                0x1FFFFFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_D2D_TOSTACK_PAD_CONF_MEM_D2D_TOSTACK_PAD_CONF_S 0






#define OCP_SHARED_D2D_MISC_PAD_CONF_MEM_D2D_POR_RESET_N \
                                0x00000200  
                                            
                                            
                                            
                                            

#define OCP_SHARED_D2D_MISC_PAD_CONF_MEM_D2D_RESET_N \
                                0x00000100  
                                            
                                            

#define OCP_SHARED_D2D_MISC_PAD_CONF_MEM_D2D_HCLK \
                                0x00000080  
                                            
                                            

#define OCP_SHARED_D2D_MISC_PAD_CONF_MEM_D2D_JTAG_TCK \
                                0x00000040  
                                            
                                            

#define OCP_SHARED_D2D_MISC_PAD_CONF_MEM_D2D_JTAG_TMS \
                                0x00000020  
                                            
                                            

#define OCP_SHARED_D2D_MISC_PAD_CONF_MEM_D2D_JTAG_TDI \
                                0x00000010  
                                            
                                            

#define OCP_SHARED_D2D_MISC_PAD_CONF_MEM_D2D_PIOSC \
                                0x00000008  
                                            
                                            

#define OCP_SHARED_D2D_MISC_PAD_CONF_MEM_D2D_SPARE_M \
                                0x00000007  
                                            
                                            
                                            

#define OCP_SHARED_D2D_MISC_PAD_CONF_MEM_D2D_SPARE_S 0






#define OCP_SHARED_SOP_CONF_OVERRIDE_MEM_SOP_CONF_OVERRIDE \
                                0x00000001  
                                            
                                            
                                            







#define OCP_SHARED_CC3XX_DEBUGSS_STATUS_APPS_MCU_JTAGNSW \
                                0x00000020  
                                            
                                            
                                            
                                            

#define OCP_SHARED_CC3XX_DEBUGSS_STATUS_CJTAG_BYPASS_STATUS \
                                0x00000010  

#define OCP_SHARED_CC3XX_DEBUGSS_STATUS_SW_INTERFACE_SEL_STATUS \
                                0x00000008  

#define OCP_SHARED_CC3XX_DEBUGSS_STATUS_APPS_TAP_ENABLE_STATUS \
                                0x00000004  

#define OCP_SHARED_CC3XX_DEBUGSS_STATUS_TAPS_ENABLE_STATUS \
                                0x00000002  

#define OCP_SHARED_CC3XX_DEBUGSS_STATUS_SSBD_UNLOCK \
                                0x00000001  







#define OCP_SHARED_CC3XX_DEBUGMUX_SEL_MEM_CC3XX_DEBUGMUX_SEL_M \
                                0x0000FFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_CC3XX_DEBUGMUX_SEL_MEM_CC3XX_DEBUGMUX_SEL_S 0






#define OCP_SHARED_ALT_PC_VAL_NW_MEM_ALT_PC_VAL_NW_M \
                                0xFFFFFFFF  
                                            
                                            

#define OCP_SHARED_ALT_PC_VAL_NW_MEM_ALT_PC_VAL_NW_S 0






#define OCP_SHARED_ALT_PC_VAL_APPS_MEM_ALT_PC_VAL_APPS_M \
                                0xFFFFFFFF  
                                            
                                            

#define OCP_SHARED_ALT_PC_VAL_APPS_MEM_ALT_PC_VAL_APPS_S 0






#define OCP_SHARED_SPARE_REG_4_MEM_SPARE_REG_4_M \
                                0xFFFFFFFE  

#define OCP_SHARED_SPARE_REG_4_MEM_SPARE_REG_4_S 1
#define OCP_SHARED_SPARE_REG_4_INVERT_D2D_INTERFACE \
                                0x00000001  
                                            
                                            







#define OCP_SHARED_SPARE_REG_5_MEM_SPARE_REG_5_M \
                                0xFFFFFFFF  

#define OCP_SHARED_SPARE_REG_5_MEM_SPARE_REG_5_S 0






#define OCP_SHARED_SH_SPI_CS_MASK_MEM_SH_SPI_CS_MASK_M \
                                0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_SH_SPI_CS_MASK_MEM_SH_SPI_CS_MASK_S 0






#define OCP_SHARED_CC3XX_DEVICE_TYPE_DEVICE_TYPE_reserved_M \
                                0x00000060  

#define OCP_SHARED_CC3XX_DEVICE_TYPE_DEVICE_TYPE_reserved_S 5
#define OCP_SHARED_CC3XX_DEVICE_TYPE_DEVICE_TYPE_M \
                                0x0000001F  

#define OCP_SHARED_CC3XX_DEVICE_TYPE_DEVICE_TYPE_S 0






#define OCP_SHARED_MEM_TOPMUXCTRL_IFORCE_MEM_TOPMUXCTRL_IFORCE1_M \
                                0x000000F0  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_MEM_TOPMUXCTRL_IFORCE_MEM_TOPMUXCTRL_IFORCE1_S 4
#define OCP_SHARED_MEM_TOPMUXCTRL_IFORCE_MEM_TOPMUXCTRL_IFORCE_M \
                                0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_MEM_TOPMUXCTRL_IFORCE_MEM_TOPMUXCTRL_IFORCE_S 0






#define OCP_SHARED_CC3XX_DEV_PACKAGE_DETECT_DEV_PKG_DETECT \
                                0x00000001  
                                            







#define OCP_SHARED_AUTONMS_SPICLK_SEL_MEM_AUTONOMOUS_BYPASS \
                                0x00000002  
                                            
                                            
                                            
                                            

#define OCP_SHARED_AUTONMS_SPICLK_SEL_MEM_AUTONMS_SPICLK_SEL \
                                0x00000001  
                                            
                                            
                                            
                                            
                                            
                                            







#define OCP_SHARED_CC3XX_DEV_PADCONF_MEM_CC3XX_DEV_PADCONF_M \
                                0x0000FFFF

#define OCP_SHARED_CC3XX_DEV_PADCONF_MEM_CC3XX_DEV_PADCONF_S 0






#define OCP_SHARED_IDMEM_TIM_UPDATE_MEM_IDMEM_TIM_UPDATE_M \
                                0xFFFFFFFF

#define OCP_SHARED_IDMEM_TIM_UPDATE_MEM_IDMEM_TIM_UPDATE_S 0






#define OCP_SHARED_SPARE_REG_6_MEM_SPARE_REG_6_M \
                                0xFFFFFFFF  

#define OCP_SHARED_SPARE_REG_6_MEM_SPARE_REG_6_S 0






#define OCP_SHARED_SPARE_REG_7_MEM_SPARE_REG_7_M \
                                0xFFFFFFFF  

#define OCP_SHARED_SPARE_REG_7_MEM_SPARE_REG_7_S 0






#define OCP_SHARED_APPS_WLAN_ORBIT_mem_orbit_spare_M \
                                0xFFFFFC00  

#define OCP_SHARED_APPS_WLAN_ORBIT_mem_orbit_spare_S 10
#define OCP_SHARED_APPS_WLAN_ORBIT_mem_orbit_test_status \
                                0x00000200  
                                            
                                            
                                            
                                            

#define OCP_SHARED_APPS_WLAN_ORBIT_mem_orbit_test_exec \
                                0x00000100  
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define OCP_SHARED_APPS_WLAN_ORBIT_mem_orbit_test_id_M \
                                0x000000FC  
                                            

#define OCP_SHARED_APPS_WLAN_ORBIT_mem_orbit_test_id_S 2
#define OCP_SHARED_APPS_WLAN_ORBIT_mem_orbit_halt_proc \
                                0x00000002  
                                            
                                            

#define OCP_SHARED_APPS_WLAN_ORBIT_mem_orbit_test_mode \
                                0x00000001  
                                            
                                            
                                            
                                            







#define OCP_SHARED_APPS_WLAN_SCRATCH_PAD_MEM_APPS_WLAN_SCRATCH_PAD_M \
                                0xFFFFFFFF  

#define OCP_SHARED_APPS_WLAN_SCRATCH_PAD_MEM_APPS_WLAN_SCRATCH_PAD_S 0



#endif 
