#ifndef __SYSTEM_LOCAL_H_INCLUDED__
#define __SYSTEM_LOCAL_H_INCLUDED__
#ifdef HRT_ISP_CSS_CUSTOM_HOST
#ifndef HRT_USE_VIR_ADDRS
#define HRT_USE_VIR_ADDRS
#endif
#endif
#include "system_global.h"
#include "hive_types.h"
#define GP_FIFO_BASE   ((hrt_address)0x0000000000090104)		 
extern const hrt_address ISP_CTRL_BASE[N_ISP_ID];
extern const hrt_address ISP_DMEM_BASE[N_ISP_ID];
extern const hrt_address ISP_BAMEM_BASE[N_BAMEM_ID];
extern const hrt_address SP_CTRL_BASE[N_SP_ID];
extern const hrt_address SP_DMEM_BASE[N_SP_ID];
extern const hrt_address MMU_BASE[N_MMU_ID];
extern const hrt_address DMA_BASE[N_DMA_ID];
extern const hrt_address ISYS2401_DMA_BASE[N_ISYS2401_DMA_ID];
extern const hrt_address IRQ_BASE[N_IRQ_ID];
extern const hrt_address GDC_BASE[N_GDC_ID];
extern const hrt_address FIFO_MONITOR_BASE[N_FIFO_MONITOR_ID];
extern const hrt_address GP_DEVICE_BASE[N_GP_DEVICE_ID];
extern const hrt_address GP_TIMER_BASE;
extern const hrt_address GPIO_BASE[N_GPIO_ID];
extern const hrt_address TIMED_CTRL_BASE[N_TIMED_CTRL_ID];
extern const hrt_address INPUT_FORMATTER_BASE[N_INPUT_FORMATTER_ID];
extern const hrt_address INPUT_SYSTEM_BASE[N_INPUT_SYSTEM_ID];
extern const hrt_address RX_BASE[N_RX_ID];
extern const hrt_address IBUF_CTRL_BASE[N_IBUF_CTRL_ID];
extern const hrt_address ISYS_IRQ_BASE[N_ISYS_IRQ_ID];
extern const hrt_address CSI_RX_FE_CTRL_BASE[N_CSI_RX_FRONTEND_ID];
extern const hrt_address CSI_RX_BE_CTRL_BASE[N_CSI_RX_BACKEND_ID];
extern const hrt_address PIXELGEN_CTRL_BASE[N_PIXELGEN_ID];
extern const hrt_address STREAM2MMIO_CTRL_BASE[N_STREAM2MMIO_ID];
#endif  
