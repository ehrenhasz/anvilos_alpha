
#ifndef HAL_DATA_H_
#define HAL_DATA_H_
#include <stdint.h>
#include "common_data.h"
#include "r_lpm.h"
#include "r_lpm_api.h"
#include "r_flash_hp.h"
#include "r_flash_api.h"
FSP_HEADER


extern const lpm_instance_t g_lpm0;


extern lpm_instance_ctrl_t g_lpm0_ctrl;
extern const lpm_cfg_t g_lpm0_cfg;


extern const flash_instance_t g_flash0;


extern flash_hp_instance_ctrl_t g_flash0_ctrl;
extern const flash_cfg_t g_flash0_cfg;

#ifndef NULL
void NULL(flash_callback_args_t *p_args);
#endif

FSP_FOOTER
#endif 
