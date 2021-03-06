/**
 * \file fundb.c
 *
 * \brief Dbref-related functions for mushcode.
 *
 *
 */

#include "copyrite.h"

#include "config.h"
#include <string.h>
#include "conf.h"
#include "externs.h"
#include "dbdefs.h"
#include "flags.h"

#include "match.h"
#include "parse.h"
#include "boolexp.h"
#include "command.h"
#include "game.h"
#include "mushdb.h"
#include "privtab.h"
#include "lock.h"
#include "log.h"
#include "attrib.h"
#include "function.h"
#include "confmagic.h"

#ifdef WIN32
#pragma warning( disable : 4761)        /* NJG: disable warning re conversion */
#endif

extern PRIV attr_privs_view[];
static lock_type get_locktype(char *str);
extern struct db_stat_info *get_stats(dbref owner);
static int lattr_helper(dbref player, dbref thing, dbref parent, char const *pattern,
                        ATTR *atr, void *args);

static dbref
 dbwalk(char *buff, char **bp, dbref executor, dbref enactor,
             int type, dbref loc, dbref after, int skipdark,
                     int start, int count, int *retcount);


const char *
do_get_attrib(dbref executor, dbref thing, const char *attrib)
{
  ATTR *a;
  char const *value;

  a = atr_get(thing, strupper(attrib));
  if (a) {
    if (Can_Read_Attr(executor, thing, a)) {
      if (strlen(value = atr_value(a)) < BUFFER_LEN)
        return value;
      else
        return T("#-1 ATTRIBUTE LENGTH TOO LONG");
    }
    return T(e_atrperm);
  }
  a = atr_match(attrib);
  if (a) {
    if (Can_Read_Attr(executor, thing, a))
      return "";
    return T(e_atrperm);
  }
  if (!Can_Examine(executor, thing))
    return T(e_atrperm);
  return "";
}

/** Structure containing arguments for lattr_helper */
struct lh_args {
  int first;            /**< Is this is the first attribute, or later? */
  char *buff;           /**< Buffer to store output */
  char **bp;            /**< Pointer to address of insertion point in buff */
};

/* this function produces a space-separated list of attributes that are
 * on an object.
 */
/* ARGSUSED */
static int
lattr_helper(dbref player __attribute__ ((__unused__)),
             dbref thing __attribute__ ((__unused__)),
             dbref parent __attribute__ ((__unused__)),
             char const *pattern __attribute__ ((__unused__)),
             ATTR *atr, void *args)
{
  struct lh_args *lh = args;
  if (lh->first)
    lh->first = 0;
  else
    safe_chr(' ', lh->buff, lh->bp);
  safe_str(AL_NAME(atr), lh->buff, lh->bp);
  return 1;
}


