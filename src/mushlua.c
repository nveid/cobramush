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

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "mushlua.h"

extern void luaopen_cobra(lua_State *);
static lua_State *mush_lua_env = NULL;
void mlua_test(dbref, char *);


void mush_lua_start() {

  mush_lua_env = lua_open();
  /* Load Global Values */


  /* Load Cobra SWIG Wrappers */
  luaopen_mush(mush_lua_env);

  luaL_openlibs(mush_lua_env);
}

void mush_lua_stop() {

  if(!mush_lua_env)
    return;

  lua_close(mush_lua_env);
}

/* Until we have real stuff.. this tests my lua scripts.. */
void mlua_run(dbref enactor, char *filename) {
  int s;

  s = luaL_loadfile(mush_lua_env, tprintf("lua/%s.lua", filename));

  if(s==0)
    s = lua_pcall(mush_lua_env, 0, LUA_MULTRET, 0);

  if(s != 0)
    notify_format(enactor, "Lua Error: %s\n", lua_tostring(mush_lua_env, -1));
}

/* @lua <file> */
COMMAND(cmd_mushlua) {
  if(SW_ISSET(sw, SWITCH_RESTART)) {
    mush_lua_stop();
    mush_lua_start();
    notify(player, "Lua Engine Restarted.");
  } else 
    mlua_run(player, arg_left);
}
