


#ifndef __NFP_BPF_H__
#define __NFP_BPF_H__ 1

#include <linux/bitfield.h>
#include <linux/bpf.h>
#include <linux/bpf_verifier.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/rhashtable.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <linux/wait.h>

#include "../ccm.h"
#include "../nfp_asm.h"
#include "fw.h"

#define cmsg_warn(bpf, msg...)	nn_dp_warn(&(bpf)->app->ctrl->dp, msg)


#define OP_RELO_TYPE	0xff00000000000000ULL

enum nfp_relo_type {
	RELO_NONE = 0,
	
	RELO_BR_REL,
	
	RELO_BR_GO_OUT,
	RELO_BR_GO_ABORT,
	RELO_BR_GO_CALL_PUSH_REGS,
	RELO_BR_GO_CALL_POP_REGS,
	
	RELO_BR_NEXT_PKT,
	RELO_BR_HELPER,
	
	RELO_IMMED_REL,
};


#define BR_OFF_RELO		15000

enum static_regs {
	STATIC_REG_IMMA		= 20, 
	STATIC_REG_IMM		= 21, 
	STATIC_REG_STACK	= 22, 
	STATIC_REG_PKT_LEN	= 22, 
};

enum pkt_vec {
	PKT_VEC_PKT_LEN		= 0,
	PKT_VEC_PKT_PTR		= 2,
	PKT_VEC_QSEL_SET	= 4,
	PKT_VEC_QSEL_VAL	= 6,
};

#define PKT_VEL_QSEL_SET_BIT	4

#define pv_len(np)	reg_lm(1, PKT_VEC_PKT_LEN)
#define pv_ctm_ptr(np)	reg_lm(1, PKT_VEC_PKT_PTR)
#define pv_qsel_set(np)	reg_lm(1, PKT_VEC_QSEL_SET)
#define pv_qsel_val(np)	reg_lm(1, PKT_VEC_QSEL_VAL)

#define stack_reg(np)	reg_a(STATIC_REG_STACK)
#define stack_imm(np)	imm_b(np)
#define plen_reg(np)	reg_b(STATIC_REG_PKT_LEN)
#define pptr_reg(np)	pv_ctm_ptr(np)
#define imm_a(np)	reg_a(STATIC_REG_IMM)
#define imm_b(np)	reg_b(STATIC_REG_IMM)
#define imma_a(np)	reg_a(STATIC_REG_IMMA)
#define imma_b(np)	reg_b(STATIC_REG_IMMA)
#define imm_both(np)	reg_both(STATIC_REG_IMM)
#define ret_reg(np)	imm_a(np)

#define NFP_BPF_ABI_FLAGS	reg_imm(0)
#define   NFP_BPF_ABI_FLAG_MARK	1


struct nfp_app_bpf {
	struct nfp_app *app;
	struct nfp_ccm ccm;

	struct bpf_offload_dev *bpf_dev;

	unsigned int cmsg_key_sz;
	unsigned int cmsg_val_sz;

	unsigned int cmsg_cache_cnt;

	struct list_head map_list;
	unsigned int maps_in_use;
	unsigned int map_elems_in_use;

	struct rhashtable maps_neutral;

	u32 abi_version;

	struct nfp_bpf_cap_adjust_head {
		u32 flags;
		int off_min;
		int off_max;
		int guaranteed_sub;
		int guaranteed_add;
	} adjust_head;

	struct {
		u32 types;
		u32 max_maps;
		u32 max_elems;
		u32 max_key_sz;
		u32 max_val_sz;
		u32 max_elem_sz;
	} maps;

	struct {
		u32 map_lookup;
		u32 map_update;
		u32 map_delete;
		u32 perf_event_output;
	} helpers;

	bool pseudo_random;
	bool queue_select;
	bool adjust_tail;
	bool cmsg_multi_ent;
};

enum nfp_bpf_map_use {
	NFP_MAP_UNUSED = 0,
	NFP_MAP_USE_READ,
	NFP_MAP_USE_WRITE,
	NFP_MAP_USE_ATOMIC_CNT,
};

