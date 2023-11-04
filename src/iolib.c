/*
** $Id: iolib.c $
** Standard I/O (and system) library
** See Copyright Notice in hydrogen.h
** Input Output
*/

#define liolib_c
#define HYDROGEN_LIB

#include "prefix.h"


#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hydrogen.h"

#include "auxlib.h"
#include "hydrogenlib.h"




/*
** Change this macro to accept other modes for 'fopen' besides
** the standard ones.
*/
#if !defined(l_checkmode)

/* accepted extensions to 'mode' in 'fopen' */
#if !defined(L_MODEEXT)
#define L_MODEEXT	"b"
#endif

/* Check whether 'mode' matches '[rwa]%+?[L_MODEEXT]*' */
static int l_checkmode (const char *mode) {
  return (*mode != '\0' && strchr("rwa", *(mode++)) != NULL &&
         (*mode != '+' || ((void)(++mode), 1)) &&  /* skip if char is '+' */
         (strspn(mode, L_MODEEXT) == strlen(mode)));  /* check extensions */
}

#endif

/*
** {======================================================
** l_popen spawns a new process connected to the current
** one through the file streams.
** =======================================================
*/

#if !defined(l_popen)		/* { */

#if defined(HYDROGEN_USE_POSIX)	/* { */

#define l_popen(L,c,m)		(fflush(NULL), popen(c,m))
#define l_pclose(L,file)	(pclose(file))

#elif defined(HYDROGEN_USE_WINDOWS)	/* }{ */

#define l_popen(L,c,m)		(_popen(c,m))
#define l_pclose(L,file)	(_pclose(file))

#if !defined(l_checkmodep)
/* Windows accepts "[rw][bt]?" as valid modes */
#define l_checkmodep(m)	((m[0] == 'r' || m[0] == 'w') && \
  (m[1] == '\0' || ((m[1] == 'b' || m[1] == 't') && m[2] == '\0')))
#endif

#else				/* }{ */

/* ISO C definitions */
#define l_popen(L,c,m)  \
	  ((void)c, (void)m, \
	  hydrogenL_error(L, "'popen' not supported"), \
	  (FILE*)0)
#define l_pclose(L,file)		((void)L, (void)file, -1)

#endif				/* } */

#endif				/* } */


#if !defined(l_checkmodep)
/* By default, Hydrogen accepts only "r" or "w" as valid modes */
#define l_checkmodep(m)        ((m[0] == 'r' || m[0] == 'w') && m[1] == '\0')
#endif

/* }====================================================== */


#if !defined(l_getc)		/* { */

#if defined(HYDROGEN_USE_POSIX)
#define l_getc(f)		getc_unlocked(f)
#define l_lockfile(f)		flockfile(f)
#define l_unlockfile(f)		funlockfile(f)
#else
#define l_getc(f)		getc(f)
#define l_lockfile(f)		((void)0)
#define l_unlockfile(f)		((void)0)
#endif

#endif				/* } */


/*
** {======================================================
** l_fseek: configuration for longer offsets
** =======================================================
*/

#if !defined(l_fseek)		/* { */

#if defined(HYDROGEN_USE_POSIX)	/* { */

#include <sys/types.h>

#define l_fseek(f,o,w)		fseeko(f,o,w)
#define l_ftell(f)		ftello(f)
#define l_seeknum		off_t

#elif defined(HYDROGEN_USE_WINDOWS) && !defined(_CRTIMP_TYPEINFO) \
   && defined(_MSC_VER) && (_MSC_VER >= 1400)	/* }{ */

/* Windows (but not DDK) and Visual C++ 2005 or higher */
#define l_fseek(f,o,w)		_fseeki64(f,o,w)
#define l_ftell(f)		_ftelli64(f)
#define l_seeknum		__int64

#else				/* }{ */

/* ISO C definitions */
#define l_fseek(f,o,w)		fseek(f,o,w)
#define l_ftell(f)		ftell(f)
#define l_seeknum		long

#endif				/* } */

#endif				/* } */

/* }====================================================== */



