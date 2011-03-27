/**
 * \file externs.h
 *
 * \brief Header file for external functions called from many source files.
 *
 *
 */


#ifndef __EXTERNS_H
#define __EXTERNS_H
/* Get the time_t definition that we use in prototypes here */
#include <time.h>
#ifdef I_LIBINTL
#include <libintl.h>
#endif
#if defined(HAS_GETTEXT) && !defined(DONT_TRANSLATE)
/** Macro for a translated string */
#define T(str) gettext(str)
/** Macro to note that a string has a translation but not to translate */
#define N_(str) gettext_noop(str)
#else
#define T(str) str
#define N_(str) str
#endif
#include "config.h"
#include "copyrite.h"
#include "compile.h"
#include "mushtype.h"
#include "dbdefs.h"
#include "confmagic.h"
#ifndef HAS_STRCASECMP
#ifdef WIN32
#define strcasecmp(s1,s2) _stricmp((s1), (s2))
#define strncasecmp(s1,s2,n) _strnicmp((s1), (s2), (n))
#else
extern int strcasecmp(const char *s1, const char *s2);
extern int strncasecmp(const char *s1, const char *s2, size_t n);
#endif
#endif

/* General Messages */
#define MSG_HUH	T("Huh? (Type \"Help\" for Help.)") 

/* these symbols must be defined by the interface */
extern time_t mudtime;

#define FOPEN_READ "rb"	     /**< Arguments to fopen when reading */
#define FOPEN_WRITE "wb"     /**< Arguments to fopen when writing */

extern int shutdown_flag;	/* if non-zero, interface should shut down */
extern void emergency_shutdown(void);
extern void boot_desc(DESC *d);	/* remove a player */
extern DESC *player_desc(dbref player);	/* find descriptors */
extern DESC *inactive_desc(dbref player);	/* find descriptors */
extern DESC *port_desc(int port);	/* find descriptors */
extern void WIN32_CDECL flag_broadcast(const char *flag1,
				       const char *flag2, const char *fmt, ...)
  __attribute__ ((__format__(__printf__, 3, 4)));

extern void raw_notify(dbref player, const char *msg);
extern void notify_list(dbref speaker, dbref thing, const char *atr,
			const char *msg, int flags);
extern dbref short_page(const char *match);
extern dbref visible_short_page(dbref player, const char *match);
extern void do_doing(dbref player, const char *message);

/* from funtime.c */
extern int etime_to_secs(char *str1, int *secs);

/* the following symbols are provided by game.c */
extern void process_command(dbref player, char *command,
			    dbref cause, dbref realcause, int from_port);
extern void init_qids();
extern int init_game_dbs(void);
extern void init_game_postdb(const char *conf);
extern void init_game_config(const char *conf);
extern void dump_database(void);
extern void NORETURN mush_panic(const char *message);
extern void NORETURN mush_panicf(const char *fmt, ...)
  __attribute__ ((__format__(__printf__, 1, 2)));
extern char *scan_list(dbref player, char *command);


#ifdef WIN32
/* From timer.c */
extern void init_timer(void);
#endif                          /* WIN32 */

/* From attrib.c */
extern dbref atr_on_obj;

/* From bsd.c */
extern FILE *connlog_fp;
extern FILE *checklog_fp;
extern FILE *wizlog_fp;
extern FILE *tracelog_fp;
extern FILE *cmdlog_fp;
extern int restarting;
#ifdef SUN_OS
extern int f_close(FILE * file);
/** SunOS fclose macro */
#define fclose(f) f_close(f);
#endif
extern int hidden(dbref player);
extern dbref guest_to_connect(dbref player);
void dump_reboot_db(void);
void close_ssl_connections(void);
int least_idle_time(dbref player);
int least_idle_time_priv(dbref player);
int most_conn_time(dbref player);
int most_conn_time_priv(dbref player);
char *least_idle_ip(dbref player);
char *least_idle_hostname(dbref player);
extern int do_command(DESC *d, char *command);

