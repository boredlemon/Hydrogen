/*
** $Id: lstrlib.c $
** Standard library for string operations and pattern-matching
** See Copyright Notice in viper.h
*/

#define lstrlib_c
#define VIPER_LIB

#include "lprefix.h"


#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "viper.h"

#include "lauxlib.h"
#include "viperlib.h"


/*
** maximum number of captures that a pattern can do during
** pattern-matching. This limit is arbitrary, but must fit in
** an unsigned char.
*/
#if !defined(VIPER_MAXCAPTURES)
#define VIPER_MAXCAPTURES		32
#endif


/* macro to 'unsign' a character */
#define uchar(c)	((unsigned char)(c))


/*
** Some sizes are better limited to fit in 'int', but must also fit in
** 'size_t'. (We assume that 'viper_Integer' cannot be smaller than 'int'.)
*/
#define MAX_SIZET	((size_t)(~(size_t)0))

#define MAXSIZE  \
	(sizeof(size_t) < sizeof(int) ? MAX_SIZET : (size_t)(INT_MAX))




static int str_len (viper_State *L) {
  size_t l;
  viperL_checklstring(L, 1, &l);
  viper_pushinteger(L, (viper_Integer)l);
  return 1;
}


/*
** translate a relative initial string position
** (negative means back from end): clip result to [1, inf).
** The length of any string in Viper must fit in a viper_Integer,
** so there are no overflows in the casts.
** The inverted comparison avoids a possible overflow
** computing '-pos'.
*/
static size_t posrelatI (viper_Integer pos, size_t len) {
  if (pos > 0)
    return (size_t)pos;
  else if (pos == 0)
    return 1;
  else if (pos < -(viper_Integer)len)  /* inverted comparison */
    return 1;  /* clip to 1 */
  else return len + (size_t)pos + 1;
}


/*
** Gets an optional ending string position from argument 'arg',
** with default value 'def'.
** Negative means back from end: clip result to [0, len]
*/
static size_t getendpos (viper_State *L, int arg, viper_Integer def,
                         size_t len) {
  viper_Integer pos = viperL_optinteger(L, arg, def);
  if (pos > (viper_Integer)len)
    return len;
  else if (pos >= 0)
    return (size_t)pos;
  else if (pos < -(viper_Integer)len)
    return 0;
  else return len + (size_t)pos + 1;
}


static int str_sub (viper_State *L) {
  size_t l;
  const char *s = viperL_checklstring(L, 1, &l);
  size_t start = posrelatI(viperL_checkinteger(L, 2), l);
  size_t end = getendpos(L, 3, -1, l);
  if (start <= end)
    viper_pushlstring(L, s + start - 1, (end - start) + 1);
  else viper_pushliteral(L, "");
  return 1;
}


static int str_reverse (viper_State *L) {
  size_t l, i;
  viperL_Buffer b;
  const char *s = viperL_checklstring(L, 1, &l);
  char *p = viperL_buffinitsize(L, &b, l);
  for (i = 0; i < l; i++)
    p[i] = s[l - i - 1];
  viperL_pushresultsize(&b, l);
  return 1;
}


static int str_lower (viper_State *L) {
  size_t l;
  size_t i;
  viperL_Buffer b;
  const char *s = viperL_checklstring(L, 1, &l);
  char *p = viperL_buffinitsize(L, &b, l);
  for (i=0; i<l; i++)
    p[i] = tolower(uchar(s[i]));
  viperL_pushresultsize(&b, l);
  return 1;
}


static int str_upper (viper_State *L) {
  size_t l;
  size_t i;
  viperL_Buffer b;
  const char *s = viperL_checklstring(L, 1, &l);
  char *p = viperL_buffinitsize(L, &b, l);
  for (i=0; i<l; i++)
    p[i] = toupper(uchar(s[i]));
  viperL_pushresultsize(&b, l);
  return 1;
}


static int str_rep (viper_State *L) {
  size_t l, lsep;
  const char *s = viperL_checklstring(L, 1, &l);
  viper_Integer n = viperL_checkinteger(L, 2);
  const char *sep = viperL_optlstring(L, 3, "", &lsep);
  if (n <= 0)
    viper_pushliteral(L, "");
  else if (l_unlikely(l + lsep < l || l + lsep > MAXSIZE / n))
    return viperL_error(L, "resulting string too large");
  else {
    size_t totallen = (size_t)n * l + (size_t)(n - 1) * lsep;
    viperL_Buffer b;
    char *p = viperL_buffinitsize(L, &b, totallen);
    while (n-- > 1) {  /* first n-1 copies (followed by separator) */
      memcpy(p, s, l * sizeof(char)); p += l;
      if (lsep > 0) {  /* empty 'memcpy' is not that cheap */
        memcpy(p, sep, lsep * sizeof(char));
        p += lsep;
      }
    }
    memcpy(p, s, l * sizeof(char));  /* last copy (not followed by separator) */
    viperL_pushresultsize(&b, totallen);
  }
  return 1;
}


static int str_byte (viper_State *L) {
  size_t l;
  const char *s = viperL_checklstring(L, 1, &l);
  viper_Integer pi = viperL_optinteger(L, 2, 1);
  size_t posi = posrelatI(pi, l);
  size_t pose = getendpos(L, 3, pi, l);
  int n, i;
  if (posi > pose) return 0;  /* empty interval; return no values */
  if (l_unlikely(pose - posi >= (size_t)INT_MAX))  /* arithmetic overflow? */
    return viperL_error(L, "string slice too long");
  n = (int)(pose -  posi) + 1;
  viperL_checkstack(L, n, "string slice too long");
  for (i=0; i<n; i++)
    viper_pushinteger(L, uchar(s[posi+i-1]));
  return n;
}


static int str_char (viper_State *L) {
  int n = viper_gettop(L);  /* number of arguments */
  int i;
  viperL_Buffer b;
  char *p = viperL_buffinitsize(L, &b, n);
  for (i=1; i<=n; i++) {
    viper_Unsigned c = (viper_Unsigned)viperL_checkinteger(L, i);
    viperL_argcheck(L, c <= (viper_Unsigned)UCHAR_MAX, i, "value out of range");
    p[i - 1] = uchar(c);
  }
  viperL_pushresultsize(&b, n);
  return 1;
}


/*
** Buffer to store the result of 'string.dump'. It must be initialized
** after the call to 'viper_dump', to ensure that the function is on the
** top of the stack when 'viper_dump' is called. ('viperL_buffinit' might
** push stuff.)
*/
struct str_Writer {
  int init;  /* true iff buffer has been initialized */
  viperL_Buffer B;
};


static int writer (viper_State *L, const void *b, size_t size, void *ud) {
  struct str_Writer *state = (struct str_Writer *)ud;
  if (!state->init) {
    state->init = 1;
    viperL_buffinit(L, &state->B);
  }
  viperL_addlstring(&state->B, (const char *)b, size);
  return 0;
}


static int str_dump (viper_State *L) {
  struct str_Writer state;
  int strip = viper_toboolean(L, 2);
  viperL_checktype(L, 1, VIPER_TFUNCTION);
  viper_settop(L, 1);  /* ensure function is on the top of the stack */
  state.init = 0;
  if (l_unlikely(viper_dump(L, writer, &state, strip) != 0))
    return viperL_error(L, "unable to dump given function");
  viperL_pushresult(&state.B);
  return 1;
}



/*
** {======================================================
** METAMETHODS
** =======================================================
*/

#if defined(VIPER_NOCVTS2N)	/* { */

/* no coercion from strings to numbers */

static const viperL_Reg stringmetamethods[] = {
  {"__index", NULL},  /* placeholder */
  {NULL, NULL}
};

#else		/* }{ */

static int tonum (viper_State *L, int arg) {
  if (viper_type(L, arg) == VIPER_TNUMBER) {  /* already a number? */
    viper_pushvalue(L, arg);
    return 1;
  }
  else {  /* check whether it is a numerical string */
    size_t len;
    const char *s = viper_tolstring(L, arg, &len);
    return (s != NULL && viper_stringtonumber(L, s) == len + 1);
  }
}


