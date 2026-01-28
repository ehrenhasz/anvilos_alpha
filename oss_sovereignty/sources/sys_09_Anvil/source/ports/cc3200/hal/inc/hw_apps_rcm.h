


































#ifndef __HW_APPS_RCM_H__
#define __HW_APPS_RCM_H__






#define APPS_RCM_O_CAMERA_CLK_GEN \
                                0x00000000

#define APPS_RCM_O_CAMERA_CLK_GATING \
                                0x00000004

#define APPS_RCM_O_CAMERA_SOFT_RESET \
                                0x00000008

#define APPS_RCM_O_MCASP_CLK_GATING \
                                0x00000014

#define APPS_RCM_O_MCASP_SOFT_RESET \
                                0x00000018

#define APPS_RCM_O_MMCHS_CLK_GEN \
                                0x00000020

#define APPS_RCM_O_MMCHS_CLK_GATING \
                                0x00000024

#define APPS_RCM_O_MMCHS_SOFT_RESET \
                                0x00000028

#define APPS_RCM_O_MCSPI_A1_CLK_GEN \
                                0x0000002C

#define APPS_RCM_O_MCSPI_A1_CLK_GATING \
                                0x00000030

#define APPS_RCM_O_MCSPI_A1_SOFT_RESET \
                                0x00000034

#define APPS_RCM_O_MCSPI_A2_CLK_GEN \
                                0x00000038

#define APPS_RCM_O_MCSPI_A2_CLK_GATING \
                                0x00000040

#define APPS_RCM_O_MCSPI_A2_SOFT_RESET \
                                0x00000044

#define APPS_RCM_O_UDMA_A_CLK_GATING \
                                0x00000048

#define APPS_RCM_O_UDMA_A_SOFT_RESET \
                                0x0000004C

#define APPS_RCM_O_GPIO_A_CLK_GATING \
                                0x00000050

#define APPS_RCM_O_GPIO_A_SOFT_RESET \
                                0x00000054

#define APPS_RCM_O_GPIO_B_CLK_GATING \
                                0x00000058

#define APPS_RCM_O_GPIO_B_SOFT_RESET \
                                0x0000005C

#define APPS_RCM_O_GPIO_C_CLK_GATING \
                                0x00000060

#define APPS_RCM_O_GPIO_C_SOFT_RESET \
                                0x00000064

#define APPS_RCM_O_GPIO_D_CLK_GATING \
                                0x00000068

#define APPS_RCM_O_GPIO_D_SOFT_RESET \
                                0x0000006C

#define APPS_RCM_O_GPIO_E_CLK_GATING \
                                0x00000070

#define APPS_RCM_O_GPIO_E_SOFT_RESET \
                                0x00000074

#define APPS_RCM_O_WDOG_A_CLK_GATING \
                                0x00000078

#define APPS_RCM_O_WDOG_A_SOFT_RESET \
                                0x0000007C

#define APPS_RCM_O_UART_A0_CLK_GATING \
                                0x00000080

#define APPS_RCM_O_UART_A0_SOFT_RESET \
                                0x00000084

#define APPS_RCM_O_UART_A1_CLK_GATING \
                                0x00000088

#define APPS_RCM_O_UART_A1_SOFT_RESET \
                                0x0000008C

#define APPS_RCM_O_GPT_A0_CLK_GATING \
                                0x00000090

#define APPS_RCM_O_GPT_A0_SOFT_RESET \
                                0x00000094

#define APPS_RCM_O_GPT_A1_CLK_GATING \
                                0x00000098

#define APPS_RCM_O_GPT_A1_SOFT_RESET \
                                0x0000009C

#define APPS_RCM_O_GPT_A2_CLK_GATING \
                                0x000000A0

#define APPS_RCM_O_GPT_A2_SOFT_RESET \
                                0x000000A4

#define APPS_RCM_O_GPT_A3_CLK_GATING \
                                0x000000A8

#define APPS_RCM_O_GPT_A3_SOFT_RESET \
                                0x000000AC

#define APPS_RCM_O_MCASP_FRAC_CLK_CONFIG0 \
                                0x000000B0

#define APPS_RCM_O_MCASP_FRAC_CLK_CONFIG1 \
                                0x000000B4

#define APPS_RCM_O_CRYPTO_CLK_GATING \
                                0x000000B8

#define APPS_RCM_O_CRYPTO_SOFT_RESET \
                                0x000000BC

