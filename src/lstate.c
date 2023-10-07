/*
** $Id: lstate.c $
** Global State
** See Copyright Notice in cup.h
*/

#define lstate_c
#define CUP_CORE

#include "lprefix.h"


#include <stddef.h>
#include <string.h>

#include "cup.h"

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
  lu_byte extra_[CUP_EXTRASPACE];
  cup_State l;
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
#if !defined(cupi_makeseed)

#include <time.h>

/*
** Compute an initial seed with some level of randomness.
** Rely on Address Space Layout Randomization (if present) and
** current time.
*/
#define addbuff(b,p,e) \
  { size_t t = cast_sizet(e); \
    memcpy(b + p, &t, sizeof(t)); p += sizeof(t); }

static unsigned int cupi_makeseed (cup_State *L) {
  char buff[3 * sizeof(size_t)];
  unsigned int h = cast_uint(time(NULL));
  int p = 0;
  addbuff(buff, p, L);  /* heap variable */
  addbuff(buff, p, &h);  /* local variable */
  addbuff(buff, p, &cup_newstate);  /* public function */
  cup_assert(p == sizeof(buff));
  return cupS_hash(buff, p, h);
}

#endif


/*
** set GCdebt to a new value keeping the value (totalbytes + GCdebt)
** invariant (and avoiding underflows in 'totalbytes')
*/
void cupE_setdebt (global_State *g, l_mem debt) {
  l_mem tb = gettotalbytes(g);
  cup_assert(tb > 0);
  if (debt < tb - MAX_LMEM)
    debt = tb - MAX_LMEM;  /* will make 'totalbytes == MAX_LMEM' */
  g->totalbytes = tb - debt;
  g->GCdebt = debt;
}


CUP_API int cup_setcstacklimit (cup_State *L, unsigned int limit) {
  UNUSED(L); UNUSED(limit);
  return CUPI_MAXCCALLS;  /* warning?? */
}


