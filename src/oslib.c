/*
** $Id: oslib.c $
** Standard Operating System library
** See Copyright Notice in venom.h
*/

#define oslib_c
#define VENOM_LIB

#include "prefix.h"


#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "venom.h"

#include "auxlib.h"
#include "venomlib.h"


/*
** {==================================================================
** List of valid conversion specifiers for the 'strftime' function;
** options are grouped by length; group of length 2 start with '||'.
** ===================================================================
*/
#if !defined(VENOM_STRFTIMEOPTIONS)	/* { */

/* options for ANSI C 89 (only 1-char options) */
#define L_STRFTIMEC89		"aAbBcdHIjmMpSUwWxXyYZ%"

/* options for ISO C 99 and POSIX */
#define L_STRFTIMEC99 "aAbBcCdDeFgGhHIjmMnprRStTuUVwWxXyYzZ%" \
    "||" "EcECExEXEyEY" "OdOeOHOIOmOMOSOuOUOVOwOWOy"  /* two-char options */

/* options for Windows */
#define L_STRFTIMEWIN "aAbBcdHIjmMpSUwWxXyYzZ%" \
    "||" "#c#x#d#H#I#j#m#M#S#U#w#W#y#Y"  /* two-char options */

#if defined(VENOM_USE_WINDOWS)
#define VENOM_STRFTIMEOPTIONS	L_STRFTIMEWIN
#elif defined(VENOM_USE_C89)
#define VENOM_STRFTIMEOPTIONS	L_STRFTIMEC89
#else  /* C99 specification */
#define VENOM_STRFTIMEOPTIONS	L_STRFTIMEC99
#endif

#endif					/* } */
/* }================================================================== */


/*
** {==================================================================
** Configuration for time-related stuff
** ===================================================================
*/

/*
** type to represent time_t in Venom
*/
#if !defined(VENOM_NUMTIME)	/* { */

#define l_timet			venom_Integer
#define l_pushtime(L,t)		venom_pushinteger(L,(venom_Integer)(t))
#define l_gettime(L,arg)	venomL_checkinteger(L, arg)

#else				/* }{ */

#define l_timet			venom_Number
#define l_pushtime(L,t)		venom_pushnumber(L,(venom_Number)(t))
#define l_gettime(L,arg)	venomL_checknumber(L, arg)

#endif				/* } */


#if !defined(l_gmtime)		/* { */
/*
** By default, Venom uses gmtime/localtime, except when POSIX is available,
** where it uses gmtime_r/localtime_r
*/

#if defined(VENOM_USE_POSIX)	/* { */

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
** By default, Venom uses tmpnam except when POSIX is available, where
** it uses mkstemp.
** ===================================================================
*/
#if !defined(venom_tmpnam)	/* { */

#if defined(VENOM_USE_POSIX)	/* { */

#include <unistd.h>

#define VENOM_TMPNAMBUFSIZE	32

#if !defined(VENOM_TMPNAMTEMPLATE)
#define VENOM_TMPNAMTEMPLATE	"/tmp/venom_XXXXXX"
#endif

#define venom_tmpnam(b,e) { \
        strcpy(b, VENOM_TMPNAMTEMPLATE); \
        e = mkstemp(b); \
        if (e != -1) close(e); \
        e = (e == -1); }

#else				/* }{ */

/* ISO C definitions */
#define VENOM_TMPNAMBUFSIZE	L_tmpnam
#define venom_tmpnam(b,e)		{ e = (tmpnam(b) == NULL); }

#endif				/* } */

#endif				/* } */
/* }================================================================== */



static int os_execute (venom_State *L) {
  const char *cmd = venomL_optstring(L, 1, NULL);
  int stat;
  errno = 0;
  stat = system(cmd);
  if (cmd != NULL)
    return venomL_execresult(L, stat);
  else {
    venom_pushboolean(L, stat);  /* true if there is a shell */
    return 1;
  }
}


static int os_remove (venom_State *L) {
  const char *filename = venomL_checkstring(L, 1);
  return venomL_fileresult(L, remove(filename) == 0, filename);
}


static int os_rename (venom_State *L) {
  const char *fromname = venomL_checkstring(L, 1);
  const char *toname = venomL_checkstring(L, 2);
  return venomL_fileresult(L, rename(fromname, toname) == 0, NULL);
}


static int os_tmpname (venom_State *L) {
  char buff[VENOM_TMPNAMBUFSIZE];
  int err;
  venom_tmpnam(buff, err);
  if (l_unlikely(err))
    return venomL_error(L, "unable to generate a unique filename");
  venom_pushstring(L, buff);
  return 1;
}


static int os_getenv (venom_State *L) {
  venom_pushstring(L, getenv(venomL_checkstring(L, 1)));  /* if NULL push nil */
  return 1;
}


