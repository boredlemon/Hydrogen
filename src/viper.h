

#ifndef viper_h
#define viper_h

#include <stdarg.h>
#include <stddef.h>


#include "viperconf.h"


#define VIPER_VERSION_MAJOR	"9"
#define VIPER_VERSION_MINOR	"1"
#define VIPER_VERSION_RELEASE	"1"

#define VIPER_VERSION_NUM			504
#define VIPER_VERSION_RELEASE_NUM		(VIPER_VERSION_NUM * 100 + 4)

#define VIPER_VERSION	"Viper " VIPER_VERSION_MAJOR "." VIPER_VERSION_MINOR
#define VIPER_RELEASE	VIPER_VERSION "." VIPER_VERSION_RELEASE
#define VIPER_COPYRIGHT	VIPER_RELEASE "  made by Coffee or Dolphin#6086"
#define VIPER_AUTHORS	"made by Coffee/Dolphin from discord"


/* mark for precompiled code ('<esc>Viper') */
#define VIPER_SIGNATURE	"\x1b"

/* option for multiple returns in 'viper_pcall' and 'viper_call' */
#define VIPER_MULTRET	(-1)


/*
** Pseudo-indices
** (-VIPERI_MAXSTACK is the minimum valid index; we keep some free empty
** space after that to help overflow detection)
*/
#define VIPER_REGISTRYINDEX	(-VIPERI_MAXSTACK - 1000)
#define viper_upvalueindex(i)	(VIPER_REGISTRYINDEX - (i))


/* thread status */
#define VIPER_OK		0
#define VIPER_YIELD	1
#define VIPER_ERRRUN	2
#define VIPER_ERRSYNTAX	3
#define VIPER_ERRMEM	4
#define VIPER_ERRERR	5


typedef struct viper_State viper_State;


/*
** basic types
*/
#define VIPER_TNONE		(-1)

#define VIPER_TNIL		0
#define VIPER_TBOOLEAN		1
#define VIPER_TLIGHTUSERDATA	2
#define VIPER_TNUMBER		3
#define VIPER_TSTRING		4
#define VIPER_TTABLE		5
#define VIPER_TFUNCTION		6
#define VIPER_TUSERDATA		7
#define VIPER_TTHREAD		8

#define VIPER_NUMTYPES		9



/* minimum Viper stack available to a C function */
#define VIPER_MINSTACK	20


/* predefined values in the registry */
#define VIPER_RIDX_MAINTHREAD	1
#define VIPER_RIDX_GLOBALS	2
#define VIPER_RIDX_LAST		VIPER_RIDX_GLOBALS


/* type of numbers in Viper */
typedef VIPER_NUMBER viper_Number;


/* type for integer functions */
typedef VIPER_INTEGER viper_Integer;

/* unsigned integer type */
typedef VIPER_UNSIGNED viper_Unsigned;

/* type for continuation-function contexts */
typedef VIPER_KCONTEXT viper_KContext;


/*
** Type for C functions registered with Viper
*/
typedef int (*viper_CFunction) (viper_State *L);

/*
** Type for continuation functions
*/
typedef int (*viper_KFunction) (viper_State *L, int status, viper_KContext ctx);


/*
** Type for functions that read/write blocks when loading/dumping Viper chunks
*/
typedef const char * (*viper_Reader) (viper_State *L, void *ud, size_t *sz);

typedef int (*viper_Writer) (viper_State *L, const void *p, size_t sz, void *ud);


