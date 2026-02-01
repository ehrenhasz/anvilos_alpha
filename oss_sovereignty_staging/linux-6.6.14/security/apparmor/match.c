
 

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/err.h>
#include <linux/kref.h>

#include "include/lib.h"
#include "include/match.h"

#define base_idx(X) ((X) & 0xffffff)

static char nulldfa_src[] = {
	#include "nulldfa.in"
};
struct aa_dfa *nulldfa;

static char stacksplitdfa_src[] = {
	#include "stacksplitdfa.in"
};
struct aa_dfa *stacksplitdfa;

int __init aa_setup_dfa_engine(void)
{
	int error;

	nulldfa = aa_dfa_unpack(nulldfa_src, sizeof(nulldfa_src),
				TO_ACCEPT1_FLAG(YYTD_DATA32) |
				TO_ACCEPT2_FLAG(YYTD_DATA32));
	if (IS_ERR(nulldfa)) {
		error = PTR_ERR(nulldfa);
		nulldfa = NULL;
		return error;
	}

	stacksplitdfa = aa_dfa_unpack(stacksplitdfa_src,
				      sizeof(stacksplitdfa_src),
				      TO_ACCEPT1_FLAG(YYTD_DATA32) |
				      TO_ACCEPT2_FLAG(YYTD_DATA32));
	if (IS_ERR(stacksplitdfa)) {
		aa_put_dfa(nulldfa);
		nulldfa = NULL;
		error = PTR_ERR(stacksplitdfa);
		stacksplitdfa = NULL;
		return error;
	}

	return 0;
}

void __init aa_teardown_dfa_engine(void)
{
	aa_put_dfa(stacksplitdfa);
	aa_put_dfa(nulldfa);
}

 
static struct table_header *unpack_table(char *blob, size_t bsize)
{
	struct table_header *table = NULL;
	struct table_header th;
	size_t tsize;

	if (bsize < sizeof(struct table_header))
		goto out;

	 
	th.td_id = be16_to_cpu(*(__be16 *) (blob)) - 1;
	if (th.td_id > YYTD_ID_MAX)
		goto out;
	th.td_flags = be16_to_cpu(*(__be16 *) (blob + 2));
	th.td_lolen = be32_to_cpu(*(__be32 *) (blob + 8));
	blob += sizeof(struct table_header);

	if (!(th.td_flags == YYTD_DATA16 || th.td_flags == YYTD_DATA32 ||
	      th.td_flags == YYTD_DATA8))
		goto out;

	 
	if (th.td_lolen == 0)
		goto out;
	tsize = table_size(th.td_lolen, th.td_flags);
	if (bsize < tsize)
		goto out;

