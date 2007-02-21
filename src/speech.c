/**
 * \file speech.c
 *
 * \brief Speech-related commands in PennMUSH.
 *
 *
 */
/* speech.c */

#include "copyrite.h"
#include "config.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "conf.h"
#include "ansi.h"
#include "externs.h"
#include "mushdb.h"
#include "dbdefs.h"
#include "lock.h"
#include "flags.h"
#include "log.h"
#include "match.h"
#include "attrib.h"
#include "parse.h"
#include "game.h"
#include "pcre.h"
#include "confmagic.h"

int okay_pemit(dbref player, dbref target);
static dbref speech_loc(dbref thing);
void propagate_sound(dbref thing, const char *msg);
static void do_audible_stuff(dbref loc, dbref *excs, int numexcs,
			     const char *msg);
static void do_one_remit(dbref player, const char *target, const char *msg,
			 int flags);
static int dbref_comp(const void *a, const void *b);

dbref na_zemit(dbref current, void *data);

const char *
spname(dbref thing)
{
  /* if FULL_INVIS is defined, dark admins and dark objects will be
   * Someone and Something, respectively.
   */

  if (FULL_INVIS && DarkLegal(thing)) {
    if (IsPlayer(thing))
      return "Someone";
    else
      return "Something";
  } else {
    return accented_name(thing);
  }
}

/** Can player pemit to target?
 * You can pemit if you're pemit_all, if you're pemitting to yourself,
 * if you're pemitting to a non-player, or if you pass target's
 * pagelock and target isn't HAVEN.
 * \param player dbref attempting to pemit.
 * \param target target dbref to pemit to.
 * \retval 1 player may pemit to target.
 * \retval 0 player may not pemit to target.
 */
int
okay_pemit(dbref player, dbref target)
{
  if (Can_Pemit(player, target))
    return 1;
  if (IsPlayer(target) && Haven(target))
    return 0;
  if (!eval_lock(player, target, Page_Lock)) {
    fail_lock(player, target, Page_Lock, NULL, NOTHING);
    return 0;
  }
  return 1;
}

static dbref
speech_loc(dbref thing)
{
  /* This is the place where speech, poses, and @emits by thing should be
   * heard. For things and players, it's the loc; For rooms, it's the room
   * itself; for exits, it's the source. */
  if (!GoodObject(thing))
    return NOTHING;
  switch (Typeof(thing)) {
  case TYPE_ROOM:
    return thing;
  case TYPE_EXIT:
    return Home(thing);
  default:
    return Location(thing);
  }
}

/** The teach command.
 * \param player the enactor.
 * \param cause the object causing the command to run.
 * \param tbuf1 the command being taught.
 */
void
do_teach(dbref player, dbref cause, const char *tbuf1)
{
  dbref loc;
  static int recurse = 0;
  char *command;

  loc = speech_loc(player);
  if (!GoodObject(loc))
    return;

  if (!CanSpeak(player,loc)) {
    notify(player, T("You may not speak here!"));
    return;
  }

  if (recurse) {
    /* Somebody tried to teach the teach command. Cute. Dumb. */
    notify(player, T("You can't teach 'teach', sorry."));
    recurse = 0;
    return;
  }

  if (!tbuf1 || !*tbuf1) {
    notify(player, T("What command do you want to teach?"));
    return;
  }

  recurse = 1;			/* Protect use from recursive teach */
  notify_except(Contents(loc), NOTHING,
		tprintf(T("%s types --> %s%s%s"), spname(player),
			ANSI_HILITE, tbuf1, ANSI_NORMAL), NA_INTER_HEAR);
  command = mush_strdup(tbuf1, "string");	/* process_command is destructive */
  process_command(player, command, cause, cause, 1);
  mush_free(command, "string");
  recurse = 0;			/* Ok, we can be called again safely */
}

/** The say command.
 * \param player the enactor.
 * \param tbuf1 the message to say.
 */
void
do_say(dbref player, const char *tbuf1)
{
  dbref loc;

  loc = speech_loc(player);
  if (!GoodObject(loc))
    return;

  if(!CanSpeak(player,loc)) {
    notify(player, T("You may not speak here!"));
    return;
  }

  if (*tbuf1 == SAY_TOKEN && CHAT_STRIP_QUOTE)
    tbuf1++;

  /* notify everybody */
  notify_format(player, T("You say, \"%s\""), tbuf1);
  notify_except(Contents(loc), player,
		tprintf(T("%s says, \"%s\""), spname(player), tbuf1),
		NA_INTER_HEAR);
}

/** A comparator for raw dbrefs.
 * This exists because the i_comp function in funlist.c got hairy some time ago.
 * \param a the first element to compare.
 * \param b the second element to compare.
 */
static int
dbref_comp(const void *a, const void *b)
{
  const dbref *x = (const dbref *) a;
  const dbref *y = (const dbref *) b;
  return (int) *x - (int) *y;
}

/** The oemit(/list) command.
 * \verbatim
 * This implements @oemit and @oemit/list.
 * \endverbatim
 * \param player the enactor.
 * \param list the list of dbrefs to oemit from the emit.
 * \param message the message to emit.
 * \param flags PEMIT_* flags.
 */
