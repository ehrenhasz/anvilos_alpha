
 

#ifndef __BOOT_COMPRESSED
#define error(v)	pr_err(v)
#define has_cpuflag(f)	boot_cpu_has(f)
#else
#undef WARN
#define WARN(condition, format...) (!!(condition))
#endif

 
struct cpuid_leaf {
	u32 fn;
	u32 subfn;
	u32 eax;
	u32 ebx;
	u32 ecx;
	u32 edx;
};

 
struct snp_cpuid_fn {
	u32 eax_in;
	u32 ecx_in;
	u64 xcr0_in;
	u64 xss_in;
	u32 eax;
	u32 ebx;
	u32 ecx;
	u32 edx;
	u64 __reserved;
} __packed;

 
#define SNP_CPUID_COUNT_MAX 64

struct snp_cpuid_table {
	u32 count;
	u32 __reserved1;
	u64 __reserved2;
	struct snp_cpuid_fn fn[SNP_CPUID_COUNT_MAX];
} __packed;

 
static u16 ghcb_version __ro_after_init;

 
static struct snp_cpuid_table cpuid_table_copy __ro_after_init;

 
static u32 cpuid_std_range_max __ro_after_init;
static u32 cpuid_hyp_range_max __ro_after_init;
static u32 cpuid_ext_range_max __ro_after_init;

static bool __init sev_es_check_cpu_features(void)
{
	if (!has_cpuflag(X86_FEATURE_RDRAND)) {
		error("RDRAND instruction not supported - no trusted source of randomness available\n");
		return false;
	}

	return true;
}

static void __noreturn sev_es_terminate(unsigned int set, unsigned int reason)
{
	u64 val = GHCB_MSR_TERM_REQ;

	 
	val |= GHCB_SEV_TERM_REASON(set, reason);

	 
	sev_es_wr_ghcb_msr(val);
	VMGEXIT();

	while (true)
		asm volatile("hlt\n" : : : "memory");
}

 
static u64 get_hv_features(void)
{
	u64 val;

	if (ghcb_version < 2)
		return 0;

	sev_es_wr_ghcb_msr(GHCB_MSR_HV_FT_REQ);
	VMGEXIT();

	val = sev_es_rd_ghcb_msr();
	if (GHCB_RESP_CODE(val) != GHCB_MSR_HV_FT_RESP)
		return 0;

	return GHCB_MSR_HV_FT_RESP_VAL(val);
}

static void snp_register_ghcb_early(unsigned long paddr)
{
	unsigned long pfn = paddr >> PAGE_SHIFT;
	u64 val;

	sev_es_wr_ghcb_msr(GHCB_MSR_REG_GPA_REQ_VAL(pfn));
	VMGEXIT();

	val = sev_es_rd_ghcb_msr();

	 
	if ((GHCB_RESP_CODE(val) != GHCB_MSR_REG_GPA_RESP) ||
	    (GHCB_MSR_REG_GPA_RESP_VAL(val) != pfn))
		sev_es_terminate(SEV_TERM_SET_LINUX, GHCB_TERM_REGISTER);
}

static bool sev_es_negotiate_protocol(void)
{
	u64 val;

	 
	sev_es_wr_ghcb_msr(GHCB_MSR_SEV_INFO_REQ);
	VMGEXIT();
	val = sev_es_rd_ghcb_msr();

	if (GHCB_MSR_INFO(val) != GHCB_MSR_SEV_INFO_RESP)
		return false;

	if (GHCB_MSR_PROTO_MAX(val) < GHCB_PROTOCOL_MIN ||
	    GHCB_MSR_PROTO_MIN(val) > GHCB_PROTOCOL_MAX)
		return false;

	ghcb_version = min_t(size_t, GHCB_MSR_PROTO_MAX(val), GHCB_PROTOCOL_MAX);

	return true;
}

static __always_inline void vc_ghcb_invalidate(struct ghcb *ghcb)
{
	ghcb->save.sw_exit_code = 0;
	__builtin_memset(ghcb->save.valid_bitmap, 0, sizeof(ghcb->save.valid_bitmap));
}

