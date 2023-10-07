/*
** $Id: lmem.h $
** Interface to Memory Manager
** See Copyright Notice in cup.h
*/

#ifndef lmem_h
#define lmem_h


#include <stddef.h>

#include "llimits.h"
#include "cup.h"


#define cupM_error(L)	cupD_throw(L, CUP_ERRMEM)


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
#define cupM_testsize(n,e)  \
	(sizeof(n) >= sizeof(size_t) && cast_sizet((n)) + 1 > MAX_SIZET/(e))

#define cupM_checksize(L,n,e)  \
	(cupM_testsize(n,e) ? cupM_toobig(L) : cast_void(0))


/*
** Computes the minimum between 'n' and 'MAX_SIZET/sizeof(t)', so that
** the result is not larger than 'n' and cannot overflow a 'size_t'
** when multiplied by the size of type 't'. (Assumes that 'n' is an
** 'int' or 'unsigned int' and that 'int' is not larger than 'size_t'.)
*/
#define cupM_limitN(n,t)  \
  ((cast_sizet(n) <= MAX_SIZET/sizeof(t)) ? (n) :  \
     cast_uint((MAX_SIZET/sizeof(t))))


/*
** Arrays of chars do not need any test
*/
#define cupM_reallocvchar(L,b,on,n)  \
  cast_charp(cupM_saferealloc_(L, (b), (on)*sizeof(char), (n)*sizeof(char)))

#define cupM_freemem(L, b, s)	cupM_free_(L, (b), (s))
#define cupM_free(L, b)		cupM_free_(L, (b), sizeof(*(b)))
#define cupM_freearray(L, b, n)   cupM_free_(L, (b), (n)*sizeof(*(b)))

#define cupM_new(L,t)		cast(t*, cupM_malloc_(L, sizeof(t), 0))
#define cupM_newvector(L,n,t)	cast(t*, cupM_malloc_(L, (n)*sizeof(t), 0))
#define cupM_newvectorchecked(L,n,t) \
  (cupM_checksize(L,n,sizeof(t)), cupM_newvector(L,n,t))

#define cupM_newobject(L,tag,s)	cupM_malloc_(L, (s), tag)

#define cupM_growvector(L,v,nelems,size,t,limit,e) \
	((v)=cast(t *, cupM_growaux_(L,v,nelems,&(size),sizeof(t), \
                         cupM_limitN(limit,t),e)))

#define cupM_reallocvector(L, v,oldn,n,t) \
   (cast(t *, cupM_realloc_(L, v, cast_sizet(oldn) * sizeof(t), \
                                  cast_sizet(n) * sizeof(t))))

#define cupM_shrinkvector(L,v,size,fs,t) \
   ((v)=cast(t *, cupM_shrinkvector_(L, v, &(size), fs, sizeof(t))))

CUPI_FUNC l_noret cupM_toobig (cup_State *L);

/* not to be called directly */
CUPI_FUNC void *cupM_realloc_ (cup_State *L, void *block, size_t oldsize,
                                                          size_t size);
CUPI_FUNC void *cupM_saferealloc_ (cup_State *L, void *block, size_t oldsize,
                                                              size_t size);
CUPI_FUNC void cupM_free_ (cup_State *L, void *block, size_t osize);
CUPI_FUNC void *cupM_growaux_ (cup_State *L, void *block, int nelems,
                               int *size, int size_elem, int limit,
                               const char *what);
CUPI_FUNC void *cupM_shrinkvector_ (cup_State *L, void *block, int *nelem,
                                    int final_n, int size_elem);
CUPI_FUNC void *cupM_malloc_ (cup_State *L, size_t size, int tag);

#endif

