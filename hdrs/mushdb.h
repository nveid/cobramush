/* mushdb.h */

#include "config.h"
#include "copyrite.h"

#ifndef __DB_H
#define __DB_H

/* Power macros */
#include "flags.h"

#define LastMod(x)		(db[x].lastmod)
#define TC_Builder(x)       (command_check_byname(x, "@dig"))
#define Builder(x)	    OOREF(x,TC_Builder(x), TC_Builder(ooref))
#define TC_CanModify(x,y)   (x == y || div_powover(x,y,"Modify"))
#define CanModify(x,y)	    OOREF(x,TC_CanModify(x,y), TC_CanModify(ooref,y))
#define TC_Site(x)          (God(x) || div_powover(x,x,"Site") || (Inherit_Powers(x) && div_powover(Owner(x),Owner(x),"Site")))
#define Site(x)		    OOREF(x, TC_Site(x), TC_Site(ooref))
#define Guest(x)         (LEVEL(x) == LEVEL_GUEST) /* Guest needs no twincheck */
#define TC_Tel_Anywhere(x)  (God(x))
#define Tel_Anywhere(x)	    OOREF(x,TC_Tel_Anywhere(x),TC_Tel_Anywhere(ooref))
#define Tel_Anything(x)  (God(x)) /* This needs no twincheck. This is already accounted for in dbdefs.h */
#define Tel_Where(x,y)   (Tel_Anywhere(x) || OOREF(x,div_powover(x,y,"Tel_Place"),div_powover(ooref,y,"Tel_Place")))
#define Tel_Thing(x,y)   (Tel_Anything(x) || OOREF(x,div_powover(x,y,"Tel_Thing"),div_powover(ooref,y,"Tel_Thing")))
#define TC_RPTEL(x)	(div_powover(x,x, "RPTel") || (Inherit_Powers(x) && div_powover(Owner(x), Owner(x), "RPTel")))
#define Can_RPTEL(x)	OOREF(x,TC_RPTEL(x), TC_RPTEL(x))
#define Can_BCREATE(x)	(OOREF(x,div_powover(x,x, "BCreate"), div_powover(ooref, ooref, "BCreate")))
#define See_All(x)       (God(x))
#define CanNewpass(x,y)	OOREF(x,div_powover(x,y,"Newpass"), div_powover(ooref,y,"Newpass"))
/* #define CanSee(x,y)      (God(x) || div_powover(x,y,POW_SEE_ALL)) */
#define Prived(x)        OOREF(x,div_powover(x,x,"Privilege"),div_powover(ooref,ooref,"Privilege"))
#define Priv_Who(x)      (OOREF(x,div_powover(x,x,"PrivWho"),div_powover(ooref,ooref, "PrivWho")) || Site(x))
#define Can_Hide(x)      OOREF(x,div_powover(x,x,"Hide"),div_powover(ooref,ooref,"Hide"))
#define Can_Login(x)     (div_powover(x,x,"Login")) /* This will never be checked via ooref */
#define Can_Idle(x)      (div_powover(x,x,"Idle")) /* this won't either */
#define Pass_Lock(x,y)   OOREF(x,div_powover(x,y,"Pass_Locks"),div_powover(ooref,y,"Pass_Locks"))
#define TC_IsMailAdmin(x) (God(x) || (check_power_yescode(DPBITS(x),find_power("MailAdmin")) > 0))
#define MailAdministrator(x)	OOREF(x,TC_IsMailAdmin(x),TC_IsMailAdmin(ooref))
#define MailAdmin(x,y)   OOREF(x,div_powover(x,y,"MailAdmin"),div_powover(ooref,y,"MailAdmin"))
#define TC_Long_Fingers(x) (div_powover(x,x,"Remote") || (Inherit_Powers(x) && div_powover(Owner(x),Owner(x), "Remote")))
#define Long_Fingers(x)  OOREF(x,TC_Long_Fingers(x),TC_Long_Fingers(ooref))
#define CanRemote(x,y)   OOREF(x,div_powover(x,y,"Remote"),div_powover(ooref,y,"Remote"))
#define CanOpen(x,y)     OOREF(x,div_powover(x,y,"Open"),div_powover(ooref,y,"Open"))
#define Link_Anywhere(x,y) OOREF(x,div_powover(x,y,"Link"),div_powover(ooref,y,"Link"))
#define Can_Boot(x,y)    OOREF(x,div_powover(x,y,"Boot"),div_powover(ooref,y,"Boot"))
#define Do_Quotas(x,y)   OOREF(x,div_powover(x,y,"Quota"),div_powover(ooref,y,"Quota"))
#define Change_Quota(x,y) OOREF(x,div_powover(x,y,"SetQuotas"),div_powover(ooref,y,"SetQuotas"))
#define Change_Poll(x)   OOREF(x,div_powover(x,x,"Poll"),div_powover(ooref,ooref,"Poll"))
#define HugeQueue(x)     OOREF(x,div_powover(x,x,"Queue"),div_powover(ooref,ooref,"Queue"))
#define LookQueue(x)     OOREF(x,div_powover(x,x,"See_Queue"),div_powover(ooref,ooref,"See_Queue"))
#define CanSeeQ(x,y)     OOREF(x,div_powover(x,y,"See_Queue"),div_powover(ooref,y,"See_Queue"))

