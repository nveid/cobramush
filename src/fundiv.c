/* fundiv.c - 3/19/02 RLB created to give division functions their own place
 */
#include "copyrite.h"

#include "config.h"
#ifdef I_STRING
#include <string.h>
#else
#include <strings.h>
#endif
#include "conf.h"
#include "externs.h"
#include "division.h"
#include "dbdefs.h"
#include "flags.h"

#include "match.h"
#include "parse.h"
#include "boolexp.h"
#include "command.h"
#include "game.h"
#include "mushdb.h"
#include "privtab.h"
#ifdef MEM_CHECK
#include "memcheck.h"
#endif
#include "lock.h"
#include "log.h"
#include "attrib.h"
#include "confmagic.h"

#ifdef WIN32
#pragma warning( disable : 4761)        /* NJG: disable warning re conversion */
#endif

#ifdef CHAT_SYSTEM
#include "extchat.h"
extern CHAN *channels;
#endif

/* From division.c */
extern int powergroup_has(dbref, POWERGROUP *);

extern POWERSPACE ps_tab;

FUNCTION(fun_level)
{
  dbref obj;
  int l;

  obj = match_thing(executor, args[0]);

  if (obj == NOTHING) {
    safe_str("#-1 BAD OBJECT", buff, bp);
    return;
  }

  if (args[1]) {
    l = division_level(executor, obj, atoi(args[1]));
    if (l > 0)
      safe_format(buff, bp, "%d", LEVEL(obj));
    else
      safe_str("#-1 PERMISSION DENIED", buff, bp);
    return;
  }
  safe_integer(LEVEL(obj), buff, bp);
}


/* Side-Effect, returns nothing */
FUNCTION(fun_empower)
{
  dbref target;

  target = match_thing(executor, args[0]);

  if (target == NOTHING)
    return;

  division_empower(executor, target, args[1]);
}

FUNCTION(fun_divscope)
{
  dbref obj1, obj2;
/* check if arg[1] is in divscope of arg[0] */
  obj1 = match_thing(executor, args[0]);
  if (obj1 == NOTHING) {
    safe_str("#-1", buff, bp);
    return;
  }
  if (nargs > 1) {
    obj2 = match_thing(executor, args[1]);
    if (obj2 == NOTHING) {
      safe_str("#-1", buff, bp);
      return;
    }
    safe_format(buff, bp, "%d", div_inscope(obj1, obj2));
  } else
    safe_format(buff, bp, "%d", div_inscope(executor, obj1));
}

FUNCTION(fun_division)
{
  dbref obj1;

  obj1 = match_thing(executor, args[0]);
  if (obj1 == NOTHING) {
    safe_str(unparse_dbref(obj1), buff, bp);
  } else {
    if (!IsDivision(SDIV(obj1).object))
      SDIV(obj1).object = -1;
    safe_str(unparse_dbref(SDIV(obj1).object), buff, bp);
  }
}

FUNCTION(fun_hasdivpower)
{
  dbref obj1;
  POWER *power;
  int level, levc;

  obj1 = match_thing(executor, args[0]);

  if (obj1 == NOTHING) {
    safe_str("#-1 NO SUCH OBJECT", buff, bp);
    return;
  }
  if (!Can_Examine(executor, obj1)) {
    safe_str("#-1 PERMISSION DENIED", buff, bp);
    return;
  }

  power = find_power(args[1]);
  if (!power) {
    safe_str("#-1 NO SUCH POWER", buff, bp);
    return;
  }
  level = God(obj1) ? YES : check_power_yescode(DPBITS(obj1), power);
  if (nargs > 2) {
    levc = yescode_i(args[2]);
    if (level == levc)
      safe_chr('1', buff, bp);
    else
      safe_chr('0', buff, bp);
    return;
  }
  if (!strcasecmp(called_as, "haspower"))       /* for compatibility */
    safe_chr(level > NO ? '1' : '0', buff, bp);
  else
    safe_format(buff, bp, "%d", level);

}

/* Check if a powergroup has a power */
/* syntax: pghaspower(powergroup,auto/max,power[,level]) */
FUNCTION(fun_pghaspower)
{
  POWERGROUP *pgrp;
  POWER *power;
  int level, levc;
  int auto_chk = 0;

  if (!strcasecmp(args[1], "auto"))
    auto_chk = 1;
  else if (!!strcasecmp(args[1], "max")) {
    safe_str("#-1 POWERFIELD NOT SPECIFIED", buff, bp);
    return;
  }

  pgrp = powergroup_find(args[0]);

  if (!pgrp) {
    safe_str("#-1 NO SUCH POWERGROUP", buff, bp);
    return;
  }

  power = find_power(args[2]);
  if (!power) {
    safe_str("#-1 NO SUCH POWER", buff, bp);
    return;
  }
  level =
      check_power_yescode(auto_chk ? pgrp->auto_powers : pgrp->max_powers,
                          power);
  if (nargs > 3) {
    levc = yescode_i(args[3]);
    if (level == levc)
      safe_chr('1', buff, bp);
    else
      safe_chr('0', buff, bp);
    return;
  }

  safe_format(buff, bp, "%d", level);
}

