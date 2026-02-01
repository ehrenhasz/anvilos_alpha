 


#define ldo_c
#define LUA_CORE

#include <sys/lua/lua.h>
#include <sys/asm_linkage.h>

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lvm.h"
#include "lzio.h"


 
#if defined (_KERNEL) && defined(__linux__)
#include <asm/current.h>
static intptr_t stack_remaining(void) {
  intptr_t local;
  local = (intptr_t)&local - (intptr_t)current->stack;
  return local;
}
#elif defined (_KERNEL) && defined(__FreeBSD__)
#include <sys/pcpu.h>
static intptr_t stack_remaining(void) {
  intptr_t local;
  local = (intptr_t)&local - (intptr_t)curthread->td_kstack;
  return local;
}
#else
static intptr_t stack_remaining(void) {
  return INTPTR_MAX;
}
#endif

 

 
#if !defined(LUAI_THROW)

#ifdef _KERNEL

#ifdef __linux__
#if defined(__i386__)
#define	JMP_BUF_CNT	6
#elif defined(__x86_64__)
#define	JMP_BUF_CNT	8
#elif defined(__sparc__) && defined(__arch64__)
#define	JMP_BUF_CNT	6
#elif defined(__powerpc__)
#define	JMP_BUF_CNT	26
#elif defined(__aarch64__)
#define	JMP_BUF_CNT	64
#elif defined(__arm__)
#define	JMP_BUF_CNT	65
#elif defined(__mips__)
#define JMP_BUF_CNT	12
#elif defined(__s390x__)
#define JMP_BUF_CNT	18
#elif defined(__riscv)
#define JMP_BUF_CNT     64
#elif defined(__loongarch_lp64)
#define JMP_BUF_CNT     64
#else
#define	JMP_BUF_CNT	1
#endif

typedef	struct _label_t { long long unsigned val[JMP_BUF_CNT]; } label_t;

int ASMABI setjmp(label_t *) __attribute__ ((__nothrow__));
extern __attribute__((noreturn)) void ASMABI longjmp(label_t *);

#define LUAI_THROW(L,c)		longjmp(&(c)->b)
#define LUAI_TRY(L,c,a)		if (setjmp(&(c)->b) == 0) { a }
#define luai_jmpbuf		label_t

 
#if JMP_BUF_CNT == 1
int setjmp (label_t *buf) {
	return 1;
}

void longjmp (label_t * buf) {
	for (;;);
}
#endif
#else
#define LUAI_THROW(L,c)		longjmp((c)->b, 1)
#define LUAI_TRY(L,c,a)		if (setjmp((c)->b) == 0) { a }
#define luai_jmpbuf		jmp_buf
#endif

#else  

#if defined(__cplusplus) && !defined(LUA_USE_LONGJMP)
 
#define LUAI_THROW(L,c)		throw(c)
#define LUAI_TRY(L,c,a) \
	try { a } catch(...) { if ((c)->status == 0) (c)->status = -1; }
#define luai_jmpbuf		int   

#elif defined(LUA_USE_ULONGJMP)
 
#define LUAI_THROW(L,c)		_longjmp((c)->b, 1)
#define LUAI_TRY(L,c,a)		if (_setjmp((c)->b) == 0) { a }
#define luai_jmpbuf		jmp_buf

#else
 
#define LUAI_THROW(L,c)		longjmp((c)->b, 1)
#define LUAI_TRY(L,c,a)		if (setjmp((c)->b) == 0) { a }
#define luai_jmpbuf		jmp_buf

#endif

#endif  

#endif  


 
struct lua_longjmp {
  struct lua_longjmp *previous;
  luai_jmpbuf b;
  volatile int status;   
};


