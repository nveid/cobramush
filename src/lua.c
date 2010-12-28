/* lua API for CobraMUSH */
#include "config.h"
#include <string.h>
#include "conf.h"
#include "dbio.h"
#include "externs.h"
#include "parse.h"
#include "htab.h"
#include "command.h"
#include "confmagic.h"


#include "mushlua.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.c"

extern void luaopen_cobra(lua_State *);
static lua_State *mush_lua_env = NULL;
static lua_CFunction mlua_notify(lua_State *);
void mlua_test(dbref);


static const luaL_reg mlua_lib[] = {
  {"notify", mlua_notify},
  {NULL, NULL}
};

void mush_lua_start() {

  mush_lua_env = lua_open();

   /* Load MUSH Library */
  luaL_register(mush_lua_env, "mush", mlua_lib);
  /* Load Cobra SWIG Wrappers */
  luaopen_cobra(mush_lua_env);

  luaL_openlibs(mush_lua_env);
}

void mush_lua_stop() {

  if(!mush_lua_env)
    return;

  lua_close(mush_lua_env);
}

/* Until we have real stuff.. this tests my lua scripts.. */
void mlua_test(dbref enactor) {
  int s;

  s = luaL_loadfile(mush_lua_env, "lua/test.lua");

  if(s==0)
    s = lua_pcall(mush_lua_env, 0, LUA_MULTRET, 0);

  if(s != 0)
    notify_format(enactor, "Lua Error: %s\n", lua_tostring(mush_lua_env, -1));
}

/* lua to MUSH API functions */
static lua_CFunction mlua_notify(lua_State *L) {
  int nargs;
  dbref obj;

  nargs = lua_gettop(L);

  if(nargs < 2) {
    lua_pushstring(L, "incorrect number of arguments");
    lua_error(L);

  } else if(!lua_isnumber(L, 1)) {
    lua_pushstring(L, "first argument must be an integer");
    lua_error(L);
  } else { 
    lua_concat(L, nargs-1);
    obj = lua_tonumber(L, 1); 
    notify(obj, lua_tostring(L, -1) );
  }
}

