/*
** $Id: auxlib.c $
** Auxiliary functions for building Venom libraries
** See Copyright Notice in venom.h
*/

#define auxlib_c
#define VENOM_LIB

#include "prefix.h"


#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
** This file uses only the official API of Venom.
** Any function declared here could be written as an application function.
*/

#include "venom.h"

#include "auxlib.h"


#if !defined(MAX_SIZET)
/* maximum value for size_t */
#define MAX_SIZET	((size_t)(~(size_t)0))
#endif


/*
** {======================================================
** Traceback
** =======================================================
*/


#define LEVELS1	10	/* size of the first part of the stack */
#define LEVELS2	11	/* size of the second part of the stack */



/*
** Search for 'objidx' in table at index -1. ('objidx' must be an
** absolute index.) Return 1 + string at top if it found a Venomod name.
*/
static int findfield (venom_State *L, int objidx, int level) {
  if (level == 0 || !venom_istable(L, -1))
    return 0;  /* not found */
  venom_pushnil(L);  /* start 'next' loop */
  while (venom_next(L, -2)) {  /* for each pair in table */
    if (venom_type(L, -2) == VENOM_TSTRING) {  /* ignore non-string keys */
      if (venom_rawequal(L, objidx, -1)) {  /* found object? */
        venom_pop(L, 1);  /* remove value (but keep name) */
        return 1;
      }
      else if (findfield(L, objidx, level - 1)) {  /* try recursively */
        /* stack: lib_name, lib_table, field_name (top) */
        venom_pushliteral(L, ".");  /* place '.' between the two names */
        venom_replace(L, -3);  /* (in the slot ocvenomied by table) */
        venom_concat(L, 3);  /* lib_name.field_name */
        return 1;
      }
    }
    venom_pop(L, 1);  /* remove value */
  }
  return 0;  /* not found */
}


/*
** Search for a name for a function in all loaded modules
*/
static int pushglobalfuncname (venom_State *L, venom_Debug *ar) {
  int top = venom_gettop(L);
  venom_getinfo(L, "f", ar);  /* push function */
  venom_getfield(L, VENOM_REGISTRYINDEX, VENOM_LOADED_TABLE);
  if (findfield(L, top + 1, 2)) {
    const char *name = venom_tostring(L, -1);
    if (strncmp(name, VENOM_GNAME ".", 3) == 0) {  /* name start with '_G.'? */
      venom_pushstring(L, name + 3);  /* push name without prefix */
      venom_remove(L, -2);  /* remove original name */
    }
    venom_copy(L, -1, top + 1);  /* copy name to proper place */
    venom_settop(L, top + 1);  /* remove table "loaded" and name copy */
    return 1;
  }
  else {
    venom_settop(L, top);  /* remove function and global table */
    return 0;
  }
}


static void pushfuncname (venom_State *L, venom_Debug *ar) {
  if (pushglobalfuncname(L, ar)) {  /* try first a global name */
    venom_pushfstring(L, "function '%s'", venom_tostring(L, -1));
    venom_remove(L, -2);  /* remove name */
  }
  else if (*ar->namewhat != '\0')  /* is there a name from code? */
    venom_pushfstring(L, "%s '%s'", ar->namewhat, ar->name);  /* use it */
  else if (*ar->what == 'm')  /* main? */
      venom_pushliteral(L, "main chunk");
  else if (*ar->what != 'C')  /* for Venom functions, use <file:line> */
    venom_pushfstring(L, "function <%s:%d>", ar->short_src, ar->linedefined);
  else  /* nothing left... */
    venom_pushliteral(L, "?");
}


