/*
** $Id: dblib.c $
** Interface from Venom to its debug API
** See Copyright Notice in venom.h
*/

#define dblib_c
#define VENOM_LIB

#include "prefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "venom.h"

#include "auxlib.h"
#include "venomlib.h"

/*
** The hook table at registry[HOOKKEY] maps threads to their current
** hook function.
*/
static const char *const HOOKKEY = "_HOOKKEY";


/*
** If L1 != L, L1 can be in any state, and therefore there are no
** guarantees about its stack space; any push in L1 must be
** checked.
*/
static void checkstack (venom_State *L, venom_State *L1, int n) {
  if (l_unlikely(L != L1 && !venom_checkstack(L1, n)))
    venomL_error(L, "stack overflow");
}


static int db_getregistry (venom_State *L) {
  venom_pushvalue(L, VENOM_REGISTRYINDEX);
  return 1;
}


static int db_getmetatable (venom_State *L) {
  venomL_checkany(L, 1);
  if (!venom_getmetatable(L, 1)) {
    venom_pushnil(L);  /* no metatable */
  }
  return 1;
}


static int db_setmetatable (venom_State *L) {
  int t = venom_type(L, 2);
  venomL_argexpected(L, t == VENOM_TNIL || t == VENOM_TTABLE, 2, "nil or table");
  venom_settop(L, 2);
  venom_setmetatable(L, 1);
  return 1;  /* return 1st argument */
}


static int db_getuservalue (venom_State *L) {
  int n = (int)venomL_optinteger(L, 2, 1);
  if (venom_type(L, 1) != VENOM_TUSERDATA)
    venomL_pushfail(L);
  else if (venom_getiuservalue(L, 1, n) != VENOM_TNONE) {
    venom_pushboolean(L, 1);
    return 2;
  }
  return 1;
}


static int db_setuservalue (venom_State *L) {
  int n = (int)venomL_optinteger(L, 3, 1);
  venomL_checktype(L, 1, VENOM_TUSERDATA);
  venomL_checkany(L, 2);
  venom_settop(L, 2);
  if (!venom_setiuservalue(L, 1, n))
    venomL_pushfail(L);
  return 1;
}


/*
** Auxiliary function used by several library functions: check for
** an optional thread as function's first argument and set 'arg' with
** 1 if this argument is present (so that functions can skip it to
** access their other arguments)
*/
static venom_State *getthread (venom_State *L, int *arg) {
  if (venom_isthread(L, 1)) {
    *arg = 1;
    return venom_tothread(L, 1);
  }
  else {
    *arg = 0;
    return L;  /* function will operate over current thread */
  }
}


/*
** Variations of 'venom_settable', used by 'db_getinfo' to put results
** from 'venom_getinfo' into result table. Key is always a string;
** value can be a string, an int, or a boolean.
*/
static void settabss (venom_State *L, const char *k, const char *v) {
  venom_pushstring(L, v);
  venom_setfield(L, -2, k);
}

static void settabsi (venom_State *L, const char *k, int v) {
  venom_pushinteger(L, v);
  venom_setfield(L, -2, k);
}

static void settabsb (venom_State *L, const char *k, int v) {
  venom_pushboolean(L, v);
  venom_setfield(L, -2, k);
}


/*
** In function 'db_getinfo', the call to 'venom_getinfo' may push
** results on the stack; later it creates the result table to put
** these objects. Function 'treatstackoption' puts the result from
** 'venom_getinfo' on top of the result table so that it can call
** 'venom_setfield'.
*/
static void treatstackoption (venom_State *L, venom_State *L1, const char *fname) {
  if (L == L1)
    venom_rotate(L, -2, 1);  /* exchange object and table */
  else
    venom_xmove(L1, L, 1);  /* move object to the "main" stack */
  venom_setfield(L, -2, fname);  /* put object into table */
}


