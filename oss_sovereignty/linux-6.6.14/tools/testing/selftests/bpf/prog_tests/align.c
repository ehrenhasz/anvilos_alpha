
#include <test_progs.h>

#define MAX_INSNS	512
#define MAX_MATCHES	24

struct bpf_reg_match {
	unsigned int line;
	const char *match;
};

struct bpf_align_test {
	const char *descr;
	struct bpf_insn	insns[MAX_INSNS];
	enum {
		UNDEF,
		ACCEPT,
		REJECT
	} result;
	enum bpf_prog_type prog_type;
	 
	struct bpf_reg_match matches[MAX_MATCHES];
};

static struct bpf_align_test tests[] = {
	 
	{
		.descr = "mov",
		.insns = {
			BPF_MOV64_IMM(BPF_REG_3, 2),
			BPF_MOV64_IMM(BPF_REG_3, 4),
			BPF_MOV64_IMM(BPF_REG_3, 8),
			BPF_MOV64_IMM(BPF_REG_3, 16),
			BPF_MOV64_IMM(BPF_REG_3, 32),
			BPF_MOV64_IMM(BPF_REG_0, 0),
			BPF_EXIT_INSN(),
		},
		.prog_type = BPF_PROG_TYPE_SCHED_CLS,
		.matches = {
			{0, "R1=ctx(off=0,imm=0)"},
			{0, "R10=fp0"},
			{0, "R3_w=2"},
			{1, "R3_w=4"},
			{2, "R3_w=8"},
			{3, "R3_w=16"},
			{4, "R3_w=32"},
		},
	},
	{
		.descr = "shift",
		.insns = {
			BPF_MOV64_IMM(BPF_REG_3, 1),
			BPF_ALU64_IMM(BPF_LSH, BPF_REG_3, 1),
			BPF_ALU64_IMM(BPF_LSH, BPF_REG_3, 1),
			BPF_ALU64_IMM(BPF_LSH, BPF_REG_3, 1),
			BPF_ALU64_IMM(BPF_LSH, BPF_REG_3, 1),
			BPF_ALU64_IMM(BPF_RSH, BPF_REG_3, 4),
			BPF_MOV64_IMM(BPF_REG_4, 32),
			BPF_ALU64_IMM(BPF_RSH, BPF_REG_4, 1),
			BPF_ALU64_IMM(BPF_RSH, BPF_REG_4, 1),
			BPF_ALU64_IMM(BPF_RSH, BPF_REG_4, 1),
			BPF_ALU64_IMM(BPF_RSH, BPF_REG_4, 1),
			BPF_MOV64_IMM(BPF_REG_0, 0),
			BPF_EXIT_INSN(),
		},
		.prog_type = BPF_PROG_TYPE_SCHED_CLS,
		.matches = {
			{0, "R1=ctx(off=0,imm=0)"},
			{0, "R10=fp0"},
			{0, "R3_w=1"},
			{1, "R3_w=2"},
			{2, "R3_w=4"},
			{3, "R3_w=8"},
			{4, "R3_w=16"},
			{5, "R3_w=1"},
			{6, "R4_w=32"},
			{7, "R4_w=16"},
			{8, "R4_w=8"},
			{9, "R4_w=4"},
			{10, "R4_w=2"},
		},
	},
	{
		.descr = "addsub",
		.insns = {
			BPF_MOV64_IMM(BPF_REG_3, 4),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_3, 4),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_3, 2),
			BPF_MOV64_IMM(BPF_REG_4, 8),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, 4),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, 2),
			BPF_MOV64_IMM(BPF_REG_0, 0),
			BPF_EXIT_INSN(),
		},
		.prog_type = BPF_PROG_TYPE_SCHED_CLS,
		.matches = {
			{0, "R1=ctx(off=0,imm=0)"},
			{0, "R10=fp0"},
			{0, "R3_w=4"},
			{1, "R3_w=8"},
			{2, "R3_w=10"},
			{3, "R4_w=8"},
			{4, "R4_w=12"},
			{5, "R4_w=14"},
		},
	},
	{
		.descr = "mul",
		.insns = {
			BPF_MOV64_IMM(BPF_REG_3, 7),
			BPF_ALU64_IMM(BPF_MUL, BPF_REG_3, 1),
			BPF_ALU64_IMM(BPF_MUL, BPF_REG_3, 2),
			BPF_ALU64_IMM(BPF_MUL, BPF_REG_3, 4),
			BPF_MOV64_IMM(BPF_REG_0, 0),
			BPF_EXIT_INSN(),
		},
		.prog_type = BPF_PROG_TYPE_SCHED_CLS,
		.matches = {
			{0, "R1=ctx(off=0,imm=0)"},
			{0, "R10=fp0"},
			{0, "R3_w=7"},
			{1, "R3_w=7"},
			{2, "R3_w=14"},
			{3, "R3_w=56"},
		},
	},

	 
