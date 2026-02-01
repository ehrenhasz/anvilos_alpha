 
#ifndef _FEATURES_H
# include <features.h>
#endif

 
#if defined __GNUC__ && !defined __STDC__
# error "You need a ISO C conforming compiler to use the glibc headers"
#endif

 
#undef	__P
#undef	__PMT

 
#if (defined __has_attribute \
     && (!defined __clang_minor__ \
         || (defined __apple_build_version__ \
             ? 6000000 <= __apple_build_version__ \
             : 3 < __clang_major__ + (5 <= __clang_minor__))))
# define __glibc_has_attribute(attr) __has_attribute (attr)
#else
# define __glibc_has_attribute(attr) 0
#endif
#ifdef __has_builtin
# define __glibc_has_builtin(name) __has_builtin (name)
#else
# define __glibc_has_builtin(name) 0
#endif
#ifdef __has_extension
# define __glibc_has_extension(ext) __has_extension (ext)
#else
# define __glibc_has_extension(ext) 0
#endif

#if defined __GNUC__ || defined __clang__

 
# if __GNUC_PREREQ (4, 6) && !defined _LIBC
#  define __LEAF , __leaf__
#  define __LEAF_ATTR __attribute__ ((__leaf__))
# else
#  define __LEAF
#  define __LEAF_ATTR
# endif

 
# if !defined __cplusplus \
     && (__GNUC_PREREQ (3, 4) || __glibc_has_attribute (__nothrow__))
#  define __THROW	__attribute__ ((__nothrow__ __LEAF))
#  define __THROWNL	__attribute__ ((__nothrow__))
#  define __NTH(fct)	__attribute__ ((__nothrow__ __LEAF)) fct
#  define __NTHNL(fct)  __attribute__ ((__nothrow__)) fct
# else
#  if defined __cplusplus && (__GNUC_PREREQ (2,8) || __clang_major >= 4)
#   if __cplusplus >= 201103L
#    define __THROW	noexcept (true)
#   else
#    define __THROW	throw ()
#   endif
#   define __THROWNL	__THROW
#   define __NTH(fct)	__LEAF_ATTR fct __THROW
#   define __NTHNL(fct) fct __THROW
#  else
#   define __THROW
#   define __THROWNL
#   define __NTH(fct)	fct
#   define __NTHNL(fct) fct
#  endif
# endif

#else	 

# if (defined __cplusplus						\
      || (defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L))
#  define __inline	inline
# else
#  define __inline		 
# endif

# define __THROW
# define __THROWNL
# define __NTH(fct)	fct

#endif	 

 
#define __P(args)	args
#define __PMT(args)	args

 

#define __CONCAT(x,y)	x ## y
#define __STRING(x)	#x

 
#define __ptr_t void *


 
#ifdef	__cplusplus
# define __BEGIN_DECLS	extern "C" {
# define __END_DECLS	}
#else
# define __BEGIN_DECLS
# define __END_DECLS
#endif


 
#ifndef __GNULIB_CDEFS

 
# define __bos(ptr) __builtin_object_size (ptr, __USE_FORTIFY_LEVEL > 1)
# define __bos0(ptr) __builtin_object_size (ptr, 0)

 
# if __USE_FORTIFY_LEVEL == 3 && (__glibc_clang_prereq (9, 0)		      \
				  || __GNUC_PREREQ (12, 0))
#  define __glibc_objsize0(__o) __builtin_dynamic_object_size (__o, 0)
#  define __glibc_objsize(__o) __builtin_dynamic_object_size (__o, 1)
# else
#  define __glibc_objsize0(__o) __bos0 (__o)
#  define __glibc_objsize(__o) __bos (__o)
# endif

 

# define __glibc_safe_len_cond(__l, __s, __osz) ((__l) <= (__osz) / (__s))
# define __glibc_unsigned_or_positive(__l) \
  ((__typeof (__l)) 0 < (__typeof (__l)) -1				      \
   || (__builtin_constant_p (__l) && (__l) > 0))

 
# define __glibc_safe_or_unknown_len(__l, __s, __osz) \
  ((__osz) == (__SIZE_TYPE__) -1					      \
   || (__glibc_unsigned_or_positive (__l)				      \
       && __builtin_constant_p (__glibc_safe_len_cond ((__SIZE_TYPE__) (__l), \
						       (__s), (__osz)))	      \
       && __glibc_safe_len_cond ((__SIZE_TYPE__) (__l), (__s), (__osz))))

 
