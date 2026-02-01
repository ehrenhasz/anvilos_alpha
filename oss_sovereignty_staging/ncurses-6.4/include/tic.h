 

 

 

#ifndef __TIC_H
#define __TIC_H
 
#ifdef __cplusplus
extern "C" {
#endif

#include <ncurses_cfg.h>

#include <curses.h>	 

 

#define MAGIC		0432	 
#define MAGIC2		01036	 

#undef  BYTE
#define BYTE(p,n)	(unsigned char)((p)[n])

#define IS_NEG1(p)	((BYTE(p,0) == 0377) && (BYTE(p,1) == 0377))
#define IS_NEG2(p)	((BYTE(p,0) == 0376) && (BYTE(p,1) == 0377))
#define LOW_MSB(p)	(BYTE(p,0) + 256*BYTE(p,1))

#define IS_TIC_MAGIC(p)	(LOW_MSB(p) == MAGIC || LOW_MSB(p) == MAGIC2)

#define quick_prefix(s) (!strncmp((s), "b64:", (size_t)4) || !strncmp((s), "hex:", (size_t)4))

 
#define MAX_NAME_SIZE	512	 
#define MAX_ENTRY_SIZE1	4096	 
#define MAX_ENTRY_SIZE2	32768	 

#if NCURSES_EXT_COLORS && HAVE_INIT_EXTENDED_COLOR
#define MAX_ENTRY_SIZE MAX_ENTRY_SIZE2
#else
#define MAX_ENTRY_SIZE MAX_ENTRY_SIZE1
#endif

 
#if HAVE_LONG_FILE_NAMES
#define MAX_ALIAS	32	 
#else
#define MAX_ALIAS	14	 
#endif

 
#define PRIVATE_INFO	"%s/.terminfo"	 

 

#define MAX_DEBUG_LEVEL 15
#define DEBUG_LEVEL(n)	((n) << TRACE_SHIFT)

#define set_trace_level(n) \
	_nc_tracing &= TRACE_MAXIMUM, \
	_nc_tracing |= DEBUG_LEVEL(n)

#ifdef TRACE
#define DEBUG(n, a)	if (_nc_tracing >= DEBUG_LEVEL(n)) _tracef a
#else
#define DEBUG(n, a)	 
#endif

 

#define BOOLEAN 0		 
#define NUMBER 1		 
#define STRING 2		 
#define CANCEL 3		 
#define NAMES  4		 
#define UNDEF  5		 

#define NO_PUSHBACK	-1	 

 

struct token
{
	char	*tk_name;	 
	int	tk_valnumber;	 
	char	*tk_valstring;	 
};

 
struct tinfo_fkeys {
	unsigned offset;
	chtype code;
	};

typedef short HashValue;

 
struct name_table_entry
{
	const char *nte_name;	 
	int	nte_type;	 
	HashValue nte_index;	 
	HashValue nte_link;	 
};

 
typedef struct {
	unsigned table_size;
	const HashValue *table_data;
	HashValue (*hash_of)(const char *);
	int (*compare_names)(const char *, const char *);
} HashData;

struct alias
{
	const char	*from;
	const char	*to;
	const char	*source;
};

#define NOTFOUND	((struct name_table_entry *) 0)

 
struct user_table_entry
{
	const char *ute_name;	 
	int	ute_type;	 
	unsigned ute_argc;	 
	unsigned ute_args;	 
	HashValue ute_index;	 
	HashValue ute_link;	 
};

 

 
#define ABSENT_BOOLEAN		((signed char)-1)	 
#define ABSENT_NUMERIC		(-1)
#define ABSENT_STRING		(char *)0

 
#define CANCELLED_BOOLEAN	((signed char)-2)	 
#define CANCELLED_NUMERIC	(-2)
#define CANCELLED_STRING	(char *)(-1)

#define VALID_BOOLEAN(s) ((unsigned char)(s) <= 1)  
#define VALID_NUMERIC(s) ((s) >= 0)
#define VALID_STRING(s)  ((s) != CANCELLED_STRING && (s) != ABSENT_STRING)

 
#define MAX_TERMCAP_LENGTH	1023

 
#define MAX_TERMINFO_LENGTH	4096

#ifndef TERMINFO
#define TERMINFO "/usr/share/terminfo"
#endif

#ifdef NCURSES_TERM_ENTRY_H_incl

 
#ifdef NCURSES_INTERNALS
 
extern NCURSES_EXPORT(unsigned) _nc_pathlast (const char *);
extern NCURSES_EXPORT(bool) _nc_is_abs_path (const char *);
extern NCURSES_EXPORT(bool) _nc_is_dir_path (const char *);
extern NCURSES_EXPORT(bool) _nc_is_file_path (const char *);
extern NCURSES_EXPORT(char *) _nc_basename (char *);
extern NCURSES_EXPORT(char *) _nc_rootname (char *);

 
extern NCURSES_EXPORT(const struct name_table_entry *) _nc_get_table (bool);
extern NCURSES_EXPORT(const HashData *) _nc_get_hash_info (bool);
extern NCURSES_EXPORT(const struct alias *) _nc_get_alias_table (bool);

 
extern NCURSES_EXPORT(struct name_table_entry const *) _nc_find_type_entry
	(const char *, int, bool);
extern NCURSES_EXPORT(struct user_table_entry const *) _nc_find_user_entry
	(const char *);

 
extern NCURSES_EXPORT(int)  _nc_get_token (bool);
extern NCURSES_EXPORT(void) _nc_panic_mode (char);
extern NCURSES_EXPORT(void) _nc_push_token (int);
extern NCURSES_EXPORT_VAR(int) _nc_curr_col;
extern NCURSES_EXPORT_VAR(int) _nc_curr_line;
extern NCURSES_EXPORT_VAR(int) _nc_syntax;
extern NCURSES_EXPORT_VAR(int) _nc_strict_bsd;
extern NCURSES_EXPORT_VAR(long) _nc_comment_end;
extern NCURSES_EXPORT_VAR(long) _nc_comment_start;
extern NCURSES_EXPORT_VAR(long) _nc_curr_file_pos;
extern NCURSES_EXPORT_VAR(long) _nc_start_line;
#define SYN_TERMINFO	0
#define SYN_TERMCAP	1

 
extern NCURSES_EXPORT(const char *) _nc_get_source (void);
extern GCC_NORETURN NCURSES_EXPORT(void) _nc_err_abort (const char *const,...) GCC_PRINTFLIKE(1,2);
extern NCURSES_EXPORT(void) _nc_get_type (char *name);
extern NCURSES_EXPORT(void) _nc_set_source (const char *const);
extern NCURSES_EXPORT(void) _nc_set_type (const char *const);
extern GCC_NORETURN NCURSES_EXPORT(void) _nc_syserr_abort (const char *const,...) GCC_PRINTFLIKE(1,2);
extern NCURSES_EXPORT(void) _nc_warning (const char *const,...) GCC_PRINTFLIKE(1,2);
extern NCURSES_EXPORT_VAR(bool) _nc_suppress_warnings;

 
extern NCURSES_EXPORT_VAR(struct token)	_nc_curr_token;

 
NCURSES_EXPORT(const struct user_table_entry *) _nc_get_userdefs_table (void);
NCURSES_EXPORT(const HashData *) _nc_get_hash_user (void);

 
extern NCURSES_EXPORT(char *) _nc_captoinfo (const char *, const char *, int const);
extern NCURSES_EXPORT(char *) _nc_infotocap (const char *, const char *, int const);

 
extern NCURSES_EXPORT(char *) _nc_home_terminfo (void);

 
#if	BROKEN_LINKER
#define	_nc_tinfo_fkeys	_nc_tinfo_fkeysf()
extern NCURSES_EXPORT(const struct tinfo_fkeys *) _nc_tinfo_fkeysf (void);
#else
extern NCURSES_EXPORT_VAR(const struct tinfo_fkeys) _nc_tinfo_fkeys[];
#endif

 
#define NUM_PARM 9

extern NCURSES_EXPORT_VAR(int) _nc_tparm_err;

extern NCURSES_EXPORT(int) _nc_tparm_analyze(TERMINAL *, const char *, char **, int *);
extern NCURSES_EXPORT(void) _nc_reset_tparm(TERMINAL *);

 
extern NCURSES_EXPORT_VAR(unsigned) _nc_tracing;
extern NCURSES_EXPORT(const char *) _nc_visbuf (const char *);
extern NCURSES_EXPORT(const char *) _nc_visbuf2 (int, const char *);

 
extern NCURSES_EXPORT_VAR(int) _nc_nulls_sent;	 

 
extern const char * _nc_progname;

 
extern NCURSES_EXPORT(const char *) _nc_next_db(DBDIRS *, int *);
extern NCURSES_EXPORT(const char *) _nc_tic_dir (const char *);
extern NCURSES_EXPORT(void) _nc_first_db(DBDIRS *, int *);
extern NCURSES_EXPORT(void) _nc_last_db(void);

 
extern NCURSES_EXPORT(int) _nc_tic_written (void);

#endif  

 

#undef  NCURSES_TACK_1_08
#ifdef  NCURSES_INTERNALS
#define NCURSES_TACK_1_08  
#else
#define NCURSES_TACK_1_08 GCC_DEPRECATED("upgrade to tack 1.08")
#endif

 
extern NCURSES_EXPORT(struct name_table_entry const *) _nc_find_entry
	(const char *, const HashValue *) NCURSES_TACK_1_08;
extern NCURSES_EXPORT(const HashValue *) _nc_get_hash_table (bool) NCURSES_TACK_1_08;

 
extern NCURSES_EXPORT(void) _nc_reset_input (FILE *, char *) NCURSES_TACK_1_08;

 
extern NCURSES_EXPORT(char *) _nc_tic_expand (const char *, bool, int) NCURSES_TACK_1_08;

 
extern NCURSES_EXPORT(int) _nc_trans_string (char *, char *) NCURSES_TACK_1_08;

#endif  

#ifdef __cplusplus
}
#endif

 
#endif  