static int lastlevel (venom_State *L) {
  venom_Debug ar;
  int li = 1, le = 1;
  /* find an upper bound */
  while (venom_getstack(L, le, &ar)) { li = le; le *= 2; }
  /* do a binary search */
  while (li < le) {
    int m = (li + le)/2;
    if (venom_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}


VENOMLIB_API void venomL_traceback (venom_State *L, venom_State *L1,
                                const char *msg, int level) {
  venomL_Buffer b;
  venom_Debug ar;
  int last = lastlevel(L1);
  int limit2show = (last - level > LEVELS1 + LEVELS2) ? LEVELS1 : -1;
  venomL_buffinit(L, &b);
  if (msg) {
    venomL_addstring(&b, msg);
    venomL_addchar(&b, '\n');
  }
  venomL_addstring(&b, "stack traceback:");
  while (venom_getstack(L1, level++, &ar)) {
    if (limit2show-- == 0) {  /* too many levels? */
      int n = last - level - LEVELS2 + 1;  /* number of levels to skip */
      venom_pushfstring(L, "\n\t...\t(skipping %d levels)", n);
      venomL_addvalue(&b);  /* add warning about skip */
      level += n;  /* and skip to last levels */
    }
    else {
      venom_getinfo(L1, "Slnt", &ar);
      if (ar.currentline <= 0)
        venom_pushfstring(L, "\n\t%s: in ", ar.short_src);
      else
        venom_pushfstring(L, "\n\t%s:%d: in ", ar.short_src, ar.currentline);
      venomL_addvalue(&b);
      pushfuncname(L, &ar);
      venomL_addvalue(&b);
      if (ar.istailcall)
        venomL_addstring(&b, "\n\t(...tail calls...)");
    }
  }
  venomL_pushresult(&b);
}

/* }====================================================== */


/*
** {======================================================
** Error-report functions
** =======================================================
*/

VENOMLIB_API int venomL_argerror (venom_State *L, int arg, const char *extramsg) {
  venom_Debug ar;
  if (!venom_getstack(L, 0, &ar))  /* no stack frame? */
    return venomL_error(L, "bad argument #%d (%s)", arg, extramsg);
  venom_getinfo(L, "n", &ar);
  if (strcmp(ar.namewhat, "method") == 0) {
    arg--;  /* do not count 'self' */
    if (arg == 0)  /* error is in the self argument itself? */
      return venomL_error(L, "calling '%s' on bad self (%s)",
                           ar.name, extramsg);
  }
  if (ar.name == NULL)
    ar.name = (pushglobalfuncname(L, &ar)) ? venom_tostring(L, -1) : "?";
  return venomL_error(L, "bad argument #%d to '%s' (%s)",
                        arg, ar.name, extramsg);
}


VENOMLIB_API int venomL_typeerror (venom_State *L, int arg, const char *tname) {
  const char *msg;
  const char *typearg;  /* name for the type of the actual argument */
  if (venomL_getmetafield(L, arg, "__name") == VENOM_TSTRING)
    typearg = venom_tostring(L, -1);  /* use the given type name */
  else if (venom_type(L, arg) == VENOM_TLIGHTUSERDATA)
    typearg = "light userdata";  /* special name for messages */
  else
    typearg = venomL_typename(L, arg);  /* standard name */
  msg = venom_pushfstring(L, "%s expected, Venomt %s", tname, typearg);
  return venomL_argerror(L, arg, msg);
}


static void tag_error (venom_State *L, int arg, int tag) {
  venomL_typeerror(L, arg, venom_typename(L, tag));
}


/*
** The use of 'venom_pushfstring' ensures this function does not
** need reserved stack space when called.
*/
VENOMLIB_API void venomL_where (venom_State *L, int level) {
  venom_Debug ar;
  if (venom_getstack(L, level, &ar)) {  /* check function at level */
    venom_getinfo(L, "Sl", &ar);  /* get info about it */
    if (ar.currentline > 0) {  /* is there info? */
      venom_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline);
      return;
    }
  }
  venom_pushfstring(L, "");  /* else, no information available... */
}


/*
** Again, the use of 'venom_pushvfstring' ensures this function does
** not need reserved stack space when called. (At worst, it generates
** an error with "stack overflow" instead of the given message.)
*/
VENOMLIB_API int venomL_error (venom_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  venomL_where(L, 1);
  venom_pushvfstring(L, fmt, argp);
  va_end(argp);
  venom_concat(L, 2);
  return venom_error(L);
}


VENOMLIB_API int venomL_fileresult (venom_State *L, int stat, const char *fname) {
  int en = errno;  /* calls to Venom API may change this value */
  if (stat) {
    venom_pushboolean(L, 1);
    return 1;
  }
  else {
    venomL_pushfail(L);
    if (fname)
      venom_pushfstring(L, "%s: %s", fname, strerror(en));
    else
      venom_pushstring(L, strerror(en));
    venom_pushinteger(L, en);
    return 3;
  }
}


#if !defined(l_inspectstat)	/* { */

#if defined(VENOM_USE_POSIX)

#include <sys/wait.h>

/*
** use appropriate macros to interpret 'pclose' return status
*/
#define l_inspectstat(stat,what)  \
   if (WIFEXITED(stat)) { stat = WEXITSTATUS(stat); } \
   else if (WIFSIGNALED(stat)) { stat = WTERMSIG(stat); what = "signal"; }

#else

#define l_inspectstat(stat,what)  /* no op */

#endif

#endif				/* } */


VENOMLIB_API int venomL_execresult (venom_State *L, int stat) {
  if (stat != 0 && errno != 0)  /* error with an 'errno'? */
    return venomL_fileresult(L, 0, NULL);
  else {
    const char *what = "exit";  /* type of termination */
    l_inspectstat(stat, what);  /* interpret result */
    if (*what == 'e' && stat == 0)  /* successful termination? */
      venom_pushboolean(L, 1);
    else
      venomL_pushfail(L);
    venom_pushstring(L, what);
    venom_pushinteger(L, stat);
    return 3;  /* return true/fail,what,code */
  }
}

/* }====================================================== */



/*
** {======================================================
** Userdata's metatable manipulation
** =======================================================
*/

VENOMLIB_API int venomL_newmetatable (venom_State *L, const char *tname) {
  if (venomL_getmetatable(L, tname) != VENOM_TNIL)  /* name already in use? */
    return 0;  /* leave previous value on top, but return 0 */
  venom_pop(L, 1);
  venom_createtable(L, 0, 2);  /* create metatable */
  venom_pushstring(L, tname);
  venom_setfield(L, -2, "__name");  /* metatable.__name = tname */
  venom_pushvalue(L, -1);
  venom_setfield(L, VENOM_REGISTRYINDEX, tname);  /* registry.name = metatable */
  return 1;
}


VENOMLIB_API void venomL_setmetatable (venom_State *L, const char *tname) {
  venomL_getmetatable(L, tname);
  venom_setmetatable(L, -2);
}


VENOMLIB_API void *venomL_testudata (venom_State *L, int ud, const char *tname) {
  void *p = venom_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    if (venom_getmetatable(L, ud)) {  /* does it have a metatable? */
      venomL_getmetatable(L, tname);  /* get correct metatable */
      if (!venom_rawequal(L, -1, -2))  /* not the same? */
        p = NULL;  /* value is a userdata with wrong metatable */
      venom_pop(L, 2);  /* remove both metatables */
      return p;
    }
  }
  return NULL;  /* value is not a userdata with a metatable */
}