CallInfo *cupE_extendCI (cup_State *L) {
  CallInfo *ci;
  cup_assert(L->ci->next == NULL);
  ci = cupM_new(L, CallInfo);
  cup_assert(L->ci->next == NULL);
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
void cupE_freeCI (cup_State *L) {
  CallInfo *ci = L->ci;
  CallInfo *next = ci->next;
  ci->next = NULL;
  while ((ci = next) != NULL) {
    next = ci->next;
    cupM_free(L, ci);
    L->nci--;
  }
}


/*
** free half of the CallInfo structures not in use by a thread,
** keeping the first one.
*/
void cupE_shrinkCI (cup_State *L) {
  CallInfo *ci = L->ci->next;  /* first free CallInfo */
  CallInfo *next;
  if (ci == NULL)
    return;  /* no extra elements */
  while ((next = ci->next) != NULL) {  /* two extra elements? */
    CallInfo *next2 = next->next;  /* next's next */
    ci->next = next2;  /* remove next from the list */
    L->nci--;
    cupM_free(L, next);  /* free next */
    if (next2 == NULL)
      break;  /* no more elements */
    else {
      next2->previous = ci;
      ci = next2;  /* continue */
    }
  }
}


/*
** Called when 'getCcalls(L)' larger or equal to CUPI_MAXCCALLS.
** If equal, raises an overflow error. If value is larger than
** CUPI_MAXCCALLS (which means it is handling an overflow) but
** not much larger, does not report an error (to allow overflow
** handling to work).
*/
void cupE_checkcstack (cup_State *L) {
  if (getCcalls(L) == CUPI_MAXCCALLS)
    cupG_runerror(L, "C stack overflow");
  else if (getCcalls(L) >= (CUPI_MAXCCALLS / 10 * 11))
    cupD_throw(L, CUP_ERRERR);  /* error while handling stack error */
}


CUPI_FUNC void cupE_incCstack (cup_State *L) {
  L->nCcalls++;
  if (l_unlikely(getCcalls(L) >= CUPI_MAXCCALLS))
    cupE_checkcstack(L);
}


static void stack_init (cup_State *L1, cup_State *L) {
  int i; CallInfo *ci;
  /* initialize stack array */
  L1->stack = cupM_newvector(L, BASIC_STACK_SIZE + EXTRA_STACK, StackValue);
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
  ci->top = L1->top + CUP_MINSTACK;
  L1->ci = ci;
}


static void freestack (cup_State *L) {
  if (L->stack == NULL)
    return;  /* stack not completely built yet */
  L->ci = &L->base_ci;  /* free the entire 'ci' list */
  cupE_freeCI(L);
  cup_assert(L->nci == 0);
  cupM_freearray(L, L->stack, stacksize(L) + EXTRA_STACK);  /* free stack */
}


/*
** Create registry table and its predefined values
*/
static void init_registry (cup_State *L, global_State *g) {
  /* create registry */
  Table *registry = cupH_new(L);
  sethvalue(L, &g->l_registry, registry);
  cupH_resize(L, registry, CUP_RIDX_LAST, 0);
  /* registry[CUP_RIDX_MAINTHREAD] = L */
  setthvalue(L, &registry->array[CUP_RIDX_MAINTHREAD - 1], L);
  /* registry[CUP_RIDX_GLOBALS] = new table (table of globals) */
  sethvalue(L, &registry->array[CUP_RIDX_GLOBALS - 1], cupH_new(L));
}


/*
** open parts of the state that may cause memory-allocation errors.
*/
static void f_cupopen (cup_State *L, void *ud) {
  global_State *g = G(L);
  UNUSED(ud);
  stack_init(L, L);  /* init stack */
  init_registry(L, g);
  cupS_init(L);
  cupT_init(L);
  cupX_init(L);
  g->gcstp = 0;  /* allow gc */
  setnilvalue(&g->nilvalue);  /* now state is complete */
  cupi_userstateopen(L);
}


/*
** preinitialize a thread with consistent values without allocating
** any memory (to avoid errors)
*/
static void preinit_thread (cup_State *L, global_State *g) {
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
  L->status = CUP_OK;
  L->errfunc = 0;
  L->oldpc = 0;
}


static void close_state (cup_State *L) {
  global_State *g = G(L);
  if (!completestate(g))  /* closing a partially built state? */
    cupC_freeallobjects(L);  /* just collect its objects */
  else {  /* closing a fully built state */
    L->ci = &L->base_ci;  /* unwind CallInfo list */
    cupD_closeprotected(L, 1, CUP_OK);  /* close all upvalues */
    cupC_freeallobjects(L);  /* collect all objects */
    cupi_userstateclose(L);
  }
  cupM_freearray(L, G(L)->strt.hash, G(L)->strt.size);
  freestack(L);
  cup_assert(gettotalbytes(g) == sizeof(LG));
  (*g->frealloc)(g->ud, fromstate(L), sizeof(LG), 0);  /* free main block */
}


CUP_API cup_State *cup_newthread (cup_State *L) {
  global_State *g;
  cup_State *L1;
  cup_lock(L);
  g = G(L);
  cupC_checkGC(L);
  /* create new thread */
  L1 = &cast(LX *, cupM_newobject(L, CUP_TTHREAD, sizeof(LX)))->l;
  L1->marked = cupC_white(g);
  L1->tt = CUP_VTHREAD;
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
  memcpy(cup_getextraspace(L1), cup_getextraspace(g->mainthread),
         CUP_EXTRASPACE);
  cupi_userstatethread(L, L1);
  stack_init(L1, L);  /* init stack */
  cup_unlock(L);
  return L1;
}


void cupE_freethread (cup_State *L, cup_State *L1) {
  LX *l = fromstate(L1);
  cupF_closeupval(L1, L1->stack);  /* close all upvalues */
  cup_assert(L1->openupval == NULL);
  cupi_userstatefree(L, L1);
  freestack(L1);
  cupM_free(L, l);
}


int cupE_resetthread (cup_State *L, int status) {
  CallInfo *ci = L->ci = &L->base_ci;  /* unwind CallInfo list */
  setnilvalue(s2v(L->stack));  /* 'function' entry for basic 'ci' */
  ci->func = L->stack;
  ci->callstatus = CIST_C;
  if (status == CUP_YIELD)
    status = CUP_OK;
  L->status = CUP_OK;  /* so it can run __close metamethods */
  status = cupD_closeprotected(L, 1, status);
  if (status != CUP_OK)  /* errors? */
    cupD_seterrorobj(L, status, L->stack + 1);
  else
    L->top = L->stack + 1;
  ci->top = L->top + CUP_MINSTACK;
  cupD_reallocstack(L, cast_int(ci->top - L->stack), 0);
  return status;
}


CUP_API int cup_resetthread (cup_State *L) {
  int status;
  cup_lock(L);
  status = cupE_resetthread(L, L->status);
  cup_unlock(L);
  return status;
}


CUP_API cup_State *cup_newstate (cup_Alloc f, void *ud) {
  int i;
  cup_State *L;
  global_State *g;
  LG *l = cast(LG *, (*f)(ud, NULL, CUP_TTHREAD, sizeof(LG)));
  if (l == NULL) return NULL;
  L = &l->l.l;
  g = &l->g;
  L->tt = CUP_VTHREAD;
  g->currentwhite = bitmask(WHITE0BIT);
  L->marked = cupC_white(g);
  preinit_thread(L, g);
  g->allgc = obj2gco(L);  /* by now, only object is the main thread */
  L->next = NULL;
  incnny(L);  /* main thread is always non yieldable */
  g->frealloc = f;
  g->ud = ud;
  g->warnf = NULL;
  g->ud_warn = NULL;
  g->mainthread = L;
  g->seed = cupi_makeseed(L);
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
  setgcparam(g->gcpause, CUPI_GCPAUSE);
  setgcparam(g->gcstepmul, CUPI_GCMUL);
  g->gcstepsize = CUPI_GCSTEPSIZE;
  setgcparam(g->genmajormul, CUPI_GENMAJORMUL);
  g->genminormul = CUPI_GENMINORMUL;
  for (i=0; i < CUP_NUMTAGS; i++) g->mt[i] = NULL;
  if (cupD_rawrunprotected(L, f_cupopen, NULL) != CUP_OK) {
    /* memory allocation error: free partial state */
    close_state(L);
    L = NULL;
  }
  return L;
}


CUP_API void cup_close (cup_State *L) {
  cup_lock(L);
  L = G(L)->mainthread;  /* only the main thread can be closed */
  close_state(L);
}


void cupE_warning (cup_State *L, const char *msg, int tocont) {
  cup_WarnFunction wf = G(L)->warnf;
  if (wf != NULL)
    wf(G(L)->ud_warn, msg, tocont);
}


/*
** Generate a warning from an error message
*/
void cupE_warnerror (cup_State *L, const char *where) {
  TValue *errobj = s2v(L->top - 1);  /* error object */
  const char *msg = (ttisstring(errobj))
                  ? svalue(errobj)
                  : "error object is not a string";
  /* produce warning "error in %s (%s)" (where, msg) */
  cupE_warning(L, "error in ", 1);
  cupE_warning(L, where, 1);
  cupE_warning(L, " (", 1);
  cupE_warning(L, msg, 1);
  cupE_warning(L, ")", 0);
}

