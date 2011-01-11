/**
 * \file flags.c
 *
 * \brief Flags and powers (and sometimes object types) in PennMUSH
 *
 *
 * Functions to cope with flags and powers (and also object types,
 * in some cases).
 *
 * Flag functions actually involve with several related entities:
 *  Flag definitions (FLAG objects)
 *  Bitmasks representing sets of flags (object_flag_type's). The
 *    bits involved may differ between dbs.
 *  Strings of space-separated flag names. This is a string representation
 *    of a bitmask, suitable for display and storage
 *  Strings of flag characters
 *
 */

#include "config.h"

#ifdef I_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif
#include <string.h>
#include <stdlib.h>

#include "conf.h"
#include "externs.h"
#include "boolexp.h"
#include "command.h"
#include "attrib.h"
#include "mushdb.h"
#include "parse.h"
#include "match.h"
#include "ptab.h"
#include "htab.h"
#include "privtab.h"
#include "game.h"
#include "flags.h"
#include "dbdefs.h"
#include "lock.h"
#include "log.h"
#include "dbio.h"
#include "oldflags.h"
#include "division.h"
#include "ansi.h"
#include "confmagic.h"



static int can_set_flag(dbref player, dbref thing, FLAG *flagp, int negate);
static FLAG *letter_to_flagptr(FLAGSPACE * n, char c, int type);
static void flag_add(FLAGSPACE * n, const char *name, FLAG *f);
static int has_flag(dbref thing, FLAG *f);

static FLAG *flag_read(FILE * in);
static FLAG *flag_read_oldstyle(FILE * in);
static void flag_read_all_oldstyle(FILE * in, const char *ns);
static void flag_write(FILE * out, FLAG *f, const char *name);
static FLAG *flag_hash_lookup(FLAGSPACE * n, const char *name, int type);
static FLAG *clone_flag(FLAG *f);
static FLAG *new_flag(void);
static void flag_add_additional(void);
static char *list_aliases(FLAGSPACE * n, FLAG *given);
static void realloc_object_flag_bitmasks(int numbytes);
static FLAG *match_flag_ns(FLAGSPACE * n, const char *name);
static void flag_fake_read(FILE *);

PTAB ptab_flag;                 /**< Table of flags by name, inc. aliases */
HASHTAB htab_flagspaces;                /**< Hash of flagspaces */

extern PTAB ptab_command;       /* Uses flag bitmasks */
extern long flagdb_flags;

/** Attempt to find a flagspace from its name */
#define Flagspace_Lookup(n,ns)  if (!(n = (FLAGSPACE *)hashfind(ns,&htab_flagspaces))) mush_panic("Unable to locate flagspace");

/** This is the old default flag table. We still use it when we have to
 * convert old dbs, but once you have a converted db, it's the flag
 * table in the db that counts, not this one.
 */
/* Name     Letter   Type(s)   Flag   Perms   Negate_Perm */
FLAG flag_table[] = {
  {"CHOWN_OK", 'C', NOTYPE, CHOWN_OK, F_ANY, F_ANY},
  {"DARK", 'D', NOTYPE, DARK, F_ANY, F_ANY},
  {"GOING", 'G', NOTYPE, GOING, F_INTERNAL, F_ANY},
  {"HAVEN", 'H', NOTYPE, HAVEN, F_ANY, F_ANY},
  {"LINK_OK", 'L', NOTYPE, LINK_OK, F_ANY, F_ANY},
  {"OPAQUE", 'O', NOTYPE, LOOK_OPAQUE, F_ANY, F_ANY},
  {"QUIET", 'Q', NOTYPE, QUIET, F_ANY, F_ANY},
  {"STICKY", 'S', NOTYPE, STICKY, F_ANY, F_ANY},
  {"UNFINDABLE", 'U', NOTYPE, UNFIND, F_ANY, F_ANY},
  {"VISUAL", 'V', NOTYPE, VISUAL, F_ANY, F_ANY},
  {"SAFE", 'X', NOTYPE, SAFE, F_ANY, F_ANY},
  {"AUDIBLE", 'a', NOTYPE, AUDIBLE, F_ANY, F_ANY},
  {"DEBUG", 'b', NOTYPE, DEBUGGING, F_ANY, F_ANY},
  {"NO_WARN", 'w', NOTYPE, NOWARN, F_ANY, F_ANY},
  {"ENTER_OK", 'e', NOTYPE, ENTER_OK, F_ANY, F_ANY},
  {"HALT", 'h', NOTYPE, HALT, F_ANY, F_ANY},
  {"NO_COMMAND", 'n', NOTYPE, NO_COMMAND, F_ANY, F_ANY},
  {"LIGHT", 'l', NOTYPE, LIGHT, F_ANY, F_ANY},
  {"TRANSPARENT", 't', NOTYPE, TRANSPARENTED, F_ANY, F_ANY},
  {"VERBOSE", 'v', NOTYPE, VERBOSE, F_ANY, F_ANY},
  {"ANSI", 'A', TYPE_PLAYER, PLAYER_ANSI, F_ANY, F_ANY},
  {"COLOR", 'C', TYPE_PLAYER, PLAYER_COLOR, F_ANY, F_ANY},
  {"MONITOR", 'M', TYPE_PLAYER | TYPE_ROOM | TYPE_THING | TYPE_DIVISION, 0, F_ANY, F_ANY},
  {"NOSPOOF", '"', TYPE_PLAYER, PLAYER_NOSPOOF, F_ANY | F_ODARK,
   F_ANY | F_ODARK},
  {"SHARED", 'Z', TYPE_PLAYER, PLAYER_ZONE, F_ANY, F_ANY},
  {"TRACK_MONEY", '\0', TYPE_PLAYER, 0, F_ANY, F_ANY},
  {"CONNECTED", 'c', TYPE_PLAYER, PLAYER_CONNECT, F_INTERNAL | F_MDARK,
   F_INTERNAL | F_MDARK},
  {"GAGGED", 'g', TYPE_PLAYER, PLAYER_GAGGED, F_PRIVILEGE, F_PRIVILEGE},
  {"MYOPIC", 'm', TYPE_PLAYER, PLAYER_MYOPIC, F_ANY, F_ANY},
  {"TERSE", 'x', TYPE_PLAYER | TYPE_THING, PLAYER_TERSE, F_ANY, F_ANY},
  {"FIXED", 'F', TYPE_PLAYER, PLAYER_FIXED, F_PRIVILEGE, F_PRIVILEGE},
#ifdef VACATION_FLAG
  {"ON-VACATION", 'o', TYPE_PLAYER, PLAYER_VACATION, F_ANY, F_ANY},
#endif
  {"SUSPECT", 's', TYPE_PLAYER, PLAYER_SUSPECT, F_PRIVILEGE | F_DARK,
   F_PRIVILEGE | F_DARK},
  {"PARANOID", '\0', TYPE_PLAYER, PLAYER_PARANOID, F_ANY | F_ODARK,
   F_ANY | F_ODARK},
  {"INPROGRAM", '\0', TYPE_PLAYER, PLAYER_PROG, F_INTERNAL, F_INTERNAL},
  {"NOACCENTS", '~', TYPE_PLAYER, PLAYER_NOACCENTS, F_ANY, F_ANY},
  {"DESTROY_OK", 'd', TYPE_THING, THING_DEST_OK, F_ANY, F_ANY},
  {"PUPPET", 'p', TYPE_THING | TYPE_DIVISION, THING_PUPPET, F_ANY, F_ANY},
  {"NO_LEAVE", 'N', TYPE_THING, THING_NOLEAVE, F_ANY, F_ANY},
  {"LISTEN_PARENT", '^', TYPE_THING | TYPE_ROOM | TYPE_DIVISION, 0, F_ANY, F_ANY},
  {"Z_TEL", 'Z', TYPE_THING | TYPE_ROOM, 0, F_ANY, F_ANY},
  {"ABODE", 'A', TYPE_ROOM, ROOM_ABODE, F_ANY, F_ANY},
  {"FLOATING", 'F', TYPE_ROOM, ROOM_FLOATING, F_ANY, F_ANY},
  {"JUMP_OK", 'J', TYPE_ROOM, ROOM_JUMP_OK, F_ANY, F_ANY},
  {"NO_TEL", 'N', TYPE_ROOM, ROOM_NO_TEL, F_ANY, F_ANY},
#ifdef UNINSPECTED_FLAG
  {"UNINSPECTED", 'u', TYPE_ROOM, ROOM_UNINSPECT, F_PRIVILEGE, F_PRIVILEGE},
#endif
  {"CLOUDY", 'x', TYPE_EXIT, EXIT_CLOUDY, F_ANY, F_ANY},
  {"GOING_TWICE", '\0', NOTYPE, GOING_TWICE, F_INTERNAL | F_DARK,
   F_INTERNAL | F_DARK},
  {NULL, '\0', 0, 0, 0, 0}
};

/** The old table to kludge multi-type toggles. Now used only 
 * for conversion.
 */
static FLAG hack_table[] = {
  {"MONITOR", 'M', TYPE_PLAYER, PLAYER_MONITOR, F_PRIVILEGE, F_PRIVILEGE},
  {"MONITOR", 'M', TYPE_THING, THING_LISTEN, F_ANY, F_ANY},
  {"MONITOR", 'M', TYPE_ROOM, ROOM_LISTEN, F_ANY, F_ANY},
  {"LISTEN_PARENT", '^', TYPE_THING, THING_INHEARIT, F_ANY, F_ANY},
  {"LISTEN_PARENT", '^', TYPE_ROOM, ROOM_INHEARIT, F_ANY, F_ANY},
  {"Z_TEL", 'Z', TYPE_THING, THING_Z_TEL, F_ANY, F_ANY},
  {"Z_TEL", 'Z', TYPE_ROOM, ROOM_Z_TEL, F_ANY, F_ANY},
  {NULL, '\0', 0, 0, 0, 0}
};


/** A table of types, as if they were flags. Some functions that
 * expect flags also accept, for historical reasons, types.
 */
static FLAG type_table[] = {
  {"PLAYER", 'P', TYPE_PLAYER, TYPE_PLAYER, F_INTERNAL, F_INTERNAL},
  {"ROOM", 'R', TYPE_ROOM, TYPE_ROOM, F_INTERNAL, F_INTERNAL},
  {"EXIT", 'E', TYPE_EXIT, TYPE_EXIT, F_INTERNAL, F_INTERNAL},
  {"THING", 'T', TYPE_THING, TYPE_THING, F_INTERNAL, F_INTERNAL},
  {"DIVISION", 'D', TYPE_DIVISION, TYPE_DIVISION, F_INTERNAL, F_INTERNAL},
  {NULL, '\0', 0, 0, 0, 0}
};

/** A table of types, as privileges. */
static PRIV type_privs[] = {
  {"PLAYER", 'P', TYPE_PLAYER, TYPE_PLAYER},
  {"ROOM", 'R', TYPE_ROOM, TYPE_ROOM},
  {"EXIT", 'E', TYPE_EXIT, TYPE_EXIT},
  {"THING", 'T', TYPE_THING, TYPE_THING},
  {"DIVISION", 'D', TYPE_DIVISION, TYPE_DIVISION},
  {NULL, '\0', 0, 0}
};

/** The old default aliases for flags. This table is only used in conversion
 * of old databases. Once a database is converted, the alias list in the
 * database is what counts.
 */
static FLAG_ALIAS flag_alias_tab[] = {
  {"TRACE", "DEBUG"},
  {"NOWARN", "NO_WARN"},
  {"NOCOMMAND", "NO_COMMAND"},
  {"LISTENER", "MONITOR"},
  {"WATCHER", "MONITOR"},
  {"ZONE", "SHARED"},
  {"COLOUR", "COLOR"},
#ifdef VACATION_FLAG
  {"VACATION", "ON-VACATION"},
#endif
  {"DEST_OK", "DESTROY_OK"},
  {"NOLEAVE", "NO_LEAVE"},
  {"TEL_OK", "JUMP_OK"},
  {"TELOK", "JUMP_OK"},
  {"TEL-OK", "JUMP_OK"},
  {"^", "LISTEN_PARENT"},

  {NULL, NULL}
};