static void trymt (viper_State *L, const char *mtname) {
  viper_settop(L, 2);  /* back to the original arguments */
  if (l_unlikely(viper_type(L, 2) == VIPER_TSTRING ||
                 !viperL_getmetafield(L, 2, mtname)))
    viperL_error(L, "attempt to %s a '%s' with a '%s'", mtname + 2,
                  viperL_typename(L, -2), viperL_typename(L, -1));
  viper_insert(L, -3);  /* put metamethod before arguments */
  viper_call(L, 2, 1);  /* call metamethod */
}


static int arith (viper_State *L, int op, const char *mtname) {
  if (tonum(L, 1) && tonum(L, 2))
    viper_arith(L, op);  /* result will be on the top */
  else
    trymt(L, mtname);
  return 1;
}


static int arith_add (viper_State *L) {
  return arith(L, VIPER_OPADD, "__add");
}

static int arith_sub (viper_State *L) {
  return arith(L, VIPER_OPSUB, "__sub");
}

static int arith_mul (viper_State *L) {
  return arith(L, VIPER_OPMUL, "__mul");
}

static int arith_mod (viper_State *L) {
  return arith(L, VIPER_OPMOD, "__mod");
}

static int arith_pow (viper_State *L) {
  return arith(L, VIPER_OPPOW, "__pow");
}

static int arith_div (viper_State *L) {
  return arith(L, VIPER_OPDIV, "__div");
}

static int arith_idiv (viper_State *L) {
  return arith(L, VIPER_OPIDIV, "__idiv");
}

static int arith_unm (viper_State *L) {
  return arith(L, VIPER_OPUNM, "__unm");
}


static const viperL_Reg stringmetamethods[] = {
  {"__add", arith_add},
  {"__sub", arith_sub},
  {"__mul", arith_mul},
  {"__mod", arith_mod},
  {"__pow", arith_pow},
  {"__div", arith_div},
  {"__idiv", arith_idiv},
  {"__unm", arith_unm},
  {"__index", NULL},  /* placeholder */
  {NULL, NULL}
};

#endif		/* } */

/* }====================================================== */

/*
** {======================================================
** PATTERN MATCHING
** =======================================================
*/


#define CAP_UNFINISHED	(-1)
#define CAP_POSITION	(-2)


typedef struct MatchState {
  const char *src_init;  /* init of source string */
  const char *src_end;  /* end ('\0') of source string */
  const char *p_end;  /* end ('\0') of pattern */
  viper_State *L;
  int matchdepth;  /* control for recursive depth (to avoid C stack overflow) */
  unsigned char level;  /* total number of captures (finished or unfinished) */
  struct {
    const char *init;
    ptrdiff_t len;
  } capture[VIPER_MAXCAPTURES];
} MatchState;


/* recursive function */
static const char *match (MatchState *ms, const char *s, const char *p);


/* maximum recursion depth for 'match' */
#if !defined(MAXCCALLS)
#define MAXCCALLS	200
#endif


#define L_ESC		'%'
#define SPECIALS	"^$*+?.([%-"


static int check_capture (MatchState *ms, int l) {
  l -= '1';
  if (l_unlikely(l < 0 || l >= ms->level ||
                 ms->capture[l].len == CAP_UNFINISHED))
    return viperL_error(ms->L, "invalid capture index %%%d", l + 1);
  return l;
}


static int capture_to_close (MatchState *ms) {
  int level = ms->level;
  for (level--; level>=0; level--)
    if (ms->capture[level].len == CAP_UNFINISHED) return level;
  return viperL_error(ms->L, "invalid pattern capture");
}


static const char *classend (MatchState *ms, const char *p) {
  switch (*p++) {
    case L_ESC: {
      if (l_unlikely(p == ms->p_end))
        viperL_error(ms->L, "malformed pattern (ends with '%%')");
      return p+1;
    }
    case '[': {
      if (*p == '^') p++;
      do {  /* look for a ']' */
        if (l_unlikely(p == ms->p_end))
          viperL_error(ms->L, "malformed pattern (missing ']')");
        if (*(p++) == L_ESC && p < ms->p_end)
          p++;  /* skip escapes (e.g. '%]') */
      } while (*p != ']');
      return p+1;
    }
    default: {
      return p;
    }
  }
}


static int match_class (int c, int cl) {
  int res;
  switch (tolower(cl)) {
    case 'a' : res = isalpha(c); break;
    case 'c' : res = iscntrl(c); break;
    case 'd' : res = isdigit(c); break;
    case 'g' : res = isgraph(c); break;
    case 'l' : res = islower(c); break;
    case 'p' : res = ispunct(c); break;
    case 's' : res = isspace(c); break;
    case 'u' : res = isupper(c); break;
    case 'w' : res = isalnum(c); break;
    case 'x' : res = isxdigit(c); break;
    case 'z' : res = (c == 0); break;  /* deprecated option */
    default: return (cl == c);
  }
  return (islower(cl) ? res : !res);
}


static int matchbracketclass (int c, const char *p, const char *ec) {
  int sig = 1;
  if (*(p+1) == '^') {
    sig = 0;
    p++;  /* skip the '^' */
  }
  while (++p < ec) {
    if (*p == L_ESC) {
      p++;
      if (match_class(c, uchar(*p)))
        return sig;
    }
    else if ((*(p+1) == '-') && (p+2 < ec)) {
      p+=2;
      if (uchar(*(p-2)) <= c && c <= uchar(*p))
        return sig;
    }
    else if (uchar(*p) == c) return sig;
  }
  return !sig;
}


static int singlematch (MatchState *ms, const char *s, const char *p,
                        const char *ep) {
  if (s >= ms->src_end)
    return 0;
  else {
    int c = uchar(*s);
    switch (*p) {
      case '.': return 1;  /* matches any char */
      case L_ESC: return match_class(c, uchar(*(p+1)));
      case '[': return matchbracketclass(c, p, ep-1);
      default:  return (uchar(*p) == c);
    }
  }
}


static const char *matchbalance (MatchState *ms, const char *s,
                                   const char *p) {
  if (l_unlikely(p >= ms->p_end - 1))
    viperL_error(ms->L, "malformed pattern (missing arguments to '%%b')");
  if (*s != *p) return NULL;
  else {
    int b = *p;
    int e = *(p+1);
    int cont = 1;
    while (++s < ms->src_end) {
      if (*s == e) {
        if (--cont == 0) return s+1;
      }
      else if (*s == b) cont++;
    }
  }
  return NULL;  /* string ends out of balance */
}


static const char *max_expand (MatchState *ms, const char *s,
                                 const char *p, const char *ep) {
  ptrdiff_t i = 0;  /* counts maximum expand for item */
  while (singlematch(ms, s + i, p, ep))
    i++;
  /* keeps trying to match with the maximum repetitions */
  while (i>=0) {
    const char *res = match(ms, (s+i), ep+1);
    if (res) return res;
    i--;  /* else didn't match; reduce 1 repetition to try again */
  }
  return NULL;
}


static const char *min_expand (MatchState *ms, const char *s,
                                 const char *p, const char *ep) {
  for (;;) {
    const char *res = match(ms, s, ep+1);
    if (res != NULL)
      return res;
    else if (singlematch(ms, s, p, ep))
      s++;  /* try with one more repetition */
    else return NULL;
  }
}


static const char *start_capture (MatchState *ms, const char *s,
                                    const char *p, int what) {
  const char *res;
  int level = ms->level;
  if (level >= VIPER_MAXCAPTURES) viperL_error(ms->L, "too many captures");
  ms->capture[level].init = s;
  ms->capture[level].len = what;
  ms->level = level+1;
  if ((res=match(ms, s, p)) == NULL)  /* match failed? */
    ms->level--;  /* undo capture */
  return res;
}


