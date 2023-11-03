/*
** $Id: zio.h $
** Buffered streams
** See Copyright Notice in nebula.h
*/


#ifndef zio_h
#define zio_h

#include "nebula.h"

#include "memory.h"


#define EOZ	(-1)			/* end of stream */

typedef struct Zio ZIO;

#define zgetc(z)  (((z)->n--)>0 ?  cast_uchar(*(z)->p++) : nebulaZ_fill(z))


typedef struct Mbuffer {
  char *buffer;
  size_t n;
  size_t buffsize;
} Mbuffer;

#define nebulaZ_initbuffer(L, buff) ((buff)->buffer = NULL, (buff)->buffsize = 0)

#define nebulaZ_buffer(buff)	((buff)->buffer)
#define nebulaZ_sizebuffer(buff)	((buff)->buffsize)
#define nebulaZ_bufflen(buff)	((buff)->n)

#define nebulaZ_buffremove(buff,i)	((buff)->n -= (i))
#define nebulaZ_resetbuffer(buff) ((buff)->n = 0)


#define nebulaZ_resizebuffer(L, buff, size) \
	((buff)->buffer = nebulaM_reallocvchar(L, (buff)->buffer, \
				(buff)->buffsize, size), \
	(buff)->buffsize = size)

#define nebulaZ_freebuffer(L, buff)	nebulaZ_resizebuffer(L, buff, 0)


NEBULAI_FUNC void nebulaZ_init (nebula_State *L, ZIO *z, nebula_Reader reader,
                                        void *data);
NEBULAI_FUNC size_t nebulaZ_read (ZIO* z, void *b, size_t n);	/* read next n bytes */



/* --------- Private Part ------------------ */

struct Zio {
  size_t n;			/* bytes still unread */
  const char *p;		/* current position in buffer */
  nebula_Reader reader;		/* reader function */
  void *data;			/* additional data */
  nebula_State *L;			/* Nebula state (for reader) */
};


NEBULAI_FUNC int nebulaZ_fill (ZIO *z);

#endif
