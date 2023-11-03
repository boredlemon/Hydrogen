/*
** $Id: virtualMachine.c $
** Nebula virtual machine
** See Copyright Notice in nebula.h
*/

#define virtualMachine_c
#define NEBULA_CORE

#include "prefix.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nebula.h"

#include "debug.h"
#include "do.h"
#include "function.h"
#include "garbageCollection.h"
#include "object.h"
#include "opcodes.h"
#include "state.h"
#include "string.h"
#include "table.h"
#include "tagMethods.h"
#include "virtualMachine.h"


/*
** By default, use jump tables in the main interpreter loop on gcc
** and compatible compilers.
*/
#if !defined(NEBULA_USE_JUMPTABLE)
#if defined(__GNUC__)
#define NEBULA_USE_JUMPTABLE	1
#else
#define NEBULA_USE_JUMPTABLE	0
#endif
#endif



/* limit for table tag-method chains (to avoid infinite loops) */
#define MAXTAGLOOP	2000


/*
** 'l_intfitsf' checks whether a given integer is in the range that
** can be converted to a float without rounding. Used in comparisons.
*/

/* number of bits in the mantissa of a float */
#define NBM		(l_floatatt(MANT_DIG))

/*
** Check whether some integers may not fit in a float, testing whether
** (maxinteger >> NBM) > 0. (That implies (1 << NBM) <= maxinteger.)
** (The shifts are done in parts, to avoid shifting by more than the size
** of an integer. In a worst case, NBM == 113 for long double and
** sizeof(long) == 32.)
*/
#if ((((NEBULA_MAXINTEGER >> (NBM / 4)) >> (NBM / 4)) >> (NBM / 4)) \
	>> (NBM - (3 * (NBM / 4))))  >  0

/* limit for integers that fit in a float */
#define MAXINTFITSF	((nebula_Unsigned)1 << NBM)

/* check whether 'i' is in the interval [-MAXINTFITSF, MAXINTFITSF] */
#define l_intfitsf(i)	((MAXINTFITSF + l_castS2U(i)) <= (2 * MAXINTFITSF))

#else  /* all integers fit in a float precisely */

#define l_intfitsf(i)	1

#endif


/*
** Try to convert a value from string to a number value.
** If the value is not a string or is a string not representing
** a valid numeral (or if coercions from strings to numbers
** are disabled via macro 'cvt2num'), do not modify 'result'
** and return 0.
*/
static int l_strton (const TValue *obj, TValue *result) {
  nebula_assert(obj != result);
  if (!cvt2num(obj))  /* is object not a string? */
    return 0;
  else
    return (nebulaO_str2num(svalue(obj), result) == vslen(obj) + 1);
}


/*
** Try to convert a value to a float. The float case is already handled
** by the macro 'tonumber'.
*/
int nebulaV_tonumber_ (const TValue *obj, nebula_Number *n) {
  TValue v;
  if (ttisinteger(obj)) {
    *n = cast_num(ivalue(obj));
    return 1;
  }
  else if (l_strton(obj, &v)) {  /* string coercible to number? */
    *n = nvalue(&v);  /* convert result of 'nebulaO_str2num' to a float */
    return 1;
  }
  else
    return 0;  /* conversion failed */
}


/*
** try to convert a float to an integer, rounding according to 'mode'.
*/
int nebulaV_flttointeger (nebula_Number n, nebula_Integer *p, F2Imod mode) {
  nebula_Number f = l_floor(n);
  if (n != f) {  /* not an integral value? */
    if (mode == F2Ieq) return 0;  /* fails if mode demands integral value */
    else if (mode == F2Iceil)  /* needs ceil? */
      f += 1;  /* convert floor to ceil (remember: n != f) */
  }
  return nebula_numbertointeger(f, p);
}


/*
** try to convert a value to an integer, rounding according to 'mode',
** without string coercion.
** ("Fast track" handled by macro 'tointegerns'.)
*/
int nebulaV_tointegerns (const TValue *obj, nebula_Integer *p, F2Imod mode) {
  if (ttisfloat(obj))
    return nebulaV_flttointeger(fltvalue(obj), p, mode);
  else if (ttisinteger(obj)) {
    *p = ivalue(obj);
    return 1;
  }
  else
    return 0;
}


/*
** try to convert a value to an integer.
*/
int nebulaV_tointeger (const TValue *obj, nebula_Integer *p, F2Imod mode) {
  TValue v;
  if (l_strton(obj, &v))  /* does 'obj' point to a numerical string? */
    obj = &v;  /* change it to point to its corresponding number */
  return nebulaV_tointegerns(obj, p, mode);
}


/*
** Try to convert a 'for' limit to an integer, preserving the semantics
** of the loop. Return true if the loop must not run; otherwise, '*p'
** gets the integer limit.
** (The following explanation assumes a positive step; it is valid for
** negative steps mutatis mutandis.)
** If the limit is an integer or can be converted to an integer,
** rounding down, that is the limit.
** Otherwise, check whether the limit can be converted to a float. If
** the float is too large, clip it to NEBULA_MAXINTEGER.  If the float
** is too negative, the loop should not run, because any initial
** integer value is greater than such limit; so, the function returns
** true to signal that. (For this latter case, no integer limit would be
** correct; even a limit of NEBULA_MININTEGER would run the loop once for
** an initial value equal to NEBULA_MININTEGER.)
*/
static int forlimit (nebula_State *L, nebula_Integer init, const TValue *lim,
                                   nebula_Integer *p, nebula_Integer step) {
  if (!nebulaV_tointeger(lim, p, (step < 0 ? F2Iceil : F2Ifloor))) {
    /* not coercible to in integer */
    nebula_Number flim;  /* try to convert to float */
    if (!tonumber(lim, &flim)) /* cannot convert to float? */
      nebulaG_forerror(L, lim, "limit");
    /* else 'flim' is a float out of integer bounds */
    if (nebulai_numlt(0, flim)) {  /* if it is positive, it is too large */
      if (step < 0) return 1;  /* initial value must be less than it */
      *p = NEBULA_MAXINTEGER;  /* truncate */
    }
    else {  /* it is less than min integer */
      if (step > 0) return 1;  /* initial value must be greater than it */
      *p = NEBULA_MININTEGER;  /* truncate */
    }
  }
  return (step > 0 ? init > *p : init < *p);  /* not to run? */
}


/*
** Prepare a numerical for loop (opcode OP_FORPREP).
** Return true to skip the loop. Otherwise,
** after preparation, stack will be as follows:
**   ra : internal index (safe copy of the control variable)
**   ra + 1 : loop counter (integer loops) or limit (float loops)
**   ra + 2 : step
**   ra + 3 : control variable
*/
static int forprep (nebula_State *L, StkId ra) {
  TValue *pinit = s2v(ra);
  TValue *plimit = s2v(ra + 1);
  TValue *pstep = s2v(ra + 2);
  if (ttisinteger(pinit) && ttisinteger(pstep)) { /* integer loop? */
    nebula_Integer init = ivalue(pinit);
    nebula_Integer step = ivalue(pstep);
    nebula_Integer limit;
    if (step == 0)
      nebulaG_runerror(L, "'for' step is zero");
    setivalue(s2v(ra + 3), init);  /* control variable */
    if (forlimit(L, init, plimit, &limit, step))
      return 1;  /* skip the loop */
    else {  /* prepare loop counter */
      nebula_Unsigned count;
      if (step > 0) {  /* ascending loop? */
        count = l_castS2U(limit) - l_castS2U(init);
        if (step != 1)  /* avoid division in the too common case */
          count /= l_castS2U(step);
      }
      else {  /* step < 0; descending loop */
        count = l_castS2U(init) - l_castS2U(limit);
        /* 'step+1' avoids negating 'mininteger' */
        count /= l_castS2U(-(step + 1)) + 1u;
      }
      /* store the counter in place of the limit (which won't be
         needed anymore) */
      setivalue(plimit, l_castU2S(count));
    }
  }
  else {  /* try making all values floats */
    nebula_Number init; nebula_Number limit; nebula_Number step;
    if (l_unlikely(!tonumber(plimit, &limit)))
      nebulaG_forerror(L, plimit, "limit");
    if (l_unlikely(!tonumber(pstep, &step)))
      nebulaG_forerror(L, pstep, "step");
    if (l_unlikely(!tonumber(pinit, &init)))
      nebulaG_forerror(L, pinit, "initial value");
    if (step == 0)
      nebulaG_runerror(L, "'for' step is zero");
    if (nebulai_numlt(0, step) ? nebulai_numlt(limit, init)
                            : nebulai_numlt(init, limit))
      return 1;  /* skip the loop */
    else {
      /* make sure internal values are all floats */
      setfltvalue(plimit, limit);
      setfltvalue(pstep, step);
      setfltvalue(s2v(ra), init);  /* internal index */
      setfltvalue(s2v(ra + 3), init);  /* control variable */
    }
  }
  return 0;
}


