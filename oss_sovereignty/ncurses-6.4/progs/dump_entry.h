 

 

 

#ifndef DUMP_ENTRY_H
#define DUMP_ENTRY_H 1

#define NCURSES_OPAQUE    0
#define NCURSES_INTERNALS 1
#include <curses.h>
#include <term.h>

 
#define F_TERMINFO	0	 
#define F_VARIABLE	1	 
#define F_TERMCAP	2	 
#define F_TCONVERR	3	 
#define F_LITERAL	4	 

 
#define S_DEFAULT	0	 
#define S_NOSORT	1	 
#define S_TERMINFO	2	 
#define S_VARIABLE	3	 
#define S_TERMCAP	4	 

 
#define CMP_BOOLEAN	0	 
#define CMP_NUMBER	1	 
#define CMP_STRING	2	 
#define CMP_USE		3	 

#ifndef _TERMSORT_H
typedef unsigned PredType;
typedef unsigned PredIdx;
#endif

typedef int (*PredFunc) (PredType, PredIdx);
typedef void (*PredHook) (PredType, PredIdx, const char *);

extern NCURSES_CONST char *nametrans(const char *);
extern bool has_params(const char *, bool);
extern int fmt_entry(TERMTYPE2 *, PredFunc, int, int, int, int);
extern int show_entry(void);
extern void compare_entry(PredHook, TERMTYPE2 *, bool);
extern void dump_entry(TERMTYPE2 *, int, int, int, PredFunc);
extern void dump_init(const char *, int, int, bool, int, int, unsigned, bool,
		      bool, int);
extern void dump_uses(const char *, bool);
extern void repair_acsc(TERMTYPE2 *tp);

#define L_CURL "{"
#define R_CURL "}"

#define FAIL	-1

#endif  