VENOMLIB_API void *venomL_checkudata (venom_State *L, int ud, const char *tname) {
  void *p = venomL_testudata(L, ud, tname);
  venomL_argexpected(L, p != NULL, ud, tname);
  return p;
}

/* }====================================================== */


/*
** {======================================================
** Argument check functions
** =======================================================
*/

VENOMLIB_API int venomL_checkoption (venom_State *L, int arg, const char *def,
                                 const char *const lst[]) {
  const char *name = (def) ? venomL_optstring(L, arg, def) :
                             venomL_checkstring(L, arg);
  int i;
  for (i=0; lst[i]; i++)
    if (strcmp(lst[i], name) == 0)
      return i;
  return venomL_argerror(L, arg,
                       venom_pushfstring(L, "invalid option '%s'", name));
}


/*
** Ensures the stack has at least 'space' extra slots, raising an error
** if it cannot fulfill the request. (The error handling needs a few
** extra slots to format the error message. In case of an error without
** this extra space, Venom will generate the same 'stack overflow' error,
** but without 'msg'.)
*/
VENOMLIB_API void venomL_checkstack (venom_State *L, int space, const char *msg) {
  if (l_unlikely(!venom_checkstack(L, space))) {
    if (msg)
      venomL_error(L, "stack overflow (%s)", msg);
    else
      venomL_error(L, "stack overflow");
  }
}


VENOMLIB_API void venomL_checktype (venom_State *L, int arg, int t) {
  if (l_unlikely(venom_type(L, arg) != t))
    tag_error(L, arg, t);
}


VENOMLIB_API void venomL_checkany (venom_State *L, int arg) {
  if (l_unlikely(venom_type(L, arg) == VENOM_TNONE))
    venomL_argerror(L, arg, "value expected");
}


VENOMLIB_API const char *venomL_checklstring (venom_State *L, int arg, size_t *len) {
  const char *s = venom_tolstring(L, arg, len);
  if (l_unlikely(!s)) tag_error(L, arg, VENOM_TSTRING);
  return s;
}


VENOMLIB_API const char *venomL_optlstring (venom_State *L, int arg,
                                        const char *def, size_t *len) {
  if (venom_isnoneornil(L, arg)) {
    if (len)
      *len = (def ? strlen(def) : 0);
    return def;
  }
  else return venomL_checklstring(L, arg, len);
}


VENOMLIB_API venom_Number venomL_checknumber (venom_State *L, int arg) {
  int isnum;
  venom_Number d = venom_tonumberx(L, arg, &isnum);
  if (l_unlikely(!isnum))
    tag_error(L, arg, VENOM_TNUMBER);
  return d;
}


