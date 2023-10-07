/*
** $Id: ldo.c $
** Stack and Call structure of Cup
** See Copyright Notice in cup.h
*/

#define ldo_c
#define CUP_CORE

#include "lprefix.h"


#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "cup.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"
#include "lzio.h"



#define errorstatus(s)	((s) > CUP_YIELD)


/*
** {======================================================
** Error-recovery functions
** =======================================================
*/

/*
** CUPI_THROW/CUPI_TRY define how Cup does exception handling. By
** default, Cup handles errors with exceptions when compiling as
** C++ code, with _longjmp/_setjmp when asked to use them, and with
** longjmp/setjmp otherwise.
*/
#if !defined(CUPI_THROW)				/* { */

#if defined(__cplusplus) && !defined(CUP_USE_LONGJMP)	/* { */

/* C++ exceptions */
#define CUPI_THROW(L,c)		throw(c)
#define CUPI_TRY(L,c,a) \
	try { a } catch(...) { if ((c)->status == 0) (c)->status = -1; }
#define cupi_jmpbuf		int  /* dummy variable */

#elif defined(CUP_USE_POSIX)				/* }{ */

/* in POSIX, try _longjmp/_setjmp (more efficient) */
#define CUPI_THROW(L,c)		_longjmp((c)->b, 1)
#define CUPI_TRY(L,c,a)		if (_setjmp((c)->b) == 0) { a }
#define cupi_jmpbuf		jmp_buf

#else							/* }{ */

/* ISO C handling with long jumps */
#define CUPI_THROW(L,c)		longjmp((c)->b, 1)
#define CUPI_TRY(L,c,a)		if (setjmp((c)->b) == 0) { a }
#define cupi_jmpbuf		jmp_buf

#endif							/* } */

#endif							/* } */



/* chain list of long jump buffers */
struct cup_longjmp {
  struct cup_longjmp *previous;
  cupi_jmpbuf b;
  volatile int status;  /* error code */
};


void cupD_seterrorobj (cup_State *L, int errcode, StkId oldtop) {
  switch (errcode) {
    case CUP_ERRMEM: {  /* memory error? */
      setsvalue2s(L, oldtop, G(L)->memerrmsg); /* reuse preregistered msg. */
      break;
    }
    case CUP_ERRERR: {
      setsvalue2s(L, oldtop, cupS_newliteral(L, "error in error handling"));
      break;
    }
    case CUP_OK: {  /* special case only for closing upvalues */
      setnilvalue(s2v(oldtop));  /* no error message */
      break;
    }
    default: {
      cup_assert(errorstatus(errcode));  /* real error */
      setobjs2s(L, oldtop, L->top - 1);  /* error message on current top */
      break;
    }
  }
  L->top = oldtop + 1;
}


l_noret cupD_throw (cup_State *L, int errcode) {
  if (L->errorJmp) {  /* thread has an error handler? */
    L->errorJmp->status = errcode;  /* set status */
    CUPI_THROW(L, L->errorJmp);  /* jump to it */
  }
  else {  /* thread has no error handler */
    global_State *g = G(L);
    errcode = cupE_resetthread(L, errcode);  /* close all upvalues */
    if (g->mainthread->errorJmp) {  /* main thread has a handler? */
      setobjs2s(L, g->mainthread->top++, L->top - 1);  /* copy error obj. */
      cupD_throw(g->mainthread, errcode);  /* re-throw in main thread */
    }
    else {  /* no handler at all; abort */
      if (g->panic) {  /* panic function? */
        cup_unlock(L);
        g->panic(L);  /* call panic function (last chance to jump out) */
      }
      abort();
    }
  }
}


