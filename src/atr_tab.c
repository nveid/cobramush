/**
 * \file atr_tab.c
 *
 * \brief The table of standard attributes and code to manipulate it.
 *
 *
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "conf.h"
#include "externs.h"
#include "attrib.h"
#include "atr_tab.h"
#include "command.h"
#include "ptab.h"
#include "privtab.h"
#include "mymalloc.h"
#include "dbdefs.h"
#include "log.h"
#include "parse.h"
#include "confmagic.h"

/* CatchAll */
extern ATTR *catchall;

/** An alias for an attribute.
 */
typedef struct atr_alias {
  const char *alias;		/**< The alias. */
  const char *realname;		/**< The attribute's canonical name. */
} ATRALIAS;


/** Prefix table for standard attribute names */
PTAB ptab_attrib;

/** Attribute flags for setting */
PRIV attr_privs_set[] = {
  {"no_command", '$', AF_NOPROG, AF_NOPROG},
  {"no_inherit", 'i', AF_PRIVATE, AF_PRIVATE},
  {"private", 'i', AF_PRIVATE, AF_PRIVATE},
  {"no_clone", 'c', AF_NOCOPY, AF_NOCOPY},
  {"privilege", 'w', AF_PRIVILEGE, AF_PRIVILEGE},
  {"visual", 'v', AF_VISUAL, AF_VISUAL},
  {"mortal_dark", 'm', AF_MDARK, AF_MDARK},
  {"hidden", 'm', AF_MDARK, AF_MDARK},
  {"regexp", 'R', AF_REGEXP, AF_REGEXP},
  {"case", 'C', AF_CASE, AF_CASE},
  {"locked", '+', AF_LOCKED, AF_LOCKED},
  {"safe", 'S', AF_SAFE, AF_SAFE},
  {"prefixmatch", '\0', AF_PREFIXMATCH, AF_PREFIXMATCH},
  {"veiled", 'V', AF_VEILED, AF_VEILED},
  {"debug", 'b', AF_DEBUG, AF_DEBUG},
  {"public", 'p', AF_PUBLIC, AF_PUBLIC},
  {"nearby", 'n', AF_NEARBY, AF_NEARBY},
  {"amhear", 'M', AF_MHEAR, AF_MHEAR},
  {"aahear", 'A', AF_AHEAR, AF_AHEAR},
  {NULL, '\0', 0, 0}
};

/** Attribute flags for viewing */
PRIV attr_privs_view[] = {
  {"no_command", '$', AF_NOPROG, AF_NOPROG},
  {"no_inherit", 'i', AF_PRIVATE, AF_PRIVATE},
  {"pow_inherit", 't', AF_POWINHERIT, AF_POWINHERIT},
  {"private", 'i', AF_PRIVATE, AF_PRIVATE},
  {"no_clone", 'c', AF_NOCOPY, AF_NOCOPY},
  {"privilege", 'w', AF_PRIVILEGE, AF_PRIVILEGE},
  {"visual", 'v', AF_VISUAL, AF_VISUAL},
  {"mortal_dark", 'm', AF_MDARK, AF_MDARK},
  {"hidden", 'm', AF_MDARK, AF_MDARK},
  {"regexp", 'R', AF_REGEXP, AF_REGEXP},
  {"case", 'C', AF_CASE, AF_CASE},
  {"locked", '+', AF_LOCKED, AF_LOCKED},
  {"safe", 'S', AF_SAFE, AF_SAFE},
  {"internal", '\0', AF_INTERNAL, AF_INTERNAL},
  {"prefixmatch", '\0', AF_PREFIXMATCH, AF_PREFIXMATCH},
  {"veiled", 'V', AF_VEILED, AF_VEILED},
  {"debug", 'b', AF_DEBUG, AF_DEBUG},
  {"public", 'p', AF_PUBLIC, AF_PUBLIC},
  {"nearby", 'n', AF_NEARBY, AF_NEARBY},
  {"amhear", 'M', AF_MHEAR, AF_MHEAR},
  {"aahear", 'A', AF_AHEAR, AF_AHEAR},
  {NULL, '\0', 0, 0}
};


/*----------------------------------------------------------------------
 * Prefix-table functions of various sorts
 */

static ATTR *aname_find_exact(const char *name);
void init_aname_table(void);

/** Attribute table lookup by name or alias.
 * given an attribute name, look it up in the complete attribute table
 * (real names plus aliases), and return the appropriate real attribute.
 */
ATTR *
aname_hash_lookup(const char *name)
{
  ATTR *ap;
  /* Exact matches always work */
  if ((ap = (ATTR *) ptab_find_exact(&ptab_attrib, name)))
    return ap;
  /* Prefix matches work if the attribute is AF_PREFIXMATCH */
  if ((ap = (ATTR *) ptab_find(&ptab_attrib, name)) && AF_Prefixmatch(ap))
    return ap;
  return NULL;
}

