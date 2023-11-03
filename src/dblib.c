/*
** $Id: dblib.c $
** Interface from Nebula to its debug API
** See Copyright Notice in nebula.h
*/

#define dblib_c
#define NEBULA_LIB

#include "prefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nebula.h"

#include "auxlib.h"
#include "nebulalib.h"

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
static void checkstack (nebula_State *L, nebula_State *L1, int n) {
  if (l_unlikely(L != L1 && !nebula_checkstack(L1, n)))
    nebulaL_error(L, "stack overflow");
}


static int db_getregistry (nebula_State *L) {
  nebula_pushvalue(L, NEBULA_REGISTRYINDEX);
  return 1;
}


static int db_getmetatable (nebula_State *L) {
  nebulaL_checkany(L, 1);
  if (!nebula_getmetatable(L, 1)) {
    nebula_pushnil(L);  /* no metatable */
  }
  return 1;
}


static int db_setmetatable (nebula_State *L) {
  int t = nebula_type(L, 2);
  nebulaL_argexpected(L, t == NEBULA_TNIL || t == NEBULA_TTABLE, 2, "nil or table");
  nebula_settop(L, 2);
  nebula_setmetatable(L, 1);
  return 1;  /* return 1st argument */
}


static int db_getuservalue (nebula_State *L) {
  int n = (int)nebulaL_optinteger(L, 2, 1);
  if (nebula_type(L, 1) != NEBULA_TUSERDATA)
    nebulaL_pushfail(L);
  else if (nebula_getiuservalue(L, 1, n) != NEBULA_TNONE) {
    nebula_pushboolean(L, 1);
    return 2;
  }
  return 1;
}


static int db_setuservalue (nebula_State *L) {
  int n = (int)nebulaL_optinteger(L, 3, 1);
  nebulaL_checktype(L, 1, NEBULA_TUSERDATA);
  nebulaL_checkany(L, 2);
  nebula_settop(L, 2);
  if (!nebula_setiuservalue(L, 1, n))
    nebulaL_pushfail(L);
  return 1;
}


/*
** Auxiliary function used by several library functions: check for
** an optional thread as function's first argument and set 'arg' with
** 1 if this argument is present (so that functions can skip it to
** access their other arguments)
*/
static nebula_State *getthread (nebula_State *L, int *arg) {
  if (nebula_isthread(L, 1)) {
    *arg = 1;
    return nebula_tothread(L, 1);
  }
  else {
    *arg = 0;
    return L;  /* function will operate over current thread */
  }
}


/*
** Variations of 'nebula_settable', used by 'db_getinfo' to put results
** from 'nebula_getinfo' into result table. Key is always a string;
** value can be a string, an int, or a boolean.
*/
static void settabss (nebula_State *L, const char *k, const char *v) {
  nebula_pushstring(L, v);
  nebula_setfield(L, -2, k);
}

static void settabsi (nebula_State *L, const char *k, int v) {
  nebula_pushinteger(L, v);
  nebula_setfield(L, -2, k);
}

static void settabsb (nebula_State *L, const char *k, int v) {
  nebula_pushboolean(L, v);
  nebula_setfield(L, -2, k);
}


/*
** In function 'db_getinfo', the call to 'nebula_getinfo' may push
** results on the stack; later it creates the result table to put
** these objects. Function 'treatstackoption' puts the result from
** 'nebula_getinfo' on top of the result table so that it can call
** 'nebula_setfield'.
*/
static void treatstackoption (nebula_State *L, nebula_State *L1, const char *fname) {
  if (L == L1)
    nebula_rotate(L, -2, 1);  /* exchange object and table */
  else
    nebula_xmove(L1, L, 1);  /* move object to the "main" stack */
  nebula_setfield(L, -2, fname);  /* put object into table */
}