#define APPS_RCM_O_MCSPI_S0_CLK_GATING \
                                0x000000C8

#define APPS_RCM_O_MCSPI_S0_SOFT_RESET \
                                0x000000CC

#define APPS_RCM_O_MCSPI_S0_CLKDIV_CFG \
                                0x000000D0

#define APPS_RCM_O_I2C_CLK_GATING \
                                0x000000D8

#define APPS_RCM_O_I2C_SOFT_RESET \
                                0x000000DC

#define APPS_RCM_O_APPS_LPDS_REQ \
                                0x000000E4

#define APPS_RCM_O_APPS_TURBO_REQ \
                                0x000000EC

#define APPS_RCM_O_APPS_DSLP_WAKE_CONFIG \
                                0x00000108

#define APPS_RCM_O_APPS_DSLP_WAKE_TIMER_CFG \
                                0x0000010C

#define APPS_RCM_O_APPS_RCM_SLP_WAKE_ENABLE \
                                0x00000110

#define APPS_RCM_O_APPS_SLP_WAKETIMER_CFG \
                                0x00000114

#define APPS_RCM_O_APPS_TO_NWP_WAKE_REQUEST \
                                0x00000118

#define APPS_RCM_O_APPS_RCM_INTERRUPT_STATUS \
                                0x00000120

#define APPS_RCM_O_APPS_RCM_INTERRUPT_ENABLE \
                                0x00000124











#define APPS_RCM_CAMERA_CLK_GEN_CAMERA_PLLCKDIV_OFF_TIME_M \
                                0x00000700  
                                            
                                            
                                            
                                            
                                            

#define APPS_RCM_CAMERA_CLK_GEN_CAMERA_PLLCKDIV_OFF_TIME_S 8
#define APPS_RCM_CAMERA_CLK_GEN_NU1_M \
                                0x000000F8

#define APPS_RCM_CAMERA_CLK_GEN_NU1_S 3
#define APPS_RCM_CAMERA_CLK_GEN_CAMERA_PLLCKDIV_ON_TIME_M \
                                0x00000007  
                                            
                                            
                                            
                                            
                                            

#define APPS_RCM_CAMERA_CLK_GEN_CAMERA_PLLCKDIV_ON_TIME_S 0






#define APPS_RCM_CAMERA_CLK_GATING_NU1_M \
                                0x00FE0000

#define APPS_RCM_CAMERA_CLK_GATING_NU1_S 17
#define APPS_RCM_CAMERA_CLK_GATING_CAMERA_DSLP_CLK_ENABLE \
                                0x00010000  
                                            

#define APPS_RCM_CAMERA_CLK_GATING_NU2_M \
                                0x0000FE00

#define APPS_RCM_CAMERA_CLK_GATING_NU2_S 9
#define APPS_RCM_CAMERA_CLK_GATING_CAMERA_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_CAMERA_CLK_GATING_NU3_M \
                                0x000000FE

#define APPS_RCM_CAMERA_CLK_GATING_NU3_S 1
#define APPS_RCM_CAMERA_CLK_GATING_CAMERA_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_CAMERA_SOFT_RESET_CAMERA_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_CAMERA_SOFT_RESET_CAMERA_SOFT_RESET \
                                0x00000001  
                                            
                                            







#define APPS_RCM_MCASP_CLK_GATING_NU1_M \
                                0x00FE0000

#define APPS_RCM_MCASP_CLK_GATING_NU1_S 17
#define APPS_RCM_MCASP_CLK_GATING_MCASP_DSLP_CLK_ENABLE \
                                0x00010000  
                                            

#define APPS_RCM_MCASP_CLK_GATING_NU2_M \
                                0x0000FE00

#define APPS_RCM_MCASP_CLK_GATING_NU2_S 9
#define APPS_RCM_MCASP_CLK_GATING_MCASP_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_MCASP_CLK_GATING_NU3_M \
                                0x000000FE

#define APPS_RCM_MCASP_CLK_GATING_NU3_S 1
#define APPS_RCM_MCASP_CLK_GATING_MCASP_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_MCASP_SOFT_RESET_MCASP_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_MCASP_SOFT_RESET_MCASP_SOFT_RESET \
                                0x00000001  
                                            
                                            







#define APPS_RCM_MMCHS_CLK_GEN_MMCHS_PLLCKDIV_OFF_TIME_M \
                                0x00000700  
                                            
                                            
                                            
                                            
                                            