void
do_oemit_list(dbref player, char *list, const char *message, int flags)
{
  char *temp, *p, *s;
  dbref who;
  dbref pass[12], locs[10];
  int i, oneloc = 0;
  int na_flags = NA_INTER_HEAR;

  /* If no message, further processing is pointless.
   * If no list, they should have used @remit. */
  if (!message || !*message || !list || !*list)
    return;

  orator = player;
  pass[0] = 0;
  /* Find out what room to do this in. If they supplied a db# before
   * the '/', then oemit to anyone in the room who's not on list.
   * Otherwise, oemit to every location which has at least one of the
   * people in the list. This is intended for actions which involve
   * players who are in different rooms, e.g.:
   *
   * X (in #0) fires an arrow at Y (in #2).
   *
   * X sees: You fire an arrow at Y. (pemit to X)
   * Y sees: X fires an arrow at you! (pemit to Y)
   * #0 sees: X fires an arrow at Y. (oemit/list to X Y)
   * #2 sees: X fires an arrow at Y. (from the same oemit)
   */
  /* Find out what room to do this in. They should have supplied a db#
   * before the '/'. */
  if ((temp = strchr(list, '/'))) {
    *temp++ = '\0';
    pass[1] = noisy_match_result(player, list, NOTYPE, MAT_EVERYTHING);
    if (!GoodObject(pass[1])) {
      notify(player, T("I can't find that room."));
      return;
    }

    if(!CanSpeak(player, pass[1])) {
      notify(player, T("You may not speak there!"));
      return;
    }

    oneloc = 1;			/* we are only oemitting to one location */
  } else {
    temp = list;
  }

  s = trim_space_sep(temp, ' ');
  while (s) {
    p = split_token(&s, ' ');
    /* If a room was given, we match relative to the room */
    if (oneloc)
      who = match_result(pass[1], p, NOTYPE, MAT_POSSESSION | MAT_ABSOLUTE);
    else
      who = noisy_match_result(player, p, NOTYPE, MAT_OBJECTS);
    /* pass[0] tracks the number of valid players we've found.
     * pass[1] is the given room (possibly nothing right now)
     * pass[2..12] are dbrefs of players
     * locs[0..10] are corresponding dbrefs of locations
     */
    if (GoodObject(who) && GoodObject(Location(who))
	&& (CanSpeak(player, Location(who)))
      ) {
      if (pass[0] < 10) {
	locs[pass[0]] = Location(who);
	pass[pass[0] + 2] = who;
	pass[0]++;
      } else {
	notify(player, T("Too many people to oemit to."));
	break;
      }
    }
  }

  /* Sort the list of rooms to oemit to so we don't oemit to the same           
   * room twice */
  qsort((void *) locs, pass[0], sizeof(locs[0]), dbref_comp);

  if (flags & PEMIT_SPOOF)
    na_flags |= NA_SPOOF;
  for (i = 0; i < pass[0]; i++) {
    if (i != 0 && locs[i] == locs[i - 1])
      continue;
    pass[1] = locs[i];
    notify_anything_loc(orator, na_exceptN, pass, ns_esnotify, na_flags,
			message, locs[i]);
    do_audible_stuff(pass[1], &pass[2], pass[0], message);
  }
}


/** The whisper command.
 * \param player the enactor.
 * \param arg1 name of the object to whisper to.
 * \param arg2 message to whisper.
 * \param noisy if 1, others overhear that a whisper has occurred.
 */
void
do_whisper(dbref player, const char *arg1, const char *arg2, int noisy)
{
  dbref who;
  int key;
  const char *gap;
  char *tbuf, *tp;
  char *p;
  dbref good[100];
  int gcount = 0;
  const char *head;
  int overheard;
  char *current;
  const char **start;

  if (!arg1 || !*arg1) {
    notify(player, T("Whisper to whom?"));
    return;
  }
  if (!arg2 || !*arg2) {
    notify(player, T("Whisper what?"));
    return;
  }
  tp = tbuf = (char *) mush_malloc(BUFFER_LEN, "string");
  if (!tbuf)
    mush_panic("Unable to allocate memory in do_whisper_list");

  overheard = 0;
  head = arg1;
  start = &head;
  /* Figure out what kind of message */
  gap = " ";
  switch (*arg2) {
  case SEMI_POSE_TOKEN:
    gap = "";
  case POSE_TOKEN:
    key = 1;
    arg2++;
    break;
  default:
    key = 2;
    break;
  }

  *tp = '\0';
  /* Make up a list of good and bad names */
  while (head && *head) {
    current = next_in_list(start);
    who = match_result(player, current, TYPE_PLAYER, MAT_NEAR_THINGS |
		       MAT_CONTAINER );
    if (!GoodObject(who) || !can_interact(player, who, INTERACT_HEAR)) {
      safe_chr(' ', tbuf, &tp);
      safe_str_space(current, tbuf, &tp);
      if (GoodObject(who))
	notify_format(player, T("%s can't hear you."), Name(who));
    } else {
      /* A good whisper */
      good[gcount++] = who;
      if (gcount >= 100) {
	notify(player, T("Too many people to whisper to."));
	break;
      }
    }
  }

  *tp = '\0';
  if (*tbuf)
    notify_format(player, T("Unable to whisper to:%s"), tbuf);

  if (!gcount) {
    mush_free((Malloc_t) tbuf, "string");
    return;
  }

  /* Drunk admins... */
  if (Dark(player))
    noisy = 0;

  /* Set up list of good names */
  tp = tbuf;
  safe_str(" to ", tbuf, &tp);
  for (who = 0; who < gcount; who++) {
    if (noisy && (get_random_long(0, 100) < WHISPER_LOUDNESS))
      overheard = 1;
    safe_itemizer(who + 1, (who == gcount - 1), ",", T("and"), " ", tbuf, &tp);
    safe_str(Name(good[who]), tbuf, &tp);
  }
  *tp = '\0';

  if (key == 1) {
    notify_format(player, (gcount > 1) ? T("%s sense: %s%s%s") :
		  T("%s senses: %s%s%s"), tbuf + 4, Name(player), gap, arg2);
    p = tprintf("You sense: %s%s%s", Name(player), gap, arg2);
  } else {
    notify_format(player, T("You whisper, \"%s\"%s."), arg2, tbuf);
    p = tprintf("%s whispers%s: %s", Name(player),
		gcount > 1 ? tbuf : "", arg2);
  }

  for (who = 0; who < gcount; who++) {
    notify_must_puppet(good[who], p);
    if (Location(good[who]) != Location(player))
      overheard = 0;
  }
  if (overheard) {
    dbref first = Contents(Location(player));
    if (!GoodObject(first))
      return;
    p = tprintf("%s whispers%s.", Name(player), tbuf);
    DOLIST(first, first) {
      overheard = 1;
      for (who = 0; who < gcount; who++) {
	if ((first == player) || (first == good[who])) {
	  overheard = 0;
	  break;
	}
      }
      if (overheard)
	notify_noecho(first, p);
    }
  }
  mush_free((Malloc_t) tbuf, "string");
}

