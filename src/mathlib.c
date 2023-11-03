/*
** $Id: mathlib.c $
** Standard mathematical library
** See Copyright Notice in nebula.h
*/

#define mathlib_c
#define NEBULA_LIB

#include "prefix.h"


#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "nebula.h"

#include "auxlib.h"
#include "nebulalib.h"


#undef PI
#define PI	(l_mathop(3.141592653589793238462643383279502884))


static int math_abs (nebula_State *L) {
  if (nebula_isinteger(L, 1)) {
    nebula_Integer n = nebula_tointeger(L, 1);
    if (n < 0) n = (nebula_Integer)(0u - (nebula_Unsigned)n);
    nebula_pushinteger(L, n);
  }
  else
    nebula_pushnumber(L, l_mathop(fabs)(nebulaL_checknumber(L, 1)));
  return 1;
}

static int math_sin (nebula_State *L) {
  nebula_pushnumber(L, l_mathop(sin)(nebulaL_checknumber(L, 1)));
  return 1;
}

static int math_cos (nebula_State *L) {
  nebula_pushnumber(L, l_mathop(cos)(nebulaL_checknumber(L, 1)));
  return 1;
}

static int math_tan (nebula_State *L) {
  nebula_pushnumber(L, l_mathop(tan)(nebulaL_checknumber(L, 1)));
  return 1;
}

static int math_asin (nebula_State *L) {
  nebula_pushnumber(L, l_mathop(asin)(nebulaL_checknumber(L, 1)));
  return 1;
}

static int math_acos (nebula_State *L) {
  nebula_pushnumber(L, l_mathop(acos)(nebulaL_checknumber(L, 1)));
  return 1;
}

static int math_atan (nebula_State *L) {
  nebula_Number y = nebulaL_checknumber(L, 1);
  nebula_Number x = nebulaL_optnumber(L, 2, 1);
  nebula_pushnumber(L, l_mathop(atan2)(y, x));
  return 1;
}


static int math_toint (nebula_State *L) {
  int valid;
  nebula_Integer n = nebula_tointegerx(L, 1, &valid);
  if (l_likely(valid))
    nebula_pushinteger(L, n);
  else {
    nebulaL_checkany(L, 1);
    nebulaL_pushfail(L);  /* value is not convertible to integer */
  }
  return 1;
}


static void pushnumint (nebula_State *L, nebula_Number d) {
  nebula_Integer n;
  if (nebula_numbertointeger(d, &n))  /* does 'd' fit in an integer? */
    nebula_pushinteger(L, n);  /* result is integer */
  else
    nebula_pushnumber(L, d);  /* result is float */
}


static int math_floor (nebula_State *L) {
  if (nebula_isinteger(L, 1))
    nebula_settop(L, 1);  /* integer is its own floor */
  else {
    nebula_Number d = l_mathop(floor)(nebulaL_checknumber(L, 1));
    pushnumint(L, d);
  }
  return 1;
}


static int math_ceil (nebula_State *L) {
  if (nebula_isinteger(L, 1))
    nebula_settop(L, 1);  /* integer is its own ceil */
  else {
    nebula_Number d = l_mathop(ceil)(nebulaL_checknumber(L, 1));
    pushnumint(L, d);
  }
  return 1;
}


static int math_fmod (nebula_State *L) {
  if (nebula_isinteger(L, 1) && nebula_isinteger(L, 2)) {
    nebula_Integer d = nebula_tointeger(L, 2);
    if ((nebula_Unsigned)d + 1u <= 1u) {  /* special cases: -1 or 0 */
      nebulaL_argcheck(L, d != 0, 2, "zero");
      nebula_pushinteger(L, 0);  /* avoid overflow with 0x80000... / -1 */
    }
    else
      nebula_pushinteger(L, nebula_tointeger(L, 1) % d);
  }
  else
    nebula_pushnumber(L, l_mathop(fmod)(nebulaL_checknumber(L, 1),
                                     nebulaL_checknumber(L, 2)));
  return 1;
}


