


#ifndef __PF_H_
#define __PF_H_

enum reason_type {
	NOT_ME,	
	NOTHING,	
	REG_READ,	
	REG_WRITE,	
	IMM_WRITE,	
	OTHERS	
};

enum reason_type get_ins_type(unsigned long ins_addr);
unsigned int get_ins_mem_width(unsigned long ins_addr);
unsigned long get_ins_reg_val(unsigned long ins_addr, struct pt_regs *regs);
unsigned long get_ins_imm_val(unsigned long ins_addr);

#endif 