int cupD_rawrunprotected (cup_State *L, Pfunc f, void *ud) {
  l_uint32 oldnCcalls = L->nCcalls;
  struct cup_longjmp lj;
  lj.status = CUP_OK;
  lj.previous = L->errorJmp;  /* chain new error handler */
  L->errorJmp = &lj;
  CUPI_TRY(L, &lj,
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
static void correctstack (cup_State *L, StkId oldstack, StkId newstack) {
  CallInfo *ci;
  UpVal *up;
  L->top = (L->top - oldstack) + newstack;
  L->tbclist = (L->tbclist - oldstack) + newstack;
  for (up = L->openupval; up != NULL; up = up->u.open.next)
    up->v = s2v((uplevel(up) - oldstack) + newstack);
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    ci->top = (ci->top - oldstack) + newstack;
    ci->func = (ci->func - oldstack) + newstack;
    if (isCup(ci))
      ci->u.l.trap = 1;  /* signal to update 'trap' in 'cupV_execute' */
  }
}


/* some space for error handling */
#define ERRORSTACKSIZE	(CUPI_MAXSTACK + 200)


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
int cupD_reallocstack (cup_State *L, int newsize, int raiseerror) {
  int oldsize = stacksize(L);
  int i;
  StkId newstack = cupM_reallocvector(L, NULL, 0,
                                      newsize + EXTRA_STACK, StackValue);
  cup_assert(newsize <= CUPI_MAXSTACK || newsize == ERRORSTACKSIZE);
  if (l_unlikely(newstack == NULL)) {  /* reallocation failed? */
    if (raiseerror)
      cupM_error(L);
    else return 0;  /* do not raise an error */
  }
  /* number of elements to be copied to the new stack */
  i = ((oldsize <= newsize) ? oldsize : newsize) + EXTRA_STACK;
  memcpy(newstack, L->stack, i * sizeof(StackValue));
  for (; i < newsize + EXTRA_STACK; i++)
    setnilvalue(s2v(newstack + i)); /* erase new segment */
  correctstack(L, L->stack, newstack);
  cupM_freearray(L, L->stack, oldsize + EXTRA_STACK);
  L->stack = newstack;
  L->stack_last = L->stack + newsize;
  return 1;
}


/*
** Try to grow the stack by at least 'n' elements. when 'raiseerror'
** is true, raises any error; otherwise, return 0 in case of errors.
*/
int cupD_growstack (cup_State *L, int n, int raiseerror) {
  int size = stacksize(L);
  if (l_unlikely(size > CUPI_MAXSTACK)) {
    /* if stack is larger than maximum, thread is already using the
       extra space reserved for errors, that is, thread is handling
       a stack error; cannot grow further than that. */
    cup_assert(stacksize(L) == ERRORSTACKSIZE);
    if (raiseerror)
      cupD_throw(L, CUP_ERRERR);  /* error inside message handler */
    return 0;  /* if not 'raiseerror', just signal it */
  }
  else {
    int newsize = 2 * size;  /* tentative new size */
    int needed = cast_int(L->top - L->stack) + n;
    if (newsize > CUPI_MAXSTACK)  /* cannot cross the limit */
      newsize = CUPI_MAXSTACK;
    if (newsize < needed)  /* but must respect what was asked for */
      newsize = needed;
    if (l_likely(newsize <= CUPI_MAXSTACK))
      return cupD_reallocstack(L, newsize, raiseerror);
    else {  /* stack overflow */
      /* add extra size to be able to handle the error message */
      cupD_reallocstack(L, ERRORSTACKSIZE, raiseerror);
      if (raiseerror)
        cupG_runerror(L, "stack overflow");
      return 0;
    }
  }
}


static int stackinuse (cup_State *L) {
  CallInfo *ci;
  int res;
  StkId lim = L->top;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    if (lim < ci->top) lim = ci->top;
  }
  cup_assert(lim <= L->stack_last);
  res = cast_int(lim - L->stack) + 1;  /* part of stack in use */
  if (res < CUP_MINSTACK)
    res = CUP_MINSTACK;  /* ensure a minimum size */
  return res;
}


