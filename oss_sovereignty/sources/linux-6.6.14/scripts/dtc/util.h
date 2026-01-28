
#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <getopt.h>



#ifdef __GNUC__
#if __GNUC__ >= 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)
#define PRINTF(i, j)	__attribute__((format (gnu_printf, i, j)))
#else
#define PRINTF(i, j)	__attribute__((format (printf, i, j)))
#endif
#define NORETURN	__attribute__((noreturn))
#else
#define PRINTF(i, j)
#define NORETURN
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define stringify(s)	stringify_(s)
#define stringify_(s)	#s

static inline void NORETURN PRINTF(1, 2) die(const char *str, ...)
{
	va_list ap;

	va_start(ap, str);
	fprintf(stderr, "FATAL ERROR: ");
	vfprintf(stderr, str, ap);
	va_end(ap);
	exit(1);
}

static inline void *xmalloc(size_t len)
{
	void *new = malloc(len);

	if (!new)
		die("malloc() failed\n");

	return new;
}

static inline void *xrealloc(void *p, size_t len)
{
	void *new = realloc(p, len);

	if (!new)
		die("realloc() failed (len=%zd)\n", len);

	return new;
}

extern char *xstrdup(const char *s);
extern char *xstrndup(const char *s, size_t len);

extern int PRINTF(2, 3) xasprintf(char **strp, const char *fmt, ...);
extern int PRINTF(2, 3) xasprintf_append(char **strp, const char *fmt, ...);
extern int xavsprintf_append(char **strp, const char *fmt, va_list ap);
extern char *join_path(const char *path, const char *name);


bool util_is_printable_string(const void *data, int len);


char get_escape_char(const char *s, int *i);


char *utilfdt_read(const char *filename, size_t *len);


int utilfdt_read_err(const char *filename, char **buffp, size_t *len);


int utilfdt_write(const char *filename, const void *blob);


int utilfdt_write_err(const char *filename, const void *blob);


int utilfdt_decode_type(const char *fmt, int *type, int *size);



#define USAGE_TYPE_MSG \
	"<type>\ts=string, i=int, u=unsigned, x=hex, r=raw\n" \
	"\tOptional modifier prefix:\n" \
	"\t\thh or b=byte, h=2 byte, l=4 byte (default)";


void utilfdt_print_data(const char *data, int len);


void NORETURN util_version(void);


void NORETURN util_usage(const char *errmsg, const char *synopsis,
			 const char *short_opts,
			 struct option const long_opts[],
			 const char * const opts_help[]);


#define usage(errmsg) \
	util_usage(errmsg, usage_synopsis, usage_short_opts, \
		   usage_long_opts, usage_opts_help)


#define util_getopt_long() getopt_long(argc, argv, usage_short_opts, \
				       usage_long_opts, NULL)


#define a_argument required_argument


#define USAGE_COMMON_SHORT_OPTS "hV"


#define USAGE_COMMON_LONG_OPTS \
	{"help",      no_argument, NULL, 'h'}, \
	{"version",   no_argument, NULL, 'V'}, \
	{NULL,        no_argument, NULL, 0x0}


#define USAGE_COMMON_OPTS_HELP \
	"Print this help and exit", \
	"Print version and exit", \
	NULL


#define case_USAGE_COMMON_FLAGS \
	case 'h': usage(NULL); \
	case 'V': util_version(); \
	case '?': usage("unknown option");

#endif 
