/*
** $Id: string.h $
** String table (keep all strings handled by Hydrogen)
** See Copyright Notice in hydrogen.h
*/

#ifndef string_h
#define string_h

#include "garbageCollection.h"
#include "object.h"
#include "state.h"


/*
** Memory-allocation error message must be preallocated (it cannot
** be created after memory is exhausted)
*/
#define MEMERRMSG       "not enough memory"


/*
** Size of a TString: Size of the header plus space for the string
** itself (including final '\0').
*/
#define sizestring(l)  (offsetof(TString, contents) + ((l) + 1) * sizeof(char))

#define hydrogenS_newliteral(L, s)	(hydrogenS_newlstr(L, "" s, \
                                 (sizeof(s)/sizeof(char))-1))


/*
** test whether a string is a reserved word
*/
#define isreserved(s)	((s)->tt == HYDROGEN_VSHRSTR && (s)->extra > 0)


/*
** equality for short strings, which are always internalized
*/
#define eqshrstr(a,b)	check_exp((a)->tt == HYDROGEN_VSHRSTR, (a) == (b))


HYDROGENI_FUNC unsigned int hydrogenS_hash (const char *str, size_t l, unsigned int seed);
HYDROGENI_FUNC unsigned int hydrogenS_hashlongstr (TString *ts);
HYDROGENI_FUNC int hydrogenS_eqlngstr (TString *a, TString *b);
HYDROGENI_FUNC void hydrogenS_resize (hydrogen_State *L, int newsize);
HYDROGENI_FUNC void hydrogenS_clearcache (global_State *g);
HYDROGENI_FUNC void hydrogenS_init (hydrogen_State *L);
HYDROGENI_FUNC void hydrogenS_remove (hydrogen_State *L, TString *ts);
HYDROGENI_FUNC Udata *hydrogenS_newudata (hydrogen_State *L, size_t s, int nuvalue);
HYDROGENI_FUNC TString *hydrogenS_newlstr (hydrogen_State *L, const char *str, size_t l);
HYDROGENI_FUNC TString *hydrogenS_new (hydrogen_State *L, const char *str);
HYDROGENI_FUNC TString *hydrogenS_createlngstrobj (hydrogen_State *L, size_t l);


#endif
