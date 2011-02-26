#include <stdlib.h>
#include <string.h>
#include "casting.h"

#include "buffer.h"
#include "lc_error.h"

static int grow(buffer_t *b, size_t end);

#define ensure(b,p,s) ((p) + (s) > (b)->size ? grow((b),(p) + (s)) : SUCCESS)
#define check(b,p,s) ((p) + (s) > (b)->last ? ERR_OVERFLOW : SUCCESS)

static int grow(buffer_t *b, size_t end) {
  if (b->size >= end) return SUCCESS;
  size_t new_size = b->size;

  while (end > new_size)
    new_size = new_size + (new_size >> 1) + 8;

  char *n = lc_alloc(new_size);
  if (!n) return ERR_NOMEM;

  memcpy(n, b->p, b->last);

  if (b->p != b->b) lc_free(b->p);

  b->p = n;
  b->size = new_size;

  return SUCCESS;
}

#define max(x,y) ((x) > (y) ? (x) : (y))

void *buf_release(buffer_t *b) {
  void *p = NULL;
  if (!b) return p;

  if (b->p == b->b) {
    p = lc_alloc(b->last);
    memcpy(p, b->p, b->last);
  } else {
    //p = lc_realloc(b->p, b->last, b->last);
    p = b->p;
  }
  //buf_init(b);

  return p;
}

void buf_free(buffer_t *b) {
  if (b->p != b->b) lc_free(b->p);
}

int buf_compact(buffer_t *b) {
  if (!b) return ERR_INVAL;
  if (b->last < b->size) {
    b->p = lc_realloc(b->p, b->size, b->last);
    b->size = b->last;
  }
  return SUCCESS;
}

int buf_write(buffer_t *b, const void *p, size_t size) {
  if (!b) return ERR_INVAL;
  int err = buf_write_at(b, b->pos, p, size);
  if (err != SUCCESS) return err;
  b->pos += size;
  return SUCCESS;
}

int buf_write_at(buffer_t *b, size_t pos, const void *p, size_t size) {
  if (!b) return ERR_INVAL;
  if (ensure(b,pos,size) != SUCCESS) return ERR_NOMEM;
  memcpy(&b->p[pos], p, size);
  b->last = max(b->last,pos+size);
  return SUCCESS;
}

void *buf_read(buffer_t *b, size_t size) {
  void *p = buf_read_from(b, b->pos, size);
  if (p) b->pos += size;
  return p;
}

void *buf_read_from(buffer_t *b, size_t pos, size_t size) {
  void *p = NULL;
  if (b && check(b,pos,size) == SUCCESS) {
    p = &b->p[pos];
  }
  return p;
}

void *buf_reserve(buffer_t *b, size_t size) {
  void *p = NULL;
  if (b && ensure(b,b->pos,size) == SUCCESS) {
    p = &b->p[b->pos];
    b->pos += size;
    b->last = max(b->last,b->pos);
  }
  return p;
}

int buf_seek(buffer_t *b, size_t pos) {
  if (!b) return ERR_INVAL;

  if (pos > b->last) return ERR_OVERFLOW;
  b->pos = pos;
  return SUCCESS;
}
