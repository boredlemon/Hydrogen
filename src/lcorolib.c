/*
** $Id: lcorolib.c $
** Coroutine Library
** See Copyright Notice in acorn.h
*/

#define lcorolib_c
#define ACORN_LIB

#include "lprefix.h"


#include <stdlib.h>

#include "acorn.h"

#include "lauxlib.h"
#include "acornlib.h"


static acorn_State *getco (acorn_State *L) {
  acorn_State *co = acorn_tothread(L, 1);
  acornL_argexpected(L, co, 1, "thread");
  return co;
}


/*
** Resumes a coroutine. Returns the number of results for non-error
** cases or -1 for errors.
*/
static int auxresume (acorn_State *L, acorn_State *co, int narg) {
  int status, nres;
  if (l_unlikely(!acorn_checkstack(co, narg))) {
    acorn_pushliteral(L, "too many arguments to resume");
    return -1;  /* error flag */
  }
  acorn_xmove(L, co, narg);
  status = acorn_resume(co, L, narg, &nres);
  if (l_likely(status == ACORN_OK || status == ACORN_YIELD)) {
    if (l_unlikely(!acorn_checkstack(L, nres + 1))) {
      acorn_pop(co, nres);  /* remove results anyway */
      acorn_pushliteral(L, "too many results to resume");
      return -1;  /* error flag */
    }
    acorn_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    acorn_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}


static int acornB_coresume (acorn_State *L) {
  acorn_State *co = getco(L);
  int r;
  r = auxresume(L, co, acorn_gettop(L) - 1);
  if (l_unlikely(r < 0)) {
    acorn_pushboolean(L, 0);
    acorn_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    acorn_pushboolean(L, 1);
    acorn_insert(L, -(r + 1));
    return r + 1;  /* return true + 'resume' returns */
  }
}


static int acornB_auxwrap (acorn_State *L) {
  acorn_State *co = acorn_tothread(L, acorn_upvalueindex(1));
  int r = auxresume(L, co, acorn_gettop(L));
  if (l_unlikely(r < 0)) {  /* error? */
    int stat = acorn_status(co);
    if (stat != ACORN_OK && stat != ACORN_YIELD) {  /* error in the coroutine? */
      stat = acorn_resetthread(co);  /* close its tbc variables */
      acorn_assert(stat != ACORN_OK);
      acorn_xmove(co, L, 1);  /* move error message to the caller */
    }
    if (stat != ACORN_ERRMEM &&  /* not a memory error and ... */
        acorn_type(L, -1) == ACORN_TSTRING) {  /* ... error object is a string? */
      acornL_where(L, 1);  /* add extra info, if available */
      acorn_insert(L, -2);
      acorn_concat(L, 2);
    }
    return acorn_error(L);  /* propagate error */
  }
  return r;
}


static int acornB_cocreate (acorn_State *L) {
  acorn_State *NL;
  acornL_checktype(L, 1, ACORN_TFUNCTION);
  NL = acorn_newthread(L);
  acorn_pushvalue(L, 1);  /* move function to top */
  acorn_xmove(L, NL, 1);  /* move function from L to NL */
  return 1;
}


static int acornB_cowrap (acorn_State *L) {
  acornB_cocreate(L);
  acorn_pushcclosure(L, acornB_auxwrap, 1);
  return 1;
}


static int acornB_yield (acorn_State *L) {
  return acorn_yield(L, acorn_gettop(L));
}


#define COS_RUN		0
#define COS_DEAD	1
#define COS_YIELD	2
#define COS_NORM	3


static const char *const statname[] =
  {"running", "dead", "suspended", "normal"};


static int auxstatus (acorn_State *L, acorn_State *co) {
  if (L == co) return COS_RUN;
  else {
    switch (acorn_status(co)) {
      case ACORN_YIELD:
        return COS_YIELD;
      case ACORN_OK: {
        acorn_Debug ar;
        if (acorn_getstack(co, 0, &ar))  /* does it have frames? */
          return COS_NORM;  /* it is running */
        else if (acorn_gettop(co) == 0)
            return COS_DEAD;
        else
          return COS_YIELD;  /* initial state */
      }
      default:  /* some error occurred */
        return COS_DEAD;
    }
  }
}


static int acornB_costatus (acorn_State *L) {
  acorn_State *co = getco(L);
  acorn_pushstring(L, statname[auxstatus(L, co)]);
  return 1;
}


static int acornB_yieldable (acorn_State *L) {
  acorn_State *co = acorn_isnone(L, 1) ? L : getco(L);
  acorn_pushboolean(L, acorn_isyieldable(co));
  return 1;
}


static int acornB_corunning (acorn_State *L) {
  int ismain = acorn_pushthread(L);
  acorn_pushboolean(L, ismain);
  return 2;
}


static int acornB_close (acorn_State *L) {
  acorn_State *co = getco(L);
  int status = auxstatus(L, co);
  switch (status) {
    case COS_DEAD: case COS_YIELD: {
      status = acorn_resetthread(co);
      if (status == ACORN_OK) {
        acorn_pushboolean(L, 1);
        return 1;
      }
      else {
        acorn_pushboolean(L, 0);
        acorn_xmove(co, L, 1);  /* move error message */
        return 2;
      }
    }
    default:  /* normal or running coroutine */
      return acornL_error(L, "cannot close a %s coroutine", statname[status]);
  }
}


static const acornL_Reg co_funcs[] = {
  {"create", acornB_cocreate},
  {"resume", acornB_coresume},
  {"running", acornB_corunning},
  {"status", acornB_costatus},
  {"wrap", acornB_cowrap},
  {"yield", acornB_yield},
  {"isyieldable", acornB_yieldable},
  {"close", acornB_close},
  {NULL, NULL}
};



ACORNMOD_API int acornopen_coroutine (acorn_State *L) {
  acornL_newlib(L, co_funcs);
  return 1;
}

