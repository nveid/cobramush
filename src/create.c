/**
 * \file create.c
 *
 * \brief Functions for creating objects of all types.
 *
 *
 */

#include "copyrite.h"
#include "config.h"
#include <string.h>
#include <ltdl.h>
#include "conf.h"
#include "externs.h"
#include "mushdb.h"
#include "attrib.h"
#include "match.h"
#include "extchat.h"
#include "log.h"
#include "flags.h"
#include "dbdefs.h"
#include "lock.h"
#include "parse.h"
#include "game.h"
#include "command.h"
#include "confmagic.h"
#include "modules.h"

extern struct module_entry_t *module_list;

static dbref parse_linkable_room(dbref player, const char *room_name);
static dbref check_var_link(const char *dest_name);
static dbref clone_object(dbref player, dbref thing, const char *newname,
                          int preserve);

struct db_stat_info current_state; /**< Current stats for database */


dbref copy_exit(dbref loc, dbref tail, dbref exit, dbref *oldrooms, dbref *newrooms, dbref new_owner, int num_rooms); 
dbref copy_room(dbref room, dbref newowner);
dbref copy_zmo(dbref old_zone, dbref new_owner);

/* utility for open and link */
static dbref
parse_linkable_room(dbref player, const char *room_name)
{
  dbref room;

  /* parse room */
  if (!strcasecmp(room_name, "here")) {
    room = IsExit(player) ? Source(player) : Location(player);
  } else if (!strcasecmp(room_name, "home")) {
    return HOME;                /* HOME is always linkable */
  } else {
    room = parse_dbref(room_name);
  }

  /* check room */
  if (!GoodObject(room)) {
    notify(player, T("That is not a valid object."));
    return NOTHING;
  } else if (Going(room)) {
    notify(player, T("That room is being destroyed. Sorry."));
    return NOTHING;
  } else if (!can_link_to(player, room)) {
    notify(player, T("You can't link to that."));
    return NOTHING;
  } else {
    return room;
  }
}

static dbref
check_var_link(const char *dest_name)
{
  /* This allows an exit to be linked to a 'variable' destination.
   * Such exits can be linked by anyone but the owner's ability
   * to link to the destination is checked when it's computed.
   */

  if (!strcasecmp("VARIABLE", dest_name))
    return AMBIGUOUS;
  else
    return NOTHING;
}

/** Create an exit.
 * This function opens an exit and optionally links it.
 * \param player the enactor.
 * \param direction the name of the exit.
 * \param linkto the room to link to, as a string.
 * \param pseudo a phony location for player if a back exit is needed. This is bpass by do_open() as the source room of the back exit.
 * \return dbref of the new exit, or NOTHING.
 */
