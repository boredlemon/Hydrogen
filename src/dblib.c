/*
** $Id: dblib.c $
** Interface from Hydrogen to its debug API
** See Copyright Notice in hydrogen.h
*/

#define dblib_c
#define HYDROGEN_LIB

#include "prefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hydrogen.h"

#include "auxlib.h"
#include "hydrogenlib.h"

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
static void checkstack (hydrogen_State *L, hydrogen_State *L1, int n) {
  if (l_unlikely(L != L1 && !hydrogen_checkstack(L1, n)))
    hydrogenL_error(L, "stack overflow");
}


static int db_getregistry (hydrogen_State *L) {
  hydrogen_pushvalue(L, HYDROGEN_REGISTRYINDEX);
  return 1;
}


static int db_getmetatable (hydrogen_State *L) {
  hydrogenL_checkany(L, 1);
  if (!hydrogen_getmetatable(L, 1)) {
    hydrogen_pushnil(L);  /* no metatable */
  }
  return 1;
}


static int db_setmetatable (hydrogen_State *L) {
  int t = hydrogen_type(L, 2);
  hydrogenL_argexpected(L, t == HYDROGEN_TNIL || t == HYDROGEN_TTABLE, 2, "nil or table");
  hydrogen_settop(L, 2);
  hydrogen_setmetatable(L, 1);
  return 1;  /* return 1st argument */
}


static int db_getuservalue (hydrogen_State *L) {
  int n = (int)hydrogenL_optinteger(L, 2, 1);
  if (hydrogen_type(L, 1) != HYDROGEN_TUSERDATA)
    hydrogenL_pushfail(L);
  else if (hydrogen_getiuservalue(L, 1, n) != HYDROGEN_TNONE) {
    hydrogen_pushboolean(L, 1);
    return 2;
  }
  return 1;
}


static int db_setuservalue (hydrogen_State *L) {
  int n = (int)hydrogenL_optinteger(L, 3, 1);
  hydrogenL_checktype(L, 1, HYDROGEN_TUSERDATA);
  hydrogenL_checkany(L, 2);
  hydrogen_settop(L, 2);
  if (!hydrogen_setiuservalue(L, 1, n))
    hydrogenL_pushfail(L);
  return 1;
}


/*
** Auxiliary function used by several library functions: check for
** an optional thread as function's first argument and set 'arg' with
** 1 if this argument is present (so that functions can skip it to
** access their other arguments)
*/
static hydrogen_State *getthread (hydrogen_State *L, int *arg) {
  if (hydrogen_isthread(L, 1)) {
    *arg = 1;
    return hydrogen_tothread(L, 1);
  }
  else {
    *arg = 0;
    return L;  /* function will operate over current thread */
  }
}


/*
** Variations of 'hydrogen_settable', used by 'db_getinfo' to put results
** from 'hydrogen_getinfo' into result table. Key is always a string;
** value can be a string, an int, or a boolean.
*/
static void settabss (hydrogen_State *L, const char *k, const char *v) {
  hydrogen_pushstring(L, v);
  hydrogen_setfield(L, -2, k);
}

static void settabsi (hydrogen_State *L, const char *k, int v) {
  hydrogen_pushinteger(L, v);
  hydrogen_setfield(L, -2, k);
}

static void settabsb (hydrogen_State *L, const char *k, int v) {
  hydrogen_pushboolean(L, v);
  hydrogen_setfield(L, -2, k);
}


/*
** In function 'db_getinfo', the call to 'hydrogen_getinfo' may push
** results on the stack; later it creates the result table to put
** these objects. Function 'treatstackoption' puts the result from
** 'hydrogen_getinfo' on top of the result table so that it can call
** 'hydrogen_setfield'.
*/
static void treatstackoption (hydrogen_State *L, hydrogen_State *L1, const char *fname) {
  if (L == L1)
    hydrogen_rotate(L, -2, 1);  /* exchange object and table */
  else
    hydrogen_xmove(L1, L, 1);  /* move object to the "main" stack */
  hydrogen_setfield(L, -2, fname);  /* put object into table */
}


