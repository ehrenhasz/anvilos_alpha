#include <errno.h>
#include <libgen.h>
#include <libintl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "zpool_util.h"
void *
safe_malloc(size_t size)
{
	void *data;
	if ((data = calloc(1, size)) == NULL) {
		(void) fprintf(stderr, "internal error: out of memory\n");
		exit(1);
	}
	return (data);
}
void *
safe_realloc(void *from, size_t size)
{
	void *data;
	if ((data = realloc(from, size)) == NULL) {
		(void) fprintf(stderr, "internal error: out of memory\n");
		exit(1);
	}
	return (data);
}
void
zpool_no_memory(void)
{
	assert(errno == ENOMEM);
	(void) fprintf(stderr,
	    gettext("internal error: out of memory\n"));
	exit(1);
}
uint_t
num_logs(nvlist_t *nv)
{
	uint_t nlogs = 0;
	uint_t c, children;
	nvlist_t **child;
	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0)
		return (0);
	for (c = 0; c < children; c++) {
		uint64_t is_log = B_FALSE;
		(void) nvlist_lookup_uint64(child[c], ZPOOL_CONFIG_IS_LOG,
		    &is_log);
		if (is_log)
			nlogs++;
	}
	return (nlogs);
}
uint64_t
array64_max(uint64_t array[], unsigned int len)
{
	uint64_t max = 0;
	int i;
	for (i = 0; i < len; i++)
		max = MAX(max, array[i]);
	return (max);
}
int
highbit64(uint64_t i)
{
	if (i == 0)
		return (0);
	return (NBBY * sizeof (uint64_t) - __builtin_clzll(i));
}
int
lowbit64(uint64_t i)
{
	if (i == 0)
		return (0);
	return (__builtin_ffsll(i));
}
