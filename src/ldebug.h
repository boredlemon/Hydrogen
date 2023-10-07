/*
** $Id: ldebug.h $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in cup.h
*/

#ifndef ldebug_h
#define ldebug_h


#include "lstate.h"


#define pcRel(pc, p)	(cast_int((pc) - (p)->code) - 1)


/* Active Cup function (given call info) */
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


CUPI_FUNC int cupG_getfuncline (const Proto *f, int pc);
CUPI_FUNC const char *cupG_findlocal (cup_State *L, CallInfo *ci, int n,
                                                    StkId *pos);
CUPI_FUNC l_noret cupG_typeerror (cup_State *L, const TValue *o,
                                                const char *opname);
CUPI_FUNC l_noret cupG_callerror (cup_State *L, const TValue *o);
CUPI_FUNC l_noret cupG_forerror (cup_State *L, const TValue *o,
                                               const char *what);
CUPI_FUNC l_noret cupG_concaterror (cup_State *L, const TValue *p1,
                                                  const TValue *p2);
CUPI_FUNC l_noret cupG_opinterror (cup_State *L, const TValue *p1,
                                                 const TValue *p2,
                                                 const char *msg);
CUPI_FUNC l_noret cupG_tointerror (cup_State *L, const TValue *p1,
                                                 const TValue *p2);
CUPI_FUNC l_noret cupG_ordererror (cup_State *L, const TValue *p1,
                                                 const TValue *p2);
CUPI_FUNC l_noret cupG_runerror (cup_State *L, const char *fmt, ...);
CUPI_FUNC const char *cupG_addinfo (cup_State *L, const char *msg,
                                                  TString *src, int line);
CUPI_FUNC l_noret cupG_errormsg (cup_State *L);
CUPI_FUNC int cupG_traceexec (cup_State *L, const Instruction *pc);


#endif