/** Send a message to a list of dbrefs. To avoid repeated generation
 * of the NOSPOOF string, we set it up the first time we encounter
 * something Nospoof, and then check for it thereafter.
 * The list is destructively modified.
 * \param player the enactor.
 * \param list the list of players to pemit to, destructively modified.
 * \param message the message to pemit.
 * \param flags PEMIT_* flags
 */
void
do_pemit_list(dbref player, char *list, const char *message, int flags)
{
  char *bp, *p;
  char *nsbuf, *nspbuf;
  char *l;
  dbref who;
  int nospoof;

  /* If no list or no message, further processing is pointless. */
  if (!message || !*message || !list || !*list)
    return;

  nspbuf = nsbuf = NULL;
  nospoof = (flags & PEMIT_SPOOF) ? 0 : 1;
  list[BUFFER_LEN - 1] = '\0';
  l = trim_space_sep(list, ' ');

  while ((p = split_token(&l, ' '))) {
    who = noisy_match_result(player, p, NOTYPE, MAT_ABSOLUTE);
    if (GoodObject(who) && okay_pemit(player, who)) {
      if (nospoof && Nospoof(who)) {
	if (Paranoid(who)) {
	  if (!nspbuf) {
	    bp = nspbuf = mush_malloc(BUFFER_LEN, "string");
	    if (player == Owner(player))
	      safe_format(nspbuf, &bp, "[%s(#%d)->] %s", Name(player),
			  player, message);
	    else
	      safe_format(nspbuf, &bp, "[%s(#%d)'s %s(#%d)->] %s",
			  Name(Owner(player)), Owner(player),
			  Name(player), player, message);
	    *bp = '\0';
	  }
	  notify(who, nspbuf);
	} else {
	  if (!nsbuf) {
	    bp = nsbuf = mush_malloc(BUFFER_LEN, "string");
	    safe_format(nsbuf, &bp, "[%s->] %s", Name(player), message);
	    *bp = '\0';
	  }
	  notify(who, nsbuf);
	}
      } else {
	notify_must_puppet(who, message);
      }
    }
  }
  if (nsbuf)
    mush_free(nsbuf, "string");
  if (nspbuf)
    mush_free(nspbuf, "string");

}

/** Send a message to an object.
 * \param player the enactor.
 * \param arg1 the name of the object to pemit to.
 * \param arg2 the message to pemit.
 * \param flags PEMIT_* flags.
 */
void
do_pemit(dbref player, const char *arg1, const char *arg2, int flags)
{
  dbref who;
  int silent, nospoof;

  silent = (flags & PEMIT_SILENT) ? 1 : 0;
  nospoof = (flags & PEMIT_SPOOF) ? 0 : 1;

  switch (who = match_result(player, arg1, NOTYPE,
			     MAT_OBJECTS | MAT_HERE | MAT_CONTAINER)) {
  case NOTHING:
    notify(player, T("I don't see that here."));
    break;
  case AMBIGUOUS:
    notify(player, T("I don't know who you mean!"));
    break;
  default:
    if (!okay_pemit(player, who)) {
      notify_format(player,
		    T("I'm sorry, but %s wishes to be left alone now."),
		    Name(who));
      return;
    }
    if (!silent)
      notify_format(player, T("You pemit \"%s\" to %s."), arg2, Name(who));
    if (nospoof && Nospoof(who)) {
      if (Paranoid(who)) {
	if (player == Owner(player))
	  notify_format(who, "[%s(#%d)->%s] %s", Name(player), player,
			Name(who), arg2);
	else
	  notify_format(who, "[%s(#%d)'s %s(#%d)->%s] %s",
			Name(Owner(player)), Owner(player),
			Name(player), player, Name(who), arg2);
      } else
	notify_format(who, "[%s->%s] %s", Name(player), Name(who), arg2);
    } else {
      notify_must_puppet(who, arg2);
    }
    break;
  }
}

