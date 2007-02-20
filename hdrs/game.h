/* game.h */
/* Command handlers */

#ifndef __GAME_H
#define __GAME_H

/* Miscellaneous flags */
#define CHECK_INVENTORY            0x10
#define CHECK_NEIGHBORS            0x20
#define CHECK_SELF                 0x40
#define CHECK_HERE                 0x80
#define CHECK_ZONE                 0x100
#define CHECK_GLOBAL               0x200

/* hash table stuff */
extern void init_func_hashtab(void);	/* eval.c */
extern void init_math_hashtab(void);	/* funmath.c */
extern void init_aname_table(void);	/* atr_tab.c */
extern void init_flagspaces(void);	/* flags.c */
extern void init_flag_table(const char *ns);	/* flags.c */
extern void init_tag_hashtab(void);	/* funstr.c */
extern void init_pronouns(void);	/* funstr.c */

/* From bsd.c */
extern void fcache_init(void);
extern void fcache_load(dbref player);
extern void hide_player(dbref player, int hide);
enum motd_type { MOTD_MOTD, MOTD_DOWN, MOTD_FULL, MOTD_LIST };
extern void do_motd(dbref player, enum motd_type key, const char *message);
extern void do_poll(dbref player, const char *message);
/* From cque.c */
extern int do_wait
  (dbref player, dbref cause, char *arg1, char *cmd, int until, char finvoc);
enum queue_type { QUEUE_ALL, QUEUE_NORMAL, QUEUE_SUMMARY, QUEUE_QUICK };
extern void do_queue(dbref player, const char *what, enum queue_type flag);
extern void do_halt1(dbref player, const char *arg1, const char *arg2);
extern void do_allhalt(dbref player);
extern void do_allrestart(dbref player);
extern void do_restart(void);
extern void do_restart_com(dbref player, const char *arg1);
/* Only QID_ACTIVE, QID_FALSE, and QID_FREEZE actually get thrown on a qid cell.
 *  * other are just used to pass in function shit */
enum qid_flags {QID_ACTIVE, QID_KILL, QID_FREEZE, QID_CONT, QID_TIME, QID_QUERY_T, QID_FALSE};
extern int do_signal_qid(dbref signalby, int qid, enum qid_flags qid_flags, int time);

/* From command.c */
enum hook_type { HOOK_BEFORE, HOOK_AFTER, HOOK_IGNORE, HOOK_OVERRIDE };
extern void do_hook(dbref player, char *command, char *obj, char *attrname,
		    enum hook_type flag);


/* From compress.c */
#if (COMPRESSION_TYPE > 0)
extern int init_compress(FILE * f);
#endif

/* From conf.c */
extern int config_file_startup(const char *conf, int restrictions);

/* From game.c */
enum dump_type { DUMP_NORMAL, DUMP_DEBUG, DUMP_PARANOID };
extern void do_dump(dbref player, char *num, enum dump_type flag);
enum shutdown_type { SHUT_NORMAL, SHUT_PANIC, SHUT_PARANOID };
extern void do_shutdown(dbref player, enum shutdown_type panic_flag);

/* From look.c */
enum exam_type { EXAM_NORMAL, EXAM_BRIEF, EXAM_MORTAL };
extern void do_examine(dbref player, const char *name, enum exam_type flag,
		       int all);
extern void do_inventory(dbref player);
extern void do_find(dbref player, const char *name, char **argv);
extern void do_whereis(dbref player, const char *name);
extern void do_score(dbref player);
extern void do_sweep(dbref player, const char *arg1);
enum ent_type { ENT_EXITS, ENT_THINGS, ENT_PLAYERS, ENT_ROOMS, ENT_ALL };
extern void do_entrances(dbref player, const char *where, char **argv,
			 enum ent_type val);
enum dec_type { DEC_NORMAL, DEC_DB, DEC_FLAG, DEC_ATTR };
extern void do_decompile(dbref player, const char *name, const char *prefix,
			 enum dec_type dbflag, int skipdef);

/* From move.c */
extern void do_get(dbref player, const char *what);
extern void do_drop(dbref player, const char *name);
extern void do_enter(dbref player, const char *what);
extern void do_leave(dbref player);
extern void do_empty(dbref player, const char *what);
extern void do_firstexit(dbref player, const char *what);

/* From player.c */
extern void do_password(dbref player, dbref cause,
			const char *old, const char *newobj);

/* From predicat.c */
extern void do_switch
  (dbref player, char *expression, char **argv, dbref cause, int first,
   int notifyme, int regexp);
