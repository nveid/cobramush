/**
 * \file set.c
 *
 * \brief PennMUSH commands that set parameters.
 *
 *
 */

#include "copyrite.h"
#include "config.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#ifdef I_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#include <stdlib.h>

#include "conf.h"
#include "externs.h"
#include "mushdb.h"
#include "game.h"
#include "match.h"
#include "attrib.h"
#include "ansi.h"
#include "command.h"
#include "mymalloc.h"
#include "flags.h"
#include "dbdefs.h"
#include "lock.h"
#include "log.h"
#include "confmagic.h"

static int chown_ok(dbref player, dbref thing, dbref newowner);
void do_attrib_flags
  (dbref player, const char *obj, const char *atrname, const char *flag);
static int af_helper(dbref player, dbref thing, dbref parent, 
                       char const *pattern, ATTR *atr, void *args);
static int gedit_helper(dbref player, dbref thing, dbref parent, char const *pattern,
                        ATTR *atr, void *args);
static int wipe_helper(dbref player, dbref thing, dbref parent, char const *pattern,
                       ATTR *atr, void *args);
static void copy_attrib_flags(dbref player, dbref target, ATTR *atr, int flags);


/** Rename something.
 * \verbatim
 * This implements @name.
 * \endverbatim
 * \param player the enactor.
 * \param name current name of object to rename.
 * \param newname new name for object.
 */
void
do_name(dbref player, const char *name, char *newname)
{
  dbref thing;
  char *password;
  char *myenv[10];
  int i;

  newname = trim_space_sep(newname, ' ');

  if ((thing = match_controlled(player, name)) != NOTHING) {
    /* check for bad name */
    if ((*newname == '\0') || strchr(newname, '[')) {
      notify(player, T("Give it what new name?"));
      return;
    }
    /* check for renaming a player */
    if (IsPlayer(thing)) {
      if (PLAYER_NAME_SPACES) {
        if (*newname == '\"') {
          for (; *newname && ((*newname == '\"')
                              || isspace((unsigned char) *newname));
               newname++) ;
          password = newname;
          while (*password && (*password != '\"')) {
            while (*password && (*password != '\"'))
              password++;
            if (*password == '\"') {
              *password++ = '\0';
              while (*password && isspace((unsigned char) *password))
                password++;
              break;
            }
          }
        } else {
          password = newname;
          while (*password && !isspace((unsigned char) *password))
            password++;
          if (*password) {
            *password++ = '\0';
            while (*password && isspace((unsigned char) *password))
              password++;
          }
        }
      } else {

        /* split off password */
        for (password = newname + strlen(newname) - 1;
             *password && !isspace((unsigned char) *password); password--) ;
        for (; *password && isspace((unsigned char) *password); password--) ;
        /* eat whitespace */
        if (*password) {
          *++password = '\0';   /* terminate name */
          password++;
          while (*password && isspace((unsigned char) *password))
            password++;
        }
      }
      if (!ok_player_name(newname, player, thing)) {
        notify(player, T("You can't give a player that name."));
        return;
      }
      /* everything ok, notify */
      do_log(LT_CONN, 0, 0, T("Name change by %s(#%d) to %s"),
             Name(thing), thing, newname);
      /* everything ok, we can fall through to change the name */
    } else {
      if (!ok_name(newname)) {
        notify(player, T("That is not a reasonable name."));
        return;
      }
    }

    /* everything ok, change the name */
    myenv[0] = (char *) mush_malloc(BUFFER_LEN, "string");
    myenv[1] = (char *) mush_malloc(BUFFER_LEN, "string");
    strncpy(myenv[0], Name(thing), BUFFER_LEN - 1);
    myenv[0][BUFFER_LEN - 1] = '\0';
    strcpy(myenv[1], newname);
    for (i = 2; i < 10; i++)
      myenv[i] = NULL;

    if (IsPlayer(thing))
      reset_player_list(thing, Name(thing), NULL, newname, NULL);
    set_name(thing, newname);
    if(!IsPlayer(thing)) {
          char lmbuf[1024];
          ModTime(thing) = mudtime;
          snprintf(lmbuf, 1023, "@name[#%d]", player);
          lmbuf[strlen(lmbuf)+1] = '\0';
          set_lmod(thing, lmbuf);
    }
    if (IsPlayer(thing))
      add_player(thing);

    if (!AreQuiet(player, thing))
      notify(player, T("Name set."));
    real_did_it(player, thing, NULL, NULL, "ONAME", NULL, "ANAME", NOTHING,
                myenv, NA_INTER_PRESENCE);
    mush_free(myenv[0], "string");
    mush_free(myenv[1], "string");
  }
}