dbref
do_real_open(dbref player, const char *direction, const char *linkto,
             dbref pseudo)
{
  struct module_entry_t *m;
  void (*handle)(dbref);


  dbref loc =
    (pseudo !=
     NOTHING) ? pseudo : (IsExit(player) ? Source(player) : Location(player));
  dbref new_exit;
  if (!command_check_byname(player, "@dig")) {
    notify(player, "Permission denied.");
    return NOTHING;
  }
  if ((loc == NOTHING) || (!IsRoom(loc))) {
    notify(player, T("Sorry you can only make exits out of rooms."));
    return NOTHING;
  }
  if (Going(loc)) {
    notify(player, T("You can't make an exit in a place that's crumbling."));
    return NOTHING;
  }
  if (!*direction) {
    notify(player, T("Open where?"));
    return NOTHING;
  } else if (!ok_name(direction)) {
    notify(player, T("That's a strange name for an exit!"));
    return NOTHING;
  }
  if (!CanOpen(player, loc) && !controls(player, loc)) {
    notify(player, T("Permission denied."));
  } else if (can_pay_fees(player, EXIT_COST)) {
    /* create the exit */
    new_exit = new_object();

    /* initialize everything */
    set_name(new_exit, direction);
    Owner(new_exit) = Owner(player);
    Zone(new_exit) = Zone(player);
    SDIV(new_exit).object = SDIV(player).object;
    SLEVEL(new_exit) = LEVEL(player);
    Source(new_exit) = loc;
    Type(new_exit) = TYPE_EXIT;
    Flags(new_exit) = string_to_bits("FLAG", options.exit_flags);
    set_lmod(new_exit, "NONE");

    /* link it in */
    PUSH(new_exit, Exits(loc));

    /* and we're done */
    notify_format(player, T("Opened exit %s"), unparse_dbref(new_exit));

    /* check second arg to see if we should do a link */
    if (linkto && *linkto != '\0') {
      notify(player, T("Trying to link..."));
      if ((loc = check_var_link(linkto)) == NOTHING)
        loc = parse_linkable_room(player, linkto);
      if (loc != NOTHING) {
        if (!payfor(player, LINK_COST)) {
          notify_format(player, T("You don't have enough %s to link."), MONIES);
        } else {
          /* it's ok, link it */
          Location(new_exit) = loc;
          notify_format(player, T("Linked exit #%d to #%d"), new_exit, loc);
        }
      }
    }
    current_state.exits++;
    /* Replacement for local_data_create */
    MODULE_ITER(m)
      MODULE_FUNC(handle, m->handle, "module_data_create", new_exit);

    return new_exit;
  }
  return NOTHING;
}

/** Open a new exit.
 * \verbatim
 * This is the top-level function for @open. It calls do_real_open()
 * to do the real work of opening both the exit forward and the exit back.
 * \endverbatim
 * \param player the enactor.
 * \param direction name of the exit forward.
 * \param links 1-based array containing name of destination and optionally name of exit back.
 */
void
do_open(dbref player, const char *direction, char **links)
{
  dbref forward;
  forward = do_real_open(player, direction, links[1], NOTHING);
  if (links[2] && GoodObject(forward) && GoodObject(Location(forward))) {
    do_real_open(player, links[2], "here", Location(forward));
  }
}

/** Unlink an exit or room.
 * \verbatim
 * This is the top-level function for @unlink, which can unlink an exit
 * or remove a drop-to from a room.
 * \endverbatim
 * \param player the enactor.
 * \param name name of the object to unlink.
 */
void
do_unlink(dbref player, const char *name)
{
  dbref exit_l, old_loc;
  long match_flags = MAT_EXIT | MAT_HERE | MAT_ABSOLUTE;

  if (!OOREF(player,div_powover(player, player,  "Link"), div_powover(ooref, ooref, "Link"))) {
    match_flags |= MAT_CONTROL;
  }
  switch (exit_l = match_result(player, name, TYPE_EXIT, match_flags)) {
  case NOTHING:
    notify(player, T("Unlink what?"));
    break;
  case AMBIGUOUS:
    notify(player, T("I don't know which one you mean!"));
    break;
  default:
    if (!controls(player, exit_l)) {
      notify(player, T("Permission denied."));
    } else {
      switch (Typeof(exit_l)) {
      case TYPE_EXIT:
        old_loc = Location(exit_l);
        Location(exit_l) = NOTHING;
        notify_format(player, T("Unlinked exit #%d (Used to lead to %s)."),
                      exit_l, unparse_object(player, old_loc));
        break;
      case TYPE_ROOM:
        Location(exit_l) = NOTHING;
        notify(player, T("Dropto removed."));
        break;
      default:
        notify(player, T("You can't unlink that!"));
        break;
      }
    }
  }
}

/** Link an exit, room, player, or thing.
 * \verbatim
 * This is the top-level function for @link, which is used to link an
 * exit to a destination, set a player or thing's home, or set a 
 * drop-to on a room.
 *
 * Linking an exit usually seizes ownership of the exit and costs 1 penny.
 * 1 penny is also transferred to the former owner.
 * \endverbatim
 * \param player the enactor.
 * \param name the name of the object to link.
 * \param room_name the name of the link destination.
 * \param preserve if 1, preserve ownership and zone data on exit relink.
 */
