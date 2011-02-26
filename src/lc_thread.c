#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <semaphore.h>

#include "lc_thread.h"
#include "lc_error.h"
#include "map.h"
#include "queue.h"
#include "casting.h"

void *atomic_ptr_get(volatile void *addr) {
  __sync_synchronize();
  void *p = *(void **) addr;
  return p;
}

void atomic_ptr_set(volatile void *addr, void *p) {
  __sync_synchronize();
  *(void **) addr = p;
}

int atomic_ptr_cas(volatile void *addr, void *o, void *n) {
  return __sync_bool_compare_and_swap((volatile void **) addr, o, n);
}

int atomic_int_get(volatile int *addr) {
  __sync_synchronize();
  int i = *addr;
  return i;
}

void atomic_int_set(volatile int *addr, int i) {
  __sync_synchronize();
  *addr = i;
}

int atomic_int_cas(volatile int *addr, int o, int n) {
  return __sync_bool_compare_and_swap(addr, o, n);
}

int atomic_int_tas(volatile int *addr, int n) {
  return __sync_lock_test_and_set(addr, n);
}

int atomic_int_add(volatile int *addr, int i) {
  return __sync_add_and_fetch(addr, i);
}

int atomic_int_sub(volatile int *addr, int i) {
  return __sync_sub_and_fetch(addr, i);
}

int atomic_int_inc(volatile int *addr) {
  return atomic_int_add(addr, 1);
}

int atomic_int_dec(volatile int *addr) {
  return atomic_int_sub(addr, 1);
}

struct _lc_spin {
  volatile int s;
};

static inline int lc_spin_init(lc_spin_t *spin) {
  spin->s = 0;
  return SUCCESS;
}

lc_spin_t *lc_spin_new( ) {
  lc_spin_t *s = lc_alloc(sizeof(lc_spin_t));
  if (!s) return NULL;
  lc_spin_init(s);
  return s;
}

int lc_spin_lock(lc_spin_t *s) {
  if (!s) return ERR_INVAL;
  while (!atomic_int_cas(&s->s, 0, 1)) {
    sched_yield();
  }
  return SUCCESS;
}

int lc_spin_trylock(lc_spin_t *s) {
  if (!s) return ERR_INVAL;
  if (atomic_int_cas(&s->s, 0, 1)) {
    return SUCCESS;
  }
  return FAIL;
}

int lc_spin_unlock(lc_spin_t *s) {
  if (!s) return ERR_INVAL;
  atomic_int_tas(&s->s, 0);
  return SUCCESS;
}

int lc_spin_destroy(lc_spin_t *s) {
  // this is a no-op
  return SUCCESS;
}

struct _lc_mutex {
  pthread_mutex_t lock;
};

lc_mutex_t *lc_mutex_new( ) {
  lc_mutex_t *mtx = lc_alloc(sizeof(lc_mutex_t));

  if (mtx) {
    if (pthread_mutex_init(&mtx->lock, NULL) != 0) {
      mtx = lc_free(mtx);
    }
  }
  return mtx;
}

int lc_mutex_lock(lc_mutex_t *mtx) {
  if (!mtx) return ERR_INVAL;
  if (pthread_mutex_lock(&mtx->lock) != 0) {
    return FAIL;
  }
  return SUCCESS;
}

int lc_mutex_unlock(lc_mutex_t *mtx) {
  if (!mtx) return ERR_INVAL;
  if (pthread_mutex_unlock(&mtx->lock) != 0) {
    return FAIL;
  }
  return SUCCESS;
}

int lc_mutex_trylock(lc_mutex_t *mtx) {
  if (!mtx) return ERR_INVAL;
  if (pthread_mutex_trylock(&mtx->lock) != 0) {
    return FAIL;
  }
  return SUCCESS;
}

int lc_mutex_destroy(lc_mutex_t *mtx) {
  if (!mtx) return ERR_INVAL;
  pthread_mutex_destroy(&mtx->lock);
  lc_free(mtx);
  return SUCCESS;
}

struct _lc_cond {
  pthread_cond_t cond;
};

