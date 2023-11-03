/*
** $Id: auxlib.c $
** Auxiliary functions for building Nebula libraries
** See Copyright Notice in nebula.h
*/

#define auxlib_c
#define NEBULA_LIB

#include "prefix.h"


#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
** This file uses only the official API of Nebula.
** Any function declared here could be written as an application function.
*/

#include "nebula.h"

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
** absolute index.) Return 1 + string at top if it found a Nebulaod name.
*/
static int findfield (nebula_State *L, int objidx, int level) {
  if (level == 0 || !nebula_istable(L, -1))
    return 0;  /* not found */
  nebula_pushnil(L);  /* start 'next' loop */
  while (nebula_next(L, -2)) {  /* for each pair in table */
    if (nebula_type(L, -2) == NEBULA_TSTRING) {  /* ignore non-string keys */
      if (nebula_rawequal(L, objidx, -1)) {  /* found object? */
        nebula_pop(L, 1);  /* remove value (but keep name) */
        return 1;
      }
      else if (findfield(L, objidx, level - 1)) {  /* try recursively */
        /* stack: lib_name, lib_table, field_name (top) */
        nebula_pushliteral(L, ".");  /* place '.' between the two names */
        nebula_replace(L, -3);  /* (in the slot ocnebulaied by table) */
        nebula_concat(L, 3);  /* lib_name.field_name */
        return 1;
      }
    }
    nebula_pop(L, 1);  /* remove value */
  }
  return 0;  /* not found */
}


/*
** Search for a name for a function in all loaded modules
*/
static int pushglobalfuncname (nebula_State *L, nebula_Debug *ar) {
  int top = nebula_gettop(L);
  nebula_getinfo(L, "f", ar);  /* push function */
  nebula_getfield(L, NEBULA_REGISTRYINDEX, NEBULA_LOADED_TABLE);
  if (findfield(L, top + 1, 2)) {
    const char *name = nebula_tostring(L, -1);
    if (strncmp(name, NEBULA_GNAME ".", 3) == 0) {  /* name start with '_G.'? */
      nebula_pushstring(L, name + 3);  /* push name without prefix */
      nebula_remove(L, -2);  /* remove original name */
    }
    nebula_copy(L, -1, top + 1);  /* copy name to proper place */
    nebula_settop(L, top + 1);  /* remove table "loaded" and name copy */
    return 1;
  }
  else {
    nebula_settop(L, top);  /* remove function and global table */
    return 0;
  }
}


static void pushfuncname (nebula_State *L, nebula_Debug *ar) {
  if (pushglobalfuncname(L, ar)) {  /* try first a global name */
    nebula_pushfstring(L, "function '%s'", nebula_tostring(L, -1));
    nebula_remove(L, -2);  /* remove name */
  }
  else if (*ar->namewhat != '\0')  /* is there a name from code? */
    nebula_pushfstring(L, "%s '%s'", ar->namewhat, ar->name);  /* use it */
  else if (*ar->what == 'm')  /* main? */
      nebula_pushliteral(L, "main chunk");
  else if (*ar->what != 'C')  /* for Nebula functions, use <file:line> */
    nebula_pushfstring(L, "function <%s:%d>", ar->short_src, ar->linedefined);
  else  /* nothing left... */
    nebula_pushliteral(L, "?");
}


