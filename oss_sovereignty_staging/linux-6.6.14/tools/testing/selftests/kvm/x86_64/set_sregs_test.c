
 
#define _GNU_SOURCE  
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include "test_util.h"

#include "kvm_util.h"
#include "processor.h"

#define TEST_INVALID_CR_BIT(vcpu, cr, orig, bit)				\
do {										\
	struct kvm_sregs new;							\
	int rc;									\
										\
	 			\
	if (orig.cr & bit)							\
		break;								\
										\
	memcpy(&new, &orig, sizeof(sregs));					\
	new.cr |= bit;								\
										\
	rc = _vcpu_sregs_set(vcpu, &new);					\
	TEST_ASSERT(rc, "KVM allowed invalid " #cr " bit (0x%lx)", bit);	\
										\
	 			\
	vcpu_sregs_get(vcpu, &new);						\
	TEST_ASSERT(!memcmp(&new, &orig, sizeof(new)), "KVM modified sregs");	\
} while (0)

static uint64_t calc_supported_cr4_feature_bits(void)
{
	uint64_t cr4;

	cr4 = X86_CR4_VME | X86_CR4_PVI | X86_CR4_TSD | X86_CR4_DE |
	      X86_CR4_PSE | X86_CR4_PAE | X86_CR4_MCE | X86_CR4_PGE |
	      X86_CR4_PCE | X86_CR4_OSFXSR | X86_CR4_OSXMMEXCPT;
	if (kvm_cpu_has(X86_FEATURE_UMIP))
		cr4 |= X86_CR4_UMIP;
	if (kvm_cpu_has(X86_FEATURE_LA57))
		cr4 |= X86_CR4_LA57;
	if (kvm_cpu_has(X86_FEATURE_VMX))
		cr4 |= X86_CR4_VMXE;
	if (kvm_cpu_has(X86_FEATURE_SMX))
		cr4 |= X86_CR4_SMXE;
	if (kvm_cpu_has(X86_FEATURE_FSGSBASE))
		cr4 |= X86_CR4_FSGSBASE;
	if (kvm_cpu_has(X86_FEATURE_PCID))
		cr4 |= X86_CR4_PCIDE;
	if (kvm_cpu_has(X86_FEATURE_XSAVE))
		cr4 |= X86_CR4_OSXSAVE;
	if (kvm_cpu_has(X86_FEATURE_SMEP))
		cr4 |= X86_CR4_SMEP;
	if (kvm_cpu_has(X86_FEATURE_SMAP))
		cr4 |= X86_CR4_SMAP;
	if (kvm_cpu_has(X86_FEATURE_PKU))
		cr4 |= X86_CR4_PKE;

	return cr4;
}

int main(int argc, char *argv[])
{
	struct kvm_sregs sregs;
	struct kvm_vcpu *vcpu;
	struct kvm_vm *vm;
	uint64_t cr4;
	int rc, i;

	 
	vm = vm_create_barebones();
	vcpu = __vm_vcpu_add(vm, 0);

	vcpu_sregs_get(vcpu, &sregs);

	sregs.cr0 = 0;
	sregs.cr4 |= calc_supported_cr4_feature_bits();
	cr4 = sregs.cr4;

	rc = _vcpu_sregs_set(vcpu, &sregs);
	TEST_ASSERT(!rc, "Failed to set supported CR4 bits (0x%lx)", cr4);

	vcpu_sregs_get(vcpu, &sregs);
	TEST_ASSERT(sregs.cr4 == cr4, "sregs.CR4 (0x%llx) != CR4 (0x%lx)",
		    sregs.cr4, cr4);

	 
	TEST_INVALID_CR_BIT(vcpu, cr4, sregs, X86_CR4_UMIP);
	TEST_INVALID_CR_BIT(vcpu, cr4, sregs, X86_CR4_LA57);
	TEST_INVALID_CR_BIT(vcpu, cr4, sregs, X86_CR4_VMXE);
	TEST_INVALID_CR_BIT(vcpu, cr4, sregs, X86_CR4_SMXE);
	TEST_INVALID_CR_BIT(vcpu, cr4, sregs, X86_CR4_FSGSBASE);
	TEST_INVALID_CR_BIT(vcpu, cr4, sregs, X86_CR4_PCIDE);
	TEST_INVALID_CR_BIT(vcpu, cr4, sregs, X86_CR4_OSXSAVE);
	TEST_INVALID_CR_BIT(vcpu, cr4, sregs, X86_CR4_SMEP);
	TEST_INVALID_CR_BIT(vcpu, cr4, sregs, X86_CR4_SMAP);
	TEST_INVALID_CR_BIT(vcpu, cr4, sregs, X86_CR4_PKE);

	for (i = 32; i < 64; i++)
		TEST_INVALID_CR_BIT(vcpu, cr0, sregs, BIT(i));

	 
	TEST_INVALID_CR_BIT(vcpu, cr0, sregs, X86_CR0_NW);
	TEST_INVALID_CR_BIT(vcpu, cr0, sregs, X86_CR0_PG);

	kvm_vm_free(vm);

	 
	vm = vm_create_with_one_vcpu(&vcpu, NULL);

	vcpu_sregs_get(vcpu, &sregs);
	sregs.apic_base = 1 << 10;
	rc = _vcpu_sregs_set(vcpu, &sregs);
	TEST_ASSERT(rc, "Set IA32_APIC_BASE to %llx (invalid)",
		    sregs.apic_base);
	sregs.apic_base = 1 << 11;
	rc = _vcpu_sregs_set(vcpu, &sregs);
	TEST_ASSERT(!rc, "Couldn't set IA32_APIC_BASE to %llx (valid)",
		    sregs.apic_base);

	kvm_vm_free(vm);

	return 0;
}