static bool vc_decoding_needed(unsigned long exit_code)
{
	 
	return !(exit_code >= SVM_EXIT_EXCP_BASE &&
		 exit_code <= SVM_EXIT_LAST_EXCP);
}

static enum es_result vc_init_em_ctxt(struct es_em_ctxt *ctxt,
				      struct pt_regs *regs,
				      unsigned long exit_code)
{
	enum es_result ret = ES_OK;

	memset(ctxt, 0, sizeof(*ctxt));
	ctxt->regs = regs;

	if (vc_decoding_needed(exit_code))
		ret = vc_decode_insn(ctxt);

	return ret;
}

static void vc_finish_insn(struct es_em_ctxt *ctxt)
{
	ctxt->regs->ip += ctxt->insn.length;
}

static enum es_result verify_exception_info(struct ghcb *ghcb, struct es_em_ctxt *ctxt)
{
	u32 ret;

	ret = ghcb->save.sw_exit_info_1 & GENMASK_ULL(31, 0);
	if (!ret)
		return ES_OK;

	if (ret == 1) {
		u64 info = ghcb->save.sw_exit_info_2;
		unsigned long v = info & SVM_EVTINJ_VEC_MASK;

		 
		if ((info & SVM_EVTINJ_VALID) &&
		    ((v == X86_TRAP_GP) || (v == X86_TRAP_UD)) &&
		    ((info & SVM_EVTINJ_TYPE_MASK) == SVM_EVTINJ_TYPE_EXEPT)) {
			ctxt->fi.vector = v;

			if (info & SVM_EVTINJ_VALID_ERR)
				ctxt->fi.error_code = info >> 32;

			return ES_EXCEPTION;
		}
	}

	return ES_VMM_ERROR;
}

static enum es_result sev_es_ghcb_hv_call(struct ghcb *ghcb,
					  struct es_em_ctxt *ctxt,
					  u64 exit_code, u64 exit_info_1,
					  u64 exit_info_2)
{
	 
	ghcb->protocol_version = ghcb_version;
	ghcb->ghcb_usage       = GHCB_DEFAULT_USAGE;

	ghcb_set_sw_exit_code(ghcb, exit_code);
	ghcb_set_sw_exit_info_1(ghcb, exit_info_1);
	ghcb_set_sw_exit_info_2(ghcb, exit_info_2);

	sev_es_wr_ghcb_msr(__pa(ghcb));
	VMGEXIT();

	return verify_exception_info(ghcb, ctxt);
}

static int __sev_cpuid_hv(u32 fn, int reg_idx, u32 *reg)
{
	u64 val;

	sev_es_wr_ghcb_msr(GHCB_CPUID_REQ(fn, reg_idx));
	VMGEXIT();
	val = sev_es_rd_ghcb_msr();
	if (GHCB_RESP_CODE(val) != GHCB_MSR_CPUID_RESP)
		return -EIO;

	*reg = (val >> 32);

	return 0;
}

static int __sev_cpuid_hv_msr(struct cpuid_leaf *leaf)
{
	int ret;

	 
	if (cpuid_function_is_indexed(leaf->fn) && leaf->subfn)
		return -EINVAL;

	ret =         __sev_cpuid_hv(leaf->fn, GHCB_CPUID_REQ_EAX, &leaf->eax);
	ret = ret ? : __sev_cpuid_hv(leaf->fn, GHCB_CPUID_REQ_EBX, &leaf->ebx);
	ret = ret ? : __sev_cpuid_hv(leaf->fn, GHCB_CPUID_REQ_ECX, &leaf->ecx);
	ret = ret ? : __sev_cpuid_hv(leaf->fn, GHCB_CPUID_REQ_EDX, &leaf->edx);

	return ret;
}