#define IO_PREFIX	"_IO_"
#define IOPREF_LEN	(sizeof(IO_PREFIX)/sizeof(char) - 1)
#define IO_INPUT	(IO_PREFIX "input")
#define IO_OUTPUT	(IO_PREFIX "output")


typedef hydrogenL_Stream LStream;


#define tolstream(L)	((LStream *)hydrogenL_checkudata(L, 1, HYDROGEN_FILEHANDLE))

#define isclosed(p)	((p)->closef == NULL)


static int io_type (hydrogen_State *L) {
  LStream *p;
  hydrogenL_checkany(L, 1);
  p = (LStream *)hydrogenL_testudata(L, 1, HYDROGEN_FILEHANDLE);
  if (p == NULL)
    hydrogenL_pushfail(L);  /* not a file */
  else if (isclosed(p))
    hydrogen_pushliteral(L, "closed file");
  else
    hydrogen_pushliteral(L, "file");
  return 1;
}


static int f_tostring (hydrogen_State *L) {
  LStream *p = tolstream(L);
  if (isclosed(p))
    hydrogen_pushliteral(L, "file (closed)");
  else
    hydrogen_pushfstring(L, "file (%p)", p->f);
  return 1;
}


static FILE *tofile (hydrogen_State *L) {
  LStream *p = tolstream(L);
  if (l_unlikely(isclosed(p)))
    hydrogenL_error(L, "attempt to use a closed file");
  hydrogen_assert(p->f);
  return p->f;
}


/*
** When creating file handles, always creates a 'closed' file handle
** before opening the actual file; so, if there is a memory error, the
** handle is in a consistent state.
*/
static LStream *newprefile (hydrogen_State *L) {
  LStream *p = (LStream *)hydrogen_newuserdatauv(L, sizeof(LStream), 0);
  p->closef = NULL;  /* mark file handle as 'closed' */
  hydrogenL_setmetatable(L, HYDROGEN_FILEHANDLE);
  return p;
}


/*
** Calls the 'close' function from a file handle. The 'volatile' avoids
** a bug in some versions of the Clang compiler (e.g., clang 3.0 for
** 32 bits).
*/
static int aux_close (hydrogen_State *L) {
  LStream *p = tolstream(L);
  volatile hydrogen_CFunction cf = p->closef;
  p->closef = NULL;  /* mark stream as closed */
  return (*cf)(L);  /* close it */
}


static int f_close (hydrogen_State *L) {
  tofile(L);  /* make sure argument is an open stream */
  return aux_close(L);
}


static int io_close (hydrogen_State *L) {
  if (hydrogen_isnone(L, 1))  /* no argument? */
    hydrogen_getfield(L, HYDROGEN_REGISTRYINDEX, IO_OUTPUT);  /* use default output */
  return f_close(L);
}


static int f_gc (hydrogen_State *L) {
  LStream *p = tolstream(L);
  if (!isclosed(p) && p->f != NULL)
    aux_close(L);  /* ignore closed and incompletely open files */
  return 0;
}


/*
** function to close regular files
*/
static int io_fclose (hydrogen_State *L) {
  LStream *p = tolstream(L);
  int res = fclose(p->f);
  return hydrogenL_fileresult(L, (res == 0), NULL);
}


static LStream *newfile (hydrogen_State *L) {
  LStream *p = newprefile(L);
  p->f = NULL;
  p->closef = &io_fclose;
  return p;
}


static void opencheck (hydrogen_State *L, const char *fname, const char *mode) {
  LStream *p = newfile(L);
  p->f = fopen(fname, mode);
  if (l_unlikely(p->f == NULL))
    hydrogenL_error(L, "cannot open file '%s' (%s)", fname, strerror(errno));
}


static int io_open (hydrogen_State *L) {
  const char *filename = hydrogenL_checkstring(L, 1);
  const char *mode = hydrogenL_optstring(L, 2, "r");
  LStream *p = newfile(L);
  const char *md = mode;  /* to traverse/check mode */
  hydrogenL_argcheck(L, l_checkmode(md), 2, "invalid mode");
  p->f = fopen(filename, mode);
  return (p->f == NULL) ? hydrogenL_fileresult(L, 0, filename) : 1;
}