static void seterrorobj (lua_State *L, int errcode, StkId oldtop) {
  switch (errcode) {
    case LUA_ERRMEM: {   
      setsvalue2s(L, oldtop, G(L)->memerrmsg);  
      break;
    }
    case LUA_ERRERR: {
      setsvalue2s(L, oldtop, luaS_newliteral(L, "error in error handling"));
      break;
    }
    default: {
      setobjs2s(L, oldtop, L->top - 1);   
      break;
    }
  }
  L->top = oldtop + 1;
}

 
#if defined(__GNUC__) && !defined(__clang__) && \
	defined(HAVE_KERNEL_INFINITE_RECURSION)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winfinite-recursion"
#endif

l_noret luaD_throw (lua_State *L, int errcode) {
  if (L->errorJmp) {   
    L->errorJmp->status = errcode;   
    LUAI_THROW(L, L->errorJmp);   
  }
  else {   
    L->status = cast_byte(errcode);   
    if (G(L)->mainthread->errorJmp) {   
      setobjs2s(L, G(L)->mainthread->top++, L->top - 1);   
      luaD_throw(G(L)->mainthread, errcode);   
    }
    else {   
      if (G(L)->panic) {   
        lua_unlock(L);
        G(L)->panic(L);   
      }
      panic("no error handler");
    }
  }
}

#if defined(__GNUC__) && !defined(__clang__) && \
	defined(HAVE_INFINITE_RECURSION)
#pragma GCC diagnostic pop
#endif


int luaD_rawrunprotected (lua_State *L, Pfunc f, void *ud) {
  unsigned short oldnCcalls = L->nCcalls;
  struct lua_longjmp lj;
  lj.status = LUA_OK;
  lj.previous = L->errorJmp;   
  L->errorJmp = &lj;
  LUAI_TRY(L, &lj,
    (*f)(L, ud);
  );
  L->errorJmp = lj.previous;   
  L->nCcalls = oldnCcalls;
  return lj.status;
}

 


static void correctstack (lua_State *L, TValue *oldstack) {
  CallInfo *ci;
  GCObject *up;
  L->top = (L->top - oldstack) + L->stack;
  for (up = L->openupval; up != NULL; up = up->gch.next)
    gco2uv(up)->v = (gco2uv(up)->v - oldstack) + L->stack;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    ci->top = (ci->top - oldstack) + L->stack;
    ci->func = (ci->func - oldstack) + L->stack;
    if (isLua(ci))
      ci->u.l.base = (ci->u.l.base - oldstack) + L->stack;
  }
}


 
#define ERRORSTACKSIZE	(LUAI_MAXSTACK + 200)


void luaD_reallocstack (lua_State *L, int newsize) {
  TValue *oldstack = L->stack;
  int lim = L->stacksize;
  lua_assert(newsize <= LUAI_MAXSTACK || newsize == ERRORSTACKSIZE);
  lua_assert(L->stack_last - L->stack == L->stacksize - EXTRA_STACK);
  luaM_reallocvector(L, L->stack, L->stacksize, newsize, TValue);
  for (; lim < newsize; lim++)
    setnilvalue(L->stack + lim);  
  L->stacksize = newsize;
  L->stack_last = L->stack + newsize - EXTRA_STACK;
  correctstack(L, oldstack);
}


void luaD_growstack (lua_State *L, int n) {
  int size = L->stacksize;
  if (size > LUAI_MAXSTACK)   
    luaD_throw(L, LUA_ERRERR);
  else {
    int needed = cast_int(L->top - L->stack) + n + EXTRA_STACK;
    int newsize = 2 * size;
    if (newsize > LUAI_MAXSTACK) newsize = LUAI_MAXSTACK;
    if (newsize < needed) newsize = needed;
    if (newsize > LUAI_MAXSTACK) {   
      luaD_reallocstack(L, ERRORSTACKSIZE);
      luaG_runerror(L, "stack overflow");
    }
    else
      luaD_reallocstack(L, newsize);
  }
}


static int stackinuse (lua_State *L) {
  CallInfo *ci;
  StkId lim = L->top;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    lua_assert(ci->top <= L->stack_last);
    if (lim < ci->top) lim = ci->top;
  }
  return cast_int(lim - L->stack) + 1;   
}


