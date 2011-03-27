/**
 * \file attrib.c
 *
 * \brief Manipulate attributes on objects.
 *
 *
 */
#include "copyrite.h"

#include "config.h"
#include <string.h>
#include <ctype.h>
#include "conf.h"
#include "externs.h"
#include "chunk.h"
#include "attrib.h"
#include "dbdefs.h"
#include "match.h"
#include "parse.h"
#include "htab.h"
#include "privtab.h"
#include "mymalloc.h"
#include "strtree.h"
#include "flags.h"
#include "mushdb.h"
#include "lock.h"
#include "log.h"
#include "confmagic.h"

#ifdef WIN32
#pragma warning( disable : 4761)        /* disable warning re conversion */
#endif

/* Catchall Attribute any non-standard attributes will conform to this */
ATTR *catchall;

/** A string tree of attribute names in use, to save us memory since
 * many are duplicated.
 */
StrTree atr_names;
/** Table of attribute flags. */
extern PRIV attr_privs_set[];
extern PRIV attr_privs_view[];
dbref atr_on_obj = NOTHING;

static int real_atr_clr(dbref thinking, char const *atr, dbref player,
                        int we_are_wiping);


dbref global_parent_depth[] = { 0, 0 };

/** A string to hold the name of a missing prefix branch, set by
 * can_write_attr_internal.  Again, gross and ugly.  Please fix.
 */
static char missing_name[ATTRIBUTE_NAME_LIMIT + 1];

/*======================================================================*/

/** How many attributes go in a "page" of attribute memory? */
#define ATTRS_PER_PAGE (200)

/** A page of memory for attributes.
 * This structure is a collection of attribute memory. Rather than
 * allocate new attributes one at a time, we allocate them in pages,
 * and build a linked free list from the allocated page.
 */
typedef struct atrpage {
  ATTR atrs[ATTRS_PER_PAGE];    /**< Array of attribute structures */
} ATTRPAGE;

static int atr_clear_children(dbref player, dbref thing, ATTR *root);
static ATTR *atr_free_list = NULL;
static ATTR *alloc_atr(void);
static ATTR *pop_free_list(void);
static void push_free_list(ATTR *);
static void atr_free_one(ATTR *);
static ATTR *find_atr_pos_in_list(ATTR *** pos, char const *name);
static atr_err can_create_attr(dbref player, dbref obj, char const *atr_name,
			       unsigned int flags);
static int can_create_attr(dbref player, dbref obj, char const *atr_name,
                           unsigned int flags);
static ATTR *find_atr_in_list(ATTR * atr, char const *name);

/** Utility define for can_write_attr_internal and can_create_attr.
 * \param p the player trying to write
 * \param a the attribute to be written
 * \param o the object trying to write attribute to
 * \param s obey the safe flag?
 * \param n do non-standard check
 * \param lo lock owner
 */

/* (!n || !catchall || (AL_WLock(catchall) == TRUE_BOOLEXP))
 */

/*
#define Cannot_Write_This_Attr(p,a,o,s,n) (!God(p) && AF_Internal(a) || \
	      (s && AF_Safe(a)) || \
	    ( ((AL_FLAGS(a) & AF_PRIVILEGE) && !(Prived(p) || (Inherit_Powers(p) && Prived(Owner(p)) )))   ||  \
	      !(  (controls(p, o) && ( (Owner(o) == Owner(lo)) || controls(p,lo)) && catchall && AL_WLock(a) == TRUE_BOOLEXP ? \
		      AL_CREATOR(catchall) : AL_CREATOR(a)))) ||   ((AL_WLock(a) == TRUE_BOOLEXP && (!n || !catchall || \
		      (AL_WLock(catchall) == TRUE_BOOLEXP))) && controls(p,o) ) ||   ((AL_WLock(a) != TRUE_BOOLEXP ? \
		       eval_boolexp(p, AL_WLock(a), o, NULL) : (n && catchall && AL_WLock(catchall) != TRUE_BOOLEXP && \
						  eval_boolexp(p, AL_WLock(catchall), o, NULL))) )) 
						  */


/*
#define Cannot_Write_This_Attr(p,a,o,s,n,lo) (!God(p) && AF_Internal(a) || \
              (s && AF_Safe(a)) || \
            ( ((AL_FLAGS(a) & AF_PRIVILEGE) && !(Prived(p) || (Inherit_Powers(p) && Prived(Owner(p)) )))   ||  \
              !(  (controls(p, o) && ( (Owner(o) == Owner(lo)) || controls(p,lo))) ||  \
                (AL_WLock(a) == TRUE_BOOLEXP && controls(p,o) ) ||  \
               (AL_WLock(a) != TRUE_BOOLEXP && eval_boolexp(p, AL_WLock(a), o, NULL) )) ))

*/
#define Cannot_Write_This_Attr(p,a,o,s,n,lo) cannot_write_this_attr_internal(p,a,o,s,n,lo)



/* player - player trying to write
 * attr        - attribute trying to write
 * obj         - object trying to be written to
 * safe        - obey safe flag
 * ns_chk      - do non-standard check
 * lock_owner  - lock owner
 **/

int cannot_write_this_attr_internal(dbref player, ATTR *attr, dbref obj, char safe, char ns_chk, dbref lowner) {
  dbref lock_owner;

  if(!GoodObject(lowner))
    lock_owner = AL_CREATOR(attr);
  else
    lock_owner = lowner;

  if(God(player))
      return 0;

  if(AF_Internal(attr))
    return 1;

  if((AL_FLAGS(attr) & AF_PRIVILEGE) && !(Prived(player) || (Inherit_Powers(player) && Prived(Owner(player)))))
    return 1;

  if(AF_Mdark(attr) && !(Admin(player)
			 || (Inherit_Powers(player) && Admin(Owner(player)))))
    return 1;

  if(ns_chk && catchall && !controls(player, AL_CREATOR(catchall)) && AL_WLock(catchall) != TRUE_BOOLEXP && 
      !eval_boolexp(player, AL_WLock(catchall), obj, NULL))
    return 1;
 
  if(!controls(player, lock_owner) && AL_WLock(attr) != TRUE_BOOLEXP && !eval_boolexp(player, AL_WLock(attr), obj, NULL))
    return 1;

  /* As well if no lock is set.. and they don't controlt he player in the first place.. then they obviously can't.. */
  if(AL_WLock(attr) == TRUE_BOOLEXP && !controls(player, obj))
    return 1;

  return 0;
}
/*======================================================================*/

/** Initialize the attribute string tree.
 */
void
init_atr_name_tree(void)
{
  st_init(&atr_names);
}

/** Lookup table for good_atr_name */
extern char atr_name_table[UCHAR_MAX + 1];

/** Decide if a name is valid for an attribute.
 * A good attribute name is at least one character long, no more than
 * ATTRIBUTE_NAME_LIMIT characters long, and every character is a 
 * valid character. An attribute name may not start or end with a backtick.
 * An attribute name may not contain multiple consecutive backticks.
 * \param s a string to test for validity as an attribute name.
 */
int
good_atr_name(char const *s)
{
  const unsigned char *a;
  int len = 0;
  if (!s || !*s)
    return 0;
  if (*s == '`')
    return 0;
  if (strstr(s, "``"))
    return 0;
  for (a = (const unsigned char *) s; *a; a++, len++)
    if (!atr_name_table[*a])
      return 0;
  if (*(s + len - 1) == '`')
    return 0;
  return len <= ATTRIBUTE_NAME_LIMIT;
}

/** Find an attribute table entry, given a name.
 * A trivial wrapper around aname_hash_lookup.
 * \param string an attribute name.
 */
ATTR *
atr_match(const char *string)
{
  return aname_hash_lookup(string);
}

/** Find the first attribute branching off the specified attribute.
 * \param branch the attribute to look under
 */
ATTR *
atr_sub_branch(ATTR * branch)
{
  char const *name, *n2;
  size_t len;

  name = AL_NAME(branch);
  len = strlen(name);
  for (branch = AL_NEXT(branch); branch; branch = AL_NEXT(branch)) {
    n2 = AL_NAME(branch);
    if (strlen(n2) <= len)
      return NULL;
    if (n2[len] == '`') {
      if (!strncmp(n2, name, len))
        return branch;
      else
        return NULL;
    }
  }
  return NULL;
}

/** Scan an attribute list for an attribute with the specified name.
 * This continues from whatever start point is passed in.
 * \param atr the start of the list to search from
 * \param name the attribute name to look for
 * \return the matching attribute, or NULL
 */
static ATTR *
find_atr_in_list(ATTR * atr, char const *name)
{
  int comp;

  while (atr) {
    comp = strcoll(name, AL_NAME(atr));
    if (comp < 0)
      return NULL;
    if (comp == 0)
      return atr;
    atr = AL_NEXT(atr);
  }

  return NULL;
}

/** Find the place to insert an attribute with the specified name.
 * \param pos a pointer to the ATTR ** holding the list position
 * \param name the attribute name to look for
 * \return the matching attribute, or NULL if no matching attribute
 */
static ATTR *
find_atr_pos_in_list(ATTR *** pos, char const *name)
{
  int comp;

  while (**pos) {
    comp = strcoll(name, AL_NAME(**pos));
    if (comp < 0)
      return NULL;
    if (comp == 0)
      return **pos;
    *pos = &AL_NEXT(**pos);
  }

  return NULL;
}

/** Convert a string of attribute flags to a bitmask.
 * Given a space-separated string of attribute flags, look them up
 * and return a bitmask of them if player is permitted to use
 * all of those flags.
 * \param player the dbref to use for privilege checks.
 * \param p a space-separated string of attribute flags.
 * \return an attribute flag bitmask.
 */
int
string_to_atrflag(dbref player, char const *p)
{
  int f;
  f = string_to_privs(attr_privs_set, p, 0);
  if (!f)
    return -1;
  if (!Admin(player) && (f & AF_MDARK))
    return -1;
  if (!div_powover(player, player, "Privilege") && (f & AF_PRIVILEGE))
    return -1;
  f &= ~AF_INTERNAL;
  return f;
}

/** Convert a string of attribute flags to a pair of bitmasks.
 * Given a space-separated string of attribute flags, look them up
 * and return bitmasks of those to set or clear
 * if player is permitted to use all of those flags.
 * \param player the dbref to use for privilege checks.
 * \param p a space-separated string of attribute flags.
 * \param setbits pointer to address of bitmask to set.
 * \param clrbits pointer to address of bitmask to clear.
 * \return setbits or -1 on error.
 */
