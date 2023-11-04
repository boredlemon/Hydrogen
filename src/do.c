/*
** $Id: do.c $
** Stack and Call structure of Hydrogen
** See Copyright Notice in hydrogen.h
*/

#define do_c
#define HYDROGEN_CORE

#include "prefix.h"


#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "hydrogen.h"

#include "api.h"
#include "debug.h"
#include "do.h"
#include "function.h"
#include "garbageCollection.h"
#include "memory.h"
#include "object.h"
#include "opcodes.h"
#include "parser.h"
#include "state.h"
#include "string.h"
#include "table.h"
#include "tagMethods.h"
#include "undump.h"
#include "virtualMachine.h"
#include "zio.h"



#define errorstatus(s)	((s) > HYDROGEN_YIELD)


/*
** {======================================================
** Error-recovery functions
** =======================================================
*/

/*
** HYDROGENI_THROW/HYDROGENI_TRY define how Hydrogen does exception handling. By
** default, Hydrogen handles errors with exceptions when compiling as
** C++ code, with _longjmp/_setjmp when asked to use them, and with
** longjmp/setjmp otherwise.
*/
#if !defined(HYDROGENI_THROW)				/* { */

#if defined(__cplusplus) && !defined(HYDROGEN_USE_LONGJMP)	/* { */

/* C++ exceptions */
#define HYDROGENI_THROW(L,c)		throw(c)
#define HYDROGENI_TRY(L,c,a) \
	try { a } catch(...) { if ((c)->status == 0) (c)->status = -1; }
#define hydrogeni_jmpbuf		int  /* dummy variable */

#elif defined(HYDROGEN_USE_POSIX)				/* }{ */

/* in POSIX, try _longjmp/_setjmp (more efficient) */
#define HYDROGENI_THROW(L,c)		_longjmp((c)->b, 1)
#define HYDROGENI_TRY(L,c,a)		if (_setjmp((c)->b) == 0) { a }
#define hydrogeni_jmpbuf		jmp_buf

#else							/* }{ */

/* ISO C handling with long jumps */
#define HYDROGENI_THROW(L,c)		longjmp((c)->b, 1)
#define HYDROGENI_TRY(L,c,a)		if (setjmp((c)->b) == 0) { a }
#define hydrogeni_jmpbuf		jmp_buf

#endif							/* } */

#endif							/* } */



/* chain list of long jump buffers */
struct hydrogen_longjmp {
  struct hydrogen_longjmp *previous;
  hydrogeni_jmpbuf b;
  volatile int status;  /* error code */
};


void hydrogenD_seterrorobj (hydrogen_State *L, int errcode, StkId oldtop) {
  switch (errcode) {
    case HYDROGEN_ERRMEM: {  /* memory error? */
      setsvalue2s(L, oldtop, G(L)->memerrmsg); /* reuse preregistered msg. */
      break;
    }
    case HYDROGEN_ERRERR: {
      setsvalue2s(L, oldtop, hydrogenS_newliteral(L, "error in error handling"));
      break;
    }
    case HYDROGEN_OK: {  /* special case only for closing upvalues */
      setnilvalue(s2v(oldtop));  /* no error message */
      break;
    }
    default: {
      hydrogen_assert(errorstatus(errcode));  /* real error */
      setobjs2s(L, oldtop, L->top - 1);  /* error message on current top */
      break;
    }
  }
  L->top = oldtop + 1;
}


l_noret hydrogenD_throw (hydrogen_State *L, int errcode) {
  if (L->errorJmp) {  /* thread has an error handler? */
    L->errorJmp->status = errcode;  /* set status */
    HYDROGENI_THROW(L, L->errorJmp);  /* jump to it */
  }
  else {  /* thread has no error handler */
    global_State *g = G(L);
    errcode = hydrogenE_resetthread(L, errcode);  /* close all upvalues */
    if (g->mainthread->errorJmp) {  /* main thread has a handler? */
      setobjs2s(L, g->mainthread->top++, L->top - 1);  /* copy error obj. */
      hydrogenD_throw(g->mainthread, errcode);  /* re-throw in main thread */
    }
    else {  /* no handler at all; abort */
      if (g->panic) {  /* panic function? */
        hydrogen_unlock(L);
        g->panic(L);  /* call panic function (last chance to jump out) */
      }
      abort();
    }
  }
}


int hydrogenD_rawrunprotected (hydrogen_State *L, Pfunc f, void *ud) {
  l_uint32 oldnCcalls = L->nCcalls;
  struct hydrogen_longjmp lj;
  lj.status = HYDROGEN_OK;
  lj.previous = L->errorJmp;  /* chain new error handler */
  L->errorJmp = &lj;
  HYDROGENI_TRY(L, &lj,
    (*f)(L, ud);
  );
  L->errorJmp = lj.previous;  /* restore old error handler */
  L->nCcalls = oldnCcalls;
  return lj.status;
}

/* }====================================================== */


