/*
** $Id: ldo.h $
** Stack and Call structure of Acorn
** See Copyright Notice in acorn.h
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
#define acornD_checkstackaux(L,n,pre,pos)  \
	if (l_unlikely(L->stack_last - L->top <= (n))) \
	  { pre; acornD_growstack(L, n, 1); pos; } \
        else { condmovestack(L,pre,pos); }

/* In general, 'pre'/'pos' are empty (nothing to save) */
#define acornD_checkstack(L,n)	acornD_checkstackaux(L,n,(void)0,(void)0)



#define savestack(L,p)		((char *)(p) - (char *)L->stack)
#define restorestack(L,n)	((StkId)((char *)L->stack + (n)))


/* macro to check stack size, preserving 'p' */
#define checkstackGCp(L,n,p)  \
  acornD_checkstackaux(L, n, \
    ptrdiff_t t__ = savestack(L, p);  /* save 'p' */ \
    acornC_checkGC(L),  /* stack grow uses memory */ \
    p = restorestack(L, t__))  /* 'pos' part: restore 'p' */


/* macro to check stack size and GC */
#define checkstackGC(L,fsize)  \
	acornD_checkstackaux(L, (fsize), acornC_checkGC(L), (void)0)


/* type of protected functions, to be ran by 'runprotected' */
typedef void (*Pfunc) (acorn_State *L, void *ud);

ACORNI_FUNC void acornD_seterrorobj (acorn_State *L, int errcode, StkId oldtop);
ACORNI_FUNC int acornD_protectedparser (acorn_State *L, ZIO *z, const char *name,
                                                  const char *mode);
ACORNI_FUNC void acornD_hook (acorn_State *L, int event, int line,
                                        int fTransfer, int nTransfer);
ACORNI_FUNC void acornD_hookcall (acorn_State *L, CallInfo *ci);
ACORNI_FUNC int acornD_pretailcall (acorn_State *L, CallInfo *ci, StkId func,                                                    int narg1, int delta);
ACORNI_FUNC CallInfo *acornD_precall (acorn_State *L, StkId func, int nResults);
ACORNI_FUNC void acornD_call (acorn_State *L, StkId func, int nResults);
ACORNI_FUNC void acornD_callnoyield (acorn_State *L, StkId func, int nResults);
ACORNI_FUNC StkId acornD_tryfuncTM (acorn_State *L, StkId func);
ACORNI_FUNC int acornD_closeprotected (acorn_State *L, ptrdiff_t level, int status);
ACORNI_FUNC int acornD_pcall (acorn_State *L, Pfunc func, void *u,
                                        ptrdiff_t oldtop, ptrdiff_t ef);
ACORNI_FUNC void acornD_poscall (acorn_State *L, CallInfo *ci, int nres);
ACORNI_FUNC int acornD_reallocstack (acorn_State *L, int newsize, int raiseerror);
ACORNI_FUNC int acornD_growstack (acorn_State *L, int n, int raiseerror);
ACORNI_FUNC void acornD_shrinkstack (acorn_State *L);
ACORNI_FUNC void acornD_inctop (acorn_State *L);

ACORNI_FUNC l_noret acornD_throw (acorn_State *L, int errcode);
ACORNI_FUNC int acornD_rawrunprotected (acorn_State *L, Pfunc f, void *ud);

#endif

