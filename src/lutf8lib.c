/*
** $Id: lutf8lib.c $
** Standard library for UTF-8 manipulation
** See Copyright Notice in cup.h
*/

#define lutf8lib_c
#define CUP_LIB

#include "lprefix.h"


#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "cup.h"

#include "lauxlib.h"
#include "cuplib.h"


#define MAXUNICODE	0x10FFFFu

#define MAXUTF		0x7FFFFFFFu

/*
** Integer type for decoded UTF-8 values; MAXUTF needs 31 bits.
*/
#if (UINT_MAX >> 30) >= 1
typedef unsigned int utfint;
#else
typedef unsigned long utfint;
#endif


#define iscont(p)	((*(p) & 0xC0) == 0x80)


/* from strlib */
/* translate a relative string position: negative means back from end */
static cup_Integer u_posrelat (cup_Integer pos, size_t len) {
  if (pos >= 0) return pos;
  else if (0u - (size_t)pos > len) return 0;
  else return (cup_Integer)len + pos + 1;
}


/*
** Decode one UTF-8 sequence, returning NULL if byte sequence is
** invalid.  The array 'limits' stores the minimum value for each
** sequence length, to check for overlong representations. Its first
** entry forces an error for non-ascii bytes with no continuation
** bytes (count == 0).
*/
static const char *utf8_decode (const char *s, utfint *val, int strict) {
  static const utfint limits[] =
        {~(utfint)0, 0x80, 0x800, 0x10000u, 0x200000u, 0x4000000u};
  unsigned int c = (unsigned char)s[0];
  utfint res = 0;  /* final result */
  if (c < 0x80)  /* ascii? */
    res = c;
  else {
    int count = 0;  /* to count number of continuation bytes */
    for (; c & 0x40; c <<= 1) {  /* while it needs continuation bytes... */
      unsigned int cc = (unsigned char)s[++count];  /* read next byte */
      if ((cc & 0xC0) != 0x80)  /* not a continuation byte? */
        return NULL;  /* invalid byte sequence */
      res = (res << 6) | (cc & 0x3F);  /* add lower 6 bits from cont. byte */
    }
    res |= ((utfint)(c & 0x7F) << (count * 5));  /* add first byte */
    if (count > 5 || res > MAXUTF || res < limits[count])
      return NULL;  /* invalid byte sequence */
    s += count;  /* skip continuation bytes read */
  }
  if (strict) {
    /* check for invalid code points; too large or surrogates */
    if (res > MAXUNICODE || (0xD800u <= res && res <= 0xDFFFu))
      return NULL;
  }
  if (val) *val = res;
  return s + 1;  /* +1 to include first byte */
}


/*
** utf8len(s [, i [, j [, lax]]]) --> number of characters that
** start in the range [i,j], or nil + current position if 's' is not
** well formed in that interval
*/
static int utflen (cup_State *L) {
  cup_Integer n = 0;  /* counter for the number of characters */
  size_t len;  /* string length in bytes */
  const char *s = cupL_checklstring(L, 1, &len);
  cup_Integer posi = u_posrelat(cupL_optinteger(L, 2, 1), len);
  cup_Integer posj = u_posrelat(cupL_optinteger(L, 3, -1), len);
  int lax = cup_toboolean(L, 4);
  cupL_argcheck(L, 1 <= posi && --posi <= (cup_Integer)len, 2,
                   "initial position out of bounds");
  cupL_argcheck(L, --posj < (cup_Integer)len, 3,
                   "final position out of bounds");
  while (posi <= posj) {
    const char *s1 = utf8_decode(s + posi, NULL, !lax);
    if (s1 == NULL) {  /* conversion error? */
      cupL_pushfail(L);  /* return fail ... */
      cup_pushinteger(L, posi + 1);  /* ... and current position */
      return 2;
    }
    posi = s1 - s;
    n++;
  }
  cup_pushinteger(L, n);
  return 1;
}


/*
** codepoint(s, [i, [j [, lax]]]) -> returns codepoints for all
** characters that start in the range [i,j]
*/
static int codepoint (cup_State *L) {
  size_t len;
  const char *s = cupL_checklstring(L, 1, &len);
  cup_Integer posi = u_posrelat(cupL_optinteger(L, 2, 1), len);
  cup_Integer pose = u_posrelat(cupL_optinteger(L, 3, posi), len);
  int lax = cup_toboolean(L, 4);
  int n;
  const char *se;
  cupL_argcheck(L, posi >= 1, 2, "out of bounds");
  cupL_argcheck(L, pose <= (cup_Integer)len, 3, "out of bounds");
  if (posi > pose) return 0;  /* empty interval; return no values */
  if (pose - posi >= INT_MAX)  /* (cup_Integer -> int) overflow? */
    return cupL_error(L, "string slice too long");
  n = (int)(pose -  posi) + 1;  /* upper bound for number of returns */
  cupL_checkstack(L, n, "string slice too long");
  n = 0;  /* count the number of returns */
  se = s + pose;  /* string end */
  for (s += posi - 1; s < se;) {
    utfint code;
    s = utf8_decode(s, &code, !lax);
    if (s == NULL)
      return cupL_error(L, "invalid UTF-8 code");
    cup_pushinteger(L, code);
    n++;
  }
  return n;
}


