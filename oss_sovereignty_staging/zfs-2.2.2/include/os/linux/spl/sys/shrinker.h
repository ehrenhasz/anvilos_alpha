 

#ifndef _SPL_SHRINKER_H
#define	_SPL_SHRINKER_H

#include <linux/mm.h>
#include <linux/fs.h>

 

#ifdef HAVE_REGISTER_SHRINKER_VARARG
#define	spl_register_shrinker(x)	register_shrinker(x, "zfs-arc-shrinker")
#else
#define	spl_register_shrinker(x)	register_shrinker(x)
#endif
#define	spl_unregister_shrinker(x)	unregister_shrinker(x)

 
#if defined(HAVE_SINGLE_SHRINKER_CALLBACK)
#define	SPL_SHRINKER_DECLARE(varname, countfunc, scanfunc, seek_cost)	\
static int								\
__ ## varname ## _wrapper(struct shrinker *shrink, struct shrink_control *sc)\
{									\
	if (sc->nr_to_scan != 0) {					\
		(void) scanfunc(shrink, sc);				\
	}								\
	return (countfunc(shrink, sc));					\
}									\
									\
static struct shrinker varname = {					\
	.shrink = __ ## varname ## _wrapper,				\
	.seeks = seek_cost,						\
}

#define	SHRINK_STOP	(-1)

 
#elif defined(HAVE_SPLIT_SHRINKER_CALLBACK)
#define	SPL_SHRINKER_DECLARE(varname, countfunc, scanfunc, seek_cost)	\
static struct shrinker varname = {					\
	.count_objects = countfunc,					\
	.scan_objects = scanfunc,					\
	.seeks = seek_cost,						\
}

#else
 
#error "Unknown shrinker callback"
#endif

#endif  