int
string_to_atrflagsets(dbref player, char const *p, int *setbits, int *clrbits)
{
  int f;
  *setbits = *clrbits = 0;
  f = string_to_privsets(attr_privs_set, p, setbits, clrbits);
  if (f <= 0)
    return -1;
  if (!Prived(player) && ((*setbits & AF_MDARK) || (*clrbits & AF_MDARK)))
    return -1;
  if (!See_All(player) && ((*setbits & AF_PRIVILEGE) || (*clrbits & AF_PRIVILEGE)))
    return -1;
  f &= ~AF_INTERNAL;
  return *setbits;
}

/** Convert an attribute flag bitmask into a list of the full
 * names of the flags.
 * \param mask the bitmask of attribute flags to display.
 * \return a pointer to a static buffer with the full names of the flags.
 */
const char *
atrflag_to_string(int mask)
{
  return privs_to_string(attr_privs_view, mask);
}

/** Utility define for atr_add and can_create_attr */
#define set_default_flags(atr,flags, lock_owner, ns_chk) \
  do { \
    ATTR *std = atr_match(AL_NAME((atr))); \
                                           \
    if (std && !strcmp(AL_NAME(std), AL_NAME((atr)))) { \
      AL_FLAGS(atr) = AL_FLAGS(std); \
      AL_WLock(atr) = dup_bool(AL_WLock(std)); \
      AL_RLock(atr) = dup_bool(AL_RLock(std)); \
      lock_owner = AL_CREATOR(std); \
      ns_chk = 0; \
    } else { \
      AL_FLAGS(atr) = flags; \
      AL_WLock(atr) = AL_RLock(atr) = TRUE_BOOLEXP; \
      lock_owner = NOTHING; \
      ns_chk = 1; \
    } \
  } while (0)

#define free_atr_locks(atr) \
    free_boolexp(AL_WLock(atr)); \
    free_boolexp(AL_RLock(atr)); 

/** Can an attribute of specified name be created?
 * This function determines if an attribute can be created by examining
 * the tree path to the attribute, and the standard attribute flags for
 * those parts of the path that don't exist yet.
 * \param player the player trying to do the write.
 * \param obj the object targetted for the write.
 * \param atr the attribute being interrogated.
 * \param flags the default flags to add to the attribute. 
 *              0 for default flags if it's a builtin attribute.
 * \retval 0 if the player cannot write the attribute.
 * \retval 1 if the player can write the attribute.
 */
static int
can_create_attr(dbref player, dbref obj, char const *atr_name,
                unsigned int flags)
{
  char *p;
  ATTR tmpatr, *atr;
  int ns_chk, num_new = 1;
  dbref lock_owner;
  missing_name[0] = '\0';

  atr = &tmpatr;
  AL_FLAGS(atr) = 0;
  AL_CREATOR(atr) = ooref != NOTHING ? ooref : player;
  AL_NAME(atr) = atr_name;

  set_default_flags(atr, flags, lock_owner, ns_chk);
  if(lock_owner == NOTHING)
    lock_owner = AL_CREATOR(atr);

  if (Cannot_Write_This_Attr(player, atr, obj, 1, ns_chk, lock_owner)) {
    free_atr_locks(atr);
    return AE_ERROR;
  }

  free_atr_locks(atr);

  strcpy(missing_name, atr_name);
  atr = List(obj);
  for (p = strchr(missing_name, '`'); p; p = strchr(p + 1, '`')) {
    *p = '\0';
    if (atr != &tmpatr)
      atr = find_atr_in_list(atr, missing_name);
    if (!atr) {
      atr = &tmpatr;
      AL_CREATOR(atr) = ooref != NOTHING ? Owner(ooref) : Owner(player);
    }
    if (atr == &tmpatr) {
      AL_NAME(atr) = missing_name;
      set_default_flags(atr, flags, lock_owner, ns_chk);
      if(lock_owner == NOTHING)
	lock_owner = AL_CREATOR(atr);
      num_new++;
    }
    /* Only GOD can create an AF_NODUMP attribute (used for semaphores)
     * or add a leaf to a tree with such an attribute
     */
    if ((AL_FLAGS(atr) & AF_NODUMP) && (player != GOD)) {
      missing_name[0] = '\0';
      return AE_ERROR;
    }
    if (Cannot_Write_This_Attr(player, atr, obj, 1, ns_chk, lock_owner)) {
      free_atr_locks(atr);
      missing_name[0] = '\0';
      return AE_ERROR;
    }
    free_atr_locks(atr);
    *p = '`';
  }

  if ((AttrCount(obj) + num_new) >
      (Many_Attribs(obj) ? HARD_MAX_ATTRCOUNT : MAX_ATTRCOUNT)) {
    do_log(LT_ERR, player, obj,
           T("Attempt by %s(%d) to create too many attributes on %s(%d)"),
           Name(player), player, Name(obj), obj);
    return AE_TOOMANY;
  }

  return AE_OKAY;
}

/*======================================================================*/

/** Do the work of creating the attribute entry on an object.
 * This doesn't do any permissions checking.  You should do that yourself.
 * \param thing the object to hold the attribute
 * \param atr_name the name for the attribute
 */
static ATTR *
create_atr(dbref thing, char const *atr_name)
{
  ATTR *ptr, **ins;
  char const *name;

  /* put the name in the string table */
  name = st_insert(atr_name, &atr_names);
  if (!name)
    return NULL;

  /* allocate a new page, if needed */
  ptr = pop_free_list();
  if (ptr == NULL) {
    st_delete(name, &atr_names);
    return NULL;
  }

  /* initialize atr */
  AL_WLock(ptr) = TRUE_BOOLEXP;
  AL_RLock(ptr) = TRUE_BOOLEXP;
  AL_FLAGS(ptr) = 0;
  AL_NAME(ptr) = name;
  ptr->data = NULL_CHUNK_REFERENCE;

  /* link it in */
  ins = &List(thing);
  (void) find_atr_pos_in_list(&ins, AL_NAME(ptr));
  AL_NEXT(ptr) = *ins;
  *ins = ptr;
  AttrCount(thing)++;

  return ptr;
}

/** Add an attribute to an object, dangerously.
 * This is a stripped down version of atr_add, without duplicate checking,
 * permissions checking, attribute count checking, or auto-ODARKing.  
 * If anyone uses this outside of database load or atr_cpy (below), 
 * I will personally string them up by their toes.  - Alex 
 * \param thing object to set the attribute on.
 * \param atr name of the attribute to set.
 * \param s value of the attribute to set.
 * \param player the attribute creator.
 * \param flags bitmask of attribute flags for this attribute.
 * \param derefs the initial deref count to use for the attribute value.
 */
void
atr_new_add(dbref thing, const char *RESTRICT atr, const char *RESTRICT s,
            dbref player, unsigned int flags, unsigned char derefs, boolexp wlock,
            boolexp rlock, time_t modtime)
{
  ATTR *ptr;
  boolexp lock;
  dbref lock_owner;
  int ns_chk;
  char *p, root_name[ATTRIBUTE_NAME_LIMIT + 1];

  if (!EMPTY_ATTRS && !*s)
    return;

  /* Don't fail on a bad name, but do log it */
  if (!good_atr_name(atr))
    do_rawlog(LT_ERR, T("Bad attribute name %s on object %s"), atr,
              unparse_dbref(thing));

  ptr = create_atr(thing, atr);
  if (!ptr)
    return;

  if((lock = dup_bool(wlock))) 
    AL_WLock(ptr) = lock;

  if((lock = dup_bool(rlock))) 
    AL_RLock(ptr) = lock;

  strcpy(root_name, atr);
  if ((p = strrchr(root_name, '`'))) {
    ATTR *root = NULL;
    *p = '\0';
    root = find_atr_in_list(List(thing), root_name);
    if (!root) {
      do_rawlog(LT_ERR, T("Missing root attribute '%s' on object #%d!\n"),
		root_name, thing);
      root = create_atr(thing, root_name);
      set_default_flags(root, 0, lock_owner, ns_chk);
      if(lock_owner == NOTHING)
	lock_owner = player;
      AL_FLAGS(root) |= AF_ROOT;
      AL_CREATOR(root) = player;
      if (!EMPTY_ATTRS) {
	unsigned char *t = compress(" ");
	if (!t) {
	  mush_panic(T("Unable to allocate memory in atr_new_add()!"));
	}
	root->data = chunk_create(t, u_strlen(t), 0);
	free(t);
      }
    } else {
      if (!AL_FLAGS(root) & AF_ROOT)	/* Upgrading old database */
	AL_FLAGS(root) |= AF_ROOT;
    }
  }

  AL_FLAGS(ptr) = flags;
  AL_FLAGS(ptr) &= ~AF_COMMAND & ~AF_LISTEN;
  AL_CREATOR(ptr) = ooref != NOTHING ? ooref : player;

  AL_MODTIME(ptr) = modtime;

  /* replace string with new string */
  if (!s || !*s) {
    /* nothing */
  } else {
    unsigned char *t = compress(s);
    if (!t)
      return;

    ptr->data = chunk_create(t, u_strlen(t), derefs);
    free(t);

    if (*s == '$')
      AL_FLAGS(ptr) |= AF_COMMAND;
    if (*s == '^')
      AL_FLAGS(ptr) |= AF_LISTEN;
  }
}

/** Add an attribute to an object, safely.
 * \verbatim
 * This is the function that should be called in hardcode to add
 * an attribute to an object (but not to process things like @set that
 * may add or clear an attribute - see do_set_atr() for that).
 * \endverbatim
 * \param thing object to set the attribute on.
 * \param atr name of the attribute to set.
 * \param s value of the attribute to set.
 * \param player the attribute creator.
 * \param flags bitmask of attribute flags for this attribute. 0 will use
 *         default.
 * \return AE_OKAY or an AE_* error code.
 */