void
do_link(dbref player, const char *name, const char *room_name, int preserve)
{
  /* Use this to link to a room that you own. 
   * It usually seizes ownership of the exit and costs 1 penny,
   * plus a penny transferred to the exit owner if they aren't you.
   * You must own the linked-to room AND specify it by room number.
   */

  dbref thing;
  dbref room;

  if (!room_name || !*room_name) {
    do_unlink(player, name);
    return;
  }
  if (!IsRoom(player) && GoodObject(Location(player)) &&
      IsExit(Location(player))) {
    notify(player, T("You somehow wound up in a exit. No biscuit."));
    return;
  }
  if ((thing = noisy_match_result(player, name, TYPE_EXIT, MAT_EVERYTHING))
      != NOTHING) {
    switch (Typeof(thing)) {
    case TYPE_EXIT:
      if ((room = check_var_link(room_name)) == NOTHING)
        room = parse_linkable_room(player, room_name);
      if (room == NOTHING)
        return;
      if (GoodObject(room) && !can_link_to(player, room)) {
        notify(player, T("Permission denied."));
        break;
      }
      /* We may link an exit if it's unlinked and we pass the link-lock
       * or if we control it.
       */
      if (!(controls(player, thing)
            || ((Location(thing) == NOTHING)
                && eval_lock(player, thing, Link_Lock)))) {
        notify(player, T("Permission denied."));
        return;
      }
      if (preserve && !OOREF(player,div_powover(player, player,  "Link"), div_powover(ooref,ooref, "Link"))) {
        notify(player, T("Permission denied."));
        return;
      }
      /* handle costs */
      if (Owner(thing) == Owner(player)) {
        if (!payfor(player, LINK_COST)) {
          notify_format(player, T("It costs %d %s to link this exit."),
                        LINK_COST, ((LINK_COST == 1) ? MONEY : MONIES));
          return;
        }
      } else {
        if (!payfor(player, LINK_COST + EXIT_COST)) {
          int a = LINK_COST + EXIT_COST;
          notify_format(player, T("It costs %d %s to link this exit."), a,
                        ((a == 1) ? MONEY : MONIES));
          return;
        } else if (!preserve) {
          /* pay the owner for his loss */
          giveto(Owner(thing), EXIT_COST);
          chown_object(player, thing, player, 0);
        }
      }

      /* link has been validated and paid for; do it */
      if (!preserve) {
        Owner(thing) = Owner(player);
        Zone(thing) = Zone(player);
        SDIV(thing).object = SDIV(player).object;
        SLEVEL(thing) = LEVEL(player);
      }
      Location(thing) = room;

      /* notify the player */
      notify_format(player, T("Linked exit #%d to %s"), thing,
                    unparse_object(player, room));
      break;
    case TYPE_DIVISION:
    case TYPE_PLAYER:
    case TYPE_THING:
      if ((room =
           noisy_match_result(player, room_name, NOTYPE, MAT_EVERYTHING)) < 0) {
        notify(player, T("No match."));
        return;
      }
      if (IsExit(room)) {
        notify(player, T("That is an exit."));
        return;
      }
      if (thing == room) {
        notify(player, T("You may not link something to itself."));
        return;
      }
      /* abode */
      if (!controls(player, room) && !Abode(room)) {
        notify(player, T("Permission denied."));
        break;
      }
      if (!controls(player, thing)) {
        notify(player, T("Permission denied."));
      } else if (room == HOME) {
        notify(player, T("Can't set home to home."));
      } else {
        /* do the link */
        Home(thing) = room;     /* home */
        if (!Quiet(player) && !(Quiet(thing) && (Owner(thing) == player)))
          notify(player, T("Home set."));
      }
      break;
    case TYPE_ROOM:
      if ((room = parse_linkable_room(player, room_name)) == NOTHING)
        return;
      if ((room != HOME) && (!IsRoom(room))) {
        notify(player, T("That is not a room!"));
        return;
      }
      if (!controls(player, thing)) {
        notify(player, T("Permission denied."));
      } else {
        /* do the link, in location */
        Location(thing) = room; /* dropto */
        notify(player, T("Dropto set."));
      }
      break;
    default:
      notify(player, "Internal error: weird object type.");
      do_log(LT_ERR, NOTHING, NOTHING,
             T("Weird object! Type of #%d is %d"), thing, Typeof(thing));
      break;
    }
  }
}

