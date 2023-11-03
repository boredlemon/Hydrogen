/*
** $Id: oslib.c $
** Standard Operating System library
** See Copyright Notice in nebula.h
*/

#define oslib_c
#define NEBULA_LIB

#include "prefix.h"


#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "nebula.h"

#include "auxlib.h"
#include "nebulalib.h"


/*
** {==================================================================
** List of valid conversion specifiers for the 'strftime' function;
** options are grouped by length; group of length 2 start with '||'.
** ===================================================================
*/
#if !defined(NEBULA_STRFTIMEOPTIONS)	/* { */

/* options for ANSI C 89 (only 1-char options) */
#define L_STRFTIMEC89		"aAbBcdHIjmMpSUwWxXyYZ%"

/* options for ISO C 99 and POSIX */
#define L_STRFTIMEC99 "aAbBcCdDeFgGhHIjmMnprRStTuUVwWxXyYzZ%" \
    "||" "EcECExEXEyEY" "OdOeOHOIOmOMOSOuOUOVOwOWOy"  /* two-char options */

/* options for Windows */
#define L_STRFTIMEWIN "aAbBcdHIjmMpSUwWxXyYzZ%" \
    "||" "#c#x#d#H#I#j#m#M#S#U#w#W#y#Y"  /* two-char options */

#if defined(NEBULA_USE_WINDOWS)
#define NEBULA_STRFTIMEOPTIONS	L_STRFTIMEWIN
#elif defined(NEBULA_USE_C89)
#define NEBULA_STRFTIMEOPTIONS	L_STRFTIMEC89
#else  /* C99 specification */
#define NEBULA_STRFTIMEOPTIONS	L_STRFTIMEC99
#endif

#endif					/* } */
/* }================================================================== */


/*
** {==================================================================
** Configuration for time-related stuff
** ===================================================================
*/

/*
** type to represent time_t in Nebula
*/
#if !defined(NEBULA_NUMTIME)	/* { */

#define l_timet			nebula_Integer
#define l_pushtime(L,t)		nebula_pushinteger(L,(nebula_Integer)(t))
#define l_gettime(L,arg)	nebulaL_checkinteger(L, arg)

#else				/* }{ */

#define l_timet			nebula_Number
#define l_pushtime(L,t)		nebula_pushnumber(L,(nebula_Number)(t))
#define l_gettime(L,arg)	nebulaL_checknumber(L, arg)

#endif				/* } */


#if !defined(l_gmtime)		/* { */
/*
** By default, Nebula uses gmtime/localtime, except when POSIX is available,
** where it uses gmtime_r/localtime_r
*/

#if defined(NEBULA_USE_POSIX)	/* { */

#define l_gmtime(t,r)		gmtime_r(t,r)
#define l_localtime(t,r)	localtime_r(t,r)

#else				/* }{ */

/* ISO C definitions */
#define l_gmtime(t,r)		((void)(r)->tm_sec, gmtime(t))
#define l_localtime(t,r)	((void)(r)->tm_sec, localtime(t))

#endif				/* } */

#endif				/* } */

/* }================================================================== */


/*
** {==================================================================
** Configuration for 'tmpnam':
** By default, Nebula uses tmpnam except when POSIX is available, where
** it uses mkstemp.
** ===================================================================
*/
#if !defined(nebula_tmpnam)	/* { */

#if defined(NEBULA_USE_POSIX)	/* { */

#include <unistd.h>

#define NEBULA_TMPNAMBUFSIZE	32

#if !defined(NEBULA_TMPNAMTEMPLATE)
#define NEBULA_TMPNAMTEMPLATE	"/tmp/nebula_XXXXXX"
#endif

#define nebula_tmpnam(b,e) { \
        strcpy(b, NEBULA_TMPNAMTEMPLATE); \
        e = mkstemp(b); \
        if (e != -1) close(e); \
        e = (e == -1); }

#else				/* }{ */

/* ISO C definitions */
#define NEBULA_TMPNAMBUFSIZE	L_tmpnam
#define nebula_tmpnam(b,e)		{ e = (tmpnam(b) == NULL); }

#endif				/* } */

#endif				/* } */
/* }================================================================== */



static int os_execute (nebula_State *L) {
  const char *cmd = nebulaL_optstring(L, 1, NULL);
  int stat;
  errno = 0;
  stat = system(cmd);
  if (cmd != NULL)
    return nebulaL_execresult(L, stat);
  else {
    nebula_pushboolean(L, stat);  /* true if there is a shell */
    return 1;
  }
}


static int os_remove (nebula_State *L) {
  const char *filename = nebulaL_checkstring(L, 1);
  return nebulaL_fileresult(L, remove(filename) == 0, filename);
}


static int os_rename (nebula_State *L) {
  const char *fromname = nebulaL_checkstring(L, 1);
  const char *toname = nebulaL_checkstring(L, 2);
  return nebulaL_fileresult(L, rename(fromname, toname) == 0, NULL);
}


