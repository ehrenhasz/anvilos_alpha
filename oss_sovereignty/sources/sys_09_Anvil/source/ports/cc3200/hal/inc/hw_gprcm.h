


































#ifndef __HW_GPRCM_H__
#define __HW_GPRCM_H__






#define GPRCM_O_APPS_SOFT_RESET 0x00000000
#define GPRCM_O_APPS_LPDS_WAKEUP_CFG \
                                0x00000004

#define GPRCM_O_APPS_LPDS_WAKEUP_SRC \
                                0x00000008

#define GPRCM_O_APPS_RESET_CAUSE \
                                0x0000000C

#define GPRCM_O_APPS_LPDS_WAKETIME_OPP_CFG \
                                0x00000010

#define GPRCM_O_APPS_SRAM_DSLP_CFG \
                                0x00000018

#define GPRCM_O_APPS_SRAM_LPDS_CFG \
                                0x0000001C

#define GPRCM_O_APPS_LPDS_WAKETIME_WAKE_CFG \
                                0x00000020

#define GPRCM_O_TOP_DIE_ENABLE  0x00000100
#define GPRCM_O_TOP_DIE_ENABLE_PARAMETERS \
                                0x00000104

#define GPRCM_O_MCU_GLOBAL_SOFT_RESET \
                                0x00000108

#define GPRCM_O_ADC_CLK_CONFIG  0x0000010C
#define GPRCM_O_APPS_GPIO_WAKE_CONF \
                                0x00000110

#define GPRCM_O_EN_NWP_BOOT_WO_DEVINIT \
                                0x00000114

#define GPRCM_O_MEM_HCLK_DIV_CFG \
                                0x00000118

#define GPRCM_O_MEM_SYSCLK_DIV_CFG \
                                0x0000011C

#define GPRCM_O_APLLMCS_LOCK_TIME_CONF \
                                0x00000120

#define GPRCM_O_NWP_SOFT_RESET  0x00000400
#define GPRCM_O_NWP_LPDS_WAKEUP_CFG \
                                0x00000404

#define GPRCM_O_NWP_LPDS_WAKEUP_SRC \
                                0x00000408

#define GPRCM_O_NWP_RESET_CAUSE 0x0000040C
#define GPRCM_O_NWP_LPDS_WAKETIME_OPP_CFG \
                                0x00000410

#define GPRCM_O_NWP_SRAM_DSLP_CFG \
                                0x00000418

#define GPRCM_O_NWP_SRAM_LPDS_CFG \
                                0x0000041C

#define GPRCM_O_NWP_LPDS_WAKETIME_WAKE_CFG \
                                0x00000420

#define GPRCM_O_NWP_AUTONMS_SPI_MASTER_SEL \
                                0x00000424

#define GPRCM_O_NWP_AUTONMS_SPI_IDLE_REQ \
                                0x00000428

#define GPRCM_O_WLAN_TO_NWP_WAKE_REQUEST \
                                0x0000042C

#define GPRCM_O_NWP_TO_WLAN_WAKE_REQUEST \
                                0x00000430

#define GPRCM_O_NWP_GPIO_WAKE_CONF \
                                0x00000434

#define GPRCM_O_GPRCM_EFUSE_READ_REG12 \
                                0x00000438

#define GPRCM_O_GPRCM_DIEID_READ_REG5 \
                                0x00000448

#define GPRCM_O_GPRCM_DIEID_READ_REG6 \
                                0x0000044C

#define GPRCM_O_REF_FSM_CFG0    0x00000800
#define GPRCM_O_REF_FSM_CFG1    0x00000804
#define GPRCM_O_APLLMCS_WLAN_CONFIG0_40 \
                                0x00000808

#define GPRCM_O_APLLMCS_WLAN_CONFIG1_40 \
                                0x0000080C

#define GPRCM_O_APLLMCS_WLAN_CONFIG0_26 \
                                0x00000810

#define GPRCM_O_APLLMCS_WLAN_CONFIG1_26 \
                                0x00000814

#define GPRCM_O_APLLMCS_WLAN_OVERRIDES \
                                0x00000818

#define GPRCM_O_APLLMCS_MCU_RUN_CONFIG0_38P4 \
                                0x0000081C

#define GPRCM_O_APLLMCS_MCU_RUN_CONFIG1_38P4 \
                                0x00000820

#define GPRCM_O_APLLMCS_MCU_RUN_CONFIG0_26 \
                                0x00000824

#define GPRCM_O_APLLMCS_MCU_RUN_CONFIG1_26 \
                                0x00000828

#define GPRCM_O_SPARE_RW0       0x0000082C
#define GPRCM_O_SPARE_RW1       0x00000830
#define GPRCM_O_APLLMCS_MCU_OVERRIDES \
                                0x00000834

#define GPRCM_O_SYSCLK_SWITCH_STATUS \
                                0x00000838

#define GPRCM_O_REF_LDO_CONTROLS \
                                0x0000083C

#define GPRCM_O_REF_RTRIM_CONTROL \
                                0x00000840

#define GPRCM_O_REF_SLICER_CONTROLS0 \
                                0x00000844

#define GPRCM_O_REF_SLICER_CONTROLS1 \
                                0x00000848

#define GPRCM_O_REF_ANA_BGAP_CONTROLS0 \
                                0x0000084C

#define GPRCM_O_REF_ANA_BGAP_CONTROLS1 \
                                0x00000850

#define GPRCM_O_REF_ANA_SPARE_CONTROLS0 \
                                0x00000854

#define GPRCM_O_REF_ANA_SPARE_CONTROLS1 \
                                0x00000858

#define GPRCM_O_MEMSS_PSCON_OVERRIDES0 \
                                0x0000085C

#define GPRCM_O_MEMSS_PSCON_OVERRIDES1 \
                                0x00000860

#define GPRCM_O_PLL_REF_LOCK_OVERRIDES \
                                0x00000864

#define GPRCM_O_MCU_PSCON_DEBUG 0x00000868
#define GPRCM_O_MEMSS_PWR_PS    0x0000086C
#define GPRCM_O_REF_FSM_DEBUG   0x00000870
#define GPRCM_O_MEM_SYS_OPP_REQ_OVERRIDE \
                                0x00000874

#define GPRCM_O_MEM_TESTCTRL_PD_OPP_CONFIG \
                                0x00000878

#define GPRCM_O_MEM_WL_FAST_CLK_REQ_OVERRIDES \
                                0x0000087C

#define GPRCM_O_MEM_MCU_PD_MODE_REQ_OVERRIDES \
                                0x00000880

#define GPRCM_O_MEM_MCSPI_SRAM_OFF_REQ_OVERRIDES \
                                0x00000884

#define GPRCM_O_MEM_WLAN_APLLMCS_OVERRIDES \
                                0x00000888

#define GPRCM_O_MEM_REF_FSM_CFG2 \
                                0x0000088C

#define GPRCM_O_TESTCTRL_POWER_CTRL \
                                0x00000C10

#define GPRCM_O_SSDIO_POWER_CTRL \
                                0x00000C14

#define GPRCM_O_MCSPI_N1_POWER_CTRL \
                                0x00000C18

#define GPRCM_O_WELP_POWER_CTRL 0x00000C1C
#define GPRCM_O_WL_SDIO_POWER_CTRL \
                                0x00000C20

#define GPRCM_O_WLAN_SRAM_ACTIVE_PWR_CFG \
                                0x00000C24

#define GPRCM_O_WLAN_SRAM_SLEEP_PWR_CFG \
                                0x00000C28

#define GPRCM_O_APPS_SECURE_INIT_DONE \
                                0x00000C30

#define GPRCM_O_APPS_DEV_MODE_INIT_DONE \
                                0x00000C34

#define GPRCM_O_EN_APPS_REBOOT  0x00000C38
#define GPRCM_O_MEM_APPS_PERIPH_PRESENT \
                                0x00000C3C

#define GPRCM_O_MEM_NWP_PERIPH_PRESENT \
                                0x00000C40

#define GPRCM_O_MEM_SHARED_PERIPH_PRESENT \
                                0x00000C44

#define GPRCM_O_NWP_PWR_STATE   0x00000C48
#define GPRCM_O_APPS_PWR_STATE  0x00000C4C
#define GPRCM_O_MCU_PWR_STATE   0x00000C50
#define GPRCM_O_WTOP_PM_PS      0x00000C54
#define GPRCM_O_WTOP_PD_RESETZ_OVERRIDE_REG \
                                0x00000C58

#define GPRCM_O_WELP_PD_RESETZ_OVERRIDE_REG \
                                0x00000C5C

#define GPRCM_O_WL_SDIO_PD_RESETZ_OVERRIDE_REG \
                                0x00000C60

#define GPRCM_O_SSDIO_PD_RESETZ_OVERRIDE_REG \
                                0x00000C64

#define GPRCM_O_MCSPI_N1_PD_RESETZ_OVERRIDE_REG \
                                0x00000C68

#define GPRCM_O_TESTCTRL_PD_RESETZ_OVERRIDE_REG \
                                0x00000C6C

#define GPRCM_O_MCU_PD_RESETZ_OVERRIDE_REG \
                                0x00000C70

#define GPRCM_O_GPRCM_EFUSE_READ_REG0 \
                                0x00000C78

#define GPRCM_O_GPRCM_EFUSE_READ_REG1 \
                                0x00000C7C

#define GPRCM_O_GPRCM_EFUSE_READ_REG2 \
                                0x00000C80

#define GPRCM_O_GPRCM_EFUSE_READ_REG3 \
                                0x00000C84

#define GPRCM_O_WTOP_MEM_RET_CFG \
                                0x00000C88

#define GPRCM_O_COEX_CLK_SWALLOW_CFG0 \
                                0x00000C8C

#define GPRCM_O_COEX_CLK_SWALLOW_CFG1 \
                                0x00000C90

#define GPRCM_O_COEX_CLK_SWALLOW_CFG2 \
                                0x00000C94

#define GPRCM_O_COEX_CLK_SWALLOW_ENABLE \
                                0x00000C98

#define GPRCM_O_DCDC_CLK_GEN_CONFIG \
                                0x00000C9C

#define GPRCM_O_GPRCM_EFUSE_READ_REG4 \
                                0x00000CA0

#define GPRCM_O_GPRCM_EFUSE_READ_REG5 \
                                0x00000CA4

#define GPRCM_O_GPRCM_EFUSE_READ_REG6 \
                                0x00000CA8

#define GPRCM_O_GPRCM_EFUSE_READ_REG7 \
                                0x00000CAC

#define GPRCM_O_GPRCM_EFUSE_READ_REG8 \
                                0x00000CB0

#define GPRCM_O_GPRCM_EFUSE_READ_REG9 \
                                0x00000CB4

