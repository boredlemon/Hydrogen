/*
** $Id: corolib.c $
** Coroutine Library
** See Copyright Notice in nebula.h
*/

#define corolib_c
#define NEBULA_LIB

#include "prefix.h"


#include <stdlib.h>

#include "nebula.h"

#include "auxlib.h"
#include "nebulalib.h"


static nebula_State *getco (nebula_State *L) {
  nebula_State *co = nebula_tothread(L, 1);
  nebulaL_argexpected(L, co, 1, "thread");
  return co;
}


/*
** Resumes a coroutine. Returns the number of results for non-error
** cases or -1 for errors.
*/
static int auxresume (nebula_State *L, nebula_State *co, int narg) {
  int status, nres;
  if (l_unlikely(!nebula_checkstack(co, narg))) {
    nebula_pushliteral(L, "too many arguments to resume");
    return -1;  /* error flag */
  }
  nebula_xmove(L, co, narg);
  status = nebula_resume(co, L, narg, &nres);
  if (l_likely(status == NEBULA_OK || status == NEBULA_YIELD)) {
    if (l_unlikely(!nebula_checkstack(L, nres + 1))) {
      nebula_pop(co, nres);  /* remove results anyway */
      nebula_pushliteral(L, "too many results to resume");
      return -1;  /* error flag */
    }
    nebula_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    nebula_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}


static int nebulaB_coresume (nebula_State *L) {
  nebula_State *co = getco(L);
  int r;
  r = auxresume(L, co, nebula_gettop(L) - 1);
  if (l_unlikely(r < 0)) {
    nebula_pushboolean(L, 0);
    nebula_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    nebula_pushboolean(L, 1);
    nebula_insert(L, -(r + 1));
    return r + 1;  /* return true + 'resume' returns */
  }
}


static int nebulaB_auxwrap (nebula_State *L) {
  nebula_State *co = nebula_tothread(L, nebula_upvalueindex(1));
  int r = auxresume(L, co, nebula_gettop(L));
  if (l_unlikely(r < 0)) {  /* error? */
    int stat = nebula_status(co);
    if (stat != NEBULA_OK && stat != NEBULA_YIELD) {  /* error in the coroutine? */
      stat = nebula_resetthread(co);  /* close its tbc variables */
      nebula_assert(stat != NEBULA_OK);
      nebula_xmove(co, L, 1);  /* move error message to the caller */
    }
    if (stat != NEBULA_ERRMEM &&  /* not a memory error and ... */
        nebula_type(L, -1) == NEBULA_TSTRING) {  /* ... error object is a string? */
      nebulaL_where(L, 1);  /* add extra info, if available */
      nebula_insert(L, -2);
      nebula_concat(L, 2);
    }
    return nebula_error(L);  /* propagate error */
  }
  return r;
}


static int nebulaB_cocreate (nebula_State *L) {
  nebula_State *NL;
  nebulaL_checktype(L, 1, NEBULA_TFUNCTION);
  NL = nebula_newthread(L);
  nebula_pushvalue(L, 1);  /* move function to top */
  nebula_xmove(L, NL, 1);  /* move function from L to NL */
  return 1;
}


static int nebulaB_cowrap (nebula_State *L) {
  nebulaB_cocreate(L);
  nebula_pushcclosure(L, nebulaB_auxwrap, 1);
  return 1;
}


static int nebulaB_yield (nebula_State *L) {
  return nebula_yield(L, nebula_gettop(L));
}


#define COS_RUN		0
#define COS_DEAD	1
#define COS_YIELD	2
#define COS_NORM	3


static const char *const statname[] =
  {"running", "dead", "suspended", "normal"};


static int auxstatus (nebula_State *L, nebula_State *co) {
  if (L == co) return COS_RUN;
  else {
    switch (nebula_status(co)) {
      case NEBULA_YIELD:
        return COS_YIELD;
      case NEBULA_OK: {
        nebula_Debug ar;
        if (nebula_getstack(co, 0, &ar))  /* does it have frames? */
          return COS_NORM;  /* it is running */
        else if (nebula_gettop(co) == 0)
            return COS_DEAD;
        else
          return COS_YIELD;  /* initial state */
      }
      default:  /* some error occurred */
        return COS_DEAD;
    }
  }
}


static int nebulaB_costatus (nebula_State *L) {
  nebula_State *co = getco(L);
  nebula_pushstring(L, statname[auxstatus(L, co)]);
  return 1;
}


static int nebulaB_yieldable (nebula_State *L) {
  nebula_State *co = nebula_isnone(L, 1) ? L : getco(L);
  nebula_pushboolean(L, nebula_isyieldable(co));
  return 1;
}


static int nebulaB_corunning (nebula_State *L) {
  int ismain = nebula_pushthread(L);
  nebula_pushboolean(L, ismain);
  return 2;
}


static int nebulaB_close (nebula_State *L) {
  nebula_State *co = getco(L);
  int status = auxstatus(L, co);
  switch (status) {
    case COS_DEAD: case COS_YIELD: {
      status = nebula_resetthread(co);
      if (status == NEBULA_OK) {
        nebula_pushboolean(L, 1);
        return 1;
      }
      else {
        nebula_pushboolean(L, 0);
        nebula_xmove(co, L, 1);  /* move error message */
        return 2;
      }
    }
    default:  /* normal or running coroutine */
      return nebulaL_error(L, "cannot close a %s coroutine", statname[status]);
  }
}


static const nebulaL_Reg co_funcs[] = {
  {"create", nebulaB_cocreate},
  {"resume", nebulaB_coresume},
  {"running", nebulaB_corunning},
  {"status", nebulaB_costatus},
  {"wrap", nebulaB_cowrap},
  {"yield", nebulaB_yield},
  {"isyieldable", nebulaB_yieldable},
  {"close", nebulaB_close},
  {NULL, NULL}
};



NEBULAMOD_API int nebulaopen_coroutine (nebula_State *L) {
  nebulaL_newlib(L, co_funcs);
  return 1;
}

