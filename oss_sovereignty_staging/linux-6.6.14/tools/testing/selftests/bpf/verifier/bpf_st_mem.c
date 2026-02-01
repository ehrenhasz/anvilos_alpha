{
	"BPF_ST_MEM stack imm non-zero",
	.insns = {
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 42),
	BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_10, -8),
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, -42),
	 
	BPF_EXIT_INSN(),
	},
	.result = ACCEPT,
	 
	.prog_type = BPF_PROG_TYPE_SK_LOOKUP,
	.expected_attach_type = BPF_SK_LOOKUP,
	.runs = -1,
},
{
	"BPF_ST_MEM stack imm zero",
	.insns = {
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 0),
	 
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_LDX_MEM(BPF_B, BPF_REG_1, BPF_REG_10, -8),
	BPF_ALU64_REG(BPF_ADD, BPF_REG_0, BPF_REG_1),
	BPF_LDX_MEM(BPF_B, BPF_REG_1, BPF_REG_10, -4),
	BPF_ALU64_REG(BPF_ADD, BPF_REG_0, BPF_REG_1),
	BPF_LDX_MEM(BPF_B, BPF_REG_1, BPF_REG_10, -1),
	BPF_ALU64_REG(BPF_ADD, BPF_REG_0, BPF_REG_1),
	 
	BPF_EXIT_INSN(),
	},
	.result = ACCEPT,
	 
	.prog_type = BPF_PROG_TYPE_SK_LOOKUP,
	.expected_attach_type = BPF_SK_LOOKUP,
	.runs = -1,
},
{
	"BPF_ST_MEM stack imm zero, variable offset",
	.insns = {
	 
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -16, 0),
	BPF_ST_MEM(BPF_DW, BPF_REG_10, -24, 0),
	 
	BPF_EMIT_CALL(BPF_FUNC_get_prandom_u32),
	BPF_JMP_IMM(BPF_JLE, BPF_REG_0, 16, 2),
	BPF_MOV64_IMM(BPF_REG_0, 0),
	BPF_EXIT_INSN(),
	BPF_ALU64_IMM(BPF_SUB, BPF_REG_0, 32),
	 
	BPF_ALU64_REG(BPF_ADD, BPF_REG_0, BPF_REG_10),
	BPF_ST_MEM(BPF_B, BPF_REG_0, 0, 0),
	 
	BPF_LDX_MEM(BPF_B, BPF_REG_0, BPF_REG_10, -20),
	 
	BPF_EXIT_INSN(),
	},
	.result = ACCEPT,
	 
	.prog_type = BPF_PROG_TYPE_SK_LOOKUP,
	.expected_attach_type = BPF_SK_LOOKUP,
	.runs = -1,
},