static const char *end_capture (MatchState *ms, const char *s,
                                  const char *p) {
  int l = capture_to_close(ms);
  const char *res;
  ms->capture[l].len = s - ms->capture[l].init;  /* close capture */
  if ((res = match(ms, s, p)) == NULL)  /* match failed? */
    ms->capture[l].len = CAP_UNFINISHED;  /* undo capture */
  return res;
}


static const char *match_capture (MatchState *ms, const char *s, int l) {
  size_t len;
  l = check_capture(ms, l);
  len = ms->capture[l].len;
  if ((size_t)(ms->src_end-s) >= len &&
      memcmp(ms->capture[l].init, s, len) == 0)
    return s+len;
  else return NULL;
}


static const char *match (MatchState *ms, const char *s, const char *p) {
  if (l_unlikely(ms->matchdepth-- == 0))
    viperL_error(ms->L, "pattern too complex");
  init: /* using goto's to optimize tail recursion */
  if (p != ms->p_end) {  /* end of pattern? */
    switch (*p) {
      case '(': {  /* start capture */
        if (*(p + 1) == ')')  /* position capture? */
          s = start_capture(ms, s, p + 2, CAP_POSITION);
        else
          s = start_capture(ms, s, p + 1, CAP_UNFINISHED);
        break;
      }
      case ')': {  /* end capture */
        s = end_capture(ms, s, p + 1);
        break;
      }
      case '$': {
        if ((p + 1) != ms->p_end)  /* is the '$' the last char in pattern? */
          goto dflt;  /* no; Viper to default */
        s = (s == ms->src_end) ? s : NULL;  /* check end of string */
        break;
      }
      case L_ESC: {  /* escaped sequences not in the format class[*+?-]? */
        switch (*(p + 1)) {
          case 'b': {  /* balanced string? */
            s = matchbalance(ms, s, p + 2);
            if (s != NULL) {
              p += 4; goto init;  /* return match(ms, s, p + 4); */
            }  /* else fail (s == NULL) */
            break;
          }
          case 'f': {  /* frontier? */
            const char *ep; char previous;
            p += 2;
            if (l_unlikely(*p != '['))
              viperL_error(ms->L, "missing '[' after '%%f' in pattern");
            ep = classend(ms, p);  /* points to what is next */
            previous = (s == ms->src_init) ? '\0' : *(s - 1);
            if (!matchbracketclass(uchar(previous), p, ep - 1) &&
               matchbracketclass(uchar(*s), p, ep - 1)) {
              p = ep; goto init;  /* return match(ms, s, ep); */
            }
            s = NULL;  /* match failed */
            break;
          }
          case '0': case '1': case '2': case '3':
          case '4': case '5': case '6': case '7':
          case '8': case '9': {  /* capture results (%0-%9)? */
            s = match_capture(ms, s, uchar(*(p + 1)));
            if (s != NULL) {
              p += 2; goto init;  /* return match(ms, s, p + 2) */
            }
            break;
          }
          default: goto dflt;
        }
        break;
      }
      default: dflt: {  /* pattern class plus optional suffix */
        const char *ep = classend(ms, p);  /* points to optional suffix */
        /* does not match at least once? */
        if (!singlematch(ms, s, p, ep)) {
          if (*ep == '*' || *ep == '?' || *ep == '-') {  /* accept empty? */
            p = ep + 1; goto init;  /* return match(ms, s, ep + 1); */
          }
          else  /* '+' or no suffix */
            s = NULL;  /* fail */
        }
        else {  /* matched once */
          switch (*ep) {  /* handle optional suffix */
            case '?': {  /* optional */
              const char *res;
              if ((res = match(ms, s + 1, ep + 1)) != NULL)
                s = res;
              else {
                p = ep + 1; goto init;  /* else return match(ms, s, ep + 1); */
              }
              break;
            }
            case '+':  /* 1 or more repetitions */
              s++;  /* 1 match already done */
              /* FALLTHROUGH */
            case '*':  /* 0 or more repetitions */
              s = max_expand(ms, s, p, ep);
              break;
            case '-':  /* 0 or more repetitions (minimum) */
              s = min_expand(ms, s, p, ep);
              break;
            default:  /* no suffix */
              s++; p = ep; goto init;  /* return match(ms, s + 1, ep); */
          }
        }
        break;
      }
    }
  }
  ms->matchdepth++;
  return s;
}



static const char *lmemfind (const char *s1, size_t l1,
                               const char *s2, size_t l2) {
  if (l2 == 0) return s1;  /* empty strings are everywhere */
  else if (l2 > l1) return NULL;  /* avoids a negative 'l1' */
  else {
    const char *init;  /* to search for a '*s2' inside 's1' */
    l2--;  /* 1st char will be checked by 'memchr' */
    l1 = l1-l2;  /* 's2' cannot be found after that */
    while (l1 > 0 && (init = (const char *)memchr(s1, *s2, l1)) != NULL) {
      init++;   /* 1st char is already checked */
      if (memcmp(init, s2+1, l2) == 0)
        return init-1;
      else {  /* correct 'l1' and 's1' to try again */
        l1 -= init-s1;
        s1 = init;
      }
    }
    return NULL;  /* not found */
  }
}


/*
** get information about the i-th capture. If there are no captures
** and 'i==0', return information about the whole match, which
** is the range 's'..'e'. If the capture is a string, return
** its length and put its address in '*cap'. If it is an integer
** (a position), push it on the stack and return CAP_POSITION.
*/
static size_t get_onecapture (MatchState *ms, int i, const char *s,
                              const char *e, const char **cap) {
  if (i >= ms->level) {
    if (l_unlikely(i != 0))
      viperL_error(ms->L, "invalid capture index %%%d", i + 1);
    *cap = s;
    return e - s;
  }
  else {
    ptrdiff_t capl = ms->capture[i].len;
    *cap = ms->capture[i].init;
    if (l_unlikely(capl == CAP_UNFINISHED))
      viperL_error(ms->L, "unfinished capture");
    else if (capl == CAP_POSITION)
      viper_pushinteger(ms->L, (ms->capture[i].init - ms->src_init) + 1);
    return capl;
  }
}


/*
** Push the i-th capture on the stack.
*/
static void push_onecapture (MatchState *ms, int i, const char *s,
                                                    const char *e) {
  const char *cap;
  ptrdiff_t l = get_onecapture(ms, i, s, e, &cap);
  if (l != CAP_POSITION)
    viper_pushlstring(ms->L, cap, l);
  /* else position was already pushed */
}


static int push_captures (MatchState *ms, const char *s, const char *e) {
  int i;
  int nlevels = (ms->level == 0 && s) ? 1 : ms->level;
  viperL_checkstack(ms->L, nlevels, "too many captures");
  for (i = 0; i < nlevels; i++)
    push_onecapture(ms, i, s, e);
  return nlevels;  /* number of strings pushed */
}


/* check whether pattern has no special characters */
static int nospecials (const char *p, size_t l) {
  size_t upto = 0;
  do {
    if (strpbrk(p + upto, SPECIALS))
      return 0;  /* pattern has a special character */
    upto += strlen(p + upto) + 1;  /* may have more after \0 */
  } while (upto <= l);
  return 1;  /* no special chars found */
}


static void prepstate (MatchState *ms, viper_State *L,
                       const char *s, size_t ls, const char *p, size_t lp) {
  ms->L = L;
  ms->matchdepth = MAXCCALLS;
  ms->src_init = s;
  ms->src_end = s + ls;
  ms->p_end = p + lp;
}


static void reprepstate (MatchState *ms) {
  ms->level = 0;
  viper_assert(ms->matchdepth == MAXCCALLS);
}