/** Change an object's owner.
 * \verbatim
 * This implements @chown.
 * \endverbatim
 * \param player the enactor.
 * \param name name of object to change owner of.
 * \param newobj name of new owner for object.
 * \param preserve if 1, preserve privileges and don't halt the object.
 */
void
do_chown(dbref player, const char *name, const char *newobj, int preserve)
{
  dbref thing;
  dbref newowner = NOTHING;
  char *sp;
  long match_flags = MAT_POSSESSION | MAT_HERE | MAT_EXIT | MAT_ABSOLUTE;


  /* check for '@chown <object>/<atr>=<player>'  */
  sp = strchr(name, '/');
  if (sp) {
    do_atrchown(player, name, newobj);
    return;
  }
  if (OOREF(player,div_powover(player, player, "Chown"),div_powover(ooref, ooref, "Chown")))
    match_flags |= MAT_PLAYER;

  if ((thing = noisy_match_result(player, name, TYPE_THING, match_flags))
      == NOTHING)
    return;

  if (!*newobj || !strcasecmp(newobj, "me")) {
    newowner = player;
  } else {
    if ((newowner = lookup_player(newobj)) == NOTHING) {
      notify(player, T("I couldn't find that player."));
      return;
    }
  }

  if (IsPlayer(thing) && !God(player)) {
    notify(player, T("Players always own themselves."));
    return;
  }
  /* Permissions checking */
  if (!chown_ok(player, thing, newowner)) {
    notify(player, T("Permission denied."));
    return;
  }
  if (IsThing(thing) && !CanChown(player,thing) &&
      !(GoodObject(Location(thing)) && (Location(thing) == player))) {
    notify(player, T("You must carry the object to @chown it."));
    return;
  }
  if (preserve && !OOREF(player,div_powover(player, player, "Chown"),div_powover(ooref, ooref, "Chown"))) {
    notify(player, T("You cannot @CHOWN/PRESERVE. Use normal @CHOWN."));
    return;
  }
  /* chowns to the zone master don't count towards fees */
  if (!ZMaster(newowner)) {
    /* Debit the owner-to-be */
    if (!can_pay_fees(newowner, Pennies(thing))) {
      /* not enough money or quota */
      if (newowner != player)
        notify(player,
               T
               ("That player doesn't have enough money or quota to receive that object."));
      return;
    }
    /* Credit the current owner */
    giveto(Owner(thing), Pennies(thing));
    change_quota(Owner(thing), QUOTA_COST);
  }
  chown_object(player, thing, newowner, preserve);
  notify(player, T("Owner changed."));
}

static int
chown_ok(dbref player, dbref thing, dbref newowner)
{
  /* Cant' touch garbage */
  if (IsGarbage(thing))
    return 0;

  /* God can do it all */
  if (God(player))
    return 1;

  /* In order for non-admin player to @chown thing to newowner,
   * player must control newowner or newowner must be a Zone Master
   * and player must pass its zone lock.
   *
   * In addition, one of the following must apply:
   *   1.  player owns thing, or
   *   2.  player controls Owner(thing), newowner is a zone master,
   *       and Owner(thing) passes newowner's zone-lock, or
   *   3.  thing is CHOWN_OK, and player holds thing if it's an object.
   *
   * The third condition is syntactic sugar to handle the situation
   * where Joe owns Box, an ordinary object, and Tool, an inherit object, 
   * and ZMP, a Zone Master Player, is zone-locked to =tool.
   * In this case, if Joe doesn't pass ZMP's lock, we don't want
   *   Joe to be able to @fo Tool=@chown Box=ZMP
   */

  /* Does player a) control object, b) have chown power */
  if(OOREF(player,controls(player,thing),controls(ooref,thing)) && 
      OOREF(player,div_powover(player, player, "Chown"),div_powover(ooref, ooref, "Chown")))
    return 1;
  /* Does player control newowner, or is newowner a Zone Master and player
   * passes the lock?
   */
  if (!(OOREF(player,controls(player, newowner),controls(ooref, newowner)) ||
        (ZMaster(newowner) && eval_lock(player, newowner, Zone_Lock))))
    return 0;

  /* Target player is legitimate. Does player control the object? */
  if (Owns(player, thing))
    return 1;

  if (controls(player, Owner(thing)) &&
      ZMaster(newowner) && eval_lock(Owner(thing), newowner, Zone_Lock))
    return 1;

  if (ChownOk(thing) && (!IsThing(thing) || (Location(thing) == player) || CanRemote(player, thing)))
    return 1;

  return 0;
}


