#ifndef __MAP_H__
#define __MAP_H__

/*
 Map implementation using Anderson tree library, based on public domain code made freely
 available by Julienne Walker at Eternally Confuzzled (www.eternallyconfuzzled.com).

 Visit Eternally Confuzzled for the original source along with many other
 excellent tutorials and examples of common algorithms in C
 */
#include <stddef.h>
#include "lc_error.h"
#include "algorithm.h"

#ifndef HEIGHT_LIMIT
# define HEIGHT_LIMIT 64 /* Tallest allowable map */
#endif

typedef struct _map_node {
  int level; /* Horizontal level for balance */
  void *data; /* User-defined content */
  struct _map_node *link[2]; /* Left (0) and right (1) links */
} map_node_t;

typedef struct _map {
  map_node_t *root; /* Top of the map */
  compare_cb cmp; /* Compare two items */
  copy_cb dup; /* Clone an item (user-defined) */
  release_cb rel; /* Destroy an item (user-defined) */
  long size; /* Number of items (user-defined) */
} map_t;

typedef struct _map_cursor {
  map_t *map; /* Paired map */
  map_node_t *it; /* Current node */
  map_node_t *path[HEIGHT_LIMIT]; /* Traversal path */
  size_t top; /* Top of stack */
} map_cursor_t;

int map_init(map_t *map, compare_cb cmp, copy_cb dup, release_cb rel);
map_t *map_new(compare_cb cmp, copy_cb dup, release_cb rel);
int map_free(map_t *map);
int map_clear(map_t *map);
void *map_find(map_t *map, void *data);
long map_insert(map_t *map, void *data);
void *map_remove(map_t *map, void *data);
long map_size(map_t *map);
void map_ordered(map_t *map, order_cb);

/* Traversal functions */
map_cursor_t *map_cursor(void);
void map_cursor_free(map_cursor_t *cur);
void *map_first(map_cursor_t *cur, map_t *map);
void *map_last(map_cursor_t *cur, map_t *map);
void *map_next(map_cursor_t *cur);
void *map_prev(map_cursor_t *cur);

#endif // __MAP_H__