static int str_find_aux (viper_State *L, int find) {
  size_t ls, lp;
  const char *s = viperL_checklstring(L, 1, &ls);
  const char *p = viperL_checklstring(L, 2, &lp);
  size_t init = posrelatI(viperL_optinteger(L, 3, 1), ls) - 1;
  if (init > ls) {  /* start after string's end? */
    viperL_pushfail(L);  /* cannot find anything */
    return 1;
  }
  /* explicit request or no special characters? */
  if (find && (viper_toboolean(L, 4) || nospecials(p, lp))) {
    /* do a plain search */
    const char *s2 = lmemfind(s + init, ls - init, p, lp);
    if (s2) {
      viper_pushinteger(L, (s2 - s) + 1);
      viper_pushinteger(L, (s2 - s) + lp);
      return 2;
    }
  }
  else {
    MatchState ms;
    const char *s1 = s + init;
    int anchor = (*p == '^');
    if (anchor) {
      p++; lp--;  /* skip anchor character */
    }
    prepstate(&ms, L, s, ls, p, lp);
    do {
      const char *res;
      reprepstate(&ms);
      if ((res=match(&ms, s1, p)) != NULL) {
        if (find) {
          viper_pushinteger(L, (s1 - s) + 1);  /* start */
          viper_pushinteger(L, res - s);   /* end */
          return push_captures(&ms, NULL, 0) + 2;
        }
        else
          return push_captures(&ms, s1, res);
      }
    } while (s1++ < ms.src_end && !anchor);
  }
  viperL_pushfail(L);  /* not found */
  return 1;
}


static int str_find (viper_State *L) {
  return str_find_aux(L, 1);
}


static int str_match (viper_State *L) {
  return str_find_aux(L, 0);
}


/* state for 'gmatch' */
typedef struct GMatchState {
  const char *src;  /* current position */
  const char *p;  /* pattern */
  const char *lastmatch;  /* end of last match */
  MatchState ms;  /* match state */
} GMatchState;


static int gmatch_aux (viper_State *L) {
  GMatchState *gm = (GMatchState *)viper_touserdata(L, viper_upvalueindex(3));
  const char *src;
  gm->ms.L = L;
  for (src = gm->src; src <= gm->ms.src_end; src++) {
    const char *e;
    reprepstate(&gm->ms);
    if ((e = match(&gm->ms, src, gm->p)) != NULL && e != gm->lastmatch) {
      gm->src = gm->lastmatch = e;
      return push_captures(&gm->ms, src, e);
    }
  }
  return 0;  /* not found */
}


static int gmatch (viper_State *L) {
  size_t ls, lp;
  const char *s = viperL_checklstring(L, 1, &ls);
  const char *p = viperL_checklstring(L, 2, &lp);
  size_t init = posrelatI(viperL_optinteger(L, 3, 1), ls) - 1;
  GMatchState *gm;
  viper_settop(L, 2);  /* keep strings on closure to avoid being collected */
  gm = (GMatchState *)viper_newuserdatauv(L, sizeof(GMatchState), 0);
  if (init > ls)  /* start after string's end? */
    init = ls + 1;  /* avoid overflows in 's + init' */
  prepstate(&gm->ms, L, s, ls, p, lp);
  gm->src = s + init; gm->p = p; gm->lastmatch = NULL;
  viper_pushcclosure(L, gmatch_aux, 3);
  return 1;
}


static void add_s (MatchState *ms, viperL_Buffer *b, const char *s,
                                                   const char *e) {
  size_t l;
  viper_State *L = ms->L;
  const char *news = viper_tolstring(L, 3, &l);
  const char *p;
  while ((p = (char *)memchr(news, L_ESC, l)) != NULL) {
    viperL_addlstring(b, news, p - news);
    p++;  /* skip ESC */
    if (*p == L_ESC)  /* '%%' */
      viperL_addchar(b, *p);
    else if (*p == '0')  /* '%0' */
        viperL_addlstring(b, s, e - s);
    else if (isdigit(uchar(*p))) {  /* '%n' */
      const char *cap;
      ptrdiff_t resl = get_onecapture(ms, *p - '1', s, e, &cap);
      if (resl == CAP_POSITION)
        viperL_addvalue(b);  /* add position to accumulated result */
      else
        viperL_addlstring(b, cap, resl);
    }
    else
      viperL_error(L, "invalid use of '%c' in replacement string", L_ESC);
    l -= p + 1 - news;
    news = p + 1;
  }
  viperL_addlstring(b, news, l);
}


/*
** Add the replacement value to the string buffer 'b'.
** Return true if the original string was changed. (Function calls and
** table indexing resulting in nil or false do not change the subject.)
*/
static int add_value (MatchState *ms, viperL_Buffer *b, const char *s,
                                      const char *e, int tr) {
  viper_State *L = ms->L;
  switch (tr) {
    case VIPER_TFUNCTION: {  /* call the function */
      int n;
      viper_pushvalue(L, 3);  /* push the function */
      n = push_captures(ms, s, e);  /* all captures as arguments */
      viper_call(L, n, 1);  /* call it */
      break;
    }
    case VIPER_TTABLE: {  /* index the table */
      push_onecapture(ms, 0, s, e);  /* first capture is the index */
      viper_gettable(L, 3);
      break;
    }
    default: {  /* VIPER_TNUMBER or VIPER_TSTRING */
      add_s(ms, b, s, e);  /* add value to the buffer */
      return 1;  /* something changed */
    }
  }
  if (!viper_toboolean(L, -1)) {  /* nil or false? */
    viper_pop(L, 1);  /* remove value */
    viperL_addlstring(b, s, e - s);  /* keep original text */
    return 0;  /* no changes */
  }
  else if (l_unlikely(!viper_isstring(L, -1)))
    return viperL_error(L, "invalid replacement value (a %s)",
                         viperL_typename(L, -1));
  else {
    viperL_addvalue(b);  /* add result to accumulator */
    return 1;  /* something changed */
  }
}


static int str_gsub (viper_State *L) {
  size_t srcl, lp;
  const char *src = viperL_checklstring(L, 1, &srcl);  /* subject */
  const char *p = viperL_checklstring(L, 2, &lp);  /* pattern */
  const char *lastmatch = NULL;  /* end of last match */
  int tr = viper_type(L, 3);  /* replacement type */
  viper_Integer max_s = viperL_optinteger(L, 4, srcl + 1);  /* max replacements */
  int anchor = (*p == '^');
  viper_Integer n = 0;  /* replacement count */
  int changed = 0;  /* change flag */
  MatchState ms;
  viperL_Buffer b;
  viperL_argexpected(L, tr == VIPER_TNUMBER || tr == VIPER_TSTRING ||
                   tr == VIPER_TFUNCTION || tr == VIPER_TTABLE, 3,
                      "string/function/table");
  viperL_buffinit(L, &b);
  if (anchor) {
    p++; lp--;  /* skip anchor character */
  }
  prepstate(&ms, L, src, srcl, p, lp);
  while (n < max_s) {
    const char *e;
    reprepstate(&ms);  /* (re)prepare state for new match */
    if ((e = match(&ms, src, p)) != NULL && e != lastmatch) {  /* match? */
      n++;
      changed = add_value(&ms, &b, src, e, tr) | changed;
      src = lastmatch = e;
    }
    else if (src < ms.src_end)  /* otherwise, skip one character */
      viperL_addchar(&b, *src++);
    else break;  /* end of subject */
    if (anchor) break;
  }
  if (!changed)  /* no changes? */
    viper_pushvalue(L, 1);  /* return original string */
  else {  /* something changed */
    viperL_addlstring(&b, src, ms.src_end-src);
    viperL_pushresult(&b);  /* create and return new string */
  }
  viper_pushinteger(L, n);  /* number of substitutions */
  return 2;
}

/* }====================================================== */



/*
** {======================================================
** STRING FORMAT
** =======================================================
*/

#if !defined(viper_number2strx)	/* { */

/*
** Hexadecimal floating-point formatter
*/

#define SIZELENMOD	(sizeof(VIPER_NUMBER_FRMLEN)/sizeof(char))