/*
** If stack size is more than 3 times the current use, reduce that size
** to twice the current use. (So, the final stack size is at most 2/3 the
** previous size, and half of its entries are empty.)
** As a particular case, if stack was handling a stack overflow and now
** it is not, 'max' (limited by CUPI_MAXSTACK) will be smaller than
** stacksize (equal to ERRORSTACKSIZE in this case), and so the stack
** will be reduced to a "regular" size.
*/
void cupD_shrinkstack (cup_State *L) {
  int inuse = stackinuse(L);
  int nsize = inuse * 2;  /* proposed new size */
  int max = inuse * 3;  /* maximum "reasonable" size */
  if (max > CUPI_MAXSTACK) {
    max = CUPI_MAXSTACK;  /* respect stack limit */
    if (nsize > CUPI_MAXSTACK)
      nsize = CUPI_MAXSTACK;
  }
  /* if thread is currently not handling a stack overflow and its
     size is larger than maximum "reasonable" size, shrink it */
  if (inuse <= CUPI_MAXSTACK && stacksize(L) > max)
    cupD_reallocstack(L, nsize, 0);  /* ok if that fails */
  else  /* don't change stack */
    condmovestack(L,{},{});  /* (change only for debugging) */
  cupE_shrinkCI(L);  /* shrink CI list */
}


void cupD_inctop (cup_State *L) {
  cupD_checkstack(L, 1);
  L->top++;
}

/* }================================================================== */


/*
** Call a hook for the given event. Make sure there is a hook to be
** called. (Both 'L->hook' and 'L->hookmask', which trigger this
** function, can be changed asynchronously by signals.)
*/
void cupD_hook (cup_State *L, int event, int line,
                              int ftransfer, int ntransfer) {
  cup_Hook hook = L->hook;
  if (hook && L->allowhook) {  /* make sure there is a hook */
    int mask = CIST_HOOKED;
    CallInfo *ci = L->ci;
    ptrdiff_t top = savestack(L, L->top);  /* preserve original 'top' */
    ptrdiff_t ci_top = savestack(L, ci->top);  /* idem for 'ci->top' */
    cup_Debug ar;
    ar.event = event;
    ar.currentline = line;
    ar.i_ci = ci;
    if (ntransfer != 0) {
      mask |= CIST_TRAN;  /* 'ci' has transfer information */
      ci->u2.transferinfo.ftransfer = ftransfer;
      ci->u2.transferinfo.ntransfer = ntransfer;
    }
    if (isCup(ci) && L->top < ci->top)
      L->top = ci->top;  /* protect entire activation register */
    cupD_checkstack(L, CUP_MINSTACK);  /* ensure minimum stack size */
    if (ci->top < L->top + CUP_MINSTACK)
      ci->top = L->top + CUP_MINSTACK;
    L->allowhook = 0;  /* cannot call hooks inside a hook */
    ci->callstatus |= mask;
    cup_unlock(L);
    (*hook)(L, &ar);
    cup_lock(L);
    cup_assert(!L->allowhook);
    L->allowhook = 1;
    ci->top = restorestack(L, ci_top);
    L->top = restorestack(L, top);
    ci->callstatus &= ~mask;
  }
}


/*
** Executes a call hook for Cup functions. This function is called
** whenever 'hookmask' is not zero, so it checks whether call hooks are
** active.
*/
void cupD_hookcall (cup_State *L, CallInfo *ci) {
  L->oldpc = 0;  /* set 'oldpc' for new function */
  if (L->hookmask & CUP_MASKCALL) {  /* is call hook on? */
    int event = (ci->callstatus & CIST_TAIL) ? CUP_HOOKTAILCALL
                                             : CUP_HOOKCALL;
    Proto *p = ci_func(ci)->p;
    ci->u.l.savedpc++;  /* hooks assume 'pc' is already incremented */
    cupD_hook(L, event, -1, 1, p->numparams);
    ci->u.l.savedpc--;  /* correct 'pc' */
  }
}


/*
** Executes a return hook for Cup and C functions and sets/corrects
** 'oldpc'. (Note that this correction is needed by the line hook, so it
** is done even when return hooks are off.)
*/
static void rethook (cup_State *L, CallInfo *ci, int nres) {
  if (L->hookmask & CUP_MASKRET) {  /* is return hook on? */
    StkId firstres = L->top - nres;  /* index of first result */
    int delta = 0;  /* correction for vararg functions */
    int ftransfer;
    if (isCup(ci)) {
      Proto *p = ci_func(ci)->p;
      if (p->is_vararg)
        delta = ci->u.l.nextraargs + p->numparams + 1;
    }
    ci->func += delta;  /* if vararg, back to virtual 'func' */
    ftransfer = cast(unsigned short, firstres - ci->func);
    cupD_hook(L, CUP_HOOKRET, -1, ftransfer, nres);  /* call it */
    ci->func -= delta;
  }
  if (isCup(ci = ci->previous))
    L->oldpc = pcRel(ci->u.l.savedpc, ci_func(ci)->p);  /* set 'oldpc' */
}