extern void do_verb(dbref player, dbref cause, char *arg1, char **argv);
extern void do_grep
  (dbref player, char *obj, char *lookfor, int flag, int insensitive);

/* From rob.c */
extern void do_give(dbref player, char *recipient, char *amnt, int silent);
extern void do_buy(dbref player, char *item, char *from, int price);

/* From set.c */
extern void do_name(dbref player, const char *name, char *newname);
extern void do_chown
  (dbref player, const char *name, const char *newobj, int preserve);
extern int do_chzone(dbref player, const char *name, const char *newobj,
		     int noisy);
extern int do_set(dbref player, const char *name, char *flag);
extern void do_cpattr
  (dbref player, char *oldpair, char **newpair, int move, int noflagcopy);
  enum edit_type { EDIT_FIRST, EDIT_ALL} ;
extern void do_gedit(dbref player, char *it, char **argv,
		     enum edit_type target, int doit);
extern void do_trigger(dbref player, char *object, char **argv);
extern void do_use(dbref player, const char *what);
extern void do_parent(dbref player, char *name, char *parent_name);
extern void do_wipe(dbref player, char *name);

/* From speech.c */
extern void do_say(dbref player, const char *tbuf1);
extern void do_whisper
  (dbref player, const char *arg1, const char *arg2, int noisy);
extern void do_whisper_list
  (dbref player, const char *arg1, const char *arg2, int noisy);
extern void do_pose(dbref player, const char *tbuf1, int space);
enum wall_type { WALL_ALL };
extern void do_wall(dbref player, const char *message, enum wall_type target,
		    int emit);
extern void do_page(dbref player, const char *arg1, const char *arg2,
		    dbref cause, int noeval, int multipage, int override);
extern void do_page_port(dbref player, const char *arg1, const char *arg2);
extern void do_think(dbref player, const char *message);
#define PEMIT_SILENT 0x1
#define PEMIT_LIST   0x2
#define PEMIT_SPOOF  0x4
extern void do_emit(dbref player, const char *tbuf1, int flags);
extern void do_pemit
  (dbref player, const char *arg1, const char *arg2, int flags);
extern void do_pemit_list(dbref player, char *list, const char *message,
			  int flags);
extern void do_remit(dbref player, char *arg1, const char *arg2, int flags);
extern void do_lemit(dbref player, const char *tbuf1, int flags);
extern void do_zemit(dbref player, const char *arg1, const char *arg2,
		     int flags);
extern void do_oemit_list(dbref player, char *arg1, const char *arg2,
			  int flags);
extern void do_teach(dbref player, dbref cause, const char *tbuf1);

/* From wiz.c */
extern void do_debug_examine(dbref player, const char *name);
extern void do_enable(dbref player, const char *param, int state);
extern void do_kick(dbref player, const char *num);
extern void do_search(dbref player, const char *arg1, char **arg3);
extern dbref do_pcreate
  (dbref creator, const char *player_name, const char *player_password);
extern void do_quota
  (dbref player, const char *arg1, const char *arg2, int set_q);
extern void do_allquota(dbref player, const char *arg1, int quiet);
extern void do_teleport
  (dbref player, const char *arg1, const char *arg2, int silent, int inside);
extern void do_force(dbref player, const char *what, char *command);
extern void do_stats(dbref player, const char *name);
extern void do_newpassword
  (dbref player, dbref cause, const char *name, const char *password);
enum boot_type { BOOT_NAME, BOOT_DESC, BOOT_SELF };
extern void do_boot(dbref player, const char *name, enum boot_type flag);
extern void do_chzoneall(dbref player, const char *name, const char *target);
extern int parse_force(char *command);
enum sitelock_type { SITELOCK_ADD, SITELOCK_REMOVE, SITELOCK_BAN,
  SITELOCK_CHECK, SITELOCK_LIST
};
extern void do_sitelock
  (dbref player, const char *site, const char *opts, const char *charname,
   enum sitelock_type type);
extern void do_sitelock_name(dbref player, const char *name);
extern void do_chownall
  (dbref player, const char *name, const char *target, int preserve);
extern void NORETURN do_reboot(dbref player, int flag);

/* From destroy.c */
extern void do_dbck(dbref player);
extern void do_destroy(dbref player, char *name, int confirm);
extern void pre_destroy(dbref player, dbref thing);

/* From timer.c */
extern void init_timer(void);
extern void signal_cpu_limit(int signo);

/* From version.c */
extern void do_version(dbref player);

#endif				/* __GAME_H */