#define APPS_RCM_MMCHS_CLK_GEN_MMCHS_PLLCKDIV_OFF_TIME_S 8
#define APPS_RCM_MMCHS_CLK_GEN_NU1_M \
                                0x000000F8

#define APPS_RCM_MMCHS_CLK_GEN_NU1_S 3
#define APPS_RCM_MMCHS_CLK_GEN_MMCHS_PLLCKDIV_ON_TIME_M \
                                0x00000007  
                                            
                                            
                                            
                                            
                                            

#define APPS_RCM_MMCHS_CLK_GEN_MMCHS_PLLCKDIV_ON_TIME_S 0






#define APPS_RCM_MMCHS_CLK_GATING_NU1_M \
                                0x00FE0000

#define APPS_RCM_MMCHS_CLK_GATING_NU1_S 17
#define APPS_RCM_MMCHS_CLK_GATING_MMCHS_DSLP_CLK_ENABLE \
                                0x00010000  
                                            

#define APPS_RCM_MMCHS_CLK_GATING_NU2_M \
                                0x0000FE00

#define APPS_RCM_MMCHS_CLK_GATING_NU2_S 9
#define APPS_RCM_MMCHS_CLK_GATING_MMCHS_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_MMCHS_CLK_GATING_NU3_M \
                                0x000000FE

#define APPS_RCM_MMCHS_CLK_GATING_NU3_S 1
#define APPS_RCM_MMCHS_CLK_GATING_MMCHS_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_MMCHS_SOFT_RESET_MMCHS_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_MMCHS_SOFT_RESET_MMCHS_SOFT_RESET \
                                0x00000001  
                                            
                                            







#define APPS_RCM_MCSPI_A1_CLK_GEN_MCSPI_A1_BAUD_CLK_SEL \
                                0x00010000  
                                            
                                            

#define APPS_RCM_MCSPI_A1_CLK_GEN_NU1_M \
                                0x0000F800

#define APPS_RCM_MCSPI_A1_CLK_GEN_NU1_S 11
#define APPS_RCM_MCSPI_A1_CLK_GEN_MCSPI_A1_PLLCLKDIV_OFF_TIME_M \
                                0x00000700  
                                            
                                            
                                            
                                            
                                            

#define APPS_RCM_MCSPI_A1_CLK_GEN_MCSPI_A1_PLLCLKDIV_OFF_TIME_S 8
#define APPS_RCM_MCSPI_A1_CLK_GEN_NU2_M \
                                0x000000F8

#define APPS_RCM_MCSPI_A1_CLK_GEN_NU2_S 3
#define APPS_RCM_MCSPI_A1_CLK_GEN_MCSPI_A1_PLLCLKDIV_ON_TIME_M \
                                0x00000007  
                                            
                                            
                                            
                                            
                                            

#define APPS_RCM_MCSPI_A1_CLK_GEN_MCSPI_A1_PLLCLKDIV_ON_TIME_S 0






#define APPS_RCM_MCSPI_A1_CLK_GATING_NU1_M \
                                0x00FE0000

#define APPS_RCM_MCSPI_A1_CLK_GATING_NU1_S 17
#define APPS_RCM_MCSPI_A1_CLK_GATING_MCSPI_A1_DSLP_CLK_ENABLE \
                                0x00010000  
                                            

#define APPS_RCM_MCSPI_A1_CLK_GATING_NU2_M \
                                0x0000FE00

#define APPS_RCM_MCSPI_A1_CLK_GATING_NU2_S 9
#define APPS_RCM_MCSPI_A1_CLK_GATING_MCSPI_A1_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_MCSPI_A1_CLK_GATING_NU3_M \
                                0x000000FE

#define APPS_RCM_MCSPI_A1_CLK_GATING_NU3_S 1
#define APPS_RCM_MCSPI_A1_CLK_GATING_MCSPI_A1_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_MCSPI_A1_SOFT_RESET_MCSPI_A1_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_MCSPI_A1_SOFT_RESET_MCSPI_A1_SOFT_RESET \
                                0x00000001  
                                            
                                            







#define APPS_RCM_MCSPI_A2_CLK_GEN_MCSPI_A2_BAUD_CLK_SEL \
                                0x00010000  
                                            
                                            

