/*
** $Id: oslib.c $
** Standard Operating System library
** See Copyright Notice in viper.h
*/

#define oslib_c
#define VIPER_LIB

#include "prefix.h"


#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "viper.h"

#include "auxlib.h"
#include "viperlib.h"


/*
** {==================================================================
** List of valid conversion specifiers for the 'strftime' function;
** options are grouped by length; group of length 2 start with '||'.
** ===================================================================
*/
#if !defined(VIPER_STRFTIMEOPTIONS)	/* { */

/* options for ANSI C 89 (only 1-char options) */
#define L_STRFTIMEC89		"aAbBcdHIjmMpSUwWxXyYZ%"

/* options for ISO C 99 and POSIX */
#define L_STRFTIMEC99 "aAbBcCdDeFgGhHIjmMnprRStTuUVwWxXyYzZ%" \
    "||" "EcECExEXEyEY" "OdOeOHOIOmOMOSOuOUOVOwOWOy"  /* two-char options */

/* options for Windows */
#define L_STRFTIMEWIN "aAbBcdHIjmMpSUwWxXyYzZ%" \
    "||" "#c#x#d#H#I#j#m#M#S#U#w#W#y#Y"  /* two-char options */

#if defined(VIPER_USE_WINDOWS)
#define VIPER_STRFTIMEOPTIONS	L_STRFTIMEWIN
#elif defined(VIPER_USE_C89)
#define VIPER_STRFTIMEOPTIONS	L_STRFTIMEC89
#else  /* C99 specification */
#define VIPER_STRFTIMEOPTIONS	L_STRFTIMEC99
#endif

#endif					/* } */
/* }================================================================== */


/*
** {==================================================================
** Configuration for time-related stuff
** ===================================================================
*/

/*
** type to represent time_t in Viper
*/
#if !defined(VIPER_NUMTIME)	/* { */

#define l_timet			viper_Integer
#define l_pushtime(L,t)		viper_pushinteger(L,(viper_Integer)(t))
#define l_gettime(L,arg)	viperL_checkinteger(L, arg)

#else				/* }{ */

#define l_timet			viper_Number
#define l_pushtime(L,t)		viper_pushnumber(L,(viper_Number)(t))
#define l_gettime(L,arg)	viperL_checknumber(L, arg)

#endif				/* } */


#if !defined(l_gmtime)		/* { */
/*
** By default, Viper uses gmtime/localtime, except when POSIX is available,
** where it uses gmtime_r/localtime_r
*/

#if defined(VIPER_USE_POSIX)	/* { */

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
** By default, Viper uses tmpnam except when POSIX is available, where
** it uses mkstemp.
** ===================================================================
*/
#if !defined(viper_tmpnam)	/* { */

#if defined(VIPER_USE_POSIX)	/* { */

#include <unistd.h>

#define VIPER_TMPNAMBUFSIZE	32

#if !defined(VIPER_TMPNAMTEMPLATE)
#define VIPER_TMPNAMTEMPLATE	"/tmp/viper_XXXXXX"
#endif

#define viper_tmpnam(b,e) { \
        strcpy(b, VIPER_TMPNAMTEMPLATE); \
        e = mkstemp(b); \
        if (e != -1) close(e); \
        e = (e == -1); }

#else				/* }{ */

/* ISO C definitions */
#define VIPER_TMPNAMBUFSIZE	L_tmpnam
#define viper_tmpnam(b,e)		{ e = (tmpnam(b) == NULL); }

#endif				/* } */

#endif				/* } */
/* }================================================================== */



static int os_execute (viper_State *L) {
  const char *cmd = viperL_optstring(L, 1, NULL);
  int stat;
  errno = 0;
  stat = system(cmd);
  if (cmd != NULL)
    return viperL_execresult(L, stat);
  else {
    viper_pushboolean(L, stat);  /* true if there is a shell */
    return 1;
  }
}


static int os_remove (viper_State *L) {
  const char *filename = viperL_checkstring(L, 1);
  return viperL_fileresult(L, remove(filename) == 0, filename);
}


static int os_rename (viper_State *L) {
  const char *fromname = viperL_checkstring(L, 1);
  const char *toname = viperL_checkstring(L, 2);
  return viperL_fileresult(L, rename(fromname, toname) == 0, NULL);
}


static int os_tmpname (viper_State *L) {
  char buff[VIPER_TMPNAMBUFSIZE];
  int err;
  viper_tmpnam(buff, err);
  if (l_unlikely(err))
    return viperL_error(L, "unable to generate a unique filename");
  viper_pushstring(L, buff);
  return 1;
}


static int os_getenv (viper_State *L) {
  viper_pushstring(L, getenv(viperL_checkstring(L, 1)));  /* if NULL push nil */
  return 1;
}


