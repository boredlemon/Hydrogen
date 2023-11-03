/*
** $Id: zio.c $
** Buffered streams
** See Copyright Notice in nebula.h
*/

#define zio_c
#define NEBULA_CORE

#include "prefix.h"


#include <string.h>

#include "nebula.h"

#include "limits.h"
#include "memory.h"
#include "state.h"
#include "zio.h"


int nebulaZ_fill (ZIO *z) {
  size_t size;
  nebula_State *L = z->L;
  const char *buff;
  nebula_unlock(L);
  buff = z->reader(L, z->data, &size);
  nebula_lock(L);
  if (buff == NULL || size == 0)
    return EOZ;
  z->n = size - 1;  /* discount char being returned */
  z->p = buff;
  return cast_uchar(*(z->p++));
}


void nebulaZ_init (nebula_State *L, ZIO *z, nebula_Reader reader, void *data) {
  z->L = L;
  z->reader = reader;
  z->data = data;
  z->n = 0;
  z->p = NULL;
}


/* --------------------------------------------------------------- read --- */
size_t nebulaZ_read (ZIO *z, void *b, size_t n) {
  while (n) {
    size_t m;
    if (z->n == 0) {  /* no bytes in buffer? */
      if (nebulaZ_fill(z) == EOZ)  /* try to read more */
        return n;  /* no more input; return number of missing bytes */
      else {
        z->n++;  /* nebulaZ_fill consumed first byte; put it back */
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

