/**
 * \file wiz.c
 *
 * \brief Admin commands in PennMUSH.
 *
 *
 */

#include "copyrite.h"
#include "config.h"

#ifdef I_UNISTD
#include <unistd.h>
#endif
#include <string.h>
#include <math.h>
#ifdef I_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#ifdef I_FCNTL
#include <fcntl.h>
#endif
#ifdef WIN32
#include <windows.h>
#include "process.h"
#endif
#include "conf.h"
#include "externs.h"
#include "mushdb.h"
#include "attrib.h"
#include "match.h"
#include "access.h"
#include "parse.h"
#include "mymalloc.h"
#include "flags.h"
#include "lock.h"
#include "log.h"
#include "game.h"
#include "command.h"
#include "dbdefs.h"
#include "extmail.h"


#include "confmagic.h"

extern dbref find_entrance(dbref door);
struct db_stat_info *get_stats(dbref owner);
extern dbref find_player_by_desc(int port);


#ifndef WIN32
#ifdef I_SYS_FILE
#include <sys/file.h>
#endif
#endif

struct search_spec {
  dbref owner;	/**< Limit to this owner, if specified */
  int type;	/**< Limit to this type */
  dbref parent;	/**< Limit to children of this parent */
  dbref zone;	/**< Limit to those in this zone */
  char flags[BUFFER_LEN];	/**< Limit to those with these flags */
  char lflags[BUFFER_LEN];	/**< Limit to those with these flags */
  char powers[BUFFER_LEN];	/**< Limit to those with these powers */
  char eval[BUFFER_LEN];	/**< Limit to those where this evals true */
  char name[BUFFER_LEN];	/**< Limit to those prefix-matching this name */
  dbref low;	/**< Limit to dbrefs here or higher */
  dbref high;	/**< Limit to dbrefs here or lower */
};

int tport_dest_ok(dbref player, dbref victim, dbref dest);
int tport_control_ok(dbref player, dbref victim, dbref loc);
static int mem_usage(dbref thing);
static int raw_search(dbref player, const char *owner, int nargs,
		      const char **args, dbref **result, PE_Info * pe_info);
static int fill_search_spec(dbref player, const char *owner, int nargs,
			    const char **args, struct search_spec *spec);

#ifdef INFO_SLAVE
void kill_info_slave(void);
#endif

extern char confname[BUFFER_LEN];
extern char errlog[BUFFER_LEN];

/** Create a player by admin fiat.
 * \verbatim
 * This implements @pcreate.
 * \endverbatim
 * \param creator the enactor.
 * \param player_name name of player to create.
 * \param player_password password for player.
 * \return dbref of created player object, or NOTHING if failure.
 */
dbref
do_pcreate(dbref creator, const char *player_name, const char *player_password)
{
  dbref player;

  if (!Create_Player(creator)) {
    notify(creator, T("You do not have the power over body and mind!"));
    return NOTHING;
  }
  if (!can_pay_fees(creator, 0))
    return NOTHING;
  player = create_player(player_name, player_password, "None", "None");
  if (player == NOTHING) {
    notify_format(creator, T("Failure creating '%s' (bad name)"), player_name);
    return NOTHING;
  }
  if (player == AMBIGUOUS) {
    notify_format(creator, T("Failure creating '%s' (bad password)"),
		  player_name);
    return NOTHING;
  }
  notify_format(creator, T("New player '%s' (#%d) created with password '%s'"),
		player_name, player, player_password);
  do_log(LT_WIZ, creator, player, T("Player creation"));
  return player;
}

/** Set or check a player's quota.
 * \verbatim
 * This implements @quota and @squota.
 * \endverbatim
 * \param player the enactor.
 * \param arg1 name of player whose quota should be set or checked.
 * \param arg2 amount to set or adjust quota, ignored if checking.
 * \param seq_q if 1, set quota; if 0, check quota.
 */
void
do_quota(dbref player, const char *arg1, const char *arg2, int set_q)
{
  dbref who, thing;
  int owned, limit, dlimit = -1, downed = -1, adjust;

  /* determine the victim */
  if (!arg1 || !*arg1 || !strcmp(arg1, "me"))
    who = player;
  else {
    who = lookup_player(arg1);
    if (who == NOTHING && ((who = match_result(player, arg1, TYPE_DIVISION, MAT_EVERYTHING)) == NOTHING 
	  || Typeof(who) != TYPE_DIVISION)) {
      notify(player, T("No such player or division."));
      return;
    }
  }

  /* check permissions */
  if (set_q && !Change_Quota(player, who)) {
    notify(player, T("Permission denied."));
    return;
  }
  if ((player != who) && !Do_Quotas(player, who) && !CanSee(player, who)) {
    notify(player, T("You can't look at someone else's quota."));
    return;
  }
  /* count up all owned objects */
  owned = -1;			/* a player is never included in his own
				 * quota */
  for (thing = 0; thing < db_top; thing++) {
    if (Owner(thing) == who && !IsDivision(who)) {
      if (!IsGarbage(thing))
	++owned;
    } else if(IsDivision(who) && IsDivision(Division(thing)) && div_inscope(who,thing) ) { /* incase we're doing division quotas */
      if(!IsGarbage(thing) && !IsPlayer(thing)) /* we don't include garbage or player objects in div quotas */
	++owned;
    }
    /* And No matter what.. we have to calculate the same info from the division we're in.. */
    if( !IsMasterDivision(Division(who)) && !IsGarbage(thing) && !IsPlayer(thing) && IsDivision(Division(thing))&& 
	div_inscope(Division(who), thing)) 
	downed++;
  }

   if(!IsMasterDivision(Division(who)) && !NoQuota(Division(who)))
    dlimit = get_current_quota(Division(who));


  /* the quotas of players with the NoQuota power are unlimited and cannot be set. */
  if (!USE_QUOTA || NoQuota(who)) {
    notify_format(player, T("Objects: %d   Limit: UNLIMITED"), owned);
    return;
  }
  /* if we're not doing a change, determine the mortal's quota limit. 
   * RQUOTA is the objects _left_, not the quota itself.
   */

  if (!set_q) {
    limit = get_current_quota(who);
    notify_format(player, T("Objects: %d   Limit: %d"), owned, (downed + dlimit) < (owned + limit) && dlimit != NOTHING
	? (downed + dlimit) : (owned + limit));
    return;
  }
  /* set a new quota */
  if (!arg2 || !*arg2) {
    limit = get_current_quota(who);
    notify_format(player, T("Objects: %d   Limit: %d"), owned, (downed + dlimit) < (owned + limit) && dlimit != NOTHING ? 
	(downed + dlimit) : (owned + limit));
    notify(player, T("What do you want to set the quota to?"));
    return;
  }
  adjust = ((*arg2 == '+') || (*arg2 == '-'));
  if (adjust)
    limit = owned + get_current_quota(who) + atoi(arg2);
  else
    limit = atoi(arg2);
  if (limit < owned)		/* always have enough quota for your objects */
    limit = owned;
  if(limit < dlimit && dlimit != NOTHING)
    limit = dlimit;

  (void) atr_add(!IsDivision(who) ? Owner(who) : who, "RQUOTA", tprintf("%d", limit - owned), GOD,
		 NOTHING);

  notify_format(player, T("Objects: %d   Limit: %d"), owned, limit);
}


/** Check or set quota globally.
 * \verbatim
 * This implements @allquota.
 * \endverbatim
 * \param player the enactor.
 * \param arg1 new quota limit, as a string.
 * \param quiet if 1, don't display every player's quota.
 */
