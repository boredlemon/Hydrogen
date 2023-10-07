/*
** $Id: ldump.c $
** save precompiled Cup chunks
** See Copyright Notice in cup.h
*/

#define ldump_c
#define CUP_CORE

#include "lprefix.h"


#include <stddef.h>

#include "cup.h"

#include "lobject.h"
#include "lstate.h"
#include "lundump.h"


typedef struct {
  cup_State *L;
  cup_Writer writer;
  void *data;
  int strip;
  int status;
} DumpState;


/*
** All high-level dumps Cup through dumpVector; you can change it to
** change the endianness of the result
*/
#define dumpVector(D,v,n)	dumpBlock(D,v,(n)*sizeof((v)[0]))

#define dumpLiteral(D, s)	dumpBlock(D,s,sizeof(s) - sizeof(char))


static void dumpBlock (DumpState *D, const void *b, size_t size) {
  if (D->status == 0 && size > 0) {
    cup_unlock(D->L);
    D->status = (*D->writer)(D->L, b, size, D->data);
    cup_lock(D->L);
  }
}


#define dumpVar(D,x)		dumpVector(D,&x,1)


static void dumpByte (DumpState *D, int y) {
  lu_byte x = (lu_byte)y;
  dumpVar(D, x);
}


/* dumpInt Buff Size */
#define DIBS    ((sizeof(size_t) * 8 / 7) + 1)

static void dumpSize (DumpState *D, size_t x) {
  lu_byte buff[DIBS];
  int n = 0;
  do {
    buff[DIBS - (++n)] = x & 0x7f;  /* fill buffer in reverse order */
    x >>= 7;
  } while (x != 0);
  buff[DIBS - 1] |= 0x80;  /* mark last byte */
  dumpVector(D, buff + DIBS - n, n);
}


static void dumpInt (DumpState *D, int x) {
  dumpSize(D, x);
}


static void dumpNumber (DumpState *D, cup_Number x) {
  dumpVar(D, x);
}


static void dumpInteger (DumpState *D, cup_Integer x) {
  dumpVar(D, x);
}


static void dumpString (DumpState *D, const TString *s) {
  if (s == NULL)
    dumpSize(D, 0);
  else {
    size_t size = tsslen(s);
    const char *str = getstr(s);
    dumpSize(D, size + 1);
    dumpVector(D, str, size);
  }
}


static void dumpCode (DumpState *D, const Proto *f) {
  dumpInt(D, f->sizecode);
  dumpVector(D, f->code, f->sizecode);
}


static void dumpFunction(DumpState *D, const Proto *f, TString *psource);

static void dumpConstants (DumpState *D, const Proto *f) {
  int i;
  int n = f->sizek;
  dumpInt(D, n);
  for (i = 0; i < n; i++) {
    const TValue *o = &f->k[i];
    int tt = ttypetag(o);
    dumpByte(D, tt);
    switch (tt) {
      case CUP_VNUMFLT:
        dumpNumber(D, fltvalue(o));
        break;
      case CUP_VNUMINT:
        dumpInteger(D, ivalue(o));
        break;
      case CUP_VSHRSTR:
      case CUP_VLNGSTR:
        dumpString(D, tsvalue(o));
        break;
      default:
        cup_assert(tt == CUP_VNIL || tt == CUP_VFALSE || tt == CUP_VTRUE);
    }
  }
}


static void dumpProtos (DumpState *D, const Proto *f) {
  int i;
  int n = f->sizep;
  dumpInt(D, n);
  for (i = 0; i < n; i++)
    dumpFunction(D, f->p[i], f->source);
}


static void dumpUpvalues (DumpState *D, const Proto *f) {
  int i, n = f->sizeupvalues;
  dumpInt(D, n);
  for (i = 0; i < n; i++) {
    dumpByte(D, f->upvalues[i].instack);
    dumpByte(D, f->upvalues[i].idx);
    dumpByte(D, f->upvalues[i].kind);
  }
}


static void dumpDebug (DumpState *D, const Proto *f) {
  int i, n;
  n = (D->strip) ? 0 : f->sizelineinfo;
  dumpInt(D, n);
  dumpVector(D, f->lineinfo, n);
  n = (D->strip) ? 0 : f->sizeabslineinfo;
  dumpInt(D, n);
  for (i = 0; i < n; i++) {
    dumpInt(D, f->abslineinfo[i].pc);
    dumpInt(D, f->abslineinfo[i].line);
  }
  n = (D->strip) ? 0 : f->sizelocvars;
  dumpInt(D, n);
  for (i = 0; i < n; i++) {
    dumpString(D, f->locvars[i].varname);
    dumpInt(D, f->locvars[i].startpc);
    dumpInt(D, f->locvars[i].endpc);
  }
  n = (D->strip) ? 0 : f->sizeupvalues;
  dumpInt(D, n);
  for (i = 0; i < n; i++)
    dumpString(D, f->upvalues[i].name);
}


static void dumpFunction (DumpState *D, const Proto *f, TString *psource) {
  if (D->strip || f->source == psource)
    dumpString(D, NULL);  /* no debug info or same source as its parent */
  else
    dumpString(D, f->source);
  dumpInt(D, f->linedefined);
  dumpInt(D, f->lastlinedefined);
  dumpByte(D, f->numparams);
  dumpByte(D, f->is_vararg);
  dumpByte(D, f->maxstacksize);
  dumpCode(D, f);
  dumpConstants(D, f);
  dumpUpvalues(D, f);
  dumpProtos(D, f);
  dumpDebug(D, f);
}


static void dumpHeader (DumpState *D) {
  dumpLiteral(D, CUP_SIGNATURE);
  dumpByte(D, CUPC_VERSION);
  dumpByte(D, CUPC_FORMAT);
  dumpLiteral(D, CUPC_DATA);
  dumpByte(D, sizeof(Instruction));
  dumpByte(D, sizeof(cup_Integer));
  dumpByte(D, sizeof(cup_Number));
  dumpInteger(D, CUPC_INT);
  dumpNumber(D, CUPC_NUM);
}


/*
** dump Cup function as precompiled chunk
*/
int cupU_dump(cup_State *L, const Proto *f, cup_Writer w, void *data,
              int strip) {
  DumpState D;
  D.L = L;
  D.writer = w;
  D.data = data;
  D.strip = strip;
  D.status = 0;
  dumpHeader(&D);
  dumpByte(&D, f->sizeupvalues);
  dumpFunction(&D, f, NULL);
  return D.status;
}

