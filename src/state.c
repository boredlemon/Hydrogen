/*
** $Id: state.c $
** Global State
** See Copyright Notice in hydrogen.h
*/

#define state_c
#define HYDROGEN_CORE

#include "prefix.h"


#include <stddef.h>
#include <string.h>

#include "hydrogen.h"

#include "api.h"
#include "debug.h"
#include "do.h"
#include "function.h"
#include "garbageCollection.h"
#include "lexer.h"
#include "memory.h"
#include "state.h"
#include "string.h"
#include "table.h"
#include "tagMethods.h"



/*
** thread state + extra space
*/
typedef struct LX {
  lu_byte extra_[HYDROGEN_EXTRASPACE];
  hydrogen_State l;
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
#if !defined(hydrogeni_makeseed)

#include <time.h>

/*
** Compute an initial seed with some level of randomness.
** Rely on Address Space Layout Randomization (if present) and
** current time.
*/
#define addbuff(b,p,e) \
  { size_t t = cast_sizet(e); \
    memcpy(b + p, &t, sizeof(t)); p += sizeof(t); }

static unsigned int hydrogeni_makeseed (hydrogen_State *L) {
  char buff[3 * sizeof(size_t)];
  unsigned int h = cast_uint(time(NULL));
  int p = 0;
  addbuff(buff, p, L);  /* heap variable */
  addbuff(buff, p, &h);  /* local variable */
  addbuff(buff, p, &hydrogen_newstate);  /* public function */
  hydrogen_assert(p == sizeof(buff));
  return hydrogenS_hash(buff, p, h);
}

#endif


/*
** set GCdebt to a new value keeping the value (totalbytes + GCdebt)
** invariant (and avoiding underflows in 'totalbytes')
*/
void hydrogenE_setdebt (global_State *g, l_mem debt) {
  l_mem tb = gettotalbytes(g);
  hydrogen_assert(tb > 0);
  if (debt < tb - MAX_memory)
    debt = tb - MAX_memory;  /* will make 'totalbytes == MAX_memory' */
  g->totalbytes = tb - debt;
  g->GCdebt = debt;
}


HYDROGEN_API int hydrogen_setcstacklimit (hydrogen_State *L, unsigned int limit) {
  UNUSED(L); UNUSED(limit);
  return HYDROGENI_MAXCCALLS;  /* warning?? */
}


CallInfo *hydrogenE_extendCI (hydrogen_State *L) {
  CallInfo *ci;
  hydrogen_assert(L->ci->next == NULL);
  ci = hydrogenM_new(L, CallInfo);
  hydrogen_assert(L->ci->next == NULL);
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
void hydrogenE_freeCI (hydrogen_State *L) {
  CallInfo *ci = L->ci;
  CallInfo *next = ci->next;
  ci->next = NULL;
  while ((ci = next) != NULL) {
    next = ci->next;
    hydrogenM_free(L, ci);
    L->nci--;
  }
}


/*
** free half of the CallInfo structures not in use by a thread,
** keeping the first one.
*/
void hydrogenE_shrinkCI (hydrogen_State *L) {
  CallInfo *ci = L->ci->next;  /* first free CallInfo */
  CallInfo *next;
  if (ci == NULL)
    return;  /* no extra elements */
  while ((next = ci->next) != NULL) {  /* two extra elements? */
    CallInfo *next2 = next->next;  /* next's next */
    ci->next = next2;  /* remove next from the list */
    L->nci--;
    hydrogenM_free(L, next);  /* free next */
    if (next2 == NULL)
      break;  /* no more elements */
    else {
      next2->previous = ci;
      ci = next2;  /* continue */
    }
  }
}


/*
** Called when 'getCcalls(L)' larger or equal to HYDROGENI_MAXCCALLS.
** If equal, raises an overflow error. If value is larger than
** HYDROGENI_MAXCCALLS (which means it is handling an overflow) but
** not much larger, does not report an error (to allow overflow
** handling to work).
*/
void hydrogenE_checkcstack (hydrogen_State *L) {
  if (getCcalls(L) == HYDROGENI_MAXCCALLS)
    hydrogenG_runerror(L, "C stack overflow");
  else if (getCcalls(L) >= (HYDROGENI_MAXCCALLS / 10 * 11))
    hydrogenD_throw(L, HYDROGEN_ERRERR);  /* error while handling stack error */
}


HYDROGENI_FUNC void hydrogenE_incCstack (hydrogen_State *L) {
  L->nCcalls++;
  if (l_unlikely(getCcalls(L) >= HYDROGENI_MAXCCALLS))
    hydrogenE_checkcstack(L);
}


static void stack_init (hydrogen_State *L1, hydrogen_State *L) {
  int i; CallInfo *ci;
  /* initialize stack array */
  L1->stack = hydrogenM_newvector(L, BASIC_STACK_SIZE + EXTRA_STACK, StackValue);
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
  ci->top = L1->top + HYDROGEN_MINSTACK;
  L1->ci = ci;
}


static void freestack (hydrogen_State *L) {
  if (L->stack == NULL)
    return;  /* stack not completely built yet */
  L->ci = &L->base_ci;  /* free the entire 'ci' list */
  hydrogenE_freeCI(L);
  hydrogen_assert(L->nci == 0);
  hydrogenM_freearray(L, L->stack, stacksize(L) + EXTRA_STACK);  /* free stack */
}


/*
** Create registry table and its predefined values
*/
static void init_registry (hydrogen_State *L, global_State *g) {
  /* create registry */
  Table *registry = hydrogenH_new(L);
  sethvalue(L, &g->l_registry, registry);
  hydrogenH_resize(L, registry, HYDROGEN_RIDX_LAST, 0);
  /* registry[HYDROGEN_RIDX_MAINTHREAD] = L */
  setthvalue(L, &registry->array[HYDROGEN_RIDX_MAINTHREAD - 1], L);
  /* registry[HYDROGEN_RIDX_GLOBALS] = new table (table of globals) */
  sethvalue(L, &registry->array[HYDROGEN_RIDX_GLOBALS - 1], hydrogenH_new(L));
}


/*
** open parts of the state that may cause memory-allocation errors.
*/
static void f_hydrogenopen (hydrogen_State *L, void *ud) {
  global_State *g = G(L);
  UNUSED(ud);
  stack_init(L, L);  /* init stack */
  init_registry(L, g);
  hydrogenS_init(L);
  hydrogenT_init(L);
  hydrogenX_init(L);
  g->gcstp = 0;  /* allow gc */
  setnilvalue(&g->nilvalue);  /* now state is complete */
  hydrogeni_userstateopen(L);
}


/*
** preinitialize a thread with consistent values without allocating
** any memory (to avoid errors)
*/
static void preinit_thread (hydrogen_State *L, global_State *g) {
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
  L->status = HYDROGEN_OK;
  L->errfunc = 0;
  L->oldpc = 0;
}


static void close_state (hydrogen_State *L) {
  global_State *g = G(L);
  if (!completestate(g))  /* closing a partially built state? */
    hydrogenC_freealobjects(L);  /* just collect its objects */
  else {  /* closing a fully built state */
    L->ci = &L->base_ci;  /* unwind CallInfo list */
    hydrogenD_closeprotected(L, 1, HYDROGEN_OK);  /* close all upvalues */
    hydrogenC_freealobjects(L);  /* collect all objects */
    hydrogeni_userstateclose(L);
  }
  hydrogenM_freearray(L, G(L)->strt.hash, G(L)->strt.size);
  freestack(L);
  hydrogen_assert(gettotalbytes(g) == sizeof(LG));
  (*g->frealloc)(g->ud, fromstate(L), sizeof(LG), 0);  /* free main block */
}


HYDROGEN_API hydrogen_State *hydrogen_newthread (hydrogen_State *L) {
  global_State *g;
  hydrogen_State *L1;
  hydrogen_lock(L);
  g = G(L);
  hydrogenC_checkGC(L);
  /* create new thread */
  L1 = &cast(LX *, hydrogenM_newobject(L, HYDROGEN_TTHREAD, sizeof(LX)))->l;
  L1->marked = hydrogenC_white(g);
  L1->tt = HYDROGEN_VTHREAD;
  /* link it on list 'allgarbageCollection' */
  L1->next = g->allgarbageCollection;
  g->allgarbageCollection = obj2gco(L1);
  /* anchor it on L stack */
  setthvalue2s(L, L->top, L1);
  api_incr_top(L);
  preinit_thread(L1, g);
  L1->hookmask = L->hookmask;
  L1->basehookcount = L->basehookcount;
  L1->hook = L->hook;
  resethookcount(L1);
  /* initialize L1 extra space */
  memcpy(hydrogen_getextraspace(L1), hydrogen_getextraspace(g->mainthread),
         HYDROGEN_EXTRASPACE);
  hydrogeni_userstatethread(L, L1);
  stack_init(L1, L);  /* init stack */
  hydrogen_unlock(L);
  return L1;
}


void hydrogenE_freethread (hydrogen_State *L, hydrogen_State *L1) {
  LX *l = fromstate(L1);
  hydrogenF_closeupval(L1, L1->stack);  /* close all upvalues */
  hydrogen_assert(L1->openupval == NULL);
  hydrogeni_userstatefree(L, L1);
  freestack(L1);
  hydrogenM_free(L, l);
}


int hydrogenE_resetthread (hydrogen_State *L, int status) {
  CallInfo *ci = L->ci = &L->base_ci;  /* unwind CallInfo list */
  setnilvalue(s2v(L->stack));  /* 'function' entry for basic 'ci' */
  ci->func = L->stack;
  ci->callstatus = CIST_C;
  if (status == HYDROGEN_YIELD)
    status = HYDROGEN_OK;
  L->status = HYDROGEN_OK;  /* so it can run __close metamethods */
  status = hydrogenD_closeprotected(L, 1, status);
  if (status != HYDROGEN_OK)  /* errors? */
    hydrogenD_seterrorobj(L, status, L->stack + 1);
  else
    L->top = L->stack + 1;
  ci->top = L->top + HYDROGEN_MINSTACK;
  hydrogenD_reallocstack(L, cast_int(ci->top - L->stack), 0);
  return status;
}


HYDROGEN_API int hydrogen_resetthread (hydrogen_State *L) {
  int status;
  hydrogen_lock(L);
  status = hydrogenE_resetthread(L, L->status);
  hydrogen_unlock(L);
  return status;
}


HYDROGEN_API hydrogen_State *hydrogen_newstate (hydrogen_Alloc f, void *ud) {
  int i;
  hydrogen_State *L;
  global_State *g;
  LG *l = cast(LG *, (*f)(ud, NULL, HYDROGEN_TTHREAD, sizeof(LG)));
  if (l == NULL) return NULL;
  L = &l->l.l;
  g = &l->g;
  L->tt = HYDROGEN_VTHREAD;
  g->currentwhite = bitmask(WHITE0BIT);
  L->marked = hydrogenC_white(g);
  preinit_thread(L, g);
  g->allgarbageCollection = obj2gco(L);  /* by now, only object is the main thread */
  L->next = NULL;
  incnny(L);  /* main thread is always non yieldable */
  g->frealloc = f;
  g->ud = ud;
  g->warnf = NULL;
  g->ud_warn = NULL;
  g->mainthread = L;
  g->seed = hydrogeni_makeseed(L);
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
  setgcparam(g->gcpause, HYDROGENI_GCPAUSE);
  setgcparam(g->gcstepmul, HYDROGENI_GCMUL);
  g->gcstepsize = HYDROGENI_GCSTEPSIZE;
  setgcparam(g->genmajormul, HYDROGENI_GENMAJORMUL);
  g->genminormul = HYDROGENI_GENMINORMUL;
  for (i=0; i < HYDROGEN_NUMTAGS; i++) g->mt[i] = NULL;
  if (hydrogenD_rawrunprotected(L, f_hydrogenopen, NULL) != HYDROGEN_OK) {
    /* memory allocation error: free partial state */
    close_state(L);
    L = NULL;
  }
  return L;
}


HYDROGEN_API void hydrogen_close (hydrogen_State *L) {
  hydrogen_lock(L);
  L = G(L)->mainthread;  /* only the main thread can be closed */
  close_state(L);
}


void hydrogenE_warning (hydrogen_State *L, const char *msg, int tocont) {
  hydrogen_WarnFunction wf = G(L)->warnf;
  if (wf != NULL)
    wf(G(L)->ud_warn, msg, tocont);
}


/*
** Generate a warning from an error message
*/
void hydrogenE_warnerror (hydrogen_State *L, const char *where) {
  TValue *errobj = s2v(L->top - 1);  /* error object */
  const char *msg = (ttisstring(errobj))
                  ? svalue(errobj)
                  : "error object is not a string";
  /* produce warning "error in %s (%s)" (where, msg) */
  hydrogenE_warning(L, "error in ", 1);
  hydrogenE_warning(L, where, 1);
  hydrogenE_warning(L, " (", 1);
  hydrogenE_warning(L, msg, 1);
  hydrogenE_warning(L, ")", 0);
}