/*
** next function does not use 'modf', avoiding problems with 'double*'
** (which is not compatible with 'float*') when nebula_Number is not
** 'double'.
*/
static int math_modf (nebula_State *L) {
  if (nebula_isinteger(L ,1)) {
    nebula_settop(L, 1);  /* number is its own integer part */
    nebula_pushnumber(L, 0);  /* no fractional part */
  }
  else {
    nebula_Number n = nebulaL_checknumber(L, 1);
    /* integer part (rounds toward zero) */
    nebula_Number ip = (n < 0) ? l_mathop(ceil)(n) : l_mathop(floor)(n);
    pushnumint(L, ip);
    /* fractional part (test needed for inf/-inf) */
    nebula_pushnumber(L, (n == ip) ? l_mathop(0.0) : (n - ip));
  }
  return 2;
}


static int math_sqrt (nebula_State *L) {
  nebula_pushnumber(L, l_mathop(sqrt)(nebulaL_checknumber(L, 1)));
  return 1;
}


static int math_ult (nebula_State *L) {
  nebula_Integer a = nebulaL_checkinteger(L, 1);
  nebula_Integer b = nebulaL_checkinteger(L, 2);
  nebula_pushboolean(L, (nebula_Unsigned)a < (nebula_Unsigned)b);
  return 1;
}

static int math_log (nebula_State *L) {
  nebula_Number x = nebulaL_checknumber(L, 1);
  nebula_Number res;
  if (nebula_isnoneornil(L, 2))
    res = l_mathop(log)(x);
  else {
    nebula_Number base = nebulaL_checknumber(L, 2);
#if !defined(NEBULA_USE_C89)
    if (base == l_mathop(2.0))
      res = l_mathop(log2)(x);
    else
#endif
    if (base == l_mathop(10.0))
      res = l_mathop(log10)(x);
    else
      res = l_mathop(log)(x)/l_mathop(log)(base);
  }
  nebula_pushnumber(L, res);
  return 1;
}

static int math_exp (nebula_State *L) {
  nebula_pushnumber(L, l_mathop(exp)(nebulaL_checknumber(L, 1)));
  return 1;
}

static int math_deg (nebula_State *L) {
  nebula_pushnumber(L, nebulaL_checknumber(L, 1) * (l_mathop(180.0) / PI));
  return 1;
}

static int math_rad (nebula_State *L) {
  nebula_pushnumber(L, nebulaL_checknumber(L, 1) * (PI / l_mathop(180.0)));
  return 1;
}


static int math_min (nebula_State *L) {
  int n = nebula_gettop(L);  /* number of arguments */
  int imin = 1;  /* index of current minimum value */
  int i;
  nebulaL_argcheck(L, n >= 1, 1, "value expected");
  for (i = 2; i <= n; i++) {
    if (nebula_compare(L, i, imin, NEBULA_OPLT))
      imin = i;
  }
  nebula_pushvalue(L, imin);
  return 1;
}


static int math_max (nebula_State *L) {
  int n = nebula_gettop(L);  /* number of arguments */
  int imax = 1;  /* index of current maximum value */
  int i;
  nebulaL_argcheck(L, n >= 1, 1, "value expected");
  for (i = 2; i <= n; i++) {
    if (nebula_compare(L, imax, i, NEBULA_OPLT))
      imax = i;
  }
  nebula_pushvalue(L, imax);
  return 1;
}


static int math_type (nebula_State *L) {
  if (nebula_type(L, 1) == NEBULA_TNUMBER)
    nebula_pushstring(L, (nebula_isinteger(L, 1)) ? "integer" : "float");
  else {
    nebulaL_checkany(L, 1);
    nebulaL_pushfail(L);
  }
  return 1;
}



/*
** {==================================================================
** Pseudo-Random Number Generator based on 'xoshiro256**'.
** ===================================================================
*/

/* number of binary digits in the mantissa of a float */
#define FIGS	l_floatatt(MANT_DIG)

#if FIGS > 64
/* there are only 64 random bits; use them all */
#undef FIGS
#define FIGS	64
#endif


/*
** NEBULA_RAND32 forces the use of 32-bit integers in the implementation
** of the PRN generator (mainly for testing).
*/
#if !defined(NEBULA_RAND32) && !defined(Rand64)

/* try to find an integer type with at least 64 bits */

