#include "casting.h"
#include "lc_error.h"
#include "queue.h"

static q_node_t *new_node(queue_t *q, void *data) {
  int err;
  q_node_t *node = lc_alloc(sizeof(q_node_t));

  if (node) {
    if (q->copy && data) {
      if ((err = q->copy(data, &node->data)) != SUCCESS) {
        node = lc_free(node);
      }
    } else {
      node->data = data;
    }
  }
  node->next = NULL;
  return node;
}

int queue_init(queue_t *q, copy_cb copy, release_cb rel) {
  q->copy = copy;
  q->rel = rel;
  q->head.next = NULL;
  q->head.data = NULL;
  q->tail = &q->head;
  q->size = 0;
  return SUCCESS;

}

queue_t *queue_new(copy_cb copy, release_cb rel) {
  queue_t *q = lc_alloc(sizeof(queue_t));
  if (q) {
    q->copy = copy;
    q->rel = rel;
    q->head.next = NULL;
    q->head.data = NULL;
    q->tail = &q->head;
    q->size = 0;
  }
  return q;
}

int queue_push(queue_t *q, void *p) {
  if (!q) return ERR_INVAL;

  q_node_t *n = new_node(q, p);
  if (!n) return ERR_NOMEM;

  q->tail->next = n;
  q->tail = n;
  q->size++;
  return SUCCESS;
}

void *queue_pop(queue_t *q) {
  void *p = NULL;
  q_node_t *head = q->head.next;
  if (head == NULL) {
    return NULL;
  }
  q_node_t *next = head->next;
  if (q->tail == head) q->tail = &q->head;

  p = head->data;
  q->head.next = next;

  lc_free(head);
  q->size--;
  return p;
}

void *queue_peek(queue_t *q) {
  q_node_t *head = q->head.next;
  if (head == NULL) {
    return NULL;
  } else {
    return head->data;
  }
}

int queue_clear(queue_t *q) {
  if (!q) return ERR_INVAL;
  void *d = NULL;
  while ((d = queue_pop(q))) {
    q->rel ? q->rel(d) : (void) 0;
  }

  return SUCCESS;
}

int queue_free(queue_t *q) {
  int rc = queue_clear(q);
  lc_free(q);
  return rc;
}

int queue_isempty(queue_t *q) {
  return q->tail == &q->head;
}

int queue_size(queue_t *q) {
  return q->size;
}
