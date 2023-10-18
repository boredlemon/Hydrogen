/*
** $Id: lstate.c $
** Global State
** See Copyright Notice in acorn.h
*/

#define lstate_c
#define ACORN_CORE

#include "lprefix.h"


#include <stddef.h>
#include <string.h>

#include "acorn.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"



/*
** thread state + extra space
*/
typedef struct LX {
  lu_byte extra_[ACORN_EXTRASPACE];
  acorn_State l;
} LX;


/*
** Main thread combines a thread state and the global state
*/
typedef struct LG {
  LX l;
  global_State g;
} LG;



#define fromstate(L)	(cast(LX *, cast(lu_byte *, (L)) - offsetof(LX, l)))


/*
** A macro to create a "random" seed when a state is created;
** the seed is used to randomize string hashes.
*/
#if !defined(acorni_makeseed)

#include <time.h>

/*
** Compute an initial seed with some level of randomness.
** Rely on Address Space Layout Randomization (if present) and
** current time.
*/
#define addbuff(b,p,e) \
  { size_t t = cast_sizet(e); \
    memcpy(b + p, &t, sizeof(t)); p += sizeof(t); }

static unsigned int acorni_makeseed (acorn_State *L) {
  char buff[3 * sizeof(size_t)];
  unsigned int h = cast_uint(time(NULL));
  int p = 0;
  addbuff(buff, p, L);  /* heap variable */
  addbuff(buff, p, &h);  /* local variable */
  addbuff(buff, p, &acorn_newstate);  /* public function */
  acorn_assert(p == sizeof(buff));
  return acornS_hash(buff, p, h);
}

#endif


/*
** set GCdebt to a new value keeping the value (totalbytes + GCdebt)
** invariant (and avoiding underflows in 'totalbytes')
*/
void acornE_setdebt (global_State *g, l_mem debt) {
  l_mem tb = gettotalbytes(g);
  acorn_assert(tb > 0);
  if (debt < tb - MAX_LMEM)
    debt = tb - MAX_LMEM;  /* will make 'totalbytes == MAX_LMEM' */
  g->totalbytes = tb - debt;
  g->GCdebt = debt;
}


ACORN_API int acorn_setcstacklimit (acorn_State *L, unsigned int limit) {
  UNUSED(L); UNUSED(limit);
  return ACORNI_MAXCCALLS;  /* warning?? */
}


CallInfo *acornE_extendCI (acorn_State *L) {
  CallInfo *ci;
  acorn_assert(L->ci->next == NULL);
  ci = acornM_new(L, CallInfo);
  acorn_assert(L->ci->next == NULL);
  L->ci->next = ci;
  ci->previous = L->ci;
  ci->next = NULL;
  ci->u.l.trap = 0;
  L->nci++;
  return ci;
}


/*
** free all CallInfo structures not in use by a thread
*/
void acornE_freeCI (acorn_State *L) {
  CallInfo *ci = L->ci;
  CallInfo *next = ci->next;
  ci->next = NULL;
  while ((ci = next) != NULL) {
    next = ci->next;
    acornM_free(L, ci);
    L->nci--;
  }
}


/*
** free half of the CallInfo structures not in use by a thread,
** keeping the first one.
*/
void acornE_shrinkCI (acorn_State *L) {
  CallInfo *ci = L->ci->next;  /* first free CallInfo */
  CallInfo *next;
  if (ci == NULL)
    return;  /* no extra elements */
  while ((next = ci->next) != NULL) {  /* two extra elements? */
    CallInfo *next2 = next->next;  /* next's next */
    ci->next = next2;  /* remove next from the list */
    L->nci--;
    acornM_free(L, next);  /* free next */
    if (next2 == NULL)
      break;  /* no more elements */
    else {
      next2->previous = ci;
      ci = next2;  /* continue */
    }
  }
}


/*
** Called when 'getCcalls(L)' larger or equal to ACORNI_MAXCCALLS.
** If equal, raises an overflow error. If value is larger than
** ACORNI_MAXCCALLS (which means it is handling an overflow) but
** not much larger, does not report an error (to allow overflow
** handling to work).
*/
void acornE_checkcstack (acorn_State *L) {
  if (getCcalls(L) == ACORNI_MAXCCALLS)
    acornG_runerror(L, "C stack overflow");
  else if (getCcalls(L) >= (ACORNI_MAXCCALLS / 10 * 11))
    acornD_throw(L, ACORN_ERRERR);  /* error while handling stack error */
}


ACORNI_FUNC void acornE_incCstack (acorn_State *L) {
  L->nCcalls++;
  if (l_unlikely(getCcalls(L) >= ACORNI_MAXCCALLS))
    acornE_checkcstack(L);
}


