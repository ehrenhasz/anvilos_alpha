
#ifndef MICROPY_INCLUDED_STM32_QSPI_H
#define MICROPY_INCLUDED_STM32_QSPI_H

#include "drivers/bus/qspi.h"

extern const mp_qspi_proto_t qspi_proto;

void qspi_init(void);
void qspi_memory_map(void);

#endif 
