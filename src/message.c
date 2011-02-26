#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>

#include "casting.h"
#include "lc_thread.h"
#include "message.h"

static int next_index(message_builder_t *mb) {
  if (mb->last == mb->size) {
    int new_size = mb->size << 1;
    value_t *n = lc_alloc(sizeof(value_t) * new_size);
    if (n == NULL) return ERR_NOMEM; // TODO error checking here ?
    memcpy(n, mb->values, mb->last * sizeof(value_t));
    if (mb->values != mb->buf) lc_free(mb->values);
    mb->values = n;
    mb->size = new_size;
  }
  return mb->last++;
}

value_t *msg_value(message_builder_t *mb, int *idx) {
  if (!mb || !idx || *idx < MSG_NEXT) return NULL; // TODO raise error
  if (*idx = MSG_NEXT) {
    *idx = MSG_NEXT;
  }

  return &mb->values[*idx];
}

#define mem_write(s,v) do { \
  memcpy(*(s),&(v),sizeof(v)); \
  *(s) = *(s) + sizeof(v); } while(0)

#define mem_writes(s,d,l) do { \
    memcpy(*(s),(d),(l)); \
    *(s) = *(s) + (l); } while(0)

message_t *msg_new(message_builder_t *mb) {
  if (!mb) return NULL;
  message_t *msg;

  size_t v_size = mb->last;
  msg = lc_alloc(sizeof(message_t) + mb->bytes);
  msg->ref_count = 1;
  msg->count = mb->count;
  msg->refs = mb->refs;
  msg->size = sizeof(message_t) + mb->bytes;

  char *p = msg->data;

  for (int i = 0; i < mb->last; i++) {
    value_t *v = &mb->values[i];
    mem_write(&p, v->type);

    switch (value_type(v)) {
      case T_NIL:
      case T_TRUE:
      case T_FALSE:
        break;
      case T_NUMBER:
        mem_write(&p, v->data.number);
        break;
      case T_STRING:
        mem_write(&p, v->len);
        mem_writes(&p, v->ptr, v->len);
        break;
      case T_TABLE:
        mem_write(&p, v->data.table.slots);
        mem_write(&p, v->data.table.array);
        break;
      case T_USERDATA:
      case T_FUNCTION:
        mem_write(&p, v->data.upvals);
        mem_write(&p, v->len);
        mem_writes(&p, v->ptr, v->len);
        lc_free((void *)v->ptr); // remove the buffer
        break;
      case T_REFERENCE:
        mem_write(&p, v->data.ref);
        break;
      default:
        break;
    }
  }

  if (mb->values != mb->buf) lc_free(mb->values);
  if (mb->refmap) map_free(mb->refmap);

  return msg;
}

message_t *msg_ref(message_t *m) {
  if (m) {
    atomic_int_inc(&m->ref_count);
  }
  return m;
}

int msg_destroy(message_t *m) {
  if (!m) return ERR_INVAL;

  if (atomic_int_dec(&m->ref_count) == 0) {
    lc_free(m);
  }
  return SUCCESS;
}

int lc_pushnil(message_builder_t *mb) {
  int idx = next_index(mb);
  value_t *v = &mb->values[idx];
  v->type = T_NIL;
  mb->bytes += sizeof(v->type);
  ++mb->count;
  return idx;
}

int lc_pushboolean(message_builder_t *mb, int b) {
  int idx = next_index(mb);
  value_t *v = &mb->values[idx];
  v->type = b ? T_TRUE : T_FALSE;
  mb->bytes += sizeof(v->type);
  ++mb->count;
  return idx;
}

int lc_pushnumber(message_builder_t *mb, lua_Number n) {
  int idx = next_index(mb);
  value_t *v = &mb->values[idx];
  v->type = T_NUMBER;
  v->data.number = n;
  mb->bytes += sizeof(v->type) + sizeof(v->data.number);
  ++mb->count;
  return idx;
}

int lc_pushlstring(message_builder_t *mb, const char *p, size_t sz) {
  int idx = next_index(mb);
  value_t *v = &mb->values[idx];
  v->type = T_STRING;
  v->len = sz;
  v->ptr = p;
  mb->bytes += sizeof(v->type) + sizeof(v->len) + sz;
  ++mb->count;
  return idx;
}

