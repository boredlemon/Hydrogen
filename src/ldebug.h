/*
** $Id: ldebug.h $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in acorn.h
*/

#ifndef ldebug_h
#define ldebug_h


#include "lstate.h"


#define pcRel(pc, p)	(cast_int((pc) - (p)->code) - 1)


/* Active Acorn function (given call info) */
#define ci_func(ci)		(clLvalue(s2v((ci)->func)))


#define resethookcount(L)	(L->hookcount = L->basehookcount)

/*
** mark for entries in 'lineinfo' array that has absolute information in
** 'abslineinfo' array
*/
#define ABSLINEINFO	(-0x80)


/*
** MAXimum number of successive Instructions WiTHout ABSolute line
** information. (A power of two allows fast divisions.)
*/
#if !defined(MAXIWTHABS)
#define MAXIWTHABS	128
#endif


ACORNI_FUNC int acornG_getfuncline (const Proto *f, int pc);
ACORNI_FUNC const char *acornG_findlocal (acorn_State *L, CallInfo *ci, int n,
                                                    StkId *pos);
ACORNI_FUNC l_noret acornG_typeerror (acorn_State *L, const TValue *o,
                                                const char *opname);
ACORNI_FUNC l_noret acornG_callerror (acorn_State *L, const TValue *o);
ACORNI_FUNC l_noret acornG_forerror (acorn_State *L, const TValue *o,
                                               const char *what);
ACORNI_FUNC l_noret acornG_concaterror (acorn_State *L, const TValue *p1,
                                                  const TValue *p2);
ACORNI_FUNC l_noret acornG_opinterror (acorn_State *L, const TValue *p1,
                                                 const TValue *p2,
                                                 const char *msg);
ACORNI_FUNC l_noret acornG_tointerror (acorn_State *L, const TValue *p1,
                                                 const TValue *p2);
ACORNI_FUNC l_noret acornG_ordererror (acorn_State *L, const TValue *p1,
                                                 const TValue *p2);
ACORNI_FUNC l_noret acornG_runerror (acorn_State *L, const char *fmt, ...);
ACORNI_FUNC const char *acornG_addinfo (acorn_State *L, const char *msg,
                                                  TString *src, int line);
ACORNI_FUNC l_noret acornG_errormsg (acorn_State *L);
ACORNI_FUNC int acornG_traceexec (acorn_State *L, const Instruction *pc);


#endif
