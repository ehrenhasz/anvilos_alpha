
#ifndef HAL_DATA_H_
#define HAL_DATA_H_
#include <stdint.h>
#include "bsp_api.h"
#include "common_data.h"
#include "r_dtc.h"
#include "r_transfer_api.h"
#include "r_sdhi.h"
#include "r_sdmmc_api.h"
#include "r_qspi.h"
#include "r_spi_flash_api.h"
#include "r_iic_master.h"
#include "r_i2c_master_api.h"
#include "r_adc.h"
#include "r_adc_api.h"
#include "r_lpm.h"
#include "r_lpm_api.h"
#include "r_spi.h"
#include "r_agt.h"
#include "r_timer_api.h"
#include "r_flash_hp.h"
#include "r_flash_api.h"
#include "r_rtc.h"
#include "r_rtc_api.h"
#include "r_sci_uart.h"
#include "r_uart_api.h"
FSP_HEADER

extern const transfer_instance_t g_transfer2;


extern dtc_instance_ctrl_t g_transfer2_ctrl;
extern const transfer_cfg_t g_transfer2_cfg;

extern const sdmmc_instance_t g_sdmmc0;


extern sdhi_instance_ctrl_t g_sdmmc0_ctrl;
extern sdmmc_cfg_t g_sdmmc0_cfg;

#ifndef sdhi_ISR
void sdhi_ISR(sdmmc_callback_args_t *p_args);
#endif
extern const spi_flash_instance_t g_qspi0;
extern qspi_instance_ctrl_t g_qspi0_ctrl;
extern const spi_flash_cfg_t g_qspi0_cfg;

extern const i2c_master_instance_t g_i2c_master2;


extern iic_master_instance_ctrl_t g_i2c_master2_ctrl;
extern const i2c_master_cfg_t g_i2c_master2_cfg;

#ifndef callback_iic
void callback_iic(i2c_master_callback_args_t *p_args);
#endif

extern const adc_instance_t g_adc1;


extern adc_instance_ctrl_t g_adc1_ctrl;
extern const adc_cfg_t g_adc1_cfg;
extern const adc_channel_cfg_t g_adc1_channel_cfg;

#ifndef NULL
void NULL(adc_callback_args_t *p_args);
#endif

#ifndef NULL
#define ADC_DMAC_CHANNELS_PER_BLOCK_NULL  0
#endif

extern const adc_instance_t g_adc0;


extern adc_instance_ctrl_t g_adc0_ctrl;
extern const adc_cfg_t g_adc0_cfg;
extern const adc_channel_cfg_t g_adc0_channel_cfg;

#ifndef NULL
void NULL(adc_callback_args_t *p_args);
#endif

#ifndef NULL
#define ADC_DMAC_CHANNELS_PER_BLOCK_NULL  0
#endif

extern const lpm_instance_t g_lpm0;


extern lpm_instance_ctrl_t g_lpm0_ctrl;
extern const lpm_cfg_t g_lpm0_cfg;

extern const transfer_instance_t g_transfer1;


extern dtc_instance_ctrl_t g_transfer1_ctrl;
extern const transfer_cfg_t g_transfer1_cfg;

extern const transfer_instance_t g_transfer0;


extern dtc_instance_ctrl_t g_transfer0_ctrl;
extern const transfer_cfg_t g_transfer0_cfg;

extern const spi_instance_t g_spi0;


extern spi_instance_ctrl_t g_spi0_ctrl;
extern const spi_cfg_t g_spi0_cfg;


#ifndef spi_callback
void spi_callback(spi_callback_args_t *p_args);
#endif

#define RA_NOT_DEFINED (1)
#if (RA_NOT_DEFINED == g_transfer0)
    #define g_spi0_P_TRANSFER_TX (NULL)
#else
#define g_spi0_P_TRANSFER_TX (&g_transfer0)
#endif
#if (RA_NOT_DEFINED == g_transfer1)
    #define g_spi0_P_TRANSFER_RX (NULL)
#else
#define g_spi0_P_TRANSFER_RX (&g_transfer1)
#endif
#undef RA_NOT_DEFINED

extern const timer_instance_t g_timer1;


extern agt_instance_ctrl_t g_timer1_ctrl;
extern const timer_cfg_t g_timer1_cfg;

#ifndef callback_agt
void callback_agt(timer_callback_args_t *p_args);
#endif

extern const timer_instance_t g_timer0;


extern agt_instance_ctrl_t g_timer0_ctrl;
extern const timer_cfg_t g_timer0_cfg;

#ifndef callback_agt
void callback_agt(timer_callback_args_t *p_args);
#endif

extern const flash_instance_t g_flash0;


extern flash_hp_instance_ctrl_t g_flash0_ctrl;
extern const flash_cfg_t g_flash0_cfg;

#ifndef NULL
void NULL(flash_callback_args_t *p_args);
#endif

extern const rtc_instance_t g_rtc0;


extern rtc_instance_ctrl_t g_rtc0_ctrl;
extern const rtc_cfg_t g_rtc0_cfg;

#ifndef NULL
void NULL(rtc_callback_args_t *p_args);
#endif

extern const uart_instance_t g_uart9;


extern sci_uart_instance_ctrl_t g_uart9_ctrl;
extern const uart_cfg_t g_uart9_cfg;
extern const sci_uart_extended_cfg_t g_uart9_cfg_extend;

#ifndef user_uart_callback
void user_uart_callback(uart_callback_args_t *p_args);
#endif

extern const uart_instance_t g_uart7;


extern sci_uart_instance_ctrl_t g_uart7_ctrl;
extern const uart_cfg_t g_uart7_cfg;
extern const sci_uart_extended_cfg_t g_uart7_cfg_extend;

#ifndef user_uart_callback
void user_uart_callback(uart_callback_args_t *p_args);
#endif

extern const uart_instance_t g_uart6;


extern sci_uart_instance_ctrl_t g_uart6_ctrl;
extern const uart_cfg_t g_uart6_cfg;
extern const sci_uart_extended_cfg_t g_uart6_cfg_extend;

#ifndef user_uart_callback
void user_uart_callback(uart_callback_args_t *p_args);
#endif
void g_hal_init(void);
FSP_FOOTER
#endif 