#define APPS_RCM_MCSPI_A2_CLK_GEN_NU1_M \
                                0x0000F800

#define APPS_RCM_MCSPI_A2_CLK_GEN_NU1_S 11
#define APPS_RCM_MCSPI_A2_CLK_GEN_MCSPI_A2_PLLCKDIV_OFF_TIME_M \
                                0x00000700  
                                            
                                            
                                            
                                            
                                            

#define APPS_RCM_MCSPI_A2_CLK_GEN_MCSPI_A2_PLLCKDIV_OFF_TIME_S 8
#define APPS_RCM_MCSPI_A2_CLK_GEN_NU2_M \
                                0x000000F8

#define APPS_RCM_MCSPI_A2_CLK_GEN_NU2_S 3
#define APPS_RCM_MCSPI_A2_CLK_GEN_MCSPI_A2_PLLCKDIV_ON_TIME_M \
                                0x00000007  
                                            
                                            
                                            
                                            
                                            

#define APPS_RCM_MCSPI_A2_CLK_GEN_MCSPI_A2_PLLCKDIV_ON_TIME_S 0






#define APPS_RCM_MCSPI_A2_CLK_GATING_NU1_M \
                                0x00FE0000

#define APPS_RCM_MCSPI_A2_CLK_GATING_NU1_S 17
#define APPS_RCM_MCSPI_A2_CLK_GATING_MCSPI_A2_DSLP_CLK_ENABLE \
                                0x00010000  
                                            

#define APPS_RCM_MCSPI_A2_CLK_GATING_NU2_M \
                                0x0000FE00

#define APPS_RCM_MCSPI_A2_CLK_GATING_NU2_S 9
#define APPS_RCM_MCSPI_A2_CLK_GATING_MCSPI_A2_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_MCSPI_A2_CLK_GATING_NU3_M \
                                0x000000FE

#define APPS_RCM_MCSPI_A2_CLK_GATING_NU3_S 1
#define APPS_RCM_MCSPI_A2_CLK_GATING_MCSPI_A2_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_MCSPI_A2_SOFT_RESET_MCSPI_A2_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_MCSPI_A2_SOFT_RESET_MCSPI_A2_SOFT_RESET \
                                0x00000001  
                                            
                                            







#define APPS_RCM_UDMA_A_CLK_GATING_UDMA_A_DSLP_CLK_ENABLE \
                                0x00010000  
                                            
                                            
                                            

#define APPS_RCM_UDMA_A_CLK_GATING_NU1_M \
                                0x0000FE00

#define APPS_RCM_UDMA_A_CLK_GATING_NU1_S 9
#define APPS_RCM_UDMA_A_CLK_GATING_UDMA_A_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_UDMA_A_CLK_GATING_NU2_M \
                                0x000000FE

#define APPS_RCM_UDMA_A_CLK_GATING_NU2_S 1
#define APPS_RCM_UDMA_A_CLK_GATING_UDMA_A_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_UDMA_A_SOFT_RESET_UDMA_A_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_UDMA_A_SOFT_RESET_UDMA_A_SOFT_RESET \
                                0x00000001  
                                            







#define APPS_RCM_GPIO_A_CLK_GATING_GPIO_A_DSLP_CLK_ENABLE \
                                0x00010000  
                                            
                                            
                                            

#define APPS_RCM_GPIO_A_CLK_GATING_NU1_M \
                                0x0000FE00

#define APPS_RCM_GPIO_A_CLK_GATING_NU1_S 9
#define APPS_RCM_GPIO_A_CLK_GATING_GPIO_A_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_GPIO_A_CLK_GATING_NU2_M \
                                0x000000FE

#define APPS_RCM_GPIO_A_CLK_GATING_NU2_S 1
#define APPS_RCM_GPIO_A_CLK_GATING_GPIO_A_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_GPIO_A_SOFT_RESET_GPIO_A_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_GPIO_A_SOFT_RESET_GPIO_A_SOFT_RESET \
                                0x00000001  
                                            







#define APPS_RCM_GPIO_B_CLK_GATING_GPIO_B_DSLP_CLK_ENABLE \
                                0x00010000  
                                            
                                            
                                            

#define APPS_RCM_GPIO_B_CLK_GATING_NU1_M \
                                0x0000FE00

#define APPS_RCM_GPIO_B_CLK_GATING_NU1_S 9
#define APPS_RCM_GPIO_B_CLK_GATING_GPIO_B_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_GPIO_B_CLK_GATING_NU2_M \
                                0x000000FE