/*
** function to close 'popen' files
*/
static int io_pclose (hydrogen_State *L) {
  LStream *p = tolstream(L);
  errno = 0;
  return hydrogenL_execresult(L, l_pclose(L, p->f));
}


static int io_popen (hydrogen_State *L) {
  const char *filename = hydrogenL_checkstring(L, 1);
  const char *mode = hydrogenL_optstring(L, 2, "r");
  LStream *p = newprefile(L);
  hydrogenL_argcheck(L, l_checkmodep(mode), 2, "invalid mode");
  p->f = l_popen(L, filename, mode);
  p->closef = &io_pclose;
  return (p->f == NULL) ? hydrogenL_fileresult(L, 0, filename) : 1;
}


static int io_tmpfile (hydrogen_State *L) {
  LStream *p = newfile(L);
  p->f = tmpfile();
  return (p->f == NULL) ? hydrogenL_fileresult(L, 0, NULL) : 1;
}


static FILE *getiofile (hydrogen_State *L, const char *findex) {
  LStream *p;
  hydrogen_getfield(L, HYDROGEN_REGISTRYINDEX, findex);
  p = (LStream *)hydrogen_touserdata(L, -1);
  if (l_unlikely(isclosed(p)))
    hydrogenL_error(L, "default %s file is closed", findex + IOPREF_LEN);
  return p->f;
}


static int g_iofile (hydrogen_State *L, const char *f, const char *mode) {
  if (!hydrogen_isnoneornil(L, 1)) {
    const char *filename = hydrogen_tostring(L, 1);
    if (filename)
      opencheck(L, filename, mode);
    else {
      tofile(L);  /* check that it's a valid file handle */
      hydrogen_pushvalue(L, 1);
    }
    hydrogen_setfield(L, HYDROGEN_REGISTRYINDEX, f);
  }
  /* return current value */
  hydrogen_getfield(L, HYDROGEN_REGISTRYINDEX, f);
  return 1;
}


static int io_input (hydrogen_State *L) {
  return g_iofile(L, IO_INPUT, "r");
}


static int io_output (hydrogen_State *L) {
  return g_iofile(L, IO_OUTPUT, "w");
}


static int io_readline (hydrogen_State *L);


/*
** maximum number of arguments to 'f:lines'/'io.lines' (it + 3 must fit
** in the limit for upvalues of a closure)
*/
#define MAXARGLINE	250

/*
** Auxiliary function to create the iteration function for 'lines'.
** The iteration function is a closure over 'io_readline', with
** the following upvalues:
** 1) The file being read (first value in the stack)
** 2) the number of arguments to read
** 3) a boolean, true iff file has to be closed when finished ('toclose')
** *) a variable number of format arguments (rest of the stack)
*/
static void aux_lines (hydrogen_State *L, int toclose) {
  int n = hydrogen_gettop(L) - 1;  /* number of arguments to read */
  hydrogenL_argcheck(L, n <= MAXARGLINE, MAXARGLINE + 2, "too many arguments");
  hydrogen_pushvalue(L, 1);  /* file */
  hydrogen_pushinteger(L, n);  /* number of arguments to read */
  hydrogen_pushboolean(L, toclose);  /* close/not close file when finished */
  hydrogen_rotate(L, 2, 3);  /* move the three values to their positions */
  hydrogen_pushcclosure(L, io_readline, 3 + n);
}


static int f_lines (hydrogen_State *L) {
  tofile(L);  /* check that it's a valid file handle */
  aux_lines(L, 0);
  return 1;
}


