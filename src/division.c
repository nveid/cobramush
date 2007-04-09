/* division.c - RLB 3/11/02
 *		Part of the Division system for PennMUSH
 * 	   4/12/02 - RLB - Rewrite level set, much more readable now =)
 * 	   4/17/02 - RLB - Code StreamLining
 *	   1/18/04 - RLB - began work on power conversion code
 *	   1/23/04 - RLB - completed power conversion code
 *	   3/26/04 - RLB - Added translation wrappers to messages.
 *	   7/27/05 - RLB - DB Powers Completed
 */

/* Required Includes {{{1 */
#include "copyrite.h"
#include "config.h"
#ifdef I_STRING
#include <string.h>
#else
#include <strings.h>
#endif
#include <ctype.h>
#include "conf.h"
#include "externs.h"
#include "boolexp.h"
#include "command.h"
#include "division.h"
#include "parse.h"
#include "htab.h"
#include "cmds.h"
#include "confmagic.h"
#include "flags.h"
#include "dbdefs.h"
#include "dbio.h"
#include "match.h"
#include "log.h"
#include "boolexp.h"
#include "attrib.h"
#include "ansi.h"
/* }}}1 */

/* Static Function Prototypes {{{1 */
static void adjust_levels(dbref, int);
static void adjust_divisions(dbref, dbref, dbref);
static void power_add(POWER * pow);
static POWER *new_power(void);
static void realloc_object_powers(int);
static void power_add_additional();
static void power_read(FILE *);
static void powergroup_read(FILE *);
static void power_read_alias(FILE *);
static void clear_all_powers();
static int power_delete(const char *);
static int check_old_power_ycode(int, unsigned char *);
static int new_power_bit(int);
static int power_goodname(char *);

/* External Accessible Function Prototypes {{{1 */
void division_set(dbref, dbref, const char *);
void division_empower(dbref, dbref, const char *);
const char *division_list_powerz(dbref, int);
const char *list_dp_powerz(div_pbits, int);
dbref create_div(dbref, const char *);
int division_level(dbref, dbref, int);
int div_inscope(dbref, dbref);
int div_powover(dbref, dbref, const char *);
void clear_division(dbref);
int yescode_i(char *);
char *yescode_str(int);
int can_give_power(dbref, dbref, POWER *, int);
int can_have_power(dbref, POWER *, int);
int can_have_power_at(dbref, POWER *);
POWER *add_power_type(const char *name, const char *type);
void init_powers();
div_pbits new_power_bitmask();
void powers_read_all(FILE * in);
POWER *has_power(dbref, const char *);
int can_have_pg(dbref object, POWERGROUP * pgrp);
void do_list_powers(dbref player, const char *name);
char *list_all_powers(dbref player, const char *name);
void add_to_div_exit_path(dbref player, dbref div_obj);

/* PowerGroup Related Functions {{{2
 */
char *powergroups_list(dbref, char flag);
char *powergroups_list_on(dbref, char);
char powergroup_delete(dbref, const char *);
POWERGROUP *powergroup_add(const char *);
int powergroup_empower(dbref, POWERGROUP *, const char *, char);
void powergroup_db_set(dbref, dbref, const char *, char);
void rem_pg_from_player(dbref player, POWERGROUP * powergroup);
void add_pg_to_player(dbref player, POWERGROUP * powergroup);
int powergroup_has(dbref, POWERGROUP *);
int can_receive_power_at(dbref player, POWER * power);

/* Pre-Defined Tables {{{1
 */
/* Pre-Defined Power Aliases {{{2 */
static struct power_alias power_alias_tab[] = {
  {"Functions", "GFuncs"},
  {"@Cemit", "Cemit"},
  {"CQuota", "SetQuotas"},
  {"chat_privs", "Chat"},
  {"link_anywhere", "Link"},
  {"long_fingers", "Remote"},
  {"Pemit_All", "Pemit"},
  {"Open_Anywhere", "Open"},
  {"Tport_Anything", "Tel_Thing"},
  {"Tel_Where", "Tel_Place"},
  {"Tport_Anywhere", "Tel_Place"},
  {"@wall", "Announce"},
  {"wall", "Announce"},
  {NULL, NULL}
};

/* Power Types {{{2 */

static struct division_power_entry_t powc_list[] = {
  {"self", &powc_self, YES, YES, YES},
  {"levchk", &powc_levchk, YES, YESLTE, YESLT},
  {"levchk_lte", &powc_levchk_lte, YES, YES, YES},
  {"levset", &powc_levset, YES, YESLTE, YESLT},
  {"bcreate", &powc_bcreate, YES, YES, YES},
  {NULL, NULL, -1, -1, -1},
};

/* Pre-Defined Power-Groups {{{2 */

static struct powergroup_text_t predefined_powergroups[] = {

  /* PowerGroup Name */
  {"Wizard",
   /* Max Powers */
   "Announce Attach BCreate Boot Builder Can_NsPemit Cemit Chat Chown Combat Cron DAnnounce Division EAnnounce Economy Empire Empower GFuncs Halt Hide Idle Join Level Link Login MailAdmin Many_Attribs Modify:LTE Newpass:LTE NoPay NoQuota Nuke:LTE Open Pass_Locks PCreate PEmit Poll Privilege PrivWho ProgLock Program Powergroup:lt Pueblo_Send Queue Quota RCACHE Remote RPChat RPEmit RPTel Search See_All See_Queue SetQuotas Site SQL_Ok Summon Tel_Place Tel_Thing @SU:LTE",
   /* Auto Powers */
   "Announce Attach BCreate Boot Builder Can_NsPemit Cemit Chat Chown Combat Cron DAnnounce Division EAnnounce Empire Empower GFuncs Halt Hide Idle Join Level Link Login MailAdmin Many_Attribs Modify:LTE Newpass:LTE NoPay NoQuota Nuke:LTE Open PCreate PEmit Poll Powergroup:lt Privilege PrivWho ProgLock Program Pueblo_Send Queue Quota RCACHE Remote RPChat RPEmit RPTel Search See_All See_Queue SetQuotas Site SQL_Ok Summon Tel_Place Tel_Thing @SU:LTE"},

  /* PowerGroup Name */
  {"Royalty",
   /* Max Powers */
   "See_All NoPay NoQuota Queue See_Queue Remote Chat Builder Announce Boot PrivWho Login PEmit Hide Privilege Tel_Place Tel_Thing",
   /* Auto Powers */
   "See_All NoPay NoQuota Queue See_Queue Remote Chat Builder Announce Boot PrivWho Login PEmit Hide Privilege Tel_Place Tel_Thing"},

  /* PowerGroup Name */
  {"Director",
   /* Max Powers */
   "@SU:LTE Announce Attach BCreate Boot Builder Can_NsPemit Cemit Chat Chown Combat Cron DAnnounce Division EAnnounce Empire Empower GFuncs Halt Hide Idle Join Level Link Login MailAdmin Many_Attribs Modify Newpass:2 NoPay NoQuota Nuke:LTE Open:2 Pass_Locks PCreate PEmit Poll PowerGroup:LTE Privilege PrivWho Program ProgLock:lte Pueblo_Send Quota Queue RCACHE Remote RPChat RPEmit RPTel Search See_All See_Queue SetQuotas  Site SQL_Ok Summon Tel_Thing Tel_Place",
   /* Auto Powers */
   "Announce Attach BCreate Boot:2 Builder Can_NsPemit Cemit Chat Chown:2 Combat Cron DAnnounce Division EAnnounce Economy Empire Empower:2 GFuncs Halt Hide Idle Join Level:1 Link Login MailAdmin:1 Many_Attribs Modify:2 Newpass:1 NoPay NoQuota Nuke:LT Open:2 Pass_Locks PCreate PEmit Poll PowerGroup:LTE Privilege PrivWho Program ProgLock:1 Pueblo_Send Quota Queue RCACHE Remote RPChat RPEmit RPTel Search:LTE See_All Search:lte See_Queue SetQuotas Site SQL_Ok Summon Tel_Thing Tel_Place"},

  /* PowerGroup Name */
  {"Admin",
   /* Max Powers */
   "Announce Attach BCreate Boot:1 Builder Can_NsPemit Cemit Chat Chown(LTE) Combat Cron DAnnounce Division EAnnounce Economy Empire Empower GFuncs Halt Hide Idle Join Level Link Login MailAdmin:2 Many_Attribs Modify Newpass:2 NoPay Nuke:LTE NoQuota Open:2 Pass_Locks:2 PCreate PEmit Poll Powergroup:lte Privilege PrivWho Program ProgLock:2 Pueblo_Send Quota Queue RCACHE Remote RPChat RPEmit RPTel Search:LTE See_All See_Queue SetQuotas SQL_Ok Summon Tel_Thing Tel_Place",
   /* Auto Powers */
   "Announce Attach:1 BCreate Builder Can_NsPemit Cemit Chat Combat Cron DAnnounce Division EAnnounce Economy Empire Empower:2 GFuncs Halt:2 Hide Idle Join Level:1 Link:2 Login Modify:2 Newpass:1 NoPay Nuke:LT NoQuota Open:2 PCreate PEmit Poll PowerGroup:LTE Program Pueblo_Send Quota Queue RCACHE Remote RPChat RPEmit RPTel Search:2 See_All:2 See_Queue Summon:1 Tel_Thing:1 Tel_Place:2 "},

  /* PowerGroup Name */
  {"EmpireHead",
   /* Max Powers */
   "Announce Attach:1 BCreate Boot:1 Builder Can_NsPemit Cemit Chat Combat Cron DAnnounce Division EAnnounce Economy Empower GFuncs Halt Hide Idle Join Level Link Login Many_Attribs Modify:2 Newpass:1 NoPay NoQuota Nuke:LT Open:2 Pass_Locks:2 PCreate PEmit Poll PowerGroup:LT Privilege PrivWho Program:2 Pueblo_Send Quota Queue RCACHE Remote RPChat RPEmit RPTel Search:2 See_All:2 See_Queue SetQuotas:LTE SQL_Ok Summon Tel_Thing:2 Tel_Place:1",
   /* Auto Powers */
   "Announce Attach:1 BCreate Boot:1 Builder Cemit Chat DAnnounce Division EAnnounce Empower:1 Halt:1 Hide Idle Join:2 Level:1 Link:1 Login Modify:2 Newpass:1 NoPay NoQuota Open:2 Pass_Locks PCreate PEmit:1 Poll PowerGroup:LT PrivWho Program:2 Pueblo_Send Quota Queue Remote RPChat RPEmit Search:1 See_Queue SetQuotas:1 Summon:1 Tel_Thing:1 Tel_Place:2 "},

  /* PowerGroup Name */
  {"EmpireAdmin",
   /* Max Powers */
   "",
   /* Auto Powers */
   ""},
  /* PowerGroup Name */
  {"Builder",
   /* Max Powers */
   "Builder Cemit Chat Combat DAnnounce Division EAnnounce Economy Empower GFuncs Halt Hide Idle Join Level Link Login Many_Attribs Modify:2 NoPay NoQuota Open:2 PCreate PEmit Poll PrivWho Program:2 Pueblo_Send Quota Queue RCACHE Remote RPChat RPEmit RPTel Search:2 See_All:2 See_Queue Space Summon Tel_Thing:2 Tel_Place:2",
   /* Auto Powers */
   "Builder Hide Idle Join:2 Level:LT Login NoPay NoQuota Open:LTE PEmit:LT Poll Program:LTE Pueblo_Send Quota Queue RPChat RPEmit See_Queue Tel_Thing:LT Tel_Place:2"},

  /* PowerGroup Name */
  {"Player",
   /* Max Powers */
   "Builder Chat Combat Hide Idle Login NoPay PEmit Poll RPChat RPEmit Summon Join",
   /* Auto Powers */
   "RPChat RPEmit"},
  /* end of powers */
  {NULL, NULL, NULL}
};

