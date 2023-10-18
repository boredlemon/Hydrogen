/*
** $Id: lauxlib.c $
** Auxiliary functions for building Acorn libraries
** See Copyright Notice in acorn.h
*/

#define lauxlib_c
#define ACORN_LIB

#include "lprefix.h"


#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
** This file uses only the official API of Acorn.
** Any function declared here could be written as an application function.
*/

#include "acorn.h"

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
** absolute index.) Return 1 + string at top if it found a Acornod name.
*/
static int findfield (acorn_State *L, int objidx, int level) {
  if (level == 0 || !acorn_istable(L, -1))
    return 0;  /* not found */
  acorn_pushnil(L);  /* start 'next' loop */
  while (acorn_next(L, -2)) {  /* for each pair in table */
    if (acorn_type(L, -2) == ACORN_TSTRING) {  /* ignore non-string keys */
      if (acorn_rawequal(L, objidx, -1)) {  /* found object? */
        acorn_pop(L, 1);  /* remove value (but keep name) */
        return 1;
      }
      else if (findfield(L, objidx, level - 1)) {  /* try recursively */
        /* stack: lib_name, lib_table, field_name (top) */
        acorn_pushliteral(L, ".");  /* place '.' between the two names */
        acorn_replace(L, -3);  /* (in the slot ocacornied by table) */
        acorn_concat(L, 3);  /* lib_name.field_name */
        return 1;
      }
    }
    acorn_pop(L, 1);  /* remove value */
  }
  return 0;  /* not found */
}


/*
** Search for a name for a function in all loaded modules
*/
static int pushglobalfuncname (acorn_State *L, acorn_Debug *ar) {
  int top = acorn_gettop(L);
  acorn_getinfo(L, "f", ar);  /* push function */
  acorn_getfield(L, ACORN_REGISTRYINDEX, ACORN_LOADED_TABLE);
  if (findfield(L, top + 1, 2)) {
    const char *name = acorn_tostring(L, -1);
    if (strncmp(name, ACORN_GNAME ".", 3) == 0) {  /* name start with '_G.'? */
      acorn_pushstring(L, name + 3);  /* push name without prefix */
      acorn_remove(L, -2);  /* remove original name */
    }
    acorn_copy(L, -1, top + 1);  /* copy name to proper place */
    acorn_settop(L, top + 1);  /* remove table "loaded" and name copy */
    return 1;
  }
  else {
    acorn_settop(L, top);  /* remove function and global table */
    return 0;
  }
}


static void pushfuncname (acorn_State *L, acorn_Debug *ar) {
  if (pushglobalfuncname(L, ar)) {  /* try first a global name */
    acorn_pushfstring(L, "function '%s'", acorn_tostring(L, -1));
    acorn_remove(L, -2);  /* remove name */
  }
  else if (*ar->namewhat != '\0')  /* is there a name from code? */
    acorn_pushfstring(L, "%s '%s'", ar->namewhat, ar->name);  /* use it */
  else if (*ar->what == 'm')  /* main? */
      acorn_pushliteral(L, "main chunk");
  else if (*ar->what != 'C')  /* for Acorn functions, use <file:line> */
    acorn_pushfstring(L, "function <%s:%d>", ar->short_src, ar->linedefined);
  else  /* nothing left... */
    acorn_pushliteral(L, "?");
}


