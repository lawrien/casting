#ifndef __LC_THREAD_H__
#define __LC_THREAD_H__

#include "casting.h"
#include "lc_error.h"

void init_thread( );

typedef struct _lc_mutex lc_mutex_t;
typedef struct _lc_spin lc_spin_t;
typedef struct _lc_sem lc_sem_t;
typedef struct _lc_cond lc_cond_t;
typedef struct _local lc_local_t;
typedef struct _lc_threadpool lc_threadpool_t;

void *atomic_ptr_get(volatile void *addr);
void atomic_ptr_set(volatile void *addr,void *p);
int atomic_ptr_cas(volatile void *addr, void *o, void *n);
int atomic_int_get(volatile int *addr);
void atomic_int_set(volatile int *addr,int i);
int atomic_int_cas(volatile int *addr, int o, int n);
int atomic_int_tas(volatile int *addr,int n);
int atomic_int_add(volatile int *addr, int i);
int atomic_int_sub(volatile int *addr, int i);
int atomic_int_inc(volatile int *addr);
int atomic_int_dec(volatile int *addr);

lc_mutex_t *lc_mutex_new( );
int lc_mutex_lock(lc_mutex_t *mtx);
int lc_mutex_unlock(lc_mutex_t *mtx);
int lc_mutex_trylock(lc_mutex_t *mtx);
int lc_mutex_destroy(lc_mutex_t *mtx);

lc_spin_t *lc_spin_new( );
int lc_spin_lock(lc_spin_t *s);
int lc_spin_trylock(lc_spin_t *s);
int lc_spin_unlock(lc_spin_t *s);
int lc_spin_destroy(lc_spin_t *s);

lc_sem_t * lc_sem_new(unsigned int v);
int lc_sem_wait(lc_sem_t *s);
int lc_sem_timedwait(lc_sem_t *s, unsigned long millis);
int lc_sem_trywait(lc_sem_t *s);
int lc_sem_post(lc_sem_t *s);
int lc_sem_destroy(lc_sem_t *s);

lc_cond_t *lc_cond_new( );
int lc_cond_signal(lc_cond_t *cond);
int lc_cond_broadcast(lc_cond_t *cond);
int lc_cond_wait(lc_cond_t *cond, lc_mutex_t *mtx);
int lc_cond_timedwait(lc_cond_t *cond, lc_mutex_t *mtx, long millis);
int lc_cond_destroy(lc_cond_t *cond);

lc_local_t *lc_local_new(void (*destroy_fn)(void *));
int lc_local_set(lc_local_t *local, const void *val);
void *lc_local_get(lc_local_t *local);
int lc_local_destroy(lc_local_t *local);

#define THREAD_WAIT_MILLIS  2500

typedef void (*threadpool_fn)(void *values);

lc_threadpool_t *lc_threadpool_new(int min, int max);
int lc_threadpool_quit(lc_threadpool_t *pool);
int lc_threadpool_run(lc_threadpool_t *pool, threadpool_fn fn, void *data);
int lc_threadpool_set_threads(lc_threadpool_t *pool,int min, int max);
int lc_threadpool_min(lc_threadpool_t *pool);
int lc_threadpool_max(lc_threadpool_t *pool);

#endif // __LC_THREAD_H__
