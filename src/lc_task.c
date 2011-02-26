#include <string.h>
#include "lc_session.h"

static lc_local_t *task_key;
static lc_spin_t *lock;
static map_t *tasks;
static lc_local_t *task_key;

static void task_key_deleter(void *d) {
  lc_free(d);
}

void task_set_current(task_id tid) {
  task_id *ptid = lc_local_get(task_key);
  if (!ptid) {
    ptid = lc_alloc(sizeof(task_id));
    *ptid = tid;
    lc_local_set(task_key, ptid);
  }
}

task_id task_current( ) {
  task_id *ptid = lc_local_get(task_key);
  return (ptid) ? *ptid : 0;
}

static int cmp_task(const void *a, const void *b) {
  int a_id = ((task_t *) a)->id;
  int b_id = ((task_t *) b)->id;
  return a_id == b_id ? 0 : a_id > b_id ? 1 : -1;
}

static int dup_task(const void *a, void **n) {
  task_t *d = (task_t *) lc_alloc(sizeof(task_t));
  if (d) {
    memcpy(d, a, sizeof(task_t));
  }
  *n = d;
  return 0;
}

static void rel_task(void *d) {
  lc_free(d);
}

static task_t *tasks_find(task_id tid) {
  task_t f = { tid };
  lc_spin_lock(lock);
  task_t *t = map_find(tasks, &f);
  lc_spin_unlock(lock);
  return t;
}

task_t *task_ref(task_id tid) {
  task_t f = { tid };
  lc_spin_lock(lock);
  task_t *t = map_find(tasks, &f);
  if (t) {
    atomic_int_inc(&t->ref_count);
  }
  lc_spin_unlock(lock);

  return t;
}

int task_free(task_id tid) {
  task_t f = { tid };

  do {
    lc_spin_lock(lock);
    task_t *t = map_find(tasks, &f);
    if (t) {
      if (atomic_int_dec(&t->ref_count) > 0) break;
      map_remove(tasks, t);
      lc_free(t);
      //printf("Freed task <%f>\n",tid);
    }
  } while (0);

  lc_spin_unlock(lock);

  return SUCCESS;
}

task_id task_new(session_id sid) {
  static task_id next = 0;

  task_t t = { };
  do {
    t.sid = sid;
    t.ref_count = 1;
    t.L = NULL;
    t.lock = lc_spin_new();
    t.status = ready;

    lc_spin_lock(lock);
    t.id = ++next;
    map_insert(tasks, &t);
    lc_spin_unlock(lock);
  } while (0);

  return t.id;
}

int task_run(task_id tid, lua_State *L, message_t *m) {
  task_t *t = task_ref(tid);
  if (!t) return ERR_INVAL;

  task_set_current(tid);

  int rc = 0;
  int count;
  switch (t->status) {
    case ready:
      if (!m) {
        t->status = finished;
        break;
      }
      t->L = lua_newthread(L);
      count = lua_decodemessage(t->L, m) - 1;
      msg_destroy(m);
      t->status = running;
      STACK(t->L,"Resume from ready %f\n",t->id);
      rc = lua_resume(t->L, count);
      break;
    case suspended:
      count = m ? lua_decodemessage(t->L, m) : 0;
      if (m) msg_destroy(m);
      t->status = running;
      STACK(t->L,"Resume from suspended %f\n",t->id);
      rc = lua_resume(t->L, count);
      break;
    default:
      break;
  }

  if (rc == LUA_ERRRUN) {
    INFO("Error code %d",rc);
    t->status = error;
    STACK(t->L,"Error running task");
  } else if (rc == LUA_YIELD) {
    STACK(t->L,"YIELDED (ref = %d)",t->ref_count);
    t->status = suspended; // TODO YIELD
  } else if (rc == 0) {
    STACK(t->L,"QUITTED (ref = %d)",t->ref_count);
    t->status = finished;
  }

  // TODO task->coro = get current coroutine
  task_set_current(0);
  task_free(tid);
  // TODO handle rc
  return SUCCESS;
}

int task_yield(task_id tid) {
  task_t *t = task_ref(tid);
  if (!t) return ERR_INVAL;
  lua_State *L = t->L;
  task_free(tid);
  //task_resume(tid,m);
  return lua_yield(L,0);
}

int task_resume(task_id tid, message_t *m) {
  task_t *t = task_ref(tid);
  if (!t) return ERR_INVAL;
  int status = t->status;
  task_free(tid);
  if (status == finished || status == error) return ERR_TASKSTATE;

  return session_queue_task(tid, m);
}

task_id lc_createtask(lua_State *L, session_id sid) {
  task_id tid = task_new(sid);
  if (tid > 0) {
    lua_Task *lt = (lua_Task *) lua_newuserdata(L, sizeof(lua_Task)); // [task]
    if (!lt) {
      task_free(tid);
      return FAIL;
    }

    lt->tid = tid;
    luaL_getmetatable(L,CASTING_TASK); // [ud][meta]
    lua_setmetatable(L, -2);
  }

  return tid;
}

lua_Task *get_task(lua_State *L, int idx) {
  return luaL_checkudata(L, idx, CASTING_TASK);
}

static int luat_resume(lua_State *L) {
  lua_Task *lt = get_task(L, 1);
  int top = lua_gettop(L);
  message_t *m = lua_newmessage(L, top - 1);
  int rc = task_resume(lt->tid, m);
  if (rc != SUCCESS) {
    return luaL_error(L, "Unable to resume task %d", rc);
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int luat_destroy(lua_State *L) {
  lua_Task *lt = get_task(L, 1);
  task_free(lt->tid);
  return 0;
}

static int luat_tostring(lua_State *L) {
  lua_Task *lt = get_task(L, 1);
  lua_pushfstring(L, CASTING_TASK " <%f>", lt->tid);
  return 1;
}

static luaL_Reg task_meths[] = { { "__gc", luat_destroy },
                                  { "__tostring", luat_tostring },
                                  { "resume", luat_resume },
                                  { NULL, NULL } };

static void init_task( ) {
  static int init = 0;

  while (!atomic_int_cas(&init, 1, 1)) {
    task_key = lc_local_new(task_key_deleter);
    lock = lc_spin_new();
    tasks = map_new(cmp_task, dup_task, rel_task);
    INFO("Initialized task");
    init = 1;
  }
}

int lc_open_task(lua_State *L) {
  init_task();

  if (luaL_newmetatable(L, CASTING_TASK) == 1) {
    luaL_register(L, NULL, task_meths); // [tbl][tbl]
    lua_setfield(L, -1, "__index");
  }
  return 0;
}
