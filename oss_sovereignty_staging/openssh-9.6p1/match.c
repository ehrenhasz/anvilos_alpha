 
 
 

#include "includes.h"

#include <sys/types.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "xmalloc.h"
#include "match.h"
#include "misc.h"

 
int
match_pattern(const char *s, const char *pattern)
{
	for (;;) {
		 
		if (!*pattern)
			return !*s;

		if (*pattern == '*') {
			 
			while (*pattern == '*')
				pattern++;

			 
			if (!*pattern)
				return 1;

			 
			if (*pattern != '?' && *pattern != '*') {
				 
				for (; *s; s++)
					if (*s == *pattern &&
					    match_pattern(s + 1, pattern + 1))
						return 1;
				 
				return 0;
			}
			 
			for (; *s; s++)
				if (match_pattern(s, pattern))
					return 1;
			 
			return 0;
		}
		 
		if (!*s)
			return 0;

		 
		if (*pattern != '?' && *pattern != *s)
			return 0;

		 
		s++;
		pattern++;
	}
	 
}

 
int
match_pattern_list(const char *string, const char *pattern, int dolower)
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
			sub[subi] = dolower && isupper((u_char)pattern[i]) ?
			    tolower((u_char)pattern[i]) : pattern[i];
		 
		if (subi >= sizeof(sub) - 1)
			return 0;

		 
		if (i < len && pattern[i] == ',')
			i++;

		 
		sub[subi] = '\0';

		 
		if (match_pattern(string, sub)) {
			if (negated)
				return -1;		 
			else
				got_positive = 1;	 
		}
	}

	 
	return got_positive;
}

 
int
match_usergroup_pattern_list(const char *string, const char *pattern)
{
#ifdef HAVE_CYGWIN
	 
	return cygwin_ug_match_pattern_list(string, pattern);
#else
	 
	return match_pattern_list(string, pattern, 0);
#endif
}

 
int
match_hostname(const char *host, const char *pattern)
{
	char *hostcopy = xstrdup(host);
	int r;

	lowercase(hostcopy);
	r = match_pattern_list(hostcopy, pattern, 1);
	free(hostcopy);
	return r;
}

 
int
match_host_and_ip(const char *host, const char *ipaddr,
    const char *patterns)
{
	int mhost, mip;

	if ((mip = addr_match_list(ipaddr, patterns)) == -2)
		return -1;  
	else if (host == NULL || ipaddr == NULL || mip == -1)
		return 0;  

	 
	if ((mhost = match_hostname(host, patterns)) == -1)
		return 0;
	 
	if (mhost == 0 && mip == 0)
		return 0;
	return 1;
}

 
int
match_user(const char *user, const char *host, const char *ipaddr,
    const char *pattern)
{
	char *p, *pat;
	int ret;

	 
	if (user == NULL && host == NULL && ipaddr == NULL) {
		if ((p = strchr(pattern, '@')) != NULL &&
		    match_host_and_ip(NULL, NULL, p + 1) < 0)
			return -1;
		return 0;
	}

	if (user == NULL)
		return 0;  

	if ((p = strchr(pattern, '@')) == NULL)
		return match_pattern(user, pattern);

	pat = xstrdup(pattern);
	p = strchr(pat, '@');
	*p++ = '\0';

	if ((ret = match_pattern(user, pat)) == 1)
		ret = match_host_and_ip(host, ipaddr, p);
	free(pat);

	return ret;
}

 
#define	MAX_PROP	40
#define	SEP	","
char *
match_list(const char *client, const char *server, u_int *next)
{
	char *sproposals[MAX_PROP];
	char *c, *s, *p, *ret, *cp, *sp;
	int i, j, nproposals;

	c = cp = xstrdup(client);
	s = sp = xstrdup(server);

	for ((p = strsep(&sp, SEP)), i=0; p && *p != '\0';
	    (p = strsep(&sp, SEP)), i++) {
		if (i < MAX_PROP)
			sproposals[i] = p;
		else
			break;
	}
	nproposals = i;

	for ((p = strsep(&cp, SEP)), i=0; p && *p != '\0';
	    (p = strsep(&cp, SEP)), i++) {
		for (j = 0; j < nproposals; j++) {
			if (strcmp(p, sproposals[j]) == 0) {
				ret = xstrdup(p);
				if (next != NULL)
					*next = (cp == NULL) ?
					    strlen(c) : (u_int)(cp - c);
				free(c);
				free(s);
				return ret;
			}
		}
	}
	if (next != NULL)
		*next = strlen(c);
	free(c);
	free(s);
	return NULL;
}

 
static char *
filter_list(const char *proposal, const char *filter, int denylist)
{
	size_t len = strlen(proposal) + 1;
	char *fix_prop = malloc(len);
	char *orig_prop = strdup(proposal);
	char *cp, *tmp;
	int r;

	if (fix_prop == NULL || orig_prop == NULL) {
		free(orig_prop);
		free(fix_prop);
		return NULL;
	}

	tmp = orig_prop;
	*fix_prop = '\0';
	while ((cp = strsep(&tmp, ",")) != NULL) {
		r = match_pattern_list(cp, filter, 0);
		if ((denylist && r != 1) || (!denylist && r == 1)) {
			if (*fix_prop != '\0')
				strlcat(fix_prop, ",", len);
			strlcat(fix_prop, cp, len);
		}
	}
	free(orig_prop);
	return fix_prop;
}

 
char *
match_filter_denylist(const char *proposal, const char *filter)
{
	return filter_list(proposal, filter, 1);
}

 
char *
match_filter_allowlist(const char *proposal, const char *filter)
{
	return filter_list(proposal, filter, 0);
}