#define GPRCM_O_GPRCM_EFUSE_READ_REG10 \
                                0x00000CB8

#define GPRCM_O_GPRCM_EFUSE_READ_REG11 \
                                0x00000CBC

#define GPRCM_O_GPRCM_DIEID_READ_REG0 \
                                0x00000CC0

#define GPRCM_O_GPRCM_DIEID_READ_REG1 \
                                0x00000CC4

#define GPRCM_O_GPRCM_DIEID_READ_REG2 \
                                0x00000CC8

#define GPRCM_O_GPRCM_DIEID_READ_REG3 \
                                0x00000CCC

#define GPRCM_O_GPRCM_DIEID_READ_REG4 \
                                0x00000CD0

#define GPRCM_O_APPS_SS_OVERRIDES \
                                0x00000CD4

#define GPRCM_O_NWP_SS_OVERRIDES \
                                0x00000CD8

#define GPRCM_O_SHARED_SS_OVERRIDES \
                                0x00000CDC

#define GPRCM_O_IDMEM_CORE_RST_OVERRIDES \
                                0x00000CE0

#define GPRCM_O_TOP_DIE_FSM_OVERRIDES \
                                0x00000CE4

#define GPRCM_O_MCU_PSCON_OVERRIDES \
                                0x00000CE8

#define GPRCM_O_WTOP_PSCON_OVERRIDES \
                                0x00000CEC

#define GPRCM_O_WELP_PSCON_OVERRIDES \
                                0x00000CF0

#define GPRCM_O_WL_SDIO_PSCON_OVERRIDES \
                                0x00000CF4

#define GPRCM_O_MCSPI_PSCON_OVERRIDES \
                                0x00000CF8

#define GPRCM_O_SSDIO_PSCON_OVERRIDES \
                                0x00000CFC










#define GPRCM_APPS_SOFT_RESET_APPS_SOFT_RESET1 \
                                0x00000002  
                                            
                                            
                                            
                                            

#define GPRCM_APPS_SOFT_RESET_APPS_SOFT_RESET0 \
                                0x00000001  
                                            
                                            
                                            







#define GPRCM_APPS_LPDS_WAKEUP_CFG_APPS_LPDS_WAKEUP_CFG_M \
                                0x000000FF  
                                            
                                            
                                            
                                            
                                            

#define GPRCM_APPS_LPDS_WAKEUP_CFG_APPS_LPDS_WAKEUP_CFG_S 0






#define GPRCM_APPS_LPDS_WAKEUP_SRC_APPS_LPDS_WAKEUP_SRC_M \
                                0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_APPS_LPDS_WAKEUP_SRC_APPS_LPDS_WAKEUP_SRC_S 0






#define GPRCM_APPS_RESET_CAUSE_APPS_RESET_CAUSE_M \
                                0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_APPS_RESET_CAUSE_APPS_RESET_CAUSE_S 0






#define GPRCM_APPS_LPDS_WAKETIME_OPP_CFG_APPS_LPDS_WAKETIME_OPP_CFG_M \
                                0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_APPS_LPDS_WAKETIME_OPP_CFG_APPS_LPDS_WAKETIME_OPP_CFG_S 0






#define GPRCM_APPS_SRAM_DSLP_CFG_APPS_SRAM_DSLP_CFG_M \
                                0x000FFFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_APPS_SRAM_DSLP_CFG_APPS_SRAM_DSLP_CFG_S 0






#define GPRCM_APPS_SRAM_LPDS_CFG_APPS_SRAM_LPDS_CFG_M \
                                0x000FFFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_APPS_SRAM_LPDS_CFG_APPS_SRAM_LPDS_CFG_S 0






#define GPRCM_APPS_LPDS_WAKETIME_WAKE_CFG_APPS_LPDS_WAKETIME_WAKE_CFG_M \
                                0xFFFFFFFF  
                                            
                                            
                                            

#define GPRCM_APPS_LPDS_WAKETIME_WAKE_CFG_APPS_LPDS_WAKETIME_WAKE_CFG_S 0






#define GPRCM_TOP_DIE_ENABLE_FLASH_BUSY \
                                0x00001000

#define GPRCM_TOP_DIE_ENABLE_TOP_DIE_PWR_PS_M \
                                0x00000F00

#define GPRCM_TOP_DIE_ENABLE_TOP_DIE_PWR_PS_S 8
#define GPRCM_TOP_DIE_ENABLE_TOP_DIE_ENABLE_STATUS \
                                0x00000002  

#define GPRCM_TOP_DIE_ENABLE_TOP_DIE_ENABLE \
                                0x00000001  
                                            







#define GPRCM_TOP_DIE_ENABLE_PARAMETERS_FLASH_3P3_RSTN2D2D_POR_RSTN_M \
                                0xF0000000  
                                            
                                            

#define GPRCM_TOP_DIE_ENABLE_PARAMETERS_FLASH_3P3_RSTN2D2D_POR_RSTN_S 28
#define GPRCM_TOP_DIE_ENABLE_PARAMETERS_TOP_DIE_SW_EN2TOP_DIE_FLASH_3P3_RSTN_M \
                                0x00FF0000  
                                            
                                            
                                            

#define GPRCM_TOP_DIE_ENABLE_PARAMETERS_TOP_DIE_SW_EN2TOP_DIE_FLASH_3P3_RSTN_S 16
#define GPRCM_TOP_DIE_ENABLE_PARAMETERS_TOP_DIE_POR_RSTN2BOTT_DIE_FMC_RSTN_M \
                                0x000000FF  
                                            
                                            
                                            

#define GPRCM_TOP_DIE_ENABLE_PARAMETERS_TOP_DIE_POR_RSTN2BOTT_DIE_FMC_RSTN_S 0






#define GPRCM_MCU_GLOBAL_SOFT_RESET_MCU_GLOBAL_SOFT_RESET \
                                0x00000001  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            







#define GPRCM_ADC_CLK_CONFIG_ADC_CLKGEN_OFF_TIME_M \
                                0x000007C0  
                                            
                                            

#define GPRCM_ADC_CLK_CONFIG_ADC_CLKGEN_OFF_TIME_S 6
#define GPRCM_ADC_CLK_CONFIG_ADC_CLKGEN_ON_TIME_M \
                                0x0000003E  
                                            
                                            

#define GPRCM_ADC_CLK_CONFIG_ADC_CLKGEN_ON_TIME_S 1
#define GPRCM_ADC_CLK_CONFIG_ADC_CLK_ENABLE \
                                0x00000001  
                                            







#define GPRCM_APPS_GPIO_WAKE_CONF_APPS_GPIO_WAKE_CONF_M \
                                0x00000003  
                                            
                                            
                                            
                                            

#define GPRCM_APPS_GPIO_WAKE_CONF_APPS_GPIO_WAKE_CONF_S 0






#define GPRCM_EN_NWP_BOOT_WO_DEVINIT_reserved_M \
                                0xFFFFFFFE

#define GPRCM_EN_NWP_BOOT_WO_DEVINIT_reserved_S 1
#define GPRCM_EN_NWP_BOOT_WO_DEVINIT_mem_en_nwp_boot_wo_devinit \
                                0x00000001  
                                            
                                            
                                            
                                            
                                            







#define GPRCM_MEM_HCLK_DIV_CFG_mem_hclk_div_cfg_M \
                                0x00000007  
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_MEM_HCLK_DIV_CFG_mem_hclk_div_cfg_S 0






#define GPRCM_MEM_SYSCLK_DIV_CFG_mem_sysclk_div_off_time_M \
                                0x00000038

#define GPRCM_MEM_SYSCLK_DIV_CFG_mem_sysclk_div_off_time_S 3
#define GPRCM_MEM_SYSCLK_DIV_CFG_mem_sysclk_div_on_time_M \
                                0x00000007

#define GPRCM_MEM_SYSCLK_DIV_CFG_mem_sysclk_div_on_time_S 0






#define GPRCM_APLLMCS_LOCK_TIME_CONF_mem_apllmcs_wlan_lock_time_M \
                                0x0000FF00

#define GPRCM_APLLMCS_LOCK_TIME_CONF_mem_apllmcs_wlan_lock_time_S 8
#define GPRCM_APLLMCS_LOCK_TIME_CONF_mem_apllmcs_mcu_lock_time_M \
                                0x000000FF

#define GPRCM_APLLMCS_LOCK_TIME_CONF_mem_apllmcs_mcu_lock_time_S 0






#define GPRCM_NWP_SOFT_RESET_NWP_SOFT_RESET1 \
                                0x00000002  
                                            
                                            
                                            

#define GPRCM_NWP_SOFT_RESET_NWP_SOFT_RESET0 \
                                0x00000001  
                                            
                                            







#define GPRCM_NWP_LPDS_WAKEUP_CFG_NWP_LPDS_WAKEUP_CFG_M \
                                0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_NWP_LPDS_WAKEUP_CFG_NWP_LPDS_WAKEUP_CFG_S 0






#define GPRCM_NWP_LPDS_WAKEUP_SRC_NWP_LPDS_WAKEUP_SRC_M \
                                0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_NWP_LPDS_WAKEUP_SRC_NWP_LPDS_WAKEUP_SRC_S 0






#define GPRCM_NWP_RESET_CAUSE_NWP_RESET_CAUSE_M \
                                0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_NWP_RESET_CAUSE_NWP_RESET_CAUSE_S 0






#define GPRCM_NWP_LPDS_WAKETIME_OPP_CFG_NWP_LPDS_WAKETIME_OPP_CFG_M \
                                0xFFFFFFFF  
                                            
                                            

#define GPRCM_NWP_LPDS_WAKETIME_OPP_CFG_NWP_LPDS_WAKETIME_OPP_CFG_S 0






#define GPRCM_NWP_SRAM_DSLP_CFG_NWP_SRAM_DSLP_CFG_M \
                                0x000FFFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_NWP_SRAM_DSLP_CFG_NWP_SRAM_DSLP_CFG_S 0






#define GPRCM_NWP_SRAM_LPDS_CFG_NWP_SRAM_LPDS_CFG_M \
                                0x000FFFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_NWP_SRAM_LPDS_CFG_NWP_SRAM_LPDS_CFG_S 0






#define GPRCM_NWP_LPDS_WAKETIME_WAKE_CFG_NWP_LPDS_WAKETIME_WAKE_CFG_M \
                                0xFFFFFFFF  
                                            
                                            

#define GPRCM_NWP_LPDS_WAKETIME_WAKE_CFG_NWP_LPDS_WAKETIME_WAKE_CFG_S 0






#define GPRCM_NWP_AUTONMS_SPI_MASTER_SEL_F_M \
                                0xFFFE0000