/** The pose and semipose command.
 * \param player the enactor.
 * \param tbuf1 the message to pose.
 * \param space if 1, put a space between name and pose; if 0, don't (semipose)
 */
void
do_pose(dbref player, const char *tbuf1, int space)
{
  dbref loc;

  loc = speech_loc(player);
  if (!GoodObject(loc))
    return;

  if(!CanSpeak(player,loc)) {
    notify(player, T("You may not speak here!"));
    return;
  }

  /* notify everybody */
  if (!space)
    notify_except(Contents(loc), NOTHING,
		  tprintf("%s %s", spname(player), tbuf1), NA_INTER_HEAR);
  else
    notify_except(Contents(loc), NOTHING,
		  tprintf("%s%s", spname(player), tbuf1), NA_INTER_HEAR);
}

/** The *wall commands.
 * \param player the enactor.
 * \param message message to broadcast.
 * \param target type of broadcast (all)
 * \param emit if 1, this is a wallemit.
 */
void
do_wall(dbref player, const char *message, enum wall_type target, int emit)
{
  const char *gap = "", *prefix;
  const char *mask;
  int pose = 0;

  if (!Can_Announce(player)) {
    notify(player, T("Posing as an admin could be hazardous to your health."));
    return;
  }
  /* put together the message and figure out what type it is */
  if (!emit) {
    gap = " ";
    switch (*message) {
    case SAY_TOKEN:
      if (CHAT_STRIP_QUOTE)
	message++;
      break;
    case SEMI_POSE_TOKEN:
      gap = "";
    case POSE_TOKEN:
      pose = 1;
      message++;
      break;
    }
  }

  if (!*message) {
    notify(player, T("What did you want to say?"));
    return;
  }
  /* to everyone */
  mask = NULL;
  prefix = WALL_PREFIX;

  /* broadcast the message */
  if (pose)
    flag_broadcast(mask, 0, "%s %s%s%s", prefix, Name(player), gap, message);
  else if (emit)
    flag_broadcast(mask, 0, "%s [%s]: %s", prefix, Name(player), message);
  else
    flag_broadcast(mask, 0,
		   "%s %s %s, \"%s\"", prefix, Name(player),
		   target == WALL_ALL ? "shouts" : "says", message);
}

/** The page command.
 * \param player the enactor.
 * \param arg1 the list of players to page.
 * \param arg2 the message to page.
 * \param cause the object that caused the command to run.
 * \param noeval if 1, page/noeval.
 * \param multipage if 1, a page/list; if 0, a page/blind.
 * \param override if 1, page/override.
 * \param has_eq if 1, the command had an = in it.
 */
