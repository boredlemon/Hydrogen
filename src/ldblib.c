/*
** $Id: ldblib.c $
** Interface from Cup to its debug API
** See Copyright Notice in cup.h
*/

#define ldblib_c
#define CUP_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cup.h"

#include "lauxlib.h"
#include "cuplib.h"


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
static void checkstack (cup_State *L, cup_State *L1, int n) {
  if (l_unlikely(L != L1 && !cup_checkstack(L1, n)))
    cupL_error(L, "stack overflow");
}


static int db_getregistry (cup_State *L) {
  cup_pushvalue(L, CUP_REGISTRYINDEX);
  return 1;
}


static int db_getmetatable (cup_State *L) {
  cupL_checkany(L, 1);
  if (!cup_getmetatable(L, 1)) {
    cup_pushnil(L);  /* no metatable */
  }
  return 1;
}


static int db_setmetatable (cup_State *L) {
  int t = cup_type(L, 2);
  cupL_argexpected(L, t == CUP_TNIL || t == CUP_TTABLE, 2, "nil or table");
  cup_settop(L, 2);
  cup_setmetatable(L, 1);
  return 1;  /* return 1st argument */
}


static int db_getuservalue (cup_State *L) {
  int n = (int)cupL_optinteger(L, 2, 1);
  if (cup_type(L, 1) != CUP_TUSERDATA)
    cupL_pushfail(L);
  else if (cup_getiuservalue(L, 1, n) != CUP_TNONE) {
    cup_pushboolean(L, 1);
    return 2;
  }
  return 1;
}


static int db_setuservalue (cup_State *L) {
  int n = (int)cupL_optinteger(L, 3, 1);
  cupL_checktype(L, 1, CUP_TUSERDATA);
  cupL_checkany(L, 2);
  cup_settop(L, 2);
  if (!cup_setiuservalue(L, 1, n))
    cupL_pushfail(L);
  return 1;
}


/*
** Auxiliary function used by several library functions: check for
** an optional thread as function's first argument and set 'arg' with
** 1 if this argument is present (so that functions can skip it to
** access their other arguments)
*/
static cup_State *getthread (cup_State *L, int *arg) {
  if (cup_isthread(L, 1)) {
    *arg = 1;
    return cup_tothread(L, 1);
  }
  else {
    *arg = 0;
    return L;  /* function will operate over current thread */
  }
}


/*
** Variations of 'cup_settable', used by 'db_getinfo' to put results
** from 'cup_getinfo' into result table. Key is always a string;
** value can be a string, an int, or a boolean.
*/
static void settabss (cup_State *L, const char *k, const char *v) {
  cup_pushstring(L, v);
  cup_setfield(L, -2, k);
}

static void settabsi (cup_State *L, const char *k, int v) {
  cup_pushinteger(L, v);
  cup_setfield(L, -2, k);
}

static void settabsb (cup_State *L, const char *k, int v) {
  cup_pushboolean(L, v);
  cup_setfield(L, -2, k);
}


/*
** In function 'db_getinfo', the call to 'cup_getinfo' may push
** results on the stack; later it creates the result table to put
** these objects. Function 'treatstackoption' puts the result from
** 'cup_getinfo' on top of the result table so that it can call
** 'cup_setfield'.
*/
static void treatstackoption (cup_State *L, cup_State *L1, const char *fname) {
  if (L == L1)
    cup_rotate(L, -2, 1);  /* exchange object and table */
  else
    cup_xmove(L1, L, 1);  /* move object to the "main" stack */
  cup_setfield(L, -2, fname);  /* put object into table */
}