static int lastlevel (nebula_State *L) {
  nebula_Debug ar;
  int li = 1, le = 1;
  /* find an upper bound */
  while (nebula_getstack(L, le, &ar)) { li = le; le *= 2; }
  /* do a binary search */
  while (li < le) {
    int m = (li + le)/2;
    if (nebula_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}


NEBULALIB_API void nebulaL_traceback (nebula_State *L, nebula_State *L1,
                                const char *msg, int level) {
  nebulaL_Buffer b;
  nebula_Debug ar;
  int last = lastlevel(L1);
  int limit2show = (last - level > LEVELS1 + LEVELS2) ? LEVELS1 : -1;
  nebulaL_buffinit(L, &b);
  if (msg) {
    nebulaL_addstring(&b, msg);
    nebulaL_addchar(&b, '\n');
  }
  nebulaL_addstring(&b, "stack traceback:");
  while (nebula_getstack(L1, level++, &ar)) {
    if (limit2show-- == 0) {  /* too many levels? */
      int n = last - level - LEVELS2 + 1;  /* number of levels to skip */
      nebula_pushfstring(L, "\n\t...\t(skipping %d levels)", n);
      nebulaL_addvalue(&b);  /* add warning about skip */
      level += n;  /* and skip to last levels */
    }
    else {
      nebula_getinfo(L1, "Slnt", &ar);
      if (ar.currentline <= 0)
        nebula_pushfstring(L, "\n\t%s: in ", ar.short_src);
      else
        nebula_pushfstring(L, "\n\t%s:%d: in ", ar.short_src, ar.currentline);
      nebulaL_addvalue(&b);
      pushfuncname(L, &ar);
      nebulaL_addvalue(&b);
      if (ar.istailcall)
        nebulaL_addstring(&b, "\n\t(...tail calls...)");
    }
  }
  nebulaL_pushresult(&b);
}

/* }====================================================== */


/*
** {======================================================
** Error-report functions
** =======================================================
*/

NEBULALIB_API int nebulaL_argerror (nebula_State *L, int arg, const char *extramsg) {
  nebula_Debug ar;
  if (!nebula_getstack(L, 0, &ar))  /* no stack frame? */
    return nebulaL_error(L, "bad argument #%d (%s)", arg, extramsg);
  nebula_getinfo(L, "n", &ar);
  if (strcmp(ar.namewhat, "method") == 0) {
    arg--;  /* do not count 'self' */
    if (arg == 0)  /* error is in the self argument itself? */
      return nebulaL_error(L, "calling '%s' on bad self (%s)",
                           ar.name, extramsg);
  }
  if (ar.name == NULL)
    ar.name = (pushglobalfuncname(L, &ar)) ? nebula_tostring(L, -1) : "?";
  return nebulaL_error(L, "bad argument #%d to '%s' (%s)",
                        arg, ar.name, extramsg);
}


NEBULALIB_API int nebulaL_typeerror (nebula_State *L, int arg, const char *tname) {
  const char *msg;
  const char *typearg;  /* name for the type of the actual argument */
  if (nebulaL_getmetafield(L, arg, "__name") == NEBULA_TSTRING)
    typearg = nebula_tostring(L, -1);  /* use the given type name */
  else if (nebula_type(L, arg) == NEBULA_TLIGHTUSERDATA)
    typearg = "light userdata";  /* special name for messages */
  else
    typearg = nebulaL_typename(L, arg);  /* standard name */
  msg = nebula_pushfstring(L, "%s expected, Nebulat %s", tname, typearg);
  return nebulaL_argerror(L, arg, msg);
}


static void tag_error (nebula_State *L, int arg, int tag) {
  nebulaL_typeerror(L, arg, nebula_typename(L, tag));
}


/*
** The use of 'nebula_pushfstring' ensures this function does not
** need reserved stack space when called.
*/
NEBULALIB_API void nebulaL_where (nebula_State *L, int level) {
  nebula_Debug ar;
  if (nebula_getstack(L, level, &ar)) {  /* check function at level */
    nebula_getinfo(L, "Sl", &ar);  /* get info about it */
    if (ar.currentline > 0) {  /* is there info? */
      nebula_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline);
      return;
    }
  }
  nebula_pushfstring(L, "");  /* else, no information available... */
}


/*
** Again, the use of 'nebula_pushvfstring' ensures this function does
** not need reserved stack space when called. (At worst, it generates
** an error with "stack overflow" instead of the given message.)
*/
NEBULALIB_API int nebulaL_error (nebula_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  nebulaL_where(L, 1);
  nebula_pushvfstring(L, fmt, argp);
  va_end(argp);
  nebula_concat(L, 2);
  return nebula_error(L);
}


NEBULALIB_API int nebulaL_fileresult (nebula_State *L, int stat, const char *fname) {
  int en = errno;  /* calls to Nebula API may change this value */
  if (stat) {
    nebula_pushboolean(L, 1);
    return 1;
  }
  else {
    nebulaL_pushfail(L);
    if (fname)
      nebula_pushfstring(L, "%s: %s", fname, strerror(en));
    else
      nebula_pushstring(L, strerror(en));
    nebula_pushinteger(L, en);
    return 3;
  }
}


#if !defined(l_inspectstat)	/* { */

#if defined(NEBULA_USE_POSIX)

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


NEBULALIB_API int nebulaL_execresult (nebula_State *L, int stat) {
  if (stat != 0 && errno != 0)  /* error with an 'errno'? */
    return nebulaL_fileresult(L, 0, NULL);
  else {
    const char *what = "exit";  /* type of termination */
    l_inspectstat(stat, what);  /* interpret result */
    if (*what == 'e' && stat == 0)  /* successful termination? */
      nebula_pushboolean(L, 1);
    else
      nebulaL_pushfail(L);
    nebula_pushstring(L, what);
    nebula_pushinteger(L, stat);
    return 3;  /* return true/fail,what,code */
  }
}

/* }====================================================== */



/*
** {======================================================
** Userdata's metatable manipulation
** =======================================================
*/

NEBULALIB_API int nebulaL_newmetatable (nebula_State *L, const char *tname) {
  if (nebulaL_getmetatable(L, tname) != NEBULA_TNIL)  /* name already in use? */
    return 0;  /* leave previous value on top, but return 0 */
  nebula_pop(L, 1);
  nebula_createtable(L, 0, 2);  /* create metatable */
  nebula_pushstring(L, tname);
  nebula_setfield(L, -2, "__name");  /* metatable.__name = tname */
  nebula_pushvalue(L, -1);
  nebula_setfield(L, NEBULA_REGISTRYINDEX, tname);  /* registry.name = metatable */
  return 1;
}


NEBULALIB_API void nebulaL_setmetatable (nebula_State *L, const char *tname) {
  nebulaL_getmetatable(L, tname);
  nebula_setmetatable(L, -2);
}


NEBULALIB_API void *nebulaL_testudata (nebula_State *L, int ud, const char *tname) {
  void *p = nebula_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    if (nebula_getmetatable(L, ud)) {  /* does it have a metatable? */
      nebulaL_getmetatable(L, tname);  /* get correct metatable */
      if (!nebula_rawequal(L, -1, -2))  /* not the same? */
        p = NULL;  /* value is a userdata with wrong metatable */
      nebula_pop(L, 2);  /* remove both metatables */
      return p;
    }
  }
  return NULL;  /* value is not a userdata with a metatable */
}