struct nfp_bpf_map_word {
	unsigned char type		:4;
	unsigned char non_zero_update	:1;
};

#define NFP_BPF_MAP_CACHE_CNT		4U
#define NFP_BPF_MAP_CACHE_TIME_NS	(250 * 1000)


struct nfp_bpf_map {
	struct bpf_offloaded_map *offmap;
	struct nfp_app_bpf *bpf;
	u32 tid;

	spinlock_t cache_lock;
	u32 cache_blockers;
	u32 cache_gen;
	u64 cache_to;
	struct sk_buff *cache;

	struct list_head l;
	struct nfp_bpf_map_word use_map[];
};

struct nfp_bpf_neutral_map {
	struct rhash_head l;
	struct bpf_map *ptr;
	u32 map_id;
	u32 count;
};

extern const struct rhashtable_params nfp_bpf_maps_neutral_params;

struct nfp_prog;
struct nfp_insn_meta;
typedef int (*instr_cb_t)(struct nfp_prog *, struct nfp_insn_meta *);

#define nfp_prog_first_meta(nfp_prog)					\
	list_first_entry(&(nfp_prog)->insns, struct nfp_insn_meta, l)
#define nfp_prog_last_meta(nfp_prog)					\
	list_last_entry(&(nfp_prog)->insns, struct nfp_insn_meta, l)
#define nfp_meta_next(meta)	list_next_entry(meta, l)
#define nfp_meta_prev(meta)	list_prev_entry(meta, l)


struct nfp_bpf_reg_state {
	struct bpf_reg_state reg;
	bool var_off;
};

#define FLAG_INSN_IS_JUMP_DST			BIT(0)
#define FLAG_INSN_IS_SUBPROG_START		BIT(1)
#define FLAG_INSN_PTR_CALLER_STACK_FRAME	BIT(2)

#define FLAG_INSN_SKIP_NOOP			BIT(3)

#define FLAG_INSN_SKIP_PREC_DEPENDENT		BIT(4)

#define FLAG_INSN_SKIP_VERIFIER_OPT		BIT(5)

#define FLAG_INSN_DO_ZEXT			BIT(6)

#define FLAG_INSN_SKIP_MASK		(FLAG_INSN_SKIP_NOOP | \
					 FLAG_INSN_SKIP_PREC_DEPENDENT | \
					 FLAG_INSN_SKIP_VERIFIER_OPT)


struct nfp_insn_meta {
	struct bpf_insn insn;
	union {
		
		struct {
			struct bpf_reg_state ptr;
			struct bpf_insn *paired_st;
			s16 ldst_gather_len;
			bool ptr_not_const;
			struct {
				s16 range_start;
				s16 range_end;
				bool do_init;
			} pkt_cache;
			bool xadd_over_16bit;
			bool xadd_maybe_16bit;
		};
		
		struct {
			struct nfp_insn_meta *jmp_dst;
			bool jump_neg_op;
			u32 num_insns_after_br; 
		};
		
		struct {
			u32 func_id;
			struct bpf_reg_state arg1;
			struct nfp_bpf_reg_state arg2;
		};
		
		struct {
			u64 umin_src;
			u64 umax_src;
			u64 umin_dst;
			u64 umax_dst;
		};
	};
	unsigned int off;
	unsigned short n;
	unsigned short flags;
	unsigned short subprog_idx;
	instr_cb_t double_cb;

	struct list_head l;
};

#define BPF_SIZE_MASK	0x18

static inline u8 mbpf_class(const struct nfp_insn_meta *meta)
{
	return BPF_CLASS(meta->insn.code);
}

static inline u8 mbpf_src(const struct nfp_insn_meta *meta)
{
	return BPF_SRC(meta->insn.code);
}

static inline u8 mbpf_op(const struct nfp_insn_meta *meta)
{
	return BPF_OP(meta->insn.code);
}

static inline u8 mbpf_mode(const struct nfp_insn_meta *meta)
{
	return BPF_MODE(meta->insn.code);
}

