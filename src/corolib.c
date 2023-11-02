/*
** $Id: corolib.c $
** Coroutine Library
** See Copyright Notice in venom.h
*/

#define corolib_c
#define VENOM_LIB

#include "prefix.h"


#include <stdlib.h>

#include "venom.h"

#include "auxlib.h"
#include "venomlib.h"


static venom_State *getco (venom_State *L) {
  venom_State *co = venom_tothread(L, 1);
  venomL_argexpected(L, co, 1, "thread");
  return co;
}


/*
** Resumes a coroutine. Returns the number of results for non-error
** cases or -1 for errors.
*/
static int auxresume (venom_State *L, venom_State *co, int narg) {
  int status, nres;
  if (l_unlikely(!venom_checkstack(co, narg))) {
    venom_pushliteral(L, "too many arguments to resume");
    return -1;  /* error flag */
  }
  venom_xmove(L, co, narg);
  status = venom_resume(co, L, narg, &nres);
  if (l_likely(status == VENOM_OK || status == VENOM_YIELD)) {
    if (l_unlikely(!venom_checkstack(L, nres + 1))) {
      venom_pop(co, nres);  /* remove results anyway */
      venom_pushliteral(L, "too many results to resume");
      return -1;  /* error flag */
    }
    venom_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    venom_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}


static int venomB_coresume (venom_State *L) {
  venom_State *co = getco(L);
  int r;
  r = auxresume(L, co, venom_gettop(L) - 1);
  if (l_unlikely(r < 0)) {
    venom_pushboolean(L, 0);
    venom_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    venom_pushboolean(L, 1);
    venom_insert(L, -(r + 1));
    return r + 1;  /* return true + 'resume' returns */
  }
}


static int venomB_auxwrap (venom_State *L) {
  venom_State *co = venom_tothread(L, venom_upvalueindex(1));
  int r = auxresume(L, co, venom_gettop(L));
  if (l_unlikely(r < 0)) {  /* error? */
    int stat = venom_status(co);
    if (stat != VENOM_OK && stat != VENOM_YIELD) {  /* error in the coroutine? */
      stat = venom_resetthread(co);  /* close its tbc variables */
      venom_assert(stat != VENOM_OK);
      venom_xmove(co, L, 1);  /* move error message to the caller */
    }
    if (stat != VENOM_ERRMEM &&  /* not a memory error and ... */
        venom_type(L, -1) == VENOM_TSTRING) {  /* ... error object is a string? */
      venomL_where(L, 1);  /* add extra info, if available */
      venom_insert(L, -2);
      venom_concat(L, 2);
    }
    return venom_error(L);  /* propagate error */
  }
  return r;
}


static int venomB_cocreate (venom_State *L) {
  venom_State *NL;
  venomL_checktype(L, 1, VENOM_TFUNCTION);
  NL = venom_newthread(L);
  venom_pushvalue(L, 1);  /* move function to top */
  venom_xmove(L, NL, 1);  /* move function from L to NL */
  return 1;
}


static int venomB_cowrap (venom_State *L) {
  venomB_cocreate(L);
  venom_pushcclosure(L, venomB_auxwrap, 1);
  return 1;
}


static int venomB_yield (venom_State *L) {
  return venom_yield(L, venom_gettop(L));
}


#define COS_RUN		0
#define COS_DEAD	1
#define COS_YIELD	2
#define COS_NORM	3


static const char *const statname[] =
  {"running", "dead", "suspended", "normal"};


static int auxstatus (venom_State *L, venom_State *co) {
  if (L == co) return COS_RUN;
  else {
    switch (venom_status(co)) {
      case VENOM_YIELD:
        return COS_YIELD;
      case VENOM_OK: {
        venom_Debug ar;
        if (venom_getstack(co, 0, &ar))  /* does it have frames? */
          return COS_NORM;  /* it is running */
        else if (venom_gettop(co) == 0)
            return COS_DEAD;
        else
          return COS_YIELD;  /* initial state */
      }
      default:  /* some error occurred */
        return COS_DEAD;
    }
  }
}


static int venomB_costatus (venom_State *L) {
  venom_State *co = getco(L);
  venom_pushstring(L, statname[auxstatus(L, co)]);
  return 1;
}


static int venomB_yieldable (venom_State *L) {
  venom_State *co = venom_isnone(L, 1) ? L : getco(L);
  venom_pushboolean(L, venom_isyieldable(co));
  return 1;
}


static int venomB_corunning (venom_State *L) {
  int ismain = venom_pushthread(L);
  venom_pushboolean(L, ismain);
  return 2;
}


static int venomB_close (venom_State *L) {
  venom_State *co = getco(L);
  int status = auxstatus(L, co);
  switch (status) {
    case COS_DEAD: case COS_YIELD: {
      status = venom_resetthread(co);
      if (status == VENOM_OK) {
        venom_pushboolean(L, 1);
        return 1;
      }
      else {
        venom_pushboolean(L, 0);
        venom_xmove(co, L, 1);  /* move error message */
        return 2;
      }
    }
    default:  /* normal or running coroutine */
      return venomL_error(L, "cannot close a %s coroutine", statname[status]);
  }
}


static const venomL_Reg co_funcs[] = {
  {"create", venomB_cocreate},
  {"resume", venomB_coresume},
  {"running", venomB_corunning},
  {"status", venomB_costatus},
  {"wrap", venomB_cowrap},
  {"yield", venomB_yield},
  {"isyieldable", venomB_yieldable},
  {"close", venomB_close},
  {NULL, NULL}
};



VENOMMOD_API int venomopen_coroutine (venom_State *L) {
  venomL_newlib(L, co_funcs);
  return 1;
}

