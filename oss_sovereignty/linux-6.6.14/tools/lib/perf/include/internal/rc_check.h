 
#ifndef __LIBPERF_INTERNAL_RC_CHECK_H
#define __LIBPERF_INTERNAL_RC_CHECK_H

#include <stdlib.h>
#include <linux/zalloc.h>

 
#if defined(__SANITIZE_ADDRESS__) || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
#define REFCNT_CHECKING 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(leak_sanitizer)
#define REFCNT_CHECKING 1
#endif
#endif

 

#ifndef REFCNT_CHECKING
 
#define DECLARE_RC_STRUCT(struct_name)		\
	struct struct_name

 
#define RC_STRUCT(struct_name) struct struct_name

 
#define ADD_RC_CHK(result, object) (result = object, object)

 
#define RC_CHK_ACCESS(object) object

 
#define RC_CHK_FREE(object) free(object)

 
#define RC_CHK_GET(result, object) ADD_RC_CHK(result, object)

 
#define RC_CHK_PUT(object) {}

#else

 
#define DECLARE_RC_STRUCT(struct_name)			\
	struct original_##struct_name;			\
	struct struct_name {				\
		struct original_##struct_name *orig;	\
	};						\
	struct original_##struct_name

 
#define RC_STRUCT(struct_name) struct original_##struct_name

 
#define ADD_RC_CHK(result, object)					\
	(								\
		object ? (result = malloc(sizeof(*result)),		\
			result ? (result->orig = object, result)	\
			: (result = NULL, NULL))			\
		: (result = NULL, NULL)					\
		)

 
#define RC_CHK_ACCESS(object) object->orig

 
#define RC_CHK_FREE(object)			\
	do {					\
		zfree(&object->orig);		\
		free(object);			\
	} while(0)

 
#define RC_CHK_GET(result, object) ADD_RC_CHK(result, (object ? object->orig : NULL))

 
#define RC_CHK_PUT(object)			\
	do {					\
		if (object) {			\
			object->orig = NULL;	\
			free(object);		\
		}				\
	} while(0)

#endif

#endif  
