/*
** $Id: ldo.h $
** Stack and Call structure of Cup
** See Copyright Notice in cup.h
*/

#ifndef ldo_h
#define ldo_h


#include "lobject.h"
#include "lstate.h"
#include "lzio.h"


/*
** Macro to check stack size and grow stack if needed.  Parameters
** 'pre'/'pos' allow the macro to preserve a pointer into the
** stack across reallocations, doing the work only when needed.
** It also allows the running of one GC step when the stack is
** reallocated.
** 'condmovestack' is used in heavy tests to force a stack reallocation
** at every check.
*/
#define cupD_checkstackaux(L,n,pre,pos)  \
	if (l_unlikely(L->stack_last - L->top <= (n))) \
	  { pre; cupD_growstack(L, n, 1); pos; } \
        else { condmovestack(L,pre,pos); }

/* In general, 'pre'/'pos' are empty (nothing to save) */
#define cupD_checkstack(L,n)	cupD_checkstackaux(L,n,(void)0,(void)0)



#define savestack(L,p)		((char *)(p) - (char *)L->stack)
#define restorestack(L,n)	((StkId)((char *)L->stack + (n)))


/* macro to check stack size, preserving 'p' */
#define checkstackGCp(L,n,p)  \
  cupD_checkstackaux(L, n, \
    ptrdiff_t t__ = savestack(L, p);  /* save 'p' */ \
    cupC_checkGC(L),  /* stack grow uses memory */ \
    p = restorestack(L, t__))  /* 'pos' part: restore 'p' */


/* macro to check stack size and GC */
#define checkstackGC(L,fsize)  \
	cupD_checkstackaux(L, (fsize), cupC_checkGC(L), (void)0)


/* type of protected functions, to be ran by 'runprotected' */
typedef void (*Pfunc) (cup_State *L, void *ud);

CUPI_FUNC void cupD_seterrorobj (cup_State *L, int errcode, StkId oldtop);
CUPI_FUNC int cupD_protectedparser (cup_State *L, ZIO *z, const char *name,
                                                  const char *mode);
CUPI_FUNC void cupD_hook (cup_State *L, int event, int line,
                                        int fTransfer, int nTransfer);
CUPI_FUNC void cupD_hookcall (cup_State *L, CallInfo *ci);
CUPI_FUNC int cupD_pretailcall (cup_State *L, CallInfo *ci, StkId func,                                                    int narg1, int delta);
CUPI_FUNC CallInfo *cupD_precall (cup_State *L, StkId func, int nResults);
CUPI_FUNC void cupD_call (cup_State *L, StkId func, int nResults);
CUPI_FUNC void cupD_callnoyield (cup_State *L, StkId func, int nResults);
CUPI_FUNC StkId cupD_tryfuncTM (cup_State *L, StkId func);
CUPI_FUNC int cupD_closeprotected (cup_State *L, ptrdiff_t level, int status);
CUPI_FUNC int cupD_pcall (cup_State *L, Pfunc func, void *u,
                                        ptrdiff_t oldtop, ptrdiff_t ef);
CUPI_FUNC void cupD_poscall (cup_State *L, CallInfo *ci, int nres);
CUPI_FUNC int cupD_reallocstack (cup_State *L, int newsize, int raiseerror);
CUPI_FUNC int cupD_growstack (cup_State *L, int n, int raiseerror);
CUPI_FUNC void cupD_shrinkstack (cup_State *L);
CUPI_FUNC void cupD_inctop (cup_State *L);

CUPI_FUNC l_noret cupD_throw (cup_State *L, int errcode);
CUPI_FUNC int cupD_rawrunprotected (cup_State *L, Pfunc f, void *ud);

#endif

