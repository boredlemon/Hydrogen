/*
** $Id: dump.c $
** save precompiled Nebula chunks
** See Copyright Notice in nebula.h
*/

#define dump_c
#define NEBULA_CORE

#include "prefix.h"


#include <stddef.h>

#include "nebula.h"

#include "object.h"
#include "state.h"
#include "undump.h"


typedef struct {
  nebula_State *L;
  nebula_Writer writer;
  void *data;
  int strip;
  int status;
} DumpState;


/*
** All high-level dumps Nebula through dumpVector; you can change it to
** change the endianness of the result
*/
#define dumpVector(D,v,n)	dumpBlock(D,v,(n)*sizeof((v)[0]))

#define dumpLiteral(D, s)	dumpBlock(D,s,sizeof(s) - sizeof(char))


static void dumpBlock (DumpState *D, const void *b, size_t size) {
  if (D->status == 0 && size > 0) {
    nebula_unlock(D->L);
    D->status = (*D->writer)(D->L, b, size, D->data);
    nebula_lock(D->L);
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


static void dumpNumber (DumpState *D, nebula_Number x) {
  dumpVar(D, x);
}


static void dumpInteger (DumpState *D, nebula_Integer x) {
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
      case NEBULA_VNUMFLT:
        dumpNumber(D, fltvalue(o));
        break;
      case NEBULA_VNUMINT:
        dumpInteger(D, ivalue(o));
        break;
      case NEBULA_VSHRSTR:
      case NEBULA_VLNGSTR:
        dumpString(D, tsvalue(o));
        break;
      default:
        nebula_assert(tt == NEBULA_VNIL || tt == NEBULA_VFALSE || tt == NEBULA_VTRUE);
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
  dumpLiteral(D, NEBULA_SIGNATURE);
  dumpByte(D, NEBULAC_VERSION);
  dumpByte(D, NEBULAC_FORMAT);
  dumpLiteral(D, NEBULAC_DATA);
  dumpByte(D, sizeof(Instruction));
  dumpByte(D, sizeof(nebula_Integer));
  dumpByte(D, sizeof(nebula_Number));
  dumpInteger(D, NEBULAC_INT);
  dumpNumber(D, NEBULAC_NUM);
}


/*
** dump Nebula function as precompiled chunk
*/
int nebulaU_dump(nebula_State *L, const Proto *f, nebula_Writer w, void *data,
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

