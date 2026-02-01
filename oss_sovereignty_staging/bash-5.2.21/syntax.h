 

 

#ifndef _SYNTAX_H_
#define _SYNTAX_H_

 

#define slashify_in_quotes "\\`$\"\n"
#define slashify_in_here_document "\\`$"

#define shell_meta_chars   "()<>;&|"
#define shell_break_chars  "()<>;&| \t\n"

#define shell_quote_chars	"\"`'"

#if defined (PROCESS_SUBSTITUTION)
#  define shell_exp_chars		"$<>"
#else
#  define shell_exp_chars		"$"
#endif

#if defined (EXTENDED_GLOB)
#  define ext_glob_chars	"@*+?!"
#else
#  define ext_glob_chars	""
#endif
#define shell_glob_chars	"*?[]^"

 

 

#define CWORD		0x0000	 
#define CSHMETA		0x0001	 
#define CSHBRK		0x0002	 
#define CBACKQ		0x0004	 
#define CQUOTE		0x0008	 
#define CSPECL		0x0010	 
#define CEXP		0x0020	 
#define CBSDQUOTE	0x0040	 
#define CBSHDOC		0x0080	 
#define CGLOB		0x0100	 
#define CXGLOB		0x0200	 
#define CXQUOTE		0x0400	 
#define CSPECVAR	0x0800	 
#define CSUBSTOP	0x1000	 
#define CBLANK		0x2000	 

 
extern int sh_syntaxtab[];
extern int sh_syntabsiz;

#define shellmeta(c)	(sh_syntaxtab[(unsigned char)(c)] & CSHMETA)
#define shellbreak(c)	(sh_syntaxtab[(unsigned char)(c)] & CSHBRK)
#define shellquote(c)	(sh_syntaxtab[(unsigned char)(c)] & CQUOTE)
#define shellxquote(c)	(sh_syntaxtab[(unsigned char)(c)] & CXQUOTE)

#define shellblank(c)	(sh_syntaxtab[(unsigned char)(c)] & CBLANK)

#define parserblank(c)	((c) == ' ' || (c) == '\t')

#define issyntype(c, t)	((sh_syntaxtab[(unsigned char)(c)] & (t)) != 0)
#define notsyntype(c,t) ((sh_syntaxtab[(unsigned char)(c)] & (t)) == 0)

#if defined (PROCESS_SUBSTITUTION)
#  define shellexp(c)	((c) == '$' || (c) == '<' || (c) == '>')
#else
#  define shellexp(c)	((c) == '$')
#endif

#if defined (EXTENDED_GLOB)
#  define PATTERN_CHAR(c) \
	((c) == '@' || (c) == '*' || (c) == '+' || (c) == '?' || (c) == '!')
#else
#  define PATTERN_CHAR(c) 0
#endif

#define GLOB_CHAR(c) \
	((c) == '*' || (c) == '?' || (c) == '[' || (c) == ']' || (c) == '^')

#define CTLESC '\001'
#define CTLNUL '\177'

#if !defined (HAVE_ISBLANK) && !defined (isblank)
#  define isblank(x)	((x) == ' ' || (x) == '\t')
#endif

#endif  
