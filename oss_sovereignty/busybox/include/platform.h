#ifndef BB_PLATFORM_H
#define BB_PLATFORM_H 1
#undef __GNUC_PREREQ
#if defined __GNUC__ && defined __GNUC_MINOR__
# define __GNUC_PREREQ(maj, min) \
		((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
# define __GNUC_PREREQ(maj, min) 0
#endif
#if !__GNUC_PREREQ(2,92)
# ifndef __restrict
#  define __restrict
# endif
#endif
#if !__GNUC_PREREQ(2,7)
# ifndef __attribute__
#  define __attribute__(x)
# endif
#endif
#if !__GNUC_PREREQ(5,0)
# define deprecated(msg) deprecated
#endif
#undef inline
#if defined(__STDC_VERSION__) && __STDC_VERSION__ > 199901L
#elif __GNUC_PREREQ(2,7)
# define inline __inline__
#else
# define inline
#endif
#ifndef __const
# define __const const
#endif
#define UNUSED_PARAM __attribute__ ((__unused__))
#define NORETURN __attribute__ ((__noreturn__))
#if __GNUC_PREREQ(4,5)
# define bb_unreachable(altcode) __builtin_unreachable()
#else
# define bb_unreachable(altcode) altcode
#endif
#define RETURNS_MALLOC __attribute__ ((malloc))
#define PACKED __attribute__ ((__packed__))
#define ALIGNED(m) __attribute__ ((__aligned__(m)))
#if __GNUC_PREREQ(3,0) && !defined(__NO_INLINE__)
# define ALWAYS_INLINE __attribute__ ((always_inline)) inline
# define NOINLINE      __attribute__((__noinline__))
# if !ENABLE_WERROR
#  define DEPRECATED __attribute__ ((__deprecated__))
#  define UNUSED_PARAM_RESULT __attribute__ ((warn_unused_result))
# else
#  define DEPRECATED
#  define UNUSED_PARAM_RESULT
# endif
#else
# define ALWAYS_INLINE inline
# define NOINLINE
# define DEPRECATED
# define UNUSED_PARAM_RESULT
#endif
#define INIT_FUNC __attribute__ ((constructor))
#if __GNUC_PREREQ(4,1)
# define EXTERNALLY_VISIBLE __attribute__(( visibility("default") ))
#else
# define EXTERNALLY_VISIBLE
#endif
#if __GNUC_PREREQ(4,4)
# define FIX_ALIASING __attribute__((__may_alias__))
#else
# define FIX_ALIASING
#endif
#if !__GNUC_PREREQ(2,8)
# ifndef __extension__
#  define __extension__
# endif
#endif
#ifndef FAST_FUNC
# if __GNUC_PREREQ(3,0) && defined(i386)
#  define FAST_FUNC __attribute__((regparm(3),stdcall))
# else
#  define FAST_FUNC
# endif
#endif
#if __GNUC_PREREQ(4,1) && !defined(__CYGWIN__)
# define PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN _Pragma("GCC visibility push(hidden)")
# define POP_SAVED_FUNCTION_VISIBILITY              _Pragma("GCC visibility pop")
#else
# define PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN
# define POP_SAVED_FUNCTION_VISIBILITY
#endif
#if !__GNUC_PREREQ(3,0)
# include <stdarg.h>
# if !defined va_copy && defined __va_copy
#  define va_copy(d,s) __va_copy((d),(s))
# endif
#endif
#include <limits.h>
#if defined(__digital__) && defined(__unix__)
# include <sex.h>
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) \
   || defined(__APPLE__)
# include <sys/resource.h>   
# include <machine/endian.h>
# define bswap_64 __bswap64
# define bswap_32 __bswap32
# define bswap_16 __bswap16
#else
# include <byteswap.h>
# include <endian.h>
#endif
#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN
# define BB_BIG_ENDIAN 1
# define BB_LITTLE_ENDIAN 0
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN
# define BB_BIG_ENDIAN 0
# define BB_LITTLE_ENDIAN 1
#elif defined(_BYTE_ORDER) && _BYTE_ORDER == _BIG_ENDIAN
# define BB_BIG_ENDIAN 1
# define BB_LITTLE_ENDIAN 0
#elif defined(_BYTE_ORDER) && _BYTE_ORDER == _LITTLE_ENDIAN
# define BB_BIG_ENDIAN 0
# define BB_LITTLE_ENDIAN 1
#elif defined(BYTE_ORDER) && BYTE_ORDER == BIG_ENDIAN
# define BB_BIG_ENDIAN 1
# define BB_LITTLE_ENDIAN 0
#elif defined(BYTE_ORDER) && BYTE_ORDER == LITTLE_ENDIAN
# define BB_BIG_ENDIAN 0
# define BB_LITTLE_ENDIAN 1
#elif defined(__386__)
# define BB_BIG_ENDIAN 0
# define BB_LITTLE_ENDIAN 1
#else
# error "Can't determine endianness"
#endif
#if ULONG_MAX > 0xffffffff
# define bb_bswap_64(x) bswap_64(x)
#endif
#if BB_BIG_ENDIAN
# define SWAP_BE16(x) (x)
# define SWAP_BE32(x) (x)
# define SWAP_BE64(x) (x)
# define SWAP_LE16(x) bswap_16(x)
# define SWAP_LE32(x) bswap_32(x)
# define SWAP_LE64(x) bb_bswap_64(x)
# define IF_BIG_ENDIAN(...) __VA_ARGS__
# define IF_LITTLE_ENDIAN(...)
#else
# define SWAP_BE16(x) bswap_16(x)
# define SWAP_BE32(x) bswap_32(x)
# define SWAP_BE64(x) bb_bswap_64(x)
# define SWAP_LE16(x) (x)
# define SWAP_LE32(x) (x)
# define SWAP_LE64(x) (x)
# define IF_BIG_ENDIAN(...)
# define IF_LITTLE_ENDIAN(...) __VA_ARGS__
#endif
#include <stdint.h>
typedef int      bb__aliased_int      FIX_ALIASING;
typedef long     bb__aliased_long     FIX_ALIASING;
typedef uint16_t bb__aliased_uint16_t FIX_ALIASING;
typedef uint32_t bb__aliased_uint32_t FIX_ALIASING;
typedef uint64_t bb__aliased_uint64_t FIX_ALIASING;
#if defined(i386) || defined(__x86_64__) || defined(__powerpc__)
# define BB_UNALIGNED_MEMACCESS_OK 1
# define move_from_unaligned_int(v, intp)  ((v) = *(bb__aliased_int*)(intp))
# define move_from_unaligned_long(v, longp) ((v) = *(bb__aliased_long*)(longp))
# define move_from_unaligned16(v, u16p) ((v) = *(bb__aliased_uint16_t*)(u16p))
# define move_from_unaligned32(v, u32p) ((v) = *(bb__aliased_uint32_t*)(u32p))
# define move_from_unaligned64(v, u64p) ((v) = *(bb__aliased_uint64_t*)(u64p))
# define move_to_unaligned16(u16p, v)   (*(bb__aliased_uint16_t*)(u16p) = (v))
# define move_to_unaligned32(u32p, v)   (*(bb__aliased_uint32_t*)(u32p) = (v))
# define move_to_unaligned64(u64p, v)   (*(bb__aliased_uint64_t*)(u64p) = (v))
#else
# define BB_UNALIGNED_MEMACCESS_OK 0
# define move_from_unaligned_int(v, intp) (memcpy(&(v), (intp), sizeof(int)))
# define move_from_unaligned_long(v, longp) (memcpy(&(v), (longp), sizeof(long)))
# define move_from_unaligned16(v, u16p) (memcpy(&(v), (u16p), 2))
# define move_from_unaligned32(v, u32p) (memcpy(&(v), (u32p), 4))
# define move_from_unaligned64(v, u64p) (memcpy(&(v), (u64p), 8))
# define move_to_unaligned16(u16p, v) do { \
	uint16_t __t = (v); \
	memcpy((u16p), &__t, 2); \
} while (0)
# define move_to_unaligned32(u32p, v) do { \
	uint32_t __t = (v); \
	memcpy((u32p), &__t, 4); \
} while (0)
# define move_to_unaligned64(u64p, v) do { \
	uint64_t __t = (v); \
	memcpy((u64p), &__t, 8); \
} while (0)
#endif
#define get_unaligned_le32(buf) ({ uint32_t v; move_from_unaligned32(v, buf); SWAP_LE32(v); })
#define get_unaligned_be32(buf) ({ uint32_t v; move_from_unaligned32(v, buf); SWAP_BE32(v); })
#define put_unaligned_le32(val, buf) move_to_unaligned32(buf, SWAP_LE32(val))
#define put_unaligned_be32(val, buf) move_to_unaligned32(buf, SWAP_BE32(val))
#define get_le32(u32p) ({ uint32_t v = *(bb__aliased_uint32_t*)(u32p); SWAP_LE32(v); })
#if defined(i386) || defined(__x86_64__) || defined(__mips__) || defined(__cris__)
typedef signed char smallint;
typedef unsigned char smalluint;
#else
typedef int smallint;
typedef unsigned smalluint;
#endif
#if (defined __digital__ && defined __unix__)
# define bool smalluint
#else
# include <stdbool.h>
#endif
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#ifdef __UCLIBC__
# define UCLIBC_VERSION KERNEL_VERSION(__UCLIBC_MAJOR__, __UCLIBC_MINOR__, __UCLIBC_SUBLEVEL__)
#else
# define UCLIBC_VERSION 0
#endif
#if defined __GLIBC__ \
 || defined __UCLIBC__ \
 || defined __dietlibc__ \
 || defined __BIONIC__ \
 || defined _NEWLIB_VERSION