/*
** Calls 'hydrogen_getinfo' and collects all results in a new table.
** L1 needs stack space for an optional input (function) plus
** two optional outputs (function and line table) from function
** 'hydrogen_getinfo'.
*/
static int db_getinfo (hydrogen_State *L) {
  hydrogen_Debug ar;
  int arg;
  hydrogen_State *L1 = getthread(L, &arg);
  const char *options = hydrogenL_optstring(L, arg+2, "flnSrtu");
  checkstack(L, L1, 3);
  hydrogenL_argcheck(L, options[0] != '>', arg + 2, "invalid option '>'");
  if (hydrogen_isfunction(L, arg + 1)) {  /* info about a function? */
    options = hydrogen_pushfstring(L, ">%s", options);  /* add '>' to 'options' */
    hydrogen_pushvalue(L, arg + 1);  /* move function to 'L1' stack */
    hydrogen_xmove(L, L1, 1);
  }
  else {  /* stack level */
    if (!hydrogen_getstack(L1, (int)hydrogenL_checkinteger(L, arg + 1), &ar)) {
      hydrogenL_pushfail(L);  /* level out of range */
      return 1;
    }
  }
  if (!hydrogen_getinfo(L1, options, &ar))
    return hydrogenL_argerror(L, arg+2, "invalid option");
  hydrogen_newtable(L);  /* table to collect results */
  if (strchr(options, 'S')) {
    hydrogen_pushlstring(L, ar.source, ar.srclen);
    hydrogen_setfield(L, -2, "source");
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


static int db_getlocal (hydrogen_State *L) {
  int arg;
  hydrogen_State *L1 = getthread(L, &arg);
  int nvar = (int)hydrogenL_checkinteger(L, arg + 2);  /* local-variable index */
  if (hydrogen_isfunction(L, arg + 1)) {  /* function argument? */
    hydrogen_pushvalue(L, arg + 1);  /* push function */
    hydrogen_pushstring(L, hydrogen_getlocal(L, NULL, nvar));  /* push local name */
    return 1;  /* return only name (there is no value) */
  }
  else {  /* stack-level argument */
    hydrogen_Debug ar;
    const char *name;
    int level = (int)hydrogenL_checkinteger(L, arg + 1);
    if (l_unlikely(!hydrogen_getstack(L1, level, &ar)))  /* out of range? */
      return hydrogenL_argerror(L, arg+1, "level out of range");
    checkstack(L, L1, 1);
    name = hydrogen_getlocal(L1, &ar, nvar);
    if (name) {
      hydrogen_xmove(L1, L, 1);  /* move local value */
      hydrogen_pushstring(L, name);  /* push name */
      hydrogen_rotate(L, -2, 1);  /* re-order */
      return 2;
    }
    else {
      hydrogenL_pushfail(L);  /* no name (nor value) */
      return 1;
    }
  }
}


static int db_setlocal (hydrogen_State *L) {
  int arg;
  const char *name;
  hydrogen_State *L1 = getthread(L, &arg);
  hydrogen_Debug ar;
  int level = (int)hydrogenL_checkinteger(L, arg + 1);
  int nvar = (int)hydrogenL_checkinteger(L, arg + 2);
  if (l_unlikely(!hydrogen_getstack(L1, level, &ar)))  /* out of range? */
    return hydrogenL_argerror(L, arg+1, "level out of range");
  hydrogenL_checkany(L, arg+3);
  hydrogen_settop(L, arg+3);
  checkstack(L, L1, 1);
  hydrogen_xmove(L, L1, 1);
  name = hydrogen_setlocal(L1, &ar, nvar);
  if (name == NULL)
    hydrogen_pop(L1, 1);  /* pop value (if not popped by 'hydrogen_setlocal') */
  hydrogen_pushstring(L, name);
  return 1;
}


/*
** get (if 'get' is true) or set an upvalue from a closure
*/
static int auxupvalue (hydrogen_State *L, int get) {
  const char *name;
  int n = (int)hydrogenL_checkinteger(L, 2);  /* upvalue index */
  hydrogenL_checktype(L, 1, HYDROGEN_TFUNCTION);  /* closure */
  name = get ? hydrogen_getupvalue(L, 1, n) : hydrogen_setupvalue(L, 1, n);
  if (name == NULL) return 0;
  hydrogen_pushstring(L, name);
  hydrogen_insert(L, -(get+1));  /* no-op if get is false */
  return get + 1;
}


static int db_getupvalue (hydrogen_State *L) {
  return auxupvalue(L, 1);
}


static int db_setupvalue (hydrogen_State *L) {
  hydrogenL_checkany(L, 3);
  return auxupvalue(L, 0);
}


/*
** Check whether a given upvalue from a given closure exists and
** returns its index
*/
static void *checkupval (hydrogen_State *L, int argf, int argnup, int *pnup) {
  void *id;
  int nup = (int)hydrogenL_checkinteger(L, argnup);  /* upvalue index */
  hydrogenL_checktype(L, argf, HYDROGEN_TFUNCTION);  /* closure */
  id = hydrogen_upvalueid(L, argf, nup);
  if (pnup) {
    hydrogenL_argcheck(L, id != NULL, argnup, "invalid upvalue index");
    *pnup = nup;
  }
  return id;
}


static int db_upvalueid (hydrogen_State *L) {
  void *id = checkupval(L, 1, 2, NULL);
  if (id != NULL)
    hydrogen_pushlightuserdata(L, id);
  else
    hydrogenL_pushfail(L);
  return 1;
}


static int db_upvaluejoin (hydrogen_State *L) {
  int n1, n2;
  checkupval(L, 1, 2, &n1);
  checkupval(L, 3, 4, &n2);
  hydrogenL_argcheck(L, !hydrogen_iscfunction(L, 1), 1, "Hydrogen function expected");
  hydrogenL_argcheck(L, !hydrogen_iscfunction(L, 3), 3, "Hydrogen function expected");
  hydrogen_upvaluejoin(L, 1, n1, 3, n2);
  return 0;
}


/*
** Call hook function registered at hook table for the current
** thread (if there is one)
*/
static void hookf (hydrogen_State *L, hydrogen_Debug *ar) {
  static const char *const hooknames[] =
    {"call", "return", "line", "count", "tail call"};
  hydrogen_getfield(L, HYDROGEN_REGISTRYINDEX, HOOKKEY);
  hydrogen_pushthread(L);
  if (hydrogen_rawget(L, -2) == HYDROGEN_TFUNCTION) {  /* is there a hook function? */
    hydrogen_pushstring(L, hooknames[(int)ar->event]);  /* push event name */
    if (ar->currentline >= 0)
      hydrogen_pushinteger(L, ar->currentline);  /* push current line */
    else hydrogen_pushnil(L);
    hydrogen_assert(hydrogen_getinfo(L, "lS", ar));
    hydrogen_call(L, 2, 0);  /* call hook function */
  }
}


/*
** Convert a string mask (for 'sethook') into a bit mask
*/
static int makemask (const char *smask, int count) {
  int mask = 0;
  if (strchr(smask, 'c')) mask |= HYDROGEN_MASKCALL;
  if (strchr(smask, 'r')) mask |= HYDROGEN_MASKRET;
  if (strchr(smask, 'l')) mask |= HYDROGEN_MASKLINE;
  if (count > 0) mask |= HYDROGEN_MASKCOUNT;
  return mask;
}


/*
** Convert a bit mask (for 'gethook') into a string mask
*/
static char *unmakemask (int mask, char *smask) {
  int i = 0;
  if (mask & HYDROGEN_MASKCALL) smask[i++] = 'c';
  if (mask & HYDROGEN_MASKRET) smask[i++] = 'r';
  if (mask & HYDROGEN_MASKLINE) smask[i++] = 'l';
  smask[i] = '\0';
  return smask;
}


static int db_sethook (hydrogen_State *L) {
  int arg, mask, count;
  hydrogen_Hook func;
  hydrogen_State *L1 = getthread(L, &arg);
  if (hydrogen_isnoneornil(L, arg+1)) {  /* no hook? */
    hydrogen_settop(L, arg+1);
    func = NULL; mask = 0; count = 0;  /* turn off hooks */
  }
  else {
    const char *smask = hydrogenL_checkstring(L, arg+2);
    hydrogenL_checktype(L, arg+1, HYDROGEN_TFUNCTION);
    count = (int)hydrogenL_optinteger(L, arg + 3, 0);
    func = hookf; mask = makemask(smask, count);
  }
  if (!hydrogenL_getsubtable(L, HYDROGEN_REGISTRYINDEX, HOOKKEY)) {
    /* table just created; initialize it */
    hydrogen_pushliteral(L, "k");
    hydrogen_setfield(L, -2, "__mode");  /** hooktable.__mode = "k" */
    hydrogen_pushvalue(L, -1);
    hydrogen_setmetatable(L, -2);  /* metatable(hooktable) = hooktable */
  }
  checkstack(L, L1, 1);
  hydrogen_pushthread(L1); hydrogen_xmove(L1, L, 1);  /* key (thread) */
  hydrogen_pushvalue(L, arg + 1);  /* value (hook function) */
  hydrogen_rawset(L, -3);  /* hooktable[L1] = new Hydrogen hook */
  hydrogen_sethook(L1, func, mask, count);
  return 0;
}


static int db_gethook (hydrogen_State *L) {
  int arg;
  hydrogen_State *L1 = getthread(L, &arg);
  char buff[5];
  int mask = hydrogen_gethookmask(L1);
  hydrogen_Hook hook = hydrogen_gethook(L1);
  if (hook == NULL) {  /* no hook? */
    hydrogenL_pushfail(L);
    return 1;
  }
  else if (hook != hookf)  /* external hook? */
    hydrogen_pushliteral(L, "external hook");
  else {  /* hook table must exist */
    hydrogen_getfield(L, HYDROGEN_REGISTRYINDEX, HOOKKEY);
    checkstack(L, L1, 1);
    hydrogen_pushthread(L1); hydrogen_xmove(L1, L, 1);
    hydrogen_rawget(L, -2);   /* 1st result = hooktable[L1] */
    hydrogen_remove(L, -2);  /* remove hook table */
  }
  hydrogen_pushstring(L, unmakemask(mask, buff));  /* 2nd result = mask */
  hydrogen_pushinteger(L, hydrogen_gethookcount(L1));  /* 3rd result = count */
  return 3;
}


static int db_debug (hydrogen_State *L) {
  for (;;) {
    char buffer[250];
    hydrogen_writestringerror("%s", "hydrogen_debug> ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL ||
        strcmp(buffer, "cont\n") == 0)
      return 0;
    if (hydrogenL_loadbuffer(L, buffer, strlen(buffer), "=(debug command)") ||
        hydrogen_pcall(L, 0, 0, 0))
      hydrogen_writestringerror("%s\n", hydrogenL_tolstring(L, -1, NULL));
    hydrogen_settop(L, 0);  /* remove eventual returns */
  }
}


static int db_traceback (hydrogen_State *L) {
  int arg;
  hydrogen_State *L1 = getthread(L, &arg);
  const char *msg = hydrogen_tostring(L, arg + 1);
  if (msg == NULL && !hydrogen_isnoneornil(L, arg + 1))  /* non-string 'msg'? */
    hydrogen_pushvalue(L, arg + 1);  /* return it untouched */
  else {
    int level = (int)hydrogenL_optinteger(L, arg + 2, (L == L1) ? 1 : 0);
    hydrogenL_traceback(L, L1, msg, level);
  }
  return 1;
}


static int db_setcstacklimit (hydrogen_State *L) {
  int limit = (int)hydrogenL_checkinteger(L, 1);
  int res = hydrogen_setcstacklimit(L, limit);
  hydrogen_pushinteger(L, res);
  return 1;
}


static const hydrogenL_Reg dblib[] = {
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


HYDROGENMOD_API int hydrogenopen_debug (hydrogen_State *L) {
  hydrogenL_newlib(L, dblib);
  return 1;
}