

#ifndef nebula_h
#define nebula_h

#include <stdarg.h>
#include <stddef.h>


#include "nebulaconf.h"


#define NEBULA_VERSION_MAJOR	"9"
#define NEBULA_VERSION_MINOR	"1"
#define NEBULA_VERSION_RELEASE	"1"

#define NEBULA_VERSION_NUM			504
#define NEBULA_VERSION_RELEASE_NUM		(NEBULA_VERSION_NUM * 100 + 4)

#define NEBULA_VERSION	"Nebula " NEBULA_VERSION_MAJOR "." NEBULA_VERSION_MINOR
#define NEBULA_RELEASE	NEBULA_VERSION "." NEBULA_VERSION_RELEASE
#define NEBULA_COPYRIGHT	NEBULA_RELEASE "  Made by Toast"
#define NEBULA_AUTHORS	"made by Toast"


/* mark for precompiled code ('<esc>Nebula') */
#define NEBULA_SIGNATURE	"\x1b"

/* option for multiple returns in 'nebula_pcall' and 'nebula_call' */
#define NEBULA_MULTRET	(-1)


/*
** Pseudo-indices
** (-NEBULAI_MAXSTACK is the minimum valid index; we keep some free empty
** space after that to help overflow detection)
*/
#define NEBULA_REGISTRYINDEX	(-NEBULAI_MAXSTACK - 1000)
#define nebula_upvalueindex(i)	(NEBULA_REGISTRYINDEX - (i))


/* thread status */
#define NEBULA_OK		0
#define NEBULA_YIELD	1
#define NEBULA_ERRRUN	2
#define NEBULA_ERRSYNTAX	3
#define NEBULA_ERRMEM	4
#define NEBULA_ERRERR	5


typedef struct nebula_State nebula_State;


/*
** basic types
*/
#define NEBULA_TNONE		(-1)

#define NEBULA_TNIL		0
#define NEBULA_TBOOLEAN		1
#define NEBULA_TLIGHTUSERDATA	2
#define NEBULA_TNUMBER		3
#define NEBULA_TSTRING		4
#define NEBULA_TTABLE		5
#define NEBULA_TFUNCTION		6
#define NEBULA_TUSERDATA		7
#define NEBULA_TTHREAD		8

#define NEBULA_NUMTYPES		9



/* minimum Nebula stack available to a C function */
#define NEBULA_MINSTACK	20


/* predefined values in the registry */
#define NEBULA_RIDX_MAINTHREAD	1
#define NEBULA_RIDX_GLOBALS	2
#define NEBULA_RIDX_LAST		NEBULA_RIDX_GLOBALS


/* type of numbers in Nebula */
typedef NEBULA_NUMBER nebula_Number;


/* type for integer functions */
typedef NEBULA_INTEGER nebula_Integer;

/* unsigned integer type */
typedef NEBULA_UNSIGNED nebula_Unsigned;

/* type for continuation-function contexts */
typedef NEBULA_KCONTEXT nebula_KContext;


/*
** Type for C functions registered with Nebula
*/
typedef int (*nebula_CFunction) (nebula_State *L);

/*
** Type for continuation functions
*/
typedef int (*nebula_KFunction) (nebula_State *L, int status, nebula_KContext ctx);


/*
** Type for functions that read/write blocks when loading/dumping Nebula chunks
*/
typedef const char * (*nebula_Reader) (nebula_State *L, void *ud, size_t *sz);

typedef int (*nebula_Writer) (nebula_State *L, const void *p, size_t sz, void *ud);


