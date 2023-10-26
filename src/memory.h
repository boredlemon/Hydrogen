/*
** $Id: memory.h $
** Interface to Memory Manager
** See Copyright Notice in viper.h
*/

#ifndef memory_h
#define memory_h


#include <stddef.h>

#include "limits.h"
#include "viper.h"


#define viperM_error(L)	viperD_throw(L, VIPER_ERRMEM)


/*
** This macro tests whether it is safe to multiply 'n' by the size of
** type 't' without overflows. Because 'e' is always constant, it avoids
** the runtime division MAX_SIZET/(e).
** (The macro is somewhat complex to avoid warnings:  The 'sizeof'
** comparison avoids a runtime comparison when overflow cannot occur.
** The compiler should be able to optimize the real test by itself, but
** when it does it, it may give a warning about "comparison is always
** false due to limited range of data type"; the +1 tricks the compiler,
** avoiding this warning but also this optimization.)
*/
#define viperM_testsize(n,e)  \
	(sizeof(n) >= sizeof(size_t) && cast_sizet((n)) + 1 > MAX_SIZET/(e))

#define viperM_checksize(L,n,e)  \
	(viperM_testsize(n,e) ? viperM_toobig(L) : cast_void(0))


/*
** Computes the minimum between 'n' and 'MAX_SIZET/sizeof(t)', so that
** the result is not larger than 'n' and cannot overflow a 'size_t'
** when multiplied by the size of type 't'. (Assumes that 'n' is an
** 'int' or 'unsigned int' and that 'int' is not larger than 'size_t'.)
*/
#define viperM_limitN(n,t)  \
  ((cast_sizet(n) <= MAX_SIZET/sizeof(t)) ? (n) :  \
     cast_uint((MAX_SIZET/sizeof(t))))


/*
** Arrays of chars do not need any test
*/
#define viperM_reallocvchar(L,b,on,n)  \
  cast_charp(viperM_saferealloc_(L, (b), (on)*sizeof(char), (n)*sizeof(char)))

#define viperM_freemem(L, b, s)	viperM_free_(L, (b), (s))
#define viperM_free(L, b)		viperM_free_(L, (b), sizeof(*(b)))
#define viperM_freearray(L, b, n)   viperM_free_(L, (b), (n)*sizeof(*(b)))

#define viperM_new(L,t)		cast(t*, viperM_malloc_(L, sizeof(t), 0))
#define viperM_newvector(L,n,t)	cast(t*, viperM_malloc_(L, (n)*sizeof(t), 0))
#define viperM_newvectorchecked(L,n,t) \
  (viperM_checksize(L,n,sizeof(t)), viperM_newvector(L,n,t))

#define viperM_newobject(L,tag,s)	viperM_malloc_(L, (s), tag)

#define viperM_growvector(L,v,nelems,size,t,limit,e) \
	((v)=cast(t *, viperM_growaux_(L,v,nelems,&(size),sizeof(t), \
                         viperM_limitN(limit,t),e)))

#define viperM_reallocvector(L, v,oldn,n,t) \
   (cast(t *, viperM_realloc_(L, v, cast_sizet(oldn) * sizeof(t), \
                                  cast_sizet(n) * sizeof(t))))

#define viperM_shrinkvector(L,v,size,fs,t) \
   ((v)=cast(t *, viperM_shrinkvector_(L, v, &(size), fs, sizeof(t))))

VIPERI_FUNC l_noret viperM_toobig (viper_State *L);

/* not to be called directly */
VIPERI_FUNC void *viperM_realloc_ (viper_State *L, void *block, size_t oldsize,
                                                          size_t size);
VIPERI_FUNC void *viperM_saferealloc_ (viper_State *L, void *block, size_t oldsize,
                                                              size_t size);
VIPERI_FUNC void viperM_free_ (viper_State *L, void *block, size_t osize);
VIPERI_FUNC void *viperM_growaux_ (viper_State *L, void *block, int nelems,
                               int *size, int size_elem, int limit,
                               const char *what);
VIPERI_FUNC void *viperM_shrinkvector_ (viper_State *L, void *block, int *nelem,
                                    int final_n, int size_elem);
VIPERI_FUNC void *viperM_malloc_ (viper_State *L, size_t size, int tag);

#endif

