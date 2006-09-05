#ifndef _DIVISION_H_
#include "ptab.h"
#define _DIVISION_H_

/* LIMITS {{{1 */
#define MAX_LEVEL	30
#define DP_BYTES       ((ps_tab.powerbits >> 3)+1) /* Spots for powers (DP_BYTES * 8) == Max Powers. This is uped automatically */
#define MAX_DIVREC	12 /* Max Division Command Recursion */

/* PowerScope Levels {{{1 */
#define YES		3
#define	YESLTE		2
#define	YESLT		1
#define	NO		0

/* PREDEFINED LEVELS {{{1 */
#define LEVEL_GOD       	30
#define	LEVEL_DIRECTOR  	29	/* This fills the spot for wizard status */
#define LEVEL_ADMIN     	28
#define LEVEL_SYSCODER          27
#define LEVEL_SYSBUILDER        26
#define LEVEL_GENERALBUILDER    25
#define LEVEL_EMPHEAD   	24
#define LEVEL_EMPADMIN          23
#define LEVEL_EMPBUILDER        22
#define LEVEL_RPADMIN           21
#define LEVEL_BUILDER           20
#define LEVEL_ICLEADER         19
#define LEVEL_CLASS18           18
#define LEVEL_CLASS17           17
#define LEVEL_CLASS16           16
#define LEVEL_TOPADMIRAL        15
#define LEVEL_FLAGADMIRAL       14
#define LEVEL_VICEADMIRAL       13
#define LEVEL_REARADMIRAL       12
#define LEVEL_COMMODORE         11
#define LEVEL_CAPTAIN           10
#define LEVEL_COMMANDER          9
#define LEVEL_LTCOMMANDER        8
#define LEVEL_LIEUTENANT         7
#define LEVEL_JUNIORLIEUT        6
#define LEVEL_ENSIGN             5
#define LEVEL_MIDSHIPMAN         4
#define LEVEL_PLAYER             3
#define LEVEL_UNREGISTERED       2
#define LEVEL_GUEST              1

/* Old CobraMUSH PowerBits {{{1 - Kept for compatibility */
#define POW_DIVISION            1	/* @DIVCREATE/@DIVDELETE */
#define	POW_ATTACH		2	/* @ATTACH & @DETACH LTE */
#define POW_ATTACH_LT		3	/* @ATTACH LT */
#define POW_BCREATE		4	/* @PCREATE/BUILDER & @NEWPASS BUILDER */
#define	POW_EMPOWER		5	/* @EMPOWER RANKS LT */
#define POW_MODIFY		6	/* Full MODIFY */
#define POW_MODIFY_LTE		7	/* LTE */
#define	POW_MODIFY_LT		8	/* LT */
#define POW_RERANK		9	/* RERANK LTE */
#define POW_RERANK_LT		10	/* RERANKK LT */
#define	POW_SEE_ALL		11	/* Full SeeAll */
#define POW_SEE_ALL_LTE		12	/* SeeAll LTE */
#define POW_SEE_ALL_LT		13	/* SeeAll LT */
#define POW_SUMMON              14	/* Summon other players to your location */
#define POW_SUMMON_LT           15	/* Summon players of lower class level */
#define POW_JOIN                16	/* Join other players */
#define POW_JOIN_LT             17	/* Join other players of lower class level */
#define POW_ANNOUNCE            18	/* Power to use @ann, @gann and @dann */
#define POW_PRIVILEGE           19	/* Expands the functionality of basic powerz */
#define POW_TPORT_T		20	/* Tport Anything */
#define POW_TPORT_T_LTE		21	/* Tport Anything LTE */
#define POW_TPORT_T_LT		22	/* Tport Anything LT */
#define	POW_TPORT_O		23	/* Tport Anywhere */
#define	POW_TPORT_O_LTE		24	/* Tport Anywhere LTE */
#define	POW_TPORT_O_LT		25	/* Tport Anywhere LT */
#define POW_EMPIRE              26	/* Able to use empire related commands */
                 /* 27 was a YV Only Power */