void
do_allquota(dbref player, const char *arg1, int quiet)
{
  int oldlimit, limit, owned;
  dbref who, thing;

  if (!God(player)) {
    notify(player, T("Who do you think you are, GOD?"));
    return;
  }
  if (!arg1 || !*arg1) {
    limit = -1;
  } else if (!is_integer(arg1)) {
    notify(player, T("You can only set quotas to a number."));
    return;
  } else {
    limit = parse_integer(arg1);
    if (limit < 0) {
      notify(player, T("You can only set quotas to a positive number."));
      return;
    }
  }

  for (who = 0; who < db_top; who++) {
    if (!IsPlayer(who))
      continue;

    /* count up all owned objects */
    owned = -1;			/* a player is never included in his own
				 * quota */
    for (thing = 0; thing < db_top; thing++) {
      if (Owner(thing) == who)
	if (!IsGarbage(thing))
	  ++owned;
    }

    if (NoQuota(who)) {
      if (!quiet)
	notify_format(player, "%s: Objects: %d   Limit: UNLIMITED",
		      Name(who), owned);
      continue;
    }
    if (!quiet) {
      oldlimit = get_current_quota(who);
      notify_format(player, "%s: Objects: %d   Limit: %d",
		    Name(who), owned, oldlimit);
    }
    if (limit != -1) {
      if (limit <= owned) {
	(void) atr_add(who, "RQUOTA", "0", GOD, NOTHING);
      } else {
	(void) atr_add(who, "RQUOTA", tprintf("%d", limit - owned), GOD,
		       NOTHING);
      }
    }
  }
  if (limit == -1)
    notify(player, T("Quotas not changed."));
  else
    notify_format(player, T("All quotas changed to %d."), limit);
}

int
tport_dest_ok(dbref player, dbref victim, dbref dest)
{
  /* can player legitimately send something to dest */

  if (Tel_Where(player, dest))
    return 1;

  if (controls(player, dest))
    return 1;

  /* beyond this point, if you don't control it and it's not a room, no hope */
  if (!IsRoom(dest))
    return 0;

  /* Check for a teleport lock. It fails if the player is not properly
   * empowered, and the room is tport-locked against the victim, and the
   * victim does not control the room.
   */
  if (!eval_lock(victim, dest, Tport_Lock))
    return 0;

  if (JumpOk(dest))
    return 1;

  return 0;
}

int
tport_control_ok(dbref player, dbref victim, dbref loc)
{
  /* can player legitimately move victim from loc */

  if (God(victim) && !God(player))
    return 0;

  if(RPMODE(victim) && !(Can_RPTEL(player)||Director(player))) /* require RPTEL power to tport things in RPMODE */
	  return 0;

  if (Tel_Thing(player, victim))
    return 1;

  if (controls(player, victim))
    return 1;

  /* mortals can't @tel priv'ed players just on basis of location ownership */

  if (controls(player, loc) && (!Admin(victim) || Owns(player, victim)))
    return 1;

  return 0;
}

/** Teleport something somewhere.
 * \verbatim
 * This implements @tel.
 * \endverbatim
 * \param player the enactor.
 * \param arg1 the object to teleport (or location if no object given)
 * \param arg2 the location to teleport to.
 * \param silent if 1, don't trigger teleport messagse.
 * \param inside if 1, always @tel to inventory, even of a player
 */
void
do_teleport(dbref player, const char *arg1, const char *arg2, int silent,
	    int inside)
{
  dbref victim;
  dbref destination;
  dbref loc;
  const char *to;
  dbref absroom;		/* "absolute room", for NO_TEL check */

  /* get victim, destination */
  if (*arg2 == '\0') {
    victim = player;
    to = arg1;
  } else {
    if ((victim =
	 noisy_match_result(player, arg1, NOTYPE,
			    MAT_OBJECTS | MAT_ENGLISH)) == NOTHING) {
      return;
    }
    to = arg2;
  }

  if (IsRoom(victim)) {
    notify(player, T("You can't teleport rooms."));
    return;
  }
  if (IsGarbage(victim)) {
    notify(player, T("Garbage belongs in the garbage dump."));
    return;
  }
  /* get destination */

  if (!strcasecmp(to, "home")) {
    /* If the object is @tel'ing itself home, treat it the way we'd  
     * treat a 'home' command 
     */
    if (player == victim) {
      if (Fixed(victim) || RPMODE(victim))
	notify(player, T("You can't do that IC!"));
      else
	safe_tel(victim, HOME, silent);
      return;
    } else
      destination = Home(victim);
  } else {
    destination = match_result(player, to, TYPE_PLAYER, MAT_EVERYTHING);
  }

  switch (destination) {
  case NOTHING:
    notify(player, T("No match."));
    break;
  case AMBIGUOUS:
    notify(player, T("I don't know which destination you mean!"));
    break;
  case HOME:
    destination = Home(victim);
    /* FALL THROUGH */
  default:
    /* check victim, destination types, teleport if ok */
    if (!GoodObject(destination) || IsGarbage(destination)) {
      notify(player, T("Bad destination."));
      return;
    }
    if (recursive_member(destination, victim, 0)
	|| (victim == destination)) {
      notify(player, T("Bad destination."));
      return;
    }
    if (!Tel_Where(player, destination)
	&& IsPlayer(victim) && IsPlayer(destination)) {
      notify(player, T("Bad destination."));
      return;
    }
    if (IsExit(victim)) {
      /* Teleporting an exit means moving its source */
      if (!IsRoom(destination)) {
	notify(player, T("Exits can only be teleported to other rooms."));
	return;
      }
      if (Going(destination)) {
	notify(player,
	       T("You can't move an exit to someplace that's crumbling."));
	return;
      }
      if (!GoodObject(Home(victim)))
	loc = find_entrance(victim);
      else
	loc = Home(victim);
      /* Unlike normal teleport, you must control the destination 
       * or have the open_anywhere power
       */
      if (!(tport_control_ok(player, victim, loc) &&
	    (controls(player, destination) || CanOpen(player, destination)))) {
	notify(player, T("Permission denied."));
	return;
      }
      /* Remove it from its old room */
      Exits(loc) = remove_first(Exits(loc), victim);
      /* Put it into its new room */
      Source(victim) = destination;
      PUSH(victim, Exits(destination));
      if (!Quiet(player) && !(Quiet(victim) && (Owner(victim) == player)))
	notify(player, T("Teleported."));
      return;
    }
    loc = Location(victim);

    /* if properly empowered and destination is player, tel to location */
    if (IsPlayer(destination) && Can_Locate(player,destination) && Tel_Where(player, destination) && IsPlayer(victim)
	&& !inside) {
      if (!silent && loc != Location(destination))
	did_it(victim, victim, NULL, NULL, "OXTPORT", NULL, NULL, loc);
      safe_tel(victim, Location(destination), silent);
      if (!silent && loc != Location(destination))
	did_it(victim, victim, "TPORT", NULL, "OTPORT", NULL, "ATPORT",
	       Location(destination));
      return;
    }
    /* check needed for NOTHING. Especially important for unlinked exits */
    if ((absroom = Location(victim)) == NOTHING) {
      notify(victim, T("You're in the Void. This is not a good thing."));
      /* At this point, they're in a bad location, so let's check
       * if home is valid before sending them there. */
      if (!GoodObject(Home(victim)))
	Home(victim) = PLAYER_START;
      do_move(victim, "home", 0);
      return;
    } else {
      /* valid location, perform other checks */

      /* if player is inside himself, send him home */
      if (absroom == victim) {
	notify(player, T("What are you doing inside of yourself?"));
	if (Home(victim) == absroom)
	  Home(victim) = PLAYER_START;
	do_move(victim, "home", 0);
	return;
      }
      /* find the "absolute" room */
      absroom = absolute_room(victim);

      if (absroom == NOTHING) {
	notify(victim, T("You're in the void - sending you home."));
	if (Home(victim) == Location(victim))
	  Home(victim) = PLAYER_START;
	do_move(victim, "home", 0);
	return;
      }
      /* if there are a lot of containers, send him home */
      if (absroom == AMBIGUOUS) {
	notify(victim, T("You're in too many containers."));
	if (Home(victim) == Location(victim))
	  Home(victim) = PLAYER_START;
	do_move(victim, "home", 0);
	return;
      }
      /* note that we check the NO_TEL status of the victim rather
       * than the player that issued the command. This prevents someone
       * in a NO_TEL room from having one of his objects @tel him out.
       * The control check, however, is detemined by command-giving
       * player. */

      /* now check to see if the absolute room is set NO_TEL */
      if (NoTel(absroom) && !controls(player, absroom)
	  && !Tel_Where(player, absroom)) {
	notify(player, T("Teleports are not allowed in this room."));
	return;
      }

      /* Check leave lock on room, if necessary */
      if (!controls(player, absroom) && !Tel_Where(player, absroom) &&
	  !eval_lock(player, absroom, Leave_Lock)) {
	fail_lock(player, absroom, Leave_Lock,
		  T("Teleports are not allowed in this room."), NOTHING);
	return;
      }

      /* Now check the Z_TEL status of the victim's room.
       * Just like NO_TEL above, except that if the room (or its
       * Zone Master Room, if any, is Z_TEL,
       * the destination must also be a room in the same zone
       */
      if (GoodObject(Zone(absroom)) && (ZTel(absroom) || ZTel(Zone(absroom)))
	  && !controls(player, absroom) && !Tel_Where(player, absroom)
	  && (Zone(absroom) != Zone(destination))) {
	notify(player,
	       T("You may not teleport out of the zone from this room."));
	return;
      }
    }

    if (!IsExit(destination)) {
      if (tport_control_ok(player, victim, Location(victim)) &&
	  tport_dest_ok(player, victim, destination)
	  && (Tel_Thing(player, victim) ||
	      (Tel_Where(player, destination) && (player == victim)) ||
	      (destination == Owner(victim)) ||
	      (!Fixed(Owner(victim)) && !Fixed(player)))) {
	if (!silent && loc != destination)
	  did_it(victim, victim, NULL, NULL, "OXTPORT", NULL, NULL, loc);
	safe_tel(victim, destination, silent);
	if (!silent && loc != destination)
	  did_it(victim, victim, "TPORT", NULL, "OTPORT", NULL, "ATPORT",
		 destination);
	if ((victim != player) && !(Puppet(victim) &&
				    (Owner(victim) == Owner(player)))) {
	  if (!Quiet(player) && !(Quiet(victim) && (Owner(victim) == player)))
	    notify(player, T("Teleported."));
	}
	return;
      }
      /* we can't do it */
      fail_lock(player, destination, Enter_Lock, T("Permission denied."),
		Location(player));
      return;
    } else {
      /* attempted teleport to an exit */
      if ((Tel_Thing(player, victim) || controls(player, victim)
	   || controls(player, Location(victim)))
	  && !Fixed(Owner(victim)) && !Fixed(player)) {
	do_move(victim, to, 0);
      } else {
	notify(player, T("Permission denied."));
	if (victim != player)
	  notify_format(victim,
			T("%s tries to impose his will on you and fails."),
			Name(player));
      }
    }
  }
}

