#include <lua.h>
#include <lauxlib.h>

#include <stdlib.h>
#include <string.h>

#include "casting.h"
#include "message.h"
#include "lc_session.h"
#include "lc_channel.h"
#include "lc_error.h"
#include "lc_thread.h"

static const luaL_Reg funcs[] = { { NULL, NULL } };

static const luaL_Reg packages[] = { { "Session", lc_open_session },
                                      { "Message", lc_open_message },
                                      { "Channel", lc_open_channel },
                                      { NULL, NULL } };

LUALIB_API int luaopen_casting(lua_State *L) {
  lua_newtable(L); // [tbl]
  luaL_register(L, NULL, funcs); //[tbl]

  int i;
  for (i = 0; packages[i].name; i++) {
    packages[i].func(L); // [tbl][tbl]
    lua_setfield(L, -2, packages[i].name); // [tbl]
  }

  return 1;
}