/*
** Number of bits that Viperes into the first digit. It can be any value
** between 1 and 4; the following definition tries to align the number
** to nibble boundaries by making what is left after that first digit a
** multiple of 4.
*/
#define L_NBFD		((l_floatatt(MANT_DIG) - 1)%4 + 1)


/*
** Add integer part of 'x' to buffer and return new 'x'
*/
static viper_Number adddigit (char *buff, int n, viper_Number x) {
  viper_Number dd = l_mathop(floor)(x);  /* get integer part from 'x' */
  int d = (int)dd;
  buff[n] = (d < 10 ? d + '0' : d - 10 + 'a');  /* add to buffer */
  return x - dd;  /* return what is left */
}


static int num2straux (char *buff, int sz, viper_Number x) {
  /* if 'inf' or 'NaN', format it like '%g' */
  if (x != x || x == (viper_Number)HUGE_VAL || x == -(viper_Number)HUGE_VAL)
    return l_sprintf(buff, sz, VIPER_NUMBER_FMT, (VIPERI_UACNUMBER)x);
  else if (x == 0) {  /* can be -0... */
    /* create "0" or "-0" followed by exponent */
    return l_sprintf(buff, sz, VIPER_NUMBER_FMT "x0p+0", (VIPERI_UACNUMBER)x);
  }
  else {
    int e;
    viper_Number m = l_mathop(frexp)(x, &e);  /* 'x' fraction and exponent */
    int n = 0;  /* character count */
    if (m < 0) {  /* is number negative? */
      buff[n++] = '-';  /* add sign */
      m = -m;  /* make it positive */
    }
    buff[n++] = '0'; buff[n++] = 'x';  /* add "0x" */
    m = adddigit(buff, n++, m * (1 << L_NBFD));  /* add first digit */
    e -= L_NBFD;  /* this digit Viperes before the radix point */
    if (m > 0) {  /* more digits? */
      buff[n++] = viper_getlocaledecpoint();  /* add radix point */
      do {  /* add as many digits as needed */
        m = adddigit(buff, n++, m * 16);
      } while (m > 0);
    }
    n += l_sprintf(buff + n, sz - n, "p%+d", e);  /* add exponent */
    viper_assert(n < sz);
    return n;
  }
}


static int viper_number2strx (viper_State *L, char *buff, int sz,
                            const char *fmt, viper_Number x) {
  int n = num2straux(buff, sz, x);
  if (fmt[SIZELENMOD] == 'A') {
    int i;
    for (i = 0; i < n; i++)
      buff[i] = toupper(uchar(buff[i]));
  }
  else if (l_unlikely(fmt[SIZELENMOD] != 'a'))
    return viperL_error(L, "modifiers for format '%%a'/'%%A' not implemented");
  return n;
}

#endif				/* } */


/*
** Maximum size for items formatted with '%f'. This size is produced
** by format('%.99f', -maxfloat), and is equal to 99 + 3 ('-', '.',
** and '\0') + number of decimal digits to represent maxfloat (which
** is maximum exponent + 1). (99+3+1, adding some extra, 110)
*/
#define MAX_ITEMF	(110 + l_floatatt(MAX_10_EXP))


/*
** All formats except '%f' do not need that large limit.  The other
** float formats use exponents, so that they fit in the 99 limit for
** significant digits; 's' for large strings and 'q' add items directly
** to the buffer; all integer formats also fit in the 99 limit.  The
** worst case are floats: they may need 99 significant digits, plus
** '0x', '-', '.', 'e+XXXX', and '\0'. Adding some extra, 120.
*/
#define MAX_ITEM	120


/* valid flags in a format specification */
#if !defined(L_FMTFLAGSF)

/* valid flags for a, A, e, E, f, F, g, and G conversions */
#define L_FMTFLAGSF	"-+#0 "

/* valid flags for o, x, and X conversions */
#define L_FMTFLAGSX	"-#0"

/* valid flags for d and i conversions */
#define L_FMTFLAGSI	"-+0 "

/* valid flags for u conversions */
#define L_FMTFLAGSU	"-0"

/* valid flags for c, p, and s conversions */
#define L_FMTFLAGSC	"-"

#endif


/*
** Maximum size of each format specification (such as "%-099.99d"):
** Initial '%', flags (up to 5), width (2), period, precision (2),
** length modifier (8), conversion specifier, and final '\0', plus some
** extra.
*/
#define MAX_FORMAT	32


static void addquoted (viperL_Buffer *b, const char *s, size_t len) {
  viperL_addchar(b, '"');
  while (len--) {
    if (*s == '"' || *s == '\\' || *s == '\n') {
      viperL_addchar(b, '\\');
      viperL_addchar(b, *s);
    }
    else if (iscntrl(uchar(*s))) {
      char buff[10];
      if (!isdigit(uchar(*(s+1))))
        l_sprintf(buff, sizeof(buff), "\\%d", (int)uchar(*s));
      else
        l_sprintf(buff, sizeof(buff), "\\%03d", (int)uchar(*s));
      viperL_addstring(b, buff);
    }
    else
      viperL_addchar(b, *s);
    s++;
  }
  viperL_addchar(b, '"');
}


/*
** Serialize a floating-point number in such a way that it can be
** scanned back by Viper. Use hexadecimal format for "common" numbers
** (to preserve precision); inf, -inf, and NaN are handled separately.
** (NaN cannot be expressed as a numeral, so we write '(0/0)' for it.)
*/
static int quotefloat (viper_State *L, char *buff, viper_Number n) {
  const char *s;  /* for the fixed representations */
  if (n == (viper_Number)HUGE_VAL)  /* inf? */
    s = "1e9999";
  else if (n == -(viper_Number)HUGE_VAL)  /* -inf? */
    s = "-1e9999";
  else if (n != n)  /* NaN? */
    s = "(0/0)";
  else {  /* format number as hexadecimal */
    int  nb = viper_number2strx(L, buff, MAX_ITEM,
                                 "%" VIPER_NUMBER_FRMLEN "a", n);
    /* ensures that 'buff' string uses a dot as the radix character */
    if (memchr(buff, '.', nb) == NULL) {  /* no dot? */
      char point = viper_getlocaledecpoint();  /* try locale point */
      char *ppoint = (char *)memchr(buff, point, nb);
      if (ppoint) *ppoint = '.';  /* change it to a dot */
    }
    return nb;
  }
  /* for the fixed representations */
  return l_sprintf(buff, MAX_ITEM, "%s", s);
}


static void addliteral (viper_State *L, viperL_Buffer *b, int arg) {
  switch (viper_type(L, arg)) {
    case VIPER_TSTRING: {
      size_t len;
      const char *s = viper_tolstring(L, arg, &len);
      addquoted(b, s, len);
      break;
    }
    case VIPER_TNUMBER: {
      char *buff = viperL_prepbuffsize(b, MAX_ITEM);
      int nb;
      if (!viper_isinteger(L, arg))  /* float? */
        nb = quotefloat(L, buff, viper_tonumber(L, arg));
      else {  /* integers */
        viper_Integer n = viper_tointeger(L, arg);
        const char *format = (n == VIPER_MININTEGER)  /* corner case? */
                           ? "0x%" VIPER_INTEGER_FRMLEN "x"  /* use hex */
                           : VIPER_INTEGER_FMT;  /* else use default format */
        nb = l_sprintf(buff, MAX_ITEM, format, (VIPERI_UACINT)n);
      }
      viperL_addsize(b, nb);
      break;
    }
    case VIPER_TNIL: case VIPER_TBOOLEAN: {
      viperL_tolstring(L, arg, NULL);
      viperL_addvalue(b);
      break;
    }
    default: {
      viperL_argerror(L, arg, "value has no literal form");
    }
  }
}


static const char *get2digits (const char *s) {
  if (isdigit(uchar(*s))) {
    s++;
    if (isdigit(uchar(*s))) s++;  /* (2 digits at most) */
  }
  return s;
}


