 


#define lvm_c
#define LUA_CORE

#include <sys/lua/lua.h>

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lvm.h"

#ifdef _KERNEL
#define	strcoll(l,r) (strcmp((l),(r)))
#endif

 
#define MAXTAGLOOP	100


const TValue *luaV_tonumber (const TValue *obj, TValue *n) {
  lua_Number num;
  if (ttisnumber(obj)) return obj;
  if (ttisstring(obj) && luaO_str2d(svalue(obj), tsvalue(obj)->len, &num)) {
    setnvalue(n, num);
    return n;
  }
  else
    return NULL;
}


int luaV_tostring (lua_State *L, StkId obj) {
  if (!ttisnumber(obj))
    return 0;
  else {
    char s[LUAI_MAXNUMBER2STR];
    lua_Number n = nvalue(obj);
    int l = lua_number2str(s, n);
    setsvalue2s(L, obj, luaS_newlstr(L, s, l));
    return 1;
  }
}


static void traceexec (lua_State *L) {
  CallInfo *ci = L->ci;
  lu_byte mask = L->hookmask;
  int counthook = ((mask & LUA_MASKCOUNT) && L->hookcount == 0);
  if (counthook)
    resethookcount(L);   
  if (ci->callstatus & CIST_HOOKYIELD) {   
    ci->callstatus &= ~CIST_HOOKYIELD;   
    return;   
  }
  if (counthook)
    luaD_hook(L, LUA_HOOKCOUNT, -1);   
  if (mask & LUA_MASKLINE) {
    Proto *p = ci_func(ci)->p;
    int npc = pcRel(ci->u.l.savedpc, p);
    int newline = getfuncline(p, npc);
    if (npc == 0 ||   
        ci->u.l.savedpc <= L->oldpc ||   
        newline != getfuncline(p, pcRel(L->oldpc, p)))   
      luaD_hook(L, LUA_HOOKLINE, newline);   
  }
  L->oldpc = ci->u.l.savedpc;
  if (L->status == LUA_YIELD) {   
    if (counthook)
      L->hookcount = 1;   
    ci->u.l.savedpc--;   
    ci->callstatus |= CIST_HOOKYIELD;   
    ci->func = L->top - 1;   
    luaD_throw(L, LUA_YIELD);
  }
}


static void callTM (lua_State *L, const TValue *f, const TValue *p1,
                    const TValue *p2, TValue *p3, int hasres) {
  if (L == NULL) return;

  ptrdiff_t result = savestack(L, p3);
  setobj2s(L, L->top++, f);   
  setobj2s(L, L->top++, p1);   
  setobj2s(L, L->top++, p2);   
  if (!hasres)   
    setobj2s(L, L->top++, p3);   
   
  luaD_call(L, L->top - (4 - hasres), hasres, isLua(L->ci));
  if (hasres) {   
    p3 = restorestack(L, result);
    setobjs2s(L, p3, --L->top);
  }
}


void luaV_gettable (lua_State *L, const TValue *t, TValue *key, StkId val) {
  int loop;
  for (loop = 0; loop < MAXTAGLOOP; loop++) {
    const TValue *tm;
    if (ttistable(t)) {   
      Table *h = hvalue(t);
      const TValue *res = luaH_get(h, key);  
      if (!ttisnil(res) ||   
          (tm = fasttm(L, h->metatable, TM_INDEX)) == NULL) {  
        setobj2s(L, val, res);
        return;
      }
       
    }
    else if (ttisnil(tm = luaT_gettmbyobj(L, t, TM_INDEX)))
      luaG_typeerror(L, t, "index");
    if (ttisfunction(tm)) {
      callTM(L, tm, t, key, val, 1);
      return;
    }
    t = tm;   
  }
  luaG_runerror(L, "loop in gettable");
}