/*
** Return an iteration function for 'io.lines'. If file has to be
** closed, also returns the file itself as a second result (to be
** closed as the state at the exit of a generic for).
*/
static int io_lines (hydrogen_State *L) {
  int toclose;
  if (hydrogen_isnone(L, 1)) hydrogen_pushnil(L);  /* at least one argument */
  if (hydrogen_isnil(L, 1)) {  /* no file name? */
    hydrogen_getfield(L, HYDROGEN_REGISTRYINDEX, IO_INPUT);  /* get default input */
    hydrogen_replace(L, 1);  /* put it at index 1 */
    tofile(L);  /* check that it's a valid file handle */
    toclose = 0;  /* do not close it after iteration */
  }
  else {  /* open a new file */
    const char *filename = hydrogenL_checkstring(L, 1);
    opencheck(L, filename, "r");
    hydrogen_replace(L, 1);  /* put file at index 1 */
    toclose = 1;  /* close it after iteration */
  }
  aux_lines(L, toclose);  /* push iteration function */
  if (toclose) {
    hydrogen_pushnil(L);  /* state */
    hydrogen_pushnil(L);  /* control */
    hydrogen_pushvalue(L, 1);  /* file is the to-be-closed variable (4th result) */
    return 4;
  }
  else
    return 1;
}


/*
** {======================================================
** READ
** =======================================================
*/


/* maximum length of a numeral */
#if !defined (L_MAXLENNUM)
#define L_MAXLENNUM     200
#endif


/* auxiliary structure used by 'read_number' */
typedef struct {
  FILE *f;  /* file being read */
  int c;  /* current character (look ahead) */
  int n;  /* number of elements in buffer 'buff' */
  char buff[L_MAXLENNUM + 1];  /* +1 for ending '\0' */
} RN;


/*
** Add current char to buffer (if not out of space) and read next one
*/
static int nextc (RN *rn) {
  if (l_unlikely(rn->n >= L_MAXLENNUM)) {  /* buffer overflow? */
    rn->buff[0] = '\0';  /* invalidate result */
    return 0;  /* fail */
  }
  else {
    rn->buff[rn->n++] = rn->c;  /* save current char */
    rn->c = l_getc(rn->f);  /* read next one */
    return 1;
  }
}


/*
** Accept current char if it is in 'set' (of size 2)
*/
static int test2 (RN *rn, const char *set) {
  if (rn->c == set[0] || rn->c == set[1])
    return nextc(rn);
  else return 0;
}


/*
** Read a sequence of (hex)digits
*/
static int readdigits (RN *rn, int hex) {
  int count = 0;
  while ((hex ? isxdigit(rn->c) : isdigit(rn->c)) && nextc(rn))
    count++;
  return count;
}


/*
** Read a number: first reads a valid prefix of a numeral into a buffer.
** Then it calls 'hydrogen_stringtonumber' to check whether the format is
** correct and to convert it to a Hydrogen number.
*/
static int read_number (hydrogen_State *L, FILE *f) {
  RN rn;
  int count = 0;
  int hex = 0;
  char decp[2];
  rn.f = f; rn.n = 0;
  decp[0] = hydrogen_getlocaledecpoint();  /* get decimal point from locale */
  decp[1] = '.';  /* always accept a dot */
  l_lockfile(rn.f);
  do { rn.c = l_getc(rn.f); } while (isspace(rn.c));  /* skip spaces */
  test2(&rn, "-+");  /* optional sign */
  if (test2(&rn, "00")) {
    if (test2(&rn, "xX")) hex = 1;  /* numeral is hexadecimal */
    else count = 1;  /* count initial '0' as a valid digit */
  }
  count += readdigits(&rn, hex);  /* integral part */
  if (test2(&rn, decp))  /* decimal point? */
    count += readdigits(&rn, hex);  /* fractional part */
  if (count > 0 && test2(&rn, (hex ? "pP" : "eE"))) {  /* exponent mark? */
    test2(&rn, "-+");  /* exponent sign */
    readdigits(&rn, 0);  /* exponent digits */
  }
  ungetc(rn.c, rn.f);  /* unread look-ahead char */
  l_unlockfile(rn.f);
  rn.buff[rn.n] = '\0';  /* finish string */
  if (l_likely(hydrogen_stringtonumber(L, rn.buff)))
    return 1;  /* ok, it is a valid number */
  else {  /* invalid format */
   hydrogen_pushnil(L);  /* "result" to be removed */
   return 0;  /* read fails */
  }
}