/*
** Check whether a conversion specification is valid. When called,
** first character in 'form' must be '%' and last character must
** be a valid conversion specifier. 'flags' are the accepted flags;
** 'precision' signals whether to accept a precision.
*/
static void checkformat (viper_State *L, const char *form, const char *flags,
                                       int precision) {
  const char *spec = form + 1;  /* skip '%' */
  spec += strspn(spec, flags);  /* skip flags */
  if (*spec != '0') {  /* a width cannot start with '0' */
    spec = get2digits(spec);  /* skip width */
    if (*spec == '.' && precision) {
      spec++;
      spec = get2digits(spec);  /* skip precision */
    }
  }
  if (!isalpha(uchar(*spec)))  /* did not Viper to the end? */
    viperL_error(L, "invalid conversion specification: '%s'", form);
}


/*
** Get a conversion specification and copy it to 'form'.
** Return the address of its last character.
*/
static const char *getformat (viper_State *L, const char *strfrmt,
                                            char *form) {
  /* spans flags, width, and precision ('0' is included as a flag) */
  size_t len = strspn(strfrmt, L_FMTFLAGSF "123456789.");
  len++;  /* adds following character (should be the specifier) */
  /* still needs space for '%', '\0', plus a length modifier */
  if (len >= MAX_FORMAT - 10)
    viperL_error(L, "invalid format (too long)");
  *(form++) = '%';
  memcpy(form, strfrmt, len * sizeof(char));
  *(form + len) = '\0';
  return strfrmt + len - 1;
}


/*
** add length modifier into formats
*/
static void addlenmod (char *form, const char *lenmod) {
  size_t l = strlen(form);
  size_t lm = strlen(lenmod);
  char spec = form[l - 1];
  strcpy(form + l - 1, lenmod);
  form[l + lm - 1] = spec;
  form[l + lm] = '\0';
}


static int str_format (viper_State *L) {
  int top = viper_gettop(L);
  int arg = 1;
  size_t sfl;
  const char *strfrmt = viperL_checklstring(L, arg, &sfl);
  const char *strfrmt_end = strfrmt+sfl;
  const char *flags;
  viperL_Buffer b;
  viperL_buffinit(L, &b);
  while (strfrmt < strfrmt_end) {
    if (*strfrmt != L_ESC)
      viperL_addchar(&b, *strfrmt++);
    else if (*++strfrmt == L_ESC)
      viperL_addchar(&b, *strfrmt++);  /* %% */
    else { /* format item */
      char form[MAX_FORMAT];  /* to store the format ('%...') */
      int maxitem = MAX_ITEM;  /* maximum length for the result */
      char *buff = viperL_prepbuffsize(&b, maxitem);  /* to put result */
      int nb = 0;  /* number of bytes in result */
      if (++arg > top)
        return viperL_argerror(L, arg, "no value");
      strfrmt = getformat(L, strfrmt, form);
      switch (*strfrmt++) {
        case 'c': {
          checkformat(L, form, L_FMTFLAGSC, 0);
          nb = l_sprintf(buff, maxitem, form, (int)viperL_checkinteger(L, arg));
          break;
        }
        case 'd': case 'i':
          flags = L_FMTFLAGSI;
          goto intcase;
        case 'u':
          flags = L_FMTFLAGSU;
          goto intcase;
        case 'o': case 'x': case 'X':
          flags = L_FMTFLAGSX;
         intcase: {
          viper_Integer n = viperL_checkinteger(L, arg);
          checkformat(L, form, flags, 1);
          addlenmod(form, VIPER_INTEGER_FRMLEN);
          nb = l_sprintf(buff, maxitem, form, (VIPERI_UACINT)n);
          break;
        }
        case 'a': case 'A':
          checkformat(L, form, L_FMTFLAGSF, 1);
          addlenmod(form, VIPER_NUMBER_FRMLEN);
          nb = viper_number2strx(L, buff, maxitem, form,
                                  viperL_checknumber(L, arg));
          break;
        case 'f':
          maxitem = MAX_ITEMF;  /* extra space for '%f' */
          buff = viperL_prepbuffsize(&b, maxitem);
          /* FALLTHROUGH */
        case 'e': case 'E': case 'g': case 'G': {
          viper_Number n = viperL_checknumber(L, arg);
          checkformat(L, form, L_FMTFLAGSF, 1);
          addlenmod(form, VIPER_NUMBER_FRMLEN);
          nb = l_sprintf(buff, maxitem, form, (VIPERI_UACNUMBER)n);
          break;
        }
        case 'p': {
          const void *p = viper_topointer(L, arg);
          checkformat(L, form, L_FMTFLAGSC, 0);
          if (p == NULL) {  /* avoid calling 'printf' with argument NULL */
            p = "(null)";  /* result */
            form[strlen(form) - 1] = 's';  /* format it as a string */
          }
          nb = l_sprintf(buff, maxitem, form, p);
          break;
        }
        case 'q': {
          if (form[2] != '\0')  /* modifiers? */
            return viperL_error(L, "specifier '%%q' cannot have modifiers");
          addliteral(L, &b, arg);
          break;
        }
        case 's': {
          size_t l;
          const char *s = viperL_tolstring(L, arg, &l);
          if (form[2] == '\0')  /* no modifiers? */
            viperL_addvalue(&b);  /* keep entire string */
          else {
            viperL_argcheck(L, l == strlen(s), arg, "string contains zeros");
            checkformat(L, form, L_FMTFLAGSC, 1);
            if (strchr(form, '.') == NULL && l >= 100) {
              /* no precision and string is too long to be formatted */
              viperL_addvalue(&b);  /* keep entire string */
            }
            else {  /* format the string into 'buff' */
              nb = l_sprintf(buff, maxitem, form, s);
              viper_pop(L, 1);  /* remove result from 'viperL_tolstring' */
            }
          }
          break;
        }
        default: {  /* also treat cases 'pnLlh' */
          return viperL_error(L, "invalid conversion '%s' to 'format'", form);
        }
      }
      viper_assert(nb < maxitem);
      viperL_addsize(&b, nb);
    }
  }
  viperL_pushresult(&b);
  return 1;
}

/* }====================================================== */


/*
** {======================================================
** PACK/UNPACK
** =======================================================
*/


/* value used for padding */
#if !defined(VIPERL_PACKPADBYTE)
#define VIPERL_PACKPADBYTE		0x00
#endif

/* maximum size for the binary representation of an integer */
#define MAXINTSIZE	16

/* number of bits in a character */
#define NB	CHAR_BIT

/* mask for one character (NB 1's) */
#define MC	((1 << NB) - 1)

/* size of a viper_Integer */
#define SZINT	((int)sizeof(viper_Integer))


/* dummy union to get native endianness */
static const union {
  int dummy;
  char little;  /* true iff machine is little endian */
} nativeendian = {1};


/*
** information to pack/unpack stuff
*/
typedef struct Header {
  viper_State *L;
  int islittle;
  int maxalign;
} Header;


/*
** options for pack/unpack
*/
typedef enum KOption {
  Kint,		/* signed integers */
  Kuint,	/* unsigned integers */
  Kfloat,	/* single-precision floating-point numbers */
  Knumber,	/* Viper "native" floating-point numbers */
  Kdouble,	/* double-precision floating-point numbers */
  Kchar,	/* fixed-length strings */
  Kstring,	/* strings with prefixed length */
  Kzstr,	/* zero-terminated strings */
  Kpadding,	/* padding */
  Kpaddalign,	/* padding for alignment */
  Knop		/* no-op (configuration or spaces) */
} KOption;


/*
** Read an integer numeral from string 'fmt' or return 'df' if
** there is no numeral
*/
static int digit (int c) { return '0' <= c && c <= '9'; }

static int getnum (const char **fmt, int df) {
  if (!digit(**fmt))  /* no number? */
    return df;  /* return default value */
  else {
    int a = 0;
    do {
      a = a*10 + (*((*fmt)++) - '0');
    } while (digit(**fmt) && a <= ((int)MAXSIZE - 9)/10);
    return a;
  }
}