/** Build the basic attribute table.
 */
void
init_aname_table(void)
{
  ATTR *ap;

  ptab_init(&ptab_attrib);
  ptab_start_inserts(&ptab_attrib);
  for (ap = attr; ap->name; ap++)
    ptab_insert(&ptab_attrib, ap->name, ap);
  ptab_end_inserts(&ptab_attrib);
}

/** Associate a new alias with an existing attribute.
 */
int
alias_attribute(const char *atr, const char *alias)
{
  ATTR *ap;

  /* Make sure the alias doesn't exist already */
  if (aname_find_exact(alias))
    return 0;

  /* Look up the original */
  ap = aname_find_exact(atr);
  if (!ap)
    return 0;

  ptab_start_inserts(&ptab_attrib);
  ptab_insert(&ptab_attrib, strupper(alias), ap);
  ptab_end_inserts(&ptab_attrib);
  return 1;
}

static ATTR *
aname_find_exact(const char *name)
{
  char atrname[BUFFER_LEN];
  strcpy(atrname, name);
  upcasestr(atrname);
  return (ATTR *) ptab_find_exact(&ptab_attrib, atrname);
}

/* Sets global locks on attributes */

void do_attribute_lock(dbref player, char *name, char *lock, switch_mask swi) {
  ATTR *ap, *ap2;
  int i, insert = 0;
  boolexp key;

  /* Parse name and lock  */
  if ((!name || !*name) && !SW_ISSET(swi, SWITCH_DEFAULTS)) {
    notify(player, T("Which attribute do you mean?"));
    return;
  }

/* Parse Lock Here */
  key = parse_boolexp(player, lock, "ATTR");

  if(key == TRUE_BOOLEXP) {
    notify(player, T("I don't understand that key."));
    return;
  }
 
  if(!SW_ISSET(swi, SWITCH_DEFAULTS)) {
    upcasestr(name);
    /* Is this attribute already in the table? */
    if(*name == '@')
      name++;
    ap = (ATTR *) ptab_find_exact(&ptab_attrib, name);
  } else ap = catchall;

  if (ap) {
    if (AF_Internal(ap)) {
      /* Don't muck with internal attributes */
      notify(player, T("That attribute's permissions can not be changed."));
      return;
    }

    if(!controls(player, AL_CREATOR(ap))) {
      notify(player, T(e_perm));
      return;
    } 

     /* If a lock is already set on whatever we're doing.. clear it */
    free_boolexp(SW_ISSET(swi, SWITCH_WRITE) ? AL_WLock(ap) : AL_RLock(ap));
  } else {
    /* Create fresh if the name is ok */
    if (!good_atr_name(name)) {
      notify(player, T("Invalid attribute name."));
      return;
    }
    insert = 1;
    ap = (ATTR *) mush_malloc(sizeof(ATTR), "ATTR");
    if (!ap) {
      notify(player, "Critical memory failure - Alert God!");
      do_log(LT_ERR, 0, 0, "do_attribute_lock: unable to malloc ATTR");
      return;
    }
    if(!SW_ISSET(swi, SWITCH_DEFAULTS))
      AL_NAME(ap) = mush_strdup(name, "GLOBAL.ATR.NAME");
    else 
      AL_NAME(ap) = NULL;
    AL_FLAGS(ap) = 0;
    AL_WLock(ap) = TRUE_BOOLEXP;
    AL_RLock(ap) = TRUE_BOOLEXP;   
    ap->data = NULL_CHUNK_REFERENCE;
  }


  AL_CREATOR(ap) = player;
  if(SW_ISSET(swi, SWITCH_WRITE))
    AL_WLock(ap) = key;
  else
    AL_RLock(ap) = key;


  /* Only insert when it's not already in the table */
  if (insert) {
    if(!SW_ISSET(swi, SWITCH_DEFAULTS)) {
      ptab_start_inserts(&ptab_attrib);
      ptab_insert(&ptab_attrib, name, ap);
      ptab_end_inserts(&ptab_attrib);
    } 
    /* And If its the catchall.. just point the catchall to ap */
    else catchall = ap;
  }

  /* Ok, now we need to see if there are any attributes of this name
   * set on objects in the db. If so, and if we're retroactive, set 
   * perms/creator 
   */
  if (SW_ISSET(swi, SWITCH_RETROACTIVE) && !SW_ISSET(swi, SWITCH_DEFAULTS)) {
    for (i = 0; i < db_top; i++) {
      if ((ap2 = atr_get_noparent(i, name))) {
	free_boolexp(SW_ISSET(swi, SWITCH_WRITE) ? AL_WLock(ap2) : AL_RLock(ap2));
	if(SW_ISSET(swi, SWITCH_WRITE))
	  AL_WLock(ap2) = dup_bool(AL_WLock(ap));
	else
	  AL_RLock(ap2) = dup_bool(AL_RLock(ap));
      }
    }
  }

  notify(player, "Global Atr Lock Set.");
}


