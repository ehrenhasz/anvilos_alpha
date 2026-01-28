#ifndef __ASM_ARC_IRQFLAGS_H
#define __ASM_ARC_IRQFLAGS_H
#ifdef CONFIG_ISA_ARCOMPACT
#include <asm/irqflags-compact.h>
#else
#include <asm/irqflags-arcv2.h>
#endif
#endif