/** Create a room.
 * \verbatim
 * This is the top-level interface for @dig.
 * \endverbatim
 * \param player the enactor.
 * \param name the name of the room to create.
 * \param argv array of additional arguments to command (exit forward,exit back)
 * \param tport if 1, teleport the player to the new room.
 * \return dbref of new room, or NOTHING.
 */
dbref
do_dig(dbref player, const char *name, char **argv, int tport)
{
  struct module_entry_t *m;
  void (*handle)(dbref);
  dbref room;

  /* we don't need to know player's location!  hooray! */
  if (*name == '\0') {
    notify(player, T("Dig what?"));
  } else if (!ok_name(name)) {
    notify(player, T("That's a silly name for a room!"));
  } else if (can_pay_fees(player, ROOM_COST)) {
    room = new_object();

    /* Initialize everything */
    set_name(room, name);
    Owner(room) = Owner(player);
    Zone(room) = Zone(player);
    SDIV(room).object = SDIV(player).object;
    SLEVEL(room) = LEVEL(player);
    Type(room) = TYPE_ROOM;
    Flags(room) = string_to_bits("FLAG", options.room_flags);
    set_lmod(room, "NONE");

    notify_format(player, T("%s created with room number %d."), name, room);
    if (argv[1] && *argv[1]) {
      char nbuff[MAX_COMMAND_LEN];
      sprintf(nbuff, "#%d", room);
      do_real_open(player, argv[1], nbuff, NOTHING);
    }
    if (argv[2] && *argv[2]) {
      do_real_open(player, argv[2], "here", room);
    }
    current_state.rooms++;

    /* Replacement for local_data_create */
    MODULE_ITER(m)
      MODULE_FUNC(handle, m->handle, "module_data_create", room);

    if (tport) {
      /* We need to use the full command, because we need NO_TEL
       * and Z_TEL checking */
      char roomstr[MAX_COMMAND_LEN];
      sprintf(roomstr, "#%d", room);
      do_teleport(player, "me", roomstr, 0, 0); /* if flag, move the player */
    }
    return room;
  }
  return NOTHING;
}

/** Create a thing.
 * \verbatim
 * This is the top-level function for @create.
 * \endverbatim
 * \param player the enactor.
 * \param name name of thing to create.
 * \param cost pennies spent in creation.
 * \return dbref of new thing, or NOTHING.
 */
dbref
do_create(dbref player, char *name, int cost)
{
  struct module_entry_t *m;
  void (*handle)(dbref);
  dbref loc;
  dbref thing;

  if (*name == '\0') {
    notify(player, T("Create what?"));
    return NOTHING;
  } else if (!ok_name(name)) {
    notify(player, T("That's a silly name for a thing!"));
    return NOTHING;
  } else if (cost < OBJECT_COST) {
    cost = OBJECT_COST;
  }
  if (can_pay_fees(player, cost)) {
    /* create the object */
    thing = new_object();

    /* initialize everything */
    set_name(thing, name);
    if (!IsExit(player))        /* Exits shouldn't have contents! */
      Location(thing) = player;
    else
      Location(thing) = Source(player);
    Owner(thing) = Owner(player);
    Zone(thing) = Zone(player);
    SDIV(thing).object = SDIV(player).object;
    SLEVEL(thing) = LEVEL(player);
    s_Pennies(thing, cost);
    Type(thing) = TYPE_THING;
    Flags(thing) = string_to_bits("FLAG", options.thing_flags);
    set_lmod(thing, "NONE");

    /* home is here (if we can link to it) or player's home */
    if ((loc = Location(player)) != NOTHING &&
        (controls(player, loc) || Abode(loc))) {
      Home(thing) = loc;        /* home */
    } else {
      Home(thing) = Home(player);       /* home */
    }

    /* link it in */
    if (!IsExit(player))
      PUSH(thing, Contents(player));
    else
      PUSH(thing, Contents(Source(player)));

    /* and we're done */
    notify_format(player, "Created: Object %s.", unparse_dbref(thing));
    current_state.things++;
    /* Replacement for local_data_create */
    MODULE_ITER(m)
      MODULE_FUNC(handle, m->handle, "module_data_create", thing);

    return thing;
  }
  return NOTHING;
}