VENOMLIB_API venom_Number venomL_optnumber (venom_State *L, int arg, venom_Number def) {
  return venomL_opt(L, venomL_checknumber, arg, def);
}


static void interror (venom_State *L, int arg) {
  if (venom_isnumber(L, arg))
    venomL_argerror(L, arg, "number has no integer representation");
  else
    tag_error(L, arg, VENOM_TNUMBER);
}


VENOMLIB_API venom_Integer venomL_checkinteger (venom_State *L, int arg) {
  int isnum;
  venom_Integer d = venom_tointegerx(L, arg, &isnum);
  if (l_unlikely(!isnum)) {
    interror(L, arg);
  }
  return d;
}


VENOMLIB_API venom_Integer venomL_optinteger (venom_State *L, int arg,
                                                      venom_Integer def) {
  return venomL_opt(L, venomL_checkinteger, arg, def);
}

/* }====================================================== */


/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

/* userdata to box arbitrary data */
typedef struct UBox {
  void *box;
  size_t bsize;
} UBox;


static void *resizebox (venom_State *L, int idx, size_t newsize) {
  void *ud;
  venom_Alloc allocf = venom_getallocf(L, &ud);
  UBox *box = (UBox *)venom_touserdata(L, idx);
  void *temp = allocf(ud, box->box, box->bsize, newsize);
  if (l_unlikely(temp == NULL && newsize > 0)) {  /* allocation error? */
    venom_pushliteral(L, "not enough memory");
    venom_error(L);  /* raise a memory error */
  }
  box->box = temp;
  box->bsize = newsize;
  return temp;
}


static int boxgc (venom_State *L) {
  resizebox(L, 1, 0);
  return 0;
}


static const venomL_Reg boxmt[] = {  /* box metamethods */
  {"__gc", boxgc},
  {"__close", boxgc},
  {NULL, NULL}
};


static void newbox (venom_State *L) {
  UBox *box = (UBox *)venom_newuserdatauv(L, sizeof(UBox), 0);
  box->box = NULL;
  box->bsize = 0;
  if (venomL_newmetatable(L, "_UBOX*"))  /* creating metatable? */
    venomL_setfuncs(L, boxmt, 0);  /* set its metamethods */
  venom_setmetatable(L, -2);
}


/*
** check whether buffer is using a userdata on the stack as a temporary
** buffer
*/
#define buffonstack(B)	((B)->b != (B)->init.b)


/*
** Whenever buffer is accessed, slot 'idx' must either be a box (which
** cannot be NULL) or it is a placeholder for the buffer.
*/
#define checkbufferlevel(B,idx)  \
  venom_assert(buffonstack(B) ? venom_touserdata(B->L, idx) != NULL  \
                            : venom_touserdata(B->L, idx) == (void*)B)


/*
** Compute new size for buffer 'B', enough to accommodate extra 'sz'
** bytes.
*/
static size_t newbuffsize (venomL_Buffer *B, size_t sz) {
  size_t newsize = B->size * 2;  /* double buffer size */
  if (l_unlikely(MAX_SIZET - sz < B->n))  /* overflow in (B->n + sz)? */
    return venomL_error(B->L, "buffer too large");
  if (newsize < B->n + sz)  /* double is not big enough? */
    newsize = B->n + sz;
  return newsize;
}


/*
** Returns a pointer to a free area with at least 'sz' bytes in buffer
** 'B'. 'boxidx' is the relative position in the stack where is the
** buffer's box or its placeholder.
*/
static char *prepbuffsize (venomL_Buffer *B, size_t sz, int boxidx) {
  checkbufferlevel(B, boxidx);
  if (B->size - B->n >= sz)  /* enough space? */
    return B->b + B->n;
  else {
    venom_State *L = B->L;
    char *newbuff;
    size_t newsize = newbuffsize(B, sz);
    /* create larger buffer */
    if (buffonstack(B))  /* buffer already has a box? */
      newbuff = (char *)resizebox(L, boxidx, newsize);  /* resize it */
    else {  /* no box yet */
      venom_remove(L, boxidx);  /* remove placeholder */
      newbox(L);  /* create a new box */
      venom_insert(L, boxidx);  /* move box to its intended position */
      venom_toclose(L, boxidx);
      newbuff = (char *)resizebox(L, boxidx, newsize);
      memcpy(newbuff, B->b, B->n * sizeof(char));  /* copy original content */
    }
    B->b = newbuff;
    B->size = newsize;
    return newbuff + B->n;
  }
}

/*
** returns a pointer to a free area with at least 'sz' bytes
*/
VENOMLIB_API char *venomL_prepbuffsize (venomL_Buffer *B, size_t sz) {
  return prepbuffsize(B, sz, -1);
}


