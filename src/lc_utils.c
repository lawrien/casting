#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "lua.h"
#include "lauxlib.h"

#include "lc_utils.h"
#include "lc_error.h"

void *lc_realloc(void *ptr, size_t old, size_t new) {
  void *p = NULL;

  if (ptr) {
    if (new == 0) {
      free(ptr);
    } else if (old > 0 && new > 0) {
      if (old > new) {
        p = realloc(ptr, new);
      } else {
        p = malloc(new);
        if (p) {
          memcpy(p, ptr, old);
          free(ptr);
        } else {
          lc_err = ERR_NOMEM;
        }
      }
    }
  } else if (new > 0) {
    p = malloc(new);
    if (!p) lc_err = ERR_NOMEM;
  }
  return p;
}

void lc_register_closures(lua_State *L, int idx, int count, const luaL_Reg *funcs) {
  idx = lc_absindex(L,idx);

  for (int i = 0;; i++) {
    if (funcs[i].name == NULL) break;
    for (int j = 0; j < count; j++) {
      lua_pushvalue(L, -(count));
    }
    lua_pushcclosure(L, funcs[i].func, count); // [cfn]
    lua_setfield(L, idx, funcs[i].name);
  }
}

void print_element(lua_State *L, int i);

static void print_nil(lua_State *L, int i) {
  printf(" -");
}

static void print_boolean(lua_State *L, int i) {
  int b = lua_toboolean(L, i);
  if (b == 0) {
    printf(" F");
  } else {
    printf(" T");
  }
}

static void print_number(lua_State *L, int i) {
  printf(" %f", lua_tonumber(L, i));
}

static void print_string(lua_State *L, int i) {
  printf(" \"%s\"", lua_tostring(L,i));
}

static void print_table(lua_State *L, int i) {
  printf(" [tbl %p]", lua_topointer(L, i));
}

static void print_function(lua_State *L, int i) {
  printf(" fn()");
}

static void print_cfunction(lua_State *L, int i) {
  printf(" cfn(");
  int x = 0;
  while (lua_getupvalue(L, i, ++x)) {
    print_element(L, -1);
    lua_pop(L,1);
  }
  printf(")");
}

static void print_userdata(lua_State *L, int i) {
  printf(" [ud: %p]", lua_touserdata(L, i));
}

static void print_lightuserdata(lua_State *L, int i) {
  printf(" [lud: %p]", lua_touserdata(L, i));
}

static void print_thread(lua_State *L, int i) {
  printf(" [thread: %p]", lua_topointer(L, i));
}

void print_element(lua_State *L, int i) {
  if (lua_isnil(L,i)) {
    print_nil(L, i);
  } else if (lua_isboolean(L,i)) {
    print_boolean(L, i);
  } else if (lua_isnumber(L, i)) {
    print_number(L, i);
  } else if (lua_isstring(L, i)) {
    print_string(L, i);
  } else if (lua_istable(L,i)) {
    print_table(L, i);
  } else if (lua_iscfunction(L, i)) {
    print_cfunction(L, i);
  } else if (lua_isfunction(L,i)) {
    print_function(L, i);
  } else if (lua_islightuserdata(L,i)) {
    print_lightuserdata(L, i);
  } else if (lua_isuserdata(L, i)) {
    print_userdata(L, i);
  } else if (lua_isthread(L,i)) {
    print_thread(L, i);
  } else {
    printf(" unknown");
  }
}

void print_stack(lua_State *L, const char *fun, const char *msg, ...) {
  int size = lua_gettop(L);
  printf("%s -- ", fun);
  va_list args;
  va_start(args, msg);
  vfprintf(stdout, msg, args);
  va_end(args);

  int i;
  for (i = 1; i <= size; i++) {
    print_element(L, i);
  }
  printf("\n");
}

void print_info(const char *file, const char *fun, int line, const char *msg, ...) {
  printf("%s (%d) -- ", fun, line);
  va_list args;
  va_start(args, msg);
  vfprintf(stdout, msg, args);
  va_end(args);

  printf("\n");
}