void
do_page(dbref player, const char *arg1, const char *arg2, dbref cause,
	int noeval, int multipage, int override, int has_eq)
{
  dbref target;
  const char *message;
  const char *gap;
  int key;
  char *tbuf, *tp;
  char *tbuf2, *tp2;
  dbref good[100], almost_good[100], *gptr;
  int gcount = 0, ag_count = 0;
  char *msgbuf, *mb;
  const char *head;
  const char *hp = NULL;
  const char **start;
  char *current;
  int i;
  int repage = 0;
  int fails_lock;
  int is_haven;
  ATTR *a;
  char *alias;

  tp2 = tbuf2 = (char *) mush_malloc(BUFFER_LEN, "string");
  if (!tbuf2)
    mush_panic("Unable to allocate memory in do_page");

  if (*arg1 && has_eq) {
    /* page to=[msg]. Always evaluate to, maybe evaluate msg */
    process_expression(tbuf2, &tp2, &arg1, player, cause, cause,
		       PE_DEFAULT, PT_DEFAULT, NULL);
    *tp2 = '\0';
    head = tbuf2;
    message = arg2;
  } else if (arg2 && *arg2) {
    /* page =msg */
    message = arg2;
    repage = 1;
  } else {
    /* page msg */
    message = arg1;
    repage = 1;
  }
  if (repage) {
    a = atr_get_noparent(player, "LASTPAGED");
    if (!a || !*((hp = head = safe_atr_value(a)))) {
      notify(player, T("You haven't paged anyone since connecting."));
      mush_free((Malloc_t) tbuf2, "string");
      return;
    }
    if (!message || !*message) {
      notify_format(player, T("You last paged %s."), head);
      mush_free((Malloc_t) tbuf2, "string");
      if (hp)
	free((Malloc_t) hp);
      return;
    }
  }

  tp = tbuf = (char *) mush_malloc(BUFFER_LEN, "string");
  if (!tbuf)
    mush_panic("Unable to allocate memory in do_page");

  start = &head;
  while (head && *head && ((gcount+ag_count) < 99)) {
    current = next_in_list(start);
    target = lookup_player(current);
    if (!GoodObject(target))
      target = short_page(current);
    if (target == NOTHING) {
      notify_format(player,
		    T("I can't find who you're trying to page with: %s"),
		    current);
      safe_chr(' ', tbuf, &tp);
      safe_str_space(current, tbuf, &tp);
    } else if (target == AMBIGUOUS) {
      notify_format(player,
		    T("I'm not sure who you want to page with: %s"), current);
      safe_chr(' ', tbuf, &tp);
      safe_str_space(current, tbuf, &tp);
    } else {
      fails_lock = !(override || eval_lock(player, target, Page_Lock));
      is_haven = !override && Haven(target);
      if (!Connected(target) || (Dark(target) && (is_haven || fails_lock))) {
	/* A player isn't connected if they aren't connected, or if
	 * they're DARK and HAVEN, or DARK and the pagelock fails. */
	page_return(player, target, "Away", "AWAY",
		    tprintf(T("%s is not connected."), Name(target)));
	if (fails_lock)
	  fail_lock(player, target, Page_Lock, NULL, NOTHING);
	safe_chr(' ', tbuf, &tp);
	safe_str_space(current, tbuf, &tp);
#ifdef RPMODE_SYS
      } else if(RPMODE(player) && LEVEL(target) < 23) {
	      notify(player, "You can't do that in RPMODE.");
	      safe_chr(' ', tbuf, &tp);
	      safe_str_space(Name(target), tbuf, &tp);
#endif
      } else if (is_haven) {
	page_return(player, target, "Haven", "HAVEN",
		    tprintf(T("%s is not accepting any pages."), Name(target)));
	safe_chr(' ', tbuf, &tp);
	safe_str_space(Name(target), tbuf, &tp);
      } else if (fails_lock) {
	page_return(player, target, "Haven", "HAVEN",
		    tprintf(T("%s is not accepting your pages."),
			    Name(target)));
	fail_lock(player, target, Page_Lock, NULL, NOTHING);
	safe_chr(' ', tbuf, &tp);
	safe_str_space(Name(target), tbuf, &tp);
      } else if(RPMODE(target) && LEVEL(player) < 23 && LEVEL(target) < 23 ) {
	      page_return(player, target, 
	  	   "RPMode", "RPMODE", 
		      tprintf(T("%s is in RPMode and can not communicate OOCly at this moment."), Name(target)));
			      safe_chr(' ', tbuf , &tp);
			      safe_str_space(current, tbuf, &tp);
		      } else if(hidden(target) && !CanSee(player,target)){
	      /* this is a page that appears bad, but is good */
	          page_return(player, target, "Away", "AWAY",
	                   tprintf(T("%s is not connected."), Name(target)));
	          safe_chr(' ', tbuf, &tp);
	          safe_str_space(current, tbuf, &tp);

             almost_good[ag_count] = target;
             ag_count++;
      } else {
	/* This is a good page */
	good[gcount] = target;
	gcount++;
      }
    }
  }

  /* Reset tbuf2 to use later */
  tp2 = tbuf2;

  /* We now have an array of good[] dbrefs, a gcount of the good ones,
   * and a tbuf with bad ones.
   */

  /* We don't know what the heck's going on here, but we're not paging
   * anyone, this looks like a spam attack. */
  if ((gcount+ag_count) ==  98) {
    notify(player, T("You're trying to page too many people at once."));
    mush_free((Malloc_t) tbuf, "string");
    mush_free((Malloc_t) tbuf2, "string");
    if (hp)
      free((Malloc_t) hp);
    return;
  }

  /* We used to stick 'Unable to page' on at the start, but this is
   * faster for the 90% of the cases where there isn't a bad name
   * That may sound high, but, remember, we (almost) never have a bad
   * name if we're repaging, which is probably 75% of all pages */
  *tp = '\0';
  if (*tbuf)
    notify_format(player, T("Unable to page:%s"), tbuf);

  if (!gcount && !ag_count) {
    /* Well, that was a total waste of time. */
    mush_free((Malloc_t) tbuf, "string");
    mush_free((Malloc_t) tbuf2, "string");
    if (hp)
      free((Malloc_t) hp);
    return;
  }

  /* Can the player afford to pay for this thing? */
  if (!payfor(player, PAGE_COST * (gcount + ag_count))) {
    notify_format(player, T("You don't have enough %s."), MONIES);
    mush_free((Malloc_t) tbuf, "string");
    mush_free((Malloc_t) tbuf2, "string");
    if (hp)
      free((Malloc_t) hp);
    return;
  }

  /* Okay, we have a real page, the player can pay for it, and it's
   * actually going to someone. We're in this for keeps now. */

  /* Evaluate the message if we need to. */
  if (noeval)
    msgbuf = NULL;
  else {
    mb = msgbuf = (char *) mush_malloc(BUFFER_LEN, "string");
    if (!msgbuf)
      mush_panic("Unable to allocate memory in do_page");

    process_expression(msgbuf, &mb, &message, player, cause, cause,
		       PE_DEFAULT, PT_DEFAULT, NULL);
    *mb = '\0';
    message = msgbuf;
  }

  if (Haven(player))
    notify(player, T("You are set HAVEN and cannot receive pages."));

  /* Figure out what kind of message */
  global_eval_context.wenv[0] = (char *) message;
  gap = " ";
  switch (*message) {
  case SEMI_POSE_TOKEN:
    gap = "";
  case POSE_TOKEN:
    key = 1;
    message++;
    break;
  default:
    key = 3;
    break;
  }

  /* Reset tbuf and tbuf2 to use later */
  tp = tbuf;
  tp2 = tbuf2;

  /* tbuf2 is used to hold a fancy formatted list of names,
   * with commas and the word 'and' , if needed. */
  /* tbuf holds a space-separated list of names for repaging */

  /* Set up a pretty formatted list. */
  for (i = 0; i < gcount; i++) {
    if (i)
      safe_chr(' ', tbuf, &tp);
    safe_str_space(Name(good[i]), tbuf, &tp);
    safe_itemizer(i + 1, (i == gcount - 1), ",", T("and"), " ", tbuf2, &tp2);
    safe_str(Name(good[i]), tbuf2, &tp2);
  }
  *tp = '\0';
  *tp2 = '\0';
  (void) atr_add(player, "LASTPAGED", tbuf, GOD, NOTHING);

  /* Reset tbuf to use later */
  tp = tbuf;

  /* Figure out the one success message, and send it */
  
  if(gcount > 0) {
    if (key == 1)
       notify_format(player, T("Long distance to %s%s: %s%s%s"),
		  ((gcount > 1) && (!multipage)) ? "(blind) " : "", tbuf2,
		  Name(player), gap, message);
    else
       notify_format(player, T("You paged %s%s with '%s'"),
		  ((gcount > 1) && (!multipage)) ? "(blind) " : "", tbuf2,
		  message);
  }

  /* Figure out the 'name' of the player */
  if (PAGE_ALIASES && (alias = shortalias(player)) && *alias)
    current = tprintf("%s (%s)", Name(player), alias);
  else
    current = (char *) Name(player);

  /* Now, build the thing we want to send to the pagees,
   * and put it in tbuf */

  /* Build the header */
  if (key == 1) {
    safe_str(T("From afar"), tbuf, &tp);
    if ((gcount > 1) && (multipage)) {
      safe_str(T(" (to "), tbuf, &tp);
      safe_str(tbuf2, tbuf, &tp);
      safe_chr(')', tbuf, &tp);
    }
    safe_str(", ", tbuf, &tp);
    safe_str(current, tbuf, &tp);
    safe_str(gap, tbuf, &tp);
  } else {
    safe_str(current, tbuf, &tp);
    safe_str(T(" pages"), tbuf, &tp);
    if ((gcount > 1) && (multipage)) {
      safe_chr(' ', tbuf, &tp);
      safe_str(tbuf2, tbuf, &tp);
    }
    safe_str(": ", tbuf, &tp);
  }
  /* Tack on the message */
  safe_str(message, tbuf, &tp);
  *tp = '\0';

  /* Tell each page recipient with tbuf */
  for (i = 0; i < (gcount+ag_count); i++) {

    if(i >= gcount || gcount == 0)
	    gptr = almost_good + (i-gcount);
    else
	    gptr = good + i;
    
    if (!IsPlayer(player) && Nospoof((dbref)*gptr))
      notify_format((dbref)*gptr, "[#%d] %s%s", player,(i >= gcount || gcount == 0) ? "Hidden Receive>": "" ,  tbuf);
    else
      notify_format((dbref)*gptr, "%s%s", (i >= gcount || gcount == 0) ? "Hidden Receive>": "" ,tbuf);

    if((i < gcount) && gcount != 0)
      page_return(player, (dbref)*gptr, "Idle", "IDLE", NULL);
  }

  mush_free((Malloc_t) tbuf, "string");
  mush_free((Malloc_t) tbuf2, "string");
  if (msgbuf)
    mush_free((Malloc_t) msgbuf, "string");
  if (hp)
    free((Malloc_t) hp);
}