#define GPRCM_NWP_AUTONMS_SPI_MASTER_SEL_F_S 17
#define GPRCM_NWP_AUTONMS_SPI_MASTER_SEL_MEM_AUTONMS_SPI_MASTER_SEL \
                                0x00010000  
                                            
                                            
                                            







#define GPRCM_NWP_AUTONMS_SPI_IDLE_REQ_NWP_AUTONMS_SPI_IDLE_WAKEUP \
                                0x00010000

#define GPRCM_NWP_AUTONMS_SPI_IDLE_REQ_NWP_AUTONMS_SPI_IDLE_ACK \
                                0x00000002  
                                            
                                            

#define GPRCM_NWP_AUTONMS_SPI_IDLE_REQ_NWP_AUTONMS_SPI_IDLE_REQ \
                                0x00000001  
                                            
                                            







#define GPRCM_WLAN_TO_NWP_WAKE_REQUEST_WLAN_TO_NWP_WAKE_REQUEST \
                                0x00000001  
                                            
                                            







#define GPRCM_NWP_TO_WLAN_WAKE_REQUEST_NWP_TO_WLAN_WAKE_REQUEST \
                                0x00000001  
                                            
                                            







#define GPRCM_NWP_GPIO_WAKE_CONF_NWP_GPIO_WAKE_CONF_M \
                                0x00000003  
                                            
                                            
                                            
                                            

#define GPRCM_NWP_GPIO_WAKE_CONF_NWP_GPIO_WAKE_CONF_S 0






#define GPRCM_GPRCM_EFUSE_READ_REG12_FUSEFARM_ROW_32_MSW_M \
                                0x0000FFFF  
                                            

#define GPRCM_GPRCM_EFUSE_READ_REG12_FUSEFARM_ROW_32_MSW_S 0






#define GPRCM_GPRCM_DIEID_READ_REG5_FUSEFARM_ROW_10_M \
                                0xFFFFFFFF  
                                            
                                            
                                            
                                            

#define GPRCM_GPRCM_DIEID_READ_REG5_FUSEFARM_ROW_10_S 0






#define GPRCM_GPRCM_DIEID_READ_REG6_FUSEFARM_ROW_11_M \
                                0xFFFFFFFF  
                                            

#define GPRCM_GPRCM_DIEID_READ_REG6_FUSEFARM_ROW_11_S 0






#define GPRCM_REF_FSM_CFG0_BGAP_SETTLING_TIME_M \
                                0x00FF0000  
                                            

#define GPRCM_REF_FSM_CFG0_BGAP_SETTLING_TIME_S 16
#define GPRCM_REF_FSM_CFG0_FREF_LDO_SETTLING_TIME_M \
                                0x0000FF00  
                                            

#define GPRCM_REF_FSM_CFG0_FREF_LDO_SETTLING_TIME_S 8
#define GPRCM_REF_FSM_CFG0_DIG_BUF_SETTLING_TIME_M \
                                0x000000FF  
                                            

#define GPRCM_REF_FSM_CFG0_DIG_BUF_SETTLING_TIME_S 0






#define GPRCM_REF_FSM_CFG1_XTAL_SETTLING_TIME_M \
                                0xFF000000  
                                            

#define GPRCM_REF_FSM_CFG1_XTAL_SETTLING_TIME_S 24
#define GPRCM_REF_FSM_CFG1_SLICER_LV_SETTLING_TIME_M \
                                0x00FF0000  

#define GPRCM_REF_FSM_CFG1_SLICER_LV_SETTLING_TIME_S 16
#define GPRCM_REF_FSM_CFG1_SLICER_HV_PD_SETTLING_TIME_M \
                                0x0000FF00  
                                            

#define GPRCM_REF_FSM_CFG1_SLICER_HV_PD_SETTLING_TIME_S 8
#define GPRCM_REF_FSM_CFG1_SLICER_HV_SETTLING_TIME_M \
                                0x000000FF  

#define GPRCM_REF_FSM_CFG1_SLICER_HV_SETTLING_TIME_S 0






#define GPRCM_APLLMCS_WLAN_CONFIG0_40_APLLMCS_WLAN_N_40_M \
                                0x00007F00  
                                            
                                            

#define GPRCM_APLLMCS_WLAN_CONFIG0_40_APLLMCS_WLAN_N_40_S 8
#define GPRCM_APLLMCS_WLAN_CONFIG0_40_APLLMCS_WLAN_M_40_M \
                                0x000000FF  
                                            
                                            

#define GPRCM_APLLMCS_WLAN_CONFIG0_40_APLLMCS_WLAN_M_40_S 0






#define GPRCM_APLLMCS_WLAN_CONFIG1_40_APLLMCS_HISPEED_40 \
                                0x00000010  
                                            
                                            

#define GPRCM_APLLMCS_WLAN_CONFIG1_40_APLLMCS_SEL96_40 \
                                0x00000008  
                                            
                                            

#define GPRCM_APLLMCS_WLAN_CONFIG1_40_APLLMCS_SELINPFREQ_40_M \
                                0x00000007  
                                            
                                            

#define GPRCM_APLLMCS_WLAN_CONFIG1_40_APLLMCS_SELINPFREQ_40_S 0






#define GPRCM_APLLMCS_WLAN_CONFIG0_26_APLLMCS_WLAN_N_26_M \
                                0x00007F00  
                                            
                                            

#define GPRCM_APLLMCS_WLAN_CONFIG0_26_APLLMCS_WLAN_N_26_S 8
#define GPRCM_APLLMCS_WLAN_CONFIG0_26_APLLMCS_WLAN_M_26_M \
                                0x000000FF  
                                            
                                            

#define GPRCM_APLLMCS_WLAN_CONFIG0_26_APLLMCS_WLAN_M_26_S 0






#define GPRCM_APLLMCS_WLAN_CONFIG1_26_APLLMCS_HISPEED_26 \
                                0x00000010  
                                            
                                            

#define GPRCM_APLLMCS_WLAN_CONFIG1_26_APLLMCS_SEL96_26 \
                                0x00000008  
                                            
                                            

#define GPRCM_APLLMCS_WLAN_CONFIG1_26_APLLMCS_SELINPFREQ_26_M \
                                0x00000007  
                                            
                                            

#define GPRCM_APLLMCS_WLAN_CONFIG1_26_APLLMCS_SELINPFREQ_26_S 0






#define GPRCM_APLLMCS_WLAN_OVERRIDES_APLLMCS_WLAN_POSTDIV_OVERRIDE_CTRL \
                                0x00080000

#define GPRCM_APLLMCS_WLAN_OVERRIDES_APLLMCS_WLAN_POSTDIV_OVERRIDE_M \
                                0x00070000

#define GPRCM_APLLMCS_WLAN_OVERRIDES_APLLMCS_WLAN_POSTDIV_OVERRIDE_S 16
#define GPRCM_APLLMCS_WLAN_OVERRIDES_APLLMCS_WLAN_SPARE_M \
                                0x00000700

#define GPRCM_APLLMCS_WLAN_OVERRIDES_APLLMCS_WLAN_SPARE_S 8
#define GPRCM_APLLMCS_WLAN_OVERRIDES_APLLMCS_WLAN_M_8_OVERRIDE_CTRL \
                                0x00000020  
                                            
                                            
                                            

#define GPRCM_APLLMCS_WLAN_OVERRIDES_APLLMCS_WLAN_M_8_OVERRIDE \
                                0x00000010  
                                            
                                            
                                            

#define GPRCM_APLLMCS_WLAN_OVERRIDES_APLLMCS_WLAN_N_7_8_OVERRIDE_CTRL \
                                0x00000004  
                                            
                                            
                                            
                                            

#define GPRCM_APLLMCS_WLAN_OVERRIDES_APLLMCS_WLAN_N_7_8_OVERRIDE_M \
                                0x00000003  
                                            
                                            
                                            
                                            

#define GPRCM_APLLMCS_WLAN_OVERRIDES_APLLMCS_WLAN_N_7_8_OVERRIDE_S 0






#define GPRCM_APLLMCS_MCU_RUN_CONFIG0_38P4_APLLMCS_MCU_POSTDIV_M \
                                0x38000000

#define GPRCM_APLLMCS_MCU_RUN_CONFIG0_38P4_APLLMCS_MCU_POSTDIV_S 27
#define GPRCM_APLLMCS_MCU_RUN_CONFIG0_38P4_APLLMCS_MCU_SPARE_M \
                                0x07000000

#define GPRCM_APLLMCS_MCU_RUN_CONFIG0_38P4_APLLMCS_MCU_SPARE_S 24
#define GPRCM_APLLMCS_MCU_RUN_CONFIG0_38P4_APLLMCS_MCU_RUN_N_38P4_M \
                                0x007F0000  
                                            
                                            
                                            

#define GPRCM_APLLMCS_MCU_RUN_CONFIG0_38P4_APLLMCS_MCU_RUN_N_38P4_S 16
#define GPRCM_APLLMCS_MCU_RUN_CONFIG0_38P4_APLLMCS_MCU_RUN_M_38P4_M \
                                0x0000FF00  
                                            
                                            
                                            

#define GPRCM_APLLMCS_MCU_RUN_CONFIG0_38P4_APLLMCS_MCU_RUN_M_38P4_S 8
#define GPRCM_APLLMCS_MCU_RUN_CONFIG0_38P4_APLLMCS_MCU_RUN_M_8_38P4 \
                                0x00000010  
                                            
                                            
                                            

#define GPRCM_APLLMCS_MCU_RUN_CONFIG0_38P4_APLLMCS_MCU_RUN_N_7_8_38P4_M \
                                0x00000003  
                                            
                                            
                                            

#define GPRCM_APLLMCS_MCU_RUN_CONFIG0_38P4_APLLMCS_MCU_RUN_N_7_8_38P4_S 0






#define GPRCM_APLLMCS_MCU_RUN_CONFIG1_38P4_APLLMCS_MCU_RUN_HISPEED_38P4 \
                                0x00000010  
                                            
                                            
                                            

#define GPRCM_APLLMCS_MCU_RUN_CONFIG1_38P4_APLLMCS_MCU_RUN_SEL96_38P4 \
                                0x00000008  
                                            
                                            
                                            

#define GPRCM_APLLMCS_MCU_RUN_CONFIG1_38P4_APLLMCS_MCU_RUN_SELINPFREQ_38P4_M \
                                0x00000007  
                                            
                                            
                                            

#define GPRCM_APLLMCS_MCU_RUN_CONFIG1_38P4_APLLMCS_MCU_RUN_SELINPFREQ_38P4_S 0






#define GPRCM_APLLMCS_MCU_RUN_CONFIG0_26_APLLMCS_MCU_RUN_N_26_M \
                                0x007F0000  
                                            
                                            
                                            

