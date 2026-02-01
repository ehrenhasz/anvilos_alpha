 


#ifndef lobject_h
#define lobject_h


#include "llimits.h"
#include <sys/lua/lua.h>


 
#define LUA_TPROTO	LUA_NUMTAGS
#define LUA_TUPVAL	(LUA_NUMTAGS+1)
#define LUA_TDEADKEY	(LUA_NUMTAGS+2)

 
#define LUA_TOTALTAGS	(LUA_TUPVAL+2)


 

#define VARBITS		(3 << 4)


 

 
#define LUA_TLCL	(LUA_TFUNCTION | (0 << 4))   
#define LUA_TLCF	(LUA_TFUNCTION | (1 << 4))   
#define LUA_TCCL	(LUA_TFUNCTION | (2 << 4))   


 
#define LUA_TSHRSTR	(LUA_TSTRING | (0 << 4))   
#define LUA_TLNGSTR	(LUA_TSTRING | (1 << 4))   


 
#define BIT_ISCOLLECTABLE	(1 << 6)

 
#define ctb(t)			((t) | BIT_ISCOLLECTABLE)


 
typedef union GCObject GCObject;


 
#define CommonHeader	GCObject *next; lu_byte tt; lu_byte marked


 
typedef struct GCheader {
  CommonHeader;
} GCheader;



 
typedef union Value Value;


#define numfield	lua_Number n;     



 

#define TValuefields	Value value_; int tt_

typedef struct lua_TValue TValue;


 
#define NILCONSTANT	{NULL}, LUA_TNIL


#define val_(o)		((o)->value_)
#define num_(o)		(val_(o).n)


 
#define rttype(o)	((o)->tt_)

 
#define novariant(x)	((x) & 0x0F)

 
#define ttype(o)	(rttype(o) & 0x3F)

 
#define ttypenv(o)	(novariant(rttype(o)))


 
#define checktag(o,t)		(rttype(o) == (t))
#define checktype(o,t)		(ttypenv(o) == (t))
#define ttisnumber(o)		checktag((o), LUA_TNUMBER)
#define ttisnil(o)		checktag((o), LUA_TNIL)
#define ttisboolean(o)		checktag((o), LUA_TBOOLEAN)
#define ttislightuserdata(o)	checktag((o), LUA_TLIGHTUSERDATA)
#define ttisstring(o)		checktype((o), LUA_TSTRING)
#define ttisshrstring(o)	checktag((o), ctb(LUA_TSHRSTR))
#define ttislngstring(o)	checktag((o), ctb(LUA_TLNGSTR))
#define ttistable(o)		checktag((o), ctb(LUA_TTABLE))
#define ttisfunction(o)		checktype(o, LUA_TFUNCTION)
#define ttisclosure(o)		((rttype(o) & 0x1F) == LUA_TFUNCTION)
#define ttisCclosure(o)		checktag((o), ctb(LUA_TCCL))
#define ttisLclosure(o)		checktag((o), ctb(LUA_TLCL))
#define ttislcf(o)		checktag((o), LUA_TLCF)
#define ttisuserdata(o)		checktag((o), ctb(LUA_TUSERDATA))
#define ttisthread(o)		checktag((o), ctb(LUA_TTHREAD))
#define ttisdeadkey(o)		checktag((o), LUA_TDEADKEY)

#define ttisequal(o1,o2)	(rttype(o1) == rttype(o2))

 
#define nvalue(o)	check_exp(ttisnumber(o), num_(o))
#define gcvalue(o)	check_exp(iscollectable(o), val_(o).gc)
#define pvalue(o)	check_exp(ttislightuserdata(o), val_(o).p)
#define rawtsvalue(o)	check_exp(ttisstring(o), &val_(o).gc->ts)
#define tsvalue(o)	(&rawtsvalue(o)->tsv)
#define rawuvalue(o)	check_exp(ttisuserdata(o), &val_(o).gc->u)
#define uvalue(o)	(&rawuvalue(o)->uv)
#define clvalue(o)	check_exp(ttisclosure(o), &val_(o).gc->cl)
#define clLvalue(o)	check_exp(ttisLclosure(o), &val_(o).gc->cl.l)
#define clCvalue(o)	check_exp(ttisCclosure(o), &val_(o).gc->cl.c)
#define fvalue(o)	check_exp(ttislcf(o), val_(o).f)
#define hvalue(o)	check_exp(ttistable(o), &val_(o).gc->h)
#define bvalue(o)	check_exp(ttisboolean(o), val_(o).b)
#define thvalue(o)	check_exp(ttisthread(o), &val_(o).gc->th)
 
#define deadvalue(o)	check_exp(ttisdeadkey(o), cast(void *, val_(o).gc))

#define l_isfalse(o)	(ttisnil(o) || (ttisboolean(o) && bvalue(o) == 0))


#define iscollectable(o)	(rttype(o) & BIT_ISCOLLECTABLE)


 
#define righttt(obj)		(ttype(obj) == gcvalue(obj)->gch.tt)

