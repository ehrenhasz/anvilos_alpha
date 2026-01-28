#ifndef _ASM_MMU_H
#define _ASM_MMU_H
#include <asm/vdso.h>
struct mm_context {
	unsigned long long generation;
	unsigned long ptbase;
	struct hexagon_vdso *vdso;
};
typedef struct mm_context mm_context_t;
#endif