static int lastlevel (acorn_State *L) {
  acorn_Debug ar;
  int li = 1, le = 1;
  /* find an upper bound */
  while (acorn_getstack(L, le, &ar)) { li = le; le *= 2; }
  /* do a binary search */
  while (li < le) {
    int m = (li + le)/2;
    if (acorn_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}


ACORNLIB_API void acornL_traceback (acorn_State *L, acorn_State *L1,
                                const char *msg, int level) {
  acornL_Buffer b;
  acorn_Debug ar;
  int last = lastlevel(L1);
  int limit2show = (last - level > LEVELS1 + LEVELS2) ? LEVELS1 : -1;
  acornL_buffinit(L, &b);
  if (msg) {
    acornL_addstring(&b, msg);
    acornL_addchar(&b, '\n');
  }
  acornL_addstring(&b, "stack traceback:");
  while (acorn_getstack(L1, level++, &ar)) {
    if (limit2show-- == 0) {  /* too many levels? */
      int n = last - level - LEVELS2 + 1;  /* number of levels to skip */
      acorn_pushfstring(L, "\n\t...\t(skipping %d levels)", n);
      acornL_addvalue(&b);  /* add warning about skip */
      level += n;  /* and skip to last levels */
    }
    else {
      acorn_getinfo(L1, "Slnt", &ar);
      if (ar.currentline <= 0)
        acorn_pushfstring(L, "\n\t%s: in ", ar.short_src);
      else
        acorn_pushfstring(L, "\n\t%s:%d: in ", ar.short_src, ar.currentline);
      acornL_addvalue(&b);
      pushfuncname(L, &ar);
      acornL_addvalue(&b);
      if (ar.istailcall)
        acornL_addstring(&b, "\n\t(...tail calls...)");
    }
  }
  acornL_pushresult(&b);
}

/* }====================================================== */


/*
** {======================================================
** Error-report functions
** =======================================================
*/

ACORNLIB_API int acornL_argerror (acorn_State *L, int arg, const char *extramsg) {
  acorn_Debug ar;
  if (!acorn_getstack(L, 0, &ar))  /* no stack frame? */
    return acornL_error(L, "bad argument #%d (%s)", arg, extramsg);
  acorn_getinfo(L, "n", &ar);
  if (strcmp(ar.namewhat, "method") == 0) {
    arg--;  /* do not count 'self' */
    if (arg == 0)  /* error is in the self argument itself? */
      return acornL_error(L, "calling '%s' on bad self (%s)",
                           ar.name, extramsg);
  }
  if (ar.name == NULL)
    ar.name = (pushglobalfuncname(L, &ar)) ? acorn_tostring(L, -1) : "?";
  return acornL_error(L, "bad argument #%d to '%s' (%s)",
                        arg, ar.name, extramsg);
}


ACORNLIB_API int acornL_typeerror (acorn_State *L, int arg, const char *tname) {
  const char *msg;
  const char *typearg;  /* name for the type of the actual argument */
  if (acornL_getmetafield(L, arg, "__name") == ACORN_TSTRING)
    typearg = acorn_tostring(L, -1);  /* use the given type name */
  else if (acorn_type(L, arg) == ACORN_TLIGHTUSERDATA)
    typearg = "light userdata";  /* special name for messages */
  else
    typearg = acornL_typename(L, arg);  /* standard name */
  msg = acorn_pushfstring(L, "%s expected, Acornt %s", tname, typearg);
  return acornL_argerror(L, arg, msg);
}


static void tag_error (acorn_State *L, int arg, int tag) {
  acornL_typeerror(L, arg, acorn_typename(L, tag));
}


/*
** The use of 'acorn_pushfstring' ensures this function does not
** need reserved stack space when called.
*/
ACORNLIB_API void acornL_where (acorn_State *L, int level) {
  acorn_Debug ar;
  if (acorn_getstack(L, level, &ar)) {  /* check function at level */
    acorn_getinfo(L, "Sl", &ar);  /* get info about it */
    if (ar.currentline > 0) {  /* is there info? */
      acorn_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline);
      return;
    }
  }
  acorn_pushfstring(L, "");  /* else, no information available... */
}


/*
** Again, the use of 'acorn_pushvfstring' ensures this function does
** not need reserved stack space when called. (At worst, it generates
** an error with "stack overflow" instead of the given message.)
*/
ACORNLIB_API int acornL_error (acorn_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  acornL_where(L, 1);
  acorn_pushvfstring(L, fmt, argp);
  va_end(argp);
  acorn_concat(L, 2);
  return acorn_error(L);
}


ACORNLIB_API int acornL_fileresult (acorn_State *L, int stat, const char *fname) {
  int en = errno;  /* calls to Acorn API may change this value */
  if (stat) {
    acorn_pushboolean(L, 1);
    return 1;
  }
  else {
    acornL_pushfail(L);
    if (fname)
      acorn_pushfstring(L, "%s: %s", fname, strerror(en));
    else
      acorn_pushstring(L, strerror(en));
    acorn_pushinteger(L, en);
    return 3;
  }
}


#if !defined(l_inspectstat)	/* { */

#if defined(ACORN_USE_POSIX)

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


ACORNLIB_API int acornL_execresult (acorn_State *L, int stat) {
  if (stat != 0 && errno != 0)  /* error with an 'errno'? */
    return acornL_fileresult(L, 0, NULL);
  else {
    const char *what = "exit";  /* type of termination */
    l_inspectstat(stat, what);  /* interpret result */
    if (*what == 'e' && stat == 0)  /* successful termination? */
      acorn_pushboolean(L, 1);
    else
      acornL_pushfail(L);
    acorn_pushstring(L, what);
    acorn_pushinteger(L, stat);
    return 3;  /* return true/fail,what,code */
  }
}

/* }====================================================== */



/*
** {======================================================
** Userdata's metatable manipulation
** =======================================================
*/

ACORNLIB_API int acornL_newmetatable (acorn_State *L, const char *tname) {
  if (acornL_getmetatable(L, tname) != ACORN_TNIL)  /* name already in use? */
    return 0;  /* leave previous value on top, but return 0 */
  acorn_pop(L, 1);
  acorn_createtable(L, 0, 2);  /* create metatable */
  acorn_pushstring(L, tname);
  acorn_setfield(L, -2, "__name");  /* metatable.__name = tname */
  acorn_pushvalue(L, -1);
  acorn_setfield(L, ACORN_REGISTRYINDEX, tname);  /* registry.name = metatable */
  return 1;
}


ACORNLIB_API void acornL_setmetatable (acorn_State *L, const char *tname) {
  acornL_getmetatable(L, tname);
  acorn_setmetatable(L, -2);
}


ACORNLIB_API void *acornL_testudata (acorn_State *L, int ud, const char *tname) {
  void *p = acorn_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    if (acorn_getmetatable(L, ud)) {  /* does it have a metatable? */
      acornL_getmetatable(L, tname);  /* get correct metatable */
      if (!acorn_rawequal(L, -1, -2))  /* not the same? */
        p = NULL;  /* value is a userdata with wrong metatable */
      acorn_pop(L, 2);  /* remove both metatables */
      return p;
    }
  }
  return NULL;  /* value is not a userdata with a metatable */
}