static inline bool is_mbpf_alu(const struct nfp_insn_meta *meta)
{
	return mbpf_class(meta) == BPF_ALU64 || mbpf_class(meta) == BPF_ALU;
}

static inline bool is_mbpf_load(const struct nfp_insn_meta *meta)
{
	return (meta->insn.code & ~BPF_SIZE_MASK) == (BPF_LDX | BPF_MEM);
}

static inline bool is_mbpf_jmp32(const struct nfp_insn_meta *meta)
{
	return mbpf_class(meta) == BPF_JMP32;
}

static inline bool is_mbpf_jmp64(const struct nfp_insn_meta *meta)
{
	return mbpf_class(meta) == BPF_JMP;
}

static inline bool is_mbpf_jmp(const struct nfp_insn_meta *meta)
{
	return is_mbpf_jmp32(meta) || is_mbpf_jmp64(meta);
}

static inline bool is_mbpf_store(const struct nfp_insn_meta *meta)
{
	return (meta->insn.code & ~BPF_SIZE_MASK) == (BPF_STX | BPF_MEM);
}

static inline bool is_mbpf_load_pkt(const struct nfp_insn_meta *meta)
{
	return is_mbpf_load(meta) && meta->ptr.type == PTR_TO_PACKET;
}

static inline bool is_mbpf_store_pkt(const struct nfp_insn_meta *meta)
{
	return is_mbpf_store(meta) && meta->ptr.type == PTR_TO_PACKET;
}

static inline bool is_mbpf_classic_load(const struct nfp_insn_meta *meta)
{
	u8 code = meta->insn.code;

	return BPF_CLASS(code) == BPF_LD &&
	       (BPF_MODE(code) == BPF_ABS || BPF_MODE(code) == BPF_IND);
}

static inline bool is_mbpf_classic_store(const struct nfp_insn_meta *meta)
{
	u8 code = meta->insn.code;

	return BPF_CLASS(code) == BPF_ST && BPF_MODE(code) == BPF_MEM;
}

static inline bool is_mbpf_classic_store_pkt(const struct nfp_insn_meta *meta)
{
	return is_mbpf_classic_store(meta) && meta->ptr.type == PTR_TO_PACKET;
}

static inline bool is_mbpf_atomic(const struct nfp_insn_meta *meta)
{
	return (meta->insn.code & ~BPF_SIZE_MASK) == (BPF_STX | BPF_ATOMIC);
}

static inline bool is_mbpf_mul(const struct nfp_insn_meta *meta)
{
	return is_mbpf_alu(meta) && mbpf_op(meta) == BPF_MUL;
}

static inline bool is_mbpf_div(const struct nfp_insn_meta *meta)
{
	return is_mbpf_alu(meta) && mbpf_op(meta) == BPF_DIV;
}

static inline bool is_mbpf_cond_jump(const struct nfp_insn_meta *meta)
{
	u8 op;

	if (is_mbpf_jmp32(meta))
		return true;

	if (!is_mbpf_jmp64(meta))
		return false;

	op = mbpf_op(meta);
	return op != BPF_JA && op != BPF_EXIT && op != BPF_CALL;
}

static inline bool is_mbpf_helper_call(const struct nfp_insn_meta *meta)
{
	struct bpf_insn insn = meta->insn;

	return insn.code == (BPF_JMP | BPF_CALL) &&
		insn.src_reg != BPF_PSEUDO_CALL;
}

static inline bool is_mbpf_pseudo_call(const struct nfp_insn_meta *meta)
{
	struct bpf_insn insn = meta->insn;

	return insn.code == (BPF_JMP | BPF_CALL) &&
		insn.src_reg == BPF_PSEUDO_CALL;
}

#define STACK_FRAME_ALIGN 64


struct nfp_bpf_subprog_info {
	u16 stack_depth;
	u8 needs_reg_push : 1;
};


struct nfp_prog {
	struct nfp_app_bpf *bpf;

	u64 *prog;
	unsigned int prog_len;
	unsigned int __prog_alloc_len;

