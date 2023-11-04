/*
** $Id: tablib.c $
** Library for Table Manipulation
** See Copyright Notice in hydrogen.h
*/

#define tablib_c
#define HYDROGEN_LIB

#include "prefix.h"


#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "hydrogen.h"

#include "auxlib.h"
#include "hydrogenlib.h"


/*
** Operations that an object must define to mimic a table
** (some functions only need some of them)
*/
#define TAB_R	1			/* read */
#define TAB_W	2			/* write */
#define TAB_L	4			/* length */
#define TAB_RW	(TAB_R | TAB_W)		/* read/write */


#define aux_getn(L,n,w)	(checktab(L, n, (w) | TAB_L), hydrogenL_len(L, n))


static int checkfield (hydrogen_State *L, const char *key, int n) {
  hydrogen_pushstring(L, key);
  return (hydrogen_rawget(L, -n) != HYDROGEN_TNIL);
}


/*
** Check that 'arg' either is a table or can behave like one (that is,
** has a metatable with the required metamethods)
*/
static void checktab (hydrogen_State *L, int arg, int what) {
  if (hydrogen_type(L, arg) != HYDROGEN_TTABLE) {  /* is it not a table? */
    int n = 1;  /* number of elements to pop */
    if (hydrogen_getmetatable(L, arg) &&  /* must have metatable */
        (!(what & TAB_R) || checkfield(L, "__index", ++n)) &&
        (!(what & TAB_W) || checkfield(L, "__newindex", ++n)) &&
        (!(what & TAB_L) || checkfield(L, "__len", ++n))) {
      hydrogen_pop(L, n);  /* pop metatable and tested metamethods */
    }
    else
      hydrogenL_checktype(L, arg, HYDROGEN_TTABLE);  /* force an error */
  }
}


static int tinsert (hydrogen_State *L) {
  hydrogen_Integer pos;  /* where to insert new element */
  hydrogen_Integer e = aux_getn(L, 1, TAB_RW);
  e = hydrogenL_intop(+, e, 1);  /* first empty element */
  switch (hydrogen_gettop(L)) {
    case 2: {  /* called with only 2 arguments */
      pos = e;  /* insert new element at the end */
      break;
    }
    case 3: {
      hydrogen_Integer i;
      pos = hydrogenL_checkinteger(L, 2);  /* 2nd argument is the position */
      /* check whether 'pos' is in [1, e] */
      hydrogenL_argcheck(L, (hydrogen_Unsigned)pos - 1u < (hydrogen_Unsigned)e, 2,
                       "position out of bounds");
      for (i = e; i > pos; i--) {  /* move up elements */
        hydrogen_geti(L, 1, i - 1);
        hydrogen_seti(L, 1, i);  /* t[i] = t[i - 1] */
      }
      break;
    }
    default: {
      return hydrogenL_error(L, "wrong number of arguments to 'insert'");
    }
  }
  hydrogen_seti(L, 1, pos);  /* t[pos] = v */
  return 0;
}


static int tremove (hydrogen_State *L) {
  hydrogen_Integer size = aux_getn(L, 1, TAB_RW);
  hydrogen_Integer pos = hydrogenL_optinteger(L, 2, size);
  if (pos != size)  /* validate 'pos' if given */
    /* check whether 'pos' is in [1, size + 1] */
    hydrogenL_argcheck(L, (hydrogen_Unsigned)pos - 1u <= (hydrogen_Unsigned)size, 1,
                     "position out of bounds");
  hydrogen_geti(L, 1, pos);  /* result = t[pos] */
  for ( ; pos < size; pos++) {
    hydrogen_geti(L, 1, pos + 1);
    hydrogen_seti(L, 1, pos);  /* t[pos] = t[pos + 1] */
  }
  hydrogen_pushnil(L);
  hydrogen_seti(L, 1, pos);  /* remove entry t[pos] */
  return 1;
}


