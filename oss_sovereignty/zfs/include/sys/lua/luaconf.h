#ifndef lconfig_h
#define lconfig_h
#include <sys/zfs_context.h>
extern ssize_t lcompat_sprintf(char *, size_t size, const char *, ...);
extern int64_t lcompat_strtoll(const char *, char **);
extern int64_t lcompat_pow(int64_t, int64_t);
extern int lcompat_hashnum(int64_t);
#if !defined(LUA_ANSI) && defined(__STRICT_ANSI__)
#define LUA_ANSI
#endif
#if !defined(LUA_ANSI) && defined(_WIN32) && !defined(_WIN32_WCE)
#define LUA_WIN		 
#endif
#if defined(LUA_WIN)
#define LUA_DL_DLL
#define LUA_USE_AFORMAT		 
#endif
#if defined(LUA_USE_LINUX)
#define LUA_USE_POSIX
#define LUA_USE_DLOPEN		 
#define LUA_USE_READLINE	 
#define LUA_USE_STRTODHEX	 
#define LUA_USE_AFORMAT		 
#define LUA_USE_LONGLONG	 
#endif
#if defined(LUA_USE_MACOSX)
#define LUA_USE_POSIX
#define LUA_USE_DLOPEN		 
#define LUA_USE_READLINE	 
#define LUA_USE_STRTODHEX	 
#define LUA_USE_AFORMAT		 
#define LUA_USE_LONGLONG	 
#endif
#if defined(LUA_USE_POSIX)
#define LUA_USE_MKSTEMP
#define LUA_USE_ISATTY
#define LUA_USE_POPEN
#define LUA_USE_ULONGJMP
#define LUA_USE_GMTIME_R
#endif
#if defined(_WIN32)	 
#define LUA_LDIR	"!\\lua\\"
#define LUA_CDIR	"!\\"
#define LUA_PATH_DEFAULT  \
		LUA_LDIR"?.lua;"  LUA_LDIR"?\\init.lua;" \
		LUA_CDIR"?.lua;"  LUA_CDIR"?\\init.lua;" ".\\?.lua"
#define LUA_CPATH_DEFAULT \
		LUA_CDIR"?.dll;" LUA_CDIR"loadall.dll;" ".\\?.dll"
#else			/* }{ */
#define LUA_VDIR	LUA_VERSION_MAJOR "." LUA_VERSION_MINOR "/"
#define LUA_ROOT	"/usr/local/"
#define LUA_LDIR	LUA_ROOT "share/lua/" LUA_VDIR
#define LUA_CDIR	LUA_ROOT "lib/lua/" LUA_VDIR
#define LUA_PATH_DEFAULT  \
		LUA_LDIR"?.lua;"  LUA_LDIR"?/init.lua;" \
		LUA_CDIR"?.lua;"  LUA_CDIR"?/init.lua;" "./?.lua"
#define LUA_CPATH_DEFAULT \
		LUA_CDIR"?.so;" LUA_CDIR"loadall.so;" "./?.so"
#endif			/* } */
/*
@@ LUA_DIRSEP is the directory separator (for submodules).
** CHANGE it if your machine does not use "/" as the directory separator
** and is not Windows. (On Windows Lua automatically uses "\".)
*/
#if defined(_WIN32)
#define LUA_DIRSEP	"\\"
#else
#define LUA_DIRSEP	"/"
#endif
/*
@@ LUA_ENV is the name of the variable that holds the current
@@ environment, used to access global names.
** CHANGE it if you do not like this name.
*/
#define LUA_ENV		"_ENV"
/*
@@ LUA_API is a mark for all core API functions.
@@ LUALIB_API is a mark for all auxiliary library functions.
@@ LUAMOD_API is a mark for all standard library opening functions.
** CHANGE them if you need to define those functions in some special way.
** For instance, if you want to create one Windows DLL with the core and
** the libraries, you may want to use the following definition (define
** LUA_BUILD_AS_DLL to get it).
*/
#if defined(LUA_BUILD_AS_DLL)	/* { */
#if defined(LUA_CORE) || defined(LUA_LIB)	/* { */
#define LUA_API __declspec(dllexport)
#else						/* }{ */
#define LUA_API __declspec(dllimport)
#endif						/* } */
#else				/* }{ */
#define LUA_API		extern
#endif				/* } */
/* more often than not the libs go together with the core */
#define LUALIB_API	LUA_API
#define LUAMOD_API	LUALIB_API
/*
@@ LUAI_FUNC is a mark for all extern functions that are not to be
@* exported to outside modules.
@@ LUAI_DDEF and LUAI_DDEC are marks for all extern (const) variables
@* that are not to be exported to outside modules (LUAI_DDEF for
@* definitions and LUAI_DDEC for declarations).
** CHANGE them if you need to mark them in some special way. Elf/gcc
** (versions 3.2 and later) mark them as "hidden" to optimize access
** when Lua is compiled as a shared library. Not all elf targets support
** this attribute. Unfortunately, gcc does not offer a way to check
** whether the target offers that support, and those without support
** give a warning about it. To avoid these warnings, change to the
** default definition.
*/
#if defined(__GNUC__) && ((__GNUC__*100 + __GNUC_MINOR__) >= 302) && \
    defined(__ELF__)		/* { */
