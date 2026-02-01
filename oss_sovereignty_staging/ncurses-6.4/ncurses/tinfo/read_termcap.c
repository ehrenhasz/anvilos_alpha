 

 

 

#include <curses.priv.h>

#include <ctype.h>
#include <sys/types.h>
#include <tic.h>

MODULE_ID("$Id: read_termcap.c,v 1.102 2021/09/04 10:29:15 tom Exp $")

#if !PURE_TERMINFO

#define TC_SUCCESS     0
#define TC_NOT_FOUND  -1
#define TC_SYS_ERR    -2
#define TC_REF_LOOP   -3
#define TC_UNRESOLVED -4	 

static const char *
get_termpath(void)
{
    const char *result;

    if (!use_terminfo_vars() || (result = getenv("TERMPATH")) == 0)
	result = TERMPATH;
    TR(TRACE_DATABASE, ("TERMPATH is %s", result));
    return result;
}

 
#if USE_GETCAP

#if HAVE_BSD_CGETENT
#define _nc_cgetcap   cgetcap
#define _nc_cgetent(buf, oline, db_array, name) cgetent(buf, db_array, name)
#define _nc_cgetmatch cgetmatch
#define _nc_cgetset   cgetset
#else
static int _nc_cgetmatch(char *, const char *);
static int _nc_getent(char **, unsigned *, int *, int, char **, int, const char
		      *, int, char *);
static int _nc_nfcmp(const char *, char *);

 

 

#define	BFRAG		1024
#define	BSIZE		1024
#define	MAX_RECURSION	32	 