VENOMLIB_API void venomL_addlstring (venomL_Buffer *B, const char *s, size_t l) {
  if (l > 0) {  /* avoid 'memcpy' when 's' can be NULL */
    char *b = prepbuffsize(B, l, -1);
    memcpy(b, s, l * sizeof(char));
    venomL_addsize(B, l);
  }
}


VENOMLIB_API void venomL_addstring (venomL_Buffer *B, const char *s) {
  venomL_addlstring(B, s, strlen(s));
}


VENOMLIB_API void venomL_pushresult (venomL_Buffer *B) {
  venom_State *L = B->L;
  checkbufferlevel(B, -1);
  venom_pushlstring(L, B->b, B->n);
  if (buffonstack(B))
    venom_closeslot(L, -2);  /* close the box */
  venom_remove(L, -2);  /* remove box or placeholder from the stack */
}


VENOMLIB_API void venomL_pushresultsize (venomL_Buffer *B, size_t sz) {
  venomL_addsize(B, sz);
  venomL_pushresult(B);
}


/*
** 'venomL_addvalue' is the only function in the Buffer system where the
** box (if existent) is not on the top of the stack. So, instead of
** calling 'venomL_addlstring', it replicates the code using -2 as the
** last argument to 'prepbuffsize', signaling that the box is (or will
** be) bellow the string being added to the buffer. (Box creation can
** trigger an emergency GC, so we should not remove the string from the
** stack before we have the space guaranteed.)
*/
VENOMLIB_API void venomL_addvalue (venomL_Buffer *B) {
  venom_State *L = B->L;
  size_t len;
  const char *s = venom_tolstring(L, -1, &len);
  char *b = prepbuffsize(B, len, -2);
  memcpy(b, s, len * sizeof(char));
  venomL_addsize(B, len);
  venom_pop(L, 1);  /* pop string */
}


VENOMLIB_API void venomL_buffinit (venom_State *L, venomL_Buffer *B) {
  B->L = L;
  B->b = B->init.b;
  B->n = 0;
  B->size = VENOML_BUFFERSIZE;
  venom_pushlightuserdata(L, (void*)B);  /* push placeholder */
}


VENOMLIB_API char *venomL_buffinitsize (venom_State *L, venomL_Buffer *B, size_t sz) {
  venomL_buffinit(L, B);
  return prepbuffsize(B, sz, -1);
}

/* }====================================================== */


/*
** {======================================================
** Reference system
** =======================================================
*/

/* index of free-list header (after the predefined values) */
#define freelist	(VENOM_RIDX_LAST + 1)

/*
** The previously freed references form a linked list:
** t[freelist] is the index of a first free index, or zero if list is
** empty; t[t[freelist]] is the index of the second element; etc.
*/
VENOMLIB_API int venomL_ref (venom_State *L, int t) {
  int ref;
  if (venom_isnil(L, -1)) {
    venom_pop(L, 1);  /* remove from stack */
    return VENOM_REFNIL;  /* 'nil' has a unique fixed reference */
  }
  t = venom_absindex(L, t);
  if (venom_rawgeti(L, t, freelist) == VENOM_TNIL) {  /* first access? */
    ref = 0;  /* list is empty */
    venom_pushinteger(L, 0);  /* initialize as an empty list */
    venom_rawseti(L, t, freelist);  /* ref = t[freelist] = 0 */
  }
  else {  /* already initialized */
    venom_assert(venom_isinteger(L, -1));
    ref = (int)venom_tointeger(L, -1);  /* ref = t[freelist] */
  }
  venom_pop(L, 1);  /* remove element from stack */
  if (ref != 0) {  /* any free element? */
    venom_rawgeti(L, t, ref);  /* remove it from list */
    venom_rawseti(L, t, freelist);  /* (t[freelist] = t[ref]) */
  }
  else  /* no free elements */
    ref = (int)venom_rawlen(L, t) + 1;  /* get a new reference */
  venom_rawseti(L, t, ref);
  return ref;
}


VENOMLIB_API void venomL_unref (venom_State *L, int t, int ref) {
  if (ref >= 0) {
    t = venom_absindex(L, t);
    venom_rawgeti(L, t, freelist);
    venom_assert(venom_isinteger(L, -1));
    venom_rawseti(L, t, ref);  /* t[ref] = t[freelist] */
    venom_pushinteger(L, ref);
    venom_rawseti(L, t, freelist);  /* t[freelist] = ref */
  }
}

/* }====================================================== */


/*
** {======================================================
** Load functions
** =======================================================
*/

