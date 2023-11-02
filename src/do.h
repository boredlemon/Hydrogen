/*
** $Id: do.h $
** Stack and Call structure of Venom
** See Copyright Notice in venom.h
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
#define venomD_checkstackaux(L,n,pre,pos)  \
	if (l_unlikely(L->stack_last - L->top <= (n))) \
	  { pre; venomD_growstack(L, n, 1); pos; } \
        else { condmovestack(L,pre,pos); }

/* In general, 'pre'/'pos' are empty (nothing to save) */
#define venomD_checkstack(L,n)	venomD_checkstackaux(L,n,(void)0,(void)0)



#define savestack(L,p)		((char *)(p) - (char *)L->stack)
#define restorestack(L,n)	((StkId)((char *)L->stack + (n)))


/* macro to check stack size, preserving 'p' */
#define checkstackGCp(L,n,p)  \
  venomD_checkstackaux(L, n, \
    ptrdiff_t t__ = savestack(L, p);  /* save 'p' */ \
    venomC_checkGC(L),  /* stack grow uses memory */ \
    p = restorestack(L, t__))  /* 'pos' part: restore 'p' */


/* macro to check stack size and GC */
#define checkstackGC(L,fsize)  \
	venomD_checkstackaux(L, (fsize), venomC_checkGC(L), (void)0)


/* type of protected functions, to be ran by 'runprotected' */
typedef void (*Pfunc) (venom_State *L, void *ud);

VENOMI_FUNC void venomD_seterrorobj (venom_State *L, int errcode, StkId oldtop);
VENOMI_FUNC int venomD_protectedparser (venom_State *L, ZIO *z, const char *name,
                                                  const char *mode);
VENOMI_FUNC void venomD_hook (venom_State *L, int event, int line,
                                        int fTransfer, int nTransfer);
VENOMI_FUNC void venomD_hookcall (venom_State *L, CallInfo *ci);
VENOMI_FUNC int venomD_pretailcall (venom_State *L, CallInfo *ci, StkId func,                                                    int narg1, int delta);
VENOMI_FUNC CallInfo *venomD_precall (venom_State *L, StkId func, int nResults);
VENOMI_FUNC void venomD_call (venom_State *L, StkId func, int nResults);
VENOMI_FUNC void venomD_callnoyield (venom_State *L, StkId func, int nResults);
VENOMI_FUNC StkId venomD_tryfuncTM (venom_State *L, StkId func);
VENOMI_FUNC int venomD_closeprotected (venom_State *L, ptrdiff_t level, int status);
VENOMI_FUNC int venomD_pcall (venom_State *L, Pfunc func, void *u,
                                        ptrdiff_t oldtop, ptrdiff_t ef);
VENOMI_FUNC void venomD_poscall (venom_State *L, CallInfo *ci, int nres);
VENOMI_FUNC int venomD_reallocstack (venom_State *L, int newsize, int raiseerror);
VENOMI_FUNC int venomD_growstack (venom_State *L, int n, int raiseerror);
VENOMI_FUNC void venomD_shrinkstack (venom_State *L);
VENOMI_FUNC void venomD_inctop (venom_State *L);

VENOMI_FUNC l_noret venomD_throw (venom_State *L, int errcode);
VENOMI_FUNC int venomD_rawrunprotected (venom_State *L, Pfunc f, void *ud);

#endif