static size_t topreclen;	 
static char *toprec;		 
static int gottoprec;		 

 
static int
_nc_cgetset(const char *ent)
{
    if (ent == 0) {
	FreeIfNeeded(toprec);
	toprec = 0;
	topreclen = 0;
	return (0);
    }
    topreclen = strlen(ent);
    if ((toprec = typeMalloc(char, topreclen + 1)) == 0) {
	errno = ENOMEM;
	return (-1);
    }
    gottoprec = 0;
    _nc_STRCPY(toprec, ent, topreclen);
    return (0);
}

 
static char *
_nc_cgetcap(char *buf, const char *cap, int type)
{
    register const char *cp;
    register char *bp;

    bp = buf;
    for (;;) {
	 
	for (;;) {
	    if (*bp == '\0')
		return (0);
	    else if (*bp++ == ':')
		break;
	}

	 
	for (cp = cap; *cp == *bp && *bp != '\0'; cp++, bp++)
	    continue;
	if (*cp != '\0')
	    continue;
	if (*bp == '@')
	    return (0);
	if (type == ':') {
	    if (*bp != '\0' && *bp != ':')
		continue;
	    return (bp);
	}
	if (*bp != type)
	    continue;
	bp++;
	return (*bp == '@' ? 0 : bp);
    }
     
}

 
static int
_nc_cgetent(char **buf, int *oline, char **db_array, const char *name)
{
    unsigned dummy;

    return (_nc_getent(buf, &dummy, oline, 0, db_array, -1, name, 0, 0));
}

 
#define DOALLOC(size) typeRealloc(char, size, record)
static int
_nc_getent(
	      char **cap,	 
	      unsigned *len,	 
	      int *beginning,	 
	      int in_array,	 
	      char **db_array,	 
	      int fd,
	      const char *name,
	      int depth,
	      char *nfield)
{
    register char *r_end, *rp;
    int myfd = FALSE;
    char *record = 0;
    int tc_not_resolved;
    int current;
    int lineno;

     
    if (depth > MAX_RECURSION)
	return (TC_REF_LOOP);

     
    if (depth == 0 && toprec != 0 && _nc_cgetmatch(toprec, name) == 0) {
	if ((record = DOALLOC(topreclen + BFRAG)) == 0) {
	    errno = ENOMEM;
	    return (TC_SYS_ERR);
	}
	_nc_STRCPY(record, toprec, topreclen + BFRAG);
	rp = record + topreclen + 1;
	r_end = rp + BFRAG;
	current = in_array;
    } else {
	int foundit;

	 
	if ((record = DOALLOC(BFRAG)) == 0) {
	    errno = ENOMEM;
	    return (TC_SYS_ERR);
	}
	rp = r_end = record + BFRAG;
	foundit = FALSE;

	 
	for (current = in_array; db_array[current] != 0; current++) {
	    int eof = FALSE;

	     
	    if (fd >= 0) {
		(void) lseek(fd, (off_t) 0, SEEK_SET);
	    } else if ((_nc_access(db_array[current], R_OK) < 0)
		       || (fd = open(db_array[current], O_RDONLY, 0)) < 0) {
		 
		if (errno == ENOENT)
		    continue;
		free(record);
		return (TC_SYS_ERR);
	    } else {
		myfd = TRUE;
	    }
	    lineno = 0;

	     
	    {
		char buf[2048];
		register char *b_end = buf;
		register char *bp = buf;
		register int c;

		 

		for (;;) {
		    int first = lineno + 1;

		     
		    rp = record;
		    for (;;) {
			if (bp >= b_end) {
			    int n;

			    n = (int) read(fd, buf, sizeof(buf));
			    if (n <= 0) {
				if (myfd)
				    (void) close(fd);
				if (n < 0) {
				    free(record);
				    return (TC_SYS_ERR);
				}
				fd = -1;
				eof = TRUE;
				break;
			    }
			    b_end = buf + n;
			    bp = buf;
			}

			c = *bp++;
			if (c == '\n') {
			    lineno++;
			     
			    if (rp == record
				|| *record == '#'
				|| *(rp - 1) != '\\')
				break;
			}
			*rp++ = (char) c;

			 
			if (rp >= r_end) {
			    unsigned pos;
			    size_t newsize;

			    pos = (unsigned) (rp - record);
			    newsize = (size_t) (r_end - record + BFRAG);
			    record = DOALLOC(newsize);
			    if (record == 0) {
				if (myfd)
				    (void) close(fd);
				errno = ENOMEM;
				return (TC_SYS_ERR);
			    }
			    r_end = record + newsize;
			    rp = record + pos;
			}
		    }
		     
		    *rp++ = '\0';

		     
		    if (eof)
			break;

		     
		    if (*record == '\0' || *record == '#')
			continue;

		     
		    if (_nc_cgetmatch(record, name) == 0
			&& (nfield == 0
			    || !_nc_nfcmp(nfield, record))) {
			foundit = TRUE;
			*beginning = first;
			break;	 
		    }
		}
	    }
	    if (foundit)
		break;
	}

	if (!foundit) {
	    free(record);
	    return (TC_NOT_FOUND);
	}
    }

     
    {
	register char *newicap, *s;
	register int newilen;
	unsigned ilen;
	int diff, iret, tclen, oline;
	char *icap = 0, *scan, *tc, *tcstart, *tcend;

	 
	scan = record;
	tc_not_resolved = FALSE;
	for (;;) {
	    if ((tc = _nc_cgetcap(scan, "tc", '=')) == 0) {
		break;
	    }

	     
	    s = tc;
	    while (*s != '\0') {
		if (*s++ == ':') {
		    *(s - 1) = '\0';
		    break;
		}
	    }
	    tcstart = tc - 3;
	    tclen = (int) (s - tcstart);
	    tcend = s;

	    icap = 0;
	    iret = _nc_getent(&icap, &ilen, &oline, current, db_array, fd,
			      tc, depth + 1, 0);
	    newicap = icap;	 
	    newilen = (int) ilen;
	    if (iret != TC_SUCCESS) {
		 
		if (iret < TC_NOT_FOUND) {
		    if (myfd)
			(void) close(fd);
		    free(record);
		    FreeIfNeeded(icap);
		    return (iret);
		}
		if (iret == TC_UNRESOLVED) {
		    tc_not_resolved = TRUE;
		     
		} else if (iret == TC_NOT_FOUND) {
		    *(s - 1) = ':';
		    scan = s - 1;
		    tc_not_resolved = TRUE;
		    continue;
		}
	    }

	     
	    s = newicap;
	    while (*s != '\0' && *s++ != ':') ;
	    newilen -= (int) (s - newicap);
	    newicap = s;

	     
	    s += newilen;
	    if (*(s - 1) != ':') {
		*s = ':';	 
		newilen++;
	    }

	     
	    diff = newilen - tclen;
	    if (diff >= r_end - rp) {
		unsigned pos, tcpos, tcposend;
		size_t newsize;

		pos = (unsigned) (rp - record);
		newsize = (size_t) (r_end - record + diff + BFRAG);
		tcpos = (unsigned) (tcstart - record);
		tcposend = (unsigned) (tcend - record);
		record = DOALLOC(newsize);
		if (record == 0) {
		    if (myfd)
			(void) close(fd);
		    free(icap);
		    errno = ENOMEM;
		    return (TC_SYS_ERR);
		}
		r_end = record + newsize;
		rp = record + pos;
		tcstart = record + tcpos;
		tcend = record + tcposend;
	    }

	     
	    s = tcstart + newilen;
	    memmove(s, tcend, (size_t) (rp - tcend));
	    memmove(tcstart, newicap, (size_t) newilen);
	    rp += diff;
	    free(icap);

	     
	    scan = s - 1;
	}
    }

     
    if (myfd)
	(void) close(fd);
    *len = (unsigned) (rp - record - 1);	 
    if (r_end > rp) {
	if ((record = DOALLOC((size_t) (rp - record))) == 0) {
	    errno = ENOMEM;
	    return (TC_SYS_ERR);
	}
    }

    *cap = record;
    if (tc_not_resolved) {
	return (TC_UNRESOLVED);
    }
    return (current);
}

 
static int
_nc_cgetmatch(char *buf, const char *name)
{
    register const char *np;
    register char *bp;

     
    bp = buf;
    for (;;) {
	 
	np = name;
	for (;;) {
	    if (*np == '\0') {
		if (*bp == '|' || *bp == ':' || *bp == '\0')
		    return (0);
		else
		    break;
	    } else if (*bp++ != *np++) {
		break;
	    }
	}

	 
	bp--;			 
	for (;;) {
	    if (*bp == '\0' || *bp == ':')
		return (-1);	 
	    else if (*bp++ == '|')
		break;		 
	}
    }
}

 
static int
_nc_nfcmp(const char *nf, char *rec)
{
    char *cp, tmp;
    int ret;

    for (cp = rec; *cp != ':'; cp++) ;

    tmp = *(cp + 1);
    *(cp + 1) = '\0';
    ret = strcmp(nf, rec);
    *(cp + 1) = tmp;

    return (ret);
}
#endif  

 
#define USE_BSD_TGETENT 1