/** Does a message match a filter pattern on an object?
 * \param thing object with the filter.
 * \param msg message to match.
 * \param flag if 0, filter; if 1, infilter.
 * \retval 1 message matches filter.
 * \retval 0 message does not match filter.
 */
int
filter_found(dbref thing, const char *msg, int flag)
{
  char *filter;
  ATTR *a;
  char *p, *bp;
  char *temp;			/* need this so we don't leak memory     
				 * by failing to free the storage
				 * allocated by safe_uncompress
				 */
  int i;
  int matched = 0;

  if (!flag)
    a = atr_get(thing, "FILTER");
  else
    a = atr_get(thing, "INFILTER");
  if (!a)
    return matched;

  filter = safe_atr_value(a);
  temp = filter;

  for (i = 0; (i < MAX_ARG) && !matched; i++) {
    p = bp = filter;
    process_expression(p, &bp, (char const **) &filter, 0, 0, 0,
		       PE_NOTHING, PT_COMMA, NULL);
    if (*filter == ',')
      *filter++ = '\0';
    if (*p == '\0' && *filter == '\0')	/* No more filters */
      break;
    if (*p == '\0')		/* Empty filter */
      continue;
    if (AF_Regexp(a))
      matched = quick_regexp_match(p, msg, AF_Case(a));
    else
      matched = local_wild_match_case(p, msg, AF_Case(a));
  }

  free((Malloc_t) temp);
  return matched;
}

/** Copy a message into a buffer, adding an object's PREFIX attribute.
 * \param thing object with prefix attribute.
 * \param msg message.
 * \param tbuf1 destination buffer.
 */
