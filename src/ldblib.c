/*
** $Id: ldblib.c $
** Interface from Acorn to its debug API
** See Copyright Notice in acorn.h
*/

#define ldblib_c
#define ACORN_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acorn.h"

#include "lauxlib.h"
#include "acornlib.h"


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
static void checkstack (acorn_State *L, acorn_State *L1, int n) {
  if (l_unlikely(L != L1 && !acorn_checkstack(L1, n)))
    acornL_error(L, "stack overflow");
}


static int db_getregistry (acorn_State *L) {
  acorn_pushvalue(L, ACORN_REGISTRYINDEX);
  return 1;
}


static int db_getmetatable (acorn_State *L) {
  acornL_checkany(L, 1);
  if (!acorn_getmetatable(L, 1)) {
    acorn_pushnil(L);  /* no metatable */
  }
  return 1;
}


static int db_setmetatable (acorn_State *L) {
  int t = acorn_type(L, 2);
  acornL_argexpected(L, t == ACORN_TNIL || t == ACORN_TTABLE, 2, "nil or table");
  acorn_settop(L, 2);
  acorn_setmetatable(L, 1);
  return 1;  /* return 1st argument */
}


static int db_getuservalue (acorn_State *L) {
  int n = (int)acornL_optinteger(L, 2, 1);
  if (acorn_type(L, 1) != ACORN_TUSERDATA)
    acornL_pushfail(L);
  else if (acorn_getiuservalue(L, 1, n) != ACORN_TNONE) {
    acorn_pushboolean(L, 1);
    return 2;
  }
  return 1;
}


static int db_setuservalue (acorn_State *L) {
  int n = (int)acornL_optinteger(L, 3, 1);
  acornL_checktype(L, 1, ACORN_TUSERDATA);
  acornL_checkany(L, 2);
  acorn_settop(L, 2);
  if (!acorn_setiuservalue(L, 1, n))
    acornL_pushfail(L);
  return 1;
}


/*
** Auxiliary function used by several library functions: check for
** an optional thread as function's first argument and set 'arg' with
** 1 if this argument is present (so that functions can skip it to
** access their other arguments)
*/
static acorn_State *getthread (acorn_State *L, int *arg) {
  if (acorn_isthread(L, 1)) {
    *arg = 1;
    return acorn_tothread(L, 1);
  }
  else {
    *arg = 0;
    return L;  /* function will operate over current thread */
  }
}


/*
** Variations of 'acorn_settable', used by 'db_getinfo' to put results
** from 'acorn_getinfo' into result table. Key is always a string;
** value can be a string, an int, or a boolean.
*/
static void settabss (acorn_State *L, const char *k, const char *v) {
  acorn_pushstring(L, v);
  acorn_setfield(L, -2, k);
}

static void settabsi (acorn_State *L, const char *k, int v) {
  acorn_pushinteger(L, v);
  acorn_setfield(L, -2, k);
}

static void settabsb (acorn_State *L, const char *k, int v) {
  acorn_pushboolean(L, v);
  acorn_setfield(L, -2, k);
}


/*
** In function 'db_getinfo', the call to 'acorn_getinfo' may push
** results on the stack; later it creates the result table to put
** these objects. Function 'treatstackoption' puts the result from
** 'acorn_getinfo' on top of the result table so that it can call
** 'acorn_setfield'.
*/
static void treatstackoption (acorn_State *L, acorn_State *L1, const char *fname) {
  if (L == L1)
    acorn_rotate(L, -2, 1);  /* exchange object and table */
  else
    acorn_xmove(L1, L, 1);  /* move object to the "main" stack */
  acorn_setfield(L, -2, fname);  /* put object into table */
}


