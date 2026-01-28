
#include "py/obj.h"

typedef struct _machine_hard_spi_obj_t machine_hard_spi_obj_t;

void spi_init0(void);
void spi_transfer(const machine_hard_spi_obj_t *self,
    size_t len,
    const void *src,
    void *dest);
