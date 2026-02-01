 

 

 

 

 

#include "includes.h"
#ifndef HAVE_FNMATCH

#include <fnmatch.h>
#include <string.h>
#include <ctype.h>

#include "charclass.h"

#define	RANGE_MATCH	1
#define	RANGE_NOMATCH	0
#define	RANGE_ERROR	(-1)

static int
classmatch(const char *pattern, char test, int foldcase, const char **ep)
{
	const char * const mismatch = pattern;
	const char *colon;
	struct cclass *cc;
	int rval = RANGE_NOMATCH;
	size_t len;

	if (pattern[0] != '[' || pattern[1] != ':') {
		*ep = mismatch;
		return RANGE_ERROR;
	}
	pattern += 2;

	if ((colon = strchr(pattern, ':')) == NULL || colon[1] != ']') {
		*ep = mismatch;
		return RANGE_ERROR;
	}
	*ep = colon + 2;
	len = (size_t)(colon - pattern);

	if (foldcase && strncmp(pattern, "upper:]", 7) == 0)
		pattern = "lower:]";
	for (cc = cclasses; cc->name != NULL; cc++) {
		if (!strncmp(pattern, cc->name, len) && cc->name[len] == '\0') {
			if (cc->isctype((unsigned char)test))
				rval = RANGE_MATCH;
			break;
		}
	}
	if (cc->name == NULL) {
		 
		*ep = mismatch;
		rval = RANGE_ERROR;
	}
	return rval;
}

 
static int fnmatch_ch(const char **pattern, const char **string, int flags)
{
	const char * const mismatch = *pattern;
	const int nocase = !!(flags & FNM_CASEFOLD);
	const int escape = !(flags & FNM_NOESCAPE);
	const int slash = !!(flags & FNM_PATHNAME);
	int result = FNM_NOMATCH;
	const char *startch;
	int negate;

	if (**pattern == '[') {
		++*pattern;

		 
		negate = (**pattern == '!') || (**pattern == '^');
		if (negate)
			++*pattern;

		 
		if (**pattern == ']')
			goto leadingclosebrace;

		while (**pattern) {
			if (**pattern == ']') {
				++*pattern;
				 
				++*string;
				return (result ^ negate);
			}

			if (escape && (**pattern == '\\')) {
				++*pattern;

				 
				if (!**pattern)
					break;
			}

			 
			if (slash && (**pattern == '/'))
				break;

			 
			switch (classmatch(*pattern, **string, nocase, pattern)) {
			case RANGE_MATCH:
				result = 0;
				continue;
			case RANGE_NOMATCH:
				 
				continue;
			default:
				 
				break;
			}
			if (!**pattern)
				break;

leadingclosebrace:
			 
			if (((*pattern)[1] == '-') && ((*pattern)[2] != ']')) {
				startch = *pattern;
				*pattern += (escape && ((*pattern)[2] == '\\')) ? 3 : 2;

				 
				if (!**pattern || (slash && (**pattern == '/')))
					break;

				 
				if ((**string >= *startch) && (**string <= **pattern))
					result = 0;
				else if (nocase &&
				    (isupper((unsigned char)**string) ||
				     isupper((unsigned char)*startch) ||
				     isupper((unsigned char)**pattern)) &&
				    (tolower((unsigned char)**string) >=
				     tolower((unsigned char)*startch)) &&
				    (tolower((unsigned char)**string) <=
				     tolower((unsigned char)**pattern)))
					result = 0;

				++*pattern;
				continue;
			}

			 
			if ((**string == **pattern))
				result = 0;
			else if (nocase && (isupper((unsigned char)**string) ||
			    isupper((unsigned char)**pattern)) &&
			    (tolower((unsigned char)**string) ==
			    tolower((unsigned char)**pattern)))
				result = 0;

			++*pattern;
		}
		 
		*pattern = mismatch;
		result = FNM_NOMATCH;
	} else if (**pattern == '?') {
		 
		if (!**string || (slash && (**string == '/')))
			return FNM_NOMATCH;
		result = 0;
		goto fnmatch_ch_success;
	} else if (escape && (**pattern == '\\') && (*pattern)[1]) {
		++*pattern;
	}

	 
	if (**string == **pattern)
		result = 0;
	else if (nocase && (isupper((unsigned char)**string) ||
	    isupper((unsigned char)**pattern)) &&
	    (tolower((unsigned char)**string) ==
	    tolower((unsigned char)**pattern)))
		result = 0;

	 
	if (**string == '\0' || **pattern == '\0' ||
	    (slash && ((**string == '/') || (**pattern == '/'))))
		return result;

fnmatch_ch_success:
	++*pattern;
	++*string;
	return result;
}