#define POW_MANAGEMENT		28	/* ability to use game management commands */
#define POW_CEMIT		29	/* Can @cemit */
#define POW_NOQUOTA		30	/* Has no quota restrictions */
#define POW_PCREATE		31	/* Can @pcreate */
#define	POW_GFUNCS		32	/* Can add global @functions */
#define POW_SEARCH		33	/* Can @search anything in divscope */
#define	POW_SEARCH_LTE		34	/* Can @search anything LTE " */
#define	POW_SEARCH_LT		35	/* Can @seach LT */
#define POW_HALT		36	/* Can @halt in divscope */
#define	POW_HALT_LTE		37	/* @halt LTE */
#define	POW_HALT_LT		38	/* @halt LT */
#define	POW_IDLE		39	/* no inactivity timeout */
#define	POW_PS			40	/* Can look at anyones queuein divscope */
#define POW_PS_LTE		41	/* Can look at LTE queue */
#define	POW_PS_LT		42	/* Can look at LT queue */
#define	POW_QUEUE		43	/* queue limit of db_top */
#define	POW_PEMIT		44	/* Can @pemit HAVEN players */
#define	POW_PEMIT_LTE		45	/* @pemit LTE */
#define	POW_PEMIT_LT		46	/* @pe LT */
#define POW_CQUOTAS		47	/* Can change quotas in divscope */
#define POW_CQUOTAS_LTE		48	/* LTE */
#define	POW_CQUOTAS_LT		49	/* LT */
#define	POW_LOGIN		50	/* Login anytime */
#define POW_HIDE		51	/* Can @hide */
#define	POW_NOPAY		52	/* need no money */
#define POW_BUILD		53	/* Can use builder commands */
#define	POW_LFINGERS		54	/* can grab stuff remotely */
#define POW_LFINGERS_LTE	55	/* LTE */
#define POW_LFINGERS_LT		56	/* LT */
#define POW_BOOT		57	/* can @boot */
#define POW_BOOT_LTE		58	/* LTE */
#define POW_BOOT_LT		59	/* LT */
#define POW_POLL		60	/* can use @poll */
#define POW_LANY		61	/* Can @link to any room */
#define POW_LANY_LTE		62	/* Can @link to any room LTE */
#define POW_LANY_LT		63	/* Can @link to any room LT */
#define POW_OANY		64	/* Can @open an exit from any room */
#define	POW_OANY_LTE		65	/* LTE */
#define POW_OANY_LT		66	/* LT */
#define POW_CPRIV		67	/* Can use admin channels */
#define POW_EANNOUNCE		68	/* @Eannounce */
#define POW_DANNOUNCE		69	/* @DAnnounce */
#define	POW_NEWPASS		70	/* @newpass */
#define	POW_NEWPASS_LTE		71	/* LTE */
#define	POW_NEWPASS_LT		72	/* LT */
#define POW_VQUOTAS		73	/* Can @quota playerz */
#define POW_VQUOTAS_LTE		74
#define POW_VQUOTAS_LT		75
#define POW_MAIL		76	/* Mail Administrator */
#define POW_MAIL_LTE		77
#define POW_MAIL_LT		78
#define POW_COMBAT		79 /* Combat */
#define POW_COMBAT_LTE		80
#define POW_COMBAT_LT		81
#define POW_PROG		82 /* @Program */
#define POW_PROG_LTE		83
#define POW_PROG_LT		84
#define POW_PROGL		85 /* Prog Lock */
#define POW_PROGL_LTE		86
#define POW_PROGL_LT		87
#define POW_POWLSET		88 /* @powerlevel */
#define POW_POWLSET_LTE		89
#define POW_POWLSET_LT		90
#define POW_PASS_LOCKS		91	/* Pass all locks */
#define POW_PASS_LOCKS_LTE	92
#define POW_PASS_LOCKS_LT	93
#define POW_PWHO		94 /* Can get PrivWho */
#define POW_CHOWN               95 /* Can @chown/preserve */
#define POW_CHOWN_LTE           96
#define POW_CHOWN_LT            97
#define POW_NSPEMIT		98 /* NsPemit */
#define POW_NSPEMIT_LTE		99
#define POW_NSPEMIT_LT		100
#define POW_SQL			101 /* SQL Ok power */
#define POW_RCACHE		102 /* @READCACHE power */
#define POW_RPEMIT		103 /* Can RPMode Emit */
#define POW_RPCHAT		104 /* Can Chat in RPMODE */
#define POW_RPTEL		105 /* RPTEL.. extension to tport powers to move something in RPMODE */
#define POW_PUEBLO_SEND		106 /* Can send pueblo tags */
#define POW_MANY_ATTRIBS	107 /* Can have more than max_attrs_per_obj */