#define HaltAny(x)       (Director(x) && OOREF(x,div_powover(x,x,"Halt"),div_powover(ooref,ooref,"Halt")))
#define CanHalt(x,y)     OOREF(x,div_powover(x,y,"Halt"),div_powover(ooref,y,"Halt"))
#define CanNuke(x,y)	OOREF(x,div_powover(x,y,"Nuke"),div_powover(ooref, y, "Nuke"))
#define TC_NoPay(x)         (div_powover(x,x,"NoPay") || div_powover(Owner(x),Owner(x),"NoPay"))
#define NoPay(x)	OOREF(x,TC_NoPay(x),TC_NoPay(ooref))
#define TC_MoneyAdmin(x)    (NoPay(x) && Prived(x))
#define MoneyAdmin(x)	   OOREF(x,TC_MoneyAdmin(x),TC_MoneyAdmin(ooref))
#define TC_NoQuota(x)       (div_powover(x,x,"NoQuota") || div_powover(Owner(x),Owner(x),"NoQuota"))
#define TC_DNoQuota(x)	    (!!has_power(x, "NoQuota"))
#define NoQuota(x)	(IsDivision(x) ? OOREF(x,TC_DNoQuota(x), TC_DNoQuota(ooref)) :  OOREF(x,TC_NoQuota(x),TC_NoQuota(ooref)))
#define CanSearch(x,y)   OOREF(x,(Owner(x) == Owner(y) || div_powover(x,y,"Search")),(Owner(ooref) == Owner(y) || div_powover(ooref,y,"Search") ))
#define Global_Funcs(x)  OOREF(x,div_powover(x,x,"GFuncs"),div_powover(ooref,ooref,"GFuncs"))
#define Create_Player(x) OOREF(x,div_powover(x,x,"PCreate"),div_powover(ooref,ooref,"PCreate"))
#define Can_Announce(x)  OOREF(x,div_powover(x,x,"Announce"),div_powover(ooref,ooref,"Announce"))
#define TC_Can_Cemit(x)     (div_powover(x,x,"Cemit") || (Inherit_Powers(x) && div_powover(Owner(x),Owner(x),"Cemit")))
#define Can_Cemit(x)	OOREF(x,TC_Can_Cemit(x),TC_Can_Cemit(ooref))
#define Can_Pemit(x,y)	 OOREF(x,div_powover(x,y,"Pemit"),div_powover(ooref,y,"Pemit"))
#define Can_Nspemit(x)   (div_powover(x,x,"Can_NsPemit")) 
#define CanProg(x,y)     OOREF(x,div_powover(x,y,"Program"),div_powover(ooref,y,"Program"))
#define CanProgLock(x,y) OOREF(x,div_powover(x,y,"ProgLock"),div_powover(ooref,y,"ProgLock"))
#define Sql_Ok(x)	 (Director(x) || OOREF(x,div_powover(x,x,"SQL_Ok"),div_powover(ooref,ooref,"SQL_Ok")))
#define Many_Attribs(x)	(OOREF(x,div_powover(x,x,"Many_Attribs"),div_powover(ooref,ooref,"Many_Attribs")))
#define Can_Pueblo_Send(x)	(Director(x) || OOREF(x,div_powover(x,x,"Pueblo_Send"),div_powover(ooref,ooref,"Pueblo_Send")))
#define Can_RPEMIT(x)	(div_powover(x,x, "RPEmit") || (Inherit_Powers(x) || div_powover(Owner(x),Owner(x), "RPEmit")) ||Admin(x))
#define Can_RPCHAT(x)	(div_powover(x, x, "RPChat") || (Inherit_Powers(x) || div_powover(Owner(x),Owner(x), "RPChat")) || Admin(x))
#define Inherit_Powers(x)	(Inherit(x) && Inheritable(Owner(x)))
#define CanChown(x,y) (OOREF(x,div_powover(x,y,"Chown"),div_powover(ooref,y,"Chown")))

