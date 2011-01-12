%module mush %{
#include "conf.h"
#include "externs.h"
#include "dbdefs.h"
#include "game.h"
#include "command.h"
#include "function.h"

extern struct db_stat_info current_state;
extern void moveit(dbref what, dbref where, int nomovemsgs);

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
%native(notify) lua_CFunction mlua_notify(lua_State *L);
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