/** Actually change the ownership of something, and fix bits.
 * \param player the enactor.
 * \param thing object to change ownership of.
 * \param newowner new owner for thing.
 * \param preserve if 1, preserve privileges and don't halt.
 */
void
chown_object(dbref player, dbref thing, dbref newowner, int preserve)
{
  (void) undestroy(player, thing);
  if (God(player)) {
    Owner(thing) = newowner;
  } else {
    Owner(thing) = Owner(newowner);
  }
  if(!preserve) Zone(thing) = Zone(newowner);
  clear_flag_internal(thing, "CHOWN_OK");
  if (!preserve || !Director(player)) {
    set_flag_internal(thing, "HALT");
    if(DPBITS(thing)) 
      mush_free(DPBITS(thing), "POWER_SPOT");   /* wipe out all powers */
    DPBITS(thing) = NULL;
    do_halt(thing, "", thing);
  } else {
    adjust_powers(thing, newowner);
    if (DPBITS(thing))
      notify(player,
             T
             ("Warning: @CHOWN/PRESERVE on a target with @power privileges."));
  }
}


/** Change an object's zone.
 * \verbatim
 * This implements @chzone.
 * \endverbatim
 * \param player the enactor.
 * \param name name of the object to change zone of.
 * \param newobj name of new ZMO.
 * \param noisy if 1, notify player about success and failure.
 * \retval 0 failed to change zone.
 * \retval 1 successfully changed zone.
 */
int
do_chzone(dbref player, char const *name, char const *newobj, int noisy)
{
  dbref thing;
  dbref zone;

  if ((thing = noisy_match_result(player, name, NOTYPE, MAT_NEARBY)) == NOTHING)
    return 0;

  if (!newobj || !*newobj || !strcasecmp(newobj, "none"))
    zone = NOTHING;
  else {
    if ((zone = noisy_match_result(player, newobj, NOTYPE, MAT_EVERYTHING))
        == NOTHING)
      return 0;
  }

  if (Zone(thing) == zone) {
    if (noisy)
      notify(player, T("That object is already in that zone."));
    return 0;
  }

  /* we do use ownership instead of control as a criterion because
   * we only want the owner to be able to rezone the object. Also,
   * this allows players to @chzone themselves to an object they own.
   */
  if (!(God(player) || (Director(player) && controls(player, thing))
        || Owns(player, thing))) {
    if (noisy)
      notify(player, T("You don't have the power to shift reality."));
    return 0;
  }
  /* a player may change an object's zone to:
   * 1.  NOTHING 
   * 2.  an object he owns
   * 3.  an object with a chzone-lock that the player passes.
   * Note that an object with no chzone-lock isn't valid
   */
  if (!((Director(player) && controls(player, zone))
        || (zone == NOTHING) || Owns(player, zone) ||
        ((getlock(zone, Chzone_Lock) != TRUE_BOOLEXP) &&
         eval_lock(player, zone, Chzone_Lock)))) {
    if (noisy)
      notify(player, T("You cannot move that object to that zone."));
    return 0;
  }
  /* Don't chzone object to itself! */
  if (zone == thing) {
    if (noisy)
      notify(player, T("You shouldn't zone objects to themselves!"));
    return 0;
  }
  /* Don't allow circular zones */
  if (GoodObject(zone)) {
    dbref tmp;
    int zone_depth = MAX_ZONES;

    for (tmp = Zone(zone); GoodObject(tmp); tmp = Zone(tmp)) {
      if (tmp == thing) {
        notify(player, T("You can't make circular zones!"));
        return 0;
      }
      if (tmp == Zone(tmp))     /* Ran into an object zoned to itself */
        break;
      zone_depth--;
      if(!zone_depth) {
              notify(player, T("Overly deep zone chain"));
              return 0;
      }
    }
  }

  /* Don't allow use of a division object as a ZMO */
  if (IsDivision(zone)) {
    notify(player, T("You can't use a division object as a ZMO."));
    return 0;
  }

  /* Don't allow chzone to objects without elocks! 
   * If no lock is set, set a default lock (warn if zmo are used for control)
   * This checks for many trivial elocks (canuse/1, where &canuse=1)
   */
  if (zone != NOTHING)
          check_zone_lock(player, zone, noisy);
  /* Warn admins when they zone their stuff */
  if ((zone != NOTHING) && Admin(Owner(thing))) {
    if (noisy)
      notify(player, T("Warning: @chzoning admin-owned object!"));
  }
  /* everything is okay, do the change */
  Zone(thing) = zone;
  /* If we're not unzoning, and we're working with a non-player object,
   * we'll remove inherit and powers, for security.
   */
  if ((zone != NOTHING) && !IsPlayer(thing)) {
    /* if the object is a player, resetting these flags is rather
     * inconvenient -- although this may pose a bit of a security
     * risk. Be careful when @chzone'ing admin players.
     */
    if(DPBITS(thing))
      mush_free(DPBITS(thing), "POWER_SPOT"); /* Wipe Powers */
  } else {
    if (noisy && (zone != NOTHING)) {
      if (Admin(thing) && noisy)
        notify(player, T("Warning: @chzoning an administrator."));
    }
  }
  if (noisy)
    notify(player, T("Zone changed."));
  return 1;
}

