/*
** $Id: loslib.c $
** Standard Operating System library
** See Copyright Notice in cup.h
*/

#define loslib_c
#define CUP_LIB

#include "lprefix.h"


#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cup.h"

#include "lauxlib.h"
#include "cuplib.h"


/*
** {==================================================================
** List of valid conversion specifiers for the 'strftime' function;
** options are grouped by length; group of length 2 start with '||'.
** ===================================================================
*/
#if !defined(CUP_STRFTIMEOPTIONS)	/* { */

/* options for ANSI C 89 (only 1-char options) */
#define L_STRFTIMEC89		"aAbBcdHIjmMpSUwWxXyYZ%"

/* options for ISO C 99 and POSIX */
#define L_STRFTIMEC99 "aAbBcCdDeFgGhHIjmMnprRStTuUVwWxXyYzZ%" \
    "||" "EcECExEXEyEY" "OdOeOHOIOmOMOSOuOUOVOwOWOy"  /* two-char options */

/* options for Windows */
#define L_STRFTIMEWIN "aAbBcdHIjmMpSUwWxXyYzZ%" \
    "||" "#c#x#d#H#I#j#m#M#S#U#w#W#y#Y"  /* two-char options */

#if defined(CUP_USE_WINDOWS)
#define CUP_STRFTIMEOPTIONS	L_STRFTIMEWIN
#elif defined(CUP_USE_C89)
#define CUP_STRFTIMEOPTIONS	L_STRFTIMEC89
#else  /* C99 specification */
#define CUP_STRFTIMEOPTIONS	L_STRFTIMEC99
#endif

#endif					/* } */
/* }================================================================== */


/*
** {==================================================================
** Configuration for time-related stuff
** ===================================================================
*/

/*
** type to represent time_t in Cup
*/
#if !defined(CUP_NUMTIME)	/* { */

#define l_timet			cup_Integer
#define l_pushtime(L,t)		cup_pushinteger(L,(cup_Integer)(t))
#define l_gettime(L,arg)	cupL_checkinteger(L, arg)

#else				/* }{ */

#define l_timet			cup_Number
#define l_pushtime(L,t)		cup_pushnumber(L,(cup_Number)(t))
#define l_gettime(L,arg)	cupL_checknumber(L, arg)

#endif				/* } */


#if !defined(l_gmtime)		/* { */
/*
** By default, Cup uses gmtime/localtime, except when POSIX is available,
** where it uses gmtime_r/localtime_r
*/

#if defined(CUP_USE_POSIX)	/* { */

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
** By default, Cup uses tmpnam except when POSIX is available, where
** it uses mkstemp.
** ===================================================================
*/
#if !defined(cup_tmpnam)	/* { */

#if defined(CUP_USE_POSIX)	/* { */

#include <unistd.h>

#define CUP_TMPNAMBUFSIZE	32

#if !defined(CUP_TMPNAMTEMPLATE)
#define CUP_TMPNAMTEMPLATE	"/tmp/cup_XXXXXX"
#endif

#define cup_tmpnam(b,e) { \
        strcpy(b, CUP_TMPNAMTEMPLATE); \
        e = mkstemp(b); \
        if (e != -1) close(e); \
        e = (e == -1); }

#else				/* }{ */

/* ISO C definitions */
#define CUP_TMPNAMBUFSIZE	L_tmpnam
#define cup_tmpnam(b,e)		{ e = (tmpnam(b) == NULL); }

#endif				/* } */

#endif				/* } */
/* }================================================================== */



static int os_execute (cup_State *L) {
  const char *cmd = cupL_optstring(L, 1, NULL);
  int stat;
  errno = 0;
  stat = system(cmd);
  if (cmd != NULL)
    return cupL_execresult(L, stat);
  else {
    cup_pushboolean(L, stat);  /* true if there is a shell */
    return 1;
  }
}


static int os_remove (cup_State *L) {
  const char *filename = cupL_checkstring(L, 1);
  return cupL_fileresult(L, remove(filename) == 0, filename);
}


static int os_rename (cup_State *L) {
  const char *fromname = cupL_checkstring(L, 1);
  const char *toname = cupL_checkstring(L, 2);
  return cupL_fileresult(L, rename(fromname, toname) == 0, NULL);
}


static int os_tmpname (cup_State *L) {
  char buff[CUP_TMPNAMBUFSIZE];
  int err;
  cup_tmpnam(buff, err);
  if (l_unlikely(err))
    return cupL_error(L, "unable to generate a unique filename");
  cup_pushstring(L, buff);
  return 1;
}


static int os_getenv (cup_State *L) {
  cup_pushstring(L, getenv(cupL_checkstring(L, 1)));  /* if NULL push nil */
  return 1;
}


