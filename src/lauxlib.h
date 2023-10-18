/*
** $Id: lauxlib.h $
** Auxiliary functions for building Acorn libraries
** See Copyright Notice in acorn.h
*/


#ifndef lauxlib_h
#define lauxlib_h


#include <stddef.h>
#include <stdio.h>

#include "acornconf.h"
#include "acorn.h"


/* global table */
#define ACORN_GNAME	"_G"


typedef struct acornL_Buffer acornL_Buffer;


/* extra error code for 'acornL_loadfilex' */
#define ACORN_ERRFILE     (ACORN_ERRERR+1)


/* key, in the registry, for table of loaded modules */
#define ACORN_LOADED_TABLE	"_LOADED"


/* key, in the registry, for table of preloaded loaders */
#define ACORN_PRELOAD_TABLE	"_PRELOAD"


typedef struct acornL_Reg {
  const char *name;
  acorn_CFunction func;
} acornL_Reg;


#define ACORNL_NUMSIZES	(sizeof(acorn_Integer)*16 + sizeof(acorn_Number))

ACORNLIB_API void (acornL_checkversion_) (acorn_State *L, acorn_Number ver, size_t sz);
#define acornL_checkversion(L)  \
	  acornL_checkversion_(L, ACORN_VERSION_NUM, ACORNL_NUMSIZES)

ACORNLIB_API int (acornL_getmetafield) (acorn_State *L, int obj, const char *e);
ACORNLIB_API int (acornL_callmeta) (acorn_State *L, int obj, const char *e);
ACORNLIB_API const char *(acornL_tolstring) (acorn_State *L, int idx, size_t *len);
ACORNLIB_API int (acornL_argerror) (acorn_State *L, int arg, const char *extramsg);
ACORNLIB_API int (acornL_typeerror) (acorn_State *L, int arg, const char *tname);
ACORNLIB_API const char *(acornL_checklstring) (acorn_State *L, int arg,
                                                          size_t *l);
ACORNLIB_API const char *(acornL_optlstring) (acorn_State *L, int arg,
                                          const char *def, size_t *l);
ACORNLIB_API acorn_Number (acornL_checknumber) (acorn_State *L, int arg);
ACORNLIB_API acorn_Number (acornL_optnumber) (acorn_State *L, int arg, acorn_Number def);

ACORNLIB_API acorn_Integer (acornL_checkinteger) (acorn_State *L, int arg);
ACORNLIB_API acorn_Integer (acornL_optinteger) (acorn_State *L, int arg,
                                          acorn_Integer def);

ACORNLIB_API void (acornL_checkstack) (acorn_State *L, int sz, const char *msg);
ACORNLIB_API void (acornL_checktype) (acorn_State *L, int arg, int t);
ACORNLIB_API void (acornL_checkany) (acorn_State *L, int arg);

ACORNLIB_API int   (acornL_newmetatable) (acorn_State *L, const char *tname);
ACORNLIB_API void  (acornL_setmetatable) (acorn_State *L, const char *tname);
ACORNLIB_API void *(acornL_testudata) (acorn_State *L, int ud, const char *tname);
ACORNLIB_API void *(acornL_checkudata) (acorn_State *L, int ud, const char *tname);

ACORNLIB_API void (acornL_where) (acorn_State *L, int lvl);
ACORNLIB_API int (acornL_error) (acorn_State *L, const char *fmt, ...);

ACORNLIB_API int (acornL_checkoption) (acorn_State *L, int arg, const char *def,
                                   const char *const lst[]);

ACORNLIB_API int (acornL_fileresult) (acorn_State *L, int stat, const char *fname);
ACORNLIB_API int (acornL_execresult) (acorn_State *L, int stat);


/* predefined references */
#define ACORN_NOREF       (-2)
#define ACORN_REFNIL      (-1)

ACORNLIB_API int (acornL_ref) (acorn_State *L, int t);
ACORNLIB_API void (acornL_unref) (acorn_State *L, int t, int ref);

ACORNLIB_API int (acornL_loadfilex) (acorn_State *L, const char *filename,
                                               const char *mode);

#define acornL_loadfile(L,f)	acornL_loadfilex(L,f,NULL)

ACORNLIB_API int (acornL_loadbufferx) (acorn_State *L, const char *buff, size_t sz,
                                   const char *name, const char *mode);
ACORNLIB_API int (acornL_loadstring) (acorn_State *L, const char *s);

ACORNLIB_API acorn_State *(acornL_newstate) (void);

ACORNLIB_API acorn_Integer (acornL_len) (acorn_State *L, int idx);

ACORNLIB_API void (acornL_addgsub) (acornL_Buffer *b, const char *s,
                                     const char *p, const char *r);
ACORNLIB_API const char *(acornL_gsub) (acorn_State *L, const char *s,
                                    const char *p, const char *r);

ACORNLIB_API void (acornL_setfuncs) (acorn_State *L, const acornL_Reg *l, int nup);

ACORNLIB_API int (acornL_getsubtable) (acorn_State *L, int idx, const char *fname);

ACORNLIB_API void (acornL_traceback) (acorn_State *L, acorn_State *L1,
                                  const char *msg, int level);

ACORNLIB_API void (acornL_requiref) (acorn_State *L, const char *modname,
                                 acorn_CFunction openf, int glb);

/*
** ===============================================================
** some useful macros
** ===============================================================
*/


#define acornL_newlibtable(L,l)	\
  acorn_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define acornL_newlib(L,l)  \
  (acornL_checkversion(L), acornL_newlibtable(L,l), acornL_setfuncs(L,l,0))

