/*
** $Id: lauxlib.c $
** Auxiliary functions for building Cup libraries
** See Copyright Notice in cup.h
*/

#define lauxlib_c
#define CUP_LIB

#include "lprefix.h"


#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
** This file uses only the official API of Cup.
** Any function declared here could be written as an application function.
*/

#include "cup.h"

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
** absolute index.) Return 1 + string at top if it found a Cupod name.
*/
static int findfield (cup_State *L, int objidx, int level) {
  if (level == 0 || !cup_istable(L, -1))
    return 0;  /* not found */
  cup_pushnil(L);  /* start 'next' loop */
  while (cup_next(L, -2)) {  /* for each pair in table */
    if (cup_type(L, -2) == CUP_TSTRING) {  /* ignore non-string keys */
      if (cup_rawequal(L, objidx, -1)) {  /* found object? */
        cup_pop(L, 1);  /* remove value (but keep name) */
        return 1;
      }
      else if (findfield(L, objidx, level - 1)) {  /* try recursively */
        /* stack: lib_name, lib_table, field_name (top) */
        cup_pushliteral(L, ".");  /* place '.' between the two names */
        cup_replace(L, -3);  /* (in the slot occupied by table) */
        cup_concat(L, 3);  /* lib_name.field_name */
        return 1;
      }
    }
    cup_pop(L, 1);  /* remove value */
  }
  return 0;  /* not found */
}


/*
** Search for a name for a function in all loaded modules
*/
static int pushglobalfuncname (cup_State *L, cup_Debug *ar) {
  int top = cup_gettop(L);
  cup_getinfo(L, "f", ar);  /* push function */
  cup_getfield(L, CUP_REGISTRYINDEX, CUP_LOADED_TABLE);
  if (findfield(L, top + 1, 2)) {
    const char *name = cup_tostring(L, -1);
    if (strncmp(name, CUP_GNAME ".", 3) == 0) {  /* name start with '_G.'? */
      cup_pushstring(L, name + 3);  /* push name without prefix */
      cup_remove(L, -2);  /* remove original name */
    }
    cup_copy(L, -1, top + 1);  /* copy name to proper place */
    cup_settop(L, top + 1);  /* remove table "loaded" and name copy */
    return 1;
  }
  else {
    cup_settop(L, top);  /* remove function and global table */
    return 0;
  }
}


static void pushfuncname (cup_State *L, cup_Debug *ar) {
  if (pushglobalfuncname(L, ar)) {  /* try first a global name */
    cup_pushfstring(L, "function '%s'", cup_tostring(L, -1));
    cup_remove(L, -2);  /* remove name */
  }
  else if (*ar->namewhat != '\0')  /* is there a name from code? */
    cup_pushfstring(L, "%s '%s'", ar->namewhat, ar->name);  /* use it */
  else if (*ar->what == 'm')  /* main? */
      cup_pushliteral(L, "main chunk");
  else if (*ar->what != 'C')  /* for Cup functions, use <file:line> */
    cup_pushfstring(L, "function <%s:%d>", ar->short_src, ar->linedefined);
  else  /* nothing left... */
    cup_pushliteral(L, "?");
}


