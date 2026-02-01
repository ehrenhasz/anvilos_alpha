 

 

#include <config.h>

 
#include <errno.h>

 
#ifndef __set_errno
# define __set_errno(val) (errno = (val))
#endif

 

#ifndef __GNUC_PREREQ
# if defined __GNUC__ && defined __GNUC_MINOR__
#  define __GNUC_PREREQ(maj, min) ((maj) < __GNUC__ + ((min) <= __GNUC_MINOR__))
# else
#  define __GNUC_PREREQ(maj, min) 0
# endif
#endif

#ifndef __glibc_clang_prereq
# if defined __clang_major__ && defined __clang_minor__
#  ifdef __apple_build_version__
 
#   define __glibc_clang_prereq(maj, min) \
      ((maj) == 3 && (min) == 5 ? 6000000 <= __apple_build_version__ : 0)
#  else
#   define __glibc_clang_prereq(maj, min) \
      ((maj) < __clang_major__ + ((min) <= __clang_minor__))
#  endif
# else
#  define __glibc_clang_prereq(maj, min) 0
# endif
#endif

#ifndef __attribute_nonnull__
 

 
# ifndef _FEATURES_H
#  define _FEATURES_H 1
# endif
 
# define __GNULIB_CDEFS
 
# undef _SYS_CDEFS_H
# undef __ASMNAME
# undef __ASMNAME2
# undef __BEGIN_DECLS
# undef __CONCAT
# undef __END_DECLS
# undef __HAVE_GENERIC_SELECTION
# undef __LDBL_COMPAT
# undef __LDBL_REDIR
# undef __LDBL_REDIR1
# undef __LDBL_REDIR1_DECL
# undef __LDBL_REDIR1_NTH
# undef __LDBL_REDIR2_DECL
# undef __LDBL_REDIR_DECL
# undef __LDBL_REDIR_NTH
# undef __LEAF
# undef __LEAF_ATTR
# undef __NTH
# undef __NTHNL
# undef __REDIRECT
# undef __REDIRECT_LDBL
# undef __REDIRECT_NTH
# undef __REDIRECT_NTHNL
# undef __REDIRECT_NTH_LDBL
# undef __STRING
# undef __THROW
# undef __THROWNL
# undef __attr_access
# undef __attr_access_none
# undef __attr_dealloc
# undef __attr_dealloc_free
# undef __attribute__
# undef __attribute_alloc_align__
# undef __attribute_alloc_size__
# undef __attribute_artificial__
# undef __attribute_const__
# undef __attribute_deprecated__
# undef __attribute_deprecated_msg__
# undef __attribute_format_arg__
# undef __attribute_format_strfmon__
# undef __attribute_malloc__
# undef __attribute_maybe_unused__
# undef __attribute_noinline__
# undef __attribute_nonstring__
# undef __attribute_pure__
# undef __attribute_returns_twice__
# undef __attribute_used__
# undef __attribute_warn_unused_result__
# undef __errordecl
# undef __extension__
# undef __extern_always_inline
# undef __extern_inline
# undef __flexarr
# undef __fortified_attr_access
# undef __fortify_function
# undef __glibc_c99_flexarr_available
# undef __glibc_has_attribute
# undef __glibc_has_builtin
# undef __glibc_has_extension
# undef __glibc_likely
# undef __glibc_macro_warning
# undef __glibc_macro_warning1
# undef __glibc_unlikely
# undef __inline
# undef __ptr_t
# undef __restrict
# undef __restrict_arr
# undef __va_arg_pack
# undef __va_arg_pack_len
# undef __warnattr
# undef __wur
# ifndef __GNULIB_CDEFS
#  undef __bos
#  undef __bos0
#  undef __glibc_fortify
#  undef __glibc_fortify_n
#  undef __glibc_objsize
#  undef __glibc_objsize0
#  undef __glibc_safe_len_cond
#  undef __glibc_safe_or_unknown_len
#  undef __glibc_unsafe_len
#  undef __glibc_unsigned_or_positive
# endif

 
# include <cdefs.h>

 
# undef __inline
# ifndef HAVE___INLINE
#  if 199901 <= __STDC_VERSION__ || defined inline
#   define __inline inline
#  else
#   define __inline
#  endif
# endif

#endif  


 
#define attribute_hidden
#define libc_hidden_proto(name)
#define libc_hidden_def(name)
#define libc_hidden_weak(name)
#define libc_hidden_ver(local, name)
#define strong_alias(name, aliasname)
#define weak_alias(name, aliasname)

 
#define SHLIB_COMPAT(lib, introduced, obsoleted) 0
#define compat_symbol(lib, local, symbol, version) extern int dummy
#define versioned_symbol(lib, local, symbol, version) extern int dummy