/** Structure for af_helper() data. */
struct af_args {
  int setf;             /**< flag bits to set */
  int clrf;             /**< flag bits to clear */
  char *setflags;       /**< list of names of flags to set */
  char *clrflags;       /**< list of names of flags to clear */
};

static int
af_helper(dbref player, dbref thing, dbref parent __attribute__ ((__unused__)),
          char const *pattern
          __attribute__ ((__unused__)), ATTR *atr, void *args)
{
  struct af_args *af = args;

  /* We must be able to write to that attribute normally,
   * to prevent players from doing funky things to, say, LAST.
   * There is one special case - the resetting of the SAFE flag.
   */
  if (!(Can_Write_Attr(player, thing, AL_ATTR(atr)) ||
        ((af->clrf & AF_SAFE) &&
         Can_Write_Attr_Ignore_Safe(player, thing, AL_ATTR(atr))))) {
    notify_format(player, T("You cannot change that flag on %s/%s"),
                  Name(thing), AL_NAME(atr));
    return 0;
  }

  /* Clear flags first, then set flags */
  if (af->clrf) {
    AL_FLAGS(atr) &= ~af->clrf;
    if (!AreQuiet(player, thing))
      notify_format(player, T("%s/%s - %s reset."), Name(thing), AL_NAME(atr),
                    af->clrflags);
  }
  if (af->setf) {
    AL_FLAGS(atr) |= af->setf;
    if (!AreQuiet(player, thing))
      notify_format(player, T("%s/%s - %s set."), Name(thing), AL_NAME(atr),
                    af->setflags);
  }

  return 1;
}

static void
copy_attrib_flags(dbref player, dbref target, ATTR *atr, int flags)
{
  if (!atr)
    return;
  if (!Can_Write_Attr(player, target, AL_ATTR(atr))) {
    notify_format(player,
                  T("You cannot set attrib flags on %s/%s"), Name(target),
                  AL_NAME(atr));
    return;
  }
  AL_FLAGS(atr) = flags;
}

/** Set a flag on an attribute.
 * \param player the enactor.
 * \param obj the name of the object with the attribute.
 * \param atrname the name of the attribute.
 * \param flag the name of the flag to set or clear.
 */
void
do_attrib_flags(dbref player, const char *obj, const char *atrname,
                const char *flag)
{
  struct af_args af;
  dbref thing;
  const char *p;

  if ((thing = match_controlled(player, obj)) == NOTHING)
    return;

  if (!flag || !*flag) {
    notify(player, T("What flag do you want to set?"));
    return;
  }

  p = flag;
  /* Skip leading spaces */
  while (*p && isspace((unsigned char) *p))
    p++;

  af.setf = af.clrf = 0;
  if (string_to_atrflagsets(player, p, &af.setf, &af.clrf) < 0) {
    notify(player, T("Unrecognized attribute flag."));
    return;
  }
  af.clrflags = mush_strdup(atrflag_to_string(af.clrf), "al_flag list");
  af.setflags = mush_strdup(atrflag_to_string(af.setf), "al_flag list");
  if (!atr_iter_get(player, thing, atrname, 0, af_helper, &af))
    notify(player, T("No attribute found to change."));
  mush_free((Malloc_t) af.clrflags, "af_flag list");
  mush_free((Malloc_t) af.setflags, "af_flag list");
}


/** Set a flag, attribute flag, or attribute.
 * \verbatim
 * This implements @set.
 * \endverbatim
 * \param player the enactor.
 * \param name the first (left) argument to the command.
 * \param flag the second (right) argument to the command.
 * \retval 1 successful set.
 * \retval 0 failure to set.
 */
