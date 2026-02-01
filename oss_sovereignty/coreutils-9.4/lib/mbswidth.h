 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stddef.h>

 
#if HAVE_DECL_MBSWIDTH_IN_WCHAR_H
# include <wchar.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif


 

 
#define MBSW_REJECT_INVALID 1

 
#define MBSW_REJECT_UNPRINTABLE 2


 
#define mbswidth gnu_mbswidth   
extern int mbswidth (const char *string, int flags);

 
extern int mbsnwidth (const char *buf, size_t nbytes, int flags);


#ifdef __cplusplus
}
#endif