/*
** {==================================================================
** Stack reallocation
** ===================================================================
*/
static void correctstack (hydrogen_State *L, StkId oldstack, StkId newstack) {
  CallInfo *ci;
  UpVal *up;
  L->top = (L->top - oldstack) + newstack;
  L->tbclist = (L->tbclist - oldstack) + newstack;
  for (up = L->openupval; up != NULL; up = up->u.open.next)
    up->v = s2v((uplevel(up) - oldstack) + newstack);
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    ci->top = (ci->top - oldstack) + newstack;
    ci->func = (ci->func - oldstack) + newstack;
    if (isHydrogen(ci))
      ci->u.l.trap = 1;  /* signal to update 'trap' in 'hydrogenV_execute' */
  }
}


/* some space for error handling */
#define ERRORSTACKSIZE	(HYDROGENI_MAXSTACK + 200)


/*
** Reallocate the stack to a new size, correcting all pointers into
** it. (There are pointers to a stack from its upvalues, from its list
** of call infos, plus a few individual pointers.) The reallocation is
** done in two steps (allocation + free) because the correction must be
** done while both addresses (the old stack and the new one) are valid.
** (In ISO C, any pointer use after the pointer has been deallocated is
** undefined behavior.)
** In case of allocation error, raise an error or return false according
** to 'raiseerror'.
*/
int hydrogenD_reallocstack (hydrogen_State *L, int newsize, int raiseerror) {
  int oldsize = stacksize(L);
  int i;
  StkId newstack = hydrogenM_reallocvector(L, NULL, 0,
                                      newsize + EXTRA_STACK, StackValue);
  hydrogen_assert(newsize <= HYDROGENI_MAXSTACK || newsize == ERRORSTACKSIZE);
  if (l_unlikely(newstack == NULL)) {  /* reallocation failed? */
    if (raiseerror)
      hydrogenM_error(L);
    else return 0;  /* do not raise an error */
  }
  /* number of elements to be copied to the new stack */
  i = ((oldsize <= newsize) ? oldsize : newsize) + EXTRA_STACK;
  memcpy(newstack, L->stack, i * sizeof(StackValue));
  for (; i < newsize + EXTRA_STACK; i++)
    setnilvalue(s2v(newstack + i)); /* erase new segment */
  correctstack(L, L->stack, newstack);
  hydrogenM_freearray(L, L->stack, oldsize + EXTRA_STACK);
  L->stack = newstack;
  L->stack_last = L->stack + newsize;
  return 1;
}


/*
** Try to grow the stack by at least 'n' elements. when 'raiseerror'
** is true, raises any error; otherwise, return 0 in case of errors.
*/
int hydrogenD_growstack (hydrogen_State *L, int n, int raiseerror) {
  int size = stacksize(L);
  if (l_unlikely(size > HYDROGENI_MAXSTACK)) {
    /* if stack is larger than maximum, thread is already using the
       extra space reserved for errors, that is, thread is handling
       a stack error; cannot grow further than that. */
    hydrogen_assert(stacksize(L) == ERRORSTACKSIZE);
    if (raiseerror)
      hydrogenD_throw(L, HYDROGEN_ERRERR);  /* error inside message handler */
    return 0;  /* if not 'raiseerror', just signal it */
  }
  else {
    int newsize = 2 * size;  /* tentative new size */
    int needed = cast_int(L->top - L->stack) + n;
    if (newsize > HYDROGENI_MAXSTACK)  /* cannot cross the limit */
      newsize = HYDROGENI_MAXSTACK;
    if (newsize < needed)  /* but must respect what was asked for */
      newsize = needed;
    if (l_likely(newsize <= HYDROGENI_MAXSTACK))
      return hydrogenD_reallocstack(L, newsize, raiseerror);
    else {  /* stack overflow */
      /* add extra size to be able to handle the error message */
      hydrogenD_reallocstack(L, ERRORSTACKSIZE, raiseerror);
      if (raiseerror)
        hydrogenG_runerror(L, "stack overflow");
      return 0;
    }
  }
}


static int stackinuse (hydrogen_State *L) {
  CallInfo *ci;
  int res;
  StkId lim = L->top;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    if (lim < ci->top) lim = ci->top;
  }
  hydrogen_assert(lim <= L->stack_last);
  res = cast_int(lim - L->stack) + 1;  /* part of stack in use */
  if (res < HYDROGEN_MINSTACK)
    res = HYDROGEN_MINSTACK;  /* ensure a minimum size */
  return res;
}


/*
** If stack size is more than 3 times the current use, reduce that size
** to twice the current use. (So, the final stack size is at most 2/3 the
** previous size, and half of its entries are empty.)
** As a particular case, if stack was handling a stack overflow and now
** it is not, 'max' (limited by HYDROGENI_MAXSTACK) will be smaller than
** stacksize (equal to ERRORSTACKSIZE in this case), and so the stack
** will be reduced to a "regular" size.
*/
void hydrogenD_shrinkstack (hydrogen_State *L) {
  int inuse = stackinuse(L);
  int nsize = inuse * 2;  /* proposed new size */
  int max = inuse * 3;  /* maximum "reasonable" size */
  if (max > HYDROGENI_MAXSTACK) {
    max = HYDROGENI_MAXSTACK;  /* respect stack limit */
    if (nsize > HYDROGENI_MAXSTACK)
      nsize = HYDROGENI_MAXSTACK;
  }
  /* if thread is currently not handling a stack overflow and its
     size is larger than maximum "reasonable" size, shrink it */
  if (inuse <= HYDROGENI_MAXSTACK && stacksize(L) > max)
    hydrogenD_reallocstack(L, nsize, 0);  /* ok if that fails */
  else  /* don't change stack */
    condmovestack(L,{},{});  /* (change only for debugging) */
  hydrogenE_shrinkCI(L);  /* shrink CI list */
}