#define APPS_RCM_GPIO_B_CLK_GATING_NU2_S 1
#define APPS_RCM_GPIO_B_CLK_GATING_GPIO_B_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_GPIO_B_SOFT_RESET_GPIO_B_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_GPIO_B_SOFT_RESET_GPIO_B_SOFT_RESET \
                                0x00000001  
                                            







#define APPS_RCM_GPIO_C_CLK_GATING_GPIO_C_DSLP_CLK_ENABLE \
                                0x00010000  
                                            
                                            
                                            

#define APPS_RCM_GPIO_C_CLK_GATING_NU1_M \
                                0x0000FE00

#define APPS_RCM_GPIO_C_CLK_GATING_NU1_S 9
#define APPS_RCM_GPIO_C_CLK_GATING_GPIO_C_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_GPIO_C_CLK_GATING_NU2_M \
                                0x000000FE

#define APPS_RCM_GPIO_C_CLK_GATING_NU2_S 1
#define APPS_RCM_GPIO_C_CLK_GATING_GPIO_C_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_GPIO_C_SOFT_RESET_GPIO_C_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_GPIO_C_SOFT_RESET_GPIO_C_SOFT_RESET \
                                0x00000001  
                                            







#define APPS_RCM_GPIO_D_CLK_GATING_GPIO_D_DSLP_CLK_ENABLE \
                                0x00010000  
                                            
                                            
                                            

#define APPS_RCM_GPIO_D_CLK_GATING_NU1_M \
                                0x0000FE00

#define APPS_RCM_GPIO_D_CLK_GATING_NU1_S 9
#define APPS_RCM_GPIO_D_CLK_GATING_GPIO_D_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_GPIO_D_CLK_GATING_NU2_M \
                                0x000000FE

#define APPS_RCM_GPIO_D_CLK_GATING_NU2_S 1
#define APPS_RCM_GPIO_D_CLK_GATING_GPIO_D_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_GPIO_D_SOFT_RESET_GPIO_D_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_GPIO_D_SOFT_RESET_GPIO_D_SOFT_RESET \
                                0x00000001  
                                            







#define APPS_RCM_GPIO_E_CLK_GATING_GPIO_E_DSLP_CLK_ENABLE \
                                0x00010000  
                                            
                                            
                                            

#define APPS_RCM_GPIO_E_CLK_GATING_NU1_M \
                                0x0000FE00

#define APPS_RCM_GPIO_E_CLK_GATING_NU1_S 9
#define APPS_RCM_GPIO_E_CLK_GATING_GPIO_E_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_GPIO_E_CLK_GATING_NU2_M \
                                0x000000FE

#define APPS_RCM_GPIO_E_CLK_GATING_NU2_S 1
#define APPS_RCM_GPIO_E_CLK_GATING_GPIO_E_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_GPIO_E_SOFT_RESET_GPIO_E_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_GPIO_E_SOFT_RESET_GPIO_E_SOFT_RESET \
                                0x00000001  
                                            







#define APPS_RCM_WDOG_A_CLK_GATING_WDOG_A_BAUD_CLK_SEL_M \
                                0x03000000  
                                            

#define APPS_RCM_WDOG_A_CLK_GATING_WDOG_A_BAUD_CLK_SEL_S 24
#define APPS_RCM_WDOG_A_CLK_GATING_WDOG_A_DSLP_CLK_ENABLE \
                                0x00010000  
                                            
                                            
                                            

#define APPS_RCM_WDOG_A_CLK_GATING_NU1_M \
                                0x0000FE00

#define APPS_RCM_WDOG_A_CLK_GATING_NU1_S 9
#define APPS_RCM_WDOG_A_CLK_GATING_WDOG_A_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_WDOG_A_CLK_GATING_NU2_M \
                                0x000000FE

#define APPS_RCM_WDOG_A_CLK_GATING_NU2_S 1
#define APPS_RCM_WDOG_A_CLK_GATING_WDOG_A_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_WDOG_A_SOFT_RESET_WDOG_A_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_WDOG_A_SOFT_RESET_WDOG_A_SOFT_RESET \
                                0x00000001  
                                            







#define APPS_RCM_UART_A0_CLK_GATING_UART_A0_DSLP_CLK_ENABLE \
                                0x00010000  
                                            
                                            
                                            

