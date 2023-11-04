/*
** $Id: auxlib.c $
** Auxiliary functions for building Hydrogen libraries
** See Copyright Notice in hydrogen.h
*/

#define auxlib_c
#define HYDROGEN_LIB

#include "prefix.h"


#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
** This file uses only the official API of Hydrogen.
** Any function declared here could be written as an application function.
*/

#include "hydrogen.h"

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
** absolute index.) Return 1 + string at top if it found a Hydrogenod name.
*/
static int findfield (hydrogen_State *L, int objidx, int level) {
  if (level == 0 || !hydrogen_istable(L, -1))
    return 0;  /* not found */
  hydrogen_pushnil(L);  /* start 'next' loop */
  while (hydrogen_next(L, -2)) {  /* for each pair in table */
    if (hydrogen_type(L, -2) == HYDROGEN_TSTRING) {  /* ignore non-string keys */
      if (hydrogen_rawequal(L, objidx, -1)) {  /* found object? */
        hydrogen_pop(L, 1);  /* remove value (but keep name) */
        return 1;
      }
      else if (findfield(L, objidx, level - 1)) {  /* try recursively */
        /* stack: lib_name, lib_table, field_name (top) */
        hydrogen_pushliteral(L, ".");  /* place '.' between the two names */
        hydrogen_replace(L, -3);  /* (in the slot ochydrogenied by table) */
        hydrogen_concat(L, 3);  /* lib_name.field_name */
        return 1;
      }
    }
    hydrogen_pop(L, 1);  /* remove value */
  }
  return 0;  /* not found */
}


/*
** Search for a name for a function in all loaded modules
*/
static int pushglobalfuncname (hydrogen_State *L, hydrogen_Debug *ar) {
  int top = hydrogen_gettop(L);
  hydrogen_getinfo(L, "f", ar);  /* push function */
  hydrogen_getfield(L, HYDROGEN_REGISTRYINDEX, HYDROGEN_LOADED_TABLE);
  if (findfield(L, top + 1, 2)) {
    const char *name = hydrogen_tostring(L, -1);
    if (strncmp(name, HYDROGEN_GNAME ".", 3) == 0) {  /* name start with '_G.'? */
      hydrogen_pushstring(L, name + 3);  /* push name without prefix */
      hydrogen_remove(L, -2);  /* remove original name */
    }
    hydrogen_copy(L, -1, top + 1);  /* copy name to proper place */
    hydrogen_settop(L, top + 1);  /* remove table "loaded" and name copy */
    return 1;
  }
  else {
    hydrogen_settop(L, top);  /* remove function and global table */
    return 0;
  }
}


static void pushfuncname (hydrogen_State *L, hydrogen_Debug *ar) {
  if (pushglobalfuncname(L, ar)) {  /* try first a global name */
    hydrogen_pushfstring(L, "function '%s'", hydrogen_tostring(L, -1));
    hydrogen_remove(L, -2);  /* remove name */
  }
  else if (*ar->namewhat != '\0')  /* is there a name from code? */
    hydrogen_pushfstring(L, "%s '%s'", ar->namewhat, ar->name);  /* use it */
  else if (*ar->what == 'm')  /* main? */
      hydrogen_pushliteral(L, "main chunk");
  else if (*ar->what != 'C')  /* for Hydrogen functions, use <file:line> */
    hydrogen_pushfstring(L, "function <%s:%d>", ar->short_src, ar->linedefined);
  else  /* nothing left... */
    hydrogen_pushliteral(L, "?");
}