void hydrogenD_inctop (hydrogen_State *L) {
  hydrogenD_checkstack(L, 1);
  L->top++;
}

/* }================================================================== */


/*
** Call a hook for the given event. Make sure there is a hook to be
** called. (Both 'L->hook' and 'L->hookmask', which trigger this
** function, can be changed asynchronously by signals.)
*/
void hydrogenD_hook (hydrogen_State *L, int event, int line,
                              int ftransfer, int ntransfer) {
  hydrogen_Hook hook = L->hook;
  if (hook && L->allowhook) {  /* make sure there is a hook */
    int mask = CIST_HOOKED;
    CallInfo *ci = L->ci;
    ptrdiff_t top = savestack(L, L->top);  /* preserve original 'top' */
    ptrdiff_t ci_top = savestack(L, ci->top);  /* idem for 'ci->top' */
    hydrogen_Debug ar;
    ar.event = event;
    ar.currentline = line;
    ar.i_ci = ci;
    if (ntransfer != 0) {
      mask |= CIST_TRAN;  /* 'ci' has transfer information */
      ci->u2.transferinfo.ftransfer = ftransfer;
      ci->u2.transferinfo.ntransfer = ntransfer;
    }
    if (isHydrogen(ci) && L->top < ci->top)
      L->top = ci->top;  /* protect entire activation register */
    hydrogenD_checkstack(L, HYDROGEN_MINSTACK);  /* ensure minimum stack size */
    if (ci->top < L->top + HYDROGEN_MINSTACK)
      ci->top = L->top + HYDROGEN_MINSTACK;
    L->allowhook = 0;  /* cannot call hooks inside a hook */
    ci->callstatus |= mask;
    hydrogen_unlock(L);
    (*hook)(L, &ar);
    hydrogen_lock(L);
    hydrogen_assert(!L->allowhook);
    L->allowhook = 1;
    ci->top = restorestack(L, ci_top);
    L->top = restorestack(L, top);
    ci->callstatus &= ~mask;
  }
}


/*
** Executes a call hook for Hydrogen functions. This function is called
** whenever 'hookmask' is not zero, so it checks whether call hooks are
** active.
*/
void hydrogenD_hookcall (hydrogen_State *L, CallInfo *ci) {
  L->oldpc = 0;  /* set 'oldpc' for new function */
  if (L->hookmask & HYDROGEN_MASKCALL) {  /* is call hook on? */
    int event = (ci->callstatus & CIST_TAIL) ? HYDROGEN_HOOKTAILCALL
                                             : HYDROGEN_HOOKCALL;
    Proto *p = ci_func(ci)->p;
    ci->u.l.savedpc++;  /* hooks assume 'pc' is already incremented */
    hydrogenD_hook(L, event, -1, 1, p->numparams);
    ci->u.l.savedpc--;  /* correct 'pc' */
  }
}


/*
** Executes a return hook for Hydrogen and C functions and sets/corrects
** 'oldpc'. (Note that this correction is needed by the line hook, so it
** is done even when return hooks are off.)
*/
static void rethook (hydrogen_State *L, CallInfo *ci, int nres) {
  if (L->hookmask & HYDROGEN_MASKRET) {  /* is return hook on? */
    StkId firstres = L->top - nres;  /* index of first result */
    int delta = 0;  /* correction for vararg functions */
    int ftransfer;
    if (isHydrogen(ci)) {
      Proto *p = ci_func(ci)->p;
      if (p->is_vararg)
        delta = ci->u.l.nextraargs + p->numparams + 1;
    }
    ci->func += delta;  /* if vararg, back to virtual 'func' */
    ftransfer = cast(unsigned short, firstres - ci->func);
    hydrogenD_hook(L, HYDROGEN_HOOKRET, -1, ftransfer, nres);  /* call it */
    ci->func -= delta;
  }
  if (isHydrogen(ci = ci->previous))
    L->oldpc = pcRel(ci->u.l.savedpc, ci_func(ci)->p);  /* set 'oldpc' */
}


/*
** Check whether 'func' has a '__call' metafield. If so, put it in the
** stack, below original 'func', so that 'hydrogenD_precall' can call it. Raise
** an error if there is no '__call' metafield.
*/
StkId hydrogenD_tryfuncTM (hydrogen_State *L, StkId func) {
  const TValue *tm;
  StkId p;
  checkstackGCp(L, 1, func);  /* space for metamethod */
  tm = hydrogenT_gettmbyobj(L, s2v(func), TM_CALL);  /* (after previous GC) */
  if (l_unlikely(ttisnil(tm)))
    hydrogenG_callerror(L, s2v(func));  /* nothing to call */
  for (p = L->top; p > func; p--)  /* open space for metamethod */
    setobjs2s(L, p, p-1);
  L->top++;  /* stack space pre-allocated by the caller */
  setobj2s(L, func, tm);  /* metamethod is the new function to be called */
  return func;
}