/*
** Calls 'nebula_getinfo' and collects all results in a new table.
** L1 needs stack space for an optional input (function) plus
** two optional outputs (function and line table) from function
** 'nebula_getinfo'.
*/
static int db_getinfo (nebula_State *L) {
  nebula_Debug ar;
  int arg;
  nebula_State *L1 = getthread(L, &arg);
  const char *options = nebulaL_optstring(L, arg+2, "flnSrtu");
  checkstack(L, L1, 3);
  nebulaL_argcheck(L, options[0] != '>', arg + 2, "invalid option '>'");
  if (nebula_isfunction(L, arg + 1)) {  /* info about a function? */
    options = nebula_pushfstring(L, ">%s", options);  /* add '>' to 'options' */
    nebula_pushvalue(L, arg + 1);  /* move function to 'L1' stack */
    nebula_xmove(L, L1, 1);
  }
  else {  /* stack level */
    if (!nebula_getstack(L1, (int)nebulaL_checkinteger(L, arg + 1), &ar)) {
      nebulaL_pushfail(L);  /* level out of range */
      return 1;
    }
  }
  if (!nebula_getinfo(L1, options, &ar))
    return nebulaL_argerror(L, arg+2, "invalid option");
  nebula_newtable(L);  /* table to collect results */
  if (strchr(options, 'S')) {
    nebula_pushlstring(L, ar.source, ar.srclen);
    nebula_setfield(L, -2, "source");
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


static int db_getlocal (nebula_State *L) {
  int arg;
  nebula_State *L1 = getthread(L, &arg);
  int nvar = (int)nebulaL_checkinteger(L, arg + 2);  /* local-variable index */
  if (nebula_isfunction(L, arg + 1)) {  /* function argument? */
    nebula_pushvalue(L, arg + 1);  /* push function */
    nebula_pushstring(L, nebula_getlocal(L, NULL, nvar));  /* push local name */
    return 1;  /* return only name (there is no value) */
  }
  else {  /* stack-level argument */
    nebula_Debug ar;
    const char *name;
    int level = (int)nebulaL_checkinteger(L, arg + 1);
    if (l_unlikely(!nebula_getstack(L1, level, &ar)))  /* out of range? */
      return nebulaL_argerror(L, arg+1, "level out of range");
    checkstack(L, L1, 1);
    name = nebula_getlocal(L1, &ar, nvar);
    if (name) {
      nebula_xmove(L1, L, 1);  /* move local value */
      nebula_pushstring(L, name);  /* push name */
      nebula_rotate(L, -2, 1);  /* re-order */
      return 2;
    }
    else {
      nebulaL_pushfail(L);  /* no name (nor value) */
      return 1;
    }
  }
}


static int db_setlocal (nebula_State *L) {
  int arg;
  const char *name;
  nebula_State *L1 = getthread(L, &arg);
  nebula_Debug ar;
  int level = (int)nebulaL_checkinteger(L, arg + 1);
  int nvar = (int)nebulaL_checkinteger(L, arg + 2);
  if (l_unlikely(!nebula_getstack(L1, level, &ar)))  /* out of range? */
    return nebulaL_argerror(L, arg+1, "level out of range");
  nebulaL_checkany(L, arg+3);
  nebula_settop(L, arg+3);
  checkstack(L, L1, 1);
  nebula_xmove(L, L1, 1);
  name = nebula_setlocal(L1, &ar, nvar);
  if (name == NULL)
    nebula_pop(L1, 1);  /* pop value (if not popped by 'nebula_setlocal') */
  nebula_pushstring(L, name);
  return 1;
}


/*
** get (if 'get' is true) or set an upvalue from a closure
*/
static int auxupvalue (nebula_State *L, int get) {
  const char *name;
  int n = (int)nebulaL_checkinteger(L, 2);  /* upvalue index */
  nebulaL_checktype(L, 1, NEBULA_TFUNCTION);  /* closure */
  name = get ? nebula_getupvalue(L, 1, n) : nebula_setupvalue(L, 1, n);
  if (name == NULL) return 0;
  nebula_pushstring(L, name);
  nebula_insert(L, -(get+1));  /* no-op if get is false */
  return get + 1;
}


static int db_getupvalue (nebula_State *L) {
  return auxupvalue(L, 1);
}


static int db_setupvalue (nebula_State *L) {
  nebulaL_checkany(L, 3);
  return auxupvalue(L, 0);
}


/*
** Check whether a given upvalue from a given closure exists and
** returns its index
*/
static void *checkupval (nebula_State *L, int argf, int argnup, int *pnup) {
  void *id;
  int nup = (int)nebulaL_checkinteger(L, argnup);  /* upvalue index */
  nebulaL_checktype(L, argf, NEBULA_TFUNCTION);  /* closure */
  id = nebula_upvalueid(L, argf, nup);
  if (pnup) {
    nebulaL_argcheck(L, id != NULL, argnup, "invalid upvalue index");
    *pnup = nup;
  }
  return id;
}


static int db_upvalueid (nebula_State *L) {
  void *id = checkupval(L, 1, 2, NULL);
  if (id != NULL)
    nebula_pushlightuserdata(L, id);
  else
    nebulaL_pushfail(L);
  return 1;
}


static int db_upvaluejoin (nebula_State *L) {
  int n1, n2;
  checkupval(L, 1, 2, &n1);
  checkupval(L, 3, 4, &n2);
  nebulaL_argcheck(L, !nebula_iscfunction(L, 1), 1, "Nebula function expected");
  nebulaL_argcheck(L, !nebula_iscfunction(L, 3), 3, "Nebula function expected");
  nebula_upvaluejoin(L, 1, n1, 3, n2);
  return 0;
}


/*
** Call hook function registered at hook table for the current
** thread (if there is one)
*/
static void hookf (nebula_State *L, nebula_Debug *ar) {
  static const char *const hooknames[] =
    {"call", "return", "line", "count", "tail call"};
  nebula_getfield(L, NEBULA_REGISTRYINDEX, HOOKKEY);
  nebula_pushthread(L);
  if (nebula_rawget(L, -2) == NEBULA_TFUNCTION) {  /* is there a hook function? */
    nebula_pushstring(L, hooknames[(int)ar->event]);  /* push event name */
    if (ar->currentline >= 0)
      nebula_pushinteger(L, ar->currentline);  /* push current line */
    else nebula_pushnil(L);
    nebula_assert(nebula_getinfo(L, "lS", ar));
    nebula_call(L, 2, 0);  /* call hook function */
  }
}


/*
** Convert a string mask (for 'sethook') into a bit mask
*/
static int makemask (const char *smask, int count) {
  int mask = 0;
  if (strchr(smask, 'c')) mask |= NEBULA_MASKCALL;
  if (strchr(smask, 'r')) mask |= NEBULA_MASKRET;
  if (strchr(smask, 'l')) mask |= NEBULA_MASKLINE;
  if (count > 0) mask |= NEBULA_MASKCOUNT;
  return mask;
}


/*
** Convert a bit mask (for 'gethook') into a string mask
*/
static char *unmakemask (int mask, char *smask) {
  int i = 0;
  if (mask & NEBULA_MASKCALL) smask[i++] = 'c';
  if (mask & NEBULA_MASKRET) smask[i++] = 'r';
  if (mask & NEBULA_MASKLINE) smask[i++] = 'l';
  smask[i] = '\0';
  return smask;
}


static int db_sethook (nebula_State *L) {
  int arg, mask, count;
  nebula_Hook func;
  nebula_State *L1 = getthread(L, &arg);
  if (nebula_isnoneornil(L, arg+1)) {  /* no hook? */
    nebula_settop(L, arg+1);
    func = NULL; mask = 0; count = 0;  /* turn off hooks */
  }
  else {
    const char *smask = nebulaL_checkstring(L, arg+2);
    nebulaL_checktype(L, arg+1, NEBULA_TFUNCTION);
    count = (int)nebulaL_optinteger(L, arg + 3, 0);
    func = hookf; mask = makemask(smask, count);
  }
  if (!nebulaL_getsubtable(L, NEBULA_REGISTRYINDEX, HOOKKEY)) {
    /* table just created; initialize it */
    nebula_pushliteral(L, "k");
    nebula_setfield(L, -2, "__mode");  /** hooktable.__mode = "k" */
    nebula_pushvalue(L, -1);
    nebula_setmetatable(L, -2);  /* metatable(hooktable) = hooktable */
  }
  checkstack(L, L1, 1);
  nebula_pushthread(L1); nebula_xmove(L1, L, 1);  /* key (thread) */
  nebula_pushvalue(L, arg + 1);  /* value (hook function) */
  nebula_rawset(L, -3);  /* hooktable[L1] = new Nebula hook */
  nebula_sethook(L1, func, mask, count);
  return 0;
}


static int db_gethook (nebula_State *L) {
  int arg;
  nebula_State *L1 = getthread(L, &arg);
  char buff[5];
  int mask = nebula_gethookmask(L1);
  nebula_Hook hook = nebula_gethook(L1);
  if (hook == NULL) {  /* no hook? */
    nebulaL_pushfail(L);
    return 1;
  }
  else if (hook != hookf)  /* external hook? */
    nebula_pushliteral(L, "external hook");
  else {  /* hook table must exist */
    nebula_getfield(L, NEBULA_REGISTRYINDEX, HOOKKEY);
    checkstack(L, L1, 1);
    nebula_pushthread(L1); nebula_xmove(L1, L, 1);
    nebula_rawget(L, -2);   /* 1st result = hooktable[L1] */
    nebula_remove(L, -2);  /* remove hook table */
  }
  nebula_pushstring(L, unmakemask(mask, buff));  /* 2nd result = mask */
  nebula_pushinteger(L, nebula_gethookcount(L1));  /* 3rd result = count */
  return 3;
}


static int db_debug (nebula_State *L) {
  for (;;) {
    char buffer[250];
    nebula_writestringerror("%s", "nebula_debug> ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL ||
        strcmp(buffer, "cont\n") == 0)
      return 0;
    if (nebulaL_loadbuffer(L, buffer, strlen(buffer), "=(debug command)") ||
        nebula_pcall(L, 0, 0, 0))
      nebula_writestringerror("%s\n", nebulaL_tolstring(L, -1, NULL));
    nebula_settop(L, 0);  /* remove eventual returns */
  }
}


static int db_traceback (nebula_State *L) {
  int arg;
  nebula_State *L1 = getthread(L, &arg);
  const char *msg = nebula_tostring(L, arg + 1);
  if (msg == NULL && !nebula_isnoneornil(L, arg + 1))  /* non-string 'msg'? */
    nebula_pushvalue(L, arg + 1);  /* return it untouched */
  else {
    int level = (int)nebulaL_optinteger(L, arg + 2, (L == L1) ? 1 : 0);
    nebulaL_traceback(L, L1, msg, level);
  }
  return 1;
}


static int db_setcstacklimit (nebula_State *L) {
  int limit = (int)nebulaL_checkinteger(L, 1);
  int res = nebula_setcstacklimit(L, limit);
  nebula_pushinteger(L, res);
  return 1;
}


static const nebulaL_Reg dblib[] = {
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


NEBULAMOD_API int nebulaopen_debug (nebula_State *L) {
  nebulaL_newlib(L, dblib);
  return 1;
}