static int lastlevel (hydrogen_State *L) {
  hydrogen_Debug ar;
  int li = 1, le = 1;
  /* find an upper bound */
  while (hydrogen_getstack(L, le, &ar)) { li = le; le *= 2; }
  /* do a binary search */
  while (li < le) {
    int m = (li + le)/2;
    if (hydrogen_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}


HYDROGENLIB_API void hydrogenL_traceback (hydrogen_State *L, hydrogen_State *L1,
                                const char *msg, int level) {
  hydrogenL_Buffer b;
  hydrogen_Debug ar;
  int last = lastlevel(L1);
  int limit2show = (last - level > LEVELS1 + LEVELS2) ? LEVELS1 : -1;
  hydrogenL_buffinit(L, &b);
  if (msg) {
    hydrogenL_addstring(&b, msg);
    hydrogenL_addchar(&b, '\n');
  }
  hydrogenL_addstring(&b, "stack traceback:");
  while (hydrogen_getstack(L1, level++, &ar)) {
    if (limit2show-- == 0) {  /* too many levels? */
      int n = last - level - LEVELS2 + 1;  /* number of levels to skip */
      hydrogen_pushfstring(L, "\n\t...\t(skipping %d levels)", n);
      hydrogenL_addvalue(&b);  /* add warning about skip */
      level += n;  /* and skip to last levels */
    }
    else {
      hydrogen_getinfo(L1, "Slnt", &ar);
      if (ar.currentline <= 0)
        hydrogen_pushfstring(L, "\n\t%s: in ", ar.short_src);
      else
        hydrogen_pushfstring(L, "\n\t%s:%d: in ", ar.short_src, ar.currentline);
      hydrogenL_addvalue(&b);
      pushfuncname(L, &ar);
      hydrogenL_addvalue(&b);
      if (ar.istailcall)
        hydrogenL_addstring(&b, "\n\t(...tail calls...)");
    }
  }
  hydrogenL_pushresult(&b);
}

/* }====================================================== */


/*
** {======================================================
** Error-report functions
** =======================================================
*/

HYDROGENLIB_API int hydrogenL_argerror (hydrogen_State *L, int arg, const char *extramsg) {
  hydrogen_Debug ar;
  if (!hydrogen_getstack(L, 0, &ar))  /* no stack frame? */
    return hydrogenL_error(L, "bad argument #%d (%s)", arg, extramsg);
  hydrogen_getinfo(L, "n", &ar);
  if (strcmp(ar.namewhat, "method") == 0) {
    arg--;  /* do not count 'self' */
    if (arg == 0)  /* error is in the self argument itself? */
      return hydrogenL_error(L, "calling '%s' on bad self (%s)",
                           ar.name, extramsg);
  }
  if (ar.name == NULL)
    ar.name = (pushglobalfuncname(L, &ar)) ? hydrogen_tostring(L, -1) : "?";
  return hydrogenL_error(L, "bad argument #%d to '%s' (%s)",
                        arg, ar.name, extramsg);
}


HYDROGENLIB_API int hydrogenL_typeerror (hydrogen_State *L, int arg, const char *tname) {
  const char *msg;
  const char *typearg;  /* name for the type of the actual argument */
  if (hydrogenL_getmetafield(L, arg, "__name") == HYDROGEN_TSTRING)
    typearg = hydrogen_tostring(L, -1);  /* use the given type name */
  else if (hydrogen_type(L, arg) == HYDROGEN_TLIGHTUSERDATA)
    typearg = "light userdata";  /* special name for messages */
  else
    typearg = hydrogenL_typename(L, arg);  /* standard name */
  msg = hydrogen_pushfstring(L, "%s expected, Hydrogent %s", tname, typearg);
  return hydrogenL_argerror(L, arg, msg);
}


static void tag_error (hydrogen_State *L, int arg, int tag) {
  hydrogenL_typeerror(L, arg, hydrogen_typename(L, tag));
}


/*
** The use of 'hydrogen_pushfstring' ensures this function does not
** need reserved stack space when called.
*/
HYDROGENLIB_API void hydrogenL_where (hydrogen_State *L, int level) {
  hydrogen_Debug ar;
  if (hydrogen_getstack(L, level, &ar)) {  /* check function at level */
    hydrogen_getinfo(L, "Sl", &ar);  /* get info about it */
    if (ar.currentline > 0) {  /* is there info? */
      hydrogen_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline);
      return;
    }
  }
  hydrogen_pushfstring(L, "");  /* else, no information available... */
}


/*
** Again, the use of 'hydrogen_pushvfstring' ensures this function does
** not need reserved stack space when called. (At worst, it generates
** an error with "stack overflow" instead of the given message.)
*/
HYDROGENLIB_API int hydrogenL_error (hydrogen_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  hydrogenL_where(L, 1);
  hydrogen_pushvfstring(L, fmt, argp);
  va_end(argp);
  hydrogen_concat(L, 2);
  return hydrogen_error(L);
}


HYDROGENLIB_API int hydrogenL_fileresult (hydrogen_State *L, int stat, const char *fname) {
  int en = errno;  /* calls to Hydrogen API may change this value */
  if (stat) {
    hydrogen_pushboolean(L, 1);
    return 1;
  }
  else {
    hydrogenL_pushfail(L);
    if (fname)
      hydrogen_pushfstring(L, "%s: %s", fname, strerror(en));
    else
      hydrogen_pushstring(L, strerror(en));
    hydrogen_pushinteger(L, en);
    return 3;
  }
}


#if !defined(l_inspectstat)	/* { */

#if defined(HYDROGEN_USE_POSIX)

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


HYDROGENLIB_API int hydrogenL_execresult (hydrogen_State *L, int stat) {
  if (stat != 0 && errno != 0)  /* error with an 'errno'? */
    return hydrogenL_fileresult(L, 0, NULL);
  else {
    const char *what = "exit";  /* type of termination */
    l_inspectstat(stat, what);  /* interpret result */
    if (*what == 'e' && stat == 0)  /* successful termination? */
      hydrogen_pushboolean(L, 1);
    else
      hydrogenL_pushfail(L);
    hydrogen_pushstring(L, what);
    hydrogen_pushinteger(L, stat);
    return 3;  /* return true/fail,what,code */
  }
}

/* }====================================================== */



/*
** {======================================================
** Userdata's metatable manipulation
** =======================================================
*/

HYDROGENLIB_API int hydrogenL_newmetatable (hydrogen_State *L, const char *tname) {
  if (hydrogenL_getmetatable(L, tname) != HYDROGEN_TNIL)  /* name already in use? */
    return 0;  /* leave previous value on top, but return 0 */
  hydrogen_pop(L, 1);
  hydrogen_createtable(L, 0, 2);  /* create metatable */
  hydrogen_pushstring(L, tname);
  hydrogen_setfield(L, -2, "__name");  /* metatable.__name = tname */
  hydrogen_pushvalue(L, -1);
  hydrogen_setfield(L, HYDROGEN_REGISTRYINDEX, tname);  /* registry.name = metatable */
  return 1;
}


HYDROGENLIB_API void hydrogenL_setmetatable (hydrogen_State *L, const char *tname) {
  hydrogenL_getmetatable(L, tname);
  hydrogen_setmetatable(L, -2);
}


HYDROGENLIB_API void *hydrogenL_testudata (hydrogen_State *L, int ud, const char *tname) {
  void *p = hydrogen_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    if (hydrogen_getmetatable(L, ud)) {  /* does it have a metatable? */
      hydrogenL_getmetatable(L, tname);  /* get correct metatable */
      if (!hydrogen_rawequal(L, -1, -2))  /* not the same? */
        p = NULL;  /* value is a userdata with wrong metatable */
      hydrogen_pop(L, 2);  /* remove both metatables */
      return p;
    }
  }
  return NULL;  /* value is not a userdata with a metatable */
}


