/**
 * \file player.c
 *
 * \brief Player creation and connection for PennMUSH.
 *
 *
 */

#include "copyrite.h"
#include "config.h"
#include <stdio.h>
#ifdef I_UNISTD
#include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#ifdef I_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#include <fcntl.h>
#include <ltdl.h>

#include "conf.h"
#include "externs.h"
#include "mushdb.h"
#include "attrib.h"
#include "access.h"
#include "parse.h"
#include "mymalloc.h"
#include "log.h"
#include "dbdefs.h"
#include "flags.h"
#include "lock.h"
#include "parse.h"
#include "modules.h"

#ifdef HAS_CRYPT
#ifdef I_CRYPT
#include <crypt.h>
#else
extern char *crypt(const char *, const char *);
#endif
#endif
extern struct module_entry_t *module_list;

#include "extmail.h"
#include "confmagic.h"

dbref email_register_player
  (const char *name, const char *email, const char *host, const char *ip);
static dbref make_player
  (const char *name, const char *password, const char *host, const char *ip);
static dbref create_guest(const char *host, const char *ip);
void do_password
  (dbref player, dbref cause, const char *old, const char *newobj);

static const char pword_attr[] = "XYXXY";

extern struct db_stat_info current_state;

/** Check a player's password against a given string.
 * \param player dbref of player.
 * \param password plaintext password string to check.
 * \retval 1 password matches (or player has no password).
 * \retval 0 password fails to match.
 */
int
password_check(dbref player, const char *password)
{
  ATTR *a;
  char *saved;
  char *passwd;

  /* read the password and compare it */
  if (!(a = atr_get_noparent(player, pword_attr))) {
    /* If it's a division type object.. return 0, they can't enter this way */
    return Typeof(player) == TYPE_DIVISION ? 0 : 1; /* No password attribute */
  }

  saved = strdup(atr_value(a));

  if (!saved)
    return 0;

  passwd = mush_crypt(password);

  if (strcmp(passwd, saved) != 0) {
    /* Nope. Try non-SHS. */
#ifdef HAS_CRYPT
    /* shs encryption didn't match. Try crypt(3) */
    if (strcmp(crypt(password, "XX"), saved) != 0)
      /* Nope */
#endif                          /* HAS_CRYPT */
      /* crypt() didn't work. Try plaintext, being sure to not 
       * allow unencrypted entry of encrypted password */
      if ((strcmp(saved, password) != 0)
          || (strlen(password) < 4)
          || ((password[0] == 'X') && (password[1] == 'X'))) {
        /* Nothing worked. You lose. */
        free(saved);
        return 0;
      }
    /* Something worked. Change password to SHS-encrypted */
    (void) atr_add(player, pword_attr, passwd, GOD, NOTHING);
  }
  free(saved);
  return 1;
}

/** Check to see if someone can connect to a player.
 * \param name name of player to connect to.
 * \param password password of player to connect to.
 * \param host host from which connection is being attempted.
 * \param ip ip address from which connection is being attempted.
 * \param errbuf buffer to return connection errors.
 * \return dbref of connected player object or NOTHING for failure
 * (with reason for failure returned in errbuf).
 */
