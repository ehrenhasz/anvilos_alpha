#define MIPS_EXC_RESET              0
#define MIPS_EXC_SRESET             1
#define MIPS_EXC_DEBUG_ST           2
#define MIPS_EXC_DEBUG              3
#define MIPS_EXC_DDB                4
#define MIPS_EXC_NMI                5
#define MIPS_EXC_MCHK               6
#define MIPS_EXC_INT_TIMER          7
#define MIPS_EXC_INT_IO_1           8
#define MIPS_EXC_INT_IO_2           9
#define MIPS_EXC_EXECUTE            10
#define MIPS_EXC_INT_IPI_1          11
#define MIPS_EXC_INT_IPI_2          12
#define MIPS_EXC_MAX                13
#define C_TI        (_ULCAST_(1) << 30)
extern u32 *kvm_priority_to_irq;
u32 kvm_irq_to_priority(u32 irq);
int kvm_mips_pending_timer(struct kvm_vcpu *vcpu);
void kvm_mips_deliver_interrupts(struct kvm_vcpu *vcpu, u32 cause);
