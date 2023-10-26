/*
** $Id: object.c $
** Some generic functions over Viper objects
** See Copyright Notice in viper.h
*/

#define object_c
#define VIPER_CORE

#include "prefix.h"


#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "viper.h"

#include "ctype.h"
#include "debug.h"
#include "do.h"
#include "memory.h"
#include "object.h"
#include "state.h"
#include "string.h"
#include "virtualMachine.h"


/*
** Computes ceil(log2(x))
*/
int viperO_ceillog2 (unsigned int x) {
  static const lu_byte log_2[256] = {  /* log_2[i] = ceil(log2(i - 1)) */
    0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
  };
  int l = 0;
  x--;
  while (x >= 256) { l += 8; x >>= 8; }
  return l + log_2[x];
}


static viper_Integer intarith (viper_State *L, int op, viper_Integer v1,
                                                   viper_Integer v2) {
  switch (op) {
    case VIPER_OPADD: return intop(+, v1, v2);
    case VIPER_OPSUB:return intop(-, v1, v2);
    case VIPER_OPMUL:return intop(*, v1, v2);
    case VIPER_OPMOD: return viperV_mod(L, v1, v2);
    case VIPER_OPIDIV: return viperV_idiv(L, v1, v2);
    case VIPER_OPBAND: return intop(&, v1, v2);
    case VIPER_OPBOR: return intop(|, v1, v2);
    case VIPER_OPBXOR: return intop(^, v1, v2);
    case VIPER_OPSHL: return viperV_shiftl(v1, v2);
    case VIPER_OPSHR: return viperV_shiftl(v1, -v2);
    case VIPER_OPUNM: return intop(-, 0, v1);
    case VIPER_OPBNOT: return intop(^, ~l_castS2U(0), v1);
    default: viper_assert(0); return 0;
  }
}


static viper_Number numarith (viper_State *L, int op, viper_Number v1,
                                                  viper_Number v2) {
  switch (op) {
    case VIPER_OPADD: return viperi_numadd(L, v1, v2);
    case VIPER_OPSUB: return viperi_numsub(L, v1, v2);
    case VIPER_OPMUL: return viperi_nummul(L, v1, v2);
    case VIPER_OPDIV: return viperi_numdiv(L, v1, v2);
    case VIPER_OPPOW: return viperi_numpow(L, v1, v2);
    case VIPER_OPIDIV: return viperi_numidiv(L, v1, v2);
    case VIPER_OPUNM: return viperi_numunm(L, v1);
    case VIPER_OPMOD: return viperV_modf(L, v1, v2);
    default: viper_assert(0); return 0;
  }
}


int viperO_rawarith (viper_State *L, int op, const TValue *p1, const TValue *p2,
                   TValue *res) {
  switch (op) {
    case VIPER_OPBAND: case VIPER_OPBOR: case VIPER_OPBXOR:
    case VIPER_OPSHL: case VIPER_OPSHR:
    case VIPER_OPBNOT: {  /* operate only on integers */
      viper_Integer i1; viper_Integer i2;
      if (tointegerns(p1, &i1) && tointegerns(p2, &i2)) {
        setivalue(res, intarith(L, op, i1, i2));
        return 1;
      }
      else return 0;  /* fail */
    }
    case VIPER_OPDIV: case VIPER_OPPOW: {  /* operate only on floats */
      viper_Number n1; viper_Number n2;
      if (tonumberns(p1, n1) && tonumberns(p2, n2)) {
        setfltvalue(res, numarith(L, op, n1, n2));
        return 1;
      }
      else return 0;  /* fail */
    }
    default: {  /* other operations */
      viper_Number n1; viper_Number n2;
      if (ttisinteger(p1) && ttisinteger(p2)) {
        setivalue(res, intarith(L, op, ivalue(p1), ivalue(p2)));
        return 1;
      }
      else if (tonumberns(p1, n1) && tonumberns(p2, n2)) {
        setfltvalue(res, numarith(L, op, n1, n2));
        return 1;
      }
      else return 0;  /* fail */
    }
  }
}


void viperO_arith (viper_State *L, int op, const TValue *p1, const TValue *p2,
                 StkId res) {
  if (!viperO_rawarith(L, op, p1, p2, s2v(res))) {
    /* could not perform raw operation; try metamethod */
    viperT_trybinTM(L, p1, p2, res, cast(TMS, (op - VIPER_OPADD) + TM_ADD));
  }
}