#define GPRCM_APLLMCS_MCU_RUN_CONFIG0_26_APLLMCS_MCU_RUN_N_26_S 16
#define GPRCM_APLLMCS_MCU_RUN_CONFIG0_26_APLLMCS_MCU_RUN_M_26_M \
                                0x0000FF00  
                                            
                                            
                                            

#define GPRCM_APLLMCS_MCU_RUN_CONFIG0_26_APLLMCS_MCU_RUN_M_26_S 8
#define GPRCM_APLLMCS_MCU_RUN_CONFIG0_26_APLLMCS_MCU_RUN_M_8_26 \
                                0x00000010  
                                            
                                            
                                            

#define GPRCM_APLLMCS_MCU_RUN_CONFIG0_26_APLLMCS_MCU_RUN_N_7_8_26_M \
                                0x00000003  
                                            
                                            
                                            

#define GPRCM_APLLMCS_MCU_RUN_CONFIG0_26_APLLMCS_MCU_RUN_N_7_8_26_S 0






#define GPRCM_APLLMCS_MCU_RUN_CONFIG1_26_APLLMCS_MCU_RUN_HISPEED_26 \
                                0x00000010  
                                            
                                            
                                            

#define GPRCM_APLLMCS_MCU_RUN_CONFIG1_26_APLLMCS_MCU_RUN_SEL96_26 \
                                0x00000008  
                                            
                                            
                                            

#define GPRCM_APLLMCS_MCU_RUN_CONFIG1_26_APLLMCS_MCU_RUN_SELINPFREQ_26_M \
                                0x00000007  
                                            
                                            
                                            

#define GPRCM_APLLMCS_MCU_RUN_CONFIG1_26_APLLMCS_MCU_RUN_SELINPFREQ_26_S 0
















#define GPRCM_APLLMCS_MCU_OVERRIDES_APLLMCS_MCU_LOCK \
                                0x00000400  
                                            

#define GPRCM_APLLMCS_MCU_OVERRIDES_APLLMCS_MCU_ENABLE_OVERRIDE \
                                0x00000200  
                                            

#define GPRCM_APLLMCS_MCU_OVERRIDES_APLLMCS_MCU_ENABLE_OVERRIDE_CTRL \
                                0x00000100  
                                            
                                            

#define GPRCM_APLLMCS_MCU_OVERRIDES_SYSCLK_SRC_OVERRIDE_M \
                                0x00000006  
                                            
                                            
                                            

#define GPRCM_APLLMCS_MCU_OVERRIDES_SYSCLK_SRC_OVERRIDE_S 1
#define GPRCM_APLLMCS_MCU_OVERRIDES_SYSCLK_SRC_OVERRIDE_CTRL \
                                0x00000001  
                                            
                                            







#define GPRCM_SYSCLK_SWITCH_STATUS_SYSCLK_SWITCH_STATUS \
                                0x00000001  
                                            
                                            







#define GPRCM_REF_LDO_CONTROLS_REF_LDO_ENABLE_OVERRIDE_CTRL \
                                0x00010000  
                                            
                                            
                                            
                                            
                                            

#define GPRCM_REF_LDO_CONTROLS_REF_SPARE_CONTROL_M \
                                0x0000C000  
                                            
                                            

#define GPRCM_REF_LDO_CONTROLS_REF_SPARE_CONTROL_S 14
#define GPRCM_REF_LDO_CONTROLS_REF_TLOAD_ENABLE_M \
                                0x00003800  
                                            
                                            

#define GPRCM_REF_LDO_CONTROLS_REF_TLOAD_ENABLE_S 11
#define GPRCM_REF_LDO_CONTROLS_REF_LDO_TMUX_CONTROL_M \
                                0x00000700  
                                            
                                            

#define GPRCM_REF_LDO_CONTROLS_REF_LDO_TMUX_CONTROL_S 8
#define GPRCM_REF_LDO_CONTROLS_REF_BW_CONTROL_M \
                                0x000000C0  
                                            
                                            

#define GPRCM_REF_LDO_CONTROLS_REF_BW_CONTROL_S 6
#define GPRCM_REF_LDO_CONTROLS_REF_VTRIM_CONTROL_M \
                                0x0000003C  
                                            
                                            

#define GPRCM_REF_LDO_CONTROLS_REF_VTRIM_CONTROL_S 2
#define GPRCM_REF_LDO_CONTROLS_REF_LDO_BYPASS_ENABLE \
                                0x00000002  
                                            
                                            

#define GPRCM_REF_LDO_CONTROLS_REF_LDO_ENABLE \
                                0x00000001  
                                            
                                            
                                            
                                            







#define GPRCM_REF_RTRIM_CONTROL_TOP_PM_REG0_5_4_M \
                                0x18000000  
                                            

#define GPRCM_REF_RTRIM_CONTROL_TOP_PM_REG0_5_4_S 27
#define GPRCM_REF_RTRIM_CONTROL_TOP_CLKM_REG0_15_5_M \
                                0x07FF0000  
                                            

#define GPRCM_REF_RTRIM_CONTROL_TOP_CLKM_REG0_15_5_S 16
#define GPRCM_REF_RTRIM_CONTROL_REF_CLKM_RTRIM_OVERRIDE_CTRL \
                                0x00000100  
                                            
                                            
                                            

#define GPRCM_REF_RTRIM_CONTROL_REF_CLKM_RTRIM_M \
                                0x0000001F  
                                            
                                            

#define GPRCM_REF_RTRIM_CONTROL_REF_CLKM_RTRIM_S 0






#define GPRCM_REF_SLICER_CONTROLS0_CLK_EN_WLAN_LOWV_OVERRIDE_CTRL \
                                0x00200000  
                                            
                                            
                                            
                                            
                                            

#define GPRCM_REF_SLICER_CONTROLS0_CLK_EN_TOP_LOWV_OVERRIDE_CTRL \
                                0x00100000  
                                            
                                            
                                            
                                            
                                            

#define GPRCM_REF_SLICER_CONTROLS0_EN_XTAL_OVERRIDE_CTRL \
                                0x00080000  
                                            
                                            
                                            
                                            

#define GPRCM_REF_SLICER_CONTROLS0_EN_SLI_HV_OVERRIDE_CTRL \
                                0x00040000  
                                            
                                            
                                            
                                            
                                            

#define GPRCM_REF_SLICER_CONTROLS0_EN_SLI_LV_OVERRIDE_CTRL \
                                0x00020000  
                                            
                                            
                                            
                                            
                                            

#define GPRCM_REF_SLICER_CONTROLS0_EN_SLI_HV_PDN_OVERRIDE_CTRL \
                                0x00010000  
                                            
                                            
                                            
                                            
                                            

#define GPRCM_REF_SLICER_CONTROLS0_CLK_EN_TOP_LOWV \
                                0x00008000  
                                            
                                            
                                            
                                            

#define GPRCM_REF_SLICER_CONTROLS0_CLK_EN_WLAN_LOWV \
                                0x00004000  
                                            
                                            
                                            
                                            

#define GPRCM_REF_SLICER_CONTROLS0_CLKOUT_FLIP_EN \
                                0x00002000  
                                            
                                            

#define GPRCM_REF_SLICER_CONTROLS0_EN_DIV2_WLAN_CLK \
                                0x00001000  
                                            
                                            

#define GPRCM_REF_SLICER_CONTROLS0_EN_DIV3_WLAN_CLK \
                                0x00000800  
                                            
                                            

#define GPRCM_REF_SLICER_CONTROLS0_EN_DIV4_WLAN_CLK \
                                0x00000400  
                                            
                                            

#define GPRCM_REF_SLICER_CONTROLS0_CM_TMUX_SEL_LOWV_M \
                                0x000003C0  
                                            
                                            

#define GPRCM_REF_SLICER_CONTROLS0_CM_TMUX_SEL_LOWV_S 6
#define GPRCM_REF_SLICER_CONTROLS0_SLICER_SPARE0_M \
                                0x00000030  
                                            
                                            

#define GPRCM_REF_SLICER_CONTROLS0_SLICER_SPARE0_S 4
#define GPRCM_REF_SLICER_CONTROLS0_EN_XTAL \
                                0x00000008  
                                            

#define GPRCM_REF_SLICER_CONTROLS0_EN_SLICER_HV \
                                0x00000004  
                                            
                                            

#define GPRCM_REF_SLICER_CONTROLS0_EN_SLICER_LV \
                                0x00000002  
                                            
                                            

#define GPRCM_REF_SLICER_CONTROLS0_EN_SLICER_HV_PDN \
                                0x00000001  
                                            
                                            







#define GPRCM_REF_SLICER_CONTROLS1_SLICER_SPARE1_M \
                                0x0000FC00  
                                            

#define GPRCM_REF_SLICER_CONTROLS1_SLICER_SPARE1_S 10
#define GPRCM_REF_SLICER_CONTROLS1_XOSC_TRIM_M \
                                0x000003F0  
                                            

#define GPRCM_REF_SLICER_CONTROLS1_XOSC_TRIM_S 4
#define GPRCM_REF_SLICER_CONTROLS1_SLICER_ITRIM_CHANGE_TOGGLE \
                                0x00000008  
                                            
                                            

#define GPRCM_REF_SLICER_CONTROLS1_SLICER_LV_TRIM_M \
                                0x00000007  
                                            

#define GPRCM_REF_SLICER_CONTROLS1_SLICER_LV_TRIM_S 0






#define GPRCM_REF_ANA_BGAP_CONTROLS0_reserved_M \
                                0xFF800000

#define GPRCM_REF_ANA_BGAP_CONTROLS0_reserved_S 23
#define GPRCM_REF_ANA_BGAP_CONTROLS0_mem_ref_mag_trim_override_ctrl \
                                0x00400000  
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_REF_ANA_BGAP_CONTROLS0_mem_ref_v2i_trim_override_ctrl \
                                0x00200000  
                                            
                                            
                                            
                                            
                                            

#define GPRCM_REF_ANA_BGAP_CONTROLS0_mem_ref_temp_trim_override_ctrl \
                                0x00100000  
                                            
                                            
                                            
                                            
                                            

#define GPRCM_REF_ANA_BGAP_CONTROLS0_mem_ref_startup_en_override_ctrl \
                                0x00080000  
                                            
                                            
                                            
                                            
                                            

#define GPRCM_REF_ANA_BGAP_CONTROLS0_mem_ref_v2i_en_override_ctrl \
                                0x00040000  
                                            
                                            
                                            
                                            

#define GPRCM_REF_ANA_BGAP_CONTROLS0_mem_ref_fc_en_override_ctrl \
                                0x00020000  
                                            
                                            
                                            
                                            

#define GPRCM_REF_ANA_BGAP_CONTROLS0_mem_ref_bgap_en_override_ctrl \
                                0x00010000  
                                            
                                            
                                            
                                            