static int __sev_cpuid_hv_ghcb(struct ghcb *ghcb, struct es_em_ctxt *ctxt, struct cpuid_leaf *leaf)
{
	u32 cr4 = native_read_cr4();
	int ret;

	ghcb_set_rax(ghcb, leaf->fn);
	ghcb_set_rcx(ghcb, leaf->subfn);

	if (cr4 & X86_CR4_OSXSAVE)
		 
		ghcb_set_xcr0(ghcb, xgetbv(XCR_XFEATURE_ENABLED_MASK));
	else
		 
		ghcb_set_xcr0(ghcb, 1);

	ret = sev_es_ghcb_hv_call(ghcb, ctxt, SVM_EXIT_CPUID, 0, 0);
	if (ret != ES_OK)
		return ret;

	if (!(ghcb_rax_is_valid(ghcb) &&
	      ghcb_rbx_is_valid(ghcb) &&
	      ghcb_rcx_is_valid(ghcb) &&
	      ghcb_rdx_is_valid(ghcb)))
		return ES_VMM_ERROR;

	leaf->eax = ghcb->save.rax;
	leaf->ebx = ghcb->save.rbx;
	leaf->ecx = ghcb->save.rcx;
	leaf->edx = ghcb->save.rdx;

	return ES_OK;
}

static int sev_cpuid_hv(struct ghcb *ghcb, struct es_em_ctxt *ctxt, struct cpuid_leaf *leaf)
{
	return ghcb ? __sev_cpuid_hv_ghcb(ghcb, ctxt, leaf)
		    : __sev_cpuid_hv_msr(leaf);
}

 
static const struct snp_cpuid_table *snp_cpuid_get_table(void)
{
	void *ptr;

	asm ("lea cpuid_table_copy(%%rip), %0"
	     : "=r" (ptr)
	     : "p" (&cpuid_table_copy));

	return ptr;
}

 
static u32 snp_cpuid_calc_xsave_size(u64 xfeatures_en, bool compacted)
{
	const struct snp_cpuid_table *cpuid_table = snp_cpuid_get_table();
	u64 xfeatures_found = 0;
	u32 xsave_size = 0x240;
	int i;

	for (i = 0; i < cpuid_table->count; i++) {
		const struct snp_cpuid_fn *e = &cpuid_table->fn[i];

		if (!(e->eax_in == 0xD && e->ecx_in > 1 && e->ecx_in < 64))
			continue;
		if (!(xfeatures_en & (BIT_ULL(e->ecx_in))))
			continue;
		if (xfeatures_found & (BIT_ULL(e->ecx_in)))
			continue;

		xfeatures_found |= (BIT_ULL(e->ecx_in));

		if (compacted)
			xsave_size += e->eax;
		else
			xsave_size = max(xsave_size, e->eax + e->ebx);
	}

	 
	if (xfeatures_found != (xfeatures_en & GENMASK_ULL(63, 2)))
		return 0;

	return xsave_size;
}

static bool
snp_cpuid_get_validated_func(struct cpuid_leaf *leaf)
{
	const struct snp_cpuid_table *cpuid_table = snp_cpuid_get_table();
	int i;

	for (i = 0; i < cpuid_table->count; i++) {
		const struct snp_cpuid_fn *e = &cpuid_table->fn[i];

		if (e->eax_in != leaf->fn)
			continue;

		if (cpuid_function_is_indexed(leaf->fn) && e->ecx_in != leaf->subfn)
			continue;

		 
		if (e->eax_in == 0xD && (e->ecx_in == 0 || e->ecx_in == 1))
			if (!(e->xcr0_in == 1 || e->xcr0_in == 3) || e->xss_in)
				continue;

		leaf->eax = e->eax;
		leaf->ebx = e->ebx;
		leaf->ecx = e->ecx;
		leaf->edx = e->edx;

		return true;
	}

	return false;
}

static void snp_cpuid_hv(struct ghcb *ghcb, struct es_em_ctxt *ctxt, struct cpuid_leaf *leaf)
{
	if (sev_cpuid_hv(ghcb, ctxt, leaf))
		sev_es_terminate(SEV_TERM_SET_LINUX, GHCB_TERM_CPUID_HV);
}

static int snp_cpuid_postprocess(struct ghcb *ghcb, struct es_em_ctxt *ctxt,
				 struct cpuid_leaf *leaf)
{
	struct cpuid_leaf leaf_hv = *leaf;