static int lastlevel (cup_State *L) {
  cup_Debug ar;
  int li = 1, le = 1;
  /* find an upper bound */
  while (cup_getstack(L, le, &ar)) { li = le; le *= 2; }
  /* do a binary search */
  while (li < le) {
    int m = (li + le)/2;
    if (cup_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}


CUPLIB_API void cupL_traceback (cup_State *L, cup_State *L1,
                                const char *msg, int level) {
  cupL_Buffer b;
  cup_Debug ar;
  int last = lastlevel(L1);
  int limit2show = (last - level > LEVELS1 + LEVELS2) ? LEVELS1 : -1;
  cupL_buffinit(L, &b);
  if (msg) {
    cupL_addstring(&b, msg);
    cupL_addchar(&b, '\n');
  }
  cupL_addstring(&b, "stack traceback:");
  while (cup_getstack(L1, level++, &ar)) {
    if (limit2show-- == 0) {  /* too many levels? */
      int n = last - level - LEVELS2 + 1;  /* number of levels to skip */
      cup_pushfstring(L, "\n\t...\t(skipping %d levels)", n);
      cupL_addvalue(&b);  /* add warning about skip */
      level += n;  /* and skip to last levels */
    }
    else {
      cup_getinfo(L1, "Slnt", &ar);
      if (ar.currentline <= 0)
        cup_pushfstring(L, "\n\t%s: in ", ar.short_src);
      else
        cup_pushfstring(L, "\n\t%s:%d: in ", ar.short_src, ar.currentline);
      cupL_addvalue(&b);
      pushfuncname(L, &ar);
      cupL_addvalue(&b);
      if (ar.istailcall)
        cupL_addstring(&b, "\n\t(...tail calls...)");
    }
  }
  cupL_pushresult(&b);
}

/* }====================================================== */


/*
** {======================================================
** Error-report functions
** =======================================================
*/

CUPLIB_API int cupL_argerror (cup_State *L, int arg, const char *extramsg) {
  cup_Debug ar;
  if (!cup_getstack(L, 0, &ar))  /* no stack frame? */
    return cupL_error(L, "bad argument #%d (%s)", arg, extramsg);
  cup_getinfo(L, "n", &ar);
  if (strcmp(ar.namewhat, "method") == 0) {
    arg--;  /* do not count 'self' */
    if (arg == 0)  /* error is in the self argument itself? */
      return cupL_error(L, "calling '%s' on bad self (%s)",
                           ar.name, extramsg);
  }
  if (ar.name == NULL)
    ar.name = (pushglobalfuncname(L, &ar)) ? cup_tostring(L, -1) : "?";
  return cupL_error(L, "bad argument #%d to '%s' (%s)",
                        arg, ar.name, extramsg);
}


CUPLIB_API int cupL_typeerror (cup_State *L, int arg, const char *tname) {
  const char *msg;
  const char *typearg;  /* name for the type of the actual argument */
  if (cupL_getmetafield(L, arg, "__name") == CUP_TSTRING)
    typearg = cup_tostring(L, -1);  /* use the given type name */
  else if (cup_type(L, arg) == CUP_TLIGHTUSERDATA)
    typearg = "light userdata";  /* special name for messages */
  else
    typearg = cupL_typename(L, arg);  /* standard name */
  msg = cup_pushfstring(L, "%s expected, Cupt %s", tname, typearg);
  return cupL_argerror(L, arg, msg);
}


static void tag_error (cup_State *L, int arg, int tag) {
  cupL_typeerror(L, arg, cup_typename(L, tag));
}


/*
** The use of 'cup_pushfstring' ensures this function does not
** need reserved stack space when called.
*/
CUPLIB_API void cupL_where (cup_State *L, int level) {
  cup_Debug ar;
  if (cup_getstack(L, level, &ar)) {  /* check function at level */
    cup_getinfo(L, "Sl", &ar);  /* get info about it */
    if (ar.currentline > 0) {  /* is there info? */
      cup_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline);
      return;
    }
  }
  cup_pushfstring(L, "");  /* else, no information available... */
}


/*
** Again, the use of 'cup_pushvfstring' ensures this function does
** not need reserved stack space when called. (At worst, it generates
** an error with "stack overflow" instead of the given message.)
*/
CUPLIB_API int cupL_error (cup_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  cupL_where(L, 1);
  cup_pushvfstring(L, fmt, argp);
  va_end(argp);
  cup_concat(L, 2);
  return cup_error(L);
}


CUPLIB_API int cupL_fileresult (cup_State *L, int stat, const char *fname) {
  int en = errno;  /* calls to Cup API may change this value */
  if (stat) {
    cup_pushboolean(L, 1);
    return 1;
  }
  else {
    cupL_pushfail(L);
    if (fname)
      cup_pushfstring(L, "%s: %s", fname, strerror(en));
    else
      cup_pushstring(L, strerror(en));
    cup_pushinteger(L, en);
    return 3;
  }
}


#if !defined(l_inspectstat)	/* { */

#if defined(CUP_USE_POSIX)

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


CUPLIB_API int cupL_execresult (cup_State *L, int stat) {
  if (stat != 0 && errno != 0)  /* error with an 'errno'? */
    return cupL_fileresult(L, 0, NULL);
  else {
    const char *what = "exit";  /* type of termination */
    l_inspectstat(stat, what);  /* interpret result */
    if (*what == 'e' && stat == 0)  /* successful termination? */
      cup_pushboolean(L, 1);
    else
      cupL_pushfail(L);
    cup_pushstring(L, what);
    cup_pushinteger(L, stat);
    return 3;  /* return true/fail,what,code */
  }
}

/* }====================================================== */



/*
** {======================================================
** Userdata's metatable manipulation
** =======================================================
*/

CUPLIB_API int cupL_newmetatable (cup_State *L, const char *tname) {
  if (cupL_getmetatable(L, tname) != CUP_TNIL)  /* name already in use? */
    return 0;  /* leave previous value on top, but return 0 */
  cup_pop(L, 1);
  cup_createtable(L, 0, 2);  /* create metatable */
  cup_pushstring(L, tname);
  cup_setfield(L, -2, "__name");  /* metatable.__name = tname */
  cup_pushvalue(L, -1);
  cup_setfield(L, CUP_REGISTRYINDEX, tname);  /* registry.name = metatable */
  return 1;
}


CUPLIB_API void cupL_setmetatable (cup_State *L, const char *tname) {
  cupL_getmetatable(L, tname);
  cup_setmetatable(L, -2);
}


CUPLIB_API void *cupL_testudata (cup_State *L, int ud, const char *tname) {
  void *p = cup_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    if (cup_getmetatable(L, ud)) {  /* does it have a metatable? */
      cupL_getmetatable(L, tname);  /* get correct metatable */
      if (!cup_rawequal(L, -1, -2))  /* not the same? */
        p = NULL;  /* value is a userdata with wrong metatable */
      cup_pop(L, 2);  /* remove both metatables */
      return p;
    }
  }
  return NULL;  /* value is not a userdata with a metatable */
}


