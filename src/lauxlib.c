/*
** $Id: lauxlib.c $
** Auxiliary functions for building Viper libraries
** See Copyright Notice in viper.h
*/

#define lauxlib_c
#define VIPER_LIB

#include "lprefix.h"


#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
** This file uses only the official API of Viper.
** Any function declared here could be written as an application function.
*/

#include "viper.h"

#include "lauxlib.h"


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
** absolute index.) Return 1 + string at top if it found a Viperod name.
*/
static int findfield (viper_State *L, int objidx, int level) {
  if (level == 0 || !viper_istable(L, -1))
    return 0;  /* not found */
  viper_pushnil(L);  /* start 'next' loop */
  while (viper_next(L, -2)) {  /* for each pair in table */
    if (viper_type(L, -2) == VIPER_TSTRING) {  /* ignore non-string keys */
      if (viper_rawequal(L, objidx, -1)) {  /* found object? */
        viper_pop(L, 1);  /* remove value (but keep name) */
        return 1;
      }
      else if (findfield(L, objidx, level - 1)) {  /* try recursively */
        /* stack: lib_name, lib_table, field_name (top) */
        viper_pushliteral(L, ".");  /* place '.' between the two names */
        viper_replace(L, -3);  /* (in the slot ocviperied by table) */
        viper_concat(L, 3);  /* lib_name.field_name */
        return 1;
      }
    }
    viper_pop(L, 1);  /* remove value */
  }
  return 0;  /* not found */
}


/*
** Search for a name for a function in all loaded modules
*/
static int pushglobalfuncname (viper_State *L, viper_Debug *ar) {
  int top = viper_gettop(L);
  viper_getinfo(L, "f", ar);  /* push function */
  viper_getfield(L, VIPER_REGISTRYINDEX, VIPER_LOADED_TABLE);
  if (findfield(L, top + 1, 2)) {
    const char *name = viper_tostring(L, -1);
    if (strncmp(name, VIPER_GNAME ".", 3) == 0) {  /* name start with '_G.'? */
      viper_pushstring(L, name + 3);  /* push name without prefix */
      viper_remove(L, -2);  /* remove original name */
    }
    viper_copy(L, -1, top + 1);  /* copy name to proper place */
    viper_settop(L, top + 1);  /* remove table "loaded" and name copy */
    return 1;
  }
  else {
    viper_settop(L, top);  /* remove function and global table */
    return 0;
  }
}


static void pushfuncname (viper_State *L, viper_Debug *ar) {
  if (pushglobalfuncname(L, ar)) {  /* try first a global name */
    viper_pushfstring(L, "function '%s'", viper_tostring(L, -1));
    viper_remove(L, -2);  /* remove name */
  }
  else if (*ar->namewhat != '\0')  /* is there a name from code? */
    viper_pushfstring(L, "%s '%s'", ar->namewhat, ar->name);  /* use it */
  else if (*ar->what == 'm')  /* main? */
      viper_pushliteral(L, "main chunk");
  else if (*ar->what != 'C')  /* for Viper functions, use <file:line> */
    viper_pushfstring(L, "function <%s:%d>", ar->short_src, ar->linedefined);
  else  /* nothing left... */
    viper_pushliteral(L, "?");
}