atr_err
atr_add(dbref thing, const char *RESTRICT atr, const char *RESTRICT s,
        dbref player, unsigned int flags)
{
  ATTR *ptr, *root = NULL;
  dbref lock_owner; /* Not used.. but set in set_default_flags */
  int ns_chk;
  char *p;

  tooref = ooref;
  if (player == GOD)            /* This normally only done internally */
    ooref = NOTHING;

  if (!s || (!EMPTY_ATTRS && !*s)) {
    ooref = tooref;
    return atr_clr(thing, atr, player);
  }

  if (!good_atr_name(atr)) {
    ooref = tooref;
    return AE_BADNAME;
  }

  /* walk the list, looking for a preexisting value */
  ptr = find_atr_in_list(List(thing), atr);

  /* check for permission to modify existing atr */
  if ((ptr && AF_Safe(ptr))) {
    ooref = tooref;
    return AE_SAFE;
  }
  if (ptr && !Can_Write_Attr(player, thing, ptr)) {
    ooref = tooref;
    return AE_ERROR;
  }

  /* make a new atr, if needed */
  if (!ptr) {
    atr_err res = can_create_attr(player, thing, atr, flags);
    if (res != AE_OKAY) {
      ooref = tooref;
      return res;
    }

    strcpy(missing_name, atr);
    ptr = List(thing);
    for (p = strchr(missing_name, '`'); p; p = strchr(p + 1, '`')) {
      *p = '\0';

      root = find_atr_in_list(ptr, missing_name);

      if (!root) {
        root = create_atr(thing, missing_name);
        if (!root) {
          ooref = tooref;
          return AE_TREE;
        }

        /* update modification time here, because from now on,
         * we modify even if we fail */
        if (!IsPlayer(thing) && !AF_Nodump(ptr)) {
          char lmbuf[1024];
          ModTime(thing) = mudtime;
          snprintf(lmbuf, 1023, "%s[#%d]", ptr->name, player);
          lmbuf[strlen(lmbuf) + 1] = '\0';
          set_lmod(thing, lmbuf);
        }

        set_default_flags(root, flags, lock_owner, ns_chk);
        AL_FLAGS(root) &= ~AF_COMMAND & ~AF_LISTEN;
	AL_FLAGS(root) |= AF_ROOT;
        AL_CREATOR(root) = ooref != NOTHING ? Owner(ooref) : Owner(player);
        AL_MODTIME(root) = mudtime;
        if (!EMPTY_ATTRS) {
          unsigned char *t = compress(" ");
          if (!t) {
	    mush_panic(T("Unable to allocate memory in atr_add()!"));
            ooref = tooref;
          }
	  root->data = chunk_create(t, u_strlen(t), 0);
          free(t);
        }
      } else AL_FLAGS(root) |= AF_ROOT;

      *p = '`';
    }

    ptr = create_atr(thing, atr);
    if (!ptr) {
      ooref = tooref;
      return AE_ERROR;
    }

    set_default_flags(ptr, flags, lock_owner, ns_chk);
  }
  /* update modification time here, because from now on,
   * we modify even if we fail */
  if (!IsPlayer(thing) && !AF_Nodump(ptr)) {
    char lmbuf[1024];
    ModTime(thing) = mudtime;
    snprintf(lmbuf, 1023, "%s[#%d]", ptr->name, player);
    lmbuf[strlen(lmbuf) + 1] = '\0';
    set_lmod(thing, lmbuf);
  }

  /* change owner */
  AL_CREATOR(ptr) = Owner(player);

  AL_FLAGS(ptr) &= ~AF_COMMAND & ~AF_LISTEN;
  AL_MODTIME(ptr) = mudtime;

  /* replace string with new string */
  if (ptr->data)
    chunk_delete(ptr->data);
  if (!s || !*s) {
    ptr->data = NULL_CHUNK_REFERENCE;
  } else {
    unsigned char *t = compress(s);
    if (!t) {
      ptr->data = NULL_CHUNK_REFERENCE;
      ooref = tooref;
      return AE_ERROR;
    }
    ptr->data = chunk_create(t, u_strlen(t), 0);
    free(t);

    if (*s == '$')
      AL_FLAGS(ptr) |= AF_COMMAND;
    if (*s == '^')
      AL_FLAGS(ptr) |= AF_LISTEN;
  }

  ooref = tooref;
  return 1;
}

/** Remove an attribute from an object.
 * This function clears an attribute from an object. 
 * Permission is denied if the attribute is a branch, not a leaf.
 * \param thing object to clear attribute from.
 * \param atr name of attribute to remove.
 * \param player enactor attempting to remove attribute.
 * \param we_are_wiping true if called by \@wipe.
 * \return AE_OKAY or AE_* error code.
 */
static atr_err
real_atr_clr(dbref thing, char const *atr, dbref player, int we_are_wiping)
{
  ATTR *ptr, **prev;
  int can_clear = 1;

  tooref = ooref;
  if (player == GOD)
    ooref = NOTHING;

  prev = &List(thing);
  ptr = find_atr_pos_in_list(&prev, atr);

  if (!ptr) {
    ooref = tooref;
    return AE_NOTFOUND;
  }

  if (ptr && AF_Safe(ptr)) {
    ooref = tooref;
    return AE_SAFE;
  }
  if (!Can_Write_Attr(player, thing, ptr)) {
    ooref = tooref;
    return AE_ERROR;
  }

  if ((AL_FLAGS(ptr) & AF_ROOT) && !we_are_wiping) {
    ooref = tooref;
    return AE_TREE;
  }

  /* We only hit this if wiping. */
  if (AL_FLAGS(ptr) & AF_ROOT)
    can_clear = atr_clear_children(player, thing, ptr);

  if (can_clear) {
    char *p;
    char root_name[ATTRIBUTE_NAME_LIMIT + 1];

    strcpy(root_name, AL_NAME(ptr));

   if (!IsPlayer(thing) && !AF_Nodump(ptr))
     ModTime(thing) = mudtime;
   *prev = AL_NEXT(ptr);
   atr_free_one(ptr);
   AttrCount(thing)--;
 
   /* If this was the only leaf of a tree, clear the AF_ROOT flag from
    * the parent. */
   if ((p = strrchr(root_name, '`'))) {
     ATTR *root;
     *p = '\0';
  
     root = find_atr_in_list(List(thing), root_name);
     *p = '`';
  
     if (!root) {
	do_rawlog(LT_ERR, T("Attribute %s on object #%d lacks a parent!"),
		  root_name, thing);
     } else {
	if (!atr_sub_branch(root))
	  AL_FLAGS(root) &= ~AF_ROOT;
     }
   }
  
   return AE_OKAY;
 } else
   return AE_TREE;
}

/* Remove an attribute from an object.
 * This function clears an attribute from an object. 
 * Permission is denied if the attribute is a branch, not a leaf.
 * \param thing object to clear attribute from.
 * \param atr name of attribute to remove.
 * \param player enactor attempting to remove attribute.
 * \return AE_OKAY or an AE_* error code
 */
atr_err
atr_clr(dbref thing, char const *atr, dbref player)
{
 return real_atr_clr(thing, atr, player, 0);
}
  
  
/* \@wipe an attribute (And any leaves) from an object.
 * This function clears an attribute from an object. 
 * \param thing object to clear attribute from.
 * \param atr name of attribute to remove.
 * \param player enactor attempting to remove attribute.
 * \return AE_OKAY or an AE_* error code.
 */
atr_err
wipe_atr(dbref thing, char const *atr, dbref player)
{
 return real_atr_clr(thing, atr, player, 1);
}
 
 /** Retrieve an attribute from an object or its ancestors.
 * This function retrieves an attribute from an object, or from its
 * parent chain, returning a pointer to the first attribute that
 * matches or NULL. This is a pointer to an attribute structure, not
 * to the value of the attribute, so the value is usually accessed
 * through atr_value() or safe_atr_value().
 * \param obj the object containing the attribute.
 * \param atrname the name of the attribute.
 * \return pointer to the attribute structure retrieved, or NULL.
 */
ATTR *
atr_get(dbref obj, char const *atrname)
{
  static char name[ATTRIBUTE_NAME_LIMIT + 1];
  char *p;
  ATTR *atr;
  int parent_depth;
  dbref target;
  dbref ancestor;

  global_parent_depth[0] = 0;
  global_parent_depth[1] = NOTHING;
  if (obj == NOTHING || !good_atr_name(atrname))
    return NULL;

  /* First try given name, then try alias match. */
  strcpy(name, atrname);
  for (;;) {
    /* Hunt through the parents/ancestor chain... */
    ancestor = Ancestor_Parent(obj);
    target = obj;
    parent_depth = 0;
    while (parent_depth < MAX_PARENTS && GoodObject(target)) {
      /* If the ancestor of the object is in its explict parent chain,
       * we use it there, and don't check the ancestor later.
       */
      if (target == ancestor)
        ancestor = NOTHING;
      atr = List(target);

      /* If we're looking at a parent/ancestor, then we
       * need to check the branch path for privacy... */
      if (target != obj) {
        for (p = strchr(name, '`'); p; p = strchr(p + 1, '`')) {
          *p = '\0';
          atr = find_atr_in_list(atr, name);
          if (!atr || AF_Private(atr)) {
            *p = '`';
            goto continue_target;
          }
          *p = '`';
        }
      }

      /* Now actually find the attribute. */
      atr = find_atr_in_list(atr, name);
      global_parent_depth[1] = atr_on_obj = target;
      if (atr && (target == obj || !AF_Private(atr))) {
        global_parent_depth[0] = parent_depth;
        return atr;
      }

    continue_target:
      /* Attribute wasn't on this object.  Check a parent or ancestor. */
      parent_depth++;
      target = Parent(target);
      if (!GoodObject(target)) {
        parent_depth = 0;
        target = ancestor;
      }
    }

    /* Try the alias, too... */
    atr = atr_match(atrname);
    if (!atr || !strcmp(name, AL_NAME(atr)))
      break;
    strcpy(name, AL_NAME(atr));
  }

  global_parent_depth[0] = parent_depth;
  return NULL;
}

/** Retrieve an attribute from an object.
 * This function retrieves an attribute from an object, and does not
 * check the parent chain. It returns a pointer to the attribute
 * or NULL.  This is a pointer to an attribute structure, not
 * to the value of the attribute, so the (compressed) value is usually 
 * to the value of the attribute, so the value is usually accessed
 * through atr_value() or safe_atr_value().
 * \param thing the object containing the attribute.
 * \param atr the name of the attribute.
 * \return pointer to the attribute structure retrieved, or NULL.
 */
