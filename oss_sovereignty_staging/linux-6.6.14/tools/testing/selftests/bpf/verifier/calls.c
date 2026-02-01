{
	"calls: invalid kfunc call not eliminated",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.result  = REJECT,
	.errstr = "invalid kernel function call not eliminated in verifier pass",
},
{
	"calls: invalid kfunc call unreachable",
	.insns = {
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_JMP_IMM(BPF_JGT, BPF_REG_0, 0, 2),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.result  = ACCEPT,
},
{
	"calls: invalid kfunc call: ptr_to_mem to struct with non-scalar",
	.insns = {
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.result = REJECT,
	.errstr = "arg#0 pointer type STRUCT prog_test_fail1 must point to scalar",
	.fixup_kfunc_btf_id = {
		{ "bpf_kfunc_call_test_fail1", 2 },
	},
},
{
	"calls: invalid kfunc call: ptr_to_mem to struct with nesting depth > 4",
	.insns = {
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.result = REJECT,
	.errstr = "max struct nesting depth exceeded\narg#0 pointer type STRUCT prog_test_fail2",
	.fixup_kfunc_btf_id = {
		{ "bpf_kfunc_call_test_fail2", 2 },
	},
},
{
	"calls: invalid kfunc call: ptr_to_mem to struct with FAM",
	.insns = {
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.result = REJECT,
	.errstr = "arg#0 pointer type STRUCT prog_test_fail3 must point to scalar",
	.fixup_kfunc_btf_id = {
		{ "bpf_kfunc_call_test_fail3", 2 },
	},
},
{
	"calls: invalid kfunc call: reg->type != PTR_TO_CTX",
	.insns = {
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.result = REJECT,
	.errstr = "R1 must have zero offset when passed to release func or trusted arg to kfunc",
	.fixup_kfunc_btf_id = {
		{ "bpf_kfunc_call_test_pass_ctx", 2 },
	},
},
{
	"calls: invalid kfunc call: void * not allowed in func proto without mem size arg",
	.insns = {
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.result = REJECT,
	.errstr = "arg#0 pointer type UNKNOWN  must point to scalar",
	.fixup_kfunc_btf_id = {
		{ "bpf_kfunc_call_test_mem_len_fail1", 2 },
	},
},
{
	"calls: trigger reg2btf_ids[reg->type] for reg->type > __BPF_REG_TYPE_MAX",
	.insns = {
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_ST_MEM(BPF_DW, BPF_REG_1, 0, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.result = REJECT,
	.errstr = "Possibly NULL pointer passed to trusted arg0",
	.fixup_kfunc_btf_id = {
		{ "bpf_kfunc_call_test_acquire", 3 },
		{ "bpf_kfunc_call_test_release", 5 },
	},
},
{
	"calls: invalid kfunc call: reg->off must be zero when passed to release kfunc",
	.insns = {
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_ST_MEM(BPF_DW, BPF_REG_1, 0, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 1),
	BPF_EXIT_INSN(),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, 8),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.result = REJECT,
	.errstr = "R1 must have zero offset when passed to release func",
	.fixup_kfunc_btf_id = {
		{ "bpf_kfunc_call_test_acquire", 3 },
		{ "bpf_kfunc_call_memb_release", 8 },
	},
},
{
	"calls: invalid kfunc call: don't match first member type when passed to release kfunc",
	.insns = {
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.result = REJECT,
	.errstr = "kernel function bpf_kfunc_call_memb1_release args#0 expected pointer",
	.fixup_kfunc_btf_id = {
		{ "bpf_kfunc_call_memb_acquire", 1 },
		{ "bpf_kfunc_call_memb1_release", 5 },
	},
},
{
	"calls: invalid kfunc call: PTR_TO_BTF_ID with negative offset",
	.insns = {
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_ST_MEM(BPF_DW, BPF_REG_1, 0, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_0),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_0),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -4),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_2),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.fixup_kfunc_btf_id = {
		{ "bpf_kfunc_call_test_acquire", 3 },
		{ "bpf_kfunc_call_test_offset", 9 },
		{ "bpf_kfunc_call_test_release", 12 },
	},
	.result_unpriv = REJECT,
	.result = REJECT,
	.errstr = "ptr R1 off=-4 disallowed",
},
{
	"calls: invalid kfunc call: PTR_TO_BTF_ID with variable offset",
	.insns = {
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_ST_MEM(BPF_DW, BPF_REG_1, 0, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_0),
	BPF_LDX_MEM(BPF_W, BPF_REG_2, BPF_REG_0, 4),
	BPF_JMP_IMM(BPF_JLE, BPF_REG_2, 4, 3),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	BPF_JMP_IMM(BPF_JGE, BPF_REG_2, 0, 3),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	BPF_ALU64_REG(BPF_ADD, BPF_REG_1, BPF_REG_2),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.fixup_kfunc_btf_id = {
		{ "bpf_kfunc_call_test_acquire", 3 },
		{ "bpf_kfunc_call_test_release", 9 },
		{ "bpf_kfunc_call_test_release", 13 },
		{ "bpf_kfunc_call_test_release", 17 },
	},
	.result_unpriv = REJECT,
	.result = REJECT,
	.errstr = "variable ptr_ access var_off=(0x0; 0x7) disallowed",
},
{
	"calls: invalid kfunc call: referenced arg needs refcounted PTR_TO_BTF_ID",
	.insns = {
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_ST_MEM(BPF_DW, BPF_REG_1, 0, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_0),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_LDX_MEM(BPF_DW, BPF_REG_1, BPF_REG_6, 16),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.fixup_kfunc_btf_id = {
		{ "bpf_kfunc_call_test_acquire", 3 },
		{ "bpf_kfunc_call_test_ref", 8 },
		{ "bpf_kfunc_call_test_ref", 10 },
	},
	.result_unpriv = REJECT,
	.result = REJECT,
	.errstr = "R1 must be",
},
{
	"calls: valid kfunc call: referenced arg needs refcounted PTR_TO_BTF_ID",
	.insns = {
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_ST_MEM(BPF_DW, BPF_REG_1, 0, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_0),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_6),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_KFUNC_CALL, 0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.fixup_kfunc_btf_id = {
		{ "bpf_kfunc_call_test_acquire", 3 },
		{ "bpf_kfunc_call_test_ref", 8 },
		{ "bpf_kfunc_call_test_release", 10 },
	},
	.result_unpriv = REJECT,
	.result = ACCEPT,
},
{
	"calls: basic sanity",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 2),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_IMM(BPF_REG_0, 2),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.result = ACCEPT,
},
{
	"calls: not on unprivileged",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 2),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_IMM(BPF_REG_0, 2),
	BPF_EXIT_INSN(),
	},
	.errstr_unpriv = "loading/calling other bpf or kernel functions are allowed for",
	.result_unpriv = REJECT,
	.result = ACCEPT,
	.retval = 1,
},
{
	"calls: div by 0 in subprog",
	.insns = {
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 8),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_6),
	BPF_LDX_MEM(BPF_W, BPF_REG_1, BPF_REG_1,
		    offsetof(struct __sk_buff, data_end)),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_0),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, 8),
	BPF_JMP_REG(BPF_JGT, BPF_REG_2, BPF_REG_1, 1),
	BPF_LDX_MEM(BPF_B, BPF_REG_0, BPF_REG_0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV32_IMM(BPF_REG_2, 0),
	BPF_MOV32_IMM(BPF_REG_3, 1),
	BPF_ALU32_REG(BPF_DIV, BPF_REG_3, BPF_REG_2),
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_1,
		    offsetof(struct __sk_buff, data)),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.result = ACCEPT,
	.retval = 1,
},
{
	"calls: multiple ret types in subprog 1",
	.insns = {
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 8),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_6),
	BPF_LDX_MEM(BPF_W, BPF_REG_1, BPF_REG_1,
		    offsetof(struct __sk_buff, data_end)),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_0),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, 8),
	BPF_JMP_REG(BPF_JGT, BPF_REG_2, BPF_REG_1, 1),
	BPF_LDX_MEM(BPF_B, BPF_REG_0, BPF_REG_0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_EXIT_INSN(),
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_1,
		    offsetof(struct __sk_buff, data)),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 1),
	BPF_MOV32_IMM(BPF_REG_0, 42),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.result = REJECT,
	.errstr = "R0 invalid mem access 'scalar'",
},
{
	"calls: multiple ret types in subprog 2",
	.insns = {
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 8),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_6),
	BPF_LDX_MEM(BPF_W, BPF_REG_1, BPF_REG_1,
		    offsetof(struct __sk_buff, data_end)),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_0),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, 8),
	BPF_JMP_REG(BPF_JGT, BPF_REG_2, BPF_REG_1, 1),
	BPF_LDX_MEM(BPF_B, BPF_REG_0, BPF_REG_0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_EXIT_INSN(),
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_1,
		    offsetof(struct __sk_buff, data)),
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 9),
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 0),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -8),
	BPF_LD_MAP_FD(BPF_REG_1, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 1),
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_6,
		    offsetof(struct __sk_buff, data)),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, 64),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.fixup_map_hash_8b = { 16 },
	.result = REJECT,
	.errstr = "R0 min value is outside of the allowed memory range",
},
{
	"calls: overlapping caller/callee",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.errstr = "last insn is not an exit or jmp",
	.result = REJECT,
},
{
	"calls: wrong recursive calls",
	.insns = {
	BPF_JMP_IMM(BPF_JA, 0, 0, 4),
	BPF_JMP_IMM(BPF_JA, 0, 0, 4),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, -2),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, -2),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, -2),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.errstr = "jump out of range",
	.result = REJECT,
},
{
	"calls: wrong src reg",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 3, 0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.errstr = "BPF_CALL uses reserved fields",
	.result = REJECT,
},
{
	"calls: wrong off value",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, -1, 2),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_IMM(BPF_REG_0, 2),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.errstr = "BPF_CALL uses reserved fields",
	.result = REJECT,
},
{
	"calls: jump back loop",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, -1),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.errstr = "the call stack of 9 frames is too deep",
	.result = REJECT,
},
{
	"calls: conditional call",
	.insns = {
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_1,
		    offsetof(struct __sk_buff, mark)),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 3),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 2),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_IMM(BPF_REG_0, 2),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.errstr = "jump out of range",
	.result = REJECT,
},
{
	"calls: conditional call 2",
	.insns = {
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_1,
		    offsetof(struct __sk_buff, mark)),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 3),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 4),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_IMM(BPF_REG_0, 2),
	BPF_EXIT_INSN(),
	BPF_MOV64_IMM(BPF_REG_0, 3),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.result = ACCEPT,
},
{
	"calls: conditional call 3",
	.insns = {
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_1,
		    offsetof(struct __sk_buff, mark)),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 3),
	BPF_JMP_IMM(BPF_JA, 0, 0, 4),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_JMP_IMM(BPF_JA, 0, 0, -6),
	BPF_MOV64_IMM(BPF_REG_0, 3),
	BPF_JMP_IMM(BPF_JA, 0, 0, -6),
	},
	.prog_type = BPF_PROG_TYPE_SOCKET_FILTER,
	.errstr_unpriv = "back-edge from insn",
	.result_unpriv = REJECT,
	.result = ACCEPT,
	.retval = 1,
},
{
	"calls: conditional call 4",
	.insns = {
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_1,
		    offsetof(struct __sk_buff, mark)),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 3),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 4),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_JMP_IMM(BPF_JA, 0, 0, -5),
	BPF_MOV64_IMM(BPF_REG_0, 3),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.result = ACCEPT,
},
{
	"calls: conditional call 5",
	.insns = {
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_1,
		    offsetof(struct __sk_buff, mark)),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 3),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 4),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_JMP_IMM(BPF_JA, 0, 0, -6),
	BPF_MOV64_IMM(BPF_REG_0, 3),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.result = ACCEPT,
	.retval = 1,
},
{
	"calls: conditional call 6",
	.insns = {
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_6),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 2),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, -3),
	BPF_EXIT_INSN(),
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_1,
		    offsetof(struct __sk_buff, mark)),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.errstr = "infinite loop detected",
	.result = REJECT,
},
{
	"calls: using r0 returned by callee",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_IMM(BPF_REG_0, 2),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.result = ACCEPT,
},
{
	"calls: using uninit r0 from callee",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.errstr = "!read_ok",
	.result = REJECT,
},
{
	"calls: callee is using r1",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_1,
		    offsetof(struct __sk_buff, len)),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_ACT,
	.result = ACCEPT,
	.retval = TEST_DATA_LEN,
},
{
	"calls: callee using args1",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_1),
	BPF_EXIT_INSN(),
	},
	.errstr_unpriv = "allowed for",
	.result_unpriv = REJECT,
	.result = ACCEPT,
	.retval = POINTER_VALUE,
},
{
	"calls: callee using wrong args2",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_2),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.errstr = "R2 !read_ok",
	.result = REJECT,
},
{
	"calls: callee using two args",
	.insns = {
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_LDX_MEM(BPF_W, BPF_REG_1, BPF_REG_6,
		    offsetof(struct __sk_buff, len)),
	BPF_LDX_MEM(BPF_W, BPF_REG_2, BPF_REG_6,
		    offsetof(struct __sk_buff, len)),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_1),
	BPF_ALU64_REG(BPF_ADD, BPF_REG_0, BPF_REG_2),
	BPF_EXIT_INSN(),
	},
	.errstr_unpriv = "allowed for",
	.result_unpriv = REJECT,
	.result = ACCEPT,
	.retval = TEST_DATA_LEN + TEST_DATA_LEN - ETH_HLEN - ETH_HLEN,
},
{
	"calls: callee changing pkt pointers",
	.insns = {
	BPF_LDX_MEM(BPF_W, BPF_REG_6, BPF_REG_1, offsetof(struct xdp_md, data)),
	BPF_LDX_MEM(BPF_W, BPF_REG_7, BPF_REG_1,
		    offsetof(struct xdp_md, data_end)),
	BPF_MOV64_REG(BPF_REG_8, BPF_REG_6),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_8, 8),
	BPF_JMP_REG(BPF_JGT, BPF_REG_8, BPF_REG_7, 2),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 3),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_6, 0),
	BPF_MOV32_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	BPF_MOV64_IMM(BPF_REG_2, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_xdp_adjust_head),
	BPF_EXIT_INSN(),
	},
	.result = REJECT,
	.errstr = "R6 invalid mem access 'scalar'",
	.prog_type = BPF_PROG_TYPE_XDP,
	.flags = F_NEEDS_EFFICIENT_UNALIGNED_ACCESS,
},
{
	"calls: ptr null check in subprog",
	.insns = {
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 0),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -8),
	BPF_LD_MAP_FD(BPF_REG_1, 0),
	BPF_EMIT_CALL(BPF_FUNC_map_lookup_elem),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_0),
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 3),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 1),
	BPF_LDX_MEM(BPF_B, BPF_REG_0, BPF_REG_6, 0),
	BPF_EXIT_INSN(),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_1, 0, 1),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_EXIT_INSN(),
	},
	.errstr_unpriv = "loading/calling other bpf or kernel functions are allowed for",
	.fixup_map_hash_48b = { 3 },
	.result_unpriv = REJECT,
	.result = ACCEPT,
	.retval = 0,
},
{
	"calls: two calls with args",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 6),
	BPF_MOV64_REG(BPF_REG_7, BPF_REG_0),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_6),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 3),
	BPF_ALU64_REG(BPF_ADD, BPF_REG_7, BPF_REG_0),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_7),
	BPF_EXIT_INSN(),
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_1,
		    offsetof(struct __sk_buff, len)),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.result = ACCEPT,
	.retval = TEST_DATA_LEN + TEST_DATA_LEN,
},
{
	"calls: calls with stack arith",
	.insns = {
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -64),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -64),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -64),
	BPF_MOV64_IMM(BPF_REG_0, 42),
	BPF_STX_MEM(BPF_DW, BPF_REG_2, BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.result = ACCEPT,
	.retval = 42,
},
{
	"calls: calls with misaligned stack access",
	.insns = {
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -63),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -61),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -63),
	BPF_MOV64_IMM(BPF_REG_0, 42),
	BPF_STX_MEM(BPF_DW, BPF_REG_2, BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.flags = F_LOAD_WITH_STRICT_ALIGNMENT,
	.errstr = "misaligned stack access",
	.result = REJECT,
},
{
	"calls: calls control flow, jump test",
	.insns = {
	BPF_MOV64_IMM(BPF_REG_0, 42),
	BPF_JMP_IMM(BPF_JA, 0, 0, 2),
	BPF_MOV64_IMM(BPF_REG_0, 43),
	BPF_JMP_IMM(BPF_JA, 0, 0, 1),
	BPF_JMP_IMM(BPF_JA, 0, 0, -3),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.result = ACCEPT,
	.retval = 43,
},
{
	"calls: calls control flow, jump test 2",
	.insns = {
	BPF_MOV64_IMM(BPF_REG_0, 42),
	BPF_JMP_IMM(BPF_JA, 0, 0, 2),
	BPF_MOV64_IMM(BPF_REG_0, 43),
	BPF_JMP_IMM(BPF_JA, 0, 0, 1),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, -3),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.errstr = "jump out of range from insn 1 to 4",
	.result = REJECT,
},
{
	"calls: two calls with bad jump",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 6),
	BPF_MOV64_REG(BPF_REG_7, BPF_REG_0),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_6),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 3),
	BPF_ALU64_REG(BPF_ADD, BPF_REG_7, BPF_REG_0),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_7),
	BPF_EXIT_INSN(),
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_1,
		    offsetof(struct __sk_buff, len)),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, -3),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.errstr = "jump out of range from insn 11 to 9",
	.result = REJECT,
},
{
	"calls: recursive call. test1",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, -1),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.errstr = "the call stack of 9 frames is too deep",
	.result = REJECT,
},
{
	"calls: recursive call. test2",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, -3),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.errstr = "the call stack of 9 frames is too deep",
	.result = REJECT,
},
{
	"calls: unreachable code",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.errstr = "unreachable insn 6",
	.result = REJECT,
},
{
	"calls: invalid call",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, -4),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.errstr = "invalid destination",
	.result = REJECT,
},
{
	"calls: invalid call 2",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 0x7fffffff),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.errstr = "invalid destination",
	.result = REJECT,
},
{
	"calls: jumping across function bodies. test1",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 2),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_1, 0, -3),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.errstr = "jump out of range",
	.result = REJECT,
},
{
	"calls: jumping across function bodies. test2",
	.insns = {
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_1, 0, 3),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 2),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.errstr = "jump out of range",
	.result = REJECT,
},
{
	"calls: call without exit",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_1, 0, -2),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.errstr = "not an exit",
	.result = REJECT,
},
{
	"calls: call into middle of ld_imm64",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 3),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 3),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	BPF_LD_IMM64(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.errstr = "last insn",
	.result = REJECT,
},
{
	"calls: call into middle of other call",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 3),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 3),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.errstr = "last insn",
	.result = REJECT,
},
{
	"calls: subprog call with ld_abs in main prog",
	.insns = {
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_LD_ABS(BPF_B, 0),
	BPF_LD_ABS(BPF_H, 0),
	BPF_LD_ABS(BPF_W, 0),
	BPF_MOV64_REG(BPF_REG_7, BPF_REG_6),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_6),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 5),
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_7),
	BPF_LD_ABS(BPF_B, 0),
	BPF_LD_ABS(BPF_H, 0),
	BPF_LD_ABS(BPF_W, 0),
	BPF_EXIT_INSN(),
	BPF_MOV64_IMM(BPF_REG_2, 1),
	BPF_MOV64_IMM(BPF_REG_3, 2),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_skb_vlan_push),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.result = ACCEPT,
},
{
	"calls: two calls with bad fallthrough",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 6),
	BPF_MOV64_REG(BPF_REG_7, BPF_REG_0),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_6),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 3),
	BPF_ALU64_REG(BPF_ADD, BPF_REG_7, BPF_REG_0),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_7),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_0),
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_1,
		    offsetof(struct __sk_buff, len)),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_TRACEPOINT,
	.errstr = "not an exit",
	.result = REJECT,
},
{
	"calls: two calls with stack read",
	.insns = {
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 0),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 6),
	BPF_MOV64_REG(BPF_REG_7, BPF_REG_0),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_6),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 3),
	BPF_ALU64_REG(BPF_ADD, BPF_REG_7, BPF_REG_0),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_7),
	BPF_EXIT_INSN(),
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_1, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_XDP,
	.result = ACCEPT,
},
{
	"calls: two calls with stack write",
	.insns = {
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 0),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -16),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 2),
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_10, -16),
	BPF_EXIT_INSN(),

	 
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_MOV64_REG(BPF_REG_7, BPF_REG_2),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 7),
	BPF_MOV64_REG(BPF_REG_8, BPF_REG_0),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_6),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 4),
	BPF_ALU64_REG(BPF_ADD, BPF_REG_8, BPF_REG_0),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_8),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_7, BPF_REG_0, 0),
	BPF_EXIT_INSN(),

	 
	 
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_1, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_XDP,
	.result = ACCEPT,
},
{
	"calls: stack overflow using two frames (pre-call access)",
	.insns = {
	 
	BPF_ST_MEM(BPF_B, BPF_REG_10, -300, 0),
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),

	 
	BPF_ST_MEM(BPF_B, BPF_REG_10, -300, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_XDP,
	.errstr = "combined stack size",
	.result = REJECT,
},
{
	"calls: stack overflow using two frames (post-call access)",
	.insns = {
	 
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 2),
	BPF_ST_MEM(BPF_B, BPF_REG_10, -300, 0),
	BPF_EXIT_INSN(),

	 
	BPF_ST_MEM(BPF_B, BPF_REG_10, -300, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_XDP,
	.errstr = "combined stack size",
	.result = REJECT,
},
{
	"calls: stack depth check using three frames. test1",
	.insns = {
	 
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 4),  
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 5),  
	BPF_ST_MEM(BPF_B, BPF_REG_10, -32, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	 
	BPF_ST_MEM(BPF_B, BPF_REG_10, -256, 0),
	BPF_EXIT_INSN(),
	 
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, -3),  
	BPF_ST_MEM(BPF_B, BPF_REG_10, -64, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_XDP,
	 
	.result = ACCEPT,
},
{
	"calls: stack depth check using three frames. test2",
	.insns = {
	 
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 4),  
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 5),  
	BPF_ST_MEM(BPF_B, BPF_REG_10, -32, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	 
	BPF_ST_MEM(BPF_B, BPF_REG_10, -64, 0),
	BPF_EXIT_INSN(),
	 
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, -3),  
	BPF_ST_MEM(BPF_B, BPF_REG_10, -256, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_XDP,
	 
	.result = ACCEPT,
},
{
	"calls: stack depth check using three frames. test3",
	.insns = {
	 
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 6),  
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_6),
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 8),  
	BPF_JMP_IMM(BPF_JGE, BPF_REG_6, 0, 1),
	BPF_ST_MEM(BPF_B, BPF_REG_10, -64, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	 
	BPF_JMP_IMM(BPF_JLT, BPF_REG_1, 10, 1),
	BPF_EXIT_INSN(),
	BPF_ST_MEM(BPF_B, BPF_REG_10, -224, 0),
	BPF_JMP_IMM(BPF_JA, 0, 0, -3),
	 
	BPF_JMP_IMM(BPF_JGT, BPF_REG_1, 2, 1),
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, -6),  
	BPF_ST_MEM(BPF_B, BPF_REG_10, -256, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_XDP,
	 
	.errstr = "combined stack",
	.result = REJECT,
},
{
	"calls: stack depth check using three frames. test4",
	 
	.insns = {
	 
	BPF_MOV64_IMM(BPF_REG_1, 0),
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 6),  
	BPF_MOV64_IMM(BPF_REG_1, 1),
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 4),  
	BPF_MOV64_IMM(BPF_REG_1, 1),
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 7),  
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	 
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_1, 0, 2),
	BPF_ST_MEM(BPF_B, BPF_REG_10, -300, 0),
	BPF_EXIT_INSN(),
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 1),  
	BPF_EXIT_INSN(),
	 
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_1, 0, 1),
	BPF_ST_MEM(BPF_B, BPF_REG_10, -300, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_XDP,
	.result = REJECT,
	.errstr = "combined stack",
},
{
	"calls: stack depth check using three frames. test5",
	.insns = {
	 
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 1),  
	BPF_EXIT_INSN(),
	 
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 1),  
	BPF_EXIT_INSN(),
	 
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 1),  
	BPF_EXIT_INSN(),
	 
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 1),  
	BPF_EXIT_INSN(),
	 
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 1),  
	BPF_EXIT_INSN(),
	 
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 1),  
	BPF_EXIT_INSN(),
	 
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 1),  
	BPF_EXIT_INSN(),
	 
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 1),  
	BPF_EXIT_INSN(),
	 
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_XDP,
	.errstr = "call stack",
	.result = REJECT,
},
{
	"calls: stack depth check in dead code",
	.insns = {
	 
	BPF_MOV64_IMM(BPF_REG_1, 0),
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 1),  
	BPF_EXIT_INSN(),
	 
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_1, 0, 1),
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 2),  
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	 
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 1),  
	BPF_EXIT_INSN(),
	 
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 1),  
	BPF_EXIT_INSN(),
	 
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 1),  
	BPF_EXIT_INSN(),
	 
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 1),  
	BPF_EXIT_INSN(),
	 
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 1),  
	BPF_EXIT_INSN(),
	 
	BPF_RAW_INSN(BPF_JMP|BPF_CALL, 0, 1, 0, 1),  
	BPF_EXIT_INSN(),
	 
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_XDP,
	.errstr = "call stack",
	.result = REJECT,
},
{
	"calls: spill into caller stack frame",
	.insns = {
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 0),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),
	BPF_STX_MEM(BPF_DW, BPF_REG_1, BPF_REG_1, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_XDP,
	.errstr = "cannot spill",
	.result = REJECT,
},
{
	"calls: write into caller stack frame",
	.insns = {
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 2),
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_6, 0),
	BPF_EXIT_INSN(),
	BPF_ST_MEM(BPF_DW, BPF_REG_1, 0, 42),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_XDP,
	.result = ACCEPT,
	.retval = 42,
},
{
	"calls: write into callee stack frame",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 2),
	BPF_ST_MEM(BPF_DW, BPF_REG_0, 0, 42),
	BPF_EXIT_INSN(),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, -8),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_XDP,
	.errstr = "cannot return stack pointer",
	.result = REJECT,
},
{
	"calls: two calls with stack write and void return",
	.insns = {
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 0),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -16),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 2),
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_10, -16),
	BPF_EXIT_INSN(),

	 
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_MOV64_REG(BPF_REG_7, BPF_REG_2),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 3),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_7),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),

	 
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_1, 0, 0),
	BPF_EXIT_INSN(),  
	},
	.prog_type = BPF_PROG_TYPE_XDP,
	.result = ACCEPT,
},
{
	"calls: ambiguous return value",
	.insns = {
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 5),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_0),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_6),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 2),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_0),
	BPF_EXIT_INSN(),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_1, 0, 1),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.errstr_unpriv = "allowed for",
	.result_unpriv = REJECT,
	.errstr = "R0 !read_ok",
	.result = REJECT,
},
{
	"calls: two calls that return map_value",
	.insns = {
	 
	 
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -16),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 8),

	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_10, -8),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 1),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_0, 0, 0),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_10, -16),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 1),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_0, 0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),

	 
	 
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_MOV64_REG(BPF_REG_7, BPF_REG_2),
	 
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 3),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_7),
	 
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),

	 
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 0),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -8),
	BPF_LD_MAP_FD(BPF_REG_1, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_6, BPF_REG_0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),  
	},
	.prog_type = BPF_PROG_TYPE_XDP,
	.fixup_map_hash_8b = { 23 },
	.result = ACCEPT,
},
{
	"calls: two calls that return map_value with bool condition",
	.insns = {
	 
	 
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -16),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 2),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),

	 
	 
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_MOV64_REG(BPF_REG_7, BPF_REG_2),
	 
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 9),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 1, 2),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_6, 0),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_0, 0, 0),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_7),
	 
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 4),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 1, 2),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_7, 0),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_0, 0, 0),
	BPF_EXIT_INSN(),

	 
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 0),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -8),
	BPF_LD_MAP_FD(BPF_REG_1, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 2),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),  
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_6, BPF_REG_0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_EXIT_INSN(),  
	},
	.prog_type = BPF_PROG_TYPE_XDP,
	.fixup_map_hash_8b = { 23 },
	.result = ACCEPT,
},
{
	"calls: two calls that return map_value with incorrect bool check",
	.insns = {
	 
	 
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -16),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 2),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),

	 
	 
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_MOV64_REG(BPF_REG_7, BPF_REG_2),
	 
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 9),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 1, 2),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_6, 0),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_0, 0, 0),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_7),
	 
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 4),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 2),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_7, 0),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_0, 0, 0),
	BPF_EXIT_INSN(),

	 
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 0),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -8),
	BPF_LD_MAP_FD(BPF_REG_1, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 2),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),  
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_6, BPF_REG_0, 0),
	BPF_MOV64_IMM(BPF_REG_0, 1),
	BPF_EXIT_INSN(),  
	},
	.prog_type = BPF_PROG_TYPE_XDP,
	.fixup_map_hash_8b = { 23 },
	.result = REJECT,
	.errstr = "R0 invalid mem access 'scalar'",
	.result_unpriv = REJECT,
	.errstr_unpriv = "invalid read from stack R7 off=-16 size=8",
},
{
	"calls: two calls that receive map_value via arg=ptr_stack_of_caller. test1",
	.insns = {
	 
	 
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -16),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 2),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),

	 
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_MOV64_REG(BPF_REG_7, BPF_REG_2),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 0),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -8),
	BPF_LD_MAP_FD(BPF_REG_1, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 2),
	BPF_MOV64_IMM(BPF_REG_8, 0),
	BPF_JMP_IMM(BPF_JA, 0, 0, 2),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_6, BPF_REG_0, 0),
	BPF_MOV64_IMM(BPF_REG_8, 1),

	 
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),  
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -8),
	BPF_LD_MAP_FD(BPF_REG_1, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0,  
		     BPF_FUNC_map_lookup_elem),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 2),
	BPF_MOV64_IMM(BPF_REG_9, 0),
	BPF_JMP_IMM(BPF_JA, 0, 0, 2),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_7, BPF_REG_0, 0),
	BPF_MOV64_IMM(BPF_REG_9, 1),

	 
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_6),  
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_8),
	BPF_MOV64_REG(BPF_REG_3, BPF_REG_7),
	BPF_MOV64_REG(BPF_REG_4, BPF_REG_9),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),   
	BPF_EXIT_INSN(),

	 
	 
	BPF_JMP_IMM(BPF_JNE, BPF_REG_2, 1, 2),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_1, 0),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_0, 0, 0),

	 
	BPF_JMP_IMM(BPF_JNE, BPF_REG_4, 1, 2),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_3, 0),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_0, 2, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.fixup_map_hash_8b = { 12, 22 },
	.result = REJECT,
	.errstr = "invalid access to map value, value_size=8 off=2 size=8",
	.flags = F_NEEDS_EFFICIENT_UNALIGNED_ACCESS,
},
{
	"calls: two calls that receive map_value via arg=ptr_stack_of_caller. test2",
	.insns = {
	 
	 
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -16),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 2),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),

	 
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_MOV64_REG(BPF_REG_7, BPF_REG_2),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 0),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -8),
	BPF_LD_MAP_FD(BPF_REG_1, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 2),
	BPF_MOV64_IMM(BPF_REG_8, 0),
	BPF_JMP_IMM(BPF_JA, 0, 0, 2),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_6, BPF_REG_0, 0),
	BPF_MOV64_IMM(BPF_REG_8, 1),

	 
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),  
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -8),
	BPF_LD_MAP_FD(BPF_REG_1, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0,  
		     BPF_FUNC_map_lookup_elem),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 2),
	BPF_MOV64_IMM(BPF_REG_9, 0),
	BPF_JMP_IMM(BPF_JA, 0, 0, 2),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_7, BPF_REG_0, 0),
	BPF_MOV64_IMM(BPF_REG_9, 1),

	 
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_6),  
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_8),
	BPF_MOV64_REG(BPF_REG_3, BPF_REG_7),
	BPF_MOV64_REG(BPF_REG_4, BPF_REG_9),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),   
	BPF_EXIT_INSN(),

	 
	 
	BPF_JMP_IMM(BPF_JNE, BPF_REG_2, 1, 2),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_1, 0),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_0, 0, 0),

	 
	BPF_JMP_IMM(BPF_JNE, BPF_REG_4, 1, 2),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_3, 0),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_0, 0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.fixup_map_hash_8b = { 12, 22 },
	.result = ACCEPT,
},
{
	"calls: two jumps that receive map_value via arg=ptr_stack_of_jumper. test3",
	.insns = {
	 
	 
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -16),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_1, 0, 2),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),

	 
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_MOV64_REG(BPF_REG_7, BPF_REG_2),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -24, 0),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -24),
	BPF_LD_MAP_FD(BPF_REG_1, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 2),
	BPF_MOV64_IMM(BPF_REG_8, 0),
	BPF_JMP_IMM(BPF_JA, 0, 0, 2),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_6, BPF_REG_0, 0),
	BPF_MOV64_IMM(BPF_REG_8, 1),

	 
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -24),
	BPF_LD_MAP_FD(BPF_REG_1, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 2),
	BPF_MOV64_IMM(BPF_REG_9, 0),  
	BPF_JMP_IMM(BPF_JA, 0, 0, 2),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_7, BPF_REG_0, 0),
	BPF_MOV64_IMM(BPF_REG_9, 1),

	 
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_6), 
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_8),
	BPF_MOV64_REG(BPF_REG_3, BPF_REG_7),
	BPF_MOV64_REG(BPF_REG_4, BPF_REG_9),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_1, 0, 1), 
	BPF_JMP_IMM(BPF_JA, 0, 0, -30),

	 
	 
	BPF_JMP_IMM(BPF_JNE, BPF_REG_2, 1, 2),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_1, 0),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_0, 0, 0),

	 
	BPF_JMP_IMM(BPF_JNE, BPF_REG_4, 1, 2),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_3, 0),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_0, 2, 0),
	BPF_JMP_IMM(BPF_JA, 0, 0, -8),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.fixup_map_hash_8b = { 12, 22 },
	.result = REJECT,
	.errstr = "invalid access to map value, value_size=8 off=2 size=8",
	.flags = F_NEEDS_EFFICIENT_UNALIGNED_ACCESS,
},
{
	"calls: two calls that receive map_value_ptr_or_null via arg. test1",
	.insns = {
	 
	 
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -16),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 2),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),

	 
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_MOV64_REG(BPF_REG_7, BPF_REG_2),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 0),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -8),
	BPF_LD_MAP_FD(BPF_REG_1, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_6, BPF_REG_0, 0),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 2),
	BPF_MOV64_IMM(BPF_REG_8, 0),
	BPF_JMP_IMM(BPF_JA, 0, 0, 1),
	BPF_MOV64_IMM(BPF_REG_8, 1),

	 
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -8),
	BPF_LD_MAP_FD(BPF_REG_1, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_7, BPF_REG_0, 0),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 2),
	BPF_MOV64_IMM(BPF_REG_9, 0),
	BPF_JMP_IMM(BPF_JA, 0, 0, 1),
	BPF_MOV64_IMM(BPF_REG_9, 1),

	 
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_6),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_8),
	BPF_MOV64_REG(BPF_REG_3, BPF_REG_7),
	BPF_MOV64_REG(BPF_REG_4, BPF_REG_9),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),

	 
	 
	BPF_JMP_IMM(BPF_JNE, BPF_REG_2, 1, 2),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_1, 0),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_0, 0, 0),

	 
	BPF_JMP_IMM(BPF_JNE, BPF_REG_4, 1, 2),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_3, 0),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_0, 0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.fixup_map_hash_8b = { 12, 22 },
	.result = ACCEPT,
},
{
	"calls: two calls that receive map_value_ptr_or_null via arg. test2",
	.insns = {
	 
	 
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -8),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -16),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 2),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),

	 
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_MOV64_REG(BPF_REG_7, BPF_REG_2),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 0),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -8),
	BPF_LD_MAP_FD(BPF_REG_1, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_6, BPF_REG_0, 0),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 2),
	BPF_MOV64_IMM(BPF_REG_8, 0),
	BPF_JMP_IMM(BPF_JA, 0, 0, 1),
	BPF_MOV64_IMM(BPF_REG_8, 1),

	 
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -8),
	BPF_LD_MAP_FD(BPF_REG_1, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_7, BPF_REG_0, 0),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 2),
	BPF_MOV64_IMM(BPF_REG_9, 0),
	BPF_JMP_IMM(BPF_JA, 0, 0, 1),
	BPF_MOV64_IMM(BPF_REG_9, 1),

	 
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_6),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_8),
	BPF_MOV64_REG(BPF_REG_3, BPF_REG_7),
	BPF_MOV64_REG(BPF_REG_4, BPF_REG_9),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),

	 
	 
	BPF_JMP_IMM(BPF_JNE, BPF_REG_2, 1, 2),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_1, 0),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_0, 0, 0),

	 
	BPF_JMP_IMM(BPF_JNE, BPF_REG_4, 0, 2),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_3, 0),
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_0, 0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.fixup_map_hash_8b = { 12, 22 },
	.result = REJECT,
	.errstr = "R0 invalid mem access 'scalar'",
},
{
	"calls: pkt_ptr spill into caller stack",
	.insns = {
	BPF_MOV64_REG(BPF_REG_4, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, -8),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 1),
	BPF_EXIT_INSN(),

	 
	BPF_LDX_MEM(BPF_W, BPF_REG_2, BPF_REG_1,
		    offsetof(struct __sk_buff, data)),
	BPF_LDX_MEM(BPF_W, BPF_REG_3, BPF_REG_1,
		    offsetof(struct __sk_buff, data_end)),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_2),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, 8),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_4, BPF_REG_2, 0),
	BPF_JMP_REG(BPF_JGT, BPF_REG_0, BPF_REG_3, 2),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_2, BPF_REG_4, 0),
	 
	BPF_ST_MEM(BPF_W, BPF_REG_2, 0, 0),
	BPF_EXIT_INSN(),
	},
	.result = ACCEPT,
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.retval = POINTER_VALUE,
	.flags = F_NEEDS_EFFICIENT_UNALIGNED_ACCESS,
},
{
	"calls: pkt_ptr spill into caller stack 2",
	.insns = {
	BPF_MOV64_REG(BPF_REG_4, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, -8),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 3),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_4, BPF_REG_10, -8),
	BPF_ST_MEM(BPF_W, BPF_REG_4, 0, 0),
	BPF_EXIT_INSN(),

	 
	BPF_LDX_MEM(BPF_W, BPF_REG_2, BPF_REG_1,
		    offsetof(struct __sk_buff, data)),
	BPF_LDX_MEM(BPF_W, BPF_REG_3, BPF_REG_1,
		    offsetof(struct __sk_buff, data_end)),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_2),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, 8),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_4, BPF_REG_2, 0),
	BPF_JMP_REG(BPF_JGT, BPF_REG_0, BPF_REG_3, 2),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_2, BPF_REG_4, 0),
	 
	BPF_ST_MEM(BPF_W, BPF_REG_2, 0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.errstr = "invalid access to packet",
	.result = REJECT,
	.flags = F_NEEDS_EFFICIENT_UNALIGNED_ACCESS,
},
{
	"calls: pkt_ptr spill into caller stack 3",
	.insns = {
	BPF_MOV64_REG(BPF_REG_4, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, -8),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 4),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 2),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_4, BPF_REG_10, -8),
	BPF_ST_MEM(BPF_W, BPF_REG_4, 0, 0),
	BPF_EXIT_INSN(),

	 
	BPF_LDX_MEM(BPF_W, BPF_REG_2, BPF_REG_1,
		    offsetof(struct __sk_buff, data)),
	BPF_LDX_MEM(BPF_W, BPF_REG_3, BPF_REG_1,
		    offsetof(struct __sk_buff, data_end)),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_2),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, 8),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_4, BPF_REG_2, 0),
	BPF_MOV64_IMM(BPF_REG_5, 0),
	BPF_JMP_REG(BPF_JGT, BPF_REG_0, BPF_REG_3, 3),
	BPF_MOV64_IMM(BPF_REG_5, 1),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_2, BPF_REG_4, 0),
	 
	BPF_ST_MEM(BPF_W, BPF_REG_2, 0, 0),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_5),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.result = ACCEPT,
	.retval = 1,
	.flags = F_NEEDS_EFFICIENT_UNALIGNED_ACCESS,
},
{
	"calls: pkt_ptr spill into caller stack 4",
	.insns = {
	BPF_MOV64_REG(BPF_REG_4, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, -8),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 4),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 2),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_4, BPF_REG_10, -8),
	BPF_ST_MEM(BPF_W, BPF_REG_4, 0, 0),
	BPF_EXIT_INSN(),

	 
	BPF_LDX_MEM(BPF_W, BPF_REG_2, BPF_REG_1,
		    offsetof(struct __sk_buff, data)),
	BPF_LDX_MEM(BPF_W, BPF_REG_3, BPF_REG_1,
		    offsetof(struct __sk_buff, data_end)),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_2),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, 8),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_4, BPF_REG_2, 0),
	BPF_MOV64_IMM(BPF_REG_5, 0),
	BPF_JMP_REG(BPF_JGT, BPF_REG_0, BPF_REG_3, 2),
	BPF_MOV64_IMM(BPF_REG_5, 1),
	 
	 
	BPF_ST_MEM(BPF_W, BPF_REG_2, 0, 0),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_5),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.result = ACCEPT,
	.retval = 1,
	.flags = F_NEEDS_EFFICIENT_UNALIGNED_ACCESS,
},
{
	"calls: pkt_ptr spill into caller stack 5",
	.insns = {
	BPF_MOV64_REG(BPF_REG_4, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, -8),
	BPF_STX_MEM(BPF_DW, BPF_REG_4, BPF_REG_1, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 3),
	BPF_LDX_MEM(BPF_DW, BPF_REG_4, BPF_REG_10, -8),
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_4, 0),
	BPF_EXIT_INSN(),

	 
	BPF_LDX_MEM(BPF_W, BPF_REG_2, BPF_REG_1,
		    offsetof(struct __sk_buff, data)),
	BPF_LDX_MEM(BPF_W, BPF_REG_3, BPF_REG_1,
		    offsetof(struct __sk_buff, data_end)),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_2),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, 8),
	BPF_MOV64_IMM(BPF_REG_5, 0),
	BPF_JMP_REG(BPF_JGT, BPF_REG_0, BPF_REG_3, 3),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_4, BPF_REG_2, 0),
	BPF_MOV64_IMM(BPF_REG_5, 1),
	 
	 
	BPF_ST_MEM(BPF_W, BPF_REG_2, 0, 0),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_5),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.errstr = "same insn cannot be used with different",
	.result = REJECT,
	.flags = F_NEEDS_EFFICIENT_UNALIGNED_ACCESS,
},
{
	"calls: pkt_ptr spill into caller stack 6",
	.insns = {
	BPF_LDX_MEM(BPF_W, BPF_REG_2, BPF_REG_1,
		    offsetof(struct __sk_buff, data_end)),
	BPF_MOV64_REG(BPF_REG_4, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, -8),
	BPF_STX_MEM(BPF_DW, BPF_REG_4, BPF_REG_2, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 3),
	BPF_LDX_MEM(BPF_DW, BPF_REG_4, BPF_REG_10, -8),
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_4, 0),
	BPF_EXIT_INSN(),

	 
	BPF_LDX_MEM(BPF_W, BPF_REG_2, BPF_REG_1,
		    offsetof(struct __sk_buff, data)),
	BPF_LDX_MEM(BPF_W, BPF_REG_3, BPF_REG_1,
		    offsetof(struct __sk_buff, data_end)),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_2),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, 8),
	BPF_MOV64_IMM(BPF_REG_5, 0),
	BPF_JMP_REG(BPF_JGT, BPF_REG_0, BPF_REG_3, 3),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_4, BPF_REG_2, 0),
	BPF_MOV64_IMM(BPF_REG_5, 1),
	 
	 
	BPF_ST_MEM(BPF_W, BPF_REG_2, 0, 0),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_5),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.errstr = "R4 invalid mem access",
	.result = REJECT,
	.flags = F_NEEDS_EFFICIENT_UNALIGNED_ACCESS,
},
{
	"calls: pkt_ptr spill into caller stack 7",
	.insns = {
	BPF_MOV64_IMM(BPF_REG_2, 0),
	BPF_MOV64_REG(BPF_REG_4, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, -8),
	BPF_STX_MEM(BPF_DW, BPF_REG_4, BPF_REG_2, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 3),
	BPF_LDX_MEM(BPF_DW, BPF_REG_4, BPF_REG_10, -8),
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_4, 0),
	BPF_EXIT_INSN(),

	 
	BPF_LDX_MEM(BPF_W, BPF_REG_2, BPF_REG_1,
		    offsetof(struct __sk_buff, data)),
	BPF_LDX_MEM(BPF_W, BPF_REG_3, BPF_REG_1,
		    offsetof(struct __sk_buff, data_end)),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_2),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, 8),
	BPF_MOV64_IMM(BPF_REG_5, 0),
	BPF_JMP_REG(BPF_JGT, BPF_REG_0, BPF_REG_3, 3),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_4, BPF_REG_2, 0),
	BPF_MOV64_IMM(BPF_REG_5, 1),
	 
	 
	BPF_ST_MEM(BPF_W, BPF_REG_2, 0, 0),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_5),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.errstr = "R4 invalid mem access",
	.result = REJECT,
	.flags = F_NEEDS_EFFICIENT_UNALIGNED_ACCESS,
},
{
	"calls: pkt_ptr spill into caller stack 8",
	.insns = {
	BPF_LDX_MEM(BPF_W, BPF_REG_2, BPF_REG_1,
		    offsetof(struct __sk_buff, data)),
	BPF_LDX_MEM(BPF_W, BPF_REG_3, BPF_REG_1,
		    offsetof(struct __sk_buff, data_end)),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_2),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, 8),
	BPF_JMP_REG(BPF_JLE, BPF_REG_0, BPF_REG_3, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_REG(BPF_REG_4, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, -8),
	BPF_STX_MEM(BPF_DW, BPF_REG_4, BPF_REG_2, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 3),
	BPF_LDX_MEM(BPF_DW, BPF_REG_4, BPF_REG_10, -8),
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_4, 0),
	BPF_EXIT_INSN(),

	 
	BPF_LDX_MEM(BPF_W, BPF_REG_2, BPF_REG_1,
		    offsetof(struct __sk_buff, data)),
	BPF_LDX_MEM(BPF_W, BPF_REG_3, BPF_REG_1,
		    offsetof(struct __sk_buff, data_end)),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_2),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, 8),
	BPF_MOV64_IMM(BPF_REG_5, 0),
	BPF_JMP_REG(BPF_JGT, BPF_REG_0, BPF_REG_3, 3),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_4, BPF_REG_2, 0),
	BPF_MOV64_IMM(BPF_REG_5, 1),
	 
	 
	BPF_ST_MEM(BPF_W, BPF_REG_2, 0, 0),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_5),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.result = ACCEPT,
	.flags = F_NEEDS_EFFICIENT_UNALIGNED_ACCESS,
},
{
	"calls: pkt_ptr spill into caller stack 9",
	.insns = {
	BPF_LDX_MEM(BPF_W, BPF_REG_2, BPF_REG_1,
		    offsetof(struct __sk_buff, data)),
	BPF_LDX_MEM(BPF_W, BPF_REG_3, BPF_REG_1,
		    offsetof(struct __sk_buff, data_end)),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_2),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, 8),
	BPF_JMP_REG(BPF_JLE, BPF_REG_0, BPF_REG_3, 1),
	BPF_EXIT_INSN(),
	BPF_MOV64_REG(BPF_REG_4, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, -8),
	BPF_STX_MEM(BPF_DW, BPF_REG_4, BPF_REG_2, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 3),
	BPF_LDX_MEM(BPF_DW, BPF_REG_4, BPF_REG_10, -8),
	BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_4, 0),
	BPF_EXIT_INSN(),

	 
	BPF_LDX_MEM(BPF_W, BPF_REG_2, BPF_REG_1,
		    offsetof(struct __sk_buff, data)),
	BPF_LDX_MEM(BPF_W, BPF_REG_3, BPF_REG_1,
		    offsetof(struct __sk_buff, data_end)),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_2),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, 8),
	BPF_MOV64_IMM(BPF_REG_5, 0),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_4, BPF_REG_2, 0),
	BPF_JMP_REG(BPF_JGT, BPF_REG_0, BPF_REG_3, 2),
	BPF_MOV64_IMM(BPF_REG_5, 1),
	 
	 
	BPF_ST_MEM(BPF_W, BPF_REG_2, 0, 0),
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_5),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.errstr = "invalid access to packet",
	.result = REJECT,
	.flags = F_NEEDS_EFFICIENT_UNALIGNED_ACCESS,
},
{
	"calls: caller stack init to zero or map_value_or_null",
	.insns = {
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_STX_MEM(BPF_DW, BPF_REG_10, BPF_REG_0, -8),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -8),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 4),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_10, -8),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 1),
	 
	BPF_ST_MEM(BPF_W, BPF_REG_0, 0, 0),
	BPF_EXIT_INSN(),

	 
	 
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_1, 0, 8),
	 
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_2),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -8),
	BPF_LD_MAP_FD(BPF_REG_1, 0),
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
	 
	BPF_STX_MEM(BPF_DW, BPF_REG_6, BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.fixup_map_hash_8b = { 13 },
	.result = ACCEPT,
	.prog_type = BPF_PROG_TYPE_XDP,
},
{
	"calls: stack init to zero and pruning",
	.insns = {
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -16, 0),
	 
	BPF_EMIT_CALL(BPF_FUNC_get_prandom_u32),
	BPF_JMP_IMM(BPF_JGT, BPF_REG_0, 2, 2),
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 0),
	BPF_JMP_IMM(BPF_JA, 0, 0, 0),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -8),
	BPF_LD_MAP_FD(BPF_REG_1, 0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.fixup_map_hash_48b = { 7 },
	.errstr_unpriv = "invalid indirect read from stack R2 off -8+0 size 8",
	.result_unpriv = REJECT,
	 
	.result = ACCEPT,
},
{
	"calls: ctx read at start of subprog",
	.insns = {
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 5),
	BPF_JMP_REG(BPF_JSGT, BPF_REG_0, BPF_REG_0, 0),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_6),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 2),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_0),
	BPF_EXIT_INSN(),
	BPF_LDX_MEM(BPF_B, BPF_REG_9, BPF_REG_1, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SOCKET_FILTER,
	.errstr_unpriv = "loading/calling other bpf or kernel functions are allowed for",
	.result_unpriv = REJECT,
	.result = ACCEPT,
},
{
	"calls: cross frame pruning",
	.insns = {
	 
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_get_prandom_u32),
	BPF_MOV64_IMM(BPF_REG_8, 0),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 1),
	BPF_MOV64_IMM(BPF_REG_8, 1),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_8),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 4),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_8, 1, 1),
	BPF_LDX_MEM(BPF_B, BPF_REG_9, BPF_REG_1, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_1, 0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SOCKET_FILTER,
	.errstr_unpriv = "loading/calling other bpf or kernel functions are allowed for",
	.errstr = "!read_ok",
	.result = REJECT,
},
{
	"calls: cross frame pruning - liveness propagation",
	.insns = {
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_get_prandom_u32),
	BPF_MOV64_IMM(BPF_REG_8, 0),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 1),
	BPF_MOV64_IMM(BPF_REG_8, 1),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_get_prandom_u32),
	BPF_MOV64_IMM(BPF_REG_9, 0),
	BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 1),
	BPF_MOV64_IMM(BPF_REG_9, 1),
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_0),
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 1, 0, 4),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_8, 1, 1),
	BPF_LDX_MEM(BPF_B, BPF_REG_1, BPF_REG_2, 0),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_1, 0, 0),
	BPF_EXIT_INSN(),
	},
	.prog_type = BPF_PROG_TYPE_SOCKET_FILTER,
	.errstr_unpriv = "loading/calling other bpf or kernel functions are allowed for",
	.errstr = "!read_ok",
	.result = REJECT,
},
 