/*
** Execute a step of a float numerical for loop, returning
** true iff the loop must continue. (The integer case is
** written online with opcode OP_FORLOOP, for performance.)
*/
static int floatforloop (StkId ra) {
  nebula_Number step = fltvalue(s2v(ra + 2));
  nebula_Number limit = fltvalue(s2v(ra + 1));
  nebula_Number idx = fltvalue(s2v(ra));  /* internal index */
  idx = nebulai_numadd(L, idx, step);  /* increment index */
  if (nebulai_numlt(0, step) ? nebulai_numle(idx, limit)
                          : nebulai_numle(limit, idx)) {
    chgfltvalue(s2v(ra), idx);  /* update internal index */
    setfltvalue(s2v(ra + 3), idx);  /* and control variable */
    return 1;  /* jump back */
  }
  else
    return 0;  /* finish the loop */
}


/*
** Finish the table access 'val = t[key]'.
** if 'slot' is NULL, 't' is not a table; otherwise, 'slot' points to
** t[k] entry (which must be empty).
*/
void nebulaV_finishget (nebula_State *L, const TValue *t, TValue *key, StkId val,
                      const TValue *slot) {
  int loop;  /* counter to avoid infinite loops */
  const TValue *tm;  /* metamethod */
  for (loop = 0; loop < MAXTAGLOOP; loop++) {
    if (slot == NULL) {  /* 't' is not a table? */
      nebula_assert(!ttistable(t));
      tm = nebulaT_gettmbyobj(L, t, TM_INDEX);
      if (l_unlikely(notm(tm)))
        nebulaG_typeerror(L, t, "index");  /* no metamethod */
      /* else will try the metamethod */
    }
    else {  /* 't' is a table */
      nebula_assert(isempty(slot));
      tm = fasttm(L, hvalue(t)->metatable, TM_INDEX);  /* table's metamethod */
      if (tm == NULL) {  /* no metamethod? */
        setnilvalue(s2v(val));  /* result is nil */
        return;
      }
      /* else will try the metamethod */
    }
    if (ttisfunction(tm)) {  /* is metamethod a function? */
      nebulaT_caltagMethodsres(L, tm, t, key, val);  /* call it */
      return;
    }
    t = tm;  /* else try to access 'tm[key]' */
    if (nebulaV_fastget(L, t, key, slot, nebulaH_get)) {  /* fast track? */
      setobj2s(L, val, slot);  /* done */
      return;
    }
    /* else repeat (tail call 'nebulaV_finishget') */
  }
  nebulaG_runerror(L, "'__index' chain too long; possible loop");
}


/*
** Finish a table assignment 't[key] = val'.
** If 'slot' is NULL, 't' is not a table.  Otherwise, 'slot' points
** to the entry 't[key]', or to a value with an absent key if there
** is no such entry.  (The value at 'slot' must be empty, otherwise
** 'nebulaV_fastget' would have done the job.)
*/
void nebulaV_finishset (nebula_State *L, const TValue *t, TValue *key,
                     TValue *val, const TValue *slot) {
  int loop;  /* counter to avoid infinite loops */
  for (loop = 0; loop < MAXTAGLOOP; loop++) {
    const TValue *tm;  /* '__newindex' metamethod */
    if (slot != NULL) {  /* is 't' a table? */
      Table *h = hvalue(t);  /* save 't' table */
      nebula_assert(isempty(slot));  /* slot must be empty */
      tm = fasttm(L, h->metatable, TM_NEWINDEX);  /* get metamethod */
      if (tm == NULL) {  /* no metamethod? */
        nebulaH_finishset(L, h, key, slot, val);  /* set new value */
        invalidateTMcache(h);
        nebulaC_barrierback(L, obj2gco(h), val);
        return;
      }
      /* else will try the metamethod */
    }
    else {  /* not a table; check metamethod */
      tm = nebulaT_gettmbyobj(L, t, TM_NEWINDEX);
      if (l_unlikely(notm(tm)))
        nebulaG_typeerror(L, t, "index");
    }
    /* try the metamethod */
    if (ttisfunction(tm)) {
      nebulaT_caltagMethods(L, tm, t, key, val);
      return;
    }
    t = tm;  /* else repeat assignment over 'tm' */
    if (nebulaV_fastget(L, t, key, slot, nebulaH_get)) {
      nebulaV_finishfastset(L, t, slot, val);
      return;  /* done */
    }
    /* else 'return nebulaV_finishset(L, t, key, val, slot)' (loop) */
  }
  nebulaG_runerror(L, "'__newindex' chain too long; possible loop");
}


/*
** Compare two strings 'ls' x 'rs', returning an integer less-equal-
** -greater than zero if 'ls' is less-equal-greater than 'rs'.
** The code is a little tricky because it allows '\0' in the strings
** and it uses 'strcoll' (to respect locales) for each segments
** of the strings.
*/
static int l_strcmp (const TString *ls, const TString *rs) {
  const char *l = getstr(ls);
  size_t ll = tsslen(ls);
  const char *r = getstr(rs);
  size_t lr = tsslen(rs);
  for (;;) {  /* for each segment */
    int temp = strcoll(l, r);
    if (temp != 0)  /* not equal? */
      return temp;  /* done */
    else {  /* strings are equal up to a '\0' */
      size_t len = strlen(l);  /* index of first '\0' in both strings */
      if (len == lr)  /* 'rs' is finished? */
        return (len == ll) ? 0 : 1;  /* check 'ls' */
      else if (len == ll)  /* 'ls' is finished? */
        return -1;  /* 'ls' is less than 'rs' ('rs' is not finished) */
      /* both strings longer than 'len'; Nebula on comparing after the '\0' */
      len++;
      l += len; ll -= len; r += len; lr -= len;
    }
  }
}


/*
** Check whether integer 'i' is less than float 'f'. If 'i' has an
** exact representation as a float ('l_intfitsf'), compare numbers as
** floats. Otherwise, use the equivalence 'i < f <=> i < ceil(f)'.
** If 'ceil(f)' is out of integer range, either 'f' is greater than
** all integers or less than all integers.
** (The test with 'l_intfitsf' is only for performance; the else
** case is correct for all values, but it is slow due to the conversion
** from float to int.)
** When 'f' is NaN, comparisons must result in false.
*/
l_sinline int LTintfloat (nebula_Integer i, nebula_Number f) {
  if (l_intfitsf(i))
    return nebulai_numlt(cast_num(i), f);  /* compare them as floats */
  else {  /* i < f <=> i < ceil(f) */
    nebula_Integer fi;
    if (nebulaV_flttointeger(f, &fi, F2Iceil))  /* fi = ceil(f) */
      return i < fi;   /* compare them as integers */
    else  /* 'f' is either greater or less than all integers */
      return f > 0;  /* greater? */
  }
}


/*
** Check whether integer 'i' is less than or equal to float 'f'.
** See comments on previous function.
*/
l_sinline int LEintfloat (nebula_Integer i, nebula_Number f) {
  if (l_intfitsf(i))
    return nebulai_numle(cast_num(i), f);  /* compare them as floats */
  else {  /* i <= f <=> i <= floor(f) */
    nebula_Integer fi;
    if (nebulaV_flttointeger(f, &fi, F2Ifloor))  /* fi = floor(f) */
      return i <= fi;   /* compare them as integers */
    else  /* 'f' is either greater or less than all integers */
      return f > 0;  /* greater? */
  }
}


/*
** Check whether float 'f' is less than integer 'i'.
** See comments on previous function.
*/
l_sinline int LTfloatint (nebula_Number f, nebula_Integer i) {
  if (l_intfitsf(i))
    return nebulai_numlt(f, cast_num(i));  /* compare them as floats */
  else {  /* f < i <=> floor(f) < i */
    nebula_Integer fi;
    if (nebulaV_flttointeger(f, &fi, F2Ifloor))  /* fi = floor(f) */
      return fi < i;   /* compare them as integers */
    else  /* 'f' is either greater or less than all integers */
      return f < 0;  /* less? */
  }
}


/*
** Check whether float 'f' is less than or equal to integer 'i'.
** See comments on previous function.
*/
l_sinline int LEfloatint (nebula_Number f, nebula_Integer i) {
  if (l_intfitsf(i))
    return nebulai_numle(f, cast_num(i));  /* compare them as floats */
  else {  /* f <= i <=> ceil(f) <= i */
    nebula_Integer fi;
    if (nebulaV_flttointeger(f, &fi, F2Iceil))  /* fi = ceil(f) */
      return fi <= i;   /* compare them as integers */
    else  /* 'f' is either greater or less than all integers */
      return f < 0;  /* less? */
  }
}