int viperO_hexavalue (int c) {
  if (lisdigit(c)) return c - '0';
  else return (ltolower(c) - 'a') + 10;
}


static int isneg (const char **s) {
  if (**s == '-') { (*s)++; return 1; }
  else if (**s == '+') (*s)++;
  return 0;
}



/*
** {==================================================================
** Viper's implementation for 'viper_strx2number'
** ===================================================================
*/

#if !defined(viper_strx2number)

/* maximum number of significant digits to read (to avoid overflows
   even with single floats) */
#define MAXSIGDIG	30

/*
** convert a hexadecimal numeric string to a number, following
** C99 specification for 'strtod'
*/
static viper_Number viper_strx2number (const char *s, char **endptr) {
  int dot = viper_getlocaledecpoint();
  viper_Number r = l_mathop(0.0);  /* result (accumulator) */
  int sigdig = 0;  /* number of significant digits */
  int nosigdig = 0;  /* number of non-significant digits */
  int e = 0;  /* exponent correction */
  int neg;  /* 1 if number is negative */
  int hasdot = 0;  /* true after seen a dot */
  *endptr = cast_charp(s);  /* nothing is valid yet */
  while (lisspace(cast_uchar(*s))) s++;  /* skip initial spaces */
  neg = isneg(&s);  /* check sign */
  if (!(*s == '0' && (*(s + 1) == 'x' || *(s + 1) == 'X')))  /* check '0x' */
    return l_mathop(0.0);  /* invalid format (no '0x') */
  for (s += 2; ; s++) {  /* skip '0x' and read numeral */
    if (*s == dot) {
      if (hasdot) break;  /* second dot? stop loop */
      else hasdot = 1;
    }
    else if (lisxdigit(cast_uchar(*s))) {
      if (sigdig == 0 && *s == '0')  /* non-significant digit (zero)? */
        nosigdig++;
      else if (++sigdig <= MAXSIGDIG)  /* can read it without overflow? */
          r = (r * l_mathop(16.0)) + viperO_hexavalue(*s);
      else e++; /* too many digits; ignore, but still count for exponent */
      if (hasdot) e--;  /* decimal digit? correct exponent */
    }
    else break;  /* neither a dot nor a digit */
  }
  if (nosigdig + sigdig == 0)  /* no digits? */
    return l_mathop(0.0);  /* invalid format */
  *endptr = cast_charp(s);  /* valid up to here */
  e *= 4;  /* each digit multiplies/divides value by 2^4 */
  if (*s == 'p' || *s == 'P') {  /* exponent part? */
    int exp1 = 0;  /* exponent value */
    int neg1;  /* exponent sign */
    s++;  /* skip 'p' */
    neg1 = isneg(&s);  /* sign */
    if (!lisdigit(cast_uchar(*s)))
      return l_mathop(0.0);  /* invalid; must have at least one digit */
    while (lisdigit(cast_uchar(*s)))  /* read exponent */
      exp1 = exp1 * 10 + *(s++) - '0';
    if (neg1) exp1 = -exp1;
    e += exp1;
    *endptr = cast_charp(s);  /* valid up to here */
  }
  if (neg) r = -r;
  return l_mathop(ldexp)(r, e);
}

#endif
/* }====================================================== */


/* maximum length of a numeral to be converted to a number */
#if !defined (L_MAXLENNUM)
#define L_MAXLENNUM	200
#endif

/*
** Convert string 's' to a Viper number (put in 'result'). Return NULL on
** fail or the address of the ending '\0' on success. ('mode' == 'x')
** means a hexadecimal numeral.
*/
static const char *l_str2dloc (const char *s, viper_Number *result, int mode) {
  char *endptr;
  *result = (mode == 'x') ? viper_strx2number(s, &endptr)  /* try to convert */
                          : viper_str2number(s, &endptr);
  if (endptr == s) return NULL;  /* nothing recognized? */
  while (lisspace(cast_uchar(*endptr))) endptr++;  /* skip trailing spaces */
  return (*endptr == '\0') ? endptr : NULL;  /* OK iff no trailing chars */
}


