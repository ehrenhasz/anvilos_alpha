#define lauxlib_c
#define LUA_LIB
#include <sys/lua/lua.h>
#include <sys/lua/lauxlib.h>
#define LEVELS1	12	 
#define LEVELS2	10	 
static int findfield (lua_State *L, int objidx, int level) {
  if (level == 0 || !lua_istable(L, -1))
    return 0;   
  lua_pushnil(L);   
  while (lua_next(L, -2)) {   
    if (lua_type(L, -2) == LUA_TSTRING) {   
      if (lua_rawequal(L, objidx, -1)) {   
        lua_pop(L, 1);   
        return 1;
      }
      else if (findfield(L, objidx, level - 1)) {   
        lua_remove(L, -2);   
        lua_pushliteral(L, ".");
        lua_insert(L, -2);   
        lua_concat(L, 3);
        return 1;
      }
    }
    lua_pop(L, 1);   
  }
  return 0;   
}
static int pushglobalfuncname (lua_State *L, lua_Debug *ar) {
  int top = lua_gettop(L);
  lua_getinfo(L, "f", ar);   
  lua_pushglobaltable(L);
  if (findfield(L, top + 1, 2)) {
    lua_copy(L, -1, top + 1);   
    lua_pop(L, 2);   
    return 1;
  }
  else {
    lua_settop(L, top);   
    return 0;
  }
}
static void pushfuncname (lua_State *L, lua_Debug *ar) {
  if (*ar->namewhat != '\0')   
    lua_pushfstring(L, "function " LUA_QS, ar->name);
  else if (*ar->what == 'm')   
      lua_pushliteral(L, "main chunk");
  else if (*ar->what == 'C') {
    if (pushglobalfuncname(L, ar)) {
      lua_pushfstring(L, "function " LUA_QS, lua_tostring(L, -1));
      lua_remove(L, -2);   
    }
    else
      lua_pushliteral(L, "?");
  }
  else
    lua_pushfstring(L, "function <%s:%d>", ar->short_src, ar->linedefined);
}
static int countlevels (lua_State *L) {
  lua_Debug ar;
  int li = 1, le = 1;
  while (lua_getstack(L, le, &ar)) { li = le; le *= 2; }
  while (li < le) {
    int m = (li + le)/2;
    if (lua_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}
LUALIB_API void luaL_traceback (lua_State *L, lua_State *L1,
                                const char *msg, int level) {
  lua_Debug ar;
  int top = lua_gettop(L);
  int numlevels = countlevels(L1);
  int mark = (numlevels > LEVELS1 + LEVELS2) ? LEVELS1 : 0;
  if (msg) lua_pushfstring(L, "%s\n", msg);
  lua_pushliteral(L, "stack traceback:");
  while (lua_getstack(L1, level++, &ar)) {
    if (level == mark) {   
      lua_pushliteral(L, "\n\t...");   
      level = numlevels - LEVELS2;   
    }
    else {
      lua_getinfo(L1, "Slnt", &ar);
      lua_pushfstring(L, "\n\t%s:", ar.short_src);
      if (ar.currentline > 0)
        lua_pushfstring(L, "%d:", ar.currentline);
      lua_pushliteral(L, " in ");
      pushfuncname(L, &ar);
      if (ar.istailcall)
        lua_pushliteral(L, "\n\t(...tail calls...)");
      lua_concat(L, lua_gettop(L) - top);
    }
  }
  lua_concat(L, lua_gettop(L) - top);
}
LUALIB_API int luaL_argerror (lua_State *L, int narg, const char *extramsg) {
  lua_Debug ar;
  if (!lua_getstack(L, 0, &ar))   
    return luaL_error(L, "bad argument #%d (%s)", narg, extramsg);
  lua_getinfo(L, "n", &ar);
  if (strcmp(ar.namewhat, "method") == 0) {
    narg--;   
    if (narg == 0)   
      return luaL_error(L, "calling " LUA_QS " on bad self (%s)",
                           ar.name, extramsg);
  }
  if (ar.name == NULL)
    ar.name = (pushglobalfuncname(L, &ar)) ? lua_tostring(L, -1) : "?";
  return luaL_error(L, "bad argument #%d to " LUA_QS " (%s)",
                        narg, ar.name, extramsg);
}
static int typeerror (lua_State *L, int narg, const char *tname) {
  const char *msg = lua_pushfstring(L, "%s expected, got %s",
                                    tname, luaL_typename(L, narg));
  return luaL_argerror(L, narg, msg);
}
static void tag_error (lua_State *L, int narg, int tag) {
  typeerror(L, narg, lua_typename(L, tag));
}
LUALIB_API void luaL_where (lua_State *L, int level) {
  lua_Debug ar;
  if (lua_getstack(L, level, &ar)) {   
    lua_getinfo(L, "Sl", &ar);   
    if (ar.currentline > 0) {   
      lua_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline);
      return;
    }
  }
  lua_pushliteral(L, "");   
}
LUALIB_API int luaL_error (lua_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  luaL_where(L, 1);
  lua_pushvfstring(L, fmt, argp);
  va_end(argp);
  lua_concat(L, 2);
  return lua_error(L);
}
#if !defined(inspectstat)	 
#if defined(LUA_USE_POSIX)
#include <sys/wait.h>
#define inspectstat(stat,what)  \
   if (WIFEXITED(stat)) { stat = WEXITSTATUS(stat); } \
   else if (WIFSIGNALED(stat)) { stat = WTERMSIG(stat); what = "signal"; }
#else
#define inspectstat(stat,what)   
#endif
#endif				 
LUALIB_API int luaL_newmetatable (lua_State *L, const char *tname) {
  luaL_getmetatable(L, tname);   
  if (!lua_isnil(L, -1))   
    return 0;   
  lua_pop(L, 1);
  lua_newtable(L);   
  lua_pushvalue(L, -1);
  lua_setfield(L, LUA_REGISTRYINDEX, tname);   
  return 1;
}
LUALIB_API void luaL_setmetatable (lua_State *L, const char *tname) {
  luaL_getmetatable(L, tname);
  lua_setmetatable(L, -2);
}
LUALIB_API void *luaL_testudata (lua_State *L, int ud, const char *tname) {
  void *p = lua_touserdata(L, ud);
  if (p != NULL) {   
    if (lua_getmetatable(L, ud)) {   
      luaL_getmetatable(L, tname);   
      if (!lua_rawequal(L, -1, -2))   
        p = NULL;   
      lua_pop(L, 2);   
      return p;
    }
  }
  return NULL;   
}
LUALIB_API void *luaL_checkudata (lua_State *L, int ud, const char *tname) {
  void *p = luaL_testudata(L, ud, tname);
  if (p == NULL) typeerror(L, ud, tname);
  return p;
}
LUALIB_API int luaL_checkoption (lua_State *L, int narg, const char *def,
                                 const char *const lst[]) {
  const char *name = (def) ? luaL_optstring(L, narg, def) :
                             luaL_checkstring(L, narg);
  int i;
  for (i=0; lst[i]; i++)
    if (strcmp(lst[i], name) == 0)
      return i;
  return luaL_argerror(L, narg,
                       lua_pushfstring(L, "invalid option " LUA_QS, name));
}
LUALIB_API void luaL_checkstack (lua_State *L, int space, const char *msg) {
  const int extra = LUA_MINSTACK;
  if (!lua_checkstack(L, space + extra)) {
    if (msg)
      luaL_error(L, "stack overflow (%s)", msg);
    else
      luaL_error(L, "stack overflow");
  }
}
LUALIB_API void luaL_checktype (lua_State *L, int narg, int t) {
  if (lua_type(L, narg) != t)
    tag_error(L, narg, t);
}
LUALIB_API void luaL_checkany (lua_State *L, int narg) {
  if (lua_type(L, narg) == LUA_TNONE)
    luaL_argerror(L, narg, "value expected");
}
LUALIB_API const char *luaL_checklstring (lua_State *L, int narg, size_t *len) {
  const char *s = lua_tolstring(L, narg, len);
  if (!s) tag_error(L, narg, LUA_TSTRING);
  return s;
}
LUALIB_API const char *luaL_optlstring (lua_State *L, int narg,
                                        const char *def, size_t *len) {
  if (lua_isnoneornil(L, narg)) {
    if (len)
      *len = (def ? strlen(def) : 0);
    return def;
  }
  else return luaL_checklstring(L, narg, len);
}
LUALIB_API lua_Number luaL_checknumber (lua_State *L, int narg) {
  int isnum;
  lua_Number d = lua_tonumberx(L, narg, &isnum);
  if (!isnum)
    tag_error(L, narg, LUA_TNUMBER);
  return d;
}
LUALIB_API lua_Number luaL_optnumber (lua_State *L, int narg, lua_Number def) {
  return luaL_opt(L, luaL_checknumber, narg, def);
}
LUALIB_API lua_Integer luaL_checkinteger (lua_State *L, int narg) {
  int isnum;
  lua_Integer d = lua_tointegerx(L, narg, &isnum);
  if (!isnum)
    tag_error(L, narg, LUA_TNUMBER);
  return d;
}
LUALIB_API lua_Unsigned luaL_checkunsigned (lua_State *L, int narg) {
  int isnum;
  lua_Unsigned d = lua_tounsignedx(L, narg, &isnum);
  if (!isnum)
    tag_error(L, narg, LUA_TNUMBER);
  return d;
}
LUALIB_API lua_Integer luaL_optinteger (lua_State *L, int narg,
                                                      lua_Integer def) {
  return luaL_opt(L, luaL_checkinteger, narg, def);
}
LUALIB_API lua_Unsigned luaL_optunsigned (lua_State *L, int narg,
                                                        lua_Unsigned def) {
  return luaL_opt(L, luaL_checkunsigned, narg, def);
}
#define buffonstack(B)	((B)->b != (B)->initb)
LUALIB_API char *luaL_prepbuffsize (luaL_Buffer *B, size_t sz) {
  lua_State *L = B->L;
  if (B->size - B->n < sz) {   
    char *newbuff;
    size_t newsize = B->size * 2;   
    if (newsize - B->n < sz)   
      newsize = B->n + sz;
    if (newsize < B->n || newsize - B->n < sz)
      luaL_error(L, "buffer too large");
    newbuff = (char *)lua_newuserdata(L, newsize * sizeof(char));
    memcpy(newbuff, B->b, B->n * sizeof(char));
    if (buffonstack(B))
      lua_remove(L, -2);   
    B->b = newbuff;
    B->size = newsize;
  }
  return &B->b[B->n];
}
LUALIB_API void luaL_addlstring (luaL_Buffer *B, const char *s, size_t l) {
  char *b = luaL_prepbuffsize(B, l);
  memcpy(b, s, l * sizeof(char));
  luaL_addsize(B, l);
}
LUALIB_API void luaL_addstring (luaL_Buffer *B, const char *s) {
  luaL_addlstring(B, s, strlen(s));
}
LUALIB_API void luaL_pushresult (luaL_Buffer *B) {
  lua_State *L = B->L;
  lua_pushlstring(L, B->b, B->n);
  if (buffonstack(B))
    lua_remove(L, -2);   
}
LUALIB_API void luaL_pushresultsize (luaL_Buffer *B, size_t sz) {
  luaL_addsize(B, sz);
  luaL_pushresult(B);
}
LUALIB_API void luaL_addvalue (luaL_Buffer *B) {
  lua_State *L = B->L;
  size_t l;
  const char *s = lua_tolstring(L, -1, &l);
  if (buffonstack(B))
    lua_insert(L, -2);   
  luaL_addlstring(B, s, l);
  lua_remove(L, (buffonstack(B)) ? -2 : -1);   
}
LUALIB_API void luaL_buffinit (lua_State *L, luaL_Buffer *B) {
  B->L = L;
  B->b = B->initb;
  B->n = 0;
  B->size = LUAL_BUFFERSIZE;
}
LUALIB_API char *luaL_buffinitsize (lua_State *L, luaL_Buffer *B, size_t sz) {
  luaL_buffinit(L, B);
  return luaL_prepbuffsize(B, sz);
}
#define freelist	0
LUALIB_API int luaL_ref (lua_State *L, int t) {
  int ref;
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);   
    return LUA_REFNIL;   
  }
  t = lua_absindex(L, t);
  lua_rawgeti(L, t, freelist);   
  ref = (int)lua_tointeger(L, -1);   
  lua_pop(L, 1);   
  if (ref != 0) {   
    lua_rawgeti(L, t, ref);   
    lua_rawseti(L, t, freelist);   
  }
  else   
    ref = (int)lua_rawlen(L, t) + 1;   
  lua_rawseti(L, t, ref);
  return ref;
}
LUALIB_API void luaL_unref (lua_State *L, int t, int ref) {
  if (ref >= 0) {
    t = lua_absindex(L, t);
    lua_rawgeti(L, t, freelist);
    lua_rawseti(L, t, ref);   
    lua_pushinteger(L, ref);
    lua_rawseti(L, t, freelist);   
  }
}
typedef struct LoadS {
  const char *s;
  size_t size;
} LoadS;
static const char *getS (lua_State *L, void *ud, size_t *size) {
  LoadS *ls = (LoadS *)ud;
  (void)L;   
  if (ls->size == 0) return NULL;
  *size = ls->size;
  ls->size = 0;
  return ls->s;
}
LUALIB_API int luaL_loadbufferx (lua_State *L, const char *buff, size_t size,
                                 const char *name, const char *mode) {
  LoadS ls;
  ls.s = buff;
  ls.size = size;
  return lua_load(L, getS, &ls, name, mode);
}
LUALIB_API int luaL_loadstring (lua_State *L, const char *s) {
  return luaL_loadbuffer(L, s, strlen(s), s);
}
LUALIB_API int luaL_getmetafield (lua_State *L, int obj, const char *event) {
  if (!lua_getmetatable(L, obj))   
    return 0;
  lua_pushstring(L, event);
  lua_rawget(L, -2);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 2);   
    return 0;
  }
  else {
    lua_remove(L, -2);   
    return 1;
  }
}
LUALIB_API int luaL_callmeta (lua_State *L, int obj, const char *event) {
  obj = lua_absindex(L, obj);
  if (!luaL_getmetafield(L, obj, event))   
    return 0;
  lua_pushvalue(L, obj);
  lua_call(L, 1, 1);
  return 1;
}
LUALIB_API int luaL_len (lua_State *L, int idx) {
  int l;
  int isnum;
  lua_len(L, idx);
  l = (int)lua_tointegerx(L, -1, &isnum);
  if (!isnum)
    luaL_error(L, "object length is not a number");
  lua_pop(L, 1);   
  return l;
}
LUALIB_API const char *luaL_tolstring (lua_State *L, int idx, size_t *len) {
  if (!luaL_callmeta(L, idx, "__tostring")) {   
    switch (lua_type(L, idx)) {
      case LUA_TNUMBER:
      case LUA_TSTRING:
        lua_pushvalue(L, idx);
        break;
      case LUA_TBOOLEAN:
        lua_pushstring(L, (lua_toboolean(L, idx) ? "true" : "false"));
        break;
      case LUA_TNIL:
        lua_pushliteral(L, "nil");
        break;
      default:
        lua_pushfstring(L, "%s: %p", luaL_typename(L, idx),
                                            lua_topointer(L, idx));
        break;
    }
  }
  return lua_tolstring(L, -1, len);
}
#if defined(LUA_COMPAT_MODULE)
static const char *luaL_findtable (lua_State *L, int idx,
                                   const char *fname, int szhint) {
  const char *e;
  if (idx) lua_pushvalue(L, idx);
  do {
    e = strchr(fname, '.');
    if (e == NULL) e = fname + strlen(fname);
    lua_pushlstring(L, fname, e - fname);
    lua_rawget(L, -2);
    if (lua_isnil(L, -1)) {   
      lua_pop(L, 1);   
      lua_createtable(L, 0, (*e == '.' ? 1 : szhint));  
      lua_pushlstring(L, fname, e - fname);
      lua_pushvalue(L, -2);
      lua_settable(L, -4);   
    }
    else if (!lua_istable(L, -1)) {   
      lua_pop(L, 2);   
      return fname;   
    }
    lua_remove(L, -2);   
    fname = e + 1;
  } while (*e == '.');
  return NULL;
}
static int libsize (const luaL_Reg *l) {
  int size = 0;
  for (; l && l->name; l++) size++;
  return size;
}
LUALIB_API void luaL_pushmodule (lua_State *L, const char *modname,
                                 int sizehint) {
  luaL_findtable(L, LUA_REGISTRYINDEX, "_LOADED", 1);   
  lua_getfield(L, -1, modname);   
  if (!lua_istable(L, -1)) {   
    lua_pop(L, 1);   
    lua_pushglobaltable(L);
    if (luaL_findtable(L, 0, modname, sizehint) != NULL)
      luaL_error(L, "name conflict for module " LUA_QS, modname);
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, modname);   
  }
  lua_remove(L, -2);   
}
LUALIB_API void luaL_openlib (lua_State *L, const char *libname,
                               const luaL_Reg *l, int nup) {
  luaL_checkversion(L);
  if (libname) {
    luaL_pushmodule(L, libname, libsize(l));   
    lua_insert(L, -(nup + 1));   
  }
  if (l)
    luaL_setfuncs(L, l, nup);
  else
    lua_pop(L, nup);   
}
#endif
LUALIB_API void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
  luaL_checkversion(L);
  luaL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {   
    int i;
    for (i = 0; i < nup; i++)   
      lua_pushvalue(L, -nup);
    lua_pushcclosure(L, l->func, nup);   
    lua_setfield(L, -(nup + 2), l->name);
  }
  lua_pop(L, nup);   
}
LUALIB_API int luaL_getsubtable (lua_State *L, int idx, const char *fname) {
  lua_getfield(L, idx, fname);
  if (lua_istable(L, -1)) return 1;   
  else {
    lua_pop(L, 1);   
    idx = lua_absindex(L, idx);
    lua_newtable(L);
    lua_pushvalue(L, -1);   
    lua_setfield(L, idx, fname);   
    return 0;   
  }
}
LUALIB_API void luaL_requiref (lua_State *L, const char *modname,
                               lua_CFunction openf, int glb) {
  lua_pushcfunction(L, openf);
  lua_pushstring(L, modname);   
  lua_call(L, 1, 1);   
  luaL_getsubtable(L, LUA_REGISTRYINDEX, "_LOADED");
  lua_pushvalue(L, -2);   
  lua_setfield(L, -2, modname);   
  lua_pop(L, 1);   
  if (glb) {
    lua_pushvalue(L, -1);   
    lua_setglobal(L, modname);   
  }
}
LUALIB_API const char *luaL_gsub (lua_State *L, const char *s, const char *p,
                                                               const char *r) {
  const char *wild;
  size_t l = strlen(p);
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  while ((wild = strstr(s, p)) != NULL) {
    luaL_addlstring(&b, s, wild - s);   
    luaL_addstring(&b, r);   
    s = wild + l;   
  }
  luaL_addstring(&b, s);   
  luaL_pushresult(&b);
  return lua_tostring(L, -1);
}
LUALIB_API void luaL_checkversion_ (lua_State *L, lua_Number ver) {
  const lua_Number *v = lua_version(L);
  if (v != lua_version(NULL))
    luaL_error(L, "multiple Lua VMs detected");
  else if (*v != ver)
    luaL_error(L, "version mismatch: app. needs %f, Lua core provides %f",
                  ver, *v);
  lua_pushnumber(L, -(lua_Number)0x1234);
  if (lua_tointeger(L, -1) != -0x1234 ||
      lua_tounsigned(L, -1) != (lua_Unsigned)-0x1234)
    luaL_error(L, "bad conversion number->int;"
                  " must recompile Lua with proper settings");
  lua_pop(L, 1);
}
#if defined(_KERNEL)
EXPORT_SYMBOL(luaL_argerror);
EXPORT_SYMBOL(luaL_error);
EXPORT_SYMBOL(luaL_loadbufferx);
EXPORT_SYMBOL(luaL_newmetatable);
EXPORT_SYMBOL(luaL_traceback);
#endif