/* sql.c */
extern void sql_shutdown(void);

/* The #defs for our notify_anything hacks.. Errr. Functions */
#define NA_NORELAY      0x0001	/**< Don't relay sound */
#define NA_NOENTER      0x0002	/**< No newline at end */
#define NA_NOLISTEN     0x0004	/**< Implies NORELAY. Sorta. */
#define NA_NOPENTER     0x0010	/**< No newline, Pueblo-stylee */
#define NA_PONLY        0x0020	/**< Pueblo-only */
#define NA_PUPPET       0x0040	/**< Ok to puppet */
#define NA_PUPPET2      0x0080	/**< Message to a player from a puppet */
#define NA_MUST_PUPPET  0x0100	/**< Ok to puppet even in same room */
#define NA_INTER_HEAR   0x0200	/**< Message is auditory in nature */
#define NA_INTER_SEE    0x0400	/**< Message is visual in nature */
#define NA_INTER_PRESENCE  0x0800	/**< Message is about presence */
#define NA_NOSPOOF        0x1000	/**< Message comes via a NO_SPOOF object. */
#define NA_PARANOID       0x2000	/**< Message comes via a PARANOID object. */
#define NA_NOPREFIX       0x4000	/**< Don't use @prefix when forwarding */
#define NA_INTER_LOCK	0x10000		/**< Message subject to @lock/interact even if not otherwise marked */
#define NA_INTERACTION  (NA_INTER_HEAR|NA_INTER_SEE|NA_INTER_PRESENCE|NA_INTER_LOCK)	/**< Message follows interaction rules */
#define NA_SPOOF        0x8000	/**< @ns* message, overrides NOSPOOF */
#define NA_EVALONCONTACT 0x20000

/** A notify_anything lookup function type definition */
typedef dbref (*na_lookup) (dbref, void *);
extern void notify_anything(dbref speaker, na_lookup func,
			    void *fdata,
			    char *(*nsfunc) (dbref,
					     na_lookup func,
					     void *, int), int flags,
			    const char *message);
extern void notify_anything_format(dbref speaker, na_lookup func,
				   void *fdata,
				   char *(*nsfunc) (dbref,
						    na_lookup func,
						    void *, int), int flags,
				   const char *fmt, ...)
  __attribute__ ((__format__(__printf__, 6, 7)));
extern void notify_anything_loc(dbref speaker, na_lookup func,
				void *fdata,
				char *(*nsfunc) (dbref,
						 na_lookup func,
						 void *, int), int flags,
				const char *message, dbref loc);
extern dbref na_one(dbref current, void *data);
extern dbref na_next(dbref current, void *data);
extern dbref na_loc(dbref current, void *data);
extern dbref na_jloc(dbref current, void *data);
extern dbref na_nextbut(dbref current, void *data);
extern dbref na_except(dbref current, void *data);
extern dbref na_except2(dbref current, void *data);
extern dbref na_exceptN(dbref current, void *data);
extern dbref na_channel(dbref current, void *data);
extern char *ns_esnotify(dbref speaker, na_lookup func, void *fdata, int para);

/** Basic 'notify player with message */
#define notify(p,m)           notify_anything(orator, na_one, &(p), NULL, 0, m)
/** Notify puppet with message, even if owner's there */
#define notify_must_puppet(p,m)           notify_anything(orator, na_one, &(p), NULL, NA_MUST_PUPPET, m)
/** Notify player with message, as if from somethign specific */
#define notify_by(t,p,m)           notify_anything(t, na_one, &(p), NULL, 0, m)
/** Notfy player with message, but only puppet propagation */
#define notify_noecho(p,m)    notify_anything(orator, na_one, &(p), NULL, NA_NORELAY | NA_PUPPET, m)
/** Notify player with message if they're not set QUIET */
#define quiet_notify(p,m)     if (!IsQuiet(p)) notify(p,m)
extern void notify_format(dbref player, const char *fmt, ...)
  __attribute__ ((__format__(__printf__, 2, 3)));

