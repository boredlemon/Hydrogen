/*
** $Id: lzio.c $
** Buffered streams
** See Copyright Notice in acorn.h
*/

#define lzio_c
#define ACORN_CORE

#include "lprefix.h"


#include <string.h>

#include "acorn.h"

#include "llimits.h"
#include "lmem.h"
#include "lstate.h"
#include "lzio.h"


int acornZ_fill (ZIO *z) {
  size_t size;
  acorn_State *L = z->L;
  const char *buff;
  acorn_unlock(L);
  buff = z->reader(L, z->data, &size);
  acorn_lock(L);
  if (buff == NULL || size == 0)
    return EOZ;
  z->n = size - 1;  /* discount char being returned */
  z->p = buff;
  return cast_uchar(*(z->p++));
}


void acornZ_init (acorn_State *L, ZIO *z, acorn_Reader reader, void *data) {
  z->L = L;
  z->reader = reader;
  z->data = data;
  z->n = 0;
  z->p = NULL;
}


/* --------------------------------------------------------------- read --- */
size_t acornZ_read (ZIO *z, void *b, size_t n) {
  while (n) {
    size_t m;
    if (z->n == 0) {  /* no bytes in buffer? */
      if (acornZ_fill(z) == EOZ)  /* try to read more */
        return n;  /* no more input; return number of missing bytes */
      else {
        z->n++;  /* acornZ_fill consumed first byte; put it back */
        z->p--;
      }
    }
    m = (n <= z->n) ? n : z->n;  /* min. between n and z->n */
    memcpy(b, z->p, m);
    z->n -= m;
    z->p += m;
    b = (char *)b + m;
    n -= m;
  }
  return 0;
}

