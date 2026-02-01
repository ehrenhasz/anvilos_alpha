 
 

 

 

#include "includes.h"
#include "glob.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdlib.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <string.h>
#include <unistd.h>

#if !defined(HAVE_GLOB) || !defined(GLOB_HAS_ALTDIRFUNC) || \
    !defined(GLOB_HAS_GL_MATCHC) || !defined(GLOB_HAS_GL_STATV) || \
    !defined(HAVE_DECL_GLOB_NOMATCH) || HAVE_DECL_GLOB_NOMATCH == 0 || \
    defined(BROKEN_GLOB)

#include "charclass.h"

#ifdef TILDE
# undef TILDE
#endif

#define	DOLLAR		'$'
#define	DOT		'.'
#define	EOS		'\0'
#define	LBRACKET	'['
#define	NOT		'!'
#define	QUESTION	'?'
#define	QUOTE		'\\'
#define	RANGE		'-'
#define	RBRACKET	']'
#define	SEP		'/'
#define	STAR		'*'
#define	TILDE		'~'
#define	UNDERSCORE	'_'
#define	LBRACE		'{'
#define	RBRACE		'}'
#define	SLASH		'/'
#define	COMMA		','

#ifndef DEBUG

#define	M_QUOTE		0x8000
#define	M_PROTECT	0x4000
#define	M_MASK		0xffff
#define	M_ASCII		0x00ff

typedef u_short Char;

#else

#define	M_QUOTE		0x80
#define	M_PROTECT	0x40
#define	M_MASK		0xff
#define	M_ASCII		0x7f

typedef char Char;

#endif


#define	CHAR(c)		((Char)((c)&M_ASCII))
#define	META(c)		((Char)((c)|M_QUOTE))
#define	M_ALL		META('*')
#define	M_END		META(']')
#define	M_NOT		META('!')
#define	M_ONE		META('?')
#define	M_RNG		META('-')
#define	M_SET		META('[')
#define	M_CLASS		META(':')
#define	ismeta(c)	(((c)&M_QUOTE) != 0)

#define	GLOB_LIMIT_MALLOC	65536
#define	GLOB_LIMIT_STAT		2048
#define	GLOB_LIMIT_READDIR	16384

struct glob_lim {
	size_t	glim_malloc;
	size_t	glim_stat;
	size_t	glim_readdir;
};

struct glob_path_stat {
	char		*gps_path;
	struct stat	*gps_stat;
};

static int	 compare(const void *, const void *);
static int	 compare_gps(const void *, const void *);
static int	 g_Ctoc(const Char *, char *, size_t);
static int	 g_lstat(Char *, struct stat *, glob_t *);
static DIR	*g_opendir(Char *, glob_t *);
static Char	*g_strchr(const Char *, int);
static int	 g_strncmp(const Char *, const char *, size_t);
static int	 g_stat(Char *, struct stat *, glob_t *);
static int	 glob0(const Char *, glob_t *, struct glob_lim *);
static int	 glob1(Char *, Char *, glob_t *, struct glob_lim *);
static int	 glob2(Char *, Char *, Char *, Char *, Char *, Char *,
		    glob_t *, struct glob_lim *);
static int	 glob3(Char *, Char *, Char *, Char *, Char *,
		    Char *, Char *, glob_t *, struct glob_lim *);
static int	 globextend(const Char *, glob_t *, struct glob_lim *,
		    struct stat *);
static const Char *
		 globtilde(const Char *, Char *, size_t, glob_t *);
static int	 globexp1(const Char *, glob_t *, struct glob_lim *);
static int	 globexp2(const Char *, const Char *, glob_t *,
		    struct glob_lim *);
static int	 match(Char *, Char *, Char *);
#ifdef DEBUG
static void	 qprintf(const char *, Char *);
#endif