/* Pre-Defined Powers List {{{2 */
static struct new_division_power_entry_t new_power_list[] = {
  {"Announce", "self"},
  {"Attach", "levchk"},
  {"BCreate", "bcreate"},
  {"Boot", "levchk"},
  {"Builder", "self"},
  {"Can_NsPemit", "levchk"},
  {"Cemit", "self"},
  {"Chat", "self"},
  {"Chown", "levchk"},
  {"Combat", "levchk"},
  {"Cron", "self"},
  {"DAnnounce", "self"},
  {"Division", "self"},
  {"EAnnounce", "self"},
  {"Empire", "self"},
  {"Empower", "levchk_lte"},
  {"GFuncs", "self"},
  {"Halt", "levchk"},
  {"Hide", "self"},
  {"Idle", "self"},
  {"Join", "levchk"},
  {"Level", "levset"},
  {"Link", "levchk"},
  {"Login", "self"},
  {"MailAdmin", "levchk"},
  {"Many_Attribs", "self"},
  {"Modify", "levchk"},
  {"Newpass", "levchk"},
  {"NoPay", "self"},
  {"NoQuota", "self"},
  {"Nuke", "levchk"},
  {"Open", "levchk"},
  {"Pass_Locks", "levchk"},
  {"PCreate", "self"},
  {"PEmit", "levchk"},
  {"Poll", "self"},
  {"Powergroup", "levchk"},
  {"Privilege", "self"},
  {"PrivWho", "self"},
  {"Program", "levchk"},
  {"ProgLock", "levchk"},
  {"Pueblo_Send", "self"},
  {"Quota", "self"},
  {"Queue", "self"},
  {"RCACHE", "self"},
  {"Remote", "levchk"},
  {"RPChat", "self"},
  {"RPEmit", "self"},
  {"RPTel", "self"},
  {"Search", "levchk"},
  {"See_All", "levchk"},
  {"See_Queue", "levchk"},
  {"SetQuotas", "levchk"},
  {"Site", "self"},
  {"SQL_Ok", "self"},
  {"Summon", "levchk"},
  {"@SU", "levchk"},
  {"Tel_Thing", "levchk"},
  {"Tel_Place", "levchk"},
  {NULL, NULL}
};

/* Old CobraMUSH Powers Conversion Table {{{2 */
static struct old_division_power_entry_t old_cobra_conv_t[] = {
  /* Power, Yes, YesLTE, YesLT */
  {"Announce", POW_ANNOUNCE, POW_ANNOUNCE, POW_ANNOUNCE},
  {"Attach", POW_ATTACH, POW_ATTACH, POW_ATTACH_LT},
  {"BCreate", POW_BCREATE, POW_BCREATE, POW_BCREATE},
  {"Boot", POW_BOOT, POW_BOOT_LTE, POW_BOOT_LT},
  {"Builder", POW_BUILD, POW_BUILD, POW_BUILD},
  {"Can_NsPemit", POW_NSPEMIT, POW_NSPEMIT_LTE, POW_NSPEMIT_LT},
  {"Cemit", POW_CEMIT, POW_CEMIT, POW_CEMIT},
  {"Chat", POW_CPRIV, POW_CPRIV, POW_CPRIV},
  {"Chown", POW_CHOWN, POW_CHOWN_LTE, POW_CHOWN_LT},
  {"Combat", POW_COMBAT, POW_COMBAT_LTE, POW_COMBAT_LT},
  {"CQuota", POW_CQUOTAS, POW_CQUOTAS_LTE, POW_CQUOTAS_LT},
  {"DAnnounce", POW_DANNOUNCE, POW_DANNOUNCE, POW_DANNOUNCE},
  {"Division", POW_DIVISION, POW_DIVISION, POW_DIVISION},
  {"EAnnounce", POW_EANNOUNCE, POW_EANNOUNCE, POW_EANNOUNCE},
  {"Empire", POW_EMPIRE, POW_EMPIRE, POW_EMPIRE},
  {"Empower", POW_EMPOWER, POW_EMPOWER, POW_EMPOWER},
  {"GFuncs", POW_GFUNCS, POW_GFUNCS, POW_GFUNCS},
  {"Halt", POW_HALT, POW_HALT_LTE, POW_HALT_LT},
  {"Hide", POW_HIDE, POW_HIDE, POW_HIDE},
  {"Idle", POW_IDLE, POW_IDLE, POW_IDLE},
  {"Join", POW_JOIN, POW_JOIN, POW_JOIN_LT},
  {"Level", POW_RERANK, POW_RERANK, POW_RERANK_LT},
  {"Link", POW_LANY, POW_LANY_LTE, POW_LANY_LT},
  {"Login", POW_LOGIN, POW_LOGIN, POW_LOGIN},
  {"MailAdmin", POW_MAIL, POW_MAIL_LTE, POW_MAIL_LT},
  {"Many_Attribs", POW_MANY_ATTRIBS, POW_MANY_ATTRIBS, POW_MANY_ATTRIBS},
  {"Modify", POW_MODIFY, POW_MODIFY_LTE, POW_MODIFY_LT},
  {"Newpass", POW_NEWPASS, POW_NEWPASS_LTE, POW_NEWPASS_LT},
  {"NoPay", POW_NOPAY, POW_NOPAY, POW_NOPAY},
  {"NoQuota", POW_NOQUOTA, POW_NOQUOTA, POW_NOQUOTA},
  {"Open", POW_OANY, POW_OANY_LTE, POW_OANY_LT},
  {"Pass_Locks", POW_PASS_LOCKS, POW_PASS_LOCKS_LTE, POW_PASS_LOCKS_LT},
  {"PCreate", POW_PCREATE, POW_PCREATE, POW_PCREATE},
  {"PEmit", POW_PEMIT, POW_PEMIT_LTE, POW_PEMIT_LT},
  {"Poll", POW_POLL, POW_POLL, POW_POLL},
  {"PowerLevel", POW_POWLSET, POW_POWLSET_LTE, POW_POWLSET_LT},
  {"Privilege", POW_PRIVILEGE, POW_PRIVILEGE, POW_PRIVILEGE},
  {"PrivWho", POW_PWHO, POW_PWHO, POW_PWHO},
  {"Program", POW_PROG, POW_PROG_LTE, POW_PROG_LT},
  {"ProgLock", POW_PROGL, POW_PROGL_LTE, POW_PROGL_LT},
  {"Pueblo_Send", POW_PUEBLO_SEND, POW_PUEBLO_SEND, POW_PUEBLO_SEND},
  {"Quota", POW_VQUOTAS, POW_VQUOTAS_LTE, POW_VQUOTAS_LT},
  {"Queue", POW_QUEUE, POW_QUEUE, POW_QUEUE},
  {"RCACHE", POW_RCACHE, POW_RCACHE, POW_RCACHE},
  {"Remote", POW_LFINGERS, POW_LFINGERS_LTE, POW_LFINGERS_LT},
  {"RPChat", POW_RPCHAT, POW_RPCHAT, POW_RPCHAT},
  {"RPEmit", POW_RPEMIT, POW_RPEMIT, POW_RPEMIT},
  {"RPTel", POW_RPTEL, POW_RPTEL, POW_RPTEL},
  {"Search", POW_SEARCH, POW_SEARCH_LTE, POW_SEARCH_LT},
  {"See_All", POW_SEE_ALL, POW_SEE_ALL_LTE, POW_SEE_ALL_LT},
  {"See_Queue", POW_PS, POW_PS_LTE, POW_PS_LT},
  {"Site", POW_MANAGEMENT, POW_MANAGEMENT, POW_MANAGEMENT},
  {"SQL_Ok", POW_SQL, POW_SQL, POW_SQL},
  {"Summon", POW_SUMMON, POW_SUMMON, POW_SUMMON_LT},
  {"Tel_Thing", POW_TPORT_T, POW_TPORT_T_LTE, POW_TPORT_T_LT},
  {"Tel_Place", POW_TPORT_O, POW_TPORT_O_LTE, POW_TPORT_O_LT},
  {NULL, -1, -1, -1}
};

/* PennMUSH Power Conversion Table  {{{2 */

static struct convold_ptab pconv_ptab[] = {
  {"Announce", CAN_WALL},
  {"Boot", CAN_BOOT},
  {"Builder", CAN_BUILD},
  {"Cemit", CEMIT},
  {"Chat", CHAT_PRIVS},
  {"GFuncs", GLOBAL_FUNCS},
  {"Halt", HALT_ANYTHING},
  {"Hide", CAN_HIDE},
  {"Idle", UNLIMITED_IDLE},
  {"Link", LINK_ANYWHERE},
  {"Login", LOGIN_ANYTIME},
  {"Remote", LONG_FINGERS},
  {"NoPay", NO_PAY},
  {"NoQuota", NO_QUOTA},
  {"Open", OPEN_ANYWHERE},
  {"PEmit", PEMIT_ALL},
  {"PCreate", CREATE_PLAYER},
  {"Poll", SET_POLL},
  {"Queue", HUGE_QUEUE},
  {"See_All", SEE_ALL},
  {"See_Queue", PS_ALL},
  {"Tel_Thing", TEL_OTHER},
  {"Tel_Where", TEL_ANYWHERE},
  {NULL, 0}
};

/* 1}}} */

/* File Variable Declarations {{{1
 */
extern struct db_stat_info current_state;
extern long flagdb_flags;
POWERSPACE ps_tab = { NULL, NULL, 0, 0, NULL };
PTAB power_ptab;
PTAB powergroup_ptab;

/* }}}1 */

/* Division and Power System Functions {{{1 */
/* Database Functions {{{2 */
/* powers_read_all() {{{3 */
void
powers_read_all(FILE * in)
{
  int count;

  clear_all_powers();
  /* Allocate initially big enough for 1 */
  ps_tab.bits_taken = mush_malloc(1, "BITS_TAKEN");
  if (!ps_tab.bits_taken)
    mush_panic("Error! Bits Taken couldn't allocate!\n");
  ps_tab.powers = &power_ptab;
  ps_tab.powergroups = &powergroup_ptab;
  ps_tab.powerbits = 0;
  ptab_init(ps_tab.powers);
  ptab_init(ps_tab.powergroups);


  db_read_this_labeled_number(in, "powercount", &count);

  for (; count > 0; count--)
    power_read(in);
  db_read_this_labeled_number(in, "aliascount", &count);
  for (; count > 0; count--)
    power_read_alias(in);

  db_read_this_labeled_number(in, "powergroupcount", &count);
  for (; count > 0; count--)
    powergroup_read(in);

  ps_tab._Read_Powers_ = 1;
  power_add_additional();
}

/* init_powers() {{{3 */
void
init_powers()
{
  do_rawlog(LT_ERR,
            "ERROR: Powers failed to read from database.  Loading default power list!");
  clear_all_powers();
  ps_tab.powers = &power_ptab;
  ps_tab.powergroups = &powergroup_ptab;
  ps_tab.powerbits = 0;
  /* Allocate this initially big enough for 1 */
  ps_tab.bits_taken = mush_malloc(1, "BITS_TAKEN");
  if (!ps_tab.bits_taken)
    mush_panic("Error! Bits Taken couldn't allocate!\n");
  ptab_init(ps_tab.powers);
  ptab_init(ps_tab.powergroups);
  ps_tab._Read_Powers_ = 1;
  power_add_additional();
}

/* powers_read() {{{3 - database power read routine */
static void
power_read(FILE * in)
{
  char power_name[BUFFER_LEN];
  char *power_type;

  db_read_this_labeled_string(in, "powername", &power_type);

  if(!(flagdb_flags & FLAG_DBF_CQUOTA_RENAME) && !strcmp(power_type, "CQuota"))
    strcpy(power_name, "SetQuotas");
  else
    strcpy(power_name, power_type);

  db_read_this_labeled_string(in, "type", &power_type);

  add_power_type(power_name, power_type);
}

