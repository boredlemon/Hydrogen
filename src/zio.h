/*
** $Id: zio.h $
** Buffered streams
** See Copyright Notice in venom.h
*/


#ifndef zio_h
#define zio_h

#include "venom.h"

#include "memory.h"


#define EOZ	(-1)			/* end of stream */

typedef struct Zio ZIO;

#define zgetc(z)  (((z)->n--)>0 ?  cast_uchar(*(z)->p++) : venomZ_fill(z))


typedef struct Mbuffer {
  char *buffer;
  size_t n;
  size_t buffsize;
} Mbuffer;

#define venomZ_initbuffer(L, buff) ((buff)->buffer = NULL, (buff)->buffsize = 0)

#define venomZ_buffer(buff)	((buff)->buffer)
#define venomZ_sizebuffer(buff)	((buff)->buffsize)
#define venomZ_bufflen(buff)	((buff)->n)

#define venomZ_buffremove(buff,i)	((buff)->n -= (i))
#define venomZ_resetbuffer(buff) ((buff)->n = 0)


#define venomZ_resizebuffer(L, buff, size) \
	((buff)->buffer = venomM_reallocvchar(L, (buff)->buffer, \
				(buff)->buffsize, size), \
	(buff)->buffsize = size)

#define venomZ_freebuffer(L, buff)	venomZ_resizebuffer(L, buff, 0)


VENOMI_FUNC void venomZ_init (venom_State *L, ZIO *z, venom_Reader reader,
                                        void *data);
VENOMI_FUNC size_t venomZ_read (ZIO* z, void *b, size_t n);	/* read next n bytes */



/* --------- Private Part ------------------ */

struct Zio {
  size_t n;			/* bytes still unread */
  const char *p;		/* current position in buffer */
  venom_Reader reader;		/* reader function */
  void *data;			/* additional data */
  venom_State *L;			/* Venom state (for reader) */
};


VENOMI_FUNC int venomZ_fill (ZIO *z);

#endif