typedef struct LoadF {
  int n;  /* number of pre-read characters */
  FILE *f;  /* file being read */
  char buff[BUFSIZ];  /* area for reading file */
} LoadF;


static const char *getF (venom_State *L, void *ud, size_t *size) {
  LoadF *lf = (LoadF *)ud;
  (void)L;  /* not used */
  if (lf->n > 0) {  /* are there pre-read characters to be read? */
    *size = lf->n;  /* return them (chars already in buffer) */
    lf->n = 0;  /* no more pre-read characters */
  }
  else {  /* read a block from file */
    /* 'fread' can return > 0 *and* set the EOF flag. If next call to
       'getF' called 'fread', it might still wait for user input.
       The next check avoids this problem. */
    if (feof(lf->f)) return NULL;
    *size = fread(lf->buff, 1, sizeof(lf->buff), lf->f);  /* read block */
  }
  return lf->buff;
}


static int errfile (venom_State *L, const char *what, int fnameindex) {
  const char *serr = strerror(errno);
  const char *filename = venom_tostring(L, fnameindex) + 1;
  venom_pushfstring(L, "cannot %s %s: %s", what, filename, serr);
  venom_remove(L, fnameindex);
  return VENOM_ERRFILE;
}


static int skipBOM (LoadF *lf) {
  const char *p = "\xEF\xBB\xBF";  /* UTF-8 BOM mark */
  int c;
  lf->n = 0;
  do {
    c = getc(lf->f);
    if (c == EOF || c != *(const unsigned char *)p++) return c;
    lf->buff[lf->n++] = c;  /* to be read by the parser */
  } while (*p != '\0');
  lf->n = 0;  /* prefix matched; discard it */
  return getc(lf->f);  /* return next character */
}


/*
** reads the first character of file 'f' and skips an optional BOM mark
** in its beginning plus its first line if it starts with '#'. Returns
** true if it skipped the first line.  In any case, '*cp' has the
** first "valid" character of the file (after the optional BOM and
** a first-line comment).
*/
static int skipcomment (LoadF *lf, int *cp) {
  int c = *cp = skipBOM(lf);
  if (c == '#') {  /* first line is a comment (Unix exec. file)? */
    do {  /* skip first line */
      c = getc(lf->f);
    } while (c != EOF && c != '\n');
    *cp = getc(lf->f);  /* skip end-of-line, if present */
    return 1;  /* there was a comment */
  }
  else return 0;  /* no comment */
}


VENOMLIB_API int venomL_loadfilex (venom_State *L, const char *filename,
                                             const char *mode) {
  LoadF lf;
  int status, readstatus;
  int c;
  int fnameindex = venom_gettop(L) + 1;  /* index of filename on the stack */
  if (filename == NULL) {
    venom_pushliteral(L, "=stdin");
    lf.f = stdin;
  }
  else {
    venom_pushfstring(L, "@%s", filename);
    lf.f = fopen(filename, "r");
    if (lf.f == NULL) return errfile(L, "open", fnameindex);
  }
  if (skipcomment(&lf, &c))  /* read initial portion */
    lf.buff[lf.n++] = '\n';  /* add line to correct line numbers */
  if (c == VENOM_SIGNATURE[0] && filename) {  /* binary file? */
    lf.f = freopen(filename, "rb", lf.f);  /* reopen in binary mode */
    if (lf.f == NULL) return errfile(L, "reopen", fnameindex);
    skipcomment(&lf, &c);  /* re-read initial portion */
  }
  if (c != EOF)
    lf.buff[lf.n++] = c;  /* 'c' is the first character of the stream */
  status = venom_load(L, getF, &lf, venom_tostring(L, -1), mode);
  readstatus = ferror(lf.f);
  if (filename) fclose(lf.f);  /* close file (even in case of errors) */
  if (readstatus) {
    venom_settop(L, fnameindex);  /* ignore results from 'venom_load' */
    return errfile(L, "read", fnameindex);
  }
  venom_remove(L, fnameindex);
  return status;
}


typedef struct LoadS {
  const char *s;
  size_t size;
} LoadS;


static const char *getS (venom_State *L, void *ud, size_t *size) {
  LoadS *ls = (LoadS *)ud;
  (void)L;  /* not used */
  if (ls->size == 0) return NULL;
  *size = ls->size;
  ls->size = 0;
  return ls->s;
}


VENOMLIB_API int venomL_loadbufferx (venom_State *L, const char *buff, size_t size,
                                 const char *name, const char *mode) {
  LoadS ls;
  ls.s = buff;
  ls.size = size;
  return venom_load(L, getS, &ls, name, mode);
}


VENOMLIB_API int venomL_loadstring (venom_State *L, const char *s) {
  return venomL_loadbuffer(L, s, strlen(s), s);
}