static int lastlevel (viper_State *L) {
  viper_Debug ar;
  int li = 1, le = 1;
  /* find an upper bound */
  while (viper_getstack(L, le, &ar)) { li = le; le *= 2; }
  /* do a binary search */
  while (li < le) {
    int m = (li + le)/2;
    if (viper_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}


VIPERLIB_API void viperL_traceback (viper_State *L, viper_State *L1,
                                const char *msg, int level) {
  viperL_Buffer b;
  viper_Debug ar;
  int last = lastlevel(L1);
  int limit2show = (last - level > LEVELS1 + LEVELS2) ? LEVELS1 : -1;
  viperL_buffinit(L, &b);
  if (msg) {
    viperL_addstring(&b, msg);
    viperL_addchar(&b, '\n');
  }
  viperL_addstring(&b, "stack traceback:");
  while (viper_getstack(L1, level++, &ar)) {
    if (limit2show-- == 0) {  /* too many levels? */
      int n = last - level - LEVELS2 + 1;  /* number of levels to skip */
      viper_pushfstring(L, "\n\t...\t(skipping %d levels)", n);
      viperL_addvalue(&b);  /* add warning about skip */
      level += n;  /* and skip to last levels */
    }
    else {
      viper_getinfo(L1, "Slnt", &ar);
      if (ar.currentline <= 0)
        viper_pushfstring(L, "\n\t%s: in ", ar.short_src);
      else
        viper_pushfstring(L, "\n\t%s:%d: in ", ar.short_src, ar.currentline);
      viperL_addvalue(&b);
      pushfuncname(L, &ar);
      viperL_addvalue(&b);
      if (ar.istailcall)
        viperL_addstring(&b, "\n\t(...tail calls...)");
    }
  }
  viperL_pushresult(&b);
}

/* }====================================================== */


/*
** {======================================================
** Error-report functions
** =======================================================
*/

VIPERLIB_API int viperL_argerror (viper_State *L, int arg, const char *extramsg) {
  viper_Debug ar;
  if (!viper_getstack(L, 0, &ar))  /* no stack frame? */
    return viperL_error(L, "bad argument #%d (%s)", arg, extramsg);
  viper_getinfo(L, "n", &ar);
  if (strcmp(ar.namewhat, "method") == 0) {
    arg--;  /* do not count 'self' */
    if (arg == 0)  /* error is in the self argument itself? */
      return viperL_error(L, "calling '%s' on bad self (%s)",
                           ar.name, extramsg);
  }
  if (ar.name == NULL)
    ar.name = (pushglobalfuncname(L, &ar)) ? viper_tostring(L, -1) : "?";
  return viperL_error(L, "bad argument #%d to '%s' (%s)",
                        arg, ar.name, extramsg);
}


VIPERLIB_API int viperL_typeerror (viper_State *L, int arg, const char *tname) {
  const char *msg;
  const char *typearg;  /* name for the type of the actual argument */
  if (viperL_getmetafield(L, arg, "__name") == VIPER_TSTRING)
    typearg = viper_tostring(L, -1);  /* use the given type name */
  else if (viper_type(L, arg) == VIPER_TLIGHTUSERDATA)
    typearg = "light userdata";  /* special name for messages */
  else
    typearg = viperL_typename(L, arg);  /* standard name */
  msg = viper_pushfstring(L, "%s expected, Vipert %s", tname, typearg);
  return viperL_argerror(L, arg, msg);
}


static void tag_error (viper_State *L, int arg, int tag) {
  viperL_typeerror(L, arg, viper_typename(L, tag));
}


/*
** The use of 'viper_pushfstring' ensures this function does not
** need reserved stack space when called.
*/
VIPERLIB_API void viperL_where (viper_State *L, int level) {
  viper_Debug ar;
  if (viper_getstack(L, level, &ar)) {  /* check function at level */
    viper_getinfo(L, "Sl", &ar);  /* get info about it */
    if (ar.currentline > 0) {  /* is there info? */
      viper_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline);
      return;
    }
  }
  viper_pushfstring(L, "");  /* else, no information available... */
}


/*
** Again, the use of 'viper_pushvfstring' ensures this function does
** not need reserved stack space when called. (At worst, it generates
** an error with "stack overflow" instead of the given message.)
*/
VIPERLIB_API int viperL_error (viper_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  viperL_where(L, 1);
  viper_pushvfstring(L, fmt, argp);
  va_end(argp);
  viper_concat(L, 2);
  return viper_error(L);
}


VIPERLIB_API int viperL_fileresult (viper_State *L, int stat, const char *fname) {
  int en = errno;  /* calls to Viper API may change this value */
  if (stat) {
    viper_pushboolean(L, 1);
    return 1;
  }
  else {
    viperL_pushfail(L);
    if (fname)
      viper_pushfstring(L, "%s: %s", fname, strerror(en));
    else
      viper_pushstring(L, strerror(en));
    viper_pushinteger(L, en);
    return 3;
  }
}


#if !defined(l_inspectstat)	/* { */

#if defined(VIPER_USE_POSIX)

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


VIPERLIB_API int viperL_execresult (viper_State *L, int stat) {
  if (stat != 0 && errno != 0)  /* error with an 'errno'? */
    return viperL_fileresult(L, 0, NULL);
  else {
    const char *what = "exit";  /* type of termination */
    l_inspectstat(stat, what);  /* interpret result */
    if (*what == 'e' && stat == 0)  /* successful termination? */
      viper_pushboolean(L, 1);
    else
      viperL_pushfail(L);
    viper_pushstring(L, what);
    viper_pushinteger(L, stat);
    return 3;  /* return true/fail,what,code */
  }
}

/* }====================================================== */



/*
** {======================================================
** Userdata's metatable manipulation
** =======================================================
*/

VIPERLIB_API int viperL_newmetatable (viper_State *L, const char *tname) {
  if (viperL_getmetatable(L, tname) != VIPER_TNIL)  /* name already in use? */
    return 0;  /* leave previous value on top, but return 0 */
  viper_pop(L, 1);
  viper_createtable(L, 0, 2);  /* create metatable */
  viper_pushstring(L, tname);
  viper_setfield(L, -2, "__name");  /* metatable.__name = tname */
  viper_pushvalue(L, -1);
  viper_setfield(L, VIPER_REGISTRYINDEX, tname);  /* registry.name = metatable */
  return 1;
}


VIPERLIB_API void viperL_setmetatable (viper_State *L, const char *tname) {
  viperL_getmetatable(L, tname);
  viper_setmetatable(L, -2);
}


VIPERLIB_API void *viperL_testudata (viper_State *L, int ud, const char *tname) {
  void *p = viper_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    if (viper_getmetatable(L, ud)) {  /* does it have a metatable? */
      viperL_getmetatable(L, tname);  /* get correct metatable */
      if (!viper_rawequal(L, -1, -2))  /* not the same? */
        p = NULL;  /* value is a userdata with wrong metatable */
      viper_pop(L, 2);  /* remove both metatables */
      return p;
    }
  }
  return NULL;  /* value is not a userdata with a metatable */
}