void luaV_settable (lua_State *L, const TValue *t, TValue *key, StkId val) {
  int loop;
  for (loop = 0; loop < MAXTAGLOOP; loop++) {
    const TValue *tm;
    if (ttistable(t)) {   
      Table *h = hvalue(t);
      TValue *oldval = cast(TValue *, luaH_get(h, key));
       
      if (!ttisnil(oldval) ||
          
         ((tm = fasttm(L, h->metatable, TM_NEWINDEX)) == NULL &&
          
         (oldval != luaO_nilobject ||
          
         (oldval = luaH_newkey(L, h, key), 1)))) {
         
        setobj2t(L, oldval, val);   
        invalidateTMcache(h);
        luaC_barrierback(L, obj2gco(h), val);
        return;
      }
       
    }
    else   
      if (ttisnil(tm = luaT_gettmbyobj(L, t, TM_NEWINDEX)))
        luaG_typeerror(L, t, "index");
     
    if (ttisfunction(tm)) {
      callTM(L, tm, t, key, val, 0);
      return;
    }
    t = tm;   
  }
  luaG_runerror(L, "loop in settable");
}


static int call_binTM (lua_State *L, const TValue *p1, const TValue *p2,
                       StkId res, TMS event) {
  const TValue *tm = luaT_gettmbyobj(L, p1, event);   
  if (ttisnil(tm))
    tm = luaT_gettmbyobj(L, p2, event);   
  if (ttisnil(tm)) return 0;
  callTM(L, tm, p1, p2, res, 1);
  return 1;
}


static const TValue *get_equalTM (lua_State *L, Table *mt1, Table *mt2,
                                  TMS event) {
  const TValue *tm1 = fasttm(L, mt1, event);
  const TValue *tm2;
  if (tm1 == NULL) return NULL;   
  if (mt1 == mt2) return tm1;   
  tm2 = fasttm(L, mt2, event);
  if (tm2 == NULL) return NULL;   
  if (luaV_rawequalobj(tm1, tm2))   
    return tm1;
  return NULL;
}


static int call_orderTM (lua_State *L, const TValue *p1, const TValue *p2,
                         TMS event) {
  if (!call_binTM(L, p1, p2, L->top, event))
    return -1;   
  else
    return !l_isfalse(L->top);
}


static int l_strcmp (const TString *ls, const TString *rs) {
  const char *l = getstr(ls);
  size_t ll = ls->tsv.len;
  const char *r = getstr(rs);
  size_t lr = rs->tsv.len;
  for (;;) {
    int temp = strcoll(l, r);
    if (temp != 0) return temp;
    else {   
      size_t len = strlen(l);   
      if (len == lr)   
        return (len == ll) ? 0 : 1;
      else if (len == ll)   
        return -1;   
       
      len++;
      l += len; ll -= len; r += len; lr -= len;
    }
  }
}


int luaV_lessthan (lua_State *L, const TValue *l, const TValue *r) {
  int res;
  if (ttisnumber(l) && ttisnumber(r))
    return luai_numlt(L, nvalue(l), nvalue(r));
  else if (ttisstring(l) && ttisstring(r))
    return l_strcmp(rawtsvalue(l), rawtsvalue(r)) < 0;
  else if ((res = call_orderTM(L, l, r, TM_LT)) < 0)
    luaG_ordererror(L, l, r);
  return res;
}


