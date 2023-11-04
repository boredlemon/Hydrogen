/*
** $Id: do.h $
** Stack and Call structure of Hydrogen
** See Copyright Notice in hydrogen.h
*/

#ifndef do_h
#define do_h


#include "object.h"
#include "state.h"
#include "zio.h"


/*
** Macro to check stack size and grow stack if needed.  Parameters
** 'pre'/'pos' allow the macro to preserve a pointer into the
** stack across reallocations, doing the work only when needed.
** It also allows the running of one GC step when the stack is
** reallocated.
** 'condmovestack' is used in heavy tests to force a stack reallocation
** at every check.
*/
#define hydrogenD_checkstackaux(L,n,pre,pos)  \
	if (l_unlikely(L->stack_last - L->top <= (n))) \
	  { pre; hydrogenD_growstack(L, n, 1); pos; } \
        else { condmovestack(L,pre,pos); }

/* In general, 'pre'/'pos' are empty (nothing to save) */
#define hydrogenD_checkstack(L,n)	hydrogenD_checkstackaux(L,n,(void)0,(void)0)



#define savestack(L,p)		((char *)(p) - (char *)L->stack)
#define restorestack(L,n)	((StkId)((char *)L->stack + (n)))


/* macro to check stack size, preserving 'p' */
#define checkstackGCp(L,n,p)  \
  hydrogenD_checkstackaux(L, n, \
    ptrdiff_t t__ = savestack(L, p);  /* save 'p' */ \
    hydrogenC_checkGC(L),  /* stack grow uses memory */ \
    p = restorestack(L, t__))  /* 'pos' part: restore 'p' */


/* macro to check stack size and GC */
#define checkstackGC(L,fsize)  \
	hydrogenD_checkstackaux(L, (fsize), hydrogenC_checkGC(L), (void)0)


/* type of protected functions, to be ran by 'runprotected' */
typedef void (*Pfunc) (hydrogen_State *L, void *ud);

HYDROGENI_FUNC void hydrogenD_seterrorobj (hydrogen_State *L, int errcode, StkId oldtop);
HYDROGENI_FUNC int hydrogenD_protectedparser (hydrogen_State *L, ZIO *z, const char *name,
                                                  const char *mode);
HYDROGENI_FUNC void hydrogenD_hook (hydrogen_State *L, int event, int line,
                                        int fTransfer, int nTransfer);
HYDROGENI_FUNC void hydrogenD_hookcall (hydrogen_State *L, CallInfo *ci);
HYDROGENI_FUNC int hydrogenD_pretailcall (hydrogen_State *L, CallInfo *ci, StkId func,                                                    int narg1, int delta);
HYDROGENI_FUNC CallInfo *hydrogenD_precall (hydrogen_State *L, StkId func, int nResults);
HYDROGENI_FUNC void hydrogenD_call (hydrogen_State *L, StkId func, int nResults);
HYDROGENI_FUNC void hydrogenD_callnoyield (hydrogen_State *L, StkId func, int nResults);
HYDROGENI_FUNC StkId hydrogenD_tryfuncTM (hydrogen_State *L, StkId func);
HYDROGENI_FUNC int hydrogenD_closeprotected (hydrogen_State *L, ptrdiff_t level, int status);
HYDROGENI_FUNC int hydrogenD_pcall (hydrogen_State *L, Pfunc func, void *u,
                                        ptrdiff_t oldtop, ptrdiff_t ef);
HYDROGENI_FUNC void hydrogenD_poscall (hydrogen_State *L, CallInfo *ci, int nres);
HYDROGENI_FUNC int hydrogenD_reallocstack (hydrogen_State *L, int newsize, int raiseerror);
HYDROGENI_FUNC int hydrogenD_growstack (hydrogen_State *L, int n, int raiseerror);
HYDROGENI_FUNC void hydrogenD_shrinkstack (hydrogen_State *L);
HYDROGENI_FUNC void hydrogenD_inctop (hydrogen_State *L);

HYDROGENI_FUNC l_noret hydrogenD_throw (hydrogen_State *L, int errcode);
HYDROGENI_FUNC int hydrogenD_rawrunprotected (hydrogen_State *L, Pfunc f, void *ud);

#endif