ACORNLIB_API void *acornL_checkudata (acorn_State *L, int ud, const char *tname) {
  void *p = acornL_testudata(L, ud, tname);
  acornL_argexpected(L, p != NULL, ud, tname);
  return p;
}

/* }====================================================== */


/*
** {======================================================
** Argument check functions
** =======================================================
*/

ACORNLIB_API int acornL_checkoption (acorn_State *L, int arg, const char *def,
                                 const char *const lst[]) {
  const char *name = (def) ? acornL_optstring(L, arg, def) :
                             acornL_checkstring(L, arg);
  int i;
  for (i=0; lst[i]; i++)
    if (strcmp(lst[i], name) == 0)
      return i;
  return acornL_argerror(L, arg,
                       acorn_pushfstring(L, "invalid option '%s'", name));
}


/*
** Ensures the stack has at least 'space' extra slots, raising an error
** if it cannot fulfill the request. (The error handling needs a few
** extra slots to format the error message. In case of an error without
** this extra space, Acorn will generate the same 'stack overflow' error,
** but without 'msg'.)
*/
ACORNLIB_API void acornL_checkstack (acorn_State *L, int space, const char *msg) {
  if (l_unlikely(!acorn_checkstack(L, space))) {
    if (msg)
      acornL_error(L, "stack overflow (%s)", msg);
    else
      acornL_error(L, "stack overflow");
  }
}


ACORNLIB_API void acornL_checktype (acorn_State *L, int arg, int t) {
  if (l_unlikely(acorn_type(L, arg) != t))
    tag_error(L, arg, t);
}


ACORNLIB_API void acornL_checkany (acorn_State *L, int arg) {
  if (l_unlikely(acorn_type(L, arg) == ACORN_TNONE))
    acornL_argerror(L, arg, "value expected");
}


ACORNLIB_API const char *acornL_checklstring (acorn_State *L, int arg, size_t *len) {
  const char *s = acorn_tolstring(L, arg, len);
  if (l_unlikely(!s)) tag_error(L, arg, ACORN_TSTRING);
  return s;
}