/** The table of flag privilege bits. */
static PRIV flag_privs[] = {
  {"owned", '\0', F_OWNED, F_OWNED},
  {"privilege", '\0', F_PRIVILEGE, F_PRIVILEGE},
  {"god", '\0', F_GOD, F_GOD},
  {"internal", '\0', F_INTERNAL, F_INTERNAL},
  {"dark", '\0', F_DARK, F_DARK},
  {"mdark", '\0', F_MDARK, F_MDARK},
  {"odark", '\0', F_ODARK, F_ODARK},
  {"disabled", '\0', F_DISABLED, F_DISABLED},
  {"self", '\0', F_SELF, F_SELF},
  {NULL, '\0', 0, 0}
};



/*---------------------------------------------------------------------------
 * Flag definition functions, including flag hash table handlers 
 */

/** Convenience function to return a pointer to a flag struct
 * given the name.
 * \param name name of flag to find.
 * \return poiner to flag structure, or NULL.
 */
FLAG *
match_flag(const char *name)
{
  return (FLAG *) match_flag_ns(hashfind("FLAG", &htab_flagspaces), name);
}

/** Convenience function to return a pointer to a flag struct
 * given the name.
 * \param name name of flag to find.
 * \return poiner to flag structure, or NULL.
 */
static FLAG *
match_flag_ns(FLAGSPACE * n, const char *name)
{
  return (FLAG *) ptab_find(n->tab, name);
}

/** Given a flag name and mask of types, return a pointer to a flag struct.
 * This function first attempts to match the flag name to a flag of the
 * right type. If that fails, it tries to match flag characters if the
 * name is a single character. If all else fails, it tries to match
 * against an object type name.
 * \param n pointer to flagspace to search.
 * \param name name of flag to find.
 * \param type mask of desired flag object types.
 * \return pointer to flag structure, or NULL.
 */
static FLAG *
flag_hash_lookup(FLAGSPACE * n, const char *name, int type)
{
  FLAG *f;

  f = match_flag_ns(n, name);
  if (f && !(f->perms & F_DISABLED)) {
    if (f->type & type)
      return f;
    return NULL;
  }

  /* If the name is a single character, search the flag characters */
  if (name && *name && !*(name + 1)) {
    if ((f = letter_to_flagptr(n, *name, type)))
      return f;
  }

  /* provided for backwards compatibility: type flag checking */
  if (n->flag_table == flag_table) {
    for (f = type_table; f->name != NULL; f++)
      if (string_prefix(name, f->name))
        return f;
  }

  return NULL;
}

/* Allocate a new FLAG definition */
static FLAG *
new_flag(void)
{
  FLAG *f = (FLAG *) mush_malloc(sizeof(FLAG), "flag");
  if (!f)
    mush_panic("Unable to allocate memory for a new flag!\n");
  return f;
}

/* Deallocate all flag-related memory */
static void
clear_all_flags(FLAGSPACE * n)
{
  ptab_free(n->tab);
  /* Finally, the flags array */
  if (n->flags)
    free(n->flags);
  n->flags = NULL;
  n->flagbits = 0;
}

static FLAG *
clone_flag(FLAG *f)
{
  FLAG *clone = new_flag();
  clone->name = mush_strdup(f->name, "flag name");
  clone->letter = f->letter;
  clone->type = f->type;
  clone->bitpos = f->bitpos;
  clone->perms = f->perms;
  clone->negate_perms = f->negate_perms;
  return clone;
}

/* This is a stub function to add a flag. It performs no error-checking,
 * so it's up to you to be sure you're adding a flag that's properly
 * set up and that'll work ok. If called with autopos == 0, this
 * auto-allocates the next bitpos. Otherwise, bitpos is ignored and
 * f->bitpos is used.
 */
static void
flag_add(FLAGSPACE * n, const char *name, FLAG *f)
{
  /* If this flag has no bitpos assigned, assign it the next one.
   * We could improve this algorithm to use the next available
   * slot after deletions, too, but this will do for now.
   */
  if (f->bitpos < 0)
    f->bitpos = n->flagbits;

  /* Insert the flag in the ptab by the given name (maybe an alias) */
  ptab_start_inserts(n->tab);
  ptab_insert(n->tab, name, f);
  ptab_end_inserts(n->tab);

  /* Is this a canonical flag (as opposed to an alias?)
   * If it's an alias, we're done.
   * A canonical flag has either been given a new bitpos
   * or has not yet been stored in the flags array.
   * (An alias would have a previously used bitpos that's already 
   * indexing a flag in the flags array)
   */
  if ((f->bitpos >= n->flagbits) || (n->flags[f->bitpos] == NULL)) {
    /* It's a canonical flag */
    int i;
    if (f->bitpos >= n->flagbits) {
      /* Oops, we need a bigger array */
      if (n->flagbits == 0)
        n->flags = (FLAG **) malloc(sizeof(FLAG *));
      else {
        n->flags =
          (FLAG **) realloc(n->flags, (f->bitpos + 1) * sizeof(FLAG *));
        if (!n->flags)
          mush_panic("Unable to reallocate flags array!\n");
      }
      /* Make sure the new space is full of NULLs */
      for (i = n->flagbits; i <= f->bitpos; i++)
        n->flags[i] = NULL;
    }
    /* Put the canonical flag in the flags array */
    n->flags[f->bitpos] = f;
    n->flagbits = f->bitpos + 1;
    if (n->flagbits % 8 == 1) {
      /* We've crossed over a byte boundary, so we need to realloc
       * all the flags on all our objects to get them an additional
       * byte.
       */
      realloc_object_flag_bitmasks((n->flagbits + 7) / 8);
    }
  }
}

/* Eventually, this need to compute numbytes itself from the
 * total bytes required for flags + powers
 */
static void
realloc_object_flag_bitmasks(int numbytes)
{
  dbref it;
  object_flag_type p;

  for (it = 0; it < db_top; it++) {
    Flags(it) = (object_flag_type) realloc(Flags(it), numbytes);
    /* Zero them out */
    p = Flags(it) + numbytes - 1;
    memset(p, 0, 1);
  }
}


/* Read in a flag from a file and return it */
static FLAG *
flag_read_oldstyle(FILE * in)
{
  FLAG *f;
  char *c;
  c = mush_strdup(getstring_noalloc(in), "flag name");
  if (!strcmp(c, "FLAG ALIASES")) {
    mush_free(c, "flag name");
    return NULL;                /* We're done */
  }
  f = new_flag();
  f->name = c;
  c = (char *) getstring_noalloc(in);
  f->letter = *c;
  f->bitpos = -1;
  f->type = getref(in);
  f->perms = getref(in);
  f->negate_perms = getref(in);
  return f;
}

static FLAG *
flag_alias_read_oldstyle(FILE * in, char *alias)
{
  FLAG *f;
  char *c;
  /* Real name first */
  c = mush_strdup(getstring_noalloc(in), "flag alias");
  if (!strcmp(c, "END OF FLAGS")) {
    mush_free(c, "flag alias");
    return NULL;                /* We're done */
  }
  f = match_flag(c);
  if (!f) {
    /* Corrupt db. Recover as well as we can. */
    do_rawlog(LT_ERR,
              T
              ("FLAG READ: flag alias %s matches no known flag. Skipping aliases."),
              c);
    mush_free(c, "flag alias");
    do {
      c = (char *) getstring_noalloc(in);
    } while (strcmp(c, "END OF FLAGS"));
    return NULL;
  } else
    mush_free(c, "flag alias");

  /* Get the alias name */
  strcpy(alias, getstring_noalloc(in));
  return f;
}

/** Read flags and aliases from the database. This function expects
 * to receive file pointer that's already reading in a database file
 * and pointing at the start of the flag table. It reads the flags,
 * reads the aliases, and then does any additional flag adding that
 * needs to happen.
 * \param in file pointer to read from.
 * \param ns name of namespace to search.
 */
static void
flag_read_all_oldstyle(FILE * in, const char *ns)
{
  FLAG *f;
  FLAGSPACE *n;
  char alias[BUFFER_LEN];

  if (!(n = (FLAGSPACE *) hashfind(ns, &htab_flagspaces))) {
    do_rawlog(LT_ERR, T("FLAG READ: Unable to locate flagspace %s."), ns);
    return;
  }
  /* If we are reading flags from the db, they are definitive. */
  clear_all_flags(n);
  while ((f = flag_read_oldstyle(in))) {
    flag_add(n, f->name, f);
  }
  /* Assumes we'll always have at least one alias */
  while ((f = flag_alias_read_oldstyle(in, alias))) {
    flag_add(n, alias, f);
  }
  flag_add_additional();
}

/* Read in a flag from a file and return it */
static FLAG *
flag_read(FILE * in)
{
  FLAG *f;
  char *c;
  char *tmp;

  db_read_this_labeled_string(in, "name", &tmp);
  c = mush_strdup(tmp, "flag name");
  f = new_flag();
  f->name = c;
  db_read_this_labeled_string(in, "letter", &tmp);
  f->letter = *tmp;
  f->bitpos = -1;
  db_read_this_labeled_string(in, "type", &tmp);
  if(!strcmp(tmp, "ANY"))
    f->type = ALLTYPES;
  else {
    f->type = string_to_privs(type_privs, tmp, 0);
    /* Check to See if we're converting from Penn.. if so, change their Any to include our Any */
    if(!IS_COBRA_DB(flagdb_flags) && f->type == ALL_PENNMUSH_TYPES)
      f->type = ALLTYPES;
  }
  db_read_this_labeled_string(in, "perms", &tmp);
  f->perms = string_to_privs(flag_privs, tmp, 0);
  db_read_this_labeled_string(in, "negate_perms", &tmp);
  f->negate_perms = string_to_privs(flag_privs, tmp, 0);
  return f;
}

static FLAG *
flag_alias_read(FILE * in, char *alias, FLAGSPACE * n)
{
  FLAG *f;
  char *c;
  char *tmp;
  /* Real name first */
  db_read_this_labeled_string(in, "name", &tmp);
  c = mush_strdup(tmp, "flag alias");
  f = match_flag_ns(n, c);
  if (!f) {
    /* Corrupt db. Recover as well as we can. */
    do_rawlog(LT_ERR,
              T
              ("FLAG READ: flag alias %s matches no known flag. Skipping this alias."),
              c);
    mush_free(c, "flag alias");
    (void) getstring_noalloc(in);
    return NULL;
  } else
    mush_free(c, "flag alias");
 
  /* Get the alias name */
  db_read_this_labeled_string(in, "alias", &tmp);
  strcpy(alias, tmp);
  return f;
}
 
/** Read flags and aliases from the database. This function expects
 * to receive file pointer that's already reading in a database file
 * and pointing at the start of the flag table. It reads the flags,
 * reads the aliases, and then does any additional flag adding that
 * needs to happen.
 * \param in file pointer to read from.
 * \param ns name of namespace to search.
 */
