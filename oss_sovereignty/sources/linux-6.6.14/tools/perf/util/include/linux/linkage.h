

#ifndef PERF_LINUX_LINKAGE_H_
#define PERF_LINUX_LINKAGE_H_




#ifndef ASM_NL
#define ASM_NL		 ;
#endif

#ifndef __ALIGN
#define __ALIGN		.align 4,0x90
#define __ALIGN_STR	".align 4,0x90"
#endif


#ifndef SYM_T_FUNC
#define SYM_T_FUNC				STT_FUNC
#endif


#define SYM_A_ALIGN				ALIGN


#define SYM_L_GLOBAL(name)			.globl name
#define SYM_L_WEAK(name)			.weak name
#define SYM_L_LOCAL(name)			

#define ALIGN __ALIGN




#ifndef SYM_ENTRY
#define SYM_ENTRY(name, linkage, align...)		\
	linkage(name) ASM_NL				\
	align ASM_NL					\
	name:
#endif


#ifndef SYM_START
#define SYM_START(name, linkage, align...)		\
	SYM_ENTRY(name, linkage, align)
#endif


#ifndef SYM_END
#define SYM_END(name, sym_type)				\
	.type name sym_type ASM_NL			\
	.set .L__sym_size_##name, .-name ASM_NL		\
	.size name, .-name
#endif


#ifndef SYM_ALIAS
#define SYM_ALIAS(alias, name, sym_type, linkage)			\
	linkage(alias) ASM_NL						\
	.set alias, name ASM_NL						\
	.type alias sym_type ASM_NL					\
	.set .L__sym_size_##alias, .L__sym_size_##name ASM_NL		\
	.size alias, .L__sym_size_##alias
#endif


#ifndef SYM_FUNC_START
#define SYM_FUNC_START(name)				\
	SYM_START(name, SYM_L_GLOBAL, SYM_A_ALIGN)
#endif


#ifndef SYM_FUNC_START_LOCAL
#define SYM_FUNC_START_LOCAL(name)			\
	SYM_START(name, SYM_L_LOCAL, SYM_A_ALIGN)
#endif


#ifndef SYM_FUNC_START_WEAK
#define SYM_FUNC_START_WEAK(name)			\
	SYM_START(name, SYM_L_WEAK, SYM_A_ALIGN)
#endif


#ifndef SYM_FUNC_END
#define SYM_FUNC_END(name)				\
	SYM_END(name, SYM_T_FUNC)
#endif


#ifndef SYM_FUNC_ALIAS
#define SYM_FUNC_ALIAS(alias, name)					\
	SYM_ALIAS(alias, name, SYM_T_FUNC, SYM_L_GLOBAL)
#endif


#ifndef SYM_FUNC_ALIAS_LOCAL
#define SYM_FUNC_ALIAS_LOCAL(alias, name)				\
	SYM_ALIAS(alias, name, SYM_T_FUNC, SYM_L_LOCAL)
#endif


#ifndef SYM_FUNC_ALIAS_WEAK
#define SYM_FUNC_ALIAS_WEAK(alias, name)				\
	SYM_ALIAS(alias, name, SYM_T_FUNC, SYM_L_WEAK)
#endif




#ifndef SYM_TYPED_START
#define SYM_TYPED_START(name, linkage, align...)        \
        SYM_START(name, linkage, align)
#endif

#ifndef SYM_TYPED_FUNC_START
#define SYM_TYPED_FUNC_START(name)                      \
        SYM_TYPED_START(name, SYM_L_GLOBAL, SYM_A_ALIGN)
#endif

#endif	
