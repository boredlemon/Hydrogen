/*
** $Id: ldo.h $
** Stack and Call structure of Viper
** See Copyright Notice in viper.h
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
#define viperD_checkstackaux(L,n,pre,pos)  \
	if (l_unlikely(L->stack_last - L->top <= (n))) \
	  { pre; viperD_growstack(L, n, 1); pos; } \
        else { condmovestack(L,pre,pos); }

/* In general, 'pre'/'pos' are empty (nothing to save) */
#define viperD_checkstack(L,n)	viperD_checkstackaux(L,n,(void)0,(void)0)



#define savestack(L,p)		((char *)(p) - (char *)L->stack)
#define restorestack(L,n)	((StkId)((char *)L->stack + (n)))


/* macro to check stack size, preserving 'p' */
#define checkstackGCp(L,n,p)  \
  viperD_checkstackaux(L, n, \
    ptrdiff_t t__ = savestack(L, p);  /* save 'p' */ \
    viperC_checkGC(L),  /* stack grow uses memory */ \
    p = restorestack(L, t__))  /* 'pos' part: restore 'p' */


/* macro to check stack size and GC */
#define checkstackGC(L,fsize)  \
	viperD_checkstackaux(L, (fsize), viperC_checkGC(L), (void)0)


/* type of protected functions, to be ran by 'runprotected' */
typedef void (*Pfunc) (viper_State *L, void *ud);

VIPERI_FUNC void viperD_seterrorobj (viper_State *L, int errcode, StkId oldtop);
VIPERI_FUNC int viperD_protectedparser (viper_State *L, ZIO *z, const char *name,
                                                  const char *mode);
VIPERI_FUNC void viperD_hook (viper_State *L, int event, int line,
                                        int fTransfer, int nTransfer);
VIPERI_FUNC void viperD_hookcall (viper_State *L, CallInfo *ci);
VIPERI_FUNC int viperD_pretailcall (viper_State *L, CallInfo *ci, StkId func,                                                    int narg1, int delta);
VIPERI_FUNC CallInfo *viperD_precall (viper_State *L, StkId func, int nResults);
VIPERI_FUNC void viperD_call (viper_State *L, StkId func, int nResults);
VIPERI_FUNC void viperD_callnoyield (viper_State *L, StkId func, int nResults);
VIPERI_FUNC StkId viperD_tryfuncTM (viper_State *L, StkId func);
VIPERI_FUNC int viperD_closeprotected (viper_State *L, ptrdiff_t level, int status);
VIPERI_FUNC int viperD_pcall (viper_State *L, Pfunc func, void *u,
                                        ptrdiff_t oldtop, ptrdiff_t ef);
VIPERI_FUNC void viperD_poscall (viper_State *L, CallInfo *ci, int nres);
VIPERI_FUNC int viperD_reallocstack (viper_State *L, int newsize, int raiseerror);
VIPERI_FUNC int viperD_growstack (viper_State *L, int n, int raiseerror);
VIPERI_FUNC void viperD_shrinkstack (viper_State *L);
VIPERI_FUNC void viperD_inctop (viper_State *L);

VIPERI_FUNC l_noret viperD_throw (viper_State *L, int errcode);
VIPERI_FUNC int viperD_rawrunprotected (viper_State *L, Pfunc f, void *ud);

#endif