/** Force an object to run a command.
 * \verbatim
 * This implements @force.
 * \endverbatim
 * \param player the enactor.
 * \param what name of the object to force.
 * \param command command to force the object to run.
 */
void
do_force(dbref player, const char *what, char *command)
{
  dbref victim;
  char l = 0;
  int j;

  if ((victim = match_controlled(player, what)) == NOTHING) {
    notify(player, "Sorry.");
    return;
  }
  if (options.log_forces) {
    {
      /* Log forces when necessary */
      if (Owner(victim) != Owner(player))
	do_log(LT_WIZ, player, victim, "** FORCE: %s", command);
    }
  }
  if (God(victim) && !God(player)) {
    notify(player, T("You can't force God!"));
    return;
  }
  /* force victim to do command */
  for (j = 0; j < 10; j++)
    global_eval_context.wnxt[j] = global_eval_context.wenv[j];
  for (j = 0; j < NUMQ; j++)
    global_eval_context.rnxt[j] = global_eval_context.renv[j];
  if(ooref == NOTHING) {
      ooref = player;
      l = 1;
  }
  parse_que(victim, command, player);
  if(l==1) ooref = NOTHING;
}

/** Parse a force token command, but don't force with it.
 * \verbatim
 * This function hacks up something of the form "#<dbref> <action>",
 * finding the two args, and returns 1 if it's a sensible force,
 * otherwise 0. We know only that the command starts with #.
 * \endverbatim
 * \param command the command to parse.
 * \retval 1 sensible force command
 * \retval 0 command failed (no action given, etc.)
 */
int
parse_force(char *command)
{
  char *s;

  s = command + 1;
  while (*s && !isspace((unsigned char) *s)) {
    if (!isdigit((unsigned char) *s))
      return 0;				/* #1a is no good */
    s++;
  }
  if (!*s)
    return 0;			/* dbref with no action is no good */
  *s = '=';			/* Replace the first space with = so we have #3= <action> */
  return 1;
}


extern struct db_stat_info current_state;

/** Count up the number of objects of each type owned.
 * \param owner player to count for (or ANY_OWNER for all).
 * \return pointer to a static db_stat_info structure.
 */
struct db_stat_info *
get_stats(dbref owner)
{
  dbref i;
  static struct db_stat_info si;

  if (owner == ANY_OWNER)
    return &current_state;

  si.total = si.rooms = si.exits = si.things = si.players = si.divisions = si.garbage = 0;
  for (i = 0; i < db_top; i++) {
    if (owner == ANY_OWNER || owner == Owner(i)) {
      si.total++;
      if (IsGarbage(i)) {
	si.garbage++;
      } else {
	switch (Typeof(i)) {
	case TYPE_ROOM:
	  si.rooms++;
	  break;
	case TYPE_EXIT:
	  si.exits++;
	  break;
	case TYPE_THING:
	  si.things++;
	  break;
	case TYPE_PLAYER:
	  si.players++;
	  break;
        case TYPE_DIVISION:
          si.divisions++;
          break;
	default:
	  break;
	}
      }
    }
  }
  return &si;
}

/** The stats command.
 * \verbatim
 * This implements @stats.
 * \endverbatim
 * \param player the enactor.
 * \param name name of player to check object stats for.
 */
void
do_stats(dbref player, const char *name)
{
  struct db_stat_info *si;
  dbref owner;

  if (*name == '\0')
    owner = ANY_OWNER;
  else if (*name == '#') {
    owner = atoi(&name[1]);
    if (!GoodObject(owner))
      owner = NOTHING;
    else if (!IsPlayer(owner))
      owner = NOTHING;
  } else if (strcasecmp(name, "me") == 0)
    owner = player;
  else
    owner = lookup_player(name);
  if (owner == NOTHING) {
    notify_format(player, T("%s: No such player."), name);
    return;
  }
  if (!CanSearch(player, owner)) {
    if (owner != ANY_OWNER && owner != player) {
      notify(player, T("You need a search warrant to do that!"));
      return;
    }
  }
  si = get_stats(owner);
  if (owner == ANY_OWNER) {
    notify_format(player,
		  T
		  ("%d objects = %d rooms, %d exits, %d things, %d players, %d divisions, %d garbage."),
		  si->total, si->rooms, si->exits, si->things, si->players,
		  si->divisions, si->garbage);
    if (first_free != NOTHING)
      notify_format(player, T("The next object to be created will be #%d."),
		    first_free);
  } else {
    notify_format(player,
		  T("%d objects = %d rooms, %d exits, %d things, %d players, %d divisions."),
		  si->total - si->garbage, si->rooms, si->exits, si->things,
		  si->players, si->divisions);
  }
}

/** Reset a player's password.
 * \verbatim
 * This implements @newpassword.
 * \endverbatim
 * \param player the executor.
 * \param cause the enactor.
 * \param name the name of the player whose password is to be reset.
 * \param password the new password for the player.
 */
