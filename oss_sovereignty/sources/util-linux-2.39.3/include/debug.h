
#ifndef UTIL_LINUX_DEBUG_H
#define UTIL_LINUX_DEBUG_H




#include <stdarg.h>
#include <string.h>

struct ul_debug_maskname {
	const char *name;
	int mask;
	const char *help;
};
#define UL_DEBUG_EMPTY_MASKNAMES {{ NULL, 0, NULL }}
#define UL_DEBUG_DEFINE_MASKNAMES(m) static const struct ul_debug_maskname m ## _masknames[]
#define UL_DEBUG_MASKNAMES(m)	m ## _masknames

#define UL_DEBUG_MASK(m)         m ## _debug_mask
#define UL_DEBUG_DEFINE_MASK(m)  int UL_DEBUG_MASK(m)
#define UL_DEBUG_DECLARE_MASK(m) extern UL_DEBUG_DEFINE_MASK(m)


#define __UL_DEBUG_FL_NOADDR	(1 << 24)	



#define __UL_DBG(l, p, m, x) \
	do { \
		if ((p ## m) & l ## _debug_mask) { \
			fprintf(stderr, "%d: %s: %8s: ", getpid(), # l, # m); \
			x; \
		} \
	} while (0)

#define __UL_DBG_CALL(l, p, m, x) \
	do { \
		if ((p ## m) & l ## _debug_mask) { \
			x; \
		} \
	} while (0)

#define __UL_DBG_FLUSH(l, p) \
	do { \
		if (l ## _debug_mask && \
		    l ## _debug_mask != p ## INIT) { \
			fflush(stderr); \
		} \
	} while (0)

#define __UL_INIT_DEBUG_FROM_STRING(lib, pref, mask, str) \
	do { \
		if (lib ## _debug_mask & pref ## INIT) \
		; \
		else if (!mask && str) { \
			lib ## _debug_mask = ul_debug_parse_mask(lib ## _masknames, str); \
		} else \
			lib ## _debug_mask = mask; \
		if (lib ## _debug_mask) { \
			if (getuid() != geteuid() || getgid() != getegid()) { \
				lib ## _debug_mask |= __UL_DEBUG_FL_NOADDR; \
				fprintf(stderr, "%d: %s: don't print memory addresses (SUID executable).\n", getpid(), # lib); \
			} \
		} \
		lib ## _debug_mask |= pref ## INIT; \
	} while (0)


#define __UL_INIT_DEBUG_FROM_ENV(lib, pref, mask, env) \
	do { \
		const char *envstr = mask ? NULL : getenv(# env); \
		__UL_INIT_DEBUG_FROM_STRING(lib, pref, mask, envstr); \
	} while (0)



static inline void __attribute__ ((__format__ (__printf__, 1, 2)))
ul_debug(const char *mesg, ...)
{
	va_list ap;
	va_start(ap, mesg);
	vfprintf(stderr, mesg, ap);
	va_end(ap);
	fputc('\n', stderr);
}

static inline int ul_debug_parse_mask(
			const struct ul_debug_maskname flagnames[],
			const char *mask)
{
	int res;
	char *ptr;

	
	res = strtoul(mask, &ptr, 0);

	
	if (ptr && *ptr && flagnames && flagnames[0].name) {
		char *msbuf, *ms, *name;
		res = 0;

		ms = msbuf = strdup(mask);
		if (!ms)
			return res;

		while ((name = strtok_r(ms, ",", &ptr))) {
			const struct ul_debug_maskname *d;
			ms = ptr;

			for (d = flagnames; d && d->name; d++) {
				if (strcmp(name, d->name) == 0) {
					res |= d->mask;
					break;
				}
			}
			
			if (res == 0xffff)
				break;
		}
		free(msbuf);
	} else if (ptr && strcmp(ptr, "all") == 0)
		res = 0xffff;

	return res;
}

static inline void ul_debug_print_masks(
			const char *env,
			const struct ul_debug_maskname flagnames[])
{
	const struct ul_debug_maskname *d;

	if (!flagnames)
		return;

	fprintf(stderr, "Available \"%s=<name>[,...]|<mask>\" debug masks:\n",
			env);
	for (d = flagnames; d && d->name; d++) {
		if (!d->help)
			continue;
		fprintf(stderr, "   %-8s [0x%06x] : %s\n",
				d->name, d->mask, d->help);
	}
}

#endif 
