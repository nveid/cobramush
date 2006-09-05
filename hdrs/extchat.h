/*------------------------------------------------------------------
 * Header file for Javelin's extended @chat system
 * Based on the Battletech MUSE comsystem ported to PennMUSH by Kalkin
 *
 * Why:
 *  In the old system, channels were represented by bits set in a
 *  4-byte int on the db object. This had disadvantages - a limit
 *  of 32 channels, and players could find themselves on null channels.
 *  In addition, the old system required recompiles to permanently
 *  add channels, since the chaninfo was in the source.
 * How:
 *  Channels are a structure in a linked list.
 *  Each channel stores a whole bunch of info, including who's
 *  on it.
 *  We read/write this list using a chatdb file.
 *  We also maintain a linked list of channels that the user is
 *   connected to on the db object, which we set up at load time.
 *
 * User interface:
 * @chat channel = message
 * +channel message
 * @channel/on channel [= player] (or @channel channel = on)  do_channel()
 * @channel/off channel [= player] do_channel()
 * @channel/who channel do_channel()
 * @channel/title channel=title do_chan_title()
 * @channel/list do_chan_list()
 * @channel/add channel do_chan_admin()
 * @channel/priv channel = <privlist>  do_chan_admin()
 *  Privlist being: director, admin, private, moderated, etc.
 * @channel/joinlock channel = lock
 * @channel/speaklock channel = lock
 * @channel/modlock channel = lock
 * @channel/delete channel
 * @channel/quiet channel = yes/no
 * @channel/wipe channel
 * @channel/buffer channel = <maxlines>
 * @channel/recall channel [= <lines>]
 *
 *------------------------------------------------------------------*/

#ifndef __EXTCHAT_H
#define __EXTCHAT_H

#ifdef CHAT_SYSTEM

#include "boolexp.h"
#include "bufferq.h"

#define CU_TITLE_LEN 80

/** A channel user.
 * This structure represents an object joined to a chat channel.
 * Each chat channel maintains a linked list of users.
 */
struct chanuser {
  dbref who;			/**< Dbref of joined object */
  long int type;		/**< Bitflags for this user */
  char title[CU_TITLE_LEN];	/**< User's channel title */
  struct chanuser *next;	/**< Pointer to next user in list */
};

/* Flags and macros for channel users */
#define CU_QUIET    0x1		/* Do not hear connection messages */
#define CU_HIDE     0x2		/* Do not appear on the user list */
#define CU_GAG      0x4		/* Do not hear any messages */
#define CU_DEFAULT_FLAGS 0x0

/* channel_broadcast flags */
#define CB_CHECKQUIET 0x1	/* Check for quiet flag on recipients */
#define CB_NOSPOOF    0x2	/* Use nospoof emits */
#define CB_PRESENCE   0x4	/* This is a presence message, not sound */

#define CUdbref(u) ((u)->who)
#define CUtype(u) ((u)->type)
#define CUtitle(u) ((u)->title)
#define CUnext(u) ((u)->next)
#define Chanuser_Quiet(u)       (CUtype(u) & CU_QUIET)
#define Chanuser_Hide(u) ((CUtype(u) & CU_HIDE) || (IsPlayer(CUdbref(u)) && hidden(CUdbref(u))))
#define Chanuser_Gag(u) (CUtype(u) & CU_GAG)

/* This is a chat channel */
#define CHAN_NAME_LEN 31
#define CHAN_TITLE_LEN 256
/** A chat channel.
 * This structure represents a MUSH chat channel. Channels are organized
 * into a sorted linked list.
 */
struct channel {
  char name[CHAN_NAME_LEN];	/**< Channel name */
  char title[CHAN_TITLE_LEN];	/**< Channel description */
  long int type;		/**< Channel flags */
  long int cost;		/**< What it cost to make this channel */
  long int creator;		/**< This is who paid the cost for the channel */
  long int cobj;		/**< Channel object or #-1 */
  long int num_users;		/**< Number of connected users */
  long int max_users;		/**< Maximum allocated users */
  struct chanuser *users;	/**< Linked list of current users */
  long int num_messages;	/**< How many messages handled by this chan since startup */
  boolexp joinlock;	/**< Who may join */
  boolexp speaklock;	/**< Who may speak */
  boolexp modifylock;	/**< Who may change things and boot people */
  boolexp seelock;	/**< Who can see this in a list */
  boolexp hidelock;	/**< Who may hide from view */
  struct channel *next;		/**< Next channel in linked list */
  BUFFERQ *bufferq;		/**< Pointer to channel recall buffer queue */
};

