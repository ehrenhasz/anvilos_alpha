 

#define __PERF_EVENT_INSNS__					\
	BPF_MOV64_IMM(BPF_REG_2, 5),				\
	BPF_STX_MEM(BPF_DW, BPF_REG_10, BPF_REG_2, -8),		\
	BPF_MOV64_REG(BPF_REG_4, BPF_REG_10),			\
	BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, -8),			\
	BPF_LD_MAP_FD(BPF_REG_2, 0),				\
	BPF_MOV64_IMM(BPF_REG_3, 0),				\
	BPF_MOV64_IMM(BPF_REG_5, 8),				\
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0,		\
		     BPF_FUNC_perf_event_output),		\
	BPF_MOV64_IMM(BPF_REG_0, 1),				\
	BPF_EXIT_INSN(),
{
	"perfevent for sockops",
	.insns = { __PERF_EVENT_INSNS__ },
	.prog_type = BPF_PROG_TYPE_SOCK_OPS,
	.fixup_map_event_output = { 4 },
	.result = ACCEPT,
	.retval = 1,
},
{
	"perfevent for tc",
	.insns =  { __PERF_EVENT_INSNS__ },
	.prog_type = BPF_PROG_TYPE_SCHED_CLS,
	.fixup_map_event_output = { 4 },
	.result = ACCEPT,
	.retval = 1,
},
{
	"perfevent for lwt out",
	.insns =  { __PERF_EVENT_INSNS__ },
	.prog_type = BPF_PROG_TYPE_LWT_OUT,
	.fixup_map_event_output = { 4 },
	.result = ACCEPT,
	.retval = 1,
},
{
	"perfevent for xdp",
	.insns =  { __PERF_EVENT_INSNS__ },
	.prog_type = BPF_PROG_TYPE_XDP,
	.fixup_map_event_output = { 4 },
	.result = ACCEPT,
	.retval = 1,
},
{
	"perfevent for socket filter",
	.insns =  { __PERF_EVENT_INSNS__ },
	.prog_type = BPF_PROG_TYPE_SOCKET_FILTER,
	.fixup_map_event_output = { 4 },
	.result = ACCEPT,
	.retval = 1,
},
{
	"perfevent for sk_skb",
	.insns =  { __PERF_EVENT_INSNS__ },
	.prog_type = BPF_PROG_TYPE_SK_SKB,
	.fixup_map_event_output = { 4 },
	.result = ACCEPT,
	.retval = 1,
},
{
	"perfevent for cgroup skb",
	.insns =  { __PERF_EVENT_INSNS__ },
	.prog_type = BPF_PROG_TYPE_CGROUP_SKB,
	.fixup_map_event_output = { 4 },
	.result = ACCEPT,
	.retval = 1,
},
{
	"perfevent for cgroup dev",
	.insns =  { __PERF_EVENT_INSNS__ },
	.prog_type = BPF_PROG_TYPE_CGROUP_DEVICE,
	.fixup_map_event_output = { 4 },
	.result = ACCEPT,
	.retval = 1,
},
{
	"perfevent for cgroup sysctl",
	.insns =  { __PERF_EVENT_INSNS__ },
	.prog_type = BPF_PROG_TYPE_CGROUP_SYSCTL,
	.fixup_map_event_output = { 4 },
	.result = ACCEPT,
	.retval = 1,
},
{
	"perfevent for cgroup sockopt",
	.insns =  { __PERF_EVENT_INSNS__ },
	.prog_type = BPF_PROG_TYPE_CGROUP_SOCKOPT,
	.expected_attach_type = BPF_CGROUP_SETSOCKOPT,
	.fixup_map_event_output = { 4 },
	.result = ACCEPT,
	.retval = 1,
},
