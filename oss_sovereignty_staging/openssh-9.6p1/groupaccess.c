 
 

#include "includes.h"

#include <sys/types.h>

#include <grp.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "xmalloc.h"
#include "groupaccess.h"
#include "match.h"
#include "log.h"

static int ngroups;
static char **groups_byname;

 
int
ga_init(const char *user, gid_t base)
{
	gid_t *groups_bygid;
	int i, j, retry = 0;
	struct group *gr;

	if (ngroups > 0)
		ga_free();

	ngroups = NGROUPS_MAX;
#if defined(HAVE_SYSCONF) && defined(_SC_NGROUPS_MAX)
	ngroups = MAX(NGROUPS_MAX, sysconf(_SC_NGROUPS_MAX));
#endif

	groups_bygid = xcalloc(ngroups, sizeof(*groups_bygid));
	while (getgrouplist(user, base, groups_bygid, &ngroups) == -1) {
		if (retry++ > 0)
			fatal("getgrouplist: groups list too small");
		groups_bygid = xreallocarray(groups_bygid, ngroups,
		    sizeof(*groups_bygid));
	}
	groups_byname = xcalloc(ngroups, sizeof(*groups_byname));

	for (i = 0, j = 0; i < ngroups; i++)
		if ((gr = getgrgid(groups_bygid[i])) != NULL)
			groups_byname[j++] = xstrdup(gr->gr_name);
	free(groups_bygid);
	return (ngroups = j);
}

 
int
ga_match(char * const *groups, int n)
{
	int i, j;

	for (i = 0; i < ngroups; i++)
		for (j = 0; j < n; j++)
			if (match_pattern(groups_byname[i], groups[j]))
				return 1;
	return 0;
}

 
int
ga_match_pattern_list(const char *group_pattern)
{
	int i, found = 0;

	for (i = 0; i < ngroups; i++) {
		switch (match_usergroup_pattern_list(groups_byname[i],
		    group_pattern)) {
		case -1:
			return 0;	 
		case 0:
			continue;
		case 1:
			found = 1;
		}
	}
	return found;
}

 
void
ga_free(void)
{
	int i;

	if (ngroups > 0) {
		for (i = 0; i < ngroups; i++)
			free(groups_byname[i]);
		ngroups = 0;
		free(groups_byname);
		groups_byname = NULL;
	}
}