#define acornL_argcheck(L, cond,arg,extramsg)	\
	((void)(acorni_likely(cond) || acornL_argerror(L, (arg), (extramsg))))

#define acornL_argexpected(L,cond,arg,tname)	\
	((void)(acorni_likely(cond) || acornL_typeerror(L, (arg), (tname))))

#define acornL_checkstring(L,n)	(acornL_checklstring(L, (n), NULL))
#define acornL_optstring(L,n,d)	(acornL_optlstring(L, (n), (d), NULL))

#define acornL_typename(L,i)	acorn_typename(L, acorn_type(L,(i)))

#define acornL_dofile(L, fn) \
	(acornL_loadfile(L, fn) || acorn_pcall(L, 0, ACORN_MULTRET, 0))

#define acornL_dostring(L, s) \
	(acornL_loadstring(L, s) || acorn_pcall(L, 0, ACORN_MULTRET, 0))

#define acornL_getmetatable(L,n)	(acorn_getfield(L, ACORN_REGISTRYINDEX, (n)))

#define acornL_opt(L,f,n,d)	(acorn_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#define acornL_loadbuffer(L,s,sz,n)	acornL_loadbufferx(L,s,sz,n,NULL)


/*
** Perform arithmetic operations on acorn_Integer values with wrap-around
** semantics, as the Acorn core does.
*/
#define acornL_intop(op,v1,v2)  \
	((acorn_Integer)((acorn_Unsigned)(v1) op (acorn_Unsigned)(v2)))


/* push the value used to represent failure/error */
#define acornL_pushfail(L)	acorn_pushnil(L)


/*
** Internal assertions for in-house debugging
*/
#if !defined(acorn_assert)

#if defined ACORNI_ASSERT
  #include <assert.h>
  #define acorn_assert(c)		assert(c)
#else
  #define acorn_assert(c)		((void)0)
#endif

#endif



/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

struct acornL_Buffer {
  char *b;  /* buffer address */
  size_t size;  /* buffer size */
  size_t n;  /* number of characters in buffer */
  acorn_State *L;
  union {
    ACORNI_MAXALIGN;  /* ensure maximum alignment for buffer */
    char b[ACORNL_BUFFERSIZE];  /* initial buffer */
  } init;
};


#define acornL_bufflen(bf)	((bf)->n)
#define acornL_buffaddr(bf)	((bf)->b)


#define acornL_addchar(B,c) \
  ((void)((B)->n < (B)->size || acornL_prepbuffsize((B), 1)), \
   ((B)->b[(B)->n++] = (c)))

#define acornL_addsize(B,s)	((B)->n += (s))

#define acornL_buffsub(B,s)	((B)->n -= (s))

ACORNLIB_API void (acornL_buffinit) (acorn_State *L, acornL_Buffer *B);
ACORNLIB_API char *(acornL_prepbuffsize) (acornL_Buffer *B, size_t sz);
ACORNLIB_API void (acornL_addlstring) (acornL_Buffer *B, const char *s, size_t l);
ACORNLIB_API void (acornL_addstring) (acornL_Buffer *B, const char *s);
ACORNLIB_API void (acornL_addvalue) (acornL_Buffer *B);
ACORNLIB_API void (acornL_pushresult) (acornL_Buffer *B);
ACORNLIB_API void (acornL_pushresultsize) (acornL_Buffer *B, size_t sz);
ACORNLIB_API char *(acornL_buffinitsize) (acorn_State *L, acornL_Buffer *B, size_t sz);

#define acornL_prepbuffer(B)	acornL_prepbuffsize(B, ACORNL_BUFFERSIZE)

/* }====================================================== */



/*
** {======================================================
** File handles for IO library
** =======================================================
*/

/*
** A file handle is a userdata with metatable 'ACORN_FILEHANDLE' and
** initial structure 'acornL_Stream' (it may contain other fields
** after that initial structure).
*/

#define ACORN_FILEHANDLE          "FILE*"


typedef struct acornL_Stream {
  FILE *f;  /* stream (NULL for incompletely created streams) */
  acorn_CFunction closef;  /* to close stream (NULL for closed streams) */
} acornL_Stream;

/* }====================================================== */

/*
** {==================================================================
** "Abstraction Layer" for basic report of messages and errors
** ===================================================================
*/

/* print a string */
#if !defined(acorn_writestring)
#define acorn_writestring(s,l)   fwrite((s), sizeof(char), (l), stdout)
#endif

/* print a newline and flush the output */
#if !defined(acorn_writeline)
#define acorn_writeline()        (acorn_writestring("\n", 1), fflush(stdout))
#endif

/* print an error message */
#if !defined(acorn_writestringerror)
#define acorn_writestringerror(s,p) \
        (fprintf(stderr, (s), (p)), fflush(stderr))
#endif

/* }================================================================== */


/*
** {============================================================
** Compatibility with deprecated conversions
** =============================================================
*/
#if defined(ACORN_COMPAT_APIINTCASTS)

#define acornL_checkunsigned(L,a)	((acorn_Unsigned)acornL_checkinteger(L,a))
#define acornL_optunsigned(L,a,d)	\
	((acorn_Unsigned)acornL_optinteger(L,a,(acorn_Integer)(d)))

#define acornL_checkint(L,n)	((int)acornL_checkinteger(L, (n)))
#define acornL_optint(L,n,d)	((int)acornL_optinteger(L, (n), (d)))

#define acornL_checklong(L,n)	((long)acornL_checkinteger(L, (n)))
#define acornL_optlong(L,n,d)	((long)acornL_optinteger(L, (n), (d)))

#endif
/* }============================================================ */



#endif