int luaV_lessequal (lua_State *L, const TValue *l, const TValue *r) {
  int res;
  if (ttisnumber(l) && ttisnumber(r))
    return luai_numle(L, nvalue(l), nvalue(r));
  else if (ttisstring(l) && ttisstring(r))
    return l_strcmp(rawtsvalue(l), rawtsvalue(r)) <= 0;
  else if ((res = call_orderTM(L, l, r, TM_LE)) >= 0)   
    return res;
  else if ((res = call_orderTM(L, r, l, TM_LT)) < 0)   
    luaG_ordererror(L, l, r);
  return !res;
}


 
int luaV_equalobj_ (lua_State *L, const TValue *t1, const TValue *t2) {
  const TValue *tm;
  lua_assert(ttisequal(t1, t2));
  switch (ttype(t1)) {
    case LUA_TNIL: return 1;
    case LUA_TNUMBER: return luai_numeq(nvalue(t1), nvalue(t2));
    case LUA_TBOOLEAN: return bvalue(t1) == bvalue(t2);   
    case LUA_TLIGHTUSERDATA: return pvalue(t1) == pvalue(t2);
    case LUA_TLCF: return fvalue(t1) == fvalue(t2);
    case LUA_TSHRSTR: return eqshrstr(rawtsvalue(t1), rawtsvalue(t2));
    case LUA_TLNGSTR: return luaS_eqlngstr(rawtsvalue(t1), rawtsvalue(t2));
    case LUA_TUSERDATA: {
      if (uvalue(t1) == uvalue(t2)) return 1;
      else if (L == NULL) return 0;
      tm = get_equalTM(L, uvalue(t1)->metatable, uvalue(t2)->metatable, TM_EQ);
      break;   
    }
    case LUA_TTABLE: {
      if (hvalue(t1) == hvalue(t2)) return 1;
      else if (L == NULL) return 0;
      tm = get_equalTM(L, hvalue(t1)->metatable, hvalue(t2)->metatable, TM_EQ);
      break;   
    }
    default:
      lua_assert(iscollectable(t1));
      return gcvalue(t1) == gcvalue(t2);
  }
  if (tm == NULL || L == NULL) return 0;   
  callTM(L, tm, t1, t2, L->top, 1);   
  return !l_isfalse(L->top);
}


void luaV_concat (lua_State *L, int total) {
  lua_assert(total >= 2);
  do {
    StkId top = L->top;
    int n = 2;   
    if (!(ttisstring(top-2) || ttisnumber(top-2)) || !tostring(L, top-1)) {
      if (!call_binTM(L, top-2, top-1, top-2, TM_CONCAT))
        luaG_concaterror(L, top-2, top-1);
    }
    else if (tsvalue(top-1)->len == 0)   
      (void)tostring(L, top - 2);   
    else if (ttisstring(top-2) && tsvalue(top-2)->len == 0) {
      setobjs2s(L, top - 2, top - 1);   
    }
    else {
       
      size_t tl = tsvalue(top-1)->len;
      char *buffer;
      int i;
       
      for (i = 1; i < total && tostring(L, top-i-1); i++) {
        size_t l = tsvalue(top-i-1)->len;
        if (l >= (MAX_SIZET/sizeof(char)) - tl)
          luaG_runerror(L, "string length overflow");
        tl += l;
      }
      buffer = luaZ_openspace(L, &G(L)->buff, tl);
      tl = 0;
      n = i;
      do {   
        size_t l = tsvalue(top-i)->len;
        memcpy(buffer+tl, svalue(top-i), l * sizeof(char));
        tl += l;
      } while (--i > 0);
      setsvalue2s(L, top-n, luaS_newlstr(L, buffer, tl));
    }
    total -= n-1;   
    L->top -= n-1;   
  } while (total > 1);   
}