#define GPRCM_REF_ANA_BGAP_CONTROLS0_mem_ref_temp_trim_M \
                                0x0000FC00  
                                            
                                            
                                            
                                            
                                            

#define GPRCM_REF_ANA_BGAP_CONTROLS0_mem_ref_temp_trim_S 10
#define GPRCM_REF_ANA_BGAP_CONTROLS0_mem_ref_v2i_trim_M \
                                0x000003C0  
                                            
                                            
                                            
                                            
                                            

#define GPRCM_REF_ANA_BGAP_CONTROLS0_mem_ref_v2i_trim_S 6
#define GPRCM_REF_ANA_BGAP_CONTROLS0_NU1_M \
                                0x00000030

#define GPRCM_REF_ANA_BGAP_CONTROLS0_NU1_S 4
#define GPRCM_REF_ANA_BGAP_CONTROLS0_mem_ref_startup_en \
                                0x00000008  
                                            
                                            
                                            
                                            

#define GPRCM_REF_ANA_BGAP_CONTROLS0_mem_ref_v2i_en \
                                0x00000004  
                                            
                                            
                                            
                                            

#define GPRCM_REF_ANA_BGAP_CONTROLS0_mem_ref_fc_en \
                                0x00000002  
                                            
                                            
                                            
                                            

#define GPRCM_REF_ANA_BGAP_CONTROLS0_mem_ref_bgap_en \
                                0x00000001  
                                            
                                            
                                            
                                            







#define GPRCM_REF_ANA_BGAP_CONTROLS1_reserved_M \
                                0xFFFF0000

#define GPRCM_REF_ANA_BGAP_CONTROLS1_reserved_S 16
#define GPRCM_REF_ANA_BGAP_CONTROLS1_mem_ref_bg_spare_M \
                                0x0000C000  
                                            

#define GPRCM_REF_ANA_BGAP_CONTROLS1_mem_ref_bg_spare_S 14
#define GPRCM_REF_ANA_BGAP_CONTROLS1_mem_ref_bgap_tmux_ctrl_M \
                                0x00003E00  
                                            

#define GPRCM_REF_ANA_BGAP_CONTROLS1_mem_ref_bgap_tmux_ctrl_S 9
#define GPRCM_REF_ANA_BGAP_CONTROLS1_mem_ref_filt_trim_M \
                                0x000001E0  
                                            

#define GPRCM_REF_ANA_BGAP_CONTROLS1_mem_ref_filt_trim_S 5
#define GPRCM_REF_ANA_BGAP_CONTROLS1_mem_ref_mag_trim_M \
                                0x0000001F  
                                            
                                            
                                            
                                            
                                            

#define GPRCM_REF_ANA_BGAP_CONTROLS1_mem_ref_mag_trim_S 0






#define GPRCM_REF_ANA_SPARE_CONTROLS0_reserved_M \
                                0xFFFF0000

#define GPRCM_REF_ANA_SPARE_CONTROLS0_reserved_S 16
#define GPRCM_REF_ANA_SPARE_CONTROLS0_mem_top_pm_reg3_M \
                                0x0000FFFF  
                                            

#define GPRCM_REF_ANA_SPARE_CONTROLS0_mem_top_pm_reg3_S 0






#define GPRCM_REF_ANA_SPARE_CONTROLS1_mem_top_clkm_reg3_M \
                                0xFFFF0000  
                                            

#define GPRCM_REF_ANA_SPARE_CONTROLS1_mem_top_clkm_reg3_S 16
#define GPRCM_REF_ANA_SPARE_CONTROLS1_mem_top_clkm_reg4_M \
                                0x0000FFFF  
                                            

#define GPRCM_REF_ANA_SPARE_CONTROLS1_mem_top_clkm_reg4_S 0






#define GPRCM_MEMSS_PSCON_OVERRIDES0_mem_memss_pscon_mem_off_override_M \
                                0xFFFF0000

#define GPRCM_MEMSS_PSCON_OVERRIDES0_mem_memss_pscon_mem_off_override_S 16
#define GPRCM_MEMSS_PSCON_OVERRIDES0_mem_memss_pscon_mem_retain_override_M \
                                0x0000FFFF

#define GPRCM_MEMSS_PSCON_OVERRIDES0_mem_memss_pscon_mem_retain_override_S 0






#define GPRCM_MEMSS_PSCON_OVERRIDES1_reserved_M \
                                0xFFFFFFC0

#define GPRCM_MEMSS_PSCON_OVERRIDES1_reserved_S 6
#define GPRCM_MEMSS_PSCON_OVERRIDES1_mem_memss_pscon_mem_update_override_ctrl \
                                0x00000020

#define GPRCM_MEMSS_PSCON_OVERRIDES1_mem_memss_pscon_mem_update_override \
                                0x00000010

#define GPRCM_MEMSS_PSCON_OVERRIDES1_mem_memss_pscon_sleep_override_ctrl \
                                0x00000008

#define GPRCM_MEMSS_PSCON_OVERRIDES1_mem_memss_pscon_sleep_override \
                                0x00000004

#define GPRCM_MEMSS_PSCON_OVERRIDES1_mem_memss_pscon_mem_off_override_ctrl \
                                0x00000002

#define GPRCM_MEMSS_PSCON_OVERRIDES1_mem_memms_pscon_mem_retain_override_ctrl \
                                0x00000001







#define GPRCM_PLL_REF_LOCK_OVERRIDES_reserved_M \
                                0xFFFFFFF8

#define GPRCM_PLL_REF_LOCK_OVERRIDES_reserved_S 3
#define GPRCM_PLL_REF_LOCK_OVERRIDES_mem_mcu_apllmcs_lock_override \
                                0x00000004

#define GPRCM_PLL_REF_LOCK_OVERRIDES_mem_wlan_apllmcs_lock_override \
                                0x00000002

#define GPRCM_PLL_REF_LOCK_OVERRIDES_mem_ref_clk_valid_override \
                                0x00000001







#define GPRCM_MCU_PSCON_DEBUG_reserved_M \
                                0xFFFFFFC0

#define GPRCM_MCU_PSCON_DEBUG_reserved_S 6
#define GPRCM_MCU_PSCON_DEBUG_mcu_pscon_rtc_ps_M \
                                0x00000038  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_MCU_PSCON_DEBUG_mcu_pscon_rtc_ps_S 3
#define GPRCM_MCU_PSCON_DEBUG_mcu_pscon_sys_ps_M \
                                0x00000007

#define GPRCM_MCU_PSCON_DEBUG_mcu_pscon_sys_ps_S 0






#define GPRCM_MEMSS_PWR_PS_reserved_M \
                                0xFFFFFFF8

#define GPRCM_MEMSS_PWR_PS_reserved_S 3
#define GPRCM_MEMSS_PWR_PS_pwr_ps_memss_M \
                                0x00000007  
                                            
                                            
                                            
                                            

#define GPRCM_MEMSS_PWR_PS_pwr_ps_memss_S 0






#define GPRCM_REF_FSM_DEBUG_reserved_M \
                                0xFFFFFFC0

#define GPRCM_REF_FSM_DEBUG_reserved_S 6
#define GPRCM_REF_FSM_DEBUG_fref_mode_M \
                                0x00000030  
                                            

#define GPRCM_REF_FSM_DEBUG_fref_mode_S 4
#define GPRCM_REF_FSM_DEBUG_ref_fsm_ps_M \
                                0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_REF_FSM_DEBUG_ref_fsm_ps_S 0






#define GPRCM_MEM_SYS_OPP_REQ_OVERRIDE_reserved_M \
                                0xFFFFFFE0

#define GPRCM_MEM_SYS_OPP_REQ_OVERRIDE_reserved_S 5
#define GPRCM_MEM_SYS_OPP_REQ_OVERRIDE_mem_sys_opp_req_override_ctrl \
                                0x00000010  
                                            
                                            

#define GPRCM_MEM_SYS_OPP_REQ_OVERRIDE_mem_sys_opp_req_override_M \
                                0x0000000F  
                                            

#define GPRCM_MEM_SYS_OPP_REQ_OVERRIDE_mem_sys_opp_req_override_S 0






#define GPRCM_MEM_TESTCTRL_PD_OPP_CONFIG_reserved_M \
                                0xFFFFFFFE

#define GPRCM_MEM_TESTCTRL_PD_OPP_CONFIG_reserved_S 1
#define GPRCM_MEM_TESTCTRL_PD_OPP_CONFIG_mem_sleep_opp_enter_with_testpd_on \
                                0x00000001  
                                            
                                            
                                            
                                            







#define GPRCM_MEM_WL_FAST_CLK_REQ_OVERRIDES_reserved_M \
                                0xFFFFFFF8

#define GPRCM_MEM_WL_FAST_CLK_REQ_OVERRIDES_reserved_S 3
#define GPRCM_MEM_WL_FAST_CLK_REQ_OVERRIDES_mem_wl_fast_clk_req_override_ctrl \
                                0x00000004  

#define GPRCM_MEM_WL_FAST_CLK_REQ_OVERRIDES_mem_wl_fast_clk_req_override \
                                0x00000002  

#define GPRCM_MEM_WL_FAST_CLK_REQ_OVERRIDES_mem_wl_sleep_with_clk_req_override \
                                0x00000001  







#define GPRCM_MEM_MCU_PD_MODE_REQ_OVERRIDES_mem_mcu_pd_mode_req_override_ctrl \
                                0x00000004  
                                            

#define GPRCM_MEM_MCU_PD_MODE_REQ_OVERRIDES_mem_mcu_pd_pwrdn_req_override \
                                0x00000002  
                                            

#define GPRCM_MEM_MCU_PD_MODE_REQ_OVERRIDES_mem_mcu_pd_ret_req_override \
                                0x00000001  
                                            







#define GPRCM_MEM_MCSPI_SRAM_OFF_REQ_OVERRIDES_mem_mcspi_sram_off_req_override_ctrl \
                                0x00000002  
                                            
                                            

#define GPRCM_MEM_MCSPI_SRAM_OFF_REQ_OVERRIDES_mem_mcspi_sram_off_req_override \
                                0x00000001  
                                            
                                            
                                            







#define GPRCM_MEM_WLAN_APLLMCS_OVERRIDES_wlan_apllmcs_lock \
                                0x00000100

#define GPRCM_MEM_WLAN_APLLMCS_OVERRIDES_mem_wlan_apllmcs_enable_override \
                                0x00000002

#define GPRCM_MEM_WLAN_APLLMCS_OVERRIDES_mem_wlan_apllmcs_enable_override_ctrl \
                                0x00000001