/* powergroup_read() {{{3 - database powergroup read routine */
static void
powergroup_read(FILE * in)
{
  POWERGROUP *pgrp;
  div_pbits dbits;
  char *value;

  db_read_this_labeled_string(in, "powergroupname", &value);
  pgrp = powergroup_add(value);
  db_read_this_labeled_string(in, "maxpowers", &value);
  dbits = string_to_dpbits(value);

  if (power_is_zero(dbits, DP_BYTES) == 0) {
    mush_free(dbits, "POWER_SPOT");
    pgrp->max_powers = NULL;
  } else
    pgrp->max_powers = dbits;

  db_read_this_labeled_string(in, "autopowers", &value);
  dbits = string_to_dpbits(value);

  if (power_is_zero(dbits, DP_BYTES) == 0) {
    mush_free(dbits, "POWER_SPOT");
    pgrp->auto_powers = NULL;
  } else
    pgrp->auto_powers = dbits;
}

/* power_read_alias() {{{3 */
static void
power_read_alias(FILE * in)
{
  POWER *power;
  char alias_name[BUFFER_LEN];
  char *power_pointer;

  db_read_this_labeled_string(in, "aliasname", &power_pointer);
  strcpy(alias_name, power_pointer);
  db_read_this_labeled_string(in, "power", &power_pointer);

  /* Make sure the alias doesn't exist */
  if ((power = find_power(alias_name)))
    return;

  /* Make sure the power exists */
  if (!(power = find_power(power_pointer)))
    return;

  ptab_start_inserts(ps_tab.powers);
  ptab_insert(ps_tab.powers, alias_name, power);
  ptab_end_inserts(ps_tab.powers);
}

/* power_write_all() {{{3 */
void
power_write_all(FILE * file)
{
  POWER *power;
  POWERGROUP *pgrp;
  char pname[BUFFER_LEN];
  int i, pc_l;
  int num_powers = 0;

  for (power = ptab_firstentry_new(ps_tab.powers, pname); power;
       power = ptab_nextentry_new(ps_tab.powers, pname))
    if (!strcmp(pname, power->name))
      num_powers++;

  db_write_labeled_number(file, "powercount", num_powers);

  for (power = ptab_firstentry_new(ps_tab.powers, pname); power;
       power = ptab_nextentry_new(ps_tab.powers, pname))
    if (!strcmp(pname, power->name)) {
      db_write_labeled_string(file, "  powername", power->name);
      for (pc_l = 0; powc_list[pc_l].name; pc_l++)
        if (powc_list[pc_l].powc_chk == power->powc_chk)
          break;
      /* We will assume this /does/ match. */
      db_write_labeled_string(file, "       type", powc_list[pc_l].name);
    }
  for (i = 0, power = ptab_firstentry_new(ps_tab.powers, pname); power;
       power = ptab_nextentry_new(ps_tab.powers, pname))
    if (strcmp(pname, power->name))
      i++;

  db_write_labeled_number(file, "aliascount", i);
  for (power = ptab_firstentry_new(ps_tab.powers, pname); power;
       power = ptab_nextentry_new(ps_tab.powers, pname))
    if (strcmp(pname, power->name)) {
      db_write_labeled_string(file, "  aliasname", pname);
      db_write_labeled_string(file, "    power", power->name);
    }
  for (num_powers = 0, pgrp =
       ptab_firstentry_new(ps_tab.powergroups, pname); pgrp;
       pgrp = ptab_nextentry_new(ps_tab.powergroups, pname))
    num_powers++;
  db_write_labeled_number(file, "powergroupcount", num_powers);
  for (pgrp = ptab_firstentry_new(ps_tab.powergroups, pname); pgrp;
       pgrp = ptab_nextentry_new(ps_tab.powergroups, pname)) {
    db_write_labeled_string(file, "  powergroupname", pgrp->name);
    db_write_labeled_string(file, "    maxpowers",
                            list_dp_powerz(pgrp->max_powers, 0));
    db_write_labeled_string(file, "    autopowers",
                            list_dp_powerz(pgrp->auto_powers, 0));
  }

}

/* End of Database Functions }}}2 */
/* PowerGroup Functions {{{2  */
/* powergroup_db_set() {{{3 - Set or Reset powergroup on Player */
/* Supplying executor as NOTHING, will assume the game is doing this internally and bypass
 * security checks
 */
void
powergroup_db_set(dbref executor, dbref player, const char *powergroup,
                  char autoset)
{
  int pg_cnt, pg_indx;
  char tbuf[BUFFER_LEN], *p_buf[BUFFER_LEN / 2];
  char pname[BUFFER_LEN];
  char *p;
  POWERGROUP *pgrp;
  POWER *power;
  int ycode, reset = 0;
  int po_c;

  if (executor != NOTHING && Owner(executor) != Owner(player)
      && !(po_c = div_powover(executor, player, "POWERGROUP"))) {
    notify(executor, "Permission denied.");
    return;
  }


  if (!powergroup || !*powergroup) {
    notify(executor, "Must provide powergroup arguments.");
    return;
  }

  strcpy(tbuf, powergroup);

  pg_cnt = list2arr(p_buf, BUFFER_LEN / 2, tbuf, ' ');
  if (!pg_cnt) {
    notify(executor, "Must supply powers.");
    return;
  }
  for (pg_indx = 0; pg_indx < pg_cnt; reset = 0, pg_indx++) {
    p = p_buf[pg_indx];
    if (*p == NOT_TOKEN) {
      reset++;
      p++;
    }

    if (!(pgrp = powergroup_find(p))) {
      notify_format(executor, "No such powergroup to %s.",
                    reset ? "remove" : "add");
      continue;
    }

    if (GoodObject(executor) && !God(executor)
        && !can_have_pg(player, pgrp)) {
      notify_format(executor, "%s can not receive the '%s' powergroup.",
                    unparse_object(executor, player), pgrp->name);
      continue;
    }

    /* See if 'executor' can actually add/remove this stuff to 'player */
    /* In order do this stuff. Executor must have all the 'Max' powers in the
     * powergroup & have the PowerGroup power. 
     *                    OR
     * They own the object and the powergroup exists on them.
     */
    if (executor != NOTHING) {
      if (!(Owns(executor, player) && powergroup_has(executor, pgrp)))
        for (power = ptab_firstentry_new(ps_tab.powers, pname); power;
             power = ptab_nextentry_new(ps_tab.powers, pname))
          if (!strcmp(pname, power->name)
              && (God(executor) ? YES :
                  check_power_yescode(DPBITS(executor), power))
              < check_power_yescode(pgrp->max_powers, power)) {
            notify_format(executor,
                          "You must possess '%s' power to set the '%s' powergroup.",
                          power->name, pgrp->name);
            goto pgset_recurse;
          }
    }


    if (reset) {
      if (!powergroup_has(player, pgrp)) {
        /* TODO: Better message */
        notify(executor,
               "They don't have the powergroup in the first place.");
        continue;
      }
      /* Remove */
      rem_pg_from_player(player, pgrp);
      notify_format(executor, "You remove powergroup '%s' from %s.",
                    pgrp->name, object_header(executor, player));
      notify_format(player, "%s removed powergroup '%s' from you.",
                    object_header(player, executor), pgrp->name);
    } else {
      if (powergroup_has(player, pgrp)) {
        notify(executor,
               "They already have that powergroup.  Don't add it twice.");
        continue;
      }
      /* Add */
      add_pg_to_player(player, pgrp);
      if (autoset
          && ((executor == NOTHING)
              || div_powover(executor, player, "EMPOWER"))) {
        for (power = ptab_firstentry_new(ps_tab.powers, pname); power;
             power = ptab_nextentry_new(ps_tab.powers, pname))
          if (!strcmp(pname, power->name)
              && (ycode =
                  check_power_yescode(pgrp->auto_powers,
                                      power)) >
              (executor ==
               NOTHING ? YES : check_power_yescode(DPBITS(player),
                                                   power))) {
            switch (ycode) {
            case YES:
              GIVE_DPBIT(player, power->flag_yes);
            case YESLTE:
              GIVE_DPBIT(player, power->flag_lte);
            case YESLT:
              GIVE_DPBIT(player, power->flag_lt);
              break;
            }
          }
      }
      if (executor != NOTHING) {
        memset(tbuf, '\0', 128);
        strncpy(tbuf, object_header(executor, player), 127);
        notify_format(executor, "Added Powergroup '%s' to %s.", pgrp->name,
                      tbuf);
        memset(tbuf, '\0', 128);
        strncpy(tbuf, object_header(player, executor), 127);
        notify_format(player, "You received Powergroup '%s' from %s.",
                      pgrp->name, tbuf);
      }
    }
  pgset_recurse:
    /* Continue is here to calm down gcc warning on pgset_recurse 
     * is at the end of the for loop with nothing under it */
    continue;
  }
}

/* powergroup_empower() {{{3 */
/* Player Can be NOTHING which will bypass all security checks */
/* This is basically a powergroup version of division_empower()
 * for setting powers onto powergroups
 */
int
powergroup_empower(dbref executor, POWERGROUP * pgrp, const char *powers,
                   char autoset)
{
  POWER *power;
  div_pbits *pbits;
  int power_count, power_indx;
  char *p_buf[BUFFER_LEN / 2];
  char tbuf[BUFFER_LEN];
  char msg_buf[BUFFER_LEN], *mbp;
  int flag, pg_lscope;
  int good_object;
  char *p, *t;

  good_object = GoodObject(executor);

  if (!powers || !*powers) {
    if (good_object)
      notify(executor, "Must supply a power.");
    return 0;
  }

  pbits = autoset ? &pgrp->auto_powers : &pgrp->max_powers;

  strcpy(tbuf, powers);
  power_count = list2arr(p_buf, BUFFER_LEN / 2, tbuf, ' ');
  if (!power_count) {
    if (good_object)
      notify(executor, "Must supply powers.");
    return 0;
  }

  mbp = msg_buf;
  *mbp = '\0';

  for (power_indx = 0; power_indx < power_count; power_indx++) {
    t = p_buf[power_indx];
    if (*t == NOT_TOKEN) {
      t++;
      flag = NO;
    } else if ((p = strchr(t, ':'))) {
      *p++ = '\0';
      while (*p == ' ')
        p++;
      flag = yescode_i(p);
    } else
      flag = YES;
    power = find_power(t);
    if (!power) {
      if (good_object)
        notify_format(executor, T("No such power '%s'."), t);
      continue;
    }

    /* Check if A) They're Giving it
     *              1) Their power rating of that power must be better than it has
     *              2) If its the auto list its giving it to, it must exist in the max list
     *                 if not.. Attempt to give it to the max list first. If that fails. Fail
     *                 this.
     *          B) They're taking it away
     *              1) Their power rating of that power must equal to or better than it has
     *              2) If they're taking it away from the max list, make it disappear from the
     *                 auto list if it has it.
     */
    pg_lscope =
        (!good_object
         || God(executor)) ? YES : check_power_yescode(DPBITS(executor),
                                                       power);
    if (flag) {
      if (check_power_yescode(*pbits, power) == flag) {
        if (good_object)
          notify_format(executor,
                        "%s(%s) already exists on that powergroup.",
                        power->name, yescode_str(flag));
        continue;
      } else if ((flag > pg_lscope) && good_object) {
        notify_format(executor,
                      "You must posses %s(%s) to give it to a powergroup.",
                      power->name, yescode_str(flag));
        continue;
      }
      if (autoset) {
        /* Alrite.. they should already be able to do this.. now just check the level of
         * the power on the manual spot.  If it needs to be higher, upgrade it.
         */
        if (check_power_yescode(pgrp->max_powers, power) < flag) {
          if (!pgrp->max_powers)
            pgrp->max_powers = new_power_bitmask();
          RESET_POWER_DP(pgrp->max_powers, power);
          switch (flag) {
          case YES:
            DPBIT_SET(pgrp->max_powers, power->flag_yes);
            break;
          case YESLTE:
            DPBIT_SET(pgrp->max_powers, power->flag_lte);
            break;
          case YESLT:
            DPBIT_SET(pgrp->max_powers, power->flag_lt);
            break;
          }
          if (good_object)
            notify_format(executor, "Added '%s(%s)' as a max power.",
                          power->name, yescode_str(flag));
        }
      }
    } else {
      if (good_object && (pg_lscope < check_power_yescode(*pbits, power))) {
        notify(executor,
               "You must posses that power to take it away from a powergroup");
        continue;
      }
      if (!autoset) {
        /* If the power is on their auto list, remove it cause its no longer allowed to exist
         * there
         */
        if (check_power_yescode(pgrp->auto_powers, power) > NO) {
          RESET_POWER_DP(pgrp->auto_powers, power);
        }
        if (pgrp->auto_powers
            && power_is_zero(pgrp->auto_powers, DP_BYTES) == 0) {
          mush_free(pgrp->auto_powers, "POWER_SPOT");
          pgrp->auto_powers = NULL;
        }
        if (good_object)
          notify_format(executor, "Removed '%s' as an auto power.",
                        power->name);
      }
    }

    if (flag > NO && !*pbits)
      *pbits = new_power_bitmask();

    RESET_POWER_DP((*pbits), power);

    switch (flag) {
    case YES:
      DPBIT_SET((*pbits), power->flag_yes);
      break;
    case YESLTE:
      DPBIT_SET((*pbits), power->flag_lte);
      break;
    case YESLT:
      DPBIT_SET((*pbits), power->flag_lt);
      break;
    case NO:
      /* the power should already be clared, just check to see if we're zeroed out 
       * & then free the spot 
       */
      if (*pbits && power_is_zero(*pbits, DP_BYTES) == 0) {
        mush_free(*pbits, "POWER_SPOT");
        *pbits = NULL;
      }
    }

    if (good_object)
      safe_format(msg_buf, &mbp, "%s%s(%s)", *msg_buf != '\0' ? ", " : "",
                  power->name, yescode_str(flag));
  }
  *mbp = '\0';
  if (good_object && *msg_buf)
    notify_format(executor, T("PowerGroup '%s' Empowered: %s"), pgrp->name,
                  msg_buf);
  return 1;
}