/* }====================================================== */



VENOMLIB_API int venomL_getmetafield (venom_State *L, int obj, const char *event) {
  if (!venom_getmetatable(L, obj))  /* no metatable? */
    return VENOM_TNIL;
  else {
    int tt;
    venom_pushstring(L, event);
    tt = venom_rawget(L, -2);
    if (tt == VENOM_TNIL)  /* is metafield nil? */
      venom_pop(L, 2);  /* remove metatable and metafield */
    else
      venom_remove(L, -2);  /* remove only metatable */
    return tt;  /* return metafield type */
  }
}


VENOMLIB_API int venomL_callmeta (venom_State *L, int obj, const char *event) {
  obj = venom_absindex(L, obj);
  if (venomL_getmetafield(L, obj, event) == VENOM_TNIL)  /* no metafield? */
    return 0;
  venom_pushvalue(L, obj);
  venom_call(L, 1, 1);
  return 1;
}


VENOMLIB_API venom_Integer venomL_len (venom_State *L, int idx) {
  venom_Integer l;
  int isnum;
  venom_len(L, idx);
  l = venom_tointegerx(L, -1, &isnum);
  if (l_unlikely(!isnum))
    venomL_error(L, "object length is not an integer");
  venom_pop(L, 1);  /* remove object */
  return l;
}


VENOMLIB_API const char *venomL_tolstring (venom_State *L, int idx, size_t *len) {
  idx = venom_absindex(L,idx);
  if (venomL_callmeta(L, idx, "__tostring")) {  /* metafield? */
    if (!venom_isstring(L, -1))
      venomL_error(L, "'__tostring' must return a string");
  }
  else {
    switch (venom_type(L, idx)) {
      case VENOM_TNUMBER: {
        if (venom_isinteger(L, idx))
          venom_pushfstring(L, "%I", (VENOMI_UACINT)venom_tointeger(L, idx));
        else
          venom_pushfstring(L, "%f", (VENOMI_UACNUMBER)venom_tonumber(L, idx));
        break;
      }
      case VENOM_TSTRING:
        venom_pushvalue(L, idx);
        break;
      case VENOM_TBOOLEAN:
        venom_pushstring(L, (venom_toboolean(L, idx) ? "true" : "false"));
        break;
      case VENOM_TNIL:
        venom_pushliteral(L, "nil");
        break;
      default: {
        int tt = venomL_getmetafield(L, idx, "__name");  /* try name */
        const char *kind = (tt == VENOM_TSTRING) ? venom_tostring(L, -1) :
                                                 venomL_typename(L, idx);
        venom_pushfstring(L, "%s: %p", kind, venom_topointer(L, idx));
        if (tt != VENOM_TNIL)
          venom_remove(L, -2);  /* remove '__name' */
        break;
      }
    }
  }
  return venom_tolstring(L, -1, len);
}


/*
** set functions from list 'l' into table at top - 'nup'; each
** function gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/
VENOMLIB_API void venomL_setfuncs (venom_State *L, const venomL_Reg *l, int nup) {
  venomL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    if (l->func == NULL)  /* place holder? */
      venom_pushboolean(L, 0);
    else {
      int i;
      for (i = 0; i < nup; i++)  /* copy upvalues to the top */
        venom_pushvalue(L, -nup);
      venom_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    }
    venom_setfield(L, -(nup + 2), l->name);
  }
  venom_pop(L, nup);  /* remove upvalues */
}


/*
** ensure that stack[idx][fname] has a table and push that table
** into the stack
*/
VENOMLIB_API int venomL_getsubtable (venom_State *L, int idx, const char *fname) {
  if (venom_getfield(L, idx, fname) == VENOM_TTABLE)
    return 1;  /* table already there */
  else {
    venom_pop(L, 1);  /* remove previous result */
    idx = venom_absindex(L, idx);
    venom_newtable(L);
    venom_pushvalue(L, -1);  /* copy to be left at top */
    venom_setfield(L, idx, fname);  /* assign new table to field */
    return 0;  /* false, because did not find table there */
  }
}