/*
** Calls 'venom_getinfo' and collects all results in a new table.
** L1 needs stack space for an optional input (function) plus
** two optional outputs (function and line table) from function
** 'venom_getinfo'.
*/
static int db_getinfo (venom_State *L) {
  venom_Debug ar;
  int arg;
  venom_State *L1 = getthread(L, &arg);
  const char *options = venomL_optstring(L, arg+2, "flnSrtu");
  checkstack(L, L1, 3);
  venomL_argcheck(L, options[0] != '>', arg + 2, "invalid option '>'");
  if (venom_isfunction(L, arg + 1)) {  /* info about a function? */
    options = venom_pushfstring(L, ">%s", options);  /* add '>' to 'options' */
    venom_pushvalue(L, arg + 1);  /* move function to 'L1' stack */
    venom_xmove(L, L1, 1);
  }
  else {  /* stack level */
    if (!venom_getstack(L1, (int)venomL_checkinteger(L, arg + 1), &ar)) {
      venomL_pushfail(L);  /* level out of range */
      return 1;
    }
  }
  if (!venom_getinfo(L1, options, &ar))
    return venomL_argerror(L, arg+2, "invalid option");
  venom_newtable(L);  /* table to collect results */
  if (strchr(options, 'S')) {
    venom_pushlstring(L, ar.source, ar.srclen);
    venom_setfield(L, -2, "source");
    settabss(L, "short_src", ar.short_src);
    settabsi(L, "linedefined", ar.linedefined);
    settabsi(L, "lastlinedefined", ar.lastlinedefined);
    settabss(L, "what", ar.what);
  }
  if (strchr(options, 'l'))
    settabsi(L, "currentline", ar.currentline);
  if (strchr(options, 'u')) {
    settabsi(L, "nups", ar.nups);
    settabsi(L, "nparams", ar.nparams);
    settabsb(L, "isvararg", ar.isvararg);
  }
  if (strchr(options, 'n')) {
    settabss(L, "name", ar.name);
    settabss(L, "namewhat", ar.namewhat);
  }
  if (strchr(options, 'r')) {
    settabsi(L, "ftransfer", ar.ftransfer);
    settabsi(L, "ntransfer", ar.ntransfer);
  }
  if (strchr(options, 't'))
    settabsb(L, "istailcall", ar.istailcall);
  if (strchr(options, 'L'))
    treatstackoption(L, L1, "activelines");
  if (strchr(options, 'f'))
    treatstackoption(L, L1, "func");
  return 1;  /* return table */
}


static int db_getlocal (venom_State *L) {
  int arg;
  venom_State *L1 = getthread(L, &arg);
  int nvar = (int)venomL_checkinteger(L, arg + 2);  /* local-variable index */
  if (venom_isfunction(L, arg + 1)) {  /* function argument? */
    venom_pushvalue(L, arg + 1);  /* push function */
    venom_pushstring(L, venom_getlocal(L, NULL, nvar));  /* push local name */
    return 1;  /* return only name (there is no value) */
  }
  else {  /* stack-level argument */
    venom_Debug ar;
    const char *name;
    int level = (int)venomL_checkinteger(L, arg + 1);
    if (l_unlikely(!venom_getstack(L1, level, &ar)))  /* out of range? */
      return venomL_argerror(L, arg+1, "level out of range");
    checkstack(L, L1, 1);
    name = venom_getlocal(L1, &ar, nvar);
    if (name) {
      venom_xmove(L1, L, 1);  /* move local value */
      venom_pushstring(L, name);  /* push name */
      venom_rotate(L, -2, 1);  /* re-order */
      return 2;
    }
    else {
      venomL_pushfail(L);  /* no name (nor value) */
      return 1;
    }
  }
}


static int db_setlocal (venom_State *L) {
  int arg;
  const char *name;
  venom_State *L1 = getthread(L, &arg);
  venom_Debug ar;
  int level = (int)venomL_checkinteger(L, arg + 1);
  int nvar = (int)venomL_checkinteger(L, arg + 2);
  if (l_unlikely(!venom_getstack(L1, level, &ar)))  /* out of range? */
    return venomL_argerror(L, arg+1, "level out of range");
  venomL_checkany(L, arg+3);
  venom_settop(L, arg+3);
  checkstack(L, L1, 1);
  venom_xmove(L, L1, 1);
  name = venom_setlocal(L1, &ar, nvar);
  if (name == NULL)
    venom_pop(L1, 1);  /* pop value (if not popped by 'venom_setlocal') */
  venom_pushstring(L, name);
  return 1;
}