static int os_clock (cup_State *L) {
  cup_pushnumber(L, ((cup_Number)clock())/(cup_Number)CLOCKS_PER_SEC);
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
** is represented by a cup_Integer, because either cup_Integer is
** large enough to represent all int fields or it is not large enough
** to represent a time that cause a field to overflow.  However, if
** times are represented as doubles and cup_Integer is int, then the
** time 0x1.e1853b0d184f6p+55 would cause an overflow when adding 1900
** to compute the year.
*/
static void setfield (cup_State *L, const char *key, int value, int delta) {
  #if (defined(CUP_NUMTIME) && CUP_MAXINTEGER <= INT_MAX)
    if (l_unlikely(value > CUP_MAXINTEGER - delta))
      cupL_error(L, "field '%s' is out-of-bound", key);
  #endif
  cup_pushinteger(L, (cup_Integer)value + delta);
  cup_setfield(L, -2, key);
}


static void setboolfield (cup_State *L, const char *key, int value) {
  if (value < 0)  /* undefined? */
    return;  /* does not set field */
  cup_pushboolean(L, value);
  cup_setfield(L, -2, key);
}


/*
** Set all fields from structure 'tm' in the table on top of the stack
*/
static void setallfields (cup_State *L, struct tm *stm) {
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


static int getboolfield (cup_State *L, const char *key) {
  int res;
  res = (cup_getfield(L, -1, key) == CUP_TNIL) ? -1 : cup_toboolean(L, -1);
  cup_pop(L, 1);
  return res;
}


static int getfield (cup_State *L, const char *key, int d, int delta) {
  int isnum;
  int t = cup_getfield(L, -1, key);  /* get field and its type */
  cup_Integer res = cup_tointegerx(L, -1, &isnum);
  if (!isnum) {  /* field is not an integer? */
    if (l_unlikely(t != CUP_TNIL))  /* some other value? */
      return cupL_error(L, "field '%s' is not an integer", key);
    else if (l_unlikely(d < 0))  /* absent field; no default? */
      return cupL_error(L, "field '%s' missing in date table", key);
    res = d;
  }
  else {
    /* unsigned avoids overflow when cup_Integer has 32 bits */
    if (!(res >= 0 ? (cup_Unsigned)res <= (cup_Unsigned)INT_MAX + delta
                   : (cup_Integer)INT_MIN + delta <= res))
      return cupL_error(L, "field '%s' is out-of-bound", key);
    res -= delta;
  }
  cup_pop(L, 1);
  return (int)res;
}


static const char *checkoption (cup_State *L, const char *conv,
                                ptrdiff_t convlen, char *buff) {
  const char *option = CUP_STRFTIMEOPTIONS;
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
  cupL_argerror(L, 1,
    cup_pushfstring(L, "invalid conversion specifier '%%%s'", conv));
  return conv;  /* to avoid warnings */
}


static time_t l_checktime (cup_State *L, int arg) {
  l_timet t = l_gettime(L, arg);
  cupL_argcheck(L, (time_t)t == t, arg, "time out-of-bounds");
  return (time_t)t;
}


/* maximum size for an individual 'strftime' item */
#define SIZETIMEFMT	250


static int os_date (cup_State *L) {
  size_t slen;
  const char *s = cupL_optlstring(L, 1, "%c", &slen);
  time_t t = cupL_opt(L, l_checktime, 2, time(NULL));
  const char *se = s + slen;  /* 's' end */
  struct tm tmr, *stm;
  if (*s == '!') {  /* UTC? */
    stm = l_gmtime(&t, &tmr);
    s++;  /* skip '!' */
  }
  else
    stm = l_localtime(&t, &tmr);
  if (stm == NULL)  /* invalid date? */
    return cupL_error(L,
                 "date result cannot be represented in this installation");
  if (strcmp(s, "*t") == 0) {
    cup_createtable(L, 0, 9);  /* 9 = number of fields */
    setallfields(L, stm);
  }
  else {
    char cc[4];  /* buffer for individual conversion specifiers */
    cupL_Buffer b;
    cc[0] = '%';
    cupL_buffinit(L, &b);
    while (s < se) {
      if (*s != '%')  /* not a conversion specifier? */
        cupL_addchar(&b, *s++);
      else {
        size_t reslen;
        char *buff = cupL_prepbuffsize(&b, SIZETIMEFMT);
        s++;  /* skip '%' */
        s = checkoption(L, s, se - s, cc + 1);  /* copy specifier to 'cc' */
        reslen = strftime(buff, SIZETIMEFMT, cc, stm);
        cupL_addsize(&b, reslen);
      }
    }
    cupL_pushresult(&b);
  }
  return 1;
}


static int os_time (cup_State *L) {
  time_t t;
  if (cup_isnoneornil(L, 1))  /* called without args? */
    t = time(NULL);  /* get current time */
  else {
    struct tm ts;
    cupL_checktype(L, 1, CUP_TTABLE);
    cup_settop(L, 1);  /* make sure table is at the top */
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
    return cupL_error(L,
                  "time result cannot be represented in this installation");
  l_pushtime(L, t);
  return 1;
}


static int os_difftime (cup_State *L) {
  time_t t1 = l_checktime(L, 1);
  time_t t2 = l_checktime(L, 2);
  cup_pushnumber(L, (cup_Number)difftime(t1, t2));
  return 1;
}

/* }====================================================== */


static int os_setlocale (cup_State *L) {
  static const int cat[] = {LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY,
                      LC_NUMERIC, LC_TIME};
  static const char *const catnames[] = {"all", "collate", "ctype", "monetary",
     "numeric", "time", NULL};
  const char *l = cupL_optstring(L, 1, NULL);
  int op = cupL_checkoption(L, 2, "all", catnames);
  cup_pushstring(L, setlocale(cat[op], l));
  return 1;
}


static int os_exit (cup_State *L) {
  int status;
  if (cup_isboolean(L, 1))
    status = (cup_toboolean(L, 1) ? EXIT_SUCCESS : EXIT_FAILURE);
  else
    status = (int)cupL_optinteger(L, 1, EXIT_SUCCESS);
  if (cup_toboolean(L, 2))
    cup_close(L);
  if (L) exit(status);  /* 'if' to avoid warnings for unreachable 'return' */
  return 0;
}


static const cupL_Reg syslib[] = {
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



CUPMOD_API int cupopen_os (cup_State *L) {
  cupL_newlib(L, syslib);
  return 1;
}

