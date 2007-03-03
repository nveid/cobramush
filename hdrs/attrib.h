#ifndef _ATTRIB_H
#define _ATTRIB_H

#include "mushtype.h"
#include "boolexp.h"
#include "chunk.h"
#include "command.h"

/** An attribute on an object.
 * This structure represents an attribute set on an object.
 * Attributes form a linked list on an object, sorted alphabetically.
 */

struct attr {
  char const *name;		/**< Name of attribute */
  int flags;			/**< Attribute flags */
  chunk_reference_t data;	/**< The attribute's value, compressed */
  dbref creator;		/**< The attribute's creator's dbref */
  boolexp write_lock;		/**< Attribute lock set */
  boolexp read_lock;		/**< Attribute read lock */
  ATTR *next;			/**< Pointer to next attribute in list */
};

struct aget_oi {
	ATTR *attribute_part;
	dbref found_on;
};


/* Stuff that's actually in atr_tab.c */
extern ATTR *aname_hash_lookup(const char *name);
extern int alias_attribute(const char *atr, const char *alias);
extern void do_attribute_access
  (dbref player, char *name, char *perms, int retroactive);
extern void do_attribute_delete(dbref player, char *name, char def);
extern void do_attribute_rename(dbref player, char *old, char *newname);
extern void do_attribute_info(dbref player, char *name);
extern void do_list_attribs(dbref player, int lc);
extern char *list_attribs(void);
extern void attr_init_postconfig(void);
extern void do_attribute_lock(dbref player, char *name, char *lock, switch_mask swi);

/* From attrib.c */
extern int good_atr_name(char const *s);
extern ATTR *atr_match(char const *string);
extern ATTR *atr_sub_branch(ATTR *branch);
extern void atr_new_add(dbref thing, char const *RESTRICT atr,
			char const *RESTRICT s, dbref player, int flags,
			unsigned char derefs, boolexp wlock, boolexp rlock);
extern int atr_add(dbref thing, char const *RESTRICT atr,
		   char const *RESTRICT s, dbref player, int flags);
extern int atr_clr(dbref thing, char const *atr, dbref player);
extern ATTR *atr_get(dbref thing, char const *atr);
extern ATTR *atr_get_noparent(dbref thing, char const *atr);
typedef int (*aig_func) (dbref, dbref, dbref, const char *, ATTR *, void *);
extern int atr_iter_get(dbref player, dbref thing, char const *name,
			int mortal, aig_func func, void *args);
extern ATTR *atr_complete_match(dbref player, char const *atr, dbref privs);
extern void atr_free(dbref thing);
extern void atr_cpy(dbref dest, dbref source);
extern char const *convert_atr(int oldatr);
extern int atr_comm_match(dbref thing, dbref player, int type, int end,
			  char const *str, int just_match, char *atrname,
			  char **abp, dbref *errobj);
extern int atr_comm_divmatch(dbref thing, dbref player, int type, int end,
			     char const *str, int just_match, char *atrname,
			     char **abp, dbref *errobj);
extern int one_comm_match(dbref thing, dbref player, const char *atr,
                         const char *str);
extern int do_set_atr(dbref thing, char const *RESTRICT atr,
                     char const *RESTRICT s, dbref player, int flags);
extern void do_atrlock(dbref player, char const *arg1, char const *arg2, char write_lock);
extern void do_atrchown(dbref player, char const *arg1, char const *arg2);
extern int string_to_atrflag(dbref player, const char *p);
extern int string_to_atrflagsets(dbref player, const char *p, int *setbits,
				 int *clrbits);
extern const char *atrflag_to_string(int mask);
extern void init_atr_name_tree(void);

extern int can_read_attr_internal(dbref player, dbref obj, ATTR *attr);
extern int can_write_attr_internal(dbref player, dbref obj, ATTR *attr,
				   int safe);
extern unsigned const char *atr_get_compressed_data(ATTR *atr);
extern char *atr_value(ATTR *atr);
extern char *
safe_atr_value(ATTR *atr)
  __attribute_malloc__;


/* possible attribute flags */
#define AF_ODARK        0x1	/* OBSOLETE! Leave here but don't use */
#define AF_INTERNAL     0x2	/* no one can see it or set it */
#define AF_PRIVILEGE    0x4	/* Only privileged players can change it */
#define AF_NUKED        0x8	/* OBSOLETE! Leave here but don't use */
#define AF_LOCKED       0x10	/* Only creator of attrib can change it. */
#define AF_NOPROG       0x20	/* won't be searched for $ commands. */
#define AF_MDARK        0x40	/* Only admins can see it */
#define AF_PRIVATE      0x80	/* Children don't inherit it */
#define AF_NOCOPY       0x100	/* atr_cpy (for @clone) doesn't copy it */
#define AF_VISUAL       0x200	/* Everyone can see this attribute */
#define AF_REGEXP       0x400	/* Match $/^ patterns using regexps */
#define AF_CASE         0x800	/* Match $/^ patterns case-sensitive */
#define AF_SAFE         0x1000	/* This attribute may not be modified */
#define AF_STATIC       0x10000	/* OBSOLETE! Leave here but don't use */
#define AF_COMMAND      0x20000	/* INTERNAL: value starts with $ */
#define AF_LISTEN       0x40000	/* INTERNAL: value starts with ^ */
#define AF_NODUMP       0x80000	/* INTERNAL: attribute is not saved */
#define AF_LISTED       0x100000	/* INTERNAL: Used in @list attribs */
#define AF_PREFIXMATCH  0x200000	/* Subject to prefix-matching */
#define AF_VEILED       0x400000	/* On ex, show presence, not value */
#define AF_DEBUG	0x800000  /* Show debug when evaluated */
#define AF_NEARBY	0x1000000 /* Override AF_VISUAL if remote */
#define AF_PUBLIC	0x2000000 /* Override SAFER_UFUN */
#define AF_ANON		0x4000000 /* INTERNAL: Attribute doesn't exist in the database */
#define AF_POWINHERIT	0x8000000	/* Execute with powers of object it's on */
#define AF_MHEAR	0x20000000    /* ^-listens can be triggered by %! */
#define AF_AHEAR	0x40000000    /* ^-listens can be triggered by anyone */

/* external predefined attributes. */
    extern ATTR attr[];

/* external @wipe indicator (changes atr_clr() behaviour) */
    extern int we_are_wiping;

#define AL_ATTR(alist)          (alist)
#define AL_NAME(alist)          ((alist)->name)
#define AL_STR(alist)           (atr_get_compressed_data((alist)))
#define AL_NEXT(alist)          ((alist)->next)
#define AL_CREATOR(alist)       ((alist)->creator)
#define AL_FLAGS(alist)         ((alist)->flags)
#define AL_DEREFS(alist)        ((alist)->data?chunk_derefs((alist)->data):0)
#define AL_WLock(alist)		((alist)->write_lock)
#define AL_RLock(alist)		((alist)->read_lock)

/* Errors from ok_player_alias */
#define OPAE_SUCCESS	1
#define OPAE_INVALID	-1
#define OPAE_TOOMANY	-2
#define OPAE_NULL	-3

#endif				/* __ATTRIB_H */