void
do_newpassword(dbref player, dbref cause,
	       const char *name, const char *password)
{
  dbref victim;

  if (!global_eval_context.process_command_port) {
    char pass_eval[BUFFER_LEN];
    char const *sp;
    char *bp;

    sp = password;
    bp = pass_eval;
    process_expression(pass_eval, &bp, &sp, player, player, cause,
		       PE_DEFAULT, PT_DEFAULT, NULL);
    *bp = '\0';
    password = pass_eval;
  }

  if (((victim = lookup_player(name)) == NOTHING) && 
      ((victim = match_result(player, name, TYPE_DIVISION, MAT_ABSOLUTE | MAT_NEIGHBOR | MAT_NEAR | MAT_ENGLISH)) == NOTHING || 
     Typeof(victim) != TYPE_DIVISION )) {
    notify(player, T("No such player or division."));
  } else if(CanNewpass(player, victim)
	    || (Can_BCREATE(player) && (has_flag_by_name(victim, "BUILDER",
							 TYPE_PLAYER))
		&& (LEVEL(player) >= LEVEL(victim)))) {
    if(Typeof(victim) != TYPE_DIVISION && Typeof(victim) != TYPE_PLAYER) {
       notify(player, "Wtf happened?");
	   return;
    }
    if (*password != '\0' && !ok_password(password)) {
      /* Wiz can set null passwords, but not bad passwords */
      notify(player, T("Bad password."));
    } else {
      /* it's ok, do it */
      (void) atr_add(victim, "XYXXY", mush_crypt(password), GOD, NOTHING);
      notify_format(player, T("Password for %s changed."), Name(victim));
      notify_format(victim, T("Your password has been changed by %s."),
		    Name(player));
      do_log(LT_WIZ, player, victim, "*** NEWPASSWORD ***");
    }
  } else {
    notify(player, T("Your delusions of grandeur have been duly noted."));
    return;
  }
}

/** Disconnect a player, forcibly.
 * \verbatim
 * This implements @boot.
 * \endverbatim
 * \param player the enactor.
 * \param name name of the player or descriptor to boot.
 * \param flag the type of booting to do.
 */
void
do_boot(dbref player, const char *name, enum boot_type flag)
{
  dbref victim;
  DESC *d = NULL;

  victim = NOTHING;
  switch (flag) {
  case BOOT_SELF:
    /* self boot */
    victim = player;
    break;
  case BOOT_DESC:
    /* boot by descriptor */
    victim = find_player_by_desc(atoi(name));
    if (victim == player)
      flag = BOOT_SELF;
    else if (victim == NOTHING) {
      d = port_desc(atoi(name));
      if (!d) {
	notify(player, "There is no one connected on that descriptor.");
	return;
      } else
	victim = AMBIGUOUS;
    }
    break;
  case BOOT_NAME:
    /* boot by name */
    if ((victim =
	 noisy_match_result(player, name, TYPE_PLAYER,
			    MAT_LIMITED | MAT_ME)) == NOTHING) {
      notify(player, T("No such connected player."));
      return;
    }
    if (victim == player)
      flag = BOOT_SELF;
    break;
  }

  if ((victim != player) && !Can_Boot(player, victim)) {
    notify(player, T("You can't boot that player!"));
    return;
  }
  if (God(victim) && !God(player)) {
    notify(player, T("You're good.  That's spelled with two 'o's."));
    return;
  }
  /* look up descriptor */
  switch (flag) {
  case BOOT_NAME:
    d = player_desc(victim);
    break;
  case BOOT_DESC:
    d = port_desc(atoi(name));
    break;
  case BOOT_SELF:
    d = inactive_desc(victim);
    break;
  }

  /* check for more errors */
  if (!d) {
    if (flag == BOOT_SELF)
      notify(player, T("None of your connections are idle."));
    else
      notify(player, T("That player isn't connected!"));
  } else {
    do_log(LT_WIZ, player, victim, "*** BOOT ***");
    if (flag == BOOT_SELF)
      notify(player, T("You boot an idle self."));
    else if (victim == AMBIGUOUS)
      notify_format(player, T("You booted unconnected port %s!"), name);
    else {
      notify_format(player, T("You booted %s off!"), Name(victim));
      notify(victim, T("You are politely shown to the door."));
    }
    boot_desc(d);
  }
}

/** Chown all of a player's objects.
 * \verbatim
 * This implements @chownall
 * \endverbatim
 * \param player the enactor.
 * \param name name of player whose objects are to be chowned.
 * \param target name of new owner for objects.
 * \param preserve if 1, keep privileges and don't halt objects.
 */
void
do_chownall(dbref player, const char *name, const char *target, int preserve)
{
  int i;
  dbref victim;
  dbref n_target;
  int count = 0;

  if ((victim = noisy_match_result(player, name, TYPE_PLAYER, MAT_LIMITED))
      == NOTHING)
    return;

  if(!controls(player, victim)) {
    notify(player, T("Try asking them first!"));
    return;
  }

  if (!target || !*target) {
    n_target = player;
  } else {
    if ((n_target =
	 noisy_match_result(player, target, TYPE_PLAYER,
			    MAT_LIMITED)) == NOTHING)
      return;
  }

  for (i = 0; i < db_top; i++) {
    if ((Owner(i) == victim) && (!IsPlayer(i))) {
      chown_object(player, i, n_target, preserve);
      count++;
    }
  }

  /* change quota (this command is admin only and we can assume that
   * we intend for the recipient to get all the objects, so we
   * don't do a quota check earlier.
   */
  change_quota(victim, count);
  change_quota(n_target, -count);

  notify_format(player, T("Ownership changed for %d objects."), count);
}

/** Change the zone of all of a player's objects.
 * \verbatim
 * This implements @chzoneall.
 * \endverbatim
 * \param player the enactor.
 * \param name name of player whose objects should be rezoned.
 * \param target string containing new zone master for objects.
 */
void
do_chzoneall(dbref player, const char *name, const char *target)
{
  int i;
  dbref victim;
  dbref zone;
  int count = 0;

  if ((victim = noisy_match_result(player, name, TYPE_PLAYER, MAT_LIMITED))
      == NOTHING)
    return;

  if (!(Director(player) || controls(player, victim))) {
    notify(player, T("You do not have the power to change reality."));
    return;
  }

  if (!target || !*target) {
    notify(player, T("No zone specified."));
    return;
  }
  if (!strcasecmp(target, "none"))
    zone = NOTHING;
  else {
    switch (zone = match_result(player, target, NOTYPE, MAT_EVERYTHING)) {
    case NOTHING:
      notify(player, T("I can't seem to find that."));
      return;
    case AMBIGUOUS:
      notify(player, T("I don't know which one you mean!"));
      return;
    }
  }

  /* Okay, now that we know we're not going to spew all sorts of errors,
   * call the normal do_chzone for all the relevant objects.  This keeps
   * consistency on things like flag resetting, etc... */
  for (i = 0; i < db_top; i++) {
    if (Owner(i) == victim && Zone(i) != zone) {
      count += do_chzone(player, unparse_dbref(i), target, 0);
    }
  }
  notify_format(player, T("Zone changed for %d objects."), count);
}

/*-----------------------------------------------------------------------
 * Nasty management: @kick, examine/debug
 */

/** Execute a number of commands off the queue immediately.
 * \verbatim
 * This implements @kick, which is nasty.
 * \endverbatim
 * \param player the enactor.
 * \param num string containing number of commands to run from the queue.
 */
void
do_kick(dbref player, const char *num)
{
  int n;

  if (!num || !*num) {
    notify(player, T("How many commands do you want to execute?"));
    return;
  }
  n = atoi(num);

  if (n <= 0) {
    notify(player, T("Number out of range."));
    return;
  }
  n = do_top(n);

  notify_format(player, T("%d commands executed."), n);
}

/** examine/debug.
 * This implements examine/debug, which provides some raw values for
 * object structure elements of an examined object.
 * \param player the enactor.
 * \param name name of object to examine.
 */