/** A list of channels on an object.
 * This structure is a linked list of channels that is associated
 * with each object
 */
struct chanlist {
  CHAN *chan;			/**< Channel data */
  struct chanlist *next;	/**< Next channel in list */
};

#define Chanlist(x) ((struct chanlist *)get_objdata(x, "CHANNELS"))
#define s_Chanlist(x, y) set_objdata(x, "CHANNELS", (void *)y)

/** A structure for passing channel data to notify_anything */
struct na_cpass {
  CHANUSER *u;		  /**< Pointer to channel user */
  int checkquiet;	    /**< Should quiet property be checked? */
};


/* Channel type flags and macros */
#define CHANNEL_PLAYER  0x1	/* Players may join */
#define CHANNEL_OBJECT  0x2	/* Objects may join */
#define CHANNEL_DISABLED 0x4	/* Channel is turned off */
#define CHANNEL_QUIET   0x8	/* No broadcasts connect/disconnect */
#define CHANNEL_ADMIN   0x10	/* Admins only */
#define CHANNEL_DIRECTOR 0x20	/* Directors only */
#define CHANNEL_CANHIDE 0x40	/* Can non-DARK players hide here? */
#define CHANNEL_OPEN    0x80	/* Can you speak if you're not joined? */
#define CHANNEL_NOTITLES 0x100	/* Don't show titles of speakers */
#define CHANNEL_NONAMES 0x200	/* Don't show names of speakers */
#define CHANNEL_NOCEMIT 0x400	/* Disallow @cemit */
#define CHANNEL_COBJ	0x800  /* Channel with a channel object */
#define CHANNEL_INTERACT	0x1000	/* Filter channel output through interactions */
#define CHANNEL_DEFAULT_FLAGS   (CHANNEL_PLAYER)
#define CL_JOIN 0x1
#define CL_SPEAK 0x2
#define CL_MOD 0x4
#define CL_SEE 0x8
#define CL_HIDE 0x10
#define CHANNEL_COST (options.chan_cost)
#define MAX_PLAYER_CHANS (options.max_player_chans)
#define MAX_CHANNELS (options.max_channels)

const char *ChanObjName _((CHAN *c));
int ChanObjCheck _((CHAN *c));
#define ChanName(c) ((c)->name)
#define ChanObj(c) ((c)->cobj)
#define ChanType(c) ((c)->type)
#define ChanTitle(c) ((c)->title)
#define ChanCreator(c) ((c)->creator)
#define ChanObj(c) ((c)->cobj)
#define ChanCost(c) ((c)->cost)
#define ChanNumUsers(c) ((c)->num_users)
#define ChanMaxUsers(c) ((c)->max_users)
#define ChanUsers(c) ((c)->users)
#define ChanNext(c) ((c)->next)
#define ChanNumMsgs(c) ((c)->num_messages)
#define ChanJoinLock(c) ((c)->joinlock)
#define ChanSpeakLock(c) ((c)->speaklock)
#define ChanModLock(c) ((c)->modifylock)
#define ChanSeeLock(c) ((c)->seelock)
#define ChanHideLock(c) ((c)->hidelock)
#define ChanBufferQ(c) ((c)->bufferq)
#define Channel_Quiet(c)        (ChanType(c) & CHANNEL_QUIET)
#define Channel_Open(c) (ChanType(c) & CHANNEL_OPEN)
#define Channel_Object(c) (ChanType(c) & CHANNEL_OBJECT)
#define Channel_Player(c) (ChanType(c) & CHANNEL_PLAYER)
#define Channel_Disabled(c) (ChanType(c) & CHANNEL_DISABLED)
#define Channel_Director(c) (ChanType(c) & CHANNEL_DIRECTOR)
#define Channel_Admin(c) (ChanType(c) & CHANNEL_ADMIN)
#define Channel_CanHide(c) (ChanType(c) & CHANNEL_CANHIDE)
#define Channel_NoTitles(c) (ChanType(c) & CHANNEL_NOTITLES)
#define Channel_NoNames(c) (ChanType(c) & CHANNEL_NONAMES)
#define Channel_NoCemit(c) (ChanType(c) & CHANNEL_NOCEMIT)
#define Channel_Interact(c) (ChanType(c) & CHANNEL_INTERACT)
#define Chan_Ok_Type(c,o) \
        ((ChanObj(c) == o) || (IsPlayer(o) && Channel_Player(c)) || \
         (IsThing(o) && Channel_Object(c)))
#define Chan_Can(p,t) \
     (!(t & CHANNEL_DISABLED) && (!(t & CHANNEL_DIRECTOR) || Director(p)) || \
      (!(t & CHANNEL_ADMIN) || Admin(p) || div_powover(p,p,"Chat")))