HYDROGENLIB_API void *hydrogenL_checkudata (hydrogen_State *L, int ud, const char *tname) {
  void *p = hydrogenL_testudata(L, ud, tname);
  hydrogenL_argexpected(L, p != NULL, ud, tname);
  return p;
}

/* }====================================================== */


/*
** {======================================================
** Argument check functions
** =======================================================
*/

HYDROGENLIB_API int hydrogenL_checkoption (hydrogen_State *L, int arg, const char *def,
                                 const char *const lst[]) {
  const char *name = (def) ? hydrogenL_optstring(L, arg, def) :
                             hydrogenL_checkstring(L, arg);
  int i;
  for (i=0; lst[i]; i++)
    if (strcmp(lst[i], name) == 0)
      return i;
  return hydrogenL_argerror(L, arg,
                       hydrogen_pushfstring(L, "invalid option '%s'", name));
}


/*
** Ensures the stack has at least 'space' extra slots, raising an error
** if it cannot fulfill the request. (The error handling needs a few
** extra slots to format the error message. In case of an error without
** this extra space, Hydrogen will generate the same 'stack overflow' error,
** but without 'msg'.)
*/
HYDROGENLIB_API void hydrogenL_checkstack (hydrogen_State *L, int space, const char *msg) {
  if (l_unlikely(!hydrogen_checkstack(L, space))) {
    if (msg)
      hydrogenL_error(L, "stack overflow (%s)", msg);
    else
      hydrogenL_error(L, "stack overflow");
  }
}


HYDROGENLIB_API void hydrogenL_checktype (hydrogen_State *L, int arg, int t) {
  if (l_unlikely(hydrogen_type(L, arg) != t))
    tag_error(L, arg, t);
}


HYDROGENLIB_API void hydrogenL_checkany (hydrogen_State *L, int arg) {
  if (l_unlikely(hydrogen_type(L, arg) == HYDROGEN_TNONE))
    hydrogenL_argerror(L, arg, "value expected");
}


HYDROGENLIB_API const char *hydrogenL_checklstring (hydrogen_State *L, int arg, size_t *len) {
  const char *s = hydrogen_tolstring(L, arg, len);
  if (l_unlikely(!s)) tag_error(L, arg, HYDROGEN_TSTRING);
  return s;
}