/*
** Calls 'cup_getinfo' and collects all results in a new table.
** L1 needs stack space for an optional input (function) plus
** two optional outputs (function and line table) from function
** 'cup_getinfo'.
*/
static int db_getinfo (cup_State *L) {
  cup_Debug ar;
  int arg;
  cup_State *L1 = getthread(L, &arg);
  const char *options = cupL_optstring(L, arg+2, "flnSrtu");
  checkstack(L, L1, 3);
  cupL_argcheck(L, options[0] != '>', arg + 2, "invalid option '>'");
  if (cup_isfunction(L, arg + 1)) {  /* info about a function? */
    options = cup_pushfstring(L, ">%s", options);  /* add '>' to 'options' */
    cup_pushvalue(L, arg + 1);  /* move function to 'L1' stack */
    cup_xmove(L, L1, 1);
  }
  else {  /* stack level */
    if (!cup_getstack(L1, (int)cupL_checkinteger(L, arg + 1), &ar)) {
      cupL_pushfail(L);  /* level out of range */
      return 1;
    }
  }
  if (!cup_getinfo(L1, options, &ar))
    return cupL_argerror(L, arg+2, "invalid option");
  cup_newtable(L);  /* table to collect results */
  if (strchr(options, 'S')) {
    cup_pushlstring(L, ar.source, ar.srclen);
    cup_setfield(L, -2, "source");
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


static int db_getlocal (cup_State *L) {
  int arg;
  cup_State *L1 = getthread(L, &arg);
  int nvar = (int)cupL_checkinteger(L, arg + 2);  /* local-variable index */
  if (cup_isfunction(L, arg + 1)) {  /* function argument? */
    cup_pushvalue(L, arg + 1);  /* push function */
    cup_pushstring(L, cup_getlocal(L, NULL, nvar));  /* push local name */
    return 1;  /* return only name (there is no value) */
  }
  else {  /* stack-level argument */
    cup_Debug ar;
    const char *name;
    int level = (int)cupL_checkinteger(L, arg + 1);
    if (l_unlikely(!cup_getstack(L1, level, &ar)))  /* out of range? */
      return cupL_argerror(L, arg+1, "level out of range");
    checkstack(L, L1, 1);
    name = cup_getlocal(L1, &ar, nvar);
    if (name) {
      cup_xmove(L1, L, 1);  /* move local value */
      cup_pushstring(L, name);  /* push name */
      cup_rotate(L, -2, 1);  /* re-order */
      return 2;
    }
    else {
      cupL_pushfail(L);  /* no name (nor value) */
      return 1;
    }
  }
}


static int db_setlocal (cup_State *L) {
  int arg;
  const char *name;
  cup_State *L1 = getthread(L, &arg);
  cup_Debug ar;
  int level = (int)cupL_checkinteger(L, arg + 1);
  int nvar = (int)cupL_checkinteger(L, arg + 2);
  if (l_unlikely(!cup_getstack(L1, level, &ar)))  /* out of range? */
    return cupL_argerror(L, arg+1, "level out of range");
  cupL_checkany(L, arg+3);
  cup_settop(L, arg+3);
  checkstack(L, L1, 1);
  cup_xmove(L, L1, 1);
  name = cup_setlocal(L1, &ar, nvar);
  if (name == NULL)
    cup_pop(L1, 1);  /* pop value (if not popped by 'cup_setlocal') */
  cup_pushstring(L, name);
  return 1;
}


/*
** get (if 'get' is true) or set an upvalue from a closure
*/
static int auxupvalue (cup_State *L, int get) {
  const char *name;
  int n = (int)cupL_checkinteger(L, 2);  /* upvalue index */
  cupL_checktype(L, 1, CUP_TFUNCTION);  /* closure */
  name = get ? cup_getupvalue(L, 1, n) : cup_setupvalue(L, 1, n);
  if (name == NULL) return 0;
  cup_pushstring(L, name);
  cup_insert(L, -(get+1));  /* no-op if get is false */
  return get + 1;
}


static int db_getupvalue (cup_State *L) {
  return auxupvalue(L, 1);
}


static int db_setupvalue (cup_State *L) {
  cupL_checkany(L, 3);
  return auxupvalue(L, 0);
}


/*
** Check whether a given upvalue from a given closure exists and
** returns its index
*/
static void *checkupval (cup_State *L, int argf, int argnup, int *pnup) {
  void *id;
  int nup = (int)cupL_checkinteger(L, argnup);  /* upvalue index */
  cupL_checktype(L, argf, CUP_TFUNCTION);  /* closure */
  id = cup_upvalueid(L, argf, nup);
  if (pnup) {
    cupL_argcheck(L, id != NULL, argnup, "invalid upvalue index");
    *pnup = nup;
  }
  return id;
}


static int db_upvalueid (cup_State *L) {
  void *id = checkupval(L, 1, 2, NULL);
  if (id != NULL)
    cup_pushlightuserdata(L, id);
  else
    cupL_pushfail(L);
  return 1;
}


static int db_upvaluejoin (cup_State *L) {
  int n1, n2;
  checkupval(L, 1, 2, &n1);
  checkupval(L, 3, 4, &n2);
  cupL_argcheck(L, !cup_iscfunction(L, 1), 1, "Cup function expected");
  cupL_argcheck(L, !cup_iscfunction(L, 3), 3, "Cup function expected");
  cup_upvaluejoin(L, 1, n1, 3, n2);
  return 0;
}


/*
** Call hook function registered at hook table for the current
** thread (if there is one)
*/
static void hookf (cup_State *L, cup_Debug *ar) {
  static const char *const hooknames[] =
    {"call", "return", "line", "count", "tail call"};
  cup_getfield(L, CUP_REGISTRYINDEX, HOOKKEY);
  cup_pushthread(L);
  if (cup_rawget(L, -2) == CUP_TFUNCTION) {  /* is there a hook function? */
    cup_pushstring(L, hooknames[(int)ar->event]);  /* push event name */
    if (ar->currentline >= 0)
      cup_pushinteger(L, ar->currentline);  /* push current line */
    else cup_pushnil(L);
    cup_assert(cup_getinfo(L, "lS", ar));
    cup_call(L, 2, 0);  /* call hook function */
  }
}


/*
** Convert a string mask (for 'sethook') into a bit mask
*/
static int makemask (const char *smask, int count) {
  int mask = 0;
  if (strchr(smask, 'c')) mask |= CUP_MASKCALL;
  if (strchr(smask, 'r')) mask |= CUP_MASKRET;
  if (strchr(smask, 'l')) mask |= CUP_MASKLINE;
  if (count > 0) mask |= CUP_MASKCOUNT;
  return mask;
}


/*
** Convert a bit mask (for 'gethook') into a string mask
*/
static char *unmakemask (int mask, char *smask) {
  int i = 0;
  if (mask & CUP_MASKCALL) smask[i++] = 'c';
  if (mask & CUP_MASKRET) smask[i++] = 'r';
  if (mask & CUP_MASKLINE) smask[i++] = 'l';
  smask[i] = '\0';
  return smask;
}


static int db_sethook (cup_State *L) {
  int arg, mask, count;
  cup_Hook func;
  cup_State *L1 = getthread(L, &arg);
  if (cup_isnoneornil(L, arg+1)) {  /* no hook? */
    cup_settop(L, arg+1);
    func = NULL; mask = 0; count = 0;  /* turn off hooks */
  }
  else {
    const char *smask = cupL_checkstring(L, arg+2);
    cupL_checktype(L, arg+1, CUP_TFUNCTION);
    count = (int)cupL_optinteger(L, arg + 3, 0);
    func = hookf; mask = makemask(smask, count);
  }
  if (!cupL_getsubtable(L, CUP_REGISTRYINDEX, HOOKKEY)) {
    /* table just created; initialize it */
    cup_pushliteral(L, "k");
    cup_setfield(L, -2, "__mode");  /** hooktable.__mode = "k" */
    cup_pushvalue(L, -1);
    cup_setmetatable(L, -2);  /* metatable(hooktable) = hooktable */
  }
  checkstack(L, L1, 1);
  cup_pushthread(L1); cup_xmove(L1, L, 1);  /* key (thread) */
  cup_pushvalue(L, arg + 1);  /* value (hook function) */
  cup_rawset(L, -3);  /* hooktable[L1] = new Cup hook */
  cup_sethook(L1, func, mask, count);
  return 0;
}


static int db_gethook (cup_State *L) {
  int arg;
  cup_State *L1 = getthread(L, &arg);
  char buff[5];
  int mask = cup_gethookmask(L1);
  cup_Hook hook = cup_gethook(L1);
  if (hook == NULL) {  /* no hook? */
    cupL_pushfail(L);
    return 1;
  }
  else if (hook != hookf)  /* external hook? */
    cup_pushliteral(L, "external hook");
  else {  /* hook table must exist */
    cup_getfield(L, CUP_REGISTRYINDEX, HOOKKEY);
    checkstack(L, L1, 1);
    cup_pushthread(L1); cup_xmove(L1, L, 1);
    cup_rawget(L, -2);   /* 1st result = hooktable[L1] */
    cup_remove(L, -2);  /* remove hook table */
  }
  cup_pushstring(L, unmakemask(mask, buff));  /* 2nd result = mask */
  cup_pushinteger(L, cup_gethookcount(L1));  /* 3rd result = count */
  return 3;
}


static int db_debug (cup_State *L) {
  for (;;) {
    char buffer[250];
    cup_writestringerror("%s", "cup_debug> ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL ||
        strcmp(buffer, "cont\n") == 0)
      return 0;
    if (cupL_loadbuffer(L, buffer, strlen(buffer), "=(debug command)") ||
        cup_pcall(L, 0, 0, 0))
      cup_writestringerror("%s\n", cupL_tolstring(L, -1, NULL));
    cup_settop(L, 0);  /* remove eventual returns */
  }
}


static int db_traceback (cup_State *L) {
  int arg;
  cup_State *L1 = getthread(L, &arg);
  const char *msg = cup_tostring(L, arg + 1);
  if (msg == NULL && !cup_isnoneornil(L, arg + 1))  /* non-string 'msg'? */
    cup_pushvalue(L, arg + 1);  /* return it untouched */
  else {
    int level = (int)cupL_optinteger(L, arg + 2, (L == L1) ? 1 : 0);
    cupL_traceback(L, L1, msg, level);
  }
  return 1;
}


static int db_setcstacklimit (cup_State *L) {
  int limit = (int)cupL_checkinteger(L, 1);
  int res = cup_setcstacklimit(L, limit);
  cup_pushinteger(L, res);
  return 1;
}


static const cupL_Reg dblib[] = {
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


CUPMOD_API int cupopen_debug (cup_State *L) {
  cupL_newlib(L, dblib);
  return 1;
}