/*
** Given 'nres' results at 'firstResult', move 'wanted' of them to 'res'.
** Handle most typical cases (zero results for commands, one result for
** expressions, multiple results for tail calls/single parameters)
** separated.
*/
l_sinline void moveresults (hydrogen_State *L, StkId res, int nres, int wanted) {
  StkId firstresult;
  int i;
  switch (wanted) {  /* handle typical cases separately */
    case 0:  /* no values needed */
      L->top = res;
      return;
    case 1:  /* one value needed */
      if (nres == 0)   /* no results? */
        setnilvalue(s2v(res));  /* adjust with nil */
      else  /* at least one result */
        setobjs2s(L, res, L->top - nres);  /* move it to proper place */
      L->top = res + 1;
      return;
    case HYDROGEN_MULTRET:
      wanted = nres;  /* we want all results */
      break;
    default:  /* two/more results and/or to-be-closed variables */
      if (hastocloseCfunc(wanted)) {  /* to-be-closed variables? */
        ptrdiff_t savedres = savestack(L, res);
        L->ci->callstatus |= CIST_CLSRET;  /* in case of yields */
        L->ci->u2.nres = nres;
        hydrogenF_close(L, res, CLOSEKTOP, 1);
        L->ci->callstatus &= ~CIST_CLSRET;
        if (L->hookmask)  /* if needed, call hook after '__close's */
          rethook(L, L->ci, nres);
        res = restorestack(L, savedres);  /* close and hook can move stack */
        wanted = decodeNresults(wanted);
        if (wanted == HYDROGEN_MULTRET)
          wanted = nres;  /* we want all results */
      }
      break;
  }
  /* generic case */
  firstresult = L->top - nres;  /* index of first result */
  if (nres > wanted)  /* extra results? */
    nres = wanted;  /* don't need them */
  for (i = 0; i < nres; i++)  /* move all results to correct place */
    setobjs2s(L, res + i, firstresult + i);
  for (; i < wanted; i++)  /* complete wanted number of results */
    setnilvalue(s2v(res + i));
  L->top = res + wanted;  /* top points after the last result */
}


/*
** Finishes a function call: calls hook if necessary, moves current
** number of results to proper place, and returns to previous call
** info. If function has to close variables, hook must be called after
** that.
*/
void hydrogenD_poscall (hydrogen_State *L, CallInfo *ci, int nres) {
  int wanted = ci->nresults;
  if (l_unlikely(L->hookmask && !hastocloseCfunc(wanted)))
    rethook(L, ci, nres);
  /* move results to proper place */
  moveresults(L, ci->func, nres, wanted);
  /* function cannot be in any of these cases when returning */
  hydrogen_assert(!(ci->callstatus &
        (CIST_HOOKED | CIST_YPCALL | CIST_FIN | CIST_TRAN | CIST_CLSRET)));
  L->ci = ci->previous;  /* back to caller (after closing variables) */
}



#define next_ci(L)  (L->ci->next ? L->ci->next : hydrogenE_extendCI(L))


l_sinline CallInfo *prepCallInfo (hydrogen_State *L, StkId func, int nret,
                                                int mask, StkId top) {
  CallInfo *ci = L->ci = next_ci(L);  /* new frame */
  ci->func = func;
  ci->nresults = nret;
  ci->callstatus = mask;
  ci->top = top;
  return ci;
}


/*
** precall for C functions
*/
l_sinline int precallC (hydrogen_State *L, StkId func, int nresults,
                                            hydrogen_CFunction f) {
  int n;  /* number of returns */
  CallInfo *ci;
  checkstackGCp(L, HYDROGEN_MINSTACK, func);  /* ensure minimum stack size */
  L->ci = ci = prepCallInfo(L, func, nresults, CIST_C,
                               L->top + HYDROGEN_MINSTACK);
  hydrogen_assert(ci->top <= L->stack_last);
  if (l_unlikely(L->hookmask & HYDROGEN_MASKCALL)) {
    int narg = cast_int(L->top - func) - 1;
    hydrogenD_hook(L, HYDROGEN_HOOKCALL, -1, 1, narg);
  }
  hydrogen_unlock(L);
  n = (*f)(L);  /* do the actual call */
  hydrogen_lock(L);
  api_checknelems(L, n);
  hydrogenD_poscall(L, ci, n);
  return n;
}


