/*
** $Id: lstate.c $
** Global State
** See Copyright Notice in viper.h
*/

#define lstate_c
#define VIPER_CORE

#include "lprefix.h"


#include <stddef.h>
#include <string.h>

#include "viper.h"

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
  lu_byte extra_[VIPER_EXTRASPACE];
  viper_State l;
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
#if !defined(viperi_makeseed)

#include <time.h>

/*
** Compute an initial seed with some level of randomness.
** Rely on Address Space Layout Randomization (if present) and
** current time.
*/
#define addbuff(b,p,e) \
  { size_t t = cast_sizet(e); \
    memcpy(b + p, &t, sizeof(t)); p += sizeof(t); }

static unsigned int viperi_makeseed (viper_State *L) {
  char buff[3 * sizeof(size_t)];
  unsigned int h = cast_uint(time(NULL));
  int p = 0;
  addbuff(buff, p, L);  /* heap variable */
  addbuff(buff, p, &h);  /* local variable */
  addbuff(buff, p, &viper_newstate);  /* public function */
  viper_assert(p == sizeof(buff));
  return viperS_hash(buff, p, h);
}

#endif


/*
** set GCdebt to a new value keeping the value (totalbytes + GCdebt)
** invariant (and avoiding underflows in 'totalbytes')
*/
void viperE_setdebt (global_State *g, l_mem debt) {
  l_mem tb = gettotalbytes(g);
  viper_assert(tb > 0);
  if (debt < tb - MAX_LMEM)
    debt = tb - MAX_LMEM;  /* will make 'totalbytes == MAX_LMEM' */
  g->totalbytes = tb - debt;
  g->GCdebt = debt;
}


VIPER_API int viper_setcstacklimit (viper_State *L, unsigned int limit) {
  UNUSED(L); UNUSED(limit);
  return VIPERI_MAXCCALLS;  /* warning?? */
}