lc_cond_t *lc_cond_new( ) {
  lc_cond_t *cond = lc_alloc(sizeof(lc_cond_t));
  if (cond) {
    if (pthread_cond_init(&cond->cond, NULL) != 0) {
      cond = lc_free(cond);
    }
  }
  return cond;
}

int lc_cond_signal(lc_cond_t *cond) {
  if (!cond) return ERR_INVAL;
  if (pthread_cond_signal(&cond->cond) != 0) {
    return FAIL;
  }
  return SUCCESS;
}

int lc_cond_broadcast(lc_cond_t *cond) {
  if (!cond) return ERR_INVAL;
  if (pthread_cond_broadcast(&cond->cond) != 0) {
    return FAIL;
  }
  return SUCCESS;
}

int lc_cond_wait(lc_cond_t *cond, lc_mutex_t *mtx) {
  if (!cond || !mtx) return ERR_INVAL;
  if (pthread_cond_wait(&cond->cond, &mtx->lock) != 0) {
    return FAIL;
  }
  return SUCCESS;
}

int lc_cond_timedwait(lc_cond_t *cond, lc_mutex_t *mtx, long millis) {
  if (!cond || millis < 0) return ERR_INVAL;

  struct timespec ts;
  struct timeval tv;
  gettimeofday(&tv, NULL);

  tv.tv_usec += (millis * 1000);
  ts.tv_sec = tv.tv_sec;
  ts.tv_sec += (tv.tv_usec / 1000000);
  ts.tv_nsec = (tv.tv_usec % 1000000) * 1000;

  int rc = pthread_cond_timedwait(&cond->cond, &mtx->lock, &ts);

  if (rc == ETIMEDOUT) {
    return ERR_TIMEDOUT;
  } else if (rc != 0) {
    return FAIL;
  }
  return SUCCESS;
}

int lc_cond_destroy(lc_cond_t *cond) {
  if (!cond) return ERR_INVAL;
  pthread_cond_destroy(&cond->cond);
  lc_free(cond);
  return SUCCESS;
}

struct _lc_sem {
  sem_t sem;
};

static inline int lc_sem_init(lc_sem_t *s, unsigned int v) {
  sem_init(&s->sem, 0, v);
  return SUCCESS;
}

lc_sem_t * lc_sem_new(unsigned int v) {
  lc_sem_t *s = lc_alloc(sizeof(lc_sem_t));
  if (!s) return NULL;
  lc_sem_init(s, v);
  return s;
}

int lc_sem_wait(lc_sem_t *s) {
  if (!s) return ERR_INVAL;
  while (sem_wait(&s->sem) != 0) {
  }
  return SUCCESS;
}

int lc_sem_timedwait(lc_sem_t *s, unsigned long millis);

int lc_sem_trywait(lc_sem_t *s) {
  if (!s) return ERR_INVAL;
  if (sem_trywait(&s->sem) == EAGAIN) {
    return ERR_AGAIN;
  }
  return SUCCESS;
}

int lc_sem_post(lc_sem_t *s) {
  if (!s) return ERR_INVAL;
  return sem_post(&s->sem);
}

int lc_sem_destroy(lc_sem_t *s) {
  if (!s) return ERR_INVAL;
  sem_destroy(&s->sem);
  lc_free(&s->sem);
  return SUCCESS;
}

int lc_sem_timedwait(lc_sem_t *s, unsigned long millis) {
  if (!s) return ERR_INVAL;

  struct timespec ts;
  struct timeval tv;
  gettimeofday(&tv, NULL);

  tv.tv_usec += (millis * 1000);
  ts.tv_sec = tv.tv_sec;
  ts.tv_sec += (tv.tv_usec / 1000000);
  ts.tv_nsec = (tv.tv_usec % 1000000) * 1000;

  if (sem_timedwait(&s->sem, &ts) != 0) {
    if (errno == ETIMEDOUT) {
      return ERR_TIMEDOUT;
    }
    return FAIL;
  }
  return SUCCESS;
}

struct _local {
  pthread_key_t key;
};

