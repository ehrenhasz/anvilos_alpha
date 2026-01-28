#define ldebug_c
#define LUA_CORE
#include <sys/lua/lua.h>
#include "lapi.h"
#include "lcode.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lvm.h"
#define noLuaClosure(f)		((f) == NULL || (f)->c.tt == LUA_TCCL)
static const char *getfuncname (lua_State *L, CallInfo *ci, const char **name);
static int currentpc (CallInfo *ci) {
  lua_assert(isLua(ci));
  return pcRel(ci->u.l.savedpc, ci_func(ci)->p);
}
static int currentline (CallInfo *ci) {
  return getfuncline(ci_func(ci)->p, currentpc(ci));
}
static void swapextra (lua_State *L) {
  if (L->status == LUA_YIELD) {
    CallInfo *ci = L->ci;   
    StkId temp = ci->func;   
    ci->func = restorestack(L, ci->extra);
    ci->extra = savestack(L, temp);
  }
}
LUA_API int lua_sethook (lua_State *L, lua_Hook func, int mask, int count) {
  if (func == NULL || mask == 0) {   
    mask = 0;
    func = NULL;
  }
  if (isLua(L->ci))
    L->oldpc = L->ci->u.l.savedpc;
  L->hook = func;
  L->basehookcount = count;
  resethookcount(L);
  L->hookmask = cast_byte(mask);
  return 1;
}
LUA_API lua_Hook lua_gethook (lua_State *L) {
  return L->hook;
}
LUA_API int lua_gethookmask (lua_State *L) {
  return L->hookmask;
}
LUA_API int lua_gethookcount (lua_State *L) {
  return L->basehookcount;
}
LUA_API int lua_getstack (lua_State *L, int level, lua_Debug *ar) {
  int status;
  CallInfo *ci;
  if (level < 0) return 0;   
  lua_lock(L);
  for (ci = L->ci; level > 0 && ci != &L->base_ci; ci = ci->previous)
    level--;
  if (level == 0 && ci != &L->base_ci) {   
    status = 1;
    ar->i_ci = ci;
  }
  else status = 0;   
  lua_unlock(L);
  return status;
}
static const char *upvalname (Proto *p, int uv) {
  TString *s = check_exp(uv < p->sizeupvalues, p->upvalues[uv].name);
  if (s == NULL) return "?";
  else return getstr(s);
}
static const char *findvararg (CallInfo *ci, int n, StkId *pos) {
  int nparams = clLvalue(ci->func)->p->numparams;
  if (n >= ci->u.l.base - ci->func - nparams)
    return NULL;   
  else {
    *pos = ci->func + nparams + n;
    return "(*vararg)";   
  }
}
static const char *findlocal (lua_State *L, CallInfo *ci, int n,
                              StkId *pos) {
  const char *name = NULL;
  StkId base;
  if (isLua(ci)) {
    if (n < 0)   
      return findvararg(ci, -n, pos);
    else {
      base = ci->u.l.base;
      name = luaF_getlocalname(ci_func(ci)->p, n, currentpc(ci));
    }
  }
  else
    base = ci->func + 1;
  if (name == NULL) {   
    StkId limit = (ci == L->ci) ? L->top : ci->next->func;
    if (limit - base >= n && n > 0)   
      name = "(*temporary)";   
    else
      return NULL;   
  }
  *pos = base + (n - 1);
  return name;
}
LUA_API const char *lua_getlocal (lua_State *L, const lua_Debug *ar, int n) {
  const char *name;
  lua_lock(L);
  swapextra(L);
  if (ar == NULL) {   
    if (!isLfunction(L->top - 1))   
      name = NULL;
    else   
      name = luaF_getlocalname(clLvalue(L->top - 1)->p, n, 0);
  }
  else {   
    StkId pos = 0;   
    name = findlocal(L, ar->i_ci, n, &pos);
    if (name) {
      setobj2s(L, L->top, pos);
      api_incr_top(L);
    }
  }
  swapextra(L);
  lua_unlock(L);
  return name;
}
LUA_API const char *lua_setlocal (lua_State *L, const lua_Debug *ar, int n) {
  StkId pos = 0;   
  const char *name;
  lua_lock(L);
  swapextra(L);
  name = findlocal(L, ar->i_ci, n, &pos);
  if (name)
    setobjs2s(L, pos, L->top - 1);
  L->top--;   
  swapextra(L);
  lua_unlock(L);
  return name;
}
static void funcinfo (lua_Debug *ar, Closure *cl) {
  if (noLuaClosure(cl)) {
    ar->source = "=[C]";
    ar->linedefined = -1;
    ar->lastlinedefined = -1;
    ar->what = "C";
  }
  else {
    Proto *p = cl->l.p;
    ar->source = p->source ? getstr(p->source) : "=?";
    ar->linedefined = p->linedefined;
    ar->lastlinedefined = p->lastlinedefined;
    ar->what = (ar->linedefined == 0) ? "main" : "Lua";
  }
  luaO_chunkid(ar->short_src, ar->source, LUA_IDSIZE);
}
static void collectvalidlines (lua_State *L, Closure *f) {
  if (noLuaClosure(f)) {
    setnilvalue(L->top);
    api_incr_top(L);
  }
  else {
    int i;
    TValue v;
    int *lineinfo = f->l.p->lineinfo;
    Table *t = luaH_new(L);   
    sethvalue(L, L->top, t);   
    api_incr_top(L);
    setbvalue(&v, 1);   
    for (i = 0; i < f->l.p->sizelineinfo; i++)   
      luaH_setint(L, t, lineinfo[i], &v);   
  }
}
static int auxgetinfo (lua_State *L, const char *what, lua_Debug *ar,
                       Closure *f, CallInfo *ci) {
  int status = 1;
  for (; *what; what++) {
    switch (*what) {
      case 'S': {
        funcinfo(ar, f);
        break;
      }
      case 'l': {
        ar->currentline = (ci && isLua(ci)) ? currentline(ci) : -1;
        break;
      }
      case 'u': {
        ar->nups = (f == NULL) ? 0 : f->c.nupvalues;
        if (noLuaClosure(f)) {
          ar->isvararg = 1;
          ar->nparams = 0;
        }
        else {
          ar->isvararg = f->l.p->is_vararg;
          ar->nparams = f->l.p->numparams;
        }
        break;
      }
      case 't': {
        ar->istailcall = (ci) ? ci->callstatus & CIST_TAIL : 0;
        break;
      }
      case 'n': {
        if (ci && !(ci->callstatus & CIST_TAIL) && isLua(ci->previous))
          ar->namewhat = getfuncname(L, ci->previous, &ar->name);
        else
          ar->namewhat = NULL;
        if (ar->namewhat == NULL) {
          ar->namewhat = "";   
          ar->name = NULL;
        }
        break;
      }
      case 'L':
      case 'f':   
        break;
      default: status = 0;   
    }
  }
  return status;
}
LUA_API int lua_getinfo (lua_State *L, const char *what, lua_Debug *ar) {
  int status;
  Closure *cl;
  CallInfo *ci;
  StkId func;
  lua_lock(L);
  swapextra(L);
  if (*what == '>') {
    ci = NULL;
    func = L->top - 1;
    api_check(L, ttisfunction(func), "function expected");
    what++;   
    L->top--;   
  }
  else {
    ci = ar->i_ci;
    func = ci->func;
    lua_assert(ttisfunction(ci->func));
  }
  cl = ttisclosure(func) ? clvalue(func) : NULL;
  status = auxgetinfo(L, what, ar, cl, ci);
  if (strchr(what, 'f')) {
    setobjs2s(L, L->top, func);
    api_incr_top(L);
  }
  swapextra(L);
  if (strchr(what, 'L'))
    collectvalidlines(L, cl);
  lua_unlock(L);
  return status;
}
static const char *getobjname (Proto *p, int lastpc, int reg,
                               const char **name);