void
flag_read_all(FILE * in, const char *ns)
{
  FLAG *f;
  FLAGSPACE *n;
  char alias[BUFFER_LEN];
  int count;
 
  if (!(flagdb_flags & DBF_LABELS)) {
    flag_read_all_oldstyle(in, ns);
    return;
  }

  if(ns) {
    if (!(n = (FLAGSPACE *) hashfind(ns, &htab_flagspaces))) {
      do_rawlog(LT_ERR, T("FLAG READ: Unable to locate flagspace %s."), ns);
      return;
    }
  } else {
    do_rawlog(LT_ERR, T("FLAG READ: Fake Powers List loading.") );
    flag_fake_read(in);
    return;
  }

  /* If we are reading flags from the db, they are definitive. */
  clear_all_flags(n);
  db_read_this_labeled_number(in, "flagcount", &count);
  for (; count > 0; count--) {
    if ((f = flag_read(in))) 
     flag_add(n, f->name, f);
  }
 /* Assumes we'll always have at least one alias */
  db_read_this_labeled_number(in, "flagaliascount", &count);
  for (; count > 0; count--) {
    if ((f = flag_alias_read(in, alias, n)))
      flag_add(n, alias, f);
  }
  flag_add_additional();
}

static void flag_fake_read(FILE *f) {
  int count;

  /* Read shit into NULL-ville */
  db_read_this_labeled_number(f, "flagcount", &count);

  for(; count > 0; count--) {
    getstring_noalloc(f);
    getstring_noalloc(f);
    getstring_noalloc(f);
    getstring_noalloc(f);
    getstring_noalloc(f);
  }

  db_read_this_labeled_number(f, "flagaliascount", &count);
  for(; count > 0; count--) {
    getstring_noalloc(f);
    getstring_noalloc(f);
  }
}

/* Write a flag out to a file */
static void
flag_write(FILE * out, FLAG *f, const char *name)
{
  db_write_labeled_string(out, " name", name);
 db_write_labeled_string(out, "  letter", tprintf("%c", f->letter));
 db_write_labeled_string(out, "  type", f->type == ALLTYPES ? "ANY"  :privs_to_string(type_privs, f->type));
 db_write_labeled_string(out, "  perms",
                        privs_to_string(flag_privs, f->perms));
 db_write_labeled_string(out, "  negate_perms",
                        privs_to_string(flag_privs, f->negate_perms));
}


/* Write a flag alias out to a file */
static void
flag_alias_write(FILE * out, FLAG *f, const char *name)
{
  db_write_labeled_string(out, " name", f->name);
  db_write_labeled_string(out, "  alias", name);
}

/** Write flags and aliases to the database. This function expects
 * to receive file pointer that's already writing in a database file.
 * It writes the flags, writes the aliases.
 * \param out file pointer to write to.
 */
void
flag_write_all(FILE * out, const char *ns)
{
  int i, count;
  FLAG *f;
  FLAGSPACE *n;
  char flagname[BUFFER_LEN];

  if (!(n = (FLAGSPACE *) hashfind(ns, &htab_flagspaces))) {
    do_rawlog(LT_ERR, T("FLAG WRITE: Unable to locate flagspace %s."), ns);
    return;
  }
  /* Write out canonical flags first */
  count = 0;
  for (i = 0; i < n->flagbits; i++) {
    if (n->flags[i])
      count++;
  }
  db_write_labeled_number(out, "flagcount", count);
  for (i = 0; i < n->flagbits; i++) {
    if (n->flags[i])
      flag_write(out, n->flags[i], n->flags[i]->name);
  }
  /* Now write out aliases. An alias is a flag in the ptab whose
   * name isn't the same as the name of the canonical flag in its
   * bit position
   */
  count = 0;
  f = ptab_firstentry_new(n->tab, flagname);
  while (f) {
    if (strcmp(n->flags[f->bitpos]->name, flagname))
      count++;
    f = ptab_nextentry_new(n->tab, flagname);
  }
  db_write_labeled_number(out, "flagaliascount", count);
  f = ptab_firstentry_new(n->tab, flagname);
  while (f) {
    if (strcmp(n->flags[f->bitpos]->name, flagname)) {
      /* This is an alias! */
      flag_alias_write(out, f, flagname);
    }
    f = ptab_nextentry_new(n->tab, flagname);
  }
}

/** Initialize the flagspaces.
 */
void
init_flagspaces(void)
{
  FLAGSPACE *flags;

  hashinit(&htab_flagspaces, 4, sizeof(FLAGSPACE));
  flags = (FLAGSPACE *) mush_malloc(sizeof(FLAGSPACE), "flagspace");
  flags->tab = &ptab_flag;
  flags->flagbits = 0;
  flags->flags = NULL;
  flags->flag_table = flag_table;
  flags->flag_alias_table = flag_alias_tab;
  hashadd("FLAG", (void *) flags, &htab_flagspaces);
}


/** Initialize a flag table with defaults.
 * This function loads the standard flags as a baseline 
 * (and for dbs that haven't yet converted).
 * \param ns name of flagspace to initialize.
 */
void
init_flag_table(const char *ns)
{
  FLAG *f, *cf;
  FLAG_ALIAS *a;
  FLAGSPACE *n;

  if (!(n = (FLAGSPACE *) hashfind(ns, &htab_flagspaces))) {
    do_rawlog(LT_ERR, T("FLAG INIT: Unable to locate flagspace %s."), ns);
    return;
  }

  do_rawlog(LT_ERR, T("Initializing flag table."));

  ptab_init(n->tab);
  /* do regular flags first */
  for (f = n->flag_table; f->name; f++) {
    cf = clone_flag(f);
    cf->bitpos = -1;
    flag_add(n, cf->name, cf);
  }
  /* now add in the aliases */
  for (a = n->flag_alias_table; a->alias; a++) {
    if ((f = match_flag_ns(n, a->realname)))
      flag_add(n, a->alias, f);
    else
      do_rawlog(LT_ERR,
                T("FLAG INIT: flag alias %s matches no known flag."), a->alias);
  }
  flag_add_additional();

  do_rawlog(LT_ERR, T("Flag table initialization complete."));
}

/* This is where the developers will put flag_add statements to create
 * new flags in future penn versions. Hackers should avoid this,
 * and use local_flags() in flaglocal.c instead.
 */
static void
flag_add_additional(void)
{
  FLAG *f;
#ifdef RPMODE_SYS
  add_flag("ICFUNCS", '\0', TYPE_ROOM, F_PRIVILEGE, F_PRIVILEGE);
#endif
  add_flag("EMPIRE", 'E', TYPE_DIVISION, F_PRIVILEGE, F_PRIVILEGE);
  add_flag("INHERIT", 'I', TYPE_THING | TYPE_EXIT | TYPE_ROOM, F_PRIVILEGE, F_PRIVILEGE);
  add_flag("INHERITABLE", 'I', TYPE_PLAYER, F_PRIVILEGE | F_SELF,
           F_PRIVILEGE | F_SELF);
  add_flag("ZCLONE_OK", '\0', TYPE_THING, F_PRIVILEGE, F_PRIVILEGE);
  add_flag("WEIRDSITE", '\0', TYPE_PLAYER, F_GOD | F_DARK, F_GOD | F_DARK);
  add_flag("MISTRUST", 'm', TYPE_THING | TYPE_EXIT | TYPE_ROOM, F_PRIVILEGE,
           F_PRIVILEGE);
  add_flag("ORPHAN", 'i', NOTYPE, F_ANY, F_ANY);
  add_flag("TRACK_MONEY", '\0', TYPE_PLAYER, F_ANY, F_ANY);
  add_flag("LOUD", '\0', NOTYPE, F_PRIVILEGE, F_ANY);
  add_flag("INPROGRAM", '\0', TYPE_PLAYER, F_INTERNAL, F_INTERNAL);
  add_flag("BUILDER", '\0', TYPE_PLAYER, F_PRIVILEGE, F_PRIVILEGE);
  add_flag("AUTH_PARENT", '\0', NOTYPE, F_PRIVILEGE, F_PRIVILEGE);
  add_flag("MUTE", '\0', TYPE_PLAYER, F_PRIVILEGE, F_PRIVILEGE);
  add_flag("LEAVE_BEHIND", '\0', TYPE_PLAYER | TYPE_THING, F_ANY, F_ANY);
#ifdef RPMODE_SYS
  add_flag("RPMODE", '\0', TYPE_PLAYER, F_PRIVILEGE, F_PRIVILEGE);
  add_flag("BLIND", '\0', TYPE_PLAYER, F_PRIVILEGE, F_PRIVILEGE);
  add_flag("PARALYZED", '\0', TYPE_PLAYER, F_PRIVILEGE, F_PRIVILEGE);
  add_flag("RPAPPROVED", '\0', TYPE_THING, F_PRIVILEGE, F_PRIVILEGE);
#endif
  if ((f = match_flag("TERSE")))
    f->type |= TYPE_THING;
  if ((f = match_flag("PUPPET")))
    f->type |= TYPE_ROOM | TYPE_DIVISION;
  if ((f = match_flag("NOSPOOF"))) {
    f->type = NOTYPE;
    f->letter = '"';
  }
  if ((f = match_flag("PARANOID"))) {
    f->type = NOTYPE;
    f->letter = '\0';
  }
  f = add_flag("CHAN_USEFIRSTMATCH", '\0', NOTYPE, F_PRIVILEGE, F_PRIVILEGE);
  if (!match_flag("CHAN_FIRSTMATCH"))
    flag_add(hashfind("FLAG", &htab_flagspaces), "CHAN_FIRSTMATCH", f);
  if (!match_flag("CHAN_MATCHFIRST"))
    flag_add(hashfind("FLAG", &htab_flagspaces), "CHAN_MATCHFIRST", f);

  local_flags();
}

/** Extract object type from old-style flag value.
 * Before 1.7.7p5, object types were stored in the lowest 3 bits of the
 * flag value. Now they get their own place in the object structure,
 * but if we're reading an older database, we need to extract the types
 * from the old flag value.
 * \param old_flags an old-style flag bitmask.
 * \return a type bitflag.
 */
int
type_from_old_flags(long old_flags)
{
  switch (old_flags & OLD_TYPE_MASK) {
  case OLD_TYPE_PLAYER:
    return TYPE_PLAYER;
  case OLD_TYPE_ROOM:
    return TYPE_ROOM;
  case OLD_TYPE_EXIT:
    return TYPE_EXIT;
  case OLD_TYPE_THING:
    return TYPE_THING;
  case OLD_TYPE_GARBAGE:
    return TYPE_GARBAGE;
  case OLD_TYPE_DIVISION:
    return TYPE_DIVISION;
  }
  /* If we get here, we're in trouble. */
  return -1;
}

/** Extract flags from old-style flag and toggle values.
 * This function takes the flag and toggle bitfields from older databases,
 * allocates a new flag bitmask, and populates it appropriately
 * by looking up each flag/toggle value in the old flag table.
 * \param old_flags an old-style flag bitmask.
 * \param old_toggles an old-style toggle bitmask.
 * \param type the object type.
 * \return a newly allocated flag bitmask representing the flags and toggles.
 */
object_flag_type
flags_from_old_flags(long old_flags, long old_toggles, int type)
{
  FLAG *f, *newf;
  object_flag_type bitmask = new_flag_bitmask("FLAG");

  for (f = flag_table; f->name; f++) {
    if (f->type == NOTYPE) {
      if (f->bitpos & old_flags) {
        newf = match_flag(f->name);
        set_flag_bitmask(bitmask, newf->bitpos);
      }
    } else if (f->type & type) {
      if (f->bitpos & old_toggles) {
        newf = match_flag(f->name);
        set_flag_bitmask(bitmask, newf->bitpos);
      }
    }
  }
  for (f = hack_table; f->name; f++) {
    if ((f->type & type) && (f->bitpos & old_toggles)) {
      newf = match_flag(f->name);
      set_flag_bitmask(bitmask, newf->bitpos);
    }
  }
  return bitmask;
}

/** Macro to detrmine if flag f's name is n */
#define is_flag(f,n)    (!strcmp(f->name,n))