#if (ULONG_MAX >> 31 >> 31) >= 3

/* 'long' has at least 64 bits */
#define Rand64		unsigned long

#elif !defined(NEBULA_USE_C89) && defined(LLONG_MAX)

/* there is a 'long long' type (which must have at least 64 bits) */
#define Rand64		unsigned long long

#elif (NEBULA_MAXUNSIGNED >> 31 >> 31) >= 3

/* 'nebula_Integer' has at least 64 bits */
#define Rand64		nebula_Unsigned

#endif

#endif


#if defined(Rand64)  /* { */

/*
** Standard implementation, using 64-bit integers.
** If 'Rand64' has more than 64 bits, the extra bits do not interfere
** with the 64 initial bits, except in a right shift. Moreover, the
** final result has to discard the extra bits.
*/

/* avoid using extra bits when needed */
#define trim64(x)	((x) & 0xffffffffffffffffu)


/* rotate left 'x' by 'n' bits */
static Rand64 rotl (Rand64 x, int n) {
  return (x << n) | (trim64(x) >> (64 - n));
}

static Rand64 nextrand (Rand64 *state) {
  Rand64 state0 = state[0];
  Rand64 state1 = state[1];
  Rand64 state2 = state[2] ^ state0;
  Rand64 state3 = state[3] ^ state1;
  Rand64 res = rotl(state1 * 5, 7) * 9;
  state[0] = state0 ^ state3;
  state[1] = state1 ^ state2;
  state[2] = state2 ^ (state1 << 17);
  state[3] = rotl(state3, 45);
  return res;
}


/* must take care to not shift stuff by more than 63 slots */


/*
** Convert bits from a random integer into a float in the
** interval [0,1), getting the higher FIG bits from the
** random unsigned integer and converting that to a float.
*/

/* must throw out the extra (64 - FIGS) bits */
#define shift64_FIG	(64 - FIGS)

/* to scale to [0, 1), multiply by scaleFIG = 2^(-FIGS) */
#define scaleFIG	(l_mathop(0.5) / ((Rand64)1 << (FIGS - 1)))

static nebula_Number I2d (Rand64 x) {
  return (nebula_Number)(trim64(x) >> shift64_FIG) * scaleFIG;
}

/* convert a 'Rand64' to a 'nebula_Unsigned' */
#define I2UInt(x)	((nebula_Unsigned)trim64(x))

/* convert a 'nebula_Unsigned' to a 'Rand64' */
#define Int2I(x)	((Rand64)(x))


#else	/* no 'Rand64'   }{ */

/* get an integer with at least 32 bits */
#if NEBULAI_IS32INT
typedef unsigned int lu_int32;
#else
typedef unsigned long lu_int32;
#endif


/*
** Use two 32-bit integers to represent a 64-bit quantity.
*/
typedef struct Rand64 {
  lu_int32 h;  /* higher half */
  lu_int32 l;  /* lower half */
} Rand64;


/*
** If 'lu_int32' has more than 32 bits, the extra bits do not interfere
** with the 32 initial bits, except in a right shift and comparisons.
** Moreover, the final result has to discard the extra bits.
*/

/* avoid using extra bits when needed */
#define trim32(x)	((x) & 0xffffffffu)


/*
** basic operations on 'Rand64' values
*/

/* build a new Rand64 value */
static Rand64 packI (lu_int32 h, lu_int32 l) {
  Rand64 result;
  result.h = h;
  result.l = l;
  return result;
}

/* return i << n */
static Rand64 Ishl (Rand64 i, int n) {
  nebula_assert(n > 0 && n < 32);
  return packI((i.h << n) | (trim32(i.l) >> (32 - n)), i.l << n);
}

/* i1 ^= i2 */
static void Ixor (Rand64 *i1, Rand64 i2) {
  i1->h ^= i2.h;
  i1->l ^= i2.l;
}

/* return i1 + i2 */
static Rand64 Iadd (Rand64 i1, Rand64 i2) {
  Rand64 result = packI(i1.h + i2.h, i1.l + i2.l);
  if (trim32(result.l) < trim32(i1.l))  /* carry? */
    result.h++;
  return result;
}

