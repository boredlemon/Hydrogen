

#ifndef acorn_h
#define acorn_h

#include <stdarg.h>
#include <stddef.h>


#include "acornconf.h"


#define ACORN_VERSION_MAJOR	"0"
#define ACORN_VERSION_MINOR	"1"
#define ACORN_VERSION_RELEASE	"0"

#define ACORN_VERSION_NUM			504
#define ACORN_VERSION_RELEASE_NUM		(ACORN_VERSION_NUM * 100 + 4)

#define ACORN_VERSION	"Acorn " ACORN_VERSION_MAJOR "." ACORN_VERSION_MINOR
#define ACORN_RELEASE	ACORN_VERSION "." ACORN_VERSION_RELEASE
#define ACORN_COPYRIGHT	ACORN_RELEASE "  made by Coffee or Dolphin#6086"
#define ACORN_AUTHORS	"made by Coffee/Dolphin from discord"


/* mark for precompiled code ('<esc>Acorn') */
#define ACORN_SIGNATURE	"\x1b"

/* option for multiple returns in 'acorn_pcall' and 'acorn_call' */
#define ACORN_MULTRET	(-1)


/*
** Pseudo-indices
** (-ACORNI_MAXSTACK is the minimum valid index; we keep some free empty
** space after that to help overflow detection)
*/
#define ACORN_REGISTRYINDEX	(-ACORNI_MAXSTACK - 1000)
#define acorn_upvalueindex(i)	(ACORN_REGISTRYINDEX - (i))


/* thread status */
#define ACORN_OK		0
#define ACORN_YIELD	1
#define ACORN_ERRRUN	2
#define ACORN_ERRSYNTAX	3
#define ACORN_ERRMEM	4
#define ACORN_ERRERR	5


typedef struct acorn_State acorn_State;


/*
** basic types
*/
#define ACORN_TNONE		(-1)

#define ACORN_TNIL		0
#define ACORN_TBOOLEAN		1
#define ACORN_TLIGHTUSERDATA	2
#define ACORN_TNUMBER		3
#define ACORN_TSTRING		4
#define ACORN_TTABLE		5
#define ACORN_TFUNCTION		6
#define ACORN_TUSERDATA		7
#define ACORN_TTHREAD		8

#define ACORN_NUMTYPES		9



/* minimum Acorn stack available to a C function */
#define ACORN_MINSTACK	20


/* predefined values in the registry */
#define ACORN_RIDX_MAINTHREAD	1
#define ACORN_RIDX_GLOBALS	2
#define ACORN_RIDX_LAST		ACORN_RIDX_GLOBALS


/* type of numbers in Acorn */
typedef ACORN_NUMBER acorn_Number;


/* type for integer functions */
typedef ACORN_INTEGER acorn_Integer;

/* unsigned integer type */
typedef ACORN_UNSIGNED acorn_Unsigned;

/* type for continuation-function contexts */
typedef ACORN_KCONTEXT acorn_KContext;


/*
** Type for C functions registered with Acorn
*/
typedef int (*acorn_CFunction) (acorn_State *L);

/*
** Type for continuation functions
*/
typedef int (*acorn_KFunction) (acorn_State *L, int status, acorn_KContext ctx);


/*
** Type for functions that read/write blocks when loading/dumping Acorn chunks
*/
typedef const char * (*acorn_Reader) (acorn_State *L, void *ud, size_t *sz);

typedef int (*acorn_Writer) (acorn_State *L, const void *p, size_t sz, void *ud);


