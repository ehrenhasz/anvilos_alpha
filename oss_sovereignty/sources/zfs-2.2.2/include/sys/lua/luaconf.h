


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

#else			

#define LUA_VDIR	LUA_VERSION_MAJOR "." LUA_VERSION_MINOR "/"
#define LUA_ROOT	"/usr/local/"
#define LUA_LDIR	LUA_ROOT "share/lua/" LUA_VDIR
#define LUA_CDIR	LUA_ROOT "lib/lua/" LUA_VDIR
#define LUA_PATH_DEFAULT  \
		LUA_LDIR"?.lua;"  LUA_LDIR"?/init.lua;" \
		LUA_CDIR"?.lua;"  LUA_CDIR"?/init.lua;" "./?.lua"
#define LUA_CPATH_DEFAULT \
		LUA_CDIR"?.so;" LUA_CDIR"loadall.so;" "./?.so"
#endif			



#if defined(_WIN32)
#define LUA_DIRSEP	"\\"
#else
#define LUA_DIRSEP	"/"
#endif



#define LUA_ENV		"_ENV"



#if defined(LUA_BUILD_AS_DLL)	

#if defined(LUA_CORE) || defined(LUA_LIB)	
#define LUA_API __declspec(dllexport)
#else						
#define LUA_API __declspec(dllimport)
#endif						

#else				

#define LUA_API		extern

#endif				



#define LUALIB_API	LUA_API
#define LUAMOD_API	LUALIB_API



#if defined(__GNUC__) && ((__GNUC__*100 + __GNUC_MINOR__) >= 302) && \
    defined(__ELF__)		
#define LUAI_FUNC	__attribute__((visibility("hidden"))) extern
#define LUAI_DDEC	LUAI_FUNC
#define LUAI_DDEF	

#else				
#define LUAI_FUNC	extern
#define LUAI_DDEC	extern
#define LUAI_DDEF	
#endif				




#define LUA_QL(x)	"'" x "'"
#define LUA_QS		LUA_QL("%s")



#define LUA_IDSIZE	60



#ifdef _KERNEL
#define luai_writestringerror(s,p) \
	(zfs_dbgmsg((s), (p)))
#else
#define luai_writestringerror(s,p) \
	(fprintf(stderr, (s), (p)), fflush(stderr))
#endif



#define LUAI_MAXSHORTLEN        40






#if defined(LUA_COMPAT_ALL)	


#define LUA_COMPAT_UNPACK


#define LUA_COMPAT_LOADERS


#define lua_cpcall(L,f,u)  \
	(lua_pushcfunction(L, (f)), \
	 lua_pushlightuserdata(L,(u)), \
	 lua_pcall(L,1,0,0))



#define LUA_COMPAT_LOG10


#define LUA_COMPAT_LOADSTRING


#define LUA_COMPAT_MAXN


#define lua_strlen(L,i)		lua_rawlen(L, (i))

#define lua_objlen(L,i)		lua_rawlen(L, (i))

#define lua_equal(L,idx1,idx2)		lua_compare(L,(idx1),(idx2),LUA_OPEQ)
#define lua_lessthan(L,idx1,idx2)	lua_compare(L,(idx1),(idx2),LUA_OPLT)


#define LUA_COMPAT_MODULE

#endif				




#undef INT_MAX
#define	INT_MAX	2147483647L



#if INT_MAX-20 < 32760		
#define LUAI_BITSINT	16
#elif INT_MAX > 2147483640L	

#define LUAI_BITSINT	32
#else				
#error "you must define LUA_BITSINT with number of bits in an integer"
#endif				



#if LUAI_BITSINT >= 32		
#define LUA_INT32	int
#define LUAI_UMEM	size_t
#define LUAI_MEM	ptrdiff_t
#else				

#define LUA_INT32	long
#define LUAI_UMEM	unsigned long
#define LUAI_MEM	long
#endif				



#if LUAI_BITSINT >= 32
#define LUAI_MAXSTACK		1000000
#else
#define LUAI_MAXSTACK		15000
#endif


#define LUAI_FIRSTPSEUDOIDX	(-LUAI_MAXSTACK - 1000)



#define LUAL_BUFFERSIZE		512




#define LUA_NUMBER	int64_t


#define LUAI_UACNUMBER	int64_t



#ifndef	PRId64
#define	PRId64	"lld"
#endif

#define LUAI_MAXNUMBER2STR	32 
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