#define checkliveness(g,obj) \
	lua_longassert(!iscollectable(obj) || \
			(righttt(obj) && !isdead(g,gcvalue(obj))))


 
#define settt_(o,t)	((o)->tt_=(t))

#define setnvalue(obj,x) \
  { TValue *io=(obj); num_(io)=(x); settt_(io, LUA_TNUMBER); }

#define setnilvalue(obj) settt_(obj, LUA_TNIL)

#define setfvalue(obj,x) \
  { TValue *io=(obj); val_(io).f=(x); settt_(io, LUA_TLCF); }

#define setpvalue(obj,x) \
  { TValue *io=(obj); val_(io).p=(x); settt_(io, LUA_TLIGHTUSERDATA); }

#define setbvalue(obj,x) \
  { TValue *io=(obj); val_(io).b=(x); settt_(io, LUA_TBOOLEAN); }

#define setgcovalue(L,obj,x) \
  { TValue *io=(obj); GCObject *i_g=(x); \
    val_(io).gc=i_g; settt_(io, ctb(gch(i_g)->tt)); }

#define setsvalue(L,obj,x) \
  { TValue *io=(obj); \
    TString *x_ = (x); \
    val_(io).gc=cast(GCObject *, x_); settt_(io, ctb(x_->tsv.tt)); \
    checkliveness(G(L),io); }

#define setuvalue(L,obj,x) \
  { TValue *io=(obj); \
    val_(io).gc=cast(GCObject *, (x)); settt_(io, ctb(LUA_TUSERDATA)); \
    checkliveness(G(L),io); }

#define setthvalue(L,obj,x) \
  { TValue *io=(obj); \
    val_(io).gc=cast(GCObject *, (x)); settt_(io, ctb(LUA_TTHREAD)); \
    checkliveness(G(L),io); }

#define setclLvalue(L,obj,x) \
  { TValue *io=(obj); \
    val_(io).gc=cast(GCObject *, (x)); settt_(io, ctb(LUA_TLCL)); \
    checkliveness(G(L),io); }

#define setclCvalue(L,obj,x) \
  { TValue *io=(obj); \
    val_(io).gc=cast(GCObject *, (x)); settt_(io, ctb(LUA_TCCL)); \
    checkliveness(G(L),io); }

#define sethvalue(L,obj,x) \
  { TValue *io=(obj); \
    val_(io).gc=cast(GCObject *, (x)); settt_(io, ctb(LUA_TTABLE)); \
    checkliveness(G(L),io); }

#define setdeadvalue(obj)	settt_(obj, LUA_TDEADKEY)



#define setobj(L,obj1,obj2) \
	{ const TValue *io2=(obj2); TValue *io1=(obj1); \
	  io1->value_ = io2->value_; io1->tt_ = io2->tt_; \
	  checkliveness(G(L),io1); }


 

 
#define setobjs2s	setobj
 
#define setobj2s	setobj
#define setsvalue2s	setsvalue
#define sethvalue2s	sethvalue
#define setptvalue2s	setptvalue
 
#define setobjt2t	setobj
 
#define setobj2t	setobj
 
#define setobj2n	setobj
#define setsvalue2n	setsvalue


 
#define luai_checknum(L,o,c)	{   }


 
#if defined(LUA_NANTRICK)

 

 
#if !defined(NNMARK)	 


#if !defined(LUA_IEEEENDIAN)
#error option 'LUA_NANTRICK' needs 'LUA_IEEEENDIAN'
#endif


#define NNMARK		0x7FF7A500
#define NNMASK		0x7FFFFF00

#undef TValuefields
#undef NILCONSTANT

#if (LUA_IEEEENDIAN == 0)	 

 
#define TValuefields  \
	union { struct { Value v__; int tt__; } i; double d__; } u
#define NILCONSTANT	{{{NULL}, tag2tt(LUA_TNIL)}}
 
#define v_(o)		((o)->u.i.v__)
#define d_(o)		((o)->u.d__)
#define tt_(o)		((o)->u.i.tt__)

#else				 

 
#define TValuefields  \
	union { struct { int tt__; Value v__; } i; double d__; } u
#define NILCONSTANT	{{tag2tt(LUA_TNIL), {NULL}}}
 
#define v_(o)		((o)->u.i.v__)
#define d_(o)		((o)->u.d__)
#define tt_(o)		((o)->u.i.tt__)

#endif				 

#endif			 


 
#undef val_
#define val_(o)		v_(o)
#undef num_
#define num_(o)		d_(o)


#undef numfield
#define numfield	 

 
#undef ttisnumber
#define ttisnumber(o)	((tt_(o) & NNMASK) != NNMARK)

#define tag2tt(t)	(NNMARK | (t))

#undef rttype
#define rttype(o)	(ttisnumber(o) ? LUA_TNUMBER : tt_(o) & 0xff)

#undef settt_
#define settt_(o,t)	(tt_(o) = tag2tt(t))