/* Clone an object. The new object is owned by the cloning player */
static dbref
clone_object(dbref player, dbref thing, const char *newname, int preserve)
{
  dbref clone;

  clone = new_object();

  memcpy(REFDB(clone), REFDB(thing), sizeof(struct object));
  Owner(clone) = Owner(player);
  Name(clone) = NULL;
  if (newname && *newname)
    set_name(clone, newname);
  else
    set_name(clone, Name(thing));
  s_Pennies(clone, Pennies(thing));
  atr_cpy(clone, thing);
  Locks(clone) = NULL;
  clone_locks(player, thing, clone);
  Zone(clone) = Zone(thing);
  SDIV(clone).object = SDIV(thing).object;
  adjust_powers(clone, player);
  Parent(clone) = Parent(thing);
  Flags(clone) = clone_flag_bitmask("FLAG", Flags(thing));
  set_lmod(clone, "NONE");
#ifdef RPMODE_SYS
  db[clone].rplog.bufferq = NULL;
  db[clone].rplog.status = 0;
#endif
  DPBITS(clone) = NULL;
  if (!preserve) {
    Warnings(clone) = 0;        /* zap warnings */
  } else if (Warnings(clone) || DPBITS(clone)) {
    notify(player, T("Warning: @CLONE/PRESERVE on an object with powers or warnings."));
  }
  /* We give the clone the same modification time that its
   * other clone has, but update the creation time */

  CreTime(clone) = mudtime;
  Contents(clone) = Location(clone) = Next(clone) = NOTHING;

  return clone;

}

/** Clone an object.
 * \verbatim
 * This is the top-level function for @clone, which creates a duplicate
 * of a (non-player) object.
 * \endverbatim
 * \param player the enactor.
 * \param name the name of the object to clone.
 * \param newname the name to give the duplicate.
 * \param preserve if 1, preserve ownership and privileges on duplicate.
 * \return dbref of the duplicate, or NOTHING.
 */