void luaV_objlen (lua_State *L, StkId ra, const TValue *rb) {
  const TValue *tm;
  switch (ttypenv(rb)) {
    case LUA_TTABLE: {
      Table *h = hvalue(rb);
      tm = fasttm(L, h->metatable, TM_LEN);
      if (tm) break;   
      setnvalue(ra, cast_num(luaH_getn(h)));   
      return;
    }
    case LUA_TSTRING: {
      setnvalue(ra, cast_num(tsvalue(rb)->len));
      return;
    }
    default: {   
      tm = luaT_gettmbyobj(L, rb, TM_LEN);
      if (ttisnil(tm))   
        luaG_typeerror(L, rb, "get length of");
      break;
    }
  }
  callTM(L, tm, rb, rb, ra, 1);
}

 

 
static lua_Number luaV_div (lua_State *L, lua_Number m, lua_Number n) {
  if ((lua_Unsigned)(n) + 1u <= 1u) {   
    if (n == 0)
      luaG_runerror(L, "attempt to divide by zero");
    return (0 - m);    
    if ((m ^ n) < 0 && m % n != 0)   
      q -= 1;   
    return q;
  }
}


 
static lua_Number luaV_mod (lua_State *L, lua_Number m, lua_Number n) {
  if ((lua_Unsigned)(n) + 1u <= 1u) {   
    if (n == 0)
      luaG_runerror(L, "attempt to perform 'n%%0'");
    return 0;    
  }
  else {
    lua_Number r = m % n;
    if (r != 0 && (m ^ n) < 0)   
      r += n;   
    return r;
  }
}

 

void luaV_arith (lua_State *L, StkId ra, const TValue *rb,
                 const TValue *rc, TMS op) {
  TValue tempb, tempc;
  const TValue *b, *c;
  if ((b = luaV_tonumber(rb, &tempb)) != NULL &&
      (c = luaV_tonumber(rc, &tempc)) != NULL) {
     
    lua_Number res;
    int lop = op - TM_ADD + LUA_OPADD;
    if (lop == LUA_OPDIV) {
      res = luaV_div(L, nvalue(b), nvalue(c));
    } else if (lop == LUA_OPMOD) {
      res = luaV_mod(L, nvalue(b), nvalue(c));
    } else {
      res = luaO_arith(op - TM_ADD + LUA_OPADD, nvalue(b), nvalue(c));
    }
    setnvalue(ra, res);
  }
  else if (!call_binTM(L, rb, rc, ra, op))
    luaG_aritherror(L, rb, rc);
}


 
static Closure *getcached (Proto *p, UpVal **encup, StkId base) {
  Closure *c = p->cache;
  if (c != NULL) {   
    int nup = p->sizeupvalues;
    Upvaldesc *uv = p->upvalues;
    int i;
    for (i = 0; i < nup; i++) {   
      TValue *v = uv[i].instack ? base + uv[i].idx : encup[uv[i].idx]->v;
      if (c->l.upvals[i]->v != v)
        return NULL;   
    }
  }
  return c;   
}


 
static void pushclosure (lua_State *L, Proto *p, UpVal **encup, StkId base,
                         StkId ra) {
  int nup = p->sizeupvalues;
  Upvaldesc *uv = p->upvalues;
  int i;
  Closure *ncl = luaF_newLclosure(L, nup);
  ncl->l.p = p;
  setclLvalue(L, ra, ncl);   
  for (i = 0; i < nup; i++) {   
    if (uv[i].instack)   
      ncl->l.upvals[i] = luaF_findupval(L, base + uv[i].idx);
    else   
      ncl->l.upvals[i] = encup[uv[i].idx];
  }
  luaC_barrierproto(L, p, ncl);
  p->cache = ncl;   
}


 
void luaV_finishOp (lua_State *L) {
  CallInfo *ci = L->ci;
  StkId base = ci->u.l.base;
  Instruction inst = *(ci->u.l.savedpc - 1);   
  OpCode op = GET_OPCODE(inst);
  switch (op) {   
    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
    case OP_MOD: case OP_POW: case OP_UNM: case OP_LEN:
    case OP_GETTABUP: case OP_GETTABLE: case OP_SELF: {
      setobjs2s(L, base + GETARG_A(inst), --L->top);
      break;
    }
    case OP_LE: case OP_LT: case OP_EQ: {
      int res = !l_isfalse(L->top - 1);
      L->top--;
       
      lua_assert(!ISK(GETARG_B(inst)));
      if (op == OP_LE &&   
          ttisnil(luaT_gettmbyobj(L, base + GETARG_B(inst), TM_LE)))
        res = !res;   
      lua_assert(GET_OPCODE(*ci->u.l.savedpc) == OP_JMP);
      if (res != GETARG_A(inst))   
        ci->u.l.savedpc++;   
      break;
    }
    case OP_CONCAT: {
      StkId top = L->top - 1;   
      int b = GETARG_B(inst);       
      int total = cast_int(top - 1 - (base + b));   
      setobj2s(L, top - 2, top);   
      if (total > 1) {   
        L->top = top - 1;   
        luaV_concat(L, total);   
      }
       
      setobj2s(L, ci->u.l.base + GETARG_A(inst), L->top - 1);
      L->top = ci->top;   
      break;
    }
    case OP_TFORCALL: {
      lua_assert(GET_OPCODE(*ci->u.l.savedpc) == OP_TFORLOOP);
      L->top = ci->top;   
      break;
    }
    case OP_CALL: {
      if (GETARG_C(inst) - 1 >= 0)   
        L->top = ci->top;   
      break;
    }
    case OP_TAILCALL: case OP_SETTABUP: case OP_SETTABLE:
      break;
    default: lua_assert(0);
  }
}



 

