#ifndef _SPEAKUP_SERIAL_H
#define _SPEAKUP_SERIAL_H
#include <linux/serial.h>	 
#include <linux/serial_reg.h>	 
#include <linux/serial_core.h>
#include "spk_priv.h"
struct old_serial_port {
	unsigned int uart;  
	unsigned int baud_base;
	unsigned int port;
	unsigned int irq;
	upf_t flags;  
};
#define SPK_SERIAL_TIMEOUT SPK_SYNTH_TIMEOUT
#define SPK_XMITR_TIMEOUT 100000
#define SPK_CTS_TIMEOUT 100000
#define SPK_LO_TTY 0
#define SPK_HI_TTY 3
#define NUM_DISABLE_TIMEOUTS 3
#define SPK_TIMEOUT 100
#define spk_serial_tx_busy() \
	(!uart_lsr_tx_empty(inb(speakup_info.port_tts + UART_LSR)))
#endif