/* Syntax: pgpowers(powergroup,auto/max) */
FUNCTION(fun_pgpowers)
{
  POWERGROUP *pgrp;
  POWER *power;
  char pname[BUFFER_LEN];
  char *tbp;
  int yescode;
  int auto_chk = 0;

  if (!strcasecmp(args[1], "auto"))
    auto_chk = 1;
  else if (!!strcasecmp(args[1], "max")) {
    safe_str("#-1 POWERFIELD NOT SPECIFIED", buff, bp);
    return;
  }

  pgrp = powergroup_find(args[0]);

  if (!pgrp) {
    safe_str("#-1 NO SUCH POWERGROUP", buff, bp);
    return;
  }


  tbp = *bp;
  for (power = ptab_firstentry_new(ps_tab.powers, pname); power;
       power = ptab_nextentry_new(ps_tab.powers, pname))
    if (!strcmp(pname, power->name)) {
      yescode =
          check_power_yescode((auto_chk ? pgrp->auto_powers : pgrp->
                               max_powers), power);
      if (yescode > NO) {
        if (*tbp != **bp)
          safe_chr(' ', buff, bp);
        safe_str(pname, buff, bp);
        safe_format(buff, bp, ":%d", yescode);

      }
    }
}


/* Syntax: haspowergroup(<object> , <powergroup>) -> check to see if object has powergroup
 */
FUNCTION(fun_haspowergroup)
{
  POWERGROUP *pgrp;
  dbref obj;

  obj = match_thing(executor, args[0]);

  if (!GoodObject(obj) || IsGarbage(obj))
    safe_str(T(e_notvis), buff, bp);
  else {
    pgrp = powergroup_find(args[1]);
    if (!pgrp)
      safe_str("#-1 NO SUCH POWERGROUP", buff, bp);
    else {
      if (!powergroup_has(obj, pgrp))
        safe_chr('0', buff, bp);
      else
        safe_chr('1', buff, bp);
    }
  }
}

/* Syntax: powergroups(<object>) -> return powergroups on object
 *         powergroups()  -> return all powergroups
 */

FUNCTION(fun_powergroups)
{
  dbref obj;

  if (nargs > 0) {
    obj = match_thing(executor, args[0]);

    if (!GoodObject(obj) || IsGarbage(obj))
      safe_str(T(e_notvis), buff, bp);
    else {
      safe_str(powergroups_list_on(obj, 0), buff, bp);
    }
  } else {
    safe_str((const char *) powergroups_list(executor, 0), buff, bp);
  }
}

FUNCTION(fun_empire)
{
  dbref start;
  dbref end;
  dbref last;

  start = match_thing(executor, args[0]);

  if (!GoodObject(start)) {
    safe_str("#-1 BAD OBJECT", buff, bp);
    return;
  }
  if (!IsDivision(start)) {
    if (!IsDivision(SDIV(start).object)) {
      safe_str("#-1 INVALID DIVISION", buff, bp);
      return;
    }
    start = SDIV(start).object;
  }

  for (last = end = args[1] ? SDIV(start).object : start;
       GoodObject(end) && IsDivision(end)
       && !has_flag_by_name(end, "EMPIRE", TYPE_DIVISION);
       last = end, end = SDIV(end).object);
  if (!has_flag_by_name(end, "EMPIRE", TYPE_DIVISION)) {
    if (end == NOTHING && IsDivision(last))
      safe_str(unparse_dbref(last), buff, bp);
    else
      safe_str(unparse_dbref(start), buff, bp);
  } else
    safe_str(unparse_dbref(end), buff, bp);
}

FUNCTION(fun_updiv)
{
  dbref obj1, divi;
  int space = 1;



  obj1 = match_thing(executor, args[0]);
  if (obj1 == NOTHING) {
    safe_str("#-1", buff, bp);
    return;
  }
  if (IsDivision(obj1))
    safe_str(unparse_dbref(obj1), buff, bp);
  else
    space = 0;
  divi = SDIV(obj1).object;
  while (divi != NOTHING && GoodObject(divi) && IsDivision(divi)) {
    if (space)
      safe_chr(' ', buff, bp);
    else
      space = 1;
    safe_str(unparse_dbref(divi), buff, bp);
    divi = SDIV(divi).object;
  }
}