void
make_prefixstr(dbref thing, const char *msg, char *tbuf1)
{
  char *bp, *asave;
  char const *ap;
  char *wsave[10], *preserve[NUMQ];
  ATTR *a;
  int j;

  a = atr_get(thing, "PREFIX");

  bp = tbuf1;

  if (!a) {
    safe_str("From ", tbuf1, &bp);
    safe_str(Name(IsExit(thing) ? Source(thing) : thing), tbuf1, &bp);
    safe_str(", ", tbuf1, &bp);
  } else {
    for (j = 0; j < 10; j++) {
      wsave[j] = global_eval_context.wenv[j];
      global_eval_context.wenv[j] = NULL;
    }
    global_eval_context.wenv[0] = (char *) msg;
    save_global_regs("prefix_save", preserve);
    asave = safe_atr_value(a);
    ap = asave;
    process_expression(tbuf1, &bp, &ap, thing, orator, orator,
		       PE_DEFAULT, PT_DEFAULT, NULL);
    free((Malloc_t) asave);
    restore_global_regs("prefix_save", preserve);
    for (j = 0; j < 10; j++)
      global_eval_context.wenv[j] = wsave[j];
    if (bp != tbuf1)
      safe_chr(' ', tbuf1, &bp);
  }
  safe_str(msg, tbuf1, &bp);
  *bp = '\0';
  return;
}

/** pass a message on, for AUDIBLE, prepending a prefix, unless the
 * message matches a filter pattern.
 * \param thing object to check for filter and prefix.
 * \param msg message to pass.
 */
void
propagate_sound(dbref thing, const char *msg)
{
  char tbuf1[BUFFER_LEN];
  dbref loc = Location(thing);
  dbref pass[2];

  if (!GoodObject(loc))
    return;

  /* check to see that filter doesn't suppress message */
  if (filter_found(thing, msg, 0))
    return;

  /* figure out the prefix */
  make_prefixstr(thing, msg, tbuf1);

  /* Exits pass the message on to things in the next room.
   * Objects pass the message on to the things outside.
   * Don't tell yourself your own message.
   */

  if (IsExit(thing)) {
    notify_anything(orator, na_next, &Contents(loc), NULL, NA_INTER_HEAR,
		    tbuf1);
  } else {
    pass[0] = Contents(loc);
    pass[1] = thing;
    notify_anything(orator, na_nextbut, pass, NULL, NA_INTER_HEAR, tbuf1);
  }
}

static void
do_audible_stuff(dbref loc, dbref *excs, int numexcs, const char *msg)
{
  dbref e;
  int exclude = 0;
  int i;

  if (!Audible(loc))
    return;

  if (IsRoom(loc)) {
    DOLIST(e, Exits(loc)) {
      if (Audible(e))
	propagate_sound(e, msg);
    }
  } else {
    for (i = 0; i < numexcs; i++)
      if (*(excs + i) == loc)
	exclude = 1;
    if (!exclude)
      propagate_sound(loc, msg);
  }
}

/** notify_anthing() wrapper to notify everyone in a location except one
 * object.
 * \param first object in location to notify.
 * \param exception dbref of object not to notify, or NOTHING.
 * \param msg message to send.
 * \param flags flags to pass to notify_anything().
 */
void
notify_except(dbref first, dbref exception, const char *msg, int flags)
{
  dbref loc;
  dbref pass[2];

  if (!GoodObject(first))
    return;
  loc = Location(first);
  if (!GoodObject(loc))
    return;

  if (exception == NOTHING)
    exception = AMBIGUOUS;

  pass[0] = loc;
  pass[1] = exception;

  notify_anything(orator, na_except, pass, ns_esnotify, flags, msg);

  do_audible_stuff(loc, &pass[1], 1, msg);
}

/** notify_anthing() wrapper to notify everyone in a location except two
 * objects.
 * \param first object in location to notify.
 * \param exc1 dbref of one object not to notify, or NOTHING.
 * \param exc2 dbref of another object not to notify, or NOTHING.
 * \param msg message to send.
 * \param flags interaction flags to control type of interaction.
 */
void
notify_except2(dbref first, dbref exc1, dbref exc2, const char *msg, int flags)
{
  dbref loc;
  dbref pass[3];

  if (!GoodObject(first))
    return;
  loc = Location(first);
  if (!GoodObject(loc))
    return;

  if (exc1 == NOTHING)
    exc1 = AMBIGUOUS;
  if (exc2 == NOTHING)
    exc2 = AMBIGUOUS;

  pass[0] = loc;
  pass[1] = exc1;
  pass[2] = exc2;

  notify_anything(orator, na_except2, pass, NULL, flags, msg);

  do_audible_stuff(loc, &pass[1], 2, msg);
}

/** The think command.
 * \param player the enactor.
 * \param message the message to think.
 */
void
do_think(dbref player, const char *message)
{
  notify(player, message);
}


/** The emit command.
 * \verbatim
 * This implements @emit.
 * \endverbatim
 * \param player the enactor.
 * \param tbuf1 the message to emit.
 * \param flags bitmask of notification flags.
 */
void
do_emit(dbref player, const char *tbuf1, int flags)
{
  dbref loc;
  int na_flags = NA_INTER_HEAR;

  loc = speech_loc(player);
  if (!GoodObject(loc))
    return;

  if(!CanSpeak(player,loc)) {
    notify(player, T("You may not speak here!"));
    return;
  }

  /* notify everybody */
  if (flags & PEMIT_SPOOF)
    na_flags |= NA_SPOOF;
  notify_anything(player, na_loc, &loc, ns_esnotify, na_flags, tbuf1);

  do_audible_stuff(loc, NULL, 0, tbuf1);
}

/** Remit a message to a single room.
 * \param player the enactor.
 * \param target string containing dbref of room to remit in.
 * \param msg message to emit.
 * \param flags PEMIT_* flags
 */