static void kname (Proto *p, int pc, int c, const char **name) {
  if (ISK(c)) {   
    TValue *kvalue = &p->k[INDEXK(c)];
    if (ttisstring(kvalue)) {   
      *name = svalue(kvalue);   
      return;
    }
  }
  else {   
    const char *what = getobjname(p, pc, c, name);  
    if (what && *what == 'c') {   
      return;   
    }
  }
  *name = "?";   
}
static int filterpc (int pc, int jmptarget) {
  if (pc < jmptarget)   
    return -1;   
  else return pc;   
}
static int findsetreg (Proto *p, int lastpc, int reg) {
  int pc;
  int setreg = -1;   
  int jmptarget = 0;   
  for (pc = 0; pc < lastpc; pc++) {
    Instruction i = p->code[pc];
    OpCode op = GET_OPCODE(i);
    int a = GETARG_A(i);
    switch (op) {
      case OP_LOADNIL: {
        int b = GETARG_B(i);
        if (a <= reg && reg <= a + b)   
          setreg = filterpc(pc, jmptarget);
        break;
      }
      case OP_TFORCALL: {
        if (reg >= a + 2)   
          setreg = filterpc(pc, jmptarget);
        break;
      }
      case OP_CALL:
      case OP_TAILCALL: {
        if (reg >= a)   
          setreg = filterpc(pc, jmptarget);
        break;
      }
      case OP_JMP: {
        int b = GETARG_sBx(i);
        int dest = pc + 1 + b;
        if (pc < dest && dest <= lastpc) {
          if (dest > jmptarget)
            jmptarget = dest;   
        }
        break;
      }
      case OP_TEST: {
        if (reg == a)   
          setreg = filterpc(pc, jmptarget);
        break;
      }
      default:
        if (testAMode(op) && reg == a)   
          setreg = filterpc(pc, jmptarget);
        break;
    }
  }
  return setreg;
}
static const char *getobjname (Proto *p, int lastpc, int reg,
                               const char **name) {
  int pc;
  *name = luaF_getlocalname(p, reg + 1, lastpc);
  if (*name)   
    return "local";
  pc = findsetreg(p, lastpc, reg);
  if (pc != -1) {   
    Instruction i = p->code[pc];
    OpCode op = GET_OPCODE(i);
    switch (op) {
      case OP_MOVE: {
        int b = GETARG_B(i);   
        if (b < GETARG_A(i))
          return getobjname(p, pc, b, name);   
        break;
      }
      case OP_GETTABUP:
      case OP_GETTABLE: {
        int k = GETARG_C(i);   
        int t = GETARG_B(i);   
        const char *vn = (op == OP_GETTABLE)   
                         ? luaF_getlocalname(p, t + 1, pc)
                         : upvalname(p, t);
        kname(p, pc, k, name);
        return (vn && strcmp(vn, LUA_ENV) == 0) ? "global" : "field";
      }
      case OP_GETUPVAL: {
        *name = upvalname(p, GETARG_B(i));
        return "upvalue";
      }
      case OP_LOADK:
      case OP_LOADKX: {
        int b = (op == OP_LOADK) ? GETARG_Bx(i)
                                 : GETARG_Ax(p->code[pc + 1]);
        if (ttisstring(&p->k[b])) {
          *name = svalue(&p->k[b]);
          return "constant";
        }
        break;
      }
      case OP_SELF: {
        int k = GETARG_C(i);   
        kname(p, pc, k, name);
        return "method";
      }
      default: break;   
    }
  }
  return NULL;   
}
static const char *getfuncname (lua_State *L, CallInfo *ci, const char **name) {
  TMS tm;
  Proto *p = ci_func(ci)->p;   
  int pc = currentpc(ci);   
  Instruction i = p->code[pc];   
  switch (GET_OPCODE(i)) {
    case OP_CALL:
    case OP_TAILCALL:   
      return getobjname(p, pc, GETARG_A(i), name);
    case OP_TFORCALL: {   
      *name = "for iterator";
       return "for iterator";
    }
    case OP_SELF:
    case OP_GETTABUP:
    case OP_GETTABLE: tm = TM_INDEX; break;
    case OP_SETTABUP:
    case OP_SETTABLE: tm = TM_NEWINDEX; break;
    case OP_EQ: tm = TM_EQ; break;
    case OP_ADD: tm = TM_ADD; break;
    case OP_SUB: tm = TM_SUB; break;
    case OP_MUL: tm = TM_MUL; break;
    case OP_DIV: tm = TM_DIV; break;
    case OP_MOD: tm = TM_MOD; break;
    case OP_POW: tm = TM_POW; break;
    case OP_UNM: tm = TM_UNM; break;
    case OP_LEN: tm = TM_LEN; break;
    case OP_LT: tm = TM_LT; break;
    case OP_LE: tm = TM_LE; break;
    case OP_CONCAT: tm = TM_CONCAT; break;
    default:
      return NULL;   
  }
  *name = getstr(G(L)->tmname[tm]);
  return "metamethod";
}
static int isinstack (CallInfo *ci, const TValue *o) {
  StkId p;
  for (p = ci->u.l.base; p < ci->top; p++)
    if (o == p) return 1;
  return 0;
}
static const char *getupvalname (CallInfo *ci, const TValue *o,
                                 const char **name) {
  LClosure *c = ci_func(ci);
  int i;
  for (i = 0; i < c->nupvalues; i++) {
    if (c->upvals[i]->v == o) {
      *name = upvalname(c->p, i);
      return "upvalue";
    }
  }
  return NULL;
}
l_noret luaG_typeerror (lua_State *L, const TValue *o, const char *op) {
  CallInfo *ci = L->ci;
  const char *name = NULL;
  const char *t = objtypename(o);
  const char *kind = NULL;
  if (isLua(ci)) {
    kind = getupvalname(ci, o, &name);   
    if (!kind && isinstack(ci, o))   
      kind = getobjname(ci_func(ci)->p, currentpc(ci),
                        cast_int(o - ci->u.l.base), &name);
  }
  if (kind)
    luaG_runerror(L, "attempt to %s %s " LUA_QS " (a %s value)",
                op, kind, name, t);
  else
    luaG_runerror(L, "attempt to %s a %s value", op, t);
}
l_noret luaG_concaterror (lua_State *L, StkId p1, StkId p2) {
  if (ttisstring(p1) || ttisnumber(p1)) p1 = p2;
  lua_assert(!ttisstring(p1) && !ttisnumber(p1));
  luaG_typeerror(L, p1, "concatenate");
}
l_noret luaG_aritherror (lua_State *L, const TValue *p1, const TValue *p2) {
  TValue temp;
  if (luaV_tonumber(p1, &temp) == NULL)
    p2 = p1;   
  luaG_typeerror(L, p2, "perform arithmetic on");
}
l_noret luaG_ordererror (lua_State *L, const TValue *p1, const TValue *p2) {
  const char *t1 = objtypename(p1);
  const char *t2 = objtypename(p2);
  if (t1 == t2)
    luaG_runerror(L, "attempt to compare two %s values", t1);
  else
    luaG_runerror(L, "attempt to compare %s with %s", t1, t2);
}
static void addinfo (lua_State *L, const char *msg) {
  CallInfo *ci = L->ci;
  if (isLua(ci)) {   
    char buff[LUA_IDSIZE];   
    int line = currentline(ci);
    TString *src = ci_func(ci)->p->source;
    if (src)
      luaO_chunkid(buff, getstr(src), LUA_IDSIZE);
    else {   
      buff[0] = '?'; buff[1] = '\0';
    }
    luaO_pushfstring(L, "%s:%d: %s", buff, line, msg);
  }
}
l_noret luaG_errormsg (lua_State *L) {
  if (L->errfunc != 0) {   
    StkId errfunc = restorestack(L, L->errfunc);
    if (!ttisfunction(errfunc)) luaD_throw(L, LUA_ERRERR);
    setobjs2s(L, L->top, L->top - 1);   
    setobjs2s(L, L->top - 1, errfunc);   
    L->top++;
    luaD_call(L, L->top - 2, 1, 0);   
  }
  luaD_throw(L, LUA_ERRRUN);
}
l_noret luaG_runerror (lua_State *L, const char *fmt, ...) {
  L->runerror++;
  va_list argp;
  va_start(argp, fmt);
  addinfo(L, luaO_pushvfstring(L, fmt, argp));
  va_end(argp);
  luaG_errormsg(L);
  L->runerror--;
}