/*
** Return 'l < r', for numbers.
*/
l_sinline int LTnum (const TValue *l, const TValue *r) {
  nebula_assert(ttisnumber(l) && ttisnumber(r));
  if (ttisinteger(l)) {
    nebula_Integer li = ivalue(l);
    if (ttisinteger(r))
      return li < ivalue(r);  /* both are integers */
    else  /* 'l' is int and 'r' is float */
      return LTintfloat(li, fltvalue(r));  /* l < r ? */
  }
  else {
    nebula_Number lf = fltvalue(l);  /* 'l' must be float */
    if (ttisfloat(r))
      return nebulai_numlt(lf, fltvalue(r));  /* both are float */
    else  /* 'l' is float and 'r' is int */
      return LTfloatint(lf, ivalue(r));
  }
}


/*
** Return 'l <= r', for numbers.
*/
l_sinline int LEnum (const TValue *l, const TValue *r) {
  nebula_assert(ttisnumber(l) && ttisnumber(r));
  if (ttisinteger(l)) {
    nebula_Integer li = ivalue(l);
    if (ttisinteger(r))
      return li <= ivalue(r);  /* both are integers */
    else  /* 'l' is int and 'r' is float */
      return LEintfloat(li, fltvalue(r));  /* l <= r ? */
  }
  else {
    nebula_Number lf = fltvalue(l);  /* 'l' must be float */
    if (ttisfloat(r))
      return nebulai_numle(lf, fltvalue(r));  /* both are float */
    else  /* 'l' is float and 'r' is int */
      return LEfloatint(lf, ivalue(r));
  }
}


/*
** return 'l < r' for non-numbers.
*/
static int lessthanothers (nebula_State *L, const TValue *l, const TValue *r) {
  nebula_assert(!ttisnumber(l) || !ttisnumber(r));
  if (ttisstring(l) && ttisstring(r))  /* both are strings? */
    return l_strcmp(tsvalue(l), tsvalue(r)) < 0;
  else
    return nebulaT_callorderTM(L, l, r, TM_LT);
}


/*
** Main operation less than; return 'l < r'.
*/
int nebulaV_lessthan (nebula_State *L, const TValue *l, const TValue *r) {
  if (ttisnumber(l) && ttisnumber(r))  /* both operands are numbers? */
    return LTnum(l, r);
  else return lessthanothers(L, l, r);
}


/*
** return 'l <= r' for non-numbers.
*/
static int lessequalothers (nebula_State *L, const TValue *l, const TValue *r) {
  nebula_assert(!ttisnumber(l) || !ttisnumber(r));
  if (ttisstring(l) && ttisstring(r))  /* both are strings? */
    return l_strcmp(tsvalue(l), tsvalue(r)) <= 0;
  else
    return nebulaT_callorderTM(L, l, r, TM_LE);
}


/*
** Main operation less than or equal to; return 'l <= r'.
*/
int nebulaV_lessequal (nebula_State *L, const TValue *l, const TValue *r) {
  if (ttisnumber(l) && ttisnumber(r))  /* both operands are numbers? */
    return LEnum(l, r);
  else return lessequalothers(L, l, r);
}


/*
** Main operation for equality of Nebula values; return 't1 == t2'.
** L == NULL means raw equality (no metamethods)
*/
int nebulaV_equalobj (nebula_State *L, const TValue *t1, const TValue *t2) {
  const TValue *tm;
  if (ttypetag(t1) != ttypetag(t2)) {  /* not the same variant? */
    if (ttype(t1) != ttype(t2) || ttype(t1) != NEBULA_TNUMBER)
      return 0;  /* only numbers can be equal with different variants */
    else {  /* two numbers with different variants */
      /* One of them is an integer. If the other does not have an
         integer value, they cannot be equal; otherwise, compare their
         integer values. */
      nebula_Integer i1, i2;
      return (nebulaV_tointegerns(t1, &i1, F2Ieq) &&
              nebulaV_tointegerns(t2, &i2, F2Ieq) &&
              i1 == i2);
    }
  }
  /* values have same type and same variant */
  switch (ttypetag(t1)) {
    case NEBULA_VNIL: case NEBULA_VFALSE: case NEBULA_VTRUE: return 1;
    case NEBULA_VNUMINT: return (ivalue(t1) == ivalue(t2));
    case NEBULA_VNUMFLT: return nebulai_numeq(fltvalue(t1), fltvalue(t2));
    case NEBULA_VLIGHTUSERDATA: return pvalue(t1) == pvalue(t2);
    case NEBULA_VLCF: return fvalue(t1) == fvalue(t2);
    case NEBULA_VSHRSTR: return eqshrstr(tsvalue(t1), tsvalue(t2));
    case NEBULA_VLNGSTR: return nebulaS_eqlngstr(tsvalue(t1), tsvalue(t2));
    case NEBULA_VUSERDATA: {
      if (uvalue(t1) == uvalue(t2)) return 1;
      else if (L == NULL) return 0;
      tm = fasttm(L, uvalue(t1)->metatable, TM_EQ);
      if (tm == NULL)
        tm = fasttm(L, uvalue(t2)->metatable, TM_EQ);
      break;  /* will try TM */
    }
    case NEBULA_VTABLE: {
      if (hvalue(t1) == hvalue(t2)) return 1;
      else if (L == NULL) return 0;
      tm = fasttm(L, hvalue(t1)->metatable, TM_EQ);
      if (tm == NULL)
        tm = fasttm(L, hvalue(t2)->metatable, TM_EQ);
      break;  /* will try TM */
    }
    default:
      return gcvalue(t1) == gcvalue(t2);
  }
  if (tm == NULL)  /* no TM? */
    return 0;  /* objects are different */
  else {
    nebulaT_caltagMethodsres(L, tm, t1, t2, L->top);  /* call TM */
    return !l_isfalse(s2v(L->top));
  }
}


/* macro used by 'nebulaV_concat' to ensure that element at 'o' is a string */
#define tostring(L,o)  \
	(ttisstring(o) || (cvt2str(o) && (nebulaO_tostring(L, o), 1)))

#define isemptystr(o)	(ttisshrstring(o) && tsvalue(o)->shrlen == 0)

/* copy strings in stack from top - n up to top - 1 to buffer */
static void copy2buff (StkId top, int n, char *buff) {
  size_t tl = 0;  /* size already copied */
  do {
    size_t l = vslen(s2v(top - n));  /* length of string being copied */
    memcpy(buff + tl, svalue(s2v(top - n)), l * sizeof(char));
    tl += l;
  } while (--n > 0);
}


/*
** Main operation for concatenation: concat 'total' values in the stack,
** from 'L->top - total' up to 'L->top - 1'.
*/
void nebulaV_concat (nebula_State *L, int total) {
  if (total == 1)
    return;  /* "all" values already concatenated */
  do {
    StkId top = L->top;
    int n = 2;  /* number of elements handled in this pass (at least 2) */
    if (!(ttisstring(s2v(top - 2)) || cvt2str(s2v(top - 2))) ||
        !tostring(L, s2v(top - 1)))
      nebulaT_tryconcatTM(L);
    else if (isemptystr(s2v(top - 1)))  /* second operand is empty? */
      cast_void(tostring(L, s2v(top - 2)));  /* result is first operand */
    else if (isemptystr(s2v(top - 2))) {  /* first operand is empty string? */
      setobjs2s(L, top - 2, top - 1);  /* result is second op. */
    }
    else {
      /* at least two non-empty string values; get as many as possible */
      size_t tl = vslen(s2v(top - 1));
      TString *ts;
      /* collect total length and number of strings */
      for (n = 1; n < total && tostring(L, s2v(top - n - 1)); n++) {
        size_t l = vslen(s2v(top - n - 1));
        if (l_unlikely(l >= (MAX_SIZE/sizeof(char)) - tl))
          nebulaG_runerror(L, "string length overflow");
        tl += l;
      }
      if (tl <= NEBULAI_MAXSHORTLEN) {  /* is result a short string? */
        char buff[NEBULAI_MAXSHORTLEN];
        copy2buff(top, n, buff);  /* copy strings to buffer */
        ts = nebulaS_newlstr(L, buff, tl);
      }
      else {  /* long string; copy strings directly to final result */
        ts = nebulaS_createlngstrobj(L, tl);
        copy2buff(top, n, getstr(ts));
      }
      setsvalue2s(L, top - n, ts);  /* create result */
    }
    total -= n-1;  /* Nebulat 'n' strings to create 1 new */
    L->top -= n-1;  /* popped 'n' strings and pushed one */
  } while (total > 1);  /* repeat until only 1 result left */
}