void
do_debug_examine(dbref player, const char *name)
{
  MAIL *mp;
  dbref thing;

  if (!Admin(player)) {
    notify(player, T("Permission denied."));
    return;
  }
  /* find it */
  thing = noisy_match_result(player, name, NOTYPE, MAT_EVERYTHING);
  if (!GoodObject(thing))
    return;

  notify(player, object_header(player, thing));
  notify_format(player, T("Flags value: %s"),
		bits_to_string("FLAG", Flags(thing), GOD, NOTHING));

  notify_format(player, "Next: %d", Next(thing));
  notify_format(player, "Contents: %d", Contents(thing));

  switch (Typeof(thing)) {
  case TYPE_PLAYER:
    mp = desc_mail(thing);
    notify_format(player, T("First mail sender: %d"), mp ? mp->from : NOTHING);
  case TYPE_THING:
    notify_format(player, "Location: %d", Location(thing));
    notify_format(player, "Home: %d", Home(thing));
    break;
  case TYPE_EXIT:
    notify_format(player, "Destination: %d", Location(thing));
    notify_format(player, "Source: %d", Source(thing));
    break;
  case TYPE_ROOM:
    notify_format(player, "Drop-to: %d", Location(thing));
    notify_format(player, "Exits: %d", Exits(thing));
    break;
  case TYPE_DIVISION:
    notify_format(player, "Location: %d", Location(thing));
    notify_format(player, "Home: %d", Home(thing));
  case TYPE_GARBAGE:
    break;
  default:
    notify(player, T("Bad object type."));
  }
}

/*----------------------------------------------------------------------------
 * Search functions
 */

/** User command to search the db for matching objects.
 * \verbatim
 * This implements @search.
 * \endverbatim
 * \param player the enactor.
 * \param arg1 name of player whose objects are to be searched.
 * \param arg3 additional search arguments.
 */
void
do_search(dbref player, const char *arg1, char **arg3)
{
  char tbuf[BUFFER_LEN], *arg2 = tbuf, *tbp;
  dbref *results = NULL;
  int nresults;

  /* parse first argument into two */
  if (!arg1 || *arg1 == '\0')
    arg1 = "me";

  /* First argument is a player, so we could have a quoted name */
  if (PLAYER_NAME_SPACES && *arg1 == '\"') {
    for (; *arg1 && ((*arg1 == '\"') || isspace((unsigned char) *arg1));
	 arg1++) ;
    strcpy(tbuf, arg1);
    while (*arg2 && (*arg2 != '\"')) {
      while (*arg2 && (*arg2 != '\"'))
	arg2++;
      if (*arg2 == '\"') {
	*arg2++ = '\0';
	while (*arg2 && isspace((unsigned char) *arg2))
	  arg2++;
	break;
      }
    }
  } else {
    strcpy(tbuf, arg1);
    while (*arg2 && !isspace((unsigned char) *arg2))
      arg2++;
    if (*arg2)
      *arg2++ = '\0';
    while (*arg2 && isspace((unsigned char) *arg2))
      arg2++;
  }

  if (!*arg2) {
    if (!arg3[1] || !*arg3[1])
      arg2 = (char *) "";	/* arg1 */
    else {
      arg2 = (char *) arg1;	/* arg2=arg3 */
      tbuf[0] = '\0';
    }
  }
  {
    const char *myargs[4];
    myargs[0] = arg2;
    myargs[1] = arg3[1];
    myargs[2] = arg3[2];
    myargs[3] = arg3[3];
    nresults = raw_search(player, tbuf, 4, myargs, &results, NULL);
  }

  if (nresults == 0) {
    notify(player, T("Nothing found."));
  } else if (nresults > 0) {
    /* Split the results up by type and report. */
    int n;
    int nthings = 0, nexits = 0, nrooms = 0, nplayers = 0, ndivisions = 0;
    dbref *things, *exits, *rooms, *players, *divisions;

    things = (dbref *) mush_malloc(sizeof(dbref) * nresults, "dbref_list");
    exits = (dbref *) mush_malloc(sizeof(dbref) * nresults, "dbref_list");
    rooms = (dbref *) mush_malloc(sizeof(dbref) * nresults, "dbref_list");
    players = (dbref *) mush_malloc(sizeof(dbref) * nresults, "dbref_list");
    divisions = (dbref *) mush_malloc(sizeof(dbref) * nresults, "dbref_list");

    for (n = 0; n < nresults; n++) {
      switch (Typeof(results[n])) {
      case TYPE_THING:
	things[nthings++] = results[n];
	break;
      case TYPE_EXIT:
	exits[nexits++] = results[n];
	break;
      case TYPE_ROOM:
	rooms[nrooms++] = results[n];
	break;
      case TYPE_PLAYER:
	players[nplayers++] = results[n];
	break;
      case TYPE_DIVISION:
        divisions[ndivisions++] = results[n];
        break;
      default:
	/* Unknown type. Ignore. */
	do_rawlog(LT_ERR, T("Weird type for dbref #%d"), results[n]);
      }
    }

    if (nrooms) {
      notify(player, "\nROOMS:");
      for (n = 0; n < nrooms; n++) {
	tbp = tbuf;
	safe_format(tbuf, &tbp, "%s [owner: ", object_header(player, rooms[n]));
	safe_str(object_header(player, Owner(rooms[n])), tbuf, &tbp);
	safe_chr(']', tbuf, &tbp);
	*tbp = '\0';
	notify(player, tbuf);
      }
    }

    if (nexits) {
      dbref from, to;

      notify(player, "\nEXITS:");
      for (n = 0; n < nexits; n++) {
	tbp = tbuf;
	if (Source(exits[n]) == NOTHING)
	  from = NOTHING;
	else
	  from = Source(exits[n]);
	to = Destination(exits[n]);
	safe_format(tbuf, &tbp, "%s [from ", object_header(player, exits[n]));
	safe_str((from == NOTHING) ? "NOWHERE" : object_header(player, from),
		 tbuf, &tbp);
	safe_str(" to ", tbuf, &tbp);
	safe_str((to == NOTHING) ? "NOWHERE" : object_header(player, to),
		 tbuf, &tbp);
	safe_chr(']', tbuf, &tbp);
	*tbp = '\0';
	notify(player, tbuf);
      }
    }

    if (nthings) {
      notify(player, "\nTHINGS:");
      for (n = 0; n < nthings; n++) {
	tbp = tbuf;
	safe_format(tbuf, &tbp, "%s [owner: ",
		    object_header(player, things[n]));
	safe_str(object_header(player, Owner(things[n])), tbuf, &tbp);
	safe_chr(']', tbuf, &tbp);
	*tbp = '\0';
	notify(player, tbuf);
      }
    }

    if (nplayers) {
      notify(player, "\nPLAYERS:");
      for (n = 0; n < nplayers; n++) {
	tbp = tbuf;
	safe_str(object_header(player, players[n]), tbuf, &tbp);
	if (Can_Examine(player, players[n]))
	  safe_format(tbuf, &tbp,
		      T(" [location: %s]"),
		      object_header(player, Location(players[n])));
	*tbp = '\0';
	notify(player, tbuf);
      }
    }

    if (ndivisions) {
      notify(player, "\nDIVISIONS:");
      for (n = 0; n < ndivisions; n++) {
	tbp = tbuf;
	safe_format(tbuf, &tbp, "%s [owner: ",
		    object_header(player, divisions[n]));
	safe_str(object_header(player, Owner(divisions[n])), tbuf, &tbp);
	safe_chr(']', tbuf, &tbp);
	*tbp = '\0';
	notify(player, tbuf);
      }
    }

    notify(player, T("----------  Search Done  ----------"));
    notify_format(player,
		  T
		  ("Totals: Rooms...%d  Exits...%d  Things...%d  Players...%d  Divisions...%d"),
		  nrooms, nexits, nthings, nplayers, ndivisions);
    mush_free((Malloc_t) rooms, "dbref_list");
    mush_free((Malloc_t) exits, "dbref_list");
    mush_free((Malloc_t) things, "dbref_list");
    mush_free((Malloc_t) players, "dbref_list");
    mush_free((Malloc_t) divisions, "dbref_list");
  }
  if (results)
    mush_free(results, "search_results");
}

