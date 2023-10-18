/*
** $Id: loslib.c $
** Standard Operating System library
** See Copyright Notice in acorn.h
*/

#define loslib_c
#define ACORN_LIB

#include "lprefix.h"


#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "acorn.h"

#include "lauxlib.h"
#include "acornlib.h"


/*
** {==================================================================
** List of valid conversion specifiers for the 'strftime' function;
** options are grouped by length; group of length 2 start with '||'.
** ===================================================================
*/
#if !defined(ACORN_STRFTIMEOPTIONS)	/* { */

/* options for ANSI C 89 (only 1-char options) */
#define L_STRFTIMEC89		"aAbBcdHIjmMpSUwWxXyYZ%"

/* options for ISO C 99 and POSIX */
#define L_STRFTIMEC99 "aAbBcCdDeFgGhHIjmMnprRStTuUVwWxXyYzZ%" \
    "||" "EcECExEXEyEY" "OdOeOHOIOmOMOSOuOUOVOwOWOy"  /* two-char options */

/* options for Windows */
#define L_STRFTIMEWIN "aAbBcdHIjmMpSUwWxXyYzZ%" \
    "||" "#c#x#d#H#I#j#m#M#S#U#w#W#y#Y"  /* two-char options */

#if defined(ACORN_USE_WINDOWS)
#define ACORN_STRFTIMEOPTIONS	L_STRFTIMEWIN
#elif defined(ACORN_USE_C89)
#define ACORN_STRFTIMEOPTIONS	L_STRFTIMEC89
#else  /* C99 specification */
#define ACORN_STRFTIMEOPTIONS	L_STRFTIMEC99
#endif

#endif					/* } */
/* }================================================================== */


/*
** {==================================================================
** Configuration for time-related stuff
** ===================================================================
*/

/*
** type to represent time_t in Acorn
*/
#if !defined(ACORN_NUMTIME)	/* { */

#define l_timet			acorn_Integer
#define l_pushtime(L,t)		acorn_pushinteger(L,(acorn_Integer)(t))
#define l_gettime(L,arg)	acornL_checkinteger(L, arg)

#else				/* }{ */

#define l_timet			acorn_Number
#define l_pushtime(L,t)		acorn_pushnumber(L,(acorn_Number)(t))
#define l_gettime(L,arg)	acornL_checknumber(L, arg)

#endif				/* } */


#if !defined(l_gmtime)		/* { */
/*
** By default, Acorn uses gmtime/localtime, except when POSIX is available,
** where it uses gmtime_r/localtime_r
*/

#if defined(ACORN_USE_POSIX)	/* { */

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
** By default, Acorn uses tmpnam except when POSIX is available, where
** it uses mkstemp.
** ===================================================================
*/
#if !defined(acorn_tmpnam)	/* { */

#if defined(ACORN_USE_POSIX)	/* { */

#include <unistd.h>

#define ACORN_TMPNAMBUFSIZE	32

#if !defined(ACORN_TMPNAMTEMPLATE)
#define ACORN_TMPNAMTEMPLATE	"/tmp/acorn_XXXXXX"
#endif

#define acorn_tmpnam(b,e) { \
        strcpy(b, ACORN_TMPNAMTEMPLATE); \
        e = mkstemp(b); \
        if (e != -1) close(e); \
        e = (e == -1); }

#else				/* }{ */

/* ISO C definitions */
#define ACORN_TMPNAMBUFSIZE	L_tmpnam
#define acorn_tmpnam(b,e)		{ e = (tmpnam(b) == NULL); }

#endif				/* } */

#endif				/* } */
/* }================================================================== */



static int os_execute (acorn_State *L) {
  const char *cmd = acornL_optstring(L, 1, NULL);
  int stat;
  errno = 0;
  stat = system(cmd);
  if (cmd != NULL)
    return acornL_execresult(L, stat);
  else {
    acorn_pushboolean(L, stat);  /* true if there is a shell */
    return 1;
  }
}


static int os_remove (acorn_State *L) {
  const char *filename = acornL_checkstring(L, 1);
  return acornL_fileresult(L, remove(filename) == 0, filename);
}


static int os_rename (acorn_State *L) {
  const char *fromname = acornL_checkstring(L, 1);
  const char *toname = acornL_checkstring(L, 2);
  return acornL_fileresult(L, rename(fromname, toname) == 0, NULL);
}


static int os_tmpname (acorn_State *L) {
  char buff[ACORN_TMPNAMBUFSIZE];
  int err;
  acorn_tmpnam(buff, err);
  if (l_unlikely(err))
    return acornL_error(L, "unable to generate a unique filename");
  acorn_pushstring(L, buff);
  return 1;
}


static int os_getenv (acorn_State *L) {
  acorn_pushstring(L, getenv(acornL_checkstring(L, 1)));  /* if NULL push nil */
  return 1;
}


