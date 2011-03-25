/**
 * \file plyrlist.c
 *
 * \brief Player list management for PennMUSH.
 *
 *
 */

#include "config.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "copyrite.h"

#include "conf.h"
#include "externs.h"
#include "mushdb.h"
#include "dbdefs.h"
#include "flags.h"
#include "attrib.h"
#include "htab.h"
#include "confmagic.h"


/** Hash table of player names */
HASHTAB htab_player_list;

static int hft_initialized = 0;
static void init_hft(void);
static void delete_dbref(void *);

/** Free a player_dbref struct. */
static void
delete_dbref(void *data)
{
  mush_free(data, "plyrlist.entry");
}


static void
init_hft(void)
{
  hash_init(&htab_player_list, 256, sizeof(dbref), delete_dbref);
  hft_initialized = 1;
}

/** Clear the player list htab. */
void
clear_players(void)
{
  if (hft_initialized)
    hashflush(&htab_player_list, 256);
  else
    init_hft();
}


/** Add a player to the player list htab.
 * \param player dbref of player to add.
 */
void
add_player(dbref player)
{
   dbref *p;
   if (!hft_initialized)
     init_hft();
   p = mush_malloc(sizeof *p, "plyrlist.entry");
   if (!p)
     mush_panic(T("Unable to allocate memory in plyrlist!"));
   *p = player;
   hashadd(strupper(Name(player)), p, &htab_player_list);
}
 

/** Add a player's alias list to the player list htab.
 * \param player dbref of player to add.
 * \param alias list of names ot use as hash table keys for player,
 * semicolon-separated.
 */
void
add_player_alias(dbref player, const char *alias)
{
  long tmp;
  char tbuf1[BUFFER_LEN], *s, *sp;
  if (!hft_initialized)
    init_hft();
  if (!alias) {
    add_player(player);
    return;
  }
  strncpy(tbuf1, alias, BUFFER_LEN - 1);
  tbuf1[BUFFER_LEN - 1] = '\0';
  s = trim_space_sep(tbuf1, ALIAS_DELIMITER);
  while (s) {
    sp = split_token(&s, ALIAS_DELIMITER);
    while (sp && *sp && *sp == ' ')
      sp++;
    if (sp && *sp) {
      tmp = player;
      hashadd(strupper(sp), (void *) tmp, &htab_player_list);
    }
  }
}

/** Look up a player in the player list htab (or by dbref).
 * \param name name of player to find.
 * \return dbref of player, or NOTHING.
 */
dbref
lookup_player(const char *name)
{
  int p;

  if (!name || !*name)
    return NOTHING;
  if (*name == NUMBER_TOKEN) {
    name++;
    if (!is_strict_number(name))
      return NOTHING;
    p = atoi(name);
    return ((GoodObject(p) && IsPlayer(p)) ? p : NOTHING);
  }
  if (*name == LOOKUP_TOKEN)
    name++;
  return lookup_player_name(name);
}

/** Look up a player in the player list htab only.
 * \param name name of player to find.
 * \return dbref of player, or NOTHING.
 */
dbref
lookup_player_name(const char *name)
{
  dbref *p;
  p = hashfind(strupper(name), &htab_player_list);
  if (!p)
    return NOTHING;
  return *p;
}
 
/** Remove a player from the player list htab.
 * \param player dbref of player to remove.
 * \param alias key to remove if given.
 */
void
delete_player(dbref player, const char *alias)
{
  if (!hft_initialized) {
    init_hft();
    return;
  }
  if (alias) {
    /* This could be a compound alias, in which case we need to delete
     * them all, but we shouldn't delete the player's own name!
     */
    char tbuf1[BUFFER_LEN], *s, *sp;
    strncpy(tbuf1, alias, BUFFER_LEN - 1);
    tbuf1[BUFFER_LEN - 1] = '\0';
    s = trim_space_sep(tbuf1, ALIAS_DELIMITER);
    while (s) {
      sp = split_token(&s, ALIAS_DELIMITER);
      while (sp && *sp && *sp == ' ')
      sp++;
      if (sp && *sp) {
	dbref *p;
	p = mush_malloc(sizeof *p, "plyrlist.entry");
	if (!p)
	  mush_panic(T("Unable to allocate memory in plyrlist!"));
	*p = player;
	hashadd(strupper(sp), p, &htab_player_list);
      }
    }
  } else
    hashdelete(strupper(Name(player)), &htab_player_list);
}

/** Reset all of a player's player list entries (names/aliases).
 * This is called when a player changes name or alias.
 * We remove all their old entries, and add back their new ones.
 * \param player dbref of player
 * \param oldname player's former name (NULL if not changing)
 * \param oldalias player's former aliases (NULL if not changing)
 * \param name player's new name
 * \param alias player's new aliases
 */
void
reset_player_list(dbref player, const char *oldname, const char *oldalias,
		  const char *name, const char *alias)
{
  char tbuf1[BUFFER_LEN];
  char tbuf2[BUFFER_LEN];
  if (!oldname) {
    oldname = Name(player);
    name = Name(player);
  }
  if (oldalias) {
    strncpy(tbuf1, oldalias, BUFFER_LEN - 1);
    tbuf1[BUFFER_LEN - 1] = '\0';
    if (alias) {
      strncpy(tbuf2, alias, BUFFER_LEN - 1);
      tbuf2[BUFFER_LEN - 1] = '\0';
    } else {
      tbuf2[0] = '\0';
    }
  } else {
    /* We are not changing aliases, just name, but we need to get the
     * aliases anyway, since we may change name to something that's
     * in the alias, and thus must not be deleted.
     */
    ATTR *a = atr_get_noparent(player, "ALIAS");
    if (a) {
      strncpy(tbuf1, atr_value(a), BUFFER_LEN - 1);
      tbuf1[BUFFER_LEN - 1] = '\0';
    } else {
      tbuf1[0] = '\0';
    }
    strcpy(tbuf2, tbuf1);
  }
  /* Delete all the old stuff */
  delete_player(player, tbuf1);
  delete_player(player, NULL);
  /* Add in the new stuff */
  add_player_alias(player, name);
  add_player_alias(player, tbuf2);
}
