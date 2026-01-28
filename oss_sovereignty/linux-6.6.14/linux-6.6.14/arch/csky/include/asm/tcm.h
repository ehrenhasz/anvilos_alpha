#ifndef __ASM_CSKY_TCM_H
#define __ASM_CSKY_TCM_H
#ifndef CONFIG_HAVE_TCM
#error "You should not be including tcm.h unless you have a TCM!"
#endif
#include <linux/compiler.h>
#define __tcmdata __section(".tcm.data")
#define __tcmconst __section(".tcm.rodata")
#define __tcmfunc __section(".tcm.text") noinline
#define __tcmlocalfunc __section(".tcm.text")
void *tcm_alloc(size_t len);
void tcm_free(void *addr, size_t len);
#endif