/* Who can change channel privileges to type t */
#define Chan_Can_Priv(p,t,c) (!((t & CHANNEL_COBJ) && !(c & CHANNEL_COBJ)) && !(!(t & CHANNEL_COBJ) && (c & CHANNEL_COBJ)) && Chan_Can(p,t))
#define Chan_Can_Access(c,p) (Chan_Can(p,ChanType(c)))
#define Chan_Can_Join(c,p) \
     (Chan_Can_Access(c,p) && \
      (eval_chan_lock(c,p,CLOCK_JOIN)))
#define Chan_Can_Speak(c,p) \
     (Chan_Can_Access(c,p) && \
      (eval_chan_lock(c,p, CLOCK_SPEAK)))
#define Chan_Can_Cemit(c,p) \
     (!Channel_NoCemit(c) && Chan_Can_Speak(c,p))
#define Chan_Can_Modify(c,p) \
     ((ChanCreator(c) == (p) || Director(p)) ||  \
     (!Guest(p) && Chan_Can_Access(c,p) && \
      (eval_chan_lock(c,p,CLOCK_MOD))))
#define Chan_Can_See(c,p) \
     ((Admin(p) || See_All(p)) || (Chan_Can_Access(c,p) && \
				 (eval_chan_lock(c,p,CLOCK_SEE))))
#define Chan_Can_Hide(c,p) \
     (Can_Hide(p) || (Channel_CanHide(c) && Chan_Can_Access(c,p) && \
		      (eval_chan_lock(c,p,CLOCK_HIDE))))
#define Chan_Can_Nuke(c,p) (ChanCreator(c) == (p) || div_powover(p, ChanCreator(c), "Chat"))
#define Chan_Can_Decomp(c,p) (See_All(p) || (ChanCreator(c) == (p)))



     /* For use in channel matching */
enum cmatch_type { CMATCH_NONE, CMATCH_EXACT, CMATCH_PARTIAL, CMATCH_AMBIG };
#define CMATCHED(i) (((i) == CMATCH_EXACT) | ((i) == CMATCH_PARTIAL))

     /* Some globals */
extern int num_channels;
extern void WIN32_CDECL channel_broadcast
  (CHAN *channel, dbref player, int flags, const char *fmt, ...)
  __attribute__ ((__format__(__printf__, 4, 5)));
extern CHANUSER *onchannel(dbref who, CHAN *c);
extern void init_chatdb(void);
extern int load_chatdb(FILE * fp);
extern int save_chatdb(FILE * fp);
extern void do_cemit
  (dbref player, const char *name, const char *msg, int noisy);
extern void do_chan_user_flags
  (dbref player, char *name, const char *isyn, int flag, int silent);
extern void do_chan_wipe(dbref player, const char *name);
extern void do_chan_lock
  (dbref player, const char *name, const char *lockstr, int whichlock);
extern void do_chan_what(dbref player, const char *partname);
extern void do_chan_desc(dbref player, const char *name, const char *title);
extern void do_chan_title(dbref player, const char *name, const char *title);
extern void do_chan_recall(dbref player, const char *name, char *lineinfo[],
			   int quiet);
extern void do_chan_buffer(dbref player, const char *name, const char *lines);
extern void init_chat(void);
extern void do_channel
  (dbref player, const char *name, const char *target, const char *com);
extern void do_chat(dbref player, CHAN *chan, const char *arg1);
extern void do_chan_admin
  (dbref player, char *name, const char *perms, int flag);
extern enum cmatch_type find_channel(const char *p, CHAN **chan, dbref player);
extern enum cmatch_type find_channel_partial(const char *p, CHAN **chan,
					     dbref player);
extern void do_channel_list(dbref player, const char *partname);
extern int do_chat_by_name
  (dbref player, const char *name, const char *msg, int source);
extern void do_chan_decompile(dbref player, const char *name, int brief);
extern void do_chan_chown(dbref player, const char *name, const char *newowner);
extern const char *channel_description(dbref player);

enum clock_type { CLOCK_JOIN, CLOCK_SPEAK, CLOCK_SEE, CLOCK_HIDE, CLOCK_MOD };
extern int eval_chan_lock(CHAN *c, dbref p, enum clock_type type);

/** Ways to match channels by partial name */
enum chan_match_type {
  PMATCH_ALL,  /**< Match all channels */
  PMATCH_OFF,  /**< Match channels user isn't on */
  PMATCH_ON    /**< Match channels user is on */
};


#endif				/* CHAT_SYSTEM */
#endif				/* __EXTCHAT_H */