/* From compress.c */
/* Define this to get some statistics on the attribute compression
 * in @stats/tables. Only for word-based compression (COMPRESSION_TYPE 3 or 4
 */
/* #define COMP_STATS /* */
#if (COMPRESSION_TYPE != 0)
extern unsigned char *
compress(char const *s)
  __attribute_malloc__;
    extern char *uncompress(unsigned char const *s);
    extern char *safe_uncompress(unsigned char const *s) __attribute_malloc__;
#else
extern char ucbuff[];
#define init_compress(f) 0
#define compress(s) ((unsigned char *)strdup(s))
#define uncompress(s) (strcpy(ucbuff, (char *) s))
#define safe_uncompress(s) (strdup((char *) s))
#endif

/* From cque.c */
struct real_pcre;
struct eval_context {
  char *wenv[10];                 /**< working environment (%0-%9) */
  char renv[NUMQ][BUFFER_LEN];    /**< working registers q0-q9,qa-qz */
  char *wnxt[10];                 /**< environment to shove into the queue */
  char *rnxt[NUMQ];               /**< registers to shove into the queue */
  int process_command_port;   /**< port number that a command came from */
  dbref cplr;                     /**< initiating player */
  char ccom[BUFFER_LEN];          /**< raw command */
  char ucom[BUFFER_LEN];      /**< evaluated command */
  int break_called;           /**< Has the break command been called? */
  char break_replace[BUFFER_LEN];  /**< What to replace the break with */
  struct real_pcre *re_code;		  /**< The compiled re */
  int re_subpatterns;	      /**< The number of re subpatterns */
  int *re_offsets;	      /**< The offsets for the subpatterns */
  char *re_from;	      /**< The positions of the subpatterns */
  HASHTAB namedregs;
  HASHTAB namedregsnxt;
};

typedef struct eval_context EVAL_CONTEXT;
extern EVAL_CONTEXT global_eval_context;
/*
extern char *wenv[10], renv[NUMQ][BUFFER_LEN];
extern char *wnxt[10], *rnxt[NUMQ];
*/
#ifdef _SWMP_
extern int sql_env[2]; /* Sql Environment */
#endif
extern void do_second(void);
extern int do_top(int ncom);
extern void do_halt(dbref owner, const char *ncom, dbref victim);
extern void parse_que(dbref player, const char *command, dbref cause);
extern void div_parse_que(dbref division, const char *command, dbref called_division, dbref player);
extern int queue_attribute_base
  (dbref executor, const char *atrname, dbref enactor, int noparent);
extern ATTR *queue_attribute_getatr(dbref executor, const char *atrname,
                                    int noparent);
extern int queue_attribute_useatr(dbref executor, ATTR *a, dbref enactor);

/** Queue the code in an attribute, including parent objects */
#define queue_attribute(a,b,c) queue_attribute_base(a,b,c,0)
/** Queue the code in an attribute, excluding parent objects */
#define queue_attribute_noparent(a,b,c) queue_attribute_base(a,b,c,1)
extern void dequeue_semaphores(dbref thing, char const *aname, int count,
                               int all, int drain);
extern void shutdown_queues(void);
extern void do_hourly(void);

extern void init_namedregs(HASHTAB *);
extern void free_namedregs(HASHTAB *);
extern void clear_namedregs(HASHTAB *);
extern void copy_namedregs(HASHTAB *, HASHTAB *);
extern void set_namedreg(HASHTAB *, const char *, const char *);
extern const char *get_namedreg(HASHTAB *, const char *);


/* From create.c */
extern dbref do_dig(dbref player, const char *name, char **argv, int tport);
extern dbref do_create(dbref player, char *name, int cost);
extern dbref do_real_open(dbref player, const char *direction,
                          const char *linkto, dbref pseudo);
extern void do_open(dbref player, const char *direction, char **links);
extern void do_link(dbref player, const char *name, const char *room_name,
                    int preserve);