/*
** Main operation 'ra = #rb'.
*/
void nebulaV_objlen (nebula_State *L, StkId ra, const TValue *rb) {
  const TValue *tm;
  switch (ttypetag(rb)) {
    case NEBULA_VTABLE: {
      Table *h = hvalue(rb);
      tm = fasttm(L, h->metatable, TM_LEN);
      if (tm) break;  /* metamethod? break switch to call it */
      setivalue(s2v(ra), nebulaH_getn(h));  /* else primitive len */
      return;
    }
    case NEBULA_VSHRSTR: {
      setivalue(s2v(ra), tsvalue(rb)->shrlen);
      return;
    }
    case NEBULA_VLNGSTR: {
      setivalue(s2v(ra), tsvalue(rb)->u.lnglen);
      return;
    }
    default: {  /* try metamethod */
      tm = nebulaT_gettmbyobj(L, rb, TM_LEN);
      if (l_unlikely(notm(tm)))  /* no metamethod? */
        nebulaG_typeerror(L, rb, "get length of");
      break;
    }
  }
  nebulaT_caltagMethodsres(L, tm, rb, rb, ra);
}


/*
** Integer division; return 'm // n', that is, floor(m/n).
** C division truncates its result (rounds towards zero).
** 'floor(q) == trunc(q)' when 'q >= 0' or when 'q' is integer,
** otherwise 'floor(q) == trunc(q) - 1'.
*/
nebula_Integer nebulaV_idiv (nebula_State *L, nebula_Integer m, nebula_Integer n) {
  if (l_unlikely(l_castS2U(n) + 1u <= 1u)) {  /* special cases: -1 or 0 */
    if (n == 0)
      nebulaG_runerror(L, "attempt to divide by zero");
    return intop(-, 0, m);   /* n==-1; avoid overflow with 0x80000...//-1 */
  }
  else {
    nebula_Integer q = m / n;  /* perform C division */
    if ((m ^ n) < 0 && m % n != 0)  /* 'm/n' would be negative non-integer? */
      q -= 1;  /* correct result for different rounding */
    return q;
  }
}


/*
** Integer modulus; return 'm % n'. (Assume that C '%' with
** negative operands follows C99 behavior. See previous comment
** about nebulaV_idiv.)
*/
nebula_Integer nebulaV_mod (nebula_State *L, nebula_Integer m, nebula_Integer n) {
  if (l_unlikely(l_castS2U(n) + 1u <= 1u)) {  /* special cases: -1 or 0 */
    if (n == 0)
      nebulaG_runerror(L, "attempt to perform 'n%%0'");
    return 0;   /* m % -1 == 0; avoid overflow with 0x80000...%-1 */
  }
  else {
    nebula_Integer r = m % n;
    if (r != 0 && (r ^ n) < 0)  /* 'm/n' would be non-integer negative? */
      r += n;  /* correct result for different rounding */
    return r;
  }
}


/*
** Float modulus
*/
nebula_Number nebulaV_modf (nebula_State *L, nebula_Number m, nebula_Number n) {
  nebula_Number r;
  nebulai_nummod(L, m, n, r);
  return r;
}


/* number of bits in an integer */
#define NBITS	cast_int(sizeof(nebula_Integer) * CHAR_BIT)

/*
** Shift left operation. (Shift right just negates 'y'.)
*/
#define nebulaV_shiftr(x,y)	nebulaV_shiftl(x,intop(-, 0, y))


nebula_Integer nebulaV_shiftl (nebula_Integer x, nebula_Integer y) {
  if (y < 0) {  /* shift right? */
    if (y <= -NBITS) return 0;
    else return intop(>>, x, -y);
  }
  else {  /* shift left */
    if (y >= NBITS) return 0;
    else return intop(<<, x, y);
  }
}


/*
** create a new Nebula closure, push it in the stack, and initialize
** its upvalues.
*/
static void pushclosure (nebula_State *L, Proto *p, UpVal **ennebula, StkId base,
                         StkId ra) {
  int nup = p->sizeupvalues;
  Upvaldesc *uv = p->upvalues;
  int i;
  LClosure *ncl = nebulaF_newLclosure(L, nup);
  ncl->p = p;
  setclLvalue2s(L, ra, ncl);  /* anchor new closure in stack */
  for (i = 0; i < nup; i++) {  /* fill in its upvalues */
    if (uv[i].instack)  /* upvalue refers to local variable? */
      ncl->upvals[i] = nebulaF_findupval(L, base + uv[i].idx);
    else  /* get upvalue from enclosing function */
      ncl->upvals[i] = ennebula[uv[i].idx];
    nebulaC_objbarrier(L, ncl, ncl->upvals[i]);
  }
}


/*
** finish execution of an opcode interrupted by a yield
*/
void nebulaV_finishOp (nebula_State *L) {
  CallInfo *ci = L->ci;
  StkId base = ci->func + 1;
  Instruction inst = *(ci->u.l.savedpc - 1);  /* interrupted instruction */
  OpCode op = GET_OPCODE(inst);
  switch (op) {  /* finish its execution */
    case OP_MMBIN: case OP_MMBINI: case OP_MMBINK: {
      setobjs2s(L, base + GETARG_A(*(ci->u.l.savedpc - 2)), --L->top);
      break;
    }
    case OP_UNM: case OP_BNOT: case OP_LEN:
    case OP_GETTABUP: case OP_GETTABLE: case OP_GETI:
    case OP_GETFIELD: case OP_SELF: {
      setobjs2s(L, base + GETARG_A(inst), --L->top);
      break;
    }
    case OP_LT: case OP_LE:
    case OP_LTI: case OP_LEI:
    case OP_GTI: case OP_GEI:
    case OP_EQ: {  /* note that 'OP_EQI'/'OP_EQK' cannot yield */
      int res = !l_isfalse(s2v(L->top - 1));
      L->top--;
#if defined(NEBULA_COMPAT_LT_LE)
      if (ci->callstatus & CIST_LEQ) {  /* "<=" using "<" instead? */
        ci->callstatus ^= CIST_LEQ;  /* clear mark */
        res = !res;  /* negate result */
      }
#endif
      nebula_assert(GET_OPCODE(*ci->u.l.savedpc) == OP_JMP);
      if (res != GETARG_k(inst))  /* condition failed? */
        ci->u.l.savedpc++;  /* skip jump instruction */
      break;
    }
    case OP_CONCAT: {
      StkId top = L->top - 1;  /* top when 'nebulaT_tryconcatTM' was called */
      int a = GETARG_A(inst);      /* first element to concatenate */
      int total = cast_int(top - 1 - (base + a));  /* yet to concatenate */
      setobjs2s(L, top - 2, top);  /* put TM result in proper position */
      L->top = top - 1;  /* top is one after last element (at top-2) */
      nebulaV_concat(L, total);  /* concat them (may yield again) */
      break;
    }
    case OP_CLOSE: {  /* yielded closing variables */
      ci->u.l.savedpc--;  /* repeat instruction to close other vars. */
      break;
    }
    case OP_RETURN: {  /* yielded closing variables */
      StkId ra = base + GETARG_A(inst);
      /* adjust top to signal correct number of returns, in case the
         return is "up to top" ('isIT') */
      L->top = ra + ci->u2.nres;
      /* repeat instruction to close other vars. and complete the return */
      ci->u.l.savedpc--;
      break;
    }
    default: {
      /* only these other opcodes can yield */
      nebula_assert(op == OP_TFORCALL || op == OP_CALL ||
           op == OP_TAILCALL || op == OP_SETTABUP || op == OP_SETTABLE ||
           op == OP_SETI || op == OP_SETFIELD);
      break;
    }
  }
}




/*
** {==================================================================
** Macros for arithmetic/bitwise/comparison opcodes in 'nebulaV_execute'
** ===================================================================
*/

#define l_addi(L,a,b)	intop(+, a, b)
#define l_subi(L,a,b)	intop(-, a, b)
#define l_muli(L,a,b)	intop(*, a, b)
#define l_band(a,b)	intop(&, a, b)
#define l_bor(a,b)	intop(|, a, b)
#define l_bxor(a,b)	intop(^, a, b)

#define l_lti(a,b)	(a < b)
#define l_lei(a,b)	(a <= b)
#define l_gti(a,b)	(a > b)
#define l_gei(a,b)	(a >= b)