NEBULALIB_API void *nebulaL_checkudata (nebula_State *L, int ud, const char *tname) {
  void *p = nebulaL_testudata(L, ud, tname);
  nebulaL_argexpected(L, p != NULL, ud, tname);
  return p;
}

/* }====================================================== */


/*
** {======================================================
** Argument check functions
** =======================================================
*/

NEBULALIB_API int nebulaL_checkoption (nebula_State *L, int arg, const char *def,
                                 const char *const lst[]) {
  const char *name = (def) ? nebulaL_optstring(L, arg, def) :
                             nebulaL_checkstring(L, arg);
  int i;
  for (i=0; lst[i]; i++)
    if (strcmp(lst[i], name) == 0)
      return i;
  return nebulaL_argerror(L, arg,
                       nebula_pushfstring(L, "invalid option '%s'", name));
}


/*
** Ensures the stack has at least 'space' extra slots, raising an error
** if it cannot fulfill the request. (The error handling needs a few
** extra slots to format the error message. In case of an error without
** this extra space, Nebula will generate the same 'stack overflow' error,
** but without 'msg'.)
*/
NEBULALIB_API void nebulaL_checkstack (nebula_State *L, int space, const char *msg) {
  if (l_unlikely(!nebula_checkstack(L, space))) {
    if (msg)
      nebulaL_error(L, "stack overflow (%s)", msg);
    else
      nebulaL_error(L, "stack overflow");
  }
}


NEBULALIB_API void nebulaL_checktype (nebula_State *L, int arg, int t) {
  if (l_unlikely(nebula_type(L, arg) != t))
    tag_error(L, arg, t);
}


NEBULALIB_API void nebulaL_checkany (nebula_State *L, int arg) {
  if (l_unlikely(nebula_type(L, arg) == NEBULA_TNONE))
    nebulaL_argerror(L, arg, "value expected");
}


NEBULALIB_API const char *nebulaL_checklstring (nebula_State *L, int arg, size_t *len) {
  const char *s = nebula_tolstring(L, arg, len);
  if (l_unlikely(!s)) tag_error(L, arg, NEBULA_TSTRING);
  return s;
}


