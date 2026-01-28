#ifndef __IA_CSS_IRQ_H
#define __IA_CSS_IRQ_H
#include "ia_css_err.h"
#include "ia_css_pipe_public.h"
#include "ia_css_input_port.h"
#include <linux/bits.h>
enum ia_css_irq_type {
	IA_CSS_IRQ_TYPE_EDGE,   
	IA_CSS_IRQ_TYPE_PULSE   
};
enum ia_css_irq_info {
	IA_CSS_IRQ_INFO_CSS_RECEIVER_ERROR            = BIT(0),
	IA_CSS_IRQ_INFO_CSS_RECEIVER_FIFO_OVERFLOW    = BIT(1),
	IA_CSS_IRQ_INFO_CSS_RECEIVER_SOF              = BIT(2),
	IA_CSS_IRQ_INFO_CSS_RECEIVER_EOF              = BIT(3),
	IA_CSS_IRQ_INFO_CSS_RECEIVER_SOL              = BIT(4),
	IA_CSS_IRQ_INFO_EVENTS_READY                  = BIT(5),
	IA_CSS_IRQ_INFO_CSS_RECEIVER_EOL              = BIT(6),
	IA_CSS_IRQ_INFO_CSS_RECEIVER_SIDEBAND_CHANGED = BIT(7),
	IA_CSS_IRQ_INFO_CSS_RECEIVER_GEN_SHORT_0      = BIT(8),
	IA_CSS_IRQ_INFO_CSS_RECEIVER_GEN_SHORT_1      = BIT(9),
	IA_CSS_IRQ_INFO_IF_PRIM_ERROR                 = BIT(10),
	IA_CSS_IRQ_INFO_IF_PRIM_B_ERROR               = BIT(11),
	IA_CSS_IRQ_INFO_IF_SEC_ERROR                  = BIT(12),
	IA_CSS_IRQ_INFO_STREAM_TO_MEM_ERROR           = BIT(13),
	IA_CSS_IRQ_INFO_SW_0                          = BIT(14),
	IA_CSS_IRQ_INFO_SW_1                          = BIT(15),
	IA_CSS_IRQ_INFO_SW_2                          = BIT(16),
	IA_CSS_IRQ_INFO_ISP_BINARY_STATISTICS_READY   = BIT(17),
	IA_CSS_IRQ_INFO_INPUT_SYSTEM_ERROR            = BIT(18),
	IA_CSS_IRQ_INFO_IF_ERROR                      = BIT(19),
	IA_CSS_IRQ_INFO_DMA_ERROR                     = BIT(20),
	IA_CSS_IRQ_INFO_ISYS_EVENTS_READY             = BIT(21),
};
enum ia_css_rx_irq_info {
	IA_CSS_RX_IRQ_INFO_BUFFER_OVERRUN   = BIT(0),   
	IA_CSS_RX_IRQ_INFO_ENTER_SLEEP_MODE = BIT(1),   
	IA_CSS_RX_IRQ_INFO_EXIT_SLEEP_MODE  = BIT(2),   
	IA_CSS_RX_IRQ_INFO_ECC_CORRECTED    = BIT(3),   
	IA_CSS_RX_IRQ_INFO_ERR_SOT          = BIT(4),
	IA_CSS_RX_IRQ_INFO_ERR_SOT_SYNC     = BIT(5),   
	IA_CSS_RX_IRQ_INFO_ERR_CONTROL      = BIT(6),   
	IA_CSS_RX_IRQ_INFO_ERR_ECC_DOUBLE   = BIT(7),   
	IA_CSS_RX_IRQ_INFO_ERR_CRC          = BIT(8),   
	IA_CSS_RX_IRQ_INFO_ERR_UNKNOWN_ID   = BIT(9),   
	IA_CSS_RX_IRQ_INFO_ERR_FRAME_SYNC   = BIT(10),  
	IA_CSS_RX_IRQ_INFO_ERR_FRAME_DATA   = BIT(11),  
	IA_CSS_RX_IRQ_INFO_ERR_DATA_TIMEOUT = BIT(12),  
	IA_CSS_RX_IRQ_INFO_ERR_UNKNOWN_ESC  = BIT(13),  
	IA_CSS_RX_IRQ_INFO_ERR_LINE_SYNC    = BIT(14),  
	IA_CSS_RX_IRQ_INFO_INIT_TIMEOUT     = BIT(15),
};
struct ia_css_irq {
	enum ia_css_irq_info type;  
	unsigned int sw_irq_0_val;  
	unsigned int sw_irq_1_val;  
	unsigned int sw_irq_2_val;  
	struct ia_css_pipe *pipe;
};
int
ia_css_irq_translate(unsigned int *info);
void
ia_css_rx_get_irq_info(unsigned int *irq_bits);
void
ia_css_rx_port_get_irq_info(enum mipi_port_id port, unsigned int *irq_bits);
void
ia_css_rx_clear_irq_info(unsigned int irq_bits);
void
ia_css_rx_port_clear_irq_info(enum mipi_port_id port, unsigned int irq_bits);
int
ia_css_irq_enable(enum ia_css_irq_info type, bool enable);
#endif  
