#ifndef __ASM_OPENRISC_PROCESSOR_H
#define __ASM_OPENRISC_PROCESSOR_H
#include <asm/spr_defs.h>
#include <asm/page.h>
#include <asm/ptrace.h>
#define STACK_TOP       TASK_SIZE
#define STACK_TOP_MAX	STACK_TOP
#define KERNEL_SR (SPR_SR_DME | SPR_SR_IME | SPR_SR_ICE \
		   | SPR_SR_DCE | SPR_SR_SM)
#define USER_SR   (SPR_SR_DME | SPR_SR_IME | SPR_SR_ICE \
		   | SPR_SR_DCE | SPR_SR_IEE | SPR_SR_TEE)
#define TASK_SIZE       (0x80000000UL)
#define TASK_UNMAPPED_BASE      (TASK_SIZE / 8 * 3)
#ifndef __ASSEMBLY__
struct task_struct;
struct thread_struct {
};
#define user_regs(thread_info)  (((struct pt_regs *)((unsigned long)(thread_info) + THREAD_SIZE - STACK_FRAME_OVERHEAD)) - 1)
#define task_pt_regs(task) user_regs(task_thread_info(task))
#define INIT_SP         (sizeof(init_stack) + (unsigned long) &init_stack)
#define INIT_THREAD  { }
#define KSTK_EIP(tsk)   (task_pt_regs(tsk)->pc)
#define KSTK_ESP(tsk)   (task_pt_regs(tsk)->sp)
void start_thread(struct pt_regs *regs, unsigned long nip, unsigned long sp);
unsigned long __get_wchan(struct task_struct *p);
void show_registers(struct pt_regs *regs);
#define cpu_relax()     barrier()
#endif  
#endif  