/* TYPEDEF {{{1 */
typedef struct div_table DIVISION;
typedef unsigned char * div_pbits;
typedef struct power_alias POWER_ALIAS;
typedef struct division_power_entry_t POWER;
typedef struct power_space POWERSPACE;
typedef struct power_group_t POWERGROUP;


/* MACROS {{{1 */
#define DIV(x)	(((db[x].division.object == -1) && !IsPlayer(x)) ? DIV(Owner(x)) : db[x].division)
#define SDIV(x)		(db[x].division)
#define DPBITS(x)               (SDIV(x).dp_bytes)
#define Division(x)	(db[x].division.object)
#define	SLEVEL(x)	(SDIV(x).level)
#define LEVEL(x)	(God(x) ? LEVEL_GOD : SDIV(x).level)
#define div_cansee(x,y)	((Owns(x,y) && LEVEL(x) >= LEVEL(y)) ||div_powover(x,y,"See_All"))

#define DPBIT_SET(m,n)          (m[(n) >> 3] |= (1 << ((n) & 0x7)))
#define DPBIT_CLR(m,n)          (m[(n) >> 3] &= ~(1 << ((n) & 0x7)))
#define DPBIT_ISSET(m,n)        (m[(n) >> 3] & (1 << ((n) & 0x7)))
#define DPBIT_ZERO(m)           memset(m, 0, DP_BYTES)
#define powergroup_find(key)	(POWERGROUP *) ptab_find(ps_tab.powergroups, key)
#define find_power(key)		(POWER *) ptab_find(ps_tab.powers, key)

/* Check for DPBITS() before you do anything otherwise we might crash from old code */
#define TAKE_DPBIT(m,n)         ((void) (DPBITS(m) && (SDIV(m).dp_bytes[(n) >> 3] &= ~(1 << ((n) & 0x7)))))
#define GIVE_DPBIT(m,n)         if(!DPBITS(m)) \
				  DPBITS(m) = new_power_bitmask();    \
                               ((void) (DPBITS(m) && (SDIV(m).dp_bytes[(n) >> 3] |= (1 << ((n) & 0x7)))))
#define HAS_DPBIT(m,n)          (God(m) || DPBIT_ISSET(DPBITS(m), n))

#define RESET_POWER(obj,power)  TAKE_DPBIT(obj, power->flag_yes); \
                                TAKE_DPBIT(obj, power->flag_lte); \
                                TAKE_DPBIT(obj, power->flag_lt);
#define RESET_POWER_DP(m,power) if(m) { \
                                  DPBIT_CLR(m, power->flag_yes); \
                                  DPBIT_CLR(m, power->flag_lte); \
                                  DPBIT_CLR(m, power->flag_lt); \
                                 }
/* toggle, is 1 - on, 0 - off
   marker, is 1 - wiz, 0 - royalty
   thing, is the object receiving the effects of marker powerz
   */
