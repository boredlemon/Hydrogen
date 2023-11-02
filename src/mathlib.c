/*
** $Id: mathlib.c $
** Standard mathematical library
** See Copyright Notice in venom.h
*/

#define mathlib_c
#define VENOM_LIB

#include "prefix.h"


#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "venom.h"

#include "auxlib.h"
#include "venomlib.h"


#undef PI
#define PI	(l_mathop(3.141592653589793238462643383279502884))


static int math_abs (venom_State *L) {
  if (venom_isinteger(L, 1)) {
    venom_Integer n = venom_tointeger(L, 1);
    if (n < 0) n = (venom_Integer)(0u - (venom_Unsigned)n);
    venom_pushinteger(L, n);
  }
  else
    venom_pushnumber(L, l_mathop(fabs)(venomL_checknumber(L, 1)));
  return 1;
}

static int math_sin (venom_State *L) {
  venom_pushnumber(L, l_mathop(sin)(venomL_checknumber(L, 1)));
  return 1;
}

static int math_cos (venom_State *L) {
  venom_pushnumber(L, l_mathop(cos)(venomL_checknumber(L, 1)));
  return 1;
}

static int math_tan (venom_State *L) {
  venom_pushnumber(L, l_mathop(tan)(venomL_checknumber(L, 1)));
  return 1;
}

static int math_asin (venom_State *L) {
  venom_pushnumber(L, l_mathop(asin)(venomL_checknumber(L, 1)));
  return 1;
}

static int math_acos (venom_State *L) {
  venom_pushnumber(L, l_mathop(acos)(venomL_checknumber(L, 1)));
  return 1;
}

static int math_atan (venom_State *L) {
  venom_Number y = venomL_checknumber(L, 1);
  venom_Number x = venomL_optnumber(L, 2, 1);
  venom_pushnumber(L, l_mathop(atan2)(y, x));
  return 1;
}


static int math_toint (venom_State *L) {
  int valid;
  venom_Integer n = venom_tointegerx(L, 1, &valid);
  if (l_likely(valid))
    venom_pushinteger(L, n);
  else {
    venomL_checkany(L, 1);
    venomL_pushfail(L);  /* value is not convertible to integer */
  }
  return 1;
}


static void pushnumint (venom_State *L, venom_Number d) {
  venom_Integer n;
  if (venom_numbertointeger(d, &n))  /* does 'd' fit in an integer? */
    venom_pushinteger(L, n);  /* result is integer */
  else
    venom_pushnumber(L, d);  /* result is float */
}


static int math_floor (venom_State *L) {
  if (venom_isinteger(L, 1))
    venom_settop(L, 1);  /* integer is its own floor */
  else {
    venom_Number d = l_mathop(floor)(venomL_checknumber(L, 1));
    pushnumint(L, d);
  }
  return 1;
}


static int math_ceil (venom_State *L) {
  if (venom_isinteger(L, 1))
    venom_settop(L, 1);  /* integer is its own ceil */
  else {
    venom_Number d = l_mathop(ceil)(venomL_checknumber(L, 1));
    pushnumint(L, d);
  }
  return 1;
}


static int math_fmod (venom_State *L) {
  if (venom_isinteger(L, 1) && venom_isinteger(L, 2)) {
    venom_Integer d = venom_tointeger(L, 2);
    if ((venom_Unsigned)d + 1u <= 1u) {  /* special cases: -1 or 0 */
      venomL_argcheck(L, d != 0, 2, "zero");
      venom_pushinteger(L, 0);  /* avoid overflow with 0x80000... / -1 */
    }
    else
      venom_pushinteger(L, venom_tointeger(L, 1) % d);
  }
  else
    venom_pushnumber(L, l_mathop(fmod)(venomL_checknumber(L, 1),
                                     venomL_checknumber(L, 2)));
  return 1;
}


/*
** next function does not use 'modf', avoiding problems with 'double*'
** (which is not compatible with 'float*') when venom_Number is not
** 'double'.
*/
static int math_modf (venom_State *L) {
  if (venom_isinteger(L ,1)) {
    venom_settop(L, 1);  /* number is its own integer part */
    venom_pushnumber(L, 0);  /* no fractional part */
  }
  else {
    venom_Number n = venomL_checknumber(L, 1);
    /* integer part (rounds toward zero) */
    venom_Number ip = (n < 0) ? l_mathop(ceil)(n) : l_mathop(floor)(n);
    pushnumint(L, ip);
    /* fractional part (test needed for inf/-inf) */
    venom_pushnumber(L, (n == ip) ? l_mathop(0.0) : (n - ip));
  }
  return 2;
}


