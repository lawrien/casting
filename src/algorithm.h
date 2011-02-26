#ifndef __ALGORITHM_H__
#define __ALGORITHM_H__

typedef int (*compare_cb)(const void *p1, const void *p2);
typedef int (*copy_cb)(const void *p,void **d);
typedef void (*release_cb)(void *p);
typedef void (*order_cb)(void *p);

#endif //__ALGORITHM_H__
