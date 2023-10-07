/*
** $Id: lstring.h $
** String table (keep all strings handled by Cup)
** See Copyright Notice in cup.h
*/

#ifndef lstring_h
#define lstring_h

#include "lgc.h"
#include "lobject.h"
#include "lstate.h"


/*
** Memory-allocation error message must be preallocated (it cannot
** be created after memory is exhausted)
*/
#define MEMERRMSG       "not enough memory"


/*
** Size of a TString: Size of the header plus space for the string
** itself (including final '\0').
*/
#define sizelstring(l)  (offsetof(TString, contents) + ((l) + 1) * sizeof(char))

#define cupS_newliteral(L, s)	(cupS_newlstr(L, "" s, \
                                 (sizeof(s)/sizeof(char))-1))


/*
** test whether a string is a reserved word
*/
#define isreserved(s)	((s)->tt == CUP_VSHRSTR && (s)->extra > 0)


/*
** equality for short strings, which are always internalized
*/
#define eqshrstr(a,b)	check_exp((a)->tt == CUP_VSHRSTR, (a) == (b))


CUPI_FUNC unsigned int cupS_hash (const char *str, size_t l, unsigned int seed);
CUPI_FUNC unsigned int cupS_hashlongstr (TString *ts);
CUPI_FUNC int cupS_eqlngstr (TString *a, TString *b);
CUPI_FUNC void cupS_resize (cup_State *L, int newsize);
CUPI_FUNC void cupS_clearcache (global_State *g);
CUPI_FUNC void cupS_init (cup_State *L);
CUPI_FUNC void cupS_remove (cup_State *L, TString *ts);
CUPI_FUNC Udata *cupS_newudata (cup_State *L, size_t s, int nuvalue);
CUPI_FUNC TString *cupS_newlstr (cup_State *L, const char *str, size_t l);
CUPI_FUNC TString *cupS_new (cup_State *L, const char *str);
CUPI_FUNC TString *cupS_createlngstrobj (cup_State *L, size_t l);


#endif