static int math_sqrt (venom_State *L) {
  venom_pushnumber(L, l_mathop(sqrt)(venomL_checknumber(L, 1)));
  return 1;
}


static int math_ult (venom_State *L) {
  venom_Integer a = venomL_checkinteger(L, 1);
  venom_Integer b = venomL_checkinteger(L, 2);
  venom_pushboolean(L, (venom_Unsigned)a < (venom_Unsigned)b);
  return 1;
}

static int math_log (venom_State *L) {
  venom_Number x = venomL_checknumber(L, 1);
  venom_Number res;
  if (venom_isnoneornil(L, 2))
    res = l_mathop(log)(x);
  else {
    venom_Number base = venomL_checknumber(L, 2);
#if !defined(VENOM_USE_C89)
    if (base == l_mathop(2.0))
      res = l_mathop(log2)(x);
    else
#endif
    if (base == l_mathop(10.0))
      res = l_mathop(log10)(x);
    else
      res = l_mathop(log)(x)/l_mathop(log)(base);
  }
  venom_pushnumber(L, res);
  return 1;
}

static int math_exp (venom_State *L) {
  venom_pushnumber(L, l_mathop(exp)(venomL_checknumber(L, 1)));
  return 1;
}

static int math_deg (venom_State *L) {
  venom_pushnumber(L, venomL_checknumber(L, 1) * (l_mathop(180.0) / PI));
  return 1;
}

static int math_rad (venom_State *L) {
  venom_pushnumber(L, venomL_checknumber(L, 1) * (PI / l_mathop(180.0)));
  return 1;
}


static int math_min (venom_State *L) {
  int n = venom_gettop(L);  /* number of arguments */
  int imin = 1;  /* index of current minimum value */
  int i;
  venomL_argcheck(L, n >= 1, 1, "value expected");
  for (i = 2; i <= n; i++) {
    if (venom_compare(L, i, imin, VENOM_OPLT))
      imin = i;
  }
  venom_pushvalue(L, imin);
  return 1;
}


static int math_max (venom_State *L) {
  int n = venom_gettop(L);  /* number of arguments */
  int imax = 1;  /* index of current maximum value */
  int i;
  venomL_argcheck(L, n >= 1, 1, "value expected");
  for (i = 2; i <= n; i++) {
    if (venom_compare(L, imax, i, VENOM_OPLT))
      imax = i;
  }
  venom_pushvalue(L, imax);
  return 1;
}


