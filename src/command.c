/**
 * \file command.c

 *
 * \brief Parsing of commands.
 *
 * Sets up a hash table for the commands, and parses input for commands.
 * This implementation is almost totally by Thorvald Natvig, with
 * various mods by Javelin and others over time.
 *
 */

#include "copyrite.h"
#include "config.h"

#include <string.h>

#include "conf.h"
#include "externs.h"
#include "dbdefs.h"
#include "mushdb.h"
#include "game.h"
#include "match.h"
#include "attrib.h"
#include "extmail.h"
#include "getpgsiz.h"
#include "parse.h"
#include "access.h"
#include "version.h"
#include "ptab.h"
#include "htab.h"
#include "function.h"
#include "command.h"
#include "mymalloc.h"
#include "flags.h"
#include "log.h"
#include "cmds.h"
#include "confmagic.h"

PTAB ptab_command;      /**< Prefix table for command names. */
PTAB ptab_command_perms;        /**< Prefix table for command permissions */

HASHTAB htab_reserved_aliases;  /**< Hash table for reserved command aliases */

static const char *command_isattr(char *command);
static int command_check(dbref player, COMMAND_INFO *cmd, switch_mask sw);
static int switch_find(COMMAND_INFO *cmd, char *sw);
static void strccat(char *buff, char **bp, const char *from);
static int has_hook(struct hook_data *hook);
extern int global_fun_invocations;      /**< Counter for function invocations */
extern int global_fun_recursions;       /**< Counter for function recursion */

int run_hook(dbref player, dbref cause, struct hook_data *hook,
             char *saveregs[], int save);
int command_lock(const char *name, const char *lock);

/** The list of standard commands. Additional commands can be added
 * at runtime with add_command().
 */
