#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#include <lua.h>
#include <lauxlib.h>

#include "casting.h"
#include "lc_thread.h"
#include "lc_channel.h"
#include "queue.h"
#include "map.h"
#include "message.h"
#include "lc_session.h"

static lc_spin_t *lock;
static map_t *channels;

typedef enum {
  channel_open = 1, channel_closed
} channel_status;

struct _channel {
  channel_id id;
  int ref_count;
  lc_spin_t *lock;
  int buf_size;
  channel_status status;
  queue_t *messages;
  queue_t *readers;
  queue_t *writers;
};

typedef struct _reader {
  channel_callback cb;
  void *data;
} reader_t;

typedef struct _writer {
  channel_callback cb;
  void *data;
  message_t *message;
} writer_t;

static inline int cmp_channel(const void *p1, const void *p2) {
  channel_t *a = (channel_t *) p1;
  channel_t *b = (channel_t *) p2;

  return a->id == b->id ? 0 : a->id > b->id ? 1 : -1;
}

static int dup_channel(const void *a, void **n) {
  channel_t *d = (channel_t *) lc_alloc(sizeof(channel_t));
  if (d) {
    memcpy(d, a, sizeof(channel_t));
  }
  *n = d;
  return 0;
}

static void rel_channel(void *d) {
  lc_free(d);
}

static void rel_message(void *d) {
  message_t *m = (message_t *) d;
  msg_destroy(m);
}

static int dup_reader(const void *a, void **d) {
  reader_t *r = lc_alloc(sizeof(reader_t));
  if (r) {
    memcpy(r, a, sizeof(reader_t));
  }
  *d = r;
  return SUCCESS;
}

static int dup_writer(const void *a, void **d) {
  writer_t *w = lc_alloc(sizeof(writer_t));
  if (w) {
    memcpy(w, a, sizeof(writer_t));
  }
  *d = w;
  return SUCCESS;
}

static void rel_reader(void *d) {
  reader_t *r = (reader_t *) d;
  r->cb(NULL, r->data, closed);
}

static void rel_writer(void *d) {
  writer_t *w = (writer_t *) d;
  w->cb(w->message, w->data, closed);
}

static channel_t *channel_find(channel_id cid) {
  channel_t f = { cid };
  lc_spin_lock(lock);
  channel_t *c = map_find(channels, &f);
  lc_spin_unlock(lock);

  return c;
}

static channel_t *channel_ref(channel_id cid) {
  channel_t f = { cid };
  lc_spin_lock(lock);
  channel_t *c = map_find(channels, &f);
  if (c) {
    atomic_int_inc(&c->ref_count);
  }
  lc_spin_unlock(lock);

  return c;
}

static void channel_free(channel_t *c) {
  do {
    lc_spin_lock(lock);

    if (c) {
      if (atomic_int_dec(&c->ref_count) > 0) break;
      map_remove(channels, c);
      lc_spin_destroy(c->lock);
      // TODO should each reader/writer be informed of the closure of the channel ??
      queue_clear(c->messages);
      queue_clear(c->readers);
      queue_clear(c->writers);
      c = lc_free(c);
    }
  } while (0);
  lc_spin_unlock(lock);
}

int channel_close(channel_t *c) {
  if (!c) return ERR_INVAL;

  lc_spin_lock(c->lock);
  if (c->status == channel_open) {
    queue_clear(c->messages);
    queue_clear(c->readers);
    queue_clear(c->writers);
    c->status = channel_closed;
  }
  lc_spin_unlock(c->lock);
  return SUCCESS;
}

channel_t *channel_new(int size) {
  static channel_id next_id = 0;

  channel_t c = { };

  c.ref_count = 1;
  c.buf_size = (size < 0) ? INT_MAX : size;
  c.status = channel_open;
  // TODO put an appropriate release routine on the queues
  c.messages = queue_new(NULL, rel_message);
  c.readers = queue_new(dup_reader, rel_reader);
  c.writers = queue_new(dup_writer, rel_writer);

  c.lock = lc_spin_new();
  lc_spin_lock(lock);
  c.id = ++next_id;
  map_insert(channels, &c);
  lc_spin_unlock(lock);

  return channel_find(c.id);
}

int channel_count(channel_id cid) {
  channel_t *c = channel_ref(cid);
  if (!c) return ERR_INVAL;

  lc_spin_lock(c->lock);
  int count = queue_size(c->messages);
  lc_spin_unlock(c->lock);

  channel_free(c);
  return count;
}