static int os_clock (acorn_State *L) {
  acorn_pushnumber(L, ((acorn_Number)clock())/(acorn_Number)CLOCKS_PER_SEC);
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
** is represented by a acorn_Integer, because either acorn_Integer is
** large enough to represent all int fields or it is not large enough
** to represent a time that cause a field to overflow.  However, if
** times are represented as doubles and acorn_Integer is int, then the
** time 0x1.e1853b0d184f6p+55 would cause an overflow when adding 1900
** to compute the year.
*/
static void setfield (acorn_State *L, const char *key, int value, int delta) {
  #if (defined(ACORN_NUMTIME) && ACORN_MAXINTEGER <= INT_MAX)
    if (l_unlikely(value > ACORN_MAXINTEGER - delta))
      acornL_error(L, "field '%s' is out-of-bound", key);
  #endif
  acorn_pushinteger(L, (acorn_Integer)value + delta);
  acorn_setfield(L, -2, key);
}


static void setboolfield (acorn_State *L, const char *key, int value) {
  if (value < 0)  /* undefined? */
    return;  /* does not set field */
  acorn_pushboolean(L, value);
  acorn_setfield(L, -2, key);
}


/*
** Set all fields from structure 'tm' in the table on top of the stack
*/
static void setallfields (acorn_State *L, struct tm *stm) {
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


static int getboolfield (acorn_State *L, const char *key) {
  int res;
  res = (acorn_getfield(L, -1, key) == ACORN_TNIL) ? -1 : acorn_toboolean(L, -1);
  acorn_pop(L, 1);
  return res;
}


static int getfield (acorn_State *L, const char *key, int d, int delta) {
  int isnum;
  int t = acorn_getfield(L, -1, key);  /* get field and its type */
  acorn_Integer res = acorn_tointegerx(L, -1, &isnum);
  if (!isnum) {  /* field is not an integer? */
    if (l_unlikely(t != ACORN_TNIL))  /* some other value? */
      return acornL_error(L, "field '%s' is not an integer", key);
    else if (l_unlikely(d < 0))  /* absent field; no default? */
      return acornL_error(L, "field '%s' missing in date table", key);
    res = d;
  }
  else {
    /* unsigned avoids overflow when acorn_Integer has 32 bits */
    if (!(res >= 0 ? (acorn_Unsigned)res <= (acorn_Unsigned)INT_MAX + delta
                   : (acorn_Integer)INT_MIN + delta <= res))
      return acornL_error(L, "field '%s' is out-of-bound", key);
    res -= delta;
  }
  acorn_pop(L, 1);
  return (int)res;
}


static const char *checkoption (acorn_State *L, const char *conv,
                                ptrdiff_t convlen, char *buff) {
  const char *option = ACORN_STRFTIMEOPTIONS;
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
  acornL_argerror(L, 1,
    acorn_pushfstring(L, "invalid conversion specifier '%%%s'", conv));
  return conv;  /* to avoid warnings */
}


static time_t l_checktime (acorn_State *L, int arg) {
  l_timet t = l_gettime(L, arg);
  acornL_argcheck(L, (time_t)t == t, arg, "time out-of-bounds");
  return (time_t)t;
}


/* maximum size for an individual 'strftime' item */
#define SIZETIMEFMT	250


static int os_date (acorn_State *L) {
  size_t slen;
  const char *s = acornL_optlstring(L, 1, "%c", &slen);
  time_t t = acornL_opt(L, l_checktime, 2, time(NULL));
  const char *se = s + slen;  /* 's' end */
  struct tm tmr, *stm;
  if (*s == '!') {  /* UTC? */
    stm = l_gmtime(&t, &tmr);
    s++;  /* skip '!' */
  }
  else
    stm = l_localtime(&t, &tmr);
  if (stm == NULL)  /* invalid date? */
    return acornL_error(L,
                 "date result cannot be represented in this installation");
  if (strcmp(s, "*t") == 0) {
    acorn_createtable(L, 0, 9);  /* 9 = number of fields */
    setallfields(L, stm);
  }
  else {
    char cc[4];  /* buffer for individual conversion specifiers */
    acornL_Buffer b;
    cc[0] = '%';
    acornL_buffinit(L, &b);
    while (s < se) {
      if (*s != '%')  /* not a conversion specifier? */
        acornL_addchar(&b, *s++);
      else {
        size_t reslen;
        char *buff = acornL_prepbuffsize(&b, SIZETIMEFMT);
        s++;  /* skip '%' */
        s = checkoption(L, s, se - s, cc + 1);  /* copy specifier to 'cc' */
        reslen = strftime(buff, SIZETIMEFMT, cc, stm);
        acornL_addsize(&b, reslen);
      }
    }
    acornL_pushresult(&b);
  }
  return 1;
}


static int os_time (acorn_State *L) {
  time_t t;
  if (acorn_isnoneornil(L, 1))  /* called without args? */
    t = time(NULL);  /* get current time */
  else {
    struct tm ts;
    acornL_checktype(L, 1, ACORN_TTABLE);
    acorn_settop(L, 1);  /* make sure table is at the top */
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
    return acornL_error(L,
                  "time result cannot be represented in this installation");
  l_pushtime(L, t);
  return 1;
}


static int os_difftime (acorn_State *L) {
  time_t t1 = l_checktime(L, 1);
  time_t t2 = l_checktime(L, 2);
  acorn_pushnumber(L, (acorn_Number)difftime(t1, t2));
  return 1;
}

/* }====================================================== */


static int os_setlocale (acorn_State *L) {
  static const int cat[] = {LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY,
                      LC_NUMERIC, LC_TIME};
  static const char *const catnames[] = {"all", "collate", "ctype", "monetary",
     "numeric", "time", NULL};
  const char *l = acornL_optstring(L, 1, NULL);
  int op = acornL_checkoption(L, 2, "all", catnames);
  acorn_pushstring(L, setlocale(cat[op], l));
  return 1;
}


static int os_exit (acorn_State *L) {
  int status;
  if (acorn_isboolean(L, 1))
    status = (acorn_toboolean(L, 1) ? EXIT_SUCCESS : EXIT_FAILURE);
  else
    status = (int)acornL_optinteger(L, 1, EXIT_SUCCESS);
  if (acorn_toboolean(L, 2))
    acorn_close(L);
  if (L) exit(status);  /* 'if' to avoid warnings for unreachable 'return' */
  return 0;
}


static const acornL_Reg syslib[] = {
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



ACORNMOD_API int acornopen_os (acorn_State *L) {
  acornL_newlib(L, syslib);
  return 1;
}