COMLIST commands[] = {

  {"@COMMAND",
   "ADD ALIAS DELETE EQSPLIT LOCK LSARGS RSARGS NOEVAL ON OFF QUIET ENABLE DISABLE RESTRICT NOPARSE",
   cmd_command,
   CMD_T_PLAYER | CMD_T_EQSPLIT, NULL },
  {"@@", NULL, cmd_null, CMD_T_ANY | CMD_T_NOPARSE, NULL},
  {"@ALLHALT", NULL, cmd_allhalt, CMD_T_ANY, "POWER^HALT"},
  {"@ALLQUOTA", "QUIET", cmd_allquota, CMD_T_ANY, "POWER^CQUOTA"},
  {"@ASSERT", NULL, cmd_assert, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_NOPARSE, NULL},
  {"@ATRLOCK", "READ WRITE", cmd_atrlock, CMD_T_ANY | CMD_T_EQSPLIT, NULL},
  {"@ATRCHOWN", NULL, cmd_atrchown, CMD_T_EQSPLIT, NULL},
  {"@ATTRIBUTE", "ACCESS DEFAULTS LOCK WRITE READ DELETE RENAME RETROACTIVE", cmd_attribute,
   CMD_T_ANY | CMD_T_EQSPLIT, "POWER^GFUNCS"},
  {"@BOOT", "PORT ME", cmd_boot, CMD_T_ANY, NULL},
  {"@BREAK", NULL, cmd_break, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_NOPARSE, NULL},
#ifdef CHAT_SYSTEM
  {"@CEMIT", "NOEVAL NOISY SPOOF", cmd_cemit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, NULL},
  {"@CHANNEL",
   "LIST ADD DELETE RENAME NAME PRIVS QUIET NOISY DECOMPILE DESCRIBE CHOWN WIPE MUTE UNMUTE GAG UNGAG HIDE UNHIDE WHAT TITLE BRIEF RECALL BUFFER SET OBJECT",
   cmd_channel,
   CMD_T_ANY | CMD_T_SWITCHES | CMD_T_EQSPLIT | CMD_T_NOGAGGED | CMD_T_RS_ARGS,
   NULL},
  {"@CHAT", NULL, cmd_chat, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, NULL},
#endif /* CHAT_SYSTEM */
  {"@CHOWNALL", "PRESERVE", cmd_chownall, CMD_T_ANY | CMD_T_EQSPLIT, "POWER^MODIFY"},

  {"@CHOWN", "PRESERVE", cmd_chown,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, NULL},
  {"@CHZONEALL", NULL, cmd_chzoneall, CMD_T_ANY | CMD_T_EQSPLIT, NULL},

  {"@CHZONE", NULL, cmd_chzone,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, NULL},
#ifdef CHAT_SYSTEM
  {"@COBJ", "RESET", cmd_cobj, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED , NULL},
#endif /* CHAT_SYSTEM */
  {"@CONFIG",
   "SET LOWERCASE LIST GLOBALS DEFAULTS COSTS FLAGS POWERS FUNCTIONS COMMANDS ATTRIBS",
   cmd_config, CMD_T_ANY | CMD_T_EQSPLIT, NULL},
  {"@CPATTR", "CONVERT NOFLAGCOPY", cmd_cpattr,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS,
   NULL},
  {"@CREATE", NULL, cmd_create, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED,
   NULL},
#ifdef MUSHCRON
  {"@CRON", "ADD DELETE LIST COMMANDS FUNCTIONS", cmd_cron, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_LS_NOPARSE, "POWER^CRON"},
#endif /* MUSHCRON */
  {"@CLONE", "PRESERVE", cmd_clone, CMD_T_ANY | CMD_T_NOGAGGED | CMD_T_EQSPLIT,
   NULL},
#ifdef CHAT_SYSTEM
  {"@CLOCK", "JOIN SPEAK MOD SEE HIDE", cmd_clock,
   CMD_T_ANY | CMD_T_EQSPLIT, NULL},
#endif
  {"@DBCK", NULL, cmd_dbck, CMD_T_ANY, "POWER^SITE"},

  {"@DECOMPILE", "DB PREFIX TF FLAGS ATTRIBS SKIPDEFAULTS", cmd_decompile,
   CMD_T_ANY, NULL},

  {"@DESTROY", "OVERRIDE", cmd_destroy, CMD_T_ANY, NULL},

  {"@DIG", "TELEPORT", cmd_dig,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, "POWER^BUILDER"},
  {"@DISABLE", NULL, cmd_disable, CMD_T_ANY, "POWER^SITE"},
  {"@DIVISION", "CREATE", cmd_division, CMD_T_ANY | CMD_T_EQSPLIT, "(POWER^DIVISION|POWER^ATTACH)"},

  {"@DOING", "HEADER", cmd_doing,
   CMD_T_ANY | CMD_T_NOPARSE | CMD_T_NOGAGGED, NULL},
  {"@DOLIST", "NOTIFY DELIMIT", cmd_dolist,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_NOPARSE, NULL},
  {"@DRAIN", "ALL ANY", cmd_notify_drain, CMD_T_ANY | CMD_T_EQSPLIT, NULL},
  {"@DUMP", "PARANOID DEBUG", cmd_dump, CMD_T_ANY, "POWER^SITE"},

  {"@EDIT", NULL, cmd_edit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_RS_NOPARSE |
   CMD_T_NOGAGGED, NULL},
  {"@ELOCK", NULL, cmd_elock, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED,
   NULL},
  {"@EMIT", "ROOM NOEVAL SILENT SPOOF", cmd_emit, CMD_T_ANY | CMD_T_NOGAGGED, NULL},
  {"@EMPOWER", NULL, cmd_empower, CMD_T_ANY | CMD_T_EQSPLIT, "POWER^EMPOWER"},
  {"@ENABLE", NULL, cmd_enable, CMD_T_ANY | CMD_T_NOGAGGED, "POWER^SITE"},

  {"@ENTRANCES", "EXITS THINGS PLAYERS ROOMS", cmd_entrances,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, NULL},
  {"@EUNLOCK", NULL, cmd_eunlock, CMD_T_ANY | CMD_T_NOGAGGED, NULL},

  {"@FIND", NULL, cmd_find,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, NULL},
  {"@FIRSTEXIT", NULL, cmd_firstexit, CMD_T_ANY, NULL},
  {"@FLAG", "ADD TYPE LETTER LIST RESTRICT DELETE ALIAS DISABLE ENABLE",
   cmd_flag,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, NULL},

  {"@FORCE", "NOEVAL", cmd_force, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED,
   NULL},
  {"@FUNCTION", "DELETE ENABLE DISABLE PRESERVE RESTORE RESTRICT", cmd_function,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, NULL},
  {"@GREP", "LIST PRINT ILIST IPRINT", cmd_grep,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_NOPARSE | CMD_T_NOGAGGED, NULL},
  {"@HALT", "ALL", cmd_halt, CMD_T_ANY | CMD_T_EQSPLIT, NULL},
  {"@HIDE", "NO OFF YES ON", cmd_hide, CMD_T_ANY, NULL},
  {"@HOOK", "LIST AFTER BEFORE IGNORE OVERRIDE", cmd_hook,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS,
   "POWER^SITE"},
  {"@KICK", NULL, cmd_kick, CMD_T_ANY, "POWER^SITE"},

  {"@LEMIT", "NOEVAL SILENT SPOOF", cmd_lemit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, NULL},
  {"@LEVEL", NULL, cmd_level, CMD_T_ANY | CMD_T_EQSPLIT, NULL},
  {"@LINK", "PRESERVE", cmd_link, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, NULL},
  {"@LISTMOTD", NULL, cmd_listmotd, CMD_T_ANY, NULL},

  {"@LIST", "LOWERCASE MOTD LOCKS FLAGS FUNCTIONS POWERS COMMANDS ATTRIBS",
   cmd_list,
   CMD_T_ANY, NULL},
  {"@LOCK", NULL, cmd_lock,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_SWITCHES | CMD_T_NOGAGGED, NULL},
  {"@LOG", "CHECK CMD CONN ERR TRACE WIZ", cmd_log,
   CMD_T_ANY | CMD_T_NOGAGGED, "POWER^SITE"},
  {"@LOGWIPE", "CHECK CMD CONN ERR TRACE WIZ", cmd_logwipe,
   CMD_T_ANY | CMD_T_NOGAGGED | CMD_T_GOD, "POWER^SITE"},
  {"@LSET", NULL, cmd_lset,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, NULL},
  {"@LUA", "RESTART", cmd_mushlua, CMD_T_ANY, NULL},
#ifdef USE_MAILER
  {"@MAIL",
   "NOEVAL NOSIG STATS DSTATS FSTATS DEBUG NUKE FOLDERS UNFOLDER LIST READ CLEAR UNCLEAR PURGE FILE TAG UNTAG FWD FORWARD SEND SILENT URGENT",
   cmd_mail, CMD_T_ANY | CMD_T_EQSPLIT, NULL},
  {"@MALIAS",
   "SET CREATE DESTROY DESCRIBE RENAME STATS CHOWN NUKE ADD REMOVE LIST ALL WHO MEMBERS USEFLAG SEEFLAG",
   cmd_malias, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, NULL},
#endif
  {"@MAP", "DELIMIT", cmd_map, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_NOPARSE,
   NULL},
  {"@MODULE", "LOAD UNLOAD", cmd_module, CMD_T_ANY, "POWER^SITE"},
  {"@MOTD", "CONNECT LIST DOWN FULL", cmd_motd,
   CMD_T_ANY | CMD_T_NOGAGGED, NULL},
  {"@MVATTR", "CONVERT NOFLAGCOPY", cmd_mvattr,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS,
   NULL},
  {"@NAME", NULL, cmd_name, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED
   | CMD_T_NOGUEST, NULL},
  {"@NEWPASSWORD", NULL, cmd_newpassword, CMD_T_ANY | CMD_T_EQSPLIT
   | CMD_T_RS_NOPARSE, "(POWER^NEWPASS|POWER^BCREATE)"},
  {"@NOTIFY", "ALL ANY", cmd_notify_drain, CMD_T_ANY | CMD_T_EQSPLIT, NULL},
#ifdef CHAT_SYSTEM
  {"@NSCEMIT", "NOEVAL NOISY SPOOF", cmd_cemit,
    CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, "POWER^CAN_NSPEMIT"},
#endif /* CHAT_SYSTEM */
  {"@NSPEMIT", "LIST SILENY NOISY NOEVAL", cmd_pemit,
   CMD_T_ANY | CMD_T_EQSPLIT, "POWER^CAN_NSPEMIT"},
  {"@NSEMIT", "ROOM NOEVAL SILENT SPOOF", cmd_emit, CMD_T_ANY | CMD_T_NOGAGGED,
    "POWER^CAN_NSPEMIT"},
  {"@NSLEMIT", "NOEVAL SILENT SPOOF", cmd_lemit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, "POWER^CAN_NSPEMIT"},
  {"@NSOEMIT", "NOEVAL SPOOF", cmd_oemit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, "POWER^CAN_NSPEMIT"},
  {"@NSPEMIT", "LIST SILENT NOISY NOEVAL", cmd_pemit,
   CMD_T_ANY | CMD_T_EQSPLIT, "POWER^CAN_NSPEMIT"},
  {"@NSREMIT", "LIST NOEVAL NOISY SILENT SPOOF", cmd_remit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, "POWER^CAN_NSPEMIT"},
  {"@NSZEMIT", NULL, cmd_zemit, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED,
   "POWER^CAN_NSPEMIT"},
  {"@NUKE", NULL, cmd_nuke, CMD_T_ANY | CMD_T_NOGAGGED, NULL},
  {"@OEMIT", "NOEVAL SPOOF", cmd_oemit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED,
   NULL},
  {"@OPEN", NULL, cmd_open,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, NULL},
  {"@PARENT", NULL, cmd_parent, CMD_T_ANY | CMD_T_EQSPLIT, NULL},
  {"@PASSWORD", NULL, cmd_password, CMD_T_PLAYER | CMD_T_EQSPLIT
   | CMD_T_RS_NOPARSE | CMD_T_NOGUEST, NULL},
  {"@PCREATE", NULL, cmd_pcreate, CMD_T_ANY | CMD_T_EQSPLIT, NULL},

  {"@PEMIT", "LIST CONTENTS SILENT NOISY NOEVAL SPOOF", cmd_pemit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, NULL},
  {"@POLL", "CLEAR", cmd_poll, CMD_T_ANY, NULL},
  {"@POOR", NULL, cmd_poor, CMD_T_ANY, NULL},
  {"@POWER", "ALIAS LIST ADD DELETE", cmd_power, CMD_T_ANY | CMD_T_EQSPLIT , NULL},
  {"@POWERGROUP", "AUTO MAX ADD DELETE LIST RAW", cmd_powergroup, CMD_T_ANY | CMD_T_EQSPLIT, NULL},
  {"@PROGRAM", "LOCK QUIT", cmd_prog, CMD_T_ANY | CMD_T_EQSPLIT, NULL},
  {"@PROMPT", NULL, cmd_prompt, CMD_T_ANY | CMD_T_EQSPLIT, NULL},
  {"@PS", "ALL SUMMARY COUNT QUICK", cmd_ps, CMD_T_ANY, NULL},
  {"@PURGE", NULL, cmd_purge, CMD_T_ANY,  "POWER^SITE"},
  {"@QUOTA", "ALL SET", cmd_quota, CMD_T_ANY | CMD_T_EQSPLIT, NULL},
  {"@READCACHE", NULL, cmd_readcache, CMD_T_ANY,  "(POWER^SITE|POWER^RCACHE)"},
  {"@RECYCLE", "OVERRIDE", cmd_destroy, CMD_T_ANY | CMD_T_NOGAGGED, NULL},

  {"@REMIT", "LIST NOEVAL NOISY SILENT SPOOF", cmd_remit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, NULL},
  {"@REJECTMOTD", NULL, cmd_rejectmotd, CMD_T_ANY, "POWER^SITE"},
  {"@RESTART", "ALL", cmd_restart, CMD_T_ANY | CMD_T_NOGAGGED, NULL},
#ifdef RPMODE_SYS
  {"@CRPLOG", "QUIET RESET COMBAT", cmd_rplog, CMD_T_ANY, "POWER^COMBAT"},
#endif
  {"@SCAN", "ROOM SELF ZONE GLOBALS", cmd_scan,
   CMD_T_ANY | CMD_T_NOGAGGED, NULL},
  {"@SD", "LOGOUT", cmd_su, CMD_T_ANY, NULL},
  {"@SEARCH", NULL, cmd_search,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_RS_NOPARSE, NULL},
  {"@SELECT", "NOTIFY REGEXP", cmd_select,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_RS_NOPARSE, NULL},
  {"@SET", NULL, cmd_set, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, NULL},
  {"@SHUTDOWN", "PANIC REBOOT PARANOID", cmd_shutdown, CMD_T_ANY, "POWER^Site"},
#ifdef HAS_MYSQL
  {"@SQL", NULL, cmd_sql, CMD_T_ANY, "POWER^SQL_OK"},
#else
  {"@SQL", NULL, cmd_unimplemented, CMD_T_ANY,  "POWER^SQL_OK"},
#endif

  {"@SITELOCK", "BAN CHECK REGISTER REMOVE NAME", cmd_sitelock,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS, "POWER^SITE"},
   {"@SNOOP", "LIST", cmd_snoop, CMD_T_ANY, "POWER^SITE"},
  {"@STATS", "CHUNKS FREESPACE PAGING REGIONS TABLES", cmd_stats,
   CMD_T_ANY, NULL},

  {"@SWEEP", "CONNECTED HERE INVENTORY EXITS", cmd_sweep, CMD_T_ANY, NULL},
  {"@SWITCH", "NOTIFY FIRST ALL REGEXP", cmd_switch,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_RS_NOPARSE |
   CMD_T_NOGAGGED, NULL},
  {"@SQUOTA", NULL, cmd_squota, CMD_T_ANY | CMD_T_EQSPLIT, NULL},
  {"@SU", NULL, cmd_su, CMD_T_ANY, NULL},
  {"@TELEPORT", "SILENT INSIDE", cmd_teleport,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, NULL},
  {"@TRIGGER", NULL, cmd_trigger,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, NULL},
  {"@ULOCK", NULL, cmd_ulock, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED,
   NULL},
  {"@UNDESTROY", NULL, cmd_undestroy, CMD_T_ANY | CMD_T_NOGAGGED, NULL},
  {"@UNLINK", NULL, cmd_unlink, CMD_T_ANY | CMD_T_NOGAGGED, NULL},

  {"@UNLOCK", NULL, cmd_unlock,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_SWITCHES | CMD_T_NOGAGGED, NULL},
  {"@UNRECYCLE", NULL, cmd_undestroy, CMD_T_ANY | CMD_T_NOGAGGED, NULL},
  {"@UPTIME", "MORTAL", cmd_uptime, CMD_T_ANY, NULL},
  {"@UUNLOCK", NULL, cmd_uunlock, CMD_T_ANY | CMD_T_NOGAGGED, NULL},
  {"@VERB", NULL, cmd_verb, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS, NULL},
  {"@VERSION", NULL, cmd_version, CMD_T_ANY, NULL},
  {"@WAIT", "UNTIL", cmd_wait, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_NOPARSE,
   NULL},
  {"@WALL", "NOEVAL EMIT", cmd_wall, CMD_T_ANY, "POWER^ANNOUNCE"},

  {"@WARNINGS", NULL, cmd_warnings, CMD_T_ANY | CMD_T_EQSPLIT, NULL},
  {"@WCHECK", "ALL ME", cmd_wcheck, CMD_T_ANY, NULL},
  {"@WHEREIS", NULL, cmd_whereis, CMD_T_ANY | CMD_T_NOGAGGED, NULL},
  {"@WIPE", NULL, cmd_wipe, CMD_T_ANY, NULL},
  {"@ZCLONE", NULL, cmd_zclone, CMD_T_ANY, "POWER^BUILDER"},
  {"@ZEMIT", NULL, cmd_zemit, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED,
   NULL},

  {"BUY", NULL, cmd_buy, CMD_T_ANY | CMD_T_NOGAGGED, NULL},
  {"BRIEF", NULL, cmd_brief, CMD_T_ANY, NULL},
  {"DESERT", NULL, cmd_desert, CMD_T_PLAYER | CMD_T_THING, NULL},
  {"DISMISS", NULL, cmd_dismiss, CMD_T_PLAYER | CMD_T_THING, NULL},
  {"DROP", NULL, cmd_drop, CMD_T_PLAYER | CMD_T_THING, NULL},
  {"EXAMINE", "ALL BRIEF DEBUG MORTAL", cmd_examine, CMD_T_ANY, NULL},
  {"EMPTY", NULL, cmd_empty, CMD_T_PLAYER | CMD_T_THING | CMD_T_NOGAGGED, NULL},
  {"ENTER", NULL, cmd_enter, CMD_T_ANY, NULL},

  {"FOLLOW", NULL, cmd_follow,
   CMD_T_PLAYER | CMD_T_THING | CMD_T_NOGAGGED, NULL},

  {"GET", NULL, cmd_get, CMD_T_PLAYER | CMD_T_THING | CMD_T_NOGAGGED, NULL},
  {"GIVE", "SILENT", cmd_give, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, NULL},
  {"GOTO", NULL, cmd_goto, CMD_T_PLAYER | CMD_T_THING, NULL},
  {"HOME", NULL, cmd_home, CMD_T_PLAYER | CMD_T_THING, NULL},
  {"INVENTORY", NULL, cmd_inventory, CMD_T_ANY, NULL},

  {"LOOK", "OUTSIDE", cmd_look, CMD_T_ANY, NULL},
  {"LEAVE", NULL, cmd_leave, CMD_T_PLAYER | CMD_T_THING, NULL},

  {"PAGE", "BLIND NOEVAL LIST PORT OVERRIDE", cmd_page,
   CMD_T_ANY | CMD_T_RS_NOPARSE | CMD_T_NOPARSE | CMD_T_EQSPLIT |
   CMD_T_NOGAGGED, NULL},
  {"POSE", "NOEVAL NOSPACE", cmd_pose, CMD_T_ANY | CMD_T_NOGAGGED, NULL},
  {"SCORE", NULL, cmd_score, CMD_T_ANY, NULL},
  {"SAY", "NOEVAL", cmd_say, CMD_T_ANY | CMD_T_NOGAGGED, NULL},
  {"SEMIPOSE", "NOEVAL", cmd_semipose, CMD_T_ANY | CMD_T_NOGAGGED, NULL},

  {"TAKE", NULL, cmd_take, CMD_T_PLAYER | CMD_T_THING | CMD_T_NOGAGGED,
   NULL},
  {"TEACH", NULL, cmd_teach, CMD_T_ANY | CMD_T_NOPARSE, NULL},
  {"THINK", "NOEVAL", cmd_think, CMD_T_ANY | CMD_T_NOGAGGED, NULL},

  {"UNFOLLOW", NULL, cmd_unfollow,
   CMD_T_PLAYER | CMD_T_THING | CMD_T_NOGAGGED, NULL},
  {"USE", NULL, cmd_use, CMD_T_ANY | CMD_T_NOGAGGED, NULL},

  {"WHISPER", "LIST NOISY SILENT NOEVAL", cmd_whisper,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, NULL},
  {"WITH", "NOEVAL ROOM", cmd_with, CMD_T_PLAYER | CMD_T_THING | CMD_T_EQSPLIT,
   NULL},

/* ATTRIB_SET is an undocumented command - it's sugar to make it possible
 * enable/disable attribute setting with &XX or @XX
 */

  {"ATTRIB_SET", NULL, command_atrset,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED | CMD_T_INTERNAL, NULL},

/* A way to stop people starting commands with functions */
  {"WARN_ON_MISSING", NULL, cmd_warn_on_missing,
   CMD_T_ANY | CMD_T_NOPARSE | CMD_T_INTERNAL, NULL},
/* A way to let people override the Huh? message */
  {"HUH_COMMAND", NULL, cmd_huh_command,
   CMD_T_ANY | CMD_T_NOPARSE | CMD_T_INTERNAL, NULL},
/* A way to let people override the unimplemented message */
  {"UNIMPLEMENTED_COMMAND", NULL, cmd_unimplemented,
   CMD_T_ANY | CMD_T_NOPARSE | CMD_T_INTERNAL, NULL},
  {NULL, NULL, NULL, 0, NULL}, 
};