static void
do_one_remit(dbref player, const char *target, const char *msg, int flags)
{
  dbref room;
  int na_flags = NA_INTER_HEAR;
  room = match_result(player, target, NOTYPE, MAT_EVERYTHING);
  if (!GoodObject(room)) {
    notify(player, T("I can't find that."));
  } else {
    if (IsExit(room)) {
      notify(player, T("There can't be anything in that!"));
    } else if (!okay_pemit(player, room)) {
      notify_format(player,
		    T("I'm sorry, but %s wishes to be left alone now."),
		    Name(room));
    } else if (!CanSpeak(player,room)) {
      notify(player, T("You may not speak there!"));
    } else {
      if (!(flags & PEMIT_SILENT) && (Location(player) != room)) {
	const char *rmno;
	rmno = unparse_object(player, room);
	notify_format(player, T("You remit, \"%s\" in %s"), msg, rmno);
      }
      if (flags & PEMIT_SPOOF)
	na_flags |= NA_SPOOF;
      notify_anything_loc(player, na_loc, &room, ns_esnotify, na_flags,
			  msg, room);
      do_audible_stuff(room, NULL, 0, msg);
    }
  }
}

/** Remit a message
 * \verbatim
 * This implements @remit.
 * \endverbatim
 * \param player the enactor.
 * \param arg1 string containing dbref(s) of rooms to remit it.
 * \param arg2 message to emit.
 * \param flags for remit.
 */
void
do_remit(dbref player, char *arg1, const char *arg2, int flags)
{
  if (flags & PEMIT_LIST) {
    /* @remit/list */
    char *current;
    arg1 = trim_space_sep(arg1, ' ');
    while ((current = split_token(&arg1, ' ')) != NULL)
      do_one_remit(player, current, arg2, flags);
  } else {
    do_one_remit(player, arg1, arg2, flags);
  }
}

/** Emit a message to the absolute location of enactor.
 * \param player the enactor.
 * \param tbuf1 message to emit.
 * \param flags bitmask of notification flags.
 */
void
do_lemit(dbref player, const char *tbuf1, int flags)
{
  /* give a message to the "absolute" location of an object */
  dbref room;
  int rec = 0;
  int na_flags = NA_INTER_HEAR;
  int silent = (flags & PEMIT_SILENT) ? 1 : 0;

  /* only players and things may use this command */
  if (!Mobile(player))
    return;

  /* prevent infinite loop if player is inside himself */
  if (((room = Location(player)) == player) || !GoodObject(room)) {
    notify(player, T("Invalid container object."));
    do_rawlog(LT_ERR, T("** BAD CONTAINER **  #%d is inside #%d."), player,
	      room);
    return;
  }
  while (!IsRoom(room) && (rec < 15)) {
    room = Location(room);
    rec++;
  }
  if (rec > 15) {
    notify(player, T("Too many containers."));
    return;
  } else if (!CanSpeak(player, room)) {
    notify(player, T("You may not speak there!"));
  } else {
    if (!silent && (Location(player) != room))
      notify_format(player, T("You lemit: \"%s\""), tbuf1);
    if (flags & PEMIT_SPOOF)
      na_flags |= NA_SPOOF;
    notify_anything(player, na_loc, &room, ns_esnotify, na_flags, tbuf1);
  }
}

/** notify_anything() function for zone emits.
 * \param current unused.
 * \param data array of notify data.
 * \return last object in zone, or NOTHING.
 */
dbref
na_zemit(dbref current __attribute__ ((__unused__)), void *data)
{
  dbref this;
  dbref room;
  dbref *dbrefs = data;
  this = dbrefs[0];
  do {
    if (this == NOTHING) {
      for (room = dbrefs[1]; room < db_top; room++) {
	if (IsRoom(room) && (Zone(room) == dbrefs[2])
	    && CanSpeak(dbrefs[3], room))
	  break;
      }
      if (!(room < db_top))
	return NOTHING;
      this = room;
      dbrefs[1] = room + 1;
    } else if (IsRoom(this)) {
      this = Contents(this);
    } else {
      this = Next(this);
    }
  } while ((this == NOTHING) || (this == dbrefs[4]));
  dbrefs[0] = this;
  return this;
}

/** The zemit command.
 * \verbatim
 * This implements @zemit and @nszemit.
 * \endverbatim
 * \param player the enactor.
 * \param arg1 string containing dbref of ZMO.
 * \param arg2 message to emit.
 * \param flags bitmask of notificati flags.
 */
void
do_zemit(dbref player, const char *arg1, const char *arg2, int flags)
{
  const char *where;
  dbref zone;
  dbref pass[5];
  int na_flags = NA_INTER_HEAR;

  zone = match_result(player, arg1, NOTYPE, MAT_ABSOLUTE);
  if (!GoodObject(zone)) {
    notify(player, T("Invalid zone."));
    return;
  }
  if (!controls(player, zone)) {
    notify(player, T("Permission denied."));
    return;
  }
  /* this command is computationally expensive */
  if (!payfor(player, FIND_COST)) {
    notify(player, T("Sorry, you don't have enough money to do that."));
    return;
  }
  where = unparse_object(player, zone);
  notify_format(player, T("You zemit, \"%s\" in zone %s"), arg2, where);

  pass[0] = NOTHING;
  pass[1] = 0;
  pass[2] = zone;
  pass[3] = player;
  pass[4] = player;
  if (flags & PEMIT_SPOOF)
    na_flags |= NA_SPOOF;
  notify_anything(player, na_zemit, pass, ns_esnotify, na_flags, arg2);
}