/* powergroups_list() {{{3 - List powergroups able to set from 'players' point of view */
char *
powergroups_list(dbref player, char flag)
{
  POWERGROUP *pgrp;
  char pg_name[BUFFER_LEN];
  static char st_buf[BUFFER_LEN];
  char *st_bp;
  /* For right now this should list all powergroups period */

  st_bp = st_buf;
  *st_bp = '\0';

  for (pgrp = ptab_firstentry_new(ps_tab.powergroups, pg_name); pgrp;
       pgrp = ptab_nextentry_new(ps_tab.powergroups, pg_name)) {
    if (*st_buf != '\0') {
      if (flag)
        safe_str(", ", st_buf, &st_bp);
      else
        safe_chr(' ', st_buf, &st_bp);
    }
    safe_str(pg_name, st_buf, &st_bp);
  }
  *st_bp = '\0';
  return st_buf;

}

/* powergroups_list_on() {{{3 - List powergroups on 'player' */
char *
powergroups_list_on(dbref player, char commaize)
{
  static char st_buf[BUFFER_LEN];
  char *st_bp;
  struct power_group_list *pg_l;

  st_bp = st_buf;
  *st_bp = '\0';
  if ((pg_l = SDIV(player).powergroups)) {
    for (; pg_l; pg_l = pg_l->next) {
      if (*st_buf != '\0')
        safe_str(commaize ? ", " : " ", st_buf, &st_bp);
      safe_str(pg_l->power_group->name, st_buf, &st_bp);
    }
    *st_bp = '\0';
  } else
    return (char *) "None";
  return st_buf;
}

/* powergroup_delete() {{{3 - delete powergroup */
char
powergroup_delete(dbref player, const char *key)
{
  POWERGROUP *pgrp;
  int i;

  if (!God(player)) {
    notify(player, "Permission denied.");
    return 0;
  }

  if (!(pgrp = (POWERGROUP *) ptab_find_exact(ps_tab.powergroups, key)))
    return 0;

  for (i = 0; i < db_top; i++)
    if (powergroup_has(i, pgrp))
      rem_pg_from_player(i, pgrp);

  /* Delete & Free stuff */
  mush_free((Malloc_t) pgrp->name, "PG_NAME");
  if (pgrp->auto_powers)
    mush_free((Malloc_t) pgrp->auto_powers, "POWER_SPOT");
  if (pgrp->max_powers)
    mush_free((Malloc_t) pgrp->max_powers, "POWER_SPOT");
  mush_free((Malloc_t) pgrp, "POWERGROUP");
  ptab_delete(ps_tab.powergroups, key);
  return 1;
}

/* add_pg_to_player() {{{3 */
/* This function only adds the powergroup actually onto teh players
 * powergroup linked list.  Nothing more
 */
void
add_pg_to_player(dbref player, POWERGROUP * powergroup)
{
  struct power_group_list *pgl;
  struct power_group_list *newpgl;

  pgl = SDIV(player).powergroups;

  /* Is this their first power group? */
  if (pgl == NULL) {
    newpgl =
        (struct power_group_list *)
        mush_malloc(sizeof(struct power_group_list), "PG_LIST");
    newpgl->power_group = powergroup;
    newpgl->next = NULL;
    SDIV(player).powergroups = newpgl;
    return;
  }

  /* They already have it, so cancel the operation - this catches the first
   * one only */
  if (pgl->power_group == powergroup)
    return;

  /* Should it be the first one? */
  if (strcasecmp(pgl->power_group->name, powergroup->name) > 0) {
    newpgl =
        (struct power_group_list *)
        mush_malloc(sizeof(struct power_group_list), "PG_LIST");
    newpgl->power_group = powergroup;
    newpgl->next = pgl;
    SDIV(player).powergroups = newpgl;
    return;
  }

  for (; pgl->next; pgl = pgl->next) {
    /* They already have it, so cancel the operation */
    if (pgl->next->power_group == powergroup)
      return;

    /* Keep power groups sorted by name */
    if (strcasecmp(pgl->next->power_group->name, powergroup->name) > 0)
      break;
  }

  newpgl =
      (struct power_group_list *)
      mush_malloc(sizeof(struct power_group_list), "PG_LIST");
  newpgl->power_group = powergroup;

  newpgl->next = pgl->next;
  pgl->next = newpgl;
}

/* powergroup_has() {{{3 - Check to see if player has the powergroup on 'em */
int
powergroup_has(dbref player, POWERGROUP * pgrp)
{
  struct power_group_list *pg_list;

  for (pg_list = SDIV(player).powergroups; pg_list;
       pg_list = pg_list->next)
    if (pg_list->power_group == pgrp)
      return 1;
  return 0;
}

/* rem_pg_from_player() {{{3 - Remove Power Group from object */
void
rem_pg_from_player(dbref player, POWERGROUP * powergroup)
{
  POWER *power;
  char pname[BUFFER_LEN];
  struct power_group_list *pgl, *prev;
  int ycode;

  prev = NULL;
  for (pgl = SDIV(player).powergroups; pgl; prev = pgl, pgl = pgl->next)
    if (pgl->power_group == powergroup)
      break;

  /* Did they even have it to begin with? */
  if (!pgl)
    return;

  /* Was it the first one? */
  if (!prev) {
    SDIV(player).powergroups = pgl->next;
    mush_free((Malloc_t) pgl, "PG_LIST"); 
    SDIV(player).powergroups = NULL;
} else {
    prev->next = pgl->next;
   mush_free((Malloc_t) pgl, "PG_LIST");
 }

  /* After its all removed and shit.  Make sure they can still have 
   * all those happy little fuckin powers */
  for (power = ptab_firstentry_new(ps_tab.powers, pname); power;
       power = ptab_nextentry_new(ps_tab.powers, pname))
    if (!strcmp(pname, power->name))
      if (!can_have_power
          (player, power, check_power_yescode(DPBITS(player), power))) {
        RESET_POWER(player, power);
        /* If they can have the power but at a lower level. just downgrade the power */
        ycode = can_receive_power_at(player, power);
        switch (ycode) {
        case YES:
          GIVE_DPBIT(player, power->flag_yes);
        case YESLTE:
          GIVE_DPBIT(player, power->flag_lte);
        case YESLT:
          GIVE_DPBIT(player, power->flag_lt);
          break;
        }
      }

}

/* powergroup_add() {{{3 */
POWERGROUP *
powergroup_add(const char *pg_name)
{
  POWERGROUP *pgrp;

  if (!pg_name || !*pg_name)
    return NULL;

  /* Don't add if it already exists */
  if (powergroup_find(pg_name))
    return NULL;

  ptab_start_inserts(ps_tab.powergroups);

  /* Initialize Power group */
  pgrp = (POWERGROUP *) mush_malloc(sizeof(POWERGROUP), "POWERGROUP");
  pgrp->name = mush_strdup(pg_name, "PG_NAME");
  pgrp->max_powers = NULL;
  pgrp->auto_powers = NULL;

  /* Insert it into the ptab */
  ptab_insert(ps_tab.powergroups, pg_name, pgrp);
  ptab_end_inserts(ps_tab.powergroups);

  return pgrp;
}


/* Power Control Checks {{{2  */
/* powc_levchk() {{{3 * Normal Power Over checking */
int
powc_levchk(int plev, dbref who, dbref what)
{

  /* quick check if its themself */
  if (plev > NO && who == what)
    return 1;
  switch (plev) {
  case YES:
    return 1;
  case YESLTE:
    if (LEVEL(who) >= LEVEL(what))
      return 1;
    break;
  case YESLT:
    if ((LEVEL(who) > LEVEL(what))
        || (Owner(who) == Owner(what) && LEVEL(who) == LEVEL(what)))
      return 1;
  }
  return 0;
}

/* powc_levchk() {{{3  - special check for level setting */
int
powc_levset(int plev, dbref who, dbref what)
{
  int i;
  i = powc_levchk(plev, who, what);
  if (IsDivision(what) && !div_powover(who, who, "Division"))
    return 0;
  return i;
}

/* powc_self() {{{3 - Can only use it over yourself Check */
int
powc_self(int plev __attribute__ ((__unused__)), dbref p1, dbref p2)
{
  return (p1 == p2);
}

/* powc_bcreate() {{{3 - Bcreate check.. dude using it over must be a builder (used for newpass check) */
int
powc_bcreate(int plev __attribute__ ((__unused__)), dbref who, dbref what)
{
  if (!div_powover(who, who, "Builder"))
    return 0;
  return (LEVEL(who) >= LEVEL(what));
}

/* powc_levchk_lte()  {{{3 - FullYes power(like BCreate) but can only be used LTE */
int
powc_levchk_lte(int plev
                __attribute__ ((__unused__)), dbref who, dbref what)
{
  return (LEVEL(who) >= LEVEL(what) || (plev > NO && who == what));
}

/* End of POWC Checks }}}2 */

/* COMMAND(cmd_power) {{{2 */
/* 
 * @power/list
 * - List Powers
 *   
 *   @power/add Power=Type
 *   - Add Power
 *
 *   @power/Delete Power
 *
 *   @power <power>
 *   - Power Info
 *
 *   @power <player>=<power>
 *   - Empower Equivalent
 *
 *   @power/alias <power to point to>=<alias>
 *   - Alias Power
 */

