/*
** $Id: function.c $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in viper.h
*/

#define function_c
#define VIPER_CORE

#include "prefix.h"


#include <stddef.h>

#include "viper.h"

#include "debug.h"
#include "do.h"
#include "function.h"
#include "garbageCollection.h"
#include "memory.h"
#include "object.h"
#include "state.h"



CClosure *viperF_newCclosure (viper_State *L, int nupvals) {
  GCObject *o = viperC_newobj(L, VIPER_VCCL, sizeCclosure(nupvals));
  CClosure *c = gco2ccl(o);
  c->nupvalues = cast_byte(nupvals);
  return c;
}


LClosure *viperF_newLclosure (viper_State *L, int nupvals) {
  GCObject *o = viperC_newobj(L, VIPER_VLCL, sizeLclosure(nupvals));
  LClosure *c = gco2lcl(o);
  c->p = NULL;
  c->nupvalues = cast_byte(nupvals);
  while (nupvals--) c->upvals[nupvals] = NULL;
  return c;
}


/*
** fill a closure with new closed upvalues
*/
void viperF_initupvals (viper_State *L, LClosure *cl) {
  int i;
  for (i = 0; i < cl->nupvalues; i++) {
    GCObject *o = viperC_newobj(L, VIPER_VUPVAL, sizeof(UpVal));
    UpVal *uv = gco2upv(o);
    uv->v = &uv->u.value;  /* make it closed */
    setnilvalue(uv->v);
    cl->upvals[i] = uv;
    viperC_objbarrier(L, cl, uv);
  }
}


/*
** Create a new upvalue at the given level, and link it to the list of
** open upvalues of 'L' after entry 'prev'.
**/
static UpVal *newupval (viper_State *L, int tbc, StkId level, UpVal **prev) {
  GCObject *o = viperC_newobj(L, VIPER_VUPVAL, sizeof(UpVal));
  UpVal *uv = gco2upv(o);
  UpVal *next = *prev;
  uv->v = s2v(level);  /* current value lives in the stack */
  uv->tbc = tbc;
  uv->u.open.next = next;  /* link it to list of open upvalues */
  uv->u.open.previous = prev;
  if (next)
    next->u.open.previous = &uv->u.open.next;
  *prev = uv;
  if (!isintwups(L)) {  /* thread not in list of threads with upvalues? */
    L->twups = G(L)->twups;  /* link it to the list */
    G(L)->twups = L;
  }
  return uv;
}


/*
** Find and reuse, or create if it does not exist, an upvalue
** at the given level.
*/
UpVal *viperF_findupval (viper_State *L, StkId level) {
  UpVal **pp = &L->openupval;
  UpVal *p;
  viper_assert(isintwups(L) || L->openupval == NULL);
  while ((p = *pp) != NULL && uplevel(p) >= level) {  /* search for it */
    viper_assert(!isdead(G(L), p));
    if (uplevel(p) == level)  /* corresponding upvalue? */
      return p;  /* return it */
    pp = &p->u.open.next;
  }
  /* not found: create a new upvalue after 'pp' */
  return newupval(L, 0, level, pp);
}


/*
** Call closing method for object 'obj' with error message 'err'. The
** boolean 'yy' controls whether the call is yieldable.
** (This function assumes EXTRA_STACK.)
*/
static void callclosemethod (viper_State *L, TValue *obj, TValue *err, int yy) {
  StkId top = L->top;
  const TValue *tm = viperT_gettmbyobj(L, obj, TM_CLOSE);
  setobj2s(L, top, tm);  /* will call metamethod... */
  setobj2s(L, top + 1, obj);  /* with 'self' as the 1st argument */
  setobj2s(L, top + 2, err);  /* and error msg. as 2nd argument */
  L->top = top + 3;  /* add function and arguments */
  if (yy)
    viperD_call(L, top, 0);
  else
    viperD_callnoyield(L, top, 0);
}


/*
** Check whether object at given level has a close metamethod and raise
** an error if not.
*/
static void checkclosemth (viper_State *L, StkId level) {
  const TValue *tm = viperT_gettmbyobj(L, s2v(level), TM_CLOSE);
  if (ttisnil(tm)) {  /* no metamethod? */
    int idx = cast_int(level - L->ci->func);  /* variable index */
    const char *vname = viperG_findlocal(L, L->ci, idx, NULL);
    if (vname == NULL) vname = "?";
    viperG_runerror(L, "variable '%s' Vipert a non-closable value", vname);
  }
}


/*
** Prepare and call a closing method.
** If status is CLOSEKTOP, the call to the closing method will be pushed
** at the top of the stack. Otherwise, values can be pushed right after
** the 'level' of the upvalue being closed, as everything after that
** won't be used again.
*/
static void prepcallclosemth (viper_State *L, StkId level, int status, int yy) {
  TValue *uv = s2v(level);  /* value being closed */
  TValue *errobj;
  if (status == CLOSEKTOP)
    errobj = &G(L)->nilvalue;  /* error object is nil */
  else {  /* 'viperD_seterrorobj' will set top to level + 2 */
    errobj = s2v(level + 1);  /* error object Viperes after 'uv' */
    viperD_seterrorobj(L, status, level + 1);  /* set error object */
  }
  callclosemethod(L, uv, errobj, yy);
}