/* switch_list is defined in switchinc.c */
#include "switchinc.c"

/** Table of command permissions/restrictions. */
struct command_perms_t command_perms[] = {
  {"player", CMD_T_PLAYER},
  {"thing", CMD_T_THING},
  {"exit", CMD_T_EXIT},
  {"room", CMD_T_ROOM},
  {"any", CMD_T_ANY},
  {"god", CMD_T_GOD},
  {"nobody", CMD_T_DISABLED},
  {"nogagged", CMD_T_NOGAGGED},
  {"noguest", CMD_T_NOGUEST},
  {"nofix", CMD_T_NOFIXED},
  {"nofixed", CMD_T_NOFIXED},
  {"norp", CMD_T_NORPMODE},
  {"norpmode", CMD_T_NORPMODE},
  {"logargs", CMD_T_LOGARGS},
  {"logname", CMD_T_LOGNAME},
#ifdef DANGEROUS
  {"listed", CMD_T_LISTED},
  {"switches", CMD_T_SWITCHES},
  {"internal", CMD_T_INTERNAL},
  {"ls_space", CMD_T_LS_SPACE},
  {"ls_noparse", CMD_T_LS_NOPARSE},
  {"rs_space", CMD_T_RS_SPACE},
  {"rs_noparse", CMD_T_RS_NOPARSE},
  {"eqsplit", CMD_T_EQSPLIT},
  {"ls_args", CMD_T_LS_ARGS},
  {"rs_args", CMD_T_RS_ARGS},
#endif
  {NULL, 0}
};


static void
strccat(char *buff, char **bp, const char *from)
{
  if (*buff)
    safe_str(", ", buff, bp);
  safe_str(from, buff, bp);
}
static int
switch_find(COMMAND_INFO *cmd, char *sw)
        {
  SWITCH_VALUE *sw_val;
  int len;

  if (!sw || !*sw)
    return 0;
  len = strlen(sw);
  /* Special case, for init */
  sw_val = switch_list;
  if (!cmd) {
    while (sw_val->name) {
      if (strcmp(sw_val->name, sw) == 0)
        return sw_val->value;
      sw_val++;
    }
    return 0;
  } else {
    while (sw_val->name) {
          if (SW_ISSET(cmd->sw, sw_val->value) && (strncmp(sw_val->name, sw, len) == 0))
                  return sw_val->value;
      sw_val++;
    }
  }
  return 0;
}

/** Allocate and populate a COMMAND_INFO structure.
 * This function generates a new COMMAND_INFO structure, populates it
 * with given values, and returns a pointer. It should not be used
 * for local hacks - use command_add() instead.
 * \param name command name.
 * \param type types of objects that can use the command.
 * \param flagmask mask of flags (one is sufficient to use the command).
 * \param powers mask of powers (one is sufficient to use the command).
 * \param sw mask of switches the command accepts.
 * \param func function to call when the command is executed.
 * \return pointer to a newly allocated COMMAND_INFO structure.
 */
COMMAND_INFO *
make_command(const char *name, int type, switch_mask *sw, command_func func, const char *command_lock)
{
  COMMAND_INFO *cmd;
  cmd = (COMMAND_INFO *) mush_malloc(sizeof(COMMAND_INFO), "command");
  memset(cmd, 0, sizeof(COMMAND_INFO));
  cmd->name = name;
  cmd->restrict_message = NULL;
  cmd->func = func;
  cmd->type = type;
  cmd->lock = !command_lock ? TRUE_BOOLEXP : parse_boolexp(GOD, command_lock, "Command_Lock");
  if (sw)
    memcpy(cmd->sw, sw, sizeof(switch_mask));
  else
    SW_ZERO(cmd->sw);
  cmd->hooks.before.obj = NOTHING;
  cmd->hooks.before.attrname = NULL;
  cmd->hooks.after.obj = NOTHING;
  cmd->hooks.after.attrname = NULL;
  cmd->hooks.ignore.obj = NOTHING;
  cmd->hooks.ignore.attrname = NULL;
  cmd->hooks.override.obj = NOTHING;
  cmd->hooks.override.attrname = NULL;
  return cmd;
}

/** Add a new command to the command table.
 * This function is the top-level function for adding a new command.
 * \param name name of the command.
 * \param type types of objects that can use the command.
 * \param switchstr space-separated list of switches for the command.
 * \param func function to call when command is executed.
 * \param command_lock is the lock boolexp string to lock command to
 * \return pointer to a newly allocated COMMAND_INFO entry or NULL.
 */