/*
** Convert string 's' to a Viper number (put in 'result') handling the
** current locale.
** This function accepts both the current locale or a dot as the radix
** mark. If the conversion fails, it may mean number has a dot but
** locale accepts something else. In that case, the code copies 's'
** to a buffer (because 's' is read-only), changes the dot to the
** current locale radix mark, and tries to convert again.
** The variable 'mode' checks for special characters in the string:
** - 'n' means 'inf' or 'nan' (which should be rejected)
** - 'x' means a hexadecimal numeral
** - '.' just optimizes the search for the common case (no special chars)
*/
static const char *l_str2d (const char *s, viper_Number *result) {
  const char *endptr;
  const char *pmode = strpbrk(s, ".xXnN");  /* look for special chars */
  int mode = pmode ? ltolower(cast_uchar(*pmode)) : 0;
  if (mode == 'n')  /* reject 'inf' and 'nan' */
    return NULL;
  endptr = l_str2dloc(s, result, mode);  /* try to convert */
  if (endptr == NULL) {  /* failed? may be a different locale */
    char buff[L_MAXLENNUM + 1];
    const char *pdot = strchr(s, '.');
    if (pdot == NULL || strlen(s) > L_MAXLENNUM)
      return NULL;  /* string too long or no dot; fail */
    strcpy(buff, s);  /* copy string to buffer */
    buff[pdot - s] = viper_getlocaledecpoint();  /* correct decimal point */
    endptr = l_str2dloc(buff, result, mode);  /* try again */
    if (endptr != NULL)
      endptr = s + (endptr - buff);  /* make relative to 's' */
  }
  return endptr;
}


#define MAXBY10		cast(viper_Unsigned, VIPER_MAXINTEGER / 10)
#define MAXLASTD	cast_int(VIPER_MAXINTEGER % 10)

static const char *l_str2int (const char *s, viper_Integer *result) {
  viper_Unsigned a = 0;
  int empty = 1;
  int neg;
  while (lisspace(cast_uchar(*s))) s++;  /* skip initial spaces */
  neg = isneg(&s);
  if (s[0] == '0' &&
      (s[1] == 'x' || s[1] == 'X')) {  /* hex? */
    s += 2;  /* skip '0x' */
    for (; lisxdigit(cast_uchar(*s)); s++) {
      a = a * 16 + viperO_hexavalue(*s);
      empty = 0;
    }
  }
  else {  /* decimal */
    for (; lisdigit(cast_uchar(*s)); s++) {
      int d = *s - '0';
      if (a >= MAXBY10 && (a > MAXBY10 || d > MAXLASTD + neg))  /* overflow? */
        return NULL;  /* do not accept it (as integer) */
      a = a * 10 + d;
      empty = 0;
    }
  }
  while (lisspace(cast_uchar(*s))) s++;  /* skip trailing spaces */
  if (empty || *s != '\0') return NULL;  /* something wrong in the numeral */
  else {
    *result = l_castU2S((neg) ? 0u - a : a);
    return s;
  }
}


size_t viperO_str2num (const char *s, TValue *o) {
  viper_Integer i; viper_Number n;
  const char *e;
  if ((e = l_str2int(s, &i)) != NULL) {  /* try as an integer */
    setivalue(o, i);
  }
  else if ((e = l_str2d(s, &n)) != NULL) {  /* else try as a float */
    setfltvalue(o, n);
  }
  else
    return 0;  /* conversion failed */
  return (e - s) + 1;  /* success; return string size */
}


int viperO_utf8esc (char *buff, unsigned long x) {
  int n = 1;  /* number of bytes put in buffer (backwards) */
  viper_assert(x <= 0x7FFFFFFFu);
  if (x < 0x80)  /* ascii? */
    buff[UTF8BUFFSZ - 1] = cast_char(x);
  else {  /* need continuation bytes */
    unsigned int mfb = 0x3f;  /* maximum that fits in first byte */
    do {  /* add continuation bytes */
      buff[UTF8BUFFSZ - (n++)] = cast_char(0x80 | (x & 0x3f));
      x >>= 6;  /* remove added bits */
      mfb >>= 1;  /* now there is one less bit available in first byte */
    } while (x > mfb);  /* still needs continuation byte? */
    buff[UTF8BUFFSZ - n] = cast_char((~mfb << 1) | x);  /* add first byte */
  }
  return n;
}