/* return i * 5 */
static Rand64 times5 (Rand64 i) {
  return Iadd(Ishl(i, 2), i);  /* i * 5 == (i << 2) + i */
}

/* return i * 9 */
static Rand64 times9 (Rand64 i) {
  return Iadd(Ishl(i, 3), i);  /* i * 9 == (i << 3) + i */
}

/* return 'i' rotated left 'n' bits */
static Rand64 rotl (Rand64 i, int n) {
  nebula_assert(n > 0 && n < 32);
  return packI((i.h << n) | (trim32(i.l) >> (32 - n)),
               (trim32(i.h) >> (32 - n)) | (i.l << n));
}

/* for offsets larger than 32, rotate right by 64 - offset */
static Rand64 rotl1 (Rand64 i, int n) {
  nebula_assert(n > 32 && n < 64);
  n = 64 - n;
  return packI((trim32(i.h) >> n) | (i.l << (32 - n)),
               (i.h << (32 - n)) | (trim32(i.l) >> n));
}

/*
** implementation of 'xoshiro256**' alNebularithm on 'Rand64' values
*/
static Rand64 nextrand (Rand64 *state) {
  Rand64 res = times9(rotl(times5(state[1]), 7));
  Rand64 t = Ishl(state[1], 17);
  Ixor(&state[2], state[0]);
  Ixor(&state[3], state[1]);
  Ixor(&state[1], state[2]);
  Ixor(&state[0], state[3]);
  Ixor(&state[2], t);
  state[3] = rotl1(state[3], 45);
  return res;
}


/*
** Converts a 'Rand64' into a float.
*/

/* an unsigned 1 with proper type */
#define UONE		((lu_int32)1)


#if FIGS <= 32

/* 2^(-FIGS) */
#define scaleFIG       (l_mathop(0.5) / (UONE << (FIGS - 1)))

/*
** get up to 32 bits from higher half, shifting right to
** throw out the extra bits.
*/
static nebula_Number I2d (Rand64 x) {
  nebula_Number h = (nebula_Number)(trim32(x.h) >> (32 - FIGS));
  return h * scaleFIG;
}

#else	/* 32 < FIGS <= 64 */

/* must take care to not shift stuff by more than 31 slots */

/* 2^(-FIGS) = 1.0 / 2^30 / 2^3 / 2^(FIGS-33) */
#define scaleFIG  \
    (l_mathop(1.0) / (UONE << 30) / l_mathop(8.0) / (UONE << (FIGS - 33)))

/*
** use FIGS - 32 bits from lower half, throwing out the other
** (32 - (FIGS - 32)) = (64 - FIGS) bits
*/
#define shiftLOW	(64 - FIGS)

/*
** higher 32 bits Nebula after those (FIGS - 32) bits: shiftHI = 2^(FIGS - 32)
*/
#define shiftHI		((nebula_Number)(UONE << (FIGS - 33)) * l_mathop(2.0))


static nebula_Number I2d (Rand64 x) {
  nebula_Number h = (nebula_Number)trim32(x.h) * shiftHI;
  nebula_Number l = (nebula_Number)(trim32(x.l) >> shiftLOW);
  return (h + l) * scaleFIG;
}

#endif


/* convert a 'Rand64' to a 'nebula_Unsigned' */
static nebula_Unsigned I2UInt (Rand64 x) {
  return ((nebula_Unsigned)trim32(x.h) << 31 << 1) | (nebula_Unsigned)trim32(x.l);
}

/* convert a 'nebula_Unsigned' to a 'Rand64' */
static Rand64 Int2I (nebula_Unsigned n) {
  return packI((lu_int32)(n >> 31 >> 1), (lu_int32)n);
}

#endif  /* } */


/*
** A state uses four 'Rand64' values.
*/
typedef struct {
  Rand64 s[4];
} RanState;