/*
** Check whether 'func' has a '__call' metafield. If so, put it in the
** stack, below original 'func', so that 'cupD_precall' can call it. Raise
** an error if there is no '__call' metafield.
*/
StkId cupD_tryfuncTM (cup_State *L, StkId func) {
  const TValue *tm;
  StkId p;
  checkstackGCp(L, 1, func);  /* space for metamethod */
  tm = cupT_gettmbyobj(L, s2v(func), TM_CALL);  /* (after previous GC) */
  if (l_unlikely(ttisnil(tm)))
    cupG_callerror(L, s2v(func));  /* nothing to call */
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
l_sinline void moveresults (cup_State *L, StkId res, int nres, int wanted) {
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
    case CUP_MULTRET:
      wanted = nres;  /* we want all results */
      break;
    default:  /* two/more results and/or to-be-closed variables */
      if (hastocloseCfunc(wanted)) {  /* to-be-closed variables? */
        ptrdiff_t savedres = savestack(L, res);
        L->ci->callstatus |= CIST_CLSRET;  /* in case of yields */
        L->ci->u2.nres = nres;
        cupF_close(L, res, CLOSEKTOP, 1);
        L->ci->callstatus &= ~CIST_CLSRET;
        if (L->hookmask)  /* if needed, call hook after '__close's */
          rethook(L, L->ci, nres);
        res = restorestack(L, savedres);  /* close and hook can move stack */
        wanted = decodeNresults(wanted);
        if (wanted == CUP_MULTRET)
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
void cupD_poscall (cup_State *L, CallInfo *ci, int nres) {
  int wanted = ci->nresults;
  if (l_unlikely(L->hookmask && !hastocloseCfunc(wanted)))
    rethook(L, ci, nres);
  /* move results to proper place */
  moveresults(L, ci->func, nres, wanted);
  /* function cannot be in any of these cases when returning */
  cup_assert(!(ci->callstatus &
        (CIST_HOOKED | CIST_YPCALL | CIST_FIN | CIST_TRAN | CIST_CLSRET)));
  L->ci = ci->previous;  /* back to caller (after closing variables) */
}



#define next_ci(L)  (L->ci->next ? L->ci->next : cupE_extendCI(L))


l_sinline CallInfo *prepCallInfo (cup_State *L, StkId func, int nret,
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
l_sinline int precallC (cup_State *L, StkId func, int nresults,
                                            cup_CFunction f) {
  int n;  /* number of returns */
  CallInfo *ci;
  checkstackGCp(L, CUP_MINSTACK, func);  /* ensure minimum stack size */
  L->ci = ci = prepCallInfo(L, func, nresults, CIST_C,
                               L->top + CUP_MINSTACK);
  cup_assert(ci->top <= L->stack_last);
  if (l_unlikely(L->hookmask & CUP_MASKCALL)) {
    int narg = cast_int(L->top - func) - 1;
    cupD_hook(L, CUP_HOOKCALL, -1, 1, narg);
  }
  cup_unlock(L);
  n = (*f)(L);  /* do the actual call */
  cup_lock(L);
  api_checknelems(L, n);
  cupD_poscall(L, ci, n);
  return n;
}


/*
** Prepare a function for a tail call, building its call info on top
** of the current call info. 'narg1' is the number of arguments plus 1
** (so that it includes the function itself). Return the number of
** results, if it was a C function, or -1 for a Cup function.
*/
int cupD_pretailcall (cup_State *L, CallInfo *ci, StkId func,
                                    int narg1, int delta) {
 retry:
  switch (ttypetag(s2v(func))) {
    case CUP_VCCL:  /* C closure */
      return precallC(L, func, CUP_MULTRET, clCvalue(s2v(func))->f);
    case CUP_VLCF:  /* light C function */
      return precallC(L, func, CUP_MULTRET, fvalue(s2v(func)));
    case CUP_VLCL: {  /* Cup function */
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
      cup_assert(ci->top <= L->stack_last);
      ci->u.l.savedpc = p->code;  /* starting point */
      ci->callstatus |= CIST_TAIL;
      L->top = func + narg1;  /* set top */
      return -1;
    }
    default: {  /* not a function */
      func = cupD_tryfuncTM(L, func);  /* try to get '__call' metamethod */
      /* return cupD_pretailcall(L, ci, func, narg1 + 1, delta); */
      narg1++;
      goto retry;  
      /* try again */
    }
  }
}


/*
** Prepares the call to a function (C or Cup). For C functions, also do
** the call. The function to be called is at '*func'.  The arguments
** are on the stack, right after the function.  Returns the CallInfo
** to be executed, if it was a Cup function. Otherwise (a C function)
** returns NULL, with all the results on the stack, starting at the
** original function position.
*/
CallInfo *cupD_precall (cup_State *L, StkId func, int nresults) {
 retry:
  switch (ttypetag(s2v(func))) {
    case CUP_VCCL:  /* C closure */
      precallC(L, func, nresults, clCvalue(s2v(func))->f);
      return NULL;
    case CUP_VLCF:  /* light C function */
      precallC(L, func, nresults, fvalue(s2v(func)));
      return NULL;
    case CUP_VLCL: {  /* Cup function */
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
      cup_assert(ci->top <= L->stack_last);
      return ci;
    }
    default: {  /* not a function */
      func = cupD_tryfuncTM(L, func);  /* try to get '__call' metamethod */
      /* return cupD_precall(L, func, nresults); */
      goto retry;  /* try again with metamethod */
    }
  }
}


/*
** Call a function (C or Cup) through C. 'inc' can be 1 (increment
** number of recursive invocations in the C stack) or nyci (the same
** plus increment number of non-yieldable calls).
*/
l_sinline void ccall (cup_State *L, StkId func, int nResults, int inc) {
  CallInfo *ci;
  L->nCcalls += inc;
  if (l_unlikely(getCcalls(L) >= CUPI_MAXCCALLS))
    cupE_checkcstack(L);
  if ((ci = cupD_precall(L, func, nResults)) != NULL) {  /* Cup function? */
    ci->callstatus = CIST_FRESH;  /* mark that it is a "fresh" execute */
    cupV_execute(L, ci);  /* call it */
  }
  L->nCcalls -= inc;
}


/*
** External interface for 'ccall'
*/
void cupD_call (cup_State *L, StkId func, int nResults) {
  ccall(L, func, nResults, 1);
}


/*
** Similar to 'cupD_call', but does not allow yields during the call.
*/
void cupD_callnoyield (cup_State *L, StkId func, int nResults) {
  ccall(L, func, nResults, nyci);
}


/*
** Finish the job of 'cup_pcallk' after it was interrupted by an yield.
** (The caller, 'finishCcall', does the final call to 'adjustresults'.)
** The main job is to complete the 'cupD_pcall' called by 'cup_pcallk'.
** If a '__close' method yields here, eventually control will be back
** to 'finishCcall' (when that '__close' method finally returns) and
** 'finishpcallk' will run again and close any still pending '__close'
** methods. Similarly, if a '__close' method errs, 'precover' calls
** 'unroll' which calls ''finishCcall' and we are back here again, to
** close any pending '__close' methods.
** Note that, up to the call to 'cupF_close', the corresponding
** 'CallInfo' is not modified, so that this repeated run works like the
** first one (except that it has at least one less '__close' to do). In
** particular, field CIST_RECST preserves the error status across these
** multiple runs, changing only if there is a new error.
*/
static int finishpcallk (cup_State *L,  CallInfo *ci) {
  int status = getcistrecst(ci);  /* get original status */
  if (l_likely(status == CUP_OK))  /* no error? */
    status = CUP_YIELD;  /* was interrupted by an yield */
  else {  /* error */
    StkId func = restorestack(L, ci->u2.funcidx);
    L->allowhook = getoah(ci->callstatus);  /* restore 'allowhook' */
    cupF_close(L, func, status, 1);  /* can yield or raise an error */
    func = restorestack(L, ci->u2.funcidx);  /* stack may be moved */
    cupD_seterrorobj(L, status, func);
    cupD_shrinkstack(L);   /* restore stack size in case of overflow */
    setcistrecst(ci, CUP_OK);  /* clear original status */
  }
  ci->callstatus &= ~CIST_YPCALL;
  L->errfunc = ci->u.c.old_errfunc;
  /* if it is here, there were errors or yields; unlike 'cup_pcallk',
     do not change status */
  return status;
}


/*
** Completes the execution of a C function interrupted by an yield.
** The interruption must have happened while the function was either
** closing its tbc variables in 'moveresults' or executing
** 'cup_callk'/'cup_pcallk'. In the first case, it just redoes
** 'cupD_poscall'. In the second case, the call to 'finishpcallk'
** finishes the interrupted execution of 'cup_pcallk'.  After that, it
** calls the continuation of the interrupted function and finally it
** completes the job of the 'cupD_call' that called the function.  In
** the call to 'adjustresults', we do not know the number of results
** of the function called by 'cup_callk'/'cup_pcallk', so we are
** conservative and use CUP_MULTRET (always adjust).
*/
static void finishCcall (cup_State *L, CallInfo *ci) {
  int n;  /* actual number of results from C function */
  if (ci->callstatus & CIST_CLSRET) {  /* was returning? */
    cup_assert(hastocloseCfunc(ci->nresults));
    n = ci->u2.nres;  /* just redo 'cupD_poscall' */
    /* don't need to reset CIST_CLSRET, as it will be set again anyway */
  }
  else {
    int status = CUP_YIELD;  /* default if there were no errors */
    /* must have a continuation and must be able to call it */
    cup_assert(ci->u.c.k != NULL && yieldable(L));
    if (ci->callstatus & CIST_YPCALL)   /* was inside a 'cup_pcallk'? */
      status = finishpcallk(L, ci);  /* finish it */
    adjustresults(L, CUP_MULTRET);  /* finish 'cup_callk' */
    cup_unlock(L);
    n = (*ci->u.c.k)(L, status, ci->u.c.ctx);  /* call continuation */
    cup_lock(L);
    api_checknelems(L, n);
  }
  cupD_poscall(L, ci, n);  /* finish 'cupD_call' */
}


/*
** Executes "full continuation" (everything in the stack) of a
** previously interrupted coroutine until the stack is empty (or another
** interruption long-jumps out of the loop).
*/
static void unroll (cup_State *L, void *ud) {
  CallInfo *ci;
  UNUSED(ud);
  while ((ci = L->ci) != &L->base_ci) {  /* something in the stack */
    if (!isCup(ci))  /* C function? */
      finishCcall(L, ci);  /* complete its execution */
    else {  /* Cup function */
      cupV_finishOp(L);  /* finish interrupted instruction */
      cupV_execute(L, ci);  /* execute down to higher C 'boundary' */
    }
  }
}


/*
** Try to find a suspended protected call (a "recover point") for the
** given thread.
*/
static CallInfo *findpcall (cup_State *L) {
  CallInfo *ci;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {  /* search for a pcall */
    if (ci->callstatus & CIST_YPCALL)
      return ci;
  }
  return NULL;  /* no pending pcall */
}


/*
** Signal an error in the call to 'cup_resume', not in the execution
** of the coroutine itself. (Such errors should not be handled by any
** coroutine error handler and should not kill the coroutine.)
*/
static int resume_error (cup_State *L, const char *msg, int narg) {
  L->top -= narg;  /* remove args from the stack */
  setsvalue2s(L, L->top, cupS_new(L, msg));  /* push error message */
  api_incr_top(L);
  cup_unlock(L);
  return CUP_ERRRUN;
}


/*
** Do the work for 'cup_resume' in protected mode. Most of the work
** depends on the status of the coroutine: initial state, suspended
** inside a hook, or regularly suspended (optionally with a continuation
** function), plus erroneous cases: non-suspended coroutine or dead
** coroutine.
*/
static void resume (cup_State *L, void *ud) {
  int n = *(cast(int*, ud));  /* number of arguments */
  StkId firstArg = L->top - n;  /* first argument */
  CallInfo *ci = L->ci;
  if (L->status == CUP_OK)  /* starting a coroutine? */
    ccall(L, firstArg - 1, CUP_MULTRET, 0);  /* just call its body */
  else {  /* resuming from previous yield */
    cup_assert(L->status == CUP_YIELD);
    L->status = CUP_OK;  /* mark that it is running (again) */
    if (isCup(ci)) {  /* yielded inside a hook? */
      L->top = firstArg;  /* discard arguments */
      cupV_execute(L, ci);  /* just continue running Cup code */
    }
    else {  /* 'common' yield */
      if (ci->u.c.k != NULL) {  /* does it have a continuation function? */
        cup_unlock(L);
        n = (*ci->u.c.k)(L, CUP_YIELD, ci->u.c.ctx); /* call continuation */
        cup_lock(L);
        api_checknelems(L, n);
      }
      cupD_poscall(L, ci, n);  /* finish 'cupD_call' */
    }
    unroll(L, NULL);  /* run continuation */
  }
}


/*
** Unrolls a coroutine in protected mode while there are recoverable
** errors, that is, errors inside a protected call. (Any error
** interrupts 'unroll', and this loop protects it again so it can
** continue.) Stops with a normal end (status == CUP_OK), an yield
** (status == CUP_YIELD), or an unprotected error ('findpcall' doesn't
** find a recover point).
*/
static int precover (cup_State *L, int status) {
  CallInfo *ci;
  while (errorstatus(status) && (ci = findpcall(L)) != NULL) {
    L->ci = ci;  /* Cup down to recovery functions */
    setcistrecst(ci, status);  /* status to finish 'pcall' */
    status = cupD_rawrunprotected(L, unroll, NULL);
  }
  return status;
}


CUP_API int cup_resume (cup_State *L, cup_State *from, int nargs,
                                      int *nresults) {
  int status;
  cup_lock(L);
  if (L->status == CUP_OK) {  /* may be starting a coroutine */
    if (L->ci != &L->base_ci)  /* not in base level? */
      return resume_error(L, "cannot resume non-suspended coroutine", nargs);
    else if (L->top - (L->ci->func + 1) == nargs)  /* no function? */
      return resume_error(L, "cannot resume dead coroutine", nargs);
  }
  else if (L->status != CUP_YIELD)  /* ended with errors? */
    return resume_error(L, "cannot resume dead coroutine", nargs);
  L->nCcalls = (from) ? getCcalls(from) : 0;
  if (getCcalls(L) >= CUPI_MAXCCALLS)
    return resume_error(L, "C stack overflow", nargs);
  L->nCcalls++;
  cupi_userstateresume(L, nargs);
  api_checknelems(L, (L->status == CUP_OK) ? nargs + 1 : nargs);
  status = cupD_rawrunprotected(L, resume, &nargs);
   /* continue running after recoverable errors */
  status = precover(L, status);
  if (l_likely(!errorstatus(status)))
    cup_assert(status == L->status);  /* normal end or yield */
  else {  /* unrecoverable error */
    L->status = cast_byte(status);  /* mark thread as 'dead' */
    cupD_seterrorobj(L, status, L->top);  /* push error message */
    L->ci->top = L->top;
  }
  *nresults = (status == CUP_YIELD) ? L->ci->u2.nyield
                                    : cast_int(L->top - (L->ci->func + 1));
  cup_unlock(L);
  return status;
}


CUP_API int cup_isyieldable (cup_State *L) {
  return yieldable(L);
}


CUP_API int cup_yieldk (cup_State *L, int nresults, cup_KContext ctx,
                        cup_KFunction k) {
  CallInfo *ci;
  cupi_userstateyield(L, nresults);
  cup_lock(L);
  ci = L->ci;
  api_checknelems(L, nresults);
  if (l_unlikely(!yieldable(L))) {
    if (L != G(L)->mainthread)
      cupG_runerror(L, "attempt to yield across a C-call boundary");
    else
      cupG_runerror(L, "attempt to yield from outside a coroutine");
  }
  L->status = CUP_YIELD;
  ci->u2.nyield = nresults;  /* save number of results */
  if (isCup(ci)) {  /* inside a hook? */
    cup_assert(!isCupcode(ci));
    api_check(L, nresults == 0, "hooks cannot yield values");
    api_check(L, k == NULL, "hooks cannot continue after yielding");
  }
  else {
    if ((ci->u.c.k = k) != NULL)  /* is there a continuation? */
      ci->u.c.ctx = ctx;  /* save context */
    cupD_throw(L, CUP_YIELD);
  }
  cup_assert(ci->callstatus & CIST_HOOKED);  /* must be inside a hook */
  cup_unlock(L);
  return 0;  /* return to 'cupD_hook' */
}


/*
** Auxiliary structure to call 'cupF_close' in protected mode.
*/
struct CloseP {
  StkId level;
  int status;
};


/*
** Auxiliary function to call 'cupF_close' in protected mode.
*/
static void closepaux (cup_State *L, void *ud) {
  struct CloseP *pcl = cast(struct CloseP *, ud);
  cupF_close(L, pcl->level, pcl->status, 0);
}


/*
** Calls 'cupF_close' in protected mode. Return the original status
** or, in case of errors, the new status.
*/
int cupD_closeprotected (cup_State *L, ptrdiff_t level, int status) {
  CallInfo *old_ci = L->ci;
  lu_byte old_allowhooks = L->allowhook;
  for (;;) {  /* keep closing upvalues until no more errors */
    struct CloseP pcl;
    pcl.level = restorestack(L, level); pcl.status = status;
    status = cupD_rawrunprotected(L, &closepaux, &pcl);
    if (l_likely(status == CUP_OK))  /* no more errors? */
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
int cupD_pcall (cup_State *L, Pfunc func, void *u,
                ptrdiff_t old_top, ptrdiff_t ef) {
  int status;
  CallInfo *old_ci = L->ci;
  lu_byte old_allowhooks = L->allowhook;
  ptrdiff_t old_errfunc = L->errfunc;
  L->errfunc = ef;
  status = cupD_rawrunprotected(L, func, u);
  if (l_unlikely(status != CUP_OK)) {  /* an error occurred? */
    L->ci = old_ci;
    L->allowhook = old_allowhooks;
    status = cupD_closeprotected(L, old_top, status);
    cupD_seterrorobj(L, status, restorestack(L, old_top));
    cupD_shrinkstack(L);   /* restore stack size in case of overflow */
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


static void checkmode (cup_State *L, const char *mode, const char *x) {
  if (mode && strchr(mode, x[0]) == NULL) {
    cupO_pushfstring(L,
       "attempt to load a %s chunk (mode is '%s')", x, mode);
    cupD_throw(L, CUP_ERRSYNTAX);
  }
}


static void f_parser (cup_State *L, void *ud) {
  LClosure *cl;
  struct SParser *p = cast(struct SParser *, ud);
  int c = zgetc(p->z);  /* read first character */
  if (c == CUP_SIGNATURE[0]) {
    checkmode(L, p->mode, "binary");
    cl = cupU_undump(L, p->z, p->name);
  }
  else {
    checkmode(L, p->mode, "text");
    cl = cupY_parser(L, p->z, &p->buff, &p->dyd, p->name, c);
  }
  cup_assert(cl->nupvalues == cl->p->sizeupvalues);
  cupF_initupvals(L, cl);
}


int cupD_protectedparser (cup_State *L, ZIO *z, const char *name,
                                        const char *mode) {
  struct SParser p;
  int status;
  incnny(L);  /* cannot yield during parsing */
  p.z = z; p.name = name; p.mode = mode;
  p.dyd.actvar.arr = NULL; p.dyd.actvar.size = 0;
  p.dyd.gt.arr = NULL; p.dyd.gt.size = 0;
  p.dyd.label.arr = NULL; p.dyd.label.size = 0;
  cupZ_initbuffer(L, &p.buff);
  status = cupD_pcall(L, f_parser, &p, savestack(L, L->top), L->errfunc);
  cupZ_freebuffer(L, &p.buff);
  cupM_freearray(L, p.dyd.actvar.arr, p.dyd.actvar.size);
  cupM_freearray(L, p.dyd.gt.arr, p.dyd.gt.size);
  cupM_freearray(L, p.dyd.label.arr, p.dyd.label.size);
  decnny(L);
  return status;
}