COMMAND_INFO *
command_add(const char *name, int type, const char *switchstr, command_func func, const char *command_lock)
{
  switch_mask *sw = switchmask(switchstr);

  ptab_start_inserts(&ptab_command);
  ptab_insert(&ptab_command, name,
              make_command(name, type, sw, func, command_lock));
  ptab_end_inserts(&ptab_command);
  return command_find(name);
}


/** Search for a command by (partial) name.
 * This function searches the command table for a (partial) name match
 * and returns a pointer to the COMMAND_INFO entry. It returns NULL
 * if the name given is a reserved alias or if no command table entry
 * matches.
 * \param name name of command to match.
 * \return pointer to a COMMAND_INFO entry, or NULL.
 */
COMMAND_INFO *
command_find(const char *name)
{

  char cmdname[BUFFER_LEN];
  strcpy(cmdname, name);
  upcasestr(cmdname);
  if (hash_find(&htab_reserved_aliases, cmdname))
    return NULL;
  return (COMMAND_INFO *) ptab_find(&ptab_command, cmdname);
}

/** Search for a command by exact name.
 * This function searches the command table for an exact name match
 * and returns a pointer to the COMMAND_INFO entry. It returns NULL
 * if the name given is a reserved alias or if no command table entry
 * matches.
 * \param name name of command to match.
 * \return pointer to a COMMAND_INFO entry, or NULL.
 */
COMMAND_INFO *
command_find_exact(const char *name)
{

  char cmdname[BUFFER_LEN];
  strcpy(cmdname, name);
  upcasestr(cmdname);
  if (hash_find(&htab_reserved_aliases, cmdname))
    return NULL;
  return (COMMAND_INFO *) ptab_find_exact(&ptab_command, cmdname);
}


/** Modify a command's entry in the table.
 * Given a command name and other parameters, look up the command
 * in the table, and if it's there, modify the parameters.
 * \param name name of command to modify.
 * \param type new types for command, or -1 to leave unchanged.
 * \param flagmask new mask of flags for command, or NULL to leave unchanged.
 * \param powers new mask of powers for command, or -1 to leave unchanged.
 * \param sw new mask of switches for command, or NULL to leave unchanged.
 * \param func new function to call, or NULL to leave unchanged.
 * \return pointer to modified command entry, or NULL.
 */
COMMAND_INFO *
command_modify(const char *name, int type, const char *command_lock, switch_mask *sw, command_func func)
{
  COMMAND_INFO *cmd;
  cmd = command_find(name);
  if (!cmd)
    return NULL;
  if (type != -1)
    cmd->type = type;
  if (command_lock) {
    if(cmd->lock != TRUE_BOOLEXP)
      free_boolexp(cmd->lock);
    cmd->lock = parse_boolexp(GOD, command_lock, "Command_Lock");
  }
  if (sw)
    memcpy(cmd->sw, sw, sizeof(switch_mask));
  if (func)
    cmd->func = func;
  return cmd;
}

/** Convert a switch string to a switch mask.
 * Given a space-separated list of switches in string form, return
 * a pointer to a static switch mask.
 * \param switches list of switches as a string.
 * \return pointer to a static switch mask.
 */
switch_mask *
switchmask(const char *switches)
{
  static switch_mask sw;
  char buff[BUFFER_LEN];
  char *p, *s;
  int switchnum;
  SW_ZERO(sw);
  if (!switches || !switches[0])
    return NULL;
  strcpy(buff, switches);
  p = buff;
  while (p) {
    s = split_token(&p, ' ');
    switchnum = switch_find(NULL, s);
    if (!switchnum)
      return NULL;
    else
      SW_SET(sw, switchnum);
  }
  return &sw;
}

/** Add an alias to the table of reserved aliases.
 * This function adds an alias to the table of reserved aliases, preventing
 * it from being matched for standard commands. It's typically used to
 * insure that 'e' will match a global 'east;e' exit rather than the
 * 'examine' command.
 * \param a alias to reserve.
 */
void
reserve_alias(const char *a)
{
  static char placeholder[2] = "x";
  hashadd(strupper(a), (void *) placeholder, &htab_reserved_aliases);
}

/** Initialize command tables (before reading config file).
 * This function performs command table initialization that should take place
 * before the configuration file has been read. It initializes the
 * command prefix table and the reserved alias table, inserts all of the
 * commands from the commands array into the prefix table, initializes
 * the command permission prefix table, and inserts all the permissions
 * from the command_perms array into the prefix table. Finally, it
 * calls local_commands() to do any cmdlocal.c work.
 */
void
command_init_preconfig(void)
{
  struct command_perms_t *c;
  COMLIST *cmd;
  static int done = 0;
  if (done == 1)
    return;
  done = 1;

  ptab_init(&ptab_command);
  hashinit(&htab_reserved_aliases, 16, sizeof(COMMAND_INFO));
/*  reserve_aliases();  this can now be handeled in respective modules */
  ptab_start_inserts(&ptab_command);
  for (cmd = commands; cmd->name; cmd++) {
    ptab_insert(&ptab_command, cmd->name,
                make_command(cmd->name, cmd->type, switchmask(cmd->switches),
                             cmd->func, cmd->command_lock));
  }
  ptab_end_inserts(&ptab_command);

  ptab_init(&ptab_command_perms);
  ptab_start_inserts(&ptab_command_perms);
  for (c = command_perms; c->name; c++)
    ptab_insert(&ptab_command_perms, c->name, c);
  ptab_end_inserts(&ptab_command_perms);

  /* no longer needed with modules. local_commands(); */
}

/** Initialize commands (after reading config file).
 * This function performs command initialization that should take place
 * after the configuration file has been read.
 * Currently, there isn't any.
 */
void
command_init_postconfig(void)
{
  return;
}


/** Alias a command.
 * Given a command name and an alias for it, install the alias.
 * \param command canonical command name.
 * \param alias alias to associate with command.
 * \retval 0 failure (couldn't locate command).
 * \retval 1 success.
 */
int
alias_command(const char *command, const char *alias)
{
  COMMAND_INFO *cmd;

  /* Make sure the alias doesn't exit already */
  if (command_find_exact(alias))
    return 0;

  /* Look up the original */
  cmd = command_find_exact(command);
  if (!cmd)
    return 0;

  ptab_start_inserts(&ptab_command);
  ptab_insert(&ptab_command, strupper(alias), cmd);
  ptab_end_inserts(&ptab_command);
  return 1;
}

/* This is set to true for EQ_SPLIT commands that actually have a rhs.
 * Used in @teleport, ATTRSET and possibly other checks. It's ugly.
 * Blame Talek for it. ;)
 */
int rhs_present;

/** Parse the command arguments into arrays.
 * This function does the real work of parsing command arguments into
 * argument arrays. It is called separately to parse the left and
 * right sides of commands that are split at equal signs.
 * \param player the enactor.
 * \param cause the dbref causing the execution.
 * \param from pointer to address of where to parse arguments from.
 * \param to string to store parsed arguments into.
 * \param argv array of parsed arguments.
 * \param cmd pointer to command data.
 * \param right_side if true, parse on the right of the =. Otherwise, left.
 * \param forcenoparse if true, do no evaluation during parsing.
 */
void
command_argparse(dbref player, dbref realcause, dbref cause, char **from, char *to,
                 char *argv[], COMMAND_INFO *cmd, int right_side,
                 int forcenoparse)
{
  int parse, split, args, i, done;
  char *t, *f;
  char *aold;

  f = *from;

  parse =
    (right_side) ? (cmd->type & CMD_T_RS_NOPARSE) : (cmd->type & CMD_T_NOPARSE);
  if (parse || forcenoparse)
    parse = PE_NOTHING;
  else
    parse = PE_DEFAULT | PE_COMMAND_BRACES;

  if (right_side)
    split = PT_NOTHING;
  else
    split = (cmd->type & CMD_T_EQSPLIT) ? PT_EQUALS : PT_NOTHING;

  if (right_side) {
    if (cmd->type & CMD_T_RS_ARGS)
      args = (cmd->type & CMD_T_RS_SPACE) ? PT_SPACE : PT_COMMA;
    else
      args = 0;
  } else {
    if (cmd->type & CMD_T_LS_ARGS)
      args = (cmd->type & CMD_T_LS_SPACE) ? PT_SPACE : PT_COMMA;
    else
      args = 0;
  }

  if ((parse == PE_NOTHING) && args)
    parse = PE_COMMAND_BRACES;

  i = 1;
  done = 0;
  *to = '\0';

  if (args) {
    t = to + 1;
  } else {
    t = to;
  }

  while (*f && !done) {
    aold = t;
    while (*f == ' ')
      f++;
    process_expression(to, &t, (const char **) &f, player, realcause, cause,
                       parse, (split | args), NULL);
    *t = '\0';
    if (args) {
      argv[i] = aold;
      if (*f)
        f++;
      i++;
      t++;
      if (i == MAX_ARG)
        done = 1;
    }
    if (split && (*f == '=')) {
      rhs_present = 1;
      f++;
      *from = f;
      done = 1;
    }
  }

  *from = f;

  if (args)
    while (i < MAX_ARG)
      argv[i++] = NULL;
}

/** Determine whether a command is an attribute to set an attribute.
 * Is this command an attempt to set an attribute like @VA or &NUM?
 * If so, return the attrib's name. Otherwise, return NULL
 * \param command command string (first word of input).
 * \return name of the attribute to be set, or NULL.
 */
