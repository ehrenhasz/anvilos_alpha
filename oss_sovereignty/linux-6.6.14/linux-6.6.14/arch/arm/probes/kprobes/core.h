#ifndef _ARM_KERNEL_KPROBES_H
#define _ARM_KERNEL_KPROBES_H
#include <asm/kprobes.h>
#include "../decode.h"
#define KPROBE_ARM_BREAKPOINT_INSTRUCTION	0x07f001f8
#define KPROBE_THUMB16_BREAKPOINT_INSTRUCTION	0xde18
#define KPROBE_THUMB32_BREAKPOINT_INSTRUCTION	0xf7f0a018
extern void kprobes_remove_breakpoint(void *addr, unsigned int insn);
enum probes_insn __kprobes
kprobe_decode_ldmstm(kprobe_opcode_t insn, struct arch_probes_insn *asi,
		const struct decode_header *h);
typedef enum probes_insn (kprobe_decode_insn_t)(probes_opcode_t,
						struct arch_probes_insn *,
						bool,
						const union decode_action *,
						const struct decode_checker *[]);
#ifdef CONFIG_THUMB2_KERNEL
extern const union decode_action kprobes_t32_actions[];
extern const union decode_action kprobes_t16_actions[];
extern const struct decode_checker *kprobes_t32_checkers[];
extern const struct decode_checker *kprobes_t16_checkers[];
#else  
extern const union decode_action kprobes_arm_actions[];
extern const struct decode_checker *kprobes_arm_checkers[];
#endif
#endif  