static int test_eof (hydrogen_State *L, FILE *f) {
  int c = getc(f);
  ungetc(c, f);  /* no-op when c == EOF */
  hydrogen_pushliteral(L, "");
  return (c != EOF);
}


static int read_line (hydrogen_State *L, FILE *f, int chop) {
  hydrogenL_Buffer b;
  int c;
  hydrogenL_buffinit(L, &b);
  do {  /* may need to read several chunks to get whole line */
    char *buff = hydrogenL_prepbuffer(&b);  /* preallocate buffer space */
    int i = 0;
    l_lockfile(f);  /* no memory errors can happen inside the lock */
    while (i < HYDROGENL_BUFFERSIZE && (c = l_getc(f)) != EOF && c != '\n')
      buff[i++] = c;  /* read up to end of line or buffer limit */
    l_unlockfile(f);
    hydrogenL_addsize(&b, i);
  } while (c != EOF && c != '\n');  /* repeat until end of line */
  if (!chop && c == '\n')  /* want a newline and have one? */
    hydrogenL_addchar(&b, c);  /* add ending newline to result */
  hydrogenL_pushresult(&b);  /* close buffer */
  /* return ok if read something (either a newline or something else) */
  return (c == '\n' || hydrogen_rawlen(L, -1) > 0);
}


static void read_all (hydrogen_State *L, FILE *f) {
  size_t nr;
  hydrogenL_Buffer b;
  hydrogenL_buffinit(L, &b);
  do {  /* read file in chunks of HYDROGENL_BUFFERSIZE bytes */
    char *p = hydrogenL_prepbuffer(&b);
    nr = fread(p, sizeof(char), HYDROGENL_BUFFERSIZE, f);
    hydrogenL_addsize(&b, nr);
  } while (nr == HYDROGENL_BUFFERSIZE);
  hydrogenL_pushresult(&b);  /* close buffer */
}


static int read_chars (hydrogen_State *L, FILE *f, size_t n) {
  size_t nr;  /* number of chars actually read */
  char *p;
  hydrogenL_Buffer b;
  hydrogenL_buffinit(L, &b);
  p = hydrogenL_prepbuffsize(&b, n);  /* prepare buffer to read whole block */
  nr = fread(p, sizeof(char), n, f);  /* try to read 'n' chars */
  hydrogenL_addsize(&b, nr);
  hydrogenL_pushresult(&b);  /* close buffer */
  return (nr > 0);  /* true iff read something */
}


static int g_read (hydrogen_State *L, FILE *f, int first) {
  int nargs = hydrogen_gettop(L) - 1;
  int n, success;
  clearerr(f);
  if (nargs == 0) {  /* no arguments? */
    success = read_line(L, f, 1);
    n = first + 1;  /* to return 1 result */
  }
  else {
    /* ensure stack space for all results and for auxlib's buffer */
    hydrogenL_checkstack(L, nargs+HYDROGEN_MINSTACK, "too many arguments");
    success = 1;
    for (n = first; nargs-- && success; n++) {
      if (hydrogen_type(L, n) == HYDROGEN_TNUMBER) {
        size_t l = (size_t)hydrogenL_checkinteger(L, n);
        success = (l == 0) ? test_eof(L, f) : read_chars(L, f, l);
      }
      else {
        const char *p = hydrogenL_checkstring(L, n);
        if (*p == '*') p++;  /* skip optional '*' (for compatibility) */
        switch (*p) {
          case 'n':  /* number */
            success = read_number(L, f);
            break;
          case 'l':  /* line */
            success = read_line(L, f, 1);
            break;
          case 'L':  /* line with end-of-line */
            success = read_line(L, f, 0);
            break;
          case 'a':  /* file */
            read_all(L, f);  /* read entire file */
            success = 1; /* always success */
            break;
          default:
            return hydrogenL_argerror(L, n, "invalid format");
        }
      }
    }
  }
  if (ferror(f))
    return hydrogenL_fileresult(L, 0, NULL);
  if (!success) {
    hydrogen_pop(L, 1);  /* remove last result */
    hydrogenL_pushfail(L);  /* push nil instead */
  }
  return n - first;
}


