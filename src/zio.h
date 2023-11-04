/*
** $Id: zio.h $
** Buffered streams
** See Copyright Notice in hydrogen.h
*/


#ifndef zio_h
#define zio_h

#include "hydrogen.h"

#include "memory.h"


#define EOZ	(-1)			/* end of stream */

typedef struct Zio ZIO;

#define zgetc(z)  (((z)->n--)>0 ?  cast_uchar(*(z)->p++) : hydrogenZ_fill(z))


typedef struct Mbuffer {
  char *buffer;
  size_t n;
  size_t buffsize;
} Mbuffer;

#define hydrogenZ_initbuffer(L, buff) ((buff)->buffer = NULL, (buff)->buffsize = 0)

#define hydrogenZ_buffer(buff)	((buff)->buffer)
#define hydrogenZ_sizebuffer(buff)	((buff)->buffsize)
#define hydrogenZ_bufflen(buff)	((buff)->n)

#define hydrogenZ_buffremove(buff,i)	((buff)->n -= (i))
#define hydrogenZ_resetbuffer(buff) ((buff)->n = 0)


#define hydrogenZ_resizebuffer(L, buff, size) \
	((buff)->buffer = hydrogenM_reallocvchar(L, (buff)->buffer, \
				(buff)->buffsize, size), \
	(buff)->buffsize = size)

#define hydrogenZ_freebuffer(L, buff)	hydrogenZ_resizebuffer(L, buff, 0)


HYDROGENI_FUNC void hydrogenZ_init (hydrogen_State *L, ZIO *z, hydrogen_Reader reader,
                                        void *data);
HYDROGENI_FUNC size_t hydrogenZ_read (ZIO* z, void *b, size_t n);	/* read next n bytes */



/* --------- Private Part ------------------ */

struct Zio {
  size_t n;			/* bytes still unread */
  const char *p;		/* current position in buffer */
  hydrogen_Reader reader;		/* reader function */
  void *data;			/* additional data */
  hydrogen_State *L;			/* Hydrogen state (for reader) */
};


HYDROGENI_FUNC int hydrogenZ_fill (ZIO *z);

#endif