/* Given a single character, return the matching flag definition */
static FLAG *
letter_to_flagptr(FLAGSPACE * n, char c, int type)
{
  FLAG *f;
  int i;
  for (i = 0; i < n->flagbits; i++)
    if ((f = n->flags[i])) {
      /* Doh! Kludge-city. We'll ignore the CHOWN_OK flag on players, because
       * it's more useful to check 'C' as COLOR. Argle.
       */
      if (!(is_flag(f, "CHOWN_OK") && (type == TYPE_PLAYER)) &&
          ((f->letter == c) && (f->type & type)))
        return f;
    }
  /* Do we need to do this? */
  return NULL;
}


/*----------------------------------------------------------------------
 * Functions for managing bitmasks
 */

/** Locate a specific byte given a bit position */
#define FlagByte(x) (x / 8)
/** Locate a specific bit within a byte given a bit position */
#define FlagBit(x) (7 - (x % 8))
/** How many bytes do we need for a flag bitmask? */
#define FlagBytes(n)  ((size_t)((n->flagbits + 7) / 8))


/** Allocate a new flag bitmask.
 * This function allocates a new flag bitmask of sufficient length
 * to include the current number of flags. It zeroes out the entire
 * bitmask and returns it.
 * \return a newly allocated zeroed flag bitmask.
 */
object_flag_type
new_flag_bitmask(const char *ns)
{
  object_flag_type bitmask;
  FLAGSPACE *n;
  Flagspace_Lookup(n, ns);
  bitmask = (object_flag_type) mush_malloc(FlagBytes(n), "flag_bitmask");
  if (!bitmask)
    mush_panic("Unable to allocate memory for flag bitmask");
  memset(bitmask, 0, FlagBytes(n));
  return bitmask;
}

/** Copy flag bitmask, allocating a new bitmask for the copy.
 * This function allocates a new flag bitmask of sufficient length
 * to include the current number of flags, and copies all the flags
 * from a given bitmask into the new bitmask.
 * \param ns name of namespace to search.
 * \param given a flag bitmask.
 * \return a newly allocated clone of the given bitmask.
 */
object_flag_type
clone_flag_bitmask(const char *ns, object_flag_type given)
{
  object_flag_type bitmask;
  FLAGSPACE *n;
  Flagspace_Lookup(n, ns);
  bitmask = (object_flag_type) mush_malloc(FlagBytes(n), "flag_bitmask");
  if (!bitmask)
    mush_panic("Unable to allocate memory for flag bitmask");
  memcpy(bitmask, given, FlagBytes(n));
  return bitmask;
}

/** Copy one flag bitmask into another (already allocated).
 * This is a convenience function - it's memcpy for flag bitmasks.
 * \param ns name of namespace to search.
 * \param dest destination bitmask for the given data.
 * \param given a flag bitmask to copy.
 */
/* Copy a given bitmask to an already-allocated destination bitmask */
void
copy_flag_bitmask(const char *ns, object_flag_type dest, object_flag_type given)
{
  FLAGSPACE *n;
  Flagspace_Lookup(n, ns);
  memcpy((void *) dest, (void *) given, FlagBytes(n));
}

/** Deallocate a flag bitmask.
 * \param bitmask a flag bitmask to free.
 */
void
destroy_flag_bitmask(object_flag_type bitmask)
{
  mush_free((Malloc_t) bitmask, "flag_bitmask");
}

/** Add a bit into a bitmask.
 * This function sets a particular bit in a bitmask (e.g. bit 42), 
 * by computing the appropriate byte, and the appropriate bit within the byte,
 * and setting it.
 * \param bitmask a flag bitmask.
 * \param bit the bit to set.
 */
void
set_flag_bitmask(object_flag_type bitmask, int bit)
{
  int bytepos = FlagByte(bit);
  int bitpos = FlagBit(bit);
  if (!bitmask)
    return;
  *(bitmask + bytepos) |= (1 << bitpos);
}

/** Add a bit into a bitmask.
 * This function clears a particular bit in a bitmask (e.g. bit 42), 
 * by computing the appropriate byte, and the appropriate bit within the byte,
 * and clearing it.
 * \param bitmask a flag bitmask.
 * \param bit the bit to clear.
 */
void
clear_flag_bitmask(object_flag_type bitmask, int bit)
{
  int bytepos = FlagByte(bit);
  int bitpos = FlagBit(bit);
  if (!bitmask)
    return;
  *(bitmask + bytepos) &= ~(1 << bitpos);
}

/** Test a bit in a bitmask.
 * This function tests a particular bit in a bitmask (e.g. bit 42), 
 * by computing the appropriate byte, and the appropriate bit within the byte,
 * and testing it.
 * \param bitmask a flag bitmask.
 * \param bitpos the bit to test.
 * \retval 1 bit is set.
 * \retval 0 bit is not set.
 */
int
has_bit(object_flag_type bitmask, int bitpos)
{
  int bytepos, bits_in_byte;
  /* Garbage objects, for example, have no bits set */
  if (!bitmask)
    return 0;
  bytepos = FlagByte(bitpos);
  bits_in_byte = FlagBit(bitpos);
  return *(bitmask + bytepos) & (1 << bits_in_byte);
}

/** Test a set of bits in one bitmask against all those in another.
 * This function determines if one bitmask contains (at least)
 * all of the bits set in another bitmask.
 * \param ns name of namespace to search.
 * \param source the bitmask to test.
 * \param bitmask the bitmask containing the bits to look for.
 * \retval 1 all bits in bitmask are set in source.
 * \retval 0 at least one bit in bitmask is not set in source.
 */
int
has_all_bits(const char *ns, object_flag_type source, object_flag_type bitmask)
{
  unsigned int i;
  int ok = 1;
  FLAGSPACE *n;
  Flagspace_Lookup(n, ns);
  for (i = 0; i < FlagBytes(n); i++)
    ok &= ((*(bitmask + i) & *(source + i)) == *(bitmask + i));
  return ok;
}

/** Test to see if a bitmask is entirely 0 bits.
 * \param ns name of namespace to search.
 * \param source the bitmask to test.
 * \retval 1 all bits in bitmask are 0.
 * \retval 0 at least one bit in bitmask is 1.
 */
int
null_flagmask(const char *ns, object_flag_type source)
{
  unsigned int i;
  int bad = 0;
  FLAGSPACE *n;
  Flagspace_Lookup(n, ns);
  for (i = 0; i < FlagBytes(n); i++)
    bad |= *(source + i);
  return (!bad);
}

/** Test a set of bits in one bitmask against any of those in another.
 * This function determines if one bitmask contains any
 * of the bits set in another bitmask.
 * \param ns name of namespace to search.
 * \param source the bitmask to test.
 * \param bitmask the bitmask containing the bits to look for.
 * \retval 1 at least one bit in bitmask is set in source.
 * \retval 0 no bits in bitmask are set in source.
 */
int
has_any_bits(const char *ns, object_flag_type source, object_flag_type bitmask)
{
  unsigned int i;
  int ok = 0;
  FLAGSPACE *n;
  Flagspace_Lookup(n, ns);
  for (i = 0; i < FlagBytes(n); i++)
    ok |= (*(bitmask + i) & *(source + i));
  return ok;
}

/** Produce a space-separated list of flag names, given a bitmask.
 * This function returns the string representation of a flag bitmask.
 * \param ns name of namespace to search.
 * \param bitmask a flag bitmask.
 * \param privs dbref for privilege checking for flag visibility.
 * \param thing object for which bitmask is the flag bitmask.
 * \return string representation of bitmask (list of flags).
 */
const char *
bits_to_string(const char *ns, object_flag_type bitmask, dbref privs,
               dbref thing)
{
  FLAG *f;
  FLAGSPACE *n;
  int i;
  int first = 1;
  static char buf[BUFFER_LEN];
  char *bp;

  Flagspace_Lookup(n, ns);
  bp = buf;
  for (i = 0; i < n->flagbits; i++) {
    if ((f = n->flags[i])) {
      if (has_bit(bitmask, f->bitpos) &&
          (!GoodObject(thing) || Can_See_Flag(privs, thing, f))) {
        if (!first)
          safe_chr(' ', buf, &bp);
        safe_str(f->name, buf, &bp);
        first = 0;
      }
    }
  }
  *bp = '\0';
  return buf;
}

/** Convert a flag list string to a bitmask.
 * Given a space-separated list of flag names, convert them to
 * a bitmask array (which we malloc) and return it.
 * \param ns name of namespace to search.
 * \param str list of flag names.
 * \return a newly allocated flag bitmask.
 */
object_flag_type
string_to_bits(const char *ns, const char *str)
{
  object_flag_type bitmask;
  char *copy, *s, *sp;
  FLAG *f;
  FLAGSPACE *n;

  bitmask = new_flag_bitmask(ns);
  if (!(n = (FLAGSPACE *) hashfind(ns, &htab_flagspaces))) {
    do_rawlog(LT_ERR, T("FLAG: Unable to locate flagspace %s."), ns);
    return bitmask;
  }
  if (!str)
    return bitmask;             /* We're done, then */
  copy = mush_strdup(str, "flagstring");
  s = trim_space_sep(copy, ' ');
  while (s) {
    sp = split_token(&s, ' ');
    if (!(f = match_flag_ns(n, sp)))
      /* Now what do we do? Ignore it? */
      continue;
    set_flag_bitmask(bitmask, f->bitpos);
  }
  mush_free(copy, "flagstring");
  return bitmask;
}


/*----------------------------------------------------------------------
 * Functions for working with flags on objects
 */


/** Check an object for a flag.
 * This function tests to see if an object has a flag. It is the
 * function to use for this purpose from outside of this file.
 * \param thing object to check.
 * \param flag name of flag to check for (a string).
 * \param type allowed types of flags to check for.
 * \retval 1 object has the flag.
 * \retval 0 object does not have the flag.
 */
int
has_flag_by_name(dbref thing, const char *flag, int type)
{
  FLAG *f;
  FLAGSPACE *n;
  n = hashfind("FLAG", &htab_flagspaces);
  f = flag_hash_lookup(n, flag, type);
  if (!f)
    return 0;
  return has_flag(thing, f);
}

static int
has_flag(dbref thing, FLAG *f)
{
  if (!GoodObject(thing) || IsGarbage(thing))
    return 0;
  return has_bit(Flags(thing), f->bitpos);
}

static int
can_set_flag(dbref player, dbref thing, FLAG *flagp, int negate)
{
  /* returns 1 if player can set a flag on thing. */
  int myperms;
  if (!flagp || !GoodObject(player) || !GoodObject(thing))
    return 0;
  myperms = negate ? flagp->negate_perms : flagp->perms;
  if ((myperms & F_INTERNAL) || (myperms & F_DISABLED))
    return 0;
  if (!(flagp->type & Typeof(thing)))
    return 0;
  /* K.. we've passed internal and type checks.. if we're god we can do anything */
  if(God(player))
    return 1;
  /* You've got to *own* something (or be Director) to set it
   * chown_ok or dest_ok. This prevents subversion of the
   * zone-restriction on @chown and @dest
   */
  if (is_flag(flagp, "CHOWN_OK") || is_flag(flagp, "DESTROY_OK")) {
    if (!Owns(player, thing) && !Director(player))
      return 0;
    else
      return 1;
  }

  /* You must be privileged to set/clear the MONITOR flag on a player */
  if (IsPlayer(thing) && is_flag(flagp, "MONITOR") && !Admin(player))
    return 0;
  /* Checking for the ZONE flag. If you set this, the player had
   * better be zone-locked! 
   */
  if (!negate && is_flag(flagp, "ZONE") &&
      (getlock(thing, Zone_Lock) == TRUE_BOOLEXP)) {
    notify(player, T("You must @lock/zone before you can set a player ZONE"));
    return 0;
  }
  if ((myperms & F_SELF) && (player == thing))
    return 1;
  /* Privilege, permissions work if a) they hav priv power & b) they control the thing */
  if ((myperms & F_PRIVILEGE) && !(div_powover(player, player, "Privilege") && controls(player,thing)))
    return 0;
  if ((myperms & F_GOD) && !God(player))
    return 0;
  if (Director(thing) && is_flag(flagp, "GAGGED"))
    return 0;                   /* can't gag directors/God */
  /* Check if its wiz or roy flag.. Make sure they're in a division before they can do 'em */
  if ((is_flag(flagp, "WIZARD") || is_flag(flagp, "ROYALTY")) 
      && (SDIV(thing).object == NOTHING))
    return 0;
  if (God(player))              /* God can do (almost) anything) */
    return 1;
  /* Make sure we don't accidentally permission-check toggles when
   * checking priv bits.
   */
  /* A wiz can set things he owns WIZ, but nothing else. */
  if (is_flag(flagp, "WIZARD") && !negate)
    return (has_flag_by_name(player, "WIZARD", NOTYPE) && Owns(player, thing) && !IsPlayer(thing));
  /* A wiz can unset the WIZ bit on any non-player */
  if (is_flag(flagp, "WIZARD") && negate)
    return (has_flag_by_name(player, "WIZARD", NOTYPE) && !IsPlayer(thing));
  /* Wizards can set or unset anything royalty. Royalty can set anything
   * they own royalty, but cannot reset their own bits. */
  if (is_flag(flagp, "ROYALTY")) {
    return (!Guest(thing) && (has_flag_by_name(player, "WIZARD", NOTYPE) || (has_flag_by_name(player, "ROYALTY", NOTYPE) &&
                                                 Owns(player, thing)
                                                 && !IsPlayer(thing))));
  }
  return 1;
}

