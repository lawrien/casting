#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "casting.h"
#include "lc_thread.h"
#include "map.h"
#include "queue.h"
#include "lc_session.h"
#include "message.h"
#include "lc_channel.h"

static lc_threadpool_t *pool;
static lc_local_t *task_key;
static lc_spin_t *lock;
static map_t *sessions;

typedef struct _job {
  task_id tid;
  message_t *message;
} job_t;

static int dup_job(const void *a, void **n) {
  job_t *j = (job_t *) lc_alloc(sizeof(job_t));
  if (j) {
    memcpy(j, a, sizeof(job_t));
  }
  *n = j;
  return 0;
}

static void rel_job(void *d) {
  lc_free(d);
}

int session_close(session_id);

static int cmp_session(const void *a, const void *b) {
  int a_id = ((session_t *) a)->id;
  int b_id = ((session_t *) b)->id;
  return a_id == b_id ? 0 : a_id > b_id ? 1 : -1;
}

static int dup_session(const void *a, void **n) {
  session_t *d = (session_t *) lc_alloc(sizeof(session_t));
  if (d) {
    memcpy(d, a, sizeof(session_t));
  }
  *n = d;
  return 0;
}

static void rel_session(void *d) {
  lc_free(d);
}

static session_t *session_ref(session_id sid) {
  session_t f = { sid };
  lc_spin_lock(lock);
  session_t *s = map_find(sessions, &f);
  if (s) {
    atomic_int_inc(&s->ref_count);
  }
  lc_spin_unlock(lock);

  return s;
}

static session_t *session_free(session_id sid) {
  session_t f = { sid };
  do {
    lc_spin_lock(lock);
    session_t *s = map_find(sessions, &f);
    if (s) {
      if (atomic_int_dec(&s->ref_count) > 0) break;
      map_remove(sessions, s);
      lc_sem_destroy(s->sem);
      lc_spin_destroy(s->lock);
      queue_free(s->tasks);
      lua_close(s->state);
      lc_free(s);
      //printf("Freed session <%f>\n",sid);
    }
  } while (0);

  lc_spin_unlock(lock);

  return SUCCESS;
}

static void registerlib(lua_State *L, const char *name, lua_CFunction f) {
  lua_getglobal(L,"package");
  lua_getfield(L, -1, "preload");
  lua_pushcfunction(L,f);
  lua_setfield(L, -2, name);
  lua_pop(L,2);
}

// TODO Move this somewhere where it is common
// TODO allow the libs required to be specified
static void openlibs(lua_State *L) {
  lua_pushcfunction(L,luaopen_base);
  lua_pcall(L, 0, 0, 0);
  lua_pushcfunction(L,luaopen_package);
  lua_pcall(L, 0, 0, 0);
  lua_pushcfunction(L, luaopen_casting);
  lua_pcall(L, 0, 0, 0);
  // TODO allow io ? replace with non-blocking io ?
  registerlib(L, "io", luaopen_io);
  registerlib(L, "os", luaopen_os);
  registerlib(L, "table", luaopen_table);
  registerlib(L, "string", luaopen_string);
  registerlib(L, "math", luaopen_math);
  registerlib(L, "debug", luaopen_debug);
}

session_id session_new( ) {
  static session_id next = 0;

  session_t s = { };
  do {
    s.ref_count = 1;
    s.status = ready;
    s.state = luaL_newstate();
    openlibs(s.state);
    if (!s.state) {
      break;
    } else {
      // TODO this should be optional
      openlibs(s.state);
      s.lock = lc_spin_new();
      s.sem = lc_sem_new(0);
      s.tasks = queue_new(dup_job, rel_job);
    }
    lc_spin_lock(lock);
    s.id = ++next;
    map_insert(sessions, &s);
    lc_spin_unlock(lock);
  } while (0);

  return s.id;
}

lua_Session *get_session(lua_State *L, int idx) {
  return luaL_checkudata(L, idx, CASTING_SESSION);
}

session_id lc_createsession(lua_State *L) {

  session_id sid = session_new();
  if (sid > 0) {
    lua_Session *ls = (lua_Session *) lua_newuserdata(L, sizeof(lua_Session)); // [session]
    if (!ls) {
      session_close(sid);
      return FAIL;
    }

    ls->sid = sid;
    luaL_getmetatable(L,CASTING_SESSION); // [ud][meta]
    lua_setmetatable(L, -2);
  }

  return sid;
}

int session_close(session_id sid) {
  session_t *s = session_free(sid);

  if (s) {
    // simply wait until whoever is using it has finished
    lc_spin_lock(s->lock);
    lc_spin_destroy(s->lock);
    queue_free(s->tasks);
    lua_close(s->state);
  }
  return SUCCESS;
}

int session_wait(session_id sid) {
  session_t *s = session_ref(sid);
  lc_sem_wait(s->sem);
  session_free(sid);
  return SUCCESS;
}