dbref
do_clone(dbref player, char *name, char *newname, int preserve)
{
  struct module_entry_t *m;
  void (*handle)(dbref , dbref);
  dbref clone, thing;
  char dbnum[BUFFER_LEN];

  if (newname && *newname && !ok_name(newname)) {
    notify(player, T("That is not a reasonable name."));
    return NOTHING;
  }
  thing = noisy_match_result(player, name, NOTYPE, MAT_EVERYTHING);
  if ((thing == NOTHING))
    return NOTHING;

  if (!controls(player, thing) || IsPlayer(thing) ||
      (IsRoom(thing) && !command_check_byname(player, "@dig")) ||
      (IsExit(thing) && !command_check_byname(player, "@open")) ||
      (IsThing(thing) && !command_check_byname(player, "@create"))) {
    notify(player, T("Permission denied."));
    return NOTHING;
  }
  /* don't allow cloning of destructed things */
  if (IsGarbage(thing)) {
    notify(player, T("There's nothing left of it to clone!"));
    return NOTHING;
  }
  if (preserve && !Director(player)) {
    notify(player,
           T("You cannot @CLONE/PRESERVE.  Use normal @CLONE instead."));
    return NOTHING;
  }
  /* make sure owner can afford it */
  switch (Typeof(thing)) {
  case TYPE_THING:
    if (can_pay_fees(player, Pennies(thing))) {
      clone = clone_object(player, thing, newname, preserve);
      notify_format(player, T("Cloned: Object %s."), unparse_dbref(clone));
      if (IsRoom(player))
        moveto(clone, player);
      else
        moveto(clone, Location(player));
      current_state.things++;
      /* Replacement for local_data_clone */
      MODULE_ITER(m)
	MODULE_FUNC(handle, m->handle, "module_data_clone", clone,thing);

      real_did_it(player, clone, NULL, NULL, NULL, NULL, "ACLONE", NOTHING,
                  global_eval_context.wenv, 0);
      return clone;
    }
    return NOTHING;
    break;
  case TYPE_ROOM:
    if (can_pay_fees(player, ROOM_COST)) {
      clone = clone_object(player, thing, newname, preserve);
      Exits(clone) = NOTHING;
      notify_format(player, T("Cloned: Room #%d."), clone);
      current_state.rooms++;
      /* local_data_clone replacement */
      MODULE_ITER(m)
	MODULE_FUNC(handle, m->handle, "module_data_clone", clone,thing);
      real_did_it(player, clone, NULL, NULL, NULL, NULL, "ACLONE", NOTHING,
                  global_eval_context.wenv, 0);
      return clone;
    }
    return NOTHING;
    break;
  case TYPE_EXIT:
    /* For exits, we don't want people to be able to link it to
       a location they can't with @open. So, all this stuff. 
     */
    switch (Location(thing)) {
    case NOTHING:
      strcpy(dbnum, "#-1");
      break;
    case HOME:
      strcpy(dbnum, "home");
      break;
    case AMBIGUOUS:
      strcpy(dbnum, "variable");
      break;
    default:
      strcpy(dbnum, unparse_dbref(Location(thing)));
    }
    if (newname && *newname)
      clone = do_real_open(player, newname, dbnum, NOTHING);
    else
      clone = do_real_open(player, Name(thing), dbnum, NOTHING);
    if (!GoodObject(clone))
      return NOTHING;
    else {
      atr_cpy(clone, thing);
      clone_locks(player, thing, clone);
      Zone(clone) = Zone(thing);
      SDIV(clone).object = SDIV(thing).object;
      SLEVEL(clone) = LEVEL(thing);
      Parent(clone) = Parent(thing);
      Flags(clone) = clone_flag_bitmask("FLAG", Flags(thing));
      adjust_powers(clone, player);
      DPBITS(clone) = NULL;
#ifdef RPMODE_SYS
      db[clone].rplog.bufferq = NULL;
      db[clone].rplog.status = 0;
#endif /* RPMODE_SYS */
      if (!preserve) {
        Warnings(clone) = 0;    /* zap warnings */
      } else {
        Warnings(clone) = Warnings(thing);
      }
      if (Warnings(clone) || DPBITS(clone))
        notify(player, T("Warning: @CLONE/PRESERVE on an exit with powers or warnings."));
      notify_format(player, T("Cloned: Exit #%d."), clone);
      /* Replacement for local_data_clone */
      MODULE_ITER(m)
	MODULE_FUNC(handle, m->handle, "module_data_clone", clone,thing);
     return clone;
    }
  }
  return NOTHING;

}