void luaD_shrinkstack (lua_State *L) {
  int inuse = stackinuse(L);
  int goodsize = inuse + (inuse / 8) + 2*EXTRA_STACK;
  if (goodsize > LUAI_MAXSTACK) goodsize = LUAI_MAXSTACK;
  if (inuse > LUAI_MAXSTACK ||   
      goodsize >= L->stacksize)   
    condmovestack(L);   
  else
    luaD_reallocstack(L, goodsize);   
}


void luaD_hook (lua_State *L, int event, int line) {
  lua_Hook hook = L->hook;
  if (hook && L->allowhook) {
    CallInfo *ci = L->ci;
    ptrdiff_t top = savestack(L, L->top);
    ptrdiff_t ci_top = savestack(L, ci->top);
    lua_Debug ar;
    ar.event = event;
    ar.currentline = line;
    ar.i_ci = ci;
    luaD_checkstack(L, LUA_MINSTACK);   
    ci->top = L->top + LUA_MINSTACK;
    lua_assert(ci->top <= L->stack_last);
    L->allowhook = 0;   
    ci->callstatus |= CIST_HOOKED;
    lua_unlock(L);
    (*hook)(L, &ar);
    lua_lock(L);
    lua_assert(!L->allowhook);
    L->allowhook = 1;
    ci->top = restorestack(L, ci_top);
    L->top = restorestack(L, top);
    ci->callstatus &= ~CIST_HOOKED;
  }
}


static void callhook (lua_State *L, CallInfo *ci) {
  int hook = LUA_HOOKCALL;
  ci->u.l.savedpc++;   
  if (isLua(ci->previous) &&
      GET_OPCODE(*(ci->previous->u.l.savedpc - 1)) == OP_TAILCALL) {
    ci->callstatus |= CIST_TAIL;
    hook = LUA_HOOKTAILCALL;
  }
  luaD_hook(L, hook, -1);
  ci->u.l.savedpc--;   
}


static StkId adjust_varargs (lua_State *L, Proto *p, int actual) {
  int i;
  int nfixargs = p->numparams;
  StkId base, fixed;
  lua_assert(actual >= nfixargs);
   
  luaD_checkstack(L, p->maxstacksize);   
  fixed = L->top - actual;   
  base = L->top;   
  for (i=0; i<nfixargs; i++) {
    setobjs2s(L, L->top++, fixed + i);
    setnilvalue(fixed + i);
  }
  return base;
}


static StkId tryfuncTM (lua_State *L, StkId func) {
  const TValue *tm = luaT_gettmbyobj(L, func, TM_CALL);
  StkId p;
  ptrdiff_t funcr = savestack(L, func);
  if (!ttisfunction(tm))
    luaG_typeerror(L, func, "call");
   
  for (p = L->top; p > func; p--) setobjs2s(L, p, p-1);
  incr_top(L);
  func = restorestack(L, funcr);   
  setobj2s(L, func, tm);   
  return func;
}