#define LUAI_FUNC	__attribute__((visibility("hidden"))) extern
#define LUAI_DDEC	LUAI_FUNC
#define LUAI_DDEF	/* empty */
#else				/* }{ */
#define LUAI_FUNC	extern
#define LUAI_DDEC	extern
#define LUAI_DDEF	/* empty */
#endif				/* } */
/*
@@ LUA_QL describes how error messages quote program elements.
** CHANGE it if you want a different appearance.
*/
#define LUA_QL(x)	"'" x "'"
#define LUA_QS		LUA_QL("%s")
/*
@@ LUA_IDSIZE gives the maximum size for the description of the source
@* of a function in debug information.
** CHANGE it if you want a different size.
*/
#define LUA_IDSIZE	60
/*
@@ luai_writestringerror defines how to print error messages.
** (A format string with one argument is enough for Lua...)
*/
#ifdef _KERNEL
#define luai_writestringerror(s,p) \
	(zfs_dbgmsg((s), (p)))
#else
#define luai_writestringerror(s,p) \
	(fprintf(stderr, (s), (p)), fflush(stderr))
#endif
/*
@@ LUAI_MAXSHORTLEN is the maximum length for short strings, that is,
** strings that are internalized. (Cannot be smaller than reserved words
** or tags for metamethods, as these strings must be internalized;
** #("function") = 8, #("__newindex") = 10.)
*/
#define LUAI_MAXSHORTLEN        40
/*
** {==================================================================
** Compatibility with previous versions
** ===================================================================
*/
/*
@@ LUA_COMPAT_ALL controls all compatibility options.
** You can define it to get all options, or change specific options
** to fit your specific needs.
*/
#if defined(LUA_COMPAT_ALL)	/* { */
/*
@@ LUA_COMPAT_UNPACK controls the presence of global 'unpack'.
** You can replace it with 'table.unpack'.
*/
#define LUA_COMPAT_UNPACK
/*
@@ LUA_COMPAT_LOADERS controls the presence of table 'package.loaders'.
** You can replace it with 'package.searchers'.
*/
#define LUA_COMPAT_LOADERS
/*
@@ macro 'lua_cpcall' emulates deprecated function lua_cpcall.
** You can call your C function directly (with light C functions).
*/
#define lua_cpcall(L,f,u)  \
	(lua_pushcfunction(L, (f)), \
	 lua_pushlightuserdata(L,(u)), \
	 lua_pcall(L,1,0,0))