/*
** Stripped-down 'require': After checking "loaded" table, calls 'openf'
** to open a module, registers the result in 'package.loaded' table and,
** if 'glb' is true, also registers the result in the global table.
** Leaves resulting module on the top.
*/
VENOMLIB_API void venomL_requiref (venom_State *L, const char *modname,
                               venom_CFunction openf, int glb) {
  venomL_getsubtable(L, VENOM_REGISTRYINDEX, VENOM_LOADED_TABLE);
  venom_getfield(L, -1, modname);  /* LOADED[modname] */
  if (!venom_toboolean(L, -1)) {  /* package not already loaded? */
    venom_pop(L, 1);  /* remove field */
    venom_pushcfunction(L, openf);
    venom_pushstring(L, modname);  /* argument to open function */
    venom_call(L, 1, 1);  /* call 'openf' to open module */
    venom_pushvalue(L, -1);  /* make copy of module (call result) */
    venom_setfield(L, -3, modname);  /* LOADED[modname] = module */
  }
  venom_remove(L, -2);  /* remove LOADED table */
  if (glb) {
    venom_pushvalue(L, -1);  /* copy of module */
    venom_setglobal(L, modname);  /* _G[modname] = module */
  }
}


VENOMLIB_API void venomL_addgsub (venomL_Buffer *b, const char *s,
                                     const char *p, const char *r) {
  const char *wild;
  size_t l = strlen(p);
  while ((wild = strstr(s, p)) != NULL) {
    venomL_addlstring(b, s, wild - s);  /* push prefix */
    venomL_addstring(b, r);  /* push replacement in place of pattern */
    s = wild + l;  /* continue after 'p' */
  }
  venomL_addstring(b, s);  /* push last suffix */
}


VENOMLIB_API const char *venomL_gsub (venom_State *L, const char *s,
                                  const char *p, const char *r) {
  venomL_Buffer b;
  venomL_buffinit(L, &b);
  venomL_addgsub(&b, s, p, r);
  venomL_pushresult(&b);
  return venom_tostring(L, -1);
}


static void *l_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud; (void)osize;  /* not used */
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  else
    return realloc(ptr, nsize);
}


static int panic (venom_State *L) {
  const char *msg = venom_tostring(L, -1);
  if (msg == NULL) msg = "error object is not a string";
  venom_writestringerror("PANIC: unprotected error in call to Venom API (%s)\n",
                        msg);
  return 0;  /* return to Venom to abort */
}


/*
** Warning functions:
** warnfoff: warning system is off
** warnfon: ready to start a new message
** warnfcont: previous message is to be continued
*/
static void warnfoff (void *ud, const char *message, int tocont);
static void warnfon (void *ud, const char *message, int tocont);
static void warnfcont (void *ud, const char *message, int tocont);


/*
** Check whether message is a control message. If so, execute the
** control or ignore it if unknown.
*/
static int checkcontrol (venom_State *L, const char *message, int tocont) {
  if (tocont || *(message++) != '@')  /* not a control message? */
    return 0;
  else {
    if (strcmp(message, "off") == 0)
      venom_setwarnf(L, warnfoff, L);  /* turn warnings off */
    else if (strcmp(message, "on") == 0)
      venom_setwarnf(L, warnfon, L);   /* turn warnings on */
    return 1;  /* it was a control message */
  }
}


static void warnfoff (void *ud, const char *message, int tocont) {
  checkcontrol((venom_State *)ud, message, tocont);
}


/*
** Writes the message and handle 'tocont', finishing the message
** if needed and setting the next warn function.
*/
static void warnfcont (void *ud, const char *message, int tocont) {
  venom_State *L = (venom_State *)ud;
  venom_writestringerror("%s", message);  /* write message */
  if (tocont)  /* not the last part? */
    venom_setwarnf(L, warnfcont, L);  /* to be continued */
  else {  /* last part */
    venom_writestringerror("%s", "\n");  /* finish message with end-of-line */
    venom_setwarnf(L, warnfon, L);  /* next call is a new message */
  }
}


static void warnfon (void *ud, const char *message, int tocont) {
  if (checkcontrol((venom_State *)ud, message, tocont))  /* control message? */
    return;  /* nothing else to be done */
  venom_writestringerror("%s", "Venom warning: ");  /* start a new warning */
  warnfcont(ud, message, tocont);  /* finish processing */
}


VENOMLIB_API venom_State *venomL_newstate (void) {
  venom_State *L = venom_newstate(l_alloc, NULL);
  if (l_likely(L)) {
    venom_atpanic(L, &panic);
    venom_setwarnf(L, warnfoff, L);  /* default is warnings off */
  }
  return L;
}


VENOMLIB_API void venomL_checkversion_ (venom_State *L, venom_Number ver, size_t sz) {
  venom_Number v = venom_version(L);
  if (sz != VENOML_NUMSIZES)  /* check numeric types */
    venomL_error(L, "core and library have incompatible numeric types");
  else if (v != ver)
    venomL_error(L, "version mismatch: app. needs %f, Venom core provides %f",
                  (VENOMI_UACNUMBER)ver, (VENOMI_UACNUMBER)v);
}