	switch (leaf->fn) {
	case 0x1:
		snp_cpuid_hv(ghcb, ctxt, &leaf_hv);

		 
		leaf->ebx = (leaf_hv.ebx & GENMASK(31, 24)) | (leaf->ebx & GENMASK(23, 0));
		 
		leaf->edx = (leaf_hv.edx & BIT(9)) | (leaf->edx & ~BIT(9));

		 
		if (native_read_cr4() & X86_CR4_OSXSAVE)
			leaf->ecx |= BIT(27);
		break;
	case 0x7:
		 
		leaf->ecx &= ~BIT(4);
		if (native_read_cr4() & X86_CR4_PKE)
			leaf->ecx |= BIT(4);
		break;
	case 0xB:
		leaf_hv.subfn = 0;
		snp_cpuid_hv(ghcb, ctxt, &leaf_hv);

		 
		leaf->edx = leaf_hv.edx;
		break;
	case 0xD: {
		bool compacted = false;
		u64 xcr0 = 1, xss = 0;
		u32 xsave_size;

		if (leaf->subfn != 0 && leaf->subfn != 1)
			return 0;

		if (native_read_cr4() & X86_CR4_OSXSAVE)
			xcr0 = xgetbv(XCR_XFEATURE_ENABLED_MASK);
		if (leaf->subfn == 1) {
			 
			if (leaf->eax & BIT(3)) {
				unsigned long lo, hi;

				asm volatile("rdmsr" : "=a" (lo), "=d" (hi)
						     : "c" (MSR_IA32_XSS));
				xss = (hi << 32) | lo;
			}

			 
			if (!(leaf->eax & (BIT(1) | BIT(3))))
				return -EINVAL;

			compacted = true;
		}

		xsave_size = snp_cpuid_calc_xsave_size(xcr0 | xss, compacted);
		if (!xsave_size)
			return -EINVAL;

		leaf->ebx = xsave_size;
		}
		break;
	case 0x8000001E:
		snp_cpuid_hv(ghcb, ctxt, &leaf_hv);

		 
		leaf->eax = leaf_hv.eax;
		 
		leaf->ebx = (leaf->ebx & GENMASK(31, 8)) | (leaf_hv.ebx & GENMASK(7, 0));
		 
		leaf->ecx = (leaf->ecx & GENMASK(31, 8)) | (leaf_hv.ecx & GENMASK(7, 0));
		break;
	default:
		 
		break;
	}

	return 0;
}

 
static int snp_cpuid(struct ghcb *ghcb, struct es_em_ctxt *ctxt, struct cpuid_leaf *leaf)
{
	const struct snp_cpuid_table *cpuid_table = snp_cpuid_get_table();

	if (!cpuid_table->count)
		return -EOPNOTSUPP;

	if (!snp_cpuid_get_validated_func(leaf)) {
		 
		leaf->eax = leaf->ebx = leaf->ecx = leaf->edx = 0;

		 
		if (!(leaf->fn <= cpuid_std_range_max ||
		      (leaf->fn >= 0x40000000 && leaf->fn <= cpuid_hyp_range_max) ||
		      (leaf->fn >= 0x80000000 && leaf->fn <= cpuid_ext_range_max)))
			return 0;
	}

	return snp_cpuid_postprocess(ghcb, ctxt, leaf);
}

 
void __init do_vc_no_ghcb(struct pt_regs *regs, unsigned long exit_code)
{
	unsigned int subfn = lower_bits(regs->cx, 32);
	unsigned int fn = lower_bits(regs->ax, 32);
	struct cpuid_leaf leaf;
	int ret;

	 
	if (exit_code != SVM_EXIT_CPUID)
		goto fail;

	leaf.fn = fn;
	leaf.subfn = subfn;

	ret = snp_cpuid(NULL, NULL, &leaf);
	if (!ret)
		goto cpuid_done;

	if (ret != -EOPNOTSUPP)
		goto fail;

	if (__sev_cpuid_hv_msr(&leaf))
		goto fail;

cpuid_done:
	regs->ax = leaf.eax;
	regs->bx = leaf.ebx;
	regs->cx = leaf.ecx;
	regs->dx = leaf.edx;

	 

	if (fn == 0x80000000 && (regs->ax < 0x8000001f))
		 
		goto fail;
	else if ((fn == 0x8000001f && !(regs->ax & BIT(1))))
		 
		goto fail;

	 
	regs->ip += 2;

	return;

fail:
	 
	sev_es_terminate(SEV_TERM_SET_GEN, GHCB_SEV_ES_GEN_REQ);
}

