/*
** $Id: ldblib.c $
** Interface from Viper to its debug API
** See Copyright Notice in viper.h
*/

#define ldblib_c
#define VIPER_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "viper.h"

#include "lauxlib.h"
#include "viperlib.h"


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
static void checkstack (viper_State *L, viper_State *L1, int n) {
  if (l_unlikely(L != L1 && !viper_checkstack(L1, n)))
    viperL_error(L, "stack overflow");
}


static int db_getregistry (viper_State *L) {
  viper_pushvalue(L, VIPER_REGISTRYINDEX);
  return 1;
}


static int db_getmetatable (viper_State *L) {
  viperL_checkany(L, 1);
  if (!viper_getmetatable(L, 1)) {
    viper_pushnil(L);  /* no metatable */
  }
  return 1;
}


static int db_setmetatable (viper_State *L) {
  int t = viper_type(L, 2);
  viperL_argexpected(L, t == VIPER_TNIL || t == VIPER_TTABLE, 2, "nil or table");
  viper_settop(L, 2);
  viper_setmetatable(L, 1);
  return 1;  /* return 1st argument */
}


static int db_getuservalue (viper_State *L) {
  int n = (int)viperL_optinteger(L, 2, 1);
  if (viper_type(L, 1) != VIPER_TUSERDATA)
    viperL_pushfail(L);
  else if (viper_getiuservalue(L, 1, n) != VIPER_TNONE) {
    viper_pushboolean(L, 1);
    return 2;
  }
  return 1;
}


static int db_setuservalue (viper_State *L) {
  int n = (int)viperL_optinteger(L, 3, 1);
  viperL_checktype(L, 1, VIPER_TUSERDATA);
  viperL_checkany(L, 2);
  viper_settop(L, 2);
  if (!viper_setiuservalue(L, 1, n))
    viperL_pushfail(L);
  return 1;
}


/*
** Auxiliary function used by several library functions: check for
** an optional thread as function's first argument and set 'arg' with
** 1 if this argument is present (so that functions can skip it to
** access their other arguments)
*/
static viper_State *getthread (viper_State *L, int *arg) {
  if (viper_isthread(L, 1)) {
    *arg = 1;
    return viper_tothread(L, 1);
  }
  else {
    *arg = 0;
    return L;  /* function will operate over current thread */
  }
}


/*
** Variations of 'viper_settable', used by 'db_getinfo' to put results
** from 'viper_getinfo' into result table. Key is always a string;
** value can be a string, an int, or a boolean.
*/
static void settabss (viper_State *L, const char *k, const char *v) {
  viper_pushstring(L, v);
  viper_setfield(L, -2, k);
}

static void settabsi (viper_State *L, const char *k, int v) {
  viper_pushinteger(L, v);
  viper_setfield(L, -2, k);
}

static void settabsb (viper_State *L, const char *k, int v) {
  viper_pushboolean(L, v);
  viper_setfield(L, -2, k);
}


/*
** In function 'db_getinfo', the call to 'viper_getinfo' may push
** results on the stack; later it creates the result table to put
** these objects. Function 'treatstackoption' puts the result from
** 'viper_getinfo' on top of the result table so that it can call
** 'viper_setfield'.
*/
static void treatstackoption (viper_State *L, viper_State *L1, const char *fname) {
  if (L == L1)
    viper_rotate(L, -2, 1);  /* exchange object and table */
  else
    viper_xmove(L1, L, 1);  /* move object to the "main" stack */
  viper_setfield(L, -2, fname);  /* put object into table */
}


