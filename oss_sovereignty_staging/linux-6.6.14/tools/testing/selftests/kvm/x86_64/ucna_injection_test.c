
 

#define _GNU_SOURCE  
#include <pthread.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>

#include "kvm_util_base.h"
#include "kvm_util.h"
#include "mce.h"
#include "processor.h"
#include "test_util.h"
#include "apic.h"

#define SYNC_FIRST_UCNA 9
#define SYNC_SECOND_UCNA 10
#define SYNC_GP 11
#define FIRST_UCNA_ADDR 0xdeadbeef
#define SECOND_UCNA_ADDR 0xcafeb0ba

 
#define CMCI_VECTOR  0xa9

#define UCNA_BANK  0x7	

#define MCI_CTL2_RESERVED_BIT BIT_ULL(29)

static uint64_t supported_mcg_caps;

 
static volatile uint64_t i_ucna_rcvd;
static volatile uint64_t i_ucna_addr;
static volatile uint64_t ucna_addr;
static volatile uint64_t ucna_addr2;

struct thread_params {
	struct kvm_vcpu *vcpu;
	uint64_t *p_i_ucna_rcvd;
	uint64_t *p_i_ucna_addr;
	uint64_t *p_ucna_addr;
	uint64_t *p_ucna_addr2;
};

static void verify_apic_base_addr(void)
{
	uint64_t msr = rdmsr(MSR_IA32_APICBASE);
	uint64_t base = GET_APIC_BASE(msr);

	GUEST_ASSERT(base == APIC_DEFAULT_GPA);
}

static void ucna_injection_guest_code(void)
{
	uint64_t ctl2;
	verify_apic_base_addr();
	xapic_enable();

	 
	xapic_write_reg(APIC_LVTCMCI, CMCI_VECTOR | APIC_DM_FIXED);
	ctl2 = rdmsr(MSR_IA32_MCx_CTL2(UCNA_BANK));
	wrmsr(MSR_IA32_MCx_CTL2(UCNA_BANK), ctl2 | MCI_CTL2_CMCI_EN);

	 
	asm volatile("sti");

	 
	GUEST_SYNC(SYNC_FIRST_UCNA);

	ucna_addr = rdmsr(MSR_IA32_MCx_ADDR(UCNA_BANK));

	 
	ctl2 = rdmsr(MSR_IA32_MCx_CTL2(UCNA_BANK));
	wrmsr(MSR_IA32_MCx_CTL2(UCNA_BANK), ctl2 & ~MCI_CTL2_CMCI_EN);

	 
	GUEST_SYNC(SYNC_SECOND_UCNA);

	ucna_addr2 = rdmsr(MSR_IA32_MCx_ADDR(UCNA_BANK));
	GUEST_DONE();
}

static void cmci_disabled_guest_code(void)
{
	uint64_t ctl2 = rdmsr(MSR_IA32_MCx_CTL2(UCNA_BANK));
	wrmsr(MSR_IA32_MCx_CTL2(UCNA_BANK), ctl2 | MCI_CTL2_CMCI_EN);

	GUEST_DONE();
}

static void cmci_enabled_guest_code(void)
{
	uint64_t ctl2 = rdmsr(MSR_IA32_MCx_CTL2(UCNA_BANK));
	wrmsr(MSR_IA32_MCx_CTL2(UCNA_BANK), ctl2 | MCI_CTL2_RESERVED_BIT);

	GUEST_DONE();
}

static void guest_cmci_handler(struct ex_regs *regs)
{
	i_ucna_rcvd++;
	i_ucna_addr = rdmsr(MSR_IA32_MCx_ADDR(UCNA_BANK));
	xapic_write_reg(APIC_EOI, 0);
}

static void guest_gp_handler(struct ex_regs *regs)
{
	GUEST_SYNC(SYNC_GP);
}

static void run_vcpu_expect_gp(struct kvm_vcpu *vcpu)
{
	struct ucall uc;

	vcpu_run(vcpu);

	TEST_ASSERT_KVM_EXIT_REASON(vcpu, KVM_EXIT_IO);
	TEST_ASSERT(get_ucall(vcpu, &uc) == UCALL_SYNC,
		    "Expect UCALL_SYNC\n");
	TEST_ASSERT(uc.args[1] == SYNC_GP, "#GP is expected.");
	printf("vCPU received GP in guest.\n");
}

static void inject_ucna(struct kvm_vcpu *vcpu, uint64_t addr) {
	 
	uint64_t status = MCI_STATUS_VAL | MCI_STATUS_UC | MCI_STATUS_EN |
			  MCI_STATUS_MISCV | MCI_STATUS_ADDRV | 0x10090;
	struct kvm_x86_mce mce = {};
	mce.status = status;
	mce.mcg_status = 0;
	 
	mce.misc = (MCM_ADDR_PHYS << 6) | 0xc;
	mce.addr = addr;
	mce.bank = UCNA_BANK;

	vcpu_ioctl(vcpu, KVM_X86_SET_MCE, &mce);
}