/*
** Prepare a function for a tail call, building its call info on top
** of the current call info. 'narg1' is the number of arguments plus 1
** (so that it includes the function itself). Return the number of
** results, if it was a C function, or -1 for a Hydrogen function.
*/
int hydrogenD_pretailcall (hydrogen_State *L, CallInfo *ci, StkId func,
                                    int narg1, int delta) {
 retry:
  switch (ttypetag(s2v(func))) {
    case HYDROGEN_VCCL:  /* C closure */
      return precallC(L, func, HYDROGEN_MULTRET, clCvalue(s2v(func))->f);
    case HYDROGEN_VLCF:  /* light C function */
      return precallC(L, func, HYDROGEN_MULTRET, fvalue(s2v(func)));
    case HYDROGEN_VLCL: {  /* Hydrogen function */
      Proto *p = clLvalue(s2v(func))->p;
      int fsize = p->maxstacksize;  /* frame size */
      int nfixparams = p->numparams;
      int i;
      checkstackGCp(L, fsize - delta, func);
      ci->func -= delta;  /* restore 'func' (if vararg) */
      for (i = 0; i < narg1; i++)  /* move down function and arguments */
        setobjs2s(L, ci->func + i, func + i);
      func = ci->func;  /* moved-down function */
      for (; narg1 <= nfixparams; narg1++)
        setnilvalue(s2v(func + narg1));  /* complete missing arguments */
      ci->top = func + 1 + fsize;  /* top for new function */
      hydrogen_assert(ci->top <= L->stack_last);
      ci->u.l.savedpc = p->code;  /* starting point */
      ci->callstatus |= CIST_TAIL;
      L->top = func + narg1;  /* set top */
      return -1;
    }
    default: {  /* not a function */
      func = hydrogenD_tryfuncTM(L, func);  /* try to get '__call' metamethod */
      /* return hydrogenD_pretailcall(L, ci, func, narg1 + 1, delta); */
      narg1++;
      goto retry;  
      /* try again */
    }
  }
}


/*
** Prepares the call to a function (C or Hydrogen). For C functions, also do
** the call. The function to be called is at '*func'.  The arguments
** are on the stack, right after the function.  Returns the CallInfo
** to be executed, if it was a Hydrogen function. Otherwise (a C function)
** returns NULL, with all the results on the stack, starting at the
** original function position.
*/
CallInfo *hydrogenD_precall (hydrogen_State *L, StkId func, int nresults) {
 retry:
  switch (ttypetag(s2v(func))) {
    case HYDROGEN_VCCL:  /* C closure */
      precallC(L, func, nresults, clCvalue(s2v(func))->f);
      return NULL;
    case HYDROGEN_VLCF:  /* light C function */
      precallC(L, func, nresults, fvalue(s2v(func)));
      return NULL;
    case HYDROGEN_VLCL: {  /* Hydrogen function */
      CallInfo *ci;
      Proto *p = clLvalue(s2v(func))->p;
      int narg = cast_int(L->top - func) - 1;  /* number of real arguments */
      int nfixparams = p->numparams;
      int fsize = p->maxstacksize;  /* frame size */
      checkstackGCp(L, fsize, func);
      L->ci = ci = prepCallInfo(L, func, nresults, 0, func + 1 + fsize);
      ci->u.l.savedpc = p->code;  /* starting point */
      for (; narg < nfixparams; narg++)
        setnilvalue(s2v(L->top++));  /* complete missing arguments */
      hydrogen_assert(ci->top <= L->stack_last);
      return ci;
    }
    default: {  /* not a function */
      func = hydrogenD_tryfuncTM(L, func);  /* try to get '__call' metamethod */
      /* return hydrogenD_precall(L, func, nresults); */
      goto retry;  /* try again with metamethod */
    }
  }
}


/*
** Call a function (C or Hydrogen) through C. 'inc' can be 1 (increment
** number of recursive invocations in the C stack) or nyci (the same
** plus increment number of non-yieldable calls).
*/
l_sinline void ccall (hydrogen_State *L, StkId func, int nResults, int inc) {
  CallInfo *ci;
  L->nCcalls += inc;
  if (l_unlikely(getCcalls(L) >= HYDROGENI_MAXCCALLS))
    hydrogenE_checkcstack(L);
  if ((ci = hydrogenD_precall(L, func, nResults)) != NULL) {  /* Hydrogen function? */
    ci->callstatus = CIST_FRESH;  /* mark that it is a "fresh" execute */
    hydrogenV_execute(L, ci);  /* call it */
  }
  L->nCcalls -= inc;
}


/*
** External interface for 'ccall'
*/
void hydrogenD_call (hydrogen_State *L, StkId func, int nResults) {
  ccall(L, func, nResults, 1);
}


/*
** Similar to 'hydrogenD_call', but does not allow yields during the call.
*/
void hydrogenD_callnoyield (hydrogen_State *L, StkId func, int nResults) {
  ccall(L, func, nResults, nyci);
}


