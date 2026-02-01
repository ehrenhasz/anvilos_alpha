 
 

 

 

#if !defined(HAVE_GLOB_H) || !defined(GLOB_HAS_ALTDIRFUNC) || \
    !defined(GLOB_HAS_GL_MATCHC) || !defined(GLOB_HAS_GL_STATV) || \
    !defined(HAVE_DECL_GLOB_NOMATCH) || HAVE_DECL_GLOB_NOMATCH == 0 || \
    defined(BROKEN_GLOB)

#ifndef _COMPAT_GLOB_H_
#define	_COMPAT_GLOB_H_

#include <sys/stat.h>
#include <sys/types.h>

# define glob_t _ssh_compat_glob_t
# define glob(a, b, c, d)  _ssh__compat_glob(a, b, c, d)
# define globfree(a)  _ssh__compat_globfree(a)

struct stat;
typedef struct {
	size_t gl_pathc;	 
	size_t gl_matchc;	 
	size_t gl_offs;		 
	int gl_flags;		 
	char **gl_pathv;	 
	struct stat **gl_statv;	 
				 
	int (*gl_errfunc)(const char *, int);

	 
	void (*gl_closedir)(void *);
	struct dirent *(*gl_readdir)(void *);	
	void *(*gl_opendir)(const char *);
	int (*gl_lstat)(const char *, struct stat *);
	int (*gl_stat)(const char *, struct stat *);
} glob_t;

#define	GLOB_APPEND	0x0001	 
#define	GLOB_DOOFFS	0x0002	 
#define	GLOB_ERR	0x0004	 
#define	GLOB_MARK	0x0008	 
#define	GLOB_NOCHECK	0x0010	 
#define	GLOB_NOSORT	0x0020	 
#define	GLOB_NOESCAPE	0x1000	 

#define	GLOB_NOSPACE	(-1)	 
#define	GLOB_ABORTED	(-2)	 
#define	GLOB_NOMATCH	(-3)	 
#define	GLOB_NOSYS	(-4)	 

#define	GLOB_ALTDIRFUNC	0x0040	 
#define	GLOB_BRACE	0x0080	 
#define	GLOB_MAGCHAR	0x0100	 
#define	GLOB_NOMAGIC	0x0200	 
#define	GLOB_QUOTE	0x0400	 
#define	GLOB_TILDE	0x0800	 
#define GLOB_LIMIT	0x2000	 
#define	GLOB_KEEPSTAT	0x4000	 
#define GLOB_ABEND	GLOB_ABORTED  

int	glob(const char *, int, int (*)(const char *, int), glob_t *);
void	globfree(glob_t *);

#endif  

#endif  