/*
** get (if 'get' is true) or set an upvalue from a closure
*/
static int auxupvalue (venom_State *L, int get) {
  const char *name;
  int n = (int)venomL_checkinteger(L, 2);  /* upvalue index */
  venomL_checktype(L, 1, VENOM_TFUNCTION);  /* closure */
  name = get ? venom_getupvalue(L, 1, n) : venom_setupvalue(L, 1, n);
  if (name == NULL) return 0;
  venom_pushstring(L, name);
  venom_insert(L, -(get+1));  /* no-op if get is false */
  return get + 1;
}


static int db_getupvalue (venom_State *L) {
  return auxupvalue(L, 1);
}


static int db_setupvalue (venom_State *L) {
  venomL_checkany(L, 3);
  return auxupvalue(L, 0);
}


/*
** Check whether a given upvalue from a given closure exists and
** returns its index
*/
static void *checkupval (venom_State *L, int argf, int argnup, int *pnup) {
  void *id;
  int nup = (int)venomL_checkinteger(L, argnup);  /* upvalue index */
  venomL_checktype(L, argf, VENOM_TFUNCTION);  /* closure */
  id = venom_upvalueid(L, argf, nup);
  if (pnup) {
    venomL_argcheck(L, id != NULL, argnup, "invalid upvalue index");
    *pnup = nup;
  }
  return id;
}


static int db_upvalueid (venom_State *L) {
  void *id = checkupval(L, 1, 2, NULL);
  if (id != NULL)
    venom_pushlightuserdata(L, id);
  else
    venomL_pushfail(L);
  return 1;
}


static int db_upvaluejoin (venom_State *L) {
  int n1, n2;
  checkupval(L, 1, 2, &n1);
  checkupval(L, 3, 4, &n2);
  venomL_argcheck(L, !venom_iscfunction(L, 1), 1, "Venom function expected");
  venomL_argcheck(L, !venom_iscfunction(L, 3), 3, "Venom function expected");
  venom_upvaluejoin(L, 1, n1, 3, n2);
  return 0;
}


/*
** Call hook function registered at hook table for the current
** thread (if there is one)
*/
static void hookf (venom_State *L, venom_Debug *ar) {
  static const char *const hooknames[] =
    {"call", "return", "line", "count", "tail call"};
  venom_getfield(L, VENOM_REGISTRYINDEX, HOOKKEY);
  venom_pushthread(L);
  if (venom_rawget(L, -2) == VENOM_TFUNCTION) {  /* is there a hook function? */
    venom_pushstring(L, hooknames[(int)ar->event]);  /* push event name */
    if (ar->currentline >= 0)
      venom_pushinteger(L, ar->currentline);  /* push current line */
    else venom_pushnil(L);
    venom_assert(venom_getinfo(L, "lS", ar));
    venom_call(L, 2, 0);  /* call hook function */
  }
}


/*
** Convert a string mask (for 'sethook') into a bit mask
*/
static int makemask (const char *smask, int count) {
  int mask = 0;
  if (strchr(smask, 'c')) mask |= VENOM_MASKCALL;
  if (strchr(smask, 'r')) mask |= VENOM_MASKRET;
  if (strchr(smask, 'l')) mask |= VENOM_MASKLINE;
  if (count > 0) mask |= VENOM_MASKCOUNT;
  return mask;
}


/*
** Convert a bit mask (for 'gethook') into a string mask
*/
static char *unmakemask (int mask, char *smask) {
  int i = 0;
  if (mask & VENOM_MASKCALL) smask[i++] = 'c';
  if (mask & VENOM_MASKRET) smask[i++] = 'r';
  if (mask & VENOM_MASKLINE) smask[i++] = 'l';
  smask[i] = '\0';
  return smask;
}