ATTR *
atr_get_noparent(dbref thing, char const *atr)
{
  ATTR *ptr;

  if (thing == NOTHING || !good_atr_name(atr))
    return NULL;

  /* try real name */
  ptr = find_atr_in_list(List(thing), atr);
  if (ptr)
    return ptr;

  ptr = atr_match(atr);
  if (!ptr || !strcmp(atr, AL_NAME(ptr)))
    return NULL;
  atr = AL_NAME(ptr);

  /* try alias */
  ptr = find_atr_in_list(List(thing), atr);
  if (ptr)
    return ptr;

  return NULL;
}


/** Apply a function to a set of attributes.
 * This function applies another function to a set of attributes on an
 * object specified by a (wildcarded) pattern to match against the
 * attribute name.
 * \param player the enactor.
 * \param thing the object containing the attribute.
 * \param name the pattern to match against the attribute name.
 * \param mortal only fetch mortal-visible attributes?
 * \param func the function to call for each matching attribute.
 * \param args additional arguments to pass to the function.
 * \return the sum of the return values of the functions called.
 */
int
atr_iter_get(dbref player, dbref thing, const char *name, int mortal,
             aig_func func, void *args)
{
  ATTR *ptr, **indirect;
  int result;
  int len;

  result = 0;
  if (!name || !*name)
    name = "*";
  len = strlen(name);

  if (!wildcard(name) && name[len - 1] != '`') {
    ptr = atr_get_noparent(thing, strupper(name));
    if (ptr && (mortal ? Is_Visible_Attr(thing, ptr)
                : Can_Read_Attr(player, thing, ptr)))
      result = func(player, thing, NOTHING, name, ptr, args);
  } else {
    indirect = &List(thing);
    while (*indirect) {
      ptr = *indirect;
      if ((mortal ? Is_Visible_Attr(thing, ptr)
           : Can_Read_Attr(player, thing, ptr))
          && atr_wild(name, AL_NAME(ptr)))
        result += func(player, thing, NOTHING, name, ptr, args);
      if (ptr == *indirect)
        indirect = &AL_NEXT(ptr);
    }
  }

  return result;
}

/** Free the memory associated with all attributes of an object.
 * This function frees all of an object's attribute memory.
 * This includes the memory allocated to hold the attribute's value,
 * and the attribute's entry in the object's string tree.
 * Freed attribute structures are added to the free list.
 * \param thing dbref of object
 */
void
atr_free_all(dbref thing)
{
  ATTR *ptr;

  if (!List(thing))
    return;

  if (!IsPlayer(thing)) {
    char lmbuf[1024];
    ModTime(thing) = mudtime;
    snprintf(lmbuf, 1023, "ATRWIPE[#%d]", thing);
    lmbuf[strlen(lmbuf) + 1] = '\0';
    set_lmod(thing, lmbuf);
  }

  while ((ptr = List(thing))) {
    List(thing) = AL_NEXT(ptr);
    atr_free_one(ptr);
  }
}

/** Copy all of the attributes from one object to another.
 * \verbatim
 * This function is used by @clone to copy all of the attributes
 * from one object to another.
 * \endverbatim
 * \param dest destination object to receive attributes.
 * \param source source object containing attributes.
 */
void
atr_cpy(dbref dest, dbref source)
{
  ATTR *ptr;
  int max_attrs;

  max_attrs = (Many_Attribs(dest) ? HARD_MAX_ATTRCOUNT : MAX_ATTRCOUNT);
  List(dest) = NULL;
  for (ptr = List(source); ptr; ptr = AL_NEXT(ptr))
    if (!AF_Nocopy(ptr)
        && (AttrCount(dest) < max_attrs)) {
      atr_new_add(dest, AL_NAME(ptr), atr_value(ptr),
                  AL_CREATOR(ptr), AL_FLAGS(ptr), AL_DEREFS(ptr),
                  AL_WLock(ptr), AL_RLock(ptr), AL_MODTIME(ptr));
      AttrCount(dest)++;
    }
}

/** Structure for keeping track of which attributes have appeared
 * on children when doing command matching. */
typedef struct used_attr {
  struct used_attr *next;       /**< Next attribute in list */
  char const *name;             /**< The name of the attribute */
  int no_prog;                  /**< Was it AF_NOPROG */
} UsedAttr;

/** Find an attribute in the list of seen attributes.
 * Since attributes are checked in collation order, the pointer to the
 * list is updated to reflect the current search position.
 * For efficiency of insertions, the pointer used is a trailing pointer,
 * pointing at the pointer to the next used struct.
 * To allow a useful return code, the pointer used is actually a pointer
 * to the pointer mentioned above.  Yes, I know three-star coding is bad,
 * but I have good reason, here.
 * \param prev the pointer to the pointer to the pointer to the next
 *             used attribute.
 * \param name the name of the attribute to look for.
 * \retval 0 the attribute was not in the list,
 *           **prev now points to the next atfer.
 * \retval 1 the attribute was in the list,
 *           **prev now points to the entry for it.
 */
static int
find_attr(UsedAttr *** prev, char const *name)
{
  int comp;

  comp = 1;
  while (**prev) {
    comp = strcoll(name, prev[0][0]->name);
    if (comp <= 0)
      break;
    *prev = &prev[0][0]->next;
  }
  return comp == 0;
}

/** Insert an attribute in the list of seen attributes.
 * Since attributes are inserted in collation order, an updated insertion
 * point is returned (so subsequent calls don't have to go hunting as far).
 * \param prev the pointer to the pointer to the attribute list.
 * \param name the name of the attribute to insert.
 * \param no_prog the AF_NOPROG value from the attribute.
 * \return the pointer to the pointer to the next attribute after
 *         the one inserted.
 */
static UsedAttr **
use_attr(UsedAttr ** prev, char const *name, int no_prog)
{
  int found;
  UsedAttr *used;

  found = find_attr(&prev, name);
  if (!found) {
    used = mush_malloc(sizeof *used, "used_attr");
    used->next = *prev;
    used->name = name;
    used->no_prog = 0;
    *prev = used;
  }
  prev[0]->no_prog |= no_prog;
  /* do_rawlog(LT_TRACE, "Recorded %s: %d -> %d", name,
     no_prog, prev[0]->no_prog); */
  return &prev[0]->next;
}

/** Match input against a $command or ^listen attribute.
 * This function attempts to match a string against either the $commands
 * or ^listens on an object. Matches may be glob or regex matches, 
 * depending on the attribute's flags. With the reasonably safe assumption
 * that most of the matches are going to fail, the faster non-capturing
 * glob match is done first, and the capturing version only called when
 * we already know it'll match. Due to the way PCRE works, there's no
 * advantage to doing something similar for regular expression matches.
 * \param thing object containing attributes to check.
 * \param player the enactor, for privilege checks.
 * \param type either '$' or '^', indicating the type of attribute to check.
 * \param end character that denotes the end of a command (usually ':').
 * \param str string to match against attributes.
 * \param just_match if true, return match without executing code.
 * \param atrname used to return the list of matching object/attributes.
 * \param abp pointer to end of atrname.
 * \param errobj if an attribute matches, but the lock fails, this pointer
 *        is used to return the failing dbref. If NULL, we don't bother.
 * \return number of attributes that matched, or 0
 */
