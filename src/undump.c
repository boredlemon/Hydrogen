/*
** $Id: undump.c $
** load precompiled Viper chunks
** See Copyright Notice in viper.h
*/

#define undump_c
#define VIPER_CORE

#include "prefix.h"


#include <limits.h>
#include <string.h>

#include "viper.h"

#include "debug.h"
#include "do.h"
#include "function.h"
#include "memory.h"
#include "object.h"
#include "string.h"
#include "undump.h"
#include "zio.h"


#if !defined(viperi_verifycode)
#define viperi_verifycode(L,f)  /* empty */
#endif


typedef struct {
  viper_State *L;
  ZIO *Z;
  const char *name;
} LoadState;


static l_noret error (LoadState *S, const char *why) {
  viperO_pushfstring(S->L, "%s: bad binary format (%s)", S->name, why);
  viperD_throw(S->L, VIPER_ERRSYNTAX);
}


/*
** All high-level loads Viper through loadVector; you can change it to
** adapt to the endianness of the input
*/
#define loadVector(S,b,n)	loadBlock(S,b,(n)*sizeof((b)[0]))

static void loadBlock (LoadState *S, void *b, size_t size) {
  if (viperZ_read(S->Z, b, size) != 0)
    error(S, "truncated chunk");
}


#define loadVar(S,x)		loadVector(S,&x,1)


static lu_byte loadByte (LoadState *S) {
  int b = zgetc(S->Z);
  if (b == EOZ)
    error(S, "truncated chunk");
  return cast_byte(b);
}


static size_t loadUnsigned (LoadState *S, size_t limit) {
  size_t x = 0;
  int b;
  limit >>= 7;
  do {
    b = loadByte(S);
    if (x >= limit)
      error(S, "integer overflow");
    x = (x << 7) | (b & 0x7f);
  } while ((b & 0x80) == 0);
  return x;
}


static size_t loadSize (LoadState *S) {
  return loadUnsigned(S, ~(size_t)0);
}


static int loadInt (LoadState *S) {
  return cast_int(loadUnsigned(S, INT_MAX));
}


static viper_Number loadNumber (LoadState *S) {
  viper_Number x;
  loadVar(S, x);
  return x;
}


static viper_Integer loadInteger (LoadState *S) {
  viper_Integer x;
  loadVar(S, x);
  return x;
}


/*
** Load a nullable string into prototype 'p'.
*/
static TString *loadStringN (LoadState *S, Proto *p) {
  viper_State *L = S->L;
  TString *ts;
  size_t size = loadSize(S);
  if (size == 0)  /* no string? */
    return NULL;
  else if (--size <= VIPERI_MAXSHORTLEN) {  /* short string? */
    char buff[VIPERI_MAXSHORTLEN];
    loadVector(S, buff, size);  /* load string into buffer */
    ts = viperS_newlstr(L, buff, size);  /* create string */
  }
  else {  /* long string */
    ts = viperS_createlngstrobj(L, size);  /* create string */
    setsvalue2s(L, L->top, ts);  /* anchor it ('loadVector' can GC) */
    viperD_inctop(L);
    loadVector(S, getstr(ts), size);  /* load directly in final place */
    L->top--;  /* pop string */
  }
  viperC_objbarrier(L, p, ts);
  return ts;
}


/*
** Load a non-nullable string into prototype 'p'.
*/
static TString *loadString (LoadState *S, Proto *p) {
  TString *st = loadStringN(S, p);
  if (st == NULL)
    error(S, "bad format for constant string");
  return st;
}


static void loadCode (LoadState *S, Proto *f) {
  int n = loadInt(S);
  f->code = viperM_newvectorchecked(S->L, n, Instruction);
  f->sizecode = n;
  loadVector(S, f->code, n);
}


static void loadFunction(LoadState *S, Proto *f, TString *psource);


static void loadConstants (LoadState *S, Proto *f) {
  int i;
  int n = loadInt(S);
  f->k = viperM_newvectorchecked(S->L, n, TValue);
  f->sizek = n;
  for (i = 0; i < n; i++)
    setnilvalue(&f->k[i]);
  for (i = 0; i < n; i++) {
    TValue *o = &f->k[i];
    int t = loadByte(S);
    switch (t) {
      case VIPER_VNIL:
        setnilvalue(o);
        break;
      case VIPER_VFALSE:
        setbfvalue(o);
        break;
      case VIPER_VTRUE:
        setbtvalue(o);
        break;
      case VIPER_VNUMFLT:
        setfltvalue(o, loadNumber(S));
        break;
      case VIPER_VNUMINT:
        setivalue(o, loadInteger(S));
        break;
      case VIPER_VSHRSTR:
      case VIPER_VLNGSTR:
        setsvalue2n(S->L, o, loadString(S, f));
        break;
      default: viper_assert(0);
    }
  }
}