	unsigned int stack_size;

	struct nfp_insn_meta *verifier_meta;

	enum bpf_prog_type type;

	unsigned int last_bpf_off;
	unsigned int tgt_out;
	unsigned int tgt_abort;
	unsigned int tgt_call_push_regs;
	unsigned int tgt_call_pop_regs;

	unsigned int n_translated;
	int error;

	unsigned int stack_frame_depth;
	unsigned int adjust_head_location;

	unsigned int map_records_cnt;
	unsigned int subprog_cnt;
	struct nfp_bpf_neutral_map **map_records;
	struct nfp_bpf_subprog_info *subprog;

	unsigned int n_insns;
	struct list_head insns;
};


struct nfp_bpf_vnic {
	struct bpf_prog *tc_prog;
	unsigned int start_off;
	unsigned int tgt_done;
};

bool nfp_is_subprog_start(struct nfp_insn_meta *meta);
void nfp_bpf_jit_prepare(struct nfp_prog *nfp_prog);
int nfp_bpf_jit(struct nfp_prog *prog);
bool nfp_bpf_supported_opcode(u8 code);
bool nfp_bpf_offload_check_mtu(struct nfp_net *nn, struct bpf_prog *prog,
			       unsigned int mtu);

int nfp_verify_insn(struct bpf_verifier_env *env, int insn_idx,
		    int prev_insn_idx);
int nfp_bpf_finalize(struct bpf_verifier_env *env);

int nfp_bpf_opt_replace_insn(struct bpf_verifier_env *env, u32 off,
			     struct bpf_insn *insn);
int nfp_bpf_opt_remove_insns(struct bpf_verifier_env *env, u32 off, u32 cnt);

extern const struct bpf_prog_offload_ops nfp_bpf_dev_ops;

struct netdev_bpf;
struct nfp_app;
struct nfp_net;

int nfp_ndo_bpf(struct nfp_app *app, struct nfp_net *nn,
		struct netdev_bpf *bpf);
int nfp_net_bpf_offload(struct nfp_net *nn, struct bpf_prog *prog,
			bool old_prog, struct netlink_ext_ack *extack);

struct nfp_insn_meta *
nfp_bpf_goto_meta(struct nfp_prog *nfp_prog, struct nfp_insn_meta *meta,
		  unsigned int insn_idx);

void *nfp_bpf_relo_for_vnic(struct nfp_prog *nfp_prog, struct nfp_bpf_vnic *bv);

unsigned int nfp_bpf_ctrl_cmsg_min_mtu(struct nfp_app_bpf *bpf);
unsigned int nfp_bpf_ctrl_cmsg_mtu(struct nfp_app_bpf *bpf);
unsigned int nfp_bpf_ctrl_cmsg_cache_cnt(struct nfp_app_bpf *bpf);
long long int
nfp_bpf_ctrl_alloc_map(struct nfp_app_bpf *bpf, struct bpf_map *map);
void
nfp_bpf_ctrl_free_map(struct nfp_app_bpf *bpf, struct nfp_bpf_map *nfp_map);
int nfp_bpf_ctrl_getfirst_entry(struct bpf_offloaded_map *offmap,
				void *next_key);
int nfp_bpf_ctrl_update_entry(struct bpf_offloaded_map *offmap,
			      void *key, void *value, u64 flags);
int nfp_bpf_ctrl_del_entry(struct bpf_offloaded_map *offmap, void *key);
int nfp_bpf_ctrl_lookup_entry(struct bpf_offloaded_map *offmap,
			      void *key, void *value);
int nfp_bpf_ctrl_getnext_entry(struct bpf_offloaded_map *offmap,
			       void *key, void *next_key);

int nfp_bpf_event_output(struct nfp_app_bpf *bpf, const void *data,
			 unsigned int len);

void nfp_bpf_ctrl_msg_rx(struct nfp_app *app, struct sk_buff *skb);
void
nfp_bpf_ctrl_msg_rx_raw(struct nfp_app *app, const void *data,
			unsigned int len);
#endif
