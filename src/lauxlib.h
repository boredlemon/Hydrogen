/*
** $Id: lauxlib.h $
** Auxiliary functions for building Cup libraries
** See Copyright Notice in cup.h
*/


#ifndef lauxlib_h
#define lauxlib_h


#include <stddef.h>
#include <stdio.h>

#include "cupconf.h"
#include "cup.h"


/* global table */
#define CUP_GNAME	"_G"


typedef struct cupL_Buffer cupL_Buffer;


/* extra error code for 'cupL_loadfilex' */
#define CUP_ERRFILE     (CUP_ERRERR+1)


/* key, in the registry, for table of loaded modules */
#define CUP_LOADED_TABLE	"_LOADED"


/* key, in the registry, for table of preloaded loaders */
#define CUP_PRELOAD_TABLE	"_PRELOAD"


typedef struct cupL_Reg {
  const char *name;
  cup_CFunction func;
} cupL_Reg;


#define CUPL_NUMSIZES	(sizeof(cup_Integer)*16 + sizeof(cup_Number))

CUPLIB_API void (cupL_checkversion_) (cup_State *L, cup_Number ver, size_t sz);
#define cupL_checkversion(L)  \
	  cupL_checkversion_(L, CUP_VERSION_NUM, CUPL_NUMSIZES)

CUPLIB_API int (cupL_getmetafield) (cup_State *L, int obj, const char *e);
CUPLIB_API int (cupL_callmeta) (cup_State *L, int obj, const char *e);
CUPLIB_API const char *(cupL_tolstring) (cup_State *L, int idx, size_t *len);
CUPLIB_API int (cupL_argerror) (cup_State *L, int arg, const char *extramsg);
CUPLIB_API int (cupL_typeerror) (cup_State *L, int arg, const char *tname);
CUPLIB_API const char *(cupL_checklstring) (cup_State *L, int arg,
                                                          size_t *l);
CUPLIB_API const char *(cupL_optlstring) (cup_State *L, int arg,
                                          const char *def, size_t *l);
CUPLIB_API cup_Number (cupL_checknumber) (cup_State *L, int arg);
CUPLIB_API cup_Number (cupL_optnumber) (cup_State *L, int arg, cup_Number def);

CUPLIB_API cup_Integer (cupL_checkinteger) (cup_State *L, int arg);
CUPLIB_API cup_Integer (cupL_optinteger) (cup_State *L, int arg,
                                          cup_Integer def);

CUPLIB_API void (cupL_checkstack) (cup_State *L, int sz, const char *msg);
CUPLIB_API void (cupL_checktype) (cup_State *L, int arg, int t);
CUPLIB_API void (cupL_checkany) (cup_State *L, int arg);

CUPLIB_API int   (cupL_newmetatable) (cup_State *L, const char *tname);
CUPLIB_API void  (cupL_setmetatable) (cup_State *L, const char *tname);
CUPLIB_API void *(cupL_testudata) (cup_State *L, int ud, const char *tname);
CUPLIB_API void *(cupL_checkudata) (cup_State *L, int ud, const char *tname);

CUPLIB_API void (cupL_where) (cup_State *L, int lvl);
CUPLIB_API int (cupL_error) (cup_State *L, const char *fmt, ...);

CUPLIB_API int (cupL_checkoption) (cup_State *L, int arg, const char *def,
                                   const char *const lst[]);

CUPLIB_API int (cupL_fileresult) (cup_State *L, int stat, const char *fname);
CUPLIB_API int (cupL_execresult) (cup_State *L, int stat);


/* predefined references */
#define CUP_NOREF       (-2)
#define CUP_REFNIL      (-1)

CUPLIB_API int (cupL_ref) (cup_State *L, int t);
CUPLIB_API void (cupL_unref) (cup_State *L, int t, int ref);

CUPLIB_API int (cupL_loadfilex) (cup_State *L, const char *filename,
                                               const char *mode);

#define cupL_loadfile(L,f)	cupL_loadfilex(L,f,NULL)

CUPLIB_API int (cupL_loadbufferx) (cup_State *L, const char *buff, size_t sz,
                                   const char *name, const char *mode);
CUPLIB_API int (cupL_loadstring) (cup_State *L, const char *s);

CUPLIB_API cup_State *(cupL_newstate) (void);

CUPLIB_API cup_Integer (cupL_len) (cup_State *L, int idx);

CUPLIB_API void (cupL_addgsub) (cupL_Buffer *b, const char *s,
                                     const char *p, const char *r);
CUPLIB_API const char *(cupL_gsub) (cup_State *L, const char *s,
                                    const char *p, const char *r);

CUPLIB_API void (cupL_setfuncs) (cup_State *L, const cupL_Reg *l, int nup);

CUPLIB_API int (cupL_getsubtable) (cup_State *L, int idx, const char *fname);

CUPLIB_API void (cupL_traceback) (cup_State *L, cup_State *L1,
                                  const char *msg, int level);

CUPLIB_API void (cupL_requiref) (cup_State *L, const char *modname,
                                 cup_CFunction openf, int glb);

/*
** ===============================================================
** some useful macros
** ===============================================================
*/


#define cupL_newlibtable(L,l)	\
  cup_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define cupL_newlib(L,l)  \
  (cupL_checkversion(L), cupL_newlibtable(L,l), cupL_setfuncs(L,l,0))

#define cupL_argcheck(L, cond,arg,extramsg)	\
	((void)(cupi_likely(cond) || cupL_argerror(L, (arg), (extramsg))))