CUPLIB_API void *cupL_checkudata (cup_State *L, int ud, const char *tname) {
  void *p = cupL_testudata(L, ud, tname);
  cupL_argexpected(L, p != NULL, ud, tname);
  return p;
}

/* }====================================================== */


/*
** {======================================================
** Argument check functions
** =======================================================
*/

CUPLIB_API int cupL_checkoption (cup_State *L, int arg, const char *def,
                                 const char *const lst[]) {
  const char *name = (def) ? cupL_optstring(L, arg, def) :
                             cupL_checkstring(L, arg);
  int i;
  for (i=0; lst[i]; i++)
    if (strcmp(lst[i], name) == 0)
      return i;
  return cupL_argerror(L, arg,
                       cup_pushfstring(L, "invalid option '%s'", name));
}


/*
** Ensures the stack has at least 'space' extra slots, raising an error
** if it cannot fulfill the request. (The error handling needs a few
** extra slots to format the error message. In case of an error without
** this extra space, Cup will generate the same 'stack overflow' error,
** but without 'msg'.)
*/
CUPLIB_API void cupL_checkstack (cup_State *L, int space, const char *msg) {
  if (l_unlikely(!cup_checkstack(L, space))) {
    if (msg)
      cupL_error(L, "stack overflow (%s)", msg);
    else
      cupL_error(L, "stack overflow");
  }
}


CUPLIB_API void cupL_checktype (cup_State *L, int arg, int t) {
  if (l_unlikely(cup_type(L, arg) != t))
    tag_error(L, arg, t);
}


CUPLIB_API void cupL_checkany (cup_State *L, int arg) {
  if (l_unlikely(cup_type(L, arg) == CUP_TNONE))
    cupL_argerror(L, arg, "value expected");
}


CUPLIB_API const char *cupL_checklstring (cup_State *L, int arg, size_t *len) {
  const char *s = cup_tolstring(L, arg, len);
  if (l_unlikely(!s)) tag_error(L, arg, CUP_TSTRING);
  return s;
}