int
atr_comm_match(dbref thing, dbref player, int type, int end,
               char const *str, int just_match, char *atrname, char **abp,
               dbref * errobj)
{
  int flag_mask;
  ATTR *ptr;
  int parent_depth;
  char tbuf1[BUFFER_LEN];
  char tbuf2[BUFFER_LEN];
  char *s;
  int match, match_found;
  dbref parent;
  UsedAttr *used_list, **prev;
  ATTR *skip[ATTRIBUTE_NAME_LIMIT / 2];
  int skipcount;
  int lock_checked = 0;
  char match_space[BUFFER_LEN * 2];
  int match_space_len = BUFFER_LEN * 2;
  dbref local_ooref;

  /* check for lots of easy ways out */
  if ((type != '$' && type != '^') || !GoodObject(thing) || Halted(thing)
      || (type == '$' && NoCommand(thing)))
    return 0;

  if (type == '$') {
    flag_mask = AF_COMMAND;
    parent_depth = GoodObject(Parent(thing));
  } else {
    flag_mask = AF_LISTEN;
    if (ThingInhearit(thing) || RoomInhearit(thing)) {
      parent_depth = GoodObject(Parent(thing));
    } else {
      parent_depth = 0;
    }
  }

  match = 0;
  used_list = NULL;
  prev = &used_list;

  skipcount = 0;
  /* do_rawlog(LT_TRACE, "Searching %s:", Name(thing)); */
  for (ptr = List(thing); ptr; ptr = AL_NEXT(ptr)) {
    if (skipcount && ptr == skip[skipcount - 1]) {
      size_t len = strrchr(AL_NAME(ptr), '`') - AL_NAME(ptr);
      while (AL_NEXT(ptr) && strlen(AL_NAME(AL_NEXT(ptr))) > len &&
             AL_NAME(AL_NEXT(ptr))[len] == '`') {
        ptr = AL_NEXT(ptr);
        /* do_rawlog(LT_TRACE, "  Skipping %s", AL_NAME(ptr)); */
      }
      skipcount--;
      continue;
    }
    if (parent_depth)
      prev = use_attr(prev, AL_NAME(ptr), AF_Noprog(ptr));
    if (AF_Noprog(ptr)) {
      skip[skipcount] = atr_sub_branch(ptr);
      if (skip[skipcount])
        skipcount++;
      continue;
    }
    if (!(AL_FLAGS(ptr) & flag_mask))
      continue;
    strcpy(tbuf1, atr_value(ptr));
    s = tbuf1;
    do {
      s = strchr(s + 1, end);
    } while (s && s[-1] == '\\');
    if (!s)
      continue;
    *s++ = '\0';
    if (type == '^' && !AF_Ahear(ptr)) {
      if ((thing == player && !AF_Mhear(ptr))
	  || (thing != player && AF_Mhear(ptr)))
	continue;
    }

    if (AF_Regexp(ptr)) {
      /* Turn \: into : */
      char *from, *to;
      for (from = tbuf1, to = tbuf2; *from; from++, to++) {
        if (*from == '\\' && *(from + 1) == ':')
          from++;
        *to = *from;
      }
      *to = '\0';
    } else
      strcpy(tbuf2, tbuf1);

    match_found = 0;
    if (AF_Regexp(ptr)) {
      if (regexp_match_case_r(tbuf2 + 1, str, AF_Case(ptr),
			      global_eval_context.wnxt, 10,
			      match_space, match_space_len)) {
        match_found = 1;
        match++;
      }
    } else {
      if (quick_wild_new(tbuf2 + 1, str, AF_Case(ptr))) {
        match_found = 1;
        match++;
	if (!just_match)
	  wild_match_case_r(tbuf2 + 1, str, AF_Case(ptr),
			    global_eval_context.wnxt, 10,
			    match_space, match_space_len);
      }
    }
    if (match_found) {
      /* We only want to do the lock check once, so that any side
       * effects in the lock are only performed once per utterance.
       * Thus, '$foo *r:' and '$foo b*:' on the same object will only
       * run the lock once for 'foo bar'.
       */
      if (!lock_checked) {
        lock_checked = 1;
        if ((type == '$' && !eval_lock(player, thing, Command_Lock))
            || (type == '^' && !eval_lock(player, thing, Listen_Lock))
            || !eval_lock(player, thing, Use_Lock)) {
          match--;
          if (errobj)
            *errobj = thing;
          /* If we failed the lock, there's no point in continuing at all. */
          goto exit_sequence;
        }
      }
      if (atrname && abp) {
        safe_chr(' ', atrname, abp);
        safe_dbref(thing, atrname, abp);
        safe_chr('/', atrname, abp);
        safe_str(AL_NAME(ptr), atrname, abp);
      }
      if (!just_match) {
        local_ooref = ooref;
        ooref = AL_CREATOR(ptr);
        parse_que(thing, s, player);
        ooref = local_ooref;
      }
    }
  }

  /* Don't need to free used_list here, because if !parent_depth,
   * we would never have allocated it. */
  if (!parent_depth)
    return match;

  for (parent_depth = MAX_PARENTS, parent = Parent(thing);
       parent_depth-- && parent != NOTHING; parent = Parent(parent)) {
    /* do_rawlog(LT_TRACE, "Searching %s:", Name(parent)); */
    skipcount = 0;
    prev = &used_list;
    for (ptr = List(parent); ptr; ptr = AL_NEXT(ptr)) {
      if (skipcount && ptr == skip[skipcount - 1]) {
        size_t len = strrchr(AL_NAME(ptr), '`') - AL_NAME(ptr);
        while (AL_NEXT(ptr) && strlen(AL_NAME(AL_NEXT(ptr))) > len &&
               AL_NAME(AL_NEXT(ptr))[len] == '`') {
          ptr = AL_NEXT(ptr);
          /* do_rawlog(LT_TRACE, "  Skipping %s", AL_NAME(ptr)); */
        }
        skipcount--;
        continue;
      }
      if (AF_Private(ptr)) {
        /* do_rawlog(LT_TRACE, "Private %s:", AL_NAME(ptr)); */
        skip[skipcount] = atr_sub_branch(ptr);
        if (skip[skipcount])
          skipcount++;
        continue;
      }
      if (find_attr(&prev, AL_NAME(ptr))) {
        /* do_rawlog(LT_TRACE, "Found %s:", AL_NAME(ptr)); */
        if (prev[0]->no_prog || AF_Noprog(ptr)) {
          skip[skipcount] = atr_sub_branch(ptr);
          if (skip[skipcount])
            skipcount++;
          prev[0]->no_prog = AF_NOPROG;
        }
        continue;
      }
      if (GoodObject(Parent(parent)))
        prev = use_attr(prev, AL_NAME(ptr), AF_Noprog(ptr));
      if (AF_Noprog(ptr)) {
        /* do_rawlog(LT_TRACE, "NoProg %s:", AL_NAME(ptr)); */
        skip[skipcount] = atr_sub_branch(ptr);
        if (skip[skipcount])
          skipcount++;
        continue;
      }
      if (!(AL_FLAGS(ptr) & flag_mask))
        continue;
      strcpy(tbuf1, atr_value(ptr));
      s = tbuf1;
      do {
        s = strchr(s + 1, end);
      } while (s && s[-1] == '\\');
      if (!s)
        continue;
      *s++ = '\0';
      if (type == '^' && !AF_Ahear(ptr)) {
	if ((thing == player && !AF_Mhear(ptr))
	    || (thing != player && AF_Mhear(ptr)))
	  continue;
      }

      if (AF_Regexp(ptr)) {
        /* Turn \: into : */
        char *from, *to;
        for (from = tbuf1, to = tbuf2; *from; from++, to++) {
          if (*from == '\\' && *(from + 1) == ':')
            from++;
          *to = *from;
        }
        *to = '\0';
      } else
        strcpy(tbuf2, tbuf1);

      match_found = 0;
      if (AF_Regexp(ptr)) {
	if (regexp_match_case_r(tbuf2 + 1, str, AF_Case(ptr),
				global_eval_context.wnxt, 10,
				match_space, match_space_len)) {
          match_found = 1;
          match++;
        }
      } else {
        if (quick_wild_new(tbuf2 + 1, str, AF_Case(ptr))) {
          match_found = 1;
          match++;
	  if (!just_match)
	    wild_match_case_r(tbuf2 + 1, str, AF_Case(ptr),
			      global_eval_context.wnxt, 10,
			      match_space, match_space_len);
        }
      }
      if (match_found) {
        /* Since we're still checking the lock on the child, not the
         * parent, we don't actually want to reset lock_checked with
         * each parent checked.  Sorry for the misdirection, Alan.
         *  - Alex */
        if (!lock_checked) {
          lock_checked = 1;
          if ((type == '$' && !eval_lock(player, thing, Command_Lock))
              || (type == '^' && !eval_lock(player, thing, Listen_Lock))
              || !eval_lock(player, thing, Use_Lock)) {
            match--;
            if (errobj)
              *errobj = thing;
            /* If we failed the lock, there's no point in continuing at all. */
            goto exit_sequence;
          }
        }
        if (atrname && abp) {
          safe_chr(' ', atrname, abp);
          if (Can_Examine(player, parent))
            safe_dbref(parent, atrname, abp);
          else
            safe_dbref(thing, atrname, abp);

          safe_chr('/', atrname, abp);
          safe_str(AL_NAME(ptr), atrname, abp);
        }
        if (!just_match) {
          local_ooref = ooref;
          ooref = AL_CREATOR(ptr);
          if (AL_FLAGS(ptr) & AF_POWINHERIT)
            div_parse_que(parent, s, thing, player);
          else
            parse_que(thing, s, player);
          ooref = local_ooref;
        }
      }
    }
  }

  /* This is where I wish for 'try {} finally {}'... */
exit_sequence:
  while (used_list) {
    UsedAttr *temp = used_list->next;
    mush_free(used_list, "used_attr");
    used_list = temp;
  }
  return match;
}