static enum es_result vc_insn_string_check(struct es_em_ctxt *ctxt,
					   unsigned long address,
					   bool write)
{
	if (user_mode(ctxt->regs) && fault_in_kernel_space(address)) {
		ctxt->fi.vector     = X86_TRAP_PF;
		ctxt->fi.error_code = X86_PF_USER;
		ctxt->fi.cr2        = address;
		if (write)
			ctxt->fi.error_code |= X86_PF_WRITE;

		return ES_EXCEPTION;
	}

	return ES_OK;
}

static enum es_result vc_insn_string_read(struct es_em_ctxt *ctxt,
					  void *src, char *buf,
					  unsigned int data_size,
					  unsigned int count,
					  bool backwards)
{
	int i, b = backwards ? -1 : 1;
	unsigned long address = (unsigned long)src;
	enum es_result ret;

	ret = vc_insn_string_check(ctxt, address, false);
	if (ret != ES_OK)
		return ret;

	for (i = 0; i < count; i++) {
		void *s = src + (i * data_size * b);
		char *d = buf + (i * data_size);

		ret = vc_read_mem(ctxt, s, d, data_size);
		if (ret != ES_OK)
			break;
	}

	return ret;
}

static enum es_result vc_insn_string_write(struct es_em_ctxt *ctxt,
					   void *dst, char *buf,
					   unsigned int data_size,
					   unsigned int count,
					   bool backwards)
{
	int i, s = backwards ? -1 : 1;
	unsigned long address = (unsigned long)dst;
	enum es_result ret;

	ret = vc_insn_string_check(ctxt, address, true);
	if (ret != ES_OK)
		return ret;

	for (i = 0; i < count; i++) {
		void *d = dst + (i * data_size * s);
		char *b = buf + (i * data_size);

		ret = vc_write_mem(ctxt, d, b, data_size);
		if (ret != ES_OK)
			break;
	}

	return ret;
}

#define IOIO_TYPE_STR  BIT(2)
#define IOIO_TYPE_IN   1
#define IOIO_TYPE_INS  (IOIO_TYPE_IN | IOIO_TYPE_STR)
#define IOIO_TYPE_OUT  0
#define IOIO_TYPE_OUTS (IOIO_TYPE_OUT | IOIO_TYPE_STR)

#define IOIO_REP       BIT(3)

#define IOIO_ADDR_64   BIT(9)
#define IOIO_ADDR_32   BIT(8)
#define IOIO_ADDR_16   BIT(7)

#define IOIO_DATA_32   BIT(6)
#define IOIO_DATA_16   BIT(5)
#define IOIO_DATA_8    BIT(4)

#define IOIO_SEG_ES    (0 << 10)
#define IOIO_SEG_DS    (3 << 10)

static enum es_result vc_ioio_exitinfo(struct es_em_ctxt *ctxt, u64 *exitinfo)
{
	struct insn *insn = &ctxt->insn;
	size_t size;
	u64 port;

	*exitinfo = 0;

	switch (insn->opcode.bytes[0]) {
	 
	case 0x6c:
	case 0x6d:
		*exitinfo |= IOIO_TYPE_INS;
		*exitinfo |= IOIO_SEG_ES;
		port	   = ctxt->regs->dx & 0xffff;
		break;

	 
	case 0x6e:
	case 0x6f:
		*exitinfo |= IOIO_TYPE_OUTS;
		*exitinfo |= IOIO_SEG_DS;
		port	   = ctxt->regs->dx & 0xffff;
		break;

	 
	case 0xe4:
	case 0xe5:
		*exitinfo |= IOIO_TYPE_IN;
		port	   = (u8)insn->immediate.value & 0xffff;
		break;

	 
	case 0xe6:
	case 0xe7:
		*exitinfo |= IOIO_TYPE_OUT;
		port	   = (u8)insn->immediate.value & 0xffff;
		break;

	 
	case 0xec:
	case 0xed:
		*exitinfo |= IOIO_TYPE_IN;
		port	   = ctxt->regs->dx & 0xffff;
		break;

	 
	case 0xee:
	case 0xef:
		*exitinfo |= IOIO_TYPE_OUT;
		port	   = ctxt->regs->dx & 0xffff;
		break;

	default:
		return ES_DECODE_FAILED;
	}

