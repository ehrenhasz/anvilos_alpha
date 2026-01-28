#ifndef __AA_MATCH_H
#define __AA_MATCH_H
#include <linux/kref.h>
#define DFA_NOMATCH			0
#define DFA_START			1
#define YYTH_MAGIC	0x1B5E783D
#define YYTH_FLAG_DIFF_ENCODE	1
#define YYTH_FLAG_OOB_TRANS	2
#define YYTH_FLAGS (YYTH_FLAG_DIFF_ENCODE | YYTH_FLAG_OOB_TRANS)
#define MAX_OOB_SUPPORTED	1
struct table_set_header {
	u32 th_magic;		 
	u32 th_hsize;
	u32 th_ssize;
	u16 th_flags;
	char th_version[];
};
#define	YYTD_ID_ACCEPT	0
#define YYTD_ID_BASE	1
#define YYTD_ID_CHK	2
#define YYTD_ID_DEF	3
#define YYTD_ID_EC	4
#define YYTD_ID_META	5
#define YYTD_ID_ACCEPT2 6
#define YYTD_ID_NXT	7
#define YYTD_ID_TSIZE	8
#define YYTD_ID_MAX	8
#define YYTD_DATA8	1
#define YYTD_DATA16	2
#define YYTD_DATA32	4
#define YYTD_DATA64	8
#define ACCEPT1_FLAGS(X) ((X) & 0x3f)
#define ACCEPT2_FLAGS(X) ACCEPT1_FLAGS((X) >> YYTD_ID_ACCEPT2)
#define TO_ACCEPT1_FLAG(X) ACCEPT1_FLAGS(X)
#define TO_ACCEPT2_FLAG(X) (ACCEPT1_FLAGS(X) << YYTD_ID_ACCEPT2)
#define DFA_FLAG_VERIFY_STATES 0x1000
struct table_header {
	u16 td_id;
	u16 td_flags;
	u32 td_hilen;
	u32 td_lolen;
	char td_data[];
};
#define DEFAULT_TABLE(DFA) ((u16 *)((DFA)->tables[YYTD_ID_DEF]->td_data))
#define BASE_TABLE(DFA) ((u32 *)((DFA)->tables[YYTD_ID_BASE]->td_data))
#define NEXT_TABLE(DFA) ((u16 *)((DFA)->tables[YYTD_ID_NXT]->td_data))
#define CHECK_TABLE(DFA) ((u16 *)((DFA)->tables[YYTD_ID_CHK]->td_data))
#define EQUIV_TABLE(DFA) ((u8 *)((DFA)->tables[YYTD_ID_EC]->td_data))
#define ACCEPT_TABLE(DFA) ((u32 *)((DFA)->tables[YYTD_ID_ACCEPT]->td_data))
#define ACCEPT_TABLE2(DFA) ((u32 *)((DFA)->tables[YYTD_ID_ACCEPT2]->td_data))
struct aa_dfa {
	struct kref count;
	u16 flags;
	u32 max_oob;
	struct table_header *tables[YYTD_ID_TSIZE];
};
extern struct aa_dfa *nulldfa;
extern struct aa_dfa *stacksplitdfa;
#define byte_to_byte(X) (X)
#define UNPACK_ARRAY(TABLE, BLOB, LEN, TTYPE, BTYPE, NTOHX)	\
	do { \
		typeof(LEN) __i; \
		TTYPE *__t = (TTYPE *) TABLE; \
		BTYPE *__b = (BTYPE *) BLOB; \
		for (__i = 0; __i < LEN; __i++) { \
			__t[__i] = NTOHX(__b[__i]); \
		} \
	} while (0)
static inline size_t table_size(size_t len, size_t el_size)
{
	return ALIGN(sizeof(struct table_header) + len * el_size, 8);
}
int aa_setup_dfa_engine(void);
void aa_teardown_dfa_engine(void);
#define aa_state_t unsigned int
struct aa_dfa *aa_dfa_unpack(void *blob, size_t size, int flags);
aa_state_t aa_dfa_match_len(struct aa_dfa *dfa, aa_state_t start,
			    const char *str, int len);
aa_state_t aa_dfa_match(struct aa_dfa *dfa, aa_state_t start,
			const char *str);
aa_state_t aa_dfa_next(struct aa_dfa *dfa, aa_state_t state, const char c);
aa_state_t aa_dfa_outofband_transition(struct aa_dfa *dfa, aa_state_t state);
aa_state_t aa_dfa_match_until(struct aa_dfa *dfa, aa_state_t start,
			      const char *str, const char **retpos);
aa_state_t aa_dfa_matchn_until(struct aa_dfa *dfa, aa_state_t start,
			       const char *str, int n, const char **retpos);
void aa_dfa_free_kref(struct kref *kref);
#define WB_HISTORY_SIZE 24
struct match_workbuf {
	unsigned int count;
	unsigned int pos;
	unsigned int len;
	unsigned int size;	 
	unsigned int history[WB_HISTORY_SIZE];
};
#define DEFINE_MATCH_WB(N)		\
struct match_workbuf N = {		\
	.count = 0,			\
	.pos = 0,			\
	.len = 0,			\
}
aa_state_t aa_dfa_leftmatch(struct aa_dfa *dfa, aa_state_t start,
			    const char *str, unsigned int *count);
static inline struct aa_dfa *aa_get_dfa(struct aa_dfa *dfa)
{
	if (dfa)
		kref_get(&(dfa->count));
	return dfa;
}
static inline void aa_put_dfa(struct aa_dfa *dfa)
{
	if (dfa)
		kref_put(&dfa->count, aa_dfa_free_kref);
}
#define MATCH_FLAG_DIFF_ENCODE 0x80000000
#define MARK_DIFF_ENCODE 0x40000000
#define MATCH_FLAG_OOB_TRANSITION 0x20000000
#define MATCH_FLAGS_MASK 0xff000000
#define MATCH_FLAGS_VALID (MATCH_FLAG_DIFF_ENCODE | MATCH_FLAG_OOB_TRANSITION)
#define MATCH_FLAGS_INVALID (MATCH_FLAGS_MASK & ~MATCH_FLAGS_VALID)
#endif  