/* hacked version of atr_comm_match for divisions */
int
atr_comm_divmatch(dbref thing, dbref player, int type, int end,
                  char const *str, int just_match, char *atrname,
                  char **abp, dbref * errobj)
{
  int flag_mask;
  ATTR *ptr;
  int division_depth;
  char tbuf1[BUFFER_LEN];
  char tbuf2[BUFFER_LEN];
  char *s;
  int match, match_found;
  dbref division, local_ooref;
  UsedAttr *used_list, **prev;
  ATTR *skip[ATTRIBUTE_NAME_LIMIT / 2];
  int skipcount;
  int lock_checked = 0;
  char match_space[BUFFER_LEN * 2];
  int match_space_len = BUFFER_LEN * 2;

  /* check for lots of easy ways out */
  if ((type != '$' && type != '^') || !GoodObject(thing) || Halted(thing)
      || (type == '$' && NoCommand(thing)))
    return 0;

  if (type == '$') {
    flag_mask = AF_COMMAND;
    division_depth = GoodObject(Division(thing));
  } else {
    flag_mask = AF_LISTEN;
    if (ThingInhearit(thing) || RoomInhearit(thing)) {
      division_depth = GoodObject(Division(thing));
    } else {
      division_depth = 0;
    }
  }

  match = 0;
  used_list = NULL;
  prev = &used_list;

  skipcount = 0;
  /* do_rawlog(LT_TRACE, "Searching %s:", Name(thing)); */
  for (ptr = List(thing); ptr; ptr = AL_NEXT(ptr)) {
    if (skipcount && ptr == skip[skipcount - 1]) {
      size_t len = strrchr(AL_NAME(ptr), '`') - AL_NAME(ptr);
      while (AL_NEXT(ptr) && strlen(AL_NAME(AL_NEXT(ptr))) > len &&
             AL_NAME(AL_NEXT(ptr))[len] == '`') {
        ptr = AL_NEXT(ptr);
        /* do_rawlog(LT_TRACE, "  Skipping %s", AL_NAME(ptr)); */
      }
      skipcount--;
      continue;
    }
    if (division_depth)
      prev = use_attr(prev, AL_NAME(ptr), AF_Noprog(ptr));
    if (AF_Noprog(ptr)) {
      skip[skipcount] = atr_sub_branch(ptr);
      if (skip[skipcount])
        skipcount++;
      continue;
    }
    if (!(AL_FLAGS(ptr) & flag_mask))
      continue;
    strcpy(tbuf1, atr_value(ptr));
    s = tbuf1;
    do {
      s = strchr(s + 1, end);
    } while (s && s[-1] == '\\');
    if (!s)
      continue;
    *s++ = '\0';
    if (type == '^' && !AF_Ahear(ptr)) {
      if ((thing == player && !AF_Mhear(ptr))
	  || (thing != player && AF_Mhear(ptr)))
	continue;
    }

    if (AF_Regexp(ptr)) {
      /* Turn \: into : */
      char *from, *to;
      for (from = tbuf1, to = tbuf2; *from; from++, to++) {
        if (*from == '\\' && *(from + 1) == ':')
          from++;
        *to = *from;
      }
      *to = '\0';
    } else
      strcpy(tbuf2, tbuf1);

    match_found = 0;
    if (AF_Regexp(ptr)) {
	if (regexp_match_case_r(tbuf2 + 1, str, AF_Case(ptr),
				global_eval_context.wnxt, 10,
				match_space, match_space_len)) {
        match_found = 1;
        match++;
      }
    } else {
      if (quick_wild_new(tbuf2 + 1, str, AF_Case(ptr))) {
        match_found = 1;
        match++;
	if(!just_match)
	  wild_match_case_r(tbuf2 + 1, str, AF_Case(ptr),
	      global_eval_context.wnxt, 10,
	      match_space, match_space_len);
      }
    }
    if (match_found) {
      /* We only want to do the lock check once, so that any side
       * effects in the lock are only performed once per utterance.
       * Thus, '$foo *r:' and '$foo b*:' on the same object will only
       * run the lock once for 'foo bar'.
       */
      if (!lock_checked) {
        lock_checked = 1;
        if ((type == '$' && !eval_lock(player, thing, Command_Lock))
            || (type == '^' && !eval_lock(player, thing, Listen_Lock))
            || !eval_lock(player, thing, Use_Lock)) {
          match--;
          if (errobj)
            *errobj = thing;
          /* If we failed the lock, there's no point in continuing at all. */
          goto exit_sequence;
        }
      }
      if (atrname && abp) {
        safe_chr(' ', atrname, abp);
        safe_dbref(thing, atrname, abp);
        safe_chr('/', atrname, abp);
        safe_str(AL_NAME(ptr), atrname, abp);
      }
      if (!just_match) {
        local_ooref = ooref;
        ooref = AL_CREATOR(ptr);
        parse_que(thing, s, player);
        ooref = local_ooref;
      }
    }
  }

  /* Don't need to free used_list here, because if !division_depth,
   * we would never have allocated it. */
  if (!division_depth)
    return match;

  for (division_depth = MAX_PARENTS, division = Division(thing);
       division_depth-- && division != NOTHING;
       division = Division(division)) {
    /* do_rawlog(LT_TRACE, "Searching %s:", Name(division)); */
    skipcount = 0;
    prev = &used_list;
    for (ptr = List(division); ptr; ptr = AL_NEXT(ptr)) {
      if (skipcount && ptr == skip[skipcount - 1]) {
        size_t len = strrchr(AL_NAME(ptr), '`') - AL_NAME(ptr);
        while (AL_NEXT(ptr) && strlen(AL_NAME(AL_NEXT(ptr))) > len &&
               AL_NAME(AL_NEXT(ptr))[len] == '`') {
          ptr = AL_NEXT(ptr);
          /* do_rawlog(LT_TRACE, "  Skipping %s", AL_NAME(ptr)); */
        }
        skipcount--;
        continue;
      }
      if (AF_Private(ptr)) {
        /* do_rawlog(LT_TRACE, "Private %s:", AL_NAME(ptr)); */
        skip[skipcount] = atr_sub_branch(ptr);
        if (skip[skipcount])
          skipcount++;
        continue;
      }
      if (find_attr(&prev, AL_NAME(ptr))) {
        /* do_rawlog(LT_TRACE, "Found %s:", AL_NAME(ptr)); */
        if (prev[0]->no_prog || AF_Noprog(ptr)) {
          skip[skipcount] = atr_sub_branch(ptr);
          if (skip[skipcount])
            skipcount++;
          prev[0]->no_prog = AF_NOPROG;
        }
        continue;
      }
      if (GoodObject(Division(division)))
        prev = use_attr(prev, AL_NAME(ptr), AF_Noprog(ptr));
      if (AF_Noprog(ptr)) {
        /* do_rawlog(LT_TRACE, "NoProg %s:", AL_NAME(ptr)); */
        skip[skipcount] = atr_sub_branch(ptr);
        if (skip[skipcount])
          skipcount++;
        continue;
      }
      if (!(AL_FLAGS(ptr) & flag_mask))
        continue;
      strcpy(tbuf1, atr_value(ptr));
      s = tbuf1;
      do {
        s = strchr(s + 1, end);
      } while (s && s[-1] == '\\');
      if (!s)
        continue;
      *s++ = '\0';
      if (type == '^' && !AF_Ahear(ptr)) {
	if ((thing == player && !AF_Mhear(ptr))
	    || (thing != player && AF_Mhear(ptr)))
	  continue;
      }

      if (AF_Regexp(ptr)) {
        /* Turn \: into : */
        char *from, *to;
        for (from = tbuf1, to = tbuf2; *from; from++, to++) {
          if (*from == '\\' && *(from + 1) == ':')
            from++;
          *to = *from;
        }
        *to = '\0';
      } else
        strcpy(tbuf2, tbuf1);

      match_found = 0;
      if (AF_Regexp(ptr)) {
	if (regexp_match_case_r(tbuf2 + 1, str, AF_Case(ptr),
				global_eval_context.wnxt, 10,
				match_space, match_space_len)) {
          match_found = 1;
          match++;
        }
      } else {
        if (quick_wild_new(tbuf2 + 1, str, AF_Case(ptr))) {
          match_found = 1;
          match++;
	  if(!just_match)
	    wild_match_case_r(tbuf2 + 1, str, AF_Case(ptr),
			      global_eval_context.wnxt, 10,
			      match_space, match_space_len);
	}
      }
      if (match_found) {
        /* Since we're still checking the lock on the child, not the
         * division, we don't actually want to reset lock_checked with
         * each division checked.  Sorry for the misdirection, Alan.
         *  - Alex */
        if (!lock_checked) {
          lock_checked = 1;
          if ((type == '$' && !eval_lock(player, thing, Command_Lock))
              || (type == '^' && !eval_lock(player, thing, Listen_Lock))
              || !eval_lock(player, thing, Use_Lock)) {
            match--;
            if (errobj)
              *errobj = thing;
            /* If we failed the lock, there's no point in continuing at all. */
            goto exit_sequence;
          }
        }
        if (atrname && abp) {
          safe_chr(' ', atrname, abp);
          if (Can_Examine(player, division))
            safe_dbref(division, atrname, abp);
          else
            safe_dbref(thing, atrname, abp);

          safe_chr('/', atrname, abp);
          safe_str(AL_NAME(ptr), atrname, abp);
        }
        if (!just_match) {
          local_ooref = ooref;
          ooref = AL_CREATOR(ptr);
          if (AL_FLAGS(ptr) & AF_POWINHERIT)
            div_parse_que(division, s, thing, player);
          else
            parse_que(thing, s, player);
          ooref = local_ooref;
        }
      }
    }
  }

  /* This is where I wish for 'try {} finally {}'... */
exit_sequence:
  while (used_list) {
    UsedAttr *temp = used_list->next;
    mush_free(used_list, "used_attr");
    used_list = temp;
  }
  return match;
}


/** Match input against a specified object's specified $command 
 * attribute. Matches may be glob or regex matches. Used in command hooks.
 * depending on the attribute's flags. 
 * \param thing object containing attributes to check.
 * \param player the enactor, for privilege checks.
 * \param atr the name of the attribute
 * \param str the string to match
 * \retval 1 attribute matched.
 * \retval 0 attribute failed to match.
 */
int
one_comm_match(dbref thing, dbref player, const char *atr, const char *str)
{
  ATTR *ptr;
  char tbuf1[BUFFER_LEN];
  char tbuf2[BUFFER_LEN];
  char *s;
  char match_space[BUFFER_LEN * 2];
  int match_space_len = BUFFER_LEN * 2;

  /* check for lots of easy ways out */
  if (!GoodObject(thing) || Halted(thing) || NoCommand(thing))
    return 0;

  if (!(ptr = atr_get(thing, atr)))
    return 0;

  if (AF_Noprog(ptr) || !AF_Command(ptr))
    return 0;

  strcpy(tbuf1, atr_value(ptr));
  s = tbuf1;
  do {
    s = strchr(s + 1, ':');
  } while (s && s[-1] == '\\');
  if (!s)
    return 0;
  *s++ = '\0';

  if (AF_Regexp(ptr)) {
    /* Turn \: into : */
    char *from, *to;
    for (from = tbuf1, to = tbuf2; *from; from++, to++) {
      if (*from == '\\' && *(from + 1) == ':')
        from++;
      *to = *from;
    }
    *to = '\0';
  } else
    strcpy(tbuf2, tbuf1);

  if (AF_Regexp(ptr) ?
      regexp_match_case_r(tbuf2 + 1, str, AF_Case(ptr),
	global_eval_context.wnxt, 10, match_space,
	match_space_len) : wild_match_case_r(tbuf2 + 1, str,
	  AF_Case(ptr), global_eval_context.  wnxt, 10, match_space,
	  match_space_len))
  {
    if (!eval_lock(player, thing, Command_Lock)
        || !eval_lock(player, thing, Use_Lock))
      return 0;
    parse_que(thing, s, player);
    return 1;
  }
  return 0;
}

/*======================================================================*/

/** Set or clear an attribute on an object.
 * \verbatim
 * This is the primary function for implementing @set.
 * A new interface (as of 1.6.9p0) for setting attributes,
 * which takes care of case-fixing, object-level flag munging,
 * alias recognition, add/clr distinction, etc.  Enjoy.
 * \endverbatim
 * \param thing object to set the attribute on or remove it from.
 * \param atr name of the attribute to set or clear.
 * \param s value to set the attribute to (or NULL to clear).
 * \param player enactor, for permission checks.
 * \param flags attribute flags.
 * \retval -1 failure of one sort.
 * \retval 0 failure of another sort.
 * \retval 1 success.
 */