FUNCTION(fun_lsearch)
{
  int nresults;
  dbref *results = NULL;
  int rev = !strcmp(called_as, "LSEARCHR");

  if (!command_check_byname(executor, "@search")) {
    safe_str(T(e_perm), buff, bp);
    return;
  }

  if (!strcmp(called_as, "CHILDREN")) {
    const char *myargs[2];
    myargs[0] = "PARENT";
    myargs[1] = args[0];
    nresults = raw_search(executor, NULL, 2, myargs, &results, pe_info);
  } else
    nresults =
      raw_search(executor, args[0], nargs - 1, (const char **) (args + 1),
		 &results, pe_info);

  if (nresults < 0) {
    safe_str("#-1", buff, bp);
  } else if (nresults == 0) {
    notify(executor, T("Nothing found."));
  } else {
    int first = 1, n;
    if (!rev) {
      for (n = 0; n < nresults; n++) {
	if (first)
	  first = 0;
	else if (safe_chr(' ', buff, bp))
	  break;
	if (safe_dbref(results[n], buff, bp))
	  break;
      }
    } else {
      for (n = nresults - 1; n >= 0; n--) {
	if (first)
	  first = 0;
	else if (safe_chr(' ', buff, bp))
	  break;
	if (safe_dbref(results[n], buff, bp))
	  break;
      }
    }
  }
  if (results)
    mush_free(results, "search_results");
}


#ifdef WIN32
#pragma warning( disable : 4761)	/* Disable bogus conversion warning */
#endif
/* ARGSUSED */
FUNCTION(fun_hidden)
{
  dbref it = match_thing(executor, args[0]);
  if (CanSee(executor, it) && ((Admin(executor) ||
				  Location(executor) == Location(it) || Location(it) == executor))) {
  if ((it == NOTHING) || (!IsPlayer(it))) {
    notify(executor, T("Couldn't find that player."));
    safe_str("#-1", buff, bp);
    return;
  }
  safe_boolean(hidden(it), buff, bp);
  return;
  } else { 
	  notify(executor, T("Permission denied.")); 
	  safe_str("#-1", buff, bp); 
	  return;
  }
}

#ifdef WIN32
#pragma warning( default : 4761)	/* Re-enable conversion warning */
#endif

/* ARGSUSED */
FUNCTION(fun_quota)
{
  int owned;
  /* Tell us player's quota */
  dbref thing;
  dbref who = lookup_player(args[0]);
  if ((who == NOTHING) || !IsPlayer(who)) {
    notify(executor, T("Couldn't find that player."));
    safe_str("#-1", buff, bp);
    return;
  }
  if (!(Do_Quotas(executor, who) || CanSee(executor, who)
	|| controls(executor, who))) {
    notify(executor, T("You can't see someone else's quota!"));
    safe_str("#-1", buff, bp);
    return;
  }
  if (NoQuota(who)) {
    /* Unlimited, but return a big number to be sensible */
    safe_str("99999", buff, bp);
    return;
  }
  /* count up all owned objects */
  owned = -1;			/* a player is never included in his own
				 * quota */
  for (thing = 0; thing < db_top; thing++) {
    if (Owner(thing) == who)
      if (!IsGarbage(thing))
	++owned;
  }

  safe_integer(owned + get_current_quota(who), buff, bp);
  return;
}

/** Modify access rules for a site.
 * \verbatim
 * This implements @sitelock.
 * \endverbatim
 * \param player the enactor.
 * \param site name of site
 * \param opts access rules to apply.
 * \param who string containing dbref of player to whom rule applies.
 * \param type sitelock operation to do.
 */
void
do_sitelock(dbref player, const char *site, const char *opts, const char *who,
	    enum sitelock_type type)
{

  if (opts && *opts) {
    int can, cant;
    dbref whod = AMBIGUOUS;
    /* Options form of the command. */
    if (!site || !*site) {
      notify(player, T("What site did you want to lock?"));
      return;
    }
    can = cant = 0;
    if (!parse_access_options(opts, NULL, &can, &cant, player)) {
      notify(player, T("No valid options found."));
      return;
    }
    if (who && *who) {		/* Specify a character */
      whod = lookup_player(who);
      if (!GoodObject(whod)) {
	notify(player, T("Who do you want to lock?"));
	return;
      }
    }

    add_access_sitelock(player, site, whod, can, cant);
    write_access_file();
    if (whod != AMBIGUOUS) {
      notify_format(player,
		    T("Site %s access options for %s(%s) set to %s"),
		    site, Name(whod), unparse_dbref(whod), opts);
      do_log(LT_WIZ, player, NOTHING,
	     T("*** SITELOCK *** %s for %s(%s) --> %s"), site,
	     Name(whod), unparse_dbref(whod), opts);
    } else {
      notify_format(player, T("Site %s access options set to %s"), site, opts);
      do_log(LT_WIZ, player, NOTHING, "*** SITELOCK *** %s --> %s", site, opts);
    }
    return;
  } else {
    /* Backward-compatible non-options form of the command,
     * or @sitelock/name
     */
    switch (type) {
    case SITELOCK_LIST:
      /* List bad sites */
      do_list_access(player);
      return;
    case SITELOCK_ADD:
      add_access_sitelock(player, site, AMBIGUOUS, 0, ACS_CREATE);
      write_access_file();
      notify_format(player, T("Site %s locked"), site);
      do_log(LT_WIZ, player, NOTHING, "*** SITELOCK *** %s", site);
      break;
    case SITELOCK_BAN:
      add_access_sitelock(player, site, AMBIGUOUS, 0, ACS_DEFAULT);
      write_access_file();
      notify_format(player, T("Site %s banned"), site);
      do_log(LT_WIZ, player, NOTHING, "*** SITELOCK *** %s", site);
      break;
    case SITELOCK_CHECK:{
	struct access *ap;
	char tbuf[BUFFER_LEN], *bp;
	int rulenum;
	if (!site || !*site) {
	  do_list_access(player);
	  return;
	}
	ap = site_check_access(site, AMBIGUOUS, &rulenum);
	bp = tbuf;
	format_access(ap, rulenum, AMBIGUOUS, tbuf, &bp);
	*bp = '\0';
	notify(player, tbuf);
	break;
      }
    case SITELOCK_REMOVE:{
	int n;
	n = remove_access_sitelock(site);
	if (n > 0)
	  write_access_file();
	notify_format(player, T("%d sitelocks removed."), n);
	break;
      }
    }
  }
}

/** Edit the list of restricted player names
 * \verbatim
 * This implements @sitelock/name
 * \endverbatim
 * \param player the player doing the command.
 * \param name the name (Actually, wildcard pattern) to restrict.
 */
