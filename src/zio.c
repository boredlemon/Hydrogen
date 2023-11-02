/*
** $Id: zio.c $
** Buffered streams
** See Copyright Notice in venom.h
*/

#define zio_c
#define VENOM_CORE

#include "prefix.h"


#include <string.h>

#include "venom.h"

#include "limits.h"
#include "memory.h"
#include "state.h"
#include "zio.h"


int venomZ_fill (ZIO *z) {
  size_t size;
  venom_State *L = z->L;
  const char *buff;
  venom_unlock(L);
  buff = z->reader(L, z->data, &size);
  venom_lock(L);
  if (buff == NULL || size == 0)
    return EOZ;
  z->n = size - 1;  /* discount char being returned */
  z->p = buff;
  return cast_uchar(*(z->p++));
}


void venomZ_init (venom_State *L, ZIO *z, venom_Reader reader, void *data) {
  z->L = L;
  z->reader = reader;
  z->data = data;
  z->n = 0;
  z->p = NULL;
}


/* --------------------------------------------------------------- read --- */
size_t venomZ_read (ZIO *z, void *b, size_t n) {
  while (n) {
    size_t m;
    if (z->n == 0) {  /* no bytes in buffer? */
      if (venomZ_fill(z) == EOZ)  /* try to read more */
        return n;  /* no more input; return number of missing bytes */
      else {
        z->n++;  /* venomZ_fill consumed first byte; put it back */
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