dbref
connect_player(const char *name, const char *password, const char *host,
               const char *ip, char *errbuf)
{
  dbref player;
  char isgst = 0;

  /* Default error */
  strcpy(errbuf,
         T("Either that player does not exist, or has a different password."));

  if (!name || !*name)
    return NOTHING;

  /* validate name */
  /* first check if it's a guest */
  if(!strcasecmp(name, GUEST_KEYWORD))
    isgst = 1;
  else if ((player = lookup_player(name)) == NOTHING)
    return NOTHING;

  /* See if player is allowed to connect like this */
  if (!isgst && ( Going(player) || Going_Twice(player) )) {
    do_log(LT_CONN, 0, 0,
           T("Connection to GOING player %s not allowed from %s (%s)"), name,
           host, ip);
    return NOTHING;
  }
  if (!Site_Can_Connect(host, player) || !Site_Can_Connect(ip, player)) {
    if (!Deny_Silent_Site(host, player) && !Deny_Silent_Site(ip, player)) {
      do_log(LT_CONN, 0, 0,
             T("Connection to %s not allowed from %s (%s)"), name,
             host, ip);
      strcpy(errbuf, T("Player connections not allowed."));
    }
    return NOTHING;
  }
  /* validate password */
  if (!isgst)
    if (!password_check(player, password)) {
      /* Increment count of login failures */
      ModTime(player)++;
      check_lastfailed(player, host);
      return NOTHING;
    }
  /* If it's a Guest player, and already connected, search the
   * db for another Guest player to connect them to. */
  if(isgst) {
  player = create_guest(host, ip);
    if (!GoodObject(player)) {
      do_log(LT_CONN, 0, 0, T("Can't connect to a guest (too many connected)"));
      strcpy(errbuf, T("Too many guests are connected now."));
      return NOTHING;
    }
  }
  if (Suspect_Site(host, player) || Suspect_Site(ip, player)) {
    do_log(LT_CONN, 0, 0,
           T("Connection from Suspect site. Setting %s(#%d) suspect."),
           Name(player), player);
    set_flag_internal(player, "SUSPECT");
  }
  return player;
}


/** Attempt to create a new guest.
 */

dbref create_guest(const char *host, const char *ip) {
        int i , mg;
        char *guest_name;
        dbref gst_id;

        mg = MAX_GUESTS == 0 ? 100 : MAX_GUESTS;
        guest_name = (char *) mush_malloc(BUFFER_LEN, "gst_buf");
        gst_id = NOTHING;

        for( i = 0 ; i  < mg ; i++)  {
                /* reset vars */
                memset(guest_name, '\0', BUFFER_LEN);
                gst_id = NOTHING;  
                strncpy(guest_name, T(GUEST_PREFIX), BUFFER_LEN-1);
                strcat(guest_name, GUEST_NUMBER(i+1));
                if(ok_player_name(guest_name, NOTHING, NOTHING))
                  break;        
                else if((gst_id = lookup_player(guest_name)) != NOTHING && !Connected(gst_id))
                        break;
                else continue;
        }
        if(gst_id == NOTHING) {
          if(i >= mg)  {
                mush_free((Malloc_t) guest_name, "gst_buf");
                return NOTHING;
          } else if(DBTOP_MAX && (db_top >= DBTOP_MAX + 1) && (first_free == NOTHING)) {
                mush_free((Malloc_t) guest_name, "gst_buf");
                do_log(LT_CONN, 0, 0, T("Failed creation (no db space) from %s"), host);
                return NOTHING;
          }
          gst_id = make_player(guest_name, "", host, ip);
        } else  { /* Reset Guest */
                object_flag_type flags;
                flags = string_to_bits("FLAG", options.player_flags);
                copy_flag_bitmask("FLAG", Flags(gst_id), flags);
        }
        mush_free((Malloc_t) guest_name, "gst_buf");
        atr_add(gst_id, "DESCRIBE", GUEST_DESCRIBE, gst_id, NOTHING);

        SLEVEL(gst_id) = LEVEL_GUEST;
        Home(gst_id) = options.guest_start;
        if(GoodObject(options.guest_start) && IsRoom(options.guest_start)) {
          Contents(Location(gst_id)) = remove_first(Contents(Location(gst_id)), gst_id);
          Location(gst_id) = GUEST_START;
          PUSH(gst_id, Contents(options.guest_start));
        }
        return gst_id;
}
/** Attempt to create a new player object.
 * \param name name of player to create.
 * \param password initial password of created player.
 * \param host host from which creation is attempted.
 * \param ip ip address from which creation is attempted.
 * \return dbref of created player, NOTHING if bad name, AMBIGUOUS if bad
 *  password.
 */