extern void do_unlink(dbref player, const char *name);
extern dbref do_clone(dbref player, char *name, char *newname, int preserve);
extern void copy_zone(dbref executor, dbref zmo);

/* From game.c */
extern void report(void);
extern int Hearer(dbref thing);
extern int Commer(dbref thing);
extern int Listener(dbref thing);
extern dbref orator;
extern dbref ooref;
extern dbref tooref;

int parse_chat(dbref player, char *command);
extern void fork_and_dump(int forking);
void reserve_fd(void);
void release_fd(void);


/* From look.c */
/** Enumeration of types of looks that can be performed */
enum look_type { LOOK_NORMAL, LOOK_TRANS, LOOK_AUTO, LOOK_CLOUDYTRANS,
  LOOK_CLOUDY
};
extern void look_room(dbref player, dbref loc, enum look_type style);
extern void do_look_around(dbref player);
extern void do_look_at(dbref player, const char *name, int key);
extern char *decompose_str(char *what);

/* From memcheck.c */
extern void add_check(const char *ref);
extern void del_check(const char *ref);
extern void list_mem_check(dbref player);
extern void log_mem_check(void);

/* From move.c */
extern void enter_room(dbref player, dbref loc, int nomovemsgs);
extern int can_move(dbref player, const char *direction);
/** Enumeration of types of movements that can be performed */
enum move_type { MOVE_NORMAL, MOVE_GLOBAL, MOVE_ZONE };
extern void do_move(dbref player, const char *direction, enum move_type type);
extern void moveto(dbref what, dbref where);
extern void safe_tel(dbref player, dbref dest, int nomovemsgs);
extern int global_exit(dbref player, const char *direction);
extern int remote_exit(dbref loc, const char *direction);
extern void move_wrapper(dbref player, const char *command);
extern void do_follow(dbref player, const char *arg);
extern void do_unfollow(dbref player, const char *arg);
extern void do_desert(dbref player, const char *arg);
extern void do_dismiss(dbref player, const char *arg);
extern void clear_followers(dbref leader, int noisy);
extern void clear_following(dbref follower, int noisy);

/* From mycrypt.c */
extern char *mush_crypt(const char *key);

/* From player.c */
extern int password_check(dbref player, const char *password);
extern dbref lookup_player(const char *name);
extern dbref lookup_player_name(const char *name);
/* from player.c */
extern dbref create_player(const char *name, const char *password,
                           const char *host, const char *ip);
extern dbref connect_player(const char *name, const char *password,
                            const char *host, const char *ip, char *errbuf);
extern void check_last(dbref player, const char *host, const char *ip);
extern void check_lastfailed(dbref player, const char *host);

/* From parse.c */
extern int is_number(const char *str);
extern int is_strict_number(const char *str);
extern int is_strict_integer(const char *str);
int is_good_number(double);

/* From plyrlist.c */
void clear_players(void);
void add_player(dbref player);
void add_player_alias(dbref player, const char *alias);
void delete_player(dbref player, const char *alias);
void reset_player_list(dbref player, const char *oldname, const char *oldalias,
                       const char *name, const char *alias);

/* From predicat.c */
extern int pay_quota(dbref, int);
extern char *WIN32_CDECL tprintf(const char *fmt, ...)
  __attribute__ ((__format__(__printf__, 1, 2)));

extern int could_doit(dbref player, dbref thing);
extern int did_it(dbref player, dbref thing, const char *what,
		  const char *def, const char *owhat, const char *odef,
		  const char *awhat, dbref loc);
extern int did_it_with(dbref player, dbref thing, const char *what,
			const char *def, const char *owhat, const char *odef,
			const char *awhat, dbref loc, dbref env0, dbref env1,
			int flags);
extern int did_it_interact(dbref player, dbref thing, const char *what,
			   const char *def, const char *owhat,
			   const char *odef, const char *awhat, dbref loc,
			   int flags);
