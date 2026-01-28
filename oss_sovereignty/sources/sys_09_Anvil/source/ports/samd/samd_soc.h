
#ifndef MICROPY_INCLUDED_SAMD_SAMD_SOC_H
#define MICROPY_INCLUDED_SAMD_SAMD_SOC_H

#include <stdint.h>
#include "sam.h"
#include "clock_config.h"

typedef struct _samd_unique_id_t {
    uint8_t bytes[16];
} samd_unique_id_t;

extern Sercom *sercom_instance[];

void samd_init(void);
void samd_main(void);

void USB_Handler_wrapper(void);

void sercom_enable(Sercom *spi, int state);
void sercom_register_irq(int sercom_id, void (*sercom_irq_handler));



void samd_get_unique_id(samd_unique_id_t *id);

#define SERCOM_IRQ_TYPE_UART  (0)
#define SERCOM_IRQ_TYPE_SPI   (1)
#define SERCOM_IRQ_TYPE_I2C   (2)

#endif 