void
do_sitelock_name(dbref player, const char *name)
{
  FILE *fp, *fptmp;
  char buffer[BUFFER_LEN];
  char *p;

  if (!Site(player)) {
    notify(player, T("Your delusions of grandeur have been noted."));
    return;
  }

  release_fd();

  if (!name || !*name) {
    /* List bad names */
    if ((fp = fopen(NAMES_FILE, FOPEN_READ)) == NULL) {
      notify(player, T("Unable to open names file."));
    } else {
      notify(player, T("Any name matching these wildcard patterns is banned:"));
      while (fgets(buffer, sizeof buffer, fp)) {
	if ((p = strchr(buffer, '\r')) != NULL)
	  *p = '\0';
	else if ((p = strchr(buffer, '\n')) != NULL)
	  *p = '\0';
	notify(player, buffer);
      }
      fclose(fp);
    }
  } else if (name[0] == '!') {	/* Delete a name */
    if ((fp = fopen(NAMES_FILE, FOPEN_READ)) != NULL) {
      if ((fptmp = fopen("tmp.tmp", FOPEN_WRITE)) == NULL) {
	notify(player, T("Unable to delete name."));
	fclose(fp);
      } else {
	while (fgets(buffer, sizeof buffer, fp)) {
	  if ((p = strchr(buffer, '\r')) != NULL)
	    *p = '\0';
	  else if ((p = strchr(buffer, '\n')) != NULL)
	    *p = '\0';
	  if (strcasecmp(buffer, name + 1) == 0)
	    /* Replace the name with #NAME, to allow things like
	       keeping track of unlocked feature names. */
	    fprintf(fptmp, "#%s\n", buffer);
	  else
	    fprintf(fptmp, "%s\n", buffer);
	}
	fclose(fp);
	fclose(fptmp);
#ifdef WIN32
	/* Windows can't rename to an existing file. */
	if (unlink(NAMES_FILE) != 0) {
	  notify(player, T("Unable to delete name."));
	} else
#endif
	if (rename("tmp.tmp", NAMES_FILE) == 0) {
	  notify(player, T("Name removed."));
	  do_log(LT_WIZ, player, NOTHING, "*** UNLOCKED NAME *** %s", name + 1);
	} else {
	  notify(player, T("Unable to delete name."));
	}
      }
    } else
      notify(player, T("Unable to delete name."));
  } else {			/* Add a name */
    if ((fp = fopen(NAMES_FILE, FOPEN_READ)) != NULL) {
      if ((fptmp = fopen("tmp.tmp", FOPEN_WRITE)) == NULL) {
	notify(player, T("Unable to lock name."));
      } else {
	/* Read the names file, looking for #NAME and writing it
	   without the commenting #. Otherwise, add the new name
	   to the end of the file */
	char commented[BUFFER_LEN + 1];
	int found = 0;
	commented[0] = '#';
	strcpy(commented + 1, name);
	while (fgets(buffer, sizeof buffer, fp) != NULL) {
	  if ((p = strchr(buffer, '\r')) != NULL)
	    *p = '\0';
	  else if ((p = strchr(buffer, '\n')) != NULL)
	    *p = '\0';
	  if (strcasecmp(commented, buffer) == 0) {
	    fprintf(fptmp, "%s\n", name);
	    found = 1;
	  } else
	    fprintf(fptmp, "%s\n", buffer);
	}
	if (!found)
	  fprintf(fptmp, "%s\n", name);
	fclose(fp);
	fclose(fptmp);

#ifdef WIN32
	/* Windows can't rename to an existing file. */
	if (unlink(NAMES_FILE) != 0) {
	  notify(player, T("Unable to lock name."));
	} else
#endif
	if (rename("tmp.tmp", NAMES_FILE) == 0) {
	  notify_format(player, T("Name %s locked."), name);
	  do_log(LT_WIZ, player, NOTHING, "*** NAMELOCK *** %s", name);
	} else
	  notify(player, T("Unable to lock name."));
      }
    }
  }
  reserve_fd();
}

/*-----------------------------------------------------------------
 * Functions which give memory information on objects or players.
 * Source code originally by Kalkin, modified by Javelin
 */

static int
mem_usage(dbref thing)
{
  int k;
  ATTR *m;
  lock_list *l;
  k = sizeof(struct object);	/* overhead */
  k += strlen(Name(thing)) + 1;	/* The name */
  for (m = List(thing); m; m = AL_NEXT(m)) {
    k += sizeof(ATTR);
    if (AL_STR(m) && *AL_STR(m))
      k += u_strlen(AL_STR(m)) + 1;
    /* NOTE! In the above, we're getting the size of the
     * compressed attrib, not the uncompressed one (as Kalkin did)
     * because (1) it's more efficient, (2) it's more accurate
     * since that's how the object is in memory. This relies on
     * compressed attribs being terminated with \0's, which they
     * are in compress.c. If that changes, this breaks.
     */
  }
  for (l = Locks(thing); l; l = l->next) {
    k += sizeof(lock_list);
    k += sizeof_boolexp(l->key);
  }
  return k;
}