/** Return a list of flag symbols that one object can see on another.
 * \param thing object to list flag symbols for.
 * \param player looker, for permission checking.
 * \return a string containing all the visible type and flag symbols.
 */
const char *
unparse_flags(dbref thing, dbref player)
{
  /* print out the flag symbols (letters) */
  static char buf[BUFFER_LEN];
  char *p;
  FLAG *f;
  int i;
  FLAGSPACE *n;

  Flagspace_Lookup(n, "FLAG");
  p = buf;
  switch (Typeof(thing)) {
  case TYPE_GARBAGE:
    *p = '\0';
    return buf;
  case TYPE_ROOM:
    *p++ = 'R';
    break;
  case TYPE_EXIT:
    *p++ = 'E';
    break;
  case TYPE_THING:
    *p++ = 'T';
    break;
  case TYPE_PLAYER:
    *p++ = 'P';
    break;
  case TYPE_DIVISION:
    *p++ = 'D';
    break;
  }
  for (i = 0; i < n->flagbits; i++) {
    if ((f = n->flags[i])) {
      if (has_flag(thing, f) && Can_See_Flag(player, thing, f))
        *p++ = f->letter;
    }
  }
  *p = '\0';
  return buf;
}

/** Return the object's type and its flag list for examine.
 * \param thing object to list flag symbols for.
 * \param player looker, for permission checking.
 * \return a string containing all the visible type and flag symbols.
 */
const char *
flag_description(dbref player, dbref thing)
{
  static char buf[BUFFER_LEN];
  char *bp;
  bp = buf;
  safe_str(T("Type: "), buf, &bp);
  safe_str(privs_to_string(type_privs, Typeof(thing)), buf, &bp);
  safe_str(T(" Flags: "), buf, &bp);
  safe_str(bits_to_string("FLAG", Flags(thing), player, thing), buf, &bp);
  *bp = '\0';
  return buf;
}



/** Print out the flags for a decompile.
 * \param player looker, for permission checking.
 * \param thing object being decompiled.
 * \param name name by which object is referred to in the decompile.
 * \param ns name of namespace to search.
 * \param command name of command used to set the 'flag'.
 */
void
decompile_flags(dbref player, dbref thing, const char *name)
{
  FLAG *f;
  int i;
  FLAGSPACE *n;
  Flagspace_Lookup(n, "FLAG");
  for (i = 0; i < n->flagbits; i++)
    if ((f = n->flags[i])) {
      if (has_flag(thing, f) && Can_See_Flag(player, thing, f))
        notify_format(player, "@set %s = %s", name, f->name);
    }
}


/** Set or clear flags on an object, without permissions/hear checking.
 * This function is for server internal use, only, when a flag should
 * be set or cleared unequivocally.
 * \param ns name of namespace to search.
 * \param thing object on which to set or clear flag.
 * \param flag name of flag to set or clear.
 * \param negate if 1, clear the flag, if 0, set the flag.
 */
void
twiddle_flag_internal(const char *ns, dbref thing, const char *flag, int negate)
{
  FLAG *f;
  FLAGSPACE *n;
  if (IsGarbage(thing))
    return;
  n = (FLAGSPACE *) hashfind(ns, &htab_flagspaces);
  if ((f = flag_hash_lookup(n, flag, Typeof(thing)))) {
    if (n->flag_table == flag_table)
      twiddle_flag(thing, f, negate);
  }
}


/** Set or clear flags on an object, with full permissions/hear checking.
 * \verbatim
 * This function is used to set and clear flags through @set and the like.
 * It does permission checking and handles the "is now listening" messages.
 * \endverbatim
 * \param player the enactor.
 * \param thing object on which to set or clear flag.
 * \param flag name of flag to set or clear.
 * \param negate if 1, clear the flag, if 0, set the flag.
 * \param hear 1 if object is a hearer.
 * \param listener 1 if object is a listener.
 */
