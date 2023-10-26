/*
** $Id: corolib.c $
** Coroutine Library
** See Copyright Notice in viper.h
*/

#define corolib_c
#define VIPER_LIB

#include "prefix.h"


#include <stdlib.h>

#include "viper.h"

#include "auxlib.h"
#include "viperlib.h"


static viper_State *getco (viper_State *L) {
  viper_State *co = viper_tothread(L, 1);
  viperL_argexpected(L, co, 1, "thread");
  return co;
}


/*
** Resumes a coroutine. Returns the number of results for non-error
** cases or -1 for errors.
*/
static int auxresume (viper_State *L, viper_State *co, int narg) {
  int status, nres;
  if (l_unlikely(!viper_checkstack(co, narg))) {
    viper_pushliteral(L, "too many arguments to resume");
    return -1;  /* error flag */
  }
  viper_xmove(L, co, narg);
  status = viper_resume(co, L, narg, &nres);
  if (l_likely(status == VIPER_OK || status == VIPER_YIELD)) {
    if (l_unlikely(!viper_checkstack(L, nres + 1))) {
      viper_pop(co, nres);  /* remove results anyway */
      viper_pushliteral(L, "too many results to resume");
      return -1;  /* error flag */
    }
    viper_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    viper_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}


static int viperB_coresume (viper_State *L) {
  viper_State *co = getco(L);
  int r;
  r = auxresume(L, co, viper_gettop(L) - 1);
  if (l_unlikely(r < 0)) {
    viper_pushboolean(L, 0);
    viper_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    viper_pushboolean(L, 1);
    viper_insert(L, -(r + 1));
    return r + 1;  /* return true + 'resume' returns */
  }
}


static int viperB_auxwrap (viper_State *L) {
  viper_State *co = viper_tothread(L, viper_upvalueindex(1));
  int r = auxresume(L, co, viper_gettop(L));
  if (l_unlikely(r < 0)) {  /* error? */
    int stat = viper_status(co);
    if (stat != VIPER_OK && stat != VIPER_YIELD) {  /* error in the coroutine? */
      stat = viper_resetthread(co);  /* close its tbc variables */
      viper_assert(stat != VIPER_OK);
      viper_xmove(co, L, 1);  /* move error message to the caller */
    }
    if (stat != VIPER_ERRMEM &&  /* not a memory error and ... */
        viper_type(L, -1) == VIPER_TSTRING) {  /* ... error object is a string? */
      viperL_where(L, 1);  /* add extra info, if available */
      viper_insert(L, -2);
      viper_concat(L, 2);
    }
    return viper_error(L);  /* propagate error */
  }
  return r;
}


static int viperB_cocreate (viper_State *L) {
  viper_State *NL;
  viperL_checktype(L, 1, VIPER_TFUNCTION);
  NL = viper_newthread(L);
  viper_pushvalue(L, 1);  /* move function to top */
  viper_xmove(L, NL, 1);  /* move function from L to NL */
  return 1;
}


static int viperB_cowrap (viper_State *L) {
  viperB_cocreate(L);
  viper_pushcclosure(L, viperB_auxwrap, 1);
  return 1;
}


static int viperB_yield (viper_State *L) {
  return viper_yield(L, viper_gettop(L));
}


#define COS_RUN		0
#define COS_DEAD	1
#define COS_YIELD	2
#define COS_NORM	3


static const char *const statname[] =
  {"running", "dead", "suspended", "normal"};


static int auxstatus (viper_State *L, viper_State *co) {
  if (L == co) return COS_RUN;
  else {
    switch (viper_status(co)) {
      case VIPER_YIELD:
        return COS_YIELD;
      case VIPER_OK: {
        viper_Debug ar;
        if (viper_getstack(co, 0, &ar))  /* does it have frames? */
          return COS_NORM;  /* it is running */
        else if (viper_gettop(co) == 0)
            return COS_DEAD;
        else
          return COS_YIELD;  /* initial state */
      }
      default:  /* some error occurred */
        return COS_DEAD;
    }
  }
}


static int viperB_costatus (viper_State *L) {
  viper_State *co = getco(L);
  viper_pushstring(L, statname[auxstatus(L, co)]);
  return 1;
}


static int viperB_yieldable (viper_State *L) {
  viper_State *co = viper_isnone(L, 1) ? L : getco(L);
  viper_pushboolean(L, viper_isyieldable(co));
  return 1;
}


static int viperB_corunning (viper_State *L) {
  int ismain = viper_pushthread(L);
  viper_pushboolean(L, ismain);
  return 2;
}


static int viperB_close (viper_State *L) {
  viper_State *co = getco(L);
  int status = auxstatus(L, co);
  switch (status) {
    case COS_DEAD: case COS_YIELD: {
      status = viper_resetthread(co);
      if (status == VIPER_OK) {
        viper_pushboolean(L, 1);
        return 1;
      }
      else {
        viper_pushboolean(L, 0);
        viper_xmove(co, L, 1);  /* move error message */
        return 2;
      }
    }
    default:  /* normal or running coroutine */
      return viperL_error(L, "cannot close a %s coroutine", statname[status]);
  }
}


static const viperL_Reg co_funcs[] = {
  {"create", viperB_cocreate},
  {"resume", viperB_coresume},
  {"running", viperB_corunning},
  {"status", viperB_costatus},
  {"wrap", viperB_cowrap},
  {"yield", viperB_yield},
  {"isyieldable", viperB_yieldable},
  {"close", viperB_close},
  {NULL, NULL}
};



VIPERMOD_API int viperopen_coroutine (viper_State *L) {
  viperL_newlib(L, co_funcs);
  return 1;
}