static int os_tmpname (nebula_State *L) {
  char buff[NEBULA_TMPNAMBUFSIZE];
  int err;
  nebula_tmpnam(buff, err);
  if (l_unlikely(err))
    return nebulaL_error(L, "unable to generate a unique filename");
  nebula_pushstring(L, buff);
  return 1;
}


static int os_getenv (nebula_State *L) {
  nebula_pushstring(L, getenv(nebulaL_checkstring(L, 1)));  /* if NULL push nil */
  return 1;
}


static int os_clock (nebula_State *L) {
  nebula_pushnumber(L, ((nebula_Number)clock())/(nebula_Number)CLOCKS_PER_SEC);
  return 1;
}


/*
** {======================================================
** Time/Date operations
** { year=%Y, month=%m, day=%d, hour=%H, min=%M, sec=%S,
**   wday=%w+1, yday=%j, isdst=? }
** =======================================================
*/

/*
** About the overflow check: an overflow cannot occur when time
** is represented by a nebula_Integer, because either nebula_Integer is
** large enough to represent all int fields or it is not large enough
** to represent a time that cause a field to overflow.  However, if
** times are represented as doubles and nebula_Integer is int, then the
** time 0x1.e1853b0d184f6p+55 would cause an overflow when adding 1900
** to compute the year.
*/
static void setfield (nebula_State *L, const char *key, int value, int delta) {
  #if (defined(NEBULA_NUMTIME) && NEBULA_MAXINTEGER <= INT_MAX)
    if (l_unlikely(value > NEBULA_MAXINTEGER - delta))
      nebulaL_error(L, "field '%s' is out-of-bound", key);
  #endif
  nebula_pushinteger(L, (nebula_Integer)value + delta);
  nebula_setfield(L, -2, key);
}


static void setboolfield (nebula_State *L, const char *key, int value) {
  if (value < 0)  /* undefined? */
    return;  /* does not set field */
  nebula_pushboolean(L, value);
  nebula_setfield(L, -2, key);
}


/*
** Set all fields from structure 'tm' in the table on top of the stack
*/
static void setallfields (nebula_State *L, struct tm *stm) {
  setfield(L, "year", stm->tm_year, 1900);
  setfield(L, "month", stm->tm_mon, 1);
  setfield(L, "day", stm->tm_mday, 0);
  setfield(L, "hour", stm->tm_hour, 0);
  setfield(L, "min", stm->tm_min, 0);
  setfield(L, "sec", stm->tm_sec, 0);
  setfield(L, "yday", stm->tm_yday, 1);
  setfield(L, "wday", stm->tm_wday, 1);
  setboolfield(L, "isdst", stm->tm_isdst);
}


static int getboolfield (nebula_State *L, const char *key) {
  int res;
  res = (nebula_getfield(L, -1, key) == NEBULA_TNIL) ? -1 : nebula_toboolean(L, -1);
  nebula_pop(L, 1);
  return res;
}


static int getfield (nebula_State *L, const char *key, int d, int delta) {
  int isnum;
  int t = nebula_getfield(L, -1, key);  /* get field and its type */
  nebula_Integer res = nebula_tointegerx(L, -1, &isnum);
  if (!isnum) {  /* field is not an integer? */
    if (l_unlikely(t != NEBULA_TNIL))  /* some other value? */
      return nebulaL_error(L, "field '%s' is not an integer", key);
    else if (l_unlikely(d < 0))  /* absent field; no default? */
      return nebulaL_error(L, "field '%s' missing in date table", key);
    res = d;
  }
  else {
    /* unsigned avoids overflow when nebula_Integer has 32 bits */
    if (!(res >= 0 ? (nebula_Unsigned)res <= (nebula_Unsigned)INT_MAX + delta
                   : (nebula_Integer)INT_MIN + delta <= res))
      return nebulaL_error(L, "field '%s' is out-of-bound", key);
    res -= delta;
  }
  nebula_pop(L, 1);
  return (int)res;
}


static const char *checkoption (nebula_State *L, const char *conv,
                                ptrdiff_t convlen, char *buff) {
  const char *option = NEBULA_STRFTIMEOPTIONS;
  int oplen = 1;  /* length of options being checked */
  for (; *option != '\0' && oplen <= convlen; option += oplen) {
    if (*option == '|')  /* next block? */
      oplen++;  /* will check options with next length (+1) */
    else if (memcmp(conv, option, oplen) == 0) {  /* match? */
      memcpy(buff, conv, oplen);  /* copy valid option to buffer */
      buff[oplen] = '\0';
      return conv + oplen;  /* return next item */
    }
  }
  nebulaL_argerror(L, 1,
    nebula_pushfstring(L, "invalid conversion specifier '%%%s'", conv));
  return conv;  /* to avoid warnings */
}