#if !defined luai_runtimecheck
#define luai_runtimecheck(L, c)		 
#endif


#define RA(i)	(base+GETARG_A(i))
 
#define RB(i)	check_exp(getBMode(GET_OPCODE(i)) == OpArgR, base+GETARG_B(i))
#define RC(i)	check_exp(getCMode(GET_OPCODE(i)) == OpArgR, base+GETARG_C(i))
#define RKB(i)	check_exp(getBMode(GET_OPCODE(i)) == OpArgK, \
	ISK(GETARG_B(i)) ? k+INDEXK(GETARG_B(i)) : base+GETARG_B(i))
#define RKC(i)	check_exp(getCMode(GET_OPCODE(i)) == OpArgK, \
	ISK(GETARG_C(i)) ? k+INDEXK(GETARG_C(i)) : base+GETARG_C(i))
#define KBx(i)  \
  (k + (GETARG_Bx(i) != 0 ? GETARG_Bx(i) - 1 : GETARG_Ax(*ci->u.l.savedpc++)))


 
#define dojump(ci,i,e) \
  { int a = GETARG_A(i); \
    if (a > 0) luaF_close(L, ci->u.l.base + a - 1); \
    ci->u.l.savedpc += GETARG_sBx(i) + e; }

 
#define donextjump(ci)	{ i = *ci->u.l.savedpc; dojump(ci, i, 1); }


#define Protect(x)	{ {x;} base = ci->u.l.base; }

#define checkGC(L,c)  \
  Protect( luaC_condGC(L,{L->top = (c);    \
                          luaC_step(L); \
                          L->top = ci->top;})    \
           luai_threadyield(L); )


#define arith_op(op,tm) { \
        TValue *rb = RKB(i); \
        TValue *rc = RKC(i); \
        if (ttisnumber(rb) && ttisnumber(rc)) { \
          lua_Number nb = nvalue(rb), nc = nvalue(rc); \
          setnvalue(ra, op(L, nb, nc)); \
        } \
        else { Protect(luaV_arith(L, ra, rb, rc, tm)); } }


#define vmdispatch(o)	switch(o)
#define vmcase(l,b)	case l: {b}  break;
#define vmcasenb(l,b)	case l: {b}		 