ACORNLIB_API const char *acornL_optlstring (acorn_State *L, int arg,
                                        const char *def, size_t *len) {
  if (acorn_isnoneornil(L, arg)) {
    if (len)
      *len = (def ? strlen(def) : 0);
    return def;
  }
  else return acornL_checklstring(L, arg, len);
}


ACORNLIB_API acorn_Number acornL_checknumber (acorn_State *L, int arg) {
  int isnum;
  acorn_Number d = acorn_tonumberx(L, arg, &isnum);
  if (l_unlikely(!isnum))
    tag_error(L, arg, ACORN_TNUMBER);
  return d;
}


ACORNLIB_API acorn_Number acornL_optnumber (acorn_State *L, int arg, acorn_Number def) {
  return acornL_opt(L, acornL_checknumber, arg, def);
}


static void interror (acorn_State *L, int arg) {
  if (acorn_isnumber(L, arg))
    acornL_argerror(L, arg, "number has no integer representation");
  else
    tag_error(L, arg, ACORN_TNUMBER);
}


ACORNLIB_API acorn_Integer acornL_checkinteger (acorn_State *L, int arg) {
  int isnum;
  acorn_Integer d = acorn_tointegerx(L, arg, &isnum);
  if (l_unlikely(!isnum)) {
    interror(L, arg);
  }
  return d;
}