HYDROGENLIB_API const char *hydrogenL_optlstring (hydrogen_State *L, int arg,
                                        const char *def, size_t *len) {
  if (hydrogen_isnoneornil(L, arg)) {
    if (len)
      *len = (def ? strlen(def) : 0);
    return def;
  }
  else return hydrogenL_checklstring(L, arg, len);
}


HYDROGENLIB_API hydrogen_Number hydrogenL_checknumber (hydrogen_State *L, int arg) {
  int isnum;
  hydrogen_Number d = hydrogen_tonumberx(L, arg, &isnum);
  if (l_unlikely(!isnum))
    tag_error(L, arg, HYDROGEN_TNUMBER);
  return d;
}


HYDROGENLIB_API hydrogen_Number hydrogenL_optnumber (hydrogen_State *L, int arg, hydrogen_Number def) {
  return hydrogenL_opt(L, hydrogenL_checknumber, arg, def);
}


static void interror (hydrogen_State *L, int arg) {
  if (hydrogen_isnumber(L, arg))
    hydrogenL_argerror(L, arg, "number has no integer representation");
  else
    tag_error(L, arg, HYDROGEN_TNUMBER);
}


HYDROGENLIB_API hydrogen_Integer hydrogenL_checkinteger (hydrogen_State *L, int arg) {
  int isnum;
  hydrogen_Integer d = hydrogen_tointegerx(L, arg, &isnum);
  if (l_unlikely(!isnum)) {
    interror(L, arg);
  }
  return d;
}


