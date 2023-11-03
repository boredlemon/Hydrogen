/*
** $Id: debug.h $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in nebula.h
*/

#ifndef debug_h
#define debug_h


#include "state.h"


#define pcRel(pc, p)	(cast_int((pc) - (p)->code) - 1)


/* Active Nebula function (given call info) */
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


NEBULAI_FUNC int nebulaG_getfuncline (const Proto *f, int pc);
NEBULAI_FUNC const char *nebulaG_findlocal (nebula_State *L, CallInfo *ci, int n,
                                                    StkId *pos);
NEBULAI_FUNC l_noret nebulaG_typeerror (nebula_State *L, const TValue *o,
                                                const char *opname);
NEBULAI_FUNC l_noret nebulaG_callerror (nebula_State *L, const TValue *o);
NEBULAI_FUNC l_noret nebulaG_forerror (nebula_State *L, const TValue *o,
                                               const char *what);
NEBULAI_FUNC l_noret nebulaG_concaterror (nebula_State *L, const TValue *p1,
                                                  const TValue *p2);
NEBULAI_FUNC l_noret nebulaG_opinterror (nebula_State *L, const TValue *p1,
                                                 const TValue *p2,
                                                 const char *msg);
NEBULAI_FUNC l_noret nebulaG_tointerror (nebula_State *L, const TValue *p1,
                                                 const TValue *p2);
NEBULAI_FUNC l_noret nebulaG_ordererror (nebula_State *L, const TValue *p1,
                                                 const TValue *p2);
NEBULAI_FUNC l_noret nebulaG_runerror (nebula_State *L, const char *fmt, ...);
NEBULAI_FUNC const char *nebulaG_addinfo (nebula_State *L, const char *msg,
                                                  TString *src, int line);
NEBULAI_FUNC l_noret nebulaG_errormsg (nebula_State *L);
NEBULAI_FUNC int nebulaG_traceexec (nebula_State *L, const Instruction *pc);


#endif
