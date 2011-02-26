#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <lua.h>

#include "casting.h"
#include "lc_error.h"
#include "buffer.h"
#include "map.h"

#define CASTING_MESSAGE       "casting.message"

typedef int type_t;

#define T_NIL           (type_t)0x01
#define T_TRUE          (type_t)0x02
#define T_FALSE         (type_t)0x03
#define T_NUMBER        (type_t)0x04
#define T_STRING        (type_t)0x05
#define T_TABLE         (type_t)0x06
#define T_FUNCTION      (type_t)0x07
#define T_USERDATA      (type_t)0x08
#define T_LUSERDATA     (type_t)0x09
#define T_REFERENCE     (type_t)0x0a

#define T_REFERENCED    (type_t)0x20
#define T_META          (type_t)0x40

#define T_TYPEMASK      ~(T_META | T_REFERENCED)

typedef struct _value {
  type_t type;
  union {
    double number;
    int ref;
    struct {
      int array;
      int slots;
    } table;
    int upvals;
  } data;
  int len;
  const char *ptr;
  int next;
} value_t;

#define value_type(v) ((v)->type & T_TYPEMASK)
#define value_is_meta(v) ((v)->type & T_META)
#define value_is_referenced(v) ((v)->type & T_REFERENCED)

#define lc_toboolean(v) ((v)->type & T_TRUE ? 1 : 0)
#define lc_tonumber(v) ((v)->data.number)
#define lc_tolstring(v,sz) (((sz) = v->len) ? (v)->ptr : NULL)

#define BUILDER_SIZE  16

typedef struct _message_builder {
  int last;
  int size;
  int bytes;
  int count;
  map_t *refmap;
  int refs;
  value_t *values;
  value_t buf[BUILDER_SIZE];
} message_builder_t;

typedef struct _message {
  int ref_count;
  int size;
  int count;
  int refs;
  char data[0];
} message_t;

typedef struct _msg_cursor {
  const message_t *msg;
  int pos;
  int count;
} msg_cursor_t;


#define MSG_NEXT -1

int lc_pushnil(message_builder_t *mb);
int lc_pushboolean(message_builder_t *mb, int b);
int lc_pushnumber(message_builder_t *mb, lua_Number n);
int lc_pushlstring(message_builder_t *mb, const char *p, size_t sz);
int lc_pushreference(message_builder_t *mb, int ref);
int lc_createtable(message_builder_t *mb);
int lc_pushfunction(message_builder_t *mb, buffer_t *b);
int lc_setfunction(message_builder_t *mb, int idx, int upvals);
int lc_pushuserdata(message_builder_t *mb, const char *name, int len);
int lc_setuserdata(message_builder_t *mb, int idx, int upvals);
int lc_settable(message_builder_t *mb, int idx, size_t slots, size_t array);
int lc_setmeta(message_builder_t *mb, int idx);

#define msg_cursor_init(c,m) do { \
  (c)->msg = (m); \
  (c)->pos = 0; \
  (c)->count = 0; \
} while (0)

#define msg_builder_init(mb) do { \
    (mb)->size = sizeof((mb)->buf) / sizeof(value_t); \
    (mb)->last = 0; \
    (mb)->bytes = 0; \
    (mb)->count = 0; \
    (mb)->refs = 0; \
    (mb)->refmap = NULL; \
    (mb)->values = (mb)->buf; \
} while(0)

message_t *msg_new(message_builder_t *mb);
message_t *msg_ref(message_t *m);
int msg_count(const message_t *m);
int msg_destroy(message_t *m);

int msg_next(msg_cursor_t *c, value_t *v);

message_t *lua_newmessage(lua_State *L, int count);
int lua_decodemessage(lua_State *L, const message_t *m);
int lua_pushmessage(lua_State *L, message_t *m);

#endif // __MESSAGE_H__