extern int real_did_it(dbref player, dbref thing, const char *what,
		       const char *def, const char *owhat, const char *odef,
		       const char *awhat, dbref loc, char *myenv[10],
		       int flags);
extern int can_see(dbref player, dbref thing, int can_see_loc);
extern int controls(dbref who, dbref what);
extern int can_pay_fees(dbref who, int pennies);
extern void giveto(dbref who, int pennies);
extern int payfor(dbref who, int cost);
extern int quiet_payfor(dbref who, int cost);
extern int nearby(dbref obj1, dbref obj2);
extern int get_current_quota(dbref who);
extern void change_quota(dbref who, int payment);
extern int ok_name(const char *name);
extern int ok_command_name(const char *name);
extern int ok_function_name(const char *name);
extern int ok_player_name(const char *name, dbref player, dbref thing);
extern int ok_player_alias(const char *alias, dbref player, dbref thing);
extern int ok_password(const char *password);
extern int ok_tag_attribute(dbref player, char *params);
extern dbref parse_match_possessor(dbref player, const char **str);
extern void page_return(dbref player, dbref target, const char *type,
			const char *message, const char *def);
extern char *grep_util(dbref player, dbref thing, char *pattern,
		       char *lookfor, int len, int insensitive);
extern dbref where_is(dbref thing);
extern int charge_action(dbref player, dbref thing, const char *awhat);
dbref first_visible(dbref player, dbref thing);

/* From set.c */
extern void chown_object(dbref player, dbref thing, dbref newowner,
                         int preserve);

/* From speech.c */
const char *spname(dbref thing);
extern void notify_except(dbref first, dbref exception, const char *msg,
                          int flags);
extern void notify_except2(dbref first, dbref exc1, dbref exc2,
                           const char *msg, int flags);
/* Return thing/PREFIX + msg */
extern void make_prefixstr(dbref thing, const char *msg, char *tbuf1);
extern int filter_found(dbref thing, const char *msg, int flag);

/* From division.c */
extern void adjust_powers(dbref obj, dbref to);
extern void clear_division(dbref divi);
extern POWERSPACE ps_tab;

/* From prog.c */
extern void prog_load_desc(DESC *d);
extern int prog_handler(DESC *d, char *input);

#ifdef COBJ
/* From chanobj.c */
extern void do_set_cobj(dbref player, const char *name, const char *obj);
extern void do_reset_cobj(dbref player, const char *name);
extern const char *ChanObjName(CHAN *c);
#endif

#ifdef RPMODE_SYS
/* From rplog.c */
extern void rplog_shutdown(void);
extern void rplog_shutdown_room(dbref room);
extern void rplog_init_room(dbref room);
extern void init_rplogs(void);
extern void rplog_room(dbref room, dbref player, char *str);
extern void rplog_reset(void);
#endif

/* from rob.c */
/* Not working? Somethig wrong with GCC?*/
extern void s_Pennies(dbref thing, int amount);

/* From strutil.c */
extern char *split_token(char **sp, char sep);
extern char *chopstr(const char *str, size_t lim);
extern int string_prefix(const char *RESTRICT string,
                         const char *RESTRICT prefix);
extern const char *string_match(const char *src, const char *sub);
extern char *strupper(const char *s);
extern char *strlower(const char *s);
extern char *strinitial(const char *s);
extern char *upcasestr(char *s);
extern char *skip_space(const char *s);
extern char *seek_char(const char *s, char c);
extern int u_strlen(const unsigned char *s);
extern unsigned char *u_strcpy
  (unsigned char *target, const unsigned char *source);
/** Unsigned char strdup */
#define u_strdup(x) (unsigned char *)strdup((char *) x)
#ifndef HAS_STRDUP
char *
strdup(const char *s)
  __attribute_malloc__;
#endif
    char *mush_strdup(const char *s, const char *check) __attribute_malloc__;
#ifdef WIN32
#ifndef HAS_VSNPRINTF
#define HAS_VSNPRINTF
#define vsnprintf _vsnprintf
#endif
#endif
    extern char *remove_markup(const char *orig, size_t * stripped_len);
    extern char *skip_leading_ansi(const char *s);