/*
** Type for memory-allocation functions
*/
typedef void * (*viper_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** Type for warning functions
*/
typedef void (*viper_WarnFunction) (void *ud, const char *msg, int tocont);




/*
** generic extra include file
*/
#if defined(VIPER_USER_H)
#include VIPER_USER_H
#endif


/*
** RCS ident string
*/
extern const char viper_ident[];


/*
** state manipulation
*/
VIPER_API viper_State *(viper_newstate) (viper_Alloc f, void *ud);
VIPER_API void       (viper_close) (viper_State *L);
VIPER_API viper_State *(viper_newthread) (viper_State *L);
VIPER_API int        (viper_resetthread) (viper_State *L);

VIPER_API viper_CFunction (viper_atpanic) (viper_State *L, viper_CFunction panicf);


VIPER_API viper_Number (viper_version) (viper_State *L);


/*
** basic stack manipulation
*/
VIPER_API int   (viper_absindex) (viper_State *L, int idx);
VIPER_API int   (viper_gettop) (viper_State *L);
VIPER_API void  (viper_settop) (viper_State *L, int idx);
VIPER_API void  (viper_pushvalue) (viper_State *L, int idx);
VIPER_API void  (viper_rotate) (viper_State *L, int idx, int n);
VIPER_API void  (viper_copy) (viper_State *L, int fromidx, int toidx);
VIPER_API int   (viper_checkstack) (viper_State *L, int n);

VIPER_API void  (viper_xmove) (viper_State *from, viper_State *to, int n);


/*
** access functions (stack -> C)
*/

VIPER_API int             (viper_isnumber) (viper_State *L, int idx);
VIPER_API int             (viper_isstring) (viper_State *L, int idx);
VIPER_API int             (viper_iscfunction) (viper_State *L, int idx);
VIPER_API int             (viper_isinteger) (viper_State *L, int idx);
VIPER_API int             (viper_isuserdata) (viper_State *L, int idx);
VIPER_API int             (viper_type) (viper_State *L, int idx);
VIPER_API const char     *(viper_typename) (viper_State *L, int tp);

VIPER_API viper_Number      (viper_tonumberx) (viper_State *L, int idx, int *isnum);
VIPER_API viper_Integer     (viper_tointegerx) (viper_State *L, int idx, int *isnum);
VIPER_API int             (viper_toboolean) (viper_State *L, int idx);
VIPER_API const char     *(viper_tolstring) (viper_State *L, int idx, size_t *len);
VIPER_API viper_Unsigned    (viper_rawlen) (viper_State *L, int idx);
VIPER_API viper_CFunction   (viper_tocfunction) (viper_State *L, int idx);
VIPER_API void	       *(viper_touserdata) (viper_State *L, int idx);
VIPER_API viper_State      *(viper_tothread) (viper_State *L, int idx);
VIPER_API const void     *(viper_topointer) (viper_State *L, int idx);


/*
** Comparison and arithmetic functions
*/

#define VIPER_OPADD	0	/* ORDER TM, ORDER OP */
#define VIPER_OPSUB	1
#define VIPER_OPMUL	2
#define VIPER_OPMOD	3
#define VIPER_OPPOW	4
#define VIPER_OPDIV	5
#define VIPER_OPIDIV	6
#define VIPER_OPBAND	7
#define VIPER_OPBOR	8
#define VIPER_OPBXOR	9
#define VIPER_OPSHL	10
#define VIPER_OPSHR	11
#define VIPER_OPUNM	12
#define VIPER_OPBNOT	13

VIPER_API void  (viper_arith) (viper_State *L, int op);

#define VIPER_OPEQ	0
#define VIPER_OPLT	1
#define VIPER_OPLE	2

VIPER_API int   (viper_rawequal) (viper_State *L, int idx1, int idx2);
VIPER_API int   (viper_compare) (viper_State *L, int idx1, int idx2, int op);


/*
** push functions (C -> stack)
*/
VIPER_API void        (viper_pushnil) (viper_State *L);
VIPER_API void        (viper_pushnumber) (viper_State *L, viper_Number n);
VIPER_API void        (viper_pushinteger) (viper_State *L, viper_Integer n);
VIPER_API const char *(viper_pushlstring) (viper_State *L, const char *s, size_t len);
VIPER_API const char *(viper_pushstring) (viper_State *L, const char *s);
VIPER_API const char *(viper_pushvfstring) (viper_State *L, const char *fmt,
                                                      va_list argp);
VIPER_API const char *(viper_pushfstring) (viper_State *L, const char *fmt, ...);
VIPER_API void  (viper_pushcclosure) (viper_State *L, viper_CFunction fn, int n);
VIPER_API void  (viper_pushboolean) (viper_State *L, int b);
VIPER_API void  (viper_pushlightuserdata) (viper_State *L, void *p);
VIPER_API int   (viper_pushthread) (viper_State *L);


/*
** get functions (Viper -> stack)
*/
VIPER_API int (viper_getglobal) (viper_State *L, const char *name);
VIPER_API int (viper_gettable) (viper_State *L, int idx);
VIPER_API int (viper_getfield) (viper_State *L, int idx, const char *k);
VIPER_API int (viper_geti) (viper_State *L, int idx, viper_Integer n);
VIPER_API int (viper_rawget) (viper_State *L, int idx);
VIPER_API int (viper_rawgeti) (viper_State *L, int idx, viper_Integer n);
VIPER_API int (viper_rawgetp) (viper_State *L, int idx, const void *p);

VIPER_API void  (viper_createtable) (viper_State *L, int narr, int nrec);
VIPER_API void *(viper_newuserdatauv) (viper_State *L, size_t sz, int nuvalue);
VIPER_API int   (viper_getmetatable) (viper_State *L, int objindex);
VIPER_API int  (viper_getiuservalue) (viper_State *L, int idx, int n);


/*
** set functions (stack -> Viper)
*/
VIPER_API void  (viper_setglobal) (viper_State *L, const char *name);
VIPER_API void  (viper_settable) (viper_State *L, int idx);
VIPER_API void  (viper_setfield) (viper_State *L, int idx, const char *k);
VIPER_API void  (viper_seti) (viper_State *L, int idx, viper_Integer n);
VIPER_API void  (viper_rawset) (viper_State *L, int idx);
VIPER_API void  (viper_rawseti) (viper_State *L, int idx, viper_Integer n);
VIPER_API void  (viper_rawsetp) (viper_State *L, int idx, const void *p);
VIPER_API int   (viper_setmetatable) (viper_State *L, int objindex);
VIPER_API int   (viper_setiuservalue) (viper_State *L, int idx, int n);


/*
** 'load' and 'call' functions (load and run Viper code)
*/
VIPER_API void  (viper_callk) (viper_State *L, int nargs, int nresults,
                           viper_KContext ctx, viper_KFunction k);
#define viper_call(L,n,r)		viper_callk(L, (n), (r), 0, NULL)

VIPER_API int   (viper_pcallk) (viper_State *L, int nargs, int nresults, int errfunc,
                            viper_KContext ctx, viper_KFunction k);
#define viper_pcall(L,n,r,f)	viper_pcallk(L, (n), (r), (f), 0, NULL)

VIPER_API int   (viper_load) (viper_State *L, viper_Reader reader, void *dt,
                          const char *chunkname, const char *mode);

VIPER_API int (viper_dump) (viper_State *L, viper_Writer writer, void *data, int strip);


/*
** coroutine functions
*/
VIPER_API int  (viper_yieldk)     (viper_State *L, int nresults, viper_KContext ctx,
                               viper_KFunction k);
VIPER_API int  (viper_resume)     (viper_State *L, viper_State *from, int narg,
                               int *nres);
VIPER_API int  (viper_status)     (viper_State *L);
VIPER_API int (viper_isyieldable) (viper_State *L);

#define viper_yield(L,n)		viper_yieldk(L, (n), 0, NULL)


/*
** Warning-related functions
*/
VIPER_API void (viper_setwarnf) (viper_State *L, viper_WarnFunction f, void *ud);
VIPER_API void (viper_warning)  (viper_State *L, const char *msg, int tocont);


/*
** garbage-collection function and options
*/

#define VIPER_GCSTOP		0
#define VIPER_GCRESTART		1
#define VIPER_GCCOLLECT		2
#define VIPER_GCCOUNT		3
#define VIPER_GCCOUNTB		4
#define VIPER_GCSTEP		5
#define VIPER_GCSETPAUSE		6
#define VIPER_GCSETSTEPMUL	7
#define VIPER_GCISRUNNING		9
#define VIPER_GCGEN		10
#define VIPER_GCINC		11

VIPER_API int (viper_gc) (viper_State *L, int what, ...);


/*
** miscellaneous functions
*/

VIPER_API int   (viper_error) (viper_State *L);

VIPER_API int   (viper_next) (viper_State *L, int idx);

VIPER_API void  (viper_concat) (viper_State *L, int n);
VIPER_API void  (viper_len)    (viper_State *L, int idx);

VIPER_API size_t   (viper_stringtonumber) (viper_State *L, const char *s);

VIPER_API viper_Alloc (viper_getallocf) (viper_State *L, void **ud);
VIPER_API void      (viper_setallocf) (viper_State *L, viper_Alloc f, void *ud);

VIPER_API void (viper_toclose) (viper_State *L, int idx);
VIPER_API void (viper_closeslot) (viper_State *L, int idx);


/*
** {==============================================================
** some useful macros
** ===============================================================
*/

#define viper_getextraspace(L)	((void *)((char *)(L) - VIPER_EXTRASPACE))

#define viper_tonumber(L,i)	viper_tonumberx(L,(i),NULL)
#define viper_tointeger(L,i)	viper_tointegerx(L,(i),NULL)

#define viper_pop(L,n)		viper_settop(L, -(n)-1)

#define viper_newtable(L)		viper_createtable(L, 0, 0)

#define viper_register(L,n,f) (viper_pushcfunction(L, (f)), viper_setglobal(L, (n)))

#define viper_pushcfunction(L,f)	viper_pushcclosure(L, (f), 0)

#define viper_isfunction(L,n)	(viper_type(L, (n)) == VIPER_TFUNCTION)
#define viper_istable(L,n)	(viper_type(L, (n)) == VIPER_TTABLE)
#define viper_islightuserdata(L,n)	(viper_type(L, (n)) == VIPER_TLIGHTUSERDATA)
#define viper_isnil(L,n)		(viper_type(L, (n)) == VIPER_TNIL)
#define viper_isboolean(L,n)	(viper_type(L, (n)) == VIPER_TBOOLEAN)
#define viper_isthread(L,n)	(viper_type(L, (n)) == VIPER_TTHREAD)
#define viper_isnone(L,n)		(viper_type(L, (n)) == VIPER_TNONE)
#define viper_isnoneornil(L, n)	(viper_type(L, (n)) <= 0)

#define viper_pushliteral(L, s)	viper_pushstring(L, "" s)

#define viper_pushglobaltable(L)  \
	((void)viper_rawgeti(L, VIPER_REGISTRYINDEX, VIPER_RIDX_GLOBALS))

#define viper_tostring(L,i)	viper_tolstring(L, (i), NULL)


#define viper_insert(L,idx)	viper_rotate(L, (idx), 1)

#define viper_remove(L,idx)	(viper_rotate(L, (idx), -1), viper_pop(L, 1))

#define viper_replace(L,idx)	(viper_copy(L, -1, (idx)), viper_pop(L, 1))

/* }============================================================== */


/*
** {==============================================================
** compatibility macros
** ===============================================================
*/
#if defined(VIPER_COMPAT_APIINTCASTS)

#define viper_pushunsigned(L,n)	viper_pushinteger(L, (viper_Integer)(n))
#define viper_tounsignedx(L,i,is)	((viper_Unsigned)viper_tointegerx(L,i,is))
#define viper_tounsigned(L,i)	viper_tounsignedx(L,(i),NULL)

#endif

#define viper_newuserdata(L,s)	viper_newuserdatauv(L,s,1)
#define viper_getuservalue(L,idx)	viper_getiuservalue(L,idx,1)
#define viper_setuservalue(L,idx)	viper_setiuservalue(L,idx,1)

#define VIPER_NUMTAGS		VIPER_NUMTYPES

/* }============================================================== */

/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/
#define VIPER_HOOKCALL	0
#define VIPER_HOOKRET	1
#define VIPER_HOOKLINE	2
#define VIPER_HOOKCOUNT	3
#define VIPER_HOOKTAILCALL 4


/*
** Event masks
*/
#define VIPER_MASKCALL	(1 << VIPER_HOOKCALL)
#define VIPER_MASKRET	(1 << VIPER_HOOKRET)
#define VIPER_MASKLINE	(1 << VIPER_HOOKLINE)
#define VIPER_MASKCOUNT	(1 << VIPER_HOOKCOUNT)

typedef struct viper_Debug viper_Debug;  /* activation record */


/* Functions to be called by the debugger in specific events */
typedef void (*viper_Hook) (viper_State *L, viper_Debug *ar);


VIPER_API int (viper_getstack) (viper_State *L, int level, viper_Debug *ar);
VIPER_API int (viper_getinfo) (viper_State *L, const char *what, viper_Debug *ar);
VIPER_API const char *(viper_getlocal) (viper_State *L, const viper_Debug *ar, int n);
VIPER_API const char *(viper_setlocal) (viper_State *L, const viper_Debug *ar, int n);
VIPER_API const char *(viper_getupvalue) (viper_State *L, int funcindex, int n);
VIPER_API const char *(viper_setupvalue) (viper_State *L, int funcindex, int n);

VIPER_API void *(viper_upvalueid) (viper_State *L, int fidx, int n);
VIPER_API void  (viper_upvaluejoin) (viper_State *L, int fidx1, int n1,
                                               int fidx2, int n2);

VIPER_API void (viper_sethook) (viper_State *L, viper_Hook func, int mask, int count);
VIPER_API viper_Hook (viper_gethook) (viper_State *L);
VIPER_API int (viper_gethookmask) (viper_State *L);
VIPER_API int (viper_gethookcount) (viper_State *L);

VIPER_API int (viper_setcstacklimit) (viper_State *L, unsigned int limit);

struct viper_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) 'global', 'local', 'field', 'method' */
  const char *what;	/* (S) 'Viper', 'C', 'main', 'tail' */
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
  char short_src[VIPER_IDSIZE]; /* (S) */
  /* private part */
  struct CallInfo *i_ci;  /* active function */
};
#endif