/*
** Maximum length of the conversion of a number to a string. Must be
** enough to accommodate both VIPER_INTEGER_FMT and VIPER_NUMBER_FMT.
** (For a long long int, this is 19 digits plus a sign and a final '\0',
** adding to 21. For a long double, it can Viper to a sign, 33 digits,
** the dot, an exponent letter, an exponent sign, 5 exponent digits,
** and a final '\0', adding to 43.)
*/
#define MAXNUMBER2STR	44


/*
** Convert a number object to a string, adding it to a buffer
*/
static int tostringbuff (TValue *obj, char *buff) {
  int len;
  viper_assert(ttisnumber(obj));
  if (ttisinteger(obj))
    len = viper_integer2str(buff, MAXNUMBER2STR, ivalue(obj));
  else {
    len = viper_number2str(buff, MAXNUMBER2STR, fltvalue(obj));
    if (buff[strspn(buff, "-0123456789")] == '\0') {  /* looks like an int? */
      buff[len++] = viper_getlocaledecpoint();
      buff[len++] = '0';  /* adds '.0' to result */
    }
  }
  return len;
}


/*
** Convert a number object to a Viper string, replacing the value at 'obj'
*/
void viperO_tostring (viper_State *L, TValue *obj) {
  char buff[MAXNUMBER2STR];
  int len = tostringbuff(obj, buff);
  setsvalue(L, obj, viperS_newlstr(L, buff, len));
}




/*
** {==================================================================
** 'viperO_pushvfstring'
** ===================================================================
*/

/* size for buffer space used by 'viperO_pushvfstring' */
#define BUFVFS		200

/* buffer used by 'viperO_pushvfstring' */
typedef struct BuffFS {
  viper_State *L;
  int pushed;  /* number of string pieces already on the stack */
  int blen;  /* length of partial string in 'space' */
  char space[BUFVFS];  /* holds last part of the result */
} BuffFS;


/*
** Push given string to the stack, as part of the buffer, and
** join the partial strings in the stack into one.
*/
static void pushstr (BuffFS *buff, const char *str, size_t l) {
  viper_State *L = buff->L;
  setsvalue2s(L, L->top, viperS_newlstr(L, str, l));
  L->top++;  /* may use one extra slot */
  buff->pushed++;
  viperV_concat(L, buff->pushed);  /* join partial results into one */
  buff->pushed = 1;
}


/*
** empty the buffer space into the stack
*/
static void clearbuff (BuffFS *buff) {
  pushstr(buff, buff->space, buff->blen);  /* push buffer contents */
  buff->blen = 0;  /* space now is empty */
}


/*
** Get a space of size 'sz' in the buffer. If buffer has not enough
** space, empty it. 'sz' must fit in an empty buffer.
*/
static char *getbuff (BuffFS *buff, int sz) {
  viper_assert(buff->blen <= BUFVFS); viper_assert(sz <= BUFVFS);
  if (sz > BUFVFS - buff->blen)  /* not enough space? */
    clearbuff(buff);
  return buff->space + buff->blen;
}


#define addsize(b,sz)	((b)->blen += (sz))


/*
** Add 'str' to the buffer. If string is larger than the buffer space,
** push the string directly to the stack.
*/
static void addstr2buff (BuffFS *buff, const char *str, size_t slen) {
  if (slen <= BUFVFS) {  /* does string fit into buffer? */
    char *bf = getbuff(buff, cast_int(slen));
    memcpy(bf, str, slen);  /* add string to buffer */
    addsize(buff, cast_int(slen));
  }
  else {  /* string larger than buffer */
    clearbuff(buff);  /* string comes after buffer's content */
    pushstr(buff, str, slen);  /* push string */
  }
}


/*
** Add a number to the buffer.
*/
static void addnum2buff (BuffFS *buff, TValue *num) {
  char *numbuff = getbuff(buff, MAXNUMBER2STR);
  int len = tostringbuff(num, numbuff);  /* format number into 'numbuff' */
  addsize(buff, len);
}