/*
** Arithmetic operations with immediate operands. 'iop' is the integer
** operation, 'fop' is the float operation.
*/
#define op_arithI(L,iop,fop) {  \
  TValue *v1 = vRB(i);  \
  int imm = GETARG_sC(i);  \
  if (ttisinteger(v1)) {  \
    nebula_Integer iv1 = ivalue(v1);  \
    pc++; setivalue(s2v(ra), iop(L, iv1, imm));  \
  }  \
  else if (ttisfloat(v1)) {  \
    nebula_Number nb = fltvalue(v1);  \
    nebula_Number fimm = cast_num(imm);  \
    pc++; setfltvalue(s2v(ra), fop(L, nb, fimm)); \
  }}


/*
** Auxiliary function for arithmetic operations over floats and others
** with two register operands.
*/
#define op_arithf_aux(L,v1,v2,fop) {  \
  nebula_Number n1; nebula_Number n2;  \
  if (tonumberns(v1, n1) && tonumberns(v2, n2)) {  \
    pc++; setfltvalue(s2v(ra), fop(L, n1, n2));  \
  }}


/*
** Arithmetic operations over floats and others with register operands.
*/
#define op_arithf(L,fop) {  \
  TValue *v1 = vRB(i);  \
  TValue *v2 = vRC(i);  \
  op_arithf_aux(L, v1, v2, fop); }


/*
** Arithmetic operations with K operands for floats.
*/
#define op_arithfK(L,fop) {  \
  TValue *v1 = vRB(i);  \
  TValue *v2 = KC(i); nebula_assert(ttisnumber(v2));  \
  op_arithf_aux(L, v1, v2, fop); }


/*
** Arithmetic operations over integers and floats.
*/
#define op_arith_aux(L,v1,v2,iop,fop) {  \
  if (ttisinteger(v1) && ttisinteger(v2)) {  \
    nebula_Integer i1 = ivalue(v1); nebula_Integer i2 = ivalue(v2);  \
    pc++; setivalue(s2v(ra), iop(L, i1, i2));  \
  }  \
  else op_arithf_aux(L, v1, v2, fop); }


/*
** Arithmetic operations with register operands.
*/
#define op_arith(L,iop,fop) {  \
  TValue *v1 = vRB(i);  \
  TValue *v2 = vRC(i);  \
  op_arith_aux(L, v1, v2, iop, fop); }


/*
** Arithmetic operations with K operands.
*/
#define op_arithK(L,iop,fop) {  \
  TValue *v1 = vRB(i);  \
  TValue *v2 = KC(i); nebula_assert(ttisnumber(v2));  \
  op_arith_aux(L, v1, v2, iop, fop); }


/*
** Bitwise operations with constant operand.
*/
#define op_bitwiseK(L,op) {  \
  TValue *v1 = vRB(i);  \
  TValue *v2 = KC(i);  \
  nebula_Integer i1;  \
  nebula_Integer i2 = ivalue(v2);  \
  if (tointegerns(v1, &i1)) {  \
    pc++; setivalue(s2v(ra), op(i1, i2));  \
  }}


/*
** Bitwise operations with register operands.
*/
#define op_bitwise(L,op) {  \
  TValue *v1 = vRB(i);  \
  TValue *v2 = vRC(i);  \
  nebula_Integer i1; nebula_Integer i2;  \
  if (tointegerns(v1, &i1) && tointegerns(v2, &i2)) {  \
    pc++; setivalue(s2v(ra), op(i1, i2));  \
  }}


/*
** Order operations with register operands. 'opn' actually works
** for all numbers, but the fast track improves performance for
** integers.
*/
#define op_order(L,opi,opn,other) {  \
        int cond;  \
        TValue *rb = vRB(i);  \
        if (ttisinteger(s2v(ra)) && ttisinteger(rb)) {  \
          nebula_Integer ia = ivalue(s2v(ra));  \
          nebula_Integer ib = ivalue(rb);  \
          cond = opi(ia, ib);  \
        }  \
        else if (ttisnumber(s2v(ra)) && ttisnumber(rb))  \
          cond = opn(s2v(ra), rb);  \
        else  \
          Protect(cond = other(L, s2v(ra), rb));  \
        docondjump(); }


/*
** Order operations with immediate operand. (Immediate operand is
** always small enough to have an exact representation as a float.)
*/
#define op_orderI(L,opi,opf,inv,tm) {  \
        int cond;  \
        int im = GETARG_sB(i);  \
        if (ttisinteger(s2v(ra)))  \
          cond = opi(ivalue(s2v(ra)), im);  \
        else if (ttisfloat(s2v(ra))) {  \
          nebula_Number fa = fltvalue(s2v(ra));  \
          nebula_Number fim = cast_num(im);  \
          cond = opf(fa, fim);  \
        }  \
        else {  \
          int isf = GETARG_C(i);  \
          Protect(cond = nebulaT_callorderiTM(L, s2v(ra), im, inv, isf, tm));  \
        }  \
        docondjump(); }

/* }================================================================== */


/*
** {==================================================================
** Function 'nebulaV_execute': main interpreter loop
** ===================================================================
*/

/*
** some macros for common tasks in 'nebulaV_execute'
*/


#define RA(i)	(base+GETARG_A(i))
#define RB(i)	(base+GETARG_B(i))
#define vRB(i)	s2v(RB(i))
#define KB(i)	(k+GETARG_B(i))
#define RC(i)	(base+GETARG_C(i))
#define vRC(i)	s2v(RC(i))
#define KC(i)	(k+GETARG_C(i))
#define RKC(i)	((TESTARG_k(i)) ? k + GETARG_C(i) : s2v(base + GETARG_C(i)))



#define updatetrap(ci)  (trap = ci->u.l.trap)

#define updatebase(ci)	(base = ci->func + 1)


#define updatestack(ci)  \
	{ if (l_unlikely(trap)) { updatebase(ci); ra = RA(i); } }


/*
** Execute a jump instruction. The 'updatetrap' allows signals to stop
** tight loops. (Without it, the local copy of 'trap' could never change.)
*/
#define dojump(ci,i,e)	{ pc += GETARG_sJ(i) + e; updatetrap(ci); }


/* for test instructions, execute the jump instruction that follows it */
#define donextjump(ci)	{ Instruction ni = *pc; dojump(ci, ni, 1); }

/*
** do a conditional jump: skip next instruction if 'cond' is not what
** was expected (parameter 'k'), else do next instruction, which must
** be a jump.
*/
#define docondjump()	if (cond != GETARG_k(i)) pc++; else donextjump(ci);


/*
** Correct global 'pc'.
*/
#define savepc(L)	(ci->u.l.savedpc = pc)


/*
** Whenever code can raise errors, the global 'pc' and the global
** 'top' must be correct to report occasional errors.
*/
#define savestate(L,ci)		(savepc(L), L->top = ci->top)


/*
** Protect code that, in general, can raise errors, reallocate the
** stack, and change the hooks.
*/
#define Protect(exp)  (savestate(L,ci), (exp), updatetrap(ci))

/* special version that does not change the top */
#define ProtectNT(exp)  (savepc(L), (exp), updatetrap(ci))

/*
** Protect code that can only raise errors. (That is, it cannot change
** the stack or hooks.)
*/
#define halfProtect(exp)  (savestate(L,ci), (exp))

/* 'c' is the limit of live values in the stack */
#define checkGC(L,c)  \
	{ nebulaC_condGC(L, (savepc(L), L->top = (c)), \
                         updatetrap(ci)); \
           nebulai_threadyield(L); }


/* fetch an instruction and prepare its execution */
#define vmfetch()	{ \
  if (l_unlikely(trap)) {  /* stack reallocation or hooks? */ \
    trap = nebulaG_traceexec(L, pc);  /* handle hooks */ \
    updatebase(ci);  /* correct stack */ \
  } \
  i = *(pc++); \
  ra = RA(i); /* WARNING: any stack reallocation invalidates 'ra' */ \
}

#define vmdispatch(o)	switch(o)
#define vmcase(l)	case l:
#define vmbreak		break