VIPERLIB_API void *viperL_checkudata (viper_State *L, int ud, const char *tname) {
  void *p = viperL_testudata(L, ud, tname);
  viperL_argexpected(L, p != NULL, ud, tname);
  return p;
}

/* }====================================================== */


/*
** {======================================================
** Argument check functions
** =======================================================
*/

VIPERLIB_API int viperL_checkoption (viper_State *L, int arg, const char *def,
                                 const char *const lst[]) {
  const char *name = (def) ? viperL_optstring(L, arg, def) :
                             viperL_checkstring(L, arg);
  int i;
  for (i=0; lst[i]; i++)
    if (strcmp(lst[i], name) == 0)
      return i;
  return viperL_argerror(L, arg,
                       viper_pushfstring(L, "invalid option '%s'", name));
}


/*
** Ensures the stack has at least 'space' extra slots, raising an error
** if it cannot fulfill the request. (The error handling needs a few
** extra slots to format the error message. In case of an error without
** this extra space, Viper will generate the same 'stack overflow' error,
** but without 'msg'.)
*/
VIPERLIB_API void viperL_checkstack (viper_State *L, int space, const char *msg) {
  if (l_unlikely(!viper_checkstack(L, space))) {
    if (msg)
      viperL_error(L, "stack overflow (%s)", msg);
    else
      viperL_error(L, "stack overflow");
  }
}


VIPERLIB_API void viperL_checktype (viper_State *L, int arg, int t) {
  if (l_unlikely(viper_type(L, arg) != t))
    tag_error(L, arg, t);
}


VIPERLIB_API void viperL_checkany (viper_State *L, int arg) {
  if (l_unlikely(viper_type(L, arg) == VIPER_TNONE))
    viperL_argerror(L, arg, "value expected");
}


VIPERLIB_API const char *viperL_checklstring (viper_State *L, int arg, size_t *len) {
  const char *s = viper_tolstring(L, arg, len);
  if (l_unlikely(!s)) tag_error(L, arg, VIPER_TSTRING);
  return s;
}