FUNCTION(fun_indivall)
{
  dbref cur_obj, obj;
  int type, space = 0;

  obj = match_thing(executor, args[0]);
  if (!GoodObject(obj) || !IsDivision(obj)) {
    safe_str("#-1 INVALID DIVISION", buff, bp);
    return;
  }
  if (nargs > 1) {
    switch (*args[1]) {
    case 'R':
    case 'r':
      type = TYPE_ROOM;
      break;
    case 'T':
    case 't':
      type = TYPE_THING;
      break;
    case 'P':
    case 'p':
      type = TYPE_PLAYER;
      break;
    case 'D':
    case 'd':
      type = TYPE_DIVISION;
      break;
    case 'E':
    case 'e':
      type = TYPE_EXIT;
      break;
    case 'N':
    case 'n':
      type = NOTYPE;
      break;
    default:
      type = TYPE_PLAYER;
    }
  } else {
    if (!strcmp(called_as, "DOWNDIV"))
      type = TYPE_DIVISION;
    else
      type = TYPE_PLAYER;
  }
  for (cur_obj = 0; cur_obj < db_top; cur_obj++)
    if ((Typeof(cur_obj) == type || type == NOTYPE)
        && (div_inscope(obj, cur_obj) && SDIV(cur_obj).object != -1)) {
      if (space)
        safe_chr(' ', buff, bp);
      else
        space = 1;
      safe_str(unparse_dbref(cur_obj), buff, bp);
    }
}

FUNCTION(fun_indiv)
{
  dbref cur_obj, obj;
  int type;
  int space = 0;

  obj = match_thing(executor, args[0]);
  if (!GoodObject(obj) || !IsDivision(obj)) {
    safe_str("#-1 INVALID DIVISION", buff, bp);
    return;
  }

  if (nargs > 1) {
    switch (*args[1]) {
    case 'R':
    case 'r':
      type = TYPE_ROOM;
      break;
    case 'T':
    case 't':
      type = TYPE_THING;
      break;
    case 'P':
    case 'p':
      type = TYPE_PLAYER;
      break;
    case 'D':
    case 'd':
      type = TYPE_DIVISION;
      break;
    case 'E':
    case 'e':
      type = TYPE_EXIT;
      break;
    case 'N':
    case 'n':
      type = NOTYPE;
      break;
    default:
      type = TYPE_PLAYER;
    }
  } else
    type = TYPE_PLAYER;
  for (cur_obj = 0; cur_obj < db_top; cur_obj++)
    if ((Typeof(cur_obj) == type || type == NOTYPE)
        && (SDIV(cur_obj).object == obj)) {
      if (space)
        safe_chr(' ', buff, bp);
      else
        space = 1;
      safe_str(unparse_dbref(cur_obj), buff, bp);
    }
}

FUNCTION(fun_powover)
{
  dbref obj1, obj2;

  obj1 = match_thing(executor, args[0]);
  obj2 = match_thing(executor, args[1]);

  if (!GoodObject(obj1) || !GoodObject(obj2)) {
    safe_str("#-1 BAD OBJECT", buff, bp);
    return;
  }
  safe_chr(div_powover(obj1, obj2, args[2]) ? '1' : '0', buff, bp);
}

FUNCTION(fun_powers)
{
  dbref it;

  it = match_thing(executor, args[0]);
  if (it == NOTHING) {
    safe_str("#-1 UNFINDABLE", buff, bp);
    return;
  }
  if (!Can_Examine(executor, it)) {
    safe_str("#-1 PERMISSION DENIED", buff, bp);
    return;
  }
  safe_str(division_list_powerz(it, 0), buff, bp);

}

/* Retrieve What their primary division is.. If they have XYXX_DIVRCD Set its the first entry, otherwise its simple
 * Division() :)
 */
FUNCTION(fun_primary_division) {
   ATTR *divrcd;
   dbref target;
   int cnt;
   char *tbuf[BUFFER_LEN / 2];


   /* Find Guy First.. */
   target = match_thing(executor, args[0]);
   /* Fetch divrcd */
   divrcd = (ATTR *) atr_get(target, (const char *) "XYXX_DIVRCD");

   if(divrcd == NULL) { /* Its just division() */
      safe_dbref(Division(target), buff, bp);
   } else {
     cnt = list2arr(tbuf, BUFFER_LEN / 2, (char *) safe_atr_value(divrcd), ' ');
     safe_dbref(parse_number(tbuf[0]), buff, bp);
   }
}
