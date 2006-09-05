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
#include "htab.h"
#include "confmagic.h"


/** Hash table of player names */
HASHTAB htab_player_list;

static int hft_initialized = 0;
static void init_hft(void);

static void
init_hft(void)
{
  hashinit(&htab_player_list, 256, sizeof(dbref));
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
 * \param alias name to use as hash table key for player, if given.
 */
void
add_player(dbref player, const char *alias)
{
  if (!hft_initialized)
    init_hft();
  if (alias)
    hashadd(strupper(alias), (void *) player, &htab_player_list);
  else
    hashadd(strupper(Name(player)), (void *) player, &htab_player_list);
}

/** Look up a player in the player list htab (or by dbref).
 * \param name name of player to find.
 * \return dbref of player, or NOTHING.
 */
dbref
lookup_player(const char *name)
{
  int p;
  void *hval;

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
  hval = hashfind(strupper(name), &htab_player_list);
  if (!hval)
    return NOTHING;
  return (dbref) hval;
  /* By the way, there's a flaw in this code. If #0 was a player, we'd
   * hash its name with a dbref of (void *)0, aka NULL, so we'd never
   * be able to retrieve that player. However, we assume that #0 will
   * always be the base room, and never a player, so that's ok.
   * Nathan Baum offered a fix (hash the value + 1 and subtract 1 on
   * lookup) but we (foolishly?) cling to our assumption and don't bother.
   */
}

/** Remove a player from the player list htab.
 * \param player dbref of player to remove.
 * \param alias key to remove if given.
 */
void
delete_player(dbref player, const char *alias)
{
  if (alias)
    hashdelete(strupper(alias), &htab_player_list);
  else
    hashdelete(strupper(Name(player)), &htab_player_list);
}
