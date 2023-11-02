/*
** $Id: auxlib.h $
** Auxiliary functions for building Venom libraries
** See Copyright Notice in venom.h
*/


#ifndef auxlib_h
#define auxlib_h


#include <stddef.h>
#include <stdio.h>

#include "venomconf.h"
#include "venom.h"


/* global table */
#define VENOM_GNAME	"_G"


typedef struct venomL_Buffer venomL_Buffer;


/* extra error code for 'venomL_loadfilex' */
#define VENOM_ERRFILE     (VENOM_ERRERR+1)


/* key, in the registry, for table of loaded modules */
#define VENOM_LOADED_TABLE	"_LOADED"


/* key, in the registry, for table of preloaded loaders */
#define VENOM_PRELOAD_TABLE	"_PRELOAD"


typedef struct venomL_Reg {
  const char *name;
  venom_CFunction func;
} venomL_Reg;


#define VENOML_NUMSIZES	(sizeof(venom_Integer)*16 + sizeof(venom_Number))

VENOMLIB_API void (venomL_checkversion_) (venom_State *L, venom_Number ver, size_t sz);
#define venomL_checkversion(L)  \
	  venomL_checkversion_(L, VENOM_VERSION_NUM, VENOML_NUMSIZES)

VENOMLIB_API int (venomL_getmetafield) (venom_State *L, int obj, const char *e);
VENOMLIB_API int (venomL_callmeta) (venom_State *L, int obj, const char *e);
VENOMLIB_API const char *(venomL_tolstring) (venom_State *L, int idx, size_t *len);
VENOMLIB_API int (venomL_argerror) (venom_State *L, int arg, const char *extramsg);
VENOMLIB_API int (venomL_typeerror) (venom_State *L, int arg, const char *tname);
VENOMLIB_API const char *(venomL_checklstring) (venom_State *L, int arg,
                                                          size_t *l);
VENOMLIB_API const char *(venomL_optlstring) (venom_State *L, int arg,
                                          const char *def, size_t *l);
VENOMLIB_API venom_Number (venomL_checknumber) (venom_State *L, int arg);
VENOMLIB_API venom_Number (venomL_optnumber) (venom_State *L, int arg, venom_Number def);

VENOMLIB_API venom_Integer (venomL_checkinteger) (venom_State *L, int arg);
VENOMLIB_API venom_Integer (venomL_optinteger) (venom_State *L, int arg,
                                          venom_Integer def);

VENOMLIB_API void (venomL_checkstack) (venom_State *L, int sz, const char *msg);
VENOMLIB_API void (venomL_checktype) (venom_State *L, int arg, int t);
VENOMLIB_API void (venomL_checkany) (venom_State *L, int arg);

VENOMLIB_API int   (venomL_newmetatable) (venom_State *L, const char *tname);
VENOMLIB_API void  (venomL_setmetatable) (venom_State *L, const char *tname);
VENOMLIB_API void *(venomL_testudata) (venom_State *L, int ud, const char *tname);
VENOMLIB_API void *(venomL_checkudata) (venom_State *L, int ud, const char *tname);

VENOMLIB_API void (venomL_where) (venom_State *L, int lvl);
VENOMLIB_API int (venomL_error) (venom_State *L, const char *fmt, ...);

VENOMLIB_API int (venomL_checkoption) (venom_State *L, int arg, const char *def,
                                   const char *const lst[]);

VENOMLIB_API int (venomL_fileresult) (venom_State *L, int stat, const char *fname);
VENOMLIB_API int (venomL_execresult) (venom_State *L, int stat);


/* predefined references */
#define VENOM_NOREF       (-2)
#define VENOM_REFNIL      (-1)

VENOMLIB_API int (venomL_ref) (venom_State *L, int t);
VENOMLIB_API void (venomL_unref) (venom_State *L, int t, int ref);

VENOMLIB_API int (venomL_loadfilex) (venom_State *L, const char *filename,
                                               const char *mode);

#define venomL_loadfile(L,f)	venomL_loadfilex(L,f,NULL)

VENOMLIB_API int (venomL_loadbufferx) (venom_State *L, const char *buff, size_t sz,
                                   const char *name, const char *mode);
VENOMLIB_API int (venomL_loadstring) (venom_State *L, const char *s);

VENOMLIB_API venom_State *(venomL_newstate) (void);

VENOMLIB_API venom_Integer (venomL_len) (venom_State *L, int idx);

VENOMLIB_API void (venomL_addgsub) (venomL_Buffer *b, const char *s,
                                     const char *p, const char *r);
VENOMLIB_API const char *(venomL_gsub) (venom_State *L, const char *s,
                                    const char *p, const char *r);

VENOMLIB_API void (venomL_setfuncs) (venom_State *L, const venomL_Reg *l, int nup);

VENOMLIB_API int (venomL_getsubtable) (venom_State *L, int idx, const char *fname);

VENOMLIB_API void (venomL_traceback) (venom_State *L, venom_State *L1,
                                  const char *msg, int level);

VENOMLIB_API void (venomL_requiref) (venom_State *L, const char *modname,
                                 venom_CFunction openf, int glb);

/*
** ===============================================================
** some useful macros
** ===============================================================
*/


#define venomL_newlibtable(L,l)	\
  venom_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define venomL_newlib(L,l)  \
  (venomL_checkversion(L), venomL_newlibtable(L,l), venomL_setfuncs(L,l,0))