#define cupL_argexpected(L,cond,arg,tname)	\
	((void)(cupi_likely(cond) || cupL_typeerror(L, (arg), (tname))))

#define cupL_checkstring(L,n)	(cupL_checklstring(L, (n), NULL))
#define cupL_optstring(L,n,d)	(cupL_optlstring(L, (n), (d), NULL))

#define cupL_typename(L,i)	cup_typename(L, cup_type(L,(i)))

#define cupL_dofile(L, fn) \
	(cupL_loadfile(L, fn) || cup_pcall(L, 0, CUP_MULTRET, 0))

#define cupL_dostring(L, s) \
	(cupL_loadstring(L, s) || cup_pcall(L, 0, CUP_MULTRET, 0))

#define cupL_getmetatable(L,n)	(cup_getfield(L, CUP_REGISTRYINDEX, (n)))

#define cupL_opt(L,f,n,d)	(cup_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#define cupL_loadbuffer(L,s,sz,n)	cupL_loadbufferx(L,s,sz,n,NULL)


/*
** Perform arithmetic operations on cup_Integer values with wrap-around
** semantics, as the Cup core does.
*/
#define cupL_intop(op,v1,v2)  \
	((cup_Integer)((cup_Unsigned)(v1) op (cup_Unsigned)(v2)))


/* push the value used to represent failure/error */
#define cupL_pushfail(L)	cup_pushnil(L)


/*
** Internal assertions for in-house debugging
*/
#if !defined(cup_assert)

#if defined CUPI_ASSERT
  #include <assert.h>
  #define cup_assert(c)		assert(c)
#else
  #define cup_assert(c)		((void)0)
#endif

#endif



/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

struct cupL_Buffer {
  char *b;  /* buffer address */
  size_t size;  /* buffer size */
  size_t n;  /* number of characters in buffer */
  cup_State *L;
  union {
    CUPI_MAXALIGN;  /* ensure maximum alignment for buffer */
    char b[CUPL_BUFFERSIZE];  /* initial buffer */
  } init;
};


#define cupL_bufflen(bf)	((bf)->n)
#define cupL_buffaddr(bf)	((bf)->b)


#define cupL_addchar(B,c) \
  ((void)((B)->n < (B)->size || cupL_prepbuffsize((B), 1)), \
   ((B)->b[(B)->n++] = (c)))

#define cupL_addsize(B,s)	((B)->n += (s))

#define cupL_buffsub(B,s)	((B)->n -= (s))

CUPLIB_API void (cupL_buffinit) (cup_State *L, cupL_Buffer *B);
CUPLIB_API char *(cupL_prepbuffsize) (cupL_Buffer *B, size_t sz);
CUPLIB_API void (cupL_addlstring) (cupL_Buffer *B, const char *s, size_t l);
CUPLIB_API void (cupL_addstring) (cupL_Buffer *B, const char *s);
CUPLIB_API void (cupL_addvalue) (cupL_Buffer *B);
CUPLIB_API void (cupL_pushresult) (cupL_Buffer *B);
CUPLIB_API void (cupL_pushresultsize) (cupL_Buffer *B, size_t sz);
CUPLIB_API char *(cupL_buffinitsize) (cup_State *L, cupL_Buffer *B, size_t sz);

#define cupL_prepbuffer(B)	cupL_prepbuffsize(B, CUPL_BUFFERSIZE)

/* }====================================================== */



/*
** {======================================================
** File handles for IO library
** =======================================================
*/

/*
** A file handle is a userdata with metatable 'CUP_FILEHANDLE' and
** initial structure 'cupL_Stream' (it may contain other fields
** after that initial structure).
*/

#define CUP_FILEHANDLE          "FILE*"


typedef struct cupL_Stream {
  FILE *f;  /* stream (NULL for incompletely created streams) */
  cup_CFunction closef;  /* to close stream (NULL for closed streams) */
} cupL_Stream;

/* }====================================================== */

/*
** {==================================================================
** "Abstraction Layer" for basic report of messages and errors
** ===================================================================
*/

/* print a string */
#if !defined(cup_writestring)
#define cup_writestring(s,l)   fwrite((s), sizeof(char), (l), stdout)
#endif

/* print a newline and flush the output */
#if !defined(cup_writeline)
#define cup_writeline()        (cup_writestring("\n", 1), fflush(stdout))
#endif

/* print an error message */
#if !defined(cup_writestringerror)
#define cup_writestringerror(s,p) \
        (fprintf(stderr, (s), (p)), fflush(stderr))
#endif

/* }================================================================== */


/*
** {============================================================
** Compatibility with deprecated conversions
** =============================================================
*/
#if defined(CUP_COMPAT_APIINTCASTS)

#define cupL_checkunsigned(L,a)	((cup_Unsigned)cupL_checkinteger(L,a))
#define cupL_optunsigned(L,a,d)	\
	((cup_Unsigned)cupL_optinteger(L,a,(cup_Integer)(d)))

#define cupL_checkint(L,n)	((int)cupL_checkinteger(L, (n)))
#define cupL_optint(L,n,d)	((int)cupL_optinteger(L, (n), (d)))

#define cupL_checklong(L,n)	((long)cupL_checkinteger(L, (n)))
#define cupL_optlong(L,n,d)	((long)cupL_optinteger(L, (n), (d)))

#endif
/* }============================================================ */



#endif


