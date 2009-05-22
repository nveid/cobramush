/* lock.h */

#include "copyrite.h"

#ifndef __LOCK_H
#define __LOCK_H

#include "mushtype.h"
#include "conf.h"
#include "boolexp.h"

/* I'm using a string for a lock type instead of a magic-cookie int for
 * several reasons:
 * 1) I don't think it will hurt efficiency that much. I'll profile it
 * to check.
 * 2) It will make debugging much easier to see lock types that can easily
 * be interpreted by a human.
 * 3) It allows the possibility of having arbitrary user-defined locks.
 */

/** A list of locks set on an object.
 * An object's locks are represented as a linked list of these structures.
 */
struct lock_list {
  lock_type type;		/**< Type of lock */
  boolexp key;		/**< Lock value ("key") */
  dbref creator;		/**< Dbref of lock creator */
  int flags;			/**< Lock flags */
  struct lock_list *next;	/**< Pointer to next lock in object's list */
};

/* Our table of lock types, attributes, and default flags */
typedef struct lock_msg_info LOCKMSGINFO;
/** A lock.
 * This structure represents a lock in the table of lock types
 */
struct lock_msg_info {
  lock_type type;		/**< Type of lock */
  const char *succbase;		/**< Base name of success attribute */
  const char *failbase;		/**< Base name of failure attribute */
};

#define LF_VISUAL  0x1		/* Anyone can see this lock with lock()/elock() */
#define LF_PRIVATE 0x2		/* This lock doesn't get inherited */
#define LF_PRIVILEGE 0x4	/* Only privileged players can set/unset it */
#define LF_LOCKED  0x8		/* Only the lock's owner can set/unset it */
#define LF_NOCLONE 0x10		/* This lock isn't copied in @clone */
#define LF_OX      0x20		/* This lock's success messages includes OX*. */
#define LF_NOSUCCACTION 0x40	/* This lock doesn't have an @a-action for success. */
#define LF_NOFAILACTION 0x80	/* This lock doesn't have an @a-action for failure */
#define LF_OWNER        0x100	/* Lock can only be set/unset by object's owner */

/* lock.c */
boolexp getlock(dbref thing, lock_type type);
boolexp getlock_noparent(dbref thing, lock_type type);
lock_type match_lock(lock_type type);
const lock_list *get_lockproto(lock_type type);
int add_lock(dbref player, dbref thing, lock_type type, boolexp key, int flags);
int add_lock_raw(dbref player, dbref thing, lock_type type,
		 boolexp key, int flags);
void free_locks(lock_list *ll);
int eval_lock(dbref player, dbref thing, lock_type ltype);
int fail_lock(dbref player, dbref thing, lock_type ltype, const char *def,
	      dbref loc);
void do_unlock(dbref player, const char *name, lock_type type);
void do_lock(dbref player, const char *name, const char *keyname,
	     lock_type type);
void init_locks(void);
void clone_locks(dbref player, dbref orig, dbref clone);
void do_lset(dbref player, char *what, char *flags);
void do_list_locks(dbref player, const char *arg, int lc, const char *label);
void list_locks(char *buff, char **bp, const char *name);
const char *lock_flags(lock_list *ll);
const char *lock_flags_long(lock_list *ll);
void check_zone_lock(dbref player, dbref zone, int noisy);
#define L_FLAGS(lock) ((lock)->flags)
#define L_CREATOR(lock) ((lock)->creator)
#define L_TYPE(lock) ((lock)->type)
#define L_KEY(lock) ((lock)->key)
#define L_NEXT(lock) ((lock)->next)
/* can p read/evaluate lock l on object x? */
int lock_visual(dbref, lock_type);
#define Can_Read_Lock(p,x,l)   \
    (CanSee(p, x) || controls(p,x) || ((Visual(x) || lock_visual(x, l)) && \
     eval_lock(p,x,Examine_Lock)))

/* The actual magic cookies. */
extern const lock_type Basic_Lock;
extern const lock_type Enter_Lock;
extern const lock_type Use_Lock;
extern const lock_type Zone_Lock;
extern const lock_type Page_Lock;
extern const lock_type Tport_Lock;
extern const lock_type Speech_Lock;	/* Who can speak aloud in me */
extern const lock_type Listen_Lock;	/* Who can trigger ^s/ahears on me */
extern const lock_type Command_Lock;	/* Who can use $commands on me */
extern const lock_type Parent_Lock;	/* Who can @parent to me */
extern const lock_type Link_Lock;	/* Who can @link to me */
extern const lock_type Leave_Lock;	/* Who can leave me */
extern const lock_type Drop_Lock;	/* Who can drop me */
extern const lock_type Give_Lock;	/* Who can give me */
extern const lock_type Mail_Lock;	/* Who can @mail me */
extern const lock_type Follow_Lock;	/* Who can follow me */
extern const lock_type Examine_Lock;	/* Who can examine visual me */
extern const lock_type Chzone_Lock;	/* Who can @chzone to this object? */
extern const lock_type Forward_Lock;	/* Who can @forwardlist to object? */
extern const lock_type Control_Lock;	/* Who can control this object? */
extern const lock_type Dropto_Lock;	/* Who follows the dropto of this room? */
extern const lock_type Destroy_Lock;	/* Who can @dest me if I'm dest_ok? */
extern const lock_type Interact_Lock;
extern const lock_type MailForward_Lock;	/* Who can forward mail to me */
extern const lock_type Take_Lock;	/* Who can take from the contents of this object? */
/* channel locks */
#ifdef  NEWCHAT
extern const lock_type chan_speak_lock;
extern const lock_type chan_join_lock;
extern const lock_type chan_mod_lock;
extern const lock_type chan_see_lock;
extern const lock_type chan_hide_lock;
#endif

/* Declare new lock types here! */

/* The only locktypes we don't pass Use and Command */
#define IS_passlock_type(ltp) (strcasecmp(ltp, Use_Lock) && strcasecmp(ltp, Command_Lock))

#endif				/* __LOCK_H */