#if USE_BSD_TGETENT
 

 

#define	PBUFSIZ		512	 
#define	PVECSIZ		32	 
#define TBUFSIZ (2048*2)

 
static char *
get_tc_token(char **srcp, int *endp)
{
    int ch;
    bool found = FALSE;
    char *s, *base;
    char *tok = 0;

    *endp = TRUE;
    for (s = base = *srcp; *s != '\0';) {
	ch = *s++;
	if (ch == '\\') {
	    if (*s == '\0') {
		break;
	    } else if (*s++ == '\n') {
		while (isspace(UChar(*s)))
		    s++;
	    } else {
		found = TRUE;
	    }
	} else if (ch == ':') {
	    if (found) {
		tok = base;
		s[-1] = '\0';
		*srcp = s;
		*endp = FALSE;
		break;
	    }
	    base = s;
	} else if (isgraph(UChar(ch))) {
	    found = TRUE;
	}
    }

     
    if (tok == 0 && found) {
	tok = base;
    }

    return tok;
}

static char *
copy_tc_token(char *dst, const char *src, size_t len)
{
    int ch;

    while ((ch = *src++) != '\0') {
	if (ch == '\\' && *src == '\n') {
	    while (isspace(UChar(*src)))
		src++;
	    continue;
	}
	if (--len == 0) {
	    dst = 0;
	    break;
	}
	*dst++ = (char) ch;
    }
    return dst;
}

 
static int
_nc_tgetent(char *bp, char **sourcename, int *lineno, const char *name)
{
    static char *the_source;

    register char *p;
    register char *cp;
    char *dummy = NULL;
    CGETENT_CONST char **fname;
    char *home;
    int i;
    char pathbuf[PBUFSIZ];	 
    CGETENT_CONST char *pathvec[PVECSIZ];	 
    const char *termpath;
    string_desc desc;

    *lineno = 1;
    fname = pathvec;
    p = pathbuf;
    cp = use_terminfo_vars()? getenv("TERMCAP") : NULL;

     
    _nc_str_init(&desc, pathbuf, sizeof(pathbuf));
    if (cp == NULL) {
	_nc_safe_strcpy(&desc, get_termpath());
    } else if (!_nc_is_abs_path(cp)) {	 
	if ((termpath = get_termpath()) != 0) {
	    _nc_safe_strcat(&desc, termpath);
	} else {
	    char temp[PBUFSIZ];
	    temp[0] = 0;
	    if ((home = getenv("HOME")) != 0 && *home != '\0'
		&& strchr(home, ' ') == 0
		&& strlen(home) < sizeof(temp) - 10) {	 
		_nc_SPRINTF(temp, _nc_SLIMIT(sizeof(temp))
			    "%s/", home);	 
	    }
	     
	    _nc_STRCAT(temp, ".termcap", sizeof(temp));
	    _nc_safe_strcat(&desc, temp);
	    _nc_safe_strcat(&desc, " ");
	    _nc_safe_strcat(&desc, get_termpath());
	}
    } else {			 
	_nc_safe_strcat(&desc, cp);	 
    }

    *fname++ = pathbuf;		 
    while (*++p) {
	if (*p == ' ' || *p == NCURSES_PATHSEP) {
	    *p = '\0';
	    while (*++p)
		if (*p != ' ' && *p != NCURSES_PATHSEP)
		    break;
	    if (*p == '\0')
		break;
	    *fname++ = p;
	    if (fname >= pathvec + PVECSIZ) {
		fname--;
		break;
	    }
	}
    }
    *fname = 0;			 
#if !HAVE_BSD_CGETENT
    (void) _nc_cgetset(0);
#endif
    if (_nc_is_abs_path(cp)) {
	if (_nc_cgetset(cp) < 0) {
	    return (TC_SYS_ERR);
	}
    }

    i = _nc_cgetent(&dummy, lineno, pathvec, name);

     
    *bp = '\0';
    if (i >= 0) {
	char *pd, *ps, *tok;
	int endflag = FALSE;
	char *list[1023];
	size_t n, count = 0;

	pd = bp;
	ps = dummy;
	while (!endflag && (tok = get_tc_token(&ps, &endflag)) != 0) {
	    bool ignore = FALSE;

	    for (n = 1; n < count; n++) {
		char *s = list[n];
		if (s[0] == tok[0]
		    && s[1] == tok[1]) {
		    ignore = TRUE;
		    break;
		}
	    }
	    if (ignore != TRUE) {
		list[count++] = tok;
		pd = copy_tc_token(pd, tok, (size_t) (TBUFSIZ - (2 + pd - bp)));
		if (pd == 0) {
		    i = -1;
		    break;
		}
		*pd++ = ':';
		*pd = '\0';
	    }
	}
    }

    FreeIfNeeded(dummy);
    FreeIfNeeded(the_source);
    the_source = 0;

     
    if (i >= 0) {
#if HAVE_BSD_CGETENT
	char temp[PATH_MAX];

	_nc_str_init(&desc, temp, sizeof(temp));
	_nc_safe_strcpy(&desc, pathvec[i]);
	_nc_safe_strcat(&desc, ".db");
	if (_nc_access(temp, R_OK) == 0) {
	    _nc_safe_strcpy(&desc, pathvec[i]);
	}
	if ((the_source = strdup(temp)) != 0)
	    *sourcename = the_source;
#else
	if ((the_source = strdup(pathvec[i])) != 0)
	    *sourcename = the_source;
#endif
    }

    return (i);
}
#endif  
#endif  