#define GPRCM_MEM_REF_FSM_CFG2_MEM_FC_DEASSERT_DELAY_M \
                                0x00380000  
                                            

#define GPRCM_MEM_REF_FSM_CFG2_MEM_FC_DEASSERT_DELAY_S 19
#define GPRCM_MEM_REF_FSM_CFG2_MEM_STARTUP_DEASSERT_DELAY_M \
                                0x00070000  
                                            

#define GPRCM_MEM_REF_FSM_CFG2_MEM_STARTUP_DEASSERT_DELAY_S 16
#define GPRCM_MEM_REF_FSM_CFG2_MEM_EXT_TCXO_SETTLING_TIME_M \
                                0x0000FFFF  
                                            

#define GPRCM_MEM_REF_FSM_CFG2_MEM_EXT_TCXO_SETTLING_TIME_S 0






#define GPRCM_TESTCTRL_POWER_CTRL_TESTCTRL_PD_STATUS_M \
                                0x00000006

#define GPRCM_TESTCTRL_POWER_CTRL_TESTCTRL_PD_STATUS_S 1
#define GPRCM_TESTCTRL_POWER_CTRL_TESTCTRL_PD_ENABLE \
                                0x00000001  
                                            







#define GPRCM_SSDIO_POWER_CTRL_SSDIO_PD_STATUS_M \
                                0x00000006  
                                            

#define GPRCM_SSDIO_POWER_CTRL_SSDIO_PD_STATUS_S 1
#define GPRCM_SSDIO_POWER_CTRL_SSDIO_PD_ENABLE \
                                0x00000001  
                                            







#define GPRCM_MCSPI_N1_POWER_CTRL_MCSPI_N1_PD_STATUS_M \
                                0x00000006  
                                            

#define GPRCM_MCSPI_N1_POWER_CTRL_MCSPI_N1_PD_STATUS_S 1
#define GPRCM_MCSPI_N1_POWER_CTRL_MCSPI_N1_PD_ENABLE \
                                0x00000001  
                                            







#define GPRCM_WELP_POWER_CTRL_WTOP_PD_STATUS_M \
                                0x00001C00

#define GPRCM_WELP_POWER_CTRL_WTOP_PD_STATUS_S 10
#define GPRCM_WELP_POWER_CTRL_WTOP_PD_REQ_OVERRIDE \
                                0x00000200

#define GPRCM_WELP_POWER_CTRL_WTOP_PD_REQ_OVERRIDE_CTRL \
                                0x00000100

#define GPRCM_WELP_POWER_CTRL_WELP_PD_STATUS_M \
                                0x00000006

#define GPRCM_WELP_POWER_CTRL_WELP_PD_STATUS_S 1
#define GPRCM_WELP_POWER_CTRL_WELP_PD_ENABLE \
                                0x00000001  
                                            







#define GPRCM_WL_SDIO_POWER_CTRL_WL_SDIO_PD_STATUS_M \
                                0x00000006

#define GPRCM_WL_SDIO_POWER_CTRL_WL_SDIO_PD_STATUS_S 1
#define GPRCM_WL_SDIO_POWER_CTRL_WL_SDIO_PD_ENABLE \
                                0x00000001  
                                            







#define GPRCM_WLAN_SRAM_ACTIVE_PWR_CFG_WLAN_SRAM_ACTIVE_PWR_CFG_M \
                                0x00FFFFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_WLAN_SRAM_ACTIVE_PWR_CFG_WLAN_SRAM_ACTIVE_PWR_CFG_S 0






#define GPRCM_WLAN_SRAM_SLEEP_PWR_CFG_WLAN_SRAM_SLEEP_PWR_CFG_M \
                                0x00FFFFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_WLAN_SRAM_SLEEP_PWR_CFG_WLAN_SRAM_SLEEP_PWR_CFG_S 0






#define GPRCM_APPS_SECURE_INIT_DONE_SECURE_INIT_DONE_STATUS \
                                0x00000002  
                                            

#define GPRCM_APPS_SECURE_INIT_DONE_APPS_SECURE_INIT_DONE \
                                0x00000001  
                                            
                                            







#define GPRCM_APPS_DEV_MODE_INIT_DONE_APPS_DEV_MODE_INIT_DONE \
                                0x00000001  
                                            
                                            
                                            
                                            
                                            







#define GPRCM_EN_APPS_REBOOT_EN_APPS_REBOOT \
                                0x00000001  
                                            
                                            
                                            
                                            
                                            







#define GPRCM_MEM_APPS_PERIPH_PRESENT_WLAN_GEM_PP \
                                0x00010000  

#define GPRCM_MEM_APPS_PERIPH_PRESENT_APPS_AES_PP \
                                0x00008000

#define GPRCM_MEM_APPS_PERIPH_PRESENT_APPS_DES_PP \
                                0x00004000

#define GPRCM_MEM_APPS_PERIPH_PRESENT_APPS_SHA_PP \
                                0x00002000

#define GPRCM_MEM_APPS_PERIPH_PRESENT_APPS_CAMERA_PP \
                                0x00001000

#define GPRCM_MEM_APPS_PERIPH_PRESENT_APPS_MMCHS_PP \
                                0x00000800

#define GPRCM_MEM_APPS_PERIPH_PRESENT_APPS_MCASP_PP \
                                0x00000400

#define GPRCM_MEM_APPS_PERIPH_PRESENT_APPS_MCSPI_A1_PP \
                                0x00000200

#define GPRCM_MEM_APPS_PERIPH_PRESENT_APPS_MCSPI_A2_PP \
                                0x00000100

#define GPRCM_MEM_APPS_PERIPH_PRESENT_APPS_UDMA_PP \
                                0x00000080

#define GPRCM_MEM_APPS_PERIPH_PRESENT_APPS_WDOG_PP \
                                0x00000040

#define GPRCM_MEM_APPS_PERIPH_PRESENT_APPS_UART_A0_PP \
                                0x00000020

#define GPRCM_MEM_APPS_PERIPH_PRESENT_APPS_UART_A1_PP \
                                0x00000010

#define GPRCM_MEM_APPS_PERIPH_PRESENT_APPS_GPT_A0_PP \
                                0x00000008

#define GPRCM_MEM_APPS_PERIPH_PRESENT_APPS_GPT_A1_PP \
                                0x00000004

#define GPRCM_MEM_APPS_PERIPH_PRESENT_APPS_GPT_A2_PP \
                                0x00000002

#define GPRCM_MEM_APPS_PERIPH_PRESENT_APPS_GPT_A3_PP \
                                0x00000001







#define GPRCM_MEM_NWP_PERIPH_PRESENT_NWP_ASYNC_BRIDGE_PP \
                                0x00000200

#define GPRCM_MEM_NWP_PERIPH_PRESENT_NWP_MCSPI_N2_PP \
                                0x00000100

#define GPRCM_MEM_NWP_PERIPH_PRESENT_NWP_GPT_N0_PP \
                                0x00000080

#define GPRCM_MEM_NWP_PERIPH_PRESENT_NWP_GPT_N1_PP \
                                0x00000040

#define GPRCM_MEM_NWP_PERIPH_PRESENT_NWP_WDOG_PP \
                                0x00000020

#define GPRCM_MEM_NWP_PERIPH_PRESENT_NWP_UDMA_PP \
                                0x00000010

#define GPRCM_MEM_NWP_PERIPH_PRESENT_NWP_UART_N0_PP \
                                0x00000008

#define GPRCM_MEM_NWP_PERIPH_PRESENT_NWP_UART_N1_PP \
                                0x00000004

#define GPRCM_MEM_NWP_PERIPH_PRESENT_NWP_SSDIO_PP \
                                0x00000002

#define GPRCM_MEM_NWP_PERIPH_PRESENT_NWP_MCSPI_N1_PP \
                                0x00000001








#define GPRCM_MEM_SHARED_PERIPH_PRESENT_SHARED_MCSPI_PP \
                                0x00000040

#define GPRCM_MEM_SHARED_PERIPH_PRESENT_SHARED_I2C_PP \
                                0x00000020

#define GPRCM_MEM_SHARED_PERIPH_PRESENT_SHARED_GPIO_A_PP \
                                0x00000010

#define GPRCM_MEM_SHARED_PERIPH_PRESENT_SHARED_GPIO_B_PP \
                                0x00000008

#define GPRCM_MEM_SHARED_PERIPH_PRESENT_SHARED_GPIO_C_PP \
                                0x00000004

#define GPRCM_MEM_SHARED_PERIPH_PRESENT_SHARED_GPIO_D_PP \
                                0x00000002

#define GPRCM_MEM_SHARED_PERIPH_PRESENT_SHARED_GPIO_E_PP \
                                0x00000001







#define GPRCM_NWP_PWR_STATE_NWP_PWR_STATE_PS_M \
                                0x00000F00  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_NWP_PWR_STATE_NWP_PWR_STATE_PS_S 8
#define GPRCM_NWP_PWR_STATE_NWP_RCM_PS_M \
                                0x00000007  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_NWP_PWR_STATE_NWP_RCM_PS_S 0






#define GPRCM_APPS_PWR_STATE_APPS_PWR_STATE_PS_M \
                                0x00000F00  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_APPS_PWR_STATE_APPS_PWR_STATE_PS_S 8
#define GPRCM_APPS_PWR_STATE_APPS_RCM_PS_M \
                                0x00000007  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_APPS_PWR_STATE_APPS_RCM_PS_S 0






#define GPRCM_MCU_PWR_STATE_MCU_OPP_PS_M \
                                0x0000001F  

#define GPRCM_MCU_PWR_STATE_MCU_OPP_PS_S 0






#define GPRCM_WTOP_PM_PS_WTOP_PM_PS_M \
                                0x00000007  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_WTOP_PM_PS_WTOP_PM_PS_S 0






#define GPRCM_WTOP_PD_RESETZ_OVERRIDE_REG_WTOP_PD_RESETZ_OVERRIDE_CTRL \
                                0x00000100  
                                            
                                            
                                            

#define GPRCM_WTOP_PD_RESETZ_OVERRIDE_REG_WTOP_PD_RESETZ_OVERRIDE \
                                0x00000001  
                                            
                                            







#define GPRCM_WELP_PD_RESETZ_OVERRIDE_REG_WELP_PD_RESETZ_OVERRIDE_CTRL \
                                0x00000100  
                                            
                                            
                                            

#define GPRCM_WELP_PD_RESETZ_OVERRIDE_REG_WELP_PD_RESETZ_OVERRIDE \
                                0x00000001  
                                            
                                            







#define GPRCM_WL_SDIO_PD_RESETZ_OVERRIDE_REG_WL_SDIO_PD_RESETZ_OVERRIDE_CTRL \
                                0x00000100  
                                            
                                            
                                            

