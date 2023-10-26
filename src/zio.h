/*
** $Id: zio.h $
** Buffered streams
** See Copyright Notice in viper.h
*/


#ifndef zio_h
#define zio_h

#include "viper.h"

#include "memory.h"


#define EOZ	(-1)			/* end of stream */

typedef struct Zio ZIO;

#define zgetc(z)  (((z)->n--)>0 ?  cast_uchar(*(z)->p++) : viperZ_fill(z))


typedef struct Mbuffer {
  char *buffer;
  size_t n;
  size_t buffsize;
} Mbuffer;

#define viperZ_initbuffer(L, buff) ((buff)->buffer = NULL, (buff)->buffsize = 0)

#define viperZ_buffer(buff)	((buff)->buffer)
#define viperZ_sizebuffer(buff)	((buff)->buffsize)
#define viperZ_bufflen(buff)	((buff)->n)

#define viperZ_buffremove(buff,i)	((buff)->n -= (i))
#define viperZ_resetbuffer(buff) ((buff)->n = 0)


#define viperZ_resizebuffer(L, buff, size) \
	((buff)->buffer = viperM_reallocvchar(L, (buff)->buffer, \
				(buff)->buffsize, size), \
	(buff)->buffsize = size)

#define viperZ_freebuffer(L, buff)	viperZ_resizebuffer(L, buff, 0)


VIPERI_FUNC void viperZ_init (viper_State *L, ZIO *z, viper_Reader reader,
                                        void *data);
VIPERI_FUNC size_t viperZ_read (ZIO* z, void *b, size_t n);	/* read next n bytes */



/* --------- Private Part ------------------ */

struct Zio {
  size_t n;			/* bytes still unread */
  const char *p;		/* current position in buffer */
  viper_Reader reader;		/* reader function */
  void *data;			/* additional data */
  viper_State *L;			/* Viper state (for reader) */
};


VIPERI_FUNC int viperZ_fill (ZIO *z);

#endif