COMMAND(cmd_power)
{
  POWER *power, *alias;
  char tbuf[BUFFER_LEN], pname[BUFFER_LEN];
  char *tbp;
  dbref target;
  int i;

  tbp = tbuf;
  *tbp = '\0';

  if (SW_ISSET(sw, SWITCH_LIST)) {
    do_list_powers(player, arg_left);
  } else if (SW_ISSET(sw, SWITCH_ADD) && Site(player)) {
    /* Add power */
    if (!arg_left || !arg_right) {
      notify(player, "Must Supply Power Name & Type");
      return;
    }
    power = find_power(arg_left);
    if (!power) {
      /* Make sure its a good power name */
      switch (power_goodname(arg_left)) {
      case 0:
        notify(player, "Must Supply power name.");
        return;
      case -1:
        notify(player, "Must be longer than one character.");
        return;
      case -2:
        notify(player, "Power may not contain spaces.");
        return;
      default:
        break;
      }
      /* Make sure type exists */
      for (i = 0; powc_list[i].name; i++)
        if (!strcasecmp(powc_list[i].name, arg_right))
          break;
      if (powc_list[i].name) {
        power = add_power_type(arg_left, powc_list[i].name);
        notify_format(player, "Added power '%s' as a '%s' type power.",
                      power->name, powc_list[i].name);
      } else
        notify(player, "Invalid Type.");
    } else
      notify(player, "No point adding a power that already exists.");
  } else if (SW_ISSET(sw, SWITCH_DELETE) && Site(player)) {
    /* This should handle deleting both aliases & powers */
    if (arg_left && *arg_left) {
      power = (POWER *) ptab_find_exact(ps_tab.powers, arg_left);
      if (power) {
        if (strcasecmp(power->name, arg_left)) {
          /* This is an alias, this one is real easy. */
          ptab_delete(ps_tab.powers, arg_left);
          notify_format(player, "Power Alias '%s' removed.", arg_left);
        } else {
          /* This is a real power */
          if (power_delete(arg_left))
            notify_format(player, "Power '%s' deleted.", arg_left);
          else
            notify(player, "No such power.");
        }
      } else
        notify(player, "No such power or alias.");
    } else
      notify(player, "Must specify a power or alias to delete.");
  } else if (SW_ISSET(sw, SWITCH_ALIAS) && Site(player)) {
    if (arg_left && *arg_left && arg_right && *arg_right) {
      /* Make sure alias doesn't already exist */
      power = find_power(arg_right);
      if (!power) {
        power = find_power(arg_left);
        if (!power) {
          notify(player, "That power does not exist.");
          return;
        }
        ptab_start_inserts(ps_tab.powers);
        ptab_insert(ps_tab.powers, arg_right, power);
        ptab_end_inserts(ps_tab.powers);
        notify_format(player, "Power alias '%s' added pointing to '%s'.",
                      arg_right, power->name);
      } else if (strcasecmp(power->name, arg_left)) {
        /* Attempt to re-alias the power, by deleting the alias & repointing it */
        power = find_power(arg_left);
        if (power) {
          /* delete */
          ptab_delete(ps_tab.powers, arg_right);
          /* re-add */
          ptab_start_inserts(ps_tab.powers);
          ptab_insert(ps_tab.powers, arg_right, power);
          ptab_end_inserts(ps_tab.powers);
          notify_format(player, "Power alias '%s' added pointing to '%s'.",
                        arg_right, power->name);
        } else
          notify(player, "That power does not exist.");
      } else
        notify(player, "Can not overwrite a real power with an alias.");
    } else
      notify(player, "Must supply alias and power to alias it to.");
  } else {
    if (arg_left && arg_right && *arg_left && *arg_right) {
      /* Empower Alias Code */
      target =
          noisy_match_result(player, arg_left, NOTYPE, MAT_EVERYTHING);
      if (target == NOTHING)
        return;
      division_empower(player, target, arg_right);
    } else if (arg_left && *arg_left) {
      /* Power Info Code */
      power = find_power(arg_left);
      if (power) {
        /* Find Power Type */
        for (i = 0; powc_list[i].name; i++)
          if (powc_list[i].powc_chk == power->powc_chk)
            break;
        for (alias = ptab_firstentry_new(ps_tab.powers, pname); alias;
             alias = ptab_nextentry_new(ps_tab.powers, pname))
          if (strcasecmp(pname, alias->name) && alias == power) {
            if (!*tbuf)
              safe_str(pname, tbuf, &tbp);
            else
              safe_format(tbuf, &tbp, ", %s", pname);
          }
        *tbp = '\0';

        notify_format(player, "Power Name    : %s", power->name);
        notify_format(player, "Power Aliases : %s", tbuf ? tbuf : "");
        notify_format(player, "Power Type    : %s", powc_list[i].name);
      } else
        notify(player, "No such power.");
    }
  }
}

/* COMMAND(cmd_powergroup) {{{2 */
/* @powergroup - syntaxes
 * 
 * @powergroup/add <powergroup>          - Add a powergroup into the system
 * @powergroup/delete <powergroup>       - Remove a powergroup from a user
 * @powergroup <powergroup>              - View powergroup Info
 * @powergroup <powergroup>=<power>      - Alias to @powergroup/max
 * @powergroup <user>=<powergroup>       - Assign a powergroup to a user
 * @powergroup/auto <powergroup>=<power> - Assign a power as an autoset for the powergroup
 * @powergroup/max <powergroup>=<power>  - Assign a power to the maxset for the powergroup
 * @powergroup/list                      - List all available powergroups
 * @powergroup/list <player>		 - List powergroups set on a object
 * @powergroup/raw <user>=<powergroup>   - Assign a powergroup to a user without assigning auto powers
 *
 */

COMMAND(cmd_powergroup)
{
  POWERGROUP *pgrp;
  dbref match;


  if (SW_ISSET(sw, SWITCH_LIST)) {
    if (!has_power(player, "POWERGROUP")) {
      notify(player, "Permission denied.");
      return;
    }

    if (arg_left && *arg_left) {
      match = match_thing(player, arg_left);
      if (!GoodObject(match))
        return;
      notify_format(player, "Powergroups on %s: %s",
                    object_header(player, match),
                    powergroups_list_on(match, 1));
    } else
      notify_format(player,
                    "PowerGroups Available to Set: %s",
                    powergroups_list(player, 1));
  } else if (SW_ISSET(sw, SWITCH_ADD)) {
    if (!has_power(player, "POWERGROUP")) {
      notify(player, "Permission denied.");
      return;
    }

    switch (power_goodname(arg_left)) {
    case 0:
      notify(player, "Must supply powergroup name.");
      return;
    case -1:
      notify(player, "Powergroup name must be longer than one character.");
      return;
    case -2:
      notify(player, "Powergroup name may not contain spaces in it.");
      return;
    default:
      break;
    }
    if (powergroup_add(arg_left))
      notify(player, "Added powergroup.");
    else
      notify(player, "Couldn't add powergroup.");
  } else if (SW_ISSET(sw, SWITCH_DELETE)) {
    if (powergroup_delete(player, arg_left))
      notify(player, "Deleted powergroup.");
    else
      notify(player, "Couldn't delete powergroup.");
  } else if (SW_ISSET(sw, SWITCH_AUTO) || SW_ISSET(sw, SWITCH_MAX)) {
    if (!has_power(player, "POWERGROUP")) {
      notify(player, "Permission denied.");
      return;
    }

    /* Assign auto powers */
    if (arg_left && *arg_left) {
      if ((pgrp = powergroup_find(arg_left))) {
        powergroup_empower(player, pgrp, arg_right,
                           !!SW_ISSET(sw, SWITCH_AUTO));
      } else
        notify(player, "No such powergroup.");

    } else
      notify(player, "Must provide a powergroup argument.");
  } else if (SW_ISSET(sw, SWITCH_RAW)) {
    if (arg_left && *arg_left && arg_right && *arg_right) {
      match = match_thing(player, arg_left);
      if (GoodObject(match))
        powergroup_db_set(player, match, arg_right, 0);
    } else
      notify(player, "Must supply a player and powergroup argument.");
  } else {
    if (!arg_left || !*arg_left) {
      notify(player, "Must supply an argument.");
      return;
    } else if (arg_right && *arg_right) {
      match = match_result(player, arg_left, NOTYPE, MAT_EVERYTHING);
      if (GoodObject(match)) {
        powergroup_db_set(player, match, arg_right, 1);
      } else {
        /* They're assigning a power to a powergroup */
        if ((pgrp = powergroup_find(arg_left))
            && has_power(player, "POWERGROUP"))
          (void) powergroup_empower(player, pgrp, arg_right, 0);
        else                    /* Give can't find that error message as if we're failing on GoodObject */
          notify(player, "I can't see that here.");
        return;
      }
    } else {
      if (!has_power(player, "POWERGROUP")) {
        notify(player, "Permission denied.");
        return;
      }

      if ((pgrp = powergroup_find(arg_left))) {
        /* They're wanting info on the powergroup */
        notify_format(player, "%sPowerGroup Name - %s%s", ANSI_HILITE,
                      pgrp->name, ANSI_NORMAL);
        notify_format(player, "%sAutoSet Powers -%s %s", ANSI_HILITE,
                      ANSI_NORMAL, list_dp_powerz(pgrp->auto_powers, 1));
        notify_format(player, "%sMaxSet Powers  -%s %s", ANSI_HILITE,
                      ANSI_NORMAL, list_dp_powerz(pgrp->max_powers, 1));
      } else
        notify(player, "No such powergroup.");
    }
  }
}

/* list_dp_powers() {{{2 */
const char *
list_dp_powerz(div_pbits pbits, int flag)
{
  POWER *power;
  char pname[BUFFER_LEN];
  static char buf1[BUFFER_LEN], *tbp;
  int yescode;

  tbp = buf1;
  for (power = ptab_firstentry_new(ps_tab.powers, pname); power;
       power = ptab_nextentry_new(ps_tab.powers, pname))
    if (!strcmp(pname, power->name)) {
      yescode = check_power_yescode(pbits, power);
      if (yescode > NO) {
        if (tbp != buf1)
          safe_chr(' ', buf1, &tbp);
        safe_str(pname, buf1, &tbp);
        if (flag) {
          if (yescode < YES)
            safe_format(buf1, &tbp, "(%s)", yescode_str(yescode));
        } else
          safe_format(buf1, &tbp, ":%d", yescode);

      }
    }
  *tbp = '\0';
  return buf1;
}

/* division_set() {{{2 */
void
division_set(dbref exec, dbref target, const char *arg2)
{
/* @div/set player=division, @div/set player=none for reset */
  ATTR *divrcd;
  char *p_buf[BUFFER_LEN / 2];
  char buf[BUFFER_LEN], *bp;
  dbref cur_obj, divi;
  struct power_group_list *pg_l;
  int cnt;

  /* Lets make sure they're completely logged out if they're in @sd/division */
  divrcd = atr_get(target, "XYXX_DIVRCD");
  
  if(divrcd != NULL && (cnt = list2arr(p_buf, BUFFER_LEN / 2, safe_atr_value(divrcd), ' ')) > 0) {
    notify(exec, "Player must log out of every occurance of @sd/division before they may be redivisioned.");
    return;
  }


  if (!div_powover(exec, target, "Attach")) {
    notify(exec, T(e_perm));
    return;
  }

  

  if (!strcasecmp(arg2, "NONE")) {
    notify(exec, T("Division reset."));
    notify(target, T("GAME: Division reset."));

    if (Typeof(target) == TYPE_PLAYER)
      adjust_divisions(target, Division(target), NOTHING);
    else {
      for (pg_l = SDIV(target).powergroups; pg_l;
           pg_l = SDIV(target).powergroups)
        rem_pg_from_player(target, pg_l->power_group);
      if (DPBITS(target))
        mush_free(DPBITS(target), "POWER_SPOT");
      DPBITS(target) = NULL;
    }

    return;
  }
  divi = match_result(exec, arg2, TYPE_THING, MAT_EVERYTHING);
  if (divi == NOTHING || !GoodObject(divi)) {
    notify(exec, T("That is not a valid division."));
    return;
  }
  if (!IsDivision(divi)) {
    notify(exec, T("That is not a valid division."));
    return;
  }
  if (!div_inscope(exec, divi)) {
    notify(exec, T("That division is not in your scope."));
    return;
  }

  if (divi == target) {
    notify(exec, T("Can't division something to itself."));
    return;
  }

  /* Make sure the receiving division has the quota to receive all of this crapp...
   */

  if(Typeof(target) == TYPE_PLAYER) {
    for(cnt = cur_obj = 0; cur_obj < db_top ; cur_obj++)
      if(!IsPlayer(cur_obj) && 
          (Owner(cur_obj) == target) 
          && (Division(cur_obj) == Division(target)))
        cnt++;
  } else cnt = 1;

  if(!pay_quota(divi, cnt)) {
    notify(exec, T("That division does not have the quota required to receive that."));
    return;
  }
  
  if(GoodObject(Division(target)) && Division(target))
    change_quota(Division(target), cnt);

  if (Typeof(target) == TYPE_PLAYER)
    adjust_divisions(target, Division(target), divi);
  else
    SDIV(target).object = divi;
  if (IsDivision(target) && GoodObject(SDIV(target).object)) {
    Parent(target) = divi;
    moveto(target, divi);
  }
  /* kludged this in, because notify_format won't work right 
   * with 2 object_header's in a row 
   */
  bp = buf;
  safe_str(T("Division for "), buf, &bp);
  safe_str(object_header(exec, target), buf, &bp);
  safe_str(T(" set to "), buf, &bp);
  safe_str(object_header(exec, divi), buf, &bp);
  safe_chr('.', buf, &bp);
  *bp = '\0';
  notify(exec, buf);
  notify_format(target, T("GAME: Division set to %s."),
                object_header(target, divi));
}