/*
** Copy elements (1[f], ..., 1[e]) into (tt[t], tt[t+1], ...). Whenever
** possible, copy in increasing order, which is better for rehashing.
** "possible" means destination after original range, or smaller
** than origin, or copying to another table.
*/
static int tmove (hydrogen_State *L) {
  hydrogen_Integer f = hydrogenL_checkinteger(L, 2);
  hydrogen_Integer e = hydrogenL_checkinteger(L, 3);
  hydrogen_Integer t = hydrogenL_checkinteger(L, 4);
  int tt = !hydrogen_isnoneornil(L, 5) ? 5 : 1;  /* destination table */
  checktab(L, 1, TAB_R);
  checktab(L, tt, TAB_W);
  if (e >= f) {  /* otherwise, nothing to move */
    hydrogen_Integer n, i;
    hydrogenL_argcheck(L, f > 0 || e < HYDROGEN_MAXINTEGER + f, 3,
                  "too many elements to move");
    n = e - f + 1;  /* number of elements to move */
    hydrogenL_argcheck(L, t <= HYDROGEN_MAXINTEGER - n + 1, 4,
                  "destination wrap around");
    if (t > e || t <= f || (tt != 1 && !hydrogen_compare(L, 1, tt, HYDROGEN_OPEQ))) {
      for (i = 0; i < n; i++) {
        hydrogen_geti(L, 1, f + i);
        hydrogen_seti(L, tt, t + i);
      }
    }
    else {
      for (i = n - 1; i >= 0; i--) {
        hydrogen_geti(L, 1, f + i);
        hydrogen_seti(L, tt, t + i);
      }
    }
  }
  hydrogen_pushvalue(L, tt);  /* return destination table */
  return 1;
}


static void addfield (hydrogen_State *L, hydrogenL_Buffer *b, hydrogen_Integer i) {
  hydrogen_geti(L, 1, i);
  if (l_unlikely(!hydrogen_isstring(L, -1)))
    hydrogenL_error(L, "invalid value (%s) at index %I in table for 'concat'",
                  hydrogenL_typename(L, -1), (HYDROGENI_UACINT)i);
  hydrogenL_addvalue(b);
}


static int tconcat (hydrogen_State *L) {
  hydrogenL_Buffer b;
  hydrogen_Integer last = aux_getn(L, 1, TAB_R);
  size_t lsep;
  const char *sep = hydrogenL_optlstring(L, 2, "", &lsep);
  hydrogen_Integer i = hydrogenL_optinteger(L, 3, 1);
  last = hydrogenL_optinteger(L, 4, last);
  hydrogenL_buffinit(L, &b);
  for (; i < last; i++) {
    addfield(L, &b, i);
    hydrogenL_addlstring(&b, sep, lsep);
  }
  if (i == last)  /* add last value (if interval was not empty) */
    addfield(L, &b, i);
  hydrogenL_pushresult(&b);
  return 1;
}


/*
** {======================================================
** Pack/unpack
** =======================================================
*/

static int tpack (hydrogen_State *L) {
  int i;
  int n = hydrogen_gettop(L);  /* number of elements to pack */
  hydrogen_createtable(L, n, 1);  /* create result table */
  hydrogen_insert(L, 1);  /* put it at index 1 */
  for (i = n; i >= 1; i--)  /* assign elements */
    hydrogen_seti(L, 1, i);
  hydrogen_pushinteger(L, n);
  hydrogen_setfield(L, 1, "n");  /* t.n = number of elements */
  return 1;  /* return table */
}


static int tunpack (hydrogen_State *L) {
  hydrogen_Unsigned n;
  hydrogen_Integer i = hydrogenL_optinteger(L, 2, 1);
  hydrogen_Integer e = hydrogenL_opt(L, hydrogenL_checkinteger, 3, hydrogenL_len(L, 1));
  if (i > e) return 0;  /* empty range */
  n = (hydrogen_Unsigned)e - i;  /* number of elements minus 1 (avoid overflows) */
  if (l_unlikely(n >= (unsigned int)INT_MAX  ||
                 !hydrogen_checkstack(L, (int)(++n))))
    return hydrogenL_error(L, "too many results to unpack");
  for (; i < e; i++) {  /* push arg[i..e - 1] (to avoid overflows) */
    hydrogen_geti(L, 1, i);
  }
  hydrogen_geti(L, 1, e);  /* push last element */
  return (int)n;
}

/* }====================================================== */



/*
** {======================================================
** Quicksort
** (based on 'AlHydrogenrithms in MODULA-3', Robert Sedgewick;
**  Addison-Wesley, 1993.)
** =======================================================
*/


/* type for array indices */
typedef unsigned int IdxT;


