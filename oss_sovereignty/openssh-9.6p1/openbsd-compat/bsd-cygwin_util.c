 

#define NO_BINARY_OPEN	 
#include "includes.h"

#ifdef HAVE_CYGWIN

#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

#include "xmalloc.h"

int
binary_open(const char *filename, int flags, ...)
{
	va_list ap;
	mode_t mode;

	va_start(ap, flags);
	mode = va_arg(ap, mode_t);
	va_end(ap);
	return (open(filename, flags | O_BINARY, mode));
}

int
check_ntsec(const char *filename)
{
	return (pathconf(filename, _PC_POSIX_PERMISSIONS));
}

const char *
cygwin_ssh_privsep_user()
{
  static char cyg_privsep_user[DNLEN + UNLEN + 2];

  if (!cyg_privsep_user[0])
    {
#ifdef CW_CYGNAME_FROM_WINNAME
      if (cygwin_internal (CW_CYGNAME_FROM_WINNAME, "sshd", cyg_privsep_user,
			   sizeof cyg_privsep_user) != 0)
#endif
	strlcpy(cyg_privsep_user, "sshd", sizeof(cyg_privsep_user));
    }
  return cyg_privsep_user;
}

#define NL(x) x, (sizeof (x) - 1)
#define WENV_SIZ (sizeof (wenv_arr) / sizeof (wenv_arr[0]))

static struct wenv {
	const char *name;
	size_t namelen;
} wenv_arr[] = {
	{ NL("ALLUSERSPROFILE=") },
	{ NL("COMPUTERNAME=") },
	{ NL("COMSPEC=") },
	{ NL("CYGWIN=") },
	{ NL("OS=") },
	{ NL("PATH=") },
	{ NL("PATHEXT=") },
	{ NL("PROGRAMFILES=") },
	{ NL("SYSTEMDRIVE=") },
	{ NL("SYSTEMROOT=") },
	{ NL("WINDIR=") }
};

char **
fetch_windows_environment(void)
{
	char **e, **p;
	unsigned int i, idx = 0;

	p = xcalloc(WENV_SIZ + 1, sizeof(char *));
	for (e = environ; *e != NULL; ++e) {
		for (i = 0; i < WENV_SIZ; ++i) {
			if (!strncmp(*e, wenv_arr[i].name, wenv_arr[i].namelen))
				p[idx++] = *e;
		}
	}
	p[idx] = NULL;
	return p;
}

void
free_windows_environment(char **p)
{
	free(p);
}

 

static int
__match_pattern (const wchar_t *s, const wchar_t *pattern)
{
	for (;;) {
		 
		if (!*pattern)
			return !*s;

		if (*pattern == '*') {
			 
			pattern++;

			 
			if (!*pattern)
				return 1;

			 
			if (*pattern != '?' && *pattern != '*') {
				 
				for (; *s; s++)
					if (*s == *pattern &&
					    __match_pattern(s + 1, pattern + 1))
						return 1;
				 
				return 0;
			}
			 
			for (; *s; s++)
				if (__match_pattern(s, pattern))
					return 1;
			 
			return 0;
		}
		 
		if (!*s)
			return 0;

		 
		if (*pattern != '?' && towlower(*pattern) != towlower(*s))
			return 0;

		 
		s++;
		pattern++;
	}
	 
}

static int
_match_pattern(const char *s, const char *pattern)
{
	wchar_t *ws;
	wchar_t *wpattern;
	size_t len;
	int ret;

	if ((len = mbstowcs(NULL, s, 0)) == (size_t) -1)
		return 0;
	ws = (wchar_t *) xcalloc(len + 1, sizeof (wchar_t));
	mbstowcs(ws, s, len + 1);
	if ((len = mbstowcs(NULL, pattern, 0)) == (size_t) -1)
		return 0;
	wpattern = (wchar_t *) xcalloc(len + 1, sizeof (wchar_t));
	mbstowcs(wpattern, pattern, len + 1);
	ret = __match_pattern (ws, wpattern);
	free(ws);
	free(wpattern);
	return ret;
}

 
int
cygwin_ug_match_pattern_list(const char *string, const char *pattern)
{
	char sub[1024];
	int negated;
	int got_positive;
	u_int i, subi, len = strlen(pattern);

	got_positive = 0;
	for (i = 0; i < len;) {
		 
		if (pattern[i] == '!') {
			negated = 1;
			i++;
		} else
			negated = 0;

		 
		for (subi = 0;
		    i < len && subi < sizeof(sub) - 1 && pattern[i] != ',';
		    subi++, i++)
			sub[subi] = pattern[i];
		 
		if (subi >= sizeof(sub) - 1)
			return 0;

		 
		if (i < len && pattern[i] == ',')
			i++;

		 
		sub[subi] = '\0';

		 
		if (_match_pattern(string, sub)) {
			if (negated)
				return -1;		 
			else
				got_positive = 1;	 
		}
	}

	 
	return got_positive;
}

#endif  