static int os_clock (viper_State *L) {
  viper_pushnumber(L, ((viper_Number)clock())/(viper_Number)CLOCKS_PER_SEC);
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
** is represented by a viper_Integer, because either viper_Integer is
** large enough to represent all int fields or it is not large enough
** to represent a time that cause a field to overflow.  However, if
** times are represented as doubles and viper_Integer is int, then the
** time 0x1.e1853b0d184f6p+55 would cause an overflow when adding 1900
** to compute the year.
*/
static void setfield (viper_State *L, const char *key, int value, int delta) {
  #if (defined(VIPER_NUMTIME) && VIPER_MAXINTEGER <= INT_MAX)
    if (l_unlikely(value > VIPER_MAXINTEGER - delta))
      viperL_error(L, "field '%s' is out-of-bound", key);
  #endif
  viper_pushinteger(L, (viper_Integer)value + delta);
  viper_setfield(L, -2, key);
}


static void setboolfield (viper_State *L, const char *key, int value) {
  if (value < 0)  /* undefined? */
    return;  /* does not set field */
  viper_pushboolean(L, value);
  viper_setfield(L, -2, key);
}


/*
** Set all fields from structure 'tm' in the table on top of the stack
*/
static void setallfields (viper_State *L, struct tm *stm) {
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


static int getboolfield (viper_State *L, const char *key) {
  int res;
  res = (viper_getfield(L, -1, key) == VIPER_TNIL) ? -1 : viper_toboolean(L, -1);
  viper_pop(L, 1);
  return res;
}


static int getfield (viper_State *L, const char *key, int d, int delta) {
  int isnum;
  int t = viper_getfield(L, -1, key);  /* get field and its type */
  viper_Integer res = viper_tointegerx(L, -1, &isnum);
  if (!isnum) {  /* field is not an integer? */
    if (l_unlikely(t != VIPER_TNIL))  /* some other value? */
      return viperL_error(L, "field '%s' is not an integer", key);
    else if (l_unlikely(d < 0))  /* absent field; no default? */
      return viperL_error(L, "field '%s' missing in date table", key);
    res = d;
  }
  else {
    /* unsigned avoids overflow when viper_Integer has 32 bits */
    if (!(res >= 0 ? (viper_Unsigned)res <= (viper_Unsigned)INT_MAX + delta
                   : (viper_Integer)INT_MIN + delta <= res))
      return viperL_error(L, "field '%s' is out-of-bound", key);
    res -= delta;
  }
  viper_pop(L, 1);
  return (int)res;
}


static const char *checkoption (viper_State *L, const char *conv,
                                ptrdiff_t convlen, char *buff) {
  const char *option = VIPER_STRFTIMEOPTIONS;
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
  viperL_argerror(L, 1,
    viper_pushfstring(L, "invalid conversion specifier '%%%s'", conv));
  return conv;  /* to avoid warnings */
}


static time_t l_checktime (viper_State *L, int arg) {
  l_timet t = l_gettime(L, arg);
  viperL_argcheck(L, (time_t)t == t, arg, "time out-of-bounds");
  return (time_t)t;
}


/* maximum size for an individual 'strftime' item */
#define SIZETIMEFMT	250


static int os_date (viper_State *L) {
  size_t slen;
  const char *s = viperL_optlstring(L, 1, "%c", &slen);
  time_t t = viperL_opt(L, l_checktime, 2, time(NULL));
  const char *se = s + slen;  /* 's' end */
  struct tm tmr, *stm;
  if (*s == '!') {  /* UTC? */
    stm = l_gmtime(&t, &tmr);
    s++;  /* skip '!' */
  }
  else
    stm = l_localtime(&t, &tmr);
  if (stm == NULL)  /* invalid date? */
    return viperL_error(L,
                 "date result cannot be represented in this installation");
  if (strcmp(s, "*t") == 0) {
    viper_createtable(L, 0, 9);  /* 9 = number of fields */
    setallfields(L, stm);
  }
  else {
    char cc[4];  /* buffer for individual conversion specifiers */
    viperL_Buffer b;
    cc[0] = '%';
    viperL_buffinit(L, &b);
    while (s < se) {
      if (*s != '%')  /* not a conversion specifier? */
        viperL_addchar(&b, *s++);
      else {
        size_t reslen;
        char *buff = viperL_prepbuffsize(&b, SIZETIMEFMT);
        s++;  /* skip '%' */
        s = checkoption(L, s, se - s, cc + 1);  /* copy specifier to 'cc' */
        reslen = strftime(buff, SIZETIMEFMT, cc, stm);
        viperL_addsize(&b, reslen);
      }
    }
    viperL_pushresult(&b);
  }
  return 1;
}


static int os_time (viper_State *L) {
  time_t t;
  if (viper_isnoneornil(L, 1))  /* called without args? */
    t = time(NULL);  /* get current time */
  else {
    struct tm ts;
    viperL_checktype(L, 1, VIPER_TTABLE);
    viper_settop(L, 1);  /* make sure table is at the top */
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
    return viperL_error(L,
                  "time result cannot be represented in this installation");
  l_pushtime(L, t);
  return 1;
}


static int os_difftime (viper_State *L) {
  time_t t1 = l_checktime(L, 1);
  time_t t2 = l_checktime(L, 2);
  viper_pushnumber(L, (viper_Number)difftime(t1, t2));
  return 1;
}

/* }====================================================== */


static int os_setlocale (viper_State *L) {
  static const int cat[] = {LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY,
                      LC_NUMERIC, LC_TIME};
  static const char *const catnames[] = {"all", "collate", "ctype", "monetary",
     "numeric", "time", NULL};
  const char *l = viperL_optstring(L, 1, NULL);
  int op = viperL_checkoption(L, 2, "all", catnames);
  viper_pushstring(L, setlocale(cat[op], l));
  return 1;
}


static int os_exit (viper_State *L) {
  int status;
  if (viper_isboolean(L, 1))
    status = (viper_toboolean(L, 1) ? EXIT_SUCCESS : EXIT_FAILURE);
  else
    status = (int)viperL_optinteger(L, 1, EXIT_SUCCESS);
  if (viper_toboolean(L, 2))
    viper_close(L);
  if (L) exit(status);  /* 'if' to avoid warnings for unreachable 'return' */
  return 0;
}


static const viperL_Reg syslib[] = {
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



VIPERMOD_API int viperopen_os (viper_State *L) {
  viperL_newlib(L, syslib);
  return 1;
}