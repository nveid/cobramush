%module cobra %{
#include "conf.h"
#include "externs.h"
#include "dbdefs.h"

extern struct db_stat_info current_state;
%}
void do_reboot(int player, int flag);
%mutable;
extern int shutdown_flag;
extern struct db_stat_info current_state;

