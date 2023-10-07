/*
** $Id: lcorolib.c $
** Coroutine Library
** See Copyright Notice in cup.h
*/

#define lcorolib_c
#define CUP_LIB

#include "lprefix.h"


#include <stdlib.h>

#include "cup.h"

#include "lauxlib.h"
#include "cuplib.h"


static cup_State *getco (cup_State *L) {
  cup_State *co = cup_tothread(L, 1);
  cupL_argexpected(L, co, 1, "thread");
  return co;
}


/*
** Resumes a coroutine. Returns the number of results for non-error
** cases or -1 for errors.
*/
static int auxresume (cup_State *L, cup_State *co, int narg) {
  int status, nres;
  if (l_unlikely(!cup_checkstack(co, narg))) {
    cup_pushliteral(L, "too many arguments to resume");
    return -1;  /* error flag */
  }
  cup_xmove(L, co, narg);
  status = cup_resume(co, L, narg, &nres);
  if (l_likely(status == CUP_OK || status == CUP_YIELD)) {
    if (l_unlikely(!cup_checkstack(L, nres + 1))) {
      cup_pop(co, nres);  /* remove results anyway */
      cup_pushliteral(L, "too many results to resume");
      return -1;  /* error flag */
    }
    cup_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    cup_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}


static int cupB_coresume (cup_State *L) {
  cup_State *co = getco(L);
  int r;
  r = auxresume(L, co, cup_gettop(L) - 1);
  if (l_unlikely(r < 0)) {
    cup_pushboolean(L, 0);
    cup_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    cup_pushboolean(L, 1);
    cup_insert(L, -(r + 1));
    return r + 1;  /* return true + 'resume' returns */
  }
}


static int cupB_auxwrap (cup_State *L) {
  cup_State *co = cup_tothread(L, cup_upvalueindex(1));
  int r = auxresume(L, co, cup_gettop(L));
  if (l_unlikely(r < 0)) {  /* error? */
    int stat = cup_status(co);
    if (stat != CUP_OK && stat != CUP_YIELD) {  /* error in the coroutine? */
      stat = cup_resetthread(co);  /* close its tbc variables */
      cup_assert(stat != CUP_OK);
      cup_xmove(co, L, 1);  /* move error message to the caller */
    }
    if (stat != CUP_ERRMEM &&  /* not a memory error and ... */
        cup_type(L, -1) == CUP_TSTRING) {  /* ... error object is a string? */
      cupL_where(L, 1);  /* add extra info, if available */
      cup_insert(L, -2);
      cup_concat(L, 2);
    }
    return cup_error(L);  /* propagate error */
  }
  return r;
}


static int cupB_cocreate (cup_State *L) {
  cup_State *NL;
  cupL_checktype(L, 1, CUP_TFUNCTION);
  NL = cup_newthread(L);
  cup_pushvalue(L, 1);  /* move function to top */
  cup_xmove(L, NL, 1);  /* move function from L to NL */
  return 1;
}


static int cupB_cowrap (cup_State *L) {
  cupB_cocreate(L);
  cup_pushcclosure(L, cupB_auxwrap, 1);
  return 1;
}


static int cupB_yield (cup_State *L) {
  return cup_yield(L, cup_gettop(L));
}


#define COS_RUN		0
#define COS_DEAD	1
#define COS_YIELD	2
#define COS_NORM	3


static const char *const statname[] =
  {"running", "dead", "suspended", "normal"};


static int auxstatus (cup_State *L, cup_State *co) {
  if (L == co) return COS_RUN;
  else {
    switch (cup_status(co)) {
      case CUP_YIELD:
        return COS_YIELD;
      case CUP_OK: {
        cup_Debug ar;
        if (cup_getstack(co, 0, &ar))  /* does it have frames? */
          return COS_NORM;  /* it is running */
        else if (cup_gettop(co) == 0)
            return COS_DEAD;
        else
          return COS_YIELD;  /* initial state */
      }
      default:  /* some error occurred */
        return COS_DEAD;
    }
  }
}


static int cupB_costatus (cup_State *L) {
  cup_State *co = getco(L);
  cup_pushstring(L, statname[auxstatus(L, co)]);
  return 1;
}


static int cupB_yieldable (cup_State *L) {
  cup_State *co = cup_isnone(L, 1) ? L : getco(L);
  cup_pushboolean(L, cup_isyieldable(co));
  return 1;
}


static int cupB_corunning (cup_State *L) {
  int ismain = cup_pushthread(L);
  cup_pushboolean(L, ismain);
  return 2;
}


static int cupB_close (cup_State *L) {
  cup_State *co = getco(L);
  int status = auxstatus(L, co);
  switch (status) {
    case COS_DEAD: case COS_YIELD: {
      status = cup_resetthread(co);
      if (status == CUP_OK) {
        cup_pushboolean(L, 1);
        return 1;
      }
      else {
        cup_pushboolean(L, 0);
        cup_xmove(co, L, 1);  /* move error message */
        return 2;
      }
    }
    default:  /* normal or running coroutine */
      return cupL_error(L, "cannot close a %s coroutine", statname[status]);
  }
}


static const cupL_Reg co_funcs[] = {
  {"create", cupB_cocreate},
  {"resume", cupB_coresume},
  {"running", cupB_corunning},
  {"status", cupB_costatus},
  {"wrap", cupB_cowrap},
  {"yield", cupB_yield},
  {"isyieldable", cupB_yieldable},
  {"close", cupB_close},
  {NULL, NULL}
};



CUPMOD_API int cupopen_coroutine (cup_State *L) {
  cupL_newlib(L, co_funcs);
  return 1;
}

