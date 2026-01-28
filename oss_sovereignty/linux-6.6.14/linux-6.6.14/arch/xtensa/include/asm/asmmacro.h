#ifndef _XTENSA_ASMMACRO_H
#define _XTENSA_ASMMACRO_H
#include <asm-generic/export.h>
#include <asm/core.h>
	.macro	__loopi ar, at, size, incr
#if XCHAL_HAVE_LOOPS
		movi	\at, ((\size + \incr - 1) / (\incr))
		loop	\at, 99f
#else
		addi	\at, \ar, \size
		98:
#endif
	.endm
	.macro	__loops	ar, as, at, incr_log2, mask_log2, cond, ncond
#if XCHAL_HAVE_LOOPS
		.ifgt \incr_log2 - 1
			addi	\at, \as, (1 << \incr_log2) - 1
			.ifnc \mask_log2,
				extui	\at, \at, \incr_log2, \mask_log2
			.else
				srli	\at, \at, \incr_log2
			.endif
		.endif
		loop\cond	\at, 99f
#else
		.ifnc \mask_log2,
			extui	\at, \as, \incr_log2, \mask_log2
		.else
			.ifnc \ncond,
				srli	\at, \as, \incr_log2
			.endif
		.endif
		.ifnc \ncond,
			b\ncond	\at, 99f
		.endif
		.ifnc \mask_log2,
			slli	\at, \at, \incr_log2
			add	\at, \ar, \at
		.else
			add	\at, \ar, \as
		.endif
#endif
		98:
	.endm
	.macro	__loopt	ar, as, at, incr_log2
#if XCHAL_HAVE_LOOPS
		sub	\at, \as, \ar
		.ifgt	\incr_log2 - 1
			addi	\at, \at, (1 << \incr_log2) - 1
			srli	\at, \at, \incr_log2
		.endif
		loop	\at, 99f
#else
		98:
#endif
	.endm
	.macro	__loop	as
#if XCHAL_HAVE_LOOPS
		loop	\as, 99f
#else
		98:
#endif
	.endm
	.macro	__endl	ar, as
#if !XCHAL_HAVE_LOOPS
		bltu	\ar, \as, 98b
#endif
		99:
	.endm
	.macro	__endla	ar, as, incr
		addi	\ar, \ar, \incr
		__endl	\ar \as
	.endm
#define EX(handler)				\
	.section __ex_table, "a";		\
	.word	97f, handler;			\
	.previous				\
97:
	.macro __src_b	r, w0, w1
#ifdef __XTENSA_EB__
		src	\r, \w0, \w1
#else
		src	\r, \w1, \w0
#endif
	.endm
	.macro __ssa8	r
#ifdef __XTENSA_EB__
		ssa8b	\r
#else
		ssa8l	\r
#endif
	.endm
	.macro	do_nsau cnt, val, tmp, a
#if XCHAL_HAVE_NSA
	nsau	\cnt, \val
#else
	mov	\a, \val
	movi	\cnt, 0
	extui	\tmp, \a, 16, 16
	bnez	\tmp, 0f
	movi	\cnt, 16
	slli	\a, \a, 16
0:
	extui	\tmp, \a, 24, 8
	bnez	\tmp, 1f
	addi	\cnt, \cnt, 8
	slli	\a, \a, 8
1:
	movi	\tmp, __nsau_data
	extui	\a, \a, 24, 8
	add	\tmp, \tmp, \a
	l8ui	\tmp, \tmp, 0
	add	\cnt, \cnt, \tmp
#endif  
	.endm
	.macro	do_abs dst, src, tmp
#if XCHAL_HAVE_ABS
	abs	\dst, \src
#else
	neg	\tmp, \src
	movgez	\tmp, \src, \src
	mov	\dst, \tmp
#endif
	.endm
#if defined(__XTENSA_WINDOWED_ABI__)
#define KABI_W
#define KABI_C0 #
#define XTENSA_FRAME_SIZE_RESERVE	16
#define XTENSA_SPILL_STACK_RESERVE	32
#define abi_entry(frame_size) \
	entry sp, (XTENSA_FRAME_SIZE_RESERVE + \
		   (((frame_size) + XTENSA_STACK_ALIGNMENT - 1) & \
		    -XTENSA_STACK_ALIGNMENT))
#define abi_entry_default abi_entry(0)
#define abi_ret(frame_size) retw
#define abi_ret_default retw
#define abi_call call4
#define abi_callx callx4
#define abi_arg0 a6
#define abi_arg1 a7
#define abi_arg2 a8
#define abi_arg3 a9
#define abi_arg4 a10
#define abi_arg5 a11
#define abi_rv a6
#define abi_saved0 a2
#define abi_saved1 a3
#define abi_tmp0 a4
#define abi_tmp1 a5
#elif defined(__XTENSA_CALL0_ABI__)
#define KABI_W #
#define KABI_C0
#define XTENSA_SPILL_STACK_RESERVE	0
#define abi_entry(frame_size) __abi_entry (frame_size)
	.macro	__abi_entry frame_size
	.ifgt \frame_size
	addi sp, sp, -(((\frame_size) + XTENSA_STACK_ALIGNMENT - 1) & \
		       -XTENSA_STACK_ALIGNMENT)
	.endif
	.endm
#define abi_entry_default
#define abi_ret(frame_size) __abi_ret (frame_size)
	.macro	__abi_ret frame_size
	.ifgt \frame_size
	addi sp, sp, (((\frame_size) + XTENSA_STACK_ALIGNMENT - 1) & \
		      -XTENSA_STACK_ALIGNMENT)
	.endif
	ret
	.endm
#define abi_ret_default ret
#define abi_call call0
#define abi_callx callx0
#define abi_arg0 a2
#define abi_arg1 a3
#define abi_arg2 a4
#define abi_arg3 a5
#define abi_arg4 a6
#define abi_arg5 a7
#define abi_rv a2
#define abi_saved0 a12
#define abi_saved1 a13
#define abi_tmp0 a8
#define abi_tmp1 a9
#else
#error Unsupported Xtensa ABI
#endif
#if defined(USER_SUPPORT_WINDOWED)
#define UABI_W
#define UABI_C0 #
#else
#define UABI_W #
#define UABI_C0
#endif
#define __XTENSA_HANDLER	.section ".exception.text", "ax"
#endif  