CUPLIB_API const char *cupL_optlstring (cup_State *L, int arg,
                                        const char *def, size_t *len) {
  if (cup_isnoneornil(L, arg)) {
    if (len)
      *len = (def ? strlen(def) : 0);
    return def;
  }
  else return cupL_checklstring(L, arg, len);
}


CUPLIB_API cup_Number cupL_checknumber (cup_State *L, int arg) {
  int isnum;
  cup_Number d = cup_tonumberx(L, arg, &isnum);
  if (l_unlikely(!isnum))
    tag_error(L, arg, CUP_TNUMBER);
  return d;
}


CUPLIB_API cup_Number cupL_optnumber (cup_State *L, int arg, cup_Number def) {
  return cupL_opt(L, cupL_checknumber, arg, def);
}


static void interror (cup_State *L, int arg) {
  if (cup_isnumber(L, arg))
    cupL_argerror(L, arg, "number has no integer representation");
  else
    tag_error(L, arg, CUP_TNUMBER);
}


CUPLIB_API cup_Integer cupL_checkinteger (cup_State *L, int arg) {
  int isnum;
  cup_Integer d = cup_tointegerx(L, arg, &isnum);
  if (l_unlikely(!isnum)) {
    interror(L, arg);
  }
  return d;
}


CUPLIB_API cup_Integer cupL_optinteger (cup_State *L, int arg,
                                                      cup_Integer def) {
  return cupL_opt(L, cupL_checkinteger, arg, def);
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


static void *resizebox (cup_State *L, int idx, size_t newsize) {
  void *ud;
  cup_Alloc allocf = cup_getallocf(L, &ud);
  UBox *box = (UBox *)cup_touserdata(L, idx);
  void *temp = allocf(ud, box->box, box->bsize, newsize);
  if (l_unlikely(temp == NULL && newsize > 0)) {  /* allocation error? */
    cup_pushliteral(L, "not enough memory");
    cup_error(L);  /* raise a memory error */
  }
  box->box = temp;
  box->bsize = newsize;
  return temp;
}


static int boxgc (cup_State *L) {
  resizebox(L, 1, 0);
  return 0;
}


static const cupL_Reg boxmt[] = {  /* box metamethods */
  {"__gc", boxgc},
  {"__close", boxgc},
  {NULL, NULL}
};


static void newbox (cup_State *L) {
  UBox *box = (UBox *)cup_newuserdatauv(L, sizeof(UBox), 0);
  box->box = NULL;
  box->bsize = 0;
  if (cupL_newmetatable(L, "_UBOX*"))  /* creating metatable? */
    cupL_setfuncs(L, boxmt, 0);  /* set its metamethods */
  cup_setmetatable(L, -2);
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
  cup_assert(buffonstack(B) ? cup_touserdata(B->L, idx) != NULL  \
                            : cup_touserdata(B->L, idx) == (void*)B)


/*
** Compute new size for buffer 'B', enough to accommodate extra 'sz'
** bytes.
*/
static size_t newbuffsize (cupL_Buffer *B, size_t sz) {
  size_t newsize = B->size * 2;  /* double buffer size */
  if (l_unlikely(MAX_SIZET - sz < B->n))  /* overflow in (B->n + sz)? */
    return cupL_error(B->L, "buffer too large");
  if (newsize < B->n + sz)  /* double is not big enough? */
    newsize = B->n + sz;
  return newsize;
}


/*
** Returns a pointer to a free area with at least 'sz' bytes in buffer
** 'B'. 'boxidx' is the relative position in the stack where is the
** buffer's box or its placeholder.
*/
static char *prepbuffsize (cupL_Buffer *B, size_t sz, int boxidx) {
  checkbufferlevel(B, boxidx);
  if (B->size - B->n >= sz)  /* enough space? */
    return B->b + B->n;
  else {
    cup_State *L = B->L;
    char *newbuff;
    size_t newsize = newbuffsize(B, sz);
    /* create larger buffer */
    if (buffonstack(B))  /* buffer already has a box? */
      newbuff = (char *)resizebox(L, boxidx, newsize);  /* resize it */
    else {  /* no box yet */
      cup_remove(L, boxidx);  /* remove placeholder */
      newbox(L);  /* create a new box */
      cup_insert(L, boxidx);  /* move box to its intended position */
      cup_toclose(L, boxidx);
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
CUPLIB_API char *cupL_prepbuffsize (cupL_Buffer *B, size_t sz) {
  return prepbuffsize(B, sz, -1);
}


CUPLIB_API void cupL_addlstring (cupL_Buffer *B, const char *s, size_t l) {
  if (l > 0) {  /* avoid 'memcpy' when 's' can be NULL */
    char *b = prepbuffsize(B, l, -1);
    memcpy(b, s, l * sizeof(char));
    cupL_addsize(B, l);
  }
}


CUPLIB_API void cupL_addstring (cupL_Buffer *B, const char *s) {
  cupL_addlstring(B, s, strlen(s));
}


CUPLIB_API void cupL_pushresult (cupL_Buffer *B) {
  cup_State *L = B->L;
  checkbufferlevel(B, -1);
  cup_pushlstring(L, B->b, B->n);
  if (buffonstack(B))
    cup_closeslot(L, -2);  /* close the box */
  cup_remove(L, -2);  /* remove box or placeholder from the stack */
}


CUPLIB_API void cupL_pushresultsize (cupL_Buffer *B, size_t sz) {
  cupL_addsize(B, sz);
  cupL_pushresult(B);
}


/*
** 'cupL_addvalue' is the only function in the Buffer system where the
** box (if existent) is not on the top of the stack. So, instead of
** calling 'cupL_addlstring', it replicates the code using -2 as the
** last argument to 'prepbuffsize', signaling that the box is (or will
** be) bellow the string being added to the buffer. (Box creation can
** trigger an emergency GC, so we should not remove the string from the
** stack before we have the space guaranteed.)
*/
CUPLIB_API void cupL_addvalue (cupL_Buffer *B) {
  cup_State *L = B->L;
  size_t len;
  const char *s = cup_tolstring(L, -1, &len);
  char *b = prepbuffsize(B, len, -2);
  memcpy(b, s, len * sizeof(char));
  cupL_addsize(B, len);
  cup_pop(L, 1);  /* pop string */
}


CUPLIB_API void cupL_buffinit (cup_State *L, cupL_Buffer *B) {
  B->L = L;
  B->b = B->init.b;
  B->n = 0;
  B->size = CUPL_BUFFERSIZE;
  cup_pushlightuserdata(L, (void*)B);  /* push placeholder */
}


CUPLIB_API char *cupL_buffinitsize (cup_State *L, cupL_Buffer *B, size_t sz) {
  cupL_buffinit(L, B);
  return prepbuffsize(B, sz, -1);
}

/* }====================================================== */


/*
** {======================================================
** Reference system
** =======================================================
*/

/* index of free-list header (after the predefined values) */
#define freelist	(CUP_RIDX_LAST + 1)

/*
** The previously freed references form a linked list:
** t[freelist] is the index of a first free index, or zero if list is
** empty; t[t[freelist]] is the index of the second element; etc.
*/
CUPLIB_API int cupL_ref (cup_State *L, int t) {
  int ref;
  if (cup_isnil(L, -1)) {
    cup_pop(L, 1);  /* remove from stack */
    return CUP_REFNIL;  /* 'nil' has a unique fixed reference */
  }
  t = cup_absindex(L, t);
  if (cup_rawgeti(L, t, freelist) == CUP_TNIL) {  /* first access? */
    ref = 0;  /* list is empty */
    cup_pushinteger(L, 0);  /* initialize as an empty list */
    cup_rawseti(L, t, freelist);  /* ref = t[freelist] = 0 */
  }
  else {  /* already initialized */
    cup_assert(cup_isinteger(L, -1));
    ref = (int)cup_tointeger(L, -1);  /* ref = t[freelist] */
  }
  cup_pop(L, 1);  /* remove element from stack */
  if (ref != 0) {  /* any free element? */
    cup_rawgeti(L, t, ref);  /* remove it from list */
    cup_rawseti(L, t, freelist);  /* (t[freelist] = t[ref]) */
  }
  else  /* no free elements */
    ref = (int)cup_rawlen(L, t) + 1;  /* get a new reference */
  cup_rawseti(L, t, ref);
  return ref;
}


CUPLIB_API void cupL_unref (cup_State *L, int t, int ref) {
  if (ref >= 0) {
    t = cup_absindex(L, t);
    cup_rawgeti(L, t, freelist);
    cup_assert(cup_isinteger(L, -1));
    cup_rawseti(L, t, ref);  /* t[ref] = t[freelist] */
    cup_pushinteger(L, ref);
    cup_rawseti(L, t, freelist);  /* t[freelist] = ref */
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


static const char *getF (cup_State *L, void *ud, size_t *size) {
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


static int errfile (cup_State *L, const char *what, int fnameindex) {
  const char *serr = strerror(errno);
  const char *filename = cup_tostring(L, fnameindex) + 1;
  cup_pushfstring(L, "cannot %s %s: %s", what, filename, serr);
  cup_remove(L, fnameindex);
  return CUP_ERRFILE;
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


CUPLIB_API int cupL_loadfilex (cup_State *L, const char *filename,
                                             const char *mode) {
  LoadF lf;
  int status, readstatus;
  int c;
  int fnameindex = cup_gettop(L) + 1;  /* index of filename on the stack */
  if (filename == NULL) {
    cup_pushliteral(L, "=stdin");
    lf.f = stdin;
  }
  else {
    cup_pushfstring(L, "@%s", filename);
    lf.f = fopen(filename, "r");
    if (lf.f == NULL) return errfile(L, "open", fnameindex);
  }
  if (skipcomment(&lf, &c))  /* read initial portion */
    lf.buff[lf.n++] = '\n';  /* add line to correct line numbers */
  if (c == CUP_SIGNATURE[0] && filename) {  /* binary file? */
    lf.f = freopen(filename, "rb", lf.f);  /* reopen in binary mode */
    if (lf.f == NULL) return errfile(L, "reopen", fnameindex);
    skipcomment(&lf, &c);  /* re-read initial portion */
  }
  if (c != EOF)
    lf.buff[lf.n++] = c;  /* 'c' is the first character of the stream */
  status = cup_load(L, getF, &lf, cup_tostring(L, -1), mode);
  readstatus = ferror(lf.f);
  if (filename) fclose(lf.f);  /* close file (even in case of errors) */
  if (readstatus) {
    cup_settop(L, fnameindex);  /* ignore results from 'cup_load' */
    return errfile(L, "read", fnameindex);
  }
  cup_remove(L, fnameindex);
  return status;
}


typedef struct LoadS {
  const char *s;
  size_t size;
} LoadS;


static const char *getS (cup_State *L, void *ud, size_t *size) {
  LoadS *ls = (LoadS *)ud;
  (void)L;  /* not used */
  if (ls->size == 0) return NULL;
  *size = ls->size;
  ls->size = 0;
  return ls->s;
}


CUPLIB_API int cupL_loadbufferx (cup_State *L, const char *buff, size_t size,
                                 const char *name, const char *mode) {
  LoadS ls;
  ls.s = buff;
  ls.size = size;
  return cup_load(L, getS, &ls, name, mode);
}


CUPLIB_API int cupL_loadstring (cup_State *L, const char *s) {
  return cupL_loadbuffer(L, s, strlen(s), s);
}

/* }====================================================== */



CUPLIB_API int cupL_getmetafield (cup_State *L, int obj, const char *event) {
  if (!cup_getmetatable(L, obj))  /* no metatable? */
    return CUP_TNIL;
  else {
    int tt;
    cup_pushstring(L, event);
    tt = cup_rawget(L, -2);
    if (tt == CUP_TNIL)  /* is metafield nil? */
      cup_pop(L, 2);  /* remove metatable and metafield */
    else
      cup_remove(L, -2);  /* remove only metatable */
    return tt;  /* return metafield type */
  }
}


CUPLIB_API int cupL_callmeta (cup_State *L, int obj, const char *event) {
  obj = cup_absindex(L, obj);
  if (cupL_getmetafield(L, obj, event) == CUP_TNIL)  /* no metafield? */
    return 0;
  cup_pushvalue(L, obj);
  cup_call(L, 1, 1);
  return 1;
}


CUPLIB_API cup_Integer cupL_len (cup_State *L, int idx) {
  cup_Integer l;
  int isnum;
  cup_len(L, idx);
  l = cup_tointegerx(L, -1, &isnum);
  if (l_unlikely(!isnum))
    cupL_error(L, "object length is not an integer");
  cup_pop(L, 1);  /* remove object */
  return l;
}


CUPLIB_API const char *cupL_tolstring (cup_State *L, int idx, size_t *len) {
  idx = cup_absindex(L,idx);
  if (cupL_callmeta(L, idx, "__tostring")) {  /* metafield? */
    if (!cup_isstring(L, -1))
      cupL_error(L, "'__tostring' must return a string");
  }
  else {
    switch (cup_type(L, idx)) {
      case CUP_TNUMBER: {
        if (cup_isinteger(L, idx))
          cup_pushfstring(L, "%I", (CUPI_UACINT)cup_tointeger(L, idx));
        else
          cup_pushfstring(L, "%f", (CUPI_UACNUMBER)cup_tonumber(L, idx));
        break;
      }
      case CUP_TSTRING:
        cup_pushvalue(L, idx);
        break;
      case CUP_TBOOLEAN:
        cup_pushstring(L, (cup_toboolean(L, idx) ? "true" : "false"));
        break;
      case CUP_TNIL:
        cup_pushliteral(L, "nil");
        break;
      default: {
        int tt = cupL_getmetafield(L, idx, "__name");  /* try name */
        const char *kind = (tt == CUP_TSTRING) ? cup_tostring(L, -1) :
                                                 cupL_typename(L, idx);
        cup_pushfstring(L, "%s: %p", kind, cup_topointer(L, idx));
        if (tt != CUP_TNIL)
          cup_remove(L, -2);  /* remove '__name' */
        break;
      }
    }
  }
  return cup_tolstring(L, -1, len);
}


/*
** set functions from list 'l' into table at top - 'nup'; each
** function gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/
CUPLIB_API void cupL_setfuncs (cup_State *L, const cupL_Reg *l, int nup) {
  cupL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    if (l->func == NULL)  /* place holder? */
      cup_pushboolean(L, 0);
    else {
      int i;
      for (i = 0; i < nup; i++)  /* copy upvalues to the top */
        cup_pushvalue(L, -nup);
      cup_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    }
    cup_setfield(L, -(nup + 2), l->name);
  }
  cup_pop(L, nup);  /* remove upvalues */
}


/*
** ensure that stack[idx][fname] has a table and push that table
** into the stack
*/
CUPLIB_API int cupL_getsubtable (cup_State *L, int idx, const char *fname) {
  if (cup_getfield(L, idx, fname) == CUP_TTABLE)
    return 1;  /* table already there */
  else {
    cup_pop(L, 1);  /* remove previous result */
    idx = cup_absindex(L, idx);
    cup_newtable(L);
    cup_pushvalue(L, -1);  /* copy to be left at top */
    cup_setfield(L, idx, fname);  /* assign new table to field */
    return 0;  /* false, because did not find table there */
  }
}


/*
** Stripped-down 'require': After checking "loaded" table, calls 'openf'
** to open a module, registers the result in 'package.loaded' table and,
** if 'glb' is true, also registers the result in the global table.
** Leaves resulting module on the top.
*/
CUPLIB_API void cupL_requiref (cup_State *L, const char *modname,
                               cup_CFunction openf, int glb) {
  cupL_getsubtable(L, CUP_REGISTRYINDEX, CUP_LOADED_TABLE);
  cup_getfield(L, -1, modname);  /* LOADED[modname] */
  if (!cup_toboolean(L, -1)) {  /* package not already loaded? */
    cup_pop(L, 1);  /* remove field */
    cup_pushcfunction(L, openf);
    cup_pushstring(L, modname);  /* argument to open function */
    cup_call(L, 1, 1);  /* call 'openf' to open module */
    cup_pushvalue(L, -1);  /* make copy of module (call result) */
    cup_setfield(L, -3, modname);  /* LOADED[modname] = module */
  }
  cup_remove(L, -2);  /* remove LOADED table */
  if (glb) {
    cup_pushvalue(L, -1);  /* copy of module */
    cup_setglobal(L, modname);  /* _G[modname] = module */
  }
}


CUPLIB_API void cupL_addgsub (cupL_Buffer *b, const char *s,
                                     const char *p, const char *r) {
  const char *wild;
  size_t l = strlen(p);
  while ((wild = strstr(s, p)) != NULL) {
    cupL_addlstring(b, s, wild - s);  /* push prefix */
    cupL_addstring(b, r);  /* push replacement in place of pattern */
    s = wild + l;  /* continue after 'p' */
  }
  cupL_addstring(b, s);  /* push last suffix */
}


CUPLIB_API const char *cupL_gsub (cup_State *L, const char *s,
                                  const char *p, const char *r) {
  cupL_Buffer b;
  cupL_buffinit(L, &b);
  cupL_addgsub(&b, s, p, r);
  cupL_pushresult(&b);
  return cup_tostring(L, -1);
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


static int panic (cup_State *L) {
  const char *msg = cup_tostring(L, -1);
  if (msg == NULL) msg = "error object is not a string";
  cup_writestringerror("PANIC: unprotected error in call to Cup API (%s)\n",
                        msg);
  return 0;  /* return to Cup to abort */
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
static int checkcontrol (cup_State *L, const char *message, int tocont) {
  if (tocont || *(message++) != '@')  /* not a control message? */
    return 0;
  else {
    if (strcmp(message, "off") == 0)
      cup_setwarnf(L, warnfoff, L);  /* turn warnings off */
    else if (strcmp(message, "on") == 0)
      cup_setwarnf(L, warnfon, L);   /* turn warnings on */
    return 1;  /* it was a control message */
  }
}


static void warnfoff (void *ud, const char *message, int tocont) {
  checkcontrol((cup_State *)ud, message, tocont);
}


/*
** Writes the message and handle 'tocont', finishing the message
** if needed and setting the next warn function.
*/
static void warnfcont (void *ud, const char *message, int tocont) {
  cup_State *L = (cup_State *)ud;
  cup_writestringerror("%s", message);  /* write message */
  if (tocont)  /* not the last part? */
    cup_setwarnf(L, warnfcont, L);  /* to be continued */
  else {  /* last part */
    cup_writestringerror("%s", "\n");  /* finish message with end-of-line */
    cup_setwarnf(L, warnfon, L);  /* next call is a new message */
  }
}


static void warnfon (void *ud, const char *message, int tocont) {
  if (checkcontrol((cup_State *)ud, message, tocont))  /* control message? */
    return;  /* nothing else to be done */
  cup_writestringerror("%s", "Cup warning: ");  /* start a new warning */
  warnfcont(ud, message, tocont);  /* finish processing */
}


CUPLIB_API cup_State *cupL_newstate (void) {
  cup_State *L = cup_newstate(l_alloc, NULL);
  if (l_likely(L)) {
    cup_atpanic(L, &panic);
    cup_setwarnf(L, warnfoff, L);  /* default is warnings off */
  }
  return L;
}


CUPLIB_API void cupL_checkversion_ (cup_State *L, cup_Number ver, size_t sz) {
  cup_Number v = cup_version(L);
  if (sz != CUPL_NUMSIZES)  /* check numeric types */
    cupL_error(L, "core and library have incompatible numeric types");
  else if (v != ver)
    cupL_error(L, "version mismatch: app. needs %f, Cup core provides %f",
                  (CUPI_UACNUMBER)ver, (CUPI_UACNUMBER)v);
}