/*
** Produce a "random" 'unsigned int' to randomize pivot choice. This
** macro is used only when 'sort' detects a big imbalance in the result
** of a partition. (If you don't want/need this "randomness", ~0 is a
** Hydrogenod choice.)
*/
#if !defined(l_randomizePivot)		/* { */

#include <time.h>

/* size of 'e' measured in number of 'unsigned int's */
#define sof(e)		(sizeof(e) / sizeof(unsigned int))

/*
** Use 'time' and 'clock' as sources of "randomness". Because we don't
** know the types 'clock_t' and 'time_t', we cannot cast them to
** anything without risking overflows. A safe way to use their values
** is to copy them to an array of a known type and use the array values.
*/
static unsigned int l_randomizePivot (void) {
  clock_t c = clock();
  time_t t = time(NULL);
  unsigned int buff[sof(c) + sof(t)];
  unsigned int i, rnd = 0;
  memcpy(buff, &c, sof(c) * sizeof(unsigned int));
  memcpy(buff + sof(c), &t, sof(t) * sizeof(unsigned int));
  for (i = 0; i < sof(buff); i++)
    rnd += buff[i];
  return rnd;
}

#endif					/* } */


/* arrays larger than 'RANLIMIT' may use randomized pivots */
#define RANLIMIT	100u


static void set2 (hydrogen_State *L, IdxT i, IdxT j) {
  hydrogen_seti(L, 1, i);
  hydrogen_seti(L, 1, j);
}


/*
** Return true iff value at stack index 'a' is less than the value at
** index 'b' (according to the order of the sort).
*/
static int sort_comp (hydrogen_State *L, int a, int b) {
  if (hydrogen_isnil(L, 2))  /* no function? */
    return hydrogen_compare(L, a, b, HYDROGEN_OPLT);  /* a < b */
  else {  /* function */
    int res;
    hydrogen_pushvalue(L, 2);    /* push function */
    hydrogen_pushvalue(L, a-1);  /* -1 to compensate function */
    hydrogen_pushvalue(L, b-2);  /* -2 to compensate function and 'a' */
    hydrogen_call(L, 2, 1);      /* call function */
    res = hydrogen_toboolean(L, -1);  /* get result */
    hydrogen_pop(L, 1);          /* pop result */
    return res;
  }
}


/*
** Does the partition: Pivot P is at the top of the stack.
** precondition: a[lo] <= P == a[up-1] <= a[up],
** so it only needs to do the partition from lo + 1 to up - 2.
** Pos-condition: a[lo .. i - 1] <= a[i] == P <= a[i + 1 .. up]
** returns 'i'.
*/
static IdxT partition (hydrogen_State *L, IdxT lo, IdxT up) {
  IdxT i = lo;  /* will be incremented before first use */
  IdxT j = up - 1;  /* will be decremented before first use */
  /* loop invariant: a[lo .. i] <= P <= a[j .. up] */
  for (;;) {
    /* next loop: repeat ++i while a[i] < P */
    while ((void)hydrogen_geti(L, 1, ++i), sort_comp(L, -1, -2)) {
      if (l_unlikely(i == up - 1))  /* a[i] < P  but a[up - 1] == P  ?? */
        hydrogenL_error(L, "invalid order function for sorting");
      hydrogen_pop(L, 1);  /* remove a[i] */
    }
    /* after the loop, a[i] >= P and a[lo .. i - 1] < P */
    /* next loop: repeat --j while P < a[j] */
    while ((void)hydrogen_geti(L, 1, --j), sort_comp(L, -3, -1)) {
      if (l_unlikely(j < i))  /* j < i  but  a[j] > P ?? */
        hydrogenL_error(L, "invalid order function for sorting");
      hydrogen_pop(L, 1);  /* remove a[j] */
    }
    /* after the loop, a[j] <= P and a[j + 1 .. up] >= P */
    if (j < i) {  /* no elements out of place? */
      /* a[lo .. i - 1] <= P <= a[j + 1 .. i .. up] */
      hydrogen_pop(L, 1);  /* pop a[j] */
      /* swap pivot (a[up - 1]) with a[i] to satisfy pos-condition */
      set2(L, up - 1, i);
      return i;
    }
    /* otherwise, swap a[i] - a[j] to restore invariant and repeat */
    set2(L, i, j);
  }
}


