 

 

#if !defined (_HISTLIB_H_)
#define _HISTLIB_H_

#if defined (HAVE_STRING_H)
#  include <string.h>
#else
#  include <strings.h>
#endif  

#if !defined (STREQ)
#define STREQ(a, b)	(((a)[0] == (b)[0]) && (strcmp ((a), (b)) == 0))
#define STREQN(a, b, n) (((n) == 0) ? (1) \
				    : ((a)[0] == (b)[0]) && (strncmp ((a), (b), (n)) == 0))
#endif

#ifndef savestring
#define savestring(x) strcpy (xmalloc (1 + strlen (x)), (x))
#endif

#ifndef whitespace
#define whitespace(c) (((c) == ' ') || ((c) == '\t'))
#endif

#ifndef _rl_digit_p
#define _rl_digit_p(c)  ((c) >= '0' && (c) <= '9')
#endif

#ifndef _rl_digit_value
#define _rl_digit_value(c) ((c) - '0')
#endif

#ifndef member
#  if !defined (strchr) && !defined (__STDC__)
extern char *strchr ();
#  endif  
#define member(c, s) ((c) ? ((char *)strchr ((s), (c)) != (char *)NULL) : 0)
#endif

#ifndef FREE
#  define FREE(x)	if (x) free (x)
#endif

 
#define EVENT_NOT_FOUND 0
#define BAD_WORD_SPEC	1
#define SUBST_FAILED	2
#define BAD_MODIFIER	3
#define NO_PREV_SUBST	4

 
#define NON_ANCHORED_SEARCH	0
#define ANCHORED_SEARCH		0x01
#define PATTERN_SEARCH		0x02

 
#define HISTORY_APPEND 0
#define HISTORY_OVERWRITE 1

 

 
extern int _hs_history_patsearch (const char *, int, int);

 
extern void _hs_replace_history_data (int, histdata_t *, histdata_t *);
extern int _hs_at_end_of_history (void);

 
extern void _hs_append_history_line (int, const char *);

#endif  
