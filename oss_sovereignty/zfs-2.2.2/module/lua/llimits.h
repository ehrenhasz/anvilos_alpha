 

#ifndef llimits_h
#define llimits_h


#include <sys/lua/lua.h>


typedef unsigned LUA_INT32 lu_int32;

typedef LUAI_UMEM lu_mem;

typedef LUAI_MEM l_mem;



 
typedef unsigned char lu_byte;


#define MAX_SIZET	((size_t)(~(size_t)0)-2)

#define MAX_LUMEM	((lu_mem)(~(lu_mem)0)-2)

#define MAX_LMEM	((l_mem) ((MAX_LUMEM >> 1) - 2))


#define MAX_INT (INT_MAX-2)   

 
#define IntPoint(p)  ((unsigned int)(lu_mem)(p))



 
#if !defined(LUAI_USER_ALIGNMENT_T)
#define LUAI_USER_ALIGNMENT_T	union { double u; void *s; long l; }
#endif

typedef LUAI_USER_ALIGNMENT_T L_Umaxalign;


 
typedef LUAI_UACNUMBER l_uacNumber;


 
#if defined(lua_assert)
#define check_exp(c,e)		(lua_assert(c), (e))
 
#define lua_longassert(c)	{ if (!(c)) lua_assert(0); }
#else
#define lua_assert(c)		((void)0)
#define check_exp(c,e)		(e)
#define lua_longassert(c)	((void)0)
#endif

 
#if !defined(luai_apicheck)

#if defined(LUA_USE_APICHECK)
#include <assert.h>
#define luai_apicheck(L,e)	assert(e)
#else
#define luai_apicheck(L,e)	lua_assert(e)
#endif

#endif

#define api_check(l,e,msg)	luai_apicheck(l,(e) && msg)


#if !defined(UNUSED)
#define UNUSED(x)	((void)(x))	 
#endif


#define cast(t, exp)	((t)(exp))

#define cast_byte(i)	cast(lu_byte, (i))
#define cast_num(i)	cast(lua_Number, (i))
#define cast_int(i)	cast(int, (i))
#define cast_uchar(i)	cast(unsigned char, (i))


 
#if defined(__GNUC__) && !defined(_KERNEL)
#define l_noret		void __attribute__((noreturn))
#elif defined(_MSC_VER)
#define l_noret		void __declspec(noreturn)
#else
#define l_noret		void
#endif



 
#if !defined(LUAI_MAXCCALLS)
#define LUAI_MAXCCALLS		20
#endif

 
#define LUAI_MINCSTACK		4096

 
#define MAXUPVAL	UCHAR_MAX


 
typedef lu_int32 Instruction;



 
#define MAXSTACK	250



 
#if !defined(MINSTRTABSIZE)
#define MINSTRTABSIZE	32
#endif


 
#if !defined(LUA_MINBUFFER)
#define LUA_MINBUFFER	32
#endif


#if !defined(lua_lock)
#define lua_lock(L)     ((void) 0)
#define lua_unlock(L)   ((void) 0)
#endif

#if !defined(luai_threadyield)
#define luai_threadyield(L)     {lua_unlock(L); lua_lock(L);}
#endif


 
#if !defined(luai_userstateopen)
#define luai_userstateopen(L)		((void)L)
#endif

#if !defined(luai_userstateclose)
#define luai_userstateclose(L)		((void)L)
#endif

#if !defined(luai_userstatethread)
#define luai_userstatethread(L,L1)	((void)L)
#endif

#if !defined(luai_userstatefree)
#define luai_userstatefree(L,L1)	((void)L)
#endif

#if !defined(luai_userstateresume)
#define luai_userstateresume(L,n)       ((void)L)
#endif

#if !defined(luai_userstateyield)
#define luai_userstateyield(L,n)        ((void)L)
#endif

 

#if defined(MS_ASMTRICK) || defined(LUA_MSASMTRICK)	 
 

#define lua_number2int(i,n)  __asm {__asm fld n   __asm fistp i}
#define lua_number2integer(i,n)		lua_number2int(i, n)
#define lua_number2unsigned(i,n)  \
  {__int64 l; __asm {__asm fld n   __asm fistp l} i = (unsigned int)l;}


#elif defined(LUA_IEEE754TRICK)		 
 

union luai_Cast { double l_d; LUA_INT32 l_p[2]; };

#if !defined(LUA_IEEEENDIAN)	 
#define LUAI_EXTRAIEEE	\
  static const union luai_Cast ieeeendian = {-(33.0 + 6755399441055744.0)};
#define LUA_IEEEENDIANLOC	(ieeeendian.l_p[1] == 33)
#else
#define LUA_IEEEENDIANLOC	LUA_IEEEENDIAN
#define LUAI_EXTRAIEEE		 
#endif				 

#define lua_number2int32(i,n,t) \
  { LUAI_EXTRAIEEE \
    volatile union luai_Cast u; u.l_d = (n) + 6755399441055744.0; \
    (i) = (t)u.l_p[LUA_IEEEENDIANLOC]; }

#define luai_hashnum(i,n)  \
  { volatile union luai_Cast u; u.l_d = (n) + 1.0;    \
    (i) = u.l_p[0]; (i) += u.l_p[1]; }   

#define lua_number2int(i,n)		lua_number2int32(i, n, int)
#define lua_number2unsigned(i,n)	lua_number2int32(i, n, lua_Unsigned)

 
#if defined(LUA_IEEELL)
#define lua_number2integer(i,n)		lua_number2int32(i, n, lua_Integer)
#endif

#endif				 


 

#if !defined(lua_number2int)
#define lua_number2int(i,n)	((i)=(int)(n))
#endif

#if !defined(lua_number2integer)
#define lua_number2integer(i,n)	((i)=(lua_Integer)(n))
#endif

#if !defined(lua_number2unsigned)	 
 
#if defined(LUA_NUMBER_DOUBLE) || defined(LUA_NUMBER_FLOAT)
#include <math.h>
#define SUPUNSIGNED	((lua_Number)(~(lua_Unsigned)0) + 1)
#define lua_number2unsigned(i,n)  \
	((i)=(lua_Unsigned)((n) - floor((n)/SUPUNSIGNED)*SUPUNSIGNED))
#else
#define lua_number2unsigned(i,n)	((i)=(lua_Unsigned)(n))
#endif
#endif				 


#if !defined(lua_unsigned2number)
 
#define lua_unsigned2number(u)  \
    (((u) <= (lua_Unsigned)INT_MAX) ? (lua_Number)(int)(u) : (lua_Number)(u))
#endif



#if defined(ltable_c) && !defined(luai_hashnum)

#define luai_hashnum(i,n) (i = lcompat_hashnum(n))

#endif



 
#if !defined(HARDSTACKTESTS)
#define condmovestack(L)	((void)0)
#else
 
#define condmovestack(L)	luaD_reallocstack((L), (L)->stacksize)
#endif

#if !defined(HARDMEMTESTS)
#define condchangemem(L)	condmovestack(L)
#else
#define condchangemem(L)  \
	((void)(!(G(L)->gcrunning) || (luaC_fullgc(L, 0), 1)))
#endif

#endif
