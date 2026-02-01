 

#define lobject_c
#define LUA_CORE

#include <sys/lua/lua.h>

#include "lctype.h"
#include "ldebug.h"
#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "lvm.h"



LUAI_DDEF const TValue luaO_nilobject_ = {NILCONSTANT};


 
int luaO_int2fb (unsigned int x) {
  int e = 0;   
  if (x < 8) return x;
  while (x >= 0x10) {
    x = (x+1) >> 1;
    e++;
  }
  return ((e+1) << 3) | (cast_int(x) - 8);
}


 
int luaO_fb2int (int x) {
  int e = (x >> 3) & 0x1f;
  if (e == 0) return x;
  else return ((x & 7) + 8) << (e - 1);
}


int luaO_ceillog2 (unsigned int x) {
  static const lu_byte log_2[256] = {
    0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
  };
  int l = 0;
  x--;
  while (x >= 256) { l += 8; x >>= 8; }
  return l + log_2[x];
}


lua_Number luaO_arith (int op, lua_Number v1, lua_Number v2) {
  switch (op) {
    case LUA_OPADD: return luai_numadd(NULL, v1, v2);
    case LUA_OPSUB: return luai_numsub(NULL, v1, v2);
    case LUA_OPMUL: return luai_nummul(NULL, v1, v2);
    case LUA_OPDIV: return luai_numdiv(NULL, v1, v2);
    case LUA_OPMOD: return luai_nummod(NULL, v1, v2);
    case LUA_OPPOW: return luai_numpow(NULL, v1, v2);
    case LUA_OPUNM: return luai_numunm(NULL, v1);
    default: lua_assert(0); return 0;
  }
}


int luaO_hexavalue (int c) {
  if (lisdigit(c)) return c - '0';
  else return ltolower(c) - 'a' + 10;
}


#if !defined(lua_strx2number)



static int isneg (const char **s) {
  if (**s == '-') { (*s)++; return 1; }
  else if (**s == '+') (*s)++;
  return 0;
}


static lua_Number readhexa (const char **s, lua_Number r, int *count) {
  for (; lisxdigit(cast_uchar(**s)); (*s)++) {   
    r = (r * cast_num(16.0)) + cast_num(luaO_hexavalue(cast_uchar(**s)));
    (*count)++;
  }
  return r;
}


 
static lua_Number lua_strx2number (const char *s, char **endptr) {
  lua_Number r = 0.0;
  int e = 0, i = 0;
  int neg = 0;   
  *endptr = cast(char *, s);   
  while (lisspace(cast_uchar(*s))) s++;   
  neg = isneg(&s);   
  if (!(*s == '0' && (*(s + 1) == 'x' || *(s + 1) == 'X')))   
    return 0.0;   
  s += 2;   
  r = readhexa(&s, r, &i);   
  if (*s == '.') {
    s++;   
    r = readhexa(&s, r, &e);   
  }
  if (i == 0 && e == 0)
    return 0.0;   
  e *= -4;   
  *endptr = cast(char *, s);   
  if (*s == 'p' || *s == 'P') {   
    int exp1 = 0;
    int neg1;
    s++;   
    neg1 = isneg(&s);   
    if (!lisdigit(cast_uchar(*s)))
      goto ret;   
    while (lisdigit(cast_uchar(*s)))   
      exp1 = exp1 * 10 + *(s++) - '0';
    if (neg1) exp1 = -exp1;
    e += exp1;
  }
  *endptr = cast(char *, s);   
 ret:
  if (neg) r = -r;
  return ((e >= 0) ? (r * (1ULL << e)) : (r / (1ULL << -e)));
}

#endif


int luaO_str2d (const char *s, size_t len, lua_Number *result) {
  char *endptr;
  if (strpbrk(s, "nN"))   
    return 0;
  else if (strpbrk(s, "xX"))   
    *result = lua_strx2number(s, &endptr);
  else
    *result = lua_str2number(s, &endptr);
  if (endptr == s) return 0;   
  while (lisspace(cast_uchar(*endptr))) endptr++;
  return (endptr == s + len);   
}



static void pushstr (lua_State *L, const char *str, size_t l) {
  setsvalue2s(L, L->top++, luaS_newlstr(L, str, l));
}


 
const char *luaO_pushvfstring (lua_State *L, const char *fmt, va_list argp) {
  int n = 0;
  for (;;) {
    const char *e = strchr(fmt, '%');
    if (e == NULL) break;
    luaD_checkstack(L, 2);   
    pushstr(L, fmt, e - fmt);
    switch (*(e+1)) {
      case 's': {
        const char *s = va_arg(argp, char *);
        if (s == NULL) s = "(null)";
        pushstr(L, s, strlen(s));
        break;
      }
      case 'c': {
        char buff;
        buff = cast(char, va_arg(argp, int));
        pushstr(L, &buff, 1);
        break;
      }
      case 'd': {
        setnvalue(L->top++, cast_num(va_arg(argp, int)));
        break;
      }
      case 'f': {
        setnvalue(L->top++, cast_num(va_arg(argp, l_uacNumber)));
        break;
      }
      case 'p': {
        char buff[4*sizeof(void *) + 8];  
        int l = lcompat_sprintf(buff, sizeof(buff), "%p", va_arg(argp, void *));
        pushstr(L, buff, l);
        break;
      }
      case '%': {
        pushstr(L, "%", 1);
        break;
      }
      default: {
        luaG_runerror(L,
            "invalid option " LUA_QL("%%%c") " to " LUA_QL("lua_pushfstring"),
            *(e + 1));
      }
    }
    n += 2;
    fmt = e+2;
  }
  luaD_checkstack(L, 1);
  pushstr(L, fmt, strlen(fmt));
  if (n > 0) luaV_concat(L, n + 1);
  return svalue(L->top - 1);
}


const char *luaO_pushfstring (lua_State *L, const char *fmt, ...) {
  const char *msg;
  va_list argp;
  va_start(argp, fmt);
  msg = luaO_pushvfstring(L, fmt, argp);
  va_end(argp);
  return msg;
}


 
#define LL(x)	(sizeof(x)/sizeof(char) - 1)

#define RETS	"..."
#define PRE	"[string \""
#define POS	"\"]"

#define addstr(a,b,l)	( memcpy(a,b,(l) * sizeof(char)), a += (l) )

void luaO_chunkid (char *out, const char *source, size_t bufflen) {
  size_t l = strlen(source);
  if (*source == '=') {   
    if (l <= bufflen)   
      memcpy(out, source + 1, l * sizeof(char));
    else {   
      addstr(out, source + 1, bufflen - 1);
      *out = '\0';
    }
  }
  else if (*source == '@') {   
    if (l <= bufflen)   
      memcpy(out, source + 1, l * sizeof(char));
    else {   
      addstr(out, RETS, LL(RETS));
      bufflen -= LL(RETS);
      memcpy(out, source + 1 + l - bufflen, bufflen * sizeof(char));
    }
  }
  else {   
    const char *nl = strchr(source, '\n');   
    addstr(out, PRE, LL(PRE));   
    bufflen -= LL(PRE RETS POS) + 1;   
    if (l < bufflen && nl == NULL) {   
      addstr(out, source, l);   
    }
    else {
      if (nl != NULL) l = nl - source;   
      if (l > bufflen) l = bufflen;
      addstr(out, source, l);
      addstr(out, RETS, LL(RETS));
    }
    memcpy(out, POS, (LL(POS) + 1) * sizeof(char));
  }
}