	table = kvzalloc(tsize, GFP_KERNEL);
	if (table) {
		table->td_id = th.td_id;
		table->td_flags = th.td_flags;
		table->td_lolen = th.td_lolen;
		if (th.td_flags == YYTD_DATA8)
			UNPACK_ARRAY(table->td_data, blob, th.td_lolen,
				     u8, u8, byte_to_byte);
		else if (th.td_flags == YYTD_DATA16)
			UNPACK_ARRAY(table->td_data, blob, th.td_lolen,
				     u16, __be16, be16_to_cpu);
		else if (th.td_flags == YYTD_DATA32)
			UNPACK_ARRAY(table->td_data, blob, th.td_lolen,
				     u32, __be32, be32_to_cpu);
		else
			goto fail;
		 
		if (is_vmalloc_addr(table))
			vm_unmap_aliases();
	}

out:
	return table;
fail:
	kvfree(table);
	return NULL;
}

 
static int verify_table_headers(struct table_header **tables, int flags)
{
	size_t state_count, trans_count;
	int error = -EPROTO;

	 
	if (!(tables[YYTD_ID_DEF] && tables[YYTD_ID_BASE] &&
	      tables[YYTD_ID_NXT] && tables[YYTD_ID_CHK]))
		goto out;

	 
	state_count = tables[YYTD_ID_BASE]->td_lolen;
	if (ACCEPT1_FLAGS(flags)) {
		if (!tables[YYTD_ID_ACCEPT])
			goto out;
		if (state_count != tables[YYTD_ID_ACCEPT]->td_lolen)
			goto out;
	}
	if (ACCEPT2_FLAGS(flags)) {
		if (!tables[YYTD_ID_ACCEPT2])
			goto out;
		if (state_count != tables[YYTD_ID_ACCEPT2]->td_lolen)
			goto out;
	}
	if (state_count != tables[YYTD_ID_DEF]->td_lolen)
		goto out;

	 
	trans_count = tables[YYTD_ID_NXT]->td_lolen;
	if (trans_count != tables[YYTD_ID_CHK]->td_lolen)
		goto out;

	 
	if (tables[YYTD_ID_EC] && tables[YYTD_ID_EC]->td_lolen != 256)
		goto out;

	error = 0;
out:
	return error;
}

 
static int verify_dfa(struct aa_dfa *dfa)
{
	size_t i, state_count, trans_count;
	int error = -EPROTO;

	state_count = dfa->tables[YYTD_ID_BASE]->td_lolen;
	trans_count = dfa->tables[YYTD_ID_NXT]->td_lolen;
	if (state_count == 0)
		goto out;
	for (i = 0; i < state_count; i++) {
		if (!(BASE_TABLE(dfa)[i] & MATCH_FLAG_DIFF_ENCODE) &&
		    (DEFAULT_TABLE(dfa)[i] >= state_count))
			goto out;
		if (BASE_TABLE(dfa)[i] & MATCH_FLAGS_INVALID) {
			pr_err("AppArmor DFA state with invalid match flags");
			goto out;
		}
		if ((BASE_TABLE(dfa)[i] & MATCH_FLAG_DIFF_ENCODE)) {
			if (!(dfa->flags & YYTH_FLAG_DIFF_ENCODE)) {
				pr_err("AppArmor DFA diff encoded transition state without header flag");
				goto out;
			}
		}
		if ((BASE_TABLE(dfa)[i] & MATCH_FLAG_OOB_TRANSITION)) {
			if (base_idx(BASE_TABLE(dfa)[i]) < dfa->max_oob) {
				pr_err("AppArmor DFA out of bad transition out of range");
				goto out;
			}
			if (!(dfa->flags & YYTH_FLAG_OOB_TRANS)) {
				pr_err("AppArmor DFA out of bad transition state without header flag");
				goto out;
			}
		}
		if (base_idx(BASE_TABLE(dfa)[i]) + 255 >= trans_count) {
			pr_err("AppArmor DFA next/check upper bounds error\n");
			goto out;
		}
	}

	for (i = 0; i < trans_count; i++) {
		if (NEXT_TABLE(dfa)[i] >= state_count)
			goto out;
		if (CHECK_TABLE(dfa)[i] >= state_count)
			goto out;
	}

	 
	for (i = 0; i < state_count; i++) {
		size_t j, k;

		for (j = i;
		     (BASE_TABLE(dfa)[j] & MATCH_FLAG_DIFF_ENCODE) &&
		     !(BASE_TABLE(dfa)[j] & MARK_DIFF_ENCODE);
		     j = k) {
			k = DEFAULT_TABLE(dfa)[j];
			if (j == k)
				goto out;
			if (k < j)
				break;		 
			BASE_TABLE(dfa)[j] |= MARK_DIFF_ENCODE;
		}
	}
	error = 0;

out:
	return error;
}

 
static void dfa_free(struct aa_dfa *dfa)
{
	if (dfa) {
		int i;

		for (i = 0; i < ARRAY_SIZE(dfa->tables); i++) {
			kvfree(dfa->tables[i]);
			dfa->tables[i] = NULL;
		}
		kfree(dfa);
	}
}

 
void aa_dfa_free_kref(struct kref *kref)
{
	struct aa_dfa *dfa = container_of(kref, struct aa_dfa, count);
	dfa_free(dfa);
}

 
struct aa_dfa *aa_dfa_unpack(void *blob, size_t size, int flags)
{
	int hsize;
	int error = -ENOMEM;
	char *data = blob;
	struct table_header *table = NULL;
	struct aa_dfa *dfa = kzalloc(sizeof(struct aa_dfa), GFP_KERNEL);
	if (!dfa)
		goto fail;

	kref_init(&dfa->count);

	error = -EPROTO;

	 
	if (size < sizeof(struct table_set_header))
		goto fail;

	if (ntohl(*(__be32 *) data) != YYTH_MAGIC)
		goto fail;

	hsize = ntohl(*(__be32 *) (data + 4));
	if (size < hsize)
		goto fail;

	dfa->flags = ntohs(*(__be16 *) (data + 12));
	if (dfa->flags & ~(YYTH_FLAGS))
		goto fail;

	 
	dfa->max_oob = 1;

	data += hsize;
	size -= hsize;