static const char *
command_isattr(char *command)
{
  ATTR *a;
  char buff[BUFFER_LEN];
  char *f, *t;

  if (((command[0] == '&') && (command[1])) ||
      ((command[0] == '@') && (command[1] == '_') && (command[2]))) {
    /* User-defined attributes: @_NUM or &NUM */
    if (command[0] == '@')
      return command + 2;
    else
      return command + 1;
  } else if (command[0] == '@') {
    f = command + 1;
    buff[0] = '@';
    t = buff + 1;
    while ((*f) && (*f != '/'))
      *t++ = *f++;
    *t = '\0';
    /* @-commands have priority over @-attributes with the same name */
    if (command_find(buff))
      return NULL;
    a = atr_match(buff + 1);
    if (a)
      return AL_NAME(a);
  }
  return NULL;
}

/** A handy macro to free up the command_parse-allocated variables */
#define command_parse_free_args \
    mush_free((Malloc_t) command, "string"); \
    mush_free((Malloc_t) swtch, "string"); \
    mush_free((Malloc_t) ls, "string"); \
    mush_free((Malloc_t) rs, "string"); \
    mush_free((Malloc_t) switches, "string")

/** Parse commands.
 * Parse the commands. This is the big one!
 * We distinguish parsing of input sent from a player at a socket
 * (in which case attribute values to set are not evaluated) and
 * input sent in any other way (in which case attribute values to set
 * are evaluated, and attributes are set NO_COMMAND).
 * Return NULL if the command was recognized and handled, the evaluated
 * text to match against $-commands otherwise.
 * \param player the enactor.
 * \param cause dbref that caused the command to be executed.
 * \param string the input to be parsed.
 * \param fromport if true, command was typed by a player at a socket.
 * \return NULL if a command was handled, otherwise the evaluated input.
 */
char *
command_parse(dbref player, dbref cause, dbref realcause, char *string, int fromport)
{
  char *command, *swtch, *ls, *rs, *switches;
  static char commandraw[BUFFER_LEN];
  static char exit_command[BUFFER_LEN], *ec;
  char *lsa[MAX_ARG];
  char *rsa[MAX_ARG];
  char *ap, *swp;
  const char *attrib, *replacer;
  COMMAND_INFO *cmd;
  char *p, *t, *c, *c2;
  char command2[BUFFER_LEN];
  char b;
  int switchnum;
  switch_mask sw;
  char switch_err[BUFFER_LEN], *se;
  int noeval;
  int noevtoken = 0;
  char *retval;

  rhs_present = 0;

  command = (char *) mush_malloc(BUFFER_LEN, "string");
  swtch = (char *) mush_malloc(BUFFER_LEN, "string");
  ls = (char *) mush_malloc(BUFFER_LEN, "string");
  rs = (char *) mush_malloc(BUFFER_LEN, "string");
  switches = (char *) mush_malloc(BUFFER_LEN, "string");
  if (!command || !swtch || !ls || !rs || !switches)
    mush_panic("Couldn't allocate memory in command_parse");
  p = string;
  replacer = NULL;
  attrib = NULL;
  cmd = NULL;
  c = command;
  /* All those one character commands.. Sigh */

  global_fun_invocations = global_fun_recursions = 0;
  if (*p == NOEVAL_TOKEN) {
    noevtoken = 1;
    p = string + 1;
    string = p;
    memmove(global_eval_context.ccom, (char *) global_eval_context.ccom + 1, BUFFER_LEN - 1);
  }
  if (*p == '[') {
    if ((cmd = command_find("WARN_ON_MISSING"))) {
      if (!(cmd->type & CMD_T_DISABLED)) {
        cmd->func(cmd, player, cause, sw, string, NULL, NULL, ls, lsa, rs, rsa, fromport);
        command_parse_free_args;
        return NULL;
      }
    }
  }
  switch (*p) {
  case '\0':
    /* Just in case. You never know */
    command_parse_free_args;
    return NULL;
  case SAY_TOKEN:
    replacer = "SAY";
    if (CHAT_STRIP_QUOTE)
      p--;                      /* Since 'say' strips out the '"' */
    break;
  case POSE_TOKEN:
    replacer = "POSE";
    break;
  case SEMI_POSE_TOKEN:
    if (*(p + 1) && *(p + 1) == ' ')
      replacer = "POSE";
    else
      replacer = "SEMIPOSE";
    break;
  case EMIT_TOKEN:
    replacer = "@EMIT";
    break;
#ifdef CHAT_SYSTEM
  case CHAT_TOKEN:
#ifdef CHAT_TOKEN_ALIAS
  case CHAT_TOKEN_ALIAS:
#endif
    /* parse_chat() destructively modifies the command to replace
     * the first space with a '=' if the command is an actual
     * chat command */
    if (parse_chat(player, p + 1) && command_check_byname(player, "@CHAT")) {
      /* This is a "+chan foo" chat style
       * We set noevtoken to keep its noeval way, and
       * set the cmd to allow @hook. */
      replacer = "@CHAT";
      noevtoken = 1;
      break;
    }
#endif /* CHAT_SYSTEM */
  case NUMBER_TOKEN:
    /* parse_force() destructively modifies the command to replace
     * the first space with a '=' if the command is an actual
     * chat command */
    if (Mobile(player) && parse_force(p)) {
      replacer = "@FORCE";
      noevtoken = 1;
    }
    break;
  }

  if (replacer) {
    cmd = command_find(replacer);
    if(*p != NUMBER_TOKEN)
      p++;
  } else {
    /* At this point, we have not done a replacer, so we continue with the
     * usual processing. Exits have next priority.  We still pass them
     * through the parser so @hook on GOTO can work on them.
     */
    if (strcasecmp(p, "home") && can_move(player, p)) {
      ec = exit_command;
      safe_str("GOTO ", exit_command, &ec);
      safe_str(p, exit_command, &ec);
      *ec = '\0';
      p = string = exit_command;
      noevtoken = 1; /* But don't parse the exit name! */
    }
    c = command;
    while (*p == ' ')
      p++;
    process_expression(command, &c, (const char **) &p, player, realcause,
                       cause, noevtoken ? PE_NOTHING :
                                          ((PE_DEFAULT & ~PE_FUNCTION_CHECK)
                                           | PE_COMMAND_BRACES),
                       PT_SPACE, NULL);
    *c = '\0';
    strcpy(commandraw, command);
    upcasestr(command);

    /* Catch &XX and @XX attribute pairs. If that's what we've got,
     * use the magical ATTRIB_SET command
     */
    attrib = command_isattr(command);
    if (attrib) {
      cmd = command_find("ATTRIB_SET");
    } else {
      c = command;
      while ((*c) && (*c != '/') && (*c != ' '))
        c++;
      b = *c;
      *c = '\0';
      cmd = command_find(command);
      *c = b;
      /* Is this for internal use? If so, players can't use it! */
      if (cmd && (cmd->type & CMD_T_INTERNAL))
        cmd = NULL;
    }
  }

  /* Set up commandraw for future use. This will contain the canonicalization
   * of the command name and may later have the parsed rest of the input
   * appended at the position pointed to by c2.
   */
  c2 = c;
  if (!cmd) {
    c2 = commandraw + strlen(commandraw);
  } else {
    if (replacer) {
      /* These commands don't allow switches, and need a space
       * added after their canonical name
       */
      c2 = commandraw;
      safe_str(cmd->name, commandraw, &c2);
      safe_chr(' ', commandraw, &c2);
    } else if (*c2 == '/') {
      /* Oh... DAMN */
      c2 = commandraw;
      strcpy(switches, commandraw);
      safe_str(cmd->name, commandraw, &c2);
      t = strchr(switches, '/');
      safe_str(t, commandraw, &c2);
    } else {
      c2 = commandraw;
      safe_str(cmd->name, commandraw, &c2);
    }
  }

  /* Test if this either isn't a command, or is a disabled one
   * If so, return Fully Parsed string for further processing.
   */

  if (!cmd || (cmd->type & CMD_T_DISABLED)) {
    if (*p) {
      if (*p == ' ') {
        safe_chr(' ', commandraw, &c2);
        p++;
      }
      process_expression(commandraw, &c2, (const char **) &p, player, realcause,
                         cause, noevtoken ? PE_NOTHING :
                         ((PE_DEFAULT & ~PE_FUNCTION_CHECK) |
                          PE_COMMAND_BRACES), PT_DEFAULT, NULL);
    }
    *c2 = '\0';
    command_parse_free_args;
    return commandraw;
  }

 /* Parse out any switches */
  SW_ZERO(sw);
  swp = switches;
  *swp = '\0';
  se = switch_err;

  t = NULL;

  /* Don't parse switches for one-char commands */
  if (!replacer) {
    while (*c == '/') {
      t = swtch;
      c++;
      while ((*c) && (*c != ' ') && (*c != '/'))
        *t++ = *c++;
      *t = '\0';
      switchnum = switch_find(cmd, upcasestr(swtch));
      if (!switchnum) {
        if (cmd->type & CMD_T_SWITCHES) {
          if (*swp)
            strcat(swp, " ");
          strcat(swp, swtch);
        } else {
          if (se == switch_err)
            safe_format(switch_err, &se,
                        T("%s doesn't know switch %s."), cmd->name, swtch);
        }
      } else {
        SW_SET(sw, switchnum);
      }
    }
  }

  *se = '\0';
  if (!t)
    SW_SET(sw, SWITCH_NONE);
  if (noevtoken)
    SW_SET(sw, SWITCH_NOEVAL);

  /* Check the permissions */
  if (!command_check(player, cmd, sw)) {
    command_parse_free_args;
    return NULL;
  }
 

  /* If we're calling ATTRIB_SET, the switch is the attribute name */
  if (attrib)
    swp = (char *) attrib;
  else if (!*swp)
    swp = NULL;

  strcpy(command2, p);
  if (*p == ' ')
    p++;
  ap = p;

  /* noeval and direct players.
   * If the noeval switch is set:
   *  (1) if we split on =, and an = is present, eval lhs, not rhs
   *  (2) if we split on =, and no = is present, do not eval arg
   *  (3) if we don't split on =, do not eval arg
   * Special case for ATTRIB_SET by a directly connected player:
   * Treat like noeval, except for #2. Eval arg if no =.
   */

  if ((cmd->func == command_atrset) && fromport) {
    /* Special case: eqsplit, noeval of rhs only */
    command_argparse(player, realcause, cause, &p, ls, lsa, cmd, 0, 0);
    command_argparse(player, realcause, cause, &p, rs, rsa, cmd, 1, 1);
    SW_SET(sw, SWITCH_NOEVAL);  /* Needed for ATTRIB_SET */
  } else {
    noeval = SW_ISSET(sw, SWITCH_NOEVAL) || noevtoken;
    if (cmd->type & CMD_T_EQSPLIT) {
      char *savep = p;
      command_argparse(player, realcause, cause, &p, ls, lsa, cmd, 0, noeval);
      if (noeval && !noevtoken && *p) {
        /* oops, we have a right hand side, should have evaluated */
        p = savep;
        command_argparse(player, realcause, cause, &p, ls, lsa, cmd, 0, 0);
      }
      command_argparse(player, realcause, cause, &p, rs, rsa, cmd, 1, noeval);
    } else {
      command_argparse(player, realcause, cause, &p, ls, lsa, cmd, 0, noeval);
    }
  }


  /* Finish setting up commandraw, if we may need it for hooks */
  if (has_hook(&cmd->hooks.ignore) || has_hook(&cmd->hooks.override)) {
    p = command2;
    if (*p && (*p == ' ')) {
      safe_chr(' ', commandraw, &c2);
      p++;
    }
    if (cmd->type & CMD_T_ARGS) {
      int lsa_index;
      if (lsa[1]) {
        safe_str(lsa[1], commandraw, &c2);
        for (lsa_index = 2; lsa[lsa_index]; lsa_index++) {
          safe_chr(',', commandraw, &c2);
          safe_str(lsa[lsa_index], commandraw, &c2);
        }
      }
    } else {
      safe_str(ls, commandraw, &c2);
    }
    if (cmd->type & CMD_T_EQSPLIT) {
      if(rhs_present) {
	safe_chr('=', commandraw, &c2);
	if (cmd->type & CMD_T_RS_ARGS) {
	  int rsa_index;
	  /* This is counterintuitive, but rsa[]
	   * starts at 1. */
	  if (rsa[1]) {
	    safe_str(rsa[1], commandraw, &c2);
	    for (rsa_index = 2; rsa[rsa_index]; rsa_index++) {
	      safe_chr(',', commandraw, &c2);
	      safe_str(rsa[rsa_index], commandraw, &c2);
	    }
	  }
	}
      }
#ifdef NEVER
      /* We used to do this, but we're not sure why */
      process_expression(commandraw, &c2, (const char **) &p, player, realcause,
                         cause, noevtoken ? PE_NOTHING :
                         ((PE_DEFAULT & ~PE_FUNCTION_CHECK) |
                          PE_COMMAND_BRACES), PT_DEFAULT, NULL);
#endif
    }
    *c2 = '\0';
  }

  retval = NULL;
  if (cmd->func == NULL) {
    do_rawlog(LT_ERR, T("No command vector on command %s."), cmd->name);
    command_parse_free_args;
    return NULL;
  } else {
    char *saveregs[NUMQ];
    init_global_regs(saveregs);
    /* If we have a hook/ignore that returns false, we don't do the command */
    if (run_hook(player, cause, &cmd->hooks.ignore, saveregs, 1)) {
      /* If we have a hook/override, we use that instead */
      if (!has_hook(&cmd->hooks.override) ||
          !one_comm_match(cmd->hooks.override.obj, player,
                          cmd->hooks.override.attrname, commandraw)) {
        /* Otherwise, we do hook/before, the command, and hook/after */
        /* But first, let's see if we had an invalid switch */
        if (*switch_err) {
          notify(player, switch_err);
          free_global_regs("hook.regs", saveregs);
          command_parse_free_args;
          return NULL;
        }
        run_hook(player, cause, &cmd->hooks.before, saveregs, 1);
        cmd->func(cmd, player, cause, sw, string, swp, ap, ls, lsa, rs, rsa, fromport);
        run_hook(player, cause, &cmd->hooks.after, saveregs, 0);
      }
      /* Either way, we might log */
      if (cmd->type & CMD_T_LOGARGS)
        do_log(LT_CMD, player, cause, "%s", string);
      else if (cmd->type & CMD_T_LOGNAME)
        do_log(LT_CMD, player, cause, "%s", commandraw);
    } else {
      retval = commandraw;
    }
    free_global_regs("hook.regs", saveregs);
  }

  command_parse_free_args;
  return retval;
}