#define PREP_PKT_POINTERS \
	BPF_LDX_MEM(BPF_W, BPF_REG_2, BPF_REG_1, \
		    offsetof(struct __sk_buff, data)), \
	BPF_LDX_MEM(BPF_W, BPF_REG_3, BPF_REG_1, \
		    offsetof(struct __sk_buff, data_end))

#define LOAD_UNKNOWN(DST_REG) \
	PREP_PKT_POINTERS, \
	BPF_MOV64_REG(BPF_REG_0, BPF_REG_2), \
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, 8), \
	BPF_JMP_REG(BPF_JGE, BPF_REG_3, BPF_REG_0, 1), \
	BPF_EXIT_INSN(), \
	BPF_LDX_MEM(BPF_B, DST_REG, BPF_REG_2, 0)

	{
		.descr = "unknown shift",
		.insns = {
			LOAD_UNKNOWN(BPF_REG_3),
			BPF_ALU64_IMM(BPF_LSH, BPF_REG_3, 1),
			BPF_ALU64_IMM(BPF_LSH, BPF_REG_3, 1),
			BPF_ALU64_IMM(BPF_LSH, BPF_REG_3, 1),
			BPF_ALU64_IMM(BPF_LSH, BPF_REG_3, 1),
			LOAD_UNKNOWN(BPF_REG_4),
			BPF_ALU64_IMM(BPF_LSH, BPF_REG_4, 5),
			BPF_ALU64_IMM(BPF_RSH, BPF_REG_4, 1),
			BPF_ALU64_IMM(BPF_RSH, BPF_REG_4, 1),
			BPF_ALU64_IMM(BPF_RSH, BPF_REG_4, 1),
			BPF_ALU64_IMM(BPF_RSH, BPF_REG_4, 1),
			BPF_MOV64_IMM(BPF_REG_0, 0),
			BPF_EXIT_INSN(),
		},
		.prog_type = BPF_PROG_TYPE_SCHED_CLS,
		.matches = {
			{6, "R0_w=pkt(off=8,r=8,imm=0)"},
			{6, "R3_w=scalar(umax=255,var_off=(0x0; 0xff))"},
			{7, "R3_w=scalar(umax=510,var_off=(0x0; 0x1fe))"},
			{8, "R3_w=scalar(umax=1020,var_off=(0x0; 0x3fc))"},
			{9, "R3_w=scalar(umax=2040,var_off=(0x0; 0x7f8))"},
			{10, "R3_w=scalar(umax=4080,var_off=(0x0; 0xff0))"},
			{12, "R3_w=pkt_end(off=0,imm=0)"},
			{17, "R4_w=scalar(umax=255,var_off=(0x0; 0xff))"},
			{18, "R4_w=scalar(umax=8160,var_off=(0x0; 0x1fe0))"},
			{19, "R4_w=scalar(umax=4080,var_off=(0x0; 0xff0))"},
			{20, "R4_w=scalar(umax=2040,var_off=(0x0; 0x7f8))"},
			{21, "R4_w=scalar(umax=1020,var_off=(0x0; 0x3fc))"},
			{22, "R4_w=scalar(umax=510,var_off=(0x0; 0x1fe))"},
		},
	},
	{
		.descr = "unknown mul",
		.insns = {
			LOAD_UNKNOWN(BPF_REG_3),
			BPF_MOV64_REG(BPF_REG_4, BPF_REG_3),
			BPF_ALU64_IMM(BPF_MUL, BPF_REG_4, 1),
			BPF_MOV64_REG(BPF_REG_4, BPF_REG_3),
			BPF_ALU64_IMM(BPF_MUL, BPF_REG_4, 2),
			BPF_MOV64_REG(BPF_REG_4, BPF_REG_3),
			BPF_ALU64_IMM(BPF_MUL, BPF_REG_4, 4),
			BPF_MOV64_REG(BPF_REG_4, BPF_REG_3),
			BPF_ALU64_IMM(BPF_MUL, BPF_REG_4, 8),
			BPF_ALU64_IMM(BPF_MUL, BPF_REG_4, 2),
			BPF_MOV64_IMM(BPF_REG_0, 0),
			BPF_EXIT_INSN(),
		},
		.prog_type = BPF_PROG_TYPE_SCHED_CLS,
		.matches = {
			{6, "R3_w=scalar(umax=255,var_off=(0x0; 0xff))"},
			{7, "R4_w=scalar(id=1,umax=255,var_off=(0x0; 0xff))"},
			{8, "R4_w=scalar(umax=255,var_off=(0x0; 0xff))"},
			{9, "R4_w=scalar(id=1,umax=255,var_off=(0x0; 0xff))"},
			{10, "R4_w=scalar(umax=510,var_off=(0x0; 0x1fe))"},
			{11, "R4_w=scalar(id=1,umax=255,var_off=(0x0; 0xff))"},
			{12, "R4_w=scalar(umax=1020,var_off=(0x0; 0x3fc))"},
			{13, "R4_w=scalar(id=1,umax=255,var_off=(0x0; 0xff))"},
			{14, "R4_w=scalar(umax=2040,var_off=(0x0; 0x7f8))"},
			{15, "R4_w=scalar(umax=4080,var_off=(0x0; 0xff0))"},
		},
	},
	{
		.descr = "packet const offset",
		.insns = {
			PREP_PKT_POINTERS,
			BPF_MOV64_REG(BPF_REG_5, BPF_REG_2),

			BPF_MOV64_IMM(BPF_REG_0, 0),

			 
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_5, 14),
			BPF_MOV64_REG(BPF_REG_4, BPF_REG_5),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, 4),
			BPF_JMP_REG(BPF_JGE, BPF_REG_3, BPF_REG_4, 1),
			BPF_EXIT_INSN(),

			BPF_LDX_MEM(BPF_B, BPF_REG_4, BPF_REG_5, 0),
			BPF_LDX_MEM(BPF_B, BPF_REG_4, BPF_REG_5, 1),
			BPF_LDX_MEM(BPF_B, BPF_REG_4, BPF_REG_5, 2),
			BPF_LDX_MEM(BPF_B, BPF_REG_4, BPF_REG_5, 3),
			BPF_LDX_MEM(BPF_H, BPF_REG_4, BPF_REG_5, 0),
			BPF_LDX_MEM(BPF_H, BPF_REG_4, BPF_REG_5, 2),
			BPF_LDX_MEM(BPF_W, BPF_REG_4, BPF_REG_5, 0),

			BPF_MOV64_IMM(BPF_REG_0, 0),
			BPF_EXIT_INSN(),
		},
		.prog_type = BPF_PROG_TYPE_SCHED_CLS,
		.matches = {
			{2, "R5_w=pkt(off=0,r=0,imm=0)"},
			{4, "R5_w=pkt(off=14,r=0,imm=0)"},
			{5, "R4_w=pkt(off=14,r=0,imm=0)"},
			{9, "R2=pkt(off=0,r=18,imm=0)"},
			{10, "R5=pkt(off=14,r=18,imm=0)"},
			{10, "R4_w=scalar(umax=255,var_off=(0x0; 0xff))"},
			{13, "R4_w=scalar(umax=65535,var_off=(0x0; 0xffff))"},
			{14, "R4_w=scalar(umax=65535,var_off=(0x0; 0xffff))"},
		},
	},
	{
		.descr = "packet variable offset",
		.insns = {
			LOAD_UNKNOWN(BPF_REG_6),
			BPF_ALU64_IMM(BPF_LSH, BPF_REG_6, 2),

			 
			BPF_MOV64_REG(BPF_REG_5, BPF_REG_2),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_5, 14),
			BPF_ALU64_REG(BPF_ADD, BPF_REG_5, BPF_REG_6),
			BPF_MOV64_REG(BPF_REG_4, BPF_REG_5),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, 4),
			BPF_JMP_REG(BPF_JGE, BPF_REG_3, BPF_REG_4, 1),
			BPF_EXIT_INSN(),
			BPF_LDX_MEM(BPF_W, BPF_REG_4, BPF_REG_5, 0),

			 
			BPF_MOV64_REG(BPF_REG_5, BPF_REG_2),
			BPF_ALU64_REG(BPF_ADD, BPF_REG_5, BPF_REG_6),
			BPF_MOV64_REG(BPF_REG_4, BPF_REG_5),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_5, 14),
			BPF_MOV64_REG(BPF_REG_4, BPF_REG_5),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, 4),
			BPF_JMP_REG(BPF_JGE, BPF_REG_3, BPF_REG_4, 1),
			BPF_EXIT_INSN(),
			BPF_LDX_MEM(BPF_W, BPF_REG_4, BPF_REG_5, 0),

			 
			BPF_MOV64_REG(BPF_REG_5, BPF_REG_2),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_5, 14),
			BPF_ALU64_REG(BPF_ADD, BPF_REG_5, BPF_REG_6),
			BPF_MOV64_REG(BPF_REG_4, BPF_REG_5),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_5, 4),
			BPF_ALU64_REG(BPF_ADD, BPF_REG_5, BPF_REG_6),
			BPF_MOV64_REG(BPF_REG_4, BPF_REG_5),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, 4),
			BPF_JMP_REG(BPF_JGE, BPF_REG_3, BPF_REG_4, 1),
			BPF_EXIT_INSN(),
			BPF_LDX_MEM(BPF_W, BPF_REG_4, BPF_REG_5, 0),

			BPF_MOV64_IMM(BPF_REG_0, 0),
			BPF_EXIT_INSN(),
		},
		.prog_type = BPF_PROG_TYPE_SCHED_CLS,
		.matches = {
			 
			{6, "R2_w=pkt(off=0,r=8,imm=0)"},
			{7, "R6_w=scalar(umax=1020,var_off=(0x0; 0x3fc))"},
			 
			{11, "R5_w=pkt(id=1,off=14,r=0,umax=1020,var_off=(0x0; 0x3fc))"},
			 
			{15, "R4=pkt(id=1,off=18,r=18,umax=1020,var_off=(0x0; 0x3fc))"},
			{15, "R5=pkt(id=1,off=14,r=18,umax=1020,var_off=(0x0; 0x3fc))"},
			 
			{18, "R4_w=pkt(id=2,off=0,r=0,umax=1020,var_off=(0x0; 0x3fc))"},
			{18, "R5_w=pkt(id=2,off=0,r=0,umax=1020,var_off=(0x0; 0x3fc))"},
			 
			{19, "R5_w=pkt(id=2,off=14,r=0,umax=1020,var_off=(0x0; 0x3fc))"},
			 
			{24, "R4=pkt(id=2,off=18,r=18,umax=1020,var_off=(0x0; 0x3fc))"},
			{24, "R5=pkt(id=2,off=14,r=18,umax=1020,var_off=(0x0; 0x3fc))"},
			 
			{26, "R5_w=pkt(off=14,r=8"},
			 
			{28, "R4_w=pkt(id=3,off=14,r=0,umax=1020,var_off=(0x0; 0x3fc))"},
			{28, "R5_w=pkt(id=3,off=14,r=0,umax=1020,var_off=(0x0; 0x3fc))"},
			 
			{29, "R5_w=pkt(id=3,off=18,r=0,umax=1020,var_off=(0x0; 0x3fc))"},
			 
			{31, "R4_w=pkt(id=4,off=18,r=0,umax=2040,var_off=(0x0; 0x7fc)"},
			{31, "R5_w=pkt(id=4,off=18,r=0,umax=2040,var_off=(0x0; 0x7fc)"},
			 
			{35, "R4=pkt(id=4,off=22,r=22,umax=2040,var_off=(0x0; 0x7fc)"},
			{35, "R5=pkt(id=4,off=18,r=22,umax=2040,var_off=(0x0; 0x7fc)"},
		},
	},
	{
		.descr = "packet variable offset 2",
		.insns = {
			 
			LOAD_UNKNOWN(BPF_REG_6),
			BPF_ALU64_IMM(BPF_LSH, BPF_REG_6, 2),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_6, 14),
			 
			BPF_MOV64_REG(BPF_REG_5, BPF_REG_2),
			BPF_ALU64_REG(BPF_ADD, BPF_REG_5, BPF_REG_6),
			 
			BPF_MOV64_REG(BPF_REG_4, BPF_REG_5),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, 4),
			BPF_JMP_REG(BPF_JGE, BPF_REG_3, BPF_REG_4, 1),
			BPF_EXIT_INSN(),
			BPF_LDX_MEM(BPF_W, BPF_REG_6, BPF_REG_5, 0),
			 
			BPF_ALU64_IMM(BPF_AND, BPF_REG_6, 0xff),
			BPF_ALU64_IMM(BPF_LSH, BPF_REG_6, 2),
			 
			BPF_ALU64_REG(BPF_ADD, BPF_REG_5, BPF_REG_6),
			 
			BPF_MOV64_REG(BPF_REG_4, BPF_REG_5),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, 4),
			BPF_JMP_REG(BPF_JGE, BPF_REG_3, BPF_REG_4, 1),
			BPF_EXIT_INSN(),
			BPF_LDX_MEM(BPF_W, BPF_REG_6, BPF_REG_5, 0),
			BPF_MOV64_IMM(BPF_REG_0, 0),
			BPF_EXIT_INSN(),
		},
		.prog_type = BPF_PROG_TYPE_SCHED_CLS,
		.matches = {
			 
			{6, "R2_w=pkt(off=0,r=8,imm=0)"},
			{7, "R6_w=scalar(umax=1020,var_off=(0x0; 0x3fc))"},
			 
			{8, "R6_w=scalar(umin=14,umax=1034,var_off=(0x2; 0x7fc))"},
			 
			{11, "R5_w=pkt(id=1,off=0,r=0,umin=14,umax=1034,var_off=(0x2; 0x7fc)"},
			{12, "R4=pkt(id=1,off=4,r=0,umin=14,umax=1034,var_off=(0x2; 0x7fc)"},
			 
			{15, "R5=pkt(id=1,off=0,r=4,umin=14,umax=1034,var_off=(0x2; 0x7fc)"},
			 
			{17, "R6_w=scalar(umax=1020,var_off=(0x0; 0x3fc))"},
			 
			{19, "R5_w=pkt(id=2,off=0,r=0,umin=14,umax=2054,var_off=(0x2; 0xffc)"},
			{20, "R4=pkt(id=2,off=4,r=0,umin=14,umax=2054,var_off=(0x2; 0xffc)"},
			 
			{23, "R5=pkt(id=2,off=0,r=4,umin=14,umax=2054,var_off=(0x2; 0xffc)"},
		},
	},
	{
		.descr = "dubious pointer arithmetic",
		.insns = {
			PREP_PKT_POINTERS,
			BPF_MOV64_IMM(BPF_REG_0, 0),
			 
			BPF_MOV64_REG(BPF_REG_5, BPF_REG_3),
			BPF_ALU64_REG(BPF_SUB, BPF_REG_5, BPF_REG_2),
			BPF_ALU64_IMM(BPF_LSH, BPF_REG_5, 2),
			 
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_5, 14),
			 
			BPF_JMP_IMM(BPF_JSGE, BPF_REG_5, 0, 1),
			BPF_EXIT_INSN(),
			 
			BPF_MOV64_REG(BPF_REG_6, BPF_REG_2),
			BPF_ALU64_REG(BPF_ADD, BPF_REG_6, BPF_REG_5),
			 
			BPF_MOV64_REG(BPF_REG_4, BPF_REG_6),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, 4),
			BPF_JMP_REG(BPF_JGE, BPF_REG_3, BPF_REG_4, 1),
			BPF_EXIT_INSN(),
			BPF_LDX_MEM(BPF_W, BPF_REG_4, BPF_REG_6, 0),
			BPF_EXIT_INSN(),
		},
		.prog_type = BPF_PROG_TYPE_SCHED_CLS,
		.result = REJECT,
		.matches = {
			{3, "R5_w=pkt_end(off=0,imm=0)"},
			 
			{5, "R5_w=scalar(smax=9223372036854775804,umax=18446744073709551612,var_off=(0x0; 0xfffffffffffffffc)"},
			 
			{6, "R5_w=scalar(smin=-9223372036854775806,smax=9223372036854775806,umin=2,umax=18446744073709551614,var_off=(0x2; 0xfffffffffffffffc)"},
			 
			{9, "R5=scalar(umin=2,umax=9223372036854775806,var_off=(0x2; 0x7ffffffffffffffc)"},
			 
			{11, "R6_w=pkt(id=1,off=0,r=0,umin=2,umax=9223372036854775806,var_off=(0x2; 0x7ffffffffffffffc)"},
			{12, "R4_w=pkt(id=1,off=4,r=0,umin=2,umax=9223372036854775806,var_off=(0x2; 0x7ffffffffffffffc)"},
			 
			{15, "R6_w=pkt(id=1,off=0,r=0,umin=2,umax=9223372036854775806,var_off=(0x2; 0x7ffffffffffffffc)"},
		}
	},
	{
		.descr = "variable subtraction",
		.insns = {
			 
			LOAD_UNKNOWN(BPF_REG_6),
			BPF_MOV64_REG(BPF_REG_7, BPF_REG_6),
			BPF_ALU64_IMM(BPF_LSH, BPF_REG_6, 2),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_6, 14),
			 
			BPF_ALU64_IMM(BPF_LSH, BPF_REG_7, 2),
			BPF_ALU64_REG(BPF_SUB, BPF_REG_6, BPF_REG_7),
			 
			BPF_JMP_IMM(BPF_JSGE, BPF_REG_6, 0, 1),
			BPF_EXIT_INSN(),
			 
			BPF_MOV64_REG(BPF_REG_5, BPF_REG_2),
			BPF_ALU64_REG(BPF_ADD, BPF_REG_5, BPF_REG_6),
			 
			BPF_MOV64_REG(BPF_REG_4, BPF_REG_5),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, 4),
			BPF_JMP_REG(BPF_JGE, BPF_REG_3, BPF_REG_4, 1),
			BPF_EXIT_INSN(),
			BPF_LDX_MEM(BPF_W, BPF_REG_6, BPF_REG_5, 0),
			BPF_EXIT_INSN(),
		},
		.prog_type = BPF_PROG_TYPE_SCHED_CLS,
		.matches = {
			 
			{6, "R2_w=pkt(off=0,r=8,imm=0)"},
			{8, "R6_w=scalar(umax=1020,var_off=(0x0; 0x3fc))"},
			 
			{9, "R6_w=scalar(umin=14,umax=1034,var_off=(0x2; 0x7fc))"},
			 
			{10, "R7_w=scalar(umax=1020,var_off=(0x0; 0x3fc))"},
			 
			{11, "R6=scalar(smin=-1006,smax=1034,umin=2,umax=18446744073709551614,var_off=(0x2; 0xfffffffffffffffc)"},
			 
			{14, "R6=scalar(umin=2,umax=1034,var_off=(0x2; 0x7fc))"},
			 
			{20, "R5=pkt(id=2,off=0,r=4,umin=2,umax=1034,var_off=(0x2; 0x7fc)"},

		},
	},
	{
		.descr = "pointer variable subtraction",
		.insns = {
			 
			LOAD_UNKNOWN(BPF_REG_6),
			BPF_MOV64_REG(BPF_REG_7, BPF_REG_6),
			BPF_ALU64_IMM(BPF_AND, BPF_REG_6, 0xf),
			BPF_ALU64_IMM(BPF_LSH, BPF_REG_6, 2),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_6, 14),
			 
			BPF_MOV64_REG(BPF_REG_5, BPF_REG_2),
			BPF_ALU64_REG(BPF_SUB, BPF_REG_5, BPF_REG_6),
			 
			BPF_ALU64_IMM(BPF_LSH, BPF_REG_7, 2),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_7, 76),
			 
			BPF_ALU64_REG(BPF_ADD, BPF_REG_5, BPF_REG_7),
			 
			BPF_MOV64_REG(BPF_REG_4, BPF_REG_5),
			BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, 4),
			BPF_JMP_REG(BPF_JGE, BPF_REG_3, BPF_REG_4, 1),
			BPF_EXIT_INSN(),
			BPF_LDX_MEM(BPF_W, BPF_REG_6, BPF_REG_5, 0),
			BPF_EXIT_INSN(),
		},
		.prog_type = BPF_PROG_TYPE_SCHED_CLS,
		.matches = {
			 
			{6, "R2_w=pkt(off=0,r=8,imm=0)"},
			{9, "R6_w=scalar(umax=60,var_off=(0x0; 0x3c))"},
			 
			{10, "R6_w=scalar(umin=14,umax=74,var_off=(0x2; 0x7c))"},
			 
			{13, "R5_w=pkt(id=2,off=0,r=8,umin=18446744073709551542,umax=18446744073709551602,var_off=(0xffffffffffffff82; 0x7c)"},
			 
			{14, "R7_w=scalar(umin=76,umax=1096,var_off=(0x0; 0x7fc))"},
			 
			{16, "R5_w=pkt(id=3,off=0,r=0,umin=2,umax=1082,var_off=(0x2; 0x7fc)"},
			 
			{20, "R5=pkt(id=3,off=0,r=4,umin=2,umax=1082,var_off=(0x2; 0x7fc)"},
		},
	},
};