int
do_set(dbref player, const char *name, char *flag)
{
  dbref thing;
  int her, listener, negate;
  char *p, *f;
  char flagbuff[BUFFER_LEN];

  /* check for attribute flag set first */
  if ((p = strchr(name, '/')) != NULL) {
    *p++ = '\0';
    do_attrib_flags(player, name, p, flag);
    return 1;
  }
  /* find thing */
  if(!GoodObject((thing = match_thing(player, name))))
      return 0;
  if (God(thing) && !God(player)) {
    notify(player, T("Only God can set himself!"));
    return 0;
  }
  /* check for attribute set first */
  if ((p = strchr(flag, ':')) != NULL) {
    *p++ = '\0';
    if (!command_check_byname(player, "ATTRIB_SET")) {
      notify(player, T("You may not set attributes."));
      return 0;
    }
    return do_set_atr(thing, flag, p, player, 1);
  }
  /* we haven't set an attribute, so we must be setting flags */
  strcpy(flagbuff, flag);
  p = trim_space_sep(flagbuff, ' ');
  if (*p == '\0') {
    notify(player, T("You must specify a flag to set."));
    return 0;
  }
  do {
    her = Hearer(thing);         /* Must be in loop, can change! */
    listener = Listener(thing);  /* Must be in loop, can change! */
    f = split_token(&p, ' ');
    negate = 0;
    if (*f == NOT_TOKEN && *(f + 1)) {
      negate = 1;
      f++;
    }
    set_flag(player, thing, f, negate, her, listener);
  } while (p);
  return 1;
}

/** Copy or move an attribute.
 * \verbatim
 * This implements @cpattr and @mvattr.
 * the command is of the format:
 * @cpattr oldobj/oldattr = newobj1/newattr1, newobj2/newattr2, etc.
 * \endverbatim
 * \param player the enactor.
 * \param oldpair the obj/attribute pair to copy from.
 * \param newpair array of obj/attribute pairs to copy to.
 * \param move if 1, move rather than copy.
 * \param noflagcopy if 1, don't copy associated flags.
 */
void
do_cpattr(dbref player, char *oldpair, char **newpair, int move, int noflagcopy)
{
  dbref oldobj, newobj;
  char tbuf1[BUFFER_LEN], tbuf2[BUFFER_LEN];
  int i;
  char *p, *q;
  ATTR *a;
  char *text;
  int copies = 0;

  /* must copy from something */
  if (!oldpair || !*oldpair) {
    notify(player, T("What do you want to copy from?"));
    return;
  }
  /* find the old object */
  strcpy(tbuf1, oldpair);
  p = strchr(tbuf1, '/');
  if (!p || !*p) {
    notify(player, T("What object do you want to copy the attribute from?"));
    return;
  }
  *p++ = '\0';
  oldobj = noisy_match_result(player, tbuf1, NOTYPE, MAT_EVERYTHING);
  if (!GoodObject(oldobj))
    return;

  strcpy(tbuf2, p);
  p = tbuf2;
  /* find the old attribute */
  a = atr_get_noparent(oldobj, strupper(p));
  if (!a) {
    notify(player, T("No such attribute to copy from."));
    return;
  }
  /* check permissions to get it */
  if (!Can_Read_Attr(player, oldobj, a)) {
    notify(player, T("Permission to read attribute denied."));
    return;
  }
  /* we can read it. Copy the value. */
  text = safe_atr_value(a);

  /* now we loop through our new object pairs and copy, calling @set. */
  for (i = 1; i < MAX_ARG && (newpair[i] != NULL); i++) {
    if (!*newpair[i]) {
      notify(player, T("What do you want to copy to?"));
    } else {
      strcpy(tbuf1, newpair[i]);
      q = strchr(tbuf1, '/');
      if (!q || !*q) {
        q = (char *) AL_NAME(a);
      } else {
        *q++ = '\0';
      }
      newobj = noisy_match_result(player, tbuf1, NOTYPE, MAT_EVERYTHING);
      if (GoodObject(newobj) &&
          ((newobj != oldobj) || strcasecmp(AL_NAME(a), q)) &&
          do_set_atr(newobj, q, text, player, 1))
        copies++;
      /* copy the attribute flags too */
      if (!noflagcopy)
        copy_attrib_flags(player, newobj,
                          atr_get_noparent(newobj, strupper(q)), a->flags);

    }
  }

  free((Malloc_t) text);        /* safe_uncompress malloc()s memory */
  if (copies) {
    notify_format(player, T("Attribute %s (%d copies)"),
                  (move ? "moved" : "copied"), copies);
    if (move)
      do_set_atr(oldobj, AL_NAME(a), NULL, player, 1);
  } else {
    notify_format(player, T("Unable to %s attribute."),
                  (move ? "move" : "copy"));
  }
  return;
}

