#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>

#include "casting.h"
#include "message.h"
#include "map.h"

typedef struct _lua_Message {
  message_t *msg;
} lua_Message;

typedef struct {
  const void *key;
  int refid;
} refdata_t;

static int cmp_ref(const void *a, const void *b) {
  const void *a_key = ((refdata_t *) a)->key;
  const void *b_key = ((refdata_t *) b)->key;
  return (a_key == b_key) ? 0 : (a_key > b_key) ? 1 : -1;
}

static int dup_ref(const void *a, void **data) {
  void *d = lc_alloc(sizeof(refdata_t));
  if (!d) return ERR_NOMEM;
  memcpy(d, a, sizeof(refdata_t));
  *data = d;
  return SUCCESS;
}

static void rel_ref(void *a) {
  lc_free(a);
}

static inline int ref_check(message_builder_t *mb, const void *p) {
  if (!mb->refmap) {
    mb->refmap = map_new(cmp_ref, dup_ref, rel_ref);

    //lua_rawgeti(s->L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS); // [globs]
    //lua_pushvalue(L, LUA_GLOBALSINDEX);

    //ref_data_t r = { lua_topointer(s->L, -1), REF_GLOBALS };
    //map_insert(s->refs, &r);
    //lua_pop(s->L,1); // []
  }

  refdata_t f = { p, mb->last };

  refdata_t *d = (refdata_t *) map_find(mb->refmap, &f);
  if (d) {
    lc_pushreference(mb, d->refid);
    ++mb->refs;
    return SUCCESS;
  } else {
    map_insert(mb->refmap, &f);
    return FAIL;
  }
}

static inline lua_Message *get_message(lua_State *L, int idx) {
  lua_Message *p = (lua_Message *) lua_touserdata(L, idx);
  if (p) {
    if (lua_getmetatable(L, idx)) {
      if (lua_rawequal(L, -1, lua_upvalueindex(1))) {
        lua_pop(L, 1);
        return p;
      }
    }
  }
  luaL_typerror(L, idx, CASTING_MESSAGE);
  return NULL;
}

int lua_pushmessage(lua_State *L, message_t *message) {
  if (!message) return ERR_INVAL;

  lua_Message *lm = (lua_Message *) lua_newuserdata(L, sizeof(lua_Message)); // [ud]
  if (!lm) {
    return ERR_NOMEM;
  }

  lm->msg = message;
  luaL_getmetatable(L, CASTING_MESSAGE); // [ud][meta]

  lua_setmetatable(L, -2); // [ud]

  return SUCCESS;
}

static inline int function_writer(lua_State *L, const void *p, size_t sz, void *ud) {
  buffer_t *b = (buffer_t *) ud;
  buf_write(b, p, sz);
  return SUCCESS;
}

static inline int write_value(message_builder_t *mb, lua_State *L, int idx) {

  switch (lua_type(L, idx)) {
    case LUA_TNIL:
      lc_pushnil(mb);
      break;
    case LUA_TBOOLEAN:
      lc_pushboolean(mb, lua_toboolean(L, idx));
      break;
    case LUA_TNUMBER:
      lc_pushnumber(mb, lua_tonumber(L, idx));
      break;
    case LUA_TSTRING:
      {
        size_t sz;
        const char *str = lua_tolstring(L, idx, &sz);
        lc_pushlstring(mb, str, sz);
      }
      break;
    case LUA_TTABLE:
      {
        const char *ptr = lua_topointer(L, idx);
        if (ref_check(mb, ptr) == SUCCESS) break;
        int tbl_idx = lc_createtable(mb);

        idx = lc_absindex(L,idx);
        int slots = 0;
        int array = lua_objlen(L, idx);
        lua_pushnil(L); // [key]

        if (!lua_checkstack(L, 2)) {
          printf("checkstack error !\n");
          exit(0);
        }
        while (lua_next(L, idx) != 0) { // ([key][val] | [])
          write_value(mb, L, -2); // [key][val]
          write_value(mb, L, -1); // [key][val]
          slots++;
          lua_pop(L, 1); // [key]
        }
        lc_settable(mb, tbl_idx, slots, array);
        if (lua_getmetatable(L, idx)) {
          int meta = write_value(mb, L, -1);
          lua_pop(L,1);
          lc_setmeta(mb, meta);
        }
      }
      break;
    case LUA_TFUNCTION:
      {
        buffer_t buf;
        buf_init(&buf);
        lua_pushvalue(L, idx); // [fn]
        lua_dump(L, function_writer, &buf);
        int f_idx = lc_pushfunction(mb, &buf);
        lua_Debug ar;
        lua_getinfo(L, ">u", &ar); // [fn]

        for (int i = 0; i < ar.nups; i++) {
          if (lua_getupvalue(L, idx, i + 1)) {
            write_value(mb, L, -1);
            lua_pop(L,1); // [fn]
          } else {
            break; // TODO raise an error more like !!
          }
        }
        lc_setfunction(mb, f_idx, ar.nups);
        return f_idx;
      }
      break;
    case LUA_TUSERDATA:
      if (luaL_getmetafield(L, idx, "__save")) { //[fn]
        lua_pushvalue(L, idx); // [fn][ud]
        int top = lua_gettop(L) - 2; // discount the [fn][ud] on the stack
        lua_call(L, 1, LUA_MULTRET);
        int newtop = lua_gettop(L);
        // get the metatable name - first argument on top of stack
        size_t sz;
        const char *name = lua_tolstring(L, top + 1, &sz);
        int ud_idx = lc_pushuserdata(mb, name, sz);

        for (int i = top + 1; i < newtop; i++) {
          write_value(mb, L, i + 1);
        }
        lc_setuserdata(mb, ud_idx, newtop - (top + 1));
      } else {
        return ERR_UNSUPPORTED;
      }
      break;
    default:
      printf("Unsupported type %d\n", lua_type(L, idx));
      return ERR_INVAL;
  }
  return SUCCESS;
}

message_t *lua_newmessage(lua_State *L, int count) {
  //TODO catch errors
  message_builder_t mb;
  msg_builder_init(&mb);

  int top = lua_gettop(L);

  // TODO catch errors
  for (int idx = (top - count) + 1; idx <= top; idx++) {
    write_value(&mb, L, idx);
  }

  message_t *msg = msg_new(&mb);
  lua_pop(L,count);
  return msg;
}

static inline int decode_value(lua_State *L, msg_cursor_t *cur, int top) {
  if (!cur) return ERR_INVAL;

  value_t v = { };
  int count = msg_next(cur, &v);
  if (count < SUCCESS) return -1;

  lua_checkstack(L, 1);

  switch (value_type(&v)) {
    case T_NIL:
      lua_pushnil(L);
      break;
    case T_TRUE:
      lua_pushboolean(L, 1);
      break;
    case T_FALSE:
      lua_pushboolean(L, 0);
      break;
    case T_NUMBER:
      lua_pushnumber(L, v.data.number);
      break;
    case T_STRING:
      {
        int sz = v.len;
        const char *p = v.ptr;
        lua_pushlstring(L, p, sz);
      }
      break;
    case T_TABLE:
      {
        lua_createtable(L, v.data.table.array, v.data.table.slots);
        if (v.type & T_REFERENCED) {
          lua_pushvalue(L, -1);
          lua_rawseti(L, top, count);
        }
        for (int i = v.data.table.slots; i; --i) {
          decode_value(L, cur, top); // key
          decode_value(L, cur, top); // value
          lua_rawset(L, -3);
        }
      }
      break;
    case T_FUNCTION:
      luaL_loadbuffer(L, v.ptr, v.len, "Something"); // [fn]
      for (int i = 0; i < v.data.upvals; i++) {
        decode_value(L, cur, top);
        lua_setupvalue(L, -2, i + 1); // [fn]
      }
      break;
    case T_USERDATA:
      lua_pushlstring(L, v.ptr, v.len); // [metaname]
      lua_gettable(L, LUA_REGISTRYINDEX); // [meta]
      lua_getfield(L, -1, "__load");
      lua_remove(L, -2); // get rid of metatable
      for (int i = 0; i < v.data.upvals; i++) {
        decode_value(L, cur, top);
      }
      lua_call(L, v.data.upvals, 1);
      break;
    case T_REFERENCE:
      STACK(L,"Got ref ",v.data.ref);
      lua_rawgeti(L, top, v.data.ref);
      STACK(L,"Done ref ",v.data.ref);
      break;
    default:
      printf("Unknown value %d\n", value_type(&v));
      return FAIL;
  }

  if (value_is_meta(&v)) {
    lua_setmetatable(L, -2);
  }

  return SUCCESS;
}

int lua_decodemessage(lua_State *L, const message_t *m) {
  if (!m) return ERR_INVAL;

  if (m->count) {
    msg_cursor_t cur;

    msg_cursor_init(&cur,m);
    if (m->refs) lua_createtable(L, 10, 0);
    int top = lua_gettop(L);

    while (decode_value(L, &cur, top) == SUCCESS) {
    }

    if (m->refs) lua_remove(L, top - 1);
    return msg_count(m);
  }
  lua_pushnil(L);
  return 1;
}

static int luaM_newmessage(lua_State *L) {
  int top = lua_gettop(L);


  message_t *m = lua_newmessage(L, top);

  if (!m) return luaL_error(L, "Unable to create new message - error %d", lc_err);

  lua_Message *lm = (lua_Message *) lua_newuserdata(L, sizeof(lua_Message)); // [ud]
  if (!lm) {
    return ERR_NOMEM;
  }

  lm->msg = m;
  lua_pushvalue(L, lua_upvalueindex(1)); // [ud][meta]
  lua_setmetatable(L, -2); // [ud]
  return 1;
}

static int luam_destroy(lua_State *L) {
  lua_Message *lm = get_message(L, 1);
  msg_destroy(lm->msg);
  return 1;
}

static int luam_tostring(lua_State *L) {
  lua_Message *lm = get_message(L, 1);
  lua_pushfstring(L, CASTING_MESSAGE " <%p>", lm->msg);
  return 1;
}

static int luam_len(lua_State *L) {
  lua_Message *lm = get_message(L, 1);
  lua_pushnumber(L, msg_count(lm->msg));
  return 1;
}

static int luam_save(lua_State *L) {
  lua_Message *lm = get_message(L, 1);
  lua_pushstring(L, CASTING_MESSAGE);
  lua_pushfstring(L, "Hello Mum (#m=%d)", lm->msg->count);
  return 2;
}

static int luam_load(lua_State *L) {
  size_t sz;
  const char *p = lua_tolstring(L, 1, &sz);
  printf("Loaded %s", p);
  return 0;
}

static int luaM_decode(lua_State *L) {
  lua_Message *lm = get_message(L, 1);

  int count = lua_decodemessage(L, lm->msg);
  return count;
}

static const luaL_Reg funcs[] =
    { { "new", luaM_newmessage }, { "decode", luaM_decode }, { NULL, NULL } };

static const luaL_Reg meths[] = { { "__gc", luam_destroy },
                                   { "__tostring", luam_tostring },
                                   { "__len", luam_len },
                                   { "decode", luaM_decode },
                                   { "__save", luam_save },
                                   { "__load", luam_load },
                                   { NULL, NULL } };

int lc_open_message(lua_State *L) {
  lua_newtable(L); // [tbl]

  if (luaL_newmetatable(L, CASTING_MESSAGE) != 0) { // [tbl][meta]
    lc_register_closures(L, -1, 1, meths);
    lua_pushvalue(L, -1); // [tbl][meta][meta]
    lua_setfield(L, -1, "__index"); // [tbl][meta]
  }

  lc_register_closures(L, -2, 1, funcs); // [tbl][meta]
  lua_pop(L,1); // [tbl]

  return SUCCESS;
}