int
do_set_atr(dbref thing, const char *RESTRICT atr, const char *RESTRICT s,
           dbref player, unsigned int flags)
{
  ATTR *old;
  char name[BUFFER_LEN];
  char tbuf1[BUFFER_LEN];
  atr_err res;
  int was_hearer;
  int was_listener;
  dbref contents;
  if (!EMPTY_ATTRS && s && !*s)
    s = NULL;
  if (IsGarbage(thing)) {
    notify(player, T("Garbage is garbage."));
    return 0;
  }

  /* Taking care of controls check in can_write_attr_internal now */
  /*
     if (!controls(player, thing)) 
     return 0;
   */

  strcpy(name, atr);
  upcasestr(name);
  if (!strcmp(name, "ALIAS") && IsPlayer(thing)) {
    old = atr_get_noparent(thing, "ALIAS");
    tbuf1[0] = '\0';
    if (old) {
      /* Old alias - we're allowed to change to a different case */
      strcpy(tbuf1, atr_value(old));
      if (s && !*s) {
	notify_format(player, T("'%s' is not a valid alias."), s);
	return -1;
      }
      if (s && strcasecmp(s, tbuf1)) {
	int opae_res = ok_player_alias(s, player, thing);
	switch (opae_res) {
	case OPAE_INVALID:
	  notify_format(player, T("'%s' is not a valid alias."), s);
	  break;
	case OPAE_TOOMANY:
	  notify_format(player, T("'%s' contains too many aliases."), s);
	  break;
	case OPAE_NULL:
	  notify_format(player, T("Null aliases are not valid."));
	  break;
	}
	if (opae_res != OPAE_SUCCESS)
	  return -1;
      }
    } else {
      /* No old alias */
      if (s && *s) {
	int opae_res = ok_player_alias(s, player, thing);
	switch (opae_res) {
	case OPAE_INVALID:
	  notify_format(player, T("'%s' is not a valid alias."), s);
	  break;
	case OPAE_TOOMANY:
	  notify_format(player, T("'%s' contains too many aliases."), s);
	  break;
	case OPAE_NULL:
	  notify_format(player, T("Null aliases are not valid."));
	  break;
	}
	if (opae_res != OPAE_SUCCESS)
	  return -1;
      }
    }
  } else if (s && *s && (!strcmp(name, "FORWARDLIST")
			 || !strcmp(name, "MAILFORWARDLIST")
                         || !strcmp(name, "DEBUGFORWARDLIST"))) {
    /* You can only set this to dbrefs of things you're allowed to
     * forward to. If you get one wrong, we puke.
     */
    char *fwdstr, *curr;
    dbref fwd;
    strcpy(tbuf1, s);
    fwdstr = trim_space_sep(tbuf1, ' ');
    while ((curr = split_token(&fwdstr, ' ')) != NULL) {
      if (!is_objid(curr)) {
        notify_format(player, T("%s should contain only dbrefs."), name);
        return -1;
      }
      fwd = parse_objid(curr);
      if (!GoodObject(fwd) || IsGarbage(fwd)) {
        notify_format(player, T("Invalid dbref #%d in %s."), fwd, name);
        return -1;
      }
      if ((!strcmp(name, "FORWARDLIST") || !strcmp(name, "DEBUGFORWARDLIST"))
	  && !Can_Forward(thing, fwd)) {
        notify_format(player, T("I don't think #%d wants to hear from %s."),
		      fwd, Name(thing));
        return -1;
      }
      if (!strcmp(name, "MAILFORWARDLIST") && !Can_MailForward(thing, fwd)) {
	notify_format(player, T("I don't think #%d wants %s's mail."), fwd,
		      Name(thing));
        return -1;
      }
    }
    /* If you made it here, all your dbrefs were ok */
  }

  was_hearer = Hearer(thing);
  was_listener = Listener(thing);
  res =
      s ? atr_add(thing, name, s, player,
                  (flags & 0x02) ? AF_NOPROG : AF_EMPTY_FLAGS)
      : atr_clr(thing, name, player);
  switch (res) {
  case AE_SAFE:
    notify_format(player, T("Attribute %s is SAFE. Set it !SAFE to modify it."),
		  name);
    return 0;
  case AE_TREE:
    if (!s) {
      notify_format(player,
		    T("Unable to remove '%s' because of a protected tree attribute."),
		    name);
      return 0;
    } else {
      notify_format(player,
		    T("Unable to set '%s' because of a failure to create a needed parent attribute."), name);
      return 0;
    }
  case AE_BADNAME:
    notify_format(player, T("That's not a very good name for an attribute."));
    return 0;
  case AE_ERROR:
    if (*missing_name) {
      if (s && (EMPTY_ATTRS || *s))
        notify_format(player, T("You must set %s first."), missing_name);
      else
        notify_format(player,
                      T
                      ("%s is a branch attribute; remove its children first."),
                      missing_name);
    } else
      notify(player, T("That attribute cannot be changed by you."));
    return 0;
  case AE_TOOMANY:
    notify(player, T("Too many attributes on that object to add another."));
    return 0;
  case AE_NOTFOUND:
    notify(player, T("No such attribute to reset."));
    return 0;
  case AE_OKAY:
    /* Success */
    break;
  }
  if (!strcmp(name, "ALIAS") && IsPlayer(thing)) {
    reset_player_list(thing, NULL, tbuf1, NULL, s);
    if (s && *s)
      notify(player, T("Alias set."));
    else
      notify(player, T("Alias removed."));
    return 1;
  } else if (!strcmp(name, "LISTEN")) {
    if (IsRoom(thing))
      contents = Contents(thing);
    else {
      contents = Location(thing);
      if (GoodObject(contents))
        contents = Contents(contents);
    }
    if (GoodObject(contents)) {
      if (!s && !was_listener && !Hearer(thing)) {
        notify_except(contents, thing,
                      tprintf(T("%s loses its ears and becomes deaf."),
                              Name(thing)), NA_INTER_PRESENCE);
      } else if (s && !was_hearer && !was_listener) {
        notify_except(contents, thing,
                      tprintf(T("%s grows ears and can now hear."),
                              Name(thing)), NA_INTER_PRESENCE);
      }
    }
  }
  if ((flags & 0x01) && !AreQuiet(player, thing))
    notify_format(player,
                  "%s/%s - %s.", Name(thing), name,
                  s ? T("Set") : T("Cleared"));
  return 1;
}

/** Lock or unlock an attribute.
 * Attribute locks are largely obsolete and should be deprecated,
 * but this is the code that does them.
 * \param player the enactor.
 * \param arg1 the object/attribute, as a string.
 * \param arg2 the desired lock status ('on' or 'off').
 */
void
do_atrlock(dbref player, const char *arg1, const char *arg2,
           char write_lock)
{
  dbref thing, creator;
  char *p;
  ATTR *ptr;
  int status;
  boolexp key;

  if (!arg2 || !*arg2)
    status = 0;

  if (!arg1 || !*arg1) {
    notify(player, T("You need to give an object/attribute pair."));
    return;
  }
  if (!(p = strchr(arg1, '/')) || !(*(p + 1))) {
    notify(player, T("You need to give an object/attribute pair."));
    return;
  }
  *p++ = '\0';

  if ((thing = noisy_match_result(player, arg1, NOTYPE, MAT_EVERYTHING)) ==
      NOTHING)
    return;

  if (!controls(player, thing)) {
    notify(player, T("Permission denied."));
    return;
  }

  ptr = atr_get_noparent(thing, strupper(p));
  if (ptr && Can_Read_Attr(player, thing, ptr)) {
    if (!Can_Write_Attr(player, thing, ptr)) {
      notify(player,
             T
             ("You need to be able to set the attribute to change its lock."));
      return;
    } else {
      creator = ooref != NOTHING ? Owner(ooref) : Owner(player);
      if (!status) {
        char lmbuf[1024];
        ModTime(thing) = mudtime;
        snprintf(lmbuf, 1023, "%s unlock-atr%slock[#%d]", ptr->name,
                 write_lock ? "write" : "read", player);
        lmbuf[strlen(lmbuf) + 1] = '\0';
        set_lmod(thing, lmbuf);
        AL_CREATOR(ptr) = creator;
        notify_format(player, "Unlocked attribute %slock.",
                      write_lock ? "write" : "read");
        free_boolexp(write_lock ? AL_WLock(ptr) : AL_RLock(ptr));
        if (write_lock)
          AL_WLock(ptr) = TRUE_BOOLEXP;
        else
          AL_RLock(ptr) = TRUE_BOOLEXP;
      } else {
        key = parse_boolexp(creator, arg2, "ATTR");
        if (key == TRUE_BOOLEXP)
          notify(player, T("I don't understand that key."));
        else {
          char lmbuf[1024];
          ModTime(thing) = mudtime;
          snprintf(lmbuf, 1023, "%s atr%slock[#%d]", ptr->name,
                   write_lock ? "write" : "read", player);
          lmbuf[strlen(lmbuf) + 1] = '\0';
          set_lmod(thing, lmbuf);
          AL_CREATOR(ptr) = creator;
          notify_format(player, "Locked attribute %slock.",
                        write_lock ? "write" : "read");
          free_boolexp(write_lock ? AL_WLock(ptr) : AL_RLock(ptr));
          if (write_lock)
            AL_WLock(ptr) = key;
          else
            AL_RLock(ptr) = key;
        }
      }
    }
  } else
    notify(player, T("No such attribute."));
  return;
}

/** Change ownership of an attribute.
 * \verbatim
 * This function is used to implement @atrchown.
 * \endverbatim
 * \param player the enactor, for permission checking.
 * \param arg1 the object/attribute to change, as a string.
 * \param arg2 the name of the new owner (or "me").
 */
void
do_atrchown(dbref player, const char *arg1, const char *arg2)
{
  dbref thing, new_owner;
  char *p;
  ATTR *ptr;
  if (!arg1 || !*arg1) {
    notify(player, T("You need to give an object/attribute pair."));
    return;
  }
  if (!(p = strchr(arg1, '/')) || !(*(p + 1))) {
    notify(player, T("You need to give an object/attribute pair."));
    return;
  }
  *p++ = '\0';

  if ((thing = noisy_match_result(player, arg1, NOTYPE, MAT_EVERYTHING)) ==
      NOTHING)
    return;

  if (ooref != NOTHING && ooref != player && !God(player)) {
    notify(player, T("Permission denied."));
    return;
  } else if (!Owns(player, thing) && !God(player)) {
    notify(player, T("Permission denied."));
    return;
  }

  if (!controls(player, thing)) {
    notify(player, T("Permission denied."));
    return;
  }

  if ((!arg2 && !*arg2) || !strcasecmp(arg2, "me"))
    new_owner = player;
  else
    new_owner = lookup_player(arg2);
  if (new_owner == NOTHING) {
    notify(player, T("I can't find that player"));
    return;
  }

  ptr = atr_get_noparent(thing, strupper(p));
  if (ptr && Can_Read_Attr(player, thing, ptr)) {
    if (Can_Write_Attr(player, thing, ptr)) {
      if (!controls(player, Owner(thing))) {
        notify(player, T("You can only chown an attribute to yourself."));
        return;
      }
      AL_CREATOR(ptr) = ooref != NOTHING ? Owner(ooref) : Owner(new_owner);
      notify(player, T("Attribute owner changed."));
      return;
    } else {
      notify(player, T("You don't have the permission to chown that."));
      return;
    }
  } else
    notify(player, T("No such attribute."));
}


/** Return the head of the attribute free list.
 * This function returns the head of the attribute free list. If there
 * are no more ATTR's on the free list, allocate a new page.
 * \return the pointer to the head of the attribute free list.
 */