static int probe_filter_length(const struct bpf_insn *fp)
{
	int len;

	for (len = MAX_INSNS - 1; len > 0; --len)
		if (fp[len].code != 0 || fp[len].imm != 0)
			break;
	return len + 1;
}

static char bpf_vlog[32768];

static int do_test_single(struct bpf_align_test *test)
{
	struct bpf_insn *prog = test->insns;
	int prog_type = test->prog_type;
	char bpf_vlog_copy[32768];
	LIBBPF_OPTS(bpf_prog_load_opts, opts,
		.prog_flags = BPF_F_STRICT_ALIGNMENT,
		.log_buf = bpf_vlog,
		.log_size = sizeof(bpf_vlog),
		.log_level = 2,
	);
	const char *line_ptr;
	int cur_line = -1;
	int prog_len, i;
	int fd_prog;
	int ret;

	prog_len = probe_filter_length(prog);
	fd_prog = bpf_prog_load(prog_type ? : BPF_PROG_TYPE_SOCKET_FILTER, NULL, "GPL",
				prog, prog_len, &opts);
	if (fd_prog < 0 && test->result != REJECT) {
		printf("Failed to load program.\n");
		printf("%s", bpf_vlog);
		ret = 1;
	} else if (fd_prog >= 0 && test->result == REJECT) {
		printf("Unexpected success to load!\n");
		printf("%s", bpf_vlog);
		ret = 1;
		close(fd_prog);
	} else {
		ret = 0;
		 
		strncpy(bpf_vlog_copy, bpf_vlog, sizeof(bpf_vlog_copy));
		line_ptr = strtok(bpf_vlog_copy, "\n");
		for (i = 0; i < MAX_MATCHES; i++) {
			struct bpf_reg_match m = test->matches[i];
			int tmp;

			if (!m.match)
				break;
			while (line_ptr) {
				cur_line = -1;
				sscanf(line_ptr, "%u: ", &cur_line);
				if (cur_line == -1)
					sscanf(line_ptr, "from %u to %u: ", &tmp, &cur_line);
				if (cur_line == m.line)
					break;
				line_ptr = strtok(NULL, "\n");
			}
			if (!line_ptr) {
				printf("Failed to find line %u for match: %s\n",
				       m.line, m.match);
				ret = 1;
				printf("%s", bpf_vlog);
				break;
			}
			 
			while (!strstr(line_ptr, m.match)) {
				cur_line = -1;
				line_ptr = strtok(NULL, "\n");
				sscanf(line_ptr ?: "", "%u: ", &cur_line);
				if (!line_ptr || cur_line != m.line)
					break;
			}
			if (cur_line != m.line || !line_ptr || !strstr(line_ptr, m.match)) {
				printf("Failed to find match %u: %s\n", m.line, m.match);
				ret = 1;
				printf("%s", bpf_vlog);
				break;
			}
		}
		if (fd_prog >= 0)
			close(fd_prog);
	}
	return ret;
}

void test_align(void)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(tests); i++) {
		struct bpf_align_test *test = &tests[i];

		if (!test__start_subtest(test->descr))
			continue;

		ASSERT_OK(do_test_single(test), test->descr);
	}
}