static time_t l_checktime (nebula_State *L, int arg) {
  l_timet t = l_gettime(L, arg);
  nebulaL_argcheck(L, (time_t)t == t, arg, "time out-of-bounds");
  return (time_t)t;
}


/* maximum size for an individual 'strftime' item */
#define SIZETIMEFMT	250


static int os_date (nebula_State *L) {
  size_t slen;
  const char *s = nebulaL_optlstring(L, 1, "%c", &slen);
  time_t t = nebulaL_opt(L, l_checktime, 2, time(NULL));
  const char *se = s + slen;  /* 's' end */
  struct tm tmr, *stm;
  if (*s == '!') {  /* UTC? */
    stm = l_gmtime(&t, &tmr);
    s++;  /* skip '!' */
  }
  else
    stm = l_localtime(&t, &tmr);
  if (stm == NULL)  /* invalid date? */
    return nebulaL_error(L,
                 "date result cannot be represented in this installation");
  if (strcmp(s, "*t") == 0) {
    nebula_createtable(L, 0, 9);  /* 9 = number of fields */
    setallfields(L, stm);
  }
  else {
    char cc[4];  /* buffer for individual conversion specifiers */
    nebulaL_Buffer b;
    cc[0] = '%';
    nebulaL_buffinit(L, &b);
    while (s < se) {
      if (*s != '%')  /* not a conversion specifier? */
        nebulaL_addchar(&b, *s++);
      else {
        size_t reslen;
        char *buff = nebulaL_prepbuffsize(&b, SIZETIMEFMT);
        s++;  /* skip '%' */
        s = checkoption(L, s, se - s, cc + 1);  /* copy specifier to 'cc' */
        reslen = strftime(buff, SIZETIMEFMT, cc, stm);
        nebulaL_addsize(&b, reslen);
      }
    }
    nebulaL_pushresult(&b);
  }
  return 1;
}


static int os_time (nebula_State *L) {
  time_t t;
  if (nebula_isnoneornil(L, 1))  /* called without args? */
    t = time(NULL);  /* get current time */
  else {
    struct tm ts;
    nebulaL_checktype(L, 1, NEBULA_TTABLE);
    nebula_settop(L, 1);  /* make sure table is at the top */
    ts.tm_year = getfield(L, "year", -1, 1900);
    ts.tm_mon = getfield(L, "month", -1, 1);
    ts.tm_mday = getfield(L, "day", -1, 0);
    ts.tm_hour = getfield(L, "hour", 12, 0);
    ts.tm_min = getfield(L, "min", 0, 0);
    ts.tm_sec = getfield(L, "sec", 0, 0);
    ts.tm_isdst = getboolfield(L, "isdst");
    t = mktime(&ts);
    setallfields(L, &ts);  /* update fields with normalized values */
  }
  if (t != (time_t)(l_timet)t || t == (time_t)(-1))
    return nebulaL_error(L,
                  "time result cannot be represented in this installation");
  l_pushtime(L, t);
  return 1;
}


static int os_difftime (nebula_State *L) {
  time_t t1 = l_checktime(L, 1);
  time_t t2 = l_checktime(L, 2);
  nebula_pushnumber(L, (nebula_Number)difftime(t1, t2));
  return 1;
}

/* }====================================================== */


static int os_setlocale (nebula_State *L) {
  static const int cat[] = {LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY,
                      LC_NUMERIC, LC_TIME};
  static const char *const catnames[] = {"all", "collate", "ctype", "monetary",
     "numeric", "time", NULL};
  const char *l = nebulaL_optstring(L, 1, NULL);
  int op = nebulaL_checkoption(L, 2, "all", catnames);
  nebula_pushstring(L, setlocale(cat[op], l));
  return 1;
}


static int os_exit (nebula_State *L) {
  int status;
  if (nebula_isboolean(L, 1))
    status = (nebula_toboolean(L, 1) ? EXIT_SUCCESS : EXIT_FAILURE);
  else
    status = (int)nebulaL_optinteger(L, 1, EXIT_SUCCESS);
  if (nebula_toboolean(L, 2))
    nebula_close(L);
  if (L) exit(status);  /* 'if' to avoid warnings for unreachable 'return' */
  return 0;
}


static const nebulaL_Reg syslib[] = {
  {"clock",     os_clock},
  {"date",      os_date},
  {"difftime",  os_difftime},
  {"execute",   os_execute},
  {"exit",      os_exit},
  {"getenv",    os_getenv},
  {"remove",    os_remove},
  {"rename",    os_rename},
  {"setlocale", os_setlocale},
  {"time",      os_time},
  {"tmpname",   os_tmpname},
  {NULL, NULL}
};

/* }====================================================== */



NEBULAMOD_API int nebulaopen_os (nebula_State *L) {
  nebulaL_newlib(L, syslib);
  return 1;
}