/*
** Finish the job of 'hydrogen_pcallk' after it was interrupted by an yield.
** (The caller, 'finishCcall', does the final call to 'adjustresults'.)
** The main job is to complete the 'hydrogenD_pcall' called by 'hydrogen_pcallk'.
** If a '__close' method yields here, eventually control will be back
** to 'finishCcall' (when that '__close' method finally returns) and
** 'finishpcallk' will run again and close any still pending '__close'
** methods. Similarly, if a '__close' method errs, 'precover' calls
** 'unroll' which calls ''finishCcall' and we are back here again, to
** close any pending '__close' methods.
** Note that, up to the call to 'hydrogenF_close', the corresponding
** 'CallInfo' is not modified, so that this repeated run works like the
** first one (except that it has at least one less '__close' to do). In
** particular, field CIST_RECST preserves the error status across these
** multiple runs, changing only if there is a new error.
*/
static int finishpcallk (hydrogen_State *L,  CallInfo *ci) {
  int status = getcistrecst(ci);  /* get original status */
  if (l_likely(status == HYDROGEN_OK))  /* no error? */
    status = HYDROGEN_YIELD;  /* was interrupted by an yield */
  else {  /* error */
    StkId func = restorestack(L, ci->u2.funcidx);
    L->allowhook = getoah(ci->callstatus);  /* restore 'allowhook' */
    hydrogenF_close(L, func, status, 1);  /* can yield or raise an error */
    func = restorestack(L, ci->u2.funcidx);  /* stack may be moved */
    hydrogenD_seterrorobj(L, status, func);
    hydrogenD_shrinkstack(L);   /* restore stack size in case of overflow */
    setcistrecst(ci, HYDROGEN_OK);  /* clear original status */
  }
  ci->callstatus &= ~CIST_YPCALL;
  L->errfunc = ci->u.c.old_errfunc;
  /* if it is here, there were errors or yields; unlike 'hydrogen_pcallk',
     do not change status */
  return status;
}


/*
** Completes the execution of a C function interrupted by an yield.
** The interruption must have happened while the function was either
** closing its tbc variables in 'moveresults' or executing
** 'hydrogen_callk'/'hydrogen_pcallk'. In the first case, it just redoes
** 'hydrogenD_poscall'. In the second case, the call to 'finishpcallk'
** finishes the interrupted execution of 'hydrogen_pcallk'.  After that, it
** calls the continuation of the interrupted function and finally it
** completes the job of the 'hydrogenD_call' that called the function.  In
** the call to 'adjustresults', we do not know the number of results
** of the function called by 'hydrogen_callk'/'hydrogen_pcallk', so we are
** conservative and use HYDROGEN_MULTRET (always adjust).
*/
static void finishCcall (hydrogen_State *L, CallInfo *ci) {
  int n;  /* actual number of results from C function */
  if (ci->callstatus & CIST_CLSRET) {  /* was returning? */
    hydrogen_assert(hastocloseCfunc(ci->nresults));
    n = ci->u2.nres;  /* just redo 'hydrogenD_poscall' */
    /* don't need to reset CIST_CLSRET, as it will be set again anyway */
  }
  else {
    int status = HYDROGEN_YIELD;  /* default if there were no errors */
    /* must have a continuation and must be able to call it */
    hydrogen_assert(ci->u.c.k != NULL && yieldable(L));
    if (ci->callstatus & CIST_YPCALL)   /* was inside a 'hydrogen_pcallk'? */
      status = finishpcallk(L, ci);  /* finish it */
    adjustresults(L, HYDROGEN_MULTRET);  /* finish 'hydrogen_callk' */
    hydrogen_unlock(L);
    n = (*ci->u.c.k)(L, status, ci->u.c.ctx);  /* call continuation */
    hydrogen_lock(L);
    api_checknelems(L, n);
  }
  hydrogenD_poscall(L, ci, n);  /* finish 'hydrogenD_call' */
}


/*
** Executes "full continuation" (everything in the stack) of a
** previously interrupted coroutine until the stack is empty (or another
** interruption long-jumps out of the loop).
*/
static void unroll (hydrogen_State *L, void *ud) {
  CallInfo *ci;
  UNUSED(ud);
  while ((ci = L->ci) != &L->base_ci) {  /* something in the stack */
    if (!isHydrogen(ci))  /* C function? */
      finishCcall(L, ci);  /* complete its execution */
    else {  /* Hydrogen function */
      hydrogenV_finishOp(L);  /* finish interrupted instruction */
      hydrogenV_execute(L, ci);  /* execute down to higher C 'boundary' */
    }
  }
}


/*
** Try to find a suspended protected call (a "recover point") for the
** given thread.
*/
static CallInfo *findpcall (hydrogen_State *L) {
  CallInfo *ci;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {  /* search for a pcall */
    if (ci->callstatus & CIST_YPCALL)
      return ci;
  }
  return NULL;  /* no pending pcall */
}


/*
** Signal an error in the call to 'hydrogen_resume', not in the execution
** of the coroutine itself. (Such errors should not be handled by any
** coroutine error handler and should not kill the coroutine.)
*/
static int resume_error (hydrogen_State *L, const char *msg, int narg) {
  L->top -= narg;  /* remove args from the stack */
  setsvalue2s(L, L->top, hydrogenS_new(L, msg));  /* push error message */
  api_incr_top(L);
  hydrogen_unlock(L);
  return HYDROGEN_ERRRUN;
}