#define MAXPATHS	32

 
#if !USE_GETCAP
static int
add_tc(char *termpaths[], char *path, int count)
{
    char *save = strchr(path, NCURSES_PATHSEP);
    if (save != 0)
	*save = '\0';
    if (count < MAXPATHS
	&& _nc_access(path, R_OK) == 0) {
	termpaths[count++] = path;
	TR(TRACE_DATABASE, ("Adding termpath %s", path));
    }
    termpaths[count] = 0;
    if (save != 0)
	*save = NCURSES_PATHSEP;
    return count;
}
#define ADD_TC(path, count) filecount = add_tc(termpaths, path, count)
#endif  

NCURSES_EXPORT(int)
_nc_read_termcap_entry(const char *const tn, TERMTYPE2 *const tp)
{
    int found = TGETENT_NO;
    ENTRY *ep;
#if USE_GETCAP_CACHE
    char cwd_buf[PATH_MAX];
#endif
#if USE_GETCAP
    char *p, tc[TBUFSIZ];
    char *tc_buf = 0;
#define MY_SIZE sizeof(tc) - 1
    int status;
    static char *source;
    static int lineno;

    TR(TRACE_DATABASE, ("read termcap entry for %s", tn));

    if (strlen(tn) == 0
	|| strcmp(tn, ".") == 0
	|| strcmp(tn, "..") == 0
	|| _nc_pathlast(tn) != 0) {
	TR(TRACE_DATABASE, ("illegal or missing entry name '%s'", tn));
	return TGETENT_NO;
    }

    if (use_terminfo_vars() && (p = getenv("TERMCAP")) != 0
	&& !_nc_is_abs_path(p) && _nc_name_match(p, tn, "|:")) {
	 
	tc_buf = strdup(p);
	_nc_set_source("TERMCAP");
    } else {
	 
	if ((status = _nc_tgetent(tc, &source, &lineno, tn)) < 0)
	    return (status == TC_NOT_FOUND ? TGETENT_NO : TGETENT_ERR);

	_nc_curr_line = lineno;
	_nc_set_source(source);
	tc_buf = tc;
    }
    if (tc_buf == 0)
	return (TGETENT_ERR);
    _nc_read_entry_source((FILE *) 0, tc_buf, FALSE, TRUE, NULLHOOK);
    if (tc_buf != tc)
	free(tc_buf);
#else
     
    FILE *fp;
    char *tc, *termpaths[MAXPATHS];
    int filecount = 0;
    int j, k;
    bool use_buffer = FALSE;
    bool normal = TRUE;
    char *tc_buf = 0;
    char pathbuf[PATH_MAX];
    char *copied = 0;
    char *cp;
    struct stat test_stat[MAXPATHS];

    termpaths[filecount] = 0;
    if (use_terminfo_vars() && (tc = getenv("TERMCAP")) != 0) {
	if (_nc_is_abs_path(tc)) {	 
	    ADD_TC(tc, 0);
	    normal = FALSE;
	} else if (_nc_name_match(tc, tn, "|:")) {	 
	    tc_buf = strdup(tc);
	    use_buffer = (tc_buf != 0);
	    normal = FALSE;
	}
    }

    if (normal) {		 
	char envhome[PATH_MAX], *h;

	copied = strdup(get_termpath());
	for (cp = copied; *cp; cp++) {
	    if (*cp == NCURSES_PATHSEP)
		*cp = '\0';
	    else if (cp == copied || cp[-1] == '\0') {
		ADD_TC(cp, filecount);
	    }
	}

#define PRIVATE_CAP "%.*s/.termcap"

	if (use_terminfo_vars() && (h = getenv("HOME")) != NULL && *h != '\0'
	    && (strlen(h) + sizeof(PRIVATE_CAP)) < PATH_MAX) {
	     
	    _nc_STRCPY(envhome, h, sizeof(envhome));
	    _nc_SPRINTF(pathbuf, _nc_SLIMIT(sizeof(pathbuf))
			PRIVATE_CAP,
			(int) (sizeof(pathbuf) - sizeof(PRIVATE_CAP)),
			envhome);
	    ADD_TC(pathbuf, filecount);
	}
    }

     
#if HAVE_LINK
    for (j = 0; j < filecount; j++) {
	bool omit = FALSE;
	if (stat(termpaths[j], &test_stat[j]) != 0
	    || !S_ISREG(test_stat[j].st_mode)) {
	    omit = TRUE;
	} else {
	    for (k = 0; k < j; k++) {
		if (test_stat[k].st_dev == test_stat[j].st_dev
		    && test_stat[k].st_ino == test_stat[j].st_ino) {
		    omit = TRUE;
		    break;
		}
	    }
	}
	if (omit) {
	    TR(TRACE_DATABASE, ("Path %s is a duplicate", termpaths[j]));
	    for (k = j + 1; k < filecount; k++) {
		termpaths[k - 1] = termpaths[k];
		test_stat[k - 1] = test_stat[k];
	    }
	    --filecount;
	    --j;
	}
    }
#endif

     
    if (use_buffer) {
	_nc_set_source("TERMCAP");

	 
	_nc_read_entry_source((FILE *) 0, tc_buf, FALSE, FALSE, NULLHOOK);
	free(tc_buf);
    } else {
	int i;

	for (i = 0; i < filecount; i++) {

	    TR(TRACE_DATABASE, ("Looking for %s in %s", tn, termpaths[i]));
	    if (_nc_access(termpaths[i], R_OK) == 0
		&& (fp = safe_fopen(termpaths[i], "r")) != (FILE *) 0) {
		_nc_set_source(termpaths[i]);

		 
		_nc_read_entry_source(fp, (char *) 0, FALSE, TRUE, NULLHOOK);

		(void) fclose(fp);
	    }
	}
    }
    if (copied != 0)
	free(copied);
#endif  

    if (_nc_head == 0)
	return (TGETENT_ERR);

     
    if (_nc_resolve_uses2(TRUE, FALSE) != TRUE)
	return (TGETENT_ERR);

     
#if USE_GETCAP_CACHE
    if (getcwd(cwd_buf, sizeof(cwd_buf)) != 0) {
	_nc_set_writedir((char *) 0);	 
#endif
	for_entry_list(ep) {
	    if (_nc_name_match(ep->tterm.term_names, tn, "|:")) {
		 
		*tp = ep->tterm;
		_nc_free_entry(_nc_head, &(ep->tterm));

		 
#if USE_GETCAP_CACHE
		(void) _nc_write_entry(tp);
#endif
		found = TGETENT_YES;
		break;
	    }
	}
#if USE_GETCAP_CACHE
	chdir(cwd_buf);
    }
#endif

    return (found);
}
#else
extern
NCURSES_EXPORT(void)
_nc_read_termcap(void);
NCURSES_EXPORT(void)
_nc_read_termcap(void)
{
}
#endif  