	*exitinfo |= port << 16;

	switch (insn->opcode.bytes[0]) {
	case 0x6c:
	case 0x6e:
	case 0xe4:
	case 0xe6:
	case 0xec:
	case 0xee:
		 
		*exitinfo |= IOIO_DATA_8;
		size       = 1;
		break;
	default:
		 
		*exitinfo |= (insn->opnd_bytes == 2) ? IOIO_DATA_16
						     : IOIO_DATA_32;
		size       = (insn->opnd_bytes == 2) ? 2 : 4;
	}

	switch (insn->addr_bytes) {
	case 2:
		*exitinfo |= IOIO_ADDR_16;
		break;
	case 4:
		*exitinfo |= IOIO_ADDR_32;
		break;
	case 8:
		*exitinfo |= IOIO_ADDR_64;
		break;
	}

	if (insn_has_rep_prefix(insn))
		*exitinfo |= IOIO_REP;

	return vc_ioio_check(ctxt, (u16)port, size);
}

static enum es_result vc_handle_ioio(struct ghcb *ghcb, struct es_em_ctxt *ctxt)
{
	struct pt_regs *regs = ctxt->regs;
	u64 exit_info_1, exit_info_2;
	enum es_result ret;

	ret = vc_ioio_exitinfo(ctxt, &exit_info_1);
	if (ret != ES_OK)
		return ret;

	if (exit_info_1 & IOIO_TYPE_STR) {

		 

		bool df = ((regs->flags & X86_EFLAGS_DF) == X86_EFLAGS_DF);
		unsigned int io_bytes, exit_bytes;
		unsigned int ghcb_count, op_count;
		unsigned long es_base;
		u64 sw_scratch;

		 
		io_bytes   = (exit_info_1 >> 4) & 0x7;
		ghcb_count = sizeof(ghcb->shared_buffer) / io_bytes;

		op_count    = (exit_info_1 & IOIO_REP) ? regs->cx : 1;
		exit_info_2 = min(op_count, ghcb_count);
		exit_bytes  = exit_info_2 * io_bytes;

		es_base = insn_get_seg_base(ctxt->regs, INAT_SEG_REG_ES);

		 
		if (!(exit_info_1 & IOIO_TYPE_IN)) {
			ret = vc_insn_string_read(ctxt,
					       (void *)(es_base + regs->si),
					       ghcb->shared_buffer, io_bytes,
					       exit_info_2, df);
			if (ret)
				return ret;
		}

		 
		sw_scratch = __pa(ghcb) + offsetof(struct ghcb, shared_buffer);
		ghcb_set_sw_scratch(ghcb, sw_scratch);
		ret = sev_es_ghcb_hv_call(ghcb, ctxt, SVM_EXIT_IOIO,
					  exit_info_1, exit_info_2);
		if (ret != ES_OK)
			return ret;

		 
		if (exit_info_1 & IOIO_TYPE_IN) {
			ret = vc_insn_string_write(ctxt,
						   (void *)(es_base + regs->di),
						   ghcb->shared_buffer, io_bytes,
						   exit_info_2, df);
			if (ret)
				return ret;

			if (df)
				regs->di -= exit_bytes;
			else
				regs->di += exit_bytes;
		} else {
			if (df)
				regs->si -= exit_bytes;
			else
				regs->si += exit_bytes;
		}

		if (exit_info_1 & IOIO_REP)
			regs->cx -= exit_info_2;

		ret = regs->cx ? ES_RETRY : ES_OK;

	} else {

		 

		int bits = (exit_info_1 & 0x70) >> 1;
		u64 rax = 0;

		if (!(exit_info_1 & IOIO_TYPE_IN))
			rax = lower_bits(regs->ax, bits);

		ghcb_set_rax(ghcb, rax);

		ret = sev_es_ghcb_hv_call(ghcb, ctxt, SVM_EXIT_IOIO, exit_info_1, 0);
		if (ret != ES_OK)
			return ret;

		if (exit_info_1 & IOIO_TYPE_IN) {
			if (!ghcb_rax_is_valid(ghcb))
				return ES_VMM_ERROR;
			regs->ax = lower_bits(ghcb->save.rax, bits);
		}
	}