/** Add new standard attributes, or change permissions on them.
 * \verbatim
 * Given the name and permission string for an attribute, add it to
 * the attribute table (or modify the permissions if it's already
 * there). Permissions may be changed retroactively, which modifies
 * permissions on any copies of that attribute set on objects in the
 * database. This is the top-level code for @attribute/access.
 * \endverbatim
 * \param player the enactor.
 * \param name the attribute name.
 * \param perms a string of attribute permissions, space-separated.
 * \param retroactive if true, apply the permissions retroactively.
 */
void
do_attribute_access(dbref player, char *name, char *perms, int retroactive)
{
  ATTR *ap, *ap2;
  int flags = 0;
  int i;
  int insert = 0;

  /* Parse name and perms */
  if (!name || !*name) {
    notify(player, T("Which attribute do you mean?"));
    return;
  }
  if (strcasecmp(perms, "none")) {
    flags = list_to_privs(attr_privs_set, perms, 0);
    if (!flags) {
      notify(player, T("I don't understand those permissions."));
      return;
    }
  }
  upcasestr(name);
  /* Is this attribute already in the table? */
  if(*name == '@')
    name++;
  ap = (ATTR *) ptab_find_exact(&ptab_attrib, name);
  if (ap) {
    if (AF_Internal(ap)) {
      /* Don't muck with internal attributes */
      notify(player, T("That attribute's permissions can not be changed."));
      return;
    }
    /* It already exists.. Make sure they can override the guy who set this before */
    if(!controls(player, AL_CREATOR(ap))) {
      notify(player, T(e_perm));
      return;
    }
  } else {
    /* Create fresh */
    insert = 1;
    ap = (ATTR *) mush_malloc(sizeof(ATTR), "ATTR");
    if (!ap) {
      notify(player, "Critical memory failure - Alert God!");
      do_log(LT_ERR, 0, 0, "do_attribute_access: unable to malloc ATTR");
      return;
    }
    AL_WLock(ap) = TRUE_BOOLEXP;
    AL_RLock(ap) = TRUE_BOOLEXP;   
    AL_NAME(ap) = mush_strdup(name, "GLOBAL.ATR.NAME");
    ap->data = NULL_CHUNK_REFERENCE;
  }

  AL_FLAGS(ap) = flags;
  AL_CREATOR(ap) = player;


  /* Only insert when it's not already in the table */
  if (insert) {
    ptab_start_inserts(&ptab_attrib);
    ptab_insert(&ptab_attrib, name, ap);
    ptab_end_inserts(&ptab_attrib);
  }

  /* Ok, now we need to see if there are any attributes of this name
   * set on objects in the db. If so, and if we're retroactive, set 
   * perms/creator 
   */
  if (retroactive) {
    for (i = 0; i < db_top; i++) {
      if ((ap2 = atr_get_noparent(i, name))) {
	AL_FLAGS(ap2) = flags;
	AL_CREATOR(ap2) = player;
      }
    }
  }

  notify_format(player, T("%s -- Attribute permissions now: %s"), name,
		privs_to_string(attr_privs_view, flags));
}


/** Delete an attribute from the attribute table.
 * \verbatim
 * Top-level function for @attrib/delete.
 * \endverbatim
 * \param player the enactor.
 * \param name the name of the attribute to delete.
 * \param catchall delete the catchall attribute
 */
void
do_attribute_delete(dbref player, char *name, char def)
{
  ATTR *ap;

  if ((!name || !*name) && !def) {
    notify(player, T("Which attribute do you mean?"));
    return;
  }

  /* Is this attribute in the table? */

  if (*name == '@')
    name++;

  ap = def ? catchall : (ATTR *) ptab_find_exact(&ptab_attrib, name);
  if (!ap) {
    notify(player, T("That attribute isn't in the attribute table"));
    return;
  }

  /* Free Atr Name */
  mush_free((Malloc_t) AL_NAME(ap), "GLOBAL.ATR.NAME");

  /* Free Locks */
  free_boolexp(AL_WLock(ap));
  free_boolexp(AL_RLock(ap));

  /* Ok, take it out of the hash table */
  if(def) {
    mush_free(catchall, "ATTR");
    catchall = NULL;
  } else
    ptab_delete(&ptab_attrib, name);

  if(def)
    notify(player, T("Default attribute permissions deleted."));
  else
    notify_format(player, T("Removed %s from attribute table."), name);

  return;
}