FUNCTION(fun_ooref) {
        safe_dbref(ooref, buff, bp);
}
/* ARGSUSED */
FUNCTION(fun_lattr)
{
  dbref thing;
  char *pattern;
  struct lh_args lh;

  pattern = strchr(args[0], '/');
  if (pattern)
    *pattern++ = '\0';
  else
    pattern = (char *) "*";     /* match anything */

  thing = match_thing(executor, args[0]);
  if (!GoodObject(thing)) {
    safe_str(T(e_notvis), buff, bp);
    return;
  }
  if (!Can_Examine(executor, thing)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  lh.first = 1;
  lh.buff = buff;
  lh.bp = bp;
  (void) atr_iter_get(executor, thing, pattern, 0, lattr_helper, &lh);
}

/* ARGSUSED */
FUNCTION(fun_nattr)
{
  dbref thing;
  thing = match_thing(executor, args[0]);
  if (!GoodObject(thing)) {
    safe_str(T(e_notvis), buff, bp);
    return;
  }
  if (!Can_Examine(executor, thing)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  safe_integer(AttrCount(thing), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_hasattr)
{
  dbref thing;
  ATTR *a;

  thing = match_thing(executor, args[0]);
  if (!GoodObject(thing)) {
    safe_str(T(e_notvis), buff, bp);
    return;
  }
  if (strchr(called_as, 'P'))
    a = atr_get(thing, upcasestr(args[1]));
  else
    a = atr_get_noparent(thing, upcasestr(args[1]));
  if (a && Can_Read_Attr(executor, thing, a)) {
    if (strchr(called_as, 'V'))
      safe_chr(*AL_STR(a) ? '1' : '0', buff, bp);
    else
      safe_chr('1', buff, bp);
    return;
  } else if (a || !Can_Examine(executor, thing)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  safe_chr('0', buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_get)
{
  dbref thing;
  char *s;

  s = strchr(args[0], '/');
  if (!s) {
    safe_str(T("#-1 BAD ARGUMENT FORMAT TO GET"), buff, bp);
    return;
  }
  *s++ = '\0';
  thing = match_thing(executor, args[0]);
  if (!GoodObject(thing)) {
    safe_str(T(e_notvis), buff, bp);
    return;
  }
  safe_str(do_get_attrib(executor, thing, s), buff, bp);
}

/* Functions like get, but uses the standard way of passing arguments */
/* to a function, and thus doesn't choke on nested functions within.  */

/* ARGSUSED */
FUNCTION(fun_xget)
{
  dbref thing;

  thing = match_thing(executor, args[0]);
  if (!GoodObject(thing)) {
    safe_str(T(e_notvis), buff, bp);
    return;
  }
  safe_str(do_get_attrib(executor, thing, args[1]), buff, bp);
}

/* Functions like get, but includes a default response if the
 * attribute isn't present or is null
 */
/* ARGSUSED */
FUNCTION(fun_default)
{
  dbref thing;
  ATTR *attrib;
  char *dp;
  const char *sp;
  char mstr[BUFFER_LEN];
  int i;
  /* find our object and attribute */
  for (i = 1; i < nargs; i++) {
    mstr[0] = '\0';
    dp = mstr;
    sp = args[i - 1];
    process_expression(mstr, &dp, &sp, executor, caller, enactor, PE_DEFAULT,
                       PT_DEFAULT, pe_info);
    *dp = '\0';
    parse_attrib(executor, mstr, &thing, &attrib);
    if (GoodObject(thing) && attrib && Can_Read_Attr(executor, thing, attrib)) {
      /* Ok, we've got it */
      dp = safe_atr_value(attrib);
      safe_str(dp, buff, bp);
      free(dp);
      return;
    }
  }
  /* We couldn't get it. Evaluate the last arg and return it */
  sp = args[nargs - 1];
  process_expression(buff, bp, &sp, executor, caller, enactor,
                     PE_DEFAULT, PT_DEFAULT, pe_info);
  return;
}

/* ARGSUSED */
FUNCTION(fun_eval)
{
  /* like xget, except pronoun substitution is done */

  dbref thing;
  char *tbuf;
  char const *tp;
  ATTR *a;

  thing = match_thing(executor, args[0]);
  if (!GoodObject(thing)) {
    safe_str(T(e_notvis), buff, bp);
    return;
  }
  a = atr_get(thing, upcasestr(args[1]));
  if (a && Can_Read_Attr(executor, thing, a)) {
    if (!CanEvalAttr(executor, thing, a)) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
    tp = tbuf = safe_atr_value(a);
    add_check("fun_eval.attr_value");
    process_expression(buff, bp, &tp, thing, executor, executor,
                       PE_DEFAULT, PT_DEFAULT, pe_info);
    mush_free((Malloc_t) tbuf, "fun_eval.attr_value");
    return;
  } else if (a || !Can_Examine(executor, thing)) {
    safe_str(T(e_atrperm), buff, bp);
    return;
  }
  return;
}

/* ARGSUSED */
FUNCTION(fun_get_eval)
{
  /* like eval, except uses obj/attr syntax. 2.x compatibility */

  dbref thing;
  char *tbuf, *s;
  char const *tp;
  ATTR *a;

  s = strchr(args[0], '/');
  if (!s) {
    safe_str(T("#-1 BAD ARGUMENT FORMAT TO GET_EVAL"), buff, bp);
    return;
  }
  *s++ = '\0';
  thing = match_thing(executor, args[0]);
  if (!GoodObject(thing)) {
    safe_str(T(e_notvis), buff, bp);
    return;
  }
  a = atr_get(thing, upcasestr(s));
  if (a && Can_Read_Attr(executor, thing, a)) {
    if (!CanEvalAttr(executor, thing, a)) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
    tp = tbuf = safe_atr_value(a);
    add_check("fun_eval.attr_value");
    process_expression(buff, bp, &tp, thing, executor, executor,
                       PE_DEFAULT, PT_DEFAULT, pe_info);
    mush_free((Malloc_t) tbuf, "fun_eval.attr_value");
    return;
  } else if (a || !Can_Examine(executor, thing)) {
    safe_str(T(e_atrperm), buff, bp);
    return;
  }
  return;
}

/* Functions like eval, but includes a default response if the
 * attribute isn't present or is null
 */
/* ARGSUSED */
FUNCTION(fun_edefault)
{
  dbref thing;
  ATTR *attrib;
  char *dp, *sbuf;
  char const *sp;
  char mstr[BUFFER_LEN];
  /* find our object and attribute */
  dp = mstr;
  sp = args[0];
  process_expression(mstr, &dp, &sp, executor, caller, enactor,
                     PE_DEFAULT, PT_DEFAULT, pe_info);
  *dp = '\0';
  parse_attrib(executor, mstr, &thing, &attrib);
  if (GoodObject(thing) && attrib && Can_Read_Attr(executor, thing, attrib)) {
    if (!CanEvalAttr(executor, thing, attrib)) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
    /* Ok, we've got it */
    sp = sbuf = safe_atr_value(attrib);
    add_check("fun_edefault.attr_value");
    process_expression(buff, bp, &sp, thing, executor, executor,
                       PE_DEFAULT, PT_DEFAULT, pe_info);
    mush_free((Malloc_t) sbuf, "fun_edefault.attr_value");
    return;
  }
  /* We couldn't get it. Evaluate args[1] and return it */
  sp = args[1];
  process_expression(buff, bp, &sp, executor, caller, enactor,
                     PE_DEFAULT, PT_DEFAULT, pe_info);
  return;
}

/* ARGSUSED */
FUNCTION(fun_v)
{
  /* handle 0-9, va-vz, n, l, # */

  int c;

  if (args[0][0] && !args[0][1]) {
    switch (c = args[0][0]) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      if (global_eval_context.wenv[c - '0'])
        safe_str(global_eval_context.wenv[c - '0'], buff, bp);
      return;
    case '#':
      /* enactor dbref */
      safe_dbref(enactor, buff, bp);
      return;
    case '@':
      /* caller dbref */
      safe_dbref(caller, buff, bp);
      return;
    case '!':
      /* executor dbref */
      safe_dbref(executor, buff, bp);
      return;
    case 'n':
    case 'N':
      /* enactor name */
      safe_str(Name(enactor), buff, bp);
      return;
    case 'l':
    case 'L':
      /* giving the location does not violate security,
       * since the object is the enactor.  */
      safe_dbref(Location(enactor), buff, bp);
      return;
    case 'c':
    case 'C':
      safe_str(global_eval_context.ccom, buff, bp);
      return;
    }
  }
  safe_str(do_get_attrib(executor, executor, args[0]), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_flags)
{
  dbref thing;
  char *p;
  ATTR *a;
  if (nargs == 0) {
    safe_str(list_all_flags("FLAG", NULL, executor, 0x1), buff, bp);
    return;
  }
  if ((p = strchr(args[0], '/')))
    *p++ = '\0';
  thing = match_thing(executor, args[0]);
  if (!GoodObject(thing)) {
    safe_str(T(e_notvis), buff, bp);
    return;
  }
  if (p) {
    /* Attribute flags, you must be able to read the attribute */
    a = atr_get_noparent(thing, upcasestr(p));
    if (!a || !Can_Read_Attr(executor, thing, a)) {
      safe_str("#-1", buff, bp);
      return;
    }
    safe_str(privs_to_letters(attr_privs_view, AL_FLAGS(a)), buff, bp);
    if (atr_sub_branch(a))
      safe_chr('`', buff, bp);
  } else {
    /* Object flags, visible to all */
    safe_str(unparse_flags(thing, executor), buff, bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_lflags)
{
  dbref thing;
  char *p;
  ATTR *a;
  if (nargs == 0) {
    safe_str(list_all_flags("FLAG", NULL, executor, 0x2), buff, bp);
    return;
  }
  if ((p = strchr(args[0], '/')))
    *p++ = '\0';
  thing = match_thing(executor, args[0]);
  if (!GoodObject(thing)) {
    safe_str(T(e_notvis), buff, bp);
    return;
  }
  if (p) {
    /* Attribute flags, you must be able to read the attribute */
    a = atr_get_noparent(thing, upcasestr(p));
    if (!a || !Can_Read_Attr(executor, thing, a)) {
      safe_str("#-1", buff, bp);
      return;
    }
    safe_str(privs_to_string(attr_privs_view, AL_FLAGS(a)), buff, bp);
  } else {
    /* Object flags, visible to all */
    safe_str(bits_to_string("FLAG", Flags(thing), executor, thing), buff, bp);
  }
}

#ifdef WIN32
#pragma warning( default : 4761)        /* Re-enable conversion warning */
#endif

/* ARGSUSED */
FUNCTION(fun_nextdbref)
{
  if (first_free != NOTHING) {
    safe_dbref(first_free, buff, bp);
  } else {
    safe_dbref(db_top, buff, bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_num)
{
  safe_dbref(match_thing(executor, args[0]), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_rnum)
{
  dbref place = match_thing(executor, args[0]);
  char *name = args[1];
  dbref thing;
  if ((place != NOTHING) &&
      (Can_Examine(executor, place) || (Location(executor) == place) ||
       (enactor == place))) {
    switch (thing = match_result(place, name, NOTYPE, MAT_REMOTE)) {
    case NOTHING:
      safe_str(T(e_match), buff, bp);
      break;
    case AMBIGUOUS:
      safe_str(T("#-1 AMBIGUOUS MATCH"), buff, bp);
      break;
    default:
      safe_dbref(thing, buff, bp);
    }
  } else
    safe_str("#-1", buff, bp);
}

/*
 * fun_lcon, fun_lexits, fun_con, fun_exit, fun_next were all
 * re-coded by d'mike, 7/12/91.
  *
 * The function behavior was changed by Amberyl, to remove what she saw
 * as a security problem, since mortals could get the contents of rooms
 * they didn't control, thus (if they were willing to go through the trouble)
 * they could build a scanner to locate anything they wanted.
 *
 * The functions were completely reorganized and largely unified by Talek,
 * in September 2001, to finally cure persistent problems with next() and
 * con() and exit() returning different results/having different permissions
 * than lcon() and lexits().
 *
 * You can get the complete contents of any room you may examine, regardless
 * of whether or not objects are dark.  You can get the partial contents
 * (obeying DARK/LIGHT/etc.) of your current location or the enactor.
 * You CANNOT get the contents of anything else, regardless of whether
 * or not you have objects in it.
 *
 * The same behavior is true for exits.
 *
 * The lvcon/lvexits/lvplayers forms work the same way, but never return
 * a dark object or disconnected player, and are useful for parent rooms.
 */

/* Valid types for this function:
 * TYPE_EXIT    (lexits, lvexits, exit, next)
 * TYPE_THING   (lcon, lvcon, con, next) - really means 'things and players'
 * TYPE_PLAYER  (lplayers, lvplayers)
 */

static dbref
dbwalk(char *buff, char **bp, dbref executor, dbref enactor,
       int type, dbref loc, dbref after, int skipdark,
       int start, int count, int *retcount)
{
  dbref result;
  int first;
  int nthing;
  dbref thing;
  int validloc;
  dbref startdb;

  nthing = 0;

  if (!GoodObject(loc)) {
    if (buff)
      safe_str("#-1", buff, bp);
    return NOTHING;
  }
  if (type == TYPE_EXIT) {
    validloc = IsRoom(loc);
    startdb = Exits(loc);
  } else {
    validloc = !IsExit(loc);
    startdb = Contents(loc);
  }

  result = NOTHING;
  if (GoodObject(loc) && validloc &&
      (Can_Examine(executor, loc) || (Location(executor) == loc) ||
       (enactor == loc))) {
    first = 1;
    DOLIST_VISIBLE(thing, startdb, executor) {
      /* Skip if:
       * - We're not checking this type
       * - We can't interact with this thing
       * - We're only listing visual objects, and it's dark
       * - It's a player, not connected, and skipdark is true
       *   Funkily, lvcon() shows unconnected players, so we
       *   use type == TYPE_PLAYER for this check. :-/.
       */
      if (!(Typeof(thing) & type) ||
          !can_interact(thing, executor, INTERACT_SEE) ||
          (skipdark && Dark(thing) && !Light(thing) && !Light(loc)) ||
          ((type == TYPE_PLAYER) && skipdark && !Connected(thing)))
        continue;
      nthing += 1;
      if (count < 1 || (nthing >= start && nthing < start + count)) {
        if (buff) {
          if (first)
            first = 0;
          else
            safe_chr(' ', buff, bp);
          safe_dbref(thing, buff, bp);
        }
      }
      if (result == NOTHING) {
        if (after == NOTHING)
          result = thing;
        if (after == thing)
          after = NOTHING;
      }
      if (retcount)
        *retcount = nthing;
    }
  } else if (buff)
    safe_str("#-1", buff, bp);
  return result;
}

/* ARGSUSED */
FUNCTION(fun_dbwalker)
{
  dbref loc = match_thing(executor, args[0]);
  int start, count;
  int vis = 0;
  int type = 0;
  int result = 0;
  const char *ptr = called_as;
  char *buffptr = buff;
  char **bptr = bp;

  start = count = 0;
  buffptr = buff;
  bptr = bp;

  switch (*(ptr++)) {
  case 'X':
    if (!is_strict_integer(args[1]) || !is_strict_integer(args[2])) {
      safe_str(T(e_int), buff, bp);
      return;
    }
    start = parse_integer(args[1]);
    count = parse_integer(args[2]);
    if (start < 1 || count < 1) {
      safe_str(T("#-1 ARGUMENT OUT OF RANGE"), buff, bp);
      return;
    }
    break;
  case 'N':
    buffptr = NULL;
    bptr = NULL;
    break;
  }

  if (*ptr == 'V') {
    vis = 1;
    ptr++;
  }

  switch (*ptr) {
  case 'D':
    type = TYPE_DIVISION; /* divisions */
    break;
  case 'C':                     /* con */
    type = TYPE_THING | TYPE_PLAYER;
    break;
  case 'T':                     /* things */
    type = TYPE_THING;
    break;
  case 'P':                     /* players */
    type = TYPE_PLAYER;
    break;
  case 'E':                     /* exits */
    type = TYPE_EXIT;
    break;
  default:
    /* This should never be reached ... */
    type = TYPE_THING | TYPE_PLAYER;
  }

  dbwalk(buffptr, bptr, executor, enactor, type, loc, NOTHING,
         vis, start, count, &result);

  if (!buffptr) {
    safe_integer(result, buff, bp);
  }
}


/* ARGSUSED */
FUNCTION(fun_con)
{
  dbref loc = match_thing(executor, args[0]);
  safe_dbref(dbwalk (NULL, NULL, executor, enactor, TYPE_THING | TYPE_PLAYER, loc, 
        NOTHING, 0, 0, 0, NULL), buff, bp);

}

/* ARGSUSED */
FUNCTION(fun_exit)
{
  dbref loc = match_thing(executor, args[0]);
    safe_dbref(dbwalk (NULL, NULL, executor, enactor, TYPE_EXIT, loc, NOTHING, 0, 0, 0,
                                    NULL), buff, bp);

}

/* ARGSUSED */
FUNCTION(fun_next)
{
  dbref it = match_thing(executor, args[0]);

  if (GoodObject(it)) {
    switch (Typeof(it)) {
    case TYPE_EXIT:
      safe_dbref(dbwalk
                 (NULL, NULL, executor, enactor, TYPE_EXIT, Source(it), it, 0,
                  0, 0, NULL), buff, bp);
      break;
    case TYPE_THING:
    case TYPE_PLAYER:
      safe_dbref(dbwalk
                 (NULL, NULL, executor, enactor, TYPE_THING | TYPE_PLAYER,
                  Location(it), it, 0, 0, 0, NULL), buff, bp);
      break;
    default:
      safe_str("#-1", buff, bp);
      break;
    }
  } else
    safe_str("#-1", buff, bp);
}


/* ARGSUSED */
FUNCTION(fun_entrances)
{
  /* All args are optional.
   * The first argument is the dbref to check (default = this room)
   * The second argument to this function is a set of characters:
   * (a)ll (default), (e)xits, (t)hings, (p)layers, (r)ooms
   * The third and fourth args limit the range of dbrefs (default=0,db_top)
   */
  dbref where = Location(executor);
  dbref low = 0;
  dbref high = db_top - 1;
  dbref counter;
  dbref entrance;
  int found;
  int exd, td, pd, rd;          /* what we're looking for */
  char *p;

  if (!command_check_byname(executor, "@entrances")) {
    safe_str(T(e_perm), buff, bp);
    return;
  }

  if (nargs > 0)
    where = match_result(executor, args[0], NOTYPE, MAT_EVERYTHING);
  if (!GoodObject(where)) {
    safe_str(T("#-1 INVALID LOCATION"), buff, bp);
    return;
  }
  exd = td = pd = rd = 0;
  if (nargs > 1) {
    if (!args[1] || !*args[1]) {
      safe_str(T("#-1 INVALID SECOND ARGUMENT"), buff, bp);
      return;
    }
    p = args[1];
    while (*p) {
      switch (*p) {
      case 'a':
      case 'A':
        exd = td = pd = rd = 1;
        break;
      case 'e':
      case 'E':
        exd = 1;
        break;
      case 't':
      case 'T':
        td = 1;
        break;
      case 'p':
      case 'P':
        pd = 1;
        break;
      case 'r':
      case 'R':
        rd = 1;
        break;
      default:
        safe_str(T("#-1 INVALID SECOND ARGUMENT"), buff, bp);
        return;
      }
      p++;
    }
  }
  if (!exd && !td && !pd && !rd) {
    exd = td = pd = rd = 1;
  }
  if (nargs > 2) {
    if (is_integer(args[2])) {
      low = parse_integer(args[2]);
    } else if (is_dbref(args[2])) {
      low = parse_dbref(args[2]);
    } else {
      safe_str(T(e_ints), buff, bp);
      return;
    }
  }
  if (nargs > 3) {
    if (is_integer(args[3])) {
      high = parse_integer(args[3]);
    } else if (is_dbref(args[3])) {
      high = parse_dbref(args[3]);
    } else {
      safe_str(T(e_ints), buff, bp);
      return;
    }
  }
  if (!GoodObject(low))
    low = 0;
  if (!GoodObject(high))
    high = db_top - 1;

  if (!controls(executor, where) && !CanSearch(executor, where)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  if (!payfor(executor, FIND_COST)) {
    notify_format(executor, T("You don't have %d %s to do that."),
                  FIND_COST, ((FIND_COST == 1) ? MONEY : MONIES));
    safe_str("#-1", buff, bp);
    return;
  }
  /* Ok, do the real work */
  found = 0;
  for (counter = low; counter <= high; counter++) {
    if (controls(executor, where) || controls(executor, counter)) {
      if ((exd && IsExit(counter)) ||
          (td && IsThing(counter)) ||
          (pd && IsPlayer(counter)) || (rd && IsRoom(counter))) {
        if (Mobile(counter))
          entrance = Home(counter);
        else
          entrance = Location(counter);
        if (entrance == where) {
          if (!found)
            found = 1;
          else
            safe_chr(' ', buff, bp);
          safe_dbref(counter, buff, bp);
        }
      }
    }
  }
  return;
}

/* ARGSUSED */
FUNCTION(fun_nearby)
{
  dbref obj1 = match_thing(executor, args[0]);
  dbref obj2 = match_thing(executor, args[1]);

  if (!controls(executor, obj1) && !controls(executor, obj2)
      && !(CanSee(executor, obj1) && CanSee(executor, obj2))
      && !nearby(executor, obj1) && !nearby(executor, obj2)) {
    safe_str(T("#-1 NO OBJECTS CONTROLLED"), buff, bp);
    return;
  }
  if (!GoodObject(obj1) || !GoodObject(obj2)) {
    safe_str("#-1", buff, bp);
    return;
  }
  safe_chr(nearby(obj1, obj2) ? '1' : '0', buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_controls)
{
  dbref it = match_thing(executor, args[0]);
  dbref thing = match_thing(executor, args[1]);

  if (!GoodObject(it))
    safe_str(T("#-1 ARG1 NOT FOUND"), buff, bp);
  else if (!GoodObject(thing))
    safe_str(T("#-1 ARG2 NOT FOUND"), buff, bp);
  else if (!(controls(executor, it) || controls(executor, thing)
             || CanSee(executor, it) || CanSee(executor, thing)))
    safe_str(T(e_perm), buff, bp);
  else
    safe_chr(controls(it, thing) ? '1' : '0', buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_visible)
{
  /* check to see if we have an object-attribute pair. If we don't,
   * then we want to know about the whole object; otherwise, we're
   * just interested in a single attribute.
   * If we encounter an error, we return 0 rather than an error
   * code, since if it doesn't exist, it obviously can't see 
   * anything or be seen.
   */

  dbref it = match_thing(executor, args[0]);
  dbref thing;
  char *name;
  ATTR *a;

  if (!GoodObject(it)) {
    safe_str(T(e_notvis), buff, bp);
    return;
  }
  if ((name = strchr(args[1], '/')))
    *name++ = '\0';
  thing = match_thing(executor, args[1]);
  if (!GoodObject(thing)) {
    safe_chr('0', buff, bp);
    return;
  }
  if (name) {
    a = atr_get(thing, upcasestr(name));
    safe_chr((a && Can_Read_Attr(it, thing, a)) ? '1' : '0', buff, bp);
  } else {
    safe_boolean(Can_Examine(it, thing), buff, bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_type)
{
  dbref it = match_thing(executor, args[0]);
  if (!GoodObject(it)) {
    safe_str(T(e_notvis), buff, bp);
    return;
  }
  switch (Typeof(it)) {
  case TYPE_PLAYER:
    safe_str("PLAYER", buff, bp);
    break;
  case TYPE_THING:
    safe_str("THING", buff, bp);
    break;
  case TYPE_EXIT:
    safe_str("EXIT", buff, bp);
    break;
  case TYPE_ROOM:
    safe_str("ROOM", buff, bp);
    break;
  case TYPE_DIVISION:
    safe_str("DIVISION", buff, bp);
    break;
  case TYPE_GARBAGE:
    safe_str("GARBAGE", buff, bp);
    break;
  default:
    safe_str("WEIRD OBJECT", buff, bp);
    do_rawlog(LT_ERR, T("WARNING: Weird object #%d (type %d)\n"), it,
              Typeof(it));
  }
}

/* ARGSUSED */
FUNCTION(fun_hasflag)
{
  dbref thing;
  ATTR *attrib;
  int f;

  if (strchr(args[0], '/')) {
    parse_attrib(executor, args[0], &thing, &attrib);
    if (!attrib)
      safe_str("#-1", buff, bp);
    else if ((f = string_to_atrflag(executor, args[1])) < 0)
      safe_str("#-1", buff, bp);
    else
      safe_boolean(AL_FLAGS(attrib) & f, buff, bp);
  } else {
    thing = match_thing(executor, args[0]);
    if (!GoodObject(thing))
      safe_str(T(e_notvis), buff, bp);
    else
      safe_boolean(sees_flag(executor, thing, args[1]), buff, bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_hastype)
{
  char *r, *s;
  int found = 0;
  dbref it = match_thing(executor, args[0]);
  if (!GoodObject(it)) {
    safe_str(T(e_notvis), buff, bp);
    return;
  }
  s = trim_space_sep(args[1], ' ');
  r = split_token(&s, ' ');
  do {
    switch (*r) {
    case 'r':
    case 'R':
      if (IsRoom(it))
      found = 1;
      break;
    case 'e':
    case 'E':
      if (IsExit(it))
      found = 1;
      break;
    case 'p':
    case 'P':
      if (IsPlayer(it))
      found = 1;
      break;
    case 't':
    case 'T':
      if (IsThing(it))
      found = 1;
      break;
    case 'd':
    case 'D':
      if (IsDivision(it))
      found = 1;
      break;
    case 'g':
    case 'G':
      if (IsGarbage(it))
      found = 1;
      break;
    default:
      safe_str(T("#-1 NO SUCH TYPE"), buff, bp);
      return;
    }
    if (found) {
      safe_boolean(found, buff, bp);
      return;
    }
  } while ((r = split_token(&s, ' ')));
  safe_boolean(0, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_orflags)
{
  dbref it = match_thing(executor, args[0]);
  if (!strcmp(called_as, "ORPOWERS"))
    safe_boolean(flaglist_check("POWER", executor, it, args[1], 0), buff, bp);
  else
    safe_boolean(flaglist_check("FLAG", executor, it, args[1], 0), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_andflags)
{
  dbref it = match_thing(executor, args[0]);
  if (!strcmp(called_as, "ANDPOWERS"))
    safe_boolean(flaglist_check("POWER", executor, it, args[1], 1), buff, bp);
  else
    safe_boolean(flaglist_check("FLAG", executor, it, args[1], 1), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_orlflags)
{
  dbref it = match_thing(executor, args[0]);
  if (!strcmp(called_as, "ORLPOWERS"))
    safe_boolean(flaglist_check_long("POWER", executor, it, args[1], 0), buff,
                 bp);
  else
    safe_boolean(flaglist_check_long("FLAG", executor, it, args[1], 0), buff,
                 bp);
}

/* ARGSUSED */
FUNCTION(fun_andlflags)
{
  dbref it = match_thing(executor, args[0]);
  if (!strcmp(called_as, "ANDLPOWERS"))
    safe_boolean(flaglist_check_long("POWER", executor, it, args[1], 1), buff,
                 bp);
  else
    safe_boolean(flaglist_check_long("FLAG", executor, it, args[1], 1), buff,
                 bp);
}

static lock_type
get_locktype(str)
    char *str;
{
  /* figure out a lock type */

  if (!str || !*str)
    return Basic_Lock;
  return upcasestr(str);
}

/* ARGSUSED */
FUNCTION(fun_lock)
{
  dbref it;
  char *ltype = NULL;
  lock_type real_ltype;

  if ((ltype = strchr(args[0], '/'))) {
    *ltype++ = '\0';
    upcasestr(ltype);
  }

  real_ltype = get_locktype(ltype);

  it = match_thing(executor, args[0]);

  if (nargs == 2) {
    if (!command_check_byname(executor, "@lock") || fun->flags & FN_NOSIDEFX) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
    if (FUNCTION_SIDE_EFFECTS)
      do_lock(executor, args[0], args[1], ltype);
    else {
      safe_str(T(e_disabled), buff, bp);
      return;
    }
  }
  if (GoodObject(it) && (real_ltype != NULL)
      && Can_Read_Lock(executor, it, real_ltype)) {
    safe_str(unparse_boolexp(executor, getlock(it, real_ltype), UB_DBREF),
             buff, bp);
    return;
  }
  safe_str("#-1", buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_elock)
{
  char *p;
  lock_type ltype;
  dbref it;
  dbref victim = match_thing(executor, args[1]);

  p = strchr(args[0], '/');
  if (p)
    *p++ = '\0';

  it = match_thing(executor, args[0]);
  ltype = get_locktype(p);

  if (!GoodObject(it) || (ltype == NULL) || !Can_Read_Lock(executor, it, ltype)) {
    safe_str("#-1", buff, bp);
    return;
  }
  if (Can_Locate(executor, victim))
    safe_boolean(eval_lock(victim, it, ltype), buff, bp);
  else
    safe_str("#-1", buff, bp);
  return;
}

/* ARGSUSED */
FUNCTION(fun_findable)
{
  dbref obj = match_thing(executor, args[0]);
  dbref victim = match_thing(executor, args[1]);

  if (!GoodObject(obj))
    safe_str(T("#-1 ARG1 NOT FOUND"), buff, bp);
  else if (!GoodObject(victim))
    safe_str(T("#-1 ARG2 NOT FOUND"), buff, bp);
  else if (!CanSee(executor, obj) && !controls(executor, obj) &&
           !controls(executor, victim))
    safe_str(T(e_perm), buff, bp);
  else
    safe_boolean(Can_Locate(obj, victim), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_loc)
{
  dbref it = match_thing(executor, args[0]);
  if (GoodObject(it) && Can_Locate(executor, it))
    safe_dbref(Location(it), buff, bp);
  else
    safe_str("#-1", buff, bp);
}

FUNCTION(fun_loctree) {
  dbref it = match_thing(executor, args[0]);
  dbref loc;

  if (GoodObject(it) && Can_Locate(executor, it)) {
        for(loc = it; GoodObject(Location(loc)) ; loc = Location(loc)) {
          if(loc != it)
            safe_chr(' ', buff, bp);
          safe_dbref(Location(loc), buff, bp);
        }
  }
   else safe_str("#-1", buff, bp);
        
}

/* ARGSUSED */
FUNCTION(fun_objid)
{
  dbref it = match_thing(executor, args[0]);

  if (GoodObject(it)) {
    safe_dbref(it, buff, bp);
    safe_chr(':', buff, bp);
    safe_integer(CreTime(it), buff, bp);
  } else
    safe_str(T(e_notvis), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_ctime)
{
  dbref it = match_thing(executor, args[0]);

  if (GoodObject(it) && !IsGarbage(it))
    safe_str(show_time(CreTime(it), 0), buff, bp);
  else
    safe_str(T(e_notvis), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_mtime)
{
  dbref it = match_thing(executor, args[0]);
  if (!GoodObject(it) || IsGarbage(it))
    safe_str(T(e_notvis), buff, bp);
  else if (!Can_Examine(executor, it) || IsPlayer(it))
    safe_str(T(e_perm), buff, bp);
  else
    safe_str(show_time(ModTime(it), 0), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_where)
{
  /* finds the "real" location of an object */

  dbref it = match_thing(executor, args[0]);
  if (GoodObject(it) && Can_Locate(executor, it))
    safe_dbref(where_is(it), buff, bp);
  else
    safe_str("#-1", buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_room)
{
  dbref it = match_thing(executor, args[0]);

  if (!GoodObject(it))
    safe_str(T(e_notvis), buff, bp);
  else if (!Can_Locate(executor, it))
    safe_str(T(e_perm), buff, bp);
  else {
    dbref room = absolute_room(it);
    if (!GoodObject(room)) {
      safe_strl("#-1", 3, buff, bp);
      return;
    }
    safe_dbref(room, buff, bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_rloc)
{
  int i;
  int deep = parse_integer(args[1]);
  dbref it = match_thing(executor, args[0]);

  if (deep > 20)
    deep = 20;
  if (deep < 0)
    deep = 0;

  if (!GoodObject(it))
    safe_str(T(e_notvis), buff, bp);
  else if (!Can_Locate(executor, it))
    safe_str(T(e_perm), buff, bp);
  else {
    for (i = 0; i < deep; i++) {
      if (!GoodObject(it) || IsRoom(it))
        break;
      it = Location(it);
    }
    safe_dbref(it, buff, bp);
    return;
  }
}

/* ARGSUSED */
FUNCTION(fun_zone)
{
  dbref it;

  if (nargs == 2) {
    if (!command_check_byname(executor, "@chzone") || fun->flags & FN_NOSIDEFX) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
    if (FUNCTION_SIDE_EFFECTS)
      (void) do_chzone(executor, args[0], args[1], 1);
    else {
      safe_str(T(e_disabled), buff, bp);
      return;
    }
  }
  it = match_thing(executor, args[0]);
  if (!GoodObject(it))
    safe_str(T(e_notvis), buff, bp);
  else if (!Can_Examine(executor, it))
    safe_str(T(e_perm), buff, bp);
  else
    safe_dbref(Zone(it), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_parent)
{
  dbref it;

  if (nargs == 2) {
    if (!command_check_byname(executor, "@parent") || fun->flags & FN_NOSIDEFX) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
    if (FUNCTION_SIDE_EFFECTS)
      do_parent(executor, args[0], args[1]);
    else {
      safe_str(T(e_disabled), buff, bp);
      return;
    }
  }
  it = match_thing(executor, args[0]);
  if (!GoodObject(it))
    safe_str(T(e_notvis), buff, bp);
  else if (!Can_Examine(executor, it))
    safe_str(T(e_perm), buff, bp);
  else
    safe_dbref(Parent(it), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_lparent)
{
  dbref it;
  dbref par;

  it = match_thing(executor, args[0]);
  if (!GoodObject(it)) {
    safe_str(T(e_notvis), buff, bp);
    return;
  }
  safe_dbref(it, buff, bp);
  par = Parent(it);
  while (GoodObject(par) && Can_Examine(executor, it)) {
    if (safe_chr(' ', buff, bp))
      break;
    safe_dbref(par, buff, bp);
    it = par;
    par = Parent(par);
  }
}

/* ARGSUSED */
FUNCTION(fun_home)
{
  dbref it = match_thing(executor, args[0]);
  if (!GoodObject(it))
    safe_str(T(e_notvis), buff, bp);
  else if (!Can_Examine(executor, it))
    safe_str(T(e_perm), buff, bp);
  else if (IsExit(it))
    safe_dbref(Source(it), buff, bp);
  else if (IsRoom(it))
    safe_dbref(Location(it), buff, bp);
  else
    safe_dbref(Home(it), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_money)
{
  /* Are we asking about something's money? */
  dbref it = match_result(executor, args[0], NOTYPE, MAT_EVERYTHING);

  if (!GoodObject(it)) {
    /* Well, are we asking for the plural/singular for some amount? */
    if (is_integer(args[0])) {
      int a = parse_integer(args[0]);
      if (abs(a) == 1)
        safe_str(MONEY, buff, bp);
      else
        safe_str(MONIES, buff, bp);
    } else {
      /* Guess we're just making a typo or something. */
      safe_str("#-1", buff, bp);
    }
    return;
  } else if (!GoodObject(it)) {
    /* Catch ambiguous matches */
    safe_str("#-1", buff, bp);
    return;
  }
  /* If the thing in question has unlimited money, respond with the
   * max money possible. We don't use the NoPay macro, though, because
   * we want to return the amount of money stored in an object, even
   * if its owner is no_pay. Softcode can check money(owner(XX)) if 
   * they want to allow objects to pay like their owners.
   */
  if (Admin(it) || (NoPay(it)))
    safe_integer(MAX_PENNIES, buff, bp);
  else
    safe_integer(Pennies(it), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_owner)
{
  dbref thing;
  ATTR *attrib;

  if (strchr(args[0], '/')) {
    parse_attrib(executor, args[0], &thing, &attrib);
    if (!GoodObject(thing) || !attrib
        || !Can_Read_Attr(executor, thing, attrib))
      safe_str("#-1", buff, bp);
    else
      safe_dbref(attrib->creator, buff, bp);
  } else {
    thing = match_thing(executor, args[0]);
    if (!GoodObject(thing))
      safe_str(T(e_notvis), buff, bp);
    else
      safe_dbref(Owner(thing), buff, bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_alias)
{
  dbref it;

  it = match_thing(executor, args[0]);
  if (!GoodObject(it))
    safe_str(T(e_notvis), buff, bp);

  /* Support changing alias via function if side-effects are enabled */
  if (nargs == 2) {
    if (!command_check_byname(executor, "ATTRIB_SET")
        || fun->flags & FN_NOSIDEFX) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
    if (!FUNCTION_SIDE_EFFECTS)
      safe_str(T(e_disabled), buff, bp);
    else
      do_set_atr(it, "ALIAS", args[1], executor, 0);
    return;
  } else {
    safe_str(shortalias(it), buff, bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_fullalias)
{
  dbref it = match_thing(executor, args[0]);
  if (GoodObject(it))
    safe_str(fullalias(it), buff, bp);
  else
    safe_str(T(e_notvis), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_name)
{
  dbref it;

  /* Special case for backward compatibility */
  if (nargs == 0)
    return;
  if (nargs == 2) {
    if (!command_check_byname(executor, "@name") || fun->flags & FN_NOSIDEFX) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
    if (FUNCTION_SIDE_EFFECTS)
      do_name(executor, args[0], args[1]);
    else
      safe_str(T(e_disabled), buff, bp);
    return;
  }
  it = match_thing(executor, args[0]);
  if (GoodObject(it))
          /* Quick Fix for most code dealing with know system */
          /* All code.. when the enactor is IC & so is the person they're
           * trying to gt the name of, they will get their 'know name' variant.
           * Only exceptions are A) Code called from the master room
           *                     B) Code called from a division object
           *                     C) The enactor has see_all over the other player & are over the empire levels
           **/
    safe_str(
                    shortname(it)
                    , buff, bp);
  else
    safe_str(T(e_notvis), buff, bp);
}

#ifdef KNOW_SYS
FUNCTION(fun_checkknow) {
        dbref who;
        dbref what;

        who = match_thing(executor, args[0]);
        what = match_thing(executor, args[1]);

        if(!IsPlayer(who) || !IsPlayer(what)) {
                safe_str((e_notvis), buff, bp);
                return;
        }

        safe_chr(check_know(who,what) ? '1' : '0' , buff, bp); 
}
#endif

/* ARGSUSED */
FUNCTION(fun_fullname)
{
  dbref it = match_thing(executor, args[0]);
  if (GoodObject(it))
    safe_str(Name(it), buff, bp);
  else
    safe_str(T(e_notvis), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_accname)
{
  dbref it = match_thing(executor, args[0]);
  if (GoodObject(it))
    safe_str(accented_name(it), buff, bp);
  else
    safe_str(T(e_notvis), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_iname)
{
  dbref it = match_thing(executor, args[0]);
  char tbuf1[BUFFER_LEN];

  if (GoodObject(it)) {
    /* You must either be see_all, control it, or be inside it */
    if (!(controls(executor, it) || CanSee(executor, it) ||
          (Location(executor) == it))) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
    if (nameformat(executor, it, tbuf1,
                   IsExit(it) ? shortname(it) : (char *) accented_name(it)))
      safe_str(tbuf1, buff, bp);
    else if (IsExit(it))
      safe_str(shortname(it), buff, bp);
    else
      safe_str(accented_name(it), buff, bp);
  } else
    safe_str(T(e_notvis), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_pmatch)
{
  dbref target;

  if (*args[0] == '*')
    target = lookup_player(args[0] + 1);
  else if (*args[0] == NUMBER_TOKEN) {
    target = parse_objid(args[0]);
    if (!(GoodObject(target) && IsPlayer(target))) {
      notify(executor, T("No match."));
      safe_str("#-1", buff, bp);
      return;
    } else {
      safe_dbref(target, buff, bp);
      return;
    }
  } else
    target = lookup_player(args[0]);
  if (target == NOTHING) {
    if (*args[0] == '*')
      target = visible_short_page(executor, args[0] + 1);
    else
      target = visible_short_page(executor, args[0]);
  }
  switch (target) {
  case NOTHING:
    notify(executor, T("No match."));
    safe_str("#-1", buff, bp);
    break;
  case AMBIGUOUS:
    notify(executor, T("I'm not sure who you mean."));
    safe_str("#-2", buff, bp);
    break;
  default:
    safe_dbref(target, buff, bp);
  }
}

/* ARGUSED */
FUNCTION(fun_namelist)
{

  int first = 1;
  char *current;
  dbref target;
  const char *start;

  start = args[0];
  while (start && *start) {
    if (!first)
      safe_str(" ", buff, bp);
    first = 0;
    current = next_in_list(&start);
    if (*current == '*')
      current = current + 1;
    target = lookup_player(current);
    if (!GoodObject(target))
      target = visible_short_page(executor, current);
    if (target == NOTHING) {
      safe_str("#-1", buff, bp);
    } else if (target == AMBIGUOUS) {
      safe_str("#-2", buff, bp);
    } else {
      safe_dbref(target, buff, bp);
    }
  }
}

/* ARGSUSED */
FUNCTION(fun_locate)
{
  dbref looker;
  int pref_type;
  dbref item, loc;
  char *p;
  int keys = 0;
  int ambig_ok = 0;
  int force_type = 0;
  long match_flags = 0;

  /* find out what we're matching in relation to */
  looker = match_thing(executor, args[0]);
  if (!GoodObject(looker)) {
    safe_str("#-1", buff, bp);
    return;
  }
  if (!CanSee(executor, looker) && !controls(executor, looker)) {
    safe_str("#-1", buff, bp);
    return;
  }

  /* find out our preferred match type and flags */
  pref_type = 0;
  for (p = args[2]; *p; p++) {
    switch (*p) {
    case 'N':
      pref_type |= NOTYPE;
      break;
    case 'E':
      pref_type |= TYPE_EXIT;
      break;
    case 'P':
      pref_type |= TYPE_PLAYER;
      break;
    case 'R':
      pref_type |= TYPE_ROOM;
      break;
    case 'T':
      pref_type |= TYPE_THING;
      break;
    case 'D':
      pref_type |= TYPE_DIVISION;
      break;
    case 'L':
      keys = 1;
      break;
    case 'F':
      force_type = 1;
      break;
    case '*':
      match_flags |= MAT_EVERYTHING;
      break;
    case 'a':
      match_flags |= MAT_ABSOLUTE;
      break;
    case 'c':
      match_flags |= MAT_CARRIED_EXIT;
      break;
    case 'e':
      match_flags |= MAT_EXIT;
      break;
    case 'h':
      match_flags |= MAT_HERE;
      break;
    case 'i':
      match_flags |= MAT_POSSESSION;
      break;
    case 'l':
      match_flags |= MAT_CONTAINER;
      break;
    case 'm':
      match_flags |= MAT_ME;
      break;
    case 'n':
      match_flags |= MAT_NEIGHBOR;
      break;
    case 'p':
      match_flags |= MAT_PLAYER;
      break;
    case 'z':
      match_flags |= MAT_ENGLISH;
      break;
    case 'X':
      ambig_ok = 1;             /* okay to pick last match */
      break;
    default:
      notify_format(executor, T("I don't understand switch '%c'."), *p);
      break;
    }
  }
  if (!pref_type)
    pref_type = NOTYPE;

  if (keys)
    match_flags = MAT_CHECK_KEYS;

  /* report the results */
  if (!ambig_ok)
    item = match_result(looker, args[1], pref_type, match_flags);
  else
    item = last_match_result(looker, args[1], pref_type, match_flags);

  if (!GoodObject(item)) {
    safe_dbref(item, buff, bp);
    return;
  }

  if (force_type && !(Typeof(item) & pref_type)) {
    safe_dbref(NOTHING, buff, bp);
    return;
  }

  /* To locate it, you must either be able to examine its location
   * or be able to see the item.
   */
  loc = Location(item);
  if (GoodObject(loc)) {
    if (Can_Examine(executor, loc))
      safe_dbref(item, buff, bp);
    else if (can_interact(item, executor, INTERACT_SEE)
             && (!DarkLegal(item) || Light(loc) || Light(item)))
      safe_dbref(item, buff, bp);
    else
      safe_dbref(NOTHING, buff, bp);
  } else {
    if (can_interact(item, executor, INTERACT_SEE)
        && (See_All(executor) || !DarkLegal(item) || Light(item)))
      safe_dbref(item, buff, bp);
    else
      safe_dbref(NOTHING, buff, bp);
  }
  return;
}


/* --------------------------------------------------------------------------
 * Creation functions: CREATE, PCREATE, OPEN, DIG
 */

/* ARGSUSED */
FUNCTION(fun_create)
{
  int cost;

  if (!FUNCTION_SIDE_EFFECTS) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }

  if (!command_check_byname(executor, "@create") || fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  if (nargs == 2)
    cost = parse_integer(args[1]);
  else
    cost = OBJECT_COST;
  safe_dbref(do_create(executor, args[0], cost), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_pcreate)
{
  if (!FUNCTION_SIDE_EFFECTS) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }
  if (!command_check_byname(executor, "@pcreate") || fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  safe_dbref(do_pcreate(executor, args[0], args[1]), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_open)
{
  if (!FUNCTION_SIDE_EFFECTS) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }
  if (!command_check_byname(executor, "@open") || fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  safe_dbref(do_real_open(executor, args[0], args[1], NOTHING), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_dig)
{
  if (!FUNCTION_SIDE_EFFECTS) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }
  if (!command_check_byname(executor, "@dig") || fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  safe_dbref(do_dig(executor, args[0], args, 0), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_clone)
{
  if (!FUNCTION_SIDE_EFFECTS) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }
  if (!command_check_byname(executor, "@clone") || fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  safe_dbref(do_clone(executor, args[0], NULL, 0), buff, bp);
}


/* --------------------------------------------------------------------------
 * Attribute functions: LINK, SET
 */

/* ARGSUSED */
FUNCTION(fun_link)
{
  if (!FUNCTION_SIDE_EFFECTS) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }
  if (!command_check_byname(executor, "@link") || fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  do_link(executor, args[0], args[1], args[2] && args[2] != '\0' ? parse_boolean(args[2]) : 0 );
}

/* ARGSUSED */
FUNCTION(fun_set)
{
  if (!FUNCTION_SIDE_EFFECTS) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }
  if (!command_check_byname(executor, "@set") || fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  do_set(executor, args[0], args[1]);
}

/* ARGSUSED */
FUNCTION(fun_wipe)
{
  if (!FUNCTION_SIDE_EFFECTS) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }
  if (!command_check_byname(executor, "@wipe") || fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  do_wipe(executor, args[0]);
}

/* ARGSUSED */
FUNCTION(fun_attrib_set)
{
  dbref thing;
  char *s;

  if (!FUNCTION_SIDE_EFFECTS) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }
  if (!command_check_byname(executor, "ATTRIB_SET") || fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    safe_str(T(e_perm), buff, bp);
    return;
  }
  s = strchr(args[0], '/');
  if (!s) {
    safe_str(T("#-1 BAD ARGUMENT FORMAT TO ATTRIB_SET"), buff, bp);
    return;
  }
  *s++ = '\0';
  thing = match_thing(executor, args[0]);
  if (!GoodObject(thing)) {
    safe_str(T(e_notvis), buff, bp);
    return;
  }
  if (nargs == 1) {
    do_set_atr(thing, s, NULL, executor, 1);
  } else if (strlen(args[1]) == 0 && !EMPTY_ATTRS) {
    do_set_atr(thing, s, " ", executor, 1);
  } else {
    do_set_atr(thing, s, args[1], executor, 1);
  }
}


/* --------------------------------------------------------------------------
 * Misc functions: TEL
 */

/* ARGSUSED */
FUNCTION(fun_tel)
{
  int silent = 0;
  int inside = 0;
  if (!FUNCTION_SIDE_EFFECTS) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }
  if (!command_check_byname(executor, "@tel") || fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  if (nargs > 2)
    silent = parse_boolean(args[2]);
  if (nargs > 3)
    silent = parse_boolean(args[3]);
  do_teleport(executor, args[0], args[1], silent, inside);
}


/* ARGSUSED */
FUNCTION(fun_isdbref)
{
  safe_boolean(parse_dbref(args[0]) != NOTHING, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_grep)
{
  char *tp;
  int wild;
  int sensitive;

  dbref it = match_thing(executor, args[0]);
  if (!GoodObject(it)) {
    safe_str(T(e_notvis), buff, bp);
    return;
  }
  /* make sure there's an attribute and a pattern */
  if (!*args[1]) {
    safe_str(T("#-1 NO SUCH ATTRIBUTE"), buff, bp);
    return;
  }
  if (!*args[2]) {
    safe_str(T("#-1 INVALID GREP PATTERN"), buff, bp);
    return;
  }

  sensitive = !strcmp(called_as, "GREP") || !strcmp(called_as, "WILDGREP");
  wild = !strcmp(called_as, "WILDGREPI") || !strcmp(called_as, "WILDGREP");
  tp = grep_util(executor, it, args[1], args[2], sensitive, wild);
  add_check("fun_grep.attr_list");
  safe_str(tp, buff, bp);
  mush_free((Malloc_t) tp, "fun_grep.attr_list");
}

/* Get database size statistics */
/* ARGSUSED */
FUNCTION(fun_lstats)
{
  dbref who;
  struct db_stat_info *si;

  if ((!args[0]) || !*args[0] || !strcasecmp(args[0], "all")) {
    who = ANY_OWNER;
  } else if (!strcasecmp(args[0], "me")) {
    who = executor;
  } else {
    who = lookup_player(args[0]);
    if (who == NOTHING) {
      safe_str(T(e_notvis), buff, bp);
      return;
    }
  }
  if (!CanSearch(executor, who)) {
    if (who != ANY_OWNER && !controls(executor, who)) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
  }
  si = get_stats(who);
  if (who != ANY_OWNER) {
    safe_format(buff, bp, "%d %d %d %d %d %d", si->total - si->garbage, si->rooms,
                si->exits, si->things, si->players, si->divisions);
  } else {
    safe_format(buff, bp, "%d %d %d %d %d %d %d", si->total, si->rooms, si->exits,
                si->things, si->players, si->divisions, si->garbage);
  }
}


/* ARGSUSED */
FUNCTION(fun_atrlock)
{
  dbref thing;
  char *p;
  ATTR *ptr;
  int status;

  if (nargs == 1)
    status = 0;
  else
    status = 1;

  if (status == 1) {
    if (FUNCTION_SIDE_EFFECTS) {
      if (!command_check_byname(executor, "@atrlock")
          || fun->flags & FN_NOSIDEFX) {
        safe_str(T(e_perm), buff, bp);
        return;
      }
      do_atrlock(executor, args[0], args[1], 0);
      return;
    } else
      safe_str(T(e_disabled), buff, bp);
    return;
  }

  if (!args[0] || !*args[0]) {
    safe_str(T("#-1 ARGUMENT MUST BE OBJ/ATTR"), buff, bp);
    return;
  }
  if (!(p = strchr(args[0], '/')) || !(*(p + 1))) {
    safe_str(T("#-1 ARGUMENT MUST BE OBJ/ATTR"), buff, bp);
    return;
  }
  *p++ = '\0';

  if ((thing =
       noisy_match_result(executor, args[0], NOTYPE,
                          MAT_EVERYTHING)) == NOTHING) {
    safe_str(T(e_notvis), buff, bp);
    return;
  }

  ptr = atr_get_noparent(thing, strupper(p));
  if (ptr && Can_Read_Attr(executor, thing, ptr))
    safe_boolean(AF_Locked(ptr), buff, bp);
  else
    safe_str("#-1", buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_atrmodtime)
{
  dbref thing;
  char *p;
  ATTR *ptr;

  if (!args[0] || !*args[0]) {
    safe_str(T("#-1 ARGUMENT MUST BE OBJ/ATTR"), buff, bp);
    return;
  }
  if (!(p = strchr(args[0], '/')) || !(*(p + 1))) {
    safe_str(T("#-1 ARGUMENT MUST BE OBJ/ATTR"), buff, bp);
    return;
  }
  *p++ = '\0';

  if ((thing =
       noisy_match_result(executor, args[0], NOTYPE,
                          MAT_EVERYTHING)) == NOTHING) {
    safe_str(T(e_notvis), buff, bp);
    return;
  }

  ptr = atr_get_noparent(thing, strupper(p));
  if (ptr && Can_Read_Attr(executor, thing, ptr))
    safe_str(show_time(AL_MODTIME(ptr), 0), buff, bp);
  else
    safe_str("#-1", buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_followers)
{
  dbref thing;
  ATTR *a;
  char *s;
  char *res;

  thing = match_controlled(executor, args[0]);
  if (!GoodObject(thing)) {
    safe_str(T("#-1 INVALID OBJECT"), buff, bp);
    return;
  }
  a = atr_get_noparent(thing, "FOLLOWERS");
  if (!a)
    return;
  s = atr_value(a);
  res = trim_space_sep(s, ' ');
  safe_str(res, buff, bp);
  return;;
}

/* ARGSUSED */
FUNCTION(fun_following)
{
  dbref thing;
  ATTR *a;
  char *s;
  char *res;

  thing = match_controlled(executor, args[0]);
  if (!GoodObject(thing)) {
    safe_str(T("#-1 INVALID OBJECT"), buff, bp);
    return;
  }
  a = atr_get_noparent(thing, "FOLLOWING");
  if (!a)
    return;
  s = atr_value(a);
  res = trim_space_sep(s, ' ');
  safe_str(res, buff, bp);
  return;;
}