#define next_ci(L) (L->ci = (L->ci->next ? L->ci->next : luaE_extendCI(L)))


 
int luaD_precall (lua_State *L, StkId func, int nresults) {
  lua_CFunction f;
  CallInfo *ci;
  int n;   
  ptrdiff_t funcr = savestack(L, func);
  switch (ttype(func)) {
    case LUA_TLCF:   
      f = fvalue(func);
      goto Cfunc;
    case LUA_TCCL: {   
      f = clCvalue(func)->f;
     Cfunc:
      luaD_checkstack(L, LUA_MINSTACK);   
      ci = next_ci(L);   
      ci->nresults = nresults;
      ci->func = restorestack(L, funcr);
      ci->top = L->top + LUA_MINSTACK;
      lua_assert(ci->top <= L->stack_last);
      ci->callstatus = 0;
      luaC_checkGC(L);   
      if (L->hookmask & LUA_MASKCALL)
        luaD_hook(L, LUA_HOOKCALL, -1);
      lua_unlock(L);
      n = (*f)(L);   
      lua_lock(L);
      api_checknelems(L, n);
      luaD_poscall(L, L->top - n);
      return 1;
    }
    case LUA_TLCL: {   
      StkId base;
      Proto *p = clLvalue(func)->p;
      n = cast_int(L->top - func) - 1;   
      luaD_checkstack(L, p->maxstacksize + p->numparams);
      for (; n < p->numparams; n++)
        setnilvalue(L->top++);   
      if (!p->is_vararg) {
        func = restorestack(L, funcr);
        base = func + 1;
      }
      else {
        base = adjust_varargs(L, p, n);
        func = restorestack(L, funcr);   
      }
      ci = next_ci(L);   
      ci->nresults = nresults;
      ci->func = func;
      ci->u.l.base = base;
      ci->top = base + p->maxstacksize;
      lua_assert(ci->top <= L->stack_last);
      ci->u.l.savedpc = p->code;   
      ci->callstatus = CIST_LUA;
      L->top = ci->top;
      luaC_checkGC(L);   
      if (L->hookmask & LUA_MASKCALL)
        callhook(L, ci);
      return 0;
    }
    default: {   
      func = tryfuncTM(L, func);   
      return luaD_precall(L, func, nresults);   
    }
  }
}


int luaD_poscall (lua_State *L, StkId firstResult) {
  StkId res;
  int wanted, i;
  CallInfo *ci = L->ci;
  if (L->hookmask & (LUA_MASKRET | LUA_MASKLINE)) {
    if (L->hookmask & LUA_MASKRET) {
      ptrdiff_t fr = savestack(L, firstResult);   
      luaD_hook(L, LUA_HOOKRET, -1);
      firstResult = restorestack(L, fr);
    }
    L->oldpc = ci->previous->u.l.savedpc;   
  }
  res = ci->func;   
  wanted = ci->nresults;
  L->ci = ci->previous;   
   
  for (i = wanted; i != 0 && firstResult < L->top; i--)
    setobjs2s(L, res++, firstResult++);
  while (i-- > 0)
    setnilvalue(res++);
  L->top = res;
  return (wanted - LUA_MULTRET);   
}


 
void luaD_call (lua_State *L, StkId func, int nResults, int allowyield) {
  if (++L->nCcalls >= LUAI_MAXCCALLS) {
    if (L->nCcalls == LUAI_MAXCCALLS)
      luaG_runerror(L, "C stack overflow");
    else if (L->nCcalls >= (LUAI_MAXCCALLS + (LUAI_MAXCCALLS>>3)))
      luaD_throw(L, LUA_ERRERR);   
  }
  intptr_t remaining = stack_remaining();
  if (L->runerror == 0 && remaining < LUAI_MINCSTACK)
    luaG_runerror(L, "C stack overflow");
  if (L->runerror != 0 && remaining < LUAI_MINCSTACK / 2)
    luaD_throw(L, LUA_ERRERR);   
  if (!allowyield) L->nny++;
  if (!luaD_precall(L, func, nResults))   
    luaV_execute(L);   
  if (!allowyield) L->nny--;
  L->nCcalls--;
}


static void finishCcall (lua_State *L) {
  CallInfo *ci = L->ci;
  int n;
  lua_assert(ci->u.c.k != NULL);   
  lua_assert(L->nny == 0);
  if (ci->callstatus & CIST_YPCALL) {   
    ci->callstatus &= ~CIST_YPCALL;   
    L->errfunc = ci->u.c.old_errfunc;
  }
   
  adjustresults(L, ci->nresults);
   
  if (!(ci->callstatus & CIST_STAT))   
    ci->u.c.status = LUA_YIELD;   
  lua_assert(ci->u.c.status != LUA_OK);
  ci->callstatus = (ci->callstatus & ~(CIST_YPCALL | CIST_STAT)) | CIST_YIELDED;
  lua_unlock(L);
  n = (*ci->u.c.k)(L);
  lua_lock(L);
  api_checknelems(L, n);
   
  luaD_poscall(L, L->top - n);
}