NEBULALIB_API const char *nebulaL_optlstring (nebula_State *L, int arg,
                                        const char *def, size_t *len) {
  if (nebula_isnoneornil(L, arg)) {
    if (len)
      *len = (def ? strlen(def) : 0);
    return def;
  }
  else return nebulaL_checklstring(L, arg, len);
}


NEBULALIB_API nebula_Number nebulaL_checknumber (nebula_State *L, int arg) {
  int isnum;
  nebula_Number d = nebula_tonumberx(L, arg, &isnum);
  if (l_unlikely(!isnum))
    tag_error(L, arg, NEBULA_TNUMBER);
  return d;
}


NEBULALIB_API nebula_Number nebulaL_optnumber (nebula_State *L, int arg, nebula_Number def) {
  return nebulaL_opt(L, nebulaL_checknumber, arg, def);
}


static void interror (nebula_State *L, int arg) {
  if (nebula_isnumber(L, arg))
    nebulaL_argerror(L, arg, "number has no integer representation");
  else
    tag_error(L, arg, NEBULA_TNUMBER);
}


NEBULALIB_API nebula_Integer nebulaL_checkinteger (nebula_State *L, int arg) {
  int isnum;
  nebula_Integer d = nebula_tointegerx(L, arg, &isnum);
  if (l_unlikely(!isnum)) {
    interror(L, arg);
  }
  return d;
}