/* Permission macros */
#define TC_Can_See_Flag(p,t,f) ((!(f->perms & (F_DARK | F_MDARK | F_ODARK | F_DISABLED)) || \
                               ((!Mistrust(p) && (Owner(p) == Owner(t))) && \
                                !(f->perms & (F_DARK | F_MDARK | F_DISABLED))) || \
                             ((div_cansee(p,t) && Admin(p)) && !(f->perms & (F_DARK | F_DISABLED))) || \
                             God(p)))
#define Can_See_Flag(p,t,f)	OOREF(p,TC_Can_See_Flag(p,t,f),TC_Can_See_Flag(ooref,t,f))

/* Can p locate x? */
int unfindable(dbref);
#define TC_Can_Locate(p,x) \
    (controls(p,x) || nearby(p,x) || CanSee(p,x) \
  || (command_check_byname(p, "@whereis") && (IsPlayer(x) && !Unfind(x) \
                     && !unfindable(Location(x))))) && (Unfind(x) ? LEVEL(p) >= LEVEL(x) : 1)
#define Can_Locate(p,x)	OOREF(p,TC_Can_Locate(p,x),TC_Can_Locate(ooref,x))


#define TC_Can_Examine(p,x)    (controls(p,x)|| \
        div_cansee(p,x) || (Visual(x) && eval_lock(p,x,Examine_Lock)))
#define Can_Examine(p,x)	OOREF(p,TC_Can_Examine(p,x),TC_Can_Examine(ooref,x))
#define CanSee(p,x)	Can_Examine(p,x)

	/***< UnUsed macro? 
	 * - RLB
#define TC_can_link(p,x)  (controls(p,x) || \
                        (IsExit(x) && (Location(x) == NOTHING)))
			*/

/* Can p link an exit to x? */
#define TC_can_link_to(p,x) \
     (GoodObject(x) \
   && (controls(p,x) || Link_Anywhere(p,x) || \
       (LinkOk(x) && eval_lock(p,x,Link_Lock))) \
   && (!NO_LINK_TO_OBJECT || IsRoom(x)))
#define can_link_to(p,x) OOREF(p,TC_can_link_to(p,x),TC_can_link_to(ooref,x))

	/* DivRead needs no TC designation */
#define Can_DivRead_Attr(p,x,a)  ((div_cansee(p,x) && !(a->flags & AF_MDARK)) \
                               || (div_cansee(p,x) && \
                                   (div_powover(p,p,"Privilege")|| (Inherit_Powers(p)  \
								  &&( div_powover(Owner(p), Owner(p),"Privilege" ))))))

/* can p access attribute a on object x? */
#define TC_Can_Read_Attr(p,x,a)  can_read_attr_internal(p,x,a) 

 /*  (God(p) || (!AF_Internal(a) && !(AF_Mdark(a) && !Admin(p)) &&  \
    can_read_attr_internal((p), (x), (a))))
   -- 
    (CanSee(p,x) || Can_DivRead_Attr(p,x,a) || \
     (!((a)->flags & AF_MDARK) && \
      (controls(p,x) || ((a)->flags & AF_VISUAL) || \
        (Visual(x) && eval_lock(p,x,Examine_Lock)) || \
       (!Mistrust(p) && (Owner((a)->creator) == Owner(p)))))))
       */
#define Can_Read_Attr(p,x,a) OOREF(p,TC_Can_Read_Attr(p,x,a), TC_Can_Read_Attr(ooref,x,a))

/** can p look at object x? */
#define TC_can_look_at(p, x) \
      (Long_Fingers(p) || nearby(p, x) || \
            (nearby(p, Location(x)) && \
                   (!Opaque(Location(x)) || controls(p, Location(x)))) || \
            (nearby(Location(p), x) && \
                   (!Opaque(Location(p)) || controls(p, Location(p)))))
#define can_look_at(p, x) OOREF(p,TC_can_look_at(p,x), TC_can_look_at(ooref,x))

 /* can anyone access attribute a on object x? */
#define Is_Visible_Attr(x,a)   \
    (!AF_Internal(a) && \
     can_read_attr_internal(NOTHING, (x), (a)))


/* can p write attribute a on object x, assuming p may modify x?
 */