dbref
create_player(const char *name, const char *password, const char *host,
              const char *ip)
{
  dbref player;
  if (!ok_player_name(name, NOTHING, NOTHING)) {
    do_log(LT_CONN, 0, 0, T("Failed creation (bad name) from %s"), host);
    return NOTHING;
  }
  if (!ok_password(password)) {
    do_log(LT_CONN, 0, 0, T("Failed creation (bad password) from %s"), host);
    return AMBIGUOUS;
  }
  if (DBTOP_MAX && (db_top >= DBTOP_MAX + 1) && (first_free == NOTHING)) {
    /* Oops, out of db space! */
    do_log(LT_CONN, 0, 0, T("Failed creation (no db space) from %s"), host);
    return NOTHING;
  }
  /* else he doesn't already exist, create him */
  player = make_player(name, password, host, ip);
  SLEVEL(player) = LEVEL_UNREGISTERED;
  powergroup_db_set(NOTHING, player, PLAYER_DEF_POWERGROUP, 1);
  return player;
}

/* The HAS_SENDMAIL ifdef is kept here as a hint to metaconfig */
#ifdef MAILER
#undef HAS_SENDMAIL
#define HAS_SENDMAIL 1
#undef SENDMAIL
#define SENDMAIL MAILER
#endif

#ifdef HAS_SENDMAIL

/** Size of the elems array */
#define NELEMS (sizeof(elems)-1)

/** Attempt to register a new player at the connect screen.
 * If registration is allowed, a new player object is created with
 * a random password which is emailed to the registering player.
 * \param name name of player to register.
 * \param email email address to send registration details.
 * \param host host from which registration is being attempted.
 * \param ip ip address from which registration is being attempted.
 * \return dbref of created player or NOTHING if creation failed.
 */