#define marker_powers(t, m, o) SLEVEL(o) = m ? LEVEL_DIRECTOR : LEVEL_ADMIN ; 
		      //         division_powerlevel_set(GOD, o, (t ? (m ? PL_DIRECTOR : PL_ROYALTY) : PL_PLAYER), 1);	


/* power over checks {{{1 */
#define POWC(x)         int x(int, dbref, dbref)
POWC(powc_levchk);
POWC(powc_self);
POWC(powc_levchk_lte);
POWC(powc_levset);
POWC(powc_powl_set);
POWC(powc_bcreate);

/* STRUCTS {{{1 */

struct power_space {
  PTAB *powers; /* Point to powers ptab */
  PTAB *powergroups; /* Point to powergroups ptab */
  int powerbits; /* current length of powerbits */
  char _Read_Powers_; /* Signify we actually read powers */
  div_pbits bits_taken; /* Set of all bits taken in the power array */
};

struct power_group_t {
  const char *name; /* power group name */
  div_pbits max_powers; /* Max setting for powers in power group */
  div_pbits auto_powers; /* auto settings for powers in power group */
};

struct power_group_list {
   struct power_group_t *power_group;
   struct power_group_list *next;
};

/* Division Struct on each object */
struct div_table {
    dbref object;		/* division object */
    int level;			/* rank in division system(classheld in an attribute) */
    div_pbits dp_bytes;		/* division flags/powerz */
    struct power_group_list *powergroups;
};

/* old powers conversion table */
struct old_division_power_entry_t {
    const char *name;		/* power name */
    int flag_yes;		/* yes flag */
    int flag_lte;		/* yeslte */
    int flag_lt;		/* yeslt */
};

/* new powers */
struct division_power_entry_t {
  const char *name; /* power name */
  int (*powc_chk)();
  int flag_yes;
  int flag_lte;
  int flag_lt;
};

/* Pre-Defined Power Struct */
struct new_division_power_entry_t  {
  const char *name;
  const char *type;
};

/* Pre-Defined PowerGroup Struct */
struct powergroup_text_t {
   const char *name;
   const char *max_powers;
   const char *auto_powers;
};

/* Built-in Alias Table */
struct power_alias {
  const char *alias;
  const char *realname;
};

/* PennMUSH Conversion Ptab */
struct convold_ptab {
  const char *Name;
  int Op; 
};

/* Declared External Variables {{{1 */
extern struct old_division_power_entry_t power_list[];
extern POWERSPACE ps_tab; 


/* PROTOTYPES {{{1 */
extern void division_set(dbref, dbref, const char *);
extern void division_empower(dbref, dbref, const char *);
extern int division_level(dbref, dbref, int);
extern const char *division_list_powerz(dbref,int);
extern int div_powover(dbref, dbref, const char *);
extern int div_inscope(dbref, dbref);
extern dbref create_div(dbref, const char *);
extern int check_power_yescode(div_pbits, POWER *);
extern int yescode_i(char *);
extern int dpbit_match(div_pbits, div_pbits);
extern char *yescode_str(int);
extern div_pbits string_to_dpbits(const char *powers);
extern void decompile_powers(dbref player, dbref thing, const char *name);
extern void init_powers();
extern div_pbits new_power_bitmask();
extern char power_is_zero(div_pbits pbits, int bytes);
extern void do_power_cmd(dbref, const char *, const char *, const char *);
extern void powers_read_all(FILE *);
extern void power_write_all(FILE *);
extern POWER *has_power(dbref object, const char *name);
extern div_pbits convert_old_cobra_powers(unsigned char *dp_bytes);
extern char *powergroups_list(dbref, char);
extern char *powergroups_list_on(dbref, char);
extern void powergroup_db_set(dbref, dbref, const char *, char);
extern POWER *add_power_type(const char *, const char *);
extern void do_list_powers(dbref, const char *);
extern char *list_all_powers(dbref, const char *);
extern void add_to_div_exit_path(dbref, dbref);

/* vim:ts=8 fdm=marker 
 */
#endif				/* _DIVISION_H_ */
