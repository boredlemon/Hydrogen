/*
** $Id: memory.h $
** Interface to Memory Manager
** See Copyright Notice in nebula.h
*/

#ifndef memory_h
#define memory_h


#include <stddef.h>

#include "limits.h"
#include "nebula.h"


#define nebulaM_error(L)	nebulaD_throw(L, NEBULA_ERRMEM)


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
#define nebulaM_testsize(n,e)  \
	(sizeof(n) >= sizeof(size_t) && cast_sizet((n)) + 1 > MAX_SIZET/(e))

#define nebulaM_checksize(L,n,e)  \
	(nebulaM_testsize(n,e) ? nebulaM_toobig(L) : cast_void(0))


/*
** Computes the minimum between 'n' and 'MAX_SIZET/sizeof(t)', so that
** the result is not larger than 'n' and cannot overflow a 'size_t'
** when multiplied by the size of type 't'. (Assumes that 'n' is an
** 'int' or 'unsigned int' and that 'int' is not larger than 'size_t'.)
*/
#define nebulaM_limitN(n,t)  \
  ((cast_sizet(n) <= MAX_SIZET/sizeof(t)) ? (n) :  \
     cast_uint((MAX_SIZET/sizeof(t))))


/*
** Arrays of chars do not need any test
*/
#define nebulaM_reallocvchar(L,b,on,n)  \
  cast_charp(nebulaM_saferealloc_(L, (b), (on)*sizeof(char), (n)*sizeof(char)))

#define nebulaM_freemem(L, b, s)	nebulaM_free_(L, (b), (s))
#define nebulaM_free(L, b)		nebulaM_free_(L, (b), sizeof(*(b)))
#define nebulaM_freearray(L, b, n)   nebulaM_free_(L, (b), (n)*sizeof(*(b)))

#define nebulaM_new(L,t)		cast(t*, nebulaM_malloc_(L, sizeof(t), 0))
#define nebulaM_newvector(L,n,t)	cast(t*, nebulaM_malloc_(L, (n)*sizeof(t), 0))
#define nebulaM_newvectorchecked(L,n,t) \
  (nebulaM_checksize(L,n,sizeof(t)), nebulaM_newvector(L,n,t))

#define nebulaM_newobject(L,tag,s)	nebulaM_malloc_(L, (s), tag)

#define nebulaM_growvector(L,v,nelems,size,t,limit,e) \
	((v)=cast(t *, nebulaM_growaux_(L,v,nelems,&(size),sizeof(t), \
                         nebulaM_limitN(limit,t),e)))

#define nebulaM_reallocvector(L, v,oldn,n,t) \
   (cast(t *, nebulaM_realloc_(L, v, cast_sizet(oldn) * sizeof(t), \
                                  cast_sizet(n) * sizeof(t))))

#define nebulaM_shrinkvector(L,v,size,fs,t) \
   ((v)=cast(t *, nebulaM_shrinkvector_(L, v, &(size), fs, sizeof(t))))

NEBULAI_FUNC l_noret nebulaM_toobig (nebula_State *L);

/* not to be called directly */
NEBULAI_FUNC void *nebulaM_realloc_ (nebula_State *L, void *block, size_t oldsize,
                                                          size_t size);
NEBULAI_FUNC void *nebulaM_saferealloc_ (nebula_State *L, void *block, size_t oldsize,
                                                              size_t size);
NEBULAI_FUNC void nebulaM_free_ (nebula_State *L, void *block, size_t osize);
NEBULAI_FUNC void *nebulaM_growaux_ (nebula_State *L, void *block, int nelems,
                               int *size, int size_elem, int limit,
                               const char *what);
NEBULAI_FUNC void *nebulaM_shrinkvector_ (nebula_State *L, void *block, int *nelem,
                                    int final_n, int size_elem);
NEBULAI_FUNC void *nebulaM_malloc_ (nebula_State *L, size_t size, int tag);

#endif