/** Rename an attribute in the attribute table.
 * \verbatim
 * Top-level function for @attrib/rename.
 * \endverbatim
 * \param player the enactor.
 * \param old the name of the attribute to rename.
 * \param newname the new name (surprise!)
 */
void
do_attribute_rename(dbref player, char *old, char *newname)
{
  ATTR *ap;
  if (!old || !*old || !newname || !*newname) {
    notify(player, T("Which attributes do you mean?"));
    return;
  }
  upcasestr(old);
  upcasestr(newname);
  /* Is the new name valid? */
  if (!good_atr_name(newname)) {
    notify(player, T("Invalid attribute name."));
    return;
  }
  /* Is the new name already in use? */
  ap = (ATTR *) ptab_find_exact(&ptab_attrib, newname);
  if (ap) {
    notify_format(player,
		  T("The name %s is already used in the attribute table."),
		  newname);
    return;
  }
  /* Is the old name a real attribute? */
  ap = (ATTR *) ptab_find_exact(&ptab_attrib, old);
  if (!ap) {
    notify(player, T("That attribute isn't in the attribute table"));
    return;
  }
  /* Ok, take it out and put it back under the new name */
  ptab_delete(&ptab_attrib, old);
  /*  This causes a slight memory leak if you rename an attribute
     added via /access. But that doesn't happen often. Will fix
     someday.  */
  AL_NAME(ap) = strdup(newname);
  ptab_start_inserts(&ptab_attrib);
  ptab_insert(&ptab_attrib, newname, ap);
  ptab_end_inserts(&ptab_attrib);
  notify_format(player,
		T("Renamed %s to %s in attribute table."), old, newname);
  return;
}

/** Display information on an attribute from the table.
 * \verbatim
 * Top-level function for @attribute.
 * \endverbatim
 * \param player the enactor.
 * \param name the name of the attribute.
 */
void
do_attribute_info(dbref player, char *name)
{
  ATTR *ap;

  ap = NULL;

  if (!name || !*name) {
    if(catchall)
      ap = catchall;
    else {
      notify(player, T("Which attribute do you mean?"));
      return;
    }
  }

  /* Is this attribute in the table? */

  if (!ap) {
    if (*name == '@')
      name++;

    ap = aname_hash_lookup(name);
  }

  if (!ap) {
    notify(player, T("That attribute isn't in the attribute table"));
    return;
  }
  notify_format(player, "Attribute: %s", ap == catchall ? "Default Attribute" : AL_NAME(ap));
  notify_format(player,
		"    Flags: %s", privs_to_string(attr_privs_view,
						 AL_FLAGS(ap)));
  notify_format(player, "  Creator: %s", unparse_dbref(AL_CREATOR(ap)));
  /* Now Locks */
    if(!(AL_RLock(ap) == TRUE_BOOLEXP))
      notify_format(player, "Readlock: %s", unparse_boolexp(player, AL_RLock(ap), UB_ALL ));
    if(!(AL_WLock(ap) == TRUE_BOOLEXP))
      notify_format(player, "Writelock: %s", unparse_boolexp(player, AL_WLock(ap), UB_ALL ));
  return;
}

/** Display a list of standard attributes.
 * \verbatim
 * Top-level function for @list/attribs.
 * \endverbatim
 * \param player the enactor.
 * \param lc if true, display the list in lowercase; otherwise uppercase.
 */
void
do_list_attribs(dbref player, int lc)
{
  char *b = list_attribs();
  notify_format(player, "Attribs: %s", lc ? strlower(b) : b);
}

/** Return a list of standard attributes.
 * This functions returns the list of standard attributes, separated by
 * spaces, in a statically allocated buffer.
 */
char *
list_attribs(void)
{
  ATTR *ap;
  const char *ptrs[BUFFER_LEN / 2];
  static char buff[BUFFER_LEN];
  char *bp;
  int nptrs = 0, i;

  ap = (ATTR *) ptab_firstentry(&ptab_attrib);
  while (ap) {
    ptrs[nptrs++] = AL_NAME(ap);
    ap = (ATTR *) ptab_nextentry(&ptab_attrib);
  }
  bp = buff;
  safe_str(ptrs[0], buff, &bp);
  for (i = 1; i < nptrs; i++) {
    safe_chr(' ', buff, &bp);
    safe_str(ptrs[i], buff, &bp);
  }
  *bp = '\0';
  return buff;
}

/** Attr things to be done after the config file is loaded but before
 * objects are restarted.
 */
void
attr_init_postconfig(void)
{
  ATTR *a;
  /* read_remote_desc affects AF_NEARBY flag on DESCRIBE attribute */
  a = aname_hash_lookup("DESCRIBE");
  if (READ_REMOTE_DESC)
    a->flags &= ~AF_NEARBY;
  else
    a->flags |= AF_NEARBY;
}