void luaV_execute (lua_State *L) {
  CallInfo *ci = L->ci;
  LClosure *cl;
  TValue *k;
  StkId base;
 newframe:   
  lua_assert(ci == L->ci);
  cl = clLvalue(ci->func);
  k = cl->p->k;
  base = ci->u.l.base;
   
  for (;;) {
    Instruction i = *(ci->u.l.savedpc++);
    StkId ra;
    if ((L->hookmask & (LUA_MASKLINE | LUA_MASKCOUNT)) &&
        (--L->hookcount == 0 || L->hookmask & LUA_MASKLINE)) {
      Protect(traceexec(L));
    }
     
    ra = RA(i);
    lua_assert(base == ci->u.l.base);
    lua_assert(base <= L->top && L->top < L->stack + L->stacksize);
    vmdispatch (GET_OPCODE(i)) {
      vmcase(OP_MOVE,
        setobjs2s(L, ra, RB(i));
      )
      vmcase(OP_LOADK,
        TValue *rb = k + GETARG_Bx(i);
        setobj2s(L, ra, rb);
      )
      vmcase(OP_LOADKX,
        TValue *rb;
        lua_assert(GET_OPCODE(*ci->u.l.savedpc) == OP_EXTRAARG);
        rb = k + GETARG_Ax(*ci->u.l.savedpc++);
        setobj2s(L, ra, rb);
      )
      vmcase(OP_LOADBOOL,
        setbvalue(ra, GETARG_B(i));
        if (GETARG_C(i)) ci->u.l.savedpc++;   
      )
      vmcase(OP_LOADNIL,
        int b = GETARG_B(i);
        do {
          setnilvalue(ra++);
        } while (b--);
      )
      vmcase(OP_GETUPVAL,
        int b = GETARG_B(i);
        setobj2s(L, ra, cl->upvals[b]->v);
      )
      vmcase(OP_GETTABUP,
        int b = GETARG_B(i);
        Protect(luaV_gettable(L, cl->upvals[b]->v, RKC(i), ra));
      )
      vmcase(OP_GETTABLE,
        Protect(luaV_gettable(L, RB(i), RKC(i), ra));
      )
      vmcase(OP_SETTABUP,
        int a = GETARG_A(i);
        Protect(luaV_settable(L, cl->upvals[a]->v, RKB(i), RKC(i)));
      )
      vmcase(OP_SETUPVAL,
        UpVal *uv = cl->upvals[GETARG_B(i)];
        setobj(L, uv->v, ra);
        luaC_barrier(L, uv, ra);
      )
      vmcase(OP_SETTABLE,
        Protect(luaV_settable(L, ra, RKB(i), RKC(i)));
      )
      vmcase(OP_NEWTABLE,
        int b = GETARG_B(i);
        int c = GETARG_C(i);
        Table *t = luaH_new(L);
        sethvalue(L, ra, t);
        if (b != 0 || c != 0)
          luaH_resize(L, t, luaO_fb2int(b), luaO_fb2int(c));
        checkGC(L, ra + 1);
      )
      vmcase(OP_SELF,
        StkId rb = RB(i);
        setobjs2s(L, ra+1, rb);
        Protect(luaV_gettable(L, rb, RKC(i), ra));
      )
      vmcase(OP_ADD,
        arith_op(luai_numadd, TM_ADD);
      )
      vmcase(OP_SUB,
        arith_op(luai_numsub, TM_SUB);
      )
      vmcase(OP_MUL,
        arith_op(luai_nummul, TM_MUL);
      )
       
      vmcase(OP_DIV,
        arith_op(luaV_div, TM_DIV);
      )
      vmcase(OP_MOD,
        arith_op(luaV_mod, TM_MOD);
      )
      vmcase(OP_POW,
        arith_op(luai_numpow, TM_POW);
      )
      vmcase(OP_UNM,
        TValue *rb = RB(i);
        if (ttisnumber(rb)) {
          lua_Number nb = nvalue(rb);
          setnvalue(ra, luai_numunm(L, nb));
        }
        else {
          Protect(luaV_arith(L, ra, rb, rb, TM_UNM));
        }
      )
      vmcase(OP_NOT,
        TValue *rb = RB(i);
        int res = l_isfalse(rb);   
        setbvalue(ra, res);
      )
      vmcase(OP_LEN,
        Protect(luaV_objlen(L, ra, RB(i)));
      )
      vmcase(OP_CONCAT,
        int b = GETARG_B(i);
        int c = GETARG_C(i);
        StkId rb;
        L->top = base + c + 1;   
        Protect(luaV_concat(L, c - b + 1));
        ra = RA(i);   
        rb = b + base;
        setobjs2s(L, ra, rb);
        checkGC(L, (ra >= rb ? ra + 1 : rb));
        L->top = ci->top;   
      )
      vmcase(OP_JMP,
        dojump(ci, i, 0);
      )
      vmcase(OP_EQ,
        TValue *rb = RKB(i);
        TValue *rc = RKC(i);
        Protect(
          if (cast_int(equalobj(L, rb, rc)) != GETARG_A(i))
            ci->u.l.savedpc++;
          else
            donextjump(ci);
        )
      )
      vmcase(OP_LT,
        Protect(
          if (luaV_lessthan(L, RKB(i), RKC(i)) != GETARG_A(i))
            ci->u.l.savedpc++;
          else
            donextjump(ci);
        )
      )
      vmcase(OP_LE,
        Protect(
          if (luaV_lessequal(L, RKB(i), RKC(i)) != GETARG_A(i))
            ci->u.l.savedpc++;
          else
            donextjump(ci);
        )
      )
      vmcase(OP_TEST,
        if (GETARG_C(i) ? l_isfalse(ra) : !l_isfalse(ra))
            ci->u.l.savedpc++;
          else
          donextjump(ci);
      )
      vmcase(OP_TESTSET,
        TValue *rb = RB(i);
        if (GETARG_C(i) ? l_isfalse(rb) : !l_isfalse(rb))
          ci->u.l.savedpc++;
        else {
          setobjs2s(L, ra, rb);
          donextjump(ci);
        }
      )
      vmcase(OP_CALL,
        int b = GETARG_B(i);
        int nresults = GETARG_C(i) - 1;
        if (b != 0) L->top = ra+b;   
        if (luaD_precall(L, ra, nresults)) {   
          if (nresults >= 0) L->top = ci->top;   
          base = ci->u.l.base;
        }
        else {   
          ci = L->ci;
          ci->callstatus |= CIST_REENTRY;
          goto newframe;   
        }
      )
      vmcase(OP_TAILCALL,
        int b = GETARG_B(i);
        if (b != 0) L->top = ra+b;   
        lua_assert(GETARG_C(i) - 1 == LUA_MULTRET);
        if (luaD_precall(L, ra, LUA_MULTRET))   
          base = ci->u.l.base;
        else {
           
          CallInfo *nci = L->ci;   
          CallInfo *oci = nci->previous;   
          StkId nfunc = nci->func;   
          StkId ofunc = oci->func;   
           
          StkId lim = nci->u.l.base + getproto(nfunc)->numparams;
          int aux;
           
          if (cl->p->sizep > 0) luaF_close(L, oci->u.l.base);
           
          for (aux = 0; nfunc + aux < lim; aux++)
            setobjs2s(L, ofunc + aux, nfunc + aux);
          oci->u.l.base = ofunc + (nci->u.l.base - nfunc);   
          oci->top = L->top = ofunc + (L->top - nfunc);   
          oci->u.l.savedpc = nci->u.l.savedpc;
          oci->callstatus |= CIST_TAIL;   
          ci = L->ci = oci;   
          lua_assert(L->top == oci->u.l.base + getproto(ofunc)->maxstacksize);
          goto newframe;   
        }
      )
      vmcasenb(OP_RETURN,
        int b = GETARG_B(i);
        if (b != 0) L->top = ra+b-1;
        if (cl->p->sizep > 0) luaF_close(L, base);
        b = luaD_poscall(L, ra);
        if (!(ci->callstatus & CIST_REENTRY))   
          return;   
        else {   
          ci = L->ci;
          if (b) L->top = ci->top;
          lua_assert(isLua(ci));
          lua_assert(GET_OPCODE(*((ci)->u.l.savedpc - 1)) == OP_CALL);
          goto newframe;   
        }
      )
      vmcase(OP_FORLOOP,
        lua_Number step = nvalue(ra+2);
        lua_Number idx = luai_numadd(L, nvalue(ra), step);  
        lua_Number limit = nvalue(ra+1);
        if (luai_numlt(L, 0, step) ? luai_numle(L, idx, limit)
                                   : luai_numle(L, limit, idx)) {
          ci->u.l.savedpc += GETARG_sBx(i);   
          setnvalue(ra, idx);   
          setnvalue(ra+3, idx);   
        }
      )
      vmcase(OP_FORPREP,
        const TValue *init = ra;
        const TValue *plimit = ra+1;
        const TValue *pstep = ra+2;
        if (!tonumber(init, ra))
          luaG_runerror(L, LUA_QL("for") " initial value must be a number");
        else if (!tonumber(plimit, ra+1))
          luaG_runerror(L, LUA_QL("for") " limit must be a number");
        else if (!tonumber(pstep, ra+2))
          luaG_runerror(L, LUA_QL("for") " step must be a number");
        setnvalue(ra, luai_numsub(L, nvalue(ra), nvalue(pstep)));
        ci->u.l.savedpc += GETARG_sBx(i);
      )
      vmcasenb(OP_TFORCALL,
        StkId cb = ra + 3;   
        setobjs2s(L, cb+2, ra+2);
        setobjs2s(L, cb+1, ra+1);
        setobjs2s(L, cb, ra);
        L->top = cb + 3;   
        Protect(luaD_call(L, cb, GETARG_C(i), 1));
        L->top = ci->top;
        i = *(ci->u.l.savedpc++);   
        ra = RA(i);
        lua_assert(GET_OPCODE(i) == OP_TFORLOOP);
        goto l_tforloop;
      )
      vmcase(OP_TFORLOOP,
        l_tforloop:
        if (!ttisnil(ra + 1)) {   
          setobjs2s(L, ra, ra + 1);   
           ci->u.l.savedpc += GETARG_sBx(i);   
        }
      )
      vmcase(OP_SETLIST,
        int n = GETARG_B(i);
        int c = GETARG_C(i);
        int last;
        Table *h;
        if (n == 0) n = cast_int(L->top - ra) - 1;
        if (c == 0) {
          lua_assert(GET_OPCODE(*ci->u.l.savedpc) == OP_EXTRAARG);
          c = GETARG_Ax(*ci->u.l.savedpc++);
        }
        luai_runtimecheck(L, ttistable(ra));
        h = hvalue(ra);
        last = ((c-1)*LFIELDS_PER_FLUSH) + n;
        if (last > h->sizearray)   
          luaH_resizearray(L, h, last);   
        for (; n > 0; n--) {
          TValue *val = ra+n;
          luaH_setint(L, h, last--, val);
          luaC_barrierback(L, obj2gco(h), val);
        }
        L->top = ci->top;   
      )
      vmcase(OP_CLOSURE,
        Proto *p = cl->p->p[GETARG_Bx(i)];
        Closure *ncl = getcached(p, cl->upvals, base);   
        if (ncl == NULL)   
          pushclosure(L, p, cl->upvals, base, ra);   
        else
          setclLvalue(L, ra, ncl);   
        checkGC(L, ra + 1);
      )
      vmcase(OP_VARARG,
        int b = GETARG_B(i) - 1;
        int j;
        int n = cast_int(base - ci->func) - cl->p->numparams - 1;
        if (b < 0) {   
          b = n;   
          Protect(luaD_checkstack(L, n));
          ra = RA(i);   
          L->top = ra + n;
        }
        for (j = 0; j < b; j++) {
          if (j < n) {
            setobjs2s(L, ra + j, base - n + j);
          }
          else {
            setnilvalue(ra + j);
          }
        }
      )
      vmcase(OP_EXTRAARG,
        lua_assert(0);
      )
    }
  }
}