#define venomL_argcheck(L, cond,arg,extramsg)	\
	((void)(venomi_likely(cond) || venomL_argerror(L, (arg), (extramsg))))

#define venomL_argexpected(L,cond,arg,tname)	\
	((void)(venomi_likely(cond) || venomL_typeerror(L, (arg), (tname))))

#define venomL_checkstring(L,n)	(venomL_checklstring(L, (n), NULL))
#define venomL_optstring(L,n,d)	(venomL_optlstring(L, (n), (d), NULL))

#define venomL_typename(L,i)	venom_typename(L, venom_type(L,(i)))

#define venomL_dofile(L, fn) \
	(venomL_loadfile(L, fn) || venom_pcall(L, 0, VENOM_MULTRET, 0))

#define venomL_dostring(L, s) \
	(venomL_loadstring(L, s) || venom_pcall(L, 0, VENOM_MULTRET, 0))

#define venomL_getmetatable(L,n)	(venom_getfield(L, VENOM_REGISTRYINDEX, (n)))

#define venomL_opt(L,f,n,d)	(venom_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#define venomL_loadbuffer(L,s,sz,n)	venomL_loadbufferx(L,s,sz,n,NULL)


/*
** Perform arithmetic operations on venom_Integer values with wrap-around
** semantics, as the Venom core does.
*/
#define venomL_intop(op,v1,v2)  \
	((venom_Integer)((venom_Unsigned)(v1) op (venom_Unsigned)(v2)))


/* push the value used to represent failure/error */
#define venomL_pushfail(L)	venom_pushnil(L)


/*
** Internal assertions for in-house debugging
*/
#if !defined(venom_assert)

#if defined VENOMI_ASSERT
  #include <assert.h>
  #define venom_assert(c)		assert(c)
#else
  #define venom_assert(c)		((void)0)
#endif

#endif



/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

struct venomL_Buffer {
  char *b;  /* buffer address */
  size_t size;  /* buffer size */
  size_t n;  /* number of characters in buffer */
  venom_State *L;
  union {
    VENOMI_MAXALIGN;  /* ensure maximum alignment for buffer */
    char b[VENOML_BUFFERSIZE];  /* initial buffer */
  } init;
};


#define venomL_bufflen(bf)	((bf)->n)
#define venomL_buffaddr(bf)	((bf)->b)


#define venomL_addchar(B,c) \
  ((void)((B)->n < (B)->size || venomL_prepbuffsize((B), 1)), \
   ((B)->b[(B)->n++] = (c)))

#define venomL_addsize(B,s)	((B)->n += (s))

#define venomL_buffsub(B,s)	((B)->n -= (s))

VENOMLIB_API void (venomL_buffinit) (venom_State *L, venomL_Buffer *B);
VENOMLIB_API char *(venomL_prepbuffsize) (venomL_Buffer *B, size_t sz);
VENOMLIB_API void (venomL_addlstring) (venomL_Buffer *B, const char *s, size_t l);
VENOMLIB_API void (venomL_addstring) (venomL_Buffer *B, const char *s);
VENOMLIB_API void (venomL_addvalue) (venomL_Buffer *B);
VENOMLIB_API void (venomL_pushresult) (venomL_Buffer *B);
VENOMLIB_API void (venomL_pushresultsize) (venomL_Buffer *B, size_t sz);
VENOMLIB_API char *(venomL_buffinitsize) (venom_State *L, venomL_Buffer *B, size_t sz);

#define venomL_prepbuffer(B)	venomL_prepbuffsize(B, VENOML_BUFFERSIZE)

/* }====================================================== */



/*
** {======================================================
** File handles for IO library
** =======================================================
*/

/*
** A file handle is a userdata with metatable 'VENOM_FILEHANDLE' and
** initial structure 'venomL_Stream' (it may contain other fields
** after that initial structure).
*/

#define VENOM_FILEHANDLE          "FILE*"


typedef struct venomL_Stream {
  FILE *f;  /* stream (NULL for incompletely created streams) */
  venom_CFunction closef;  /* to close stream (NULL for closed streams) */
} venomL_Stream;

/* }====================================================== */

/*
** {==================================================================
** "Abstraction Layer" for basic report of messages and errors
** ===================================================================
*/

/* print a string */
#if !defined(venom_writestring)
#define venom_writestring(s,l)   fwrite((s), sizeof(char), (l), stdout)
#endif

/* print a newline and flush the output */
#if !defined(venom_writeline)
#define venom_writeline()        (venom_writestring("\n", 1), fflush(stdout))
#endif

/* print an error message */
#if !defined(venom_writestringerror)
#define venom_writestringerror(s,p) \
        (fprintf(stderr, (s), (p)), fflush(stderr))
#endif

/* }================================================================== */


/*
** {============================================================
** Compatibility with deprecated conversions
** =============================================================
*/
#if defined(VENOM_COMPAT_APIINTCASTS)

#define venomL_checkunsigned(L,a)	((venom_Unsigned)venomL_checkinteger(L,a))
#define venomL_optunsigned(L,a,d)	\
	((venom_Unsigned)venomL_optinteger(L,a,(venom_Integer)(d)))

#define venomL_checkint(L,n)	((int)venomL_checkinteger(L, (n)))
#define venomL_optint(L,n,d)	((int)venomL_optinteger(L, (n), (d)))

#define venomL_checklong(L,n)	((long)venomL_checkinteger(L, (n)))
#define venomL_optlong(L,n,d)	((long)venomL_optinteger(L, (n), (d)))

#endif
/* }============================================================ */



#endif