static void unroll (lua_State *L, void *ud) {
  UNUSED(ud);
  for (;;) {
    if (L->ci == &L->base_ci)   
      return;   
    if (!isLua(L->ci))   
      finishCcall(L);
    else {   
      luaV_finishOp(L);   
      luaV_execute(L);   
    }
  }
}


 
static CallInfo *findpcall (lua_State *L) {
  CallInfo *ci;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {   
    if (ci->callstatus & CIST_YPCALL)
      return ci;
  }
  return NULL;   
}


static int recover (lua_State *L, int status) {
  StkId oldtop;
  CallInfo *ci = findpcall(L);
  if (ci == NULL) return 0;   
   
  oldtop = restorestack(L, ci->extra);
  luaF_close(L, oldtop);
  seterrorobj(L, status, oldtop);
  L->ci = ci;
  L->allowhook = ci->u.c.old_allowhook;
  L->nny = 0;   
  luaD_shrinkstack(L);
  L->errfunc = ci->u.c.old_errfunc;
  ci->callstatus |= CIST_STAT;   
  ci->u.c.status = status;   
  return 1;   
}


 
static l_noret resume_error (lua_State *L, const char *msg, StkId firstArg) {
  L->top = firstArg;   
  setsvalue2s(L, L->top, luaS_new(L, msg));   
  api_incr_top(L);
  luaD_throw(L, -1);   
}


 
static void resume_cb (lua_State *L, void *ud) {
  int nCcalls = L->nCcalls;
  StkId firstArg = cast(StkId, ud);
  CallInfo *ci = L->ci;
  if (nCcalls >= LUAI_MAXCCALLS)
    resume_error(L, "C stack overflow", firstArg);
  if (L->status == LUA_OK) {   
    if (ci != &L->base_ci)   
      resume_error(L, "cannot resume non-suspended coroutine", firstArg);
     
    if (!luaD_precall(L, firstArg - 1, LUA_MULTRET))   
      luaV_execute(L);   
  }
  else if (L->status != LUA_YIELD)
    resume_error(L, "cannot resume dead coroutine", firstArg);
  else {   
    L->status = LUA_OK;
    ci->func = restorestack(L, ci->extra);
    if (isLua(ci))   
      luaV_execute(L);   
    else {   
      if (ci->u.c.k != NULL) {   
        int n;
        ci->u.c.status = LUA_YIELD;   
        ci->callstatus |= CIST_YIELDED;
        lua_unlock(L);
        n = (*ci->u.c.k)(L);   
        lua_lock(L);
        api_checknelems(L, n);
        firstArg = L->top - n;   
      }
      luaD_poscall(L, firstArg);   
    }
    unroll(L, NULL);
  }
  lua_assert(nCcalls == L->nCcalls);
}


LUA_API int lua_resume (lua_State *L, lua_State *from, int nargs) {
  int status;
  int oldnny = L->nny;   
  lua_lock(L);
  luai_userstateresume(L, nargs);
  L->nCcalls = (from) ? from->nCcalls + 1 : 1;
  L->nny = 0;   
  api_checknelems(L, (L->status == LUA_OK) ? nargs + 1 : nargs);
  status = luaD_rawrunprotected(L, resume_cb, L->top - nargs);
  if (status == -1)   
    status = LUA_ERRRUN;
  else {   
    while (status != LUA_OK && status != LUA_YIELD) {   
      if (recover(L, status))   
        status = luaD_rawrunprotected(L, unroll, NULL);   
      else {   
        L->status = cast_byte(status);   
        seterrorobj(L, status, L->top);
        L->ci->top = L->top;
        break;
      }
    }
    lua_assert(status == L->status);
  }
  L->nny = oldnny;   
  L->nCcalls--;
  lua_assert(L->nCcalls == ((from) ? from->nCcalls : 0));
  lua_unlock(L);
  return status;
}