#define APPS_RCM_UART_A0_CLK_GATING_NU1_M \
                                0x0000FE00

#define APPS_RCM_UART_A0_CLK_GATING_NU1_S 9
#define APPS_RCM_UART_A0_CLK_GATING_UART_A0_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_UART_A0_CLK_GATING_NU2_M \
                                0x000000FE

#define APPS_RCM_UART_A0_CLK_GATING_NU2_S 1
#define APPS_RCM_UART_A0_CLK_GATING_UART_A0_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_UART_A0_SOFT_RESET_UART_A0_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_UART_A0_SOFT_RESET_UART_A0_SOFT_RESET \
                                0x00000001  
                                            







#define APPS_RCM_UART_A1_CLK_GATING_UART_A1_DSLP_CLK_ENABLE \
                                0x00010000  
                                            
                                            
                                            

#define APPS_RCM_UART_A1_CLK_GATING_NU1_M \
                                0x0000FE00

#define APPS_RCM_UART_A1_CLK_GATING_NU1_S 9
#define APPS_RCM_UART_A1_CLK_GATING_UART_A1_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_UART_A1_CLK_GATING_NU2_M \
                                0x000000FE

#define APPS_RCM_UART_A1_CLK_GATING_NU2_S 1
#define APPS_RCM_UART_A1_CLK_GATING_UART_A1_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_UART_A1_SOFT_RESET_UART_A1_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_UART_A1_SOFT_RESET_UART_A1_SOFT_RESET \
                                0x00000001  
                                            
                                            







#define APPS_RCM_GPT_A0_CLK_GATING_GPT_A0_DSLP_CLK_ENABLE \
                                0x00010000  
                                            
                                            
                                            

#define APPS_RCM_GPT_A0_CLK_GATING_NU1_M \
                                0x0000FE00

#define APPS_RCM_GPT_A0_CLK_GATING_NU1_S 9
#define APPS_RCM_GPT_A0_CLK_GATING_GPT_A0_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_GPT_A0_CLK_GATING_NU2_M \
                                0x000000FE

#define APPS_RCM_GPT_A0_CLK_GATING_NU2_S 1
#define APPS_RCM_GPT_A0_CLK_GATING_GPT_A0_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_GPT_A0_SOFT_RESET_GPT_A0_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_GPT_A0_SOFT_RESET_GPT_A0_SOFT_RESET \
                                0x00000001  
                                            
                                            







#define APPS_RCM_GPT_A1_CLK_GATING_GPT_A1_DSLP_CLK_ENABLE \
                                0x00010000  
                                            
                                            
                                            

#define APPS_RCM_GPT_A1_CLK_GATING_NU1_M \
                                0x0000FE00

#define APPS_RCM_GPT_A1_CLK_GATING_NU1_S 9
#define APPS_RCM_GPT_A1_CLK_GATING_GPT_A1_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_GPT_A1_CLK_GATING_NU2_M \
                                0x000000FE

#define APPS_RCM_GPT_A1_CLK_GATING_NU2_S 1
#define APPS_RCM_GPT_A1_CLK_GATING_GPT_A1_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_GPT_A1_SOFT_RESET_GPT_A1_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_GPT_A1_SOFT_RESET_GPT_A1_SOFT_RESET \
                                0x00000001  
                                            
                                            







#define APPS_RCM_GPT_A2_CLK_GATING_GPT_A2_DSLP_CLK_ENABLE \
                                0x00010000  
                                            
                                            
                                            

#define APPS_RCM_GPT_A2_CLK_GATING_NU1_M \
                                0x0000FE00

#define APPS_RCM_GPT_A2_CLK_GATING_NU1_S 9
#define APPS_RCM_GPT_A2_CLK_GATING_GPT_A2_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_GPT_A2_CLK_GATING_NU2_M \
                                0x000000FE

#define APPS_RCM_GPT_A2_CLK_GATING_NU2_S 1
#define APPS_RCM_GPT_A2_CLK_GATING_GPT_A2_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_GPT_A2_SOFT_RESET_GPT_A2_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_GPT_A2_SOFT_RESET_GPT_A2_SOFT_RESET \
                                0x00000001  
                                            
                                            







#define APPS_RCM_GPT_A3_CLK_GATING_GPT_A3_DSLP_CLK_ENABLE \
                                0x00010000  
                                            
                                            
                                            