#undef command_parse_free_args
/** Execute the huh_command when no command is matched.
 *  param player the enactor.
 *  param cause dbref that caused the command to be executed.
 *  param string the input given.
 */
void
generic_command_failure(dbref player, dbref cause, char *string, int fromport)
{
  COMMAND_INFO *cmd;
  char *saveregs[NUMQ];

  if ((cmd = command_find("HUH_COMMAND"))) {
    if (!(cmd->type & CMD_T_DISABLED)) {
      init_global_regs(saveregs);
      if (run_hook(player, cause, &cmd->hooks.ignore, saveregs, 1)) {
        /* If we have a hook/override, we use that instead */
        if (!has_hook(&cmd->hooks.override) ||
            !one_comm_match(cmd->hooks.override.obj, player,
                            cmd->hooks.override.attrname, "HUH_COMMAND")) {
          /* Otherwise, we do hook/before, the command, and hook/after */
          run_hook(player, cause, &cmd->hooks.before, saveregs, 1);
          cmd->func(cmd, player, cause, NULL, string, NULL, NULL, string, NULL,
                    NULL, NULL, fromport);
          run_hook(player, cause, &cmd->hooks.after, saveregs, 0);
        }
        /* Either way, we might log */
        if (cmd->type & CMD_T_LOGARGS)
          do_log(LT_HUH, player, cause, "%s", string);
      }
      free_global_regs("hook.regs", saveregs);
    }
  }
}


/** Add a restriction to a command.
 * Given a command name and a restriction, apply the restriction to the
 * command in addition to whatever its usual restrictions are.
 * This is used by the configuration file startup in conf.c
 * Valid restrictions are:
 * \verbatim
 *   nobody     disable the command
 *   nogagged   can't be used by gagged players
 *   nofixed    can't be used by fixed players
 *   noguest    can't be used by guests
 *   admin      can only be used by admins
 *   director   can only be used by directors
 *   god        can only be used by god
 *   noplayer   can't be used by players, just objects/rooms/exits
 *   logargs    log name and arguments when command is run
 *   logname    log just name when command is run
 * \endverbatim
 * Return 1 on success, 0 on failure.
 * \param name name of command to restrict.
 * \param restriction space-separated string of restrictions
 * \retval 1 successfully restricted command.
 * \retval 0 failure (unable to find command name).
 */
int
restrict_command(const char *name, const char *restriction)
{
  COMMAND_INFO *command;
  struct command_perms_t *c;
  char *message;
  int clear;
  char *tp;

  if (!name || !*name || !restriction || !*restriction ||
      !(command = command_find(name)))
    return 0;

  if (command->restrict_message) {
    mush_free((Malloc_t) command->restrict_message, "cmd_restrict_message");
    command->restrict_message = NULL;
  }

  message = strchr(restriction, '"');
  if (message) {
    *(message++) = '\0';
    if ((message = trim_space_sep(message, ' ')) && *message)
      command->restrict_message = mush_strdup(message, "cmd_restrict_message");
  }

  while (restriction && *restriction) {
    if ((tp = strchr(restriction, ' ')))
      *tp++ = '\0';

    clear = 0;
    if (*restriction == '!') {
      restriction++;
      clear = 1;
    }

    if (!strcasecmp(restriction, "noplayer")) {
      /* Pfft. And even !noplayer works. */
      clear = !clear;
      restriction += 2;
    }

    if ((c = ptab_find(&ptab_command_perms, restriction))) {
      if (clear)
        command->type &= ~c->type;
      else
        command->type |= c->type;
    }
    restriction = tp;
  }
  return 1;
}

int command_lock(const char *name, const char *lock) {
  COMMAND_INFO *command;
  char *message;
  boolexp key;

  if(!name || !*name || !lock || !*lock || !(command = command_find(name)))
    return 0;

  message = strchr(lock, '"');
  if(message) {
    *(message++) = '\0';
    if((message = trim_space_sep(message, ' ')) && *message) {
      if(command->restrict_message) 
        mush_free((Malloc_t) command->restrict_message, "cmd_restrict_message");
      command->restrict_message = mush_strdup(message, "cmd_restrict_message");
    }
  }
  /* Check to see if there is already a boolexp there.. If so free */
  if(command->lock != TRUE_BOOLEXP)
    free_boolexp(command->lock);

  key = parse_boolexp(GOD, lock, "Command_Lock");

  if(key == TRUE_BOOLEXP)
    return 0;
  command->lock = key;

  return 1;
}

/** Command stub for \@command/add-ed commands.
 * This does nothing more than notify the player
 * with "This command has not been implemented"
 */
