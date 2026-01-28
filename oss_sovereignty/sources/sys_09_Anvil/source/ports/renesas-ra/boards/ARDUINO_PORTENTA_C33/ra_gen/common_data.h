
#ifndef COMMON_DATA_H_
#define COMMON_DATA_H_
#include <stdint.h>
#include "bsp_api.h"
#include "r_icu.h"
#include "r_external_irq_api.h"
#include "r_sce.h"
#include "r_ioport.h"
#include "bsp_pin_cfg.h"
FSP_HEADER

extern const external_irq_instance_t g_external_irq1;


extern icu_instance_ctrl_t g_external_irq1_ctrl;
extern const external_irq_cfg_t g_external_irq1_cfg;

#ifndef NULL
void NULL(external_irq_callback_args_t *p_args);
#endif
extern sce_instance_ctrl_t sce_ctrl;
extern const sce_cfg_t sce_cfg;

extern const external_irq_instance_t g_external_irq0;


extern icu_instance_ctrl_t g_external_irq0_ctrl;
extern const external_irq_cfg_t g_external_irq0_cfg;

#ifndef NULL
void NULL(external_irq_callback_args_t *p_args);
#endif

extern const ioport_instance_t g_ioport;


extern ioport_instance_ctrl_t g_ioport_ctrl;
void g_common_init(void);
FSP_FOOTER
#endif 