#define TC_Can_Write_Attr(p,x,a) can_write_attr_internal((p), (x), (a), 1)
#define Can_Write_Attr(p,x,a)	OOREF(p,TC_Can_Write_Attr(p,x,a),TC_Can_Write_Attr(ooref,x,a))
#define TC_Can_Write_Attr_Ignore_Safe(p,x,a)  can_write_attr_internal(p,x,a, 0)
#define Can_Write_Attr_Ignore_Safe(p,x,a) \
		OOREF(p,TC_Can_Write_Attr_Ignore_Safe(p,x,a), TC_Can_Write_Attr_Ignore_Safe(ooref,x,a))
  /*
#define Can_Write_Attr(p,x,a)  \
   (God(p) || \
    (!((a)->flags & AF_INTERNAL) && \
     !((a)->flags & AF_SAFE) && \
       (((a)->creator == Owner(p)) || !((a)->flags & AF_LOCKED)) \
   ))
#define Can_Write_Attr_Ignore_Safe(p,x,a)  \
   (God(p) || \
    (!((a)->flags & AF_INTERNAL) && \
       (((a)->creator == Owner(p)) || !((a)->flags & AF_LOCKED)) \
   ))
   */


/* Can p forward a message to x (via @forwardlist)? */
#define Can_Forward(p,x)  \
    (controls(p,x) || div_powover(p,x,"Pemit") || \
        ((getlock(x, Forward_Lock) != TRUE_BOOLEXP) && \
         eval_lock(p, x, Forward_Lock)))

/* Can p forward a mail message to x (via @mailforwardlist) ? */
#define Can_MailForward(p,x)  \
    (IsPlayer(x) && (controls(p,x) || \
        ((getlock(x, MailForward_Lock) != TRUE_BOOLEXP) && \
         eval_lock(p, x, MailForward_Lock))))

/* Can from pass to's @lock/interact? */
#define Pass_Interact_Lock(from,to) \
  (Can_Pemit(from, to) || Loud(from) || eval_lock(from, to, Interact_Lock))

/* How many pennies can you have? */
#define TC_Max_Pennies(p) (Guest(p) ? MAX_GUEST_PENNIES : MAX_PENNIES)
#define Max_Pennies(p)		OOREF(p,TC_Max_Pennies(p),TC_Max_Pennies(ooref))
#define TC_Paycheck(p) (Guest(p) ? GUEST_PAY_CHECK : PAY_CHECK)
#define Paycheck(p)	OOREF(p,TC_Paycheck(p), TC_Paycheck(ooref))

/* DB flag macros - these should be defined whether or not the
 * corresponding system option is defined 
 * They are successive binary numbers
 */
#define DBF_NO_CHAT_SYSTEM      0x01
#define DBF_WARNINGS            0x02
#define DBF_CREATION_TIMES      0x04
#define DBF_NO_POWERS           0x08
#define DBF_NEW_LOCKS           0x10
#define DBF_NEW_STRINGS         0x20
#define DBF_TYPE_GARBAGE        0x40
#define DBF_SPLIT_IMMORTAL      0x80
#define DBF_NO_TEMPLE           0x100
#define DBF_LESS_GARBAGE        0x200
#define DBF_AF_VISUAL           0x400
#define DBF_VALUE_IS_COST       0x800
#define DBF_LINK_ANYWHERE       0x1000
#define DBF_NO_STARTUP_FLAG     0x2000
#define DBF_PANIC               0x4000
#define DBF_AF_NODUMP           0x8000
#define DBF_SPIFFY_LOCKS        0x10000
#define DBF_NEW_FLAGS           0x20000
#define DBF_DIVISIONS		0x40000
#define DBF_LABELS		0x100000
#define DBF_NEW_ATR_LOCK	0x200000

#define FLAG_DBF_CQUOTA_RENAME  0x01  /* Rename CQuota Power to SetQuotas */

#define HAS_COBRADBFLAG(x,y)	(!(x & DBF_TYPE_GARBAGE) && (x & y)) /* Macro exists so cobra & penn dbflags can exist as same DBFs */

#define IS_COBRA_DB(x) (!(x & DBF_TYPE_GARBAGE))

/* Reboot DB flag macros - these should be defined whether or not the
 * corresponding system option is defined 
 * They are successive binary numbers
 */
#define RDBF_SCREENSIZE         0x01
#define RDBF_TTYPE              0x02
#define RDBF_PUEBLO_CHECKSUM    0x04
/* Available: 0x08 - 0x8000 */
#define RDBF_SU_EXIT_PATH	0x00010000

#endif				/* __DB_H */
