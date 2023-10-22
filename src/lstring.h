/*
** $Id: lstring.h $
** String table (keep all strings handled by Viper)
** See Copyright Notice in viper.h
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

#define viperS_newliteral(L, s)	(viperS_newlstr(L, "" s, \
                                 (sizeof(s)/sizeof(char))-1))


/*
** test whether a string is a reserved word
*/
#define isreserved(s)	((s)->tt == VIPER_VSHRSTR && (s)->extra > 0)


/*
** equality for short strings, which are always internalized
*/
#define eqshrstr(a,b)	check_exp((a)->tt == VIPER_VSHRSTR, (a) == (b))


VIPERI_FUNC unsigned int viperS_hash (const char *str, size_t l, unsigned int seed);
VIPERI_FUNC unsigned int viperS_hashlongstr (TString *ts);
VIPERI_FUNC int viperS_eqlngstr (TString *a, TString *b);
VIPERI_FUNC void viperS_resize (viper_State *L, int newsize);
VIPERI_FUNC void viperS_clearcache (global_State *g);
VIPERI_FUNC void viperS_init (viper_State *L);
VIPERI_FUNC void viperS_remove (viper_State *L, TString *ts);
VIPERI_FUNC Udata *viperS_newudata (viper_State *L, size_t s, int nuvalue);
VIPERI_FUNC TString *viperS_newlstr (viper_State *L, const char *str, size_t l);
VIPERI_FUNC TString *viperS_new (viper_State *L, const char *str);
VIPERI_FUNC TString *viperS_createlngstrobj (viper_State *L, size_t l);


#endif