static void *run_ucna_injection(void *arg)
{
	struct thread_params *params = (struct thread_params *)arg;
	struct ucall uc;
	int old;
	int r;

	r = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old);
	TEST_ASSERT(r == 0,
		    "pthread_setcanceltype failed with errno=%d",
		    r);

	vcpu_run(params->vcpu);

	TEST_ASSERT_KVM_EXIT_REASON(params->vcpu, KVM_EXIT_IO);
	TEST_ASSERT(get_ucall(params->vcpu, &uc) == UCALL_SYNC,
		    "Expect UCALL_SYNC\n");
	TEST_ASSERT(uc.args[1] == SYNC_FIRST_UCNA, "Injecting first UCNA.");

	printf("Injecting first UCNA at %#x.\n", FIRST_UCNA_ADDR);

	inject_ucna(params->vcpu, FIRST_UCNA_ADDR);
	vcpu_run(params->vcpu);

	TEST_ASSERT_KVM_EXIT_REASON(params->vcpu, KVM_EXIT_IO);
	TEST_ASSERT(get_ucall(params->vcpu, &uc) == UCALL_SYNC,
		    "Expect UCALL_SYNC\n");
	TEST_ASSERT(uc.args[1] == SYNC_SECOND_UCNA, "Injecting second UCNA.");

	printf("Injecting second UCNA at %#x.\n", SECOND_UCNA_ADDR);

	inject_ucna(params->vcpu, SECOND_UCNA_ADDR);
	vcpu_run(params->vcpu);

	TEST_ASSERT_KVM_EXIT_REASON(params->vcpu, KVM_EXIT_IO);
	if (get_ucall(params->vcpu, &uc) == UCALL_ABORT) {
		TEST_ASSERT(false, "vCPU assertion failure: %s.\n",
			    (const char *)uc.args[0]);
	}

	return NULL;
}

static void test_ucna_injection(struct kvm_vcpu *vcpu, struct thread_params *params)
{
	struct kvm_vm *vm = vcpu->vm;
	params->vcpu = vcpu;
	params->p_i_ucna_rcvd = (uint64_t *)addr_gva2hva(vm, (uint64_t)&i_ucna_rcvd);
	params->p_i_ucna_addr = (uint64_t *)addr_gva2hva(vm, (uint64_t)&i_ucna_addr);
	params->p_ucna_addr = (uint64_t *)addr_gva2hva(vm, (uint64_t)&ucna_addr);
	params->p_ucna_addr2 = (uint64_t *)addr_gva2hva(vm, (uint64_t)&ucna_addr2);

	run_ucna_injection(params);

	TEST_ASSERT(*params->p_i_ucna_rcvd == 1, "Only first UCNA get signaled.");
	TEST_ASSERT(*params->p_i_ucna_addr == FIRST_UCNA_ADDR,
		    "Only first UCNA reported addr get recorded via interrupt.");
	TEST_ASSERT(*params->p_ucna_addr == FIRST_UCNA_ADDR,
		    "First injected UCNAs should get exposed via registers.");
	TEST_ASSERT(*params->p_ucna_addr2 == SECOND_UCNA_ADDR,
		    "Second injected UCNAs should get exposed via registers.");

	printf("Test successful.\n"
	       "UCNA CMCI interrupts received: %ld\n"
	       "Last UCNA address received via CMCI: %lx\n"
	       "First UCNA address in vCPU thread: %lx\n"
	       "Second UCNA address in vCPU thread: %lx\n",
	       *params->p_i_ucna_rcvd, *params->p_i_ucna_addr,
	       *params->p_ucna_addr, *params->p_ucna_addr2);
}

static void setup_mce_cap(struct kvm_vcpu *vcpu, bool enable_cmci_p)
{
	uint64_t mcg_caps = MCG_CTL_P | MCG_SER_P | MCG_LMCE_P | KVM_MAX_MCE_BANKS;
	if (enable_cmci_p)
		mcg_caps |= MCG_CMCI_P;

	mcg_caps &= supported_mcg_caps | MCG_CAP_BANKS_MASK;
	vcpu_ioctl(vcpu, KVM_X86_SETUP_MCE, &mcg_caps);
}

static struct kvm_vcpu *create_vcpu_with_mce_cap(struct kvm_vm *vm, uint32_t vcpuid,
						 bool enable_cmci_p, void *guest_code)
{
	struct kvm_vcpu *vcpu = vm_vcpu_add(vm, vcpuid, guest_code);
	setup_mce_cap(vcpu, enable_cmci_p);
	return vcpu;
}

int main(int argc, char *argv[])
{
	struct thread_params params;
	struct kvm_vm *vm;
	struct kvm_vcpu *ucna_vcpu;
	struct kvm_vcpu *cmcidis_vcpu;
	struct kvm_vcpu *cmci_vcpu;

	kvm_check_cap(KVM_CAP_MCE);

	vm = __vm_create(VM_MODE_DEFAULT, 3, 0);

	kvm_ioctl(vm->kvm_fd, KVM_X86_GET_MCE_CAP_SUPPORTED,
		  &supported_mcg_caps);

	if (!(supported_mcg_caps & MCG_CMCI_P)) {
		print_skip("MCG_CMCI_P is not supported");
		exit(KSFT_SKIP);
	}

	ucna_vcpu = create_vcpu_with_mce_cap(vm, 0, true, ucna_injection_guest_code);
	cmcidis_vcpu = create_vcpu_with_mce_cap(vm, 1, false, cmci_disabled_guest_code);
	cmci_vcpu = create_vcpu_with_mce_cap(vm, 2, true, cmci_enabled_guest_code);

	vm_init_descriptor_tables(vm);
	vcpu_init_descriptor_tables(ucna_vcpu);
	vcpu_init_descriptor_tables(cmcidis_vcpu);
	vcpu_init_descriptor_tables(cmci_vcpu);
	vm_install_exception_handler(vm, CMCI_VECTOR, guest_cmci_handler);
	vm_install_exception_handler(vm, GP_VECTOR, guest_gp_handler);

	virt_pg_map(vm, APIC_DEFAULT_GPA, APIC_DEFAULT_GPA);

	test_ucna_injection(ucna_vcpu, &params);
	run_vcpu_expect_gp(cmcidis_vcpu);
	run_vcpu_expect_gp(cmci_vcpu);

	kvm_vm_free(vm);
}