static int db_sethook (venom_State *L) {
  int arg, mask, count;
  venom_Hook func;
  venom_State *L1 = getthread(L, &arg);
  if (venom_isnoneornil(L, arg+1)) {  /* no hook? */
    venom_settop(L, arg+1);
    func = NULL; mask = 0; count = 0;  /* turn off hooks */
  }
  else {
    const char *smask = venomL_checkstring(L, arg+2);
    venomL_checktype(L, arg+1, VENOM_TFUNCTION);
    count = (int)venomL_optinteger(L, arg + 3, 0);
    func = hookf; mask = makemask(smask, count);
  }
  if (!venomL_getsubtable(L, VENOM_REGISTRYINDEX, HOOKKEY)) {
    /* table just created; initialize it */
    venom_pushliteral(L, "k");
    venom_setfield(L, -2, "__mode");  /** hooktable.__mode = "k" */
    venom_pushvalue(L, -1);
    venom_setmetatable(L, -2);  /* metatable(hooktable) = hooktable */
  }
  checkstack(L, L1, 1);
  venom_pushthread(L1); venom_xmove(L1, L, 1);  /* key (thread) */
  venom_pushvalue(L, arg + 1);  /* value (hook function) */
  venom_rawset(L, -3);  /* hooktable[L1] = new Venom hook */
  venom_sethook(L1, func, mask, count);
  return 0;
}


static int db_gethook (venom_State *L) {
  int arg;
  venom_State *L1 = getthread(L, &arg);
  char buff[5];
  int mask = venom_gethookmask(L1);
  venom_Hook hook = venom_gethook(L1);
  if (hook == NULL) {  /* no hook? */
    venomL_pushfail(L);
    return 1;
  }
  else if (hook != hookf)  /* external hook? */
    venom_pushliteral(L, "external hook");
  else {  /* hook table must exist */
    venom_getfield(L, VENOM_REGISTRYINDEX, HOOKKEY);
    checkstack(L, L1, 1);
    venom_pushthread(L1); venom_xmove(L1, L, 1);
    venom_rawget(L, -2);   /* 1st result = hooktable[L1] */
    venom_remove(L, -2);  /* remove hook table */
  }
  venom_pushstring(L, unmakemask(mask, buff));  /* 2nd result = mask */
  venom_pushinteger(L, venom_gethookcount(L1));  /* 3rd result = count */
  return 3;
}


static int db_debug (venom_State *L) {
  for (;;) {
    char buffer[250];
    venom_writestringerror("%s", "venom_debug> ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL ||
        strcmp(buffer, "cont\n") == 0)
      return 0;
    if (venomL_loadbuffer(L, buffer, strlen(buffer), "=(debug command)") ||
        venom_pcall(L, 0, 0, 0))
      venom_writestringerror("%s\n", venomL_tolstring(L, -1, NULL));
    venom_settop(L, 0);  /* remove eventual returns */
  }
}


static int db_traceback (venom_State *L) {
  int arg;
  venom_State *L1 = getthread(L, &arg);
  const char *msg = venom_tostring(L, arg + 1);
  if (msg == NULL && !venom_isnoneornil(L, arg + 1))  /* non-string 'msg'? */
    venom_pushvalue(L, arg + 1);  /* return it untouched */
  else {
    int level = (int)venomL_optinteger(L, arg + 2, (L == L1) ? 1 : 0);
    venomL_traceback(L, L1, msg, level);
  }
  return 1;
}


static int db_setcstacklimit (venom_State *L) {
  int limit = (int)venomL_checkinteger(L, 1);
  int res = venom_setcstacklimit(L, limit);
  venom_pushinteger(L, res);
  return 1;
}


static const venomL_Reg dblib[] = {
  {"debug", db_debug},
  {"getuservalue", db_getuservalue},
  {"gethook", db_gethook},
  {"getinfo", db_getinfo},
  {"getlocal", db_getlocal},
  {"getregistry", db_getregistry},
  {"getmetatable", db_getmetatable},
  {"getupvalue", db_getupvalue},
  {"upvaluejoin", db_upvaluejoin},
  {"upvalueid", db_upvalueid},
  {"setuservalue", db_setuservalue},
  {"sethook", db_sethook},
  {"setlocal", db_setlocal},
  {"setmetatable", db_setmetatable},
  {"setupvalue", db_setupvalue},
  {"traceback", db_traceback},
  {"setcstacklimit", db_setcstacklimit},
  {NULL, NULL}
};


VENOMMOD_API int venomopen_debug (venom_State *L) {
  venomL_newlib(L, dblib);
  return 1;
}