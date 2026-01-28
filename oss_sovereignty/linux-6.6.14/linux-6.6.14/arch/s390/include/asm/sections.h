#ifndef _S390_SECTIONS_H
#define _S390_SECTIONS_H
#include <asm-generic/sections.h>
#define __bootdata(var) __section(".boot.data." #var) var
#define __bootdata_preserved(var) __section(".boot.preserved.data." #var) var
extern char *__samode31, *__eamode31;
extern char *__stext_amode31, *__etext_amode31;
#endif