/*
** Do the work for 'hydrogen_resume' in protected mode. Most of the work
** depends on the status of the coroutine: initial state, suspended
** inside a hook, or regularly suspended (optionally with a continuation
** function), plus erroneous cases: non-suspended coroutine or dead
** coroutine.
*/
static void resume (hydrogen_State *L, void *ud) {
  int n = *(cast(int*, ud));  /* number of arguments */
  StkId firstArg = L->top - n;  /* first argument */
  CallInfo *ci = L->ci;
  if (L->status == HYDROGEN_OK)  /* starting a coroutine? */
    ccall(L, firstArg - 1, HYDROGEN_MULTRET, 0);  /* just call its body */
  else {  /* resuming from previous yield */
    hydrogen_assert(L->status == HYDROGEN_YIELD);
    L->status = HYDROGEN_OK;  /* mark that it is running (again) */
    if (isHydrogen(ci)) {  /* yielded inside a hook? */
      L->top = firstArg;  /* discard arguments */
      hydrogenV_execute(L, ci);  /* just continue running Hydrogen code */
    }
    else {  /* 'common' yield */
      if (ci->u.c.k != NULL) {  /* does it have a continuation function? */
        hydrogen_unlock(L);
        n = (*ci->u.c.k)(L, HYDROGEN_YIELD, ci->u.c.ctx); /* call continuation */
        hydrogen_lock(L);
        api_checknelems(L, n);
      }
      hydrogenD_poscall(L, ci, n);  /* finish 'hydrogenD_call' */
    }
    unroll(L, NULL);  /* run continuation */
  }
}


/*
** Unrolls a coroutine in protected mode while there are recoverable
** errors, that is, errors inside a protected call. (Any error
** interrupts 'unroll', and this loop protects it again so it can
** continue.) Stops with a normal end (status == HYDROGEN_OK), an yield
** (status == HYDROGEN_YIELD), or an unprotected error ('findpcall' doesn't
** find a recover point).
*/
static int precover (hydrogen_State *L, int status) {
  CallInfo *ci;
  while (errorstatus(status) && (ci = findpcall(L)) != NULL) {
    L->ci = ci;  /* Hydrogen down to recovery functions */
    setcistrecst(ci, status);  /* status to finish 'pcall' */
    status = hydrogenD_rawrunprotected(L, unroll, NULL);
  }
  return status;
}


HYDROGEN_API int hydrogen_resume (hydrogen_State *L, hydrogen_State *from, int nargs,
                                      int *nresults) {
  int status;
  hydrogen_lock(L);
  if (L->status == HYDROGEN_OK) {  /* may be starting a coroutine */
    if (L->ci != &L->base_ci)  /* not in base level? */
      return resume_error(L, "cannot resume non-suspended coroutine", nargs);
    else if (L->top - (L->ci->func + 1) == nargs)  /* no function? */
      return resume_error(L, "cannot resume dead coroutine", nargs);
  }
  else if (L->status != HYDROGEN_YIELD)  /* ended with errors? */
    return resume_error(L, "cannot resume dead coroutine", nargs);
  L->nCcalls = (from) ? getCcalls(from) : 0;
  if (getCcalls(L) >= HYDROGENI_MAXCCALLS)
    return resume_error(L, "C stack overflow", nargs);
  L->nCcalls++;
  hydrogeni_userstateresume(L, nargs);
  api_checknelems(L, (L->status == HYDROGEN_OK) ? nargs + 1 : nargs);
  status = hydrogenD_rawrunprotected(L, resume, &nargs);
   /* continue running after recoverable errors */
  status = precover(L, status);
  if (l_likely(!errorstatus(status)))
    hydrogen_assert(status == L->status);  /* normal end or yield */
  else {  /* unrecoverable error */
    L->status = cast_byte(status);  /* mark thread as 'dead' */
    hydrogenD_seterrorobj(L, status, L->top);  /* push error message */
    L->ci->top = L->top;
  }
  *nresults = (status == HYDROGEN_YIELD) ? L->ci->u2.nyield
                                    : cast_int(L->top - (L->ci->func + 1));
  hydrogen_unlock(L);
  return status;
}


HYDROGEN_API int hydrogen_isyieldable (hydrogen_State *L) {
  return yieldable(L);
}


HYDROGEN_API int hydrogen_yieldk (hydrogen_State *L, int nresults, hydrogen_KContext ctx,
                        hydrogen_KFunction k) {
  CallInfo *ci;
  hydrogeni_userstateyield(L, nresults);
  hydrogen_lock(L);
  ci = L->ci;
  api_checknelems(L, nresults);
  if (l_unlikely(!yieldable(L))) {
    if (L != G(L)->mainthread)
      hydrogenG_runerror(L, "attempt to yield across a C-call boundary");
    else
      hydrogenG_runerror(L, "attempt to yield from outside a coroutine");
  }
  L->status = HYDROGEN_YIELD;
  ci->u2.nyield = nresults;  /* save number of results */
  if (isHydrogen(ci)) {  /* inside a hook? */
    hydrogen_assert(!isHydrogencode(ci));
    api_check(L, nresults == 0, "hooks cannot yield values");
    api_check(L, k == NULL, "hooks cannot continue after yielding");
  }
  else {
    if ((ci->u.c.k = k) != NULL)  /* is there a continuation? */
      ci->u.c.ctx = ctx;  /* save context */
    hydrogenD_throw(L, HYDROGEN_YIELD);
  }
  hydrogen_assert(ci->callstatus & CIST_HOOKED);  /* must be inside a hook */
  hydrogen_unlock(L);
  return 0;  /* return to 'hydrogenD_hook' */
}