/** A string, with ansi attributes broken out from the text */
    typedef struct {
      char text[BUFFER_LEN];    /**< Text of the string */
      char *codes[BUFFER_LEN];  /**< Ansi codes associated with each char of text */
      size_t len;       /**< Length of text */
    } ansi_string;


    extern ansi_string *parse_ansi_string(const char *src) __attribute_malloc__;
    extern void flip_ansi_string(ansi_string *as);
    extern void free_ansi_string(ansi_string *as);
    extern void populate_codes(ansi_string *as);
    extern void depopulate_codes(ansi_string *as);
#ifdef WIN32
#define strncoll(s1,s2,n) _strncoll((s1), (s2), (n))
#define strcasecoll(s1,s2) _stricoll((s1), (s2))
#define strncasecoll(s1,s2,n) _strnicoll((s1), (s2), (n))
#else
    extern int strncoll(const char *s1, const char *s2, size_t t);
    extern int strcasecoll(const char *s1, const char *s2);
    extern int strncasecoll(const char *s1, const char *s2, size_t t);
#endif

/** Append a character to the end of a BUFFER_LEN long string.
 * You shouldn't use arguments with side effects with this macro.
 */
#define safe_chr(x, buf, bp) \
                    ((*(bp) - (buf) >= BUFFER_LEN - 1) ? \
                        1 : (*(*(bp))++ = (x), 0))
/* Like sprintf */
    extern int safe_format(char *buff, char **bp, const char *RESTRICT fmt, ...)
  __attribute__ ((__format__(__printf__, 3, 4)));
/* Append an int to the end of a buffer */
    extern int safe_integer(long i, char *buff, char **bp);
    extern int safe_uinteger(unsigned long, char *buff, char **bp);
/* Same, but for a SBUF_LEN buffer, not BUFFER_LEN */
#define SBUF_LEN 64    /**< A short buffer */
    extern int safe_integer_sbuf(long i, char *buff, char **bp);
/* Append a NVAL to a string */
    extern int safe_number(NVAL n, char *buff, char **bp);
/* Append a dbref to a buffer */
    extern int safe_dbref(dbref d, char *buff, char **bp);
/* Append a string to a buffer */
    extern int safe_str(const char *s, char *buff, char **bp);
/* Append a string to a buffer, sticking it in quotes if there's a space */
    extern int safe_str_space(const char *s, char *buff, char **bp);
/* Append len characters of a string to a buffer */
    extern int safe_strl(const char *s, int len, char *buff, char **bp);
/** Append a boolean to the end of a string */
#define safe_boolean(x, buf, bufp) \
                safe_chr((x) ? '1' : '0', (buf), (bufp))
/* Append X characters to the end of a string, taking ansi and html codes into
   account. */
extern int safe_ansi_string(ansi_string *as, size_t start, size_t len, char *buff, char **bp);
extern int safe_ansi_string2(ansi_string *as, size_t start, size_t len, char *buff, char **bp);

/* Append N copies of the character X to the end of a string */
    extern int safe_fill(char x, size_t n, char *buff, char **bp);
/* Append an accented string */
    extern int safe_accent(const char *RESTRICT base,
			   const char *RESTRICT tmplate, size_t len, char *buff,
			   char **bp);

   extern char *str_escaped_chr(const char *RESTRICT string, char escape_chr);
   extern char *mush_strncpy(char *restrict, const char *, size_t);
   extern char *replace_string
      (const char *RESTRICT old, const char *RESTRICT newbit,
       const char *RESTRICT string) __attribute_malloc__;
    extern char *replace_string2(const char *old[2], const char *newbits[2],
                                 const char *RESTRICT string)
 __attribute_malloc__;
    extern const char *standard_tokens[2];      /* ## and #@ */
    extern char *trim_space_sep(char *str, char sep);
    extern int do_wordcount(char *str, char sep);
    extern char *remove_word(char *list, char *word, char sep);
    extern char *next_in_list(const char **head);
    extern void safe_itemizer(int cur_num, int done, const char *delim,
                              const char *conjoin, const char *space,
                              char *buff, char **bp);
    extern char *show_time(time_t t, int utc);
    extern char *show_tm(struct tm *t);


