
#ifndef UTIL_LINUX_COLORS_H
#define UTIL_LINUX_COLORS_H

#include <stdio.h>
#include <unistd.h>

#include "color-names.h"


enum colortmode {
	UL_COLORMODE_AUTO = 0,
	UL_COLORMODE_NEVER,
	UL_COLORMODE_ALWAYS,
	UL_COLORMODE_UNDEF,

	__UL_NCOLORMODES	
};

#ifdef USE_COLORS_BY_DEFAULT
# define USAGE_COLORS_DEFAULT	_("colors are enabled by default")
#else
# define USAGE_COLORS_DEFAULT   _("colors are disabled by default")
#endif

extern int colormode_from_string(const char *str);
extern int colormode_or_err(const char *str, const char *errmsg);


extern int colors_init(int mode, const char *util_name);


extern int colors_wanted(void);


extern int colors_mode(void);


extern void colors_off(void);
extern void colors_on(void);



extern void color_fenable(const char *seq, FILE *f);

extern void color_scheme_fenable(const char *name, const char *dflt, FILE *f);
extern const char *color_scheme_get_sequence(const char *name, const char *dflt);

static inline void color_enable(const char *seq)
{
	color_fenable(seq, stdout);
}

static inline void color_scheme_enable(const char *name, const char *dflt)
{
	color_scheme_fenable(name, dflt, stdout);
}


extern void color_fdisable(FILE *f);

static inline void color_disable(void)
{
	color_fdisable(stdout);
}

const char *color_get_disable_sequence(void);

#endif 