/* create_div() {{{2 - Create New Division with name & return the dbref */
dbref
create_div(dbref owner, const char *name)
{
  char buf[BUFFER_LEN];
  dbref obj, loc;

  if(!can_pay_fees(owner, DIVISION_COST))
    return NOTHING;

  if (!div_powover(owner, owner, "Division")) {
    notify(owner, T(e_perm));
    return NOTHING;
  } else if (*name == '\0') {
    notify(owner, T("Create what division?"));
    return NOTHING;
  } else if (!ok_name(name)) {
    notify(owner, T("Not a valid name for a division."));
    return NOTHING;
  }
  obj = new_object();
  set_name(obj, name);
  Owner(obj) = owner;
  Zone(obj) = NOTHING;
  Type(obj) = TYPE_DIVISION;
  if ((loc = Location(owner)) != NOTHING &&
      (controls(owner, loc) || Abode(loc))) {
    Home(obj) = loc;            /* home */
  } else {
    Home(obj) = Home(owner);    /* home */
  }
  SDIV(obj).object = SDIV(owner).object;
  /* if they belong to a division set it there */
  if (SDIV(obj).object != NOTHING) {
    Location(obj) = SDIV(obj).object;
    PUSH(obj, Contents(SDIV(obj).object));
  } else {
    Location(obj) = owner;
    PUSH(obj, Contents(owner));
  }
  Parent(obj) = SDIV(owner).object;
  current_state.divisions++;
  sprintf(buf, object_header(owner, obj));
  notify_format(owner, T("Division created: %s  Parent division: %s"),
                buf, object_header(owner, SDIV(obj).object));
  return obj;
}

/* check_power_yescode() {{{2 - check integer power yescode of said POWER * */
int
check_power_yescode(div_pbits pbits, POWER * power)
{
  if (!pbits)
    return NO;
  if (DPBIT_ISSET(pbits, power->flag_yes))
    return YES;
  else if (DPBIT_ISSET(pbits, power->flag_lte))
    return YESLTE;
  else if (DPBIT_ISSET(pbits, power->flag_lt))
    return YESLT;
  else
    return NO;
}

/* div_powover() {{{2 -  check if 'who' can use power 'name' over 'what' */
int
div_powover(dbref who, dbref what, const char *name)
{
  POWER *power;
  int inscope, ycode, pcheck;

  if (God(who))                 /* god is almighty */
    return 1;

  inscope = div_inscope(who, what);
  power = find_power(name);
  if (!power)
    return 0;
  if (God(who))
    ycode = YES;
  else
    ycode = check_power_yescode(DPBITS(who), power);
  if (ycode == NO)
    pcheck = 0;
  else
    pcheck = power->powc_chk(ycode, who, what);

  if (inscope && pcheck)
    return 1;

  if (Owner(who) != who && Inherit_Powers(who)) {
    /* We don't let God's objects inherit his powers, for safety.
     */

    if (God(Owner(who)))
      return 0;

    inscope = div_inscope(Owner(who), what);
    ycode = check_power_yescode(DPBITS(Owner(who)), power);

    if (ycode == NO)
      pcheck = 0;
    else
      pcheck = power->powc_chk(ycode, who, what);
    if (inscope && pcheck)
      return 1;
  }

  return 0;
}

/* div_inscope() {{{2 - check if scopee is in divscope of scoper */
int
div_inscope(dbref scoper, dbref scopee)
{
/* check if scopee is in the divscope of scoper */
  dbref div1, div2;

  if (!GoodObject(scopee))
    return 0;
  if (God(scoper))
    return 1;

  /* For the odd case they aren't in a division.. They always in their own divscope */
  if (scoper == scopee)
    return 1;

  if (IsDivision(scoper))
    div1 = scoper;
  else
    div1 = SDIV(scoper).object;

  if (IsDivision(scopee))
    div2 = scopee;
  else
    div2 = SDIV(scopee).object;

  if (div1 == NOTHING)          /* can't match to nothing */
    return 0;
  if (div2 == NOTHING)          /*  they're automatically at the bottom of the divtree */
    return 1;

  for (; div1 != div2; div2 = SDIV(div2).object)
    if (div2 == NOTHING)        /* went off the tree */
      return 0;
    else if (div2 == SDIV(div2).object) {       /* detect & fix bad division tree */
      do_log(LT_ERR, scoper, scopee,
             T("Bad Master Division(#%d).  Corrected."), div2);
      SDIV(div2).object = NOTHING;
    }
  return 1;
}

/* division_empower() {{{2 */
void
division_empower(dbref exec, dbref target, const char *arg2)
{
  POWER *power;
  int power_count, power_indx;
  char *p_buf[BUFFER_LEN / 2];
  char tbuf[BUFFER_LEN];
  char msg_buf[BUFFER_LEN], *mbp;
  int flag, cont;
  char *p, *t;

  if (!div_powover(exec, target, "Empower")) {
    notify(exec, T(e_perm));
    return;
  }

  if (!arg2 || !*arg2) {
    notify(exec, "Must supply a power.");
    return;
  }

  strcpy(tbuf, arg2);
  power_count = list2arr(p_buf, BUFFER_LEN / 2, tbuf, ' ');
  if (!power_count) {
    notify(exec, "Must supply powers.");
    return;
  }

  mbp = msg_buf;
  *mbp = '\0';

  for (cont = power_indx = 0; power_indx < power_count;
       cont = 0, power_indx++) {
    t = p_buf[power_indx];
    if (*t == NOT_TOKEN) {
      t++;
      flag = NO;
    } else if ((p = strchr(t, ':'))) {
      *p++ = '\0';
      while (*p == ' ')
        p++;
      flag = yescode_i(p);
    } else
      flag = YES;
    power = find_power(t);
    if (!power) {
      notify_format(exec, T("No such power '%s'."), t);
      continue;
    }

    switch (can_give_power(exec, target, power, flag)) {
    case 0:
      notify(exec, T("You can't give powers you don't have."));
      cont = 1;
      break;
    case -1:
      notify(exec, T("You can't take away powers you don't have."));
      cont = 1;
      break;
    case -2:
      notify_format(exec, T("Target player can't have power: '%s'"),
                    power->name);
      cont = 1;
      break;
    }

    if (cont)
      continue;


    /* Reset power regardless, it'll be set back */
    RESET_POWER(target, power);

    switch (flag) {
    case YES:
      GIVE_DPBIT(target, power->flag_yes);
    case YESLTE:
      GIVE_DPBIT(target, power->flag_lte);
    case YESLT:
      GIVE_DPBIT(target, power->flag_lt);
      break;
    case NO:
      /* Upon getting rid of a power.. check to see if we're zeroed out & then free the spot */
      if (DPBITS(target) && power_is_zero(DPBITS(target), DP_BYTES) == 0) {
        mush_free(DPBITS(target), "POWER_SPOT");
        DPBITS(target) = NULL;
      }
    }

    safe_format(msg_buf, &mbp, "%s%s(%s)", *msg_buf != '\0' ? ", " : "",
                power->name, yescode_str(flag));
  }
  *mbp = '\0';
  if (*msg_buf) {
    notify_format(exec, T("%s : %s."), object_header(exec, target),
                  msg_buf);
    notify_format(target, T("You have been empowered '%s' by %s."),
                  msg_buf, object_header(target, exec));
    do_log(LT_WIZ, exec, target, T("POWER SET: %s"), msg_buf);
  }
}

/* can_give_power() {{{2 - check if can give power to said object */
/* giver-> guy givin power, receiver-> guy reciving power, power->pointer to power,
 * pow_lev-> powerscope level
 * Return Values: 1->Yes, 
 * No Vals:  0 -> Giver  doesn't have it
 * 	    -1 -> Giver power conflict in lowering pow_lev
 * 	    -2 -> Receiver can't receive it due to powergroup conflict
 */
int
can_give_power(dbref giver, dbref receiver, POWER * power, int pow_lev)
{
  int g_level;
  int r_level;

  if (God(giver))               /* God is almighty */
    return 1;

  if (God(giver))
    g_level = YES;
  else
    g_level = check_power_yescode(DPBITS(giver), power);
  if (g_level < pow_lev) {
    if (Inherit_Powers(giver))
      return can_give_power(Owner(giver), receiver, power, pow_lev);

    return 0;
  }

  /* They can only lower their powerscope if they're GTE */
  if (God(receiver))
    r_level = YES;
  else
    r_level = check_power_yescode(DPBITS(receiver), power);

  if (!can_have_power(receiver, power, pow_lev))
    return -2;

  if (pow_lev < r_level && g_level < r_level) {
    if (Inherit_Powers(giver)
        && can_give_power(Owner(giver), receiver, power, pow_lev) == 1)
      return 1;

    return -1;
  }

  return 1;
}

/* can_have_power() {{{2 - check if object can have power at said plevel*/
int
can_have_power(dbref player, POWER * power, int level)
{
  struct power_group_list *pg_l;

  if (God(player))
    return 1;

  for (pg_l = SDIV(player).powergroups; pg_l; pg_l = pg_l->next)
    if (check_power_yescode(pg_l->power_group->max_powers, power) >= level)
      return 1;
  return 0;
}

/* can_receive_power_at() {{{2 - return what plevel object can have power at  */
/* Returns what yescode player can receive 'power' at */
int
can_receive_power_at(dbref player, POWER * power)
{
  struct power_group_list *pg_l;
  int ycode = NO;
  int cur_ycode;

  if (God(player))
    return YES;

  for (pg_l = SDIV(player).powergroups; pg_l; pg_l = pg_l->next) {
    cur_ycode = check_power_yescode(pg_l->power_group->max_powers, power);
    if (cur_ycode > ycode)
      ycode = cur_ycode;
  }
  return ycode;
}

/* division_level() {{{2 */
int
division_level(dbref exec, dbref target, int level)
{
  POWER *power;
  int plev, nl;

  if (level < 1)
    return 0;

  power = find_power("Level");
  if (!div_powover(exec, target, "Level")) {
    if (Owner(target) == exec) {        /* if they own it.. let 'em have it */
      if (!power->powc_chk(exec, target, YESLTE))
        return 0;
    } else
      return 0;
  }

  plev = God(exec) ? YES : check_power_yescode(DPBITS(exec), power);

  if (level < LEVEL(exec) && exec == target)    /* stupidity safeguard */
    return 0;
  else if (plev < YESLTE && exec == Owner(target))
    plev = YESLTE;

  /* First make sure they can be this 'level' in the 
   * division, if not cap it down to max level. 
   */
  if (God(exec))
    nl = level > MAX_LEVEL ? MAX_LEVEL : level;
  else {
    if (GoodObject(SDIV(target).object)) {
      nl = (level >
            LEVEL(SDIV(target).object) ? LEVEL(SDIV(target).
                                               object) : level);
    } else
      nl = LEVEL_UNREGISTERED;  /* Not on the divtree? Only unregistered peeps! */
  }

  /* do proper checking for each rank level */
  switch (plev) {
  case YES:
  case YESLTE:
    if (nl > LEVEL(exec))
      return 0;
    break;
  case YESLT:
    if (nl >= LEVEL(exec))
      return 0;
    break;
  case NO:
    return 0;
  }

  do_log(LT_WIZ, exec, target, T("Level set to '%d'"), nl);

  if (Typeof(target) == TYPE_PLAYER) {
    SLEVEL(target) = nl;
    adjust_levels(target, nl);  /* security for the owner. */
  } else
    SLEVEL(target) = nl;
  return 1;
}