static void session_thread(void *d) {
  session_id sid = *(session_id *) d;
  lc_free(d);

  session_run(sid);
}

int session_queue_task(task_id tid, message_t *m) {
  task_t *t = task_ref(tid);
  if (!t) return ERR_INVAL;
  session_id sid = t->sid;
  session_t *s = session_ref(sid);
  task_free(tid);
  if (!s) return ERR_INVAL;

  job_t job = { tid, m };
  do {
    lc_spin_lock(s->lock);
    queue_push(s->tasks, &job);
    if (s->status != ready) break;

    session_id *psid = lc_alloc(sizeof(session_id));
    *psid = sid;
    lc_threadpool_run(pool, session_thread, psid);
  } while (0);
  lc_spin_unlock(s->lock);

  session_free(sid);

  return SUCCESS;
}

int session_run(session_id sid) {
  session_t *s = session_ref(sid);
  if (!s) return ERR_INVAL;

  lc_spin_lock(s->lock);
  if (s->status == running) {
    lc_spin_unlock(s->lock);
    session_free(sid);
    return SUCCESS; // TODO ERR_RUNNING ??
  }

  s->status = running;
  job_t *job = queue_pop(s->tasks);
  lua_State *L = s->state;
  lc_spin_unlock(s->lock);

  if (job) {
    task_run(job->tid, L, job->message);
    lc_free(job);
  }

  // resubmit the session to the threadpool if there are more jobs on the session to be run
  lc_spin_lock(s->lock);
  s->status = ready;
  if (queue_size(s->tasks) == 0) {
    lc_sem_post(s->sem); // let the world know we're ready for more !!
  } else {
    session_id *psid = lc_alloc(sizeof(session_id));
    *psid = sid;
    lc_threadpool_run(pool, session_thread, psid);
  }
  lc_spin_unlock(s->lock);
  // finally, either unreference or destroy the session
  session_free(sid);
  return SUCCESS;
}

static int luaS_newsession(lua_State *L) {
  session_id sid = lc_createsession(L);
  if (sid < 1) {
    return luaL_error(L, "Error creating session. Insufficient memory ?");
  }
  return 1;
}

static int luaS_createtask(lua_State *L) {
  lua_Session *ls = get_session(L, 1);
  int top = lua_gettop(L);
  luaL_checktype(L,2,LUA_TFUNCTION);

  message_t *m = lua_newmessage(L, top - 1);
  if (!m) {
    return luaL_error(L,"Unable to encode parameters");
  }
  task_id tid = lc_createtask(L, ls->sid);
  if (tid == 0) {
    return luaL_error(L,"Unable to create task");
  }
  task_resume(tid,m);
  return 1;
}

static int luaS_close(lua_State *L) {
  lua_Session *ls = get_session(L, 1);
  session_close(ls->sid);
  return 0;
}

static int luas_wait(lua_State *L) {
  lua_Session *ls = get_session(L, 1);
  session_wait(ls->sid);
  return 0;
}

static int luaT_set_threads(lua_State *L) {
  int min = luaL_checkint(L, 1);
  int max = luaL_checkint(L, 2);

  lc_threadpool_set_threads(pool, min, max);

  return 0;
}

static int luaT_threads(lua_State *L) {
  int min = lc_threadpool_min(pool);
  int max = lc_threadpool_max(pool);

  lua_pushnumber(L, min);
  lua_pushnumber(L, max);
  return 2;
}

static int luas_destroy(lua_State *L) {
  lua_Session *ls = get_session(L, 1);
  session_free(ls->sid);
  return 0;
}

static int luas_tostring(lua_State *L) {
  lua_Session *ls = get_session(L, 1);
  lua_pushfstring(L, CASTING_SESSION " <%f>", ls->sid);
  return 1;
}

static luaL_Reg functions[] = { { "new", luaS_newsession },
                                 { "close", luaS_close },
                                 { "threads", luaT_threads },
                                 { "set_threads", luaT_set_threads },
                                 { NULL, NULL } };

static luaL_Reg session_meths[] = { { "__gc", luas_destroy },
                                     { "__tostring", luas_tostring },
                                     { "create", luaS_createtask },
                                    { "wait", luas_wait },
                                     { "close", luaS_close },
                                     { NULL, NULL } };

void init_session( ) {
  static int init = 0;

  while (!atomic_int_cas(&init, 1, 1)) {
    pool = lc_threadpool_new(1, 2);
    task_key = lc_local_new(NULL);
    lock = lc_spin_new();
    sessions = map_new(cmp_session, dup_session, rel_session);
    INFO("Initialized session");
    init = 1;
  }
}

int lc_open_session(lua_State *L) {
  init_session();
  lua_newtable(L); // [tbl]
  luaL_register(L, NULL, functions);

  if (luaL_newmetatable(L, CASTING_SESSION) == 1) {
    luaL_register(L, NULL, session_meths); // [tbl][tbl]
    lua_setfield(L, -1, "__index");
  }

  lc_open_task(L);

  return 0;
}
