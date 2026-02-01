 

 

#ifndef _IA32_SYS_ASM_LINKAGE_H
#define	_IA32_SYS_ASM_LINKAGE_H

#if defined(_KERNEL) && defined(__linux__)
#include <linux/linkage.h>
#endif

#ifndef ENDBR
#if defined(__ELF__) && defined(__CET__) && defined(__has_include)
 
#if __has_include(<cet.h>)

#include <cet.h>

#ifdef _CET_ENDBR
#define	ENDBR	_CET_ENDBR
#endif  

#endif  
#endif  
#endif  

#ifndef ENDBR
#define	ENDBR
#endif
#ifndef RET
#define	RET	ret
#endif

 
#undef ASMABI
#define	ASMABI	__attribute__((sysv_abi))

#define	SECTION_TEXT .text
#define	SECTION_STATIC .section .rodata

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _ASM	 

 

 
#if !defined(__GNUC_AS__)
 
#define	D16	data16;
#define	A16	addr16;

 
#define	_CONST(const)		[const]
#define	_BITNOT(const)		-1!_CONST(const)
#define	_MUL(a, b)		_CONST(a \* b)

#else
 
#define	D16	.byte	0x66;
#define	A16	.byte	0x67;

#define	_CONST(const)		(const)
#define	_BITNOT(const)		~_CONST(const)
#define	_MUL(a, b)		_CONST(a * b)

#endif

 
#if defined(__amd64)
#define	CLONGSHIFT	3
#define	CLONGSIZE	8
#define	CLONGMASK	7
#elif defined(__i386)
#define	CLONGSHIFT	2
#define	CLONGSIZE	4
#define	CLONGMASK	3
#endif

 
#define	CPTRSHIFT	CLONGSHIFT
#define	CPTRSIZE	CLONGSIZE
#define	CPTRMASK	CLONGMASK

#if CPTRSIZE != (1 << CPTRSHIFT) || CLONGSIZE != (1 << CLONGSHIFT)
#error	"inconsistent shift constants"
#endif

#if CPTRMASK != (CPTRSIZE - 1) || CLONGMASK != (CLONGSIZE - 1)
#error	"inconsistent mask constants"
#endif

#define	ASM_ENTRY_ALIGN	16

 

#define	XMM_SIZE	16
#define	XMM_ALIGN	16

 
#undef ENTRY
#define	ENTRY(x) \
	.text; \
	.balign	ASM_ENTRY_ALIGN; \
	.globl	x; \
	.type	x, @function; \
x:	MCOUNT(x)

#define	ENTRY_NP(x) \
	.text; \
	.balign	ASM_ENTRY_ALIGN; \
	.globl	x; \
	.type	x, @function; \
x:

#define	ENTRY_ALIGN(x, a) \
	.text; \
	.balign	a; \
	.globl	x; \
	.type	x, @function; \
x:

#define	FUNCTION(x) \
	.type	x, @function; \
x:

 
#define	ENTRY2(x, y) \
	.text;	\
	.balign	ASM_ENTRY_ALIGN; \
	.globl	x, y; \
	.type	x, @function; \
	.type	y, @function; \
x:; \
y:	MCOUNT(x)

#define	ENTRY_NP2(x, y) \
	.text; \
	.balign	ASM_ENTRY_ALIGN; \
	.globl	x, y; \
	.type	x, @function; \
	.type	y, @function; \
x:; \
y:


 
#define	SET_SIZE(x) \
	.size	x, [.-x]

#define	SET_OBJ(x) .type	x, @object


#endif  

#ifdef	__cplusplus
}
#endif

#endif	 