/*
@@ LUA_COMPAT_LOG10 defines the function 'log10' in the math library.
** You can rewrite 'log10(x)' as 'log(x, 10)'.
*/
#define LUA_COMPAT_LOG10
/*
@@ LUA_COMPAT_LOADSTRING defines the function 'loadstring' in the base
** library. You can rewrite 'loadstring(s)' as 'load(s)'.
*/
#define LUA_COMPAT_LOADSTRING
/*
@@ LUA_COMPAT_MAXN defines the function 'maxn' in the table library.
*/
#define LUA_COMPAT_MAXN
/*
@@ The following macros supply trivial compatibility for some
** changes in the API. The macros themselves document how to
** change your code to avoid using them.
*/
#define lua_strlen(L,i)		lua_rawlen(L, (i))
#define lua_objlen(L,i)		lua_rawlen(L, (i))
#define lua_equal(L,idx1,idx2)		lua_compare(L,(idx1),(idx2),LUA_OPEQ)
#define lua_lessthan(L,idx1,idx2)	lua_compare(L,(idx1),(idx2),LUA_OPLT)
/*
@@ LUA_COMPAT_MODULE controls compatibility with previous
** module functions 'module' (Lua) and 'luaL_register' (C).
*/
#define LUA_COMPAT_MODULE
#endif				/* } */
/* }================================================================== */
#undef INT_MAX
#define	INT_MAX	2147483647L
/*
@@ LUAI_BITSINT defines the number of bits in an int.
** CHANGE here if Lua cannot automatically detect the number of bits of
** your machine. Probably you do not need to change this.
*/
/* avoid overflows in comparison */
#if INT_MAX-20 < 32760		/* { */
#define LUAI_BITSINT	16
#elif INT_MAX > 2147483640L	/* }{ */
/* int has at least 32 bits */
#define LUAI_BITSINT	32
#else				/* }{ */
#error "you must define LUA_BITSINT with number of bits in an integer"
#endif				/* } */
/*
@@ LUA_INT32 is a signed integer with exactly 32 bits.
@@ LUAI_UMEM is an unsigned integer big enough to count the total
@* memory used by Lua.
@@ LUAI_MEM is a signed integer big enough to count the total memory
@* used by Lua.
** CHANGE here if for some weird reason the default definitions are not
** good enough for your machine. Probably you do not need to change
** this.
*/
#if LUAI_BITSINT >= 32		/* { */
#define LUA_INT32	int
#define LUAI_UMEM	size_t
#define LUAI_MEM	ptrdiff_t
#else				/* }{ */
/* 16-bit ints */
#define LUA_INT32	long
#define LUAI_UMEM	unsigned long
#define LUAI_MEM	long
#endif				/* } */
/*
@@ LUAI_MAXSTACK limits the size of the Lua stack.
** CHANGE it if you need a different limit. This limit is arbitrary;
** its only purpose is to stop Lua from consuming unlimited stack
** space (and to reserve some numbers for pseudo-indices).
*/
#if LUAI_BITSINT >= 32
#define LUAI_MAXSTACK		1000000
#else
#define LUAI_MAXSTACK		15000
#endif
/* reserve some space for error handling */
#define LUAI_FIRSTPSEUDOIDX	(-LUAI_MAXSTACK - 1000)
/*
@@ LUAL_BUFFERSIZE is the buffer size used by the lauxlib buffer system.
** CHANGE it if it uses too much C-stack space.
*/
#define LUAL_BUFFERSIZE		512
/*
** {==================================================================
@@ LUA_NUMBER is the type of numbers in Lua.
** CHANGE the following definitions only if you want to build Lua
** with a number type different from double. You may also need to
** change lua_number2int & lua_number2integer.
** ===================================================================
*/
#define LUA_NUMBER	int64_t
/*
@@ LUAI_UACNUMBER is the result of an 'usual argument conversion'
@* over a number.
*/
#define LUAI_UACNUMBER	int64_t
/*
@@ LUA_NUMBER_SCAN is the format for reading numbers.
@@ LUA_NUMBER_FMT is the format for writing numbers.
@@ lua_number2str converts a number to a string.
@@ LUAI_MAXNUMBER2STR is maximum size of previous conversion.
*/
#ifndef	PRId64
#define	PRId64	"lld"
#endif
#define LUAI_MAXNUMBER2STR	32 /* 16 digits, sign, point, and \0 */
#define LUA_NUMBER_FMT		"%" PRId64
#define lua_number2str(s,n)	\
	lcompat_sprintf((s), LUAI_MAXNUMBER2STR, LUA_NUMBER_FMT, (n))
#define l_mathop(x)		(x ## l)
#define lua_str2number(s,p)	lcompat_strtoll((s), (p))
#if defined(LUA_USE_STRTODHEX)
#define lua_strx2number(s,p)	lcompat_strtoll((s), (p))
#endif
#if defined(lobject_c) || defined(lvm_c)
#define luai_nummod(L,a,b)	((a) % (b))
#define luai_numpow(L,a,b)	(lcompat_pow((a),(b)))
#endif
#if defined(LUA_CORE)
#define luai_numadd(L,a,b)	((a)+(b))
#define luai_numsub(L,a,b)	((a)-(b))
#define luai_nummul(L,a,b)	((a)*(b))
#define luai_numdiv(L,a,b)	((a)/(b))
#define luai_numunm(L,a)	(-(a))
#define luai_numeq(a,b)		((a)==(b))
#define luai_numlt(L,a,b)	((a)<(b))
#define luai_numle(L,a,b)	((a)<=(b))
#define luai_numisnan(L,a)	(!luai_numeq((a), (a)))
#endif
#define LUA_INTEGER	ptrdiff_t
#define LUA_UNSIGNED	uint64_t
#if defined(LUA_NUMBER_DOUBLE) && !defined(LUA_ANSI)	 
#if defined(LUA_WIN) && defined(_MSC_VER) && defined(_M_IX86)	 
#define LUA_MSASMTRICK
#define LUA_IEEEENDIAN		0
#define LUA_NANTRICK
#elif defined(__i386__) || defined(__i386) || defined(__X86__)  
#define LUA_IEEE754TRICK
#define LUA_IEEELL
#define LUA_IEEEENDIAN		0
#define LUA_NANTRICK
#elif defined(__x86_64)						 
#define LUA_IEEE754TRICK
#define LUA_IEEEENDIAN		0
#elif defined(__POWERPC__) || defined(__ppc__)			 
#define LUA_IEEE754TRICK
#define LUA_IEEEENDIAN		1
#else								 
#define LUA_IEEE754TRICK
#endif								 
#endif							 
#define	getlocaledecpoint() ('.')
#ifndef abs
#define abs(x) (((x) < 0) ? -(x) : (x))
#endif
#if !defined(UCHAR_MAX)
#define	UCHAR_MAX (0xff)
#endif
#endif