static int io_read (hydrogen_State *L) {
  return g_read(L, getiofile(L, IO_INPUT), 1);
}


static int f_read (hydrogen_State *L) {
  return g_read(L, tofile(L), 2);
}


/*
** Iteration function for 'lines'.
*/
static int io_readline (hydrogen_State *L) {
  LStream *p = (LStream *)hydrogen_touserdata(L, hydrogen_upvalueindex(1));
  int i;
  int n = (int)hydrogen_tointeger(L, hydrogen_upvalueindex(2));
  if (isclosed(p))  /* file is already closed? */
    return hydrogenL_error(L, "file is already closed");
  hydrogen_settop(L , 1);
  hydrogenL_checkstack(L, n, "too many arguments");
  for (i = 1; i <= n; i++)  /* push arguments to 'g_read' */
    hydrogen_pushvalue(L, hydrogen_upvalueindex(3 + i));
  n = g_read(L, p->f, 2);  /* 'n' is number of results */
  hydrogen_assert(n > 0);  /* should return at least a nil */
  if (hydrogen_toboolean(L, -n))  /* read at least one value? */
    return n;  /* return them */
  else {  /* first result is false: EOF or error */
    if (n > 1) {  /* is there error information? */
      /* 2nd result is error message */
      return hydrogenL_error(L, "%s", hydrogen_tostring(L, -n + 1));
    }
    if (hydrogen_toboolean(L, hydrogen_upvalueindex(3))) {  /* generator created file? */
      hydrogen_settop(L, 0);  /* clear stack */
      hydrogen_pushvalue(L, hydrogen_upvalueindex(1));  /* push file at index 1 */
      aux_close(L);  /* close it */
    }
    return 0;
  }
}

/* }====================================================== */


static int g_write (hydrogen_State *L, FILE *f, int arg) {
  int nargs = hydrogen_gettop(L) - arg;
  int status = 1;
  for (; nargs--; arg++) {
    if (hydrogen_type(L, arg) == HYDROGEN_TNUMBER) {
      /* optimization: could be done exactly as for strings */
      int len = hydrogen_isinteger(L, arg)
                ? fprintf(f, HYDROGEN_INTEGER_FMT,
                             (HYDROGENI_UACINT)hydrogen_tointeger(L, arg))
                : fprintf(f, HYDROGEN_NUMBER_FMT,
                             (HYDROGENI_UACNUMBER)hydrogen_tonumber(L, arg));
      status = status && (len > 0);
    }
    else {
      size_t l;
      const char *s = hydrogenL_checklstring(L, arg, &l);
      status = status && (fwrite(s, sizeof(char), l, f) == l);
    }
  }
  if (l_likely(status))
    return 1;  /* file handle already on stack top */
  else return hydrogenL_fileresult(L, status, NULL);
}


static int io_write (hydrogen_State *L) {
  return g_write(L, getiofile(L, IO_OUTPUT), 1);
}


static int f_write (hydrogen_State *L) {
  FILE *f = tofile(L);
  hydrogen_pushvalue(L, 1);  /* push file at the stack top (to be returned) */
  return g_write(L, f, 2);
}


static int f_seek (hydrogen_State *L) {
  static const int mode[] = {SEEK_SET, SEEK_CUR, SEEK_END};
  static const char *const modenames[] = {"set", "cur", "end", NULL};
  FILE *f = tofile(L);
  int op = hydrogenL_checkoption(L, 2, "cur", modenames);
  hydrogen_Integer p3 = hydrogenL_optinteger(L, 3, 0);
  l_seeknum offset = (l_seeknum)p3;
  hydrogenL_argcheck(L, (hydrogen_Integer)offset == p3, 3,
                  "not an integer in proper range");
  op = l_fseek(f, offset, mode[op]);
  if (l_unlikely(op))
    return hydrogenL_fileresult(L, 0, NULL);  /* error */
  else {
    hydrogen_pushinteger(L, (hydrogen_Integer)l_ftell(f));
    return 1;
  }
}