	while (size > 0) {
		table = unpack_table(data, size);
		if (!table)
			goto fail;

		switch (table->td_id) {
		case YYTD_ID_ACCEPT:
			if (!(table->td_flags & ACCEPT1_FLAGS(flags)))
				goto fail;
			break;
		case YYTD_ID_ACCEPT2:
			if (!(table->td_flags & ACCEPT2_FLAGS(flags)))
				goto fail;
			break;
		case YYTD_ID_BASE:
			if (table->td_flags != YYTD_DATA32)
				goto fail;
			break;
		case YYTD_ID_DEF:
		case YYTD_ID_NXT:
		case YYTD_ID_CHK:
			if (table->td_flags != YYTD_DATA16)
				goto fail;
			break;
		case YYTD_ID_EC:
			if (table->td_flags != YYTD_DATA8)
				goto fail;
			break;
		default:
			goto fail;
		}
		 
		if (dfa->tables[table->td_id])
			goto fail;
		dfa->tables[table->td_id] = table;
		data += table_size(table->td_lolen, table->td_flags);
		size -= table_size(table->td_lolen, table->td_flags);
		table = NULL;
	}
	error = verify_table_headers(dfa->tables, flags);
	if (error)
		goto fail;

	if (flags & DFA_FLAG_VERIFY_STATES) {
		error = verify_dfa(dfa);
		if (error)
			goto fail;
	}

	return dfa;

fail:
	kvfree(table);
	dfa_free(dfa);
	return ERR_PTR(error);
}

#define match_char(state, def, base, next, check, C)	\
do {							\
	u32 b = (base)[(state)];			\
	unsigned int pos = base_idx(b) + (C);		\
	if ((check)[pos] != (state)) {			\
		(state) = (def)[(state)];		\
		if (b & MATCH_FLAG_DIFF_ENCODE)		\
			continue;			\
		break;					\
	}						\
	(state) = (next)[pos];				\
	break;						\
} while (1)

 
aa_state_t aa_dfa_match_len(struct aa_dfa *dfa, aa_state_t start,
			    const char *str, int len)
{
	u16 *def = DEFAULT_TABLE(dfa);
	u32 *base = BASE_TABLE(dfa);
	u16 *next = NEXT_TABLE(dfa);
	u16 *check = CHECK_TABLE(dfa);
	aa_state_t state = start;

	if (state == DFA_NOMATCH)
		return DFA_NOMATCH;

	 
	if (dfa->tables[YYTD_ID_EC]) {
		 
		u8 *equiv = EQUIV_TABLE(dfa);
		for (; len; len--)
			match_char(state, def, base, next, check,
				   equiv[(u8) *str++]);
	} else {
		 
		for (; len; len--)
			match_char(state, def, base, next, check, (u8) *str++);
	}

	return state;
}

 
aa_state_t aa_dfa_match(struct aa_dfa *dfa, aa_state_t start, const char *str)
{
	u16 *def = DEFAULT_TABLE(dfa);
	u32 *base = BASE_TABLE(dfa);
	u16 *next = NEXT_TABLE(dfa);
	u16 *check = CHECK_TABLE(dfa);
	aa_state_t state = start;

	if (state == DFA_NOMATCH)
		return DFA_NOMATCH;

	 
	if (dfa->tables[YYTD_ID_EC]) {
		 
		u8 *equiv = EQUIV_TABLE(dfa);
		 
		while (*str)
			match_char(state, def, base, next, check,
				   equiv[(u8) *str++]);
	} else {
		 
		while (*str)
			match_char(state, def, base, next, check, (u8) *str++);
	}

	return state;
}

 
aa_state_t aa_dfa_next(struct aa_dfa *dfa, aa_state_t state, const char c)
{
	u16 *def = DEFAULT_TABLE(dfa);
	u32 *base = BASE_TABLE(dfa);
	u16 *next = NEXT_TABLE(dfa);
	u16 *check = CHECK_TABLE(dfa);

	 
	if (dfa->tables[YYTD_ID_EC]) {
		 
		u8 *equiv = EQUIV_TABLE(dfa);
		match_char(state, def, base, next, check, equiv[(u8) c]);
	} else
		match_char(state, def, base, next, check, (u8) c);

	return state;
}