static int math_type (venom_State *L) {
  if (venom_type(L, 1) == VENOM_TNUMBER)
    venom_pushstring(L, (venom_isinteger(L, 1)) ? "integer" : "float");
  else {
    venomL_checkany(L, 1);
    venomL_pushfail(L);
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
** VENOM_RAND32 forces the use of 32-bit integers in the implementation
** of the PRN generator (mainly for testing).
*/
#if !defined(VENOM_RAND32) && !defined(Rand64)

/* try to find an integer type with at least 64 bits */

#if (ULONG_MAX >> 31 >> 31) >= 3

/* 'long' has at least 64 bits */
#define Rand64		unsigned long

#elif !defined(VENOM_USE_C89) && defined(LLONG_MAX)

/* there is a 'long long' type (which must have at least 64 bits) */
#define Rand64		unsigned long long

#elif (VENOM_MAXUNSIGNED >> 31 >> 31) >= 3

/* 'venom_Integer' has at least 64 bits */
#define Rand64		venom_Unsigned

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

static venom_Number I2d (Rand64 x) {
  return (venom_Number)(trim64(x) >> shift64_FIG) * scaleFIG;
}

/* convert a 'Rand64' to a 'venom_Unsigned' */
#define I2UInt(x)	((venom_Unsigned)trim64(x))

/* convert a 'venom_Unsigned' to a 'Rand64' */
#define Int2I(x)	((Rand64)(x))


#else	/* no 'Rand64'   }{ */

/* get an integer with at least 32 bits */
#if VENOMI_IS32INT
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
  venom_assert(n > 0 && n < 32);
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
  venom_assert(n > 0 && n < 32);
  return packI((i.h << n) | (trim32(i.l) >> (32 - n)),
               (trim32(i.h) >> (32 - n)) | (i.l << n));
}

/* for offsets larger than 32, rotate right by 64 - offset */
static Rand64 rotl1 (Rand64 i, int n) {
  venom_assert(n > 32 && n < 64);
  n = 64 - n;
  return packI((trim32(i.h) >> n) | (i.l << (32 - n)),
               (i.h << (32 - n)) | (trim32(i.l) >> n));
}

/*
** implementation of 'xoshiro256**' alVenomrithm on 'Rand64' values
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
static venom_Number I2d (Rand64 x) {
  venom_Number h = (venom_Number)(trim32(x.h) >> (32 - FIGS));
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
** higher 32 bits Venom after those (FIGS - 32) bits: shiftHI = 2^(FIGS - 32)
*/
#define shiftHI		((venom_Number)(UONE << (FIGS - 33)) * l_mathop(2.0))


static venom_Number I2d (Rand64 x) {
  venom_Number h = (venom_Number)trim32(x.h) * shiftHI;
  venom_Number l = (venom_Number)(trim32(x.l) >> shiftLOW);
  return (h + l) * scaleFIG;
}

#endif


/* convert a 'Rand64' to a 'venom_Unsigned' */
static venom_Unsigned I2UInt (Rand64 x) {
  return ((venom_Unsigned)trim32(x.h) << 31 << 1) | (venom_Unsigned)trim32(x.l);
}

/* convert a 'venom_Unsigned' to a 'Rand64' */
static Rand64 Int2I (venom_Unsigned n) {
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
static venom_Unsigned project (venom_Unsigned ran, venom_Unsigned n,
                             RanState *state) {
  if ((n & (n + 1)) == 0)  /* is 'n + 1' a power of 2? */
    return ran & n;  /* no bias */
  else {
    venom_Unsigned lim = n;
    /* compute the smallest (2^b - 1) not smaller than 'n' */
    lim |= (lim >> 1);
    lim |= (lim >> 2);
    lim |= (lim >> 4);
    lim |= (lim >> 8);
    lim |= (lim >> 16);
#if (VENOM_MAXUNSIGNED >> 31) >= 3
    lim |= (lim >> 32);  /* integer type has more than 32 bits */
#endif
    venom_assert((lim & (lim + 1)) == 0  /* 'lim + 1' is a power of 2, */
      && lim >= n  /* not smaller than 'n', */
      && (lim >> 1) < n);  /* and it is the smallest one */
    while ((ran &= lim) > n)  /* project 'ran' into [0..lim] */
      ran = I2UInt(nextrand(state->s));  /* not inside [0..n]? try again */
    return ran;
  }
}


static int math_random (venom_State *L) {
  venom_Integer low, up;
  venom_Unsigned p;
  RanState *state = (RanState *)venom_touserdata(L, venom_upvalueindex(1));
  Rand64 rv = nextrand(state->s);  /* next pseudo-random value */
  switch (venom_gettop(L)) {  /* check number of arguments */
    case 0: {  /* no arguments */
      venom_pushnumber(L, I2d(rv));  /* float between 0 and 1 */
      return 1;
    }
    case 1: {  /* only upper limit */
      low = 1;
      up = venomL_checkinteger(L, 1);
      if (up == 0) {  /* single 0 as argument? */
        venom_pushinteger(L, I2UInt(rv));  /* full random integer */
        return 1;
      }
      break;
    }
    case 2: {  /* lower and upper limits */
      low = venomL_checkinteger(L, 1);
      up = venomL_checkinteger(L, 2);
      break;
    }
    default: return venomL_error(L, "wrong number of arguments");
  }
  /* random integer in the interval [low, up] */
  venomL_argcheck(L, low <= up, 1, "interval is empty");
  /* project random integer into the interval [0, up - low] */
  p = project(I2UInt(rv), (venom_Unsigned)up - (venom_Unsigned)low, state);
  venom_pushinteger(L, p + (venom_Unsigned)low);
  return 1;
}


static void setseed (venom_State *L, Rand64 *state,
                     venom_Unsigned n1, venom_Unsigned n2) {
  int i;
  state[0] = Int2I(n1);
  state[1] = Int2I(0xff);  /* avoid a zero state */
  state[2] = Int2I(n2);
  state[3] = Int2I(0);
  for (i = 0; i < 16; i++)
    nextrand(state);  /* discard initial values to "spread" seed */
  venom_pushinteger(L, n1);
  venom_pushinteger(L, n2);
}


/*
** Set a "random" seed. To get some randomness, use the current time
** and the address of 'L' (in case the machine does address space layout
** randomization).
*/
static void randseed (venom_State *L, RanState *state) {
  venom_Unsigned seed1 = (venom_Unsigned)time(NULL);
  venom_Unsigned seed2 = (venom_Unsigned)(size_t)L;
  setseed(L, state->s, seed1, seed2);
}


static int math_randomseed (venom_State *L) {
  RanState *state = (RanState *)venom_touserdata(L, venom_upvalueindex(1));
  if (venom_isnone(L, 1)) {
    randseed(L, state);
  }
  else {
    venom_Integer n1 = venomL_checkinteger(L, 1);
    venom_Integer n2 = venomL_optinteger(L, 2, 0);
    setseed(L, state->s, n1, n2);
  }
  return 2;  /* return seeds */
}


static const venomL_Reg randfuncs[] = {
  {"random", math_random},
  {"randomseed", math_randomseed},
  {NULL, NULL}
};


/*
** Register the random functions and initialize their state.
*/
static void setrandfunc (venom_State *L) {
  RanState *state = (RanState *)venom_newuserdatauv(L, sizeof(RanState), 0);
  randseed(L, state);  /* initialize with a "random" seed */
  venom_pop(L, 2);  /* remove pushed seeds */
  venomL_setfuncs(L, randfuncs, 1);
}

/* }================================================================== */


/*
** {==================================================================
** Deprecated functions (for compatibility only)
** ===================================================================
*/
#if defined(VENOM_COMPAT_MATHLIB)

static int math_cosh (venom_State *L) {
  venom_pushnumber(L, l_mathop(cosh)(venomL_checknumber(L, 1)));
  return 1;
}

static int math_sinh (venom_State *L) {
  venom_pushnumber(L, l_mathop(sinh)(venomL_checknumber(L, 1)));
  return 1;
}

static int math_tanh (venom_State *L) {
  venom_pushnumber(L, l_mathop(tanh)(venomL_checknumber(L, 1)));
  return 1;
}

static int math_pow (venom_State *L) {
  venom_Number x = venomL_checknumber(L, 1);
  venom_Number y = venomL_checknumber(L, 2);
  venom_pushnumber(L, l_mathop(pow)(x, y));
  return 1;
}

static int math_frexp (venom_State *L) {
  int e;
  venom_pushnumber(L, l_mathop(frexp)(venomL_checknumber(L, 1), &e));
  venom_pushinteger(L, e);
  return 2;
}

static int math_ldexp (venom_State *L) {
  venom_Number x = venomL_checknumber(L, 1);
  int ep = (int)venomL_checkinteger(L, 2);
  venom_pushnumber(L, l_mathop(ldexp)(x, ep));
  return 1;
}

static int math_log10 (venom_State *L) {
  venom_pushnumber(L, l_mathop(log10)(venomL_checknumber(L, 1)));
  return 1;
}

#endif
/* }================================================================== */



static const venomL_Reg mathlib[] = {
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
#if defined(VENOM_COMPAT_MATHLIB)
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
VENOMMOD_API int venomopen_math (venom_State *L) {
  venomL_newlib(L, mathlib);
  venom_pushnumber(L, PI);
  venom_setfield(L, -2, "pi");
  venom_pushnumber(L, (venom_Number)HUGE_VAL);
  venom_setfield(L, -2, "huge");
  venom_pushinteger(L, VENOM_MAXINTEGER);
  venom_setfield(L, -2, "maxinteger");
  venom_pushinteger(L, VENOM_MININTEGER);
  venom_setfield(L, -2, "mininteger");
  setrandfunc(L);
  return 1;
}