/*
** Type for memory-allocation functions
*/
typedef void * (*acorn_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** Type for warning functions
*/
typedef void (*acorn_WarnFunction) (void *ud, const char *msg, int tocont);




/*
** generic extra include file
*/
#if defined(ACORN_USER_H)
#include ACORN_USER_H
#endif


/*
** RCS ident string
*/
extern const char acorn_ident[];


/*
** state manipulation
*/
ACORN_API acorn_State *(acorn_newstate) (acorn_Alloc f, void *ud);
ACORN_API void       (acorn_close) (acorn_State *L);
ACORN_API acorn_State *(acorn_newthread) (acorn_State *L);
ACORN_API int        (acorn_resetthread) (acorn_State *L);

ACORN_API acorn_CFunction (acorn_atpanic) (acorn_State *L, acorn_CFunction panicf);


ACORN_API acorn_Number (acorn_version) (acorn_State *L);


/*
** basic stack manipulation
*/
ACORN_API int   (acorn_absindex) (acorn_State *L, int idx);
ACORN_API int   (acorn_gettop) (acorn_State *L);
ACORN_API void  (acorn_settop) (acorn_State *L, int idx);
ACORN_API void  (acorn_pushvalue) (acorn_State *L, int idx);
ACORN_API void  (acorn_rotate) (acorn_State *L, int idx, int n);
ACORN_API void  (acorn_copy) (acorn_State *L, int fromidx, int toidx);
ACORN_API int   (acorn_checkstack) (acorn_State *L, int n);

ACORN_API void  (acorn_xmove) (acorn_State *from, acorn_State *to, int n);


/*
** access functions (stack -> C)
*/

ACORN_API int             (acorn_isnumber) (acorn_State *L, int idx);
ACORN_API int             (acorn_isstring) (acorn_State *L, int idx);
ACORN_API int             (acorn_iscfunction) (acorn_State *L, int idx);
ACORN_API int             (acorn_isinteger) (acorn_State *L, int idx);
ACORN_API int             (acorn_isuserdata) (acorn_State *L, int idx);
ACORN_API int             (acorn_type) (acorn_State *L, int idx);
ACORN_API const char     *(acorn_typename) (acorn_State *L, int tp);

ACORN_API acorn_Number      (acorn_tonumberx) (acorn_State *L, int idx, int *isnum);
ACORN_API acorn_Integer     (acorn_tointegerx) (acorn_State *L, int idx, int *isnum);
ACORN_API int             (acorn_toboolean) (acorn_State *L, int idx);
ACORN_API const char     *(acorn_tolstring) (acorn_State *L, int idx, size_t *len);
ACORN_API acorn_Unsigned    (acorn_rawlen) (acorn_State *L, int idx);
ACORN_API acorn_CFunction   (acorn_tocfunction) (acorn_State *L, int idx);
ACORN_API void	       *(acorn_touserdata) (acorn_State *L, int idx);
ACORN_API acorn_State      *(acorn_tothread) (acorn_State *L, int idx);
ACORN_API const void     *(acorn_topointer) (acorn_State *L, int idx);


/*
** Comparison and arithmetic functions
*/

#define ACORN_OPADD	0	/* ORDER TM, ORDER OP */
#define ACORN_OPSUB	1
#define ACORN_OPMUL	2
#define ACORN_OPMOD	3
#define ACORN_OPPOW	4
#define ACORN_OPDIV	5
#define ACORN_OPIDIV	6
#define ACORN_OPBAND	7
#define ACORN_OPBOR	8
#define ACORN_OPBXOR	9
#define ACORN_OPSHL	10
#define ACORN_OPSHR	11
#define ACORN_OPUNM	12
#define ACORN_OPBNOT	13

ACORN_API void  (acorn_arith) (acorn_State *L, int op);

#define ACORN_OPEQ	0
#define ACORN_OPLT	1
#define ACORN_OPLE	2

ACORN_API int   (acorn_rawequal) (acorn_State *L, int idx1, int idx2);
ACORN_API int   (acorn_compare) (acorn_State *L, int idx1, int idx2, int op);


/*
** push functions (C -> stack)
*/
ACORN_API void        (acorn_pushnil) (acorn_State *L);
ACORN_API void        (acorn_pushnumber) (acorn_State *L, acorn_Number n);
ACORN_API void        (acorn_pushinteger) (acorn_State *L, acorn_Integer n);
ACORN_API const char *(acorn_pushlstring) (acorn_State *L, const char *s, size_t len);
ACORN_API const char *(acorn_pushstring) (acorn_State *L, const char *s);
ACORN_API const char *(acorn_pushvfstring) (acorn_State *L, const char *fmt,
                                                      va_list argp);
ACORN_API const char *(acorn_pushfstring) (acorn_State *L, const char *fmt, ...);
ACORN_API void  (acorn_pushcclosure) (acorn_State *L, acorn_CFunction fn, int n);
ACORN_API void  (acorn_pushboolean) (acorn_State *L, int b);
ACORN_API void  (acorn_pushlightuserdata) (acorn_State *L, void *p);
ACORN_API int   (acorn_pushthread) (acorn_State *L);


/*
** get functions (Acorn -> stack)
*/
ACORN_API int (acorn_getglobal) (acorn_State *L, const char *name);
ACORN_API int (acorn_gettable) (acorn_State *L, int idx);
ACORN_API int (acorn_getfield) (acorn_State *L, int idx, const char *k);
ACORN_API int (acorn_geti) (acorn_State *L, int idx, acorn_Integer n);
ACORN_API int (acorn_rawget) (acorn_State *L, int idx);
ACORN_API int (acorn_rawgeti) (acorn_State *L, int idx, acorn_Integer n);
ACORN_API int (acorn_rawgetp) (acorn_State *L, int idx, const void *p);

ACORN_API void  (acorn_createtable) (acorn_State *L, int narr, int nrec);
ACORN_API void *(acorn_newuserdatauv) (acorn_State *L, size_t sz, int nuvalue);
ACORN_API int   (acorn_getmetatable) (acorn_State *L, int objindex);
ACORN_API int  (acorn_getiuservalue) (acorn_State *L, int idx, int n);


/*
** set functions (stack -> Acorn)
*/
ACORN_API void  (acorn_setglobal) (acorn_State *L, const char *name);
ACORN_API void  (acorn_settable) (acorn_State *L, int idx);
ACORN_API void  (acorn_setfield) (acorn_State *L, int idx, const char *k);
ACORN_API void  (acorn_seti) (acorn_State *L, int idx, acorn_Integer n);
ACORN_API void  (acorn_rawset) (acorn_State *L, int idx);
ACORN_API void  (acorn_rawseti) (acorn_State *L, int idx, acorn_Integer n);
ACORN_API void  (acorn_rawsetp) (acorn_State *L, int idx, const void *p);
ACORN_API int   (acorn_setmetatable) (acorn_State *L, int objindex);
ACORN_API int   (acorn_setiuservalue) (acorn_State *L, int idx, int n);


/*
** 'load' and 'call' functions (load and run Acorn code)
*/
ACORN_API void  (acorn_callk) (acorn_State *L, int nargs, int nresults,
                           acorn_KContext ctx, acorn_KFunction k);
#define acorn_call(L,n,r)		acorn_callk(L, (n), (r), 0, NULL)

ACORN_API int   (acorn_pcallk) (acorn_State *L, int nargs, int nresults, int errfunc,
                            acorn_KContext ctx, acorn_KFunction k);
#define acorn_pcall(L,n,r,f)	acorn_pcallk(L, (n), (r), (f), 0, NULL)

ACORN_API int   (acorn_load) (acorn_State *L, acorn_Reader reader, void *dt,
                          const char *chunkname, const char *mode);

ACORN_API int (acorn_dump) (acorn_State *L, acorn_Writer writer, void *data, int strip);


/*
** coroutine functions
*/
ACORN_API int  (acorn_yieldk)     (acorn_State *L, int nresults, acorn_KContext ctx,
                               acorn_KFunction k);
ACORN_API int  (acorn_resume)     (acorn_State *L, acorn_State *from, int narg,
                               int *nres);
ACORN_API int  (acorn_status)     (acorn_State *L);
ACORN_API int (acorn_isyieldable) (acorn_State *L);

#define acorn_yield(L,n)		acorn_yieldk(L, (n), 0, NULL)


/*
** Warning-related functions
*/
ACORN_API void (acorn_setwarnf) (acorn_State *L, acorn_WarnFunction f, void *ud);
ACORN_API void (acorn_warning)  (acorn_State *L, const char *msg, int tocont);


/*
** garbage-collection function and options
*/

#define ACORN_GCSTOP		0
#define ACORN_GCRESTART		1
#define ACORN_GCCOLLECT		2
#define ACORN_GCCOUNT		3
#define ACORN_GCCOUNTB		4
#define ACORN_GCSTEP		5
#define ACORN_GCSETPAUSE		6
#define ACORN_GCSETSTEPMUL	7
#define ACORN_GCISRUNNING		9
#define ACORN_GCGEN		10
#define ACORN_GCINC		11

ACORN_API int (acorn_gc) (acorn_State *L, int what, ...);


/*
** miscellaneous functions
*/

ACORN_API int   (acorn_error) (acorn_State *L);

ACORN_API int   (acorn_next) (acorn_State *L, int idx);

ACORN_API void  (acorn_concat) (acorn_State *L, int n);
ACORN_API void  (acorn_len)    (acorn_State *L, int idx);

ACORN_API size_t   (acorn_stringtonumber) (acorn_State *L, const char *s);

ACORN_API acorn_Alloc (acorn_getallocf) (acorn_State *L, void **ud);
ACORN_API void      (acorn_setallocf) (acorn_State *L, acorn_Alloc f, void *ud);

ACORN_API void (acorn_toclose) (acorn_State *L, int idx);
ACORN_API void (acorn_closeslot) (acorn_State *L, int idx);


/*
** {==============================================================
** some useful macros
** ===============================================================
*/

#define acorn_getextraspace(L)	((void *)((char *)(L) - ACORN_EXTRASPACE))

#define acorn_tonumber(L,i)	acorn_tonumberx(L,(i),NULL)
#define acorn_tointeger(L,i)	acorn_tointegerx(L,(i),NULL)

#define acorn_pop(L,n)		acorn_settop(L, -(n)-1)

#define acorn_newtable(L)		acorn_createtable(L, 0, 0)

#define acorn_register(L,n,f) (acorn_pushcfunction(L, (f)), acorn_setglobal(L, (n)))

#define acorn_pushcfunction(L,f)	acorn_pushcclosure(L, (f), 0)

#define acorn_isfunction(L,n)	(acorn_type(L, (n)) == ACORN_TFUNCTION)
#define acorn_istable(L,n)	(acorn_type(L, (n)) == ACORN_TTABLE)
#define acorn_islightuserdata(L,n)	(acorn_type(L, (n)) == ACORN_TLIGHTUSERDATA)
#define acorn_isnil(L,n)		(acorn_type(L, (n)) == ACORN_TNIL)
#define acorn_isboolean(L,n)	(acorn_type(L, (n)) == ACORN_TBOOLEAN)
#define acorn_isthread(L,n)	(acorn_type(L, (n)) == ACORN_TTHREAD)
#define acorn_isnone(L,n)		(acorn_type(L, (n)) == ACORN_TNONE)
#define acorn_isnoneornil(L, n)	(acorn_type(L, (n)) <= 0)

#define acorn_pushliteral(L, s)	acorn_pushstring(L, "" s)

#define acorn_pushglobaltable(L)  \
	((void)acorn_rawgeti(L, ACORN_REGISTRYINDEX, ACORN_RIDX_GLOBALS))

#define acorn_tostring(L,i)	acorn_tolstring(L, (i), NULL)


#define acorn_insert(L,idx)	acorn_rotate(L, (idx), 1)

#define acorn_remove(L,idx)	(acorn_rotate(L, (idx), -1), acorn_pop(L, 1))

#define acorn_replace(L,idx)	(acorn_copy(L, -1, (idx)), acorn_pop(L, 1))

/* }============================================================== */


/*
** {==============================================================
** compatibility macros
** ===============================================================
*/
#if defined(ACORN_COMPAT_APIINTCASTS)

#define acorn_pushunsigned(L,n)	acorn_pushinteger(L, (acorn_Integer)(n))
#define acorn_tounsignedx(L,i,is)	((acorn_Unsigned)acorn_tointegerx(L,i,is))
#define acorn_tounsigned(L,i)	acorn_tounsignedx(L,(i),NULL)

#endif

#define acorn_newuserdata(L,s)	acorn_newuserdatauv(L,s,1)
#define acorn_getuservalue(L,idx)	acorn_getiuservalue(L,idx,1)
#define acorn_setuservalue(L,idx)	acorn_setiuservalue(L,idx,1)

#define ACORN_NUMTAGS		ACORN_NUMTYPES

/* }============================================================== */

/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/
#define ACORN_HOOKCALL	0
#define ACORN_HOOKRET	1
#define ACORN_HOOKLINE	2
#define ACORN_HOOKCOUNT	3
#define ACORN_HOOKTAILCALL 4


/*
** Event masks
*/
#define ACORN_MASKCALL	(1 << ACORN_HOOKCALL)
#define ACORN_MASKRET	(1 << ACORN_HOOKRET)
#define ACORN_MASKLINE	(1 << ACORN_HOOKLINE)
#define ACORN_MASKCOUNT	(1 << ACORN_HOOKCOUNT)

typedef struct acorn_Debug acorn_Debug;  /* activation record */


/* Functions to be called by the debugger in specific events */
typedef void (*acorn_Hook) (acorn_State *L, acorn_Debug *ar);


ACORN_API int (acorn_getstack) (acorn_State *L, int level, acorn_Debug *ar);
ACORN_API int (acorn_getinfo) (acorn_State *L, const char *what, acorn_Debug *ar);
ACORN_API const char *(acorn_getlocal) (acorn_State *L, const acorn_Debug *ar, int n);
ACORN_API const char *(acorn_setlocal) (acorn_State *L, const acorn_Debug *ar, int n);
ACORN_API const char *(acorn_getupvalue) (acorn_State *L, int funcindex, int n);
ACORN_API const char *(acorn_setupvalue) (acorn_State *L, int funcindex, int n);

ACORN_API void *(acorn_upvalueid) (acorn_State *L, int fidx, int n);
ACORN_API void  (acorn_upvaluejoin) (acorn_State *L, int fidx1, int n1,
                                               int fidx2, int n2);

ACORN_API void (acorn_sethook) (acorn_State *L, acorn_Hook func, int mask, int count);
ACORN_API acorn_Hook (acorn_gethook) (acorn_State *L);
ACORN_API int (acorn_gethookmask) (acorn_State *L);
ACORN_API int (acorn_gethookcount) (acorn_State *L);

ACORN_API int (acorn_setcstacklimit) (acorn_State *L, unsigned int limit);

struct acorn_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) 'global', 'local', 'field', 'method' */
  const char *what;	/* (S) 'Acorn', 'C', 'main', 'tail' */
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
  char short_src[ACORN_IDSIZE]; /* (S) */
  /* private part */
  struct CallInfo *i_ci;  /* active function */
};
#endif