static void stack_init (acorn_State *L1, acorn_State *L) {
  int i; CallInfo *ci;
  /* initialize stack array */
  L1->stack = acornM_newvector(L, BASIC_STACK_SIZE + EXTRA_STACK, StackValue);
  L1->tbclist = L1->stack;
  for (i = 0; i < BASIC_STACK_SIZE + EXTRA_STACK; i++)
    setnilvalue(s2v(L1->stack + i));  /* erase new stack */
  L1->top = L1->stack;
  L1->stack_last = L1->stack + BASIC_STACK_SIZE;
  /* initialize first ci */
  ci = &L1->base_ci;
  ci->next = ci->previous = NULL;
  ci->callstatus = CIST_C;
  ci->func = L1->top;
  ci->u.c.k = NULL;
  ci->nresults = 0;
  setnilvalue(s2v(L1->top));  /* 'function' entry for this 'ci' */
  L1->top++;
  ci->top = L1->top + ACORN_MINSTACK;
  L1->ci = ci;
}


static void freestack (acorn_State *L) {
  if (L->stack == NULL)
    return;  /* stack not completely built yet */
  L->ci = &L->base_ci;  /* free the entire 'ci' list */
  acornE_freeCI(L);
  acorn_assert(L->nci == 0);
  acornM_freearray(L, L->stack, stacksize(L) + EXTRA_STACK);  /* free stack */
}


/*
** Create registry table and its predefined values
*/
static void init_registry (acorn_State *L, global_State *g) {
  /* create registry */
  Table *registry = acornH_new(L);
  sethvalue(L, &g->l_registry, registry);
  acornH_resize(L, registry, ACORN_RIDX_LAST, 0);
  /* registry[ACORN_RIDX_MAINTHREAD] = L */
  setthvalue(L, &registry->array[ACORN_RIDX_MAINTHREAD - 1], L);
  /* registry[ACORN_RIDX_GLOBALS] = new table (table of globals) */
  sethvalue(L, &registry->array[ACORN_RIDX_GLOBALS - 1], acornH_new(L));
}


/*
** open parts of the state that may cause memory-allocation errors.
*/
static void f_acornopen (acorn_State *L, void *ud) {
  global_State *g = G(L);
  UNUSED(ud);
  stack_init(L, L);  /* init stack */
  init_registry(L, g);
  acornS_init(L);
  acornT_init(L);
  acornX_init(L);
  g->gcstp = 0;  /* allow gc */
  setnilvalue(&g->nilvalue);  /* now state is complete */
  acorni_userstateopen(L);
}


/*
** preinitialize a thread with consistent values without allocating
** any memory (to avoid errors)
*/
static void preinit_thread (acorn_State *L, global_State *g) {
  G(L) = g;
  L->stack = NULL;
  L->ci = NULL;
  L->nci = 0;
  L->twups = L;  /* thread has no upvalues */
  L->nCcalls = 0;
  L->errorJmp = NULL;
  L->hook = NULL;
  L->hookmask = 0;
  L->basehookcount = 0;
  L->allowhook = 1;
  resethookcount(L);
  L->openupval = NULL;
  L->status = ACORN_OK;
  L->errfunc = 0;
  L->oldpc = 0;
}


static void close_state (acorn_State *L) {
  global_State *g = G(L);
  if (!completestate(g))  /* closing a partially built state? */
    acornC_freeallobjects(L);  /* just collect its objects */
  else {  /* closing a fully built state */
    L->ci = &L->base_ci;  /* unwind CallInfo list */
    acornD_closeprotected(L, 1, ACORN_OK);  /* close all upvalues */
    acornC_freeallobjects(L);  /* collect all objects */
    acorni_userstateclose(L);
  }
  acornM_freearray(L, G(L)->strt.hash, G(L)->strt.size);
  freestack(L);
  acorn_assert(gettotalbytes(g) == sizeof(LG));
  (*g->frealloc)(g->ud, fromstate(L), sizeof(LG), 0);  /* free main block */
}


ACORN_API acorn_State *acorn_newthread (acorn_State *L) {
  global_State *g;
  acorn_State *L1;
  acorn_lock(L);
  g = G(L);
  acornC_checkGC(L);
  /* create new thread */
  L1 = &cast(LX *, acornM_newobject(L, ACORN_TTHREAD, sizeof(LX)))->l;
  L1->marked = acornC_white(g);
  L1->tt = ACORN_VTHREAD;
  /* link it on list 'allgc' */
  L1->next = g->allgc;
  g->allgc = obj2gco(L1);
  /* anchor it on L stack */
  setthvalue2s(L, L->top, L1);
  api_incr_top(L);
  preinit_thread(L1, g);
  L1->hookmask = L->hookmask;
  L1->basehookcount = L->basehookcount;
  L1->hook = L->hook;
  resethookcount(L1);
  /* initialize L1 extra space */
  memcpy(acorn_getextraspace(L1), acorn_getextraspace(g->mainthread),
         ACORN_EXTRASPACE);
  acorni_userstatethread(L, L1);
  stack_init(L1, L);  /* init stack */
  acorn_unlock(L);
  return L1;
}


