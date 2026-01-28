#ifndef __BPF_MISC_H__
#define __BPF_MISC_H__
#define __msg(msg)		__attribute__((btf_decl_tag("comment:test_expect_msg=" msg)))
#define __failure		__attribute__((btf_decl_tag("comment:test_expect_failure")))
#define __success		__attribute__((btf_decl_tag("comment:test_expect_success")))
#define __description(desc)	__attribute__((btf_decl_tag("comment:test_description=" desc)))
#define __msg_unpriv(msg)	__attribute__((btf_decl_tag("comment:test_expect_msg_unpriv=" msg)))
#define __failure_unpriv	__attribute__((btf_decl_tag("comment:test_expect_failure_unpriv")))
#define __success_unpriv	__attribute__((btf_decl_tag("comment:test_expect_success_unpriv")))
#define __log_level(lvl)	__attribute__((btf_decl_tag("comment:test_log_level="#lvl)))
#define __flag(flag)		__attribute__((btf_decl_tag("comment:test_prog_flags="#flag)))
#define __retval(val)		__attribute__((btf_decl_tag("comment:test_retval="#val)))
#define __retval_unpriv(val)	__attribute__((btf_decl_tag("comment:test_retval_unpriv="#val)))
#define __auxiliary		__attribute__((btf_decl_tag("comment:test_auxiliary")))
#define __auxiliary_unpriv	__attribute__((btf_decl_tag("comment:test_auxiliary_unpriv")))
#define __naked __attribute__((naked))
#define __clobber_all "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "memory"
#define __clobber_common "r0", "r1", "r2", "r3", "r4", "r5", "memory"
#define __imm(name) [name]"i"(name)
#define __imm_const(name, expr) [name]"i"(expr)
#define __imm_addr(name) [name]"i"(&name)
#define __imm_ptr(name) [name]"p"(&name)
#define __imm_insn(name, expr) [name]"i"(*(long *)&(expr))
#define POINTER_VALUE	0xcafe4all
#define TEST_DATA_LEN	64
#ifndef __used
#define __used __attribute__((used))
#endif
#if defined(__TARGET_ARCH_x86)
#define SYSCALL_WRAPPER 1
#define SYS_PREFIX "__x64_"
#elif defined(__TARGET_ARCH_s390)
#define SYSCALL_WRAPPER 1
#define SYS_PREFIX "__s390x_"
#elif defined(__TARGET_ARCH_arm64)
#define SYSCALL_WRAPPER 1
#define SYS_PREFIX "__arm64_"
#elif defined(__TARGET_ARCH_riscv)
#define SYSCALL_WRAPPER 1
#define SYS_PREFIX "__riscv_"
#else
#define SYSCALL_WRAPPER 0
#define SYS_PREFIX "__se_"
#endif
#if defined(__TARGET_ARCH_x86) || defined(__x86_64__)
#define FUNC_REG_ARG_CNT 6
#elif defined(__i386__)
#define FUNC_REG_ARG_CNT 3
#elif defined(__TARGET_ARCH_s390) || defined(__s390x__)
#define FUNC_REG_ARG_CNT 5
#elif defined(__TARGET_ARCH_arm) || defined(__arm__)
#define FUNC_REG_ARG_CNT 4
#elif defined(__TARGET_ARCH_arm64) || defined(__aarch64__)
#define FUNC_REG_ARG_CNT 8
#elif defined(__TARGET_ARCH_mips) || defined(__mips__)
#define FUNC_REG_ARG_CNT 8
#elif defined(__TARGET_ARCH_powerpc) || defined(__powerpc__) || defined(__powerpc64__)
#define FUNC_REG_ARG_CNT 8
#elif defined(__TARGET_ARCH_sparc) || defined(__sparc__)
#define FUNC_REG_ARG_CNT 6
#elif defined(__TARGET_ARCH_riscv) || defined(__riscv__)
#define FUNC_REG_ARG_CNT 8
#else
#define FUNC_REG_ARG_CNT 5
#endif
#define __sink(expr) asm volatile("" : "+g"(expr))
#endif