static int f_setvbuf (hydrogen_State *L) {
  static const int mode[] = {_IONBF, _IOFBF, _IOLBF};
  static const char *const modenames[] = {"no", "full", "line", NULL};
  FILE *f = tofile(L);
  int op = hydrogenL_checkoption(L, 2, NULL, modenames);
  hydrogen_Integer sz = hydrogenL_optinteger(L, 3, HYDROGENL_BUFFERSIZE);
  int res = setvbuf(f, NULL, mode[op], (size_t)sz);
  return hydrogenL_fileresult(L, res == 0, NULL);
}



static int io_flush (hydrogen_State *L) {
  return hydrogenL_fileresult(L, fflush(getiofile(L, IO_OUTPUT)) == 0, NULL);
}


static int f_flush (hydrogen_State *L) {
  return hydrogenL_fileresult(L, fflush(tofile(L)) == 0, NULL);
}


/*
** functions for 'io' library
*/
static const hydrogenL_Reg iolib[] = {
  {"close", io_close},
  {"flush", io_flush},
  {"input", io_input},
  {"lines", io_lines},
  {"open", io_open},
  {"output", io_output},
  {"popen", io_popen},
  {"read", io_read},
  {"tmpfile", io_tmpfile},
  {"type", io_type},
  {"write", io_write},
  {NULL, NULL}
};


/*
** methods for file handles
*/
static const hydrogenL_Reg meth[] = {
  {"read", f_read},
  {"write", f_write},
  {"lines", f_lines},
  {"flush", f_flush},
  {"seek", f_seek},
  {"close", f_close},
  {"setvbuf", f_setvbuf},
  {NULL, NULL}
};


/*
** metamethods for file handles
*/
static const hydrogenL_Reg metameth[] = {
  {"__index", NULL},  /* place holder */
  {"__gc", f_gc},
  {"__close", f_gc},
  {"__tostring", f_tostring},
  {NULL, NULL}
};


static void createmeta (hydrogen_State *L) {
  hydrogenL_newmetatable(L, HYDROGEN_FILEHANDLE);  /* metatable for file handles */
  hydrogenL_setfuncs(L, metameth, 0);  /* add metamethods to new metatable */
  hydrogenL_newlibtable(L, meth);  /* create method table */
  hydrogenL_setfuncs(L, meth, 0);  /* add file methods to method table */
  hydrogen_setfield(L, -2, "__index");  /* metatable.__index = method table */
  hydrogen_pop(L, 1);  /* pop metatable */
}


/*
** function to (not) close the standard files stdin, stdout, and stderr
*/
static int io_noclose (hydrogen_State *L) {
  LStream *p = tolstream(L);
  p->closef = &io_noclose;  /* keep file opened */
  hydrogenL_pushfail(L);
  hydrogen_pushliteral(L, "cannot close standard file");
  return 2;
}


static void createstdfile (hydrogen_State *L, FILE *f, const char *k,
                           const char *fname) {
  LStream *p = newprefile(L);
  p->f = f;
  p->closef = &io_noclose;
  if (k != NULL) {
    hydrogen_pushvalue(L, -1);
    hydrogen_setfield(L, HYDROGEN_REGISTRYINDEX, k);  /* add file to registry */
  }
  hydrogen_setfield(L, -2, fname);  /* add file to module */
}


HYDROGENMOD_API int hydrogenopen_io (hydrogen_State *L) {
  hydrogenL_newlib(L, iolib);  /* new module */
  createmeta(L);
  /* create (and set) default files */
  createstdfile(L, stdin, IO_INPUT, "stdin");
  createstdfile(L, stdout, IO_OUTPUT, "stdout");
  createstdfile(L, stderr, NULL, "stderr");
  return 1;
}