static void loadProtos (LoadState *S, Proto *f) {
  int i;
  int n = loadInt(S);
  f->p = viperM_newvectorchecked(S->L, n, Proto *);
  f->sizep = n;
  for (i = 0; i < n; i++)
    f->p[i] = NULL;
  for (i = 0; i < n; i++) {
    f->p[i] = viperF_newproto(S->L);
    viperC_objbarrier(S->L, f, f->p[i]);
    loadFunction(S, f->p[i], f->source);
  }
}


/*
** Load the upvalues for a function. The names must be filled first,
** because the filling of the other fields can raise read errors and
** the creation of the error message can call an emergency collection;
** in that case all prototypes must be consistent for the GC.
*/
static void loadUpvalues (LoadState *S, Proto *f) {
  int i, n;
  n = loadInt(S);
  f->upvalues = viperM_newvectorchecked(S->L, n, Upvaldesc);
  f->sizeupvalues = n;
  for (i = 0; i < n; i++)  /* make array valid for GC */
    f->upvalues[i].name = NULL;
  for (i = 0; i < n; i++) {  /* following calls can raise errors */
    f->upvalues[i].instack = loadByte(S);
    f->upvalues[i].idx = loadByte(S);
    f->upvalues[i].kind = loadByte(S);
  }
}


static void loadDebug (LoadState *S, Proto *f) {
  int i, n;
  n = loadInt(S);
  f->lineinfo = viperM_newvectorchecked(S->L, n, ls_byte);
  f->sizelineinfo = n;
  loadVector(S, f->lineinfo, n);
  n = loadInt(S);
  f->abslineinfo = viperM_newvectorchecked(S->L, n, AbsLineInfo);
  f->sizeabslineinfo = n;
  for (i = 0; i < n; i++) {
    f->abslineinfo[i].pc = loadInt(S);
    f->abslineinfo[i].line = loadInt(S);
  }
  n = loadInt(S);
  f->locvars = viperM_newvectorchecked(S->L, n, LocVar);
  f->sizelocvars = n;
  for (i = 0; i < n; i++)
    f->locvars[i].varname = NULL;
  for (i = 0; i < n; i++) {
    f->locvars[i].varname = loadStringN(S, f);
    f->locvars[i].startpc = loadInt(S);
    f->locvars[i].endpc = loadInt(S);
  }
  n = loadInt(S);
  for (i = 0; i < n; i++)
    f->upvalues[i].name = loadStringN(S, f);
}


static void loadFunction (LoadState *S, Proto *f, TString *psource) {
  f->source = loadStringN(S, f);
  if (f->source == NULL)  /* no source in dump? */
    f->source = psource;  /* reuse parent's source */
  f->linedefined = loadInt(S);
  f->lastlinedefined = loadInt(S);
  f->numparams = loadByte(S);
  f->is_vararg = loadByte(S);
  f->maxstacksize = loadByte(S);
  loadCode(S, f);
  loadConstants(S, f);
  loadUpvalues(S, f);
  loadProtos(S, f);
  loadDebug(S, f);
}


static void checkliteral (LoadState *S, const char *s, const char *msg) {
  char buff[sizeof(VIPER_SIGNATURE) + sizeof(VIPERC_DATA)]; /* larger than both */
  size_t len = strlen(s);
  loadVector(S, buff, len);
  if (memcmp(s, buff, len) != 0)
    error(S, msg);
}


static void fchecksize (LoadState *S, size_t size, const char *tname) {
  if (loadByte(S) != size)
    error(S, viperO_pushfstring(S->L, "%s size mismatch", tname));
}


#define checksize(S,t)	fchecksize(S,sizeof(t),#t)

static void checkHeader (LoadState *S) {
  /* skip 1st char (already read and checked) */
  checkliteral(S, &VIPER_SIGNATURE[1], "not a binary chunk");
  if (loadByte(S) != VIPERC_VERSION)
    error(S, "version mismatch");
  if (loadByte(S) != VIPERC_FORMAT)
    error(S, "format mismatch");
  checkliteral(S, VIPERC_DATA, "corrupted chunk");
  checksize(S, Instruction);
  checksize(S, viper_Integer);
  checksize(S, viper_Number);
  if (loadInteger(S) != VIPERC_INT)
    error(S, "integer format mismatch");
  if (loadNumber(S) != VIPERC_NUM)
    error(S, "float format mismatch");
}


/*
** Load precompiled chunk.
*/
LClosure *viperU_undump(viper_State *L, ZIO *Z, const char *name) {
  LoadState S;
  LClosure *cl;
  if (*name == '@' || *name == '=')
    S.name = name + 1;
  else if (*name == VIPER_SIGNATURE[0])
    S.name = "binary string";
  else
    S.name = name;
  S.L = L;
  S.Z = Z;
  checkHeader(&S);
  cl = viperF_newLclosure(L, loadByte(&S));
  setclLvalue2s(L, L->top, cl);
  viperD_inctop(L);
  cl->p = viperF_newproto(L);
  viperC_objbarrier(L, cl, cl->p);
  loadFunction(&S, cl->p, NULL);
  viper_assert(cl->nupvalues == cl->p->sizeupvalues);
  viperi_verifycode(L, cl->p);
  return cl;
}