VIPERLIB_API const char *viperL_optlstring (viper_State *L, int arg,
                                        const char *def, size_t *len) {
  if (viper_isnoneornil(L, arg)) {
    if (len)
      *len = (def ? strlen(def) : 0);
    return def;
  }
  else return viperL_checklstring(L, arg, len);
}


VIPERLIB_API viper_Number viperL_checknumber (viper_State *L, int arg) {
  int isnum;
  viper_Number d = viper_tonumberx(L, arg, &isnum);
  if (l_unlikely(!isnum))
    tag_error(L, arg, VIPER_TNUMBER);
  return d;
}


VIPERLIB_API viper_Number viperL_optnumber (viper_State *L, int arg, viper_Number def) {
  return viperL_opt(L, viperL_checknumber, arg, def);
}


static void interror (viper_State *L, int arg) {
  if (viper_isnumber(L, arg))
    viperL_argerror(L, arg, "number has no integer representation");
  else
    tag_error(L, arg, VIPER_TNUMBER);
}


VIPERLIB_API viper_Integer viperL_checkinteger (viper_State *L, int arg) {
  int isnum;
  viper_Integer d = viper_tointegerx(L, arg, &isnum);
  if (l_unlikely(!isnum)) {
    interror(L, arg);
  }
  return d;
}