/*
** this function handles only '%d', '%c', '%f', '%p', '%s', and '%%'
   conventional formats, plus Viper-specific '%I' and '%U'
*/
const char *viperO_pushvfstring (viper_State *L, const char *fmt, va_list argp) {
  BuffFS buff;  /* holds last part of the result */
  const char *e;  /* points to next '%' */
  buff.pushed = buff.blen = 0;
  buff.L = L;
  while ((e = strchr(fmt, '%')) != NULL) {
    addstr2buff(&buff, fmt, e - fmt);  /* add 'fmt' up to '%' */
    switch (*(e + 1)) {  /* conversion specifier */
      case 's': {  /* zero-terminated string */
        const char *s = va_arg(argp, char *);
        if (s == NULL) s = "(null)";
        addstr2buff(&buff, s, strlen(s));
        break;
      }
      case 'c': {  /* an 'int' as a character */
        char c = cast_uchar(va_arg(argp, int));
        addstr2buff(&buff, &c, sizeof(char));
        break;
      }
      case 'd': {  /* an 'int' */
        TValue num;
        setivalue(&num, va_arg(argp, int));
        addnum2buff(&buff, &num);
        break;
      }
      case 'I': {  /* a 'viper_Integer' */
        TValue num;
        setivalue(&num, cast(viper_Integer, va_arg(argp, l_uacInt)));
        addnum2buff(&buff, &num);
        break;
      }
      case 'f': {  /* a 'viper_Number' */
        TValue num;
        setfltvalue(&num, cast_num(va_arg(argp, l_uacNumber)));
        addnum2buff(&buff, &num);
        break;
      }
      case 'p': {  /* a pointer */
        const int sz = 3 * sizeof(void*) + 8; /* enough space for '%p' */
        char *bf = getbuff(&buff, sz);
        void *p = va_arg(argp, void *);
        int len = viper_pointer2str(bf, sz, p);
        addsize(&buff, len);
        break;
      }
      case 'U': {  /* a 'long' as a UTF-8 sequence */
        char bf[UTF8BUFFSZ];
        int len = viperO_utf8esc(bf, va_arg(argp, long));
        addstr2buff(&buff, bf + UTF8BUFFSZ - len, len);
        break;
      }
      case '%': {
        addstr2buff(&buff, "%", 1);
        break;
      }
      default: {
        viperG_runerror(L, "invalid option '%%%c' to 'viper_pushfstring'",
                         *(e + 1));
      }
    }
    fmt = e + 2;  /* skip '%' and the specifier */
  }
  addstr2buff(&buff, fmt, strlen(fmt));  /* rest of 'fmt' */
  clearbuff(&buff);  /* empty buffer into the stack */
  viper_assert(buff.pushed == 1);
  return svalue(s2v(L->top - 1));
}


const char *viperO_pushfstring (viper_State *L, const char *fmt, ...) {
  const char *msg;
  va_list argp;
  va_start(argp, fmt);
  msg = viperO_pushvfstring(L, fmt, argp);
  va_end(argp);
  return msg;
}

/* }================================================================== */


#define RETS	"..."
#define PRE	"[string \""
#define POS	"\"]"

#define addstr(a,b,l)	( memcpy(a,b,(l) * sizeof(char)), a += (l) )

void viperO_chunkid (char *out, const char *source, size_t srclen) {
  size_t bufflen = VIPER_IDSIZE;  /* free space in buffer */
  if (*source == '=') {  /* 'literal' source */
    if (srclen <= bufflen)  /* small enough? */
      memcpy(out, source + 1, srclen * sizeof(char));
    else {  /* truncate it */
      addstr(out, source + 1, bufflen - 1);
      *out = '\0';
    }
  }
  else if (*source == '@') {  /* file name */
    if (srclen <= bufflen)  /* small enough? */
      memcpy(out, source + 1, srclen * sizeof(char));
    else {  /* add '...' before rest of name */
      addstr(out, RETS, LL(RETS));
      bufflen -= LL(RETS);
      memcpy(out, source + 1 + srclen - bufflen, bufflen * sizeof(char));
    }
  }
  else {  /* string; format as [string "source"] */
    const char *nl = strchr(source, '\n');  /* find first new line (if any) */
    addstr(out, PRE, LL(PRE));  /* add prefix */
    bufflen -= LL(PRE RETS POS) + 1;  /* save space for prefix+suffix+'\0' */
    if (srclen < bufflen && nl == NULL) {  /* small one-line source? */
      addstr(out, source, srclen);  /* keep it */
    }
    else {
      if (nl != NULL) srclen = nl - source;  /* stop at first newline */
      if (srclen > bufflen) srclen = bufflen;
      addstr(out, source, srclen);
      addstr(out, RETS, LL(RETS));
    }
    memcpy(out, POS, (LL(POS) + 1) * sizeof(char));
  }
}
