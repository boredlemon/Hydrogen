/*
** $Id: lstring.h $
** String table (keep all strings handled by Acorn)
** See Copyright Notice in acorn.h
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

#define acornS_newliteral(L, s)	(acornS_newlstr(L, "" s, \
                                 (sizeof(s)/sizeof(char))-1))


/*
** test whether a string is a reserved word
*/
#define isreserved(s)	((s)->tt == ACORN_VSHRSTR && (s)->extra > 0)


/*
** equality for short strings, which are always internalized
*/
#define eqshrstr(a,b)	check_exp((a)->tt == ACORN_VSHRSTR, (a) == (b))


ACORNI_FUNC unsigned int acornS_hash (const char *str, size_t l, unsigned int seed);
ACORNI_FUNC unsigned int acornS_hashlongstr (TString *ts);
ACORNI_FUNC int acornS_eqlngstr (TString *a, TString *b);
ACORNI_FUNC void acornS_resize (acorn_State *L, int newsize);
ACORNI_FUNC void acornS_clearcache (global_State *g);
ACORNI_FUNC void acornS_init (acorn_State *L);
ACORNI_FUNC void acornS_remove (acorn_State *L, TString *ts);
ACORNI_FUNC Udata *acornS_newudata (acorn_State *L, size_t s, int nuvalue);
ACORNI_FUNC TString *acornS_newlstr (acorn_State *L, const char *str, size_t l);
ACORNI_FUNC TString *acornS_new (acorn_State *L, const char *str);
ACORNI_FUNC TString *acornS_createlngstrobj (acorn_State *L, size_t l);


#endif