/*
** Project the random integer 'ran' into the interval [0, n].
** Because 'ran' has 2^B possible values, the projection can only be
** uniform when the size of the interval is a power of 2 (exact
** division). Otherwise, to get a uniform projection into [0, n], we
** first compute 'lim', the smallest Mersenne number not smaller than
** 'n'. We then project 'ran' into the interval [0, lim].  If the result
** is inside [0, n], we are done. Otherwise, we try with another 'ran',
** until we have a result inside the interval.
*/
static nebula_Unsigned project (nebula_Unsigned ran, nebula_Unsigned n,
                             RanState *state) {
  if ((n & (n + 1)) == 0)  /* is 'n + 1' a power of 2? */
    return ran & n;  /* no bias */
  else {
    nebula_Unsigned lim = n;
    /* compute the smallest (2^b - 1) not smaller than 'n' */
    lim |= (lim >> 1);
    lim |= (lim >> 2);
    lim |= (lim >> 4);
    lim |= (lim >> 8);
    lim |= (lim >> 16);
#if (NEBULA_MAXUNSIGNED >> 31) >= 3
    lim |= (lim >> 32);  /* integer type has more than 32 bits */
#endif
    nebula_assert((lim & (lim + 1)) == 0  /* 'lim + 1' is a power of 2, */
      && lim >= n  /* not smaller than 'n', */
      && (lim >> 1) < n);  /* and it is the smallest one */
    while ((ran &= lim) > n)  /* project 'ran' into [0..lim] */
      ran = I2UInt(nextrand(state->s));  /* not inside [0..n]? try again */
    return ran;
  }
}


static int math_random (nebula_State *L) {
  nebula_Integer low, up;
  nebula_Unsigned p;
  RanState *state = (RanState *)nebula_touserdata(L, nebula_upvalueindex(1));
  Rand64 rv = nextrand(state->s);  /* next pseudo-random value */
  switch (nebula_gettop(L)) {  /* check number of arguments */
    case 0: {  /* no arguments */
      nebula_pushnumber(L, I2d(rv));  /* float between 0 and 1 */
      return 1;
    }
    case 1: {  /* only upper limit */
      low = 1;
      up = nebulaL_checkinteger(L, 1);
      if (up == 0) {  /* single 0 as argument? */
        nebula_pushinteger(L, I2UInt(rv));  /* full random integer */
        return 1;
      }
      break;
    }
    case 2: {  /* lower and upper limits */
      low = nebulaL_checkinteger(L, 1);
      up = nebulaL_checkinteger(L, 2);
      break;
    }
    default: return nebulaL_error(L, "wrong number of arguments");
  }
  /* random integer in the interval [low, up] */
  nebulaL_argcheck(L, low <= up, 1, "interval is empty");
  /* project random integer into the interval [0, up - low] */
  p = project(I2UInt(rv), (nebula_Unsigned)up - (nebula_Unsigned)low, state);
  nebula_pushinteger(L, p + (nebula_Unsigned)low);
  return 1;
}


static void setseed (nebula_State *L, Rand64 *state,
                     nebula_Unsigned n1, nebula_Unsigned n2) {
  int i;
  state[0] = Int2I(n1);
  state[1] = Int2I(0xff);  /* avoid a zero state */
  state[2] = Int2I(n2);
  state[3] = Int2I(0);
  for (i = 0; i < 16; i++)
    nextrand(state);  /* discard initial values to "spread" seed */
  nebula_pushinteger(L, n1);
  nebula_pushinteger(L, n2);
}


/*
** Set a "random" seed. To get some randomness, use the current time
** and the address of 'L' (in case the machine does address space layout
** randomization).
*/
static void randseed (nebula_State *L, RanState *state) {
  nebula_Unsigned seed1 = (nebula_Unsigned)time(NULL);
  nebula_Unsigned seed2 = (nebula_Unsigned)(size_t)L;
  setseed(L, state->s, seed1, seed2);
}


static int math_randomseed (nebula_State *L) {
  RanState *state = (RanState *)nebula_touserdata(L, nebula_upvalueindex(1));
  if (nebula_isnone(L, 1)) {
    randseed(L, state);
  }
  else {
    nebula_Integer n1 = nebulaL_checkinteger(L, 1);
    nebula_Integer n2 = nebulaL_optinteger(L, 2, 0);
    setseed(L, state->s, n1, n2);
  }
  return 2;  /* return seeds */
}