/** Argument struct for gedit_helper */
struct gedit_args {
  enum edit_type target; /**< The type of edit */
  int doit; /**< Do we actually replace the attribute, or just pretend? */
  char *from; /**< What is going to be replaced? */
  char *to; /**< What it gets replaced with. */
};

static int
gedit_helper(dbref player, dbref thing,
             dbref parent __attribute__ ((__unused__)),
             char const *pattern
             __attribute__ ((__unused__)), ATTR *a, void *args)
{
  int ansi_long_flag = 0;
  const char *r;
  char *s, *val;
  char tbuf1[BUFFER_LEN], tbuf_ansi[BUFFER_LEN];
  char *tbufp, *tbufap;
  size_t rlen, vlen;
  struct gedit_args *gargs;

  gargs = args;

  val = gargs->from;
  vlen = strlen(val);
  r = gargs->to ? gargs->to : "";
  rlen = strlen(r);

  tbufp = tbuf1;
  tbufap = tbuf_ansi;

  if (!a) {                     /* Shouldn't ever happen, but better safe than sorry */
    notify(player, T("No such attribute, try set instead."));
    return 0;
  }
  if (!Can_Write_Attr(player, thing, a)) {
    notify(player, T("You need to control an attribute to edit it."));
    return 0;
  }
  s = (char *) atr_value(a);    /* warning: pointer to static buffer */

  if (vlen == 1 && *val == '$') {
    /* append */
    safe_str(s, tbuf1, &tbufp);
    safe_str(r, tbuf1, &tbufp);

    if (safe_format(tbuf_ansi, &tbufap, "%s%s%s%s", s, ANSI_HILITE, r,
                    ANSI_NORMAL))
      ansi_long_flag = 1;
  } else if (vlen == 1 && *val == '^') {
    /* prepend */
    safe_str(r, tbuf1, &tbufp);
    safe_str(s, tbuf1, &tbufp);

    if (safe_format(tbuf_ansi, &tbufap, "%s%s%s%s", ANSI_HILITE, r, ANSI_NORMAL,
                    s))
      ansi_long_flag = 1;
  } else if (!*val) {
    /* insert replacement string between every character */
    ansi_string *haystack;
    size_t last = 0;
    int too_long = 0;

    haystack = parse_ansi_string(s);

    /* Add one at the start */
    if (!safe_strl(r, rlen, tbuf1, &tbufp)) {
      if (gargs->target != EDIT_FIRST) {
        for (last = 0; last < haystack->len; last++) {
          /* Add the next character */
          if (safe_ansi_string(haystack, last, 1, tbuf1, &tbufp)) {
            too_long = 1;
            break;
          }
          if (!ansi_long_flag) {
            if (safe_ansi_string(haystack, last, 1, tbuf_ansi, &tbufap))
              ansi_long_flag = 1;
          }
          /* Copy in r */
          if (safe_strl(r, rlen, tbuf1, &tbufp)) {
            too_long = 1;
            break;
          }
          if (!ansi_long_flag) {
            if (safe_format(tbuf_ansi, &tbufap, "%s%s%s", ANSI_HILITE, r,
                            ANSI_NORMAL))
              ansi_long_flag = 1;
          }
        }
      }
    }
    free_ansi_string(haystack);
  } else {
    /* find and replace */
    ansi_string *haystack;
    size_t last = 0;
    char *p;
    int too_long = 0;

    haystack = parse_ansi_string(s);

    while (last < haystack->len
           && (p = strstr(haystack->text + last, val)) != NULL) {
      if (safe_ansi_string(haystack, last, p - (haystack->text + last),
                           tbuf1, &tbufp)) {
        too_long = 1;
        break;
      }
      if (!ansi_long_flag) {
        if (safe_ansi_string(haystack, last, p - (haystack->text + last),
                             tbuf_ansi, &tbufap))
          ansi_long_flag = 1;
      }

      /* Copy in r */
      if (safe_strl(r, rlen, tbuf1, &tbufp)) {
        too_long = 1;
        break;
      }
      if (!ansi_long_flag) {
        if (safe_format(tbuf_ansi, &tbufap, "%s%s%s", ANSI_HILITE, r,
                        ANSI_NORMAL))
          ansi_long_flag = 1;
      }
      last = p - haystack->text + vlen;
      if (gargs->target == EDIT_FIRST)
        break;
    }
    if (last < haystack->len && !too_long) {
      safe_ansi_string(haystack, last, haystack->len, tbuf1, &tbufp);
      if (!ansi_long_flag) {
        if (safe_ansi_string(haystack, last, haystack->len, tbuf_ansi, &tbufap))
          ansi_long_flag = 1;
      }
    }
    free_ansi_string(haystack);
  }

  *tbufp = '\0';
  *tbufap = '\0';

  if (gargs->doit) {
    if (do_set_atr(thing, AL_NAME(a), tbuf1, player, 0) &&
        !AreQuiet(player, thing)) {
      if (!ansi_long_flag && ShowAnsi(player))
        notify_format(player, "%s - Set: %s", AL_NAME(a), tbuf_ansi);
      else
        notify_format(player, "%s - Set: %s", AL_NAME(a), tbuf1);
    }
  } else {
    /* We don't do it - we just pemit it. */
    if (!ansi_long_flag && ShowAnsi(player))
      notify_format(player, "%s - Set: %s", AL_NAME(a), tbuf_ansi);
    else
      notify_format(player, "%s - Set: %s", AL_NAME(a), tbuf1);
  }

  return 1;
}