VIPERLIB_API viper_Integer viperL_optinteger (viper_State *L, int arg,
                                                      viper_Integer def) {
  return viperL_opt(L, viperL_checkinteger, arg, def);
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


static void *resizebox (viper_State *L, int idx, size_t newsize) {
  void *ud;
  viper_Alloc allocf = viper_getallocf(L, &ud);
  UBox *box = (UBox *)viper_touserdata(L, idx);
  void *temp = allocf(ud, box->box, box->bsize, newsize);
  if (l_unlikely(temp == NULL && newsize > 0)) {  /* allocation error? */
    viper_pushliteral(L, "not enough memory");
    viper_error(L);  /* raise a memory error */
  }
  box->box = temp;
  box->bsize = newsize;
  return temp;
}


static int boxgc (viper_State *L) {
  resizebox(L, 1, 0);
  return 0;
}


static const viperL_Reg boxmt[] = {  /* box metamethods */
  {"__gc", boxgc},
  {"__close", boxgc},
  {NULL, NULL}
};


static void newbox (viper_State *L) {
  UBox *box = (UBox *)viper_newuserdatauv(L, sizeof(UBox), 0);
  box->box = NULL;
  box->bsize = 0;
  if (viperL_newmetatable(L, "_UBOX*"))  /* creating metatable? */
    viperL_setfuncs(L, boxmt, 0);  /* set its metamethods */
  viper_setmetatable(L, -2);
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
  viper_assert(buffonstack(B) ? viper_touserdata(B->L, idx) != NULL  \
                            : viper_touserdata(B->L, idx) == (void*)B)


/*
** Compute new size for buffer 'B', enough to accommodate extra 'sz'
** bytes.
*/
static size_t newbuffsize (viperL_Buffer *B, size_t sz) {
  size_t newsize = B->size * 2;  /* double buffer size */
  if (l_unlikely(MAX_SIZET - sz < B->n))  /* overflow in (B->n + sz)? */
    return viperL_error(B->L, "buffer too large");
  if (newsize < B->n + sz)  /* double is not big enough? */
    newsize = B->n + sz;
  return newsize;
}


/*
** Returns a pointer to a free area with at least 'sz' bytes in buffer
** 'B'. 'boxidx' is the relative position in the stack where is the
** buffer's box or its placeholder.
*/
static char *prepbuffsize (viperL_Buffer *B, size_t sz, int boxidx) {
  checkbufferlevel(B, boxidx);
  if (B->size - B->n >= sz)  /* enough space? */
    return B->b + B->n;
  else {
    viper_State *L = B->L;
    char *newbuff;
    size_t newsize = newbuffsize(B, sz);
    /* create larger buffer */
    if (buffonstack(B))  /* buffer already has a box? */
      newbuff = (char *)resizebox(L, boxidx, newsize);  /* resize it */
    else {  /* no box yet */
      viper_remove(L, boxidx);  /* remove placeholder */
      newbox(L);  /* create a new box */
      viper_insert(L, boxidx);  /* move box to its intended position */
      viper_toclose(L, boxidx);
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
VIPERLIB_API char *viperL_prepbuffsize (viperL_Buffer *B, size_t sz) {
  return prepbuffsize(B, sz, -1);
}


VIPERLIB_API void viperL_addlstring (viperL_Buffer *B, const char *s, size_t l) {
  if (l > 0) {  /* avoid 'memcpy' when 's' can be NULL */
    char *b = prepbuffsize(B, l, -1);
    memcpy(b, s, l * sizeof(char));
    viperL_addsize(B, l);
  }
}


VIPERLIB_API void viperL_addstring (viperL_Buffer *B, const char *s) {
  viperL_addlstring(B, s, strlen(s));
}


VIPERLIB_API void viperL_pushresult (viperL_Buffer *B) {
  viper_State *L = B->L;
  checkbufferlevel(B, -1);
  viper_pushlstring(L, B->b, B->n);
  if (buffonstack(B))
    viper_closeslot(L, -2);  /* close the box */
  viper_remove(L, -2);  /* remove box or placeholder from the stack */
}


VIPERLIB_API void viperL_pushresultsize (viperL_Buffer *B, size_t sz) {
  viperL_addsize(B, sz);
  viperL_pushresult(B);
}


/*
** 'viperL_addvalue' is the only function in the Buffer system where the
** box (if existent) is not on the top of the stack. So, instead of
** calling 'viperL_addlstring', it replicates the code using -2 as the
** last argument to 'prepbuffsize', signaling that the box is (or will
** be) bellow the string being added to the buffer. (Box creation can
** trigger an emergency GC, so we should not remove the string from the
** stack before we have the space guaranteed.)
*/
VIPERLIB_API void viperL_addvalue (viperL_Buffer *B) {
  viper_State *L = B->L;
  size_t len;
  const char *s = viper_tolstring(L, -1, &len);
  char *b = prepbuffsize(B, len, -2);
  memcpy(b, s, len * sizeof(char));
  viperL_addsize(B, len);
  viper_pop(L, 1);  /* pop string */
}


VIPERLIB_API void viperL_buffinit (viper_State *L, viperL_Buffer *B) {
  B->L = L;
  B->b = B->init.b;
  B->n = 0;
  B->size = VIPERL_BUFFERSIZE;
  viper_pushlightuserdata(L, (void*)B);  /* push placeholder */
}


VIPERLIB_API char *viperL_buffinitsize (viper_State *L, viperL_Buffer *B, size_t sz) {
  viperL_buffinit(L, B);
  return prepbuffsize(B, sz, -1);
}

/* }====================================================== */


/*
** {======================================================
** Reference system
** =======================================================
*/

/* index of free-list header (after the predefined values) */
#define freelist	(VIPER_RIDX_LAST + 1)

/*
** The previously freed references form a linked list:
** t[freelist] is the index of a first free index, or zero if list is
** empty; t[t[freelist]] is the index of the second element; etc.
*/
VIPERLIB_API int viperL_ref (viper_State *L, int t) {
  int ref;
  if (viper_isnil(L, -1)) {
    viper_pop(L, 1);  /* remove from stack */
    return VIPER_REFNIL;  /* 'nil' has a unique fixed reference */
  }
  t = viper_absindex(L, t);
  if (viper_rawgeti(L, t, freelist) == VIPER_TNIL) {  /* first access? */
    ref = 0;  /* list is empty */
    viper_pushinteger(L, 0);  /* initialize as an empty list */
    viper_rawseti(L, t, freelist);  /* ref = t[freelist] = 0 */
  }
  else {  /* already initialized */
    viper_assert(viper_isinteger(L, -1));
    ref = (int)viper_tointeger(L, -1);  /* ref = t[freelist] */
  }
  viper_pop(L, 1);  /* remove element from stack */
  if (ref != 0) {  /* any free element? */
    viper_rawgeti(L, t, ref);  /* remove it from list */
    viper_rawseti(L, t, freelist);  /* (t[freelist] = t[ref]) */
  }
  else  /* no free elements */
    ref = (int)viper_rawlen(L, t) + 1;  /* get a new reference */
  viper_rawseti(L, t, ref);
  return ref;
}


VIPERLIB_API void viperL_unref (viper_State *L, int t, int ref) {
  if (ref >= 0) {
    t = viper_absindex(L, t);
    viper_rawgeti(L, t, freelist);
    viper_assert(viper_isinteger(L, -1));
    viper_rawseti(L, t, ref);  /* t[ref] = t[freelist] */
    viper_pushinteger(L, ref);
    viper_rawseti(L, t, freelist);  /* t[freelist] = ref */
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


static const char *getF (viper_State *L, void *ud, size_t *size) {
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


static int errfile (viper_State *L, const char *what, int fnameindex) {
  const char *serr = strerror(errno);
  const char *filename = viper_tostring(L, fnameindex) + 1;
  viper_pushfstring(L, "cannot %s %s: %s", what, filename, serr);
  viper_remove(L, fnameindex);
  return VIPER_ERRFILE;
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


VIPERLIB_API int viperL_loadfilex (viper_State *L, const char *filename,
                                             const char *mode) {
  LoadF lf;
  int status, readstatus;
  int c;
  int fnameindex = viper_gettop(L) + 1;  /* index of filename on the stack */
  if (filename == NULL) {
    viper_pushliteral(L, "=stdin");
    lf.f = stdin;
  }
  else {
    viper_pushfstring(L, "@%s", filename);
    lf.f = fopen(filename, "r");
    if (lf.f == NULL) return errfile(L, "open", fnameindex);
  }
  if (skipcomment(&lf, &c))  /* read initial portion */
    lf.buff[lf.n++] = '\n';  /* add line to correct line numbers */
  if (c == VIPER_SIGNATURE[0] && filename) {  /* binary file? */
    lf.f = freopen(filename, "rb", lf.f);  /* reopen in binary mode */
    if (lf.f == NULL) return errfile(L, "reopen", fnameindex);
    skipcomment(&lf, &c);  /* re-read initial portion */
  }
  if (c != EOF)
    lf.buff[lf.n++] = c;  /* 'c' is the first character of the stream */
  status = viper_load(L, getF, &lf, viper_tostring(L, -1), mode);
  readstatus = ferror(lf.f);
  if (filename) fclose(lf.f);  /* close file (even in case of errors) */
  if (readstatus) {
    viper_settop(L, fnameindex);  /* ignore results from 'viper_load' */
    return errfile(L, "read", fnameindex);
  }
  viper_remove(L, fnameindex);
  return status;
}


typedef struct LoadS {
  const char *s;
  size_t size;
} LoadS;


static const char *getS (viper_State *L, void *ud, size_t *size) {
  LoadS *ls = (LoadS *)ud;
  (void)L;  /* not used */
  if (ls->size == 0) return NULL;
  *size = ls->size;
  ls->size = 0;
  return ls->s;
}


VIPERLIB_API int viperL_loadbufferx (viper_State *L, const char *buff, size_t size,
                                 const char *name, const char *mode) {
  LoadS ls;
  ls.s = buff;
  ls.size = size;
  return viper_load(L, getS, &ls, name, mode);
}


VIPERLIB_API int viperL_loadstring (viper_State *L, const char *s) {
  return viperL_loadbuffer(L, s, strlen(s), s);
}

/* }====================================================== */



VIPERLIB_API int viperL_getmetafield (viper_State *L, int obj, const char *event) {
  if (!viper_getmetatable(L, obj))  /* no metatable? */
    return VIPER_TNIL;
  else {
    int tt;
    viper_pushstring(L, event);
    tt = viper_rawget(L, -2);
    if (tt == VIPER_TNIL)  /* is metafield nil? */
      viper_pop(L, 2);  /* remove metatable and metafield */
    else
      viper_remove(L, -2);  /* remove only metatable */
    return tt;  /* return metafield type */
  }
}


VIPERLIB_API int viperL_callmeta (viper_State *L, int obj, const char *event) {
  obj = viper_absindex(L, obj);
  if (viperL_getmetafield(L, obj, event) == VIPER_TNIL)  /* no metafield? */
    return 0;
  viper_pushvalue(L, obj);
  viper_call(L, 1, 1);
  return 1;
}


VIPERLIB_API viper_Integer viperL_len (viper_State *L, int idx) {
  viper_Integer l;
  int isnum;
  viper_len(L, idx);
  l = viper_tointegerx(L, -1, &isnum);
  if (l_unlikely(!isnum))
    viperL_error(L, "object length is not an integer");
  viper_pop(L, 1);  /* remove object */
  return l;
}


VIPERLIB_API const char *viperL_tolstring (viper_State *L, int idx, size_t *len) {
  idx = viper_absindex(L,idx);
  if (viperL_callmeta(L, idx, "__tostring")) {  /* metafield? */
    if (!viper_isstring(L, -1))
      viperL_error(L, "'__tostring' must return a string");
  }
  else {
    switch (viper_type(L, idx)) {
      case VIPER_TNUMBER: {
        if (viper_isinteger(L, idx))
          viper_pushfstring(L, "%I", (VIPERI_UACINT)viper_tointeger(L, idx));
        else
          viper_pushfstring(L, "%f", (VIPERI_UACNUMBER)viper_tonumber(L, idx));
        break;
      }
      case VIPER_TSTRING:
        viper_pushvalue(L, idx);
        break;
      case VIPER_TBOOLEAN:
        viper_pushstring(L, (viper_toboolean(L, idx) ? "true" : "false"));
        break;
      case VIPER_TNIL:
        viper_pushliteral(L, "nil");
        break;
      default: {
        int tt = viperL_getmetafield(L, idx, "__name");  /* try name */
        const char *kind = (tt == VIPER_TSTRING) ? viper_tostring(L, -1) :
                                                 viperL_typename(L, idx);
        viper_pushfstring(L, "%s: %p", kind, viper_topointer(L, idx));
        if (tt != VIPER_TNIL)
          viper_remove(L, -2);  /* remove '__name' */
        break;
      }
    }
  }
  return viper_tolstring(L, -1, len);
}


/*
** set functions from list 'l' into table at top - 'nup'; each
** function gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/
VIPERLIB_API void viperL_setfuncs (viper_State *L, const viperL_Reg *l, int nup) {
  viperL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    if (l->func == NULL)  /* place holder? */
      viper_pushboolean(L, 0);
    else {
      int i;
      for (i = 0; i < nup; i++)  /* copy upvalues to the top */
        viper_pushvalue(L, -nup);
      viper_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    }
    viper_setfield(L, -(nup + 2), l->name);
  }
  viper_pop(L, nup);  /* remove upvalues */
}


/*
** ensure that stack[idx][fname] has a table and push that table
** into the stack
*/
VIPERLIB_API int viperL_getsubtable (viper_State *L, int idx, const char *fname) {
  if (viper_getfield(L, idx, fname) == VIPER_TTABLE)
    return 1;  /* table already there */
  else {
    viper_pop(L, 1);  /* remove previous result */
    idx = viper_absindex(L, idx);
    viper_newtable(L);
    viper_pushvalue(L, -1);  /* copy to be left at top */
    viper_setfield(L, idx, fname);  /* assign new table to field */
    return 0;  /* false, because did not find table there */
  }
}


/*
** Stripped-down 'require': After checking "loaded" table, calls 'openf'
** to open a module, registers the result in 'package.loaded' table and,
** if 'glb' is true, also registers the result in the global table.
** Leaves resulting module on the top.
*/
VIPERLIB_API void viperL_requiref (viper_State *L, const char *modname,
                               viper_CFunction openf, int glb) {
  viperL_getsubtable(L, VIPER_REGISTRYINDEX, VIPER_LOADED_TABLE);
  viper_getfield(L, -1, modname);  /* LOADED[modname] */
  if (!viper_toboolean(L, -1)) {  /* package not already loaded? */
    viper_pop(L, 1);  /* remove field */
    viper_pushcfunction(L, openf);
    viper_pushstring(L, modname);  /* argument to open function */
    viper_call(L, 1, 1);  /* call 'openf' to open module */
    viper_pushvalue(L, -1);  /* make copy of module (call result) */
    viper_setfield(L, -3, modname);  /* LOADED[modname] = module */
  }
  viper_remove(L, -2);  /* remove LOADED table */
  if (glb) {
    viper_pushvalue(L, -1);  /* copy of module */
    viper_setglobal(L, modname);  /* _G[modname] = module */
  }
}


VIPERLIB_API void viperL_addgsub (viperL_Buffer *b, const char *s,
                                     const char *p, const char *r) {
  const char *wild;
  size_t l = strlen(p);
  while ((wild = strstr(s, p)) != NULL) {
    viperL_addlstring(b, s, wild - s);  /* push prefix */
    viperL_addstring(b, r);  /* push replacement in place of pattern */
    s = wild + l;  /* continue after 'p' */
  }
  viperL_addstring(b, s);  /* push last suffix */
}


VIPERLIB_API const char *viperL_gsub (viper_State *L, const char *s,
                                  const char *p, const char *r) {
  viperL_Buffer b;
  viperL_buffinit(L, &b);
  viperL_addgsub(&b, s, p, r);
  viperL_pushresult(&b);
  return viper_tostring(L, -1);
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


static int panic (viper_State *L) {
  const char *msg = viper_tostring(L, -1);
  if (msg == NULL) msg = "error object is not a string";
  viper_writestringerror("PANIC: unprotected error in call to Viper API (%s)\n",
                        msg);
  return 0;  /* return to Viper to abort */
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
static int checkcontrol (viper_State *L, const char *message, int tocont) {
  if (tocont || *(message++) != '@')  /* not a control message? */
    return 0;
  else {
    if (strcmp(message, "off") == 0)
      viper_setwarnf(L, warnfoff, L);  /* turn warnings off */
    else if (strcmp(message, "on") == 0)
      viper_setwarnf(L, warnfon, L);   /* turn warnings on */
    return 1;  /* it was a control message */
  }
}


static void warnfoff (void *ud, const char *message, int tocont) {
  checkcontrol((viper_State *)ud, message, tocont);
}


/*
** Writes the message and handle 'tocont', finishing the message
** if needed and setting the next warn function.
*/
static void warnfcont (void *ud, const char *message, int tocont) {
  viper_State *L = (viper_State *)ud;
  viper_writestringerror("%s", message);  /* write message */
  if (tocont)  /* not the last part? */
    viper_setwarnf(L, warnfcont, L);  /* to be continued */
  else {  /* last part */
    viper_writestringerror("%s", "\n");  /* finish message with end-of-line */
    viper_setwarnf(L, warnfon, L);  /* next call is a new message */
  }
}


static void warnfon (void *ud, const char *message, int tocont) {
  if (checkcontrol((viper_State *)ud, message, tocont))  /* control message? */
    return;  /* nothing else to be done */
  viper_writestringerror("%s", "Viper warning: ");  /* start a new warning */
  warnfcont(ud, message, tocont);  /* finish processing */
}


VIPERLIB_API viper_State *viperL_newstate (void) {
  viper_State *L = viper_newstate(l_alloc, NULL);
  if (l_likely(L)) {
    viper_atpanic(L, &panic);
    viper_setwarnf(L, warnfoff, L);  /* default is warnings off */
  }
  return L;
}


VIPERLIB_API void viperL_checkversion_ (viper_State *L, viper_Number ver, size_t sz) {
  viper_Number v = viper_version(L);
  if (sz != VIPERL_NUMSIZES)  /* check numeric types */
    viperL_error(L, "core and library have incompatible numeric types");
  else if (v != ver)
    viperL_error(L, "version mismatch: app. needs %f, Viper core provides %f",
                  (VIPERI_UACNUMBER)ver, (VIPERI_UACNUMBER)v);
}