#undef setnvalue
#define setnvalue(obj,x) \
	{ TValue *io_=(obj); num_(io_)=(x); lua_assert(ttisnumber(io_)); }

#undef setobj
#define setobj(L,obj1,obj2) \
	{ const TValue *o2_=(obj2); TValue *o1_=(obj1); \
	  o1_->u = o2_->u; \
	  checkliveness(G(L),o1_); }


 

#undef checktag
#undef checktype
#define checktag(o,t)	(tt_(o) == tag2tt(t))
#define checktype(o,t)	(ctb(tt_(o) | VARBITS) == ctb(tag2tt(t) | VARBITS))

#undef ttisequal
#define ttisequal(o1,o2)  \
	(ttisnumber(o1) ? ttisnumber(o2) : (tt_(o1) == tt_(o2)))


#undef luai_checknum
#define luai_checknum(L,o,c)	{ if (!ttisnumber(o)) c; }

#endif
 



 


union Value {
  GCObject *gc;     
  void *p;          
  int b;            
  lua_CFunction f;  
  numfield          
};


struct lua_TValue {
  TValuefields;
};


typedef TValue *StkId;   




 
typedef union TString {
  L_Umaxalign dummy;   
  struct {
    CommonHeader;
    lu_byte extra;   
    unsigned int hash;
    size_t len;   
  } tsv;
} TString;


 
#define getstr(ts)	cast(const char *, (ts) + 1)

 
#define svalue(o)       getstr(rawtsvalue(o))


 
typedef union Udata {
  L_Umaxalign dummy;   
  struct {
    CommonHeader;
    struct Table *metatable;
    struct Table *env;
    size_t len;   
  } uv;
} Udata;



 
typedef struct Upvaldesc {
  TString *name;   
  lu_byte instack;   
  lu_byte idx;   
} Upvaldesc;


 
typedef struct LocVar {
  TString *varname;
  int startpc;   
  int endpc;     
} LocVar;


 
typedef struct Proto {
  CommonHeader;
  TValue *k;   
  Instruction *code;
  struct Proto **p;   
  int *lineinfo;   
  LocVar *locvars;   
  Upvaldesc *upvalues;   
  union Closure *cache;   
  TString  *source;   
  int sizeupvalues;   
  int sizek;   
  int sizecode;
  int sizelineinfo;
  int sizep;   
  int sizelocvars;
  int linedefined;
  int lastlinedefined;
  GCObject *gclist;
  lu_byte numparams;   
  lu_byte is_vararg;
  lu_byte maxstacksize;   
} Proto;



 
typedef struct UpVal {
  CommonHeader;
  TValue *v;   
  union {
    TValue value;   
    struct {   
      struct UpVal *prev;
      struct UpVal *next;
    } l;
  } u;
} UpVal;


 

#define ClosureHeader \
	CommonHeader; lu_byte nupvalues; GCObject *gclist

typedef struct CClosure {
  ClosureHeader;
  lua_CFunction f;
  TValue upvalue[];   
} CClosure;


typedef struct LClosure {
  ClosureHeader;
  struct Proto *p;
  UpVal *upvals[];   
} LClosure;


typedef union Closure {
  CClosure c;
  LClosure l;
} Closure;


#define isLfunction(o)	ttisLclosure(o)

#define getproto(o)	(clLvalue(o)->p)


 

typedef union TKey {
  struct {
    TValuefields;
    struct Node *next;   
  } nk;
  TValue tvk;
} TKey;


typedef struct Node {
  TValue i_val;
  TKey i_key;
} Node;


typedef struct Table {
  CommonHeader;
  lu_byte flags;   
  lu_byte lsizenode;   
  int sizearray;   
  TValue *array;   
  Node *node;
  Node *lastfree;   
  struct Table *metatable;
  GCObject *gclist;
} Table;



 
#define lmod(s,size) \
	(check_exp((size&(size-1))==0, (cast(int, (s) & ((size)-1)))))


#define twoto(x)	(1<<(x))
#define sizenode(t)	(twoto((t)->lsizenode))


 
#define luaO_nilobject		(&luaO_nilobject_)


LUAI_DDEC const TValue luaO_nilobject_;


LUAI_FUNC int luaO_int2fb (unsigned int x);
LUAI_FUNC int luaO_fb2int (int x);
LUAI_FUNC int luaO_ceillog2 (unsigned int x);
LUAI_FUNC lua_Number luaO_arith (int op, lua_Number v1, lua_Number v2);
LUAI_FUNC int luaO_str2d (const char *s, size_t len, lua_Number *result);
LUAI_FUNC int luaO_hexavalue (int c);
LUAI_FUNC const char *luaO_pushvfstring (lua_State *L, const char *fmt,
                                                       va_list argp);
LUAI_FUNC const char *luaO_pushfstring (lua_State *L, const char *fmt, ...);
LUAI_FUNC void luaO_chunkid (char *out, const char *source, size_t len);


#endif