int
glob(const char *pattern, int flags, int (*errfunc)(const char *, int),
    glob_t *pglob)
{
	const u_char *patnext;
	int c;
	Char *bufnext, *bufend, patbuf[PATH_MAX];
	struct glob_lim limit = { 0, 0, 0 };

	patnext = (u_char *) pattern;
	if (!(flags & GLOB_APPEND)) {
		pglob->gl_pathc = 0;
		pglob->gl_pathv = NULL;
		pglob->gl_statv = NULL;
		if (!(flags & GLOB_DOOFFS))
			pglob->gl_offs = 0;
	}
	pglob->gl_flags = flags & ~GLOB_MAGCHAR;
	pglob->gl_errfunc = errfunc;
	pglob->gl_matchc = 0;

	if (strnlen(pattern, PATH_MAX) == PATH_MAX)
		return(GLOB_NOMATCH);

	if (pglob->gl_offs >= SSIZE_MAX || pglob->gl_pathc >= SSIZE_MAX ||
	    pglob->gl_pathc >= SSIZE_MAX - pglob->gl_offs - 1)
		return GLOB_NOSPACE;

	bufnext = patbuf;
	bufend = bufnext + PATH_MAX - 1;
	if (flags & GLOB_NOESCAPE)
		while (bufnext < bufend && (c = *patnext++) != EOS)
			*bufnext++ = c;
	else {
		 
		while (bufnext < bufend && (c = *patnext++) != EOS)
			if (c == QUOTE) {
				if ((c = *patnext++) == EOS) {
					c = QUOTE;
					--patnext;
				}
				*bufnext++ = c | M_PROTECT;
			} else
				*bufnext++ = c;
	}
	*bufnext = EOS;

	if (flags & GLOB_BRACE)
		return globexp1(patbuf, pglob, &limit);
	else
		return glob0(patbuf, pglob, &limit);
}

 
static int
globexp1(const Char *pattern, glob_t *pglob, struct glob_lim *limitp)
{
	const Char* ptr = pattern;

	 
	if (pattern[0] == LBRACE && pattern[1] == RBRACE && pattern[2] == EOS)
		return glob0(pattern, pglob, limitp);

	if ((ptr = (const Char *) g_strchr(ptr, LBRACE)) != NULL)
		return globexp2(ptr, pattern, pglob, limitp);

	return glob0(pattern, pglob, limitp);
}


 
static int
globexp2(const Char *ptr, const Char *pattern, glob_t *pglob,
    struct glob_lim *limitp)
{
	int     i, rv;
	Char   *lm, *ls;
	const Char *pe, *pm, *pl;
	Char    patbuf[PATH_MAX];

	 
	for (lm = patbuf, pm = pattern; pm != ptr; *lm++ = *pm++)
		;
	*lm = EOS;
	ls = lm;

	 
	for (i = 0, pe = ++ptr; *pe; pe++)
		if (*pe == LBRACKET) {
			 
			for (pm = pe++; *pe != RBRACKET && *pe != EOS; pe++)
				;
			if (*pe == EOS) {
				 
				pe = pm;
			}
		} else if (*pe == LBRACE)
			i++;
		else if (*pe == RBRACE) {
			if (i == 0)
				break;
			i--;
		}

	 
	if (i != 0 || *pe == EOS)
		return glob0(patbuf, pglob, limitp);