#define APPS_RCM_GPT_A3_CLK_GATING_NU1_M \
                                0x0000FE00

#define APPS_RCM_GPT_A3_CLK_GATING_NU1_S 9
#define APPS_RCM_GPT_A3_CLK_GATING_GPT_A3_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_GPT_A3_CLK_GATING_NU2_M \
                                0x000000FE

#define APPS_RCM_GPT_A3_CLK_GATING_NU2_S 1
#define APPS_RCM_GPT_A3_CLK_GATING_GPT_A3_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_GPT_A3_SOFT_RESET_GPT_A3_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_GPT_A3_SOFT_RESET_GPT_A3_SOFT_RESET \
                                0x00000001  
                                            
                                            







#define APPS_RCM_MCASP_FRAC_CLK_CONFIG0_MCASP_FRAC_DIV_DIVISOR_M \
                                0x03FF0000

#define APPS_RCM_MCASP_FRAC_CLK_CONFIG0_MCASP_FRAC_DIV_DIVISOR_S 16
#define APPS_RCM_MCASP_FRAC_CLK_CONFIG0_MCASP_FRAC_DIV_FRACTION_M \
                                0x0000FFFF

#define APPS_RCM_MCASP_FRAC_CLK_CONFIG0_MCASP_FRAC_DIV_FRACTION_S 0






#define APPS_RCM_MCASP_FRAC_CLK_CONFIG1_MCASP_FRAC_DIV_SOFT_RESET \
                                0x00010000  
                                            
                                            

#define APPS_RCM_MCASP_FRAC_CLK_CONFIG1_MCASP_FRAC_DIV_PERIOD_M \
                                0x000003FF

#define APPS_RCM_MCASP_FRAC_CLK_CONFIG1_MCASP_FRAC_DIV_PERIOD_S 0






#define APPS_RCM_CRYPTO_CLK_GATING_CRYPTO_DSLP_CLK_ENABLE \
                                0x00010000  
                                            

#define APPS_RCM_CRYPTO_CLK_GATING_NU1_M \
                                0x0000FE00

#define APPS_RCM_CRYPTO_CLK_GATING_NU1_S 9
#define APPS_RCM_CRYPTO_CLK_GATING_CRYPTO_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_CRYPTO_CLK_GATING_NU2_M \
                                0x000000FE

#define APPS_RCM_CRYPTO_CLK_GATING_NU2_S 1
#define APPS_RCM_CRYPTO_CLK_GATING_CRYPTO_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_CRYPTO_SOFT_RESET_CRYPTO_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_CRYPTO_SOFT_RESET_CRYPTO_SOFT_RESET \
                                0x00000001  
                                            
                                            







#define APPS_RCM_MCSPI_S0_CLK_GATING_MCSPI_S0_DSLP_CLK_ENABLE \
                                0x00010000  
                                            

#define APPS_RCM_MCSPI_S0_CLK_GATING_NU1_M \
                                0x0000FE00

#define APPS_RCM_MCSPI_S0_CLK_GATING_NU1_S 9
#define APPS_RCM_MCSPI_S0_CLK_GATING_MCSPI_S0_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_MCSPI_S0_CLK_GATING_NU2_M \
                                0x000000FE

#define APPS_RCM_MCSPI_S0_CLK_GATING_NU2_S 1
#define APPS_RCM_MCSPI_S0_CLK_GATING_MCSPI_S0_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_MCSPI_S0_SOFT_RESET_MCSPI_S0_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_MCSPI_S0_SOFT_RESET_MCSPI_S0_SOFT_RESET \
                                0x00000001  
                                            
                                            







#define APPS_RCM_MCSPI_S0_CLKDIV_CFG_MCSPI_S0_BAUD_CLK_SEL \
                                0x00010000  
                                            
                                            

#define APPS_RCM_MCSPI_S0_CLKDIV_CFG_NU1_M \
                                0x0000F800

#define APPS_RCM_MCSPI_S0_CLKDIV_CFG_NU1_S 11
#define APPS_RCM_MCSPI_S0_CLKDIV_CFG_MCSPI_S0_PLLCLKDIV_OFF_TIME_M \
                                0x00000700  
                                            
                                            
                                            
                                            
                                            

#define APPS_RCM_MCSPI_S0_CLKDIV_CFG_MCSPI_S0_PLLCLKDIV_OFF_TIME_S 8
#define APPS_RCM_MCSPI_S0_CLKDIV_CFG_NU2_M \
                                0x000000F8

