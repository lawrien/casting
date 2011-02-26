#include <assert.h>
#include "casting.h"
#include "lc_error.h"
#include "map.h"

static map_node_t nil = { 0, NULL, { &nil, &nil } };
static map_node_t *sentinal = &nil;

/* Remove left horizontal links */
#define skew(t) do {                                      \
  if ( t->link[0]->level == t->level && t->level != 0 ) { \
    map_node_t *save = t->link[0];                        \
    t->link[0] = save->link[1];                           \
    save->link[1] = t;                                    \
    t = save;                                             \
  }                                                       \
} while(0)

/* Remove consecutive horizontal links */
#define split(t) do {                                              \
  if ( t->link[1]->link[1]->level == t->level && t->level != 0 ) { \
    map_node_t *save = t->link[1];                                 \
    t->link[1] = save->link[0];                                    \
    save->link[0] = t;                                             \
    t = save;                                                      \
    ++t->level;                                                    \
  }                                                                \
} while(0)

static int new_node(map_t *map, void *data, map_node_t **n) {
  int err;
  map_node_t *node = (map_node_t *) lc_alloc(sizeof(map_node_t));

  if (!node) return ERR_NOMEM;

  node->level = 1;
  if (map->dup) {
    if ((err = map->dup(data,&node->data)) != SUCCESS) {
      lc_free(node);
      return err;
    }
  } else {
    node->data = data;
  }

  node->link[0] = node->link[1] = sentinal;

  *n = node;
  return SUCCESS;
}

int map_init(map_t *map, compare_cb cmp, copy_cb dup, release_cb rel) {
  if (!map) return ERR_INVAL;

  map->root = sentinal;
  map->cmp = cmp;
  map->dup = dup;
  map->rel = rel;
  map->size = 0;

  return SUCCESS;
}

map_t *map_new(compare_cb cmp, copy_cb dup, release_cb rel) {
  map_t *map = (map_t *) lc_alloc(sizeof(map_t));

  if (map) {
    if(map_init(map, cmp, dup, rel) != SUCCESS) {
      lc_free(map);
      return NULL;
    }
  }
  return map;
}

int map_clear(map_t *map) {
  if (!map) return ERR_INVAL;
  map_node_t *it = map->root;
  map_node_t *save;

  /* Destruction by rotation */
  while (it != sentinal) {
    if (it->link[0] == sentinal) {
      /* Remove node */
      save = it->link[1];
      map->rel ? map->rel(it->data) : (void) 0;
      lc_free(it);
    } else {
      /* Rotate right */
      save = it->link[0];
      it->link[0] = save->link[1];
      save->link[1] = it;
    }

    it = save;
  }
  return SUCCESS;
}

int map_free(map_t *map) {
  if (!map) return ERR_INVAL;

  map_clear(map);
  lc_free(map);

  return SUCCESS;
}

void *map_find(map_t *map, void *data) {
  if (!map || !data) return NULL;

  map_node_t *it = map->root;

  while (it != sentinal) {
    int cmp = map->cmp(it->data, data);

    if (cmp == 0) break;

    it = it->link[cmp < 0];
  }

  return it->data;
}

/* will overwite and existing element with the same key */
long map_insert(map_t *map, void *data) {
  int err;
  if (!map || !data) return ERR_INVAL;

  if (map->root == sentinal) {
    /* Empty map case */
    if ((err = new_node(map, data, &map->root)) != SUCCESS) return err;
  } else {
    map_node_t *node = map->root;
    map_node_t *path[HEIGHT_LIMIT];
    int top = 0, dir, cmp;

    /* Find a spot and save the path */
    for (;;) {
      path[top++] = node;
      cmp = map->cmp(node->data, data);
      if (cmp == 0) {
        map->rel ? map->rel(node->data) : (void) 0;

        if (map->dup) {
          if ((err = map->dup(data,node->data)) != SUCCESS) {
            lc_free(node);
            return err;
          }
        } else {
          node->data = data;
        }

        return map->size;
      }
      dir = cmp < 0;

      if (node->link[dir] == sentinal) break;

      node = node->link[dir];
    }

    /* Create a new item */
    if ((err = new_node(map, data, &node->link[dir])) != SUCCESS) return err;

    /* Walk back and rebalance */
    while (--top >= 0) {
      /* Which child? */
      if (top != 0) dir = path[top - 1]->link[1] == path[top];

      skew ( path[top] );
      split ( path[top] );

      /* Fix the parent */
      if (top != 0)
        path[top - 1]->link[dir] = path[top];
      else
        map->root = path[top];
    }
  }

  ++map->size;

  return map->size;
}