dbref
email_register_player(const char *name, const char *email, const char *host,
                      const char *ip)
{
  char *p;
  char passwd[BUFFER_LEN];
  static char elems[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  int i, len;
  dbref player;
  FILE *fp;

  if (!ok_player_name(name, NOTHING, NOTHING)) {
    do_log(LT_CONN, 0, 0, T("Failed registration (bad name) from %s"), host);
    return NOTHING;
  }
  /* Make sure that the email address is valid. A valid address must
   * contain either an @ or a !
   * Also, to prevent someone from using the MUSH to mailbomb another site,
   * let's make sure that the site to which the user wants the email
   * sent is also allowed to use the register command.
   * If there's an @, we check whatever's after the last @
   * (since @foo.bar:user@host is a valid email)
   * If not, we check whatever comes before the first !
   */
  if ((p = strrchr(email, '@'))) {
    p++;
    if (!Site_Can_Register(p)) {
      if (!Deny_Silent_Site(p, AMBIGUOUS)) {
        do_log(LT_CONN, 0, 0,
               T("Failed registration (bad site in email: %s) from %s"),
               email, host);
      }
      return NOTHING;
    }
  } else if ((p = strchr(email, '!'))) {
    *p = '\0';
    if (!Site_Can_Register(email)) {
      *p = '!';
      if (!Deny_Silent_Site(email, AMBIGUOUS)) {
        do_log(LT_CONN, 0, 0,
               T("Failed registration (bad site in email: %s) from %s"),
               email, host);
      }
      return NOTHING;
    } else
      *p = '!';
  } else {
    if (!Deny_Silent_Site(host, AMBIGUOUS)) {
      do_log(LT_CONN, 0, 0, T("Failed registration (bad email: %s) from %s"),
             email, host);
    }
    return NOTHING;
  }

  if (DBTOP_MAX && (db_top >= DBTOP_MAX + 1) && (first_free == NOTHING)) {
    /* Oops, out of db space! */
    do_log(LT_CONN, 0, 0, T("Failed registration (no db space) from %s"), host);
    return NOTHING;
  }

  /* Come up with a random password of length 7-12 chars */
  len = get_random_long(7, 12);
  for (i = 0; i < len; i++)
    passwd[i] = elems[get_random_long(0, NELEMS - 1)];
  passwd[len] = '\0';

  /* If we've made it here, we can send the email and create the
   * character. Email first, since that's more likely to go bad.
   * Some security precautions we'll take:
   *  1) We'll use sendmail -t, so we don't pass user-given values to a shell.
   */

  release_fd();
  if ((fp =
#ifdef __LCC__
       (FILE *)
#endif
       popen(tprintf("%s -t", SENDMAIL), "w")) == NULL) {
    do_log(LT_CONN, 0, 0,
           T("Failed registration of %s by %s: unable to open sendmail"),
           name, email);
    reserve_fd();
    return NOTHING;
  }
  fprintf(fp, T("Subject: [%s] Registration of %s\n"), MUDNAME, name);
  fprintf(fp, "To: %s\n", email);
  fprintf(fp, "Precedence: junk\n");
  fprintf(fp, "\n");
  fprintf(fp, T("This is an automated message.\n"));
  fprintf(fp, "\n");
  fprintf(fp, T("Your requested player, %s, has been created.\n"), name);
  fprintf(fp, T("The password is %s\n"), passwd);
  fprintf(fp, "\n");
  fprintf(fp, T("To access this character, connect to %s and type:\n"),
          MUDNAME);
  fprintf(fp, "\tconnect \"%s\" %s\n", name, passwd);
  fprintf(fp, "\n");
  pclose(fp);
  reserve_fd();
  /* Ok, all's well, make a player */
  player = make_player(name, passwd, host, ip);
  (void) atr_add(player, "REGISTERED_EMAIL", email, GOD, NOTHING);
  SLEVEL(player) = LEVEL_UNREGISTERED;
  powergroup_db_set(NOTHING, player, PLAYER_DEF_POWERGROUP, 1);
  return player;
}
#else
dbref
email_register_player(const char *name, const char *email, const char *host,
                      const char *ip)
{
  do_log(LT_CONN, 0, 0, T("Failed registration (no sendmail) from %s"), host);
  return NOTHING;
}
#endif

static dbref
make_player(const char *name, const char *password, const char *host,
            const char *ip)
{

  dbref player;
  char temp[SBUF_LEN];
  object_flag_type flags;
  struct module_entry_t *m;
  void (*handle)(dbref player);

  player = new_object();

  /* initialize everything */
  set_name(player, name);
  Location(player) = PLAYER_START;
  Home(player) = PLAYER_START;
  Owner(player) = player;
  Parent(player) = NOTHING;
  Type(player) = TYPE_PLAYER;
  flags = string_to_bits("FLAG", options.player_flags);
  copy_flag_bitmask("FLAG", Flags(player), flags);
  destroy_flag_bitmask(flags);
  if (Suspect_Site(host, player) || Suspect_Site(ip, player))
    set_flag_internal(player, "SUSPECT");
  set_initial_warnings(player);
  /* Modtime tracks login failures */
  ModTime(player) = (time_t) 0;
  (void) atr_add(player, "XYXXY", mush_crypt(password), GOD, NOTHING);
  giveto(player, START_BONUS);  /* starting bonus */
  (void) atr_add(player, "LAST", show_time(mudtime, 0), GOD, NOTHING);
  (void) atr_add(player, "LASTSITE", host, GOD, NOTHING);
  (void) atr_add(player, "LASTIP", ip, GOD, NOTHING);
  (void) atr_add(player, "LASTFAILED", " ", GOD, NOTHING);
  sprintf(temp, "%d", START_QUOTA);
  (void) atr_add(player, "RQUOTA", temp, GOD, NOTHING);
  (void) atr_add(player, "ICLOC", EMPTY_ATTRS ? "" : " ", GOD,
                 AF_MDARK | AF_PRIVATE | AF_NOCOPY);
#ifdef USE_MAILER
  (void) atr_add(player, "MAILCURF", "0", GOD,
                 AF_LOCKED | AF_NOPROG);
  add_folder_name(player, 0, "inbox");
#endif
  /* link him to PLAYER_START */
  PUSH(player, Contents(PLAYER_START));

  add_player(player);
  add_lock(GOD, player, Basic_Lock, parse_boolexp(player, "=me", Basic_Lock),
           -1);
  add_lock(GOD, player, Enter_Lock, parse_boolexp(player, "=me", Basic_Lock),
           -1);
  add_lock(GOD, player, Use_Lock, parse_boolexp(player, "=me", Basic_Lock), -1);

  current_state.players++;

  /* Replacement for local_data_create */
  MODULE_ITER(m)
    MODULE_FUNC(handle, m->handle, "module_data_create", player);


  return player;
}


/** Change a player's password.
 * \verbatim
 * This function implements @password.
 * \endverbatim
 * \param player the executor.
 * \param cause the enactor.
 * \param old player's current password.
 * \param newobj player's desired new password.
 */
void
do_password(dbref player, dbref cause, const char *old, const char *newobj)
{
  if (!global_eval_context.process_command_port) {
    char old_eval[BUFFER_LEN];
    char new_eval[BUFFER_LEN];
    char const *sp;
    char *bp;

    sp = old;
    bp = old_eval;
    process_expression(old_eval, &bp, &sp, player, player, cause,
                       PE_DEFAULT, PT_DEFAULT, NULL);
    *bp = '\0';
    old = old_eval;

    sp = newobj;
    bp = new_eval;
    process_expression(new_eval, &bp, &sp, player, player, cause,
                       PE_DEFAULT, PT_DEFAULT, NULL);
    *bp = '\0';
    newobj = new_eval;
  }

  if (!password_check(player, old)) {
    notify(player, T("Suffering from memory loss? See an admin!"));
  } else if (!ok_password(newobj)) {
    notify(player, T("Bad new password."));
  } else {
    (void) atr_add(player, "XYXXY", mush_crypt(newobj), GOD, NOTHING);
    notify(player, T("You have changed your password."));
  }
}

/** Processing related to players' last connections.
 * Here we check to see if a player gets a paycheck, tell them their
 * last connection site, and update all their LAST* attributes.
 * \param player dbref of player.
 * \param host hostname of player's current connection.
 * \param ip ip address of player's current connection.
 */
void
check_last(dbref player, const char *host, const char *ip)
{
  char *s;
  ATTR *a;
  ATTR *h;
  char last_time[MAX_COMMAND_LEN / 8];
  char last_place[MAX_COMMAND_LEN];

  /* compare to last connect see if player gets salary */
  s = show_time(mudtime, 0);
  a = atr_get_noparent(player, "LAST");
  if (a && (strncmp(atr_value(a), s, 10) != 0))
    giveto(player, Paycheck(player));
  /* tell the player where he last connected from */
  h = atr_get_noparent(player, "LASTSITE");
  if (!Guest(player)) {
    h = atr_get_noparent(player, "LASTSITE");
    if (h && a) {
      strcpy(last_place, atr_value(h));
      strcpy(last_time, atr_value(a));
      notify_format(player, T("Last connect was from %s on %s."),
                  last_place, last_time);
    }
    /* How about last failed connection */
    h = atr_get_noparent(player, "LASTFAILED");
    if (h && a) {
      strcpy(last_place, atr_value(h));
      if (strlen(last_place) > 2)
      notify_format(player, T("Last FAILED connect was from %s."),
                    last_place);
    }
  }
  /* if there is no Lastsite, then the player is newly created.
   * the extra variables are a kludge to work around some weird
   * behavior involving uncompress.
   */

  /* set the new attributes */
  (void) atr_add(player, "LAST", s, GOD, NOTHING);
  if(!has_flag_by_name(player, "WEIRDSITE", TYPE_PLAYER)) {
   (void) atr_add(player, "LASTSITE", host, GOD, NOTHING);
   (void) atr_add(player, "LASTIP", ip, GOD, NOTHING);
  }
  (void) atr_add(player, "LASTFAILED", " ", GOD, NOTHING);
}


/** Update the LASTFAILED attribute on a failed connection.
 * \param player dbref of player.
 * \param host host from which connection attempt failed.
 */
void
check_lastfailed(dbref player, const char *host)
{
  char last_place[BUFFER_LEN], *bp;

  bp = last_place;
  safe_format(last_place, &bp, T("%s on %s"), host, show_time(mudtime, 0));
  *bp = '\0';
  (void) atr_add(player, "LASTFAILED", last_place, GOD, NOTHING);
}
