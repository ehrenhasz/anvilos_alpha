 

 

 

 

#ifndef NCURSES_TERM_ENTRY_H_incl
#define NCURSES_TERM_ENTRY_H_incl 1
 

#ifdef __cplusplus
extern "C" {
#endif

#include <curses.h>
#include <term.h>

 
#if NCURSES_XNAMES
#define NUM_BOOLEANS(tp) (tp)->num_Booleans
#define NUM_NUMBERS(tp)  (tp)->num_Numbers
#define NUM_STRINGS(tp)  (tp)->num_Strings
#define EXT_NAMES(tp,i,limit,index,table) (i >= limit) ? tp->ext_Names[index] : table[i]
#else
#define NUM_BOOLEANS(tp) BOOLCOUNT
#define NUM_NUMBERS(tp)  NUMCOUNT
#define NUM_STRINGS(tp)  STRCOUNT
#define EXT_NAMES(tp,i,limit,index,table) table[i]
#endif

#define NUM_EXT_NAMES(tp) (unsigned) ((tp)->ext_Booleans + (tp)->ext_Numbers + (tp)->ext_Strings)

#define for_each_boolean(n,tp) for(n = 0; n < NUM_BOOLEANS(tp); n++)
#define for_each_number(n,tp)  for(n = 0; n < NUM_NUMBERS(tp);  n++)
#define for_each_string(n,tp)  for(n = 0; n < NUM_STRINGS(tp);  n++)

#if NCURSES_XNAMES
#define for_each_ext_boolean(n,tp) for(n = BOOLCOUNT; (int) n < (int) NUM_BOOLEANS(tp); n++)
#define for_each_ext_number(n,tp)  for(n = NUMCOUNT; (int) n < (int) NUM_NUMBERS(tp);  n++)
#define for_each_ext_string(n,tp)  for(n = STRCOUNT; (int) n < (int) NUM_STRINGS(tp);  n++)
#endif

#define ExtBoolname(tp,i,names) EXT_NAMES(tp, i, BOOLCOUNT, (i - (tp->num_Booleans - tp->ext_Booleans)), names)
#define ExtNumname(tp,i,names)  EXT_NAMES(tp, i, NUMCOUNT, (i - (tp->num_Numbers - tp->ext_Numbers)) + tp->ext_Booleans, names)
#define ExtStrname(tp,i,names)  EXT_NAMES(tp, i, STRCOUNT, (i - (tp->num_Strings - tp->ext_Strings)) + (tp->ext_Numbers + tp->ext_Booleans), names)

 
#ifdef NCURSES_INTERNALS

 
typedef enum {
	dbdTIC = 0,		 
#if NCURSES_USE_DATABASE
	dbdEnvOnce,		 
	dbdHome,		 
	dbdEnvList,		 
	dbdCfgList,		 
	dbdCfgOnce,		 
#endif
#if NCURSES_USE_TERMCAP
	dbdEnvOnce2,		 
	dbdEnvList2,		 
	dbdCfgList2,		 
#endif
	dbdLAST
} DBDIRS;

#define MAX_USES	32
#define MAX_CROSSLINKS	16

typedef struct entry ENTRY;

typedef struct {
	char *name;
	ENTRY *link;
	long line;
} ENTRY_USES;

struct entry {
	TERMTYPE2 tterm;
	unsigned nuses;
	ENTRY_USES uses[MAX_USES];
	int ncrosslinks;
	ENTRY *crosslinks[MAX_CROSSLINKS];
	long cstart;
	long cend;
	long startline;
	ENTRY *next;
	ENTRY *last;
};

extern NCURSES_EXPORT_VAR(ENTRY *) _nc_head;
extern NCURSES_EXPORT_VAR(ENTRY *) _nc_tail;
#define for_entry_list(qp)	for (qp = _nc_head; qp; qp = qp->next)
#define for_entry_list2(qp,q0)	for (qp = q0; qp; qp = qp->next)

#define MAX_LINE	132

#define NULLHOOK        (bool(*)(ENTRY *))0

 
#define WANTED(s)	((s) == ABSENT_STRING)
#define PRESENT(s)	(((s) != ABSENT_STRING) && ((s) != CANCELLED_STRING))

#define ANDMISSING(p,q) \
		{ \
		if (PRESENT(p) && !PRESENT(q)) \
			_nc_warning(#p " but no " #q); \
		}

#define PAIRED(p,q) \
		{ \
		if (PRESENT(q) && !PRESENT(p)) \
			_nc_warning(#q " but no " #p); \
		if (PRESENT(p) && !PRESENT(q)) \
			_nc_warning(#p " but no " #q); \
		}

 

 
extern NCURSES_EXPORT(ENTRY *) _nc_copy_entry (ENTRY *oldp);
extern NCURSES_EXPORT(char *) _nc_save_str (const char *const);
extern NCURSES_EXPORT(void) _nc_init_entry (ENTRY *const);
extern NCURSES_EXPORT(void) _nc_merge_entry (ENTRY *const, ENTRY *const);
extern NCURSES_EXPORT(void) _nc_wrap_entry (ENTRY *const, bool);

 
extern NCURSES_EXPORT(void) _nc_align_termtype (TERMTYPE2 *, TERMTYPE2 *);

 
extern NCURSES_EXPORT(void) _nc_free_termtype1 (TERMTYPE *);
extern NCURSES_EXPORT(void) _nc_free_termtype2 (TERMTYPE2 *);

 
extern NCURSES_EXPORT(char *) _nc_trim_sgr0 (TERMTYPE2 *);

 
#if NCURSES_XNAMES
extern NCURSES_EXPORT_VAR(bool) _nc_user_definable;
extern NCURSES_EXPORT_VAR(bool) _nc_disable_period;
#endif
extern NCURSES_EXPORT(int) _nc_parse_entry (ENTRY *, int, bool);
extern NCURSES_EXPORT(int) _nc_capcmp (const char *, const char *);

 
extern NCURSES_EXPORT(void) _nc_set_writedir (const char *);
extern NCURSES_EXPORT(void) _nc_write_entry (TERMTYPE2 *const);
extern NCURSES_EXPORT(int) _nc_write_object (TERMTYPE2 *, char *, unsigned *, unsigned);

 
extern NCURSES_EXPORT(void) _nc_read_entry_source (FILE*, char*, int, bool, bool (*)(ENTRY*));
extern NCURSES_EXPORT(bool) _nc_entry_match (char *, char *);
extern NCURSES_EXPORT(int) _nc_resolve_uses (bool);  
extern NCURSES_EXPORT(int) _nc_resolve_uses2 (bool, bool);
extern NCURSES_EXPORT(void) _nc_free_entries (ENTRY *);
extern NCURSES_IMPEXP void (NCURSES_API *_nc_check_termtype)(TERMTYPE *);  
extern NCURSES_IMPEXP void (NCURSES_API *_nc_check_termtype2)(TERMTYPE2 *, bool);

 
extern NCURSES_EXPORT(void) _nc_trace_xnames (TERMTYPE *);

#endif  

 

#undef  NCURSES_TACK_1_08
#ifdef  NCURSES_INTERNALS
#define NCURSES_TACK_1_08  
#else
#define NCURSES_TACK_1_08 GCC_DEPRECATED("upgrade to tack 1.08")
#endif

 
extern NCURSES_EXPORT(void) _nc_copy_termtype (TERMTYPE *, const TERMTYPE *) NCURSES_TACK_1_08;

 
extern NCURSES_EXPORT(void) _nc_init_acs (void) NCURSES_TACK_1_08;	 

 
extern NCURSES_EXPORT(void) _nc_free_termtype (TERMTYPE *) NCURSES_TACK_1_08;

#ifdef __cplusplus
}
#endif

 

#endif  