/** This structure associates html entities and base ascii representations */
    typedef struct {
      const char *base;         /**< Base ascii representation */
      const char *entity;       /**< HTML entity */
    } accent_info;

    extern accent_info accent_table[];

    extern int ansi_strlen(const char *string);
    extern int ansi_strnlen(const char *string, size_t numchars);

/* From unparse.c */
    const char *real_unparse
      (dbref player, dbref loc, int obey_myopic, int use_nameformat,
       int use_nameaccent);
    extern const char *unparse_object(dbref player, dbref loc);
/** For back compatibility, an alias for unparse_object */
#define object_header(p,l) unparse_object(p,l)
    extern const char *unparse_object_myopic(dbref player, dbref loc);
    extern const char *unparse_room(dbref player, dbref loc);
    extern int nameformat(dbref player, dbref loc, char *tbuf1, char *defname);
    extern const char *accented_name(dbref thing);

/* From utils.c */
    extern void parse_attrib(dbref player, char *str, dbref *thing,
                             ATTR **attrib);
    extern void parse_anon_attrib(dbref player, char *str, dbref *thing,
                                  ATTR **attrib);
    extern void free_anon_attrib(ATTR *attrib);
    typedef struct _ufun_attrib {
      dbref thing;
      char contents[BUFFER_LEN];
      int pe_flags;
      char *errmess;
    } ufun_attrib;
    extern int fetch_ufun_attrib(char *attrname, dbref executor,
                                 ufun_attrib * ufun, int accept_lambda);
    extern int call_ufun(ufun_attrib * ufun, char **wenv_args, int wenv_argc,
                         char *ret, dbref executor, dbref enactor,
                         PE_Info * pe_info);
    extern int member(dbref thing, dbref list);
    extern int recursive_member(dbref disallow, dbref from, int count);
    extern dbref remove_first(dbref first, dbref what);
    extern dbref reverse(dbref list);
    extern Malloc_t mush_malloc(size_t size,
                                const char *check) __attribute_malloc__;
    extern void mush_free(Malloc_t RESTRICT ptr, const char *RESTRICT check);
    extern long get_random_long(long low, long high);
    extern char *fullalias(dbref it);
    extern char *shortalias(dbref it);
    extern char *shortname(dbref it);
    extern dbref absolute_room(dbref it);
    int can_interact(dbref from, dbref to, int type);


/* From warnings.c */
    extern void run_topology(void);
    extern void do_warnings(dbref player, const char *name, const char *warns);
    extern void do_wcheck(dbref player, const char *name);
    extern void do_wcheck_me(dbref player);
    extern void do_wcheck_all(dbref player);
    extern void set_initial_warnings(dbref player);
    extern const char *unparse_warnings(warn_type warnings);
    extern warn_type parse_warnings(dbref player, const char *warnings);

/* From wild.c */
    extern int local_wild_match_case(const char *RESTRICT s,
                                     const char *RESTRICT d, int cs);
    extern int wildcard(const char *s);
    extern int quick_wild_new(const char *RESTRICT tstr,
                              const char *RESTRICT dstr, int cs);
    extern int regexp_match_case_r(const char *RESTRICT s,
                                   const char *RESTRICT d, int cs, char **, int,
                                   char *, int);
    extern int quick_regexp_match(const char *RESTRICT s,
                                  const char *RESTRICT d, int cs);
    extern int wild_match_case_r(const char *RESTRICT s, const char *RESTRICT d,
                                 int cs, char **, int, char *, int);
    extern int quick_wild(const char *RESTRICT tsr, const char *RESTRICT dstr);
    extern int atr_wild(const char *RESTRICT tstr, const char *RESTRICT dstr);
