#ifndef __CASTING_H__
#define __CASTING_H__

#include <inttypes.h>
#include <lua.h>
#include <lauxlib.h>

#include "lc_error.h"
#include "lc_utils.h"

#define VERSION_MAJOR "0"
#define VERSION_MINOR "1"
#define VERSION_TINY  "0"

#define NAME          "Casting"

#define VERSION VERSION_MAJOR "." VERSION_MINOR "." VERSION_TINY
#define VERSION_STRING NAME " " VERSION

#ifdef __cplusplus
extern "C" {
#endif

LUALIB_API int luaopen_casting(lua_State *L);
int lc_open_message(lua_State *L);
int lc_open_channel(lua_State *L);
int lc_open_session(lua_State *L);

#ifdef __cplusplus
}
#endif

#endif // __CASTING_H__