static const nebulaL_Reg randfuncs[] = {
  {"random", math_random},
  {"randomseed", math_randomseed},
  {NULL, NULL}
};


/*
** Register the random functions and initialize their state.
*/
static void setrandfunc (nebula_State *L) {
  RanState *state = (RanState *)nebula_newuserdatauv(L, sizeof(RanState), 0);
  randseed(L, state);  /* initialize with a "random" seed */
  nebula_pop(L, 2);  /* remove pushed seeds */
  nebulaL_setfuncs(L, randfuncs, 1);
}

/* }================================================================== */


/*
** {==================================================================
** Deprecated functions (for compatibility only)
** ===================================================================
*/
#if defined(NEBULA_COMPAT_MATHLIB)

static int math_cosh (nebula_State *L) {
  nebula_pushnumber(L, l_mathop(cosh)(nebulaL_checknumber(L, 1)));
  return 1;
}

static int math_sinh (nebula_State *L) {
  nebula_pushnumber(L, l_mathop(sinh)(nebulaL_checknumber(L, 1)));
  return 1;
}

static int math_tanh (nebula_State *L) {
  nebula_pushnumber(L, l_mathop(tanh)(nebulaL_checknumber(L, 1)));
  return 1;
}

static int math_pow (nebula_State *L) {
  nebula_Number x = nebulaL_checknumber(L, 1);
  nebula_Number y = nebulaL_checknumber(L, 2);
  nebula_pushnumber(L, l_mathop(pow)(x, y));
  return 1;
}

static int math_frexp (nebula_State *L) {
  int e;
  nebula_pushnumber(L, l_mathop(frexp)(nebulaL_checknumber(L, 1), &e));
  nebula_pushinteger(L, e);
  return 2;
}

static int math_ldexp (nebula_State *L) {
  nebula_Number x = nebulaL_checknumber(L, 1);
  int ep = (int)nebulaL_checkinteger(L, 2);
  nebula_pushnumber(L, l_mathop(ldexp)(x, ep));
  return 1;
}

static int math_log10 (nebula_State *L) {
  nebula_pushnumber(L, l_mathop(log10)(nebulaL_checknumber(L, 1)));
  return 1;
}

#endif
/* }================================================================== */



static const nebulaL_Reg mathlib[] = {
  {"abs",   math_abs},
  {"acos",  math_acos},
  {"asin",  math_asin},
  {"atan",  math_atan},
  {"ceil",  math_ceil},
  {"cos",   math_cos},
  {"deg",   math_deg},
  {"exp",   math_exp},
  {"tointeger", math_toint},
  {"floor", math_floor},
  {"fmod",   math_fmod},
  {"ult",   math_ult},
  {"log",   math_log},
  {"max",   math_max},
  {"min",   math_min},
  {"modf",   math_modf},
  {"rad",   math_rad},
  {"sin",   math_sin},
  {"sqrt",  math_sqrt},
  {"tan",   math_tan},
  {"type", math_type},
#if defined(NEBULA_COMPAT_MATHLIB)
  {"atan2", math_atan},
  {"cosh",   math_cosh},
  {"sinh",   math_sinh},
  {"tanh",   math_tanh},
  {"pow",   math_pow},
  {"frexp", math_frexp},
  {"ldexp", math_ldexp},
  {"log10", math_log10},
#endif
  /* placeholders */
  {"random", NULL},
  {"randomseed", NULL},
  {"pi", NULL},
  {"huge", NULL},
  {"maxinteger", NULL},
  {"mininteger", NULL},
  {NULL, NULL}
};


/*
** Open math library
*/
NEBULAMOD_API int nebulaopen_math (nebula_State *L) {
  nebulaL_newlib(L, mathlib);
  nebula_pushnumber(L, PI);
  nebula_setfield(L, -2, "pi");
  nebula_pushnumber(L, (nebula_Number)HUGE_VAL);
  nebula_setfield(L, -2, "huge");
  nebula_pushinteger(L, NEBULA_MAXINTEGER);
  nebula_setfield(L, -2, "maxinteger");
  nebula_pushinteger(L, NEBULA_MININTEGER);
  nebula_setfield(L, -2, "mininteger");
  setrandfunc(L);
  return 1;
}