void copy_zone(dbref executor, dbref zmo) {
  ATTR *atr;
  dbref newzone, cur;
  int num_rooms, num_exits, i, ei;
  dbref *oldrooms, *newrooms;
  dbref *oldexits, *newexits;
  /* dbref_list variables */
  char *da_buf_arr[BUFFER_LEN / 2];
  int da_sz;
  int *da_vals = NULL;
  char buf[BUFFER_LEN], *bp;
  /* */

  newzone = copy_zmo(zmo, executor);

  /* Grab DBREF_LIST atrs and stick in da_vals */
  atr = atr_get(zmo, "DBREF_LIST");
  if(atr) {
    da_sz = list2arr(da_buf_arr, BUFFER_LEN / 2, atr_value(atr), ' ');

    da_vals = (int *) mush_malloc(sizeof(int) * da_sz, "copy_zone.dbref_list_size");
    for(i = 0; i < da_sz ; i++)
      da_vals[i] = parse_dbref(da_buf_arr[i]);
  }

  num_exits = num_rooms = 0;
  for(cur = 0; cur < db_top; cur++)
    if(IsRoom(cur) && Zone(cur) == zmo) {
      num_rooms++;
      for(i = Exits(cur); i != NOTHING; i = Next(i))
        num_exits++;
    }

  oldrooms = (dbref *) mush_malloc(sizeof(dbref) * num_rooms, "copy_zone");
  newrooms = (dbref *) mush_malloc(sizeof(dbref) * num_rooms, "copy_zone");
  oldexits = (dbref *) mush_malloc(sizeof(dbref) * num_exits, "copy_zone");
  newexits = (dbref *) mush_malloc(sizeof(dbref) * num_exits, "copy_zone");

  for(i = cur = 0; cur < db_top; cur++)
    if(IsRoom(cur) && Zone(cur) == zmo)
      oldrooms[i++] = cur;

  for(i = 0; i < num_rooms; i++) {
    /* Make Room */
    newrooms[i] = copy_room(oldrooms[i], executor);
    Zone(newrooms[i]) = newzone;
  }


  for(ei = i = 0 ; i < num_rooms; i++) {
    /* Copy Exits */
    for(cur = Exits(oldrooms[i]); cur != NOTHING; cur = Next(cur))  {
      oldexits[ei] = cur; 
      newexits[ei++] = copy_exit(newrooms[i], Exits(newrooms[i]), cur, oldrooms, newrooms, executor, num_rooms);
    }

    /* Now Turn the Exits around */
    (void) reverse(newrooms[i]);
  }


  /*  Change dbref_list on object to new dbrefs */
  if(da_vals) {
    bp = buf;
    for(cur = 0; cur < da_sz; cur++) {
      for(i = 0; i < num_rooms; i++)
        if(da_vals[cur] == oldrooms[i]) {
          safe_str(unparse_dbref(newrooms[i]), buf, &bp);
          if((cur+1) < da_sz) safe_chr(' ', buf, &bp);
          break;
        }
      if(i == num_rooms) {
        for(i = 0; i < num_exits; i++)
          if(da_vals[cur] == oldexits[i]) {
            safe_str(unparse_dbref(newexits[i]), buf, &bp);
            if((cur+1) < da_sz) safe_chr(' ', buf, &bp);
            break;
          }
        if(i == num_exits)
          safe_str("#-1 ", buf, &bp);
      }
    }
    *bp = '\0';
      /* add new dbref_list */
    atr_add(newzone, "DBREF_LIST", buf, executor, NOTHING);
    mush_free(da_vals, "copy_zone.dbref_list_size");
  }

  /* Execute AZCLONE */
  atr = atr_get(newzone, "AZCLONE");
  if(atr) {
    global_eval_context.wnxt[0] = '\0';
    parse_que(newzone, atr_value(atr), executor);
  }

  /* Free everything we're done. */
  mush_free(oldrooms, "copy_zone");
  mush_free(newrooms, "copy_zone");
  mush_free(oldexits, "copy_zone");
  mush_free(newexits, "copy_zone");
}