/*
** Calls 'acorn_getinfo' and collects all results in a new table.
** L1 needs stack space for an optional input (function) plus
** two optional outputs (function and line table) from function
** 'acorn_getinfo'.
*/
static int db_getinfo (acorn_State *L) {
  acorn_Debug ar;
  int arg;
  acorn_State *L1 = getthread(L, &arg);
  const char *options = acornL_optstring(L, arg+2, "flnSrtu");
  checkstack(L, L1, 3);
  acornL_argcheck(L, options[0] != '>', arg + 2, "invalid option '>'");
  if (acorn_isfunction(L, arg + 1)) {  /* info about a function? */
    options = acorn_pushfstring(L, ">%s", options);  /* add '>' to 'options' */
    acorn_pushvalue(L, arg + 1);  /* move function to 'L1' stack */
    acorn_xmove(L, L1, 1);
  }
  else {  /* stack level */
    if (!acorn_getstack(L1, (int)acornL_checkinteger(L, arg + 1), &ar)) {
      acornL_pushfail(L);  /* level out of range */
      return 1;
    }
  }
  if (!acorn_getinfo(L1, options, &ar))
    return acornL_argerror(L, arg+2, "invalid option");
  acorn_newtable(L);  /* table to collect results */
  if (strchr(options, 'S')) {
    acorn_pushlstring(L, ar.source, ar.srclen);
    acorn_setfield(L, -2, "source");
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


static int db_getlocal (acorn_State *L) {
  int arg;
  acorn_State *L1 = getthread(L, &arg);
  int nvar = (int)acornL_checkinteger(L, arg + 2);  /* local-variable index */
  if (acorn_isfunction(L, arg + 1)) {  /* function argument? */
    acorn_pushvalue(L, arg + 1);  /* push function */
    acorn_pushstring(L, acorn_getlocal(L, NULL, nvar));  /* push local name */
    return 1;  /* return only name (there is no value) */
  }
  else {  /* stack-level argument */
    acorn_Debug ar;
    const char *name;
    int level = (int)acornL_checkinteger(L, arg + 1);
    if (l_unlikely(!acorn_getstack(L1, level, &ar)))  /* out of range? */
      return acornL_argerror(L, arg+1, "level out of range");
    checkstack(L, L1, 1);
    name = acorn_getlocal(L1, &ar, nvar);
    if (name) {
      acorn_xmove(L1, L, 1);  /* move local value */
      acorn_pushstring(L, name);  /* push name */
      acorn_rotate(L, -2, 1);  /* re-order */
      return 2;
    }
    else {
      acornL_pushfail(L);  /* no name (nor value) */
      return 1;
    }
  }
}


static int db_setlocal (acorn_State *L) {
  int arg;
  const char *name;
  acorn_State *L1 = getthread(L, &arg);
  acorn_Debug ar;
  int level = (int)acornL_checkinteger(L, arg + 1);
  int nvar = (int)acornL_checkinteger(L, arg + 2);
  if (l_unlikely(!acorn_getstack(L1, level, &ar)))  /* out of range? */
    return acornL_argerror(L, arg+1, "level out of range");
  acornL_checkany(L, arg+3);
  acorn_settop(L, arg+3);
  checkstack(L, L1, 1);
  acorn_xmove(L, L1, 1);
  name = acorn_setlocal(L1, &ar, nvar);
  if (name == NULL)
    acorn_pop(L1, 1);  /* pop value (if not popped by 'acorn_setlocal') */
  acorn_pushstring(L, name);
  return 1;
}


/*
** get (if 'get' is true) or set an upvalue from a closure
*/
static int auxupvalue (acorn_State *L, int get) {
  const char *name;
  int n = (int)acornL_checkinteger(L, 2);  /* upvalue index */
  acornL_checktype(L, 1, ACORN_TFUNCTION);  /* closure */
  name = get ? acorn_getupvalue(L, 1, n) : acorn_setupvalue(L, 1, n);
  if (name == NULL) return 0;
  acorn_pushstring(L, name);
  acorn_insert(L, -(get+1));  /* no-op if get is false */
  return get + 1;
}


static int db_getupvalue (acorn_State *L) {
  return auxupvalue(L, 1);
}


static int db_setupvalue (acorn_State *L) {
  acornL_checkany(L, 3);
  return auxupvalue(L, 0);
}


/*
** Check whether a given upvalue from a given closure exists and
** returns its index
*/
static void *checkupval (acorn_State *L, int argf, int argnup, int *pnup) {
  void *id;
  int nup = (int)acornL_checkinteger(L, argnup);  /* upvalue index */
  acornL_checktype(L, argf, ACORN_TFUNCTION);  /* closure */
  id = acorn_upvalueid(L, argf, nup);
  if (pnup) {
    acornL_argcheck(L, id != NULL, argnup, "invalid upvalue index");
    *pnup = nup;
  }
  return id;
}


static int db_upvalueid (acorn_State *L) {
  void *id = checkupval(L, 1, 2, NULL);
  if (id != NULL)
    acorn_pushlightuserdata(L, id);
  else
    acornL_pushfail(L);
  return 1;
}


static int db_upvaluejoin (acorn_State *L) {
  int n1, n2;
  checkupval(L, 1, 2, &n1);
  checkupval(L, 3, 4, &n2);
  acornL_argcheck(L, !acorn_iscfunction(L, 1), 1, "Acorn function expected");
  acornL_argcheck(L, !acorn_iscfunction(L, 3), 3, "Acorn function expected");
  acorn_upvaluejoin(L, 1, n1, 3, n2);
  return 0;
}


/*
** Call hook function registered at hook table for the current
** thread (if there is one)
*/
static void hookf (acorn_State *L, acorn_Debug *ar) {
  static const char *const hooknames[] =
    {"call", "return", "line", "count", "tail call"};
  acorn_getfield(L, ACORN_REGISTRYINDEX, HOOKKEY);
  acorn_pushthread(L);
  if (acorn_rawget(L, -2) == ACORN_TFUNCTION) {  /* is there a hook function? */
    acorn_pushstring(L, hooknames[(int)ar->event]);  /* push event name */
    if (ar->currentline >= 0)
      acorn_pushinteger(L, ar->currentline);  /* push current line */
    else acorn_pushnil(L);
    acorn_assert(acorn_getinfo(L, "lS", ar));
    acorn_call(L, 2, 0);  /* call hook function */
  }
}


/*
** Convert a string mask (for 'sethook') into a bit mask
*/
static int makemask (const char *smask, int count) {
  int mask = 0;
  if (strchr(smask, 'c')) mask |= ACORN_MASKCALL;
  if (strchr(smask, 'r')) mask |= ACORN_MASKRET;
  if (strchr(smask, 'l')) mask |= ACORN_MASKLINE;
  if (count > 0) mask |= ACORN_MASKCOUNT;
  return mask;
}


/*
** Convert a bit mask (for 'gethook') into a string mask
*/
static char *unmakemask (int mask, char *smask) {
  int i = 0;
  if (mask & ACORN_MASKCALL) smask[i++] = 'c';
  if (mask & ACORN_MASKRET) smask[i++] = 'r';
  if (mask & ACORN_MASKLINE) smask[i++] = 'l';
  smask[i] = '\0';
  return smask;
}


static int db_sethook (acorn_State *L) {
  int arg, mask, count;
  acorn_Hook func;
  acorn_State *L1 = getthread(L, &arg);
  if (acorn_isnoneornil(L, arg+1)) {  /* no hook? */
    acorn_settop(L, arg+1);
    func = NULL; mask = 0; count = 0;  /* turn off hooks */
  }
  else {
    const char *smask = acornL_checkstring(L, arg+2);
    acornL_checktype(L, arg+1, ACORN_TFUNCTION);
    count = (int)acornL_optinteger(L, arg + 3, 0);
    func = hookf; mask = makemask(smask, count);
  }
  if (!acornL_getsubtable(L, ACORN_REGISTRYINDEX, HOOKKEY)) {
    /* table just created; initialize it */
    acorn_pushliteral(L, "k");
    acorn_setfield(L, -2, "__mode");  /** hooktable.__mode = "k" */
    acorn_pushvalue(L, -1);
    acorn_setmetatable(L, -2);  /* metatable(hooktable) = hooktable */
  }
  checkstack(L, L1, 1);
  acorn_pushthread(L1); acorn_xmove(L1, L, 1);  /* key (thread) */
  acorn_pushvalue(L, arg + 1);  /* value (hook function) */
  acorn_rawset(L, -3);  /* hooktable[L1] = new Acorn hook */
  acorn_sethook(L1, func, mask, count);
  return 0;
}


static int db_gethook (acorn_State *L) {
  int arg;
  acorn_State *L1 = getthread(L, &arg);
  char buff[5];
  int mask = acorn_gethookmask(L1);
  acorn_Hook hook = acorn_gethook(L1);
  if (hook == NULL) {  /* no hook? */
    acornL_pushfail(L);
    return 1;
  }
  else if (hook != hookf)  /* external hook? */
    acorn_pushliteral(L, "external hook");
  else {  /* hook table must exist */
    acorn_getfield(L, ACORN_REGISTRYINDEX, HOOKKEY);
    checkstack(L, L1, 1);
    acorn_pushthread(L1); acorn_xmove(L1, L, 1);
    acorn_rawget(L, -2);   /* 1st result = hooktable[L1] */
    acorn_remove(L, -2);  /* remove hook table */
  }
  acorn_pushstring(L, unmakemask(mask, buff));  /* 2nd result = mask */
  acorn_pushinteger(L, acorn_gethookcount(L1));  /* 3rd result = count */
  return 3;
}


static int db_debug (acorn_State *L) {
  for (;;) {
    char buffer[250];
    acorn_writestringerror("%s", "acorn_debug> ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL ||
        strcmp(buffer, "cont\n") == 0)
      return 0;
    if (acornL_loadbuffer(L, buffer, strlen(buffer), "=(debug command)") ||
        acorn_pcall(L, 0, 0, 0))
      acorn_writestringerror("%s\n", acornL_tolstring(L, -1, NULL));
    acorn_settop(L, 0);  /* remove eventual returns */
  }
}


static int db_traceback (acorn_State *L) {
  int arg;
  acorn_State *L1 = getthread(L, &arg);
  const char *msg = acorn_tostring(L, arg + 1);
  if (msg == NULL && !acorn_isnoneornil(L, arg + 1))  /* non-string 'msg'? */
    acorn_pushvalue(L, arg + 1);  /* return it untouched */
  else {
    int level = (int)acornL_optinteger(L, arg + 2, (L == L1) ? 1 : 0);
    acornL_traceback(L, L1, msg, level);
  }
  return 1;
}


static int db_setcstacklimit (acorn_State *L) {
  int limit = (int)acornL_checkinteger(L, 1);
  int res = acorn_setcstacklimit(L, limit);
  acorn_pushinteger(L, res);
  return 1;
}


static const acornL_Reg dblib[] = {
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


ACORNMOD_API int acornopen_debug (acorn_State *L) {
  acornL_newlib(L, dblib);
  return 1;
}