void acornE_freethread (acorn_State *L, acorn_State *L1) {
  LX *l = fromstate(L1);
  acornF_closeupval(L1, L1->stack);  /* close all upvalues */
  acorn_assert(L1->openupval == NULL);
  acorni_userstatefree(L, L1);
  freestack(L1);
  acornM_free(L, l);
}


int acornE_resetthread (acorn_State *L, int status) {
  CallInfo *ci = L->ci = &L->base_ci;  /* unwind CallInfo list */
  setnilvalue(s2v(L->stack));  /* 'function' entry for basic 'ci' */
  ci->func = L->stack;
  ci->callstatus = CIST_C;
  if (status == ACORN_YIELD)
    status = ACORN_OK;
  L->status = ACORN_OK;  /* so it can run __close metamethods */
  status = acornD_closeprotected(L, 1, status);
  if (status != ACORN_OK)  /* errors? */
    acornD_seterrorobj(L, status, L->stack + 1);
  else
    L->top = L->stack + 1;
  ci->top = L->top + ACORN_MINSTACK;
  acornD_reallocstack(L, cast_int(ci->top - L->stack), 0);
  return status;
}


ACORN_API int acorn_resetthread (acorn_State *L) {
  int status;
  acorn_lock(L);
  status = acornE_resetthread(L, L->status);
  acorn_unlock(L);
  return status;
}


ACORN_API acorn_State *acorn_newstate (acorn_Alloc f, void *ud) {
  int i;
  acorn_State *L;
  global_State *g;
  LG *l = cast(LG *, (*f)(ud, NULL, ACORN_TTHREAD, sizeof(LG)));
  if (l == NULL) return NULL;
  L = &l->l.l;
  g = &l->g;
  L->tt = ACORN_VTHREAD;
  g->currentwhite = bitmask(WHITE0BIT);
  L->marked = acornC_white(g);
  preinit_thread(L, g);
  g->allgc = obj2gco(L);  /* by now, only object is the main thread */
  L->next = NULL;
  incnny(L);  /* main thread is always non yieldable */
  g->frealloc = f;
  g->ud = ud;
  g->warnf = NULL;
  g->ud_warn = NULL;
  g->mainthread = L;
  g->seed = acorni_makeseed(L);
  g->gcstp = GCSTPGC;  /* no GC while building state */
  g->strt.size = g->strt.nuse = 0;
  g->strt.hash = NULL;
  setnilvalue(&g->l_registry);
  g->panic = NULL;
  g->gcstate = GCSpause;
  g->gckind = KGC_INC;
  g->gcstopem = 0;
  g->gcemergency = 0;
  g->finobj = g->tobefnz = g->fixedgc = NULL;
  g->firstold1 = g->survival = g->old1 = g->reallyold = NULL;
  g->finobjsur = g->finobjold1 = g->finobjrold = NULL;
  g->sweepgc = NULL;
  g->gray = g->grayagain = NULL;
  g->weak = g->ephemeron = g->allweak = NULL;
  g->twups = NULL;
  g->totalbytes = sizeof(LG);
  g->GCdebt = 0;
  g->lastatomic = 0;
  setivalue(&g->nilvalue, 0);  /* to signal that state is not yet built */
  setgcparam(g->gcpause, ACORNI_GCPAUSE);
  setgcparam(g->gcstepmul, ACORNI_GCMUL);
  g->gcstepsize = ACORNI_GCSTEPSIZE;
  setgcparam(g->genmajormul, ACORNI_GENMAJORMUL);
  g->genminormul = ACORNI_GENMINORMUL;
  for (i=0; i < ACORN_NUMTAGS; i++) g->mt[i] = NULL;
  if (acornD_rawrunprotected(L, f_acornopen, NULL) != ACORN_OK) {
    /* memory allocation error: free partial state */
    close_state(L);
    L = NULL;
  }
  return L;
}


ACORN_API void acorn_close (acorn_State *L) {
  acorn_lock(L);
  L = G(L)->mainthread;  /* only the main thread can be closed */
  close_state(L);
}


void acornE_warning (acorn_State *L, const char *msg, int tocont) {
  acorn_WarnFunction wf = G(L)->warnf;
  if (wf != NULL)
    wf(G(L)->ud_warn, msg, tocont);
}


/*
** Generate a warning from an error message
*/
void acornE_warnerror (acorn_State *L, const char *where) {
  TValue *errobj = s2v(L->top - 1);  /* error object */
  const char *msg = (ttisstring(errobj))
                  ? svalue(errobj)
                  : "error object is not a string";
  /* produce warning "error in %s (%s)" (where, msg) */
  acornE_warning(L, "error in ", 1);
  acornE_warning(L, where, 1);
  acornE_warning(L, " (", 1);
  acornE_warning(L, msg, 1);
  acornE_warning(L, ")", 0);
}