dbref copy_room(dbref room, dbref newowner) {
  dbref new_room;
  struct module_entry_t *m;
  void (*handle)(dbref);

  /* Copy room and all its shit but not exits or zone */

  new_room = new_object();


  memcpy(REFDB(new_room), REFDB(room), sizeof(struct object));

  Type(new_room) = TYPE_ROOM;
  Owner(new_room) = newowner;
  Name(new_room) = NULL;

  set_name(new_room, Name(room));

  atr_cpy(new_room, room);
  Locks(new_room) = NULL;
  clone_locks(newowner, room, new_room);

  SLEVEL(new_room) = LEVEL(newowner);
  SDIV(new_room).object = SDIV(newowner).object;

  Parent(new_room) = Parent(room);
  Flags(new_room) = clone_flag_bitmask("FLAG", Flags(room));
  set_lmod(new_room, "NONE");

  DPBITS(new_room) = NULL;
#ifdef RPMODE_SYS
  db[new_room].rplog.bufferq = NULL;
  db[new_room].rplog.status = 0;
#endif /* RPMODE_SYS */

  Contents(new_room) = Location(new_room) = Next(new_room) = Exits(new_room) =  NOTHING;


  /* we don't have to rplog_init rooms because of this */
  twiddle_flag_internal("FLAG", new_room, "ICFUNCS", 1);

  /* spruce up more later.. just copying basic room for now */

  current_state.rooms++;
  
  /* Replacement for local_data_create */
  MODULE_ITER(m)
    MODULE_FUNC(handle, m->handle, "module_data_create", room);

  return new_room;
}

dbref copy_exit(dbref loc, dbref tail, dbref exit, dbref *oldrooms, dbref *newrooms, dbref new_owner, int num_rooms) {
  int i;
  dbref new_exit;

  /* Copy exit into new_exit, finding the destination and home by
   * looking in oldrooms and using the offest into newrooms */

  new_exit = new_object();

  set_name(new_exit, Name(exit));
  Owner(new_exit) = new_owner;
  Zone(new_exit) = Zone(new_owner);
  SDIV(new_exit).object = SDIV(new_owner).object;
  SLEVEL(new_exit) = LEVEL(new_owner);
  Type(new_exit) = TYPE_EXIT;
  set_lmod(new_exit, "NONE");

  Flags(new_exit) = clone_flag_bitmask("FLAG", Flags(exit));
  Parent(new_exit) = Parent(exit);
  atr_cpy(new_exit, exit);


  DPBITS(new_exit) = NULL;
#ifdef RPMODE_SYS
  db[new_exit].rplog.bufferq = NULL;
  db[new_exit].rplog.status = 0;
#endif

  Source(new_exit) = loc;
  Destination(new_exit) = NOTHING;
  PUSH(new_exit, Exits(loc));
  current_state.exits++;

  /* Link Exit to Destination */
  for(i = 0 ; i < num_rooms; i++)
    if(oldrooms[i] == Destination(exit))
      Destination(new_exit) = newrooms[i];
  return new_exit;
}


dbref copy_zmo(dbref old_zone, dbref new_owner) {
  dbref new_zone;
  /* Clone Zone and any special old_zones.. */

  new_zone = new_object();

  memcpy(REFDB(new_zone), REFDB(old_zone), sizeof(struct object));
  Owner(new_zone) = Owner(new_owner);
  Name(new_zone) = NULL;
  set_name(new_zone, Name(old_zone));
  s_Pennies(new_zone, Pennies(old_zone));
  atr_cpy(new_zone, old_zone);
  Locks(new_zone) = NULL;
  clone_locks(new_owner, old_zone, new_zone);
  Zone(new_zone) = Zone(old_zone);
  SDIV(new_zone).object = SDIV(old_zone).object;
  adjust_powers(new_zone, new_owner);
  Parent(new_zone) = Parent(old_zone);
  Flags(new_zone) = clone_flag_bitmask("FLAG", Flags(old_zone));
  set_lmod(new_zone, "NONE");
  Warnings(new_zone) = 0;        /* zap warnings */
  DPBITS(new_zone) = NULL;
#ifdef RPMODE_SYS
  db[new_zone].rplog.bufferq = NULL;
  db[new_zone].rplog.status = 0;
#endif /* RPMODE_SYS */

  twiddle_flag_internal("FLAG", new_zone, "ZCLONE_OK", 1);

  Contents(new_zone) = Location(new_zone) = Next(new_zone) = NOTHING;

  CreTime(new_zone) =  mudtime;
  current_state.things++;
  Location(new_zone) = new_owner;
  PUSH(new_zone, Contents(new_owner));

  return new_zone;
}