/* division_list_powerz() {{{2 - return string of powers on target */
const char *
division_list_powerz(dbref target, int flag)
{
  POWER *power;
  char pname[BUFFER_LEN];
  static char buf1[BUFFER_LEN], *tbp;
  int yescode;

  tbp = buf1;
  for (power = ptab_firstentry_new(ps_tab.powers, pname); power;
       power = ptab_nextentry_new(ps_tab.powers, pname))
    if (!strcmp(pname, power->name)) {
      yescode =
          God(target) ? YES : check_power_yescode(DPBITS(target), power);
      if (yescode > NO) {
        if (tbp != buf1)
          safe_chr(' ', buf1, &tbp);
        safe_str(pname, buf1, &tbp);
        if (flag) {
          if (yescode < YES)
            safe_format(buf1, &tbp, "(%s)", yescode_str(yescode));
        } else
          safe_format(buf1, &tbp, ":%d", yescode);

      }
    }
  *tbp = '\0';
  return buf1;
}

/* adjust_powers() {{{2 - adjust powers on 'obj' to what max 'to' can have them at */
/* adjust powers to the level of 'to' */
void
adjust_powers(dbref obj, dbref to)
{
  POWER *power;
  char pname[BUFFER_LEN];
  int plev;

  for (power = ptab_firstentry_new(ps_tab.powers, pname); power;
       power = ptab_nextentry_new(ps_tab.powers, pname))
    if (!strcmp(pname, power->name)) {
      if ((God(obj) ? YES : check_power_yescode(DPBITS(obj), power)) >
          (plev =
           (God(to) ? YES : check_power_yescode(DPBITS(to), power)))) {
        RESET_POWER(obj, power);
        switch (plev) {
        case YESLTE:
          GIVE_DPBIT(obj, power->flag_lte);
        case YESLT:
          GIVE_DPBIT(obj, power->flag_lt);
          break;
        }
      }
    }
}

/* adjust_levels() {{{2 - cap levels of 'owner's object to 'level'  */
static void
adjust_levels(dbref owner, int level)
{
  dbref cur_obj;
  for (cur_obj = 0; cur_obj < db_top; cur_obj++)
    if ((Owner(cur_obj) == owner) && !IsDivision(cur_obj))
      if (SLEVEL(cur_obj) > level)
        SLEVEL(cur_obj) = level;
}

/* adjust_divisions() {{{2 */
static void
adjust_divisions(dbref owner, dbref from_division, dbref to_division)
{
  int cur_obj;
  struct power_group_list *pg_l;
  struct power_group_list *next;

  for (cur_obj = 0; cur_obj < db_top; cur_obj++) {
    if ((Owner(cur_obj) == owner) && !IsDivision(cur_obj) && Division(cur_obj) == from_division) {
      Division(cur_obj) = to_division;
      for (pg_l = SDIV(cur_obj).powergroups; pg_l; pg_l = next) {
        next = pg_l->next;
        if (!can_have_pg(cur_obj, pg_l->power_group))
          rem_pg_from_player(cur_obj, pg_l->power_group);
      }
    }
  }
}

int
can_have_pg(dbref object, POWERGROUP * pgrp)
{
  POWER *power;
  char pname[BUFFER_LEN];
  int deny = 0;

  /* Check The Division of the object.. make sure they can have the powergroup
   */

  /* If they're a player, check to see if its the default powergroup */

  if (IsPlayer(object) && !strcasecmp(pgrp->name, PLAYER_DEF_POWERGROUP))
    return 1;


  /* Otherwise If they have no division, they can't have any powergroups */

  if (!GoodObject(SDIV(object).object))
    return 0;

  /* Check to see if they're division has the powergroup, if so grant it */

  if (powergroup_has(SDIV(object).object, pgrp))
    return 1;

  /* Check to see if the division can obtain the powergroup, if so grant it */

  if (DPBITS(SDIV(object).object))
    for (deny = 1, power = ptab_firstentry_new(ps_tab.powers, pname);
         power; power = ptab_nextentry_new(ps_tab.powers, pname))
      if (!strcmp(pname, power->name)
          && check_power_yescode(DPBITS(SDIV(object).object),
                                 power) <
          check_power_yescode(pgrp->max_powers, power)) {
        deny = 0;
        break;
      }

  return deny;
}

/* clear_division() {{{2 */
void
clear_division(dbref divi)
{
  int cur_obj;
  struct power_group_list *pg_l;

  for (cur_obj = 0; cur_obj < db_top; cur_obj++)
    if ((SDIV(cur_obj).object == divi) && Typeof(cur_obj) == TYPE_DIVISION) {
      /* If we run across a division pass it up the divtree */
      SDIV(cur_obj).object = SDIV(divi).object;
      Parent(cur_obj) = SDIV(divi).object;
      if (GoodObject(Location(SDIV(divi).object)))
        moveto(cur_obj, SDIV(divi).object);
    } else if (SDIV(cur_obj).object == divi) {
      notify_format(cur_obj, T("GAME: Division '%s' has been cleared."),
                    object_header(cur_obj, divi));
      SDIV(cur_obj).object = NOTHING;
      if (Typeof(cur_obj) == TYPE_PLAYER && !God(cur_obj))      /* if player.. fix level & powerlevel */
        division_level(1, cur_obj, 2);
      /* Zap powergroups & powers as need be */
      for (pg_l = SDIV(cur_obj).powergroups; pg_l;
           pg_l = SDIV(cur_obj).powergroups)
        rem_pg_from_player(cur_obj, pg_l->power_group);
      if (DPBITS(cur_obj))
        mush_free(DPBITS(cur_obj), "POWER_SPOT");
      DPBITS(cur_obj) = NULL;
    }
}

/* yescode_str() {{{2 - return string of integer ycode */
char *
yescode_str(int ycode)
{
  static char buf[8];
  memset(buf, '\0', 8);
  switch (ycode) {
  case YES:
    strncpy(buf, "Yes", 7);
    break;
  case YESLTE:
    strncpy(buf, "YesLTE", 7);
    break;
  case YESLT:
    strncpy(buf, "YesLT", 7);
    break;
  default:
    strncpy(buf, "No", 7);
  }
  return buf;
}

/* yescode_i() {{{2 - return integer of string ycode */
/* all possibile types of yescodes */
int
yescode_i(char *str)
{
  if (!strcasecmp(str, "YES"))
    return YES;
  else if (!strcasecmp(str, "YESLTE"))
    return YESLTE;
  else if (!strcasecmp(str, "YESLT"))
    return YESLT;
  else if (!strcasecmp(str, "LTE"))
    return YESLTE;
  else if (!strcasecmp(str, "LT"))
    return YESLT;
  else if (!strcmp(str, "1"))
    return YESLT;
  else if (!strcmp(str, "2"))
    return YESLTE;
  else if (!strcmp(str, "3"))
    return YES;
  else
    return NO;
}

/* dpbit_match() {{{2 */
int
dpbit_match(div_pbits p1, div_pbits p2)
{
  int i;

  if (!p1 || !p2)
    return 0;

  for (i = 0; i < (DP_BYTES * 8); i++)
    if (DPBIT_ISSET(p1, i) && DPBIT_ISSET(p2, i))
      return 1;
  /* if p1 is bzero'd let 'em slide */
  for (i = 0; i < DP_BYTES; i++)
    if (p1[i] != 0)
      return 0;
  return 1;
}

/* string_to_dpbits() {{{2 */
div_pbits
string_to_dpbits(const char *powers)
{
  div_pbits powc;
  POWER *power;
  char *p_buff[BUFFER_LEN / 2];
  char buff[BUFFER_LEN];
  char *p, *t;
  int power_count, power_indx, plevel;

  /* create our spot */
  powc = new_power_bitmask();
  if (!powers || !powers[0])
    return powc;
  strcpy(buff, powers);
  power_count = list2arr(p_buff, BUFFER_LEN / 2, buff, ' ');
  if (!power_count)
    return NULL;
  for (power_indx = 0; power_indx < power_count; power_indx++) {
    t = p_buff[power_indx];
    if ((p = strchr(t, ':'))) {
      *p++ = '\0';
      plevel = yescode_i(p);
    } else
      plevel = YES;
    power = find_power(t);
    if (!power)
      continue;
    else {                      /* Set bits accordingly */
      switch (plevel) {
      case 3:
        DPBIT_SET(powc, power->flag_yes);
      case 2:
        DPBIT_SET(powc, power->flag_lte);
      case 1:
        DPBIT_SET(powc, power->flag_lt);
        break;
      default:
        /* Make sure nothing is set. */
        RESET_POWER_DP(powc, power);
        break;
      }
    }
  }
  return powc;
}

/* convert_object_powers() {{{2 - Convert old powers to new powers & guest power will assign them guest level
 */

void
convert_object_powers(dbref dbnum, int opbts)
{
  POWER *power;
  int i;

  /* First Convert All their powerz to new powerz */

  for (i = 0; pconv_ptab[i].Name != NULL; i++)
    if (opbts & pconv_ptab[i].Op) {     /* give them the power in its new 'full yes' form */
      power = find_power(pconv_ptab[i].Name);
      GIVE_DPBIT(dbnum, power->flag_yes);
      GIVE_DPBIT(dbnum, power->flag_lte);
      GIVE_DPBIT(dbnum, power->flag_lt);
    }

  if (opbts & IS_GUEST) {
    SLEVEL(dbnum) = -500;
  }
  if (has_flag_by_name(dbnum, "WIZARD", NOTYPE)) {
    powergroup_db_set(NOTHING, dbnum, "WIZARD", 1);
  } else if (has_flag_by_name(dbnum, "ROYALTY", NOTYPE)) {
    powergroup_db_set(NOTHING, dbnum, "ROYALTY", 1);
  }
}

/* decompile_powers() {{{2 - Do Division & Powers Decompilation */
void
decompile_powers(dbref player, dbref thing, const char *name)
{

  notify_format(player, "@level %s = %d", name, LEVEL(thing));
  /* Add powergroups and extra division crapp to this */

  if (DPBITS(thing) && power_is_zero(DPBITS(thing), DP_BYTES) != 0)
    notify_format(player, "@empower %s = %s", name,
                  division_list_powerz(thing, 0));
}

/* power_add() {{{2 - Add Power Allocation to power table */
static void
power_add(POWER * pow)
{
  ptab_start_inserts(ps_tab.powers);
  ptab_insert(ps_tab.powers, pow->name, pow);
  ptab_end_inserts(ps_tab.powers);

  /* Set flag_lt cause it should be our biggest bit */
  if (pow->flag_lt > ps_tab.powerbits) {
    /* Crossed bite boundary.. make objects power spot bigger */
    if ((pow->flag_lt >> 3) > (ps_tab.powerbits >> 3))
      realloc_object_powers((pow->flag_lt >> 3) + 1);
    ps_tab.powerbits = pow->flag_lt;
  }

  /* Make sure all these bits are set on bits_taken */
  DPBIT_SET(ps_tab.bits_taken, pow->flag_yes);
  DPBIT_SET(ps_tab.bits_taken, pow->flag_lte);
  DPBIT_SET(ps_tab.bits_taken, pow->flag_lt);
}