# define __glibc_unsafe_len(__l, __s, __osz) \
  (__glibc_unsigned_or_positive (__l)					      \
   && __builtin_constant_p (__glibc_safe_len_cond ((__SIZE_TYPE__) (__l),     \
						   __s, __osz))		      \
   && !__glibc_safe_len_cond ((__SIZE_TYPE__) (__l), __s, __osz))

 

# define __glibc_fortify(f, __l, __s, __osz, ...) \
  (__glibc_safe_or_unknown_len (__l, __s, __osz)			      \
   ? __ ## f ## _alias (__VA_ARGS__)					      \
   : (__glibc_unsafe_len (__l, __s, __osz)				      \
      ? __ ## f ## _chk_warn (__VA_ARGS__, __osz)			      \
      : __ ## f ## _chk (__VA_ARGS__, __osz)))			      \

 

# define __glibc_fortify_n(f, __l, __s, __osz, ...) \
  (__glibc_safe_or_unknown_len (__l, __s, __osz)			      \
   ? __ ## f ## _alias (__VA_ARGS__)					      \
   : (__glibc_unsafe_len (__l, __s, __osz)				      \
      ? __ ## f ## _chk_warn (__VA_ARGS__, (__osz) / (__s))		      \
      : __ ## f ## _chk (__VA_ARGS__, (__osz) / (__s))))		      \

#endif


#if __GNUC_PREREQ (4,3)
# define __warnattr(msg) __attribute__((__warning__ (msg)))
# define __errordecl(name, msg) \
  extern void name (void) __attribute__((__error__ (msg)))
#else
# define __warnattr(msg)
# define __errordecl(name, msg) extern void name (void)
#endif

 
#if defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L && !defined __HP_cc
# define __flexarr	[]
# define __glibc_c99_flexarr_available 1
#elif __GNUC_PREREQ (2,97) || defined __clang__
 
# define __flexarr	[]
# define __glibc_c99_flexarr_available 1
#elif defined __GNUC__
 
# define __flexarr	[0]
# define __glibc_c99_flexarr_available 1
#else
 
# define __flexarr	[1]
# define __glibc_c99_flexarr_available 0
#endif


 

#if (defined __GNUC__ && __GNUC__ >= 2) || (__clang_major__ >= 4)

# define __REDIRECT(name, proto, alias) name proto __asm__ (__ASMNAME (#alias))
# ifdef __cplusplus
#  define __REDIRECT_NTH(name, proto, alias) \
     name proto __THROW __asm__ (__ASMNAME (#alias))
#  define __REDIRECT_NTHNL(name, proto, alias) \
     name proto __THROWNL __asm__ (__ASMNAME (#alias))
# else
#  define __REDIRECT_NTH(name, proto, alias) \
     name proto __asm__ (__ASMNAME (#alias)) __THROW
#  define __REDIRECT_NTHNL(name, proto, alias) \
     name proto __asm__ (__ASMNAME (#alias)) __THROWNL
# endif
# define __ASMNAME(cname)  __ASMNAME2 (__USER_LABEL_PREFIX__, cname)
# define __ASMNAME2(prefix, cname) __STRING (prefix) cname

 
#endif

 
#if !(defined __GNUC__ || defined __clang__)
# define __attribute__(xyz)	 
#endif

 
#if __GNUC_PREREQ (2,96) || __glibc_has_attribute (__malloc__)
# define __attribute_malloc__ __attribute__ ((__malloc__))
#else
# define __attribute_malloc__  
#endif

 
#if __GNUC_PREREQ (4, 3)
# define __attribute_alloc_size__(params) \
  __attribute__ ((__alloc_size__ params))
#else
# define __attribute_alloc_size__(params)  
#endif

 
#if __GNUC_PREREQ (4, 9) || __glibc_has_attribute (__alloc_align__)
# define __attribute_alloc_align__(param) \
  __attribute__ ((__alloc_align__ param))
#else
# define __attribute_alloc_align__(param)  
#endif

 
#if __GNUC_PREREQ (2,96) || __glibc_has_attribute (__pure__)
# define __attribute_pure__ __attribute__ ((__pure__))
#else
# define __attribute_pure__  
#endif

 
#if __GNUC_PREREQ (2,5) || __glibc_has_attribute (__const__)
# define __attribute_const__ __attribute__ ((__const__))
#else
# define __attribute_const__  
#endif

#if __GNUC_PREREQ (2,7) || __glibc_has_attribute (__unused__)
# define __attribute_maybe_unused__ __attribute__ ((__unused__))
#else
# define __attribute_maybe_unused__  
#endif

 
#if __GNUC_PREREQ (3,1) || __glibc_has_attribute (__used__)
# define __attribute_used__ __attribute__ ((__used__))
# define __attribute_noinline__ __attribute__ ((__noinline__))
#else
# define __attribute_used__ __attribute__ ((__unused__))
# define __attribute_noinline__  
#endif

 
#if __GNUC_PREREQ (3,2) || __glibc_has_attribute (__deprecated__)
# define __attribute_deprecated__ __attribute__ ((__deprecated__))
#else
# define __attribute_deprecated__  
#endif

 
#if __GNUC_PREREQ (4,5) \
    || __glibc_has_extension (__attribute_deprecated_with_message__)
# define __attribute_deprecated_msg__(msg) \
	 __attribute__ ((__deprecated__ (msg)))
#else
# define __attribute_deprecated_msg__(msg) __attribute_deprecated__
#endif

 
#if __GNUC_PREREQ (2,8) || __glibc_has_attribute (__format_arg__)
# define __attribute_format_arg__(x) __attribute__ ((__format_arg__ (x)))
#else
# define __attribute_format_arg__(x)  
#endif

 
#if __GNUC_PREREQ (2,97) || __glibc_has_attribute (__format__)
# define __attribute_format_strfmon__(a,b) \
  __attribute__ ((__format__ (__strfmon__, a, b)))
#else
# define __attribute_format_strfmon__(a,b)  
#endif

 
#ifndef __attribute_nonnull__
# if __GNUC_PREREQ (3,3) || __glibc_has_attribute (__nonnull__)
#  define __attribute_nonnull__(params) __attribute__ ((__nonnull__ params))
# else
#  define __attribute_nonnull__(params)
# endif
#endif
#ifndef __nonnull
# define __nonnull(params) __attribute_nonnull__ (params)
#endif

 
#ifndef __returns_nonnull
# if __GNUC_PREREQ (4, 9) || __glibc_has_attribute (__returns_nonnull__)
# define __returns_nonnull __attribute__ ((__returns_nonnull__))
# else
# define __returns_nonnull
# endif
#endif

 
#if __GNUC_PREREQ (3,4) || __glibc_has_attribute (__warn_unused_result__)
# define __attribute_warn_unused_result__ \
   __attribute__ ((__warn_unused_result__))
# if defined __USE_FORTIFY_LEVEL && __USE_FORTIFY_LEVEL > 0
#  define __wur __attribute_warn_unused_result__
# endif
#else
# define __attribute_warn_unused_result__  
#endif
#ifndef __wur
# define __wur  
#endif

 
#if __GNUC_PREREQ (3,2) || __glibc_has_attribute (__always_inline__)
 
# undef __always_inline
# define __always_inline __inline __attribute__ ((__always_inline__))
#else
# undef __always_inline
# define __always_inline __inline
#endif

 
#if __GNUC_PREREQ (4,3) || __glibc_has_attribute (__artificial__)
# define __attribute_artificial__ __attribute__ ((__artificial__))
#else
# define __attribute_artificial__  
#endif

 
#if (!defined __cplusplus || __GNUC_PREREQ (4,3) \
     || (defined __clang__ && (defined __GNUC_STDC_INLINE__ \
			       || defined __GNUC_GNU_INLINE__)))
# if defined __GNUC_STDC_INLINE__ || defined __cplusplus
#  define __extern_inline extern __inline __attribute__ ((__gnu_inline__))
#  define __extern_always_inline \
  extern __always_inline __attribute__ ((__gnu_inline__))
# else
#  define __extern_inline extern __inline
#  define __extern_always_inline extern __always_inline
# endif
#endif

#ifdef __extern_always_inline
# define __fortify_function __extern_always_inline __attribute_artificial__
#endif

 
#if __GNUC_PREREQ (4,3)
# define __va_arg_pack() __builtin_va_arg_pack ()
# define __va_arg_pack_len() __builtin_va_arg_pack_len ()
#endif

 
#if !(__GNUC_PREREQ (2,8) || defined __clang__)
# define __extension__		 
#endif

 
#if !(__GNUC_PREREQ (2,92) || __clang_major__ >= 3)
# if defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L
#  define __restrict	restrict
# else
#  define __restrict	 
# endif
#endif

 
#if (__GNUC_PREREQ (3,1) || __clang_major__ >= 3) && !defined __cplusplus
# define __restrict_arr	__restrict
#else
# ifdef __GNUC__
#  define __restrict_arr	 
# else
#  if defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L
#   define __restrict_arr	restrict
#  else
 
#   define __restrict_arr	 
#  endif
# endif
#endif

#if (__GNUC__ >= 3) || __glibc_has_builtin (__builtin_expect)
# define __glibc_unlikely(cond)	__builtin_expect ((cond), 0)
# define __glibc_likely(cond)	__builtin_expect ((cond), 1)
#else
# define __glibc_unlikely(cond)	(cond)
# define __glibc_likely(cond)	(cond)
#endif

#if (!defined _Noreturn \
     && (defined __STDC_VERSION__ ? __STDC_VERSION__ : 0) < 201112 \
     &&  !(__GNUC_PREREQ (4,7) \
           || (3 < __clang_major__ + (5 <= __clang_minor__))))
# if __GNUC_PREREQ (2,8)
#  define _Noreturn __attribute__ ((__noreturn__))
# else
#  define _Noreturn
# endif
#endif

#if __GNUC_PREREQ (8, 0)
 
# define __attribute_nonstring__ __attribute__ ((__nonstring__))
#else
# define __attribute_nonstring__
#endif

 
#undef __attribute_copy__
#if __GNUC_PREREQ (9, 0)
 
# define __attribute_copy__(arg) __attribute__ ((__copy__ (arg)))
#else
# define __attribute_copy__(arg)
#endif

#if (!defined _Static_assert && !defined __cplusplus \
     && (defined __STDC_VERSION__ ? __STDC_VERSION__ : 0) < 201112 \
     && (!(__GNUC_PREREQ (4, 6) || __clang_major__ >= 4) \
         || defined __STRICT_ANSI__))
# define _Static_assert(expr, diagnostic) \
    extern int (*__Static_assert_function (void)) \
      [!!sizeof (struct { int __error_if_negative: (expr) ? 2 : -1; })]
#endif

 
#ifndef __GNULIB_CDEFS
# include <bits/wordsize.h>
# include <bits/long-double.h>
#endif

#if __LDOUBLE_REDIRECTS_TO_FLOAT128_ABI == 1
# ifdef __REDIRECT

 
#  define __LDBL_REDIR(name, proto) ... unused__ldbl_redir
#  define __LDBL_REDIR_DECL(name) \
  extern __typeof (name) name __asm (__ASMNAME ("__" #name "ieee128"));

 
#  define __LDBL_REDIR2_DECL(name) \
  extern __typeof (__##name) __##name \
    __asm (__ASMNAME ("__" #name "ieee128"));

 
#  define __LDBL_REDIR1(name, proto, alias) ... unused__ldbl_redir1
#  define __LDBL_REDIR1_DECL(name, alias) \
  extern __typeof (name) name __asm (__ASMNAME (#alias));

#  define __LDBL_REDIR1_NTH(name, proto, alias) \
  __REDIRECT_NTH (name, proto, alias)
#  define __REDIRECT_NTH_LDBL(name, proto, alias) \
  __LDBL_REDIR1_NTH (name, proto, __##alias##ieee128)

 
#  define __REDIRECT_LDBL(name, proto, alias) ... unused__redirect_ldbl
#  define __LDBL_REDIR_NTH(name, proto) ... unused__ldbl_redir_nth

# else
_Static_assert (0, "IEEE 128-bits long double requires redirection on this platform");
# endif
#elif defined __LONG_DOUBLE_MATH_OPTIONAL && defined __NO_LONG_DOUBLE_MATH
# define __LDBL_COMPAT 1
# ifdef __REDIRECT
#  define __LDBL_REDIR1(name, proto, alias) __REDIRECT (name, proto, alias)
#  define __LDBL_REDIR(name, proto) \
  __LDBL_REDIR1 (name, proto, __nldbl_##name)
#  define __LDBL_REDIR1_NTH(name, proto, alias) __REDIRECT_NTH (name, proto, alias)
#  define __LDBL_REDIR_NTH(name, proto) \
  __LDBL_REDIR1_NTH (name, proto, __nldbl_##name)
#  define __LDBL_REDIR2_DECL(name) \
  extern __typeof (__##name) __##name __asm (__ASMNAME ("__nldbl___" #name));
#  define __LDBL_REDIR1_DECL(name, alias) \
  extern __typeof (name) name __asm (__ASMNAME (#alias));
#  define __LDBL_REDIR_DECL(name) \
  extern __typeof (name) name __asm (__ASMNAME ("__nldbl_" #name));
#  define __REDIRECT_LDBL(name, proto, alias) \
  __LDBL_REDIR1 (name, proto, __nldbl_##alias)
#  define __REDIRECT_NTH_LDBL(name, proto, alias) \
  __LDBL_REDIR1_NTH (name, proto, __nldbl_##alias)
# endif
#endif
#if (!defined __LDBL_COMPAT && __LDOUBLE_REDIRECTS_TO_FLOAT128_ABI == 0) \
    || !defined __REDIRECT
# define __LDBL_REDIR1(name, proto, alias) name proto
# define __LDBL_REDIR(name, proto) name proto
# define __LDBL_REDIR1_NTH(name, proto, alias) name proto __THROW
# define __LDBL_REDIR_NTH(name, proto) name proto __THROW
# define __LDBL_REDIR2_DECL(name)
# define __LDBL_REDIR_DECL(name)
# ifdef __REDIRECT
#  define __REDIRECT_LDBL(name, proto, alias) __REDIRECT (name, proto, alias)
#  define __REDIRECT_NTH_LDBL(name, proto, alias) \
  __REDIRECT_NTH (name, proto, alias)
# endif
#endif

 
#if __GNUC_PREREQ (4,8) || __glibc_clang_prereq (3,5)
# define __glibc_macro_warning1(message) _Pragma (#message)
# define __glibc_macro_warning(message) \
  __glibc_macro_warning1 (GCC warning message)
#else
# define __glibc_macro_warning(msg)
#endif

 
#if !defined __cplusplus \
    && (__GNUC_PREREQ (4, 9) \
	|| __glibc_has_extension (c_generic_selections) \
	|| (!defined __GNUC__ && defined __STDC_VERSION__ \
	    && __STDC_VERSION__ >= 201112L))
# define __HAVE_GENERIC_SELECTION 1
#else
# define __HAVE_GENERIC_SELECTION 0
#endif

#if __GNUC_PREREQ (10, 0)
 
#  define __attr_access(x) __attribute__ ((__access__ x))
 
#  if __USE_FORTIFY_LEVEL == 3
#    define __fortified_attr_access(a, o, s) __attribute__ ((__access__ (a, o)))
#  else
#    define __fortified_attr_access(a, o, s) __attr_access ((a, o, s))
#  endif
#  if __GNUC_PREREQ (11, 0)
#    define __attr_access_none(argno) __attribute__ ((__access__ (__none__, argno)))
#  else
#    define __attr_access_none(argno)
#  endif
#else
#  define __fortified_attr_access(a, o, s)
#  define __attr_access(x)
#  define __attr_access_none(argno)
#endif

#if __GNUC_PREREQ (11, 0)
 
# define __attr_dealloc(dealloc, argno) \
    __attribute__ ((__malloc__ (dealloc, argno)))
# define __attr_dealloc_free __attr_dealloc (__builtin_free, 1)
#else
# define __attr_dealloc(dealloc, argno)
# define __attr_dealloc_free
#endif

 
#if __GNUC_PREREQ (4, 1)
# define __attribute_returns_twice__ __attribute__ ((__returns_twice__))
#else
# define __attribute_returns_twice__  
#endif

#endif	  