CallInfo *viperE_extendCI (viper_State *L) {
  CallInfo *ci;
  viper_assert(L->ci->next == NULL);
  ci = viperM_new(L, CallInfo);
  viper_assert(L->ci->next == NULL);
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
void viperE_freeCI (viper_State *L) {
  CallInfo *ci = L->ci;
  CallInfo *next = ci->next;
  ci->next = NULL;
  while ((ci = next) != NULL) {
    next = ci->next;
    viperM_free(L, ci);
    L->nci--;
  }
}


/*
** free half of the CallInfo structures not in use by a thread,
** keeping the first one.
*/
void viperE_shrinkCI (viper_State *L) {
  CallInfo *ci = L->ci->next;  /* first free CallInfo */
  CallInfo *next;
  if (ci == NULL)
    return;  /* no extra elements */
  while ((next = ci->next) != NULL) {  /* two extra elements? */
    CallInfo *next2 = next->next;  /* next's next */
    ci->next = next2;  /* remove next from the list */
    L->nci--;
    viperM_free(L, next);  /* free next */
    if (next2 == NULL)
      break;  /* no more elements */
    else {
      next2->previous = ci;
      ci = next2;  /* continue */
    }
  }
}


/*
** Called when 'getCcalls(L)' larger or equal to VIPERI_MAXCCALLS.
** If equal, raises an overflow error. If value is larger than
** VIPERI_MAXCCALLS (which means it is handling an overflow) but
** not much larger, does not report an error (to allow overflow
** handling to work).
*/
void viperE_checkcstack (viper_State *L) {
  if (getCcalls(L) == VIPERI_MAXCCALLS)
    viperG_runerror(L, "C stack overflow");
  else if (getCcalls(L) >= (VIPERI_MAXCCALLS / 10 * 11))
    viperD_throw(L, VIPER_ERRERR);  /* error while handling stack error */
}


VIPERI_FUNC void viperE_incCstack (viper_State *L) {
  L->nCcalls++;
  if (l_unlikely(getCcalls(L) >= VIPERI_MAXCCALLS))
    viperE_checkcstack(L);
}


static void stack_init (viper_State *L1, viper_State *L) {
  int i; CallInfo *ci;
  /* initialize stack array */
  L1->stack = viperM_newvector(L, BASIC_STACK_SIZE + EXTRA_STACK, StackValue);
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
  ci->top = L1->top + VIPER_MINSTACK;
  L1->ci = ci;
}


static void freestack (viper_State *L) {
  if (L->stack == NULL)
    return;  /* stack not completely built yet */
  L->ci = &L->base_ci;  /* free the entire 'ci' list */
  viperE_freeCI(L);
  viper_assert(L->nci == 0);
  viperM_freearray(L, L->stack, stacksize(L) + EXTRA_STACK);  /* free stack */
}


/*
** Create registry table and its predefined values
*/
static void init_registry (viper_State *L, global_State *g) {
  /* create registry */
  Table *registry = viperH_new(L);
  sethvalue(L, &g->l_registry, registry);
  viperH_resize(L, registry, VIPER_RIDX_LAST, 0);
  /* registry[VIPER_RIDX_MAINTHREAD] = L */
  setthvalue(L, &registry->array[VIPER_RIDX_MAINTHREAD - 1], L);
  /* registry[VIPER_RIDX_GLOBALS] = new table (table of globals) */
  sethvalue(L, &registry->array[VIPER_RIDX_GLOBALS - 1], viperH_new(L));
}


/*
** open parts of the state that may cause memory-allocation errors.
*/
static void f_viperopen (viper_State *L, void *ud) {
  global_State *g = G(L);
  UNUSED(ud);
  stack_init(L, L);  /* init stack */
  init_registry(L, g);
  viperS_init(L);
  viperT_init(L);
  viperX_init(L);
  g->gcstp = 0;  /* allow gc */
  setnilvalue(&g->nilvalue);  /* now state is complete */
  viperi_userstateopen(L);
}


/*
** preinitialize a thread with consistent values without allocating
** any memory (to avoid errors)
*/
static void preinit_thread (viper_State *L, global_State *g) {
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
  L->status = VIPER_OK;
  L->errfunc = 0;
  L->oldpc = 0;
}


static void close_state (viper_State *L) {
  global_State *g = G(L);
  if (!completestate(g))  /* closing a partially built state? */
    viperC_freeallobjects(L);  /* just collect its objects */
  else {  /* closing a fully built state */
    L->ci = &L->base_ci;  /* unwind CallInfo list */
    viperD_closeprotected(L, 1, VIPER_OK);  /* close all upvalues */
    viperC_freeallobjects(L);  /* collect all objects */
    viperi_userstateclose(L);
  }
  viperM_freearray(L, G(L)->strt.hash, G(L)->strt.size);
  freestack(L);
  viper_assert(gettotalbytes(g) == sizeof(LG));
  (*g->frealloc)(g->ud, fromstate(L), sizeof(LG), 0);  /* free main block */
}


VIPER_API viper_State *viper_newthread (viper_State *L) {
  global_State *g;
  viper_State *L1;
  viper_lock(L);
  g = G(L);
  viperC_checkGC(L);
  /* create new thread */
  L1 = &cast(LX *, viperM_newobject(L, VIPER_TTHREAD, sizeof(LX)))->l;
  L1->marked = viperC_white(g);
  L1->tt = VIPER_VTHREAD;
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
  memcpy(viper_getextraspace(L1), viper_getextraspace(g->mainthread),
         VIPER_EXTRASPACE);
  viperi_userstatethread(L, L1);
  stack_init(L1, L);  /* init stack */
  viper_unlock(L);
  return L1;
}


void viperE_freethread (viper_State *L, viper_State *L1) {
  LX *l = fromstate(L1);
  viperF_closeupval(L1, L1->stack);  /* close all upvalues */
  viper_assert(L1->openupval == NULL);
  viperi_userstatefree(L, L1);
  freestack(L1);
  viperM_free(L, l);
}


int viperE_resetthread (viper_State *L, int status) {
  CallInfo *ci = L->ci = &L->base_ci;  /* unwind CallInfo list */
  setnilvalue(s2v(L->stack));  /* 'function' entry for basic 'ci' */
  ci->func = L->stack;
  ci->callstatus = CIST_C;
  if (status == VIPER_YIELD)
    status = VIPER_OK;
  L->status = VIPER_OK;  /* so it can run __close metamethods */
  status = viperD_closeprotected(L, 1, status);
  if (status != VIPER_OK)  /* errors? */
    viperD_seterrorobj(L, status, L->stack + 1);
  else
    L->top = L->stack + 1;
  ci->top = L->top + VIPER_MINSTACK;
  viperD_reallocstack(L, cast_int(ci->top - L->stack), 0);
  return status;
}


VIPER_API int viper_resetthread (viper_State *L) {
  int status;
  viper_lock(L);
  status = viperE_resetthread(L, L->status);
  viper_unlock(L);
  return status;
}


VIPER_API viper_State *viper_newstate (viper_Alloc f, void *ud) {
  int i;
  viper_State *L;
  global_State *g;
  LG *l = cast(LG *, (*f)(ud, NULL, VIPER_TTHREAD, sizeof(LG)));
  if (l == NULL) return NULL;
  L = &l->l.l;
  g = &l->g;
  L->tt = VIPER_VTHREAD;
  g->currentwhite = bitmask(WHITE0BIT);
  L->marked = viperC_white(g);
  preinit_thread(L, g);
  g->allgc = obj2gco(L);  /* by now, only object is the main thread */
  L->next = NULL;
  incnny(L);  /* main thread is always non yieldable */
  g->frealloc = f;
  g->ud = ud;
  g->warnf = NULL;
  g->ud_warn = NULL;
  g->mainthread = L;
  g->seed = viperi_makeseed(L);
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
  setgcparam(g->gcpause, VIPERI_GCPAUSE);
  setgcparam(g->gcstepmul, VIPERI_GCMUL);
  g->gcstepsize = VIPERI_GCSTEPSIZE;
  setgcparam(g->genmajormul, VIPERI_GENMAJORMUL);
  g->genminormul = VIPERI_GENMINORMUL;
  for (i=0; i < VIPER_NUMTAGS; i++) g->mt[i] = NULL;
  if (viperD_rawrunprotected(L, f_viperopen, NULL) != VIPER_OK) {
    /* memory allocation error: free partial state */
    close_state(L);
    L = NULL;
  }
  return L;
}


VIPER_API void viper_close (viper_State *L) {
  viper_lock(L);
  L = G(L)->mainthread;  /* only the main thread can be closed */
  close_state(L);
}


void viperE_warning (viper_State *L, const char *msg, int tocont) {
  viper_WarnFunction wf = G(L)->warnf;
  if (wf != NULL)
    wf(G(L)->ud_warn, msg, tocont);
}


/*
** Generate a warning from an error message
*/
void viperE_warnerror (viper_State *L, const char *where) {
  TValue *errobj = s2v(L->top - 1);  /* error object */
  const char *msg = (ttisstring(errobj))
                  ? svalue(errobj)
                  : "error object is not a string";
  /* produce warning "error in %s (%s)" (where, msg) */
  viperE_warning(L, "error in ", 1);
  viperE_warning(L, where, 1);
  viperE_warning(L, " (", 1);
  viperE_warning(L, msg, 1);
  viperE_warning(L, ")", 0);
}