LUA_API int lua_yieldk (lua_State *L, int nresults, int ctx, lua_CFunction k) {
  CallInfo *ci = L->ci;
  luai_userstateyield(L, nresults);
  lua_lock(L);
  api_checknelems(L, nresults);
  if (L->nny > 0) {
    if (L != G(L)->mainthread)
      luaG_runerror(L, "attempt to yield across a C-call boundary");
    else
      luaG_runerror(L, "attempt to yield from outside a coroutine");
  }
  L->status = LUA_YIELD;
  ci->extra = savestack(L, ci->func);   
  if (isLua(ci)) {   
    api_check(L, k == NULL, "hooks cannot continue after yielding");
  }
  else {
    if ((ci->u.c.k = k) != NULL)   
      ci->u.c.ctx = ctx;   
    ci->func = L->top - nresults - 1;   
    luaD_throw(L, LUA_YIELD);
  }
  lua_assert(ci->callstatus & CIST_HOOKED);   
  lua_unlock(L);
  return 0;   
}


int luaD_pcall (lua_State *L, Pfunc func, void *u,
                ptrdiff_t old_top, ptrdiff_t ef) {
  int status;
  CallInfo *old_ci = L->ci;
  lu_byte old_allowhooks = L->allowhook;
  unsigned short old_nny = L->nny;
  ptrdiff_t old_errfunc = L->errfunc;
  L->errfunc = ef;
  status = luaD_rawrunprotected(L, func, u);
  if (status != LUA_OK) {   
    StkId oldtop = restorestack(L, old_top);
    luaF_close(L, oldtop);   
    seterrorobj(L, status, oldtop);
    L->ci = old_ci;
    L->allowhook = old_allowhooks;
    L->nny = old_nny;
    luaD_shrinkstack(L);
  }
  L->errfunc = old_errfunc;
  return status;
}



 
struct SParser {   
  ZIO *z;
  Mbuffer buff;   
  Dyndata dyd;   
  const char *mode;
  const char *name;
};


static void checkmode (lua_State *L, const char *mode, const char *x) {
  if (mode && strchr(mode, x[0]) == NULL) {
    luaO_pushfstring(L,
       "attempt to load a %s chunk (mode is " LUA_QS ")", x, mode);
    luaD_throw(L, LUA_ERRSYNTAX);
  }
}


static void f_parser (lua_State *L, void *ud) {
  int i;
  Closure *cl;
  struct SParser *p = cast(struct SParser *, ud);
  int c = zgetc(p->z);   
  lua_assert(c != LUA_SIGNATURE[0]);	 
  checkmode(L, p->mode, "text");
  cl = luaY_parser(L, p->z, &p->buff, &p->dyd, p->name, c);
  lua_assert(cl->l.nupvalues == cl->l.p->sizeupvalues);
  for (i = 0; i < cl->l.nupvalues; i++) {   
    UpVal *up = luaF_newupval(L);
    cl->l.upvals[i] = up;
    luaC_objbarrier(L, cl, up);
  }
}


int luaD_protectedparser (lua_State *L, ZIO *z, const char *name,
                                        const char *mode) {
  struct SParser p;
  int status;
  L->nny++;   
  p.z = z; p.name = name; p.mode = mode;
  p.dyd.actvar.arr = NULL; p.dyd.actvar.size = 0;
  p.dyd.gt.arr = NULL; p.dyd.gt.size = 0;
  p.dyd.label.arr = NULL; p.dyd.label.size = 0;
  luaZ_initbuffer(L, &p.buff);
  status = luaD_pcall(L, f_parser, &p, savestack(L, L->top), L->errfunc);
  luaZ_freebuffer(L, &p.buff);
  luaM_freearray(L, p.dyd.actvar.arr, p.dyd.actvar.size);
  luaM_freearray(L, p.dyd.gt.arr, p.dyd.gt.size);
  luaM_freearray(L, p.dyd.label.arr, p.dyd.label.size);
  L->nny--;
  return status;
}