void nebulaV_execute (nebula_State *L, CallInfo *ci) {
  LClosure *cl;
  TValue *k;
  StkId base;
  const Instruction *pc;
  int trap;
#if NEBULA_USE_JUMPTABLE
#include "jumptab.h"
#endif
 startfunc:
  trap = L->hookmask;
 returning:  /* trap already set */
  cl = clLvalue(s2v(ci->func));
  k = cl->p->k;
  pc = ci->u.l.savedpc;
  if (l_unlikely(trap)) {
    if (pc == cl->p->code) {  /* first instruction (not resuming)? */
      if (cl->p->is_vararg)
        trap = 0;  /* hooks will start after VARARGPREP instruction */
      else  /* check 'call' hook */
        nebulaD_hookcall(L, ci);
    }
    ci->u.l.trap = 1;  /* assume trap is on, for now */
  }
  base = ci->func + 1;
  /* main loop of interpreter */
  for (;;) {
    Instruction i;  /* instruction being executed */
    StkId ra;  /* instruction's A register */
    vmfetch();
    #if 0
      /* low-level line tracing for debugging Nebula */
      printf("line: %d\n", nebulaG_getfuncline(cl->p, pcRel(pc, cl->p)));
    #endif
    nebula_assert(base == ci->func + 1);
    nebula_assert(base <= L->top && L->top < L->stack_last);
    /* invalidate top for instructions not expecting it */
    nebula_assert(isIT(i) || (cast_void(L->top = base), 1));
    vmdispatch (GET_OPCODE(i)) {
      vmcase(OP_MOVE) {
        setobjs2s(L, ra, RB(i));
        vmbreak;
      }
      vmcase(OP_LOADI) {
        nebula_Integer b = GETARG_sBx(i);
        setivalue(s2v(ra), b);
        vmbreak;
      }
      vmcase(OP_LOADF) {
        int b = GETARG_sBx(i);
        setfltvalue(s2v(ra), cast_num(b));
        vmbreak;
      }
      vmcase(OP_LOADK) {
        TValue *rb = k + GETARG_Bx(i);
        setobj2s(L, ra, rb);
        vmbreak;
      }
      vmcase(OP_LOADKX) {
        TValue *rb;
        rb = k + GETARG_Ax(*pc); pc++;
        setobj2s(L, ra, rb);
        vmbreak;
      }
      vmcase(OP_LOADFALSE) {
        setbfvalue(s2v(ra));
        vmbreak;
      }
      vmcase(OP_LFALSESKIP) {
        setbfvalue(s2v(ra));
        pc++;  /* skip next instruction */
        vmbreak;
      }
      vmcase(OP_LOADTRUE) {
        setbtvalue(s2v(ra));
        vmbreak;
      }
      vmcase(OP_LOADNIL) {
        int b = GETARG_B(i);
        do {
          setnilvalue(s2v(ra++));
        } while (b--);
        vmbreak;
      }
      vmcase(OP_GETUPVAL) {
        int b = GETARG_B(i);
        setobj2s(L, ra, cl->upvals[b]->v);
        vmbreak;
      }
      vmcase(OP_SETUPVAL) {
        UpVal *uv = cl->upvals[GETARG_B(i)];
        setobj(L, uv->v, s2v(ra));
        nebulaC_barrier(L, uv, s2v(ra));
        vmbreak;
      }
      vmcase(OP_GETTABUP) {
        const TValue *slot;
        TValue *upval = cl->upvals[GETARG_B(i)]->v;
        TValue *rc = KC(i);
        TString *key = tsvalue(rc);  /* key must be a string */
        if (nebulaV_fastget(L, upval, key, slot, nebulaH_getshortstr)) {
          setobj2s(L, ra, slot);
        }
        else
          Protect(nebulaV_finishget(L, upval, rc, ra, slot));
        vmbreak;
      }
      vmcase(OP_GETTABLE) {
        const TValue *slot;
        TValue *rb = vRB(i);
        TValue *rc = vRC(i);
        nebula_Unsigned n;
        if (ttisinteger(rc)  /* fast track for integers? */
            ? (cast_void(n = ivalue(rc)), nebulaV_fastgeti(L, rb, n, slot))
            : nebulaV_fastget(L, rb, rc, slot, nebulaH_get)) {
          setobj2s(L, ra, slot);
        }
        else
          Protect(nebulaV_finishget(L, rb, rc, ra, slot));
        vmbreak;
      }
      vmcase(OP_GETI) {
        const TValue *slot;
        TValue *rb = vRB(i);
        int c = GETARG_C(i);
        if (nebulaV_fastgeti(L, rb, c, slot)) {
          setobj2s(L, ra, slot);
        }
        else {
          TValue key;
          setivalue(&key, c);
          Protect(nebulaV_finishget(L, rb, &key, ra, slot));
        }
        vmbreak;
      }
      vmcase(OP_GETFIELD) {
        const TValue *slot;
        TValue *rb = vRB(i);
        TValue *rc = KC(i);
        TString *key = tsvalue(rc);  /* key must be a string */
        if (nebulaV_fastget(L, rb, key, slot, nebulaH_getshortstr)) {
          setobj2s(L, ra, slot);
        }
        else
          Protect(nebulaV_finishget(L, rb, rc, ra, slot));
        vmbreak;
      }
      vmcase(OP_SETTABUP) {
        const TValue *slot;
        TValue *upval = cl->upvals[GETARG_A(i)]->v;
        TValue *rb = KB(i);
        TValue *rc = RKC(i);
        TString *key = tsvalue(rb);  /* key must be a string */
        if (nebulaV_fastget(L, upval, key, slot, nebulaH_getshortstr)) {
          nebulaV_finishfastset(L, upval, slot, rc);
        }
        else
          Protect(nebulaV_finishset(L, upval, rb, rc, slot));
        vmbreak;
      }
      vmcase(OP_SETTABLE) {
        const TValue *slot;
        TValue *rb = vRB(i);  /* key (table is in 'ra') */
        TValue *rc = RKC(i);  /* value */
        nebula_Unsigned n;
        if (ttisinteger(rb)  /* fast track for integers? */
            ? (cast_void(n = ivalue(rb)), nebulaV_fastgeti(L, s2v(ra), n, slot))
            : nebulaV_fastget(L, s2v(ra), rb, slot, nebulaH_get)) {
          nebulaV_finishfastset(L, s2v(ra), slot, rc);
        }
        else
          Protect(nebulaV_finishset(L, s2v(ra), rb, rc, slot));
        vmbreak;
      }
      vmcase(OP_SETI) {
        const TValue *slot;
        int c = GETARG_B(i);
        TValue *rc = RKC(i);
        if (nebulaV_fastgeti(L, s2v(ra), c, slot)) {
          nebulaV_finishfastset(L, s2v(ra), slot, rc);
        }
        else {
          TValue key;
          setivalue(&key, c);
          Protect(nebulaV_finishset(L, s2v(ra), &key, rc, slot));
        }
        vmbreak;
      }
      vmcase(OP_SETFIELD) {
        const TValue *slot;
        TValue *rb = KB(i);
        TValue *rc = RKC(i);
        TString *key = tsvalue(rb);  /* key must be a string */
        if (nebulaV_fastget(L, s2v(ra), key, slot, nebulaH_getshortstr)) {
          nebulaV_finishfastset(L, s2v(ra), slot, rc);
        }
        else
          Protect(nebulaV_finishset(L, s2v(ra), rb, rc, slot));
        vmbreak;
      }
      vmcase(OP_NEWTABLE) {
        int b = GETARG_B(i);  /* log2(hash size) + 1 */
        int c = GETARG_C(i);  /* array size */
        Table *t;
        if (b > 0)
          b = 1 << (b - 1);  /* size is 2^(b - 1) */
        nebula_assert((!TESTARG_k(i)) == (GETARG_Ax(*pc) == 0));
        if (TESTARG_k(i))  /* non-zero extra argument? */
          c += GETARG_Ax(*pc) * (MAXARG_C + 1);  /* add it to size */
        pc++;  /* skip extra argument */
        L->top = ra + 1;  /* correct top in case of emergency GC */
        t = nebulaH_new(L);  /* memory allocation */
        sethvalue2s(L, ra, t);
        if (b != 0 || c != 0)
          nebulaH_resize(L, t, c, b);  /* idem */
        checkGC(L, ra + 1);
        vmbreak;
      }
      vmcase(OP_SELF) {
        const TValue *slot;
        TValue *rb = vRB(i);
        TValue *rc = RKC(i);
        TString *key = tsvalue(rc);  /* key must be a string */
        setobj2s(L, ra + 1, rb);
        if (nebulaV_fastget(L, rb, key, slot, nebulaH_getstr)) {
          setobj2s(L, ra, slot);
        }
        else
          Protect(nebulaV_finishget(L, rb, rc, ra, slot));
        vmbreak;
      }
      vmcase(OP_ADDI) {
        op_arithI(L, l_addi, nebulai_numadd);
        vmbreak;
      }
      vmcase(OP_ADDK) {
        op_arithK(L, l_addi, nebulai_numadd);
        vmbreak;
      }
      vmcase(OP_SUBK) {
        op_arithK(L, l_subi, nebulai_numsub);
        vmbreak;
      }
      vmcase(OP_MULK) {
        op_arithK(L, l_muli, nebulai_nummul);
        vmbreak;
      }
      vmcase(OP_MODK) {
        op_arithK(L, nebulaV_mod, nebulaV_modf);
        vmbreak;
      }
      vmcase(OP_POWK) {
        op_arithfK(L, nebulai_numpow);
        vmbreak;
      }
      vmcase(OP_DIVK) {
        op_arithfK(L, nebulai_numdiv);
        vmbreak;
      }
      vmcase(OP_IDIVK) {
        op_arithK(L, nebulaV_idiv, nebulai_numidiv);
        vmbreak;
      }
      vmcase(OP_BANDK) {
        op_bitwiseK(L, l_band);
        vmbreak;
      }
      vmcase(OP_BORK) {
        op_bitwiseK(L, l_bor);
        vmbreak;
      }
      vmcase(OP_BXORK) {
        op_bitwiseK(L, l_bxor);
        vmbreak;
      }
      vmcase(OP_SHRI) {
        TValue *rb = vRB(i);
        int ic = GETARG_sC(i);
        nebula_Integer ib;
        if (tointegerns(rb, &ib)) {
          pc++; setivalue(s2v(ra), nebulaV_shiftl(ib, -ic));
        }
        vmbreak;
      }
      vmcase(OP_SHLI) {
        TValue *rb = vRB(i);
        int ic = GETARG_sC(i);
        nebula_Integer ib;
        if (tointegerns(rb, &ib)) {
          pc++; setivalue(s2v(ra), nebulaV_shiftl(ic, ib));
        }
        vmbreak;
      }
      vmcase(OP_ADD) {
        op_arith(L, l_addi, nebulai_numadd);
        vmbreak;
      }
      vmcase(OP_SUB) {
        op_arith(L, l_subi, nebulai_numsub);
        vmbreak;
      }
      vmcase(OP_MUL) {
        op_arith(L, l_muli, nebulai_nummul);
        vmbreak;
      }
      vmcase(OP_MOD) {
        op_arith(L, nebulaV_mod, nebulaV_modf);
        vmbreak;
      }
      vmcase(OP_POW) {
        op_arithf(L, nebulai_numpow);
        vmbreak;
      }
      vmcase(OP_DIV) {  /* float division (always with floats) */
        op_arithf(L, nebulai_numdiv);
        vmbreak;
      }
      vmcase(OP_IDIV) {  /* floor division */
        op_arith(L, nebulaV_idiv, nebulai_numidiv);
        vmbreak;
      }
      vmcase(OP_BAND) {
        op_bitwise(L, l_band);
        vmbreak;
      }
      vmcase(OP_BOR) {
        op_bitwise(L, l_bor);
        vmbreak;
      }
      vmcase(OP_BXOR) {
        op_bitwise(L, l_bxor);
        vmbreak;
      }
      vmcase(OP_SHR) {
        op_bitwise(L, nebulaV_shiftr);
        vmbreak;
      }
      vmcase(OP_SHL) {
        op_bitwise(L, nebulaV_shiftl);
        vmbreak;
      }
      vmcase(OP_MMBIN) {
        Instruction pi = *(pc - 2);  /* original arith. expression */
        TValue *rb = vRB(i);
        TMS tm = (TMS)GETARG_C(i);
        StkId result = RA(pi);
        nebula_assert(OP_ADD <= GET_OPCODE(pi) && GET_OPCODE(pi) <= OP_SHR);
        Protect(nebulaT_trybinTM(L, s2v(ra), rb, result, tm));
        vmbreak;
      }
      vmcase(OP_MMBINI) {
        Instruction pi = *(pc - 2);  /* original arith. expression */
        int imm = GETARG_sB(i);
        TMS tm = (TMS)GETARG_C(i);
        int flip = GETARG_k(i);
        StkId result = RA(pi);
        Protect(nebulaT_trybiniTM(L, s2v(ra), imm, flip, result, tm));
        vmbreak;
      }
      vmcase(OP_MMBINK) {
        Instruction pi = *(pc - 2);  /* original arith. expression */
        TValue *imm = KB(i);
        TMS tm = (TMS)GETARG_C(i);
        int flip = GETARG_k(i);
        StkId result = RA(pi);
        Protect(nebulaT_trybinassocTM(L, s2v(ra), imm, flip, result, tm));
        vmbreak;
      }
      vmcase(OP_UNM) {
        TValue *rb = vRB(i);
        nebula_Number nb;
        if (ttisinteger(rb)) {
          nebula_Integer ib = ivalue(rb);
          setivalue(s2v(ra), intop(-, 0, ib));
        }
        else if (tonumberns(rb, nb)) {
          setfltvalue(s2v(ra), nebulai_numunm(L, nb));
        }
        else
          Protect(nebulaT_trybinTM(L, rb, rb, ra, TM_UNM));
        vmbreak;
      }
      vmcase(OP_BNOT) {
        TValue *rb = vRB(i);
        nebula_Integer ib;
        if (tointegerns(rb, &ib)) {
          setivalue(s2v(ra), intop(^, ~l_castS2U(0), ib));
        }
        else
          Protect(nebulaT_trybinTM(L, rb, rb, ra, TM_BNOT));
        vmbreak;
      }
      vmcase(OP_NOT) {
        TValue *rb = vRB(i);
        if (l_isfalse(rb))
          setbtvalue(s2v(ra));
        else
          setbfvalue(s2v(ra));
        vmbreak;
      }
      vmcase(OP_LEN) {
        Protect(nebulaV_objlen(L, ra, vRB(i)));
        vmbreak;
      }
      vmcase(OP_CONCAT) {
        int n = GETARG_B(i);  /* number of elements to concatenate */
        L->top = ra + n;  /* mark the end of concat operands */
        ProtectNT(nebulaV_concat(L, n));
        checkGC(L, L->top); /* 'nebulaV_concat' ensures correct top */
        vmbreak;
      }
      vmcase(OP_CLOSE) {
        Protect(nebulaF_close(L, ra, NEBULA_OK, 1));
        vmbreak;
      }
      vmcase(OP_TBC) {
        /* create new to-be-closed upvalue */
        halfProtect(nebulaF_newtbnebulaval(L, ra));
        vmbreak;
      }
      vmcase(OP_JMP) {
        dojump(ci, i, 0);
        vmbreak;
      }
      vmcase(OP_EQ) {
        int cond;
        TValue *rb = vRB(i);
        Protect(cond = nebulaV_equalobj(L, s2v(ra), rb));
        docondjump();
        vmbreak;
      }
      vmcase(OP_LT) {
        op_order(L, l_lti, LTnum, lessthanothers);
        vmbreak;
      }
      vmcase(OP_LE) {
        op_order(L, l_lei, LEnum, lessequalothers);
        vmbreak;
      }
      vmcase(OP_EQK) {
        TValue *rb = KB(i);
        /* basic types do not use '__eq'; we can use raw equality */
        int cond = nebulaV_rawequalobj(s2v(ra), rb);
        docondjump();
        vmbreak;
      }
      vmcase(OP_EQI) {
        int cond;
        int im = GETARG_sB(i);
        if (ttisinteger(s2v(ra)))
          cond = (ivalue(s2v(ra)) == im);
        else if (ttisfloat(s2v(ra)))
          cond = nebulai_numeq(fltvalue(s2v(ra)), cast_num(im));
        else
          cond = 0;  /* other types cannot be equal to a number */
        docondjump();
        vmbreak;
      }
      vmcase(OP_LTI) {
        op_orderI(L, l_lti, nebulai_numlt, 0, TM_LT);
        vmbreak;
      }
      vmcase(OP_LEI) {
        op_orderI(L, l_lei, nebulai_numle, 0, TM_LE);
        vmbreak;
      }
      vmcase(OP_GTI) {
        op_orderI(L, l_gti, nebulai_numgt, 1, TM_LT);
        vmbreak;
      }
      vmcase(OP_GEI) {
        op_orderI(L, l_gei, nebulai_numge, 1, TM_LE);
        vmbreak;
      }
      vmcase(OP_TEST) {
        int cond = !l_isfalse(s2v(ra));
        docondjump();
        vmbreak;
      }
      vmcase(OP_TESTSET) {
        TValue *rb = vRB(i);
        if (l_isfalse(rb) == GETARG_k(i))
          pc++;
        else {
          setobj2s(L, ra, rb);
          donextjump(ci);
        }
        vmbreak;
      }
      vmcase(OP_CALL) {
        CallInfo *newci;
        int b = GETARG_B(i);
        int nresults = GETARG_C(i) - 1;
        if (b != 0)  /* fixed number of arguments? */
          L->top = ra + b;  /* top signals number of arguments */
        /* else previous instruction set top */
        savepc(L);  /* in case of errors */
        if ((newci = nebulaD_precall(L, ra, nresults)) == NULL)
          updatetrap(ci);  /* C call; nothing else to be done */
        else {  /* Nebula call: run function in this same C frame */
          ci = newci;
          goto startfunc;
        }
        vmbreak;
      }
      vmcase(OP_TAILCALL) {
        int b = GETARG_B(i);  /* number of arguments + 1 (function) */
        int n;  /* number of results when calling a C function */
        int nparams1 = GETARG_C(i);
        /* delta is virtual 'func' - real 'func' (vararg functions) */
        int delta = (nparams1) ? ci->u.l.nextraargs + nparams1 : 0;
        if (b != 0)
          L->top = ra + b;
        else  /* previous instruction set top */
          b = cast_int(L->top - ra);
        savepc(ci);  /* several calls here can raise errors */
        if (TESTARG_k(i)) {
          nebulaF_closeupval(L, base);  /* close upvalues from current call */
          nebula_assert(L->tbclist < base);  /* no pending tbc variables */
          nebula_assert(base == ci->func + 1);
        }
        if ((n = nebulaD_pretailcall(L, ci, ra, b, delta)) < 0)  /* Nebula function? */
          goto startfunc;  /* execute the callee */
        else {  /* C function? */
          ci->func -= delta;  /* restore 'func' (if vararg) */
          nebulaD_poscall(L, ci, n);  /* finish caller */
          updatetrap(ci);  /* 'nebulaD_poscall' can change hooks */
          goto ret;  /* caller returns after the tail call */
        }
      }
      vmcase(OP_RETURN) {
        int n = GETARG_B(i) - 1;  /* number of results */
        int nparams1 = GETARG_C(i);
        if (n < 0)  /* not fixed? */
          n = cast_int(L->top - ra);  /* get what is available */
        savepc(ci);
        if (TESTARG_k(i)) {  /* may there be open upvalues? */
          ci->u2.nres = n;  /* save number of returns */
          if (L->top < ci->top)
            L->top = ci->top;
          nebulaF_close(L, base, CLOSEKTOP, 1);
          updatetrap(ci);
          updatestack(ci);
        }
        if (nparams1)  /* vararg function? */
          ci->func -= ci->u.l.nextraargs + nparams1;
        L->top = ra + n;  /* set call for 'nebulaD_poscall' */
        nebulaD_poscall(L, ci, n);
        updatetrap(ci);  /* 'nebulaD_poscall' can change hooks */
        goto ret;
      }
      vmcase(OP_RETURN0) {
        if (l_unlikely(L->hookmask)) {
          L->top = ra;
          savepc(ci);
          nebulaD_poscall(L, ci, 0);  /* no hurry... */
          trap = 1;
        }
        else {  /* do the 'poscall' here */
          int nres;
          L->ci = ci->previous;  /* back to caller */
          L->top = base - 1;
          for (nres = ci->nresults; l_unlikely(nres > 0); nres--)
            setnilvalue(s2v(L->top++));  /* all results are nil */
        }
        goto ret;
      }
      vmcase(OP_RETURN1) {
        if (l_unlikely(L->hookmask)) {
          L->top = ra + 1;
          savepc(ci);
          nebulaD_poscall(L, ci, 1);  /* no hurry... */
          trap = 1;
        }
        else {  /* do the 'poscall' here */
          int nres = ci->nresults;
          L->ci = ci->previous;  /* back to caller */
          if (nres == 0)
            L->top = base - 1;  /* asked for no results */
          else {
            setobjs2s(L, base - 1, ra);  /* at least this result */
            L->top = base;
            for (; l_unlikely(nres > 1); nres--)
              setnilvalue(s2v(L->top++));  /* complete missing results */
          }
        }
       ret:  /* return from a Nebula function */
        if (ci->callstatus & CIST_FRESH)
          return;  /* end this frame */
        else {
          ci = ci->previous;
          goto returning;  /* continue running caller in this frame */
        }
      }
      vmcase(OP_FORLOOP) {
        if (ttisinteger(s2v(ra + 2))) {  /* integer loop? */
          nebula_Unsigned count = l_castS2U(ivalue(s2v(ra + 1)));
          if (count > 0) {  /* still more iterations? */
            nebula_Integer step = ivalue(s2v(ra + 2));
            nebula_Integer idx = ivalue(s2v(ra));  /* internal index */
            chgivalue(s2v(ra + 1), count - 1);  /* update counter */
            idx = intop(+, idx, step);  /* add step to index */
            chgivalue(s2v(ra), idx);  /* update internal index */
            setivalue(s2v(ra + 3), idx);  /* and control variable */
            pc -= GETARG_Bx(i);  /* jump back */
          }
        }
        else if (floatforloop(ra))  /* float loop */
          pc -= GETARG_Bx(i);  /* jump back */
        updatetrap(ci);  /* allows a signal to break the loop */
        vmbreak;
      }
      vmcase(OP_FORPREP) {
        savestate(L, ci);  /* in case of errors */
        if (forprep(L, ra))
          pc += GETARG_Bx(i) + 1;  /* skip the loop */
        vmbreak;
      }
      vmcase(OP_TFORPREP) {
        /* create to-be-closed upvalue (if needed) */
        halfProtect(nebulaF_newtbnebulaval(L, ra + 3));
        pc += GETARG_Bx(i);
        i = *(pc++);  /* Nebula to next instruction */
        nebula_assert(GET_OPCODE(i) == OP_TFORCALL && ra == RA(i));
        goto l_tforcall;
      }
      vmcase(OP_TFORCALL) {
       l_tforcall:
        /* 'ra' has the iterator function, 'ra + 1' has the state,
           'ra + 2' has the control variable, and 'ra + 3' has the
           to-be-closed variable. The call will use the stack after
           these values (starting at 'ra + 4')
        */
        /* push function, state, and control variable */
        memcpy(ra + 4, ra, 3 * sizeof(*ra));
        L->top = ra + 4 + 3;
        ProtectNT(nebulaD_call(L, ra + 4, GETARG_C(i)));  /* do the call */
        updatestack(ci);  /* stack may have changed */
        i = *(pc++);  /* Nebula to next instruction */
        nebula_assert(GET_OPCODE(i) == OP_TFORLOOP && ra == RA(i));
        goto l_tforloop;
      }
      vmcase(OP_TFORLOOP) {
        l_tforloop:
        if (!ttisnil(s2v(ra + 4))) {  /* continue loop? */
          setobjs2s(L, ra + 2, ra + 4);  /* save control variable */
          pc -= GETARG_Bx(i);  /* jump back */
        }
        vmbreak;
      }
      vmcase(OP_SETLIST) {
        int n = GETARG_B(i);
        unsigned int last = GETARG_C(i);
        Table *h = hvalue(s2v(ra));
        if (n == 0)
          n = cast_int(L->top - ra) - 1;  /* get up to the top */
        else
          L->top = ci->top;  /* correct top in case of emergency GC */
        last += n;
        if (TESTARG_k(i)) {
          last += GETARG_Ax(*pc) * (MAXARG_C + 1);
          pc++;
        }
        if (last > nebulaH_realasize(h))  /* needs more space? */
          nebulaH_resizearray(L, h, last);  /* preallocate it at once */
        for (; n > 0; n--) {
          TValue *val = s2v(ra + n);
          setobj2t(L, &h->array[last - 1], val);
          last--;
          nebulaC_barrierback(L, obj2gco(h), val);
        }
        vmbreak;
      }
      vmcase(OP_CLOSURE) {
        Proto *p = cl->p->p[GETARG_Bx(i)];
        halfProtect(pushclosure(L, p, cl->upvals, base, ra));
        checkGC(L, ra + 1);
        vmbreak;
      }
      vmcase(OP_VARARG) {
        int n = GETARG_C(i) - 1;  /* required results */
        Protect(nebulaT_getvarargs(L, ci, ra, n));
        vmbreak;
      }
      vmcase(OP_VARARGPREP) {
        ProtectNT(nebulaT_adjustvarargs(L, GETARG_A(i), ci, cl->p));
        if (l_unlikely(trap)) {  /* previous "Protect" updated trap */
          nebulaD_hookcall(L, ci);
          L->oldpc = 1;  /* next opcode will be seen as a "new" line */
        }
        updatebase(ci);  /* function has new base after adjustment */
        vmbreak;
      }
      vmcase(OP_EXTRAARG) {
        nebula_assert(0);
        vmbreak;
      }
    }
  }
}

/* }================================================================== */
