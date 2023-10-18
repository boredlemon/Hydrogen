/*
** $Id: lzio.h $
** Buffered streams
** See Copyright Notice in acorn.h
*/


#ifndef lzio_h
#define lzio_h

#include "acorn.h"

#include "lmem.h"


#define EOZ	(-1)			/* end of stream */

typedef struct Zio ZIO;

#define zgetc(z)  (((z)->n--)>0 ?  cast_uchar(*(z)->p++) : acornZ_fill(z))


typedef struct Mbuffer {
  char *buffer;
  size_t n;
  size_t buffsize;
} Mbuffer;

#define acornZ_initbuffer(L, buff) ((buff)->buffer = NULL, (buff)->buffsize = 0)

#define acornZ_buffer(buff)	((buff)->buffer)
#define acornZ_sizebuffer(buff)	((buff)->buffsize)
#define acornZ_bufflen(buff)	((buff)->n)

#define acornZ_buffremove(buff,i)	((buff)->n -= (i))
#define acornZ_resetbuffer(buff) ((buff)->n = 0)


#define acornZ_resizebuffer(L, buff, size) \
	((buff)->buffer = acornM_reallocvchar(L, (buff)->buffer, \
				(buff)->buffsize, size), \
	(buff)->buffsize = size)

#define acornZ_freebuffer(L, buff)	acornZ_resizebuffer(L, buff, 0)


ACORNI_FUNC void acornZ_init (acorn_State *L, ZIO *z, acorn_Reader reader,
                                        void *data);
ACORNI_FUNC size_t acornZ_read (ZIO* z, void *b, size_t n);	/* read next n bytes */



/* --------- Private Part ------------------ */

struct Zio {
  size_t n;			/* bytes still unread */
  const char *p;		/* current position in buffer */
  acorn_Reader reader;		/* reader function */
  void *data;			/* additional data */
  acorn_State *L;			/* Acorn state (for reader) */
};


ACORNI_FUNC int acornZ_fill (ZIO *z);

#endif
