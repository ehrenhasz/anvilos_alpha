 

 

#ifndef ERROR_H_MODULE
#define ERROR_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif


 
#include <stddef.h>         
#include "zstd_errors.h"   


 
#if defined(__GNUC__)
#  define ERR_STATIC static __attribute__((unused))
#elif defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)  )
#  define ERR_STATIC static inline
#elif defined(_MSC_VER)
#  define ERR_STATIC static __inline
#else
#  define ERR_STATIC static   
#endif


 
typedef ZSTD_ErrorCode ERR_enum;
#define PREFIX(name) ZSTD_error_##name


 
#undef ERROR    
#define ERROR(name) ZSTD_ERROR(name)
#define ZSTD_ERROR(name) ((size_t)-PREFIX(name))

ERR_STATIC unsigned ERR_isError(size_t code) { return (code > ERROR(maxCode)); }

ERR_STATIC ERR_enum ERR_getErrorCode(size_t code) { if (!ERR_isError(code)) return (ERR_enum)0; return (ERR_enum) (0-code); }

 
#define CHECK_V_F(e, f) size_t const e = f; if (ERR_isError(e)) return e
#define CHECK_F(f)   { CHECK_V_F(_var_err__, f); }


 

const char* ERR_getErrorString(ERR_enum code);    

ERR_STATIC const char* ERR_getErrorName(size_t code)
{
    return ERR_getErrorString(ERR_getErrorCode(code));
}

#if defined (__cplusplus)
}
#endif

#endif  
