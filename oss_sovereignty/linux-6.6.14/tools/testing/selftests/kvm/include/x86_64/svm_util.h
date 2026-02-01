 
 

#ifndef SELFTEST_KVM_SVM_UTILS_H
#define SELFTEST_KVM_SVM_UTILS_H

#include <asm/svm.h>

#include <stdint.h>
#include "svm.h"
#include "processor.h"

struct svm_test_data {
	 
	struct vmcb *vmcb;  
	void *vmcb_hva;
	uint64_t vmcb_gpa;

	 
	struct vmcb_save_area *save_area;  
	void *save_area_hva;
	uint64_t save_area_gpa;

	 
	void *msr;  
	void *msr_hva;
	uint64_t msr_gpa;
};

static inline void vmmcall(void)
{
	 
	__asm__ __volatile__("push %%rbp; vmmcall; pop %%rbp"
			     : : "a"(0xdeadbeef), "c"(0xbeefdead)
			     : "rbx", "rdx", "rsi", "rdi", "r8", "r9",
			       "r10", "r11", "r12", "r13", "r14", "r15");
}

#define stgi()			\
	__asm__ __volatile__(	\
		"stgi\n"	\
		)

#define clgi()			\
	__asm__ __volatile__(	\
		"clgi\n"	\
		)

struct svm_test_data *vcpu_alloc_svm(struct kvm_vm *vm, vm_vaddr_t *p_svm_gva);
void generic_svm_setup(struct svm_test_data *svm, void *guest_rip, void *guest_rsp);
void run_guest(struct vmcb *vmcb, uint64_t vmcb_gpa);

int open_sev_dev_path_or_exit(void);

#endif  
