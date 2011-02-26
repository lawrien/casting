#ifndef __LC_SESSION_H__
#define __LC_SESSION_H__

#include <lua.h>
#include "casting.h"
#include "message.h"
#include "lc_thread.h"
#include "lc_task.h"
#include "queue.h"

#define CASTING_SESSION "casting.session"
#define CASTING_TASK  "casting.task"

typedef enum {
  ready=1,running,suspended,finished,error
} status_t;

typedef double session_id;
typedef double task_id;

typedef struct _session {
  session_id id;
  int ref_count;
  lc_spin_t *lock;
  lua_State *state;
  status_t status;
  lc_sem_t *sem;
  queue_t *tasks;
} session_t;

typedef struct _task {
  task_id id;
  session_id sid;
  int ref_count;
  lua_State *L;
  lc_spin_t *lock;
  status_t status;
} task_t;

typedef struct {
  session_id sid;
} lua_Session;

typedef struct {
  task_id tid;
} lua_Task;

int lc_open_task(lua_State *L);

session_id session_new( );
int session_queue_task(task_id tid,message_t *m);
int session_run(session_id sid);

session_id lc_createsession(lua_State *L);
task_id lc_createtask(lua_State *L, session_id sid);

task_id task_new(session_id sid);
task_t *task_ref(task_id tid);
int task_free(task_id tid);

task_id task_current( );
void task_set_current(task_id tid);
int task_run(task_id tid,lua_State *L,message_t *m);
int task_resume(task_id tid, message_t *m);
int task_yield(task_id tid);

#endif // __LC_SESSION_H__