	return ret;
}

static int vc_handle_cpuid_snp(struct ghcb *ghcb, struct es_em_ctxt *ctxt)
{
	struct pt_regs *regs = ctxt->regs;
	struct cpuid_leaf leaf;
	int ret;

	leaf.fn = regs->ax;
	leaf.subfn = regs->cx;
	ret = snp_cpuid(ghcb, ctxt, &leaf);
	if (!ret) {
		regs->ax = leaf.eax;
		regs->bx = leaf.ebx;
		regs->cx = leaf.ecx;
		regs->dx = leaf.edx;
	}

	return ret;
}

static enum es_result vc_handle_cpuid(struct ghcb *ghcb,
				      struct es_em_ctxt *ctxt)
{
	struct pt_regs *regs = ctxt->regs;
	u32 cr4 = native_read_cr4();
	enum es_result ret;
	int snp_cpuid_ret;

	snp_cpuid_ret = vc_handle_cpuid_snp(ghcb, ctxt);
	if (!snp_cpuid_ret)
		return ES_OK;
	if (snp_cpuid_ret != -EOPNOTSUPP)
		return ES_VMM_ERROR;

	ghcb_set_rax(ghcb, regs->ax);
	ghcb_set_rcx(ghcb, regs->cx);

	if (cr4 & X86_CR4_OSXSAVE)
		 
		ghcb_set_xcr0(ghcb, xgetbv(XCR_XFEATURE_ENABLED_MASK));
	else
		 
		ghcb_set_xcr0(ghcb, 1);

	ret = sev_es_ghcb_hv_call(ghcb, ctxt, SVM_EXIT_CPUID, 0, 0);
	if (ret != ES_OK)
		return ret;

	if (!(ghcb_rax_is_valid(ghcb) &&
	      ghcb_rbx_is_valid(ghcb) &&
	      ghcb_rcx_is_valid(ghcb) &&
	      ghcb_rdx_is_valid(ghcb)))
		return ES_VMM_ERROR;

	regs->ax = ghcb->save.rax;
	regs->bx = ghcb->save.rbx;
	regs->cx = ghcb->save.rcx;
	regs->dx = ghcb->save.rdx;

	return ES_OK;
}

static enum es_result vc_handle_rdtsc(struct ghcb *ghcb,
				      struct es_em_ctxt *ctxt,
				      unsigned long exit_code)
{
	bool rdtscp = (exit_code == SVM_EXIT_RDTSCP);
	enum es_result ret;

	ret = sev_es_ghcb_hv_call(ghcb, ctxt, exit_code, 0, 0);
	if (ret != ES_OK)
		return ret;

	if (!(ghcb_rax_is_valid(ghcb) && ghcb_rdx_is_valid(ghcb) &&
	     (!rdtscp || ghcb_rcx_is_valid(ghcb))))
		return ES_VMM_ERROR;

	ctxt->regs->ax = ghcb->save.rax;
	ctxt->regs->dx = ghcb->save.rdx;
	if (rdtscp)
		ctxt->regs->cx = ghcb->save.rcx;

	return ES_OK;
}

struct cc_setup_data {
	struct setup_data header;
	u32 cc_blob_address;
};

 
static struct cc_blob_sev_info *find_cc_blob_setup_data(struct boot_params *bp)
{
	struct cc_setup_data *sd = NULL;
	struct setup_data *hdr;

	hdr = (struct setup_data *)bp->hdr.setup_data;

	while (hdr) {
		if (hdr->type == SETUP_CC_BLOB) {
			sd = (struct cc_setup_data *)hdr;
			return (struct cc_blob_sev_info *)(unsigned long)sd->cc_blob_address;
		}
		hdr = (struct setup_data *)hdr->next;
	}

