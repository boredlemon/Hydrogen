/*
** $Id: corolib.c $
** Coroutine Library
** See Copyright Notice in hydrogen.h
*/

#define corolib_c
#define HYDROGEN_LIB

#include "prefix.h"


#include <stdlib.h>

#include "hydrogen.h"

#include "auxlib.h"
#include "hydrogenlib.h"


static hydrogen_State *getco (hydrogen_State *L) {
  hydrogen_State *co = hydrogen_tothread(L, 1);
  hydrogenL_argexpected(L, co, 1, "thread");
  return co;
}


/*
** Resumes a coroutine. Returns the number of results for non-error
** cases or -1 for errors.
*/
static int auxresume (hydrogen_State *L, hydrogen_State *co, int narg) {
  int status, nres;
  if (l_unlikely(!hydrogen_checkstack(co, narg))) {
    hydrogen_pushliteral(L, "too many arguments to resume");
    return -1;  /* error flag */
  }
  hydrogen_xmove(L, co, narg);
  status = hydrogen_resume(co, L, narg, &nres);
  if (l_likely(status == HYDROGEN_OK || status == HYDROGEN_YIELD)) {
    if (l_unlikely(!hydrogen_checkstack(L, nres + 1))) {
      hydrogen_pop(co, nres);  /* remove results anyway */
      hydrogen_pushliteral(L, "too many results to resume");
      return -1;  /* error flag */
    }
    hydrogen_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    hydrogen_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}


static int hydrogenB_coresume (hydrogen_State *L) {
  hydrogen_State *co = getco(L);
  int r;
  r = auxresume(L, co, hydrogen_gettop(L) - 1);
  if (l_unlikely(r < 0)) {
    hydrogen_pushboolean(L, 0);
    hydrogen_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    hydrogen_pushboolean(L, 1);
    hydrogen_insert(L, -(r + 1));
    return r + 1;  /* return true + 'resume' returns */
  }
}


static int hydrogenB_auxwrap (hydrogen_State *L) {
  hydrogen_State *co = hydrogen_tothread(L, hydrogen_upvalueindex(1));
  int r = auxresume(L, co, hydrogen_gettop(L));
  if (l_unlikely(r < 0)) {  /* error? */
    int stat = hydrogen_status(co);
    if (stat != HYDROGEN_OK && stat != HYDROGEN_YIELD) {  /* error in the coroutine? */
      stat = hydrogen_resetthread(co);  /* close its tbc variables */
      hydrogen_assert(stat != HYDROGEN_OK);
      hydrogen_xmove(co, L, 1);  /* move error message to the caller */
    }
    if (stat != HYDROGEN_ERRMEM &&  /* not a memory error and ... */
        hydrogen_type(L, -1) == HYDROGEN_TSTRING) {  /* ... error object is a string? */
      hydrogenL_where(L, 1);  /* add extra info, if available */
      hydrogen_insert(L, -2);
      hydrogen_concat(L, 2);
    }
    return hydrogen_error(L);  /* propagate error */
  }
  return r;
}


static int hydrogenB_cocreate (hydrogen_State *L) {
  hydrogen_State *NL;
  hydrogenL_checktype(L, 1, HYDROGEN_TFUNCTION);
  NL = hydrogen_newthread(L);
  hydrogen_pushvalue(L, 1);  /* move function to top */
  hydrogen_xmove(L, NL, 1);  /* move function from L to NL */
  return 1;
}


static int hydrogenB_cowrap (hydrogen_State *L) {
  hydrogenB_cocreate(L);
  hydrogen_pushcclosure(L, hydrogenB_auxwrap, 1);
  return 1;
}


static int hydrogenB_yield (hydrogen_State *L) {
  return hydrogen_yield(L, hydrogen_gettop(L));
}


#define COS_RUN		0
#define COS_DEAD	1
#define COS_YIELD	2
#define COS_NORM	3


static const char *const statname[] =
  {"running", "dead", "suspended", "normal"};


static int auxstatus (hydrogen_State *L, hydrogen_State *co) {
  if (L == co) return COS_RUN;
  else {
    switch (hydrogen_status(co)) {
      case HYDROGEN_YIELD:
        return COS_YIELD;
      case HYDROGEN_OK: {
        hydrogen_Debug ar;
        if (hydrogen_getstack(co, 0, &ar))  /* does it have frames? */
          return COS_NORM;  /* it is running */
        else if (hydrogen_gettop(co) == 0)
            return COS_DEAD;
        else
          return COS_YIELD;  /* initial state */
      }
      default:  /* some error occurred */
        return COS_DEAD;
    }
  }
}


static int hydrogenB_costatus (hydrogen_State *L) {
  hydrogen_State *co = getco(L);
  hydrogen_pushstring(L, statname[auxstatus(L, co)]);
  return 1;
}


static int hydrogenB_yieldable (hydrogen_State *L) {
  hydrogen_State *co = hydrogen_isnone(L, 1) ? L : getco(L);
  hydrogen_pushboolean(L, hydrogen_isyieldable(co));
  return 1;
}


static int hydrogenB_corunning (hydrogen_State *L) {
  int ismain = hydrogen_pushthread(L);
  hydrogen_pushboolean(L, ismain);
  return 2;
}


static int hydrogenB_close (hydrogen_State *L) {
  hydrogen_State *co = getco(L);
  int status = auxstatus(L, co);
  switch (status) {
    case COS_DEAD: case COS_YIELD: {
      status = hydrogen_resetthread(co);
      if (status == HYDROGEN_OK) {
        hydrogen_pushboolean(L, 1);
        return 1;
      }
      else {
        hydrogen_pushboolean(L, 0);
        hydrogen_xmove(co, L, 1);  /* move error message */
        return 2;
      }
    }
    default:  /* normal or running coroutine */
      return hydrogenL_error(L, "cannot close a %s coroutine", statname[status]);
  }
}


static const hydrogenL_Reg co_funcs[] = {
  {"create", hydrogenB_cocreate},
  {"resume", hydrogenB_coresume},
  {"running", hydrogenB_corunning},
  {"status", hydrogenB_costatus},
  {"wrap", hydrogenB_cowrap},
  {"yield", hydrogenB_yield},
  {"isyieldable", hydrogenB_yieldable},
  {"close", hydrogenB_close},
  {NULL, NULL}
};



HYDROGENMOD_API int hydrogenopen_coroutine (hydrogen_State *L) {
  hydrogenL_newlib(L, co_funcs);
  return 1;
}