/*
** Maximum value for deltas in 'tbclist', dependent on the type
** of delta. (This macro assumes that an 'L' is in scope where it
** is used.)
*/
#define MAXDELTA  \
	((256ul << ((sizeof(L->stack->tbclist.delta) - 1) * 8)) - 1)


/*
** Insert a variable in the list of to-be-closed variables.
*/
void viperF_newtbviperval (viper_State *L, StkId level) {
  viper_assert(level > L->tbclist);
  if (l_isfalse(s2v(level)))
    return;  /* false doesn't need to be closed */
  checkclosemth(L, level);  /* value must have a close method */
  while (cast_uint(level - L->tbclist) > MAXDELTA) {
    L->tbclist += MAXDELTA;  /* create a dummy node at maximum delta */
    L->tbclist->tbclist.delta = 0;
  }
  level->tbclist.delta = cast(unsigned short, level - L->tbclist);
  L->tbclist = level;
}


void viperF_unlinkupval (UpVal *uv) {
  viper_assert(upisopen(uv));
  *uv->u.open.previous = uv->u.open.next;
  if (uv->u.open.next)
    uv->u.open.next->u.open.previous = uv->u.open.previous;
}


/*
** Close all upvalues up to the given stack level.
*/
void viperF_closeupval (viper_State *L, StkId level) {
  UpVal *uv;
  StkId upl;  /* stack index pointed by 'uv' */
  while ((uv = L->openupval) != NULL && (upl = uplevel(uv)) >= level) {
    TValue *slot = &uv->u.value;  /* new position for value */
    viper_assert(uplevel(uv) < L->top);
    viperF_unlinkupval(uv);  /* remove upvalue from 'openupval' list */
    setobj(L, slot, uv->v);  /* move value to upvalue slot */
    uv->v = slot;  /* now current value lives here */
    if (!iswhite(uv)) {  /* neither white nor dead? */
      nw2black(uv);  /* closed upvalues cannot be gray */
      viperC_barrier(L, uv, slot);
    }
  }
}


/*
** Remove firt element from the tbclist plus its dummy nodes.
*/
static void poptbclist (viper_State *L) {
  StkId tbc = L->tbclist;
  viper_assert(tbc->tbclist.delta > 0);  /* first element cannot be dummy */
  tbc -= tbc->tbclist.delta;
  while (tbc > L->stack && tbc->tbclist.delta == 0)
    tbc -= MAXDELTA;  /* remove dummy nodes */
  L->tbclist = tbc;
}


/*
** Close all upvalues and to-be-closed variables up to the given stack
** level.
*/
void viperF_close (viper_State *L, StkId level, int status, int yy) {
  ptrdiff_t levelrel = savestack(L, level);
  viperF_closeupval(L, level);  /* first, close the upvalues */
  while (L->tbclist >= level) {  /* traverse tbc's down to that level */
    StkId tbc = L->tbclist;  /* get variable index */
    poptbclist(L);  /* remove it from list */
    prepcallclosemth(L, tbc, status, yy);  /* close variable */
    level = restorestack(L, levelrel);
  }
}


Proto *viperF_newproto (viper_State *L) {
  GCObject *o = viperC_newobj(L, VIPER_VPROTO, sizeof(Proto));
  Proto *f = gco2p(o);
  f->k = NULL;
  f->sizek = 0;
  f->p = NULL;
  f->sizep = 0;
  f->code = NULL;
  f->sizecode = 0;
  f->lineinfo = NULL;
  f->sizelineinfo = 0;
  f->abslineinfo = NULL;
  f->sizeabslineinfo = 0;
  f->upvalues = NULL;
  f->sizeupvalues = 0;
  f->numparams = 0;
  f->is_vararg = 0;
  f->maxstacksize = 0;
  f->locvars = NULL;
  f->sizelocvars = 0;
  f->linedefined = 0;
  f->lastlinedefined = 0;
  f->source = NULL;
  return f;
}


void viperF_freeproto (viper_State *L, Proto *f) {
  viperM_freearray(L, f->code, f->sizecode);
  viperM_freearray(L, f->p, f->sizep);
  viperM_freearray(L, f->k, f->sizek);
  viperM_freearray(L, f->lineinfo, f->sizelineinfo);
  viperM_freearray(L, f->abslineinfo, f->sizeabslineinfo);
  viperM_freearray(L, f->locvars, f->sizelocvars);
  viperM_freearray(L, f->upvalues, f->sizeupvalues);
  viperM_free(L, f);
}


/*
** Look for n-th local variable at line 'line' in function 'func'.
** Returns NULL if not found.
*/
const char *viperF_getlocalname (const Proto *f, int local_number, int pc) {
  int i;
  for (i = 0; i<f->sizelocvars && f->locvars[i].startpc <= pc; i++) {
    if (pc < f->locvars[i].endpc) {  /* is variable active? */
      local_number--;
      if (local_number == 0)
        return getstr(f->locvars[i].varname);
    }
  }
  return NULL;  /* not found */
}