/*
** Read an integer numeral and raises an error if it is larger
** than the maximum size for integers.
*/
static int getnumlimit (Header *h, const char **fmt, int df) {
  int sz = getnum(fmt, df);
  if (l_unlikely(sz > MAXINTSIZE || sz <= 0))
    return viperL_error(h->L, "integral size (%d) out of limits [1,%d]",
                            sz, MAXINTSIZE);
  return sz;
}


/*
** Initialize Header
*/
static void initheader (viper_State *L, Header *h) {
  h->L = L;
  h->islittle = nativeendian.little;
  h->maxalign = 1;
}


/*
** Read and classify next option. 'size' is filled with option's size.
*/
static KOption getoption (Header *h, const char **fmt, int *size) {
  /* dummy structure to get native alignment requirements */
  struct cD { char c; union { VIPERI_MAXALIGN; } u; };
  int opt = *((*fmt)++);
  *size = 0;  /* default */
  switch (opt) {
    case 'b': *size = sizeof(char); return Kint;
    case 'B': *size = sizeof(char); return Kuint;
    case 'h': *size = sizeof(short); return Kint;
    case 'H': *size = sizeof(short); return Kuint;
    case 'l': *size = sizeof(long); return Kint;
    case 'L': *size = sizeof(long); return Kuint;
    case 'j': *size = sizeof(viper_Integer); return Kint;
    case 'J': *size = sizeof(viper_Integer); return Kuint;
    case 'T': *size = sizeof(size_t); return Kuint;
    case 'f': *size = sizeof(float); return Kfloat;
    case 'n': *size = sizeof(viper_Number); return Knumber;
    case 'd': *size = sizeof(double); return Kdouble;
    case 'i': *size = getnumlimit(h, fmt, sizeof(int)); return Kint;
    case 'I': *size = getnumlimit(h, fmt, sizeof(int)); return Kuint;
    case 's': *size = getnumlimit(h, fmt, sizeof(size_t)); return Kstring;
    case 'c':
      *size = getnum(fmt, -1);
      if (l_unlikely(*size == -1))
        viperL_error(h->L, "missing size for format option 'c'");
      return Kchar;
    case 'z': return Kzstr;
    case 'x': *size = 1; return Kpadding;
    case 'X': return Kpaddalign;
    case ' ': break;
    case '<': h->islittle = 1; break;
    case '>': h->islittle = 0; break;
    case '=': h->islittle = nativeendian.little; break;
    case '!': {
      const int maxalign = offsetof(struct cD, u);
      h->maxalign = getnumlimit(h, fmt, maxalign);
      break;
    }
    default: viperL_error(h->L, "invalid format option '%c'", opt);
  }
  return Knop;
}


/*
** Read, classify, and fill other details about the next option.
** 'psize' is filled with option's size, 'notoalign' with its
** alignment requirements.
** Local variable 'size' gets the size to be aligned. (Kpadal option
** always gets its full alignment, other options are limited by
** the maximum alignment ('maxalign'). Kchar option needs no alignment
** despite its size.
*/
static KOption getdetails (Header *h, size_t totalsize,
                           const char **fmt, int *psize, int *ntoalign) {
  KOption opt = getoption(h, fmt, psize);
  int align = *psize;  /* usually, alignment follows size */
  if (opt == Kpaddalign) {  /* 'X' gets alignment from following option */
    if (**fmt == '\0' || getoption(h, fmt, &align) == Kchar || align == 0)
      viperL_argerror(h->L, 1, "invalid next option for option 'X'");
  }
  if (align <= 1 || opt == Kchar)  /* need no alignment? */
    *ntoalign = 0;
  else {
    if (align > h->maxalign)  /* enforce maximum alignment */
      align = h->maxalign;
    if (l_unlikely((align & (align - 1)) != 0))  /* not a power of 2? */
      viperL_argerror(h->L, 1, "format asks for alignment not power of 2");
    *ntoalign = (align - (int)(totalsize & (align - 1))) & (align - 1);
  }
  return opt;
}


/*
** Pack integer 'n' with 'size' bytes and 'islittle' endianness.
** The final 'if' handles the case when 'size' is larger than
** the size of a Viper integer, correcting the extra sign-extension
** bytes if necessary (by default they would be zeros).
*/
static void packint (viperL_Buffer *b, viper_Unsigned n,
                     int islittle, int size, int neg) {
  char *buff = viperL_prepbuffsize(b, size);
  int i;
  buff[islittle ? 0 : size - 1] = (char)(n & MC);  /* first byte */
  for (i = 1; i < size; i++) {
    n >>= NB;
    buff[islittle ? i : size - 1 - i] = (char)(n & MC);
  }
  if (neg && size > SZINT) {  /* negative number need sign extension? */
    for (i = SZINT; i < size; i++)  /* correct extra bytes */
      buff[islittle ? i : size - 1 - i] = (char)MC;
  }
  viperL_addsize(b, size);  /* add result to buffer */
}


/*
** Copy 'size' bytes from 'src' to 'dest', correcting endianness if
** given 'islittle' is different from native endianness.
*/
static void copywithendian (char *dest, const char *src,
                            int size, int islittle) {
  if (islittle == nativeendian.little)
    memcpy(dest, src, size);
  else {
    dest += size - 1;
    while (size-- != 0)
      *(dest--) = *(src++);
  }
}


static int str_pack (viper_State *L) {
  viperL_Buffer b;
  Header h;
  const char *fmt = viperL_checkstring(L, 1);  /* format string */
  int arg = 1;  /* current argument to pack */
  size_t totalsize = 0;  /* accumulate total size of result */
  initheader(L, &h);
  viper_pushnil(L);  /* mark to separate arguments from string buffer */
  viperL_buffinit(L, &b);
  while (*fmt != '\0') {
    int size, ntoalign;
    KOption opt = getdetails(&h, totalsize, &fmt, &size, &ntoalign);
    totalsize += ntoalign + size;
    while (ntoalign-- > 0)
     viperL_addchar(&b, VIPERL_PACKPADBYTE);  /* fill alignment */
    arg++;
    switch (opt) {
      case Kint: {  /* signed integers */
        viper_Integer n = viperL_checkinteger(L, arg);
        if (size < SZINT) {  /* need overflow check? */
          viper_Integer lim = (viper_Integer)1 << ((size * NB) - 1);
          viperL_argcheck(L, -lim <= n && n < lim, arg, "integer overflow");
        }
        packint(&b, (viper_Unsigned)n, h.islittle, size, (n < 0));
        break;
      }
      case Kuint: {  /* unsigned integers */
        viper_Integer n = viperL_checkinteger(L, arg);
        if (size < SZINT)  /* need overflow check? */
          viperL_argcheck(L, (viper_Unsigned)n < ((viper_Unsigned)1 << (size * NB)),
                           arg, "unsigned overflow");
        packint(&b, (viper_Unsigned)n, h.islittle, size, 0);
        break;
      }
      case Kfloat: {  /* C float */
        float f = (float)viperL_checknumber(L, arg);  /* get argument */
        char *buff = viperL_prepbuffsize(&b, sizeof(f));
        /* move 'f' to final result, correcting endianness if needed */
        copywithendian(buff, (char *)&f, sizeof(f), h.islittle);
        viperL_addsize(&b, size);
        break;
      }
      case Knumber: {  /* Viper float */
        viper_Number f = viperL_checknumber(L, arg);  /* get argument */
        char *buff = viperL_prepbuffsize(&b, sizeof(f));
        /* move 'f' to final result, correcting endianness if needed */
        copywithendian(buff, (char *)&f, sizeof(f), h.islittle);
        viperL_addsize(&b, size);
        break;
      }
      case Kdouble: {  /* C double */
        double f = (double)viperL_checknumber(L, arg);  /* get argument */
        char *buff = viperL_prepbuffsize(&b, sizeof(f));
        /* move 'f' to final result, correcting endianness if needed */
        copywithendian(buff, (char *)&f, sizeof(f), h.islittle);
        viperL_addsize(&b, size);
        break;
      }
      case Kchar: {  /* fixed-size string */
        size_t len;
        const char *s = viperL_checklstring(L, arg, &len);
        viperL_argcheck(L, len <= (size_t)size, arg,
                         "string longer than given size");
        viperL_addlstring(&b, s, len);  /* add string */
        while (len++ < (size_t)size)  /* pad extra space */
          viperL_addchar(&b, VIPERL_PACKPADBYTE);
        break;
      }
      case Kstring: {  /* strings with length count */
        size_t len;
        const char *s = viperL_checklstring(L, arg, &len);
        viperL_argcheck(L, size >= (int)sizeof(size_t) ||
                         len < ((size_t)1 << (size * NB)),
                         arg, "string length does not fit in given size");
        packint(&b, (viper_Unsigned)len, h.islittle, size, 0);  /* pack length */
        viperL_addlstring(&b, s, len);
        totalsize += len;
        break;
      }
      case Kzstr: {  /* zero-terminated string */
        size_t len;
        const char *s = viperL_checklstring(L, arg, &len);
        viperL_argcheck(L, strlen(s) == len, arg, "string contains zeros");
        viperL_addlstring(&b, s, len);
        viperL_addchar(&b, '\0');  /* add zero at the end */
        totalsize += len + 1;
        break;
      }
      case Kpadding: viperL_addchar(&b, VIPERL_PACKPADBYTE);  /* FALLTHROUGH */
      case Kpaddalign: case Knop:
        arg--;  /* undo increment */
        break;
    }
  }
  viperL_pushresult(&b);
  return 1;
}


