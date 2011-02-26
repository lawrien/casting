#ifndef __QUEUE_H__
#define __QUEUE_H__

#include "algorithm.h"

typedef struct _q_node {
  struct _q_node *next;
  void *data;
} q_node_t;

typedef struct _queue {
  q_node_t head;
  q_node_t *tail;
  copy_cb copy;
  release_cb rel;
  int size;
} queue_t;

int queue_init(queue_t *q,copy_cb dup,release_cb rel);
queue_t *queue_new(copy_cb dup,release_cb rel);
int queue_push(queue_t *q, void *p);
void *queue_pop(queue_t *q);
int queue_clear(queue_t *q);
int queue_free(queue_t *q);
int queue_isempty(queue_t *q);
int queue_size(queue_t *q);
void *queue_peek(queue_t *q);

#endif // __QUEUE_H__
