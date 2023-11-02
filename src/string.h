/*
** $Id: string.h $
** String table (keep all strings handled by Venom)
** See Copyright Notice in venom.h
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

#define venomS_newliteral(L, s)	(venomS_newlstr(L, "" s, \
                                 (sizeof(s)/sizeof(char))-1))


/*
** test whether a string is a reserved word
*/
#define isreserved(s)	((s)->tt == VENOM_VSHRSTR && (s)->extra > 0)


/*
** equality for short strings, which are always internalized
*/
#define eqshrstr(a,b)	check_exp((a)->tt == VENOM_VSHRSTR, (a) == (b))


VENOMI_FUNC unsigned int venomS_hash (const char *str, size_t l, unsigned int seed);
VENOMI_FUNC unsigned int venomS_hashlongstr (TString *ts);
VENOMI_FUNC int venomS_eqlngstr (TString *a, TString *b);
VENOMI_FUNC void venomS_resize (venom_State *L, int newsize);
VENOMI_FUNC void venomS_clearcache (global_State *g);
VENOMI_FUNC void venomS_init (venom_State *L);
VENOMI_FUNC void venomS_remove (venom_State *L, TString *ts);
VENOMI_FUNC Udata *venomS_newudata (venom_State *L, size_t s, int nuvalue);
VENOMI_FUNC TString *venomS_newlstr (venom_State *L, const char *str, size_t l);
VENOMI_FUNC TString *venomS_new (venom_State *L, const char *str);
VENOMI_FUNC TString *venomS_createlngstrobj (venom_State *L, size_t l);


#endif