HYDROGENLIB_API hydrogen_Integer hydrogenL_optinteger (hydrogen_State *L, int arg,
                                                      hydrogen_Integer def) {
  return hydrogenL_opt(L, hydrogenL_checkinteger, arg, def);
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


static void *resizebox (hydrogen_State *L, int idx, size_t newsize) {
  void *ud;
  hydrogen_Alloc allocf = hydrogen_getallocf(L, &ud);
  UBox *box = (UBox *)hydrogen_touserdata(L, idx);
  void *temp = allocf(ud, box->box, box->bsize, newsize);
  if (l_unlikely(temp == NULL && newsize > 0)) {  /* allocation error? */
    hydrogen_pushliteral(L, "not enough memory");
    hydrogen_error(L);  /* raise a memory error */
  }
  box->box = temp;
  box->bsize = newsize;
  return temp;
}


static int boxgc (hydrogen_State *L) {
  resizebox(L, 1, 0);
  return 0;
}


static const hydrogenL_Reg boxmt[] = {  /* box metamethods */
  {"__gc", boxgc},
  {"__close", boxgc},
  {NULL, NULL}
};


static void newbox (hydrogen_State *L) {
  UBox *box = (UBox *)hydrogen_newuserdatauv(L, sizeof(UBox), 0);
  box->box = NULL;
  box->bsize = 0;
  if (hydrogenL_newmetatable(L, "_UBOX*"))  /* creating metatable? */
    hydrogenL_setfuncs(L, boxmt, 0);  /* set its metamethods */
  hydrogen_setmetatable(L, -2);
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
  hydrogen_assert(buffonstack(B) ? hydrogen_touserdata(B->L, idx) != NULL  \
                            : hydrogen_touserdata(B->L, idx) == (void*)B)


/*
** Compute new size for buffer 'B', enough to accommodate extra 'sz'
** bytes.
*/
static size_t newbuffsize (hydrogenL_Buffer *B, size_t sz) {
  size_t newsize = B->size * 2;  /* double buffer size */
  if (l_unlikely(MAX_SIZET - sz < B->n))  /* overflow in (B->n + sz)? */
    return hydrogenL_error(B->L, "buffer too large");
  if (newsize < B->n + sz)  /* double is not big enough? */
    newsize = B->n + sz;
  return newsize;
}


/*
** Returns a pointer to a free area with at least 'sz' bytes in buffer
** 'B'. 'boxidx' is the relative position in the stack where is the
** buffer's box or its placeholder.
*/
static char *prepbuffsize (hydrogenL_Buffer *B, size_t sz, int boxidx) {
  checkbufferlevel(B, boxidx);
  if (B->size - B->n >= sz)  /* enough space? */
    return B->b + B->n;
  else {
    hydrogen_State *L = B->L;
    char *newbuff;
    size_t newsize = newbuffsize(B, sz);
    /* create larger buffer */
    if (buffonstack(B))  /* buffer already has a box? */
      newbuff = (char *)resizebox(L, boxidx, newsize);  /* resize it */
    else {  /* no box yet */
      hydrogen_remove(L, boxidx);  /* remove placeholder */
      newbox(L);  /* create a new box */
      hydrogen_insert(L, boxidx);  /* move box to its intended position */
      hydrogen_toclose(L, boxidx);
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
HYDROGENLIB_API char *hydrogenL_prepbuffsize (hydrogenL_Buffer *B, size_t sz) {
  return prepbuffsize(B, sz, -1);
}


HYDROGENLIB_API void hydrogenL_addlstring (hydrogenL_Buffer *B, const char *s, size_t l) {
  if (l > 0) {  /* avoid 'memcpy' when 's' can be NULL */
    char *b = prepbuffsize(B, l, -1);
    memcpy(b, s, l * sizeof(char));
    hydrogenL_addsize(B, l);
  }
}


HYDROGENLIB_API void hydrogenL_addstring (hydrogenL_Buffer *B, const char *s) {
  hydrogenL_addlstring(B, s, strlen(s));
}


HYDROGENLIB_API void hydrogenL_pushresult (hydrogenL_Buffer *B) {
  hydrogen_State *L = B->L;
  checkbufferlevel(B, -1);
  hydrogen_pushlstring(L, B->b, B->n);
  if (buffonstack(B))
    hydrogen_closeslot(L, -2);  /* close the box */
  hydrogen_remove(L, -2);  /* remove box or placeholder from the stack */
}


HYDROGENLIB_API void hydrogenL_pushresultsize (hydrogenL_Buffer *B, size_t sz) {
  hydrogenL_addsize(B, sz);
  hydrogenL_pushresult(B);
}


/*
** 'hydrogenL_addvalue' is the only function in the Buffer system where the
** box (if existent) is not on the top of the stack. So, instead of
** calling 'hydrogenL_addlstring', it replicates the code using -2 as the
** last argument to 'prepbuffsize', signaling that the box is (or will
** be) bellow the string being added to the buffer. (Box creation can
** trigger an emergency GC, so we should not remove the string from the
** stack before we have the space guaranteed.)
*/
HYDROGENLIB_API void hydrogenL_addvalue (hydrogenL_Buffer *B) {
  hydrogen_State *L = B->L;
  size_t len;
  const char *s = hydrogen_tolstring(L, -1, &len);
  char *b = prepbuffsize(B, len, -2);
  memcpy(b, s, len * sizeof(char));
  hydrogenL_addsize(B, len);
  hydrogen_pop(L, 1);  /* pop string */
}


HYDROGENLIB_API void hydrogenL_buffinit (hydrogen_State *L, hydrogenL_Buffer *B) {
  B->L = L;
  B->b = B->init.b;
  B->n = 0;
  B->size = HYDROGENL_BUFFERSIZE;
  hydrogen_pushlightuserdata(L, (void*)B);  /* push placeholder */
}


HYDROGENLIB_API char *hydrogenL_buffinitsize (hydrogen_State *L, hydrogenL_Buffer *B, size_t sz) {
  hydrogenL_buffinit(L, B);
  return prepbuffsize(B, sz, -1);
}

/* }====================================================== */


/*
** {======================================================
** Reference system
** =======================================================
*/

/* index of free-list header (after the predefined values) */
#define freelist	(HYDROGEN_RIDX_LAST + 1)

/*
** The previously freed references form a linked list:
** t[freelist] is the index of a first free index, or zero if list is
** empty; t[t[freelist]] is the index of the second element; etc.
*/
HYDROGENLIB_API int hydrogenL_ref (hydrogen_State *L, int t) {
  int ref;
  if (hydrogen_isnil(L, -1)) {
    hydrogen_pop(L, 1);  /* remove from stack */
    return HYDROGEN_REFNIL;  /* 'nil' has a unique fixed reference */
  }
  t = hydrogen_absindex(L, t);
  if (hydrogen_rawgeti(L, t, freelist) == HYDROGEN_TNIL) {  /* first access? */
    ref = 0;  /* list is empty */
    hydrogen_pushinteger(L, 0);  /* initialize as an empty list */
    hydrogen_rawseti(L, t, freelist);  /* ref = t[freelist] = 0 */
  }
  else {  /* already initialized */
    hydrogen_assert(hydrogen_isinteger(L, -1));
    ref = (int)hydrogen_tointeger(L, -1);  /* ref = t[freelist] */
  }
  hydrogen_pop(L, 1);  /* remove element from stack */
  if (ref != 0) {  /* any free element? */
    hydrogen_rawgeti(L, t, ref);  /* remove it from list */
    hydrogen_rawseti(L, t, freelist);  /* (t[freelist] = t[ref]) */
  }
  else  /* no free elements */
    ref = (int)hydrogen_rawlen(L, t) + 1;  /* get a new reference */
  hydrogen_rawseti(L, t, ref);
  return ref;
}


HYDROGENLIB_API void hydrogenL_unref (hydrogen_State *L, int t, int ref) {
  if (ref >= 0) {
    t = hydrogen_absindex(L, t);
    hydrogen_rawgeti(L, t, freelist);
    hydrogen_assert(hydrogen_isinteger(L, -1));
    hydrogen_rawseti(L, t, ref);  /* t[ref] = t[freelist] */
    hydrogen_pushinteger(L, ref);
    hydrogen_rawseti(L, t, freelist);  /* t[freelist] = ref */
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


static const char *getF (hydrogen_State *L, void *ud, size_t *size) {
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


static int errfile (hydrogen_State *L, const char *what, int fnameindex) {
  const char *serr = strerror(errno);
  const char *filename = hydrogen_tostring(L, fnameindex) + 1;
  hydrogen_pushfstring(L, "cannot %s %s: %s", what, filename, serr);
  hydrogen_remove(L, fnameindex);
  return HYDROGEN_ERRFILE;
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


HYDROGENLIB_API int hydrogenL_loadfilex (hydrogen_State *L, const char *filename,
                                             const char *mode) {
  LoadF lf;
  int status, readstatus;
  int c;
  int fnameindex = hydrogen_gettop(L) + 1;  /* index of filename on the stack */
  if (filename == NULL) {
    hydrogen_pushliteral(L, "=stdin");
    lf.f = stdin;
  }
  else {
    hydrogen_pushfstring(L, "@%s", filename);
    lf.f = fopen(filename, "r");
    if (lf.f == NULL) return errfile(L, "open", fnameindex);
  }
  if (skipcomment(&lf, &c))  /* read initial portion */
    lf.buff[lf.n++] = '\n';  /* add line to correct line numbers */
  if (c == HYDROGEN_SIGNATURE[0] && filename) {  /* binary file? */
    lf.f = freopen(filename, "rb", lf.f);  /* reopen in binary mode */
    if (lf.f == NULL) return errfile(L, "reopen", fnameindex);
    skipcomment(&lf, &c);  /* re-read initial portion */
  }
  if (c != EOF)
    lf.buff[lf.n++] = c;  /* 'c' is the first character of the stream */
  status = hydrogen_load(L, getF, &lf, hydrogen_tostring(L, -1), mode);
  readstatus = ferror(lf.f);
  if (filename) fclose(lf.f);  /* close file (even in case of errors) */
  if (readstatus) {
    hydrogen_settop(L, fnameindex);  /* ignore results from 'hydrogen_load' */
    return errfile(L, "read", fnameindex);
  }
  hydrogen_remove(L, fnameindex);
  return status;
}


typedef struct LoadS {
  const char *s;
  size_t size;
} LoadS;


static const char *getS (hydrogen_State *L, void *ud, size_t *size) {
  LoadS *ls = (LoadS *)ud;
  (void)L;  /* not used */
  if (ls->size == 0) return NULL;
  *size = ls->size;
  ls->size = 0;
  return ls->s;
}


HYDROGENLIB_API int hydrogenL_loadbufferx (hydrogen_State *L, const char *buff, size_t size,
                                 const char *name, const char *mode) {
  LoadS ls;
  ls.s = buff;
  ls.size = size;
  return hydrogen_load(L, getS, &ls, name, mode);
}


HYDROGENLIB_API int hydrogenL_loadstring (hydrogen_State *L, const char *s) {
  return hydrogenL_loadbuffer(L, s, strlen(s), s);
}

/* }====================================================== */



HYDROGENLIB_API int hydrogenL_getmetafield (hydrogen_State *L, int obj, const char *event) {
  if (!hydrogen_getmetatable(L, obj))  /* no metatable? */
    return HYDROGEN_TNIL;
  else {
    int tt;
    hydrogen_pushstring(L, event);
    tt = hydrogen_rawget(L, -2);
    if (tt == HYDROGEN_TNIL)  /* is metafield nil? */
      hydrogen_pop(L, 2);  /* remove metatable and metafield */
    else
      hydrogen_remove(L, -2);  /* remove only metatable */
    return tt;  /* return metafield type */
  }
}


HYDROGENLIB_API int hydrogenL_callmeta (hydrogen_State *L, int obj, const char *event) {
  obj = hydrogen_absindex(L, obj);
  if (hydrogenL_getmetafield(L, obj, event) == HYDROGEN_TNIL)  /* no metafield? */
    return 0;
  hydrogen_pushvalue(L, obj);
  hydrogen_call(L, 1, 1);
  return 1;
}


HYDROGENLIB_API hydrogen_Integer hydrogenL_len (hydrogen_State *L, int idx) {
  hydrogen_Integer l;
  int isnum;
  hydrogen_len(L, idx);
  l = hydrogen_tointegerx(L, -1, &isnum);
  if (l_unlikely(!isnum))
    hydrogenL_error(L, "object length is not an integer");
  hydrogen_pop(L, 1);  /* remove object */
  return l;
}


HYDROGENLIB_API const char *hydrogenL_tolstring (hydrogen_State *L, int idx, size_t *len) {
  idx = hydrogen_absindex(L,idx);
  if (hydrogenL_callmeta(L, idx, "__tostring")) {  /* metafield? */
    if (!hydrogen_isstring(L, -1))
      hydrogenL_error(L, "'__tostring' must return a string");
  }
  else {
    switch (hydrogen_type(L, idx)) {
      case HYDROGEN_TNUMBER: {
        if (hydrogen_isinteger(L, idx))
          hydrogen_pushfstring(L, "%I", (HYDROGENI_UACINT)hydrogen_tointeger(L, idx));
        else
          hydrogen_pushfstring(L, "%f", (HYDROGENI_UACNUMBER)hydrogen_tonumber(L, idx));
        break;
      }
      case HYDROGEN_TSTRING:
        hydrogen_pushvalue(L, idx);
        break;
      case HYDROGEN_TBOOLEAN:
        hydrogen_pushstring(L, (hydrogen_toboolean(L, idx) ? "true" : "false"));
        break;
      case HYDROGEN_TNIL:
        hydrogen_pushliteral(L, "nil");
        break;
      default: {
        int tt = hydrogenL_getmetafield(L, idx, "__name");  /* try name */
        const char *kind = (tt == HYDROGEN_TSTRING) ? hydrogen_tostring(L, -1) :
                                                 hydrogenL_typename(L, idx);
        hydrogen_pushfstring(L, "%s: %p", kind, hydrogen_topointer(L, idx));
        if (tt != HYDROGEN_TNIL)
          hydrogen_remove(L, -2);  /* remove '__name' */
        break;
      }
    }
  }
  return hydrogen_tolstring(L, -1, len);
}


/*
** set functions from list 'l' into table at top - 'nup'; each
** function gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/
HYDROGENLIB_API void hydrogenL_setfuncs (hydrogen_State *L, const hydrogenL_Reg *l, int nup) {
  hydrogenL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    if (l->func == NULL)  /* place holder? */
      hydrogen_pushboolean(L, 0);
    else {
      int i;
      for (i = 0; i < nup; i++)  /* copy upvalues to the top */
        hydrogen_pushvalue(L, -nup);
      hydrogen_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    }
    hydrogen_setfield(L, -(nup + 2), l->name);
  }
  hydrogen_pop(L, nup);  /* remove upvalues */
}


/*
** ensure that stack[idx][fname] has a table and push that table
** into the stack
*/
HYDROGENLIB_API int hydrogenL_getsubtable (hydrogen_State *L, int idx, const char *fname) {
  if (hydrogen_getfield(L, idx, fname) == HYDROGEN_TTABLE)
    return 1;  /* table already there */
  else {
    hydrogen_pop(L, 1);  /* remove previous result */
    idx = hydrogen_absindex(L, idx);
    hydrogen_newtable(L);
    hydrogen_pushvalue(L, -1);  /* copy to be left at top */
    hydrogen_setfield(L, idx, fname);  /* assign new table to field */
    return 0;  /* false, because did not find table there */
  }
}


/*
** Stripped-down 'require': After checking "loaded" table, calls 'openf'
** to open a module, registers the result in 'package.loaded' table and,
** if 'glb' is true, also registers the result in the global table.
** Leaves resulting module on the top.
*/
HYDROGENLIB_API void hydrogenL_requiref (hydrogen_State *L, const char *modname,
                               hydrogen_CFunction openf, int glb) {
  hydrogenL_getsubtable(L, HYDROGEN_REGISTRYINDEX, HYDROGEN_LOADED_TABLE);
  hydrogen_getfield(L, -1, modname);  /* LOADED[modname] */
  if (!hydrogen_toboolean(L, -1)) {  /* package not already loaded? */
    hydrogen_pop(L, 1);  /* remove field */
    hydrogen_pushcfunction(L, openf);
    hydrogen_pushstring(L, modname);  /* argument to open function */
    hydrogen_call(L, 1, 1);  /* call 'openf' to open module */
    hydrogen_pushvalue(L, -1);  /* make copy of module (call result) */
    hydrogen_setfield(L, -3, modname);  /* LOADED[modname] = module */
  }
  hydrogen_remove(L, -2);  /* remove LOADED table */
  if (glb) {
    hydrogen_pushvalue(L, -1);  /* copy of module */
    hydrogen_setglobal(L, modname);  /* _G[modname] = module */
  }
}


HYDROGENLIB_API void hydrogenL_addgsub (hydrogenL_Buffer *b, const char *s,
                                     const char *p, const char *r) {
  const char *wild;
  size_t l = strlen(p);
  while ((wild = strstr(s, p)) != NULL) {
    hydrogenL_addlstring(b, s, wild - s);  /* push prefix */
    hydrogenL_addstring(b, r);  /* push replacement in place of pattern */
    s = wild + l;  /* continue after 'p' */
  }
  hydrogenL_addstring(b, s);  /* push last suffix */
}


HYDROGENLIB_API const char *hydrogenL_gsub (hydrogen_State *L, const char *s,
                                  const char *p, const char *r) {
  hydrogenL_Buffer b;
  hydrogenL_buffinit(L, &b);
  hydrogenL_addgsub(&b, s, p, r);
  hydrogenL_pushresult(&b);
  return hydrogen_tostring(L, -1);
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


static int panic (hydrogen_State *L) {
  const char *msg = hydrogen_tostring(L, -1);
  if (msg == NULL) msg = "error object is not a string";
  hydrogen_writestringerror("PANIC: unprotected error in call to Hydrogen API (%s)\n",
                        msg);
  return 0;  /* return to Hydrogen to abort */
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
static int checkcontrol (hydrogen_State *L, const char *message, int tocont) {
  if (tocont || *(message++) != '@')  /* not a control message? */
    return 0;
  else {
    if (strcmp(message, "off") == 0)
      hydrogen_setwarnf(L, warnfoff, L);  /* turn warnings off */
    else if (strcmp(message, "on") == 0)
      hydrogen_setwarnf(L, warnfon, L);   /* turn warnings on */
    return 1;  /* it was a control message */
  }
}


static void warnfoff (void *ud, const char *message, int tocont) {
  checkcontrol((hydrogen_State *)ud, message, tocont);
}


/*
** Writes the message and handle 'tocont', finishing the message
** if needed and setting the next warn function.
*/
static void warnfcont (void *ud, const char *message, int tocont) {
  hydrogen_State *L = (hydrogen_State *)ud;
  hydrogen_writestringerror("%s", message);  /* write message */
  if (tocont)  /* not the last part? */
    hydrogen_setwarnf(L, warnfcont, L);  /* to be continued */
  else {  /* last part */
    hydrogen_writestringerror("%s", "\n");  /* finish message with end-of-line */
    hydrogen_setwarnf(L, warnfon, L);  /* next call is a new message */
  }
}


static void warnfon (void *ud, const char *message, int tocont) {
  if (checkcontrol((hydrogen_State *)ud, message, tocont))  /* control message? */
    return;  /* nothing else to be done */
  hydrogen_writestringerror("%s", "Hydrogen warning: ");  /* start a new warning */
  warnfcont(ud, message, tocont);  /* finish processing */
}


HYDROGENLIB_API hydrogen_State *hydrogenL_newstate (void) {
  hydrogen_State *L = hydrogen_newstate(l_alloc, NULL);
  if (l_likely(L)) {
    hydrogen_atpanic(L, &panic);
    hydrogen_setwarnf(L, warnfoff, L);  /* default is warnings off */
  }
  return L;
}


HYDROGENLIB_API void hydrogenL_checkversion_ (hydrogen_State *L, hydrogen_Number ver, size_t sz) {
  hydrogen_Number v = hydrogen_version(L);
  if (sz != HYDROGENL_NUMSIZES)  /* check numeric types */
    hydrogenL_error(L, "core and library have incompatible numeric types");
  else if (v != ver)
    hydrogenL_error(L, "version mismatch: app. needs %f, Hydrogen core provides %f",
                  (HYDROGENI_UACNUMBER)ver, (HYDROGENI_UACNUMBER)v);
}