void
set_flag(dbref player, dbref thing, const char *flag, int negate,
         int hear, int listener)
{
  FLAG *f;
  char tbuf1[BUFFER_LEN];
  char *tp;
#ifdef RPMODE_SYS
  char  icloc_buf[BUFFER_LEN];
  char *tp2, *tp3;
  ATTR *a, *icloc_ptr;
  dbref icloc, absroom;
#endif /* RPMODE_SYS */
  FLAGSPACE *n;

  n = (FLAGSPACE *) hashfind("FLAG", &htab_flagspaces);
  if ((f = flag_hash_lookup(n, flag, Typeof(thing))) == NULL) {
    notify_format(player, T("%s - I don't recognize that flag."), flag);
    return;
  }

  if (!OOREF(player,can_set_flag(player, thing, f, negate), can_set_flag(ooref, thing, f, negate))) {
    notify(player, T("Permission denied."));
    return;
  }
  /* The only players who can be Dark are directors. */
  if (is_flag(f, "DARK") && !negate && Alive(thing) && !Admin(thing)) {
    notify(player, T("Permission denied."));
    return;
  }

  if(is_flag(f, "BUILDER") && !OOREF(player,div_powover(player,player, "BCreate"), 
        div_powover(ooref, ooref, "BCreate"))) {
    notify(player, T("Permission denied."));
    return;
  }
    
#ifdef RPMODE_SYS
  if(is_flag(f, "ICFUNCS") && IsRoom(thing)) {
    if(negate)
      rplog_shutdown_room(thing);
    else
      rplog_init_room(thing);
  }
  if(is_flag(f, "RPMODE")) {
       if(atr_get(thing, "RPLOCK") != NULL) {
         notify_format(player, "That player is currently locked %s RPMode.", RPMODE(thing) ? "into" : "out of");
         return;
       }
          if(negate) {
                  if(!RPMODE(thing)) {
                          notify(player, "That player isn't currently in RPMODE.");
                          return;
                  }
                  icloc = GoodObject((absroom = absolute_room(thing))) ? absroom : thing;
                  if(GoodObject(icloc) && has_flag_by_name(icloc, "ICFUNCS" , NOTYPE) 
                          && !Going(icloc)) {
                                  (void) atr_add(thing, "INF_RPLOC", unparse_dbref(Location(thing)), GOD, NOTHING); 
                                  icloc = Home(thing);
                          } else {
                                  notify(player, "That player is not an IC Location, therefore can't go OOC.");
                                  return;
                          }
          } else {
                  if(RPMODE(thing)) {
                          notify(player, "That player is already in RPMODE.");
                          return;
                  }
                  if(InProg(thing)) {
                    notify(player, "Permission denied. Can not go RPMODE while in a @PROGRAM.");
                    return;
                  }
                  /* First make sure there truerace is valid */
                  icloc_ptr = atr_get(thing, "TRUERACE");
                  if(icloc_ptr) 
                    strncpy(icloc_buf, atr_value(icloc_ptr), BUFFER_LEN-1);
                  if(!*icloc_buf) {
                          if(player != thing)
                                  notify(player, "That player does not have a valid TRUERACE set.");
                          notify(thing, "You do not have a valid TRUERACE set.  Contact an administrator.");
                          return;
                  } else if(parse_dbref(icloc_buf) == NOTHING) {
                          if(player != thing)
                                  notify(player, "That player has an invalid TRUERACE.");
                          notify(thing, "You have an invalid TRUERACE.  Contact an administrator.");
                          return;
                  }
                  icloc_ptr = atr_get(thing, "INF_RPLOC");
                  memset(icloc_buf, '\0', BUFFER_LEN);
                  if(icloc_ptr)
                          strncpy(icloc_buf, atr_value(icloc_ptr), BUFFER_LEN-1);
                  if(!*icloc_buf) {
                          if(player != thing)
                            notify(player, "That player has an invalid RP location set."); 
                          notify(thing, "You have an invalid RP location set.  Contact an administrator.");
                          return;
                  }
                  
                  icloc = parse_dbref(icloc_buf);
                  icloc = GoodObject((absroom = absolute_room(icloc))) ? absroom : icloc;

                  if(!GoodObject(icloc) ||  !has_flag_by_name(icloc, "ICFUNCS", NOTYPE) || Going(icloc)) {
                          if(player != thing)
                            notify(player, "That player has an invalid RP location set.");
                          notify(thing, "You have an invalid RP location set.  Contact an administrator.");
                          return;
                  }
                  /* Ok.. everythings good, move 'em to the IC world */
          }
  }
#endif /* RPMODE_SYS */

  twiddle_flag(thing, f, negate);

#ifdef RPMODE_SYS
  if(is_flag(f, "RPMODE")) {
        dbref oldloc;
        oldloc = Location(thing);
        enter_room(thing, icloc, 1);
        if(!negate) {
                a = atr_get( ANCESTOR_PLAYER, "AIC");
                if(a) {
                  strncpy(tbuf1, atr_value(a), BUFFER_LEN-1);
                  tbuf1[strlen(tbuf1)+1] = '\0';
                  tp = mush_strdup(unparse_dbref(thing), "rpsys.thing");
                  tp2 = mush_strdup(unparse_dbref(oldloc), "rpsys.oldloc");
                  tp3 = mush_strdup(unparse_dbref(icloc), "rpsys.icloc");
                  global_eval_context.wnxt[0] = tp;
                  global_eval_context.wnxt[1] = tp2;
                  global_eval_context.wnxt[2] = tp3;
                  global_eval_context.wnxt[3] = NULL;
                  parse_que(ANCESTOR_PLAYER, tbuf1, player);
                  if(tp)
                    mush_free(tp, "rpsys.thing");
                  if(tp2)
                    mush_free(tp2, "rpsys.oldloc");
                  if(tp3)
                    mush_free(tp3, "rpsys.icloc");
                }
                 notify_format(thing, "You wake up from a deep sleep.");
                if(oldloc != icloc)
                        notify_except(Contents(oldloc), NOTHING, tprintf("%s%sGAME%s%s:%s%s %s has entered RPMODE%s.", 
                                        ANSI_HILITE, ANSI_CYAN, ANSI_NORMAL, ANSI_RED, ANSI_HILITE, ANSI_YELLOW,
                                        Name(thing), ANSI_NORMAL), 0);
                if(Connected(thing))
                  notify_except(Contents(icloc), thing, tprintf("%s wakes up from a deep sleep.",
                                spname(thing)), NA_INTER_HEAR);
        } else  {
                a = atr_get( ANCESTOR_PLAYER, "AOOC");
                if(a) {
                  strncpy(tbuf1, atr_value(a), BUFFER_LEN-1);
                  tbuf1[strlen(tbuf1)+1] = '\0';
                  tp = mush_strdup(unparse_dbref(thing), "rpsys.thing");
                  tp2 = mush_strdup(unparse_dbref(oldloc), "rpsys.oldloc");
                  tp3 = mush_strdup(unparse_dbref(icloc), "rpsys.icloc");
                  global_eval_context.wnxt[0] = tp;
                  global_eval_context.wnxt[1] = tp2;
                  global_eval_context.wnxt[2] = tp3;
                  global_eval_context.wnxt[3] = NULL;
                  parse_que(ANCESTOR_PLAYER, tbuf1, player);
                  if(tp)
                    mush_free(tp, "rpsys.thing");
                  if(tp2)
                    mush_free(tp2, "rpsys.oldloc");
                  if(tp3)
                    mush_free(tp3, "rpsys.icloc");
                }

                notify_format(thing, "You fall into a deep sleep.");

                notify_except(Contents(icloc), thing, tprintf("%s%sGAME%s%s:%s%s %s has arrived from leaving RPMODE%s.",                                                       ANSI_HILITE, ANSI_CYAN, ANSI_NORMAL, ANSI_RED, ANSI_HILITE, ANSI_YELLOW,
                                        Name(thing), ANSI_NORMAL), 0);
                if(Connected(thing))
                  notify_anything(orator, na_loc, &Contents(oldloc), NULL, NA_INTER_HEAR,
                                tprintf("%s falls into a deep sleep.", spname(thing)));
        }
  }
#endif /* RPMODE_SYS */
  if (negate) {
    /* log if necessary */
    if (is_flag(f, "WIZARD")) {
      do_log(LT_WIZ, player, thing, "WIZFLAG RESET");
      /* Perform Marker Resets  & Check if Royalty flag is on them for special conditions */
      powergroup_db_set(player, thing, "!WIZARD", 1);
      if(has_flag_by_name(thing, "ROYALTY", NOTYPE))
        SLEVEL(thing) = 28;
      else
        SLEVEL(thing) = 3;
    } else if (is_flag(f, "ROYALTY")) {
      do_log(LT_WIZ, player, thing, "ROYAL FLAG RESET");
      /* Perform Marker Resets */
      powergroup_db_set(player, thing, "!ROYALTY", 1);
      if(!has_flag_by_name(thing, "WIZARD", NOTYPE))
        SLEVEL(thing) = 3;
    }
    if (is_flag(f, "SUSPECT"))
      do_log(LT_WIZ, player, thing, "SUSPECT FLAG RESET");
    /* those who unDARK return to the WHO */
    if (is_flag(f, "DARK") && IsPlayer(thing))
      hide_player(thing, 0);
    /* notify the area if something stops listening, but only if it
       wasn't listening before */
    if (!IsPlayer(thing) && (hear || listener) &&
        !Hearer(thing) && !Listener(thing)) {
      tp = tbuf1;
      safe_format(tbuf1, &tp, T("%s is no longer listening."), Name(thing));
      *tp = '\0';
      if (GoodObject(Location(thing)))
        notify_except(Contents(Location(thing)), NOTHING, tbuf1,
                      NA_INTER_PRESENCE);
      notify_except(Contents(thing), NOTHING, tbuf1, 0);
    }
    if (IsRoom(thing) && is_flag(f, "MONITOR") && !hear && !Listener(thing)) {
      tp = tbuf1;
      safe_format(tbuf1, &tp, T("%s is no longer listening."), Name(thing));
      *tp = '\0';
      notify_except(Contents(thing), NOTHING, tbuf1, NA_INTER_PRESENCE);
    }
    if (is_flag(f, "AUDIBLE")) {
      switch (Typeof(thing)) {
      case TYPE_EXIT:
        if (Audible(Source(thing))) {
          tp = tbuf1;
          safe_format(tbuf1, &tp, T("Exit %s is no longer broadcasting."),
                      Name(thing));
          *tp = '\0';
          notify_except(Contents(Source(thing)), NOTHING, tbuf1, 0);
        }
        break;
      case TYPE_ROOM:
        notify_except(Contents(thing), NOTHING,
                      T("Audible exits in this room have been deactivated."),
                      0);
        break;
      case TYPE_THING:
      case TYPE_PLAYER:
        notify_except(Contents(thing), thing,
                      T("This room is no longer broadcasting."), 0);
        notify(thing, T("Your contents can no longer be heard from outside."));
        break;
      }
    }
    if (is_flag(f, "QUIET") || (!AreQuiet(player, thing))) {
      tp = tbuf1;
      safe_str(Name(thing), tbuf1, &tp);
      safe_str(" - ", tbuf1, &tp);
      safe_str(f->name, tbuf1, &tp);
      safe_str(T(" reset."), tbuf1, &tp);
      *tp = '\0';
      notify(player, tbuf1);
    }
  } else {

    /* log if necessary */
    if (is_flag(f, "WIZARD")) {
      do_log(LT_WIZ, player, thing, "WIZFLAG SET");
      /* marker powers */
      powergroup_db_set(player, thing, "WIZARD", 1);
      SLEVEL(thing) = 29;
    } else if (is_flag(f, "ROYALTY")) {
      do_log(LT_WIZ, player, thing, "ROYAL FLAG SET");
      /* marker powers */
      powergroup_db_set(player, thing, "ROYALTY", 1);
      if(!has_flag_by_name(thing, "WIZARD", NOTYPE))
        SLEVEL(thing) = 28;
    }
    if (is_flag(f, "SUSPECT"))
      do_log(LT_WIZ, player, thing, "SUSPECT FLAG SET");
    if (is_flag(f, "SHARED"))
         check_zone_lock(player, thing, 1);
    /* DARK players should be treated as logged out */
    if (is_flag(f, "DARK") && IsPlayer(thing))
      hide_player(thing, 1);
    /* notify area if something starts listening */
    if (!IsPlayer(thing) &&
        (is_flag(f, "PUPPET") || is_flag(f, "MONITOR")) && !hear && !listener) {
      tp = tbuf1;
      safe_format(tbuf1, &tp, T("%s is now listening."), Name(thing));
      *tp = '\0';
      if (GoodObject(Location(thing)))
        notify_except(Contents(Location(thing)), NOTHING, tbuf1,
                      NA_INTER_PRESENCE);
      notify_except(Contents(thing), NOTHING, tbuf1, 0);
    }
    if (IsRoom(thing) && is_flag(f, "MONITOR") && !hear && !listener) {
      tp = tbuf1;
      safe_format(tbuf1, &tp, T("%s is now listening."), Name(thing));
      *tp = '\0';
      notify_except(Contents(thing), NOTHING, tbuf1, 0);
    }
    /* notify for audible exits */
    if (is_flag(f, "AUDIBLE")) {
      switch (Typeof(thing)) {
      case TYPE_EXIT:
        if (Audible(Source(thing))) {
          tp = tbuf1;
          safe_format(tbuf1, &tp, T("Exit %s is now broadcasting."),
                      Name(thing));
          *tp = '\0';
          notify_except(Contents(Source(thing)), NOTHING, tbuf1, 0);
        }
        break;
      case TYPE_ROOM:
        notify_except(Contents(thing), NOTHING,
                      T("Audible exits in this room have been activated."), 0);
        break;
      case TYPE_PLAYER:
      case TYPE_THING:
        notify_except(Contents(thing), thing,
                      T("This room is now broadcasting."), 0);
        notify(thing, T("Your contents can now be heard from outside."));
        break;
      }
    }
    if (is_flag(f, "QUIET") || (!AreQuiet(player, thing))) {
      tp = tbuf1;
      safe_str(Name(thing), tbuf1, &tp);
      safe_str(" - ", tbuf1, &tp);
      safe_str(f->name, tbuf1, &tp);
      safe_str(T(" set."), tbuf1, &tp);
      *tp = '\0';
      notify(player, tbuf1);
    }
  }
}


/** Check if an object has one or all of a list of flag characters.
 * This function is used by orflags and andflags to check to see
 * if an object has one or all of the flags signified by a list
 * of flag characters.
 * \param ns name of namespace to search.
 * \param player the object checking, for permissions
 * \param it the object on which to check for flags.
 * \param fstr string of flag characters to check for.
 * \param type 0=orflags, 1=andflags.
 * \retval 1 object has any (or all) flags.
 * \retval 0 object has no (or not all) flags.
 */
int
flaglist_check(const char *ns, dbref player, dbref it, const char *fstr,
               int type)
{
  char *s;
  FLAG *fp;
  int negate, temp;
  int ret = type;
  FLAGSPACE *n;

  negate = temp = 0;
  if (!GoodObject(it))
    return 0;
  if (!(n = (FLAGSPACE *) hashfind(ns, &htab_flagspaces))) {
    do_rawlog(LT_ERR, T("FLAG: Unable to locate flagspace %s"), ns);
    return 0;
  }
  for (s = (char *) fstr; *s; s++) {
    /* Check for a negation sign. If we find it, we note it and 
     * increment the pointer to the next character.
     */
    if (*s == '!') {
      negate = 1;
      s++;
    } else {
      negate = 0;               /* It's important to clear this at appropriate times;
                                 * else !Dc means (!D && !c), instead of (!D && c). */
    }
    if (!*s)
      /* We got a '!' that wasn't followed by a letter.
       * Fail the check. */
      return (type == 1) ? 0 : ret;
    /* Find the flag. */
    if ((fp = letter_to_flagptr(n, *s, Typeof(it))) == NULL) {
      /* Maybe *s is a type specifier (P, T, E, R). These aren't really
       * flags, but we grandfather them in to preserve old code
       */
      if ((*s == 'T') || (*s == 'R') || (*s == 'E') || (*s == 'P')) {
        temp = (*s == 'T') ? (Typeof(it) == TYPE_THING) :
          ((*s == 'R') ? (Typeof(it) == TYPE_ROOM) :
           ((*s == 'E') ? (Typeof(it) == TYPE_EXIT) :
            (Typeof(it) == TYPE_PLAYER)));
        if ((type == 1) && ((negate && temp) || (!negate && !temp)))
          return 0;
        else if ((type == 0) && ((!negate && temp) || (negate && !temp)))
          ret |= 1;
      } else {
        /* Either we got a '!' that wasn't followed by a letter, or
         * we couldn't find that flag. For AND, since we've failed
         * a check, we can return false. Otherwise we just go on.
         */
        if (type == 1)
          return 0;
        else
          continue;
      }
    } else {
      /* does the object have this flag? */
      temp = (has_flag(it, fp) && Can_See_Flag(player, it, fp));
      if ((type == 1) && ((negate && temp) || (!negate && !temp))) {
        /* Too bad there's no NXOR function...
         * At this point we've either got a flag and we don't want
         * it, or we don't have a flag and we want it. Since it's
         * AND, we return false.
         */
        return 0;
      } else if ((type == 0) && ((!negate && temp) || (negate && !temp))) {
        /* We've found something we want, in an OR. We OR a
         * true with the current value.
         */
        ret |= 1;
      }
      /* Otherwise, we don't need to do anything. */
    }
  }
  return ret;
}

/** Check if an object has one or all of a list of flag names.
 * This function is used by orlflags and andlflags to check to see
 * if an object has one or all of the flags signified by a list
 * of flag names.
 * \param ns name of namespace to search.
 * \param player the object checking, for permissions
 * \param it the object on which to check for flags.
 * \param fstr string of flag names, space-separated, to check for.
 * \param type 0=orlflags, 1=andlflags.
 * \retval 1 object has any (or all) flags.
 * \retval 0 object has no (or not all) flags.
 */