int fnmatch(const char *pattern, const char *string, int flags)
{
	static const char dummystring[2] = {' ', 0};
	const int escape = !(flags & FNM_NOESCAPE);
	const int slash = !!(flags & FNM_PATHNAME);
	const int leading_dir = !!(flags & FNM_LEADING_DIR);
	const char *dummyptr, *matchptr, *strendseg;
	int wild;
	 
	const char *strstartseg = NULL;
	const char *mismatch = NULL;
	int matchlen = 0;

	if (*pattern == '*')
		goto firstsegment;

	while (*pattern && *string) {
		 
		if (slash && escape && (*pattern == '\\') && (pattern[1] == '/'))
			++pattern;
		if (slash && (*pattern == '/') && (*string == '/')) {
			++pattern;
			++string;
		}

firstsegment:
		 
		if ((flags & FNM_PERIOD) && (*string == '.')) {
		    if (*pattern == '.')
			    ++pattern;
		    else if (escape && (*pattern == '\\') && (pattern[1] == '.'))
			    pattern += 2;
		    else
			    return FNM_NOMATCH;
		    ++string;
		}

		 
		if (slash) {
			strendseg = strchr(string, '/');
			if (!strendseg)
				strendseg = strchr(string, '\0');
		} else {
			strendseg = strchr(string, '\0');
		}

		 
		while (*pattern) {
			if ((string > strendseg) ||
			    ((string == strendseg) && (*pattern != '*')))
				break;

			if (slash && ((*pattern == '/') ||
			    (escape && (*pattern == '\\') && (pattern[1] == '/'))))
				break;

			 
			for (wild = 0; (*pattern == '*') || (*pattern == '?'); ++pattern) {
				if (*pattern == '*') {
					wild = 1;
				} else if (string < strendseg) {   
					 
					++string;
				}
				else {   
					return FNM_NOMATCH;
				}
			}

			if (wild) {
				strstartseg = string;
				mismatch = pattern;

				 
				for (matchptr = pattern, matchlen = 0; 1; ++matchlen) {
					if ((*matchptr == '\0') ||
					    (slash && ((*matchptr == '/') ||
					    (escape && (*matchptr == '\\') &&
					    (matchptr[1] == '/'))))) {
						 
						 
						if (string + matchlen > strendseg)
							return FNM_NOMATCH;

						string = strendseg - matchlen;
						wild = 0;
						break;
					}

					if (*matchptr == '*') {
						 
						 
						if (string + matchlen > strendseg)
							return FNM_NOMATCH;

						 
						break;
					}

					 
					 
					if (escape && (*matchptr == '\\') &&
					    matchptr[1]) {
						matchptr += 2;
					} else if (*matchptr == '[') {
						dummyptr = dummystring;
						fnmatch_ch(&matchptr, &dummyptr,
						    flags);
					} else {
						++matchptr;
					}
				}
			}

			 
			while (*pattern && (string < strendseg)) {
				 
				if (*pattern == '*')
					break;

				if (slash && ((*string == '/') ||
				    (*pattern == '/') || (escape &&
				    (*pattern == '\\') && (pattern[1] == '/'))))
					break;

				 
				if (!fnmatch_ch(&pattern, &string, flags))
					continue;

				 
				if (wild) {
					 
					string = ++strstartseg;
					if (string + matchlen > strendseg)
						return FNM_NOMATCH;

					pattern = mismatch;
					continue;
				} else
					return FNM_NOMATCH;
			}
		}

		if (*string && !((slash || leading_dir) && (*string == '/')))
			return FNM_NOMATCH;

		if (*pattern && !(slash && ((*pattern == '/') ||
		    (escape && (*pattern == '\\') && (pattern[1] == '/')))))
			return FNM_NOMATCH;

		if (leading_dir && !*pattern && *string == '/')
			return 0;
	}

	 
	if (!*string && !*pattern)
		return 0;

	 
	return FNM_NOMATCH;
}
#endif  