/*
** Choose an element in the middle (2nd-3th quarters) of [lo,up]
** "randomized" by 'rnd'
*/
static IdxT choosePivot (IdxT lo, IdxT up, unsigned int rnd) {
  IdxT r4 = (up - lo) / 4;  /* range/4 */
  IdxT p = rnd % (r4 * 2) + (lo + r4);
  hydrogen_assert(lo + r4 <= p && p <= up - r4);
  return p;
}


/*
** Quicksort alHydrogenrithm (recursive function)
*/
static void auxsort (hydrogen_State *L, IdxT lo, IdxT up,
                                   unsigned int rnd) {
  while (lo < up) {  /* loop for tail recursion */
    IdxT p;  /* Pivot index */
    IdxT n;  /* to be used later */
    /* sort elements 'lo', 'p', and 'up' */
    hydrogen_geti(L, 1, lo);
    hydrogen_geti(L, 1, up);
    if (sort_comp(L, -1, -2))  /* a[up] < a[lo]? */
      set2(L, lo, up);  /* swap a[lo] - a[up] */
    else
      hydrogen_pop(L, 2);  /* remove both values */
    if (up - lo == 1)  /* only 2 elements? */
      return;  /* already sorted */
    if (up - lo < RANLIMIT || rnd == 0)  /* small interval or no randomize? */
      p = (lo + up)/2;  /* middle element is a Hydrogenod pivot */
    else  /* for larger intervals, it is worth a random pivot */
      p = choosePivot(lo, up, rnd);
    hydrogen_geti(L, 1, p);
    hydrogen_geti(L, 1, lo);
    if (sort_comp(L, -2, -1))  /* a[p] < a[lo]? */
      set2(L, p, lo);  /* swap a[p] - a[lo] */
    else {
      hydrogen_pop(L, 1);  /* remove a[lo] */
      hydrogen_geti(L, 1, up);
      if (sort_comp(L, -1, -2))  /* a[up] < a[p]? */
        set2(L, p, up);  /* swap a[up] - a[p] */
      else
        hydrogen_pop(L, 2);
    }
    if (up - lo == 2)  /* only 3 elements? */
      return;  /* already sorted */
    hydrogen_geti(L, 1, p);  /* get middle element (Pivot) */
    hydrogen_pushvalue(L, -1);  /* push Pivot */
    hydrogen_geti(L, 1, up - 1);  /* push a[up - 1] */
    set2(L, p, up - 1);  /* swap Pivot (a[p]) with a[up - 1] */
    p = partition(L, lo, up);
    /* a[lo .. p - 1] <= a[p] == P <= a[p + 1 .. up] */
    if (p - lo < up - p) {  /* lower interval is smaller? */
      auxsort(L, lo, p - 1, rnd);  /* call recursively for lower interval */
      n = p - lo;  /* size of smaller interval */
      lo = p + 1;  /* tail call for [p + 1 .. up] (upper interval) */
    }
    else {
      auxsort(L, p + 1, up, rnd);  /* call recursively for upper interval */
      n = up - p;  /* size of smaller interval */
      up = p - 1;  /* tail call for [lo .. p - 1]  (lower interval) */
    }
    if ((up - lo) / 128 > n) /* partition too imbalanced? */
      rnd = l_randomizePivot();  /* try a new randomization */
  }  /* tail call auxsort(L, lo, up, rnd) */
}


static int sort (hydrogen_State *L) {
  hydrogen_Integer n = aux_getn(L, 1, TAB_RW);
  if (n > 1) {  /* non-trivial interval? */
    hydrogenL_argcheck(L, n < INT_MAX, 1, "array too big");
    if (!hydrogen_isnoneornil(L, 2))  /* is there a 2nd argument? */
      hydrogenL_checktype(L, 2, HYDROGEN_TFUNCTION);  /* must be a function */
    hydrogen_settop(L, 2);  /* make sure there are two arguments */
    auxsort(L, 1, (IdxT)n, 0);
  }
  return 0;
}

/* }====================================================== */


static const hydrogenL_Reg tab_funcs[] = {
  {"concat", tconcat},
  {"insert", tinsert},
  {"pack", tpack},
  {"unpack", tunpack},
  {"remove", tremove},
  {"move", tmove},
  {"sort", sort},
  {NULL, NULL}
};


HYDROGENMOD_API int hydrogenopen_table (hydrogen_State *L) {
  hydrogenL_newlib(L, tab_funcs);
  return 1;
}