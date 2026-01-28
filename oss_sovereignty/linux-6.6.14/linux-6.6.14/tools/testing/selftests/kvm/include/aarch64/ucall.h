#ifndef SELFTEST_KVM_UCALL_H
#define SELFTEST_KVM_UCALL_H
#include "kvm_util_base.h"
#define UCALL_EXIT_REASON       KVM_EXIT_MMIO
extern vm_vaddr_t *ucall_exit_mmio_addr;
static inline void ucall_arch_do_ucall(vm_vaddr_t uc)
{
	WRITE_ONCE(*ucall_exit_mmio_addr, uc);
}
#endif