	for (i = 0, pl = pm = ptr; pm <= pe; pm++) {
		switch (*pm) {
		case LBRACKET:
			 
			for (pl = pm++; *pm != RBRACKET && *pm != EOS; pm++)
				;
			if (*pm == EOS) {
				 
				pm = pl;
			}
			break;

		case LBRACE:
			i++;
			break;

		case RBRACE:
			if (i) {
				i--;
				break;
			}
			 
		case COMMA:
			if (i && *pm == COMMA)
				break;
			else {
				 
				for (lm = ls; (pl < pm); *lm++ = *pl++)
					;

				 
				for (pl = pe + 1; (*lm++ = *pl++) != EOS; )
					;

				 
#ifdef DEBUG
				qprintf("globexp2:", patbuf);
#endif
				rv = globexp1(patbuf, pglob, limitp);
				if (rv && rv != GLOB_NOMATCH)
					return rv;

				 
				pl = pm + 1;
			}
			break;

		default:
			break;
		}
	}
	return 0;
}



 
static const Char *
globtilde(const Char *pattern, Char *patbuf, size_t patbuf_len, glob_t *pglob)
{
	struct passwd *pwd;
	char *h;
	const Char *p;
	Char *b, *eb;

	if (*pattern != TILDE || !(pglob->gl_flags & GLOB_TILDE))
		return pattern;

	 
	eb = &patbuf[patbuf_len - 1];
	for (p = pattern + 1, h = (char *) patbuf;
	    h < (char *)eb && *p && *p != SLASH; *h++ = *p++)
		;

	*h = EOS;

#if 0
	if (h == (char *)eb)
		return what;
#endif

	if (((char *) patbuf)[0] == EOS) {
		 
#if 0
		if (issetugid() != 0 || (h = getenv("HOME")) == NULL) {
#endif
		if ((getuid() != geteuid()) || (h = getenv("HOME")) == NULL) {
			if ((pwd = getpwuid(getuid())) == NULL)
				return pattern;
			else
				h = pwd->pw_dir;
		}
	} else {
		 
		if ((pwd = getpwnam((char*) patbuf)) == NULL)
			return pattern;
		else
			h = pwd->pw_dir;
	}

	 
	for (b = patbuf; b < eb && *h; *b++ = *h++)
		;

	 
	while (b < eb && (*b++ = *p++) != EOS)
		;
	*b = EOS;

	return patbuf;
}

static int
g_strncmp(const Char *s1, const char *s2, size_t n)
{
	int rv = 0;

	while (n--) {
		rv = *(Char *)s1 - *(const unsigned char *)s2++;
		if (rv)
			break;
		if (*s1++ == '\0')
			break;
	}
	return rv;
}

static int
g_charclass(const Char **patternp, Char **bufnextp)
{
	const Char *pattern = *patternp + 1;
	Char *bufnext = *bufnextp;
	const Char *colon;
	struct cclass *cc;
	size_t len;

	if ((colon = g_strchr(pattern, ':')) == NULL || colon[1] != ']')
		return 1;	 

	len = (size_t)(colon - pattern);
	for (cc = cclasses; cc->name != NULL; cc++) {
		if (!g_strncmp(pattern, cc->name, len) && cc->name[len] == '\0')
			break;
	}
	if (cc->name == NULL)
		return -1;	 
	*bufnext++ = M_CLASS;
	*bufnext++ = (Char)(cc - &cclasses[0]);
	*bufnextp = bufnext;
	*patternp += len + 3;

	return 0;
}

 
static int
glob0(const Char *pattern, glob_t *pglob, struct glob_lim *limitp)
{
	const Char *qpatnext;
	int c, err;
	size_t oldpathc;
	Char *bufnext, patbuf[PATH_MAX];

	qpatnext = globtilde(pattern, patbuf, PATH_MAX, pglob);
	oldpathc = pglob->gl_pathc;
	bufnext = patbuf;

	 
	while ((c = *qpatnext++) != EOS) {
		switch (c) {
		case LBRACKET:
			c = *qpatnext;
			if (c == NOT)
				++qpatnext;
			if (*qpatnext == EOS ||
			    g_strchr(qpatnext+1, RBRACKET) == NULL) {
				*bufnext++ = LBRACKET;
				if (c == NOT)
					--qpatnext;
				break;
			}
			*bufnext++ = M_SET;
			if (c == NOT)
				*bufnext++ = M_NOT;
			c = *qpatnext++;
			do {
				if (c == LBRACKET && *qpatnext == ':') {
					do {
						err = g_charclass(&qpatnext,
						    &bufnext);
						if (err)
							break;
						c = *qpatnext++;
					} while (c == LBRACKET && *qpatnext == ':');
					if (err == -1 &&
					    !(pglob->gl_flags & GLOB_NOCHECK))
						return GLOB_NOMATCH;
					if (c == RBRACKET)
						break;
				}
				*bufnext++ = CHAR(c);
				if (*qpatnext == RANGE &&
				    (c = qpatnext[1]) != RBRACKET) {
					*bufnext++ = M_RNG;
					*bufnext++ = CHAR(c);
					qpatnext += 2;
				}
			} while ((c = *qpatnext++) != RBRACKET);
			pglob->gl_flags |= GLOB_MAGCHAR;
			*bufnext++ = M_END;
			break;
		case QUESTION:
			pglob->gl_flags |= GLOB_MAGCHAR;
			*bufnext++ = M_ONE;
			break;
		case STAR:
			pglob->gl_flags |= GLOB_MAGCHAR;
			 
			if (bufnext == patbuf || bufnext[-1] != M_ALL)
				*bufnext++ = M_ALL;
			break;
		default:
			*bufnext++ = CHAR(c);
			break;
		}
	}
	*bufnext = EOS;
#ifdef DEBUG
	qprintf("glob0:", patbuf);
#endif

	if ((err = glob1(patbuf, patbuf+PATH_MAX-1, pglob, limitp)) != 0)
		return(err);

	 
	if (pglob->gl_pathc == oldpathc) {
		if ((pglob->gl_flags & GLOB_NOCHECK) ||
		    ((pglob->gl_flags & GLOB_NOMAGIC) &&
		    !(pglob->gl_flags & GLOB_MAGCHAR)))
			return(globextend(pattern, pglob, limitp, NULL));
		else
			return(GLOB_NOMATCH);
	}
	if (!(pglob->gl_flags & GLOB_NOSORT)) {
		if ((pglob->gl_flags & GLOB_KEEPSTAT)) {
			 
			struct glob_path_stat *path_stat;
			size_t i;
			size_t n = pglob->gl_pathc - oldpathc;
			size_t o = pglob->gl_offs + oldpathc;

			if ((path_stat = calloc(n, sizeof(*path_stat))) == NULL)
				return GLOB_NOSPACE;
			for (i = 0; i < n; i++) {
				path_stat[i].gps_path = pglob->gl_pathv[o + i];
				path_stat[i].gps_stat = pglob->gl_statv[o + i];
			}
			qsort(path_stat, n, sizeof(*path_stat), compare_gps);
			for (i = 0; i < n; i++) {
				pglob->gl_pathv[o + i] = path_stat[i].gps_path;
				pglob->gl_statv[o + i] = path_stat[i].gps_stat;
			}
			free(path_stat);
		} else {
			qsort(pglob->gl_pathv + pglob->gl_offs + oldpathc,
			    pglob->gl_pathc - oldpathc, sizeof(char *),
			    compare);
		}
	}
	return(0);
}

static int
compare(const void *p, const void *q)
{
	return(strcmp(*(char **)p, *(char **)q));
}

static int
compare_gps(const void *_p, const void *_q)
{
	const struct glob_path_stat *p = (const struct glob_path_stat *)_p;
	const struct glob_path_stat *q = (const struct glob_path_stat *)_q;

	return(strcmp(p->gps_path, q->gps_path));
}

static int
glob1(Char *pattern, Char *pattern_last, glob_t *pglob, struct glob_lim *limitp)
{
	Char pathbuf[PATH_MAX];

	 
	if (*pattern == EOS)
		return(0);
	return(glob2(pathbuf, pathbuf+PATH_MAX-1,
	    pathbuf, pathbuf+PATH_MAX-1,
	    pattern, pattern_last, pglob, limitp));
}

 
static int
glob2(Char *pathbuf, Char *pathbuf_last, Char *pathend, Char *pathend_last,
    Char *pattern, Char *pattern_last, glob_t *pglob, struct glob_lim *limitp)
{
	struct stat sb;
	Char *p, *q;
	int anymeta;

	 
	for (anymeta = 0;;) {
		if (*pattern == EOS) {		 
			*pathend = EOS;

			if ((pglob->gl_flags & GLOB_LIMIT) &&
			    limitp->glim_stat++ >= GLOB_LIMIT_STAT) {
				errno = 0;
				*pathend++ = SEP;
				*pathend = EOS;
				return(GLOB_NOSPACE);
			}
			if (g_lstat(pathbuf, &sb, pglob))
				return(0);

			if (((pglob->gl_flags & GLOB_MARK) &&
			    pathend[-1] != SEP) && (S_ISDIR(sb.st_mode) ||
			    (S_ISLNK(sb.st_mode) &&
			    (g_stat(pathbuf, &sb, pglob) == 0) &&
			    S_ISDIR(sb.st_mode)))) {
				if (pathend+1 > pathend_last)
					return (1);
				*pathend++ = SEP;
				*pathend = EOS;
			}
			++pglob->gl_matchc;
			return(globextend(pathbuf, pglob, limitp, &sb));
		}

		 
		q = pathend;
		p = pattern;
		while (*p != EOS && *p != SEP) {
			if (ismeta(*p))
				anymeta = 1;
			if (q+1 > pathend_last)
				return (1);
			*q++ = *p++;
		}

		if (!anymeta) {		 
			pathend = q;
			pattern = p;
			while (*pattern == SEP) {
				if (pathend+1 > pathend_last)
					return (1);
				*pathend++ = *pattern++;
			}
		} else
			 
			return(glob3(pathbuf, pathbuf_last, pathend,
			    pathend_last, pattern, p, pattern_last,
			    pglob, limitp));
	}
	 
}

static int
glob3(Char *pathbuf, Char *pathbuf_last, Char *pathend, Char *pathend_last,
    Char *pattern, Char *restpattern, Char *restpattern_last, glob_t *pglob,
    struct glob_lim *limitp)
{
	struct dirent *dp;
	DIR *dirp;
	int err;
	char buf[PATH_MAX];

	 
	struct dirent *(*readdirfunc)(void *);

	if (pathend > pathend_last)
		return (1);
	*pathend = EOS;
	errno = 0;

	if ((dirp = g_opendir(pathbuf, pglob)) == NULL) {
		 
		if (pglob->gl_errfunc) {
			if (g_Ctoc(pathbuf, buf, sizeof(buf)))
				return(GLOB_ABORTED);
			if (pglob->gl_errfunc(buf, errno) ||
			    pglob->gl_flags & GLOB_ERR)
				return(GLOB_ABORTED);
		}
		return(0);
	}

	err = 0;

	 
	if (pglob->gl_flags & GLOB_ALTDIRFUNC)
		readdirfunc = pglob->gl_readdir;
	else
		readdirfunc = (struct dirent *(*)(void *))readdir;
	while ((dp = (*readdirfunc)(dirp))) {
		u_char *sc;
		Char *dc;

		if ((pglob->gl_flags & GLOB_LIMIT) &&
		    limitp->glim_readdir++ >= GLOB_LIMIT_READDIR) {
			errno = 0;
			*pathend++ = SEP;
			*pathend = EOS;
			err = GLOB_NOSPACE;
			break;
		}

		 
		if (dp->d_name[0] == DOT && *pattern != DOT)
			continue;
		dc = pathend;
		sc = (u_char *) dp->d_name;
		while (dc < pathend_last && (*dc++ = *sc++) != EOS)
			;
		if (dc >= pathend_last) {
			*dc = EOS;
			err = 1;
			break;
		}

		if (!match(pathend, pattern, restpattern)) {
			*pathend = EOS;
			continue;
		}
		err = glob2(pathbuf, pathbuf_last, --dc, pathend_last,
		    restpattern, restpattern_last, pglob, limitp);
		if (err)
			break;
	}

	if (pglob->gl_flags & GLOB_ALTDIRFUNC)
		(*pglob->gl_closedir)(dirp);
	else
		closedir(dirp);
	return(err);
}


 
static int
globextend(const Char *path, glob_t *pglob, struct glob_lim *limitp,
    struct stat *sb)
{
	char **pathv;
	size_t i, newn, len;
	char *copy = NULL;
	const Char *p;
	struct stat **statv;

	newn = 2 + pglob->gl_pathc + pglob->gl_offs;
	if (pglob->gl_offs >= SSIZE_MAX ||
	    pglob->gl_pathc >= SSIZE_MAX ||
	    newn >= SSIZE_MAX ||
	    SIZE_MAX / sizeof(*pathv) <= newn ||
	    SIZE_MAX / sizeof(*statv) <= newn) {
 nospace:
		for (i = pglob->gl_offs; i < newn - 2; i++) {
			if (pglob->gl_pathv && pglob->gl_pathv[i])
				free(pglob->gl_pathv[i]);
			if ((pglob->gl_flags & GLOB_KEEPSTAT) != 0 &&
			    pglob->gl_pathv && pglob->gl_pathv[i])
				free(pglob->gl_statv[i]);
		}
		free(pglob->gl_pathv);
		pglob->gl_pathv = NULL;
		free(pglob->gl_statv);
		pglob->gl_statv = NULL;
		return(GLOB_NOSPACE);
	}

	pathv = reallocarray(pglob->gl_pathv, newn, sizeof(*pathv));
	if (pathv == NULL)
		goto nospace;
	if (pglob->gl_pathv == NULL && pglob->gl_offs > 0) {
		 
		pathv += pglob->gl_offs;
		for (i = pglob->gl_offs; i > 0; i--)
			*--pathv = NULL;
	}
	pglob->gl_pathv = pathv;

	if ((pglob->gl_flags & GLOB_KEEPSTAT) != 0) {
		statv = reallocarray(pglob->gl_statv, newn, sizeof(*statv));
		if (statv == NULL)
			goto nospace;
		if (pglob->gl_statv == NULL && pglob->gl_offs > 0) {
			 
			statv += pglob->gl_offs;
			for (i = pglob->gl_offs; i > 0; i--)
				*--statv = NULL;
		}
		pglob->gl_statv = statv;
		if (sb == NULL)
			statv[pglob->gl_offs + pglob->gl_pathc] = NULL;
		else {
			limitp->glim_malloc += sizeof(**statv);
			if ((pglob->gl_flags & GLOB_LIMIT) &&
			    limitp->glim_malloc >= GLOB_LIMIT_MALLOC) {
				errno = 0;
				return(GLOB_NOSPACE);
			}
			if ((statv[pglob->gl_offs + pglob->gl_pathc] =
			    malloc(sizeof(**statv))) == NULL)
				goto copy_error;
			memcpy(statv[pglob->gl_offs + pglob->gl_pathc], sb,
			    sizeof(*sb));
		}
		statv[pglob->gl_offs + pglob->gl_pathc + 1] = NULL;
	}

	for (p = path; *p++;)
		;
	len = (size_t)(p - path);
	limitp->glim_malloc += len;
	if ((copy = malloc(len)) != NULL) {
		if (g_Ctoc(path, copy, len)) {
			free(copy);
			return(GLOB_NOSPACE);
		}
		pathv[pglob->gl_offs + pglob->gl_pathc++] = copy;
	}
	pathv[pglob->gl_offs + pglob->gl_pathc] = NULL;

	if ((pglob->gl_flags & GLOB_LIMIT) &&
	    (newn * sizeof(*pathv)) + limitp->glim_malloc >
	    GLOB_LIMIT_MALLOC) {
		errno = 0;
		return(GLOB_NOSPACE);
	}
 copy_error:
	return(copy == NULL ? GLOB_NOSPACE : 0);
}


 
static int
match(Char *name, Char *pat, Char *patend)
{
	int ok, negate_range;
	Char c, k;
	Char *nextp = NULL;
	Char *nextn = NULL;

loop:
	while (pat < patend) {
		c = *pat++;
		switch (c & M_MASK) {
		case M_ALL:
			while (pat < patend && (*pat & M_MASK) == M_ALL)
				pat++;	 
			if (pat == patend)
				return(1);
			if (*name == EOS)
				return(0);
			nextn = name + 1;
			nextp = pat - 1;
			break;
		case M_ONE:
			if (*name++ == EOS)
				goto fail;
			break;
		case M_SET:
			ok = 0;
			if ((k = *name++) == EOS)
				goto fail;
			if ((negate_range = ((*pat & M_MASK) == M_NOT)) != EOS)
				++pat;
			while (((c = *pat++) & M_MASK) != M_END) {
				if ((c & M_MASK) == M_CLASS) {
					Char idx = *pat & M_MASK;
					if (idx < NCCLASSES &&
					    cclasses[idx].isctype(k))
						ok = 1;
					++pat;
				}
				if ((*pat & M_MASK) == M_RNG) {
					if (c <= k && k <= pat[1])
						ok = 1;
					pat += 2;
				} else if (c == k)
					ok = 1;
			}
			if (ok == negate_range)
				goto fail;
			break;
		default:
			if (*name++ != c)
				goto fail;
			break;
		}
	}
	if (*name == EOS)
		return(1);

fail:
	if (nextn) {
		pat = nextp;
		name = nextn;
		goto loop;
	}
	return(0);
}

 
void
globfree(glob_t *pglob)
{
	size_t i;
	char **pp;

	if (pglob->gl_pathv != NULL) {
		pp = pglob->gl_pathv + pglob->gl_offs;
		for (i = pglob->gl_pathc; i--; ++pp)
			free(*pp);
		free(pglob->gl_pathv);
		pglob->gl_pathv = NULL;
	}
	if (pglob->gl_statv != NULL) {
		for (i = 0; i < pglob->gl_pathc; i++) {
			free(pglob->gl_statv[i]);
		}
		free(pglob->gl_statv);
		pglob->gl_statv = NULL;
	}
}

static DIR *
g_opendir(Char *str, glob_t *pglob)
{
	char buf[PATH_MAX];

	if (!*str)
		strlcpy(buf, ".", sizeof buf);
	else {
		if (g_Ctoc(str, buf, sizeof(buf)))
			return(NULL);
	}

	if (pglob->gl_flags & GLOB_ALTDIRFUNC)
		return((*pglob->gl_opendir)(buf));

	return(opendir(buf));
}

static int
g_lstat(Char *fn, struct stat *sb, glob_t *pglob)
{
	char buf[PATH_MAX];

	if (g_Ctoc(fn, buf, sizeof(buf)))
		return(-1);
	if (pglob->gl_flags & GLOB_ALTDIRFUNC)
		return((*pglob->gl_lstat)(buf, sb));
	return(lstat(buf, sb));
}

static int
g_stat(Char *fn, struct stat *sb, glob_t *pglob)
{
	char buf[PATH_MAX];

	if (g_Ctoc(fn, buf, sizeof(buf)))
		return(-1);
	if (pglob->gl_flags & GLOB_ALTDIRFUNC)
		return((*pglob->gl_stat)(buf, sb));
	return(stat(buf, sb));
}

static Char *
g_strchr(const Char *str, int ch)
{
	do {
		if (*str == ch)
			return ((Char *)str);
	} while (*str++);
	return (NULL);
}

static int
g_Ctoc(const Char *str, char *buf, size_t len)
{

	while (len--) {
		if ((*buf++ = *str++) == EOS)
			return (0);
	}
	return (1);
}

#ifdef DEBUG
static void
qprintf(const char *str, Char *s)
{
	Char *p;

	(void)printf("%s:\n", str);
	for (p = s; *p; p++)
		(void)printf("%c", CHAR(*p));
	(void)printf("\n");
	for (p = s; *p; p++)
		(void)printf("%c", *p & M_PROTECT ? '"' : ' ');
	(void)printf("\n");
	for (p = s; *p; p++)
		(void)printf("%c", ismeta(*p) ? '_' : ' ');
	(void)printf("\n");
}
#endif

#endif  