ACORNLIB_API acorn_Integer acornL_optinteger (acorn_State *L, int arg,
                                                      acorn_Integer def) {
  return acornL_opt(L, acornL_checkinteger, arg, def);
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


static void *resizebox (acorn_State *L, int idx, size_t newsize) {
  void *ud;
  acorn_Alloc allocf = acorn_getallocf(L, &ud);
  UBox *box = (UBox *)acorn_touserdata(L, idx);
  void *temp = allocf(ud, box->box, box->bsize, newsize);
  if (l_unlikely(temp == NULL && newsize > 0)) {  /* allocation error? */
    acorn_pushliteral(L, "not enough memory");
    acorn_error(L);  /* raise a memory error */
  }
  box->box = temp;
  box->bsize = newsize;
  return temp;
}


static int boxgc (acorn_State *L) {
  resizebox(L, 1, 0);
  return 0;
}


static const acornL_Reg boxmt[] = {  /* box metamethods */
  {"__gc", boxgc},
  {"__close", boxgc},
  {NULL, NULL}
};


static void newbox (acorn_State *L) {
  UBox *box = (UBox *)acorn_newuserdatauv(L, sizeof(UBox), 0);
  box->box = NULL;
  box->bsize = 0;
  if (acornL_newmetatable(L, "_UBOX*"))  /* creating metatable? */
    acornL_setfuncs(L, boxmt, 0);  /* set its metamethods */
  acorn_setmetatable(L, -2);
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
  acorn_assert(buffonstack(B) ? acorn_touserdata(B->L, idx) != NULL  \
                            : acorn_touserdata(B->L, idx) == (void*)B)


/*
** Compute new size for buffer 'B', enough to accommodate extra 'sz'
** bytes.
*/
static size_t newbuffsize (acornL_Buffer *B, size_t sz) {
  size_t newsize = B->size * 2;  /* double buffer size */
  if (l_unlikely(MAX_SIZET - sz < B->n))  /* overflow in (B->n + sz)? */
    return acornL_error(B->L, "buffer too large");
  if (newsize < B->n + sz)  /* double is not big enough? */
    newsize = B->n + sz;
  return newsize;
}


/*
** Returns a pointer to a free area with at least 'sz' bytes in buffer
** 'B'. 'boxidx' is the relative position in the stack where is the
** buffer's box or its placeholder.
*/
static char *prepbuffsize (acornL_Buffer *B, size_t sz, int boxidx) {
  checkbufferlevel(B, boxidx);
  if (B->size - B->n >= sz)  /* enough space? */
    return B->b + B->n;
  else {
    acorn_State *L = B->L;
    char *newbuff;
    size_t newsize = newbuffsize(B, sz);
    /* create larger buffer */
    if (buffonstack(B))  /* buffer already has a box? */
      newbuff = (char *)resizebox(L, boxidx, newsize);  /* resize it */
    else {  /* no box yet */
      acorn_remove(L, boxidx);  /* remove placeholder */
      newbox(L);  /* create a new box */
      acorn_insert(L, boxidx);  /* move box to its intended position */
      acorn_toclose(L, boxidx);
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
ACORNLIB_API char *acornL_prepbuffsize (acornL_Buffer *B, size_t sz) {
  return prepbuffsize(B, sz, -1);
}


ACORNLIB_API void acornL_addlstring (acornL_Buffer *B, const char *s, size_t l) {
  if (l > 0) {  /* avoid 'memcpy' when 's' can be NULL */
    char *b = prepbuffsize(B, l, -1);
    memcpy(b, s, l * sizeof(char));
    acornL_addsize(B, l);
  }
}


ACORNLIB_API void acornL_addstring (acornL_Buffer *B, const char *s) {
  acornL_addlstring(B, s, strlen(s));
}


ACORNLIB_API void acornL_pushresult (acornL_Buffer *B) {
  acorn_State *L = B->L;
  checkbufferlevel(B, -1);
  acorn_pushlstring(L, B->b, B->n);
  if (buffonstack(B))
    acorn_closeslot(L, -2);  /* close the box */
  acorn_remove(L, -2);  /* remove box or placeholder from the stack */
}


ACORNLIB_API void acornL_pushresultsize (acornL_Buffer *B, size_t sz) {
  acornL_addsize(B, sz);
  acornL_pushresult(B);
}


/*
** 'acornL_addvalue' is the only function in the Buffer system where the
** box (if existent) is not on the top of the stack. So, instead of
** calling 'acornL_addlstring', it replicates the code using -2 as the
** last argument to 'prepbuffsize', signaling that the box is (or will
** be) bellow the string being added to the buffer. (Box creation can
** trigger an emergency GC, so we should not remove the string from the
** stack before we have the space guaranteed.)
*/
ACORNLIB_API void acornL_addvalue (acornL_Buffer *B) {
  acorn_State *L = B->L;
  size_t len;
  const char *s = acorn_tolstring(L, -1, &len);
  char *b = prepbuffsize(B, len, -2);
  memcpy(b, s, len * sizeof(char));
  acornL_addsize(B, len);
  acorn_pop(L, 1);  /* pop string */
}


ACORNLIB_API void acornL_buffinit (acorn_State *L, acornL_Buffer *B) {
  B->L = L;
  B->b = B->init.b;
  B->n = 0;
  B->size = ACORNL_BUFFERSIZE;
  acorn_pushlightuserdata(L, (void*)B);  /* push placeholder */
}


ACORNLIB_API char *acornL_buffinitsize (acorn_State *L, acornL_Buffer *B, size_t sz) {
  acornL_buffinit(L, B);
  return prepbuffsize(B, sz, -1);
}

/* }====================================================== */


/*
** {======================================================
** Reference system
** =======================================================
*/

/* index of free-list header (after the predefined values) */
#define freelist	(ACORN_RIDX_LAST + 1)

/*
** The previously freed references form a linked list:
** t[freelist] is the index of a first free index, or zero if list is
** empty; t[t[freelist]] is the index of the second element; etc.
*/
ACORNLIB_API int acornL_ref (acorn_State *L, int t) {
  int ref;
  if (acorn_isnil(L, -1)) {
    acorn_pop(L, 1);  /* remove from stack */
    return ACORN_REFNIL;  /* 'nil' has a unique fixed reference */
  }
  t = acorn_absindex(L, t);
  if (acorn_rawgeti(L, t, freelist) == ACORN_TNIL) {  /* first access? */
    ref = 0;  /* list is empty */
    acorn_pushinteger(L, 0);  /* initialize as an empty list */
    acorn_rawseti(L, t, freelist);  /* ref = t[freelist] = 0 */
  }
  else {  /* already initialized */
    acorn_assert(acorn_isinteger(L, -1));
    ref = (int)acorn_tointeger(L, -1);  /* ref = t[freelist] */
  }
  acorn_pop(L, 1);  /* remove element from stack */
  if (ref != 0) {  /* any free element? */
    acorn_rawgeti(L, t, ref);  /* remove it from list */
    acorn_rawseti(L, t, freelist);  /* (t[freelist] = t[ref]) */
  }
  else  /* no free elements */
    ref = (int)acorn_rawlen(L, t) + 1;  /* get a new reference */
  acorn_rawseti(L, t, ref);
  return ref;
}


ACORNLIB_API void acornL_unref (acorn_State *L, int t, int ref) {
  if (ref >= 0) {
    t = acorn_absindex(L, t);
    acorn_rawgeti(L, t, freelist);
    acorn_assert(acorn_isinteger(L, -1));
    acorn_rawseti(L, t, ref);  /* t[ref] = t[freelist] */
    acorn_pushinteger(L, ref);
    acorn_rawseti(L, t, freelist);  /* t[freelist] = ref */
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


static const char *getF (acorn_State *L, void *ud, size_t *size) {
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


static int errfile (acorn_State *L, const char *what, int fnameindex) {
  const char *serr = strerror(errno);
  const char *filename = acorn_tostring(L, fnameindex) + 1;
  acorn_pushfstring(L, "cannot %s %s: %s", what, filename, serr);
  acorn_remove(L, fnameindex);
  return ACORN_ERRFILE;
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


ACORNLIB_API int acornL_loadfilex (acorn_State *L, const char *filename,
                                             const char *mode) {
  LoadF lf;
  int status, readstatus;
  int c;
  int fnameindex = acorn_gettop(L) + 1;  /* index of filename on the stack */
  if (filename == NULL) {
    acorn_pushliteral(L, "=stdin");
    lf.f = stdin;
  }
  else {
    acorn_pushfstring(L, "@%s", filename);
    lf.f = fopen(filename, "r");
    if (lf.f == NULL) return errfile(L, "open", fnameindex);
  }
  if (skipcomment(&lf, &c))  /* read initial portion */
    lf.buff[lf.n++] = '\n';  /* add line to correct line numbers */
  if (c == ACORN_SIGNATURE[0] && filename) {  /* binary file? */
    lf.f = freopen(filename, "rb", lf.f);  /* reopen in binary mode */
    if (lf.f == NULL) return errfile(L, "reopen", fnameindex);
    skipcomment(&lf, &c);  /* re-read initial portion */
  }
  if (c != EOF)
    lf.buff[lf.n++] = c;  /* 'c' is the first character of the stream */
  status = acorn_load(L, getF, &lf, acorn_tostring(L, -1), mode);
  readstatus = ferror(lf.f);
  if (filename) fclose(lf.f);  /* close file (even in case of errors) */
  if (readstatus) {
    acorn_settop(L, fnameindex);  /* ignore results from 'acorn_load' */
    return errfile(L, "read", fnameindex);
  }
  acorn_remove(L, fnameindex);
  return status;
}


typedef struct LoadS {
  const char *s;
  size_t size;
} LoadS;


static const char *getS (acorn_State *L, void *ud, size_t *size) {
  LoadS *ls = (LoadS *)ud;
  (void)L;  /* not used */
  if (ls->size == 0) return NULL;
  *size = ls->size;
  ls->size = 0;
  return ls->s;
}


ACORNLIB_API int acornL_loadbufferx (acorn_State *L, const char *buff, size_t size,
                                 const char *name, const char *mode) {
  LoadS ls;
  ls.s = buff;
  ls.size = size;
  return acorn_load(L, getS, &ls, name, mode);
}


ACORNLIB_API int acornL_loadstring (acorn_State *L, const char *s) {
  return acornL_loadbuffer(L, s, strlen(s), s);
}

/* }====================================================== */



ACORNLIB_API int acornL_getmetafield (acorn_State *L, int obj, const char *event) {
  if (!acorn_getmetatable(L, obj))  /* no metatable? */
    return ACORN_TNIL;
  else {
    int tt;
    acorn_pushstring(L, event);
    tt = acorn_rawget(L, -2);
    if (tt == ACORN_TNIL)  /* is metafield nil? */
      acorn_pop(L, 2);  /* remove metatable and metafield */
    else
      acorn_remove(L, -2);  /* remove only metatable */
    return tt;  /* return metafield type */
  }
}


ACORNLIB_API int acornL_callmeta (acorn_State *L, int obj, const char *event) {
  obj = acorn_absindex(L, obj);
  if (acornL_getmetafield(L, obj, event) == ACORN_TNIL)  /* no metafield? */
    return 0;
  acorn_pushvalue(L, obj);
  acorn_call(L, 1, 1);
  return 1;
}


ACORNLIB_API acorn_Integer acornL_len (acorn_State *L, int idx) {
  acorn_Integer l;
  int isnum;
  acorn_len(L, idx);
  l = acorn_tointegerx(L, -1, &isnum);
  if (l_unlikely(!isnum))
    acornL_error(L, "object length is not an integer");
  acorn_pop(L, 1);  /* remove object */
  return l;
}


ACORNLIB_API const char *acornL_tolstring (acorn_State *L, int idx, size_t *len) {
  idx = acorn_absindex(L,idx);
  if (acornL_callmeta(L, idx, "__tostring")) {  /* metafield? */
    if (!acorn_isstring(L, -1))
      acornL_error(L, "'__tostring' must return a string");
  }
  else {
    switch (acorn_type(L, idx)) {
      case ACORN_TNUMBER: {
        if (acorn_isinteger(L, idx))
          acorn_pushfstring(L, "%I", (ACORNI_UACINT)acorn_tointeger(L, idx));
        else
          acorn_pushfstring(L, "%f", (ACORNI_UACNUMBER)acorn_tonumber(L, idx));
        break;
      }
      case ACORN_TSTRING:
        acorn_pushvalue(L, idx);
        break;
      case ACORN_TBOOLEAN:
        acorn_pushstring(L, (acorn_toboolean(L, idx) ? "true" : "false"));
        break;
      case ACORN_TNIL:
        acorn_pushliteral(L, "nil");
        break;
      default: {
        int tt = acornL_getmetafield(L, idx, "__name");  /* try name */
        const char *kind = (tt == ACORN_TSTRING) ? acorn_tostring(L, -1) :
                                                 acornL_typename(L, idx);
        acorn_pushfstring(L, "%s: %p", kind, acorn_topointer(L, idx));
        if (tt != ACORN_TNIL)
          acorn_remove(L, -2);  /* remove '__name' */
        break;
      }
    }
  }
  return acorn_tolstring(L, -1, len);
}


/*
** set functions from list 'l' into table at top - 'nup'; each
** function gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/
ACORNLIB_API void acornL_setfuncs (acorn_State *L, const acornL_Reg *l, int nup) {
  acornL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    if (l->func == NULL)  /* place holder? */
      acorn_pushboolean(L, 0);
    else {
      int i;
      for (i = 0; i < nup; i++)  /* copy upvalues to the top */
        acorn_pushvalue(L, -nup);
      acorn_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    }
    acorn_setfield(L, -(nup + 2), l->name);
  }
  acorn_pop(L, nup);  /* remove upvalues */
}


/*
** ensure that stack[idx][fname] has a table and push that table
** into the stack
*/
ACORNLIB_API int acornL_getsubtable (acorn_State *L, int idx, const char *fname) {
  if (acorn_getfield(L, idx, fname) == ACORN_TTABLE)
    return 1;  /* table already there */
  else {
    acorn_pop(L, 1);  /* remove previous result */
    idx = acorn_absindex(L, idx);
    acorn_newtable(L);
    acorn_pushvalue(L, -1);  /* copy to be left at top */
    acorn_setfield(L, idx, fname);  /* assign new table to field */
    return 0;  /* false, because did not find table there */
  }
}


/*
** Stripped-down 'require': After checking "loaded" table, calls 'openf'
** to open a module, registers the result in 'package.loaded' table and,
** if 'glb' is true, also registers the result in the global table.
** Leaves resulting module on the top.
*/
ACORNLIB_API void acornL_requiref (acorn_State *L, const char *modname,
                               acorn_CFunction openf, int glb) {
  acornL_getsubtable(L, ACORN_REGISTRYINDEX, ACORN_LOADED_TABLE);
  acorn_getfield(L, -1, modname);  /* LOADED[modname] */
  if (!acorn_toboolean(L, -1)) {  /* package not already loaded? */
    acorn_pop(L, 1);  /* remove field */
    acorn_pushcfunction(L, openf);
    acorn_pushstring(L, modname);  /* argument to open function */
    acorn_call(L, 1, 1);  /* call 'openf' to open module */
    acorn_pushvalue(L, -1);  /* make copy of module (call result) */
    acorn_setfield(L, -3, modname);  /* LOADED[modname] = module */
  }
  acorn_remove(L, -2);  /* remove LOADED table */
  if (glb) {
    acorn_pushvalue(L, -1);  /* copy of module */
    acorn_setglobal(L, modname);  /* _G[modname] = module */
  }
}


ACORNLIB_API void acornL_addgsub (acornL_Buffer *b, const char *s,
                                     const char *p, const char *r) {
  const char *wild;
  size_t l = strlen(p);
  while ((wild = strstr(s, p)) != NULL) {
    acornL_addlstring(b, s, wild - s);  /* push prefix */
    acornL_addstring(b, r);  /* push replacement in place of pattern */
    s = wild + l;  /* continue after 'p' */
  }
  acornL_addstring(b, s);  /* push last suffix */
}


ACORNLIB_API const char *acornL_gsub (acorn_State *L, const char *s,
                                  const char *p, const char *r) {
  acornL_Buffer b;
  acornL_buffinit(L, &b);
  acornL_addgsub(&b, s, p, r);
  acornL_pushresult(&b);
  return acorn_tostring(L, -1);
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


static int panic (acorn_State *L) {
  const char *msg = acorn_tostring(L, -1);
  if (msg == NULL) msg = "error object is not a string";
  acorn_writestringerror("PANIC: unprotected error in call to Acorn API (%s)\n",
                        msg);
  return 0;  /* return to Acorn to abort */
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
static int checkcontrol (acorn_State *L, const char *message, int tocont) {
  if (tocont || *(message++) != '@')  /* not a control message? */
    return 0;
  else {
    if (strcmp(message, "off") == 0)
      acorn_setwarnf(L, warnfoff, L);  /* turn warnings off */
    else if (strcmp(message, "on") == 0)
      acorn_setwarnf(L, warnfon, L);   /* turn warnings on */
    return 1;  /* it was a control message */
  }
}


static void warnfoff (void *ud, const char *message, int tocont) {
  checkcontrol((acorn_State *)ud, message, tocont);
}


/*
** Writes the message and handle 'tocont', finishing the message
** if needed and setting the next warn function.
*/
static void warnfcont (void *ud, const char *message, int tocont) {
  acorn_State *L = (acorn_State *)ud;
  acorn_writestringerror("%s", message);  /* write message */
  if (tocont)  /* not the last part? */
    acorn_setwarnf(L, warnfcont, L);  /* to be continued */
  else {  /* last part */
    acorn_writestringerror("%s", "\n");  /* finish message with end-of-line */
    acorn_setwarnf(L, warnfon, L);  /* next call is a new message */
  }
}


static void warnfon (void *ud, const char *message, int tocont) {
  if (checkcontrol((acorn_State *)ud, message, tocont))  /* control message? */
    return;  /* nothing else to be done */
  acorn_writestringerror("%s", "Acorn warning: ");  /* start a new warning */
  warnfcont(ud, message, tocont);  /* finish processing */
}


ACORNLIB_API acorn_State *acornL_newstate (void) {
  acorn_State *L = acorn_newstate(l_alloc, NULL);
  if (l_likely(L)) {
    acorn_atpanic(L, &panic);
    acorn_setwarnf(L, warnfoff, L);  /* default is warnings off */
  }
  return L;
}


ACORNLIB_API void acornL_checkversion_ (acorn_State *L, acorn_Number ver, size_t sz) {
  acorn_Number v = acorn_version(L);
  if (sz != ACORNL_NUMSIZES)  /* check numeric types */
    acornL_error(L, "core and library have incompatible numeric types");
  else if (v != ver)
    acornL_error(L, "version mismatch: app. needs %f, Acorn core provides %f",
                  (ACORNI_UACNUMBER)ver, (ACORNI_UACNUMBER)v);
}