static int str_packsize (viper_State *L) {
  Header h;
  const char *fmt = viperL_checkstring(L, 1);  /* format string */
  size_t totalsize = 0;  /* accumulate total size of result */
  initheader(L, &h);
  while (*fmt != '\0') {
    int size, ntoalign;
    KOption opt = getdetails(&h, totalsize, &fmt, &size, &ntoalign);
    viperL_argcheck(L, opt != Kstring && opt != Kzstr, 1,
                     "variable-length format");
    size += ntoalign;  /* total space used by option */
    viperL_argcheck(L, totalsize <= MAXSIZE - size, 1,
                     "format result too large");
    totalsize += size;
  }
  viper_pushinteger(L, (viper_Integer)totalsize);
  return 1;
}


/*
** Unpack an integer with 'size' bytes and 'islittle' endianness.
** If size is smaller than the size of a Viper integer and integer
** is signed, must do sign extension (propagating the sign to the
** higher bits); if size is larger than the size of a Viper integer,
** it must check the unread bytes to see whether they do not cause an
** overflow.
*/
static viper_Integer unpackint (viper_State *L, const char *str,
                              int islittle, int size, int issigned) {
  viper_Unsigned res = 0;
  int i;
  int limit = (size  <= SZINT) ? size : SZINT;
  for (i = limit - 1; i >= 0; i--) {
    res <<= NB;
    res |= (viper_Unsigned)(unsigned char)str[islittle ? i : size - 1 - i];
  }
  if (size < SZINT) {  /* real size smaller than viper_Integer? */
    if (issigned) {  /* needs sign extension? */
      viper_Unsigned mask = (viper_Unsigned)1 << (size*NB - 1);
      res = ((res ^ mask) - mask);  /* do sign extension */
    }
  }
  else if (size > SZINT) {  /* must check unread bytes */
    int mask = (!issigned || (viper_Integer)res >= 0) ? 0 : MC;
    for (i = limit; i < size; i++) {
      if (l_unlikely((unsigned char)str[islittle ? i : size - 1 - i] != mask))
        viperL_error(L, "%d-byte integer does not fit into Viper Integer", size);
    }
  }
  return (viper_Integer)res;
}


static int str_unpack (viper_State *L) {
  Header h;
  const char *fmt = viperL_checkstring(L, 1);
  size_t ld;
  const char *data = viperL_checklstring(L, 2, &ld);
  size_t pos = posrelatI(viperL_optinteger(L, 3, 1), ld) - 1;
  int n = 0;  /* number of results */
  viperL_argcheck(L, pos <= ld, 3, "initial position out of string");
  initheader(L, &h);
  while (*fmt != '\0') {
    int size, ntoalign;
    KOption opt = getdetails(&h, pos, &fmt, &size, &ntoalign);
    viperL_argcheck(L, (size_t)ntoalign + size <= ld - pos, 2,
                    "data string too short");
    pos += ntoalign;  /* skip alignment */
    /* stack space for item + next position */
    viperL_checkstack(L, 2, "too many results");
    n++;
    switch (opt) {
      case Kint:
      case Kuint: {
        viper_Integer res = unpackint(L, data + pos, h.islittle, size,
                                       (opt == Kint));
        viper_pushinteger(L, res);
        break;
      }
      case Kfloat: {
        float f;
        copywithendian((char *)&f, data + pos, sizeof(f), h.islittle);
        viper_pushnumber(L, (viper_Number)f);
        break;
      }
      case Knumber: {
        viper_Number f;
        copywithendian((char *)&f, data + pos, sizeof(f), h.islittle);
        viper_pushnumber(L, f);
        break;
      }
      case Kdouble: {
        double f;
        copywithendian((char *)&f, data + pos, sizeof(f), h.islittle);
        viper_pushnumber(L, (viper_Number)f);
        break;
      }
      case Kchar: {
        viper_pushlstring(L, data + pos, size);
        break;
      }
      case Kstring: {
        size_t len = (size_t)unpackint(L, data + pos, h.islittle, size, 0);
        viperL_argcheck(L, len <= ld - pos - size, 2, "data string too short");
        viper_pushlstring(L, data + pos + size, len);
        pos += len;  /* skip string */
        break;
      }
      case Kzstr: {
        size_t len = strlen(data + pos);
        viperL_argcheck(L, pos + len < ld, 2,
                         "unfinished string for format 'z'");
        viper_pushlstring(L, data + pos, len);
        pos += len + 1;  /* skip string plus final '\0' */
        break;
      }
      case Kpaddalign: case Kpadding: case Knop:
        n--;  /* undo increment */
        break;
    }
    pos += size;
  }
  viper_pushinteger(L, pos + 1);  /* next position */
  return n + 1;
}

/* }====================================================== */


static const viperL_Reg strlib[] = {
  {"byte", str_byte},
  {"char", str_char},
  {"dump", str_dump},
  {"find", str_find},
  {"format", str_format},
  {"gmatch", gmatch},
  {"gsub", str_gsub},
  {"len", str_len},
  {"lower", str_lower},
  {"match", str_match},
  {"rep", str_rep},
  {"reverse", str_reverse},
  {"sub", str_sub},
  {"upper", str_upper},
  {"pack", str_pack},
  {"packsize", str_packsize},
  {"unpack", str_unpack},
  {NULL, NULL}
};


static void createmetatable (viper_State *L) {
  /* table to be metatable for strings */
  viperL_newlibtable(L, stringmetamethods);
  viperL_setfuncs(L, stringmetamethods, 0);
  viper_pushliteral(L, "");  /* dummy string */
  viper_pushvalue(L, -2);  /* copy table */
  viper_setmetatable(L, -2);  /* set table as metatable for strings */
  viper_pop(L, 1);  /* pop dummy string */
  viper_pushvalue(L, -2);  /* get string library */
  viper_setfield(L, -2, "__index");  /* metatable.__index = string */
  viper_pop(L, 1);  /* pop metatable */
}


/*
** Open string library
*/
VIPERMOD_API int viperopen_string (viper_State *L) {
  viperL_newlib(L, strlib);
  createmetatable(L);
  return 1;
}

