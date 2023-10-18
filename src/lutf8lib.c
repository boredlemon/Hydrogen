/*
** $Id: lutf8lib.c $
** Standard library for UTF-8 manipulation
** See Copyright Notice in acorn.h
*/

#define lutf8lib_c
#define ACORN_LIB

#include "lprefix.h"


#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "acorn.h"

#include "lauxlib.h"
#include "acornlib.h"


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
static acorn_Integer u_posrelat (acorn_Integer pos, size_t len) {
  if (pos >= 0) return pos;
  else if (0u - (size_t)pos > len) return 0;
  else return (acorn_Integer)len + pos + 1;
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
static int utflen (acorn_State *L) {
  acorn_Integer n = 0;  /* counter for the number of characters */
  size_t len;  /* string length in bytes */
  const char *s = acornL_checklstring(L, 1, &len);
  acorn_Integer posi = u_posrelat(acornL_optinteger(L, 2, 1), len);
  acorn_Integer posj = u_posrelat(acornL_optinteger(L, 3, -1), len);
  int lax = acorn_toboolean(L, 4);
  acornL_argcheck(L, 1 <= posi && --posi <= (acorn_Integer)len, 2,
                   "initial position out of bounds");
  acornL_argcheck(L, --posj < (acorn_Integer)len, 3,
                   "final position out of bounds");
  while (posi <= posj) {
    const char *s1 = utf8_decode(s + posi, NULL, !lax);
    if (s1 == NULL) {  /* conversion error? */
      acornL_pushfail(L);  /* return fail ... */
      acorn_pushinteger(L, posi + 1);  /* ... and current position */
      return 2;
    }
    posi = s1 - s;
    n++;
  }
  acorn_pushinteger(L, n);
  return 1;
}


/*
** codepoint(s, [i, [j [, lax]]]) -> returns codepoints for all
** characters that start in the range [i,j]
*/
static int codepoint (acorn_State *L) {
  size_t len;
  const char *s = acornL_checklstring(L, 1, &len);
  acorn_Integer posi = u_posrelat(acornL_optinteger(L, 2, 1), len);
  acorn_Integer pose = u_posrelat(acornL_optinteger(L, 3, posi), len);
  int lax = acorn_toboolean(L, 4);
  int n;
  const char *se;
  acornL_argcheck(L, posi >= 1, 2, "out of bounds");
  acornL_argcheck(L, pose <= (acorn_Integer)len, 3, "out of bounds");
  if (posi > pose) return 0;  /* empty interval; return no values */
  if (pose - posi >= INT_MAX)  /* (acorn_Integer -> int) overflow? */
    return acornL_error(L, "string slice too long");
  n = (int)(pose -  posi) + 1;  /* upper bound for number of returns */
  acornL_checkstack(L, n, "string slice too long");
  n = 0;  /* count the number of returns */
  se = s + pose;  /* string end */
  for (s += posi - 1; s < se;) {
    utfint code;
    s = utf8_decode(s, &code, !lax);
    if (s == NULL)
      return acornL_error(L, "invalid UTF-8 code");
    acorn_pushinteger(L, code);
    n++;
  }
  return n;
}


static void pushutfchar (acorn_State *L, int arg) {
  acorn_Unsigned code = (acorn_Unsigned)acornL_checkinteger(L, arg);
  acornL_argcheck(L, code <= MAXUTF, arg, "value out of range");
  acorn_pushfstring(L, "%U", (long)code);
}


/*
** utfchar(n1, n2, ...)  -> char(n1)..char(n2)...
*/
static int utfchar (acorn_State *L) {
  int n = acorn_gettop(L);  /* number of arguments */
  if (n == 1)  /* optimize common case of single char */
    pushutfchar(L, 1);
  else {
    int i;
    acornL_Buffer b;
    acornL_buffinit(L, &b);
    for (i = 1; i <= n; i++) {
      pushutfchar(L, i);
      acornL_addvalue(&b);
    }
    acornL_pushresult(&b);
  }
  return 1;
}


/*
** offset(s, n, [i])  -> index where n-th character counting from
**   position 'i' starts; 0 means character at 'i'.
*/
static int byteoffset (acorn_State *L) {
  size_t len;
  const char *s = acornL_checklstring(L, 1, &len);
  acorn_Integer n  = acornL_checkinteger(L, 2);
  acorn_Integer posi = (n >= 0) ? 1 : len + 1;
  posi = u_posrelat(acornL_optinteger(L, 3, posi), len);
  acornL_argcheck(L, 1 <= posi && --posi <= (acorn_Integer)len, 3,
                   "position out of bounds");
  if (n == 0) {
    /* find beginning of current byte sequence */
    while (posi > 0 && iscont(s + posi)) posi--;
  }
  else {
    if (iscont(s + posi))
      return acornL_error(L, "initial position is a continuation byte");
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
       while (n > 0 && posi < (acorn_Integer)len) {
         do {  /* find beginning of next character */
           posi++;
         } while (iscont(s + posi));  /* (cannot pass final '\0') */
         n--;
       }
     }
  }
  if (n == 0)  /* did it find given character? */
    acorn_pushinteger(L, posi + 1);
  else  /* no such character */
    acornL_pushfail(L);
  return 1;
}


static int iter_aux (acorn_State *L, int strict) {
  size_t len;
  const char *s = acornL_checklstring(L, 1, &len);
  acorn_Unsigned n = (acorn_Unsigned)acorn_tointeger(L, 2);
  if (n < len) {
    while (iscont(s + n)) n++;  /* skip continuation bytes */
  }
  if (n >= len)  /* (also handles original 'n' being negative) */
    return 0;  /* no more codepoints */
  else {
    utfint code;
    const char *next = utf8_decode(s + n, &code, strict);
    if (next == NULL)
      return acornL_error(L, "invalid UTF-8 code");
    acorn_pushinteger(L, n + 1);
    acorn_pushinteger(L, code);
    return 2;
  }
}


static int iter_auxstrict (acorn_State *L) {
  return iter_aux(L, 1);
}

static int iter_auxlax (acorn_State *L) {
  return iter_aux(L, 0);
}


static int iter_codes (acorn_State *L) {
  int lax = acorn_toboolean(L, 2);
  acornL_checkstring(L, 1);
  acorn_pushcfunction(L, lax ? iter_auxlax : iter_auxstrict);
  acorn_pushvalue(L, 1);
  acorn_pushinteger(L, 0);
  return 3;
}


/* pattern to match a single UTF-8 character */
#define UTF8PATT	"[\0-\x7F\xC2-\xFD][\x80-\xBF]*"


static const acornL_Reg funcs[] = {
  {"offset", byteoffset},
  {"codepoint", codepoint},
  {"char", utfchar},
  {"len", utflen},
  {"codes", iter_codes},
  /* placeholders */
  {"charpattern", NULL},
  {NULL, NULL}
};


ACORNMOD_API int acornopen_utf8 (acorn_State *L) {
  acornL_newlib(L, funcs);
  acorn_pushlstring(L, UTF8PATT, sizeof(UTF8PATT)/sizeof(char) - 1);
  acorn_setfield(L, -2, "charpattern");
  return 1;
}

