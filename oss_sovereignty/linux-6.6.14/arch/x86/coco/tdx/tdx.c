
 

#undef pr_fmt
#define pr_fmt(fmt)     "tdx: " fmt

#include <linux/cpufeature.h>
#include <linux/export.h>
#include <linux/io.h>
#include <asm/coco.h>
#include <asm/tdx.h>
#include <asm/vmx.h>
#include <asm/ia32.h>
#include <asm/insn.h>
#include <asm/insn-eval.h>
#include <asm/pgtable.h>

 
#define EPT_READ	0
#define EPT_WRITE	1

 
#define PORT_READ	0
#define PORT_WRITE	1

 
#define VE_IS_IO_IN(e)		((e) & BIT(3))
#define VE_GET_IO_SIZE(e)	(((e) & GENMASK(2, 0)) + 1)
#define VE_GET_PORT_NUM(e)	((e) >> 16)
#define VE_IS_IO_STRING(e)	((e) & BIT(4))

#define ATTR_DEBUG		BIT(0)
#define ATTR_SEPT_VE_DISABLE	BIT(28)

 
#define TDCALL_RETURN_CODE(a)	((a) >> 32)
#define TDCALL_INVALID_OPERAND	0xc0000100

#define TDREPORT_SUBTYPE_0	0

 
noinstr void __tdx_hypercall_failed(void)
{
	instrumentation_begin();
	panic("TDVMCALL failed. TDX module bug?");
}