static void pushutfchar (cup_State *L, int arg) {
  cup_Unsigned code = (cup_Unsigned)cupL_checkinteger(L, arg);
  cupL_argcheck(L, code <= MAXUTF, arg, "value out of range");
  cup_pushfstring(L, "%U", (long)code);
}


/*
** utfchar(n1, n2, ...)  -> char(n1)..char(n2)...
*/
static int utfchar (cup_State *L) {
  int n = cup_gettop(L);  /* number of arguments */
  if (n == 1)  /* optimize common case of single char */
    pushutfchar(L, 1);
  else {
    int i;
    cupL_Buffer b;
    cupL_buffinit(L, &b);
    for (i = 1; i <= n; i++) {
      pushutfchar(L, i);
      cupL_addvalue(&b);
    }
    cupL_pushresult(&b);
  }
  return 1;
}


/*
** offset(s, n, [i])  -> index where n-th character counting from
**   position 'i' starts; 0 means character at 'i'.
*/
static int byteoffset (cup_State *L) {
  size_t len;
  const char *s = cupL_checklstring(L, 1, &len);
  cup_Integer n  = cupL_checkinteger(L, 2);
  cup_Integer posi = (n >= 0) ? 1 : len + 1;
  posi = u_posrelat(cupL_optinteger(L, 3, posi), len);
  cupL_argcheck(L, 1 <= posi && --posi <= (cup_Integer)len, 3,
                   "position out of bounds");
  if (n == 0) {
    /* find beginning of current byte sequence */
    while (posi > 0 && iscont(s + posi)) posi--;
  }
  else {
    if (iscont(s + posi))
      return cupL_error(L, "initial position is a continuation byte");
    if (n < 0) {
       while (n < 0 && posi > 0) {  /* move back */
         do {  /* find beginning of previous character */
           posi--;
         } while (posi > 0 && iscont(s + posi));
         n++;
       }
     }
     else {
       n--;  /* do not move for 1st character */
       while (n > 0 && posi < (cup_Integer)len) {
         do {  /* find beginning of next character */
           posi++;
         } while (iscont(s + posi));  /* (cannot pass final '\0') */
         n--;
       }
     }
  }
  if (n == 0)  /* did it find given character? */
    cup_pushinteger(L, posi + 1);
  else  /* no such character */
    cupL_pushfail(L);
  return 1;
}


static int iter_aux (cup_State *L, int strict) {
  size_t len;
  const char *s = cupL_checklstring(L, 1, &len);
  cup_Unsigned n = (cup_Unsigned)cup_tointeger(L, 2);
  if (n < len) {
    while (iscont(s + n)) n++;  /* skip continuation bytes */
  }
  if (n >= len)  /* (also handles original 'n' being negative) */
    return 0;  /* no more codepoints */
  else {
    utfint code;
    const char *next = utf8_decode(s + n, &code, strict);
    if (next == NULL)
      return cupL_error(L, "invalid UTF-8 code");
    cup_pushinteger(L, n + 1);
    cup_pushinteger(L, code);
    return 2;
  }
}


static int iter_auxstrict (cup_State *L) {
  return iter_aux(L, 1);
}

static int iter_auxlax (cup_State *L) {
  return iter_aux(L, 0);
}


static int iter_codes (cup_State *L) {
  int lax = cup_toboolean(L, 2);
  cupL_checkstring(L, 1);
  cup_pushcfunction(L, lax ? iter_auxlax : iter_auxstrict);
  cup_pushvalue(L, 1);
  cup_pushinteger(L, 0);
  return 3;
}


/* pattern to match a single UTF-8 character */
#define UTF8PATT	"[\0-\x7F\xC2-\xFD][\x80-\xBF]*"


static const cupL_Reg funcs[] = {
  {"offset", byteoffset},
  {"codepoint", codepoint},
  {"char", utfchar},
  {"len", utflen},
  {"codes", iter_codes},
  /* placeholders */
  {"charpattern", NULL},
  {NULL, NULL}
};


CUPMOD_API int cupopen_utf8 (cup_State *L) {
  cupL_newlib(L, funcs);
  cup_pushlstring(L, UTF8PATT, sizeof(UTF8PATT)/sizeof(char) - 1);
  cup_setfield(L, -2, "charpattern");
  return 1;
}