COMMAND(cmd_unimplemented) {
  char *saveregs[NUMQ];

  if (strcmp(cmd->name, "UNIMPLEMENTED_COMMAND") != 0 &&
      (cmd = command_find("UNIMPLEMENTED_COMMAND"))) {
    if (!(cmd->type & CMD_T_DISABLED)) {
      init_global_regs(saveregs);
      if (run_hook(player, cause, &cmd->hooks.ignore, saveregs, 1)) {
      /* If we have a hook/override, we use that instead */
        if (!has_hook(&cmd->hooks.override) ||
            !one_comm_match(cmd->hooks.override.obj, player,
                            cmd->hooks.override.attrname, "HUH_COMMAND")) {
          /* Otherwise, we do hook/before, the command, and hook/after */
          run_hook(player, cause, &cmd->hooks.before, saveregs, 1);

          cmd->func(cmd, player, cause, sw, raw, switches, args_raw,
                    arg_left, args_left, arg_right, args_right, fromport);
          run_hook(player, cause, &cmd->hooks.after, saveregs, 0);
        }
      }
      free_global_regs("hook.regs", saveregs);
      return;
    }
  }

  /* Either we were already in UNIMPLEMENTED_COMMAND, or we couldn't find it */
  notify(player, "This command has not been implemented");
}

/** Adds a user-added command
 * \verbatim
 * This code implements @command/add, which adds a
 * command with cmd_unimplemented as a stub
 * \endverbatim
 * \param player the enactor
 * \param name the name
 * \param flags CMD_T_* flags
 */
void
do_command_add(dbref player, char *name, int flags)
{
  COMMAND_INFO *command;

  if (!God(player)) {
    notify(player, T("Permission denied."));
    return;
  }
  name = trim_space_sep(name, ' ');
  upcasestr(name);
  command = command_find(name);
  if (!command) {
    if (!ok_command_name(name)) {
      notify(player, T("Bad command name."));
    } else {
      command_add(mush_strdup(name, "command_add"),
                  flags, (flags & CMD_T_NOPARSE ? NULL : "NOEVAL"),
                  cmd_unimplemented, NULL);
      notify_format(player, T("Command %s added."), name);
    }
  } else {
    notify_format(player, T("Command %s already exists"), command->name);
  }
}

/** Deletes a user-added command
 * \verbatim
 * This code implements @command/delete, which deletes a
 * command added via @command/add
 * \endverbatim
 * \param player the enactor
 * \param name name of the command to delete
 */
void
do_command_delete(dbref player, char *name)
{
  int acount;
  char alias[BUFFER_LEN];
  COMMAND_INFO *cptr;
  COMMAND_INFO *command;

  if (!God(player)) {
    notify(player, T("Permission denied."));
    return;
  }
  upcasestr(name);
  command = command_find_exact(name);
  if(!command) {
          notify(player, T("No such command."));
          return;
  }
  if (strcasecmp(command->name, name) == 0) {
    /* This is the command, not an alias */
    if (command->func != cmd_unimplemented) {
      notify(player, T ("You can't delete built-in commands. @command/disable instead."));
      return;
    } else {
      acount = 0;
      cptr = ptab_firstentry_new(&ptab_command, alias);
      while (cptr) {
      if (cptr == command) {
        ptab_delete(&ptab_command, alias);
        acount++;
        cptr = ptab_firstentry_new(&ptab_command, alias);
      } else
        cptr = ptab_nextentry_new(&ptab_command, alias);
      }
      mush_free((Malloc_t) command->name, "command_add");
      mush_free((Malloc_t) command, "command");
      if (acount > 1)
     notify_format(player, T("Removed %s and aliases from command table."), name);
      else
      notify_format(player, T("Removed %s from command table."), name);
    }
  } else {
    /* This is an alias. Just remove it */
    ptab_delete(&ptab_command, name);
    notify_format(player, T("Removed %s from command table."), name);
  }
}

/** Definition of the \@command command.
 * This is the only command which should be defined in this
 * file, because it uses variables from this file, etc.
 */
COMMAND (cmd_command) {
  COMMAND_INFO *command;
  SWITCH_VALUE *sw_val;
  char buff[BUFFER_LEN];
  char *bp = buff;

  if (!arg_left[0]) {
    notify(player, T("You must specify a command."));
    return;
  }
  if (SW_ISSET(sw, SWITCH_ADD)) {
    int flags = CMD_T_ANY;
    flags |= SW_ISSET(sw, SWITCH_NOPARSE) ? CMD_T_NOPARSE : 0;
    flags |= SW_ISSET(sw, SWITCH_RSARGS) ? CMD_T_RS_ARGS : 0;
    flags |= SW_ISSET(sw, SWITCH_LSARGS) ? CMD_T_LS_ARGS : 0;
    flags |= SW_ISSET(sw, SWITCH_LSARGS) ? CMD_T_LS_ARGS : 0;
    flags |= SW_ISSET(sw, SWITCH_EQSPLIT) ? CMD_T_EQSPLIT : 0;
    if (SW_ISSET(sw, SWITCH_NOEVAL))
      notify(player,
             T
             ("WARNING: /NOEVAL no longer creates a Noparse command.\n         Use /NOPARSE if that's what you meant."));
    do_command_add(player, arg_left, flags);
    return;
  }
  if (SW_ISSET(sw, SWITCH_ALIAS)) {
    if (Director(player)) {
      if (!ok_command_name(upcasestr(arg_right))) {
        notify(player, "I can't alias a command to that!");
      } else if (!alias_command(arg_left, arg_right)) {
        notify(player, "Unable to set alias.");
      } else {
        if (!SW_ISSET(sw, SWITCH_QUIET))
          notify(player, "Alias set.");
      }
    } else {
      notify(player, T("Permission denied."));
    }
    return;
  }

  if (SW_ISSET(sw, SWITCH_DELETE)) {
    do_command_delete(player, arg_left);
    return;
  }
  command = command_find(arg_left);
  if (!command) {
    notify(player, T("No such command."));
    return;
  }
  if (Site(player)) {
    if (SW_ISSET(sw, SWITCH_ON) || SW_ISSET(sw, SWITCH_ENABLE))
      command->type &= ~CMD_T_DISABLED;
    else if (SW_ISSET(sw, SWITCH_OFF) || SW_ISSET(sw, SWITCH_DISABLE))
      command->type |= CMD_T_DISABLED;

    if(SW_ISSET(sw, SWITCH_LOCK)) {
      if(arg_right && *arg_right) {
        boolexp key;

        key = parse_boolexp(player, arg_right, "Command");
        if(key != TRUE_BOOLEXP)  {
          if(command->lock != TRUE_BOOLEXP)
            free_boolexp(command->lock);
          command->lock = key;
          notify(player, "Command locked.");
        } else notify(player, T("I don't understand that key."));
      } else {
        if(command->lock != TRUE_BOOLEXP) 
          free_boolexp(command->lock);
        command->lock = TRUE_BOOLEXP;
        notify(player, "Command unlocked.");
      }
      return;
    }

    if (SW_ISSET(sw, SWITCH_RESTRICT)) {
      if (!arg_right || !arg_right[0]) {
        notify(player, T("How do you want to restrict the command?"));
        return;
      }

      if (!restrict_command(arg_left, arg_right))
        notify(player, T("Restrict attempt failed."));
    }

    if ((command->func == cmd_command) && (command->type & CMD_T_DISABLED)) {
      notify(player, T("@command is ALWAYS enabled."));
      command->type &= ~CMD_T_DISABLED;
    }
  }
  if (!SW_ISSET(sw, SWITCH_QUIET)) {
    notify_format(player,
                  "Name         : %s (%s)", command->name,
                  (command->type & CMD_T_DISABLED) ? "Disabled" : "Enabled");
    if ((command->type & CMD_T_ANY) == CMD_T_ANY)
      safe_strl("Any", 3, buff, &bp);
    else {
      buff[0] = '\0';
      if (command->type & CMD_T_ROOM)
        strccat(buff, &bp, "Room");
      if (command->type & CMD_T_THING)
        strccat(buff, &bp, "Thing");
      if (command->type & CMD_T_EXIT)
        strccat(buff, &bp, "Exit");
      if (command->type & CMD_T_PLAYER)
        strccat(buff, &bp, "Player");
      if (command->type & CMD_T_DIVISION)
        strccat(buff, &bp, "Division");
    }
    *bp = '\0';
    notify_format(player, "Types        : %s", buff);
    buff[0] = '\0';
    bp = buff;
    if (command->type & CMD_T_SWITCHES)
      strccat(buff, &bp, "Switches");
    if (command->type & CMD_T_NOGAGGED)
      strccat(buff, &bp, "Nogagged");
    if (command->type & CMD_T_NOFIXED)
      strccat(buff, &bp, "Nofixed");
    if(command->type & CMD_T_NORPMODE)
      strccat(buff, &bp, "NoRPMode");
    if (command->type & CMD_T_NOGUEST)
      strccat(buff, &bp, "Noguest");
    if (command->type & CMD_T_EQSPLIT)
      strccat(buff, &bp, "Eqsplit");
    if (command->type & CMD_T_GOD)
      strccat(buff, &bp, "God");
    if (command->type & CMD_T_LOGARGS)
      strccat(buff, &bp, "LogArgs");
    else if (command->type & CMD_T_LOGNAME)
      strccat(buff, &bp, "LogName");
    *bp = '\0';
    notify_format(player, "Restrict     : %s", buff);
    buff[0] = '\0';
    notify_format(player, "Command Lock : %s",  unparse_boolexp(player, command->lock, UB_MEREF));
    bp = buff;
    for (sw_val = switch_list; sw_val->name; sw_val++)
      if (SW_ISSET(command->sw, sw_val->value))
        strccat(buff, &bp, sw_val->name);
    *bp = '\0';
    notify_format(player, "Switches     : %s", buff);
    buff[0] = '\0';
    bp = buff;
    if (command->type & CMD_T_LS_ARGS) {
      if (command->type & CMD_T_LS_SPACE)
        strccat(buff, &bp, "Space-Args");
      else
        strccat(buff, &bp, "Args");
    }
    if (command->type & CMD_T_LS_NOPARSE)
      strccat(buff, &bp, "Noparse");
    if (command->type & CMD_T_EQSPLIT) {
      *bp = '\0';
      notify_format(player, "Leftside     : %s", buff);
      buff[0] = '\0';
      bp = buff;
      if (command->type & CMD_T_RS_ARGS) {
        if (command->type & CMD_T_RS_SPACE)
          strccat(buff, &bp, "Space-Args");
        else
          strccat(buff, &bp, "Args");
      }
      if (command->type & CMD_T_RS_NOPARSE)
        strccat(buff, &bp, "Noparse");
      *bp = '\0';
      notify_format(player, "Rightside    : %s", buff);
    } else {
      *bp = '\0';
      notify_format(player, "Arguments    : %s", buff);
    }
    do_hook_list(player, arg_left);
  }
}

