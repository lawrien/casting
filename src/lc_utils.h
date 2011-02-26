#ifndef __LC_UTILS_H__
#define __LC_UTILS_H__

#include <stdio.h>
#include <lua.h>

void *lc_realloc(void *ptr, size_t old, size_t new);
#define lc_alloc(s) lc_realloc(NULL,0,s)
#define lc_free(p) lc_realloc((p),0,0)

#define lc_absindex(L,idx) \
  ((idx) > 0 ? (idx) : (idx) <= LUA_REGISTRYINDEX ? (idx) : lua_gettop((L)) + 1 + (idx))

void lc_register_closures(lua_State *L, int idx, int count, const luaL_Reg *l);

void print_stack(lua_State *L, const char *fun,const char *msg, ...);
void print_info(const char *file,const char *fun,int line, const char *msg, ...);
void print_element(lua_State *L,int index);

#ifdef TRACE
  #define STACK(L,msg, ...) do { \
                              print_stack(L,__func__,msg,##__VA_ARGS__); \
                            } while (0)
  #define INFO(msg, ...) do { \
                              print_info(__FILE__,__func__,__LINE__,msg,##__VA_ARGS__); \
                              } while (0)
#else
  #define STACK(L,msg, ...)
  #define INFO(msg, ...)
#endif // TRACE

#endif // __LC_UTILS_H__