/*
** Auxiliary structure to call 'hydrogenF_close' in protected mode.
*/
struct CloseP {
  StkId level;
  int status;
};


/*
** Auxiliary function to call 'hydrogenF_close' in protected mode.
*/
static void closepaux (hydrogen_State *L, void *ud) {
  struct CloseP *pcl = cast(struct CloseP *, ud);
  hydrogenF_close(L, pcl->level, pcl->status, 0);
}


/*
** Calls 'hydrogenF_close' in protected mode. Return the original status
** or, in case of errors, the new status.
*/
int hydrogenD_closeprotected (hydrogen_State *L, ptrdiff_t level, int status) {
  CallInfo *old_ci = L->ci;
  lu_byte old_allowhooks = L->allowhook;
  for (;;) {  /* keep closing upvalues until no more errors */
    struct CloseP pcl;
    pcl.level = restorestack(L, level); pcl.status = status;
    status = hydrogenD_rawrunprotected(L, &closepaux, &pcl);
    if (l_likely(status == HYDROGEN_OK))  /* no more errors? */
      return pcl.status;
    else {  /* an error occurred; restore saved state and repeat */
      L->ci = old_ci;
      L->allowhook = old_allowhooks;
    }
  }
}


/*
** Call the C function 'func' in protected mode, restoring basic
** thread information ('allowhook', etc.) and in particular
** its stack level in case of errors.
*/
int hydrogenD_pcall (hydrogen_State *L, Pfunc func, void *u,
                ptrdiff_t old_top, ptrdiff_t ef) {
  int status;
  CallInfo *old_ci = L->ci;
  lu_byte old_allowhooks = L->allowhook;
  ptrdiff_t old_errfunc = L->errfunc;
  L->errfunc = ef;
  status = hydrogenD_rawrunprotected(L, func, u);
  if (l_unlikely(status != HYDROGEN_OK)) {  /* an error occurred? */
    L->ci = old_ci;
    L->allowhook = old_allowhooks;
    status = hydrogenD_closeprotected(L, old_top, status);
    hydrogenD_seterrorobj(L, status, restorestack(L, old_top));
    hydrogenD_shrinkstack(L);   /* restore stack size in case of overflow */
  }
  L->errfunc = old_errfunc;
  return status;
}



/*
** Execute a protected parser.
*/
struct SParser {  /* data to 'f_parser' */
  ZIO *z;
  Mbuffer buff;  /* dynamic structure used by the scanner */
  Dyndata dyd;  /* dynamic structures used by the parser */
  const char *mode;
  const char *name;
};


static void checkmode (hydrogen_State *L, const char *mode, const char *x) {
  if (mode && strchr(mode, x[0]) == NULL) {
    hydrogenO_pushfstring(L,
       "attempt to load a %s chunk (mode is '%s')", x, mode);
    hydrogenD_throw(L, HYDROGEN_ERRSYNTAX);
  }
}


static void f_parser (hydrogen_State *L, void *ud) {
  LClosure *cl;
  struct SParser *p = cast(struct SParser *, ud);
  int c = zgetc(p->z);  /* read first character */
  if (c == HYDROGEN_SIGNATURE[0]) {
    checkmode(L, p->mode, "binary");
    cl = hydrogenU_undump(L, p->z, p->name);
  }
  else {
    checkmode(L, p->mode, "text");
    cl = hydrogenY_parser(L, p->z, &p->buff, &p->dyd, p->name, c);
  }
  hydrogen_assert(cl->nupvalues == cl->p->sizeupvalues);
  hydrogenF_initupvals(L, cl);
}


int hydrogenD_protectedparser (hydrogen_State *L, ZIO *z, const char *name,
                                        const char *mode) {
  struct SParser p;
  int status;
  incnny(L);  /* cannot yield during parsing */
  p.z = z; p.name = name; p.mode = mode;
  p.dyd.actvar.arr = NULL; p.dyd.actvar.size = 0;
  p.dyd.gt.arr = NULL; p.dyd.gt.size = 0;
  p.dyd.label.arr = NULL; p.dyd.label.size = 0;
  hydrogenZ_initbuffer(L, &p.buff);
  status = hydrogenD_pcall(L, f_parser, &p, savestack(L, L->top), L->errfunc);
  hydrogenZ_freebuffer(L, &p.buff);
  hydrogenM_freearray(L, p.dyd.actvar.arr, p.dyd.actvar.size);
  hydrogenM_freearray(L, p.dyd.gt.arr, p.dyd.gt.size);
  hydrogenM_freearray(L, p.dyd.label.arr, p.dyd.label.size);
  decnny(L);
  return status;
}


