

#ifndef _REGEXP_H
#define _REGEXP_H


#define NSUBEXP  10


#define NFA_MAX_BRACES 20


#define NFA_MAX_STATES 100000
#define NFA_TOO_EXPENSIVE (-1)



#define	    AUTOMATIC_ENGINE	0
#define	    BACKTRACKING_ENGINE	1
#define	    NFA_ENGINE		2

typedef struct regengine regengine_T;


typedef struct regprog
{
    regengine_T		*engine;
    unsigned		regflags;
    unsigned		re_engine;   
    unsigned		re_flags;    
    int			re_in_use;   
} regprog_T;


typedef struct
{
    
    regengine_T		*engine;
    unsigned		regflags;
    unsigned		re_engine;
    unsigned		re_flags;
    int			re_in_use;

    int			regstart;
    char_u		reganch;
    char_u		*regmust;
    int			regmlen;
#ifdef FEAT_SYN_HL
    char_u		reghasz;
#endif
    char_u		program[1];	
} bt_regprog_T;


typedef struct nfa_state nfa_state_T;
struct nfa_state
{
    int			c;
    nfa_state_T		*out;
    nfa_state_T		*out1;
    int			id;
    int			lastlist[2]; 
    int			val;
};


typedef struct
{
    
    regengine_T		*engine;
    unsigned		regflags;
    unsigned		re_engine;
    unsigned		re_flags;
    int			re_in_use;

    nfa_state_T		*start;		

    int			reganch;	
    int			regstart;	
    char_u		*match_text;	

    int			has_zend;	
    int			has_backref;	
#ifdef FEAT_SYN_HL
    int			reghasz;
#endif
    char_u		*pattern;
    int			nsubexp;	
    int			nstate;
    nfa_state_T		state[1];	
} nfa_regprog_T;


typedef struct
{
    regprog_T		*regprog;
    char_u		*startp[NSUBEXP];
    char_u		*endp[NSUBEXP];

    colnr_T		rm_matchcol;   
    int			rm_ic;
} regmatch_T;


typedef struct
{
    regprog_T		*regprog;
    lpos_T		startpos[NSUBEXP];
    lpos_T		endpos[NSUBEXP];

    colnr_T		rmm_matchcol;   
    int			rmm_ic;
    colnr_T		rmm_maxcol;	
} regmmatch_T;


typedef struct
{
    short		refcnt;
    char_u		*matches[NSUBEXP];
} reg_extmatch_T;

struct regengine
{
    
    regprog_T	*(*regcomp)(char_u*, int);
    
    void	(*regfree)(regprog_T *);
    
    int		(*regexec_nl)(regmatch_T *, char_u *, colnr_T, int);
    
    long	(*regexec_multi)(regmmatch_T *, win_T *, buf_T *, linenr_T, colnr_T, int *);
    
};


#define REGSUB_COPY	    1
#define REGSUB_MAGIC	    2
#define REGSUB_BACKSLASH    4

#endif	
