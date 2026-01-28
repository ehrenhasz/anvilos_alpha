
#ifndef MICROPY_INCLUDED_RENESAS_RA_FACTORYRESET_H
#define MICROPY_INCLUDED_RENESAS_RA_FACTORYRESET_H

#include "lib/oofatfs/ff.h"

void factory_reset_make_files(FATFS *fatfs);
int factory_reset_create_filesystem(void);

#endif 