/*
** Calls 'viper_getinfo' and collects all results in a new table.
** L1 needs stack space for an optional input (function) plus
** two optional outputs (function and line table) from function
** 'viper_getinfo'.
*/
static int db_getinfo (viper_State *L) {
  viper_Debug ar;
  int arg;
  viper_State *L1 = getthread(L, &arg);
  const char *options = viperL_optstring(L, arg+2, "flnSrtu");
  checkstack(L, L1, 3);
  viperL_argcheck(L, options[0] != '>', arg + 2, "invalid option '>'");
  if (viper_isfunction(L, arg + 1)) {  /* info about a function? */
    options = viper_pushfstring(L, ">%s", options);  /* add '>' to 'options' */
    viper_pushvalue(L, arg + 1);  /* move function to 'L1' stack */
    viper_xmove(L, L1, 1);
  }
  else {  /* stack level */
    if (!viper_getstack(L1, (int)viperL_checkinteger(L, arg + 1), &ar)) {
      viperL_pushfail(L);  /* level out of range */
      return 1;
    }
  }
  if (!viper_getinfo(L1, options, &ar))
    return viperL_argerror(L, arg+2, "invalid option");
  viper_newtable(L);  /* table to collect results */
  if (strchr(options, 'S')) {
    viper_pushlstring(L, ar.source, ar.srclen);
    viper_setfield(L, -2, "source");
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


static int db_getlocal (viper_State *L) {
  int arg;
  viper_State *L1 = getthread(L, &arg);
  int nvar = (int)viperL_checkinteger(L, arg + 2);  /* local-variable index */
  if (viper_isfunction(L, arg + 1)) {  /* function argument? */
    viper_pushvalue(L, arg + 1);  /* push function */
    viper_pushstring(L, viper_getlocal(L, NULL, nvar));  /* push local name */
    return 1;  /* return only name (there is no value) */
  }
  else {  /* stack-level argument */
    viper_Debug ar;
    const char *name;
    int level = (int)viperL_checkinteger(L, arg + 1);
    if (l_unlikely(!viper_getstack(L1, level, &ar)))  /* out of range? */
      return viperL_argerror(L, arg+1, "level out of range");
    checkstack(L, L1, 1);
    name = viper_getlocal(L1, &ar, nvar);
    if (name) {
      viper_xmove(L1, L, 1);  /* move local value */
      viper_pushstring(L, name);  /* push name */
      viper_rotate(L, -2, 1);  /* re-order */
      return 2;
    }
    else {
      viperL_pushfail(L);  /* no name (nor value) */
      return 1;
    }
  }
}


static int db_setlocal (viper_State *L) {
  int arg;
  const char *name;
  viper_State *L1 = getthread(L, &arg);
  viper_Debug ar;
  int level = (int)viperL_checkinteger(L, arg + 1);
  int nvar = (int)viperL_checkinteger(L, arg + 2);
  if (l_unlikely(!viper_getstack(L1, level, &ar)))  /* out of range? */
    return viperL_argerror(L, arg+1, "level out of range");
  viperL_checkany(L, arg+3);
  viper_settop(L, arg+3);
  checkstack(L, L1, 1);
  viper_xmove(L, L1, 1);
  name = viper_setlocal(L1, &ar, nvar);
  if (name == NULL)
    viper_pop(L1, 1);  /* pop value (if not popped by 'viper_setlocal') */
  viper_pushstring(L, name);
  return 1;
}


/*
** get (if 'get' is true) or set an upvalue from a closure
*/
static int auxupvalue (viper_State *L, int get) {
  const char *name;
  int n = (int)viperL_checkinteger(L, 2);  /* upvalue index */
  viperL_checktype(L, 1, VIPER_TFUNCTION);  /* closure */
  name = get ? viper_getupvalue(L, 1, n) : viper_setupvalue(L, 1, n);
  if (name == NULL) return 0;
  viper_pushstring(L, name);
  viper_insert(L, -(get+1));  /* no-op if get is false */
  return get + 1;
}


static int db_getupvalue (viper_State *L) {
  return auxupvalue(L, 1);
}


static int db_setupvalue (viper_State *L) {
  viperL_checkany(L, 3);
  return auxupvalue(L, 0);
}


/*
** Check whether a given upvalue from a given closure exists and
** returns its index
*/
static void *checkupval (viper_State *L, int argf, int argnup, int *pnup) {
  void *id;
  int nup = (int)viperL_checkinteger(L, argnup);  /* upvalue index */
  viperL_checktype(L, argf, VIPER_TFUNCTION);  /* closure */
  id = viper_upvalueid(L, argf, nup);
  if (pnup) {
    viperL_argcheck(L, id != NULL, argnup, "invalid upvalue index");
    *pnup = nup;
  }
  return id;
}


static int db_upvalueid (viper_State *L) {
  void *id = checkupval(L, 1, 2, NULL);
  if (id != NULL)
    viper_pushlightuserdata(L, id);
  else
    viperL_pushfail(L);
  return 1;
}


static int db_upvaluejoin (viper_State *L) {
  int n1, n2;
  checkupval(L, 1, 2, &n1);
  checkupval(L, 3, 4, &n2);
  viperL_argcheck(L, !viper_iscfunction(L, 1), 1, "Viper function expected");
  viperL_argcheck(L, !viper_iscfunction(L, 3), 3, "Viper function expected");
  viper_upvaluejoin(L, 1, n1, 3, n2);
  return 0;
}


/*
** Call hook function registered at hook table for the current
** thread (if there is one)
*/
static void hookf (viper_State *L, viper_Debug *ar) {
  static const char *const hooknames[] =
    {"call", "return", "line", "count", "tail call"};
  viper_getfield(L, VIPER_REGISTRYINDEX, HOOKKEY);
  viper_pushthread(L);
  if (viper_rawget(L, -2) == VIPER_TFUNCTION) {  /* is there a hook function? */
    viper_pushstring(L, hooknames[(int)ar->event]);  /* push event name */
    if (ar->currentline >= 0)
      viper_pushinteger(L, ar->currentline);  /* push current line */
    else viper_pushnil(L);
    viper_assert(viper_getinfo(L, "lS", ar));
    viper_call(L, 2, 0);  /* call hook function */
  }
}


/*
** Convert a string mask (for 'sethook') into a bit mask
*/
static int makemask (const char *smask, int count) {
  int mask = 0;
  if (strchr(smask, 'c')) mask |= VIPER_MASKCALL;
  if (strchr(smask, 'r')) mask |= VIPER_MASKRET;
  if (strchr(smask, 'l')) mask |= VIPER_MASKLINE;
  if (count > 0) mask |= VIPER_MASKCOUNT;
  return mask;
}


/*
** Convert a bit mask (for 'gethook') into a string mask
*/
static char *unmakemask (int mask, char *smask) {
  int i = 0;
  if (mask & VIPER_MASKCALL) smask[i++] = 'c';
  if (mask & VIPER_MASKRET) smask[i++] = 'r';
  if (mask & VIPER_MASKLINE) smask[i++] = 'l';
  smask[i] = '\0';
  return smask;
}


static int db_sethook (viper_State *L) {
  int arg, mask, count;
  viper_Hook func;
  viper_State *L1 = getthread(L, &arg);
  if (viper_isnoneornil(L, arg+1)) {  /* no hook? */
    viper_settop(L, arg+1);
    func = NULL; mask = 0; count = 0;  /* turn off hooks */
  }
  else {
    const char *smask = viperL_checkstring(L, arg+2);
    viperL_checktype(L, arg+1, VIPER_TFUNCTION);
    count = (int)viperL_optinteger(L, arg + 3, 0);
    func = hookf; mask = makemask(smask, count);
  }
  if (!viperL_getsubtable(L, VIPER_REGISTRYINDEX, HOOKKEY)) {
    /* table just created; initialize it */
    viper_pushliteral(L, "k");
    viper_setfield(L, -2, "__mode");  /** hooktable.__mode = "k" */
    viper_pushvalue(L, -1);
    viper_setmetatable(L, -2);  /* metatable(hooktable) = hooktable */
  }
  checkstack(L, L1, 1);
  viper_pushthread(L1); viper_xmove(L1, L, 1);  /* key (thread) */
  viper_pushvalue(L, arg + 1);  /* value (hook function) */
  viper_rawset(L, -3);  /* hooktable[L1] = new Viper hook */
  viper_sethook(L1, func, mask, count);
  return 0;
}


static int db_gethook (viper_State *L) {
  int arg;
  viper_State *L1 = getthread(L, &arg);
  char buff[5];
  int mask = viper_gethookmask(L1);
  viper_Hook hook = viper_gethook(L1);
  if (hook == NULL) {  /* no hook? */
    viperL_pushfail(L);
    return 1;
  }
  else if (hook != hookf)  /* external hook? */
    viper_pushliteral(L, "external hook");
  else {  /* hook table must exist */
    viper_getfield(L, VIPER_REGISTRYINDEX, HOOKKEY);
    checkstack(L, L1, 1);
    viper_pushthread(L1); viper_xmove(L1, L, 1);
    viper_rawget(L, -2);   /* 1st result = hooktable[L1] */
    viper_remove(L, -2);  /* remove hook table */
  }
  viper_pushstring(L, unmakemask(mask, buff));  /* 2nd result = mask */
  viper_pushinteger(L, viper_gethookcount(L1));  /* 3rd result = count */
  return 3;
}


static int db_debug (viper_State *L) {
  for (;;) {
    char buffer[250];
    viper_writestringerror("%s", "viper_debug> ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL ||
        strcmp(buffer, "cont\n") == 0)
      return 0;
    if (viperL_loadbuffer(L, buffer, strlen(buffer), "=(debug command)") ||
        viper_pcall(L, 0, 0, 0))
      viper_writestringerror("%s\n", viperL_tolstring(L, -1, NULL));
    viper_settop(L, 0);  /* remove eventual returns */
  }
}


static int db_traceback (viper_State *L) {
  int arg;
  viper_State *L1 = getthread(L, &arg);
  const char *msg = viper_tostring(L, arg + 1);
  if (msg == NULL && !viper_isnoneornil(L, arg + 1))  /* non-string 'msg'? */
    viper_pushvalue(L, arg + 1);  /* return it untouched */
  else {
    int level = (int)viperL_optinteger(L, arg + 2, (L == L1) ? 1 : 0);
    viperL_traceback(L, L1, msg, level);
  }
  return 1;
}


static int db_setcstacklimit (viper_State *L) {
  int limit = (int)viperL_checkinteger(L, 1);
  int res = viper_setcstacklimit(L, limit);
  viper_pushinteger(L, res);
  return 1;
}


static const viperL_Reg dblib[] = {
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


VIPERMOD_API int viperopen_debug (viper_State *L) {
  viperL_newlib(L, dblib);
  return 1;
}