/* realloc_object_powers() {{{2 - reallocate all objects in database powerbit allocation size */
static void
realloc_object_powers(int size)
{
  POWERGROUP *pgrp;
  char pg_name[BUFFER_LEN];
  int cur_obj;

  ps_tab.bits_taken = (div_pbits) realloc(ps_tab.bits_taken, size);
  *(ps_tab.bits_taken + size - 1) = 0;

  for (cur_obj = 0; cur_obj < db_top; cur_obj++)
    /* Check for */
    if (DPBITS(cur_obj)) {
      DPBITS(cur_obj) = (div_pbits) realloc(DPBITS(cur_obj), size);
      *(DPBITS(cur_obj) + size - 1) = 0;
    }
  /*  Check Powergroups */
  for (pgrp = ptab_firstentry_new(ps_tab.powergroups, pg_name); pgrp;
       pgrp = ptab_nextentry_new(ps_tab.powergroups, pg_name)) {
    if (pgrp->auto_powers) {
      pgrp->auto_powers = (div_pbits) realloc(pgrp->auto_powers, size);
      *(pgrp->auto_powers + size - 1) = 0;
    }
    if (pgrp->max_powers) {
      pgrp->max_powers = (div_pbits) realloc(pgrp->max_powers, size);
      *(pgrp->max_powers + size - 1) = 0;
    }
  }
}

/* new_power_bitmask() {{{2 - create new power bitmask allocation */
div_pbits
new_power_bitmask()
{
  div_pbits power_spot;

  power_spot = (div_pbits) mush_malloc(DP_BYTES, "POWER_SPOT");
  if (!power_spot)
    mush_panic("DPBYTES Power_Spot couldn't allocate!");

  DPBIT_ZERO(power_spot);
  return power_spot;
}

/* new_power() {{{2 - create new power allocation */
static POWER *
new_power(void)
{
  POWER *p = (POWER *) mush_malloc(sizeof(POWER), "power");
  if (!p)
    mush_panic("Unable to allocate memory for a new flag!\n");
  return p;
}

/* has_power() {{{2 - This function checks if a player has a power at all. 
 * Returns the power pointer if so
 */
POWER *
has_power(dbref object, const char *name)
{
  POWER *power;

  power = find_power(name);

  if (God(object))
    return power;

  if (check_power_yescode(DPBITS(object), power) > NO)
    return power;
  else
    return NULL;
}

/* power_is_zero() {{{2 - check to make sure there are no powers set. */
char
power_is_zero(div_pbits pbits, int bytes)
{
  int spot = 0;

  while (spot < bytes) {
    if (pbits[spot] != 0)
      return pbits[spot];
    spot++;
  }
  return 0;
}

/* add_power_type() {{{2 - sugar coated routine to add a power */
POWER *
add_power_type(const char *name, const char *type)
{
  POWER *power;
  int lastbit;
  int PowType_i;

  /* Make sure its not already there */
  if ((power = find_power(name)) != NULL)
    return power;
  lastbit = new_power_bit(0);

  for (PowType_i = 0; powc_list[PowType_i].name; PowType_i++)
    if (!strcmp(type, powc_list[PowType_i].name))
      break;
  if (!powc_list[PowType_i].name)
    return NULL;
  power = new_power();
  power->name = mush_strdup(name, "power name");
  power->powc_chk = powc_list[PowType_i].powc_chk;
  /* Figure out Our Yes/Lte/Lt Values */
  power->flag_yes = lastbit;
  if (powc_list[PowType_i].flag_lte != powc_list[PowType_i].flag_yes)
    lastbit = new_power_bit(lastbit);
  power->flag_lte = lastbit;
  if (powc_list[PowType_i].flag_lt != powc_list[PowType_i].flag_lte)
    lastbit = new_power_bit(lastbit);
  power->flag_lt = lastbit;

  power_add(power);

  return power;
}

/* new_power_bit() {{{2 - Find the next available free bit to use */
static int
new_power_bit(int bit)
{
  int c;

  /* A real simple way out.. */
  if (bit >= ps_tab.powerbits)
    return bit + 1;

  c = bit == 0 ? 1 : bit;

  if (c <= ps_tab.powerbits)
    c++;
  while (c <= ps_tab.powerbits) {
    if (!DPBIT_ISSET(ps_tab.bits_taken, c)) {
      return c;
    }
    c++;
  }
  return c;
}

/* clear_all_powers() {{{2 */
static void
clear_all_powers()
{
  POWER *power;
  char pname[BUFFER_LEN];

  if (ps_tab.powers) {
    for (power = ptab_firstentry_new(ps_tab.powers, pname); power;
         power = ptab_nextentry_new(ps_tab.powers, pname))
      if (!strcmp(pname, power->name)) {
        mush_free((Malloc_t) power->name, "power name");
        mush_free(power, "POWER_SPOT");
        return;
      }
    ptab_free(ps_tab.powers);
  }
  ps_tab.powerbits = 0;
}

/* power_delete() {{{2 */
/* 1 Success, 0 Power Didn't Exist */
static int
power_delete(const char *key)
{
  POWERGROUP *pgrp;
  POWER *power, *alias;
  char pname[BUFFER_LEN];
  int i;

  power = find_power(key);

  if (!power)
    return 0;

  /* Step 1, Remove Aliases */
  for (alias = ptab_firstentry_new(ps_tab.powers, pname); alias;
       alias = ptab_nextentry_new(ps_tab.powers, pname))
    if (strcmp(pname, alias->name))
      ptab_delete(ps_tab.powers, pname);

  /* Step 2, Unset bits on bits_taken */
  RESET_POWER_DP(ps_tab.bits_taken, power);

  /* Step 3, Unset power on every object in db */
  for (i = 0; i < db_top; i++) {
    RESET_POWER(i, power);
  }

  /* Step 4: Unset power on every powergroup */
  for (pgrp = ptab_firstentry_new(ps_tab.powergroups, pname); pgrp;
       pgrp = ptab_nextentry_new(ps_tab.powergroups, pname)) {
    RESET_POWER_DP(pgrp->auto_powers, power);
    RESET_POWER_DP(pgrp->max_powers, power);
  }


  /* Step 5, Remove Power */
  ptab_delete(ps_tab.powers, power->name);
  mush_free((Malloc_t) power->name, "power name");
  mush_free(power, "POWER_SPOT");

  return 1;
}

/* power_add_additional() {{{2 - Add pre-defined powers */
static void
power_add_additional(void)
{
  POWERGROUP *pgrp;
  POWER *power;
  int i;

  for (i = 0; new_power_list[i].name; i++)
    (void) add_power_type(new_power_list[i].name, new_power_list[i].type);

  for (i = 0; power_alias_tab[i].alias; i++) {
    /* First Make sure the alias doesn't exist in there */
    power = find_power(power_alias_tab[i].alias);
    if (power)
      continue;
    /* Now make sure the power actually exists */
    power = find_power(power_alias_tab[i].realname);
    if (!power)
      continue;
    ptab_start_inserts(ps_tab.powers);
    ptab_insert(ps_tab.powers, power_alias_tab[i].alias, power);
    ptab_end_inserts(ps_tab.powers);
  }

  /* Load pre-defined powergroups as well */
  for (i = 0; predefined_powergroups[i].name; i++)
    if ((pgrp = powergroup_add(predefined_powergroups[i].name))) {
      powergroup_empower(NOTHING, pgrp,
                         predefined_powergroups[i].max_powers, 0);
      powergroup_empower(NOTHING, pgrp,
                         predefined_powergroups[i].auto_powers, 1);
    }


}

/* convert_old_cobra_powers() {{{2 */
div_pbits
convert_old_cobra_powers(unsigned char *dp_bytes)
{
  POWER *power;
  div_pbits pbits;
  int pcheck;
  int i;

  pbits = NULL;

  for (i = 0; old_cobra_conv_t[i].name; i++) {
    power = find_power(old_cobra_conv_t[i].name);
    if (!power)
      continue;
    pcheck = check_old_power_ycode(i, dp_bytes);
    if (pcheck > NO && pbits == NULL)
      pbits = new_power_bitmask();
    switch (pcheck) {
    case YES:
      DPBIT_SET(pbits, power->flag_yes);
      break;
    case YESLTE:
      DPBIT_SET(pbits, power->flag_lte);
      break;
    case YESLT:
      DPBIT_SET(pbits, power->flag_lt);
      break;
    }
  }
  return pbits;
}

/* power_goodname() {{{2 -  check if its a good power name
 * 1 - good power
 * 0 - provide flag name
 * -1 - be longer than one char
 * -2 - no spaces
 */
int
power_goodname(char *str)
{
  if (!str || !*str)
    return 0;
  if (strlen(str) == 1)
    return -1;
  if (strchr(str, ' '))
    return -2;
  return 1;
}

/* check_old_power_ycode() {{{2 */
/* Return Yescode Appropriate with old power */
static int
check_old_power_ycode(int pnum, unsigned char *dp_bytes)
{
  if (DPBIT_ISSET(dp_bytes, old_cobra_conv_t[pnum].flag_yes))
    return YES;
  else if (DPBIT_ISSET(dp_bytes, old_cobra_conv_t[pnum].flag_lte))
    return YESLTE;
  else if (DPBIT_ISSET(dp_bytes, old_cobra_conv_t[pnum].flag_lt))
    return YESLT;
  return NO;
}

/* add_to_div_exit_path() {{{2 add division to @sd exit path */
void add_to_div_exit_path(dbref player, dbref div_obj) {
  ATTR *divrcd;
  char *tp_buf[BUFFER_LEN / 2], *tbp, tbuf[BUFFER_LEN];
  int cnt, i;

  divrcd = (ATTR *) atr_get(player, (const char *) "XYXX_DIVRCD");
  tbp = tbuf;

  if(divrcd == NULL) {
    /* simple one, just add a one liner to this guy */
    atr_add(player, "XYXX_DIVRCD", unparse_number(div_obj), GOD, NOTHING);
  } else {
    /* Ok.. First we're gonna have to list2arr to get our boys */
    cnt = list2arr(tp_buf, BUFFER_LEN / 2 , (char *) safe_atr_value(divrcd), ' '); 
    /* Now lets put us back into the list and set it back on the guy */
    safe_str(tp_buf[0], tbuf, &tbp);
    for(i = 1; i < cnt ; i++) {
      safe_chr(' ', tbuf, &tbp);
      safe_str(tp_buf[i], tbuf, &tbp);
    }
    /* Tac the new one on there */
    safe_chr(' ', tbuf, &tbp);
    safe_str(unparse_number(div_obj), tbuf, &tbp);
    *tbp = '\0';
    (void) atr_add(player, "XYXX_DIVRCD", tbuf, GOD, NOTHING);
  }

}

/* do_list_powers() {{{2 list all powers */
void do_list_powers(dbref player, const char *name) {
  POWER *power;
  char tbuf[BUFFER_LEN], pname[BUFFER_LEN];
  char *tbp;

  tbp = tbuf;
  *tbp = '\0';
    /* List Powers */
    for (power = ptab_firstentry_new(ps_tab.powers, pname); power;
         power = ptab_nextentry_new(ps_tab.powers, pname))
      if (!strcmp(power->name, pname) && (!name || !*name || quick_wild(name, power->name))) {
        if (!*tbuf)
          safe_str(pname, tbuf, &tbp);
        else
          safe_format(tbuf, &tbp, ", %s", pname);
      }
    *tbp = '\0';
    notify_format(player, "Powers List: %s", tbuf ? tbuf : "None.");
}

char *list_all_powers(dbref player, const char *name) {
  POWER *power;
  static char buff[BUFFER_LEN];
  char pname[BUFFER_LEN];
  char *bp;

  bp = buff;

  for (power = ptab_firstentry_new(ps_tab.powers, pname); power;
       power = ptab_nextentry_new(ps_tab.powers, pname)) {
    if (!strcmp(power->name, pname)
        && (!name || !*name || quick_wild(name, power->name))) {
      if (bp != buff)
        safe_str(", ", buff, &bp);
      safe_str(pname, buff, &bp);
    }
  }

  *bp = '\0';

  return buff;
}


/* Vim Folds Hint
 * vim:ts=18  fdm=marker 
 */
