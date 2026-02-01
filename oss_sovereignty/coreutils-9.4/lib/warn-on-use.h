 
#ifndef _GL_WARN_ON_USE

# if 4 < __GNUC__ || (__GNUC__ == 4 && 3 <= __GNUC_MINOR__)
 
#  define _GL_WARN_ON_USE(function, message) \
_GL_WARN_EXTERN_C __typeof__ (function) function __attribute__ ((__warning__ (message)))
#  define _GL_WARN_ON_USE_ATTRIBUTE(message) \
  __attribute__ ((__warning__ (message)))
# elif __clang_major__ >= 4
 
#  define _GL_WARN_ON_USE(function, message) \
_GL_WARN_EXTERN_C __typeof__ (function) function \
  __attribute__ ((__diagnose_if__ (1, message, "warning")))
#  define _GL_WARN_ON_USE_ATTRIBUTE(message) \
  __attribute__ ((__diagnose_if__ (1, message, "warning")))
# elif __GNUC__ >= 3 && GNULIB_STRICT_CHECKING
 
#  define _GL_WARN_ON_USE(function, message) \
_GL_WARN_EXTERN_C __typeof__ (function) function
#  define _GL_WARN_ON_USE_ATTRIBUTE(message)
# else  
#  define _GL_WARN_ON_USE(function, message) \
_GL_WARN_EXTERN_C int _gl_warn_on_use
#  define _GL_WARN_ON_USE_ATTRIBUTE(message)
# endif
#endif

 
#ifndef _GL_WARN_ON_USE_CXX
# if !defined __cplusplus
#  define _GL_WARN_ON_USE_CXX(function,rettype_gcc,rettype_clang,parameters_and_attributes,msg) \
     _GL_WARN_ON_USE (function, msg)
# else
#  if 4 < __GNUC__ || (__GNUC__ == 4 && 3 <= __GNUC_MINOR__)
 
#   define _GL_WARN_ON_USE_CXX(function,rettype_gcc,rettype_clang,parameters_and_attributes,msg) \
extern rettype_gcc function parameters_and_attributes \
  __attribute__ ((__warning__ (msg)))
#  elif __clang_major__ >= 4
 
#   define _GL_WARN_ON_USE_CXX(function,rettype_gcc,rettype_clang,parameters_and_attributes,msg) \
extern rettype_clang function parameters_and_attributes \
  __attribute__ ((__diagnose_if__ (1, msg, "warning")))
#  elif __GNUC__ >= 3 && GNULIB_STRICT_CHECKING
 
#   define _GL_WARN_ON_USE_CXX(function,rettype_gcc,rettype_clang,parameters_and_attributes,msg) \
extern rettype_gcc function parameters_and_attributes
#  else  
#   define _GL_WARN_ON_USE_CXX(function,rettype_gcc,rettype_clang,parameters_and_attributes,msg) \
_GL_WARN_EXTERN_C int _gl_warn_on_use
#  endif
# endif
#endif

 
#ifndef _GL_WARN_EXTERN_C
# if defined __cplusplus
#  define _GL_WARN_EXTERN_C extern "C"
# else
#  define _GL_WARN_EXTERN_C extern
# endif
#endif
