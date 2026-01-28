#ifndef _ASM_POWERPC_CODE_PATCHING_H
#define _ASM_POWERPC_CODE_PATCHING_H
#include <asm/types.h>
#include <asm/ppc-opcode.h>
#include <linux/string.h>
#include <linux/kallsyms.h>
#include <asm/asm-compat.h>
#include <asm/inst.h>
#define BRANCH_SET_LINK	0x1
#define BRANCH_ABSOLUTE	0x2
static inline bool is_offset_in_branch_range(long offset)
{
	return (offset >= -0x2000000 && offset <= 0x1fffffc && !(offset & 0x3));
}
static inline bool is_offset_in_cond_branch_range(long offset)
{
	return offset >= -0x8000 && offset <= 0x7fff && !(offset & 0x3);
}
static inline int create_branch(ppc_inst_t *instr, const u32 *addr,
				unsigned long target, int flags)
{
	long offset;
	*instr = ppc_inst(0);
	offset = target;
	if (! (flags & BRANCH_ABSOLUTE))
		offset = offset - (unsigned long)addr;
	if (!is_offset_in_branch_range(offset))
		return 1;
	*instr = ppc_inst(0x48000000 | (flags & 0x3) | (offset & 0x03FFFFFC));
	return 0;
}
int create_cond_branch(ppc_inst_t *instr, const u32 *addr,
		       unsigned long target, int flags);
int patch_branch(u32 *addr, unsigned long target, int flags);
int patch_instruction(u32 *addr, ppc_inst_t instr);
int raw_patch_instruction(u32 *addr, ppc_inst_t instr);
static inline unsigned long patch_site_addr(s32 *site)
{
	return (unsigned long)site + *site;
}
static inline int patch_instruction_site(s32 *site, ppc_inst_t instr)
{
	return patch_instruction((u32 *)patch_site_addr(site), instr);
}
static inline int patch_branch_site(s32 *site, unsigned long target, int flags)
{
	return patch_branch((u32 *)patch_site_addr(site), target, flags);
}
static inline int modify_instruction(unsigned int *addr, unsigned int clr,
				     unsigned int set)
{
	return patch_instruction(addr, ppc_inst((*addr & ~clr) | set));
}
static inline int modify_instruction_site(s32 *site, unsigned int clr, unsigned int set)
{
	return modify_instruction((unsigned int *)patch_site_addr(site), clr, set);
}
static inline unsigned int branch_opcode(ppc_inst_t instr)
{
	return ppc_inst_primary_opcode(instr) & 0x3F;
}
static inline int instr_is_branch_iform(ppc_inst_t instr)
{
	return branch_opcode(instr) == 18;
}
static inline int instr_is_branch_bform(ppc_inst_t instr)
{
	return branch_opcode(instr) == 16;
}
int instr_is_relative_branch(ppc_inst_t instr);
int instr_is_relative_link_branch(ppc_inst_t instr);
unsigned long branch_target(const u32 *instr);
int translate_branch(ppc_inst_t *instr, const u32 *dest, const u32 *src);
bool is_conditional_branch(ppc_inst_t instr);
#define OP_RT_RA_MASK	0xffff0000UL
#define LIS_R2		(PPC_RAW_LIS(_R2, 0))
#define ADDIS_R2_R12	(PPC_RAW_ADDIS(_R2, _R12, 0))
#define ADDI_R2_R2	(PPC_RAW_ADDI(_R2, _R2, 0))
static inline unsigned long ppc_function_entry(void *func)
{
#ifdef CONFIG_PPC64_ELF_ABI_V2
	u32 *insn = func;
	if ((((*insn & OP_RT_RA_MASK) == ADDIS_R2_R12) ||
	     ((*insn & OP_RT_RA_MASK) == LIS_R2)) &&
	    ((*(insn+1) & OP_RT_RA_MASK) == ADDI_R2_R2))
		return (unsigned long)(insn + 2);
	else
		return (unsigned long)func;
#elif defined(CONFIG_PPC64_ELF_ABI_V1)
	return ((struct func_desc *)func)->addr;
#else
	return (unsigned long)func;
#endif
}
static inline unsigned long ppc_global_function_entry(void *func)
{
#ifdef CONFIG_PPC64_ELF_ABI_V2
	return (unsigned long)func;
#else
	return ppc_function_entry(func);
#endif
}
static inline unsigned long ppc_kallsyms_lookup_name(const char *name)
{
	unsigned long addr;
#ifdef CONFIG_PPC64_ELF_ABI_V1
	char dot_name[1 + KSYM_NAME_LEN];
	bool dot_appended = false;
	if (strnlen(name, KSYM_NAME_LEN) >= KSYM_NAME_LEN)
		return 0;
	if (name[0] != '.') {
		dot_name[0] = '.';
		dot_name[1] = '\0';
		strlcat(dot_name, name, sizeof(dot_name));
		dot_appended = true;
	} else {
		dot_name[0] = '\0';
		strlcat(dot_name, name, sizeof(dot_name));
	}
	addr = kallsyms_lookup_name(dot_name);
	if (!addr && dot_appended)
		addr = kallsyms_lookup_name(name);
#elif defined(CONFIG_PPC64_ELF_ABI_V2)
	addr = kallsyms_lookup_name(name);
	if (addr)
		addr = ppc_function_entry((void *)addr);
#else
	addr = kallsyms_lookup_name(name);
#endif
	return addr;
}
#ifdef CONFIG_PPC64_ELF_ABI_V2
#define R2_STACK_OFFSET         24
#else
#define R2_STACK_OFFSET         40
#endif
#define PPC_INST_LD_TOC		PPC_RAW_LD(_R2, _R1, R2_STACK_OFFSET)
#define PPC_INST_STD_LR		PPC_RAW_STD(_R0, _R1, PPC_LR_STKOFF)
#endif  