#define APPS_RCM_MCSPI_S0_CLKDIV_CFG_NU2_S 3
#define APPS_RCM_MCSPI_S0_CLKDIV_CFG_MCSPI_S0_PLLCLKDIV_ON_TIME_M \
                                0x00000007  
                                            
                                            
                                            
                                            
                                            

#define APPS_RCM_MCSPI_S0_CLKDIV_CFG_MCSPI_S0_PLLCLKDIV_ON_TIME_S 0






#define APPS_RCM_I2C_CLK_GATING_I2C_DSLP_CLK_ENABLE \
                                0x00010000  
                                            
                                            

#define APPS_RCM_I2C_CLK_GATING_NU1_M \
                                0x0000FE00

#define APPS_RCM_I2C_CLK_GATING_NU1_S 9
#define APPS_RCM_I2C_CLK_GATING_I2C_SLP_CLK_ENABLE \
                                0x00000100  
                                            
                                            

#define APPS_RCM_I2C_CLK_GATING_NU2_M \
                                0x000000FE

#define APPS_RCM_I2C_CLK_GATING_NU2_S 1
#define APPS_RCM_I2C_CLK_GATING_I2C_RUN_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_I2C_SOFT_RESET_I2C_ENABLED_STATUS \
                                0x00000002  
                                            
                                            

#define APPS_RCM_I2C_SOFT_RESET_I2C_SOFT_RESET \
                                0x00000001  
                                            
                                            







#define APPS_RCM_APPS_LPDS_REQ_APPS_LPDS_REQ \
                                0x00000001  







#define APPS_RCM_APPS_TURBO_REQ_APPS_TURBO_REQ \
                                0x00000001  







#define APPS_RCM_APPS_DSLP_WAKE_CONFIG_DSLP_WAKE_FROM_NWP_ENABLE \
                                0x00000002  
                                            
                                            

#define APPS_RCM_APPS_DSLP_WAKE_CONFIG_DSLP_WAKE_TIMER_ENABLE \
                                0x00000001  
                                            
                                            
                                            







#define APPS_RCM_APPS_DSLP_WAKE_TIMER_CFG_DSLP_WAKE_TIMER_OPP_CFG_M \
                                0xFFFF0000  
                                            
                                            

#define APPS_RCM_APPS_DSLP_WAKE_TIMER_CFG_DSLP_WAKE_TIMER_OPP_CFG_S 16
#define APPS_RCM_APPS_DSLP_WAKE_TIMER_CFG_DSLP_WAKE_TIMER_WAKE_CFG_M \
                                0x0000FFFF  
                                            
                                            

#define APPS_RCM_APPS_DSLP_WAKE_TIMER_CFG_DSLP_WAKE_TIMER_WAKE_CFG_S 0






#define APPS_RCM_APPS_RCM_SLP_WAKE_ENABLE_SLP_WAKE_FROM_NWP_ENABLE \
                                0x00000002  
                                            
                                            

#define APPS_RCM_APPS_RCM_SLP_WAKE_ENABLE_SLP_WAKE_TIMER_ENABLE \
                                0x00000001  
                                            
                                            







#define APPS_RCM_APPS_SLP_WAKETIMER_CFG_SLP_WAKE_TIMER_CFG_M \
                                0xFFFFFFFF  
                                            
                                            

#define APPS_RCM_APPS_SLP_WAKETIMER_CFG_SLP_WAKE_TIMER_CFG_S 0






#define APPS_RCM_APPS_TO_NWP_WAKE_REQUEST_APPS_TO_NWP_WAKEUP_REQUEST \
                                0x00000001  
                                            
                                            
                                            







#define APPS_RCM_APPS_RCM_INTERRUPT_STATUS_apps_deep_sleep_timer_wake \
                                0x00000008  
                                            
                                            

#define APPS_RCM_APPS_RCM_INTERRUPT_STATUS_apps_sleep_timer_wake \
                                0x00000004  
                                            
                                            

#define APPS_RCM_APPS_RCM_INTERRUPT_STATUS_apps_deep_sleep_wake_from_nwp \
                                0x00000002  
                                            

#define APPS_RCM_APPS_RCM_INTERRUPT_STATUS_apps_sleep_wake_from_nwp \
                                0x00000001  
                                            




#endif 
