#ifndef _ARM_KERNEL_PROBES_H
#define  _ARM_KERNEL_PROBES_H
#include <linux/types.h>
#include <linux/stddef.h>
#include <asm/probes.h>
#include <asm/ptrace.h>
#include <asm/kprobes.h>
void __init arm_probes_decode_init(void);
extern probes_check_cc * const probes_condition_checks[16];
#if __LINUX_ARM_ARCH__ >= 7
#define str_pc_offset 8
#define find_str_pc_offset()
#else  
extern int str_pc_offset;
void __init find_str_pc_offset(void);
#endif
static inline void __kprobes bx_write_pc(long pcv, struct pt_regs *regs)
{
	long cpsr = regs->ARM_cpsr;
	if (pcv & 0x1) {
		cpsr |= PSR_T_BIT;
		pcv &= ~0x1;
	} else {
		cpsr &= ~PSR_T_BIT;
		pcv &= ~0x2;	 
	}
	regs->ARM_cpsr = cpsr;
	regs->ARM_pc = pcv;
}
#if __LINUX_ARM_ARCH__ >= 6
#define load_write_pc_interworks true
#define test_load_write_pc_interworking()
#else  
extern bool load_write_pc_interworks;
void __init test_load_write_pc_interworking(void);
#endif
static inline void __kprobes load_write_pc(long pcv, struct pt_regs *regs)
{
	if (load_write_pc_interworks)
		bx_write_pc(pcv, regs);
	else
		regs->ARM_pc = pcv;
}
#if __LINUX_ARM_ARCH__ >= 7
#define alu_write_pc_interworks true
#define test_alu_write_pc_interworking()
#elif __LINUX_ARM_ARCH__ <= 5
#define alu_write_pc_interworks false
#define test_alu_write_pc_interworking()
#else  
extern bool alu_write_pc_interworks;
void __init test_alu_write_pc_interworking(void);
#endif  
static inline void __kprobes alu_write_pc(long pcv, struct pt_regs *regs)
{
	if (alu_write_pc_interworks)
		bx_write_pc(pcv, regs);
	else
		regs->ARM_pc = pcv;
}
#define is_writeback(insn) ((insn ^ 0x01000000) & 0x01200000)
enum decode_type {
	DECODE_TYPE_END,
	DECODE_TYPE_TABLE,
	DECODE_TYPE_CUSTOM,
	DECODE_TYPE_SIMULATE,
	DECODE_TYPE_EMULATE,
	DECODE_TYPE_OR,
	DECODE_TYPE_REJECT,
	NUM_DECODE_TYPES  
};
#define DECODE_TYPE_BITS	4
#define DECODE_TYPE_MASK	((1 << DECODE_TYPE_BITS) - 1)
enum decode_reg_type {
	REG_TYPE_NONE = 0,  
	REG_TYPE_ANY,	    
	REG_TYPE_SAMEAS16,  
	REG_TYPE_SP,	    
	REG_TYPE_PC,	    
	REG_TYPE_NOSP,	    
	REG_TYPE_NOSPPC,    
	REG_TYPE_NOPC,	    
	REG_TYPE_NOPCWB,    
	REG_TYPE_NOPCX,	    
	REG_TYPE_NOSPPCX,   
	REG_TYPE_0 = REG_TYPE_NONE
};
#define REGS(r16, r12, r8, r4, r0)	\
	(((REG_TYPE_##r16) << 16) +	\
	((REG_TYPE_##r12) << 12) +	\
	((REG_TYPE_##r8) << 8) +	\
	((REG_TYPE_##r4) << 4) +	\
	(REG_TYPE_##r0))
union decode_item {
	u32			bits;
	const union decode_item	*table;
	int			action;
};
struct decode_header;
typedef enum probes_insn (probes_custom_decode_t)(probes_opcode_t,
						  struct arch_probes_insn *,
						  const struct decode_header *);
union decode_action {
	probes_insn_handler_t	*handler;
	probes_custom_decode_t	*decoder;
};
typedef enum probes_insn (probes_check_t)(probes_opcode_t,
					   struct arch_probes_insn *,
					   const struct decode_header *);
struct decode_checker {
	probes_check_t	*checker;
};
#define DECODE_END			\
	{.bits = DECODE_TYPE_END}
struct decode_header {
	union decode_item	type_regs;
	union decode_item	mask;
	union decode_item	value;
};
#define DECODE_HEADER(_type, _mask, _value, _regs)		\
	{.bits = (_type) | ((_regs) << DECODE_TYPE_BITS)},	\
	{.bits = (_mask)},					\
	{.bits = (_value)}
struct decode_table {
	struct decode_header	header;
	union decode_item	table;
};
#define DECODE_TABLE(_mask, _value, _table)			\
	DECODE_HEADER(DECODE_TYPE_TABLE, _mask, _value, 0),	\
	{.table = (_table)}
struct decode_custom {
	struct decode_header	header;
	union decode_item	decoder;
};
#define DECODE_CUSTOM(_mask, _value, _decoder)			\
	DECODE_HEADER(DECODE_TYPE_CUSTOM, _mask, _value, 0),	\
	{.action = (_decoder)}
struct decode_simulate {
	struct decode_header	header;
	union decode_item	handler;
};
#define DECODE_SIMULATEX(_mask, _value, _handler, _regs)		\
	DECODE_HEADER(DECODE_TYPE_SIMULATE, _mask, _value, _regs),	\
	{.action = (_handler)}
#define DECODE_SIMULATE(_mask, _value, _handler)	\
	DECODE_SIMULATEX(_mask, _value, _handler, 0)
struct decode_emulate {
	struct decode_header	header;
	union decode_item	handler;
};
#define DECODE_EMULATEX(_mask, _value, _handler, _regs)			\
	DECODE_HEADER(DECODE_TYPE_EMULATE, _mask, _value, _regs),	\
	{.action = (_handler)}
#define DECODE_EMULATE(_mask, _value, _handler)		\
	DECODE_EMULATEX(_mask, _value, _handler, 0)
struct decode_or {
	struct decode_header	header;
};
#define DECODE_OR(_mask, _value)				\
	DECODE_HEADER(DECODE_TYPE_OR, _mask, _value, 0)
enum probes_insn {
	INSN_REJECTED,
	INSN_GOOD,
	INSN_GOOD_NO_SLOT
};
struct decode_reject {
	struct decode_header	header;
};
#define DECODE_REJECT(_mask, _value)				\
	DECODE_HEADER(DECODE_TYPE_REJECT, _mask, _value, 0)
probes_insn_handler_t probes_simulate_nop;
probes_insn_handler_t probes_emulate_none;
int __kprobes
probes_decode_insn(probes_opcode_t insn, struct arch_probes_insn *asi,
		const union decode_item *table, bool thumb, bool emulate,
		const union decode_action *actions,
		const struct decode_checker **checkers);
#endif