/** Display a list of defined commands.
 * This function sends a player the list of commands.
 * \param player the enactor.
 * \param lc if true, list is in lowercase rather than uppercase.
 */
void
do_list_commands(dbref player, int lc)
{
  char *b = list_commands();
  notify_format(player, "Commands: %s", lc ? strlower(b) : b);
}

/** Return a list of defined commands.
 * This function returns a space-separated list of commands as a string.
 */
char *
list_commands(void)
{
  COMMAND_INFO *command;
  const char *ptrs[BUFFER_LEN / 2];
  static char buff[BUFFER_LEN];
  char *bp;
  int nptrs = 0, i;
  command = (COMMAND_INFO *) ptab_firstentry(&ptab_command);
  while (command) {
    ptrs[nptrs] = command->name;
    nptrs++;
    command = (COMMAND_INFO *) ptab_nextentry(&ptab_command);
  }
  bp = buff;
  safe_str(ptrs[0], buff, &bp);
  for (i = 1; i < nptrs; i++) {
    safe_chr(' ', buff, &bp);
    safe_str(ptrs[i], buff, &bp);
  }
  *bp = '\0';
  return buff;
}


/* Check command permissions. Return 1 if player can use command,
 * 0 otherwise, and maybe be noisy about it.
 */
static int
command_check(dbref player, COMMAND_INFO *cmd, switch_mask switches)
{
  int ok;
  char *mess = NULL;

  /* God doesn't get fucked with */
  if(LEVEL(player) >= LEVEL_GOD)
          return 1;
  /* If disabled, return silently */
  if (cmd->type & CMD_T_DISABLED)
    return 0;
  if ((cmd->type & CMD_T_NOGAGGED) && Gagged(player)) {
    mess = T("You cannot do that while gagged.");
    goto send_error;
  }
  if ((cmd->type & CMD_T_NOFIXED) && Fixed(player)) {
    mess = T("You cannot do that while fixed.");
    goto send_error;
  }
#ifdef RPMODE_SYS
  if((cmd->type & CMD_T_NORPMODE) && RPMODE(player)) {
          mess = T("You cannot do that while in RPMODE");
          goto send_error;
  }
#endif
  if ((cmd->type & CMD_T_NOGUEST) && Guest(player)) {
    mess =  T("Guests cannot do that.");
    goto send_error;
  }
  if ((cmd->type & CMD_T_GOD) && (!God(player))) {
    mess =  T("Only God can do that.");
    goto send_error;
  }
  switch (Typeof(player)) {
  case TYPE_ROOM:
    ok = (cmd->type & CMD_T_ROOM);
    break;
  case TYPE_THING:
    ok = (cmd->type & CMD_T_THING);
    break;
  case TYPE_EXIT:
    ok = (cmd->type & CMD_T_EXIT);
    break;
  case TYPE_PLAYER:
    ok = (cmd->type & CMD_T_PLAYER);
    break;
  case TYPE_DIVISION:
    ok = (cmd->type & CMD_T_DIVISION);
    break;
  default:
    ok = 0;
  }
  if (!ok) {
    mess = T("Permission denied, command is type-restricted.");
    goto send_error;
  }
  /* A command can specify required flags or powers, and if
   * any match, the player is ok to do the command.
   */
  ok = 1;

  if(!God(player) && !eval_boolexp(player, cmd->lock, player, switches) ) {
         mess =  T("Permission denied.");
         goto send_error;
  }

  return ok;
send_error:
  if(cmd->restrict_message)
    notify(player, cmd->restrict_message);
  else if(mess)
    notify(player, mess);
  return 0;
}

/** Determine whether a player can use a command.
 * This function checks whether a player can use a command.
 * If the command is disallowed, the player is informed.
 * \param player player whose privileges are checked.
 * \param name name of command.
 * \retval 0 player may not use command.
 * \retval 1 player may use command.
 */
int
command_check_byname(dbref player, const char *name)
{
  COMMAND_INFO *cmd;
  cmd = command_find(name);
  if (!cmd)
    return 0;
  return command_check(player, cmd, NULL);
}

static int
has_hook(struct hook_data *hook)
{
  if (!hook || !GoodObject(hook->obj) || IsGarbage(hook->obj)
      || !hook->attrname)
    return 0;
  return 1;
}


/** Run a command hook.
 * This function runs a hook before or after a command execution.
 * \param player the enactor.
 * \param cause dbref that caused command to execute.
 * \param hook pointer to the hook.
 * \param saveregs array to store a copy of the final q-registers.
 * \param save if true, use saveregs to store a ending q-registers.
 * \retval 1 Hook doesn't exist, or evaluates to a non-false value
 * \retval 0 Hook exists and evaluates to a false value
 */
int
run_hook(dbref player, dbref cause, struct hook_data *hook, char *saveregs[],
         int save)
{
  ATTR *atr;
  char *code;
  const char *cp;
  char buff[BUFFER_LEN], *bp;
  char *origregs[NUMQ];

  if (!has_hook(hook))
    return 1;

  atr = atr_get(hook->obj, hook->attrname);

  if (!atr)
    return 1;

  code = safe_atr_value(atr);
  if (!code)
    return 1;
  add_check("hook.code");

  save_global_regs("run_hook", origregs);
  restore_global_regs("hook.regs", saveregs);

  cp = code;
  bp = buff;

  process_expression(buff, &bp, &cp, hook->obj, cause, player, PE_DEFAULT,
                     PT_DEFAULT, NULL);
  *bp = '\0';

  if (save)
    save_global_regs("hook.regs", saveregs);
  restore_global_regs("run_hook", origregs);

  mush_free(code, "hook.code");
  return parse_boolean(buff);
}

/** Set up or remove a command hook.
 * \verbatim
 * This is the top-level function for @hook. If an object and attribute
 * are given, establishes a hook; if neither are given, removes a hook.
 * \endverbatim
 * \param player the enactor.
 * \param command command to hook.
 * \param obj name of object containing the hook attribute.
 * \param attrname of hook attribute on obj.
 * \param flag type of hook
 */
void
do_hook(dbref player, char *command, char *obj, char *attrname,
        enum hook_type flag)
{
  COMMAND_INFO *cmd;
  struct hook_data *h;

  cmd = command_find(command);
  if (!cmd) {
    notify(player, T("No such command."));
    return;
  }
  if ((cmd->func == cmd_password) || (cmd->func == cmd_newpassword)) {
    notify(player, T("Hooks not allowed with that command."));
    return;
  }

  if (flag == HOOK_BEFORE)
    h = &cmd->hooks.before;
  else if (flag == HOOK_AFTER)
    h = &cmd->hooks.after;
  else if (flag == HOOK_IGNORE)
    h = &cmd->hooks.ignore;
  else if (flag == HOOK_OVERRIDE)
    h = &cmd->hooks.override;
  else {
    notify(player, T("Unknown hook type"));
    return;
  }

  if (!obj && !attrname) {
    notify_format(player, T("Hook removed from %s."), cmd->name);
    h->obj = NOTHING;
    mush_free(h->attrname, "hook.attr");
    h->attrname = NULL;
  } else if (!obj || !*obj || !attrname || !*attrname) {
    notify(player, T("You must give both an object and attribute."));
  } else {
    dbref objdb = match_thing(player, obj);
    if (!GoodObject(objdb)) {
      notify(player, T("Invalid hook object."));
      return;
    }
    h->obj = objdb;
    if (h->attrname)
      mush_free(h->attrname, "hook.attr");
    h->attrname = mush_strdup(strupper(attrname), "hook.attr");
    notify_format(player, T("Hook set for %s"), cmd->name);
  }
}


/** List command hooks.
 * \verbatim
 * This is the top-level function for @hook/list, @list/hooks, and
 * the hook-listing part of @command.
 * \endverbatim
 * \param player the enactor.
 * \param command command to list hooks on.
 */
void
do_hook_list(dbref player, char *command)
{
  COMMAND_INFO *cmd;

  cmd = command_find(command);
  if (!cmd) {
    notify(player, T("No such command."));
    return;
  }
  if (Site(player)) {
    if (GoodObject(cmd->hooks.before.obj))
      notify_format(player, "@hook/before: #%d/%s",
                    cmd->hooks.before.obj, cmd->hooks.before.attrname);
    if (GoodObject(cmd->hooks.after.obj))
      notify_format(player, "@hook/after: #%d/%s", cmd->hooks.after.obj,
                    cmd->hooks.after.attrname);
    if (GoodObject(cmd->hooks.ignore.obj))
      notify_format(player, "@hook/ignore: #%d/%s",
                    cmd->hooks.ignore.obj, cmd->hooks.ignore.attrname);
    if (GoodObject(cmd->hooks.override.obj))
      notify_format(player, "@hook/override: #%d/%s",
                    cmd->hooks.override.obj, cmd->hooks.override.attrname);
  }
}
