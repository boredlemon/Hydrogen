/*
** $Id: lzio.h $
** Buffered streams
** See Copyright Notice in cup.h
*/


#ifndef lzio_h
#define lzio_h

#include "cup.h"

#include "lmem.h"


#define EOZ	(-1)			/* end of stream */

typedef struct Zio ZIO;

#define zgetc(z)  (((z)->n--)>0 ?  cast_uchar(*(z)->p++) : cupZ_fill(z))


typedef struct Mbuffer {
  char *buffer;
  size_t n;
  size_t buffsize;
} Mbuffer;

#define cupZ_initbuffer(L, buff) ((buff)->buffer = NULL, (buff)->buffsize = 0)

#define cupZ_buffer(buff)	((buff)->buffer)
#define cupZ_sizebuffer(buff)	((buff)->buffsize)
#define cupZ_bufflen(buff)	((buff)->n)

#define cupZ_buffremove(buff,i)	((buff)->n -= (i))
#define cupZ_resetbuffer(buff) ((buff)->n = 0)


#define cupZ_resizebuffer(L, buff, size) \
	((buff)->buffer = cupM_reallocvchar(L, (buff)->buffer, \
				(buff)->buffsize, size), \
	(buff)->buffsize = size)

#define cupZ_freebuffer(L, buff)	cupZ_resizebuffer(L, buff, 0)


CUPI_FUNC void cupZ_init (cup_State *L, ZIO *z, cup_Reader reader,
                                        void *data);
CUPI_FUNC size_t cupZ_read (ZIO* z, void *b, size_t n);	/* read next n bytes */



/* --------- Private Part ------------------ */

struct Zio {
  size_t n;			/* bytes still unread */
  const char *p;		/* current position in buffer */
  cup_Reader reader;		/* reader function */
  void *data;			/* additional data */
  cup_State *L;			/* Cup state (for reader) */
};


CUPI_FUNC int cupZ_fill (ZIO *z);

#endif