NEBULALIB_API nebula_Integer nebulaL_optinteger (nebula_State *L, int arg,
                                                      nebula_Integer def) {
  return nebulaL_opt(L, nebulaL_checkinteger, arg, def);
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


static void *resizebox (nebula_State *L, int idx, size_t newsize) {
  void *ud;
  nebula_Alloc allocf = nebula_getallocf(L, &ud);
  UBox *box = (UBox *)nebula_touserdata(L, idx);
  void *temp = allocf(ud, box->box, box->bsize, newsize);
  if (l_unlikely(temp == NULL && newsize > 0)) {  /* allocation error? */
    nebula_pushliteral(L, "not enough memory");
    nebula_error(L);  /* raise a memory error */
  }
  box->box = temp;
  box->bsize = newsize;
  return temp;
}


static int boxgc (nebula_State *L) {
  resizebox(L, 1, 0);
  return 0;
}


static const nebulaL_Reg boxmt[] = {  /* box metamethods */
  {"__gc", boxgc},
  {"__close", boxgc},
  {NULL, NULL}
};


static void newbox (nebula_State *L) {
  UBox *box = (UBox *)nebula_newuserdatauv(L, sizeof(UBox), 0);
  box->box = NULL;
  box->bsize = 0;
  if (nebulaL_newmetatable(L, "_UBOX*"))  /* creating metatable? */
    nebulaL_setfuncs(L, boxmt, 0);  /* set its metamethods */
  nebula_setmetatable(L, -2);
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
  nebula_assert(buffonstack(B) ? nebula_touserdata(B->L, idx) != NULL  \
                            : nebula_touserdata(B->L, idx) == (void*)B)


/*
** Compute new size for buffer 'B', enough to accommodate extra 'sz'
** bytes.
*/
static size_t newbuffsize (nebulaL_Buffer *B, size_t sz) {
  size_t newsize = B->size * 2;  /* double buffer size */
  if (l_unlikely(MAX_SIZET - sz < B->n))  /* overflow in (B->n + sz)? */
    return nebulaL_error(B->L, "buffer too large");
  if (newsize < B->n + sz)  /* double is not big enough? */
    newsize = B->n + sz;
  return newsize;
}


/*
** Returns a pointer to a free area with at least 'sz' bytes in buffer
** 'B'. 'boxidx' is the relative position in the stack where is the
** buffer's box or its placeholder.
*/
static char *prepbuffsize (nebulaL_Buffer *B, size_t sz, int boxidx) {
  checkbufferlevel(B, boxidx);
  if (B->size - B->n >= sz)  /* enough space? */
    return B->b + B->n;
  else {
    nebula_State *L = B->L;
    char *newbuff;
    size_t newsize = newbuffsize(B, sz);
    /* create larger buffer */
    if (buffonstack(B))  /* buffer already has a box? */
      newbuff = (char *)resizebox(L, boxidx, newsize);  /* resize it */
    else {  /* no box yet */
      nebula_remove(L, boxidx);  /* remove placeholder */
      newbox(L);  /* create a new box */
      nebula_insert(L, boxidx);  /* move box to its intended position */
      nebula_toclose(L, boxidx);
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
NEBULALIB_API char *nebulaL_prepbuffsize (nebulaL_Buffer *B, size_t sz) {
  return prepbuffsize(B, sz, -1);
}


NEBULALIB_API void nebulaL_addlstring (nebulaL_Buffer *B, const char *s, size_t l) {
  if (l > 0) {  /* avoid 'memcpy' when 's' can be NULL */
    char *b = prepbuffsize(B, l, -1);
    memcpy(b, s, l * sizeof(char));
    nebulaL_addsize(B, l);
  }
}


NEBULALIB_API void nebulaL_addstring (nebulaL_Buffer *B, const char *s) {
  nebulaL_addlstring(B, s, strlen(s));
}


NEBULALIB_API void nebulaL_pushresult (nebulaL_Buffer *B) {
  nebula_State *L = B->L;
  checkbufferlevel(B, -1);
  nebula_pushlstring(L, B->b, B->n);
  if (buffonstack(B))
    nebula_closeslot(L, -2);  /* close the box */
  nebula_remove(L, -2);  /* remove box or placeholder from the stack */
}


NEBULALIB_API void nebulaL_pushresultsize (nebulaL_Buffer *B, size_t sz) {
  nebulaL_addsize(B, sz);
  nebulaL_pushresult(B);
}


/*
** 'nebulaL_addvalue' is the only function in the Buffer system where the
** box (if existent) is not on the top of the stack. So, instead of
** calling 'nebulaL_addlstring', it replicates the code using -2 as the
** last argument to 'prepbuffsize', signaling that the box is (or will
** be) bellow the string being added to the buffer. (Box creation can
** trigger an emergency GC, so we should not remove the string from the
** stack before we have the space guaranteed.)
*/
NEBULALIB_API void nebulaL_addvalue (nebulaL_Buffer *B) {
  nebula_State *L = B->L;
  size_t len;
  const char *s = nebula_tolstring(L, -1, &len);
  char *b = prepbuffsize(B, len, -2);
  memcpy(b, s, len * sizeof(char));
  nebulaL_addsize(B, len);
  nebula_pop(L, 1);  /* pop string */
}


NEBULALIB_API void nebulaL_buffinit (nebula_State *L, nebulaL_Buffer *B) {
  B->L = L;
  B->b = B->init.b;
  B->n = 0;
  B->size = NEBULAL_BUFFERSIZE;
  nebula_pushlightuserdata(L, (void*)B);  /* push placeholder */
}


NEBULALIB_API char *nebulaL_buffinitsize (nebula_State *L, nebulaL_Buffer *B, size_t sz) {
  nebulaL_buffinit(L, B);
  return prepbuffsize(B, sz, -1);
}

/* }====================================================== */


/*
** {======================================================
** Reference system
** =======================================================
*/

/* index of free-list header (after the predefined values) */
#define freelist	(NEBULA_RIDX_LAST + 1)

/*
** The previously freed references form a linked list:
** t[freelist] is the index of a first free index, or zero if list is
** empty; t[t[freelist]] is the index of the second element; etc.
*/
NEBULALIB_API int nebulaL_ref (nebula_State *L, int t) {
  int ref;
  if (nebula_isnil(L, -1)) {
    nebula_pop(L, 1);  /* remove from stack */
    return NEBULA_REFNIL;  /* 'nil' has a unique fixed reference */
  }
  t = nebula_absindex(L, t);
  if (nebula_rawgeti(L, t, freelist) == NEBULA_TNIL) {  /* first access? */
    ref = 0;  /* list is empty */
    nebula_pushinteger(L, 0);  /* initialize as an empty list */
    nebula_rawseti(L, t, freelist);  /* ref = t[freelist] = 0 */
  }
  else {  /* already initialized */
    nebula_assert(nebula_isinteger(L, -1));
    ref = (int)nebula_tointeger(L, -1);  /* ref = t[freelist] */
  }
  nebula_pop(L, 1);  /* remove element from stack */
  if (ref != 0) {  /* any free element? */
    nebula_rawgeti(L, t, ref);  /* remove it from list */
    nebula_rawseti(L, t, freelist);  /* (t[freelist] = t[ref]) */
  }
  else  /* no free elements */
    ref = (int)nebula_rawlen(L, t) + 1;  /* get a new reference */
  nebula_rawseti(L, t, ref);
  return ref;
}


NEBULALIB_API void nebulaL_unref (nebula_State *L, int t, int ref) {
  if (ref >= 0) {
    t = nebula_absindex(L, t);
    nebula_rawgeti(L, t, freelist);
    nebula_assert(nebula_isinteger(L, -1));
    nebula_rawseti(L, t, ref);  /* t[ref] = t[freelist] */
    nebula_pushinteger(L, ref);
    nebula_rawseti(L, t, freelist);  /* t[freelist] = ref */
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


static const char *getF (nebula_State *L, void *ud, size_t *size) {
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


static int errfile (nebula_State *L, const char *what, int fnameindex) {
  const char *serr = strerror(errno);
  const char *filename = nebula_tostring(L, fnameindex) + 1;
  nebula_pushfstring(L, "cannot %s %s: %s", what, filename, serr);
  nebula_remove(L, fnameindex);
  return NEBULA_ERRFILE;
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


NEBULALIB_API int nebulaL_loadfilex (nebula_State *L, const char *filename,
                                             const char *mode) {
  LoadF lf;
  int status, readstatus;
  int c;
  int fnameindex = nebula_gettop(L) + 1;  /* index of filename on the stack */
  if (filename == NULL) {
    nebula_pushliteral(L, "=stdin");
    lf.f = stdin;
  }
  else {
    nebula_pushfstring(L, "@%s", filename);
    lf.f = fopen(filename, "r");
    if (lf.f == NULL) return errfile(L, "open", fnameindex);
  }
  if (skipcomment(&lf, &c))  /* read initial portion */
    lf.buff[lf.n++] = '\n';  /* add line to correct line numbers */
  if (c == NEBULA_SIGNATURE[0] && filename) {  /* binary file? */
    lf.f = freopen(filename, "rb", lf.f);  /* reopen in binary mode */
    if (lf.f == NULL) return errfile(L, "reopen", fnameindex);
    skipcomment(&lf, &c);  /* re-read initial portion */
  }
  if (c != EOF)
    lf.buff[lf.n++] = c;  /* 'c' is the first character of the stream */
  status = nebula_load(L, getF, &lf, nebula_tostring(L, -1), mode);
  readstatus = ferror(lf.f);
  if (filename) fclose(lf.f);  /* close file (even in case of errors) */
  if (readstatus) {
    nebula_settop(L, fnameindex);  /* ignore results from 'nebula_load' */
    return errfile(L, "read", fnameindex);
  }
  nebula_remove(L, fnameindex);
  return status;
}


typedef struct LoadS {
  const char *s;
  size_t size;
} LoadS;


static const char *getS (nebula_State *L, void *ud, size_t *size) {
  LoadS *ls = (LoadS *)ud;
  (void)L;  /* not used */
  if (ls->size == 0) return NULL;
  *size = ls->size;
  ls->size = 0;
  return ls->s;
}


NEBULALIB_API int nebulaL_loadbufferx (nebula_State *L, const char *buff, size_t size,
                                 const char *name, const char *mode) {
  LoadS ls;
  ls.s = buff;
  ls.size = size;
  return nebula_load(L, getS, &ls, name, mode);
}


NEBULALIB_API int nebulaL_loadstring (nebula_State *L, const char *s) {
  return nebulaL_loadbuffer(L, s, strlen(s), s);
}

/* }====================================================== */



NEBULALIB_API int nebulaL_getmetafield (nebula_State *L, int obj, const char *event) {
  if (!nebula_getmetatable(L, obj))  /* no metatable? */
    return NEBULA_TNIL;
  else {
    int tt;
    nebula_pushstring(L, event);
    tt = nebula_rawget(L, -2);
    if (tt == NEBULA_TNIL)  /* is metafield nil? */
      nebula_pop(L, 2);  /* remove metatable and metafield */
    else
      nebula_remove(L, -2);  /* remove only metatable */
    return tt;  /* return metafield type */
  }
}


NEBULALIB_API int nebulaL_callmeta (nebula_State *L, int obj, const char *event) {
  obj = nebula_absindex(L, obj);
  if (nebulaL_getmetafield(L, obj, event) == NEBULA_TNIL)  /* no metafield? */
    return 0;
  nebula_pushvalue(L, obj);
  nebula_call(L, 1, 1);
  return 1;
}


NEBULALIB_API nebula_Integer nebulaL_len (nebula_State *L, int idx) {
  nebula_Integer l;
  int isnum;
  nebula_len(L, idx);
  l = nebula_tointegerx(L, -1, &isnum);
  if (l_unlikely(!isnum))
    nebulaL_error(L, "object length is not an integer");
  nebula_pop(L, 1);  /* remove object */
  return l;
}


NEBULALIB_API const char *nebulaL_tolstring (nebula_State *L, int idx, size_t *len) {
  idx = nebula_absindex(L,idx);
  if (nebulaL_callmeta(L, idx, "__tostring")) {  /* metafield? */
    if (!nebula_isstring(L, -1))
      nebulaL_error(L, "'__tostring' must return a string");
  }
  else {
    switch (nebula_type(L, idx)) {
      case NEBULA_TNUMBER: {
        if (nebula_isinteger(L, idx))
          nebula_pushfstring(L, "%I", (NEBULAI_UACINT)nebula_tointeger(L, idx));
        else
          nebula_pushfstring(L, "%f", (NEBULAI_UACNUMBER)nebula_tonumber(L, idx));
        break;
      }
      case NEBULA_TSTRING:
        nebula_pushvalue(L, idx);
        break;
      case NEBULA_TBOOLEAN:
        nebula_pushstring(L, (nebula_toboolean(L, idx) ? "true" : "false"));
        break;
      case NEBULA_TNIL:
        nebula_pushliteral(L, "nil");
        break;
      default: {
        int tt = nebulaL_getmetafield(L, idx, "__name");  /* try name */
        const char *kind = (tt == NEBULA_TSTRING) ? nebula_tostring(L, -1) :
                                                 nebulaL_typename(L, idx);
        nebula_pushfstring(L, "%s: %p", kind, nebula_topointer(L, idx));
        if (tt != NEBULA_TNIL)
          nebula_remove(L, -2);  /* remove '__name' */
        break;
      }
    }
  }
  return nebula_tolstring(L, -1, len);
}


/*
** set functions from list 'l' into table at top - 'nup'; each
** function gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/
NEBULALIB_API void nebulaL_setfuncs (nebula_State *L, const nebulaL_Reg *l, int nup) {
  nebulaL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    if (l->func == NULL)  /* place holder? */
      nebula_pushboolean(L, 0);
    else {
      int i;
      for (i = 0; i < nup; i++)  /* copy upvalues to the top */
        nebula_pushvalue(L, -nup);
      nebula_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    }
    nebula_setfield(L, -(nup + 2), l->name);
  }
  nebula_pop(L, nup);  /* remove upvalues */
}


/*
** ensure that stack[idx][fname] has a table and push that table
** into the stack
*/
NEBULALIB_API int nebulaL_getsubtable (nebula_State *L, int idx, const char *fname) {
  if (nebula_getfield(L, idx, fname) == NEBULA_TTABLE)
    return 1;  /* table already there */
  else {
    nebula_pop(L, 1);  /* remove previous result */
    idx = nebula_absindex(L, idx);
    nebula_newtable(L);
    nebula_pushvalue(L, -1);  /* copy to be left at top */
    nebula_setfield(L, idx, fname);  /* assign new table to field */
    return 0;  /* false, because did not find table there */
  }
}


/*
** Stripped-down 'require': After checking "loaded" table, calls 'openf'
** to open a module, registers the result in 'package.loaded' table and,
** if 'glb' is true, also registers the result in the global table.
** Leaves resulting module on the top.
*/
NEBULALIB_API void nebulaL_requiref (nebula_State *L, const char *modname,
                               nebula_CFunction openf, int glb) {
  nebulaL_getsubtable(L, NEBULA_REGISTRYINDEX, NEBULA_LOADED_TABLE);
  nebula_getfield(L, -1, modname);  /* LOADED[modname] */
  if (!nebula_toboolean(L, -1)) {  /* package not already loaded? */
    nebula_pop(L, 1);  /* remove field */
    nebula_pushcfunction(L, openf);
    nebula_pushstring(L, modname);  /* argument to open function */
    nebula_call(L, 1, 1);  /* call 'openf' to open module */
    nebula_pushvalue(L, -1);  /* make copy of module (call result) */
    nebula_setfield(L, -3, modname);  /* LOADED[modname] = module */
  }
  nebula_remove(L, -2);  /* remove LOADED table */
  if (glb) {
    nebula_pushvalue(L, -1);  /* copy of module */
    nebula_setglobal(L, modname);  /* _G[modname] = module */
  }
}


NEBULALIB_API void nebulaL_addgsub (nebulaL_Buffer *b, const char *s,
                                     const char *p, const char *r) {
  const char *wild;
  size_t l = strlen(p);
  while ((wild = strstr(s, p)) != NULL) {
    nebulaL_addlstring(b, s, wild - s);  /* push prefix */
    nebulaL_addstring(b, r);  /* push replacement in place of pattern */
    s = wild + l;  /* continue after 'p' */
  }
  nebulaL_addstring(b, s);  /* push last suffix */
}


NEBULALIB_API const char *nebulaL_gsub (nebula_State *L, const char *s,
                                  const char *p, const char *r) {
  nebulaL_Buffer b;
  nebulaL_buffinit(L, &b);
  nebulaL_addgsub(&b, s, p, r);
  nebulaL_pushresult(&b);
  return nebula_tostring(L, -1);
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


static int panic (nebula_State *L) {
  const char *msg = nebula_tostring(L, -1);
  if (msg == NULL) msg = "error object is not a string";
  nebula_writestringerror("PANIC: unprotected error in call to Nebula API (%s)\n",
                        msg);
  return 0;  /* return to Nebula to abort */
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
static int checkcontrol (nebula_State *L, const char *message, int tocont) {
  if (tocont || *(message++) != '@')  /* not a control message? */
    return 0;
  else {
    if (strcmp(message, "off") == 0)
      nebula_setwarnf(L, warnfoff, L);  /* turn warnings off */
    else if (strcmp(message, "on") == 0)
      nebula_setwarnf(L, warnfon, L);   /* turn warnings on */
    return 1;  /* it was a control message */
  }
}


static void warnfoff (void *ud, const char *message, int tocont) {
  checkcontrol((nebula_State *)ud, message, tocont);
}


/*
** Writes the message and handle 'tocont', finishing the message
** if needed and setting the next warn function.
*/
static void warnfcont (void *ud, const char *message, int tocont) {
  nebula_State *L = (nebula_State *)ud;
  nebula_writestringerror("%s", message);  /* write message */
  if (tocont)  /* not the last part? */
    nebula_setwarnf(L, warnfcont, L);  /* to be continued */
  else {  /* last part */
    nebula_writestringerror("%s", "\n");  /* finish message with end-of-line */
    nebula_setwarnf(L, warnfon, L);  /* next call is a new message */
  }
}


static void warnfon (void *ud, const char *message, int tocont) {
  if (checkcontrol((nebula_State *)ud, message, tocont))  /* control message? */
    return;  /* nothing else to be done */
  nebula_writestringerror("%s", "Nebula warning: ");  /* start a new warning */
  warnfcont(ud, message, tocont);  /* finish processing */
}


NEBULALIB_API nebula_State *nebulaL_newstate (void) {
  nebula_State *L = nebula_newstate(l_alloc, NULL);
  if (l_likely(L)) {
    nebula_atpanic(L, &panic);
    nebula_setwarnf(L, warnfoff, L);  /* default is warnings off */
  }
  return L;
}


NEBULALIB_API void nebulaL_checkversion_ (nebula_State *L, nebula_Number ver, size_t sz) {
  nebula_Number v = nebula_version(L);
  if (sz != NEBULAL_NUMSIZES)  /* check numeric types */
    nebulaL_error(L, "core and library have incompatible numeric types");
  else if (v != ver)
    nebulaL_error(L, "version mismatch: app. needs %f, Nebula core provides %f",
                  (NEBULAI_UACNUMBER)ver, (NEBULAI_UACNUMBER)v);
}