/** Edit an attribute.
 * \verbatim
 * This implements @edit obj/attribute = {search}, {replace}
 * \endverbatim
 * \param player the enactor.
 * \param it the object/attribute pair.
 * \param argv array containing the search and replace strings.
 */
void
do_gedit(dbref player, char *it, char **argv, enum edit_type target, int doit)
{
  dbref thing;
  char tbuf1[BUFFER_LEN];
  char *q;
  struct gedit_args args;

  if (!(it && *it)) {
    notify(player, T("I need to know what you want to edit."));
    return;
  }
  strcpy(tbuf1, it);
  q = strchr(tbuf1, '/');
  if (!(q && *q)) {
    notify(player, T("I need to know what you want to edit."));
    return;
  }
  *q++ = '\0';
  thing = noisy_match_result(player, tbuf1, NOTYPE, MAT_EVERYTHING);

  if ((thing == NOTHING) || !controls(player, thing)) {
    notify(player, T("Permission denied."));
    return;
  }

  if (!argv[1] || !*argv[1]) {
    notify(player, T("Nothing to do."));
    return;
  }
  args.from = argv[1];
  args.to = argv[2];
  args.target = target;
  args.doit = doit;

  if (!atr_iter_get(player, thing, q, 0, gedit_helper, &args))
    notify(player, T("No matching attributes."));
}
/** Trigger an attribute.
 * \verbatim
 * This implements @trigger obj/attribute = list-of-arguments.
 * \endverbatim
 * \param player the enactor.
 * \param object the object/attribute pair.
 * \param argv array of arguments.
 */
void
do_trigger(dbref player, char *object, char **argv)
{
  dbref thing;
  int a;
  char *s;
  char tbuf1[BUFFER_LEN];

  strcpy(tbuf1, object);
  for (s = tbuf1; *s && (*s != '/'); s++) ;
  if (!*s) {
    notify(player, T("I need to know what attribute to trigger."));
    return;
  }
  *s++ = '\0';

  thing = noisy_match_result(player, tbuf1, NOTYPE, MAT_EVERYTHING);

  if (thing == NOTHING)
    return;

  if (IsDivision(thing) && GoodObject(Parent(thing))) {
    notify(player, T("Permission denied."));
    return;
  }
  if (!controls(player, thing) && !(Owns(player, thing) && LinkOk(thing))) {
    notify(player, T("Permission denied."));
    return;
  }
  if (God(thing) && !God(player)) {
    notify(player, T("You can't trigger God!"));
    return;
  }
  /* trigger modifies the stack */
  for (a = 0; a < 10; a++) {
    if(!argv[a+1])
            break;
    global_eval_context.wnxt[a] = argv[a + 1];
  }
  while(a < 10)
          global_eval_context.wnxt[a++] = NULL;

  if (charge_action(player, thing, upcasestr(s))) {
    if (!AreQuiet(player, thing))
      notify_format(player, "%s - Triggered.", Name(thing));
  } else {
    notify(player, T("No such attribute."));
  }
}


/** The use command.
 * It's here for lack of a better place.
 * \param player the enactor.
 * \param what name of the object to use.
 */
void
do_use(dbref player, const char *what)
{
  dbref thing;

  /* if we pass the use key, do it */

  if ((thing =
       noisy_match_result(player, what, TYPE_THING,
                          MAT_NEAR_THINGS)) != NOTHING) {
    if (!eval_lock(player, thing, Use_Lock)) {
      fail_lock(player, thing, Use_Lock, T("Permission denied."), NOTHING);
      return;
    } else
      did_it(player, thing, "USE", "Used.", "OUSE", NULL, "AUSE", NOTHING);
  }
}