# include <features.h>
#endif
#if (defined(__digital__) && defined(__unix__)) || defined(__FreeBSD__)
# define bb_setpgrp() do { pid_t __me = getpid(); setpgrp(__me, __me); } while (0)
#else
# define bb_setpgrp() setpgrp()
#endif
#include <unistd.h>
#define fdprintf dprintf
#if !defined(__s390__)
# define ALIGN1 __attribute__((aligned(1)))
# define ALIGN2 __attribute__((aligned(2)))
# define ALIGN4 __attribute__((aligned(4)))
#else
# define ALIGN1
# define ALIGN2
# define ALIGN4
#endif
#define ALIGN8     __attribute__((aligned(8)))
#define ALIGN_INT  __attribute__((aligned(sizeof(int))))
#define ALIGN_PTR  __attribute__((aligned(sizeof(void*))))
#if ENABLE_NOMMU || \
    (defined __UCLIBC__ && \
     UCLIBC_VERSION > KERNEL_VERSION(0, 9, 28) && \
     !defined __ARCH_USE_MMU__)
# define BB_MMU 0
# define USE_FOR_NOMMU(...) __VA_ARGS__
# define USE_FOR_MMU(...)
#else
# define BB_MMU 1
# define USE_FOR_NOMMU(...)
# define USE_FOR_MMU(...) __VA_ARGS__
#endif
#if defined(__digital__) && defined(__unix__)
# include <standards.h>
# include <inttypes.h>
# define PRIu32 "u"
# if !defined ADJ_OFFSET_SINGLESHOT && defined MOD_CLKA && defined MOD_OFFSET
#  define ADJ_OFFSET_SINGLESHOT (MOD_CLKA | MOD_OFFSET)
# endif
# if !defined ADJ_FREQUENCY && defined MOD_FREQUENCY
#  define ADJ_FREQUENCY MOD_FREQUENCY
# endif
# if !defined ADJ_TIMECONST && defined MOD_TIMECONST
#  define ADJ_TIMECONST MOD_TIMECONST
# endif
# if !defined ADJ_TICK && defined MOD_CLKB
#  define ADJ_TICK MOD_CLKB
# endif
#endif
#if defined(__CYGWIN__)
# define MAXSYMLINKS SYMLOOP_MAX
#endif
#if defined(ANDROID) || defined(__ANDROID__)
# define BB_ADDITIONAL_PATH ":/system/sbin:/system/bin:/system/xbin"
# define SYS_ioprio_set __NR_ioprio_set
# define SYS_ioprio_get __NR_ioprio_get
#endif
#define HAVE_CLEARENV 1
#define HAVE_FDATASYNC 1
#define HAVE_DPRINTF 1
#define HAVE_MEMRCHR 1
#define HAVE_MKDTEMP 1
#define HAVE_TTYNAME_R 1
#define HAVE_PTSNAME_R 1
#define HAVE_SETBIT 1
#define HAVE_SIGHANDLER_T 1
#define HAVE_STPCPY 1
#define HAVE_STPNCPY 1
#define HAVE_MEMPCPY 1
#define HAVE_STRCASESTR 1
#define HAVE_STRCHRNUL 1
#define HAVE_STRSEP 1
#define HAVE_STRSIGNAL 1
#define HAVE_STRVERSCMP 1
#define HAVE_VASPRINTF 1
#define HAVE_USLEEP 1
#define HAVE_UNLOCKED_STDIO 1
#define HAVE_UNLOCKED_LINE_OPS 1
#define HAVE_GETLINE 1
#define HAVE_XTABS 1
#define HAVE_MNTENT_H 1
#define HAVE_NET_ETHERNET_H 1
#define HAVE_SYS_STATFS_H 1
#define HAVE_PRINTF_PERCENTM 1
#define HAVE_WAIT3 1
#define HAVE_DEV_FD 1
#define DEV_FD_PREFIX "/dev/fd/"
#if defined(__UCLIBC__)
# if UCLIBC_VERSION < KERNEL_VERSION(0, 9, 32)
#  undef HAVE_STRVERSCMP
# endif
# if UCLIBC_VERSION >= KERNEL_VERSION(0, 9, 30)
#  ifndef __UCLIBC_SUSV3_LEGACY__
#   undef HAVE_USLEEP
#  endif
# endif
#endif
#if defined(__WATCOMC__)
# undef HAVE_DPRINTF
# undef HAVE_GETLINE
# undef HAVE_MEMRCHR
# undef HAVE_MKDTEMP
# undef HAVE_SETBIT
# undef HAVE_STPCPY
# undef HAVE_STPNCPY
# undef HAVE_STRCASESTR
# undef HAVE_STRCHRNUL
# undef HAVE_STRSEP
# undef HAVE_STRSIGNAL
# undef HAVE_STRVERSCMP
# undef HAVE_VASPRINTF
# undef HAVE_UNLOCKED_STDIO
# undef HAVE_UNLOCKED_LINE_OPS
# undef HAVE_NET_ETHERNET_H
#endif
#if defined(__CYGWIN__)
# undef HAVE_CLEARENV
# undef HAVE_FDPRINTF
# undef HAVE_MEMRCHR
# undef HAVE_PTSNAME_R
# undef HAVE_STRVERSCMP
# undef HAVE_UNLOCKED_LINE_OPS
#endif
#if (defined __digital__ && defined __unix__) \
 || defined __APPLE__ \
 || defined __OpenBSD__ || defined __NetBSD__