void *map_remove(map_t *map, void *data) {
  if (!map || !data) return NULL;

  void *ret = NULL;

  if (map->root != sentinal) {
    map_node_t *it = map->root;
    map_node_t *path[HEIGHT_LIMIT];
    int top = 0, dir = 0, cmp;

    /* Find node to remove and save path */
    for (;;) {
      path[top++] = it;

      if (it == sentinal) return 0;

      cmp = map->cmp(it->data, data);
      if (cmp == 0) {
        ret = it-> data;
        break;
      }

      dir = cmp < 0;
      it = it->link[dir];
    }

    /* Remove the found node */
    if (it->link[0] == sentinal || it->link[1] == sentinal) {
      /* Single child case */
      int dir2 = it->link[0] == sentinal;

      /* Unlink the item */
      if (--top != 0)
        path[top - 1]->link[dir] = it->link[dir2];
      else
        map->root = it->link[1];

      //map->rel ? map->rel(it->data) : (void)0;
      lc_free(it);
    } else {
      /* Two child case */
      map_node_t *heir = it->link[1];
      map_node_t *prev = it;

      while (heir->link[0] != sentinal) {
        path[top++] = prev = heir;
        heir = heir->link[0];
      }

      /*
       Order is important!
       (free item, replace item, free heir)
       */
      //map->rel ? map->rel(it->data) : (void)0;
      it->data = heir->data;
      prev->link[prev == it] = heir->link[1];
      lc_free(heir);
    }

    /* Walk back up and rebalance */
    while (--top >= 0) {
      map_node_t *up = path[top];

      if (top != 0) dir = path[top - 1]->link[1] == up;

      /* Rebalance (aka. black magic) */
      if (up->link[0]->level < up->level - 1 || up->link[1]->level < up->level - 1) {
        if (up->link[1]->level > --up->level) up->link[1]->level = up->level;

        /* Order is important! */
        skew ( up );
        skew ( up->link[1] );
        skew ( up->link[1]->link[1] );
        split ( up );
        split ( up->link[1] );
      }

      /* Fix the parent */
      if (top != 0)
        path[top - 1]->link[dir] = up;
      else
        map->root = up;
    }
  }

  --map->size;

  return ret;
}

long map_size(map_t *map) {
  if (!map) return ERR_INVAL;
  return map->size;
}

static void anode_ordered(map_t *map, map_node_t *node, order_cb fn) {

  if (!node || node == sentinal || !fn) return;
  anode_ordered(map, node->link[0], fn);
  fn(node->data);
  anode_ordered(map, node->link[1], fn);
}

void map_ordered(map_t *map, order_cb fn) {
  anode_ordered(map, map->root, fn);
}

map_cursor_t *map_cursor(void) {
  return lc_alloc(sizeof(map_cursor_t));
}

void map_cursor_free(map_cursor_t *cur) {
  lc_free(cur);
}

/*
 First step in traversal,
 handles min and max
 */
static void *start(map_cursor_t *cur, map_t *map, int dir) {
  cur->map = map;
  cur->it = map->root;
  cur->top = 0;

  /* Build a path to work with */
  if (cur->it != sentinal) {
    while (cur->it->link[dir] != sentinal) {
      cur->path[cur->top++] = cur->it;
      cur->it = cur->it->link[dir];
    }
  }

  /* Could be nil, but nil->data == NULL */
  return cur->it->data;
}

/*
 Subsequent traversal steps,
 handles ascending and descending
 */
static void *move(map_cursor_t *cur, int dir) {

  if (cur->it->link[dir] != sentinal) {
    /* Continue down this branch */
    cur->path[cur->top++] = cur->it;
    cur->it = cur->it->link[dir];

    while (cur->it->link[!dir] != sentinal) {
      cur->path[cur->top++] = cur->it;
      cur->it = cur->it->link[!dir];
    }
  } else {
    /* Move to the next branch */
    map_node_t *last;

    do {
      if (cur->top == 0) {
        cur->it = sentinal;
        break;
      }

      last = cur->it;
      cur->it = cur->path[--cur->top];
    } while (last == cur->it->link[dir]);
  }

  /* Could be nil, but nil->data == NULL */
  return cur->it->data;
}

void *map_first(map_cursor_t *cur, map_t *map) {
  return start(cur, map, 0); /* Min value */
}

void *map_last(map_cursor_t *cur, map_t *map) {
  return start(cur, map, 1); /* Max value */
}

void *map_next(map_cursor_t *cur) {
  return move(cur, 1); /* Toward larger items */
}

void *map_prev(map_cursor_t *cur) {
  return move(cur, 0); /* Toward smaller items */
}
