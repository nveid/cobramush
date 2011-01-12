%module mush %{
#include "conf.h"
#include "externs.h"
#include "dbdefs.h"
#include "game.h"
#include "command.h"
#include "function.h"

extern struct db_stat_info current_state;
extern void moveit(dbref what, dbref where, int nomovemsgs);

static int mlua_objref(lua_State *L) {
  int nargs;
  dbref obj;
  long cretime;

  nargs = lua_gettop(L);

  if (nargs < 1 || nargs > 2) {
    lua_pushstring(L, "incorrect number of arguments");
    lua_error(L);
  }

  if (!lua_isnumber(L, 1)) {
    lua_pushstring(L, "object's dbref must be an integer");
    lua_error(L);
  }

  obj = lua_tointeger(L, 1);

  if (obj < 0 || obj >= db_top || IsGarbage(obj)) {
    lua_pushnil(L);
    return 1;
  }

  if (nargs == 2) {
    if (!lua_isnumber(L, 2)) {
      lua_pushstring(L, "object's creation time must be an integer");
      lua_error(L);
    }
    cretime = lua_tointeger(L, 2);
    if (CreTime(obj) != cretime) {
      lua_pushnil(L);
      return 1;
    }
  }

  lua_pushinteger(L, obj); /* TODO: a type-safe pointer to the object */
  return 1;
}

static int mlua_notify(lua_State *L) {
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

  return 0;
}

%}
%native(objref) int mlua_objref(lua_State *L);
%native(notify) int mlua_notify(lua_State *L);
int alias_command(const char *command, const char *alias);
int alias_function(const char *function, const char *alias);
void twiddle_flag_internal(const char *ns, int thing, const char *flag, int negate);
void do_reboot(int player, int flag);
void moveit(int what, int towhere, int nomovemsgs);
%mutable;
extern int shutdown_flag;
extern struct db_stat_info current_state;
%immutable;
#define NOTHING -1