	return NULL;
}

 
static void __init setup_cpuid_table(const struct cc_blob_sev_info *cc_info)
{
	const struct snp_cpuid_table *cpuid_table_fw, *cpuid_table;
	int i;

	if (!cc_info || !cc_info->cpuid_phys || cc_info->cpuid_len < PAGE_SIZE)
		sev_es_terminate(SEV_TERM_SET_LINUX, GHCB_TERM_CPUID);

	cpuid_table_fw = (const struct snp_cpuid_table *)cc_info->cpuid_phys;
	if (!cpuid_table_fw->count || cpuid_table_fw->count > SNP_CPUID_COUNT_MAX)
		sev_es_terminate(SEV_TERM_SET_LINUX, GHCB_TERM_CPUID);

	cpuid_table = snp_cpuid_get_table();
	memcpy((void *)cpuid_table, cpuid_table_fw, sizeof(*cpuid_table));

	 
	for (i = 0; i < cpuid_table->count; i++) {
		const struct snp_cpuid_fn *fn = &cpuid_table->fn[i];

		if (fn->eax_in == 0x0)
			cpuid_std_range_max = fn->eax;
		else if (fn->eax_in == 0x40000000)
			cpuid_hyp_range_max = fn->eax;
		else if (fn->eax_in == 0x80000000)
			cpuid_ext_range_max = fn->eax;
	}
}

static void pvalidate_pages(struct snp_psc_desc *desc)
{
	struct psc_entry *e;
	unsigned long vaddr;
	unsigned int size;
	unsigned int i;
	bool validate;
	int rc;

	for (i = 0; i <= desc->hdr.end_entry; i++) {
		e = &desc->entries[i];

		vaddr = (unsigned long)pfn_to_kaddr(e->gfn);
		size = e->pagesize ? RMP_PG_SIZE_2M : RMP_PG_SIZE_4K;
		validate = e->operation == SNP_PAGE_STATE_PRIVATE;

		rc = pvalidate(vaddr, size, validate);
		if (rc == PVALIDATE_FAIL_SIZEMISMATCH && size == RMP_PG_SIZE_2M) {
			unsigned long vaddr_end = vaddr + PMD_SIZE;

			for (; vaddr < vaddr_end; vaddr += PAGE_SIZE) {
				rc = pvalidate(vaddr, RMP_PG_SIZE_4K, validate);
				if (rc)
					break;
			}
		}

		if (rc) {
			WARN(1, "Failed to validate address 0x%lx ret %d", vaddr, rc);
			sev_es_terminate(SEV_TERM_SET_LINUX, GHCB_TERM_PVALIDATE);
		}
	}
}

static int vmgexit_psc(struct ghcb *ghcb, struct snp_psc_desc *desc)
{
	int cur_entry, end_entry, ret = 0;
	struct snp_psc_desc *data;
	struct es_em_ctxt ctxt;

	vc_ghcb_invalidate(ghcb);

	 
	data = (struct snp_psc_desc *)ghcb->shared_buffer;
	memcpy(ghcb->shared_buffer, desc, min_t(int, GHCB_SHARED_BUF_SIZE, sizeof(*desc)));

	 
	cur_entry = data->hdr.cur_entry;
	end_entry = data->hdr.end_entry;

	while (data->hdr.cur_entry <= data->hdr.end_entry) {
		ghcb_set_sw_scratch(ghcb, (u64)__pa(data));

		 
		ret = sev_es_ghcb_hv_call(ghcb, &ctxt, SVM_VMGEXIT_PSC, 0, 0);

		 
		if (WARN(ret || ghcb->save.sw_exit_info_2,
			 "SNP: PSC failed ret=%d exit_info_2=%llx\n",
			 ret, ghcb->save.sw_exit_info_2)) {
			ret = 1;
			goto out;
		}

		 
		if (WARN(data->hdr.reserved, "Reserved bit is set in the PSC header\n")) {
			ret = 1;
			goto out;
		}

		 
		if (WARN(data->hdr.end_entry > end_entry || cur_entry > data->hdr.cur_entry,
"SNP: PSC processing going backward, end_entry %d (got %d) cur_entry %d (got %d)\n",
			 end_entry, data->hdr.end_entry, cur_entry, data->hdr.cur_entry)) {
			ret = 1;
			goto out;
		}
	}

out:
	return ret;
}
