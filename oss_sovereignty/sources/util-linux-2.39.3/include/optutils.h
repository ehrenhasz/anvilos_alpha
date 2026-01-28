
#ifndef UTIL_LINUX_OPTUTILS_H
#define UTIL_LINUX_OPTUTILS_H

#include <assert.h>

#include "c.h"
#include "nls.h"
#include "cctype.h"

static inline const char *option_to_longopt(int c, const struct option *opts)
{
	const struct option *o;

	assert(!(opts == NULL));
	for (o = opts; o->name; o++)
		if (o->val == c)
			return o->name;
	return NULL;
}

#ifndef OPTUTILS_EXIT_CODE
# define OPTUTILS_EXIT_CODE EXIT_FAILURE
#endif


#define UL_EXCL_STATUS_INIT	{ 0 }
typedef int ul_excl_t[16];

static inline void err_exclusive_options(
			int c,
			const struct option *opts,
			const ul_excl_t *excl,
			int *status)
{
	int e;

	for (e = 0; excl[e][0] && excl[e][0] <= c; e++) {
		const int *op = excl[e];

		for (; *op && *op <= c; op++) {
			if (*op != c)
				continue;
			if (status[e] == 0)
				status[e] = c;
			else if (status[e] != c) {
				size_t ct = 0;

				fprintf(stderr, _("%s: mutually exclusive "
						  "arguments:"),
						program_invocation_short_name);

				for (op = excl[e];
				     ct + 1 < ARRAY_SIZE(excl[0]) && *op;
				     op++, ct++) {
					const char *n = option_to_longopt(*op, opts);
					if (n)
						fprintf(stderr, " --%s", n);
					else if (c_isgraph(*op))
						fprintf(stderr, " -%c", *op);
				}
				fputc('\n', stderr);
				exit(OPTUTILS_EXIT_CODE);
			}
			break;
		}
	}
}

#endif

