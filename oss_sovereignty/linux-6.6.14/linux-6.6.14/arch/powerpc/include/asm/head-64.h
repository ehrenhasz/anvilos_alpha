#ifndef _ASM_POWERPC_HEAD_64_H
#define _ASM_POWERPC_HEAD_64_H
#include <asm/cache.h>
#ifdef __ASSEMBLY__
.macro define_ftsec name
	.section ".head.text.\name\()","ax",@progbits
.endm
.macro define_data_ftsec name
	.section ".head.data.\name\()","a",@progbits
.endm
.macro use_ftsec name
	.section ".head.text.\name\()","ax",@progbits
.endm
#define OPEN_FIXED_SECTION(sname, start, end)			\
	sname##_start = (start);				\
	sname##_end = (end);					\
	sname##_len = (end) - (start);				\
	define_ftsec sname;					\
	. = 0x0;						\
start_##sname:
#ifdef CONFIG_LD_HEAD_STUB_CATCH
#define OPEN_TEXT_SECTION(start)				\
	.section ".linker_stub_catch","ax",@progbits;		\
linker_stub_catch:						\
	. = 0x4;						\
	text_start = (start) + 0x100;				\
	.section ".text","ax",@progbits;			\
	.balign 0x100;						\
start_text:
#else
#define OPEN_TEXT_SECTION(start)				\
	text_start = (start);					\
	.section ".text","ax",@progbits;			\
	. = 0x0;						\
start_text:
#endif
#define ZERO_FIXED_SECTION(sname, start, end)			\
	sname##_start = (start);				\
	sname##_end = (end);					\
	sname##_len = (end) - (start);				\
	define_data_ftsec sname;				\
	. = 0x0;						\
	. = sname##_len;
#define USE_FIXED_SECTION(sname)				\
	use_ftsec sname;
#define USE_TEXT_SECTION()					\
	.text
#define CLOSE_FIXED_SECTION(sname)				\
	USE_FIXED_SECTION(sname);				\
	. = sname##_len;					\
end_##sname:
#define __FIXED_SECTION_ENTRY_BEGIN(sname, name, __align)	\
	USE_FIXED_SECTION(sname);				\
	.balign __align;					\
	.global name;						\
name:
#define FIXED_SECTION_ENTRY_BEGIN(sname, name)			\
	__FIXED_SECTION_ENTRY_BEGIN(sname, name, IFETCH_ALIGN_BYTES)
#define FIXED_SECTION_ENTRY_BEGIN_LOCATION(sname, name, start, size) \
	USE_FIXED_SECTION(sname);				\
	name##_start = (start);					\
	.if ((start) % (size) != 0);				\
	.error "Fixed section exception vector misalignment";	\
	.endif;							\
	.if ((size) != 0x20) && ((size) != 0x80) && ((size) != 0x100) && ((size) != 0x1000); \
	.error "Fixed section exception vector bad size";	\
	.endif;							\
	.if (start) < sname##_start;				\
	.error "Fixed section underflow";			\
	.abort;							\
	.endif;							\
	. = (start) - sname##_start;				\
	.global name;						\
name:
#define FIXED_SECTION_ENTRY_END_LOCATION(sname, name, start, size) \
	.if (start) + (size) > sname##_end;			\
	.error "Fixed section overflow";			\
	.abort;							\
	.endif;							\
	.if (. - name > (start) + (size) - name##_start);	\
	.error "Fixed entry overflow";				\
	.abort;							\
	.endif;							\
	. = ((start) + (size) - sname##_start);			\
#define DEFINE_FIXED_SYMBOL(label, sname) \
	label##_absolute = (label - start_ ## sname + sname ## _start)
#define FIXED_SYMBOL_ABS_ADDR(label)				\
	(label##_absolute)
#define ABS_ADDR(label, sname) (label - start_ ## sname + sname ## _start)
#endif  
#endif	 
