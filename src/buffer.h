#ifndef __BUFFER_H__
#define __BUFFER_H__

#include "lc_error.h"

#define BUFFER_SIZE 2048

typedef struct _buffer {
  size_t size;
  size_t last;
  size_t pos;
  char *p;
  char b[BUFFER_SIZE];
} buffer_t;

#define buf_init(buf) do { \
                        (buf)->size = BUFFER_SIZE; \
                        (buf)->last = 0; \
                        (buf)->pos = 0; \
                        (buf)->p = (buf)->b; \
                      } while(0)

#define buf_wrap(buf,ptr,s) do { \
                            (buf)->size = (s); \
                            (buf)->last = (s); \
                            (buf)->pos = 0; \
                            (buf)->p = (char *)(ptr); \
                          } while(0)

void *buf_release(buffer_t *buf);
void buf_free(buffer_t *b);

int buf_write(buffer_t *b, const void *p, size_t size);
int buf_write_at(buffer_t *b, size_t pos,const void *p, size_t size);

void *buf_read(buffer_t *b, size_t size);
void *buf_read_from(buffer_t *b,size_t pos, size_t size);

void *buf_reserve(buffer_t *b, size_t size);
void *buf_peek(buffer_t *b, size_t size);

#define buf_avail(b) ((b)->pos < (b)->last)
#define buf_pos(b) (b)->pos
#define buf_capacity(b) (b)->size
#define buf_end(b) (b)->last
#define buf_size(b) (b)->last
int buf_seek(buffer_t *b, size_t offset);

#endif // __BUFFER_H__