int lc_pushreference(message_builder_t *mb, int ref) {
  // TODO check it's in bounds
  ++mb->refs;
  mb->values[ref].type |= T_REFERENCED;

  int idx = next_index(mb);
  value_t *v = &mb->values[idx];
  v->type = T_REFERENCE;
  v->data.ref = ref;
  mb->bytes += sizeof(v->data.ref);
  ++mb->count;
  return idx;
}

int lc_createtable(message_builder_t *mb) {
  int idx = next_index(mb);
  value_t *v = &mb->values[idx];
  v->type = T_TABLE;
  v->data.table.array = 0;
  v->data.table.slots = 0;
  mb->bytes += sizeof(v->type) + sizeof(v->data.table.array) + sizeof(v->data.table.slots);
  ++mb->count;
  return idx;
}

int lc_pushfunction(message_builder_t *mb, buffer_t *b) {
  int idx = next_index(mb);
  value_t *v = &mb->values[idx];
  v->type = T_FUNCTION;
  v->data.upvals = 0;
  v->len = buf_size(b);
  v->ptr = buf_release(b);
  mb->bytes += sizeof(v->type) + sizeof(v->data.upvals) + sizeof(v->len) + v->len;
  ++mb->count;
  return idx;
}

int lc_setfunction(message_builder_t *mb, int idx, int upvals) {
  value_t *v = &mb->values[idx];
  v->data.upvals = upvals;
  mb->count -= upvals;
  return 0;
}

int lc_pushuserdata(message_builder_t *mb, const char *name, int len) {
  int idx = next_index(mb);
  value_t *v = &mb->values[idx];
  v->type = T_USERDATA;
  v->data.upvals = 0;
  v->len = len;
  char *n = lc_alloc(len);
  memcpy(n, name, len);
  v->ptr = n;
  mb->bytes += sizeof(v->type) + sizeof(v->data.upvals) + sizeof(v->len) + v->len;
  ++mb->count;
  return idx;
}

int lc_setuserdata(message_builder_t *mb, int idx, int upvals) {
  value_t *v = &mb->values[idx];
  v->data.upvals = upvals;
  mb->count -= upvals;
  return 0;
}

int lc_settable(message_builder_t *mb, int idx, size_t slots, size_t array) {
  // TODO some checking, Vicar ?
  value_t *v = &mb->values[idx];
  v->data.table.slots = slots;
  v->data.table.array = array;
  mb->count -= (slots * 2);
  return 0;
}

int lc_setmeta(message_builder_t *mb, int idx) {
  // TODO check that the recipitent is either a table or a table reference ?
  value_t *v = &mb->values[idx];

  v->type |= T_META;
  --mb->count;
  return 0;
}

#define mem_read(v , s) do { \
  memcpy(&(v) , *(s),sizeof(v)); \
  *(s) = *(s) + sizeof(v); } while(0)

#define mem_reads(d,s,l) do { \
    memcpy((d),*(s),(l)); \
    *(s) = *(s) + (l); } while(0)

int msg_next(msg_cursor_t *c, value_t *v) {
  if (!c || !v || !c->msg) return ERR_INVAL;

  const message_t *m = c->msg;
  const char *p = &m->data[c->pos];

  if (p >= (const char *) m + m->size) return -1;

  mem_read(v->type,&p);
  switch (value_type(v)) {
    case T_NIL:
    case T_TRUE:
    case T_FALSE:
      break;
    case T_NUMBER:
      mem_read(v->data.number,&p);
      break;
    case T_STRING:
      mem_read(v->len,&p);
      v->ptr = p;
      p += v->len;
      break;
    case T_TABLE:
      mem_read(v->data.table.slots,&p);
      mem_read(v->data.table.array,&p);
      break;
    case T_USERDATA:
    case T_FUNCTION:
      mem_read(v->data.upvals,&p );
      mem_read(v->len,&p);
      v->ptr = p;
      p += v->len;
      break;
    case T_REFERENCE:
      mem_read(v->data.ref,&p);
      break;
    default:
      return -1;
  }
  c->pos = p - m->data;
  return c->count++;
}

int msg_count(const message_t *m) {
  if (!m) return ERR_INVAL;
  return m->count;
}

//int msg_destroy(message_t *m) {
//  if (!m) return ERR_INVAL;
//
//  lc_free(m);
//
//  return SUCCESS;
//}