int
flaglist_check_long(const char *ns, dbref player, dbref it, const char *fstr,
                    int type)
{
  char *s, *copy, *sp;
  FLAG *fp;
  int negate, temp;
  int ret = type;
  FLAGSPACE *n;
  negate = temp = 0;
  if (!GoodObject(it))
    return 0;
  if (!(n = (FLAGSPACE *) hashfind(ns, &htab_flagspaces))) {
    do_rawlog(LT_ERR, T("FLAG: Unable to locate flagspace %s"), ns);
    return 0;
  }
  copy = mush_strdup(fstr, "flaglistlong");
  sp = trim_space_sep(copy, ' ');
  while (sp) {
    s = split_token(&sp, ' ');
    /* Check for a negation sign. If we find it, we note it and 
     * increment the pointer to the next character.
     */
    if (*s == '!') {
      negate = 1;
      s++;
    } else {
      negate = 0;               /* It's important to clear this at appropriate times;
                                 * else !D c means (!D && !c), instead of (!D && c). */
    }
    if (!*s) {
      /* We got a '!' that wasn't followed by a string.
       * Fail the check. */
      if (type == 1)
        ret = 0;
      break;
    }
    /* Find the flag. */
    if (!(fp = flag_hash_lookup(n, s, Typeof(it)))) {
      /* Either we got a '!' that wasn't followed by a letter, or
       * we couldn't find that flag. For AND, since we've failed
       * a check, we can return false. Otherwise we just go on.
       */
      if (type == 1) {
        ret = 0;
        break;
      } else
        continue;
    } else {
      /* does the object have this flag? There's a special case
       * here, as we want (for consistency with flaglist_check)
       * to allow types to match as well
       */
      if (!strcmp(fp->name, "PLAYER"))
        temp = IsPlayer(it);
      else if (!strcmp(fp->name, "THING"))
        temp = IsThing(it);
      else if (!strcmp(fp->name, "ROOM"))
        temp = IsRoom(it);
      else if (!strcmp(fp->name, "EXIT"))
        temp = IsExit(it);
      else
        temp = (has_flag(it, fp) && Can_See_Flag(player, it, fp));
      if ((type == 1) && ((negate && temp) || (!negate && !temp))) {
        /* Too bad there's no NXOR function...
         * At this point we've either got a flag and we don't want
         * it, or we don't have a flag and we want it. Since it's
         * AND, we return false.
         */
        ret = 0;
        break;
      } else if ((type == 0) && ((!negate && temp) || (negate && !temp))) {
        /* We've found something we want, in an OR. We OR a
         * true with the current value.
         */
        ret |= 1;
      }
      /* Otherwise, we don't need to do anything. */
    }
  }
  mush_free(copy, "flaglistlong");
  return ret;
}


/** Can a player see a flag?
 * \param privs looker.
 * \param thing object on which to look for flag.
 * \param name name of flag to look for.
 * \retval 1 object has the flag and looker can see it.
 * \retval 0 looker can not see flag on object.
 */
int
sees_flag(dbref privs, dbref thing, const char *name)
{
  /* Does thing have the flag named name && can privs see it? */
  FLAG *f;
  FLAGSPACE *n;
  n = hashfind("FLAG", &htab_flagspaces);
  if ((f = flag_hash_lookup(n, name, Typeof(thing))) == NULL)
    return 0;
  return has_flag(thing, f) && Can_See_Flag(privs, thing, f);
}


/** A hacker interface for adding a flag.
 * \verbatim
 * This function is used to add a new flag to the game. It's
 * called by @flag (via do_flag_add()), and is the right function to
 * call in flaglocal.c if you're writing a hardcoded system patch that
 * needs to add its own flags. It will not add the same flag twice.
 * \endverbatim
 * \param ns name of namespace to add flag to.
 * \param name flag name.
 * \param letter flag character (or ascii 0)
 * \param type mask of object types to which the flag applies.
 * \param perms mask of permissions to see/set the flag.
 * \param negate_perms mask of permissions to clear the flag.
 */
FLAG *
add_flag(const char *name, const char letter, int type,
         int perms, int negate_perms)
{
  FLAG *f;
  FLAGSPACE *n;
  n = hashfind("FLAG", &htab_flagspaces);
  /* Don't double-add */
  if ((f = match_flag_ns(n, strupper(name)))) {
    if (strcasecmp(f->name, name) == 0)
      return f;
  }
  f = new_flag();
  f->name = mush_strdup(strupper(name), "flag name");
  f->letter = letter;
  f->type = type;
  f->perms = perms;
  f->negate_perms = negate_perms;
  f->bitpos = -1;
  flag_add(n, f->name, f);
  return f;
}


/*--------------------------------------------------------------------------
 * MUSHcode interface
 */

/** User interface to list flags.
 * \verbatim
 * This function implements @flag/list.
 * \endverbatim
 * \param player the enactor.
 * \param arg wildcard pattern of flag names to list, or NULL for all.
 * \param lc if 1, list flags in lowercase.
 */
void
do_list_flags(dbref player, const char *arg, int lc)
{
  char *b = list_all_flags("FLAG", arg, player, 0x3);
  notify_format(player, T("Flags: %s"), lc ? strlower(b) : b);
}

/** User interface to show flag detail.
 * \verbatim
 * This function implements @flag <flag>.
 * \endverbatim
 * \param ns name of namespace to search.
 * \param player the enactor.
 * \param name name of the flag to describe.
 */
void
do_flag_info(const char *ns, dbref player, const char *name)
{
  FLAG *f;
  FLAGSPACE *n;
  /* Find the flagspace */
  if (!(n = (FLAGSPACE *) hashfind(ns, &htab_flagspaces))) {
    do_rawlog(LT_ERR, T("FLAG: Unable to locate flagspace %s"), ns);
    return;
  }

  /* Find the flag */
  f = flag_hash_lookup(n, name, NOTYPE);
  if (!f && God(player))
    f = match_flag_ns(n, name);
  if (!f) {
    notify_format(player, T("No such %s."), strlower(ns));
    return;
  }
  notify_format(player, T("Flag name: %s"), f->name);
  notify_format(player, T("Flag char: %c"), f->letter);
  notify_format(player, "  Aliases: %s", list_aliases(n, f));
  notify_format(player, "  Type(s): %s", f->type == ALLTYPES ? "ANY" : privs_to_string(type_privs, f->type));
  notify_format(player, "    Perms: %s", privs_to_string(flag_privs, f->perms));
  notify_format(player, "ResetPrms: %s",
                privs_to_string(flag_privs, f->negate_perms));
}

/** Change the permissions on a flag. 
 * \verbatim
 * This is the user-interface to @flag/restrict, which uses this syntax:
 *
 * @flag/restrict <flag> = <perms>, <negate_perms> 
 *
 * If no comma is given, use <perms> for both.
 * \endverbatim
 * \param ns name of namespace to search.
 * \param player the enactor.
 * \param name name of flag to modify.
 * \param args_right array of arguments on the right of the equal sign.
 */
void
do_flag_restrict(dbref player, const char *name, char *args_right[])
{
  FLAG *f;
  FLAGSPACE *n;
  int perms, negate_perms;

  if (!Director(player)) {
    notify(player, T("You don't have enough magic for that."));
    return;
  }
  n = hashfind("FLAG", &htab_flagspaces);
  if (!(f = flag_hash_lookup(n, name, NOTYPE))) {
    notify_format(player, T("No such flag."));
    return;
  }
  if (!args_right[1] || !*args_right[1]) {
    notify_format(player, T("How do you want to restrict that flag?"));
    return;
  }
  if (!strcasecmp(args_right[1], "any")) {
    perms = F_ANY;
  } else {
    perms = string_to_privs(flag_privs, args_right[1], 0);
    if ((!perms) || (perms & (F_INTERNAL | F_DISABLED))) {
      notify(player, T("I don't understand those permissions."));
      return;
    }
  }
  if (args_right[2] && *args_right[2]) {
    if (!strcasecmp(args_right[2], "any")) {
      negate_perms = F_ANY;
    } else {
      negate_perms = string_to_privs(flag_privs, args_right[2], 0);
      if ((!negate_perms) || (negate_perms & (F_INTERNAL | F_DISABLED))) {
        notify(player, T("I don't understand those permissions."));
        return;
      }
    }
  } else {
    negate_perms = string_to_privs(flag_privs, args_right[1], 0);
  }
  f->perms = perms;
  f->negate_perms = negate_perms;
  notify_format(player, T("Permissions on %s flag set."), f->name);
}


/** Change the type of a flag.
 * \verbatim
 * This is the user-interface to @flag/type, which uses this syntax:
 *
 * @flag/type <flag> = <type(s)>
 *
 * We refuse to change the type of a flag if there are objects
 * with the flag that would no longer be of the correct type.
 * \endverbatim
 * \param ns name of the flagspace to use.
 * \param player the enactor.
 * \param name name of flag to modify.
 * \param type_string list of types.
 */
void
do_flag_type(const char *ns, dbref player, const char *name, char *type_string)
{
  FLAG *f;
  FLAGSPACE *n;
  int type;
  dbref it;

  if (!God(player)) {
    notify(player, T("You don't have enough magic for that."));
    return;
  }
  n = hashfind(ns, &htab_flagspaces);
  if (!(f = flag_hash_lookup(n, name, NOTYPE))) {
    notify_format(player, T("No such %s."), strlower(ns));
    return;
  }
  if (!type_string || !*type_string) {
    notify_format(player, T("What type do you want to make that %s?"),
                  strlower(ns));
    return;
  }
  if (!strcasecmp(type_string, "any")) {
    type = NOTYPE;
  } else {
    type = string_to_privs(type_privs, type_string, 0);
    if (!type) {
      notify(player, T("I don't understand the list of types."));
      return;
    }
    /* Are there any objects with the flag that don't match these
     * types?
     */
    for (it = 0; it < db_top; it++) {
      if (!(type & Typeof(it)) && has_flag(it, f)) {
      notify(player,
             T
             ("Objects of other types already have this flag set. Search for them and remove the flag first."));
      return;
      }
    }
  }
  f->type = type;
  notify_format(player, T("Type of %s flag set."), f->name);
}



/** Add a new flag
 * \verbatim
 * This function implements @flag/add, which uses this syntax:
 *
 * @flag/add <flag> = <letter>, <type(s)>, <perms>, <negate_perms> 
 *
 * <letter> defaults to none. If given, it must not match an existing
 *   flag character that could apply to the given type
 * <type(s)> defaults to NOTYPE
 * <perms> defaults to any
 * <negate_perms> defaults to <perms>
 * \endverbatim
 * \param player the enactor.
 * \param name name of the flag to add.
 * \param args_right array of arguments on the right side of the equal sign.
 */