/** Default (case-insensitive) local wildcard match */
#define local_wild_match(s,d) local_wild_match_case(s, d, 0)

/** Types of lists */

    extern char ALPHANUM_LIST[];
    extern char INSENS_ALPHANUM_LIST[];
    extern char DBREF_LIST[];
    extern char NUMERIC_LIST[];
    extern char FLOAT_LIST[];
    extern char DBREF_NAME_LIST[];
    extern char DBREF_NAMEI_LIST[];
    extern char DBREF_IDLE_LIST[];
    extern char DBREF_CONN_LIST[];
    extern char DBREF_CTIME_LIST[];
    extern char DBREF_OWNER_LIST[];
    extern char DBREF_LOCATION_LIST[];
    extern char DBREF_ATTR_LIST[];
    extern char DBREF_ATTRI_LIST[];
    extern char *UNKNOWN_LIST;

/* From function.c and other fun*.c */
    extern char *strip_braces(char const *line);
    extern void save_global_regs(const char *funcname, char *preserve[]);
    extern void restore_global_regs(const char *funcname, char *preserve[]);
    extern void free_global_regs(const char *funcname, char *preserve[]);
    extern void init_global_regs(char *preserve[]);
    extern void load_global_regs(char *preserve[]);
    extern void save_global_env(const char *funcname, char *preserve[]);
    extern void restore_global_env(const char *funcname, char *preserve[]);
    extern void save_global_nxt(const char *funcname, char *preservew[],
				char *preserver[], char *valw[], char *valr[]);
    extern void restore_global_nxt(const char *funcname, char *preservew[],
				   char *preserver[], char *valw[],
				   char *valr[]);
    extern int delim_check(char *buff, char **bp, int nfargs, char **fargs,
			   int sep_arg, char *sep);
    extern int get_gender(dbref player);
    extern int gencomp(dbref player, char *a, char *b, char *sort_type);
    extern const char *do_get_attrib(dbref executor, dbref thing,
				     const char *aname);
    extern char *ArabicToRoman(int);
    extern int RomanToArabic(char *);

/* From destroy.c */
    void do_undestroy(dbref player, char *name);
    dbref free_get(void);
    void fix_free_list(void);
    void purge(void);
    void do_purge(dbref player);
    void free_object(dbref thing);

    void dbck(void);
    int undestroy(dbref player, dbref thing);

/* From db.c */

    extern const char *set_name(dbref obj, const char *newname);
    extern dbref new_object(void);
    extern const char *set_lmod(dbref obj, const char *lmod);

/* local.c */
    void local_startup(void);
    void local_configs(void);
    void local_dump_database(void);
    void local_dbck(void);
    void local_shutdown(void);
    void local_timer(void);
    void local_connect(dbref player, int isnew, int num);
    void local_disconnect(dbref player, int num);
    void local_data_create(dbref object);
    void local_data_clone(dbref clone, dbref source);
    void local_data_free(dbref object);
    int local_can_interact_first(dbref from, dbref to, int type);
    int local_can_interact_last(dbref from, dbref to, int type);

    /* flaglocal.c */
    void local_flags(void);

/* funlist.c */
    void do_gensort(dbref player, char *keys[], char *strs[], int n,
                    char *sort_type);

/* sig.c */
    /** Type definition for signal handlers */
    typedef void (*Sigfunc) (int);
/* Set up a signal handler. Use instead of signal() */
    Sigfunc install_sig_handler(int signo, Sigfunc f);
/* Call from in a signal handler to re-install the handler. Does nothing
   with persistent signals */
    void reload_sig_handler(int signo, Sigfunc f);
/* Ignore a signal. Like i_s_h with SIG_IGN) */
    void ignore_signal(int signo);
/* Block one signal temporarily. */
    void block_a_signal(int signo);
/* Unblock a signal */
    void unblock_a_signal(int signo);
/* Block all signals en masse. */
    void block_signals(void);

#endif				/* __EXTERNS_H */