aa_state_t aa_dfa_outofband_transition(struct aa_dfa *dfa, aa_state_t state)
{
	u16 *def = DEFAULT_TABLE(dfa);
	u32 *base = BASE_TABLE(dfa);
	u16 *next = NEXT_TABLE(dfa);
	u16 *check = CHECK_TABLE(dfa);
	u32 b = (base)[(state)];

	if (!(b & MATCH_FLAG_OOB_TRANSITION))
		return DFA_NOMATCH;

	 
	match_char(state, def, base, next, check, -1);

	return state;
}

 
aa_state_t aa_dfa_match_until(struct aa_dfa *dfa, aa_state_t start,
				const char *str, const char **retpos)
{
	u16 *def = DEFAULT_TABLE(dfa);
	u32 *base = BASE_TABLE(dfa);
	u16 *next = NEXT_TABLE(dfa);
	u16 *check = CHECK_TABLE(dfa);
	u32 *accept = ACCEPT_TABLE(dfa);
	aa_state_t state = start, pos;

	if (state == DFA_NOMATCH)
		return DFA_NOMATCH;

	 
	if (dfa->tables[YYTD_ID_EC]) {
		 
		u8 *equiv = EQUIV_TABLE(dfa);
		 
		while (*str) {
			pos = base_idx(base[state]) + equiv[(u8) *str++];
			if (check[pos] == state)
				state = next[pos];
			else
				state = def[state];
			if (accept[state])
				break;
		}
	} else {
		 
		while (*str) {
			pos = base_idx(base[state]) + (u8) *str++;
			if (check[pos] == state)
				state = next[pos];
			else
				state = def[state];
			if (accept[state])
				break;
		}
	}

	*retpos = str;
	return state;
}

 
aa_state_t aa_dfa_matchn_until(struct aa_dfa *dfa, aa_state_t start,
				 const char *str, int n, const char **retpos)
{
	u16 *def = DEFAULT_TABLE(dfa);
	u32 *base = BASE_TABLE(dfa);
	u16 *next = NEXT_TABLE(dfa);
	u16 *check = CHECK_TABLE(dfa);
	u32 *accept = ACCEPT_TABLE(dfa);
	aa_state_t state = start, pos;

	*retpos = NULL;
	if (state == DFA_NOMATCH)
		return DFA_NOMATCH;

	 
	if (dfa->tables[YYTD_ID_EC]) {
		 
		u8 *equiv = EQUIV_TABLE(dfa);
		 
		for (; n; n--) {
			pos = base_idx(base[state]) + equiv[(u8) *str++];
			if (check[pos] == state)
				state = next[pos];
			else
				state = def[state];
			if (accept[state])
				break;
		}
	} else {
		 
		for (; n; n--) {
			pos = base_idx(base[state]) + (u8) *str++;
			if (check[pos] == state)
				state = next[pos];
			else
				state = def[state];
			if (accept[state])
				break;
		}
	}

	*retpos = str;
	return state;
}

#define inc_wb_pos(wb)						\
do {								\
	wb->pos = (wb->pos + 1) & (WB_HISTORY_SIZE - 1);		\
	wb->len = (wb->len + 1) & (WB_HISTORY_SIZE - 1);		\
} while (0)

 
static bool is_loop(struct match_workbuf *wb, aa_state_t state,
		    unsigned int *adjust)
{
	aa_state_t pos = wb->pos;
	aa_state_t i;

	if (wb->history[pos] < state)
		return false;

	for (i = 0; i <= wb->len; i++) {
		if (wb->history[pos] == state) {
			*adjust = i;
			return true;
		}
		if (pos == 0)
			pos = WB_HISTORY_SIZE;
		pos--;
	}

	*adjust = i;
	return true;
}

static aa_state_t leftmatch_fb(struct aa_dfa *dfa, aa_state_t start,
				 const char *str, struct match_workbuf *wb,
				 unsigned int *count)
{
	u16 *def = DEFAULT_TABLE(dfa);
	u32 *base = BASE_TABLE(dfa);
	u16 *next = NEXT_TABLE(dfa);
	u16 *check = CHECK_TABLE(dfa);
	aa_state_t state = start, pos;

	AA_BUG(!dfa);
	AA_BUG(!str);
	AA_BUG(!wb);
	AA_BUG(!count);

	*count = 0;
	if (state == DFA_NOMATCH)
		return DFA_NOMATCH;

	 
	if (dfa->tables[YYTD_ID_EC]) {
		 
		u8 *equiv = EQUIV_TABLE(dfa);
		 
		while (*str) {
			unsigned int adjust;

			wb->history[wb->pos] = state;
			pos = base_idx(base[state]) + equiv[(u8) *str++];
			if (check[pos] == state)
				state = next[pos];
			else
				state = def[state];
			if (is_loop(wb, state, &adjust)) {
				state = aa_dfa_match(dfa, state, str);
				*count -= adjust;
				goto out;
			}
			inc_wb_pos(wb);
			(*count)++;
		}
	} else {
		 
		while (*str) {
			unsigned int adjust;

			wb->history[wb->pos] = state;
			pos = base_idx(base[state]) + (u8) *str++;
			if (check[pos] == state)
				state = next[pos];
			else
				state = def[state];
			if (is_loop(wb, state, &adjust)) {
				state = aa_dfa_match(dfa, state, str);
				*count -= adjust;
				goto out;
			}
			inc_wb_pos(wb);
			(*count)++;
		}
	}

out:
	if (!state)
		*count = 0;
	return state;
}

 
aa_state_t aa_dfa_leftmatch(struct aa_dfa *dfa, aa_state_t start,
			    const char *str, unsigned int *count)
{
	DEFINE_MATCH_WB(wb);

	 

	return leftmatch_fb(dfa, start, str, &wb, count);
}