void
do_flag_add(dbref player, const char *name, char *args_right[])
{
  char letter = '\0';
  int type = NOTYPE;
  int perms = F_ANY;
  int negate_perms = F_ANY;
  FLAG *f;
  FLAGSPACE *n;

  if (!Director(player)) {
    notify(player, T("You don't have enough magic for that."));
    return;
  }
  if (!name || !*name) {
    notify_format(player, T("You must provide a name for the flag."));
    return;
  }
  if (strlen(name) == 1) {
    notify_format(player, T("Flag names must be longer than one character."));
    return;
  }
  if (strchr(name, ' ')) {
    notify_format(player, T("Flag names may not contain spaces."));
    return;
  }
  Flagspace_Lookup(n, "FLAG");
  /* Do we have a letter? */
  if (args_right[1]) {
    if (strlen(args_right[1]) > 1) {
      notify_format(player, T("Flag characters must be single characters."));
      return;
    }
    letter = *args_right[1];
    /* Do we have a type? */
    if (args_right[2]) {
      if (*args_right[2] && strcasecmp(args_right[2], "any"))
        type = string_to_privs(type_privs, args_right[2], 0);
      if (!type) {
        notify(player, T("I don't understand the list of types."));
        return;
      }
    }
    /* Is this letter already in use for this type? */
    if (*args_right[1]) {
      if ((f = letter_to_flagptr(n, *args_right[1], type))) {
        notify_format(player, T("Letter conflicts with the %s flag."), f->name);
        return;
      }
    }
    /* Do we have perms? */
    if (args_right[3] && *args_right[3]) {
      if (!strcasecmp(args_right[3], "any")) {
        perms = F_ANY;
      } else {
        perms = string_to_privs(flag_privs, args_right[3], 0);
        if ((!perms) || (perms & (F_INTERNAL | F_DISABLED))) {
          notify(player, T("I don't understand those permissions."));
          return;
        }
      }
    }
    if (args_right[4] && *args_right[4]) {
      if (!strcasecmp(args_right[4], "any")) {
        negate_perms = F_ANY;
      } else {
        negate_perms = string_to_privs(flag_privs, args_right[4], 0);
        if ((!negate_perms) || (negate_perms & (F_INTERNAL | F_DISABLED))) {
          notify(player, T("I don't understand those permissions."));
          return;
        }
      }
    } else
      negate_perms = perms;
  }
  /* Ok, let's do it. */
  add_flag(name, letter, type, perms, negate_perms);
  /* Did it work? */
  if ((f = match_flag(name)))
    do_flag_info("FLAG", player, name);
  else
    notify_format(player, T("Unknown failure adding flag."));
}

/** Alias a flag.
 * \verbatim
 * This function implements the @flag/alias commmand.
 * \endverbatim
 * \param player the enactor.
 * \param name name of the flag to alias.
 * \param alias name of the alias.
 */
void
do_flag_alias(dbref player, const char *name, const char *alias)
{
  FLAG *f, *af;
  FLAGSPACE *n;
  int delete = 0;
  if (!Director(player)) {
    notify(player, T("You don't look like God."));
    return;
  }
  if (!alias || !*alias) {
    notify(player, T("You must provide a name for the alias."));
    return;
  }
  if (*alias == '!') {
    delete = 1;
    alias++;
  }
  if (strlen(alias) <= 1) {
    notify(player, T("Flag aliases must be longer than one character."));
    return;
  }
  if (strchr(alias, ' ')) {
    notify(player, T("Flag aliases may not contain spaces."));
    return;
  }
  n = hashfind("FLAG", &htab_flagspaces);
  af = match_flag_ns(n, alias);
  if (!delete && af) {
    notify_format(player, T("That alias already matches the %s flag."),
                  af->name);
    return;
  }
  f = match_flag_ns(n, name);
  if (!f) {
    notify(player, T("I don't know that flag."));
    return;
  }
  if (f->perms & F_DISABLED) {
    notify(player, T("That flag is disabled."));
    return;
  }
  if (delete && !af) {
    notify_format(player, T("That isn't an alias of the %s flag."),
                  f->name);
    return;
  }
  if (delete) {
    /* Delete the alias in the ptab if it's really an alias! */
    if (!strcasecmp(n->flags[f->bitpos]->name, alias)) {
      notify(player, T("That's the flag's name, not an alias."));
      return;
    }
    ptab_delete(n->tab, alias);
    if ((af = match_flag_ns(n, alias)))
        notify(player, T("Unknown failure deleting alias."));
    else
      do_flag_info("FLAG", player, f->name);
  } else {
    /* Insert the flag in the ptab by the given alias */
    ptab_start_inserts(n->tab);
    ptab_insert(n->tab, alias, f);
    ptab_end_inserts(n->tab);
    if ((f = match_flag_ns(n, alias)))
      do_flag_info("FLAG", player, alias);
    else
      notify(player, T("Unknown failure adding alias."));
  }
}

/** Change a flag's alias. 
 * \param player the enactor.
 * \param name name of the flag.
 * \param letter The new alias, or an empty string to remove the alias.
 */
void
do_flag_letter(dbref player, const char *name, const char *letter)
{
  FLAG *f;
  FLAGSPACE *n;

  if (!Director(player)) {
    notify(player, T("You don't look like God."));
    return;
  }
  Flagspace_Lookup(n, "FLAG");
  f = match_flag_ns(n, name);
  if (!f) {
    notify_format(player, T("I don't know that flag."));
    return;
  }
  if (letter && *letter) {
    FLAG *other;

    if (strlen(letter) > 1) {
      notify_format(player, T("Flag characters must be single characters."));
      return;
    }

    if ((other = letter_to_flagptr(n, *letter, NOTYPE))) {
      notify_format(player, T("Letter conflicts with the %s flag."),
                    other->name);
      return;
    }

    f->letter = *letter;
    notify_format(player, T("Letter for flag %s set to '%c'."),
                   f->name, *letter);
  } else {                      /* Clear a flag */
    f->letter = '\0';
    notify_format(player, T("Letter for flag %s cleared."), f->name);
  }
}


/** Disable a flag. 
 * \verbatim
 * This function implements @flag/disable.
 * Only God can do this, and it makes the flag effectively
 * unusuable - it's invisible to all but God, and can't be set, cleared,
 * or tested against.
 * \endverbatim
 * \param player the enactor.
 * \param name name of the flag to disable.
 */
void
do_flag_disable(dbref player, const char *name)
{
  FLAG *f;
  if (!Director(player)) {
    notify(player, T("You don't look like God."));
    return;
  }
  f = match_flag(name);
  if (!f) {
    notify_format(player, T("I don't know that."));
    return;
  }
  if (f->perms & F_DISABLED) {
    notify_format(player, T("That flag is already disabled."));
    return;
  }
  /* Do it. */
  f->perms |= F_DISABLED;
  notify_format(player, T("Flag %s disabled."), f->name);
}

/** Delete a flag. 
 * \verbatim
 * This function implements @flag/delete.
 * Only God can do this, and clears the flag on everyone
 * and then removes it and its aliases from the tables.
 * Danger, Will Robinson!
 * \endverbatim
 * \param player the enactor.
 * \param name name of the flag to delete.
 */
void
do_flag_delete(dbref player, const char *name)
{
  FLAG *f, *tmpf;
  char flagname[BUFFER_LEN];
  dbref i;
  int got_one;
  FLAGSPACE *n;

  if (!God(player)) {
    notify(player, T("You don't look like God."));
    return;
  }
  n = (FLAGSPACE *) hashfind("FLAG", &htab_flagspaces);
  f = ptab_find_exact(n->tab, name);
  if (!f) {
    notify_format(player, T("I don't know that flag."));
    return;
  }
  if (f->perms & F_INTERNAL) {
    notify(player, T("There are probably easier ways to crash your MUSH."));
    return;
  }
  /* Remove aliases. Convoluted because ptab_delete probably trashes
   * the firstentry/nextentry stuff
   */
  do {
    got_one = 0;
    tmpf = ptab_firstentry_new(n->tab, flagname);
    while (tmpf) {
      if (!strcmp(tmpf->name, f->name) &&
          strcmp(n->flags[f->bitpos]->name, flagname)) {
        ptab_delete(n->tab, flagname);
        got_one = 1;
        break;
      }
      tmpf = ptab_nextentry_new(n->tab, flagname);
    }
  } while (got_one);
  /* Reset the flag on all objects */
  for (i = 0; i < db_top; i++)
    twiddle_flag(i, f, 1);
  /* Remove the flag's entry in flags */
  n->flags[f->bitpos] = NULL;
  /* Remove the flag from the ptab */
  ptab_delete(n->tab, f->name);
  notify_format(player, T("Flag %s deleted."), f->name);
  /* Free the flag. */
  mush_free((char *) f->name, "flag name");
  mush_free(f, "flag");
}

/** Enable a disabled flag.
 * \verbatim
 * This function implements @flag/enable.
 * Only God can do this, and it reverses /disable.
 * \endverbatim
 * \param player the enactor.
 * \param name name of the flag to enable.
 */
void
do_flag_enable(dbref player, const char *name)
{
  FLAG *f;
  if (!Director(player)) {
    notify(player, T("You don't look like God."));
    return;
  }
  f = match_flag(name);
  if (!f) {
    notify_format(player, T("I don't know that flag."));
    return;
  }
  if (!(f->perms & F_DISABLED)) {
    notify_format(player, T("That flag is not disabled."));
    return;
  }
  /* Do it. */
  f->perms &= ~F_DISABLED;
  notify_format(player, T("Flag %s enabled."), f->name);
}


static char *
list_aliases(FLAGSPACE * n, FLAG *given)
{
  FLAG *f;
  static char buf[BUFFER_LEN];
  char *bp;
  char flagname[BUFFER_LEN];
  int first = 1;
  bp = buf;
  f = ptab_firstentry_new(n->tab, flagname);
  while (f) {
    if (!strcmp(given->name, f->name) &&
        strcmp(n->flags[f->bitpos]->name, flagname)) {
      /* This is an alias! */
      if (!first)
        safe_chr(' ', buf, &bp);
      first = 0;
      safe_str(flagname, buf, &bp);
    }
    f = ptab_nextentry_new(n->tab, flagname);
  }
  *bp = '\0';
  return buf;
}


/** Return a list of all flags.
 * \param ns name of namespace to search.
 * \param name wildcard to match against flag names, or NULL for all.
 * \param privs the looker, for permission checking.
 * \param which a bitmask of 0x1 (flag chars) and 0x2 (flag names).
 */
char *
list_all_flags(const char *ns, const char *name, dbref privs, int which)
{
  FLAG *f;
  char **ptrs;
  int i, numptrs = 0;
  static char buf[BUFFER_LEN];
  char *bp;
  int disallowed;
  FLAGSPACE *n;

  Flagspace_Lookup(n, ns);
  disallowed = God(privs) ? F_INTERNAL : (F_INTERNAL | F_DISABLED);
  if (!Admin(privs))
    disallowed |= (F_DARK | F_MDARK);
  ptrs = (char **) malloc(n->flagbits * sizeof(char *));
  for (i = 0; i < n->flagbits; i++) {
    if ((f = n->flags[i]) && !(f->perms & disallowed)) {
      if (!name || !*name || quick_wild(name, f->name))
        ptrs[numptrs++] = (char *) f->name;
    }
  }
  do_gensort(privs, ptrs, NULL, numptrs, ALPHANUM_LIST);
  bp = buf;
  for (i = 0; i < numptrs; i++) {
    switch (which) {
    case 0x3:
      if (i)
        safe_strl(", ", 2, buf, &bp);
      safe_str(ptrs[i], buf, &bp);
      f = match_flag_ns(n, ptrs[i]);
      if (!f)
        break;
      if (f->letter != '\0')
        safe_format(buf, &bp, " (%c)", f->letter);
      if (f->perms & F_DISABLED)
        safe_str(T(" (disabled)"), buf, &bp);
      break;
    case 0x2:
      if (i)
        safe_chr(' ', buf, &bp);
      safe_str(ptrs[i], buf, &bp);
      break;
    case 0x1:
      f = match_flag_ns(n, ptrs[i]);
      if (f && (f->letter != '\0'))
        safe_chr(f->letter, buf, &bp);
      break;
    }
  }
  *bp = '\0';
  free(ptrs);
  return buf;
}




/** Show the flags and powers associated with a command.
 * \param flagmask the command's flagmask.
 * \param powers the command's power mask.
 * \return string output of powers and flags.
 */
const char *
show_command_flags(object_flag_type flagmask, div_pbits powers)
{
  static char fbuf[BUFFER_LEN];
  char *bp;
  bp = fbuf;

  /* do generic flags */
  if (flagmask) {
    if (powers)
      safe_chr('\n', fbuf, &bp);
    safe_str("Flags      : ", fbuf, &bp);
    safe_str(bits_to_string("FLAG", flagmask, GOD, NOTHING), fbuf, &bp);
  }

  *bp = '\0';
  return fbuf;
}