/** Parent an object to another.
 * \verbatim
 * This implements @parent.
 * \endverbatim
 * \param player the enactor.
 * \param name the name of the child object.
 * \param parent_name the name of the new parent object.
 */
void
do_parent(dbref player, char *name, char *parent_name)
{
  dbref thing;
  dbref parent;
  dbref check;
  int i;

  if ((thing = noisy_match_result(player, name, NOTYPE, MAT_NEARBY)) == NOTHING)
    return;

  if (!parent_name || !*parent_name || !strcasecmp(parent_name, "none"))
    parent = NOTHING;
  else if ((parent = noisy_match_result(player, parent_name, NOTYPE,
                                        MAT_EVERYTHING)) == NOTHING)
    return;

  /* do control check */
  if (!controls(player, thing) && !(Owns(player, thing) && LinkOk(thing))) {
    notify(player, T("Permission denied."));
    return;
  }
  /* a player may change an object's parent to NOTHING or to an 
   * object he owns, or one that is LINK_OK when the player passes
   * the parent lock
   * mod: also when the player controls the parent, it passes the parent lock
   */
  if ((parent != NOTHING) && !controls(player, parent) &&
      !(LinkOk(parent) && eval_lock(player, parent, Parent_Lock))) {
    notify(player, T("Permission denied."));
    return;
  }
  /* check to make sure no recursion can happen */
  if (parent == thing) {
    notify(player, T("A thing cannot be its own ancestor!"));
    return;
  }
  /* check if this is a division */
  if(IsDivision(thing)) {
    if(Division(thing) != -1) {
      notify(player, "You can't only @parent the master division.");
      return;
     } else {
       /* this is a master division               */
       /* check parent recursion.. for a division */
       for(i = 0, check = parent; i < MAX_PARENTS && check != NOTHING
           ; i++ , check = Parent(check))
         if(IsDivision(check)) {
           notify(player, T("A division is not allowed to be in your parent tree."));
           return;
         }
     }
  }

  if (parent != NOTHING) {
    for (i = 0, check = Parent(parent);
         (i < MAX_PARENTS) && (check != NOTHING); i++, check = Parent(check)) {
      if (check == thing) {
        notify(player, T("You are not allowed to be your own ancestor!"));
        return;
      }
    }
    if (i >= MAX_PARENTS) {
      notify(player, T("Too many ancestors."));
      return;
    }
    if (Owner(parent) != Owner(thing)
        && !has_flag_by_name(parent, "AUTH_PARENT", NOTYPE)) {
      notify(player, T("Warning: Parent and child are owned by different players and parent is not set AUTH_PARENT."));
    }
  }
  /* everything is okay, do the change */
  Parent(thing) = parent;
  if (!AreQuiet(player, thing))
    notify(player, T("Parent changed."));
}

static int
wipe_helper(dbref player, dbref thing, dbref parent __attribute__ ((__unused__)),
    char const *pattern, ATTR *atr, void *args __attribute__ ((__unused__)))
{
  /* for added security, only God can modify privileged
   * attributes using this command and wildcards.  Wiping a specific
   * attr still works, though.
   */
  if (wildcard(pattern) && (AL_FLAGS(atr) & AF_PRIVILEGE) && !Director(player))
          return 0;
  return do_set_atr(thing, AL_NAME(atr), NULL, player, 0) == 1;
}

/** Clear an attribute.
 * \verbatim
 * This implements @wipe.
 * \endverbatim
 * \param player the enactor.
 * \param name the object/attribute-pattern to wipe.
 */
void
do_wipe(dbref player, char *name)
{
  dbref thing;
  char *pattern;

  if ((pattern = strchr(name, '/')) != NULL)
    *pattern++ = '\0';

  if ((thing = noisy_match_result(player, name, NOTYPE, MAT_NEARBY)) == NOTHING)
    return;

  /* this is too destructive of a command to be used by someone who
   * doesn't own the object. Thus, the check is on Owns not controls.
   */
  if (!controls(player, thing)) {
    notify(player, T("Permission denied."));
    return;
  }
  /* protect SAFE objects unless doing a non-wildcard pattern */
  if (Safe(thing) && !(pattern && *pattern && !wildcard(pattern))) {
    notify(player, T("That object is protected."));
    return;
  }

  we_are_wiping = 1;

  if (!atr_iter_get(player, thing, pattern, 0, wipe_helper, NULL))
    notify(player, T("No matching attributes."));
  else
    notify(player, T("Attributes wiped."));

  we_are_wiping = 0;
}