/* ARGSUSED */
FUNCTION(fun_objmem)
{
  dbref thing;
  if (!strcasecmp(args[0], "me"))
    thing = executor;
  else if (!strcasecmp(args[0], "here"))
    thing = Location(executor);
  else {
    thing = noisy_match_result(executor, args[0], NOTYPE, MAT_OBJECTS);
  }

  if (!CanSearch(executor, thing)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  if (!GoodObject(thing)) {
    safe_str(T(e_match), buff, bp);
    return;
  }
  if (!Can_Examine(executor, thing)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  safe_integer(mem_usage(thing), buff, bp);
}



/* ARGSUSED */
FUNCTION(fun_playermem)
{
  int tot = 0;
  dbref thing;
  dbref j;

  if (!strcasecmp(args[0], "me") && IsPlayer(executor))
    thing = executor;
  else if (*args[0] && *args[0] == '*')
    thing = lookup_player(args[0] + 1);
  else if (*args[0] && *args[0] == '#')
    thing = atoi(args[0] + 1);
  else
    thing = lookup_player(args[0]);
  if (!GoodObject(thing) || !IsPlayer(thing)) {
    safe_str(T(e_match), buff, bp);
    return;
  }
  if (!CanSearch(executor, thing) || !Can_Examine(executor, thing)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  for (j = 0; j < db_top; j++)
    if (Owner(j) == thing)
      tot += mem_usage(j);
  safe_integer(tot, buff, bp);
}


/** Reboot the game without disconnecting players.
 * \verbatim
 * This implements @shutdown/reboot, which performs a dump, saves
 * information about which player is associated with which socket,
 * and then re-execs the mush process without closing the sockets.
 * \endverbatim
 * \param player the enactor.
 * \param flag if 0, normal dump; if 1, paranoid dump.
 */
void
do_reboot(dbref player, int flag)
{
  if (player == NOTHING) {
    flag_broadcast(0, 0,
		   T
		   ("GAME: Reboot w/o disconnect from game account, please wait."));
  } else {
    flag_broadcast(0, 0,
		   T
		   ("GAME: Reboot w/o disconnect by %s, please wait."),
		   Name(Owner(player)));
  }
  if (flag) {
    globals.paranoid_dump = 1;
    globals.paranoid_checkpt = db_top / 5;
    if (globals.paranoid_checkpt < 1)
      globals.paranoid_checkpt = 1;
  }
#ifdef HAS_OPENSSL
  close_ssl_connections();
#endif
  sql_shutdown();
  shutdown_queues();
  fork_and_dump(0);
#ifndef PROFILING
#ifndef WIN32
  /* Some broken libcs appear to retain the itimer across exec!
   * So we make sure that if we get a SIGPROF in our next incarnation,
   * we ignore it until our proper handler is set up.
   */
  ignore_signal(SIGPROF);
#endif
#endif
  dump_reboot_db();
#ifdef INFO_SLAVE
  kill_info_slave();
#endif
  local_shutdown();
  end_all_logs();
#ifndef WIN32
  execl("netmush", "netmush", confname, NULL);
#else
  execl("pennmush.exe", "pennmush.exe", "/run", NULL);
#endif				/* WIN32 */
  exit(1);			/* Shouldn't ever get here, but just in case... */
}


static int
fill_search_spec(dbref player, const char *owner, int nargs, const char **args,
               struct search_spec *spec)
{
  int n;
  const char *class, *restriction;

  spec->zone = spec->parent = spec->owner = ANY_OWNER;
  spec->type = NOTYPE;
  strcpy(spec->flags, "");
  strcpy(spec->lflags, "");
  strcpy(spec->powers, "");
  strcpy(spec->eval, "");
  strcpy(spec->name, "");
  spec->low = 0;
  spec->high = db_top - 1;

  /* set limits on who we search */
  if (!owner || !*owner || strcasecmp(owner, "all") == 0)
    spec->owner = ANY_OWNER;
  else if (strcasecmp(owner, "me") == 0)
    spec->owner = player;
  else
    spec->owner = lookup_player(owner);
  if (spec->owner == NOTHING) {
    notify(player, T("Unknown owner."));
    return -1;
  }

  for (n = 0; n < nargs - 1; n += 2) {
    class = args[n];
    restriction = args[n + 1];
    /* A special old-timey kludge */
    if (class && !*class && restriction && *restriction) {
      if (isdigit(*restriction) || ((*restriction == '#') && *(restriction + 1)
				    && isdigit(*(restriction + 1)))) {
      size_t offset = 0;
      if (*restriction == '#')
        offset = 1;
      spec->high = parse_integer(restriction + offset);
      if (!GoodObject(spec->high))
	spec->high = db_top - 1;
	continue;
      }
    }
    if (!class || !*class || !restriction)
      continue;
    if (isdigit(*class) ||
      ((*class == '#') && *(class + 1) && isdigit(*(class + 1)))) {
      size_t offset = 0;
      if (*class == '#')
      offset = 1;
      spec->low = parse_integer(class + offset);
      if (!GoodObject(spec->low))
      spec->low = 0;
      if (isdigit(*restriction) || ((*restriction == '#') && *(restriction + 1)
				    && isdigit(*(restriction + 1)))) {
	offset = 0;
	if (*restriction == '#')
	  offset = 1;
	spec->high = parse_integer(restriction + offset);
	if (!GoodObject(spec->high))
	  spec->high = db_top - 1;
      }
      continue;
    }
    /* Figure out the class */
    /* Old-fashioned way to select everything */
    if (string_prefix("none", class))
      continue;
    if (string_prefix("mindb", class)) {
      size_t offset = 0;
      if (*restriction == '#')
      offset = 1;
      spec->low = parse_integer(restriction + offset);
      if (!GoodObject(spec->low))
	spec->low = 0;
      continue;
    } else if (string_prefix("maxdb", class)) {
      size_t offset = 0;
      if (*restriction == '#')
	offset = 1;
      spec->high = parse_integer(restriction + offset);
      if (!GoodObject(spec->high))
	spec->low = db_top - 1;
      continue;
    }

    if (string_prefix("type", class)) {
      if (string_prefix("things", restriction)
	  || string_prefix("objects", restriction)) {
	spec->type = TYPE_THING;
      } else if (string_prefix("rooms", restriction)) {
	spec->type = TYPE_ROOM;
      } else if (string_prefix("exits", restriction)) {
	spec->type = TYPE_EXIT;
      } else if (string_prefix("rooms", restriction)) {
	spec->type = TYPE_ROOM;
      } else if (string_prefix("players", restriction)) {
	spec->type = TYPE_PLAYER;
      } else {
	notify(player, T("Unknown type."));
	return -1;
      }
    } else if (string_prefix("things", class)
	       || string_prefix("objects", class)) {
      strcpy(spec->name, restriction);
      spec->type = TYPE_THING;
    } else if (string_prefix("exits", class)) {
      strcpy(spec->name, restriction);
      spec->type = TYPE_EXIT;
    } else if (string_prefix("rooms", class)) {
      strcpy(spec->name, restriction);
      spec->type = TYPE_ROOM;
    } else if (string_prefix("players", class)) {
      strcpy(spec->name, restriction);
      spec->type = TYPE_PLAYER;
    } else if (string_prefix("name", class)) {
      strcpy(spec->name, restriction);
    } else if (string_prefix("parent", class)) {
      if (!*restriction) {
	spec->parent = NOTHING;
	continue;
      }
      if (!is_objid(restriction)) {
	notify(player, T("Unknown parent."));
	return -1;
      }
      spec->parent = parse_objid(restriction);
      if (!GoodObject(spec->parent)) {
	notify(player, T("Unknown parent."));
	return -1;
      }
    } else if (string_prefix("zone", class)) {
      if (!*restriction) {
	spec->zone = NOTHING;
	continue;
      }
      if (!is_objid(restriction)) {
	notify(player, T("Unknown zone."));
	return -1;
      }
      spec->zone = parse_objid(restriction);
      if (!GoodObject(spec->zone)) {
	notify(player, T("Unknown zone."));
	return -1;
      }
    } else if (string_prefix("eval", class)) {
      strcpy(spec->eval, restriction);
    } else if (string_prefix("ethings", class) ||
	       string_prefix("eobjects", class)) {
      strcpy(spec->eval, restriction);
      spec->type = TYPE_THING;
    } else if (string_prefix("eexits", class)) {
      strcpy(spec->eval, restriction);
      spec->type = TYPE_EXIT;
    } else if (string_prefix("erooms", class)) {
      strcpy(spec->eval, restriction);
      spec->type = TYPE_ROOM;
    } else if (string_prefix("eplayers", class)) {
      strcpy(spec->eval, restriction);
      spec->type = TYPE_PLAYER;
    } else if (string_prefix("powers", class)) {
      /* Handle the checking later.  */
      if (!restriction || !*restriction) {
	notify(player, T("You must give a list of power names."));
	return -1;
      }
      strcpy(spec->powers, restriction);
    } else if (string_prefix("flags", class)) {
      /* Handle the checking later.  */
      if (!restriction || !*restriction) {
	notify(player, T("You must give a string of flag characters."));
	return -1;
      }
      strcpy(spec->flags, restriction);
    } else if (string_prefix("lflags", class)) {
      /* Handle the checking later.  */
      if (!restriction || !*restriction) {
	notify(player, T("You must give a list of flag names."));
	return -1;
      }
      strcpy(spec->lflags, restriction);
    } else {
      notify(player, T("Unknown search class."));
      return -1;
    }
  }
  return 0;
}


/* Does the actual searching */
static int
raw_search(dbref player, const char *owner, int nargs, const char **args,
	   dbref **result, PE_Info * pe_info)
{
  size_t result_size;
  size_t nresults = 0;
  int n;
  struct search_spec spec;

  /* make sure player has money to do the search */
  if (!payfor(player, FIND_COST)) {
    notify_format(player, T("Searches cost %d %s."), FIND_COST,
		  ((FIND_COST == 1) ? MONEY : MONIES));
    return -1;
  }

  if (fill_search_spec(player, owner, nargs, args, &spec) < 0) {
    giveto(player, FIND_COST);
    return -1;
  }

  if ((spec.owner != ANY_OWNER && spec.owner != player
      && !(CanSearch(player, spec.owner) || (spec.type == TYPE_PLAYER)))) {
    giveto(player, FIND_COST);
    notify(player, T("You need a search warrant to do that."));
    return -1;
  }
  
  result_size = (db_top / 4) + 1;
  *result =
    (dbref *) mush_malloc(sizeof(dbref) * result_size, "search_results");
  if (!*result)
    mush_panic(T("Couldn't allocate memory in search!"));
  
  for (n = spec.low; n <= spec.high; n++) {
    if (spec.owner == ANY_OWNER && !CanSearch(player, Owner(n)))
      continue;
    if (spec.owner != ANY_OWNER && Owner(n) != spec.owner)
      continue;
    if (spec.type != NOTYPE && Typeof(n) != spec.type)
      continue;
    if (spec.zone != ANY_OWNER && Zone(n) != spec.zone)
      continue;
    if (spec.parent != ANY_OWNER && Parent(n) != spec.parent)
      continue;
    if (*spec.name && !string_match(Name(n), spec.name))
      continue;
    if (*spec.flags && !flaglist_check("FLAG", player, n, spec.flags, 1))
      continue;
    if (*spec.lflags && !flaglist_check_long("FLAG", player, n, spec.lflags, 1))
      continue;
    if (*spec.powers
	&& !flaglist_check_long("POWER", player, n, spec.powers, 1))
      continue;
    if (*spec.eval) {
      char *ebuf1;
      const char *ebuf2;
      char tbuf1[BUFFER_LEN];
      char *bp;

      ebuf1 = replace_string("##", unparse_dbref(n), spec.eval);
      ebuf2 = ebuf1;
      bp = tbuf1;
      process_expression(tbuf1, &bp, &ebuf2, player, player, player,
			 PE_DEFAULT, PT_DEFAULT, pe_info);
      mush_free((Malloc_t) ebuf1, "replace_string.buff");
      *bp = '\0';
      if (!parse_boolean(tbuf1))
	continue;
    }
    if (nresults >= result_size) {
      dbref *newresults;
      result_size *= 2;
      newresults =
	(dbref *) realloc((Malloc_t) *result, sizeof(dbref) * result_size);
      if (!newresults)
	mush_panic(T("Couldn't reallocate memory in search!"));
      *result = newresults;
    }

    (*result)[nresults++] = (dbref) n;
  }

  return (int) nresults;
}