#define GPRCM_WL_SDIO_PD_RESETZ_OVERRIDE_REG_WL_SDIO_PD_RESETZ_OVERRIDE \
                                0x00000001  
                                            
                                            







#define GPRCM_SSDIO_PD_RESETZ_OVERRIDE_REG_SSDIO_PD_RESETZ_OVERRIDE_CTRL \
                                0x00000100  
                                            
                                            
                                            

#define GPRCM_SSDIO_PD_RESETZ_OVERRIDE_REG_SSDIO_PD_RESETZ_OVERRIDE \
                                0x00000001  
                                            
                                            







#define GPRCM_MCSPI_N1_PD_RESETZ_OVERRIDE_REG_MCSPI_N1_PD_RESETZ_OVERRIDE_CTRL \
                                0x00000100  
                                            
                                            
                                            

#define GPRCM_MCSPI_N1_PD_RESETZ_OVERRIDE_REG_MCSPI_N1_PD_RESETZ_OVERRIDE \
                                0x00000001  
                                            
                                            







#define GPRCM_TESTCTRL_PD_RESETZ_OVERRIDE_REG_TESTCTRL_PD_RESETZ_OVERRIDE_CTRL \
                                0x00000100  
                                            
                                            
                                            

#define GPRCM_TESTCTRL_PD_RESETZ_OVERRIDE_REG_TESTCTRL_PD_RESETZ_OVERRIDE \
                                0x00000001  
                                            
                                            







#define GPRCM_MCU_PD_RESETZ_OVERRIDE_REG_MCU_PD_RESETZ_OVERRIDE_CTRL \
                                0x00000100  
                                            
                                            

#define GPRCM_MCU_PD_RESETZ_OVERRIDE_REG_MCU_PD_RESETZ_OVERRIDE \
                                0x00000001  
                                            
                                            







#define GPRCM_GPRCM_EFUSE_READ_REG0_FUSEFARM_ROW_14_M \
                                0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_GPRCM_EFUSE_READ_REG0_FUSEFARM_ROW_14_S 0






#define GPRCM_GPRCM_EFUSE_READ_REG1_FUSEFARM_ROW_15_LSW_M \
                                0x0000FFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_GPRCM_EFUSE_READ_REG1_FUSEFARM_ROW_15_LSW_S 0






#define GPRCM_GPRCM_EFUSE_READ_REG2_FUSEFARM_ROW_16_LSW_ROW_15_MSW_M \
                                0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_GPRCM_EFUSE_READ_REG2_FUSEFARM_ROW_16_LSW_ROW_15_MSW_S 0






#define GPRCM_GPRCM_EFUSE_READ_REG3_FUSEFARM_ROW_17_LSW_ROW_16_MSW_M \
                                0xFFFFFFFF  
                                            
                                            
                                            

#define GPRCM_GPRCM_EFUSE_READ_REG3_FUSEFARM_ROW_17_LSW_ROW_16_MSW_S 0






#define GPRCM_WTOP_MEM_RET_CFG_WTOP_MEM_RET_CFG \
                                0x00000001  
                                            
                                            
                                            
                                            
                                            







#define GPRCM_COEX_CLK_SWALLOW_CFG0_Q_FACTOR_M \
                                0x007FFFFF  

#define GPRCM_COEX_CLK_SWALLOW_CFG0_Q_FACTOR_S 0






#define GPRCM_COEX_CLK_SWALLOW_CFG1_P_FACTOR_M \
                                0x000FFFFF  

#define GPRCM_COEX_CLK_SWALLOW_CFG1_P_FACTOR_S 0






#define GPRCM_COEX_CLK_SWALLOW_CFG2_CONSECUTIVE_SWALLOW_M \
                                0x00000018

#define GPRCM_COEX_CLK_SWALLOW_CFG2_CONSECUTIVE_SWALLOW_S 3
#define GPRCM_COEX_CLK_SWALLOW_CFG2_PRBS_GAIN \
                                0x00000004

#define GPRCM_COEX_CLK_SWALLOW_CFG2_PRBS_ENABLE \
                                0x00000002

#define GPRCM_COEX_CLK_SWALLOW_CFG2_SWALLOW_ENABLE \
                                0x00000001  







#define GPRCM_COEX_CLK_SWALLOW_ENABLE_COEX_CLK_SWALLOW_ENABLE \
                                0x00000001  
                                            
                                            
                                            







#define GPRCM_DCDC_CLK_GEN_CONFIG_DCDC_CLK_ENABLE \
                                0x00000001  
                                            
                                            







#define GPRCM_GPRCM_EFUSE_READ_REG4_FUSEFARM_ROW_17_MSW_M \
                                0x0000FFFF  
                                            
                                            

#define GPRCM_GPRCM_EFUSE_READ_REG4_FUSEFARM_ROW_17_MSW_S 0






#define GPRCM_GPRCM_EFUSE_READ_REG5_FUSEFARM_ROW_18_M \
                                0xFFFFFFFF  
                                            
                                            
                                            
                                            

#define GPRCM_GPRCM_EFUSE_READ_REG5_FUSEFARM_ROW_18_S 0






#define GPRCM_GPRCM_EFUSE_READ_REG6_FUSEFARM_ROW_19_LSW_M \
                                0x0000FFFF  
                                            
                                            

#define GPRCM_GPRCM_EFUSE_READ_REG6_FUSEFARM_ROW_19_LSW_S 0






#define GPRCM_GPRCM_EFUSE_READ_REG7_FUSEFARM_ROW_20_LSW_ROW_19_MSW_M \
                                0xFFFFFFFF  
                                            
                                            

#define GPRCM_GPRCM_EFUSE_READ_REG7_FUSEFARM_ROW_20_LSW_ROW_19_MSW_S 0






#define GPRCM_GPRCM_EFUSE_READ_REG8_FUSEFARM_ROW_21_LSW_ROW_20_MSW_M \
                                0xFFFFFFFF  
                                            
                                            

#define GPRCM_GPRCM_EFUSE_READ_REG8_FUSEFARM_ROW_21_LSW_ROW_20_MSW_S 0






#define GPRCM_GPRCM_EFUSE_READ_REG9_FUSEFARM_ROW_22_LSW_ROW_21_MSW_M \
                                0xFFFFFFFF  
                                            
                                            

#define GPRCM_GPRCM_EFUSE_READ_REG9_FUSEFARM_ROW_22_LSW_ROW_21_MSW_S 0






#define GPRCM_GPRCM_EFUSE_READ_REG10_FUSEFARM_ROW_23_LSW_ROW_22_MSW_M \
                                0xFFFFFFFF  
                                            
                                            

#define GPRCM_GPRCM_EFUSE_READ_REG10_FUSEFARM_ROW_23_LSW_ROW_22_MSW_S 0






#define GPRCM_GPRCM_EFUSE_READ_REG11_FUSEFARM_ROW_24_LSW_ROW_23_MSW_M \
                                0xFFFFFFFF  
                                            
                                            

#define GPRCM_GPRCM_EFUSE_READ_REG11_FUSEFARM_ROW_24_LSW_ROW_23_MSW_S 0






#define GPRCM_GPRCM_DIEID_READ_REG0_FUSEFARM_191_160_M \
                                0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            

#define GPRCM_GPRCM_DIEID_READ_REG0_FUSEFARM_191_160_S 0






#define GPRCM_GPRCM_DIEID_READ_REG1_FUSEFARM_223_192_M \
                                0xFFFFFFFF  
                                            
                                            
                                            

#define GPRCM_GPRCM_DIEID_READ_REG1_FUSEFARM_223_192_S 0






#define GPRCM_GPRCM_DIEID_READ_REG2_FUSEFARM_255_224_M \
                                0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            

#define GPRCM_GPRCM_DIEID_READ_REG2_FUSEFARM_255_224_S 0






#define GPRCM_GPRCM_DIEID_READ_REG3_FUSEFARM_287_256_M \
                                0xFFFFFFFF  
                                            
                                            
                                            

#define GPRCM_GPRCM_DIEID_READ_REG3_FUSEFARM_287_256_S 0






#define GPRCM_GPRCM_DIEID_READ_REG4_FUSEFARM_319_288_M \
                                0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define GPRCM_GPRCM_DIEID_READ_REG4_FUSEFARM_319_288_S 0






#define GPRCM_APPS_SS_OVERRIDES_reserved_M \
                                0xFFFFFC00

#define GPRCM_APPS_SS_OVERRIDES_reserved_S 10
#define GPRCM_APPS_SS_OVERRIDES_mem_apps_refclk_gating_override \
                                0x00000200

#define GPRCM_APPS_SS_OVERRIDES_mem_apps_refclk_gating_override_ctrl \
                                0x00000100

#define GPRCM_APPS_SS_OVERRIDES_mem_apps_pllclk_gating_override \
                                0x00000080

#define GPRCM_APPS_SS_OVERRIDES_mem_apps_pllclk_gating_override_ctrl \
                                0x00000040

#define GPRCM_APPS_SS_OVERRIDES_mem_apps_por_rstn_override \
                                0x00000020

#define GPRCM_APPS_SS_OVERRIDES_mem_apps_sysrstn_override \
                                0x00000010

#define GPRCM_APPS_SS_OVERRIDES_mem_apps_sysclk_gating_override \
                                0x00000008

#define GPRCM_APPS_SS_OVERRIDES_mem_apps_por_rstn_override_ctrl \
                                0x00000004

#define GPRCM_APPS_SS_OVERRIDES_mem_apps_sysrstn_override_ctrl \
                                0x00000002

#define GPRCM_APPS_SS_OVERRIDES_mem_apps_sysclk_gating_override_ctrl \
                                0x00000001







#define GPRCM_NWP_SS_OVERRIDES_reserved_M \
                                0xFFFFFC00

#define GPRCM_NWP_SS_OVERRIDES_reserved_S 10
#define GPRCM_NWP_SS_OVERRIDES_mem_nwp_refclk_gating_override \
                                0x00000200

#define GPRCM_NWP_SS_OVERRIDES_mem_nwp_refclk_gating_override_ctrl \
                                0x00000100

#define GPRCM_NWP_SS_OVERRIDES_mem_nwp_pllclk_gating_override \
                                0x00000080

#define GPRCM_NWP_SS_OVERRIDES_mem_nwp_pllclk_gating_override_ctrl \
                                0x00000040

#define GPRCM_NWP_SS_OVERRIDES_mem_nwp_por_rstn_override \
                                0x00000020

#define GPRCM_NWP_SS_OVERRIDES_mem_nwp_sysrstn_override \
                                0x00000010

#define GPRCM_NWP_SS_OVERRIDES_mem_nwp_sysclk_gating_override \
                                0x00000008

#define GPRCM_NWP_SS_OVERRIDES_mem_nwp_por_rstn_override_ctrl \
                                0x00000004