/*
** Type for memory-allocation functions
*/
typedef void * (*nebula_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** Type for warning functions
*/
typedef void (*nebula_WarnFunction) (void *ud, const char *msg, int tocont);




/*
** generic extra include file
*/
#if defined(NEBULA_USER_H)
#include NEBULA_USER_H
#endif


/*
** RCS ident string
*/
extern const char nebula_ident[];


/*
** state manipulation
*/
NEBULA_API nebula_State *(nebula_newstate) (nebula_Alloc f, void *ud);
NEBULA_API void       (nebula_close) (nebula_State *L);
NEBULA_API nebula_State *(nebula_newthread) (nebula_State *L);
NEBULA_API int        (nebula_resetthread) (nebula_State *L);

NEBULA_API nebula_CFunction (nebula_atpanic) (nebula_State *L, nebula_CFunction panicf);


NEBULA_API nebula_Number (nebula_version) (nebula_State *L);


/*
** basic stack manipulation
*/
NEBULA_API int   (nebula_absindex) (nebula_State *L, int idx);
NEBULA_API int   (nebula_gettop) (nebula_State *L);
NEBULA_API void  (nebula_settop) (nebula_State *L, int idx);
NEBULA_API void  (nebula_pushvalue) (nebula_State *L, int idx);
NEBULA_API void  (nebula_rotate) (nebula_State *L, int idx, int n);
NEBULA_API void  (nebula_copy) (nebula_State *L, int fromidx, int toidx);
NEBULA_API int   (nebula_checkstack) (nebula_State *L, int n);

NEBULA_API void  (nebula_xmove) (nebula_State *from, nebula_State *to, int n);


/*
** access functions (stack -> C)
*/

NEBULA_API int             (nebula_isnumber) (nebula_State *L, int idx);
NEBULA_API int             (nebula_isstring) (nebula_State *L, int idx);
NEBULA_API int             (nebula_iscfunction) (nebula_State *L, int idx);
NEBULA_API int             (nebula_isinteger) (nebula_State *L, int idx);
NEBULA_API int             (nebula_isuserdata) (nebula_State *L, int idx);
NEBULA_API int             (nebula_type) (nebula_State *L, int idx);
NEBULA_API const char     *(nebula_typename) (nebula_State *L, int tp);

NEBULA_API nebula_Number      (nebula_tonumberx) (nebula_State *L, int idx, int *isnum);
NEBULA_API nebula_Integer     (nebula_tointegerx) (nebula_State *L, int idx, int *isnum);
NEBULA_API int             (nebula_toboolean) (nebula_State *L, int idx);
NEBULA_API const char     *(nebula_tolstring) (nebula_State *L, int idx, size_t *len);
NEBULA_API nebula_Unsigned    (nebula_rawlen) (nebula_State *L, int idx);
NEBULA_API nebula_CFunction   (nebula_tocfunction) (nebula_State *L, int idx);
NEBULA_API void	       *(nebula_touserdata) (nebula_State *L, int idx);
NEBULA_API nebula_State      *(nebula_tothread) (nebula_State *L, int idx);
NEBULA_API const void     *(nebula_topointer) (nebula_State *L, int idx);


/*
** Comparison and arithmetic functions
*/

#define NEBULA_OPADD	0	/* ORDER TM, ORDER OP */
#define NEBULA_OPSUB	1
#define NEBULA_OPMUL	2
#define NEBULA_OPMOD	3
#define NEBULA_OPPOW	4
#define NEBULA_OPDIV	5
#define NEBULA_OPIDIV	6
#define NEBULA_OPBAND	7
#define NEBULA_OPBOR	8
#define NEBULA_OPBXOR	9
#define NEBULA_OPSHL	10
#define NEBULA_OPSHR	11
#define NEBULA_OPUNM	12
#define NEBULA_OPBNOT	13

NEBULA_API void  (nebula_arith) (nebula_State *L, int op);

#define NEBULA_OPEQ	0
#define NEBULA_OPLT	1
#define NEBULA_OPLE	2

NEBULA_API int   (nebula_rawequal) (nebula_State *L, int idx1, int idx2);
NEBULA_API int   (nebula_compare) (nebula_State *L, int idx1, int idx2, int op);


/*
** push functions (C -> stack)
*/
NEBULA_API void        (nebula_pushnil) (nebula_State *L);
NEBULA_API void        (nebula_pushnumber) (nebula_State *L, nebula_Number n);
NEBULA_API void        (nebula_pushinteger) (nebula_State *L, nebula_Integer n);
NEBULA_API const char *(nebula_pushlstring) (nebula_State *L, const char *s, size_t len);
NEBULA_API const char *(nebula_pushstring) (nebula_State *L, const char *s);
NEBULA_API const char *(nebula_pushvfstring) (nebula_State *L, const char *fmt,
                                                      va_list argp);
NEBULA_API const char *(nebula_pushfstring) (nebula_State *L, const char *fmt, ...);
NEBULA_API void  (nebula_pushcclosure) (nebula_State *L, nebula_CFunction fn, int n);
NEBULA_API void  (nebula_pushboolean) (nebula_State *L, int b);
NEBULA_API void  (nebula_pushlightuserdata) (nebula_State *L, void *p);
NEBULA_API int   (nebula_pushthread) (nebula_State *L);


/*
** get functions (Nebula -> stack)
*/
NEBULA_API int (nebula_getglobal) (nebula_State *L, const char *name);
NEBULA_API int (nebula_gettable) (nebula_State *L, int idx);
NEBULA_API int (nebula_getfield) (nebula_State *L, int idx, const char *k);
NEBULA_API int (nebula_geti) (nebula_State *L, int idx, nebula_Integer n);
NEBULA_API int (nebula_rawget) (nebula_State *L, int idx);
NEBULA_API int (nebula_rawgeti) (nebula_State *L, int idx, nebula_Integer n);
NEBULA_API int (nebula_rawgetp) (nebula_State *L, int idx, const void *p);

NEBULA_API void  (nebula_createtable) (nebula_State *L, int narr, int nrec);
NEBULA_API void *(nebula_newuserdatauv) (nebula_State *L, size_t sz, int nuvalue);
NEBULA_API int   (nebula_getmetatable) (nebula_State *L, int objindex);
NEBULA_API int  (nebula_getiuservalue) (nebula_State *L, int idx, int n);


/*
** set functions (stack -> Nebula)
*/
NEBULA_API void  (nebula_setglobal) (nebula_State *L, const char *name);
NEBULA_API void  (nebula_settable) (nebula_State *L, int idx);
NEBULA_API void  (nebula_setfield) (nebula_State *L, int idx, const char *k);
NEBULA_API void  (nebula_seti) (nebula_State *L, int idx, nebula_Integer n);
NEBULA_API void  (nebula_rawset) (nebula_State *L, int idx);
NEBULA_API void  (nebula_rawseti) (nebula_State *L, int idx, nebula_Integer n);
NEBULA_API void  (nebula_rawsetp) (nebula_State *L, int idx, const void *p);
NEBULA_API int   (nebula_setmetatable) (nebula_State *L, int objindex);
NEBULA_API int   (nebula_setiuservalue) (nebula_State *L, int idx, int n);


/*
** 'load' and 'call' functions (load and run Nebula code)
*/
NEBULA_API void  (nebula_callk) (nebula_State *L, int nargs, int nresults,
                           nebula_KContext ctx, nebula_KFunction k);
#define nebula_call(L,n,r)		nebula_callk(L, (n), (r), 0, NULL)

NEBULA_API int   (nebula_pcallk) (nebula_State *L, int nargs, int nresults, int errfunc,
                            nebula_KContext ctx, nebula_KFunction k);
#define nebula_pcall(L,n,r,f)	nebula_pcallk(L, (n), (r), (f), 0, NULL)

NEBULA_API int   (nebula_load) (nebula_State *L, nebula_Reader reader, void *dt,
                          const char *chunkname, const char *mode);

NEBULA_API int (nebula_dump) (nebula_State *L, nebula_Writer writer, void *data, int strip);


/*
** coroutine functions
*/
NEBULA_API int  (nebula_yieldk)     (nebula_State *L, int nresults, nebula_KContext ctx,
                               nebula_KFunction k);
NEBULA_API int  (nebula_resume)     (nebula_State *L, nebula_State *from, int narg,
                               int *nres);
NEBULA_API int  (nebula_status)     (nebula_State *L);
NEBULA_API int (nebula_isyieldable) (nebula_State *L);

#define nebula_yield(L,n)		nebula_yieldk(L, (n), 0, NULL)


/*
** Warning-related functions
*/
NEBULA_API void (nebula_setwarnf) (nebula_State *L, nebula_WarnFunction f, void *ud);
NEBULA_API void (nebula_warning)  (nebula_State *L, const char *msg, int tocont);


/*
** garbage-collection function and options
*/

#define NEBULA_GCSTOP		0
#define NEBULA_GCRESTART		1
#define NEBULA_GCCOLLECT		2
#define NEBULA_GCCOUNT		3
#define NEBULA_GCCOUNTB		4
#define NEBULA_GCSTEP		5
#define NEBULA_GCSETPAUSE		6
#define NEBULA_GCSETSTEPMUL	7
#define NEBULA_GCISRUNNING		9
#define NEBULA_GCGEN		10
#define NEBULA_GCINC		11

NEBULA_API int (nebula_gc) (nebula_State *L, int what, ...);


/*
** miscellaneous functions
*/

NEBULA_API int   (nebula_error) (nebula_State *L);

NEBULA_API int   (nebula_next) (nebula_State *L, int idx);

NEBULA_API void  (nebula_concat) (nebula_State *L, int n);
NEBULA_API void  (nebula_len)    (nebula_State *L, int idx);

NEBULA_API size_t   (nebula_stringtonumber) (nebula_State *L, const char *s);

NEBULA_API nebula_Alloc (nebula_getallocf) (nebula_State *L, void **ud);
NEBULA_API void      (nebula_setallocf) (nebula_State *L, nebula_Alloc f, void *ud);

NEBULA_API void (nebula_toclose) (nebula_State *L, int idx);
NEBULA_API void (nebula_closeslot) (nebula_State *L, int idx);


/*
** {==============================================================
** some useful macros
** ===============================================================
*/

#define nebula_getextraspace(L)	((void *)((char *)(L) - NEBULA_EXTRASPACE))

#define nebula_tonumber(L,i)	nebula_tonumberx(L,(i),NULL)
#define nebula_tointeger(L,i)	nebula_tointegerx(L,(i),NULL)

#define nebula_pop(L,n)		nebula_settop(L, -(n)-1)

#define nebula_newtable(L)		nebula_createtable(L, 0, 0)

#define nebula_register(L,n,f) (nebula_pushcfunction(L, (f)), nebula_setglobal(L, (n)))

#define nebula_pushcfunction(L,f)	nebula_pushcclosure(L, (f), 0)

#define nebula_isfunction(L,n)	(nebula_type(L, (n)) == NEBULA_TFUNCTION)
#define nebula_istable(L,n)	(nebula_type(L, (n)) == NEBULA_TTABLE)
#define nebula_islightuserdata(L,n)	(nebula_type(L, (n)) == NEBULA_TLIGHTUSERDATA)
#define nebula_isnil(L,n)		(nebula_type(L, (n)) == NEBULA_TNIL)
#define nebula_isboolean(L,n)	(nebula_type(L, (n)) == NEBULA_TBOOLEAN)
#define nebula_isthread(L,n)	(nebula_type(L, (n)) == NEBULA_TTHREAD)
#define nebula_isnone(L,n)		(nebula_type(L, (n)) == NEBULA_TNONE)
#define nebula_isnoneornil(L, n)	(nebula_type(L, (n)) <= 0)

#define nebula_pushliteral(L, s)	nebula_pushstring(L, "" s)

#define nebula_pushglobaltable(L)  \
	((void)nebula_rawgeti(L, NEBULA_REGISTRYINDEX, NEBULA_RIDX_GLOBALS))

#define nebula_tostring(L,i)	nebula_tolstring(L, (i), NULL)


#define nebula_insert(L,idx)	nebula_rotate(L, (idx), 1)

#define nebula_remove(L,idx)	(nebula_rotate(L, (idx), -1), nebula_pop(L, 1))

#define nebula_replace(L,idx)	(nebula_copy(L, -1, (idx)), nebula_pop(L, 1))

/* }============================================================== */


/*
** {==============================================================
** compatibility macros
** ===============================================================
*/
#if defined(NEBULA_COMPAT_APIINTCASTS)

#define nebula_pushunsigned(L,n)	nebula_pushinteger(L, (nebula_Integer)(n))
#define nebula_tounsignedx(L,i,is)	((nebula_Unsigned)nebula_tointegerx(L,i,is))
#define nebula_tounsigned(L,i)	nebula_tounsignedx(L,(i),NULL)

#endif

#define nebula_newuserdata(L,s)	nebula_newuserdatauv(L,s,1)
#define nebula_getuservalue(L,idx)	nebula_getiuservalue(L,idx,1)
#define nebula_setuservalue(L,idx)	nebula_setiuservalue(L,idx,1)

#define NEBULA_NUMTAGS		NEBULA_NUMTYPES

/* }============================================================== */

/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/
#define NEBULA_HOOKCALL	0
#define NEBULA_HOOKRET	1
#define NEBULA_HOOKLINE	2
#define NEBULA_HOOKCOUNT	3
#define NEBULA_HOOKTAILCALL 4


/*
** Event masks
*/
#define NEBULA_MASKCALL	(1 << NEBULA_HOOKCALL)
#define NEBULA_MASKRET	(1 << NEBULA_HOOKRET)
#define NEBULA_MASKLINE	(1 << NEBULA_HOOKLINE)
#define NEBULA_MASKCOUNT	(1 << NEBULA_HOOKCOUNT)

typedef struct nebula_Debug nebula_Debug;  /* activation record */


/* Functions to be called by the debugger in specific events */
typedef void (*nebula_Hook) (nebula_State *L, nebula_Debug *ar);


NEBULA_API int (nebula_getstack) (nebula_State *L, int level, nebula_Debug *ar);
NEBULA_API int (nebula_getinfo) (nebula_State *L, const char *what, nebula_Debug *ar);
NEBULA_API const char *(nebula_getlocal) (nebula_State *L, const nebula_Debug *ar, int n);
NEBULA_API const char *(nebula_setlocal) (nebula_State *L, const nebula_Debug *ar, int n);
NEBULA_API const char *(nebula_getupvalue) (nebula_State *L, int funcindex, int n);
NEBULA_API const char *(nebula_setupvalue) (nebula_State *L, int funcindex, int n);

NEBULA_API void *(nebula_upvalueid) (nebula_State *L, int fidx, int n);
NEBULA_API void  (nebula_upvaluejoin) (nebula_State *L, int fidx1, int n1,
                                               int fidx2, int n2);

NEBULA_API void (nebula_sethook) (nebula_State *L, nebula_Hook func, int mask, int count);
NEBULA_API nebula_Hook (nebula_gethook) (nebula_State *L);
NEBULA_API int (nebula_gethookmask) (nebula_State *L);
NEBULA_API int (nebula_gethookcount) (nebula_State *L);

NEBULA_API int (nebula_setcstacklimit) (nebula_State *L, unsigned int limit);

struct nebula_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) 'global', 'local', 'field', 'method' */
  const char *what;	/* (S) 'Nebula', 'C', 'main', 'tail' */
  const char *source;	/* (S) */
  size_t srclen;	/* (S) */
  int currentline;	/* (l) */
  int linedefined;	/* (S) */
  int lastlinedefined;	/* (S) */
  unsigned char nups;	/* (u) number of upvalues */
  unsigned char nparams;/* (u) number of parameters */
  char isvararg;        /* (u) */
  char istailcall;	/* (t) */
  unsigned short ftransfer;   /* (r) index of first value transferred */
  unsigned short ntransfer;   /* (r) number of transferred values */
  char short_src[NEBULA_IDSIZE]; /* (S) */
  /* private part */
  struct CallInfo *i_ci;  /* active function */
};
#endif