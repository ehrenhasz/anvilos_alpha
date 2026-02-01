 

 

#ifndef _TERM_PRIV_H
#define _TERM_PRIV_H 1
 

#ifdef __cplusplus
extern "C" {
#endif

#include <ncurses_cfg.h>

#undef NCURSES_OPAQUE
#define NCURSES_INTERNALS 1
#define NCURSES_OPAQUE 0

#include <limits.h>		 
#include <signal.h>		 
#include <time.h>		 
#include <term.h>		 

#ifdef USE_PTHREADS
#if USE_REENTRANT
#include <pthread.h>
#endif
#endif

 
#define STACKSIZE 20

typedef struct {
	union {
		int	num;
		char *	str;
	} data;
	bool num_type;
} STACK_FRAME;

#define NUM_VARS 26

typedef struct {
	const char *	tparam_base;

	STACK_FRAME	stack[STACKSIZE];
	int		stack_ptr;

	char *		out_buff;
	size_t		out_size;
	size_t		out_used;

	char *		fmt_buff;
	size_t		fmt_size;

	int		static_vars[NUM_VARS];
#ifdef TRACE
	const char *	tname;
#endif
} TPARM_STATE;

typedef struct {
	char *		text;
	size_t		size;
} TRACEBUF;

typedef struct {
	const char *	name;
	char *		value;
} ITERATOR_VARS;

 
typedef struct term {			 
	TERMTYPE	type;		 
	short		Filedes;	 
	TTY		Ottyb;		 
	TTY		Nttyb;		 
	int		_baudrate;	 
	char *		_termname;	 
	TPARM_STATE	tparm_state;
#if NCURSES_EXT_COLORS
	TERMTYPE2	type2;		 
#endif
#undef TERMINAL
} TERMINAL;

 
typedef	struct {
	WINDOW *	win;		 
	int		line;		 
	int		(*hook)(WINDOW *, int);  
} ripoff_t;

 
typedef struct {
	long		sequence;
	bool		last_used;
	char *		fix_sgr0;	 
	char *		last_bufp;	 
	TERMINAL *	last_term;
} TGETENT_CACHE;

#define TGETENT_MAX 4

#include <term_entry.h>		 

#ifdef USE_TERM_DRIVER
struct DriverTCB;  
#endif

 
typedef struct {
	SIG_ATOMIC_T	have_sigtstp;
	SIG_ATOMIC_T	have_sigwinch;
	SIG_ATOMIC_T	cleanup_nested;

	bool		init_signals;
	bool		init_screen;

	char *		comp_sourcename;
	char *		comp_termtype;

	bool		have_tic_directory;
	bool		keep_tic_directory;
	const char *	tic_directory;

	char *		dbi_list;
	int		dbi_size;

	char *		first_name;
	char **		keyname_table;
	int		init_keyname;

	int		slk_format;

	int		getstr_limit;	 

	char *		safeprint_buf;
	size_t		safeprint_used;

	TGETENT_CACHE	tgetent_cache[TGETENT_MAX];
	int		tgetent_index;
	long		tgetent_sequence;
	int		terminal_count;

	char *		dbd_blob;	 
	char **		dbd_list;	 
	int		dbd_size;	 
	time_t		dbd_time;	 
	ITERATOR_VARS	dbd_vars[dbdLAST];

#if HAVE_TSEARCH
	void *		cached_tparm;
	int		count_tparm;
#endif  

#ifdef USE_TERM_DRIVER
	int		(*term_driver)(struct DriverTCB*, const char*, int*);
#endif

#define WINDOWLIST struct _win_list

#ifndef USE_SP_WINDOWLIST
	WINDOWLIST *	_nc_windowlist;
#define WindowList(sp)	_nc_globals._nc_windowlist
#endif

#if USE_HOME_TERMINFO
	char *		home_terminfo;
#endif

#if !USE_SAFE_SPRINTF
	int		safeprint_cols;
	int		safeprint_rows;
#endif

#ifdef USE_PTHREADS
	pthread_mutex_t	mutex_curses;
	pthread_mutex_t	mutex_prescreen;
	pthread_mutex_t	mutex_screen;
	pthread_mutex_t	mutex_update;
	pthread_mutex_t	mutex_tst_tracef;
	pthread_mutex_t	mutex_tracef;
	int		nested_tracef;
	int		use_pthreads;
#define _nc_use_pthreads	_nc_globals.use_pthreads
#if USE_PTHREADS_EINTR
	pthread_t	read_thread;	 
#endif
#endif
#if USE_WIDEC_SUPPORT
	char		key_name[MB_LEN_MAX + 1];
#endif

#ifdef TRACE
	bool		trace_opened;
	char		trace_fname[PATH_MAX];
	int		trace_level;
	FILE *		trace_fp;
	int		trace_fd;

	char *		tracearg_buf;
	size_t		tracearg_used;

	TRACEBUF *	tracebuf_ptr;
	size_t		tracebuf_used;

	char		tracechr_buf[40];

	char *		tracedmp_buf;
	size_t		tracedmp_used;

	unsigned char *	tracetry_buf;
	size_t		tracetry_used;

	char		traceatr_color_buf[2][80];
	int		traceatr_color_sel;
	int		traceatr_color_last;
#if !defined(USE_PTHREADS) && USE_REENTRANT
	int		nested_tracef;
#endif
#endif	 

#if NO_LEAKS
	bool		leak_checking;
#endif
} NCURSES_GLOBALS;

extern NCURSES_EXPORT_VAR(NCURSES_GLOBALS) _nc_globals;

#define N_RIPS 5

#ifdef USE_PTHREADS
typedef struct _prescreen_list {
	struct _prescreen_list *next;
	pthread_t	id;
	struct screen *	sp;
} PRESCREEN_LIST;
#endif

 
typedef struct {
#ifdef USE_PTHREADS
	PRESCREEN_LIST *allocated;
#else
	struct screen * allocated;
#endif
	bool		use_env;
	bool		filter_mode;
	attr_t		previous_attr;
	TPARM_STATE	tparm_state;
	TTY *		saved_tty;	 
	bool		use_tioctl;
	NCURSES_SP_OUTC	_outch;		 
#ifndef USE_SP_RIPOFF
	ripoff_t	rippedoff[N_RIPS];
	ripoff_t *	rsp;
#endif
#if NCURSES_NO_PADDING
	bool		_no_padding;	 
#endif
#if BROKEN_LINKER || USE_REENTRANT
	chtype *	real_acs_map;
	int		_LINES;
	int		_COLS;
	int		_TABSIZE;
	int		_ESCDELAY;
	TERMINAL *	_cur_term;
#endif
#ifdef TRACE
#if BROKEN_LINKER || USE_REENTRANT
	long		_outchars;
	const char *	_tputs_trace;
#endif
#endif
} NCURSES_PRESCREEN;

extern NCURSES_EXPORT_VAR(NCURSES_PRESCREEN) _nc_prescreen;

extern NCURSES_EXPORT(void) _nc_free_tparm(TERMINAL*);

#ifdef __cplusplus
}
#endif

 

#endif  