#define GPRCM_NWP_SS_OVERRIDES_mem_nwp_sysrstn_override_ctrl \
                                0x00000002

#define GPRCM_NWP_SS_OVERRIDES_mem_nwp_sysclk_gating_override_ctrl \
                                0x00000001







#define GPRCM_SHARED_SS_OVERRIDES_reserved_M \
                                0xFFFFFF00

#define GPRCM_SHARED_SS_OVERRIDES_reserved_S 8
#define GPRCM_SHARED_SS_OVERRIDES_mem_shared_pllclk_gating_override_ctrl \
                                0x00000080

#define GPRCM_SHARED_SS_OVERRIDES_mem_shared_pllclk_gating_override \
                                0x00000040

#define GPRCM_SHARED_SS_OVERRIDES_mem_shared_refclk_gating_override_ctrl \
                                0x00000020

#define GPRCM_SHARED_SS_OVERRIDES_mem_shared_refclk_gating_override \
                                0x00000010

#define GPRCM_SHARED_SS_OVERRIDES_mem_shared_rstn_override \
                                0x00000008

#define GPRCM_SHARED_SS_OVERRIDES_mem_shared_sysclk_gating_override \
                                0x00000004

#define GPRCM_SHARED_SS_OVERRIDES_mem_shared_rstn_override_ctrl \
                                0x00000002

#define GPRCM_SHARED_SS_OVERRIDES_mem_shared_sysclk_gating_override_ctrl \
                                0x00000001







#define GPRCM_IDMEM_CORE_RST_OVERRIDES_reserved_M \
                                0xFFFFFF00

#define GPRCM_IDMEM_CORE_RST_OVERRIDES_reserved_S 8
#define GPRCM_IDMEM_CORE_RST_OVERRIDES_mem_idmem_core_sysrstn_override \
                                0x00000080

#define GPRCM_IDMEM_CORE_RST_OVERRIDES_mem_idmem_core_fmc_rstn_override \
                                0x00000040

#define GPRCM_IDMEM_CORE_RST_OVERRIDES_SPARE_RW1 \
                                0x00000020

#define GPRCM_IDMEM_CORE_RST_OVERRIDES_mem_idmem_core_piosc_gating_override \
                                0x00000010

#define GPRCM_IDMEM_CORE_RST_OVERRIDES_mem_idmem_core_sysrstn_override_ctrl \
                                0x00000008

#define GPRCM_IDMEM_CORE_RST_OVERRIDES_mem_idmem_core_fmc_rstn_override_ctrl \
                                0x00000004

#define GPRCM_IDMEM_CORE_RST_OVERRIDES_SPARE_RW0 \
                                0x00000002

#define GPRCM_IDMEM_CORE_RST_OVERRIDES_mem_idmem_core_piosc_gating_override_ctrl \
                                0x00000001







#define GPRCM_TOP_DIE_FSM_OVERRIDES_reserved_M \
                                0xFFFFF000

#define GPRCM_TOP_DIE_FSM_OVERRIDES_reserved_S 12
#define GPRCM_TOP_DIE_FSM_OVERRIDES_mem_d2d_pwr_switch_pgoodin_override_ctrl \
                                0x00000800

#define GPRCM_TOP_DIE_FSM_OVERRIDES_mem_d2d_pwr_switch_pgoodin_override \
                                0x00000400

#define GPRCM_TOP_DIE_FSM_OVERRIDES_mem_d2d_hclk_gating_override \
                                0x00000200

#define GPRCM_TOP_DIE_FSM_OVERRIDES_mem_d2d_piosc_gating_override \
                                0x00000100

#define GPRCM_TOP_DIE_FSM_OVERRIDES_mem_d2d_rstn_override \
                                0x00000080

#define GPRCM_TOP_DIE_FSM_OVERRIDES_mem_d2d_pwr_switch_ponin_override \
                                0x00000040

#define GPRCM_TOP_DIE_FSM_OVERRIDES_mem_flash_ready_override \
                                0x00000020

#define GPRCM_TOP_DIE_FSM_OVERRIDES_mem_d2d_hclk_gating_override_ctrl \
                                0x00000010

#define GPRCM_TOP_DIE_FSM_OVERRIDES_mem_d2d_piosc_gating_override_ctrl \
                                0x00000008

#define GPRCM_TOP_DIE_FSM_OVERRIDES_mem_d2d_rstn_override_ctrl \
                                0x00000004

#define GPRCM_TOP_DIE_FSM_OVERRIDES_mem_d2d_pwr_switch_ponin_override_ctrl \
                                0x00000002

#define GPRCM_TOP_DIE_FSM_OVERRIDES_mem_flash_ready_override_ctrl \
                                0x00000001







#define GPRCM_MCU_PSCON_OVERRIDES_reserved_M \
                                0xFFF00000

#define GPRCM_MCU_PSCON_OVERRIDES_reserved_S 20
#define GPRCM_MCU_PSCON_OVERRIDES_mem_mcu_pscon_mem_sleep_override_ctrl \
                                0x00080000

#define GPRCM_MCU_PSCON_OVERRIDES_mem_mcu_pscon_mem_update_override_ctrl \
                                0x00040000

#define GPRCM_MCU_PSCON_OVERRIDES_mem_mcu_pscon_mem_off_override_ctrl \
                                0x00020000

#define GPRCM_MCU_PSCON_OVERRIDES_mem_mcu_pscon_mem_retain_override_ctrl \
                                0x00010000

#define GPRCM_MCU_PSCON_OVERRIDES_NU1_M \
                                0x0000FC00

#define GPRCM_MCU_PSCON_OVERRIDES_NU1_S 10
#define GPRCM_MCU_PSCON_OVERRIDES_mem_mcu_pscon_sleep_override \
                                0x00000200

#define GPRCM_MCU_PSCON_OVERRIDES_mem_mcu_pscon_mem_update_override \
                                0x00000100

#define GPRCM_MCU_PSCON_OVERRIDES_mem_mcu_pscon_mem_off_override_M \
                                0x000000F0

#define GPRCM_MCU_PSCON_OVERRIDES_mem_mcu_pscon_mem_off_override_S 4
#define GPRCM_MCU_PSCON_OVERRIDES_mem_mcu_pscon_mem_retain_override_M \
                                0x0000000F

#define GPRCM_MCU_PSCON_OVERRIDES_mem_mcu_pscon_mem_retain_override_S 0






#define GPRCM_WTOP_PSCON_OVERRIDES_reserved_M \
                                0xFFC00000

#define GPRCM_WTOP_PSCON_OVERRIDES_reserved_S 22
#define GPRCM_WTOP_PSCON_OVERRIDES_mem_wtop_pscon_sleep_override_ctrl \
                                0x00200000

#define GPRCM_WTOP_PSCON_OVERRIDES_mem_wtop_pscon_mem_update_override_ctrl \
                                0x00100000

#define GPRCM_WTOP_PSCON_OVERRIDES_mem_wtop_pscon_mem_off_override_ctrl \
                                0x00080000

#define GPRCM_WTOP_PSCON_OVERRIDES_mem_wtop_pscon_mem_retain_override_ctrl \
                                0x00040000

#define GPRCM_WTOP_PSCON_OVERRIDES_mem_wtop_pscon_sleep_override \
                                0x00020000

#define GPRCM_WTOP_PSCON_OVERRIDES_mem_wtop_pscon_mem_update_override \
                                0x00010000

#define GPRCM_WTOP_PSCON_OVERRIDES_mem_wtop_pscon_mem_off_override_M \
                                0x0000FF00

#define GPRCM_WTOP_PSCON_OVERRIDES_mem_wtop_pscon_mem_off_override_S 8
#define GPRCM_WTOP_PSCON_OVERRIDES_mem_wtop_pscon_mem_retain_override_M \
                                0x000000FF

#define GPRCM_WTOP_PSCON_OVERRIDES_mem_wtop_pscon_mem_retain_override_S 0






#define GPRCM_WELP_PSCON_OVERRIDES_reserved_M \
                                0xFFFFFFFC

#define GPRCM_WELP_PSCON_OVERRIDES_reserved_S 2
#define GPRCM_WELP_PSCON_OVERRIDES_mem_welp_pscon_sleep_override_ctrl \
                                0x00000002

#define GPRCM_WELP_PSCON_OVERRIDES_mem_welp_pscon_sleep_override \
                                0x00000001







#define GPRCM_WL_SDIO_PSCON_OVERRIDES_reserved_M \
                                0xFFFFFFFC

#define GPRCM_WL_SDIO_PSCON_OVERRIDES_reserved_S 2
#define GPRCM_WL_SDIO_PSCON_OVERRIDES_mem_wl_sdio_pscon_sleep_override_ctrl \
                                0x00000002

#define GPRCM_WL_SDIO_PSCON_OVERRIDES_mem_wl_sdio_pscon_sleep_override \
                                0x00000001







#define GPRCM_MCSPI_PSCON_OVERRIDES_reserved_M \
                                0xFFFFFF00

#define GPRCM_MCSPI_PSCON_OVERRIDES_reserved_S 8
#define GPRCM_MCSPI_PSCON_OVERRIDES_mem_mcspi_pscon_mem_retain_override_ctrl \
                                0x00000080

#define GPRCM_MCSPI_PSCON_OVERRIDES_mem_mcspi_pscon_mem_off_override_ctrl \
                                0x00000040

#define GPRCM_MCSPI_PSCON_OVERRIDES_mem_mcspi_pscon_mem_retain_override \
                                0x00000020

#define GPRCM_MCSPI_PSCON_OVERRIDES_mem_mcspi_pscon_mem_off_override \
                                0x00000010

#define GPRCM_MCSPI_PSCON_OVERRIDES_mem_mcspi_pscon_mem_update_override_ctrl \
                                0x00000008

#define GPRCM_MCSPI_PSCON_OVERRIDES_mem_mcspi_pscon_mem_update_override \
                                0x00000004

#define GPRCM_MCSPI_PSCON_OVERRIDES_mem_mcspi_pscon_sleep_override_ctrl \
                                0x00000002

#define GPRCM_MCSPI_PSCON_OVERRIDES_mem_mcspi_pscon_sleep_override \
                                0x00000001







#define GPRCM_SSDIO_PSCON_OVERRIDES_reserved_M \
                                0xFFFFFFFC

#define GPRCM_SSDIO_PSCON_OVERRIDES_reserved_S 2
#define GPRCM_SSDIO_PSCON_OVERRIDES_mem_ssdio_pscon_sleep_override_ctrl \
                                0x00000002

#define GPRCM_SSDIO_PSCON_OVERRIDES_mem_ssdio_pscon_sleep_override \
                                0x00000001




#endif 