lc_local_t *lc_local_new(void(*destroy_fn)(void *)) {
  lc_local_t *local = lc_alloc(sizeof(lc_local_t));
  if (local) {
    if (pthread_key_create(&local->key, destroy_fn) != 0) {
      local = lc_free(local);
    }
  }
  return local;
}

int lc_local_set(lc_local_t *local, const void *val) {
  if (!local) return ERR_INVAL;
  if (pthread_setspecific(local->key, val) != 0) {
    return FAIL;
  }
  return SUCCESS;
}

void *lc_local_get(lc_local_t *local) {
  if (!local) return ERR_INVAL;
  void *p = pthread_getspecific(local->key);
  return p;
}

int lc_local_destroy(lc_local_t *local) {
  if (!local) return ERR_INVAL;
  if (pthread_key_delete(local->key) != 0) {
    return FAIL;
  }
  lc_free(local);
  return SUCCESS;
}

struct _lc_threadpool {
  lc_spin_t *lock;
  lc_sem_t *sem;
  int min_threads;
  int max_threads;
  int threads;
  queue_t jobs;
};

typedef struct _job {
  void (*fn)(void *data);
  void *data;
} job_t;

lc_threadpool_t *lc_threadpool_new(int min, int max) {
  lc_threadpool_t *pool = lc_alloc(sizeof(lc_threadpool_t));
  if (pool) {
    pool->lock = lc_spin_new();
    pool->sem = lc_sem_new(0);
    pool->min_threads = min;
    pool->max_threads = max;
    pool->threads = 0;
    queue_init(&pool->jobs, NULL, NULL);
  }
  return pool;
}

static job_t *next_job(lc_threadpool_t *tp) {
  job_t *job = NULL;
  while (lc_sem_timedwait(tp->sem, 2500) != SUCCESS) {
    lc_spin_lock(tp->lock);
    if (tp->threads > tp->min_threads) {
      lc_spin_unlock(tp->lock);
      return job;
    }
    lc_spin_unlock(tp->lock);
  }

  do {
    lc_spin_lock(tp->lock);
    job = queue_pop(&tp->jobs);
    lc_spin_unlock(tp->lock);
  } while (0);

  return job;
}

static void *pool_thread(void *data) {
  lc_threadpool_t *tp = (lc_threadpool_t *) data;

  job_t *job;
  while ((job = next_job(tp))) {
    job->fn(job->data);
    lc_free(job);
  }

  atomic_int_dec(&tp->threads);
  return NULL;
}

int lc_threadpool_run(lc_threadpool_t *tp, threadpool_fn fn,void *data) {
  int jobs;
  job_t *job = lc_alloc(sizeof(job_t));
  job->fn = fn;
  job->data = data;
  lc_spin_lock(tp->lock);
  queue_push(&tp->jobs,job);
  lc_sem_post(tp->sem);
  jobs = queue_size(&tp->jobs);

  if ((tp->threads < tp->min_threads) || (jobs > 1 && tp->threads < tp->max_threads)) {
    pthread_t tid;
    int rc = pthread_create(&tid,NULL,pool_thread,tp);
    printf("Started thread (%d:%d:%d) rc = %d\n",tp->threads,tp->min_threads,tp->max_threads,rc);
    if (rc == 0) {
      atomic_int_inc(&tp->threads);
    }
  }

  lc_spin_unlock(tp->lock);

  return SUCCESS;
}

int lc_threadpool_quit(lc_threadpool_t *pool);

int lc_threadpool_set_threads(lc_threadpool_t *tp, int min, int max) {
  if (!tp) return ERR_INVAL;
  lc_spin_lock(tp->lock);
  tp->min_threads = min;
  tp->max_threads = max;
  lc_spin_unlock(tp->lock);

  return SUCCESS;
}

int lc_threadpool_min(lc_threadpool_t *tp) {
  if (!tp) return ERR_INVAL;
  return tp->min_threads;
}

int lc_threadpool_max(lc_threadpool_t *tp) {
  if (!tp) return ERR_INVAL;
  return tp->max_threads;
}