static int os_clock (venom_State *L) {
  venom_pushnumber(L, ((venom_Number)clock())/(venom_Number)CLOCKS_PER_SEC);
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
** is represented by a venom_Integer, because either venom_Integer is
** large enough to represent all int fields or it is not large enough
** to represent a time that cause a field to overflow.  However, if
** times are represented as doubles and venom_Integer is int, then the
** time 0x1.e1853b0d184f6p+55 would cause an overflow when adding 1900
** to compute the year.
*/
static void setfield (venom_State *L, const char *key, int value, int delta) {
  #if (defined(VENOM_NUMTIME) && VENOM_MAXINTEGER <= INT_MAX)
    if (l_unlikely(value > VENOM_MAXINTEGER - delta))
      venomL_error(L, "field '%s' is out-of-bound", key);
  #endif
  venom_pushinteger(L, (venom_Integer)value + delta);
  venom_setfield(L, -2, key);
}


static void setboolfield (venom_State *L, const char *key, int value) {
  if (value < 0)  /* undefined? */
    return;  /* does not set field */
  venom_pushboolean(L, value);
  venom_setfield(L, -2, key);
}


/*
** Set all fields from structure 'tm' in the table on top of the stack
*/
static void setallfields (venom_State *L, struct tm *stm) {
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


static int getboolfield (venom_State *L, const char *key) {
  int res;
  res = (venom_getfield(L, -1, key) == VENOM_TNIL) ? -1 : venom_toboolean(L, -1);
  venom_pop(L, 1);
  return res;
}


static int getfield (venom_State *L, const char *key, int d, int delta) {
  int isnum;
  int t = venom_getfield(L, -1, key);  /* get field and its type */
  venom_Integer res = venom_tointegerx(L, -1, &isnum);
  if (!isnum) {  /* field is not an integer? */
    if (l_unlikely(t != VENOM_TNIL))  /* some other value? */
      return venomL_error(L, "field '%s' is not an integer", key);
    else if (l_unlikely(d < 0))  /* absent field; no default? */
      return venomL_error(L, "field '%s' missing in date table", key);
    res = d;
  }
  else {
    /* unsigned avoids overflow when venom_Integer has 32 bits */
    if (!(res >= 0 ? (venom_Unsigned)res <= (venom_Unsigned)INT_MAX + delta
                   : (venom_Integer)INT_MIN + delta <= res))
      return venomL_error(L, "field '%s' is out-of-bound", key);
    res -= delta;
  }
  venom_pop(L, 1);
  return (int)res;
}


static const char *checkoption (venom_State *L, const char *conv,
                                ptrdiff_t convlen, char *buff) {
  const char *option = VENOM_STRFTIMEOPTIONS;
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
  venomL_argerror(L, 1,
    venom_pushfstring(L, "invalid conversion specifier '%%%s'", conv));
  return conv;  /* to avoid warnings */
}


static time_t l_checktime (venom_State *L, int arg) {
  l_timet t = l_gettime(L, arg);
  venomL_argcheck(L, (time_t)t == t, arg, "time out-of-bounds");
  return (time_t)t;
}


/* maximum size for an individual 'strftime' item */
#define SIZETIMEFMT	250


static int os_date (venom_State *L) {
  size_t slen;
  const char *s = venomL_optlstring(L, 1, "%c", &slen);
  time_t t = venomL_opt(L, l_checktime, 2, time(NULL));
  const char *se = s + slen;  /* 's' end */
  struct tm tmr, *stm;
  if (*s == '!') {  /* UTC? */
    stm = l_gmtime(&t, &tmr);
    s++;  /* skip '!' */
  }
  else
    stm = l_localtime(&t, &tmr);
  if (stm == NULL)  /* invalid date? */
    return venomL_error(L,
                 "date result cannot be represented in this installation");
  if (strcmp(s, "*t") == 0) {
    venom_createtable(L, 0, 9);  /* 9 = number of fields */
    setallfields(L, stm);
  }
  else {
    char cc[4];  /* buffer for individual conversion specifiers */
    venomL_Buffer b;
    cc[0] = '%';
    venomL_buffinit(L, &b);
    while (s < se) {
      if (*s != '%')  /* not a conversion specifier? */
        venomL_addchar(&b, *s++);
      else {
        size_t reslen;
        char *buff = venomL_prepbuffsize(&b, SIZETIMEFMT);
        s++;  /* skip '%' */
        s = checkoption(L, s, se - s, cc + 1);  /* copy specifier to 'cc' */
        reslen = strftime(buff, SIZETIMEFMT, cc, stm);
        venomL_addsize(&b, reslen);
      }
    }
    venomL_pushresult(&b);
  }
  return 1;
}


static int os_time (venom_State *L) {
  time_t t;
  if (venom_isnoneornil(L, 1))  /* called without args? */
    t = time(NULL);  /* get current time */
  else {
    struct tm ts;
    venomL_checktype(L, 1, VENOM_TTABLE);
    venom_settop(L, 1);  /* make sure table is at the top */
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
    return venomL_error(L,
                  "time result cannot be represented in this installation");
  l_pushtime(L, t);
  return 1;
}


static int os_difftime (venom_State *L) {
  time_t t1 = l_checktime(L, 1);
  time_t t2 = l_checktime(L, 2);
  venom_pushnumber(L, (venom_Number)difftime(t1, t2));
  return 1;
}

/* }====================================================== */


static int os_setlocale (venom_State *L) {
  static const int cat[] = {LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY,
                      LC_NUMERIC, LC_TIME};
  static const char *const catnames[] = {"all", "collate", "ctype", "monetary",
     "numeric", "time", NULL};
  const char *l = venomL_optstring(L, 1, NULL);
  int op = venomL_checkoption(L, 2, "all", catnames);
  venom_pushstring(L, setlocale(cat[op], l));
  return 1;
}


static int os_exit (venom_State *L) {
  int status;
  if (venom_isboolean(L, 1))
    status = (venom_toboolean(L, 1) ? EXIT_SUCCESS : EXIT_FAILURE);
  else
    status = (int)venomL_optinteger(L, 1, EXIT_SUCCESS);
  if (venom_toboolean(L, 2))
    venom_close(L);
  if (L) exit(status);  /* 'if' to avoid warnings for unreachable 'return' */
  return 0;
}


static const venomL_Reg syslib[] = {
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



VENOMMOD_API int venomopen_os (venom_State *L) {
  venomL_newlib(L, syslib);
  return 1;
}