int channel_write(channel_t *c, message_t *message, channel_callback cb, void *data) {
  if (!c || !message || !cb) return ERR_INVAL;

  if (c->status == channel_closed) return ERR_CLOSED;

  reader_t *r;
  writer_t *w = NULL;
  message_t *m = NULL;

  int rc = SUCCESS;

  lc_spin_lock(c->lock);

  if (queue_size(c->messages) < c->buf_size) {
    queue_push(c->messages, message);
  } else {
    writer_t tmp = { cb, data, message };
    queue_push(c->writers, &tmp);
    rc = ERR_FULL;
  }
  r = queue_pop(c->readers);
  if (r) {
    m = queue_pop(c->messages);
    w = queue_pop(c->writers);
  }
  lc_spin_unlock(c->lock);

  if (rc == SUCCESS) {
    cb(message, data, write);
  }

  if (r) {
    if (!m) m = w->message;
    r->cb(m, r->data, read);
    if (w) {
      w->cb(w->message, w->data, write);
      lc_free(w);
    }
    lc_free(r);
  }

  return rc;
}

int channel_read(channel_t *c, channel_callback cb, void *data) {
  if (!c || !cb) return ERR_INVAL;

  if (c->status == channel_closed) return ERR_CLOSED;

  message_t *m;
  writer_t *w;
  int rc = SUCCESS;

  lc_spin_lock(c->lock);
  m = queue_pop(c->messages);
  w = queue_pop(c->writers);
  if (!m && !w) {
    reader_t r = { cb, data };
    queue_push(c->readers, &r);
    rc = ERR_EMPTY;
  }
  lc_spin_unlock(c->lock);

  if (rc == SUCCESS) {
    if (!m) m = w->message;
    cb(m, data, read);
    if (w) {
      w->cb(m, w->data, write);
      lc_free(w);
    }
  }

  return rc;
}

static lua_Channel *get_channel(lua_State *L, int idx) {
  lua_Channel *lc = (lua_Channel *) luaL_checkudata(L, idx, CASTING_CHANNEL);
  return lc;
}

int lua_pushchannel(lua_State *L, channel_t *c) {
  if (!c) return ERR_INVAL;

  // TODO since the channel is going to Lua, I should really bump it's ref_count here
  lua_Channel *lc = (lua_Channel *) lua_newuserdata(L, sizeof(lua_Channel)); // [ud]
  if (!lc) {
    return ERR_NOMEM;
  }

  lc->cid = c->id;
  luaL_getmetatable(L, CASTING_CHANNEL); // [ud][meta]
  lua_setmetatable(L, -2); // [ud]

  return SUCCESS;
}

static int luaC_new(lua_State *L) {
  int size = luaL_optint(L, 1, 0);
  channel_t *c = channel_new(size);
  if (!c) return luaL_error(L, "Unable to create new channel");

  if (lua_pushchannel(L, c) != SUCCESS) {
    channel_free(c);
    return luaL_error(L, "Unable to create new channel. Insufficient memory ?");
  }
  return 1;
}

static int luaC_connect(lua_State *L) {
  channel_id cid = luaL_checknumber(L, 1);

  channel_t *c = channel_ref(cid);
  if (lua_pushchannel(L, c) != SUCCESS) {
    return luaL_error(L, "Unable to connect to channel <%f>", cid);
  }
  return 1;
}

static int luac_close(lua_State *L) {
  lua_Channel *lc = get_channel(L, 1);
  channel_t *c = channel_ref(lc->cid);
  if (c) {
    channel_close(c);
    channel_free(c);
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int luac_status(lua_State *L) {
  lua_Channel *lc = get_channel(L, 1);
  channel_t *c = channel_ref(lc->cid);
  if (c) {
    switch (c->status) {
      case channel_open:
        lua_pushstring(L, "open");
        break;
      case channel_closed:
        lua_pushstring(L, "closed");
        break;
    }
  } else {
    lua_pushstring(L, "invalid");
  }
  channel_free(c);
  return 1;
}

static int luac_tostring(lua_State *L) {
  lua_Channel *lc = get_channel(L, 1);
  lua_pushfstring(L, CASTING_CHANNEL " <%f>", lc->cid);
  return 1;
}

static int luac_destroy(lua_State *L) {
  lua_Channel *lc = get_channel(L, 1);
  channel_t *c = channel_find(lc->cid);
  channel_free(c);
  return 1;
}

static int luac_save(lua_State *L) {
  lua_Channel *lc = get_channel(L, 1);
  lua_pushstring(L, CASTING_CHANNEL);
  lua_pushnumber(L, lc->cid);
  return 2;
}

static int luac_load(lua_State *L) {
  channel_id cid = lua_tonumber(L, 1);
  channel_t *c = channel_ref(cid);
  if (lua_pushchannel(L, c) == SUCCESS) {
    return 1;
  }

  lua_pushnil(L);
  return 1;
}

// TODO
// TODO
// TODO
typedef struct _session_cb {
  lua_State *L;
  lc_sem_t *sem;
} session_cb;

static void session_callback(message_t *m, void *data, channel_status_t event) {
  session_cb *s = (session_cb *) data;
  lua_State *L = s->L;
  switch (event) {
    case read:
      // TODO error check on message
      lua_decodemessage(L, m);
      lua_pushboolean(L, 1);
      break;
    case write:
      // Do nothing - we only want the message to be sent
      break;
    case closed:
      break;
  }
  lc_sem_post(s->sem);
}

// TODO
// TODO
// TODO
static void task_callback(message_t *m, void *data, channel_status_t event) {
  task_id *ptid = (task_id *) data;

  switch (event) {
    case read:
      task_resume(*ptid, m);
      break;
    case write:
      {
        message_builder_t mb;
        msg_builder_init(&mb);
        lc_pushboolean(&mb, 1);
        task_resume(*ptid, msg_new(&mb));
        msg_destroy(m);
      }
      break;
    case closed:
      break;
  }

  lc_free(data);
}

static int luac_write(lua_State *L) {
  lua_Channel *lc = get_channel(L, 1);

  // TODO error if channel is invalid or wont accept message !!
  channel_t *c = channel_ref(lc->cid);
  if (!c) {
    return luaL_error(L, "Invalid channel");
  }
  if (c->status == channel_closed) {
    lua_pushnil(L);
    lua_pushstring(L, "closed");
    channel_free(c);
    return 2;
  }
  task_id tid = task_current();

  int top = lua_gettop(L);
  message_t *m = lua_newmessage(L, top - 1);

  if (tid) {
    task_id *ptid = lc_alloc(sizeof(task_id));
    *ptid = tid;
    channel_write(c, m, task_callback, ptid);
    channel_free(c);
    return task_yield(tid);
  } else {
    session_cb s = { L, lc_sem_new(0) };
    if (channel_write(c, m, session_callback, &s) == ERR_FULL) {
      lc_sem_wait(s.sem);
    }
    lc_sem_destroy(s.sem);
    channel_free(c);
    lua_pushboolean(L, 1);
    return 1;
  }
}

static int luac_read(lua_State *L) {
  lua_Channel *lc = get_channel(L, 1);

  channel_t *c = channel_ref(lc->cid);
  if (!c) {
    return luaL_error(L, "Invalid channel");
  }
  if (c->status == channel_closed) {
    lua_pushnil(L);
    lua_pushstring(L, "closed");
    channel_free(c);
    return 2;
  }

  task_id tid = task_current();

  if (tid) {
    task_id *ptid = lc_alloc(sizeof(task_id));
    *ptid = tid;
    channel_read(c, task_callback, ptid);
    channel_free(c);
    return task_yield(tid);
  } else {
    session_cb s = { L, lc_sem_new(0) };
    if (channel_read(c, session_callback, &s) == ERR_FULL) {
      lc_sem_wait(s.sem);
    }
    lc_sem_destroy(s.sem);
    channel_free(c);
    lua_pushboolean(L, 1);
    return 1;
  }
}

static int luac_size(lua_State *L) {
  lua_Channel *lc = get_channel(L, 1);
  lua_pushnumber(L, channel_count(lc->cid));
  return 1;
}

static const luaL_Reg funcs[] = { { "new", luaC_new },
                                   { "write", luac_write },
                                   { "read", luac_read },
                                   { "connect", luaC_connect },
                                   { NULL, NULL } };

static const luaL_Reg methods[] = { { "__tostring", luac_tostring },
                                     { "__gc", luac_destroy },
                                     { "__len", luac_size },
                                     { "write", luac_write },
                                     { "read", luac_read },
                                     { "__save", luac_save },
                                     { "__load", luac_load },
                                     { "close", luac_close },
                                     { "status", luac_status },
                                     { NULL, NULL } };

void init_channel( ) {
  static int init = 0;

  while (!atomic_int_cas(&init, 1, 1)) {
    lock = lc_spin_new();
    channels = map_new(cmp_channel, dup_channel, rel_channel);
    INFO("Initialized channel");
    init = 1;
  }
}

int lc_open_channel(lua_State *L) {
  init_channel();
  lua_newtable(L); // [tbl]
  luaL_register(L, NULL, funcs); // [tbl]

  if (luaL_newmetatable(L, CASTING_CHANNEL) == 1) {
    luaL_register(L, NULL, methods); // [tbl][tbl]
    lua_setfield(L, -1, "__index");
  }
  return 0;
}
