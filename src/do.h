/*
** $Id: do.h $
** Stack and Call structure of Nebula
** See Copyright Notice in nebula.h
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
#define nebulaD_checkstackaux(L,n,pre,pos)  \
	if (l_unlikely(L->stack_last - L->top <= (n))) \
	  { pre; nebulaD_growstack(L, n, 1); pos; } \
        else { condmovestack(L,pre,pos); }

/* In general, 'pre'/'pos' are empty (nothing to save) */
#define nebulaD_checkstack(L,n)	nebulaD_checkstackaux(L,n,(void)0,(void)0)



#define savestack(L,p)		((char *)(p) - (char *)L->stack)
#define restorestack(L,n)	((StkId)((char *)L->stack + (n)))


/* macro to check stack size, preserving 'p' */
#define checkstackGCp(L,n,p)  \
  nebulaD_checkstackaux(L, n, \
    ptrdiff_t t__ = savestack(L, p);  /* save 'p' */ \
    nebulaC_checkGC(L),  /* stack grow uses memory */ \
    p = restorestack(L, t__))  /* 'pos' part: restore 'p' */


/* macro to check stack size and GC */
#define checkstackGC(L,fsize)  \
	nebulaD_checkstackaux(L, (fsize), nebulaC_checkGC(L), (void)0)


/* type of protected functions, to be ran by 'runprotected' */
typedef void (*Pfunc) (nebula_State *L, void *ud);

NEBULAI_FUNC void nebulaD_seterrorobj (nebula_State *L, int errcode, StkId oldtop);
NEBULAI_FUNC int nebulaD_protectedparser (nebula_State *L, ZIO *z, const char *name,
                                                  const char *mode);
NEBULAI_FUNC void nebulaD_hook (nebula_State *L, int event, int line,
                                        int fTransfer, int nTransfer);
NEBULAI_FUNC void nebulaD_hookcall (nebula_State *L, CallInfo *ci);
NEBULAI_FUNC int nebulaD_pretailcall (nebula_State *L, CallInfo *ci, StkId func,                                                    int narg1, int delta);
NEBULAI_FUNC CallInfo *nebulaD_precall (nebula_State *L, StkId func, int nResults);
NEBULAI_FUNC void nebulaD_call (nebula_State *L, StkId func, int nResults);
NEBULAI_FUNC void nebulaD_callnoyield (nebula_State *L, StkId func, int nResults);
NEBULAI_FUNC StkId nebulaD_tryfuncTM (nebula_State *L, StkId func);
NEBULAI_FUNC int nebulaD_closeprotected (nebula_State *L, ptrdiff_t level, int status);
NEBULAI_FUNC int nebulaD_pcall (nebula_State *L, Pfunc func, void *u,
                                        ptrdiff_t oldtop, ptrdiff_t ef);
NEBULAI_FUNC void nebulaD_poscall (nebula_State *L, CallInfo *ci, int nres);
NEBULAI_FUNC int nebulaD_reallocstack (nebula_State *L, int newsize, int raiseerror);
NEBULAI_FUNC int nebulaD_growstack (nebula_State *L, int n, int raiseerror);
NEBULAI_FUNC void nebulaD_shrinkstack (nebula_State *L);
NEBULAI_FUNC void nebulaD_inctop (nebula_State *L);

NEBULAI_FUNC l_noret nebulaD_throw (nebula_State *L, int errcode);
NEBULAI_FUNC int nebulaD_rawrunprotected (nebula_State *L, Pfunc f, void *ud);

#endif