#ifdef CONFIG_KVM_GUEST
long tdx_kvm_hypercall(unsigned int nr, unsigned long p1, unsigned long p2,
		       unsigned long p3, unsigned long p4)
{
	struct tdx_hypercall_args args = {
		.r10 = nr,
		.r11 = p1,
		.r12 = p2,
		.r13 = p3,
		.r14 = p4,
	};

	return __tdx_hypercall(&args);
}
EXPORT_SYMBOL_GPL(tdx_kvm_hypercall);
#endif

 
static inline void tdx_module_call(u64 fn, u64 rcx, u64 rdx, u64 r8, u64 r9,
				   struct tdx_module_output *out)
{
	if (__tdx_module_call(fn, rcx, rdx, r8, r9, out))
		panic("TDCALL %lld failed (Buggy TDX module!)\n", fn);
}

 
int tdx_mcall_get_report0(u8 *reportdata, u8 *tdreport)
{
	u64 ret;

	ret = __tdx_module_call(TDX_GET_REPORT, virt_to_phys(tdreport),
				virt_to_phys(reportdata), TDREPORT_SUBTYPE_0,
				0, NULL);
	if (ret) {
		if (TDCALL_RETURN_CODE(ret) == TDCALL_INVALID_OPERAND)
			return -EINVAL;
		return -EIO;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tdx_mcall_get_report0);

static void __noreturn tdx_panic(const char *msg)
{
	struct tdx_hypercall_args args = {
		.r10 = TDX_HYPERCALL_STANDARD,
		.r11 = TDVMCALL_REPORT_FATAL_ERROR,
		.r12 = 0,  
	};
	union {
		 
		struct { u64 r14, r15, rbx, rdi, rsi, r8, r9, rdx; };

		char str[64];
	} message;

	 
	strncpy(message.str, msg, 64);

	args.r8  = message.r8;
	args.r9  = message.r9;
	args.r14 = message.r14;
	args.r15 = message.r15;
	args.rdi = message.rdi;
	args.rsi = message.rsi;
	args.rbx = message.rbx;
	args.rdx = message.rdx;

	 
	while (1)
		__tdx_hypercall(&args);
}

static void tdx_parse_tdinfo(u64 *cc_mask)
{
	struct tdx_module_output out;
	unsigned int gpa_width;
	u64 td_attr;

	 
	tdx_module_call(TDX_GET_INFO, 0, 0, 0, 0, &out);

	 
	gpa_width = out.rcx & GENMASK(5, 0);
	*cc_mask = BIT_ULL(gpa_width - 1);

	 
	td_attr = out.rdx;
	if (!(td_attr & ATTR_SEPT_VE_DISABLE)) {
		const char *msg = "TD misconfiguration: SEPT_VE_DISABLE attribute must be set.";

		 
		if (td_attr & ATTR_DEBUG)
			pr_warn("%s\n", msg);
		else
			tdx_panic(msg);
	}
}

 
static int ve_instr_len(struct ve_info *ve)
{
	switch (ve->exit_reason) {
	case EXIT_REASON_HLT:
	case EXIT_REASON_MSR_READ:
	case EXIT_REASON_MSR_WRITE:
	case EXIT_REASON_CPUID:
	case EXIT_REASON_IO_INSTRUCTION:
		 
		return ve->instr_len;
	case EXIT_REASON_EPT_VIOLATION:
		 
		WARN_ONCE(1, "ve->instr_len is not defined for EPT violations");
		return 0;
	default:
		WARN_ONCE(1, "Unexpected #VE-type: %lld\n", ve->exit_reason);
		return ve->instr_len;
	}
}

static u64 __cpuidle __halt(const bool irq_disabled)
{
	struct tdx_hypercall_args args = {
		.r10 = TDX_HYPERCALL_STANDARD,
		.r11 = hcall_func(EXIT_REASON_HLT),
		.r12 = irq_disabled,
	};

	 
	return __tdx_hypercall(&args);
}

static int handle_halt(struct ve_info *ve)
{
	const bool irq_disabled = irqs_disabled();

	if (__halt(irq_disabled))
		return -EIO;

	return ve_instr_len(ve);
}

void __cpuidle tdx_safe_halt(void)
{
	const bool irq_disabled = false;

	 
	if (__halt(irq_disabled))
		WARN_ONCE(1, "HLT instruction emulation failed\n");
}

static int read_msr(struct pt_regs *regs, struct ve_info *ve)
{
	struct tdx_hypercall_args args = {
		.r10 = TDX_HYPERCALL_STANDARD,
		.r11 = hcall_func(EXIT_REASON_MSR_READ),
		.r12 = regs->cx,
	};

	 
	if (__tdx_hypercall_ret(&args))
		return -EIO;

	regs->ax = lower_32_bits(args.r11);
	regs->dx = upper_32_bits(args.r11);
	return ve_instr_len(ve);
}

static int write_msr(struct pt_regs *regs, struct ve_info *ve)
{
	struct tdx_hypercall_args args = {
		.r10 = TDX_HYPERCALL_STANDARD,
		.r11 = hcall_func(EXIT_REASON_MSR_WRITE),
		.r12 = regs->cx,
		.r13 = (u64)regs->dx << 32 | regs->ax,
	};

	 
	if (__tdx_hypercall(&args))
		return -EIO;

	return ve_instr_len(ve);
}

static int handle_cpuid(struct pt_regs *regs, struct ve_info *ve)
{
	struct tdx_hypercall_args args = {
		.r10 = TDX_HYPERCALL_STANDARD,
		.r11 = hcall_func(EXIT_REASON_CPUID),
		.r12 = regs->ax,
		.r13 = regs->cx,
	};

	 
	if (regs->ax < 0x40000000 || regs->ax > 0x4FFFFFFF) {
		regs->ax = regs->bx = regs->cx = regs->dx = 0;
		return ve_instr_len(ve);
	}

	 
	if (__tdx_hypercall_ret(&args))
		return -EIO;

	 
	regs->ax = args.r12;
	regs->bx = args.r13;
	regs->cx = args.r14;
	regs->dx = args.r15;

	return ve_instr_len(ve);
}

static bool mmio_read(int size, unsigned long addr, unsigned long *val)
{
	struct tdx_hypercall_args args = {
		.r10 = TDX_HYPERCALL_STANDARD,
		.r11 = hcall_func(EXIT_REASON_EPT_VIOLATION),
		.r12 = size,
		.r13 = EPT_READ,
		.r14 = addr,
		.r15 = *val,
	};

	if (__tdx_hypercall_ret(&args))
		return false;
	*val = args.r11;
	return true;
}

static bool mmio_write(int size, unsigned long addr, unsigned long val)
{
	return !_tdx_hypercall(hcall_func(EXIT_REASON_EPT_VIOLATION), size,
			       EPT_WRITE, addr, val);
}

static int handle_mmio(struct pt_regs *regs, struct ve_info *ve)
{
	unsigned long *reg, val, vaddr;
	char buffer[MAX_INSN_SIZE];
	enum insn_mmio_type mmio;
	struct insn insn = {};
	int size, extend_size;
	u8 extend_val = 0;

	 
	if (WARN_ON_ONCE(user_mode(regs)))
		return -EFAULT;

	if (copy_from_kernel_nofault(buffer, (void *)regs->ip, MAX_INSN_SIZE))
		return -EFAULT;

	if (insn_decode(&insn, buffer, MAX_INSN_SIZE, INSN_MODE_64))
		return -EINVAL;

	mmio = insn_decode_mmio(&insn, &size);
	if (WARN_ON_ONCE(mmio == INSN_MMIO_DECODE_FAILED))
		return -EINVAL;

	if (mmio != INSN_MMIO_WRITE_IMM && mmio != INSN_MMIO_MOVS) {
		reg = insn_get_modrm_reg_ptr(&insn, regs);
		if (!reg)
			return -EINVAL;
	}

	 
	vaddr = (unsigned long)insn_get_addr_ref(&insn, regs);
	if (vaddr / PAGE_SIZE != (vaddr + size - 1) / PAGE_SIZE)
		return -EFAULT;

	 
	switch (mmio) {
	case INSN_MMIO_WRITE:
		memcpy(&val, reg, size);
		if (!mmio_write(size, ve->gpa, val))
			return -EIO;
		return insn.length;
	case INSN_MMIO_WRITE_IMM:
		val = insn.immediate.value;
		if (!mmio_write(size, ve->gpa, val))
			return -EIO;
		return insn.length;
	case INSN_MMIO_READ:
	case INSN_MMIO_READ_ZERO_EXTEND:
	case INSN_MMIO_READ_SIGN_EXTEND:
		 
		break;
	case INSN_MMIO_MOVS:
	case INSN_MMIO_DECODE_FAILED:
		 
		return -EINVAL;
	default:
		WARN_ONCE(1, "Unknown insn_decode_mmio() decode value?");
		return -EINVAL;
	}

	 
	if (!mmio_read(size, ve->gpa, &val))
		return -EIO;

	switch (mmio) {
	case INSN_MMIO_READ:
		 
		extend_size = size == 4 ? sizeof(*reg) : 0;
		break;
	case INSN_MMIO_READ_ZERO_EXTEND:
		 
		extend_size = insn.opnd_bytes;
		break;
	case INSN_MMIO_READ_SIGN_EXTEND:
		 
		extend_size = insn.opnd_bytes;
		if (size == 1 && val & BIT(7))
			extend_val = 0xFF;
		else if (size > 1 && val & BIT(15))
			extend_val = 0xFF;
		break;
	default:
		 
		WARN_ON_ONCE(1);
		return -EINVAL;
	}

	if (extend_size)
		memset(reg, extend_val, extend_size);
	memcpy(reg, &val, size);
	return insn.length;
}

static bool handle_in(struct pt_regs *regs, int size, int port)
{
	struct tdx_hypercall_args args = {
		.r10 = TDX_HYPERCALL_STANDARD,
		.r11 = hcall_func(EXIT_REASON_IO_INSTRUCTION),
		.r12 = size,
		.r13 = PORT_READ,
		.r14 = port,
	};
	u64 mask = GENMASK(BITS_PER_BYTE * size, 0);
	bool success;

	 
	success = !__tdx_hypercall_ret(&args);

	 
	regs->ax &= ~mask;
	if (success)
		regs->ax |= args.r11 & mask;

	return success;
}

static bool handle_out(struct pt_regs *regs, int size, int port)
{
	u64 mask = GENMASK(BITS_PER_BYTE * size, 0);

	 
	return !_tdx_hypercall(hcall_func(EXIT_REASON_IO_INSTRUCTION), size,
			       PORT_WRITE, port, regs->ax & mask);
}

 
static int handle_io(struct pt_regs *regs, struct ve_info *ve)
{
	u32 exit_qual = ve->exit_qual;
	int size, port;
	bool in, ret;

	if (VE_IS_IO_STRING(exit_qual))
		return -EIO;

	in   = VE_IS_IO_IN(exit_qual);
	size = VE_GET_IO_SIZE(exit_qual);
	port = VE_GET_PORT_NUM(exit_qual);


	if (in)
		ret = handle_in(regs, size, port);
	else
		ret = handle_out(regs, size, port);
	if (!ret)
		return -EIO;

	return ve_instr_len(ve);
}

 
__init bool tdx_early_handle_ve(struct pt_regs *regs)
{
	struct ve_info ve;
	int insn_len;

	tdx_get_ve_info(&ve);

	if (ve.exit_reason != EXIT_REASON_IO_INSTRUCTION)
		return false;

	insn_len = handle_io(regs, &ve);
	if (insn_len < 0)
		return false;

	regs->ip += insn_len;
	return true;
}

void tdx_get_ve_info(struct ve_info *ve)
{
	struct tdx_module_output out;

	 
	tdx_module_call(TDX_GET_VEINFO, 0, 0, 0, 0, &out);

	 
	ve->exit_reason = out.rcx;
	ve->exit_qual   = out.rdx;
	ve->gla         = out.r8;
	ve->gpa         = out.r9;
	ve->instr_len   = lower_32_bits(out.r10);
	ve->instr_info  = upper_32_bits(out.r10);
}

 
static int virt_exception_user(struct pt_regs *regs, struct ve_info *ve)
{
	switch (ve->exit_reason) {
	case EXIT_REASON_CPUID:
		return handle_cpuid(regs, ve);
	default:
		pr_warn("Unexpected #VE: %lld\n", ve->exit_reason);
		return -EIO;
	}
}

static inline bool is_private_gpa(u64 gpa)
{
	return gpa == cc_mkenc(gpa);
}

 
static int virt_exception_kernel(struct pt_regs *regs, struct ve_info *ve)
{
	switch (ve->exit_reason) {
	case EXIT_REASON_HLT:
		return handle_halt(ve);
	case EXIT_REASON_MSR_READ:
		return read_msr(regs, ve);
	case EXIT_REASON_MSR_WRITE:
		return write_msr(regs, ve);
	case EXIT_REASON_CPUID:
		return handle_cpuid(regs, ve);
	case EXIT_REASON_EPT_VIOLATION:
		if (is_private_gpa(ve->gpa))
			panic("Unexpected EPT-violation on private memory.");
		return handle_mmio(regs, ve);
	case EXIT_REASON_IO_INSTRUCTION:
		return handle_io(regs, ve);
	default:
		pr_warn("Unexpected #VE: %lld\n", ve->exit_reason);
		return -EIO;
	}
}

bool tdx_handle_virt_exception(struct pt_regs *regs, struct ve_info *ve)
{
	int insn_len;

	if (user_mode(regs))
		insn_len = virt_exception_user(regs, ve);
	else
		insn_len = virt_exception_kernel(regs, ve);
	if (insn_len < 0)
		return false;

	 
	regs->ip += insn_len;

	return true;
}

static bool tdx_tlb_flush_required(bool private)
{
	 
	return !private;
}

static bool tdx_cache_flush_required(void)
{
	 
	return true;
}

 
static bool tdx_enc_status_changed(unsigned long vaddr, int numpages, bool enc)
{
	phys_addr_t start = __pa(vaddr);
	phys_addr_t end   = __pa(vaddr + numpages * PAGE_SIZE);

	if (!enc) {
		 
		start |= cc_mkdec(0);
		end   |= cc_mkdec(0);
	}

	 
	if (_tdx_hypercall(TDVMCALL_MAP_GPA, start, end - start, 0, 0))
		return false;

	 
	if (enc)
		return tdx_accept_memory(start, end);

	return true;
}

static bool tdx_enc_status_change_prepare(unsigned long vaddr, int numpages,
					  bool enc)
{
	 
	if (enc)
		return tdx_enc_status_changed(vaddr, numpages, enc);
	return true;
}

static bool tdx_enc_status_change_finish(unsigned long vaddr, int numpages,
					 bool enc)
{
	 
	if (!enc)
		return tdx_enc_status_changed(vaddr, numpages, enc);
	return true;
}

void __init tdx_early_init(void)
{
	u64 cc_mask;
	u32 eax, sig[3];

	cpuid_count(TDX_CPUID_LEAF_ID, 0, &eax, &sig[0], &sig[2],  &sig[1]);

	if (memcmp(TDX_IDENT, sig, sizeof(sig)))
		return;

	setup_force_cpu_cap(X86_FEATURE_TDX_GUEST);

	cc_vendor = CC_VENDOR_INTEL;
	tdx_parse_tdinfo(&cc_mask);
	cc_set_mask(cc_mask);

	 
	tdx_module_call(TDX_WR, 0, TDCS_NOTIFY_ENABLES, 0, -1ULL, NULL);

	 
	physical_mask &= cc_mask - 1;

	 
	x86_platform.guest.enc_status_change_prepare = tdx_enc_status_change_prepare;
	x86_platform.guest.enc_status_change_finish  = tdx_enc_status_change_finish;

	x86_platform.guest.enc_cache_flush_required  = tdx_cache_flush_required;
	x86_platform.guest.enc_tlb_flush_required    = tdx_tlb_flush_required;

	 
	x86_cpuinit.parallel_bringup = false;

	pr_info("Guest detected\n");
}
