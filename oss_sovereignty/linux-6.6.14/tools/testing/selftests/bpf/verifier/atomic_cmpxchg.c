{
	"atomic compare-and-exchange smoketest - 64bit",
	.insns = {
		 
		BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 3),
		 
		BPF_MOV64_IMM(BPF_REG_1, 4),
		BPF_MOV64_IMM(BPF_REG_0, 2),
		BPF_ATOMIC_OP(BPF_DW, BPF_CMPXCHG, BPF_REG_10, BPF_REG_1, -8),
		 
		BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 3, 2),
		BPF_MOV64_IMM(BPF_REG_0, 2),
		BPF_EXIT_INSN(),
		 
		BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_10, -8),
		BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 3, 2),
		BPF_MOV64_IMM(BPF_REG_0, 3),
		BPF_EXIT_INSN(),
		 
		BPF_MOV64_IMM(BPF_REG_1, 4),
		BPF_MOV64_IMM(BPF_REG_0, 3),
		BPF_ATOMIC_OP(BPF_DW, BPF_CMPXCHG, BPF_REG_10, BPF_REG_1, -8),
		 
		BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 3, 2),
		BPF_MOV64_IMM(BPF_REG_0, 4),
		BPF_EXIT_INSN(),
		 
		BPF_LDX_MEM(BPF_DW, BPF_REG_0, BPF_REG_10, -8),
		BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 4, 2),
		BPF_MOV64_IMM(BPF_REG_0, 5),
		BPF_EXIT_INSN(),
		 
		BPF_MOV64_IMM(BPF_REG_0, 0),
		BPF_EXIT_INSN(),
	},
	.result = ACCEPT,
},
{
	"atomic compare-and-exchange smoketest - 32bit",
	.insns = {
		 
		BPF_ST_MEM(BPF_W, BPF_REG_10, -4, 3),
		 
		BPF_MOV32_IMM(BPF_REG_1, 4),
		BPF_MOV32_IMM(BPF_REG_0, 2),
		BPF_ATOMIC_OP(BPF_W, BPF_CMPXCHG, BPF_REG_10, BPF_REG_1, -4),
		 
		BPF_JMP32_IMM(BPF_JEQ, BPF_REG_0, 3, 2),
		BPF_MOV32_IMM(BPF_REG_0, 2),
		BPF_EXIT_INSN(),
		 
		BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_10, -4),
		BPF_JMP32_IMM(BPF_JEQ, BPF_REG_0, 3, 2),
		BPF_MOV32_IMM(BPF_REG_0, 3),
		BPF_EXIT_INSN(),
		 
		BPF_MOV32_IMM(BPF_REG_1, 4),
		BPF_MOV32_IMM(BPF_REG_0, 3),
		BPF_ATOMIC_OP(BPF_W, BPF_CMPXCHG, BPF_REG_10, BPF_REG_1, -4),
		 
		BPF_JMP32_IMM(BPF_JEQ, BPF_REG_0, 3, 2),
		BPF_MOV32_IMM(BPF_REG_0, 4),
		BPF_EXIT_INSN(),
		 
		BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_10, -4),
		BPF_JMP32_IMM(BPF_JEQ, BPF_REG_0, 4, 2),
		BPF_MOV32_IMM(BPF_REG_0, 5),
		BPF_EXIT_INSN(),
		 
		BPF_MOV32_IMM(BPF_REG_0, 0),
		BPF_EXIT_INSN(),
	},
	.result = ACCEPT,
},
{
	"Can't use cmpxchg on uninit src reg",
	.insns = {
		BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 3),
		BPF_MOV64_IMM(BPF_REG_0, 3),
		BPF_ATOMIC_OP(BPF_DW, BPF_CMPXCHG, BPF_REG_10, BPF_REG_2, -8),
		BPF_EXIT_INSN(),
	},
	.result = REJECT,
	.errstr = "!read_ok",
},
{
	"BPF_W cmpxchg should zero top 32 bits",
	.insns = {
		 
		BPF_MOV64_IMM(BPF_REG_0, 0),
		BPF_ALU64_IMM(BPF_SUB, BPF_REG_0, 1),
		 
		BPF_STX_MEM(BPF_DW, BPF_REG_10, BPF_REG_0, -8),
		 
		BPF_MOV32_IMM(BPF_REG_1, 1),
		BPF_ATOMIC_OP(BPF_W, BPF_CMPXCHG, BPF_REG_10, BPF_REG_1, -8),
		 
		BPF_MOV64_IMM(BPF_REG_1, 1),
		BPF_ALU64_IMM(BPF_LSH, BPF_REG_1, 32),
		BPF_ALU64_IMM(BPF_SUB, BPF_REG_1, 1),
		 
		BPF_JMP_REG(BPF_JEQ, BPF_REG_0, BPF_REG_1, 2),
		BPF_MOV32_IMM(BPF_REG_0, 1),
		BPF_EXIT_INSN(),
		 
		BPF_MOV32_IMM(BPF_REG_0, 0),
		BPF_EXIT_INSN(),
	},
	.result = ACCEPT,
},
{
	"Dest pointer in r0 - fail",
	.insns = {
		 
		BPF_ST_MEM(BPF_DW, BPF_REG_10, -8, 0),
		 
		BPF_MOV64_REG(BPF_REG_0, BPF_REG_10),
		 
		BPF_MOV64_IMM(BPF_REG_1, 1),
		BPF_ATOMIC_OP(BPF_DW, BPF_CMPXCHG, BPF_REG_10, BPF_REG_1, -8),
		 
		BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 2),
		BPF_MOV64_IMM(BPF_REG_0, 1),
		BPF_EXIT_INSN(),
		 
		BPF_MOV64_IMM(BPF_REG_0, 0),
		BPF_EXIT_INSN(),
	},
	.result = ACCEPT,
	.result_unpriv = REJECT,
	.errstr_unpriv = "R0 leaks addr into mem",
},
{
	"Dest pointer in r0 - succeed",
	.insns = {
		 
		BPF_MOV64_REG(BPF_REG_0, BPF_REG_10),
		 
		BPF_STX_MEM(BPF_DW, BPF_REG_10, BPF_REG_0, -8),
		 
		BPF_MOV64_IMM(BPF_REG_1, 0),
		BPF_ATOMIC_OP(BPF_DW, BPF_CMPXCHG, BPF_REG_10, BPF_REG_1, -8),
		 
		BPF_LDX_MEM(BPF_DW, BPF_REG_1, BPF_REG_0, -8),
		 
		BPF_MOV64_IMM(BPF_REG_0, 0),
		BPF_EXIT_INSN(),
	},
	.result = ACCEPT,
	.result_unpriv = REJECT,
	.errstr_unpriv = "R0 leaks addr into mem",
},
{
	"Dest pointer in r0 - succeed, check 2",
	.insns = {
		 
		BPF_MOV64_REG(BPF_REG_0, BPF_REG_10),
		 
		BPF_STX_MEM(BPF_DW, BPF_REG_10, BPF_REG_0, -8),
		 
		BPF_MOV64_REG(BPF_REG_5, BPF_REG_10),
		 
		BPF_ATOMIC_OP(BPF_DW, BPF_CMPXCHG, BPF_REG_10, BPF_REG_5, -8),
		 
		BPF_LDX_MEM(BPF_DW, BPF_REG_1, BPF_REG_0, -8),
		 
		BPF_MOV64_IMM(BPF_REG_0, 0),
		BPF_EXIT_INSN(),
	},
	.result = ACCEPT,
	.result_unpriv = REJECT,
	.errstr_unpriv = "R0 leaks addr into mem",
},
{
	"Dest pointer in r0 - succeed, check 3",
	.insns = {
		 
		BPF_MOV64_REG(BPF_REG_0, BPF_REG_10),
		 
		BPF_STX_MEM(BPF_DW, BPF_REG_10, BPF_REG_0, -8),
		 
		BPF_MOV64_REG(BPF_REG_5, BPF_REG_10),
		 
		BPF_ATOMIC_OP(BPF_W, BPF_CMPXCHG, BPF_REG_10, BPF_REG_5, -8),
		 
		BPF_MOV64_IMM(BPF_REG_0, 0),
		BPF_EXIT_INSN(),
	},
	.result = REJECT,
	.errstr = "invalid size of register fill",
	.errstr_unpriv = "R0 leaks addr into mem",
},
{
	"Dest pointer in r0 - succeed, check 4",
	.insns = {
		 
		BPF_MOV32_REG(BPF_REG_0, BPF_REG_10),
		 
		BPF_STX_MEM(BPF_W, BPF_REG_10, BPF_REG_0, -8),
		 
		BPF_MOV32_REG(BPF_REG_5, BPF_REG_10),
		 
		BPF_ATOMIC_OP(BPF_W, BPF_CMPXCHG, BPF_REG_10, BPF_REG_5, -8),
		 
		BPF_LDX_MEM(BPF_W, BPF_REG_1, BPF_REG_10, -8),
		 
		BPF_MOV64_IMM(BPF_REG_0, 0),
		BPF_EXIT_INSN(),
	},
	.result = ACCEPT,
	.result_unpriv = REJECT,
	.errstr_unpriv = "R10 partial copy of pointer",
},
{
	"Dest pointer in r0 - succeed, check 5",
	.insns = {
		 
		BPF_MOV32_REG(BPF_REG_0, BPF_REG_10),
		 
		BPF_STX_MEM(BPF_W, BPF_REG_10, BPF_REG_0, -8),
		 
		BPF_MOV32_REG(BPF_REG_5, BPF_REG_10),
		 
		BPF_ATOMIC_OP(BPF_W, BPF_CMPXCHG, BPF_REG_10, BPF_REG_5, -8),
		 
		BPF_LDX_MEM(BPF_W, BPF_REG_1, BPF_REG_0, -8),
		 
		BPF_MOV64_IMM(BPF_REG_0, 0),
		BPF_EXIT_INSN(),
	},
	.result = REJECT,
	.errstr = "R0 invalid mem access",
	.errstr_unpriv = "R10 partial copy of pointer",
	.flags = F_NEEDS_EFFICIENT_UNALIGNED_ACCESS,
},