static ATTR *
alloc_atr(void)
{
  if (!atr_free_list) {
    ATTRPAGE *page;
    int j;
    page = (ATTRPAGE *) mush_malloc(sizeof(ATTRPAGE), "ATTRPAGE");
    if (!page)
      mush_panic("Couldn't allocate memory in alloc_attr");
    for (j = 0; j < ATTRS_PER_PAGE - 1; j++)
      AL_NEXT(page->atrs + j) = page->atrs + j + 1;
    AL_NEXT(page->atrs + ATTRS_PER_PAGE - 1) = NULL;
    atr_free_list = page->atrs;
  }
  return atr_free_list;
}

/** Traversal routine for Can_Read_Attr.
 * This function determines if an attribute can be read by examining
 * the tree path to the attribute.  This is not the full Can_Read_Attr
 * check; only the stuff after See_All (just to avoid function calls
 * when the answer is trivialized by special powers).  If the specified
 * player is NOTHING, then we're doing a generic mortal visibility check.
 * \param player the player trying to do the read.
 * \param obj the object targetted for the read (may be a child of a parent!).
 * \param atr the attribute being interrogated.
 * \retval 0 if the player cannot read the attribute.
 * \retval 1 if the player can read the attribute.
 */
int
can_read_attr_internal(dbref player, dbref obj, ATTR * atr)
{
  static char name[ATTRIBUTE_NAME_LIMIT + 1];
  char *p;
  int cansee;
  int canlook;
  int comp;
  dbref target;
  dbref ancestor;
  int visible;
  int parent_depth;
  int r_lock;

  visible = (player == NOTHING);

  /* Evaluate read lock first here */
  if (AL_RLock(atr) == TRUE_BOOLEXP)
    r_lock = 1;
  else
    r_lock = (eval_boolexp(player, AL_RLock(atr), obj, NULL)
              || controls(player, AL_CREATOR(atr)));

  if (visible) {
    cansee = ((Visual(obj) &&
               eval_lock(PLAYER_START, obj, Examine_Lock) &&
               eval_lock(MASTER_ROOM, obj, Examine_Lock))) &&
        (AL_CREATOR(atr) == Owner(player) || r_lock);
    canlook = 0;
  } else {
    cansee = ((controls(player, obj) && r_lock)
              || ((Visual(obj) && eval_lock(player, obj, Examine_Lock)))
              || div_cansee(player, obj))
        && (AL_CREATOR(atr) == Owner(player) || r_lock);
    canlook = can_look_at(player, obj);
  }

  /* Take an easy out if there is one... */
  /* If we can't see the attribute itself, then that's easy. */
  if (AF_Internal(atr) && !God(player))
    return 0;

  if(!Admin(player)
     && (AF_Mdark(atr)
	 || !(cansee
	      || ((AF_Visual(atr)
		   || ((AL_RLock(atr) != TRUE_BOOLEXP) && r_lock))
		  && (!AF_Nearby(atr) || canlook))
	      || (!visible && !Mistrust(player)
		  && (Owner(AL_CREATOR(atr)) == Owner(player))))))
    return 0;

  /* If the attribute isn't on a branch, then that's also easy. */
  if (!strchr(AL_NAME(atr), '`'))
    return 1;
  /* Nope, we actually have to go looking for the attribute in a tree. */
  strcpy(name, AL_NAME(atr));
  ancestor = Ancestor_Parent(obj);
  target = obj;
  parent_depth = 0;
  while (parent_depth < MAX_PARENTS && GoodObject(target)) {
    /* If the ancestor of the object is in its explict parent chain,
     * we use it there, and don't check the ancestor later.
     */
    if (target == ancestor)
      ancestor = NOTHING;
    atr = List(target);
    /* Check along the branch for permissions... */
    for (p = strchr(name, '`'); p; p = strchr(p + 1, '`')) {
      *p = '\0';
      while (atr) {
        comp = strcoll(name, AL_NAME(atr));
        if (comp < 0) {
          *p = '`';
          goto continue_target;
        }
        if (comp == 0)
          break;
        atr = AL_NEXT(atr);
      }
      if (!atr || (target != obj && AF_Private(atr))) {
        *p = '`';
        goto continue_target;
      }

      if (AF_Internal(atr) || (!Admin(player) && (AF_Mdark(atr) ||
                                                  !(cansee
                                                    ||
                                                    ((AF_Visual(atr)
                                                      ||
                                                      ((AL_RLock(atr) !=
                                                        TRUE_BOOLEXP)
                                                       && r_lock))
                                                     && (!AF_Nearby(atr)
                                                         || canlook))
                                                    || (!visible
                                                        &&
                                                        !Mistrust(player)
                                                        &&
                                                        (Owner
                                                         (AL_CREATOR(atr))
                                                         ==
                                                         Owner
                                                         (player)))))))
        return 0;

      *p = '`';
    }

    /* Now actually find the attribute. */
    while (atr) {
      comp = strcoll(name, AL_NAME(atr));
      if (comp < 0)
        break;
      if (comp == 0)
        return 1;
      atr = AL_NEXT(atr);
    }

  continue_target:

    /* Attribute wasn't on this object.  Check a parent or ancestor. */
    parent_depth++;
    target = Parent(target);
    if (!GoodObject(target)) {
      parent_depth = 0;
      target = ancestor;
    }
  }

  return 0;
}

/** Traversal routine for Can_Write_Attr.
 * This function determines if an attribute can be written by examining
 * the tree path to the attribute.  As a side effect, missing_name is
 * set to the name of a missing prefix branch, if any.  Yes, side effects
 * are evil.  Please fix if you can.
 * \param player the player trying to do the write.
 * \param obj the object targetted for the write.
 * \param atr the attribute being interrogated.
 * \param safe whether to check the safe attribute flag.
 * \retval 0 if the player cannot write the attribute.
 * \retval 1 if the player can write the attribute.
 */

int
can_write_attr_internal(dbref player, dbref obj, ATTR * atr, int safe)
{
  char *p;
  missing_name[0] = '\0';
  if (!can_read_attr_internal(player, obj, atr))
    return 0;
  if (Cannot_Write_This_Attr(player, atr, obj, safe, !atr_match(atr->name), AL_CREATOR(atr)))
    return 0;
  strcpy(missing_name, AL_NAME(atr));
  atr = List(obj);
  for (p = strchr(missing_name, '`'); p; p = strchr(p + 1, '`')) {
    *p = '\0';
    atr = find_atr_in_list(atr, missing_name);
    if (!atr)
      return 0;
    if (Cannot_Write_This_Attr(player, atr, obj, safe, !atr_match(atr->name), AL_CREATOR(atr))) {
      missing_name[0] = '\0';
      return 0;
    }
    *p = '`';
  }

  return 1;
}


/** Remove all child attributes from root attribute that can be.
 * \param player object doing a @wipe.
 * \param thing object being @wiped.
 * \param root root of attribute tree.
 * \return 1 if all children were deleted, 0 if some were left.
 */
static int
atr_clear_children(dbref player, dbref thing, ATTR *root)
{
  ATTR *sub, *next = NULL, *prev;
  int skipped = 0;

  prev = root;

  for (sub = atr_sub_branch(root);
       sub && string_prefix(AL_NAME(sub), AL_NAME(root)); sub = next) {
    if (AL_FLAGS(sub) & AF_ROOT) {
      if (!atr_clear_children(player, thing, sub)) {
        skipped++;
        next = AL_NEXT(sub);
        prev = sub;
        continue;
      }
    }

    next = AL_NEXT(sub);

    if (!Can_Write_Attr(player, thing, sub)) {
      skipped++;
      prev = sub;
      continue;
    }

    /* Can safely delete attribute.  */
    AL_NEXT(prev) = next;
    atr_free_one(sub);
    AttrCount(thing)--;

  }

  return !skipped;

}

/** Pop an empty attribute off of the free list for use on
 * an object.
 * \return the pointer to an attribute, or NULL on error.
 */
static ATTR *
pop_free_list(void)
{
  ATTR *ptr;
  ptr = alloc_atr();
  if (!ptr)
    return NULL;
  atr_free_list = AL_NEXT(ptr);
  AL_NEXT(ptr) = NULL;
  return ptr;
}

/** Push a now-unused attribute onto the free list
 * \param An attribute that's been deleted from an object and
 * had its chunk reference deleted.
 */
static void
push_free_list(ATTR *a)
{
  memset(a, 0, sizeof(*a));
  AL_NEXT(a) = atr_free_list;
  atr_free_list = a;
}

/** Delete one attribute, deallocating its name and data.
 * <strong>Does not update the owning object's attribute list or
 * attribute count. That is the caller's responsibility.</strong>
 *
 * \param a the attribute to free
 */
static void
atr_free_one(ATTR *a)
{
  if (!a)
    return;

  if (AL_WLock(a) != TRUE_BOOLEXP)
    free_boolexp(AL_WLock(a));
  if (AL_RLock(a) != TRUE_BOOLEXP)
    free_boolexp(AL_RLock(a));

  st_delete(AL_NAME(a), &atr_names);

  if (a->data)
    chunk_delete(a->data);

  push_free_list(a);
}

/** Return the compressed data for an attribute.
 * This is a chokepoint function for accessing the chunk data.
 * \param atr the attribute struct from which to get the data reference.
 * \return a pointer to the compressed data, in a static buffer.
 */
unsigned char const *
atr_get_compressed_data(ATTR * atr)
{
  static unsigned char buffer[BUFFER_LEN * 2];
  unsigned int len;
  if (!atr->data)
    return (unsigned char *) "";
  len = chunk_fetch(atr->data, buffer, sizeof(buffer));
  if (len > sizeof(buffer))
    return (unsigned char *) "";
  buffer[len] = '\0';
  return buffer;
}

/** Return the uncompressed data for an attribute in a static buffer.
 * This is a wrapper function, to centralize the use of compression/
 * decompression on attributes.
 * \param atr the attribute struct from which to get the data reference.
 * \return a pointer to the uncompressed data, in a static buffer.
 */
char *
atr_value(ATTR * atr)
{
  return uncompress(atr_get_compressed_data(atr));
}

/** Return the uncompressed data for an attribute in a dynamic buffer.
 * This is a wrapper function, to centralize the use of compression/
 * decompression on attributes.
 * \param atr the attribute struct from which to get the data reference.
 * \return a pointer to the uncompressed data, in a dynamic buffer.
 */
char *
safe_atr_value(ATTR * atr)
{
  return safe_uncompress(atr_get_compressed_data(atr));
}
