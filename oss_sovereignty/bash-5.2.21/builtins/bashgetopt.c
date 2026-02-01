 

 

#include <config.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include "../bashansi.h"
#include <chartypes.h>
#include <errno.h>

#include "../shell.h"
#include "common.h"

#include "bashgetopt.h"

#define ISOPT(s)	(((*(s) == '-') || (plus && *(s) == '+')) && (s)[1])
#define NOTOPT(s)	(((*(s) != '-') && (!plus || *(s) != '+')) || (s)[1] == '\0')
			
static int	sp;

char    *list_optarg;
int	list_optflags;
int	list_optopt;
int	list_opttype;

static WORD_LIST *lhead = (WORD_LIST *)NULL;
WORD_LIST	*lcurrent = (WORD_LIST *)NULL;
WORD_LIST	*loptend;	 

int
internal_getopt(list, opts)
WORD_LIST	*list;
char		*opts;
{
	register int c;
	register char *cp;
	int	plus;	 
	static char errstr[3] = { '-', '\0', '\0' };

	plus = *opts == '+';
	if (plus)
		opts++;

	if (list == 0) {
		list_optarg = (char *)NULL;
		list_optflags = 0;
		loptend = (WORD_LIST *)NULL;	 
		return -1;
	}

	if (list != lhead || lhead == 0) {
		 
		sp = 1;
		lcurrent = lhead = list;
		loptend = (WORD_LIST *)NULL;
	}

	if (sp == 1) {
		if (lcurrent == 0 || NOTOPT(lcurrent->word->word)) {
		    	lhead = (WORD_LIST *)NULL;
		    	loptend = lcurrent;
			return(-1);
		} else if (ISHELP (lcurrent->word->word)) {
			lhead = (WORD_LIST *)NULL;
			loptend = lcurrent;
			return (GETOPT_HELP);
		} else if (lcurrent->word->word[0] == '-' &&
			   lcurrent->word->word[1] == '-' &&
			   lcurrent->word->word[2] == 0) {
			lhead = (WORD_LIST *)NULL;
			loptend = lcurrent->next;
			return(-1);
		}
		errstr[0] = list_opttype = lcurrent->word->word[0];
	}

	list_optopt = c = lcurrent->word->word[sp];

	if (c == ':' || (cp = strchr(opts, c)) == NULL) {
		errstr[1] = c;
		sh_invalidopt (errstr);		
		if (lcurrent->word->word[++sp] == '\0') {
			lcurrent = lcurrent->next;
			sp = 1;
		}
		list_optarg = NULL;
		list_optflags = 0;
		if (lcurrent)
			loptend = lcurrent->next;
		return('?');
	}

	if (*++cp == ':' || *cp == ';') {
		 
		 
		 
		if (lcurrent->word->word[sp+1]) {
			list_optarg = lcurrent->word->word + sp + 1;
			list_optflags = 0;
			lcurrent = lcurrent->next;
		 
#if 0
		} else if (lcurrent->next && (*cp == ':' || lcurrent->next->word->word[0] != '-')) {
#else
		} else if (lcurrent->next && (*cp == ':' || NOTOPT(lcurrent->next->word->word))) {
#endif
			lcurrent = lcurrent->next;
			list_optarg = lcurrent->word->word;
			list_optflags = lcurrent->word->flags;
			lcurrent = lcurrent->next;
		} else if (*cp == ';') {
			list_optarg = (char *)NULL;
			list_optflags = 0;
			lcurrent = lcurrent->next;
		} else {	 
			errstr[1] = c;
			sh_needarg (errstr);
			sp = 1;
			list_optarg = (char *)NULL;
			list_optflags = 0;
			return('?');
		}
		sp = 1;
	} else if (*cp == '#') {
		 
		if (lcurrent->word->word[sp+1]) {
			if (DIGIT(lcurrent->word->word[sp+1])) {
				list_optarg = lcurrent->word->word + sp + 1;
				list_optflags = 0;
				lcurrent = lcurrent->next;
			} else {
				list_optarg = (char *)NULL;
				list_optflags = 0;
			}
		} else {
			if (lcurrent->next && legal_number(lcurrent->next->word->word, (intmax_t *)0)) {
				lcurrent = lcurrent->next;
				list_optarg = lcurrent->word->word;
				list_optflags = lcurrent->word->flags;
				lcurrent = lcurrent->next;
			} else {
				errstr[1] = c;
				sh_neednumarg (errstr);
				sp = 1;
				list_optarg = (char *)NULL;
				list_optflags = 0;
				return ('?');
			}
		}

	} else {
		 
		if (lcurrent->word->word[++sp] == '\0') {
			sp = 1;
			lcurrent = lcurrent->next;
		}
		list_optarg = (char *)NULL;
		list_optflags = 0;
	}

	return(c);
}

 

void
reset_internal_getopt ()
{
	lhead = lcurrent = loptend = (WORD_LIST *)NULL;
	sp = 1;
}