# undef HAVE_CLEARENV
# undef HAVE_FDATASYNC
# undef HAVE_GETLINE
# undef HAVE_MNTENT_H
# undef HAVE_PTSNAME_R
# undef HAVE_SYS_STATFS_H
# undef HAVE_SIGHANDLER_T
# undef HAVE_STRVERSCMP
# undef HAVE_XTABS
# undef HAVE_DPRINTF
# undef HAVE_UNLOCKED_STDIO
# undef HAVE_UNLOCKED_LINE_OPS
# undef HAVE_PRINTF_PERCENTM
#endif
#if defined(__dietlibc__)
# undef HAVE_STRCHRNUL
#endif
#if defined(__APPLE__)
# undef HAVE_STRCHRNUL
#endif
#if defined(__FreeBSD__)
# undef HAVE_MEMPCPY
# undef HAVE_CLEARENV
# undef HAVE_FDATASYNC
# undef HAVE_MNTENT_H
# undef HAVE_PTSNAME_R
# undef HAVE_SYS_STATFS_H
# undef HAVE_SIGHANDLER_T
# undef HAVE_STRVERSCMP
# undef HAVE_XTABS
# undef HAVE_UNLOCKED_LINE_OPS
# undef HAVE_PRINTF_PERCENTM
# include <osreldate.h>
# if __FreeBSD_version < 1000029
#  undef HAVE_STRCHRNUL  
# endif
#endif
#if defined(__NetBSD__)
# define HAVE_GETLINE 1   
#endif
#if defined(__digital__) && defined(__unix__)
# undef HAVE_STPCPY
# undef HAVE_STPNCPY
#endif
#if defined(ANDROID) || defined(__ANDROID__)
# if __ANDROID_API__ < 8
#  undef HAVE_DPRINTF
# elif __ANDROID_API__ < 21
#  define dprintf fdprintf
# else
# endif
# if __ANDROID_API__ < 21
#  undef HAVE_TTYNAME_R
#  undef HAVE_GETLINE
#  undef HAVE_STPCPY
#  undef HAVE_STPNCPY
# endif
# if __ANDROID_API__ >= 21
#  undef HAVE_WAIT3
# endif
# undef HAVE_MEMPCPY
# undef HAVE_STRCHRNUL
# undef HAVE_STRVERSCMP
# undef HAVE_UNLOCKED_LINE_OPS
# undef HAVE_NET_ETHERNET_H
# undef HAVE_PRINTF_PERCENTM
#endif
#ifndef HAVE_DPRINTF
extern int dprintf(int fd, const char *format, ...);
#endif
#ifndef HAVE_MEMRCHR
extern void *memrchr(const void *s, int c, size_t n) FAST_FUNC;
#endif
#ifndef HAVE_MKDTEMP
extern char *mkdtemp(char *template) FAST_FUNC;
#endif
#ifndef HAVE_TTYNAME_R
#define ttyname_r bb_ttyname_r
extern int ttyname_r(int fd, char *buf, size_t buflen);
#endif
#ifndef HAVE_SETBIT
# define setbit(a, b)  ((a)[(b) >> 3] |= 1 << ((b) & 7))
# define clrbit(a, b)  ((a)[(b) >> 3] &= ~(1 << ((b) & 7)))
#endif
#ifndef HAVE_SIGHANDLER_T
typedef void (*sighandler_t)(int);
#endif
#ifndef HAVE_STPCPY
extern char *stpcpy(char *p, const char *to_add) FAST_FUNC;
#endif
#ifndef HAVE_STPNCPY
extern char *stpncpy(char *p, const char *to_add, size_t n) FAST_FUNC;
#endif
#ifndef HAVE_MEMPCPY
#include <string.h>
#define mempcpy bb__mempcpy
static ALWAYS_INLINE void *mempcpy(void *dest, const void *src, size_t len)
{
	return memcpy(dest, src, len) + len;
}
#endif
#ifndef HAVE_STRCASESTR
extern char *strcasestr(const char *s, const char *pattern) FAST_FUNC;
#endif
#ifndef HAVE_STRCHRNUL
extern char *strchrnul(const char *s, int c) FAST_FUNC;
#endif
#ifndef HAVE_STRSEP
extern char *strsep(char **stringp, const char *delim) FAST_FUNC;
#endif
#ifndef HAVE_STRSIGNAL
# define strsignal(sig) get_signame(sig)
#endif
#ifndef HAVE_USLEEP
extern int usleep(unsigned) FAST_FUNC;
#endif
#ifndef HAVE_VASPRINTF
extern int vasprintf(char **string_ptr, const char *format, va_list p) FAST_FUNC;
#endif
#ifndef HAVE_GETLINE
# include <stdio.h>  
# include <sys/types.h>  
extern ssize_t getline(char **lineptr, size_t *n, FILE *stream) FAST_FUNC;
#endif
#endif