{
	"calls: check_ids() across call boundary",
	.insns = {
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 0),
	 
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -8),
	BPF_LD_MAP_FD(BPF_REG_1,
		      0),
	BPF_EMIT_CALL(BPF_FUNC_map_lookup_elem),
	BPF_STX_MEM(BPF_DW, BPF_REG_FP, BPF_REG_0, -24),
	 
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -8),
	BPF_LD_MAP_FD(BPF_REG_1,
		      0),
	BPF_EMIT_CALL(BPF_FUNC_map_lookup_elem),
	BPF_STX_MEM(BPF_DW, BPF_REG_FP, BPF_REG_0, -32),
	 
	BPF_MOV64_REG(BPF_REG_1, BPF_REG_FP),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, -24),
	BPF_MOV64_REG(BPF_REG_2, BPF_REG_FP),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -32),
	BPF_CALL_REL(2),
	 
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	 
	BPF_MOV64_REG(BPF_REG_9, BPF_REG_1),
	BPF_MOV64_REG(BPF_REG_8, BPF_REG_2),
	 
	BPF_EMIT_CALL(BPF_FUNC_ktime_get_ns),
	BPF_MOV64_REG(BPF_REG_7, BPF_REG_0),
	 
	BPF_EMIT_CALL(BPF_FUNC_ktime_get_ns),
	BPF_MOV64_REG(BPF_REG_6, BPF_REG_0),
	 
	BPF_JMP_REG(BPF_JGT, BPF_REG_6, BPF_REG_7, 1),
	BPF_MOV64_REG(BPF_REG_9, BPF_REG_8),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_9, BPF_REG_9, 0),
	BPF_JMP_IMM(BPF_JEQ, BPF_REG_9, 0, 1),
	 
	BPF_LDX_MEM(BPF_DW, BPF_REG_8, BPF_REG_8, 0),
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_8, 0),
	 
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	},
	.flags = BPF_F_TEST_STATE_FREQ,
	.fixup_map_hash_8b = { 3, 9 },
	.result = REJECT,
	.errstr = "R8 invalid mem access 'map_value_or_null'",
	.result_unpriv = REJECT,
	.errstr_unpriv = "",
	.prog_type = BPF_PROG_TYPE_CGROUP_SKB,
},
