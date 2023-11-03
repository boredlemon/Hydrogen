/*
** $Id: string.h $
** String table (keep all strings handled by Nebula)
** See Copyright Notice in nebula.h
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

#define nebulaS_newliteral(L, s)	(nebulaS_newlstr(L, "" s, \
                                 (sizeof(s)/sizeof(char))-1))


/*
** test whether a string is a reserved word
*/
#define isreserved(s)	((s)->tt == NEBULA_VSHRSTR && (s)->extra > 0)


/*
** equality for short strings, which are always internalized
*/
#define eqshrstr(a,b)	check_exp((a)->tt == NEBULA_VSHRSTR, (a) == (b))


NEBULAI_FUNC unsigned int nebulaS_hash (const char *str, size_t l, unsigned int seed);
NEBULAI_FUNC unsigned int nebulaS_hashlongstr (TString *ts);
NEBULAI_FUNC int nebulaS_eqlngstr (TString *a, TString *b);
NEBULAI_FUNC void nebulaS_resize (nebula_State *L, int newsize);
NEBULAI_FUNC void nebulaS_clearcache (global_State *g);
NEBULAI_FUNC void nebulaS_init (nebula_State *L);
NEBULAI_FUNC void nebulaS_remove (nebula_State *L, TString *ts);
NEBULAI_FUNC Udata *nebulaS_newudata (nebula_State *L, size_t s, int nuvalue);
NEBULAI_FUNC TString *nebulaS_newlstr (nebula_State *L, const char *str, size_t l);
NEBULAI_FUNC TString *nebulaS_new (nebula_State *L, const char *str);
NEBULAI_FUNC TString *nebulaS_createlngstrobj (nebula_State *L, size_t l);


#endif
