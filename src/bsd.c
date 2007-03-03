/**
 * \file bsd.c
 *
 * \brief Network communication through BSD sockets for PennMUSH.
 *
 * While mysocket.c provides low-level functions for working with
 * sockets, bsd.c focuses on player descriptors, a higher-level
 * structure that tracks all information associated with a connection,
 * and through which connection i/o is done.
 *
 *
 */

#include "copyrite.h"
#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#ifdef WIN32
#define FD_SETSIZE 256
#include <windows.h>
#include <winsock.h>
#include <io.h>
#define EINTR WSAEINTR
#define EWOULDBLOCK WSAEWOULDBLOCK
#define MAXHOSTNAMELEN 32
#define LC_MESSAGES 6
#pragma warning( disable : 4761)	/* disable warning re conversion */
#else				/* !WIN32 */
#ifdef I_SYS_FILE
#include <sys/file.h>
#endif
#ifdef I_SYS_TIME
#include <sys/time.h>
#endif
#include <sys/ioctl.h>
#include <errno.h>
#ifdef I_SYS_SOCKET
#include <sys/socket.h>
#endif
#ifdef I_NETINET_IN
#include <netinet/in.h>
#endif
#ifdef I_NETDB
#include <netdb.h>
#endif
#ifdef I_SYS_PARAM
#include <sys/param.h>
#endif
#ifdef I_SYS_STAT
#include <sys/stat.h>
#endif
#endif				/* !WIN32 */
#include <time.h>
#ifdef I_SYS_WAIT
#include <sys/wait.h>
#endif
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#ifdef I_SYS_SELECT
#include <sys/select.h>
#endif
#ifdef I_UNISTD
#include <unistd.h>
#endif
#ifdef HAS_GETRLIMIT
#include <sys/resource.h>
#endif
#include <limits.h>
#ifdef I_FLOATINGPOINT
#include <floatingpoint.h>
#endif
#include <locale.h>
#ifdef __APPLE__
#define LC_MESSAGES     6
#define AUTORESTART
#endif
#include <setjmp.h>

#include "conf.h"

#if defined(WIN32) && defined(INFO_SLAVE)
#undef INFO_SLAVE
#endif

#ifdef INFO_SLAVE
#include <sys/uio.h>
#endif

#include "externs.h"
#include "chunk.h"
#include "mushdb.h"
#include "dbdefs.h"
#include "flags.h"
#include "lock.h"
#include "help.h"
#include "match.h"
#include "ansi.h"
#include "pueblo.h"
#include "parse.h"
#include "access.h"
#include "command.h"
#include "version.h"
#include "patches.h"
#include "mysocket.h"
#include "ident.h"
#include "strtree.h"
#include "log.h"
#include "pcre.h"
#ifdef HAS_OPENSSL
#include "myssl.h"
#endif
#include "mymalloc.h"
#include "extmail.h"
#include "attrib.h"
#include "game.h"
#include "dbio.h"
#include "confmagic.h"
#ifdef HAS_WAITPID
/** What does wait*() return? */
#define WAIT_TYPE int
#else
#ifdef UNION_WAIT
#define WAIT_TYPE union wait
#else
#define WAIT_TYPE int
#endif
#endif


/* BSD 4.2 and maybe some others need these defined */
#ifndef FD_ZERO
/** An fd_set is 4 bytes */
#define fd_set int
/** Clear an fd_set */
#define FD_ZERO(p)       (*p = 0)
/** Set a bit in an fd_set */
#define FD_SET(n,p)      (*p |= (1<<(n)))
/** Clear a bit in an fd_set */
#define FD_CLR(n,p)      (*p &= ~(1<<(n)))
/** Check a bit in an fd_set */
#define FD_ISSET(n,p)    (*p & (1<<(n)))
#endif				/* defines for BSD 4.2 */

#ifdef HAS_GETRUSAGE
void rusage_stats(void);
#endif
int que_next(void);		/* from cque.c */

void dispatch(void);		/* from timer.c */
dbref email_register_player(const char *name, const char *email, const char *host, const char *ip);	/* from player.c */

static int extrafd;
int shutdown_flag = 0;		/**< Is it time to shut down? */
#ifdef CHAT_SYSTEM
void chat_player_announce(dbref player, char *msg, int ungag);
#endif /* CHAT_SYSTEM */

static int login_number = 0;
static int under_limit = 1;

char cf_motd_msg[BUFFER_LEN];	/**< The message of the day */
char cf_downmotd_msg[BUFFER_LEN];	/**< The down message */
char cf_fullmotd_msg[BUFFER_LEN];	/**< The 'mush full' message */
static char poll_msg[DOING_LEN];
char confname[BUFFER_LEN];	/**< Name of the config file */
char errlog[BUFFER_LEN];	/**< Name of the error log file */

/* Default Connection flags for certain clients
 */
static CLIENT_DEFAULTS client_maps[]  = {
  {"TINYFUGUE", CONN_PROMPT},
  {NULL, -1}
};


/** Is this descriptor connected to a telnet-compatible terminal? */
#define TELNET_ABLE(d) ((d)->conn_flags & (CONN_TELNET | CONN_TELNET_QUERY))


/* When the mush gets a new connection, it tries sending a telnet
 * option negotiation code for setting client-side line-editing mode
 * to it. If it gets a reply, a flag in the descriptor struct is
 * turned on indicated telnet-awareness.
 * 
 * If the reply indicates that the client supports linemode, further
 * instructions as to what linemode options are to be used is sent.
 * Those options: Client-side line editing, and expanding literal
 * client-side-entered tabs into spaces.
 * 
 * Option negotation requests sent by the client are processed,
 * with the only one we confirm rather than refuse outright being
 * suppress-go-ahead, since a number of telnet clients try it.
 *
 * The character 255 is the telnet option escape character, so when it
 * is sent to a telnet-aware client by itself (Since it's also often y-umlaut)
 * it must be doubled to escape it for the client. This is done automatically,
 * and is the original purpose of adding telnet option support.
 */

/* Telnet codes */
#define IAC		255	/**< interpret as command: */
#define GOAHEAD		249	/**< Go Ahead command */
#define NOP		241	/**< no operation */
#define AYT		246	/**< are you there? */
#define DONT		254	/**< you are not to use option */
#define DO		253	/**< please, you use option */
#define WONT		252	/**< I won't use option */
#define WILL		251	/**< I will use option */
#define SB		250	/**< interpret as subnegotiation */
#define SE		240	/**< end sub negotiation */
#define TN_SGA		3	/**< Suppress go-ahead */
#define TN_LINEMODE	34	/**< Line mode */
#define TN_NAWS		31	/**< Negotiate About Window Size */
#define TN_TTYPE	24	/**< Ask for termial type information */
static void test_telnet(DESC *d);
static void setup_telnet(DESC *d);
static int handle_telnet(DESC *d, unsigned char **q, unsigned char *qend);
static const char *empabb(dbref);
static int do_su_exit(DESC *d);

static const char *create_fail =
  "Either there is already a player with that name, or that name is illegal.";
static const char *password_fail = "The password is invalid (or missing).";
static const char *register_fail =
  "Unable to register that player with that email address.";
static const char *register_success =
  "Registration successful! You will receive your password by email.";
static const char *shutdown_message = "Going down - Bye";
#ifdef HAS_OPENSSL
static const char *ssl_shutdown_message = 
  "GAME: SSL connections must be dropped, sorry.";
#endif
/** Where we save the descriptor info across reboots. */
#define REBOOTFILE              "reboot.db"

#if 0
/* For translation */
static void dummy_msgs(void);
static void
dummy_msgs()
{
  char *temp;
  temp = T("Either that player does not exist, or has a different password.");
  temp =
    T
    ("Either there is already a player with that name, or that name is illegal.");
  temp = T("The password is invalid (or missing).");
  temp = T("Unable to register that player with that email address.");
  temp = T("Registration successful! You will receive your password by email.");
  temp = T("Going down - Bye");
  temp = T("GAME: SSL connections must be dropped, sorry.");
}

#endif

DESC *descriptor_list = NULL;	/**< The linked list of descriptors */

static int sock;
#ifdef HAS_OPENSSL
static int sslsock = 0;
SSL *ssl_master_socket = NULL;	/**< Master SSL socket for ssl port */
#endif
static int ndescriptors = 0;
#ifdef WIN32
static WSADATA wsadata;
#endif
int restarting = 0;	/**< Are we restarting the server after a reboot? */
static int maxd = 0;

extern const unsigned char *tables;

#ifdef INFO_SLAVE
static fd_set info_pending;
static int info_slave;
Pid_t info_slave_pid = -1;	/**< Process id of the info_slave process */
int info_slave_state = 0;	/**< State of the info_slave process */
static int info_query_spill, info_reap_spill;
static time_t info_queue_time = 0;
#endif

sig_atomic_t signal_shutdown_flag = 0;	/**< Have we caught a shutdown signal? */
sig_atomic_t signal_dump_flag = 0;	/**< Have we caught a dump signal? */

#ifdef HAS_GETRLIMIT
static void init_rlimit(void);
#endif
#ifndef BOOLEXP_DEBUGGING
#ifdef WIN32SERVICES
void shutdown_checkpoint(void);
void mainthread(int argc, char **argv);
#else
int main(int argc, char **argv);
#endif
#endif
void set_signals(void);
static struct timeval *timeval_sub(struct timeval *now, struct timeval *then);
#ifdef WIN32
/** Windows doesn't have gettimeofday(), so we implement it here */
#define our_gettimeofday(now) win_gettimeofday((now))
static void win_gettimeofday(struct timeval *now);
#else
/** A wrapper for gettimeofday() in case the system doesn't have it */
#define our_gettimeofday(now) gettimeofday((now), (struct timezone *)NULL)
#endif
static long int msec_diff(struct timeval *now, struct timeval *then);
static struct timeval *msec_add(struct timeval *t, int x);
static void update_quotas(struct timeval *last, struct timeval *current);

static int how_many_fds(void);
static void shovechars(Port_t port, Port_t sslport);
static int test_connection(int newsock);
#ifndef INFO_SLAVE
static DESC *new_connection(int oldsock, int *result, int use_ssl);
#endif

static void clearstrings(DESC *d);

/** A block of cached text. */
typedef struct fblock {
  unsigned char *buff;	  /**< Pointer to the block as a string */
  size_t len;		  /**< Length of buff */
} FBLOCK;

/** The complete collection of cached text files. */
struct fcache_entries {
  FBLOCK connect_fcache[2];	/**< connect.txt and connect.html */
  FBLOCK motd_fcache[2];	/**< motd.txt and motd.html */
  FBLOCK newuser_fcache[2];	/**< newuser.txt and newuser.html */
  FBLOCK register_fcache[2];	/**< register.txt and register.html */
  FBLOCK quit_fcache[2];	/**< quit.txt and quit.html */
  FBLOCK down_fcache[2];	/**< down.txt and down.html */
  FBLOCK full_fcache[2];	/**< full.txt and full.html */
  FBLOCK guest_fcache[2];	/**< guest.txt and guest.html */
};

void feed_snoop(DESC *, const char *, char );
char is_snooped(DESC *);
char set_snoop(dbref, DESC *);
void clr_snoop(dbref, DESC *);
void announce_connect(dbref player, int isnew, int num);
void announce_disconnect(dbref player);
void add_to_exit_path(DESC *d, dbref player);

static struct fcache_entries fcache;
static void fcache_dump(DESC *d, FBLOCK fp[2], const unsigned char *prefix);
static int fcache_read(FBLOCK *cp, const char *filename);
static void logout_sock(DESC *d);
static void shutdownsock(DESC *d);
static DESC *initializesock(int s, char *addr, char *ip, int use_ssl);
int process_output(DESC *d);
/* Notify.c */
extern void free_text_block(struct text_block *t);
extern void add_to_queue(struct text_queue *q, const unsigned char *b, int n);
extern int queue_write(DESC *d, const unsigned char *b, int n);
extern int queue_eol(DESC *d);
extern int queue_newwrite(DESC *d, const unsigned char *b, int n);
extern int queue_string(DESC *d, const char *s);
extern int queue_string_eol(DESC *d, const char *s);
extern void freeqs(DESC *d);
static void welcome_user(DESC *d);
static void dump_info(DESC *call_by);
static void save_command(DESC *d, const unsigned char *command);
static int process_input(DESC *d, int output_ready);
static void process_input_helper(DESC *d, char *tbuf1, int got);
static void set_userstring(unsigned char **userstring, const char *command);
static void process_commands(void);
static void parse_puebloclient(DESC *d, char *command);
static int dump_messages(DESC *d, dbref player, int new);
static int check_connect(DESC *d, const char *msg);
static void parse_connect(const char *msg, char *command, char *user,
			  char *pass);
static void close_sockets(void);
dbref find_player_by_desc(int port);
static DESC *lookup_desc(dbref executor, const char *name);
void NORETURN bailout(int sig);
void WIN32_CDECL signal_shutdown(int sig);
void WIN32_CDECL signal_dump(int sig);
void reaper(int sig);
extern Pid_t forked_dump_pid;	/**< Process id of forking dump process */
static void dump_users(DESC *call_by, char *match, int doing);
static const char *time_format_1(long int dt);
static const char *time_format_2(long int dt);

void inactivity_check(void);
#ifdef INFO_SLAVE
static void make_info_slave(void);
static void promote_info_slave(void);
static void query_info_slave(int fd);
static void reap_info_slave(void);
void kill_info_slave(void);
#endif
void reopen_logs(void);
void load_reboot_db(void);
#ifdef HAS_GETRLIMIT
static void
init_rlimit(void)
{
  /* Unlimit file descriptors. */
  /* Ultrix 4.4 and others may have getrlimit but may not be able to
   * change number of file descriptors
   */
#ifdef RLIMIT_NOFILE
  struct rlimit *rlp;

  rlp = (struct rlimit *) malloc(sizeof(struct rlimit));
  if (getrlimit(RLIMIT_NOFILE, rlp)) {
    perror("init_rlimit: getrlimit()");
    free(rlp);
    return;
  }
  /* This check seems dumb, but apparently FreeBSD may return 0 for
   * the max # of descriptors!
   */
  if (rlp->rlim_max > rlp->rlim_cur) {
    rlp->rlim_cur = rlp->rlim_max;
    if (setrlimit(RLIMIT_NOFILE, rlp))
      perror("init_rlimit: setrlimit()");
  }
  free(rlp);
#endif
  return;
}
#endif				/* HAS_GETRLIMIT */

#ifndef BOOLEXP_DEBUGGING
#ifdef WIN32SERVICES
/* Under WIN32, MUSH is a "service", so we just start a thread here.
 * The real "main" is in win32/services.c
 */
void
mainthread(int argc, char **argv)
#else
/** The main function.
 * \param argc number of arguments.
 * \param argv vector of arguments.
 * \return exit code.
 */
int
main(int argc, char **argv)
#endif				/* WIN32SERVICES */
{
#ifdef AUTORESTART
  FILE *id;
#endif
  FILE *newerr;

  /* read the configuration file */
  if (argc < 2) {
    fprintf(stderr, "ERROR: Usage: %s /path/to/config_file\n", argv[0]);
    exit(2);
  }

#ifdef WIN32
  {
    unsigned short wVersionRequested = MAKEWORD(1, 1);
    int err;

    /* Need to include library: wsock32.lib for Windows Sockets */
    err = WSAStartup(wVersionRequested, &wsadata);
    if (err) {
      printf(T("Error %i on WSAStartup\n"), err);
      exit(1);
    }
  }
#endif				/* WIN32 */

#ifdef HAS_GETRLIMIT
  init_rlimit();		/* unlimit file descriptors */
#endif

  /* These are FreeBSDisms to fix floating point exceptions */
#ifdef HAS_FPSETROUND
  fpsetround(FP_RN);
#endif
#ifdef HAS_FPSETMASK
  fpsetmask(0L);
#endif

  time(&mudtime);

  /* If we have setlocale, call it to set locale info
   * from environment variables
   */
#ifdef HAS_SETLOCALE
  {
    char *loc;
    if ((loc = setlocale(LC_CTYPE, "")) == NULL)
      do_rawlog(LT_ERR, "Failed to set ctype locale from environment.");
    else
      do_rawlog(LT_ERR, "Setting ctype locale to %s", loc);
    if ((loc = setlocale(LC_TIME, "")) == NULL)
      do_rawlog(LT_ERR, "Failed to set time locale from environment.");
    else
      do_rawlog(LT_ERR, "Setting time locale to %s", loc);
    if ((loc = setlocale(LC_MESSAGES, "")) == NULL)
      do_rawlog(LT_ERR, "Failed to set messages locale from environment.");
    else
      do_rawlog(LT_ERR, "Setting messages locale to %s", loc);
    if ((loc = setlocale(LC_COLLATE, "")) == NULL)
      do_rawlog(LT_ERR, "Failed to set collate locale from environment.");
    else
      do_rawlog(LT_ERR, "Setting collate locale to %s", loc);
  }
#endif
#ifdef HAS_TEXTDOMAIN
  textdomain("pennmush");
#endif
#ifdef HAS_BINDTEXTDOMAIN
  bindtextdomain("pennmush", "../po");
#endif

  /* Build the locale-dependant tables used by PCRE */
  tables = pcre_maketables();

/* this writes a file used by the restart script to check for active mush */
#ifdef AUTORESTART
  id = fopen("runid", "w");
  fprintf(id, "%d", getpid());
  fclose(id);
#endif

  strncpy(confname, argv[1], BUFFER_LEN - 1);
  confname[BUFFER_LEN - 1] = '\0';
  init_game_config(confname);

  /* save a file descriptor */
  reserve_fd();
#ifndef WIN32
  extrafd = open("/dev/null", O_RDWR);
#endif

  /* decide if we're in @shutdown/reboot */
  restarting = 0;
  newerr = fopen(REBOOTFILE, "r");
  if (newerr) {
    restarting = 1;
    fclose(newerr);
  }

  init_qids();
  if (init_game_dbs() < 0) {
    do_rawlog(LT_ERR, T("ERROR: Couldn't load databases! Exiting."));
    exit(2);
  }

  init_game_postdb(confname);

  globals.database_loaded = 1;

  set_signals();

#ifdef INFO_SLAVE
  make_info_slave();
#endif

  /* go do it */
#ifdef CSRI
#ifdef CSRI_DEBUG
  mal_verify(1);
#endif
#ifdef CSRI_TRACE
  mal_leaktrace(1);
#endif
#endif
  load_reboot_db();

  shovechars((Port_t) TINYPORT, (Port_t) SSLPORT);
#ifdef CSRI
#ifdef CSRI_DEBUG
  mal_verify(1);
#endif
#endif

  /* someone has told us to shut down */
#ifdef WIN32SERVICES
  /* Keep service manager happy */
  shutdown_checkpoint();
#endif

  shutdown_queues();

#ifdef WIN32SERVICES
  /* Keep service manager happy */
  shutdown_checkpoint();
#endif

  close_sockets();
  sql_shutdown();

#ifdef INFO_SLAVE
  kill_info_slave();
#endif

#ifdef WIN32SERVICES
  /* Keep service manager happy */
  shutdown_checkpoint();
#endif

  dump_database();

  local_shutdown();

#ifdef RPMODE_SYS
  rplog_shutdown();
#endif

  end_all_logs();

#ifdef CSRI
#ifdef CSRI_PROFILESIZES
  mal_statsdump(stderr);
#endif
#ifdef CSRI_TRACE
  mal_dumpleaktrace(stderr);
#endif
  fflush(stderr);
#endif

#ifdef WIN32SERVICES
  /* Keep service manager happy */
  shutdown_checkpoint();
#endif

#ifdef HAS_GETRUSAGE
  rusage_stats();
#endif				/* HAS_RUSAGE */

  do_rawlog(LT_ERR, T("MUSH shutdown completed."));

  closesocket(sock);
#ifdef WIN32
#ifdef WIN32SERVICES
  shutdown_checkpoint();
#endif
  WSACleanup();			/* clean up */
#else
#ifdef __APPLE__
  unlink("runid");
#endif
  exit(0);
#endif
}
#endif				/* BOOLEXP_DEBUGGING */

/** Close and reopen the logfiles - called on SIGHUP */
void
reopen_logs(void)
{
  FILE *newerr;
  /* close up the log files */
  end_all_logs();
  newerr = fopen(errlog, "a");
  if (!newerr) {
    fprintf(stderr,
	    T("Unable to open %s. Error output continues to stderr.\n"),
	    errlog);
  } else {
    if (!freopen(errlog, "a", stderr)) {
      printf(T("Ack!  Failed reopening stderr!"));
      exit(1);
    }
    setvbuf(stderr, NULL, _IOLBF, BUFSIZ);
    fclose(newerr);
  }
  start_all_logs();
}

/** Install our default signal handlers. */
void
set_signals(void)
{

#ifndef WIN32
  /* we don't care about SIGPIPE, we notice it in select() and write() */
  ignore_signal(SIGPIPE);
  install_sig_handler(SIGUSR2, signal_dump);
  install_sig_handler(SIGQUIT, signal_shutdown);
  install_sig_handler(SIGINT, signal_shutdown);
  install_sig_handler(SIGTERM, bailout);
#else
  /* Win32 stuff: 
   *   No support for SIGUSR2 or SIGINT.
   *   SIGTERM is never generated on NT-based Windows (according to MSDN)
   *   MSVC++ will let you get away with installing a handler anyway,
   *   but VS.NET will not. So if it's MSVC++, we give it a try.
   */
#if _MSC_VER < 1200
  install_sig_handler(SIGTERM, bailout);
#endif
#endif

#ifndef WIN32
  install_sig_handler(SIGCHLD, reaper);
#endif

}

#ifdef WIN32
/** Get the time using Windows function call.
 * Looks weird, but it works. :-P
 * \param now address to store timeval data.
 */
static void
win_gettimeofday(struct timeval *now)
{

  FILETIME win_time;

  GetSystemTimeAsFileTime(&win_time);
  /* dwLow is in 100-s nanoseconds, not microseconds */
  now->tv_usec = win_time.dwLowDateTime % 10000000 / 10;

  /* dwLow contains at most 429 least significant seconds, since 32 bits maxint is 4294967294 */
  win_time.dwLowDateTime /= 10000000;

  /* Make room for the seconds of dwLow in dwHigh */
  /* 32 bits of 1 = 4294967295. 4294967295 / 429 = 10011578 */
  win_time.dwHighDateTime %= 10011578;
  win_time.dwHighDateTime *= 429;

  /* And add them */
  now->tv_sec = win_time.dwHighDateTime + win_time.dwLowDateTime;
}

#endif

/** Return the difference between two timeval structs as a timeval struct.
 * \param now pointer to the timeval to subtract from.
 * \param then pointer to the timeval to subtract.
 * \return pointer to a statically allocated timeval of the difference.
 */
static struct timeval *
timeval_sub(struct timeval *now, struct timeval *then)
{
  static struct timeval mytime;
  mytime.tv_sec = now->tv_sec;
  mytime.tv_usec = now->tv_usec;

  mytime.tv_sec -= then->tv_sec;
  mytime.tv_usec -= then->tv_usec;
  if (mytime.tv_usec < 0) {
    mytime.tv_usec += 1000000;
    mytime.tv_sec--;
  }
  return &mytime;
}

/** Return the difference between two timeval structs in milliseconds.
 * \param now pointer to the timeval to subtract from.
 * \param then pointer to the timeval to subtract.
 * \return milliseconds of difference between them.
 */
static long int
msec_diff(struct timeval *now, struct timeval *then)
{
  long int secs = now->tv_sec - then->tv_sec;
  if (secs == 0)
    return (now->tv_usec - then->tv_usec) / 1000;
  else if (secs == 1)
    return (now->tv_usec + (1000000 - then->tv_usec)) / 100;
  else if (secs > 1)
    return (secs * 1000) + ((now->tv_usec + (1000000 - then->tv_usec)) / 1000);
  else
    return 0;
}

/** Add a given number of milliseconds to a timeval.
 * \param t pointer to a timeval struct.
 * \param x number of milliseconds to add to t.
 * \return address of static timeval struct representing the sum.
 */
static struct timeval *
msec_add(struct timeval *t, int x)
{
  static struct timeval mytime;
  mytime.tv_sec = t->tv_sec;
  mytime.tv_usec = t->tv_usec;
  mytime.tv_sec += x / 1000;
  mytime.tv_usec += (x % 1000) * 1000;
  if (mytime.tv_usec >= 1000000) {
    mytime.tv_sec += mytime.tv_usec / 1000000;
    mytime.tv_usec = mytime.tv_usec % 1000000;
  }
  return &mytime;
}

/** Update each descriptor's allowed rate of issuing commands.
 * Players are rate-limited; they may only perform up to a certain
 * number of commands per time slice. This function is run periodically
 * to refresh each descriptor's available command quota based on how
 * many slices have passed since it was last updated.
 * \param last pointer to timeval struct of last time quota was updated.
 * \param current pointer to timeval struct of current time.
 */
static void
update_quotas(struct timeval *last, struct timeval *current)
{
  int nslices;
  DESC *d;
  nslices = (int) msec_diff(current, last) / COMMAND_TIME_MSEC;

  if (nslices > 0) {
    for (d = descriptor_list; d; d = d->next) {
      d->quota += COMMANDS_PER_TIME * nslices;
      if (d->quota > COMMAND_BURST_SIZE)
	d->quota = COMMAND_BURST_SIZE;
    }
  }
}

static const char *empabb(dbref player) {
        static char str[4];
        ATTR *a;
	/*
        dbref start, end, last;
	*/
	dbref start;

        memset(str, '\0', 4);

        if(!IsDivision(SDIV(player).object))
               goto bad_empabb_value;
        start = SDIV(player).object;

	/*
        for(last = end = start; GoodObject(end) && IsDivision(end) &&
                        !has_flag_by_name(end, "EMPIRE", TYPE_DIVISION) ; last = end, end = SDIV(end).object)
                ;
        if(!has_flag_by_name(end, "EMPIRE", TYPE_DIVISION)) {
                if(end == NOTHING && IsDivision(last))
                        end = last;
                else end = start;
        }
	*/
        /* K, end is the empire we're grabbing this off of */
        a = atr_get(start, "ALIAS");
        if(!a)
                goto bad_empabb_value;
        strncpy(str, atr_value(a), 3);
        if(!str[0])
                goto bad_empabb_value;
        return str;


bad_empabb_value:
        strncpy(str, "---", 3);
        return str;
}


static void
shovechars(Port_t port, Port_t sslport __attribute__ ((__unused__)))
{
  /* this is the main game loop */

  fd_set input_set, output_set;
  time_t now;
  struct timeval last_slice, current_time, then;
  struct timeval next_slice, *returned_time;
  struct timeval timeout, slice_timeout;
  int found;
  int queue_timeout;
  DESC *d, *dnext;
#ifndef INFO_SLAVE
  DESC *newd;
  int result;
#endif
  int avail_descriptors;
#ifdef INFO_SLAVE
  union sockaddr_u addr;
  socklen_t addr_len;
  int newsock;
#endif
  unsigned long input_ready, output_ready;

  if (!restarting) {
    sock = make_socket(port, NULL, NULL, MUSH_IP_ADDR);
    if (sock >= maxd)
      maxd = sock + 1;
#ifdef HAS_OPENSSL
    if (sslport) {
      sslsock = make_socket(sslport, NULL, NULL, SSL_IP_ADDR);
      ssl_master_socket = ssl_setup_socket(sslsock);
      if (sslsock >= maxd)
	maxd = sslsock + 1;
    }
#endif
  }
  our_gettimeofday(&last_slice);

  avail_descriptors = how_many_fds() - 4;
#ifdef INFO_SLAVE
  avail_descriptors -= 2;	/* reserve some more for setting up the slave */
  FD_ZERO(&info_pending);
#endif

  /* done. print message to the log */
  do_rawlog(LT_ERR, "%d file descriptors available.", avail_descriptors);
  do_rawlog(LT_ERR, "RESTART FINISHED.");

  our_gettimeofday(&then);

  while (shutdown_flag == 0) {
    our_gettimeofday(&current_time);

    update_quotas(&last_slice, &current_time);
    last_slice.tv_sec = current_time.tv_sec;
    last_slice.tv_usec = current_time.tv_usec;

    if (msec_diff(&current_time, &then) >= 1000) {
      globals.on_second = 1;
      then.tv_sec = current_time.tv_sec;
      then.tv_usec = current_time.tv_usec;
    }

    process_commands();

    if (signal_shutdown_flag) {
      flag_broadcast(0, 0, T("GAME: Shutdown by external signal"));
      do_rawlog(LT_ERR, T("SHUTDOWN by external signal"));
#ifdef AUTORESTART
      system("touch NORESTART");
#endif
      shutdown_flag = 1;
    }

    if (signal_dump_flag) {
      globals.paranoid_dump = 0;
      do_rawlog(LT_CHECK, "DUMP by external signal");
      fork_and_dump(1);
      signal_dump_flag = 0;
    }

    if (shutdown_flag)
      break;

    /* test for events */
    dispatch();

    /* any queued robot commands waiting? */
    /* timeout.tv_sec used to be set to que_next(), the number of
     * seconds before something on the queue needed to run, but this
     * caused a problem with stuff that had to be triggered by alarm
     * signal every second, so we're reduced to what's below:
     */
    queue_timeout = que_next();
    timeout.tv_sec = queue_timeout ? 1 : 0;
    timeout.tv_usec = 0;

    returned_time = msec_add(&last_slice, COMMAND_TIME_MSEC);
    next_slice.tv_sec = returned_time->tv_sec;
    next_slice.tv_usec = returned_time->tv_usec;

    returned_time = timeval_sub(&next_slice, &current_time);
    slice_timeout.tv_sec = returned_time->tv_sec;
    slice_timeout.tv_usec = returned_time->tv_usec;
    /* Make sure slice_timeout cannot have a negative time. Better
       safe than sorry. */
    if (slice_timeout.tv_sec < 0)
      slice_timeout.tv_sec = 0;
    if (slice_timeout.tv_usec < 0)
      slice_timeout.tv_usec = 0;

    FD_ZERO(&input_set);
    FD_ZERO(&output_set);
    if (ndescriptors < avail_descriptors)
      FD_SET(sock, &input_set);
#ifdef HAS_OPENSSL
    if (sslsock)
      FD_SET(sslsock, &input_set);
#endif
#ifdef INFO_SLAVE
    if (info_slave_state > 0)
      FD_SET(info_slave, &input_set);
#endif
    for (d = descriptor_list; d; d = d->next) {
      if (d->input.head) {
	timeout.tv_sec = slice_timeout.tv_sec;
	timeout.tv_usec = slice_timeout.tv_usec;
      } else
	FD_SET(d->descriptor, &input_set);
      if (d->output.head)
	FD_SET(d->descriptor, &output_set);
    }

    found = select(maxd, &input_set, &output_set, (fd_set *) 0, &timeout);
    if (found < 0) {
#ifdef WIN32
      if (found == SOCKET_ERROR && WSAGetLastError() != WSAEINTR)
#else
      if (errno != EINTR)
#endif
      {
	perror("select");
	return;
      }
#ifdef INFO_SLAVE
      now = mudtime;
      if (info_slave_state == 2 && now > info_queue_time + 30) {
	/* rerun any pending queries that got lost */
	info_queue_time = now;
	for (newsock = 0; newsock < maxd; newsock++)
	  if (FD_ISSET(newsock, &info_pending))
	    query_info_slave(newsock);
      }
#endif
    } else {
      /* if !found then time for robot commands */

      if (!found) {
	do_top(options.queue_chunk);
	continue;
      } else {
	do_top(options.active_q_chunk);
      }
      now = mudtime;
#ifdef INFO_SLAVE
      if (info_slave_state > 0 && FD_ISSET(info_slave, &input_set)) {
	if (info_slave_state == 1)
	  promote_info_slave();
	else {
	  reap_info_slave();
	}
      } else if (info_slave_state == 2 && now > info_queue_time + 30) {
	/* rerun any pending queries that got lost */
	info_queue_time = now;
	for (newsock = 0; newsock < maxd; newsock++)
	  if (FD_ISSET(newsock, &info_pending))
	    query_info_slave(newsock);
      }

      if (FD_ISSET(sock, &input_set)) {
	addr_len = sizeof(addr);
	newsock = accept(sock, (struct sockaddr *) &addr, &addr_len);
	if (newsock < 0) {
	  if (test_connection(newsock) < 0)
	    continue;		/* this should _not_ be return. */
	}
	ndescriptors++;
	query_info_slave(newsock);
	if (newsock >= maxd)
	  maxd = newsock + 1;
      }
#ifdef HAS_OPENSSL
      if (sslsock && FD_ISSET(sslsock, &input_set)) {
	addr_len = sizeof(addr);
	newsock = accept(sslsock, (struct sockaddr *) &addr, &addr_len);
	if (newsock < 0) {
	  if (test_connection(newsock) < 0)
	    continue;		/* this should _not_ be return. */
	}
	ndescriptors++;
	query_info_slave(newsock);
	if (newsock >= maxd)
	  maxd = newsock + 1;
      }
#endif
#else				/* INFO_SLAVE */
      if (FD_ISSET(sock, &input_set)) {
	if (!(newd = new_connection(sock, &result, 0))) {
	  if (test_connection(result) < 0)
	    continue;		/* this should _not_ be return. */
	} else {
	  ndescriptors++;
	  if (newd->descriptor >= maxd)
	    maxd = newd->descriptor + 1;
	}
      }
#ifdef HAS_OPENSSL
      if (sslsock && FD_ISSET(sslsock, &input_set)) {
	if (!(newd = new_connection(sslsock, &result, 1))) {
	  if (test_connection(result) < 0)
	    continue;		/* this should _not_ be return. */
	} else {
	  ndescriptors++;
	  if (newd->descriptor >= maxd)
	    maxd = newd->descriptor + 1;
	}
      }
#endif
#endif
      for (d = descriptor_list; d; d = dnext) {
	dnext = d->next;
	input_ready = FD_ISSET(d->descriptor, &input_set);
	output_ready = FD_ISSET(d->descriptor, &output_set);
	if (input_ready) {
	  if (!process_input(d, output_ready)) {
	    shutdownsock(d);
	    continue;
	  }
	}
	if (output_ready) {
	  if (!process_output(d)) {
	    shutdownsock(d);
	  }
	}
      }
    }
  }
}

static int
test_connection(int newsock)
{
#ifdef WIN32
  if (newsock == INVALID_SOCKET && WSAGetLastError() != WSAEINTR)
#else
  if (errno && errno != EINTR)
#endif
  {
    perror("test_connection");
    return -1;
  }
  return newsock;
}


#ifndef INFO_SLAVE
static DESC *
new_connection(int oldsock, int *result, int use_ssl)
{
  int newsock;
  union sockaddr_u addr;
  struct hostname_info *hi;
  socklen_t addr_len;
  char tbuf1[BUFFER_LEN];
  char tbuf2[BUFFER_LEN];
  char *bp;
  char *socket_ident;
  char *chp;

  *result = 0;
  addr_len = MAXSOCKADDR;
  newsock = accept(oldsock, (struct sockaddr *) (addr.data), &addr_len);
  if (newsock < 0) {
    *result = newsock;
    return 0;
  }
  bp = tbuf2;
  hi = ip_convert(&addr.addr, addr_len);
  safe_str(hi ? hi->hostname : "", tbuf2, &bp);
  *bp = '\0';
  bp = tbuf1;
  if (USE_IDENT) {
    int timeout = IDENT_TIMEOUT;
    socket_ident = ident_id(newsock, &timeout);
    if (socket_ident) {
      /* Truncate at first non-printable character */
      for (chp = socket_ident; *chp && isprint((unsigned char) *chp); chp++) ;
      *chp = '\0';
      safe_str(socket_ident, tbuf1, &bp);
      safe_chr('@', tbuf1, &bp);
      free(socket_ident);
    }
  }
  hi = hostname_convert(&addr.addr, addr_len);
  safe_str(hi ? hi->hostname : "", tbuf1, &bp);
  *bp = '\0';
  if (Forbidden_Site(tbuf1) || Forbidden_Site(tbuf2)) {
    if (!Deny_Silent_Site(tbuf1, AMBIGUOUS)
	|| !Deny_Silent_Site(tbuf2, AMBIGUOUS)) {
      do_log(LT_CONN, 0, 0, "[%d/%s/%s] %s (%s %s)", newsock, tbuf1, tbuf2,
	     T("Refused connection"), T("remote port"),
	     hi ? hi->port : T("(unknown)"));
    }
    shutdown(newsock, 2);
    closesocket(newsock);
#ifndef WIN32
    errno = 0;
#endif
    return 0;
  }
  do_log(LT_CONN, 0, 0, T("[%d/%s/%s] Connection opened."), newsock, tbuf1,
	 tbuf2);
  set_keepalive(newsock);
  return initializesock(newsock, tbuf1, tbuf2, use_ssl);
}
#endif

static void
clearstrings(DESC *d)
{
  if (d->output_prefix) {
    mush_free((Malloc_t) d->output_prefix, "userstring");
    d->output_prefix = 0;
  }
  if (d->output_suffix) {
    mush_free((Malloc_t) d->output_suffix, "userstring");
    d->output_suffix = 0;
  }
}

/* Display a cached text file. If a prefix line was given,
 * display that line before the text file, but only if we've
 * got a text file to display
 */
static void
fcache_dump(DESC *d, FBLOCK fb[2], const unsigned char *prefix)
{
  /* If we've got nothing nice to say, don't say anything */
  if (!fb[0].buff && !((d->conn_flags & CONN_HTML) && fb[1].buff))
    return;
  /* We've got something to say */
  if (prefix) {
    queue_newwrite(d, prefix, u_strlen(prefix));
    queue_eol(d);
  }
  if (d->conn_flags & CONN_HTML) {
    if (fb[1].buff)
      queue_newwrite(d, fb[1].buff, fb[1].len);
    else
      queue_write(d, fb[0].buff, fb[0].len);
  } else
    queue_write(d, fb[0].buff, fb[0].len);
}


static int
fcache_read(FBLOCK *fb, const char *filename)
{
  if (!fb || !filename)
    return -1;

  /* Free prior cache */
  if (fb->buff) {
    mush_free(fb->buff, "fcache_data");
  }

  fb->buff = NULL;
  fb->len = 0;

#ifdef WIN32
  /* Win32 read code here */
  {
    HANDLE fh;
    BY_HANDLE_FILE_INFORMATION sb;
    DWORD r = 0;


    if ((fh = CreateFile(filename, GENERIC_READ, 0, NULL,
			 OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE)
      return -1;

    if (!GetFileInformationByHandle(fh, &sb)) {
      CloseHandle(fh);
      return -1;
    }

    fb->len = sb.nFileSizeLow;

    if (!(fb->buff = mush_malloc(sb.nFileSizeLow, "fcache_data"))) {
      CloseHandle(fh);
      return -1;
    }

    if (!ReadFile(fh, fb->buff, sb.nFileSizeLow, &r, NULL) || fb->len != r) {
      CloseHandle(fh);
      mush_free(fb->buff, "fcache_data");
      fb->buff = NULL;
      return -1;
    }

    CloseHandle(fh);

    fb->len = sb.nFileSizeLow;
    return (int) fb->len;
  }
#else
  /* Posix read code here */
  {
    int fd, n;
    struct stat sb;

    release_fd();
    if ((fd = open(filename, O_RDONLY, 0)) < 0) {
      do_log(LT_ERR, 0, 0, T("Couldn't open cached text file '%s'"), filename);
      reserve_fd();
      return -1;
    }

    if (fstat(fd, &sb) < 0) {
      do_log(LT_ERR, 0, 0, T("Couldn't get the size of text file '%s'"),
	     filename);
      close(fd);
      reserve_fd();
      return -1;
    }


    if (!(fb->buff = mush_malloc(sb.st_size, "fcache_data"))) {
      do_log(LT_ERR, 0, 0, T("Couldn't allocate %d bytes of memory for '%s'!"),
	     (int) sb.st_size, filename);
      close(fd);
      reserve_fd();
      return -1;
    }

    if ((n = read(fd, fb->buff, sb.st_size)) != sb.st_size) {
      do_log(LT_ERR, 0, 0, T("Couldn't read all of '%s'"), filename);
      close(fd);
      mush_free(fb->buff, "fcache_data");
      fb->buff = NULL;
      reserve_fd();
      return -1;
    }

    close(fd);
    reserve_fd();
    fb->len = sb.st_size;
  }
#endif				/* Posix read code */

  return fb->len;
}

/** Load all of the cached text files.
 * \param player the enactor.
 */
void
fcache_load(dbref player)
{
  int conn, motd, new, reg, quit, down, full;
  int guest;
  int i;

  for (i = 0; i < (SUPPORT_PUEBLO ? 2 : 1); i++) {
    conn = fcache_read(&fcache.connect_fcache[i], options.connect_file[i]);
    motd = fcache_read(&fcache.motd_fcache[i], options.motd_file[i]);
    new = fcache_read(&fcache.newuser_fcache[i], options.newuser_file[i]);
    reg = fcache_read(&fcache.register_fcache[i], options.register_file[i]);
    quit = fcache_read(&fcache.quit_fcache[i], options.quit_file[i]);
    down = fcache_read(&fcache.down_fcache[i], options.down_file[i]);
    full = fcache_read(&fcache.full_fcache[i], options.full_file[i]);
    guest = fcache_read(&fcache.guest_fcache[i], options.guest_file[i]);

    if (player != NOTHING) {
      notify_format(player,
		    T
		    ("%s sizes:  NewUser...%d  Connect...%d  Guest...%d  Motd...%d  Quit...%d  Register...%d  Down...%d  Full...%d"),
		    i ? "HTMLFile" : "File", new, conn, guest, motd, quit,
		    reg, down, full);
    }
  }

}

/** Initialize all of the cached text files (at startup).
 */
void
fcache_init(void)
{
  fcache_load(NOTHING);
}

static void
logout_sock(DESC *d)
{
  SU_PATH *path_entry;

  int n;
  char tbuf1[BUFFER_LEN];

  if (d->connected) {
    fcache_dump(d, fcache.quit_fcache, NULL);
    do_log(LT_CONN, 0, 0,
	   T("[%d/%s/%s] Logout by %s(#%d) <Connection not dropped>"),
	   d->descriptor, d->addr, d->ip, Name(d->player), d->player);
    if(d->last_time > 0) {
      d->idle_total += difftime(mudtime, d->last_time);
      d->unidle_times++;
    }
    snprintf(tbuf1, BUFFER_LEN-1, "%ld %ld %d %d", (mudtime - d->connected_at),
	d->idle_total, d->unidle_times, d->cmds); 
    tbuf1[strlen(tbuf1)+1] = '\0';
    (void) atr_add(d->player, "LASTACTIVITY", tbuf1, GOD, NOTHING);
    announce_disconnect(d->player);
    do_mail_purge(d->player);
    if (MAX_LOGINS) {
      login_number--;
      if (!under_limit && (login_number < MAX_LOGINS)) {
	under_limit = 1;
	do_log(LT_CONN, 0, 0,
	       T("Below maximum player limit of %d. Logins enabled."),
	       MAX_LOGINS);
      }
    }
  } else {
    do_log(LT_CONN, 0, 0,
	   T("[%d/%s/%s] Logout, never connected. <Connection not dropped>"),
	   d->descriptor, d->addr, d->ip);
  }
  process_output(d);		/* flush our old output */
  /* pretend we have a new connection */
  d->input_handler = do_command;
  d->connected = 0;
  d->output_prefix = 0;
  d->output_suffix = 0;
  d->output_size = 0;
  d->output.head = 0;
  d->player = 0;
  d->output.tail = &d->output.head;
  d->input.head = 0;
  d->input.tail = &d->input.head;
  d->raw_input = 0;
  d->raw_input_at = 0;
  d->quota = COMMAND_BURST_SIZE;
  d->last_time = mudtime;
  d->idle_total = 0;
  d->unidle_times = 0;
  d->cmds = 0;
  d->hide = 0;
  d->doing[0] = '\0';
  d->mailp = NULL;
  d->pinfo.object = NOTHING;
  d->pinfo.atr = NULL;
  d->pinfo.lock = 0;
  d->pinfo.function = NULL;

  while(d->su_exit_path) {
    path_entry = d->su_exit_path;
    d->su_exit_path = path_entry->next;
    mush_free(path_entry, "SU_EXIT_PATH");
  }
  welcome_user(d);
  for(n = 0; n < MAX_SNOOPS; n++)
    d->snooper[n] = -1;
}

/** Disconnect a descriptor.
 * This sends appropriate disconnection text, flushes output, and
 * then closes the associated socket.
 * \param d pointer to descriptor to disconnect.
 */
static void
shutdownsock(DESC *d)
{
  char tbuf1[BUFFER_LEN];
  if (d->connected) {
    do_log(LT_CONN, 0, 0, T("[%d/%s/%s] Logout by %s(#%d)"),
	   d->descriptor, d->addr, d->ip, Name(d->player), d->player);
    if (d->connected != 2) {
      fcache_dump(d, fcache.quit_fcache, NULL);
      /* Player was not allowed to log in from the connect screen */
      if(d->last_time > 0) {
	d->idle_total += difftime(mudtime, d->last_time);
	d->unidle_times++;
      }
      snprintf(tbuf1, BUFFER_LEN-1, "%ld %ld %d %d", (mudtime - d->connected_at), 
	  d->idle_total , d->unidle_times, d->cmds);
      tbuf1[strlen(tbuf1)+1] = '\0';
      (void) atr_add(d->player, "LASTACTIVITY", tbuf1, GOD, NOTHING);
      announce_disconnect(d->player);
      do_mail_purge(d->player);
    }
    if (MAX_LOGINS) {
      login_number--;
      if (!under_limit && (login_number < MAX_LOGINS)) {
	under_limit = 1;
	do_log(LT_CONN, 0, 0,
	       T("Below maximum player limit of %d. Logins enabled."),
	       MAX_LOGINS);
      }
    }
  } else {
    do_log(LT_CONN, 0, 0, T("[%d/%s/%s] Connection closed, never connected."),
	   d->descriptor, d->addr, d->ip);
  }
  process_output(d);
  clearstrings(d);
  shutdown(d->descriptor, 2);
  closesocket(d->descriptor);
  if (d->prev)
    d->prev->next = d->next;
  else				/* d was the first one! */
    descriptor_list = d->next;
  if (d->next)
    d->next->prev = d->prev;

#ifdef HAS_OPENSSL
  if (sslsock && d->ssl) {
    ssl_close_connection(d->ssl);
    d->ssl = NULL;
  }
#endif

  {
    freeqs(d);
    mush_free(d->ttype, "terminal description");
    mush_free((Malloc_t) d, "descriptor");
  }

  ndescriptors--;
}

/* ARGSUSED */
static DESC *
initializesock(int s, char *addr, char *ip, int use_ssl
		__attribute__ ((__unused__)))
{
  DESC *d;
  int n;

  d = (DESC *) mush_malloc(sizeof(DESC), "descriptor");
  if (!d)
    mush_panic("Out of memory.");
  d->descriptor = s;
  d->input_handler = do_command;
  d->connected = 0;
  d->connected_at = mudtime;
  make_nonblocking(s);
  d->output_prefix = 0;
  d->output_suffix = 0;
  d->output_size = 0;
  d->output.head = 0;
  d->player = 0;
  d->output.tail = &d->output.head;
  d->input.head = 0;
  d->input.tail = &d->input.head;
  d->raw_input = 0;
  d->raw_input_at = 0;
  d->quota = COMMAND_BURST_SIZE;
  d->last_time = mudtime;
  d->idle_total = 0;
  d->unidle_times = 0;
  d->cmds = 0;
  d->hide = 0;
  d->doing[0] = '\0';
  d->mailp = NULL;
  strncpy(d->addr, addr, 100);
  d->addr[99] = '\0';
  strncpy(d->ip, ip, 100);
  d->ip[99] = '\0';
  d->conn_flags = CONN_DEFAULT;
  d->input_chars = 0;
  d->output_chars = 0;
  d->ttype = mush_strdup("unknown", "terminal description");
  d->checksum[0] = '\0';
  d->su_exit_path = NULL;
  d->pinfo.object = NOTHING;
  d->pinfo.atr = NULL;
  d->pinfo.lock = 0;
  d->pinfo.function = NULL;
#ifdef HAS_OPENSSL
  d->ssl = NULL;
  d->ssl_state = 0;
#endif
  if (descriptor_list)
    descriptor_list->prev = d;
  d->next = descriptor_list;
  d->prev = NULL;
  descriptor_list = d;
  d->width = 78;
  d->height = 24;
#ifdef HAS_OPENSSL
  if (use_ssl && sslsock) {
    d->ssl = ssl_listen(d->descriptor, &d->ssl_state);
    if (d->ssl_state < 0) {
      /* Error we can't handle */
      ssl_close_connection(d->ssl);
      d->ssl = NULL;
      d->ssl_state = 0;
    }
  }
#endif
  test_telnet(d);
  welcome_user(d);
  for(n = 0; n < MAX_SNOOPS; n++)
    d->snooper[n] = -1;
  return d;
}

#ifdef INFO_SLAVE
static void
make_info_slave(void)
{
#ifdef HAS_SOCKETPAIR
  int socks[2];
#else
  union sockaddr_u addr;
  socklen_t opt;
#endif
  char num[NI_MAXSERV];
  Pid_t child;
  int n;

  if (info_slave_state != 0) {
    if (info_slave_pid > 0) {
      closesocket(info_slave);
      kill(info_slave_pid, 15);
      info_slave_pid = -1;
    }
    info_slave_state = 0;
  }
#ifdef HAS_SOCKETPAIR
  /* Use Posix.1g names... */
#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif
  if (socketpair(AF_LOCAL, SOCK_STREAM, 0, socks) < 0) {
    perror("creating slave stream socketpair");
    return;
  }
  if (socks[0] >= maxd)
    maxd = socks[0] + 1;
  if (socks[1] >= maxd)
    maxd = socks[1] + 1;
#else
  info_slave = make_socket(0, &addr, &opt, MUSH_IP_ADDR);
  if (getsockname(info_slave, (struct sockaddr *) &addr.data, &opt) < 0) {
    perror("getsockname");
    fflush(stderr);
    closesocket(info_slave);
    return;
  }
  if (getnameinfo(&addr.addr, opt, NULL, 0, num, sizeof num,
		  NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
    perror("getting address of slave stream socket");
    fflush(stderr);
    closesocket(info_slave);
    return;
  }
  listen(info_slave, 1);
#endif
  child = fork();
  if (child < 0) {
    perror("forking info slave");
#ifdef HAS_SOCKETPAIR
    closesocket(socks[0]);
    closesocket(socks[1]);
#else
    closesocket(info_slave);
#endif
    return;
  }
  if (child) {
    info_slave_state = 1;
    info_slave_pid = child;
#ifdef HAS_SOCKETPAIR
    info_slave = socks[0];
    closesocket(socks[1]);
    do_rawlog(LT_ERR,
	      "Spawning info slave using socketpair, pid %d, ident %d",
	      child, USE_IDENT);
#else
    do_rawlog(LT_ERR, "Spawning info slave on port %s, pid %d, ident %d",
	      num, child, USE_IDENT);
#endif
  } else {
    /* Close unneeded fds and sockets */
    for (n = 3; n < maxd; n++) {
      if (n == fileno(stderr))
	continue;
#ifdef HAS_SOCKETPAIR
      if (n == socks[1])
	continue;
#endif
      close(n);
    }
#ifdef HAS_SOCKETPAIR
    sprintf(num, "%d", socks[1]);
#endif
    if (!USE_IDENT)
      execl("./info_slave", "./info_slave", num, "-1", USE_DNS ? "1" : "0",
	    (char *) NULL);
    else
      execl("./info_slave", "./info_slave", num, tprintf("%d", IDENT_TIMEOUT),
	    USE_DNS ? "1" : "0", (char *) NULL);
    perror("execing info slave");
    exit(1);
  }
  info_query_spill = 0;
  info_reap_spill = 0;
  if (info_slave >= maxd)
    maxd = info_slave + 1;
#ifdef HAS_SOCKETPAIR
  promote_info_slave();
#endif
}

static void
promote_info_slave(void)
{
  int j;
#ifndef HAS_SOCKETPAIR
  int newsock;
  union sockaddr_u addr;
  socklen_t addr_len;
  char port[NI_MAXSERV];
#endif

  if (info_slave_state != 1) {
    make_info_slave();
    return;
  }
#ifndef HAS_SOCKETPAIR
  addr_len = MAXSOCKADDR;
  newsock = accept(info_slave, (struct sockaddr *) addr.data, &addr_len);
  if (newsock < 0) {
    perror("accepting info slave connection");
    make_info_slave();
    return;
  }
  closesocket(info_slave);
  info_slave = newsock;
#endif
  make_nonblocking(info_slave);
  /* Do authentication here, if we care */
  info_slave_state = 2;
#ifdef HAS_SOCKETPAIR
  do_rawlog(LT_ERR, "Accepted info slave from unix-domain socket");
#else
  if (getnameinfo(&addr.addr, addr_len, NULL, 0, port, sizeof port,
		  NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
    perror("getting info slave port number");
  } else {
    do_rawlog(LT_ERR, "Accepted info slave from port %s", port);
  }
#endif
  for (j = 0; j < maxd; j++)
    if (FD_ISSET(j, &info_pending))
      query_info_slave(j);
  if (info_slave >= maxd)
    maxd = info_slave + 1;
}

static void
query_info_slave(int fd)
{
  int size, slen;
  socklen_t llen, rlen;
  static char buf[1024];	/* overkill */
  union sockaddr_u laddr, raddr;
  char *bp;
  struct hostname_info *hi;
  struct iovec dat[6];

  FD_SET(fd, &info_pending);
  info_queue_time = mudtime;

  if (info_slave_state != 2) {
    make_info_slave();
    return;
  }

  /* cleanup for truncated packet */
  size = info_query_spill;
  memset(buf, 0, size);
  info_query_spill = 0;
  dat[0].iov_base = buf;
  dat[0].iov_len = size;

  rlen = MAXSOCKADDR;
  if (getpeername(fd, (struct sockaddr *) raddr.data, &rlen) < 0) {
    perror("socket peer vanished");
    shutdown(fd, 2);
    closesocket(fd);
    FD_CLR(fd, &info_pending);
    return;
  }

  /* Check for forbidden sites before bothering with ident */
  bp = buf;
  hi = ip_convert(&raddr.addr, rlen);
  safe_str(hi ? hi->hostname : "Not found", buf, &bp);
  *bp = '\0';
  if (Forbidden_Site(buf)) {
    char port[NI_MAXSERV];
    if (getnameinfo(&raddr.addr, rlen, NULL, 0, port, sizeof port,
		    NI_NUMERICHOST | NI_NUMERICSERV) != 0)
      perror("getting remote port number");
    else {
      if (!Deny_Silent_Site(buf, AMBIGUOUS)) {
	do_log(LT_CONN, 0, 0, T("[%d/%s] Refused connection (remote port %s)"),
	       fd, buf, port);
      }
    }
    shutdown(fd, 2);
    closesocket(fd);
    FD_CLR(fd, &info_pending);
    return;
  }

  /* Packet format: SIZESTRUCT for remote address, SIZE STRUCT for
   * local address, FD. We use writev() to make the packet format
   * explicit to the reader.  */
  dat[1].iov_base = (char *) &rlen;
  dat[1].iov_len = sizeof rlen;
  dat[2].iov_base = (char *) &raddr.addr;
  dat[2].iov_len = rlen;

  llen = MAXSOCKADDR;
  if (getsockname(fd, (struct sockaddr *) laddr.data, &llen) < 0) {
    perror("socket self vanished");
    closesocket(fd);
    FD_CLR(fd, &info_pending);
    return;
  }

  dat[3].iov_base = (char *) &llen;
  dat[3].iov_len = sizeof llen;
  dat[4].iov_base = (char *) &laddr.addr;
  dat[4].iov_len = llen;
  dat[5].iov_base = (char *) &fd;
  dat[5].iov_len = sizeof fd;
  size = dat[0].iov_len + dat[1].iov_len + dat[2].iov_len + dat[3].iov_len
    + dat[4].iov_len + dat[5].iov_len;
  slen = writev(info_slave, dat, 6);
  if (slen < 0) {
    perror("info slave query: write error");
    if (errno != EWOULDBLOCK)
      make_info_slave();
    return;
  } else if (slen < size) {
    /* drop partial packet on floor.  cleanup later. */
    perror("info slave query: partial packet");
    info_query_spill = size - slen;
  }
}

static void
reap_info_slave(void)
{
  int fd, len, size;
  static char buf[10000];	/* overkill */
  char *bp, *bp2;
  struct iovec dat[2];

  if (info_slave_state != 2) {
    make_info_slave();
    return;
  }

  if (info_reap_spill) {
    /* clean up some mess */
    len = read(info_slave, buf, info_reap_spill);
    if (len < 1) {
      /* crap.  lost the slave. */
      perror("info slave reap spill");
      make_info_slave();
      return;
    }
    info_reap_spill -= len;
    if (info_reap_spill)
      /* can't read enough, come back later */
      return;
  }
  for (;;) {
    /* Packet format: FD, STRLEN, STR. We use readv() to cut down on
       the number of read() system calls. */
    /* grab the fd and string length */
    dat[0].iov_base = (char *) &fd;
    dat[0].iov_len = sizeof fd;
    dat[1].iov_base = (char *) &size;
    dat[1].iov_len = sizeof size;
    len = readv(info_slave, dat, 2);
    if (len < 0 && errno == EWOULDBLOCK)
      /* got all the data */
      return;
    if (len < (int) (sizeof fd + sizeof size)) {
      /* we're hosed now */
      perror("info slave reap reading fd and hostname length");
      make_info_slave();
      return;
    }
    if (size < 0 || size > (int) sizeof buf) {
      perror("info slave real size");
      make_info_slave();
      return;
    }

    /* grab the actual string */
    len = read(info_slave, buf, size);

    if (len < 0) {
      /* crap.  lost the slave. */
      perror("info slave reap string");
      make_info_slave();
      return;
    }
    buf[len] = '\0';
    if (len < size) {
      /* crap... lost some.  clean up the mess and requery */
      info_reap_spill = size - len;
      query_info_slave(fd);
      return;
    }
    /* okay, now we have some info! */
    if (!FD_ISSET(fd, &info_pending))
      /* got a bloody duplicate */
      return;
    FD_CLR(fd, &info_pending);
    /* buf ideally contains ipaddr^local port^ident-info */
    bp = NULL;
    if ((bp2 = strchr(buf, '^'))) {
      *bp2++ = '\0';
      /* buf is ip addr, bp is local port^ident info */
      if ((bp = strchr(bp2, '^')))
	*bp++ = '\0';
    }
    /* Now, either buf = ipaddr, bp2 = port, bp = ident info,
     * or buf = ipaddr, bp2 = port
     */
    if (Forbidden_Site(buf) || (bp && Forbidden_Site(bp))) {
      if (!Deny_Silent_Site(buf, AMBIGUOUS) || !Deny_Silent_Site(bp, AMBIGUOUS)) {
	do_log(LT_CONN, 0, 0, T("[%d/%s/%s] Refused connection."), fd,
	       bp ? bp : "", buf);
      }
      shutdown(fd, 2);
      closesocket(fd);
      return;
    }
    do_log(LT_CONN, 0, 0, T("[%d/%s/%s] Connection opened."), fd,
	   bp ? bp : "", buf);
    set_keepalive(fd);
    (void) initializesock(fd, bp ? bp : buf, buf, (atoi(bp2) == SSLPORT));
  }
}

/** Kill the info_slave process, typically at shutdown.
 */
void
kill_info_slave(void)
{
  WAIT_TYPE my_stat;
  Pid_t pid;
  struct timeval pad;

  if (info_slave_state != 0) {
    if (info_slave_pid > 0) {
      do_rawlog(LT_ERR, "kill: killing info_slave_pid %d", info_slave_pid);

      block_a_signal(SIGCHLD);

      closesocket(info_slave);
      kill(info_slave_pid, 15);
      /* Have to wait long enough for the info_slave to actually
         die. This will hopefully be enough time. */
      pad.tv_sec = 0;
      pad.tv_usec = 100;
      select(0, NULL, NULL, NULL, &pad);

#ifdef HAS_WAITPID
      pid = waitpid(info_slave_pid, &my_stat, WNOHANG);
#else
      pid = wait3(&my_stat, WNOHANG, 0);
#endif
      info_slave_pid = -1;
      unblock_a_signal(SIGCHLD);
    }
    info_slave_state = 0;
  }
}
#endif



/** Flush pending output for a descriptor.
 * This function actually sends the queued output over the descriptor's
 * socket.
 * \param d pointer to descriptor to send output to.
 * \retval 1 successfully flushed at least some output.
 * \retval 0 something failed, and the descriptor should probably be closed.
 */
int
process_output(DESC *d)
{
  struct text_block **qp, *cur;
  int cnt;
#ifdef HAS_OPENSSL
  int input_ready = 0;
#endif

#ifdef HAS_OPENSSL
  /* Insure that we're not in a state where we need an SSL_handshake() */
  if (d->ssl && (ssl_need_handshake(d->ssl_state))) {
    d->ssl_state = ssl_handshake(d->ssl);
    if (d->ssl_state < 0) {
      /* Fatal error */
      ssl_close_connection(d->ssl);
      d->ssl = NULL;
      d->ssl_state = 0;
      return 0;
    } else if (ssl_need_handshake(d->ssl_state)) {
      /* We're still not ready to send to this connection. Alas. */
      return 1;
    }
  }
  /* Insure that we're not in a state where we need an SSL_accept() */
  if (d->ssl && (ssl_need_accept(d->ssl_state))) {
    d->ssl_state = ssl_accept(d->ssl);
    if (d->ssl_state < 0) {
      /* Fatal error */
      ssl_close_connection(d->ssl);
      d->ssl = NULL;
      d->ssl_state = 0;
      return 0;
    } else if (ssl_need_accept(d->ssl_state)) {
      /* We're still not ready to send to this connection. Alas. */
      return 1;
    }
  }
  if (d->ssl) {
    /* process_output, alas, gets called from all kinds of places.
     * We need to know if the descriptor is waiting on input, though.
     * So let's find out
     */
    struct timeval pad;
    fd_set input_set;

    pad.tv_sec = 0;
    pad.tv_usec = 0;
    FD_ZERO(&input_set);
    FD_SET(d->descriptor, &input_set);
    input_ready = select(d->descriptor + 1, &input_set, NULL, NULL, &pad);
    if (input_ready < 0) {
      /* Well, shoot, we have no idea. Guess and proceed. */
      perror("select in process_output");
      input_ready = 0;
    }
  }
#endif

  for (qp = &d->output.head; ((cur = *qp) != NULL);) {
#ifdef HAS_OPENSSL
    if (d->ssl) {
      cnt = 0;
      d->ssl_state = ssl_write(d->ssl, d->ssl_state, input_ready, 1, cur->start,
			       cur->nchars, &cnt);
      if (ssl_want_write(d->ssl_state))
	return 1;		/* Need to retry */
    } else {
#endif
      cnt = send(d->descriptor, cur->start, cur->nchars, 0);
      if (cnt < 0) {
#ifdef WIN32
	if (cnt == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
#else
#ifdef EAGAIN
	if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
#else
	if (errno == EWOULDBLOCK)
#endif
#endif
	  return 1;
	return 0;
      }
#ifdef HAS_OPENSSL
    }
#endif
    d->output_size -= cnt;
    d->output_chars += cnt;
    if (cnt == cur->nchars) {
      if (!cur->nxt)
	d->output.tail = qp;
      *qp = cur->nxt;
#ifdef DEBUG
      do_rawlog(LT_ERR, "free_text_block(0x%x) at 2.", cur);
#endif				/* DEBUG */
      free_text_block(cur);
      continue;			/* do not adv ptr */
    }
    cur->nchars -= cnt;
    cur->start += cnt;
    break;
  }
  return 1;
}


static void
welcome_user(DESC *d)
{
  if (SUPPORT_PUEBLO && !(d->conn_flags & CONN_HTML))
    queue_newwrite(d, (unsigned char *) PUEBLO_HELLO, strlen(PUEBLO_HELLO));
  fcache_dump(d, fcache.connect_fcache, NULL);
}

static void
save_command(DESC *d, const unsigned char *command)
{
  add_to_queue(&d->input, command, u_strlen(command) + 1);
}

static void
test_telnet(DESC *d)
{
  /* Use rfc 1184 to test telnet support, as it tries to set linemode
     with client-side editing. Good for Broken Telnet Programs. */
  if (!TELNET_ABLE(d)) {
    /*  IAC DO LINEMODE */
    unsigned char query[3] = "\xFF\xFD\x22";
    queue_newwrite(d, query, 3);
    d->conn_flags |= CONN_TELNET_QUERY;
    process_output(d);
  }
}

static void
setup_telnet(DESC *d)
{
  /* Win2k telnet doesn't do local echo by default,
     apparently. Unfortunately, there doesn't seem to be a telnet
     option for local echo, just remote echo. */
  d->conn_flags |= CONN_TELNET;
  if (d->conn_flags & CONN_TELNET_QUERY) {
    /* IAC DO NAWS IAC DO TERMINAL-TYPE */
    unsigned char extra_options[6] = "\xFF\xFD\x1F" "\xFF\xFD\x18";
    d->conn_flags &= ~CONN_TELNET_QUERY;
    do_log(LT_CONN, 0, 0, T("[%d/%s/%s] Switching to Telnet mode."),
	   d->descriptor, d->addr, d->ip);
    queue_newwrite(d, extra_options, 6);
    process_output(d);
  }
}

static int
handle_telnet(DESC *d, unsigned char **q, unsigned char *qend)
{
  int i;

  /* *(*q - q) == IAC at this point. */
  switch (**q) {
  case SB:			/* Sub-option */
    if (*q >= qend)
      return -1;
    (*q)++;
    if (**q == TN_LINEMODE) {
      if ((*q + 2) >= qend)
	return -1;
      *q += 2;
      while (*q < qend && **q != SE)
	(*q)++;
      if (*q >= qend)
	return -1;
    } else if (**q == TN_NAWS) {
      /* Learn what size window the client is using. */
      union {
	short s;
	unsigned char bytes[2];
      } raw;
      if (*q >= qend)
	return -1;
      (*q)++;
      /* Width */
      if (**q == IAC) {
	raw.bytes[0] = IAC;
	if (*q >= qend)
	  return -1;
	(*q)++;
      } else
	raw.bytes[0] = **q;
      if (*q >= qend)
	return -1;
      (*q)++;
      if (**q == IAC) {
	raw.bytes[1] = IAC;
	if (*q >= qend)
	  return -1;
	(*q)++;
      } else
	raw.bytes[1] = **q;
      if (*q >= qend)
	return -1;
      (*q)++;

      d->width = ntohs(raw.s);

      /* Height */
      if (**q == IAC) {
	raw.bytes[0] = IAC;
	if (*q >= qend)
	  return -1;
	(*q)++;
      } else
	raw.bytes[0] = **q;
      if (*q >= qend)
	return -1;
      (*q)++;
      if (**q == IAC) {
	raw.bytes[1] = IAC;
	if (*q >= qend)
	  return -1;
	(*q)++;
      } else
	raw.bytes[1] = **q;
      if (*q >= qend)
	return -1;
      (*q)++;
      d->height = ntohs(raw.s);

      /* IAC SE */
      if (*q + 1 >= qend)
	return -1;
      (*q)++;
    } else if (**q == TN_TTYPE) {
      /* Read the terminal type: TERMINAL-TYPE IS blah IAC SE */
      char tbuf[BUFFER_LEN], *bp = tbuf;
      if (*q >= qend)
	return -1;
      (*q)++;
      /* Skip IS */
      if (*q >= qend)
	return -1;
      (*q)++;

      /* Read up to IAC SE */
      while (1) {
	if (*q >= qend)
	  return -1;
	if (**q == IAC) {
	  if (*q + 1 >= qend)
	    return -1;
	  if (*(*q + 1) == IAC) {
	    safe_chr((char) IAC, tbuf, &bp);
	    (*q)++;
	  } else
	    break;
	} else
	  safe_chr(**q, tbuf, &bp);
	(*q)++;
      }
      while (*q < qend && **q != SE)
	(*q)++;
      *bp = '\0';
      mush_free(d->ttype, "terminal description");
      d->ttype = mush_strdup(tbuf, "terminal description");
      /* We have the terminal type, now set any defaults if we find 'em */
      for(i = 0 ;  client_maps[i].terminal != NULL; i++)
	if(!strcmp(client_maps[i].terminal, d->ttype)) {
	  d->conn_flags |= client_maps[i].flags;
	  break;
	}
    } else {
      while (*q < qend && **q != SE)
	(*q)++;
    }
    break;
  case NOP:			/* No-op */
    if (*q >= qend)
      return -1;
#ifdef DEBUG_TELNET
    fprintf(stderr, "Got IAC NOP\n");
#endif
    break;
  case AYT:			/* Are you there? */
    if (*q >= qend)
      return -1;
    else {
      static unsigned char ayt_reply[] =
	"\r\n*** AYT received, I'm here ***\r\n";
      queue_newwrite(d, ayt_reply, u_strlen(ayt_reply));
      process_output(d);
    }
    break;
  case WILL:			/* Client is willing to do something, or confirming */
    setup_telnet(d);
    if (*q >= qend)
      return -1;
    (*q)++;

    if (**q == TN_LINEMODE) {
      /* Set up our preferred linemode options. */
      /* IAC SB LINEMODE MODE (EDIT|SOFT_TAB) IAC SE */
      unsigned char reply[7] = "\xFF\xFA\x22\x01\x09\xFF\xF0";
      queue_newwrite(d, reply, 7);
#ifdef DEBUG_TELNET
      fprintf(stderr, "Setting linemode options.\n");
#endif
    } else if (**q == TN_TTYPE) {
      /* Ask for terminal type id: IAC SB TERMINAL-TYPE SEND IAC SEC */
      unsigned char reply[6] = "\xFF\xFA\x18\x01\xFF\xF0";
      queue_newwrite(d, reply, 6);
    } else if (**q == TN_SGA || **q == TN_NAWS) {
      /* This is good to be at. */
    } else {			/* Refuse options we don't handle */
      unsigned char reply[3];
      reply[0] = IAC;
      reply[1] = DONT;
      reply[2] = **q;
      queue_newwrite(d, reply, sizeof reply);
      process_output(d);
    }
    break;
  case DO:			/* Client is asking us to do something */
    setup_telnet(d);
    if (*q >= qend)
      return -1;
    (*q)++;
    if (**q == TN_LINEMODE) {
    } else if (**q == TN_SGA) {
      /* IAC WILL SGA IAC DO SGA */
      unsigned char reply[6] = "\xFF\xFB\x03\xFF\xFD\x03";
      queue_newwrite(d, reply, 6);
      process_output(d);
#ifdef DEBUG_TELNET
      fprintf(stderr, "GOT IAC DO SGA, sending IAC WILL SGA IAG DO SGA\n");
#endif
    } else {
      /* Stuff we won't do */
      unsigned char reply[3];
      reply[0] = IAC;
      reply[1] = WONT;
      reply[2] = (char) **q;
      queue_newwrite(d, reply, sizeof reply);
      process_output(d);
    }
    break;
  case WONT:			/* Client won't do something we want. */
  case DONT:			/* Client doesn't want us to do something */
    setup_telnet(d);
#ifdef DEBUG_TELNET
    fprintf(stderr, "Got IAC %s 0x%x\n", **q == WONT ? "WONT" : "DONT",
	    *(*q + 1));
#endif
    if (*q + 1 >= qend)
      return -1;
    (*q)++;
    break;
  default:			/* Also catches IAC IAC for a literal 255 */
    return 0;
  }
  return 1;
}

static void
process_input_helper(DESC *d, char *tbuf1, int got)
{
  unsigned char *p, *pend, *q, *qend;

  if (!d->raw_input) {
    d->raw_input =
      (unsigned char *) mush_malloc(sizeof(char) * MAX_COMMAND_LEN,
				    "descriptor_raw_input");
    if (!d->raw_input)
      mush_panic("Out of memory");
    d->raw_input_at = d->raw_input;
  }
  p = d->raw_input_at;
  d->input_chars += got;
  pend = d->raw_input + MAX_COMMAND_LEN - 1;
  for (q = (unsigned char *) tbuf1, qend = (unsigned char *) tbuf1 + got;
       q < qend; q++) {
    if (*q == '\r') {
      /* A broken client (read: WinXP telnet) might send only CR, and not CRLF
       * so it's nice of us to try to handle this.
       */
      *p = '\0';
      if (p > d->raw_input)
	save_command(d, d->raw_input);
      p = d->raw_input;
      if (((q + 1) < qend) && (*(q + 1) == '\n'))
	q++;			/* For clients that work */
    } else if (*q == '\n') {
      *p = '\0';
      if (p > d->raw_input)
	save_command(d, d->raw_input);
      p = d->raw_input;
    } else if (*q == '\b') {
      if (p > d->raw_input)
	p--;
    } else if ((unsigned char) *q == IAC) {	/* Telnet option foo */
      if (q >= qend)
	break;
      q++;
      if (!TELNET_ABLE(d) || handle_telnet(d, &q, qend) == 0) {
	if (p < pend && isprint(*q))
	  *p++ = *q;
      }
    } else if (p < pend && isprint(*q)) {
      *p++ = *q;
    }
  }
  if (p > d->raw_input) {
    d->raw_input_at = p;
  } else {
    mush_free((Malloc_t) d->raw_input, "descriptor_raw_input");
    d->raw_input = 0;
    d->raw_input_at = 0;
  }
}

/* ARGSUSED */
static int
process_input(DESC *d, int output_ready __attribute__ ((__unused__)))
{
  int got = 0;
  char tbuf1[BUFFER_LEN];

  errno = 0;

#ifdef HAS_OPENSSL
  if (d->ssl) {
    /* Insure that we're not in a state where we need an SSL_handshake() */
    if (ssl_need_handshake(d->ssl_state)) {
      d->ssl_state = ssl_handshake(d->ssl);
      if (d->ssl_state < 0) {
	/* Fatal error */
	ssl_close_connection(d->ssl);
	d->ssl = NULL;
	d->ssl_state = 0;
	return 0;
      } else if (ssl_need_handshake(d->ssl_state)) {
	/* We're still not ready to send to this connection. Alas. */
	return 1;
      }
    }
    /* Insure that we're not in a state where we need an SSL_accept() */
    if (ssl_need_accept(d->ssl_state)) {
      d->ssl_state = ssl_accept(d->ssl);
      if (d->ssl_state < 0) {
	/* Fatal error */
	ssl_close_connection(d->ssl);
	d->ssl = NULL;
	d->ssl_state = 0;
	return 0;
      } else if (ssl_need_accept(d->ssl_state)) {
	/* We're still not ready to send to this connection. Alas. */
	return 1;
      }
    }
    /* It's an SSL connection, proceed accordingly */
    d->ssl_state =
      ssl_read(d->ssl, d->ssl_state, 1, output_ready, tbuf1, sizeof tbuf1,
	       &got);
    if (d->ssl_state < 0) {
      /* Fatal error */
      ssl_close_connection(d->ssl);
      d->ssl = NULL;
      d->ssl_state = 0;
      return 0;
    }
  } else {
#endif
    got = recv(d->descriptor, tbuf1, sizeof tbuf1, 0);
    if (got <= 0) {
      /* At this point, select() says there's data waiting to be read from
       * the socket, but we shouldn't assume that read() will actually get it
       * and blindly act like a got of -1 is a disconnect-worthy error.
       */
#ifdef EAGAIN
      if ((errno == EWOULDBLOCK) || (errno == EAGAIN) || (errno == EINTR))
#else
      if ((errno == EWOULDBLOCK) || (errno == EINTR))
#endif
	return 1;
      else
	return 0;
    }
#ifdef HAS_OPENSSL
  }
#endif

  process_input_helper(d, tbuf1, got);

  return 1;
}

static void
set_userstring(unsigned char **userstring, const char *command)
{
  if (*userstring) {
    mush_free((Malloc_t) *userstring, "userstring");
    *userstring = NULL;
  }
  while (*command && isspace((unsigned char) *command))
    command++;
  if (*command)
    *userstring = (unsigned char *) mush_strdup(command, "userstring");
}

static void
process_commands(void)
{
  int nprocessed;
  DESC *cdesc, *dnext;
  struct text_block *t;
  int retval = 1;

  do {
    nprocessed = 0;
    for (cdesc = descriptor_list; cdesc;
	 cdesc = (nprocessed > 0 && retval > 0) ? cdesc->next : dnext) {
      dnext = cdesc->next;
      if (cdesc->quota > 0 && (t = cdesc->input.head)) {
	cdesc->quota--;
	nprocessed++;
	start_cpu_timer();
	feed_snoop(cdesc,(const char *) t->start, 0);
	/* check AUNIDLE */
	if(options.idle_time && ((mudtime - cdesc->last_time) > options.idle_time)) {
	  if(atr_get(cdesc->player, "AUNIDLE"))
	    queue_attribute_noparent(cdesc->player, "AUNIDLE", cdesc->player);
	  if(GoodObject(Division(cdesc->player)) && atr_get(Division(cdesc->player), "AUNIDLE"))
	    queue_attribute(Division(cdesc->player), "AUNIDLE", cdesc->player);
	}
	retval = cdesc->input_handler(cdesc, (char *) t->start);
	reset_cpu_timer();
	if(retval == -1 && do_su_exit(cdesc)) 
	  retval = 1;

	if (retval == 0) {
	  shutdownsock(cdesc);
	} else if (retval == -1) {
	  logout_sock(cdesc);
	} else {
	  cdesc->input.head = t->nxt;
	  if (!cdesc->input.head)
	    cdesc->input.tail = &cdesc->input.head;
	  if (t) {
#ifdef DEBUG
	    do_rawlog(LT_ERR, "free_text_block(0x%x) at 5.", t);
#endif				/* DEBUG */
	    free_text_block(t);
	  }
	}
      }
    }
  } while (nprocessed > 0);
}

/** Send a descriptor's output prefix */
#define send_prefix(d) \
  if (d->output_prefix) { \
    queue_newwrite(d, d->output_prefix, u_strlen(d->output_prefix)); \
    queue_eol(d); \
  }

/** Send a descriptor's output suffix */
#define send_suffix(d) \
  if (d->output_suffix) { \
    queue_newwrite(d, d->output_suffix, u_strlen(d->output_suffix)); \
    queue_eol(d); \
  }

int
do_command(DESC *d, char *command)
{
  int j;

  depth = 0;

  (d->cmds)++;

  if (!strcmp(command, IDLE_COMMAND))
    return 1;
  if(difftime(mudtime, d->last_time) >= 300) { 
    d->idle_total += difftime(mudtime, d->last_time);
    d->unidle_times++;
  }
  d->last_time = mudtime;
  if (!strcmp(command, QUIT_COMMAND)) {
    return 0;
  } else if (!strcmp(command, LOGOUT_COMMAND)) {
    return -1;
  } else if (!strcmp(command, INFO_COMMAND)) {
    send_prefix(d);
    dump_info(d);
    send_suffix(d);
  } else if (!strncmp(command, WHO_COMMAND, strlen(WHO_COMMAND))) {
    send_prefix(d);
    dump_users(d, command + strlen(WHO_COMMAND), 0);
    send_suffix(d);
  } else if (!strncmp(command, DOING_COMMAND, strlen(DOING_COMMAND))) {
    send_prefix(d);
    dump_users(d, command + strlen(DOING_COMMAND), 1);
    send_suffix(d);
  } else if (!strncmp(command, SESSION_COMMAND, strlen(SESSION_COMMAND))) {
    send_prefix(d);
    dump_users(d, command + strlen(SESSION_COMMAND), 2);
    send_suffix(d);
  } else if (!strncmp(command, PREFIX_COMMAND, strlen(PREFIX_COMMAND))) {
    set_userstring(&d->output_prefix, command + strlen(PREFIX_COMMAND));
  } else if (!strncmp(command, SUFFIX_COMMAND, strlen(SUFFIX_COMMAND))) {
    set_userstring(&d->output_suffix, command + strlen(SUFFIX_COMMAND));
  } else if (!strncmp(command, "SCREENWIDTH", 11)) {
    d->width = parse_integer(command + 11);
  } else if (!strncmp(command, "SCREENHEIGHT", 12)) {
    d->height = parse_integer(command + 12);
  } else if (SUPPORT_PUEBLO
	     && !strncmp(command, PUEBLO_COMMAND, strlen(PUEBLO_COMMAND))) {
    parse_puebloclient(d, command);
    if (!(d->conn_flags & CONN_HTML)) {
      queue_newwrite(d, (unsigned char *) PUEBLO_SEND, strlen(PUEBLO_SEND));
      process_output(d);
      do_log(LT_CONN, 0, 0, T("[%d/%s/%s] Switching to Pueblo mode."),
	     d->descriptor, d->addr, d->ip);
      d->conn_flags |= CONN_HTML;
      if (!d->connected)
	welcome_user(d);
    }
  } else {
    if (d->connected) {
      send_prefix(d);
      global_eval_context.cplr = d->player;
      strcpy(global_eval_context.ccom, command);
      strcpy(global_eval_context.ucom, "");

      /* Clear %0-%9 and r(0) - r(9) */
      for (j = 0; j < 10; j++)
	global_eval_context.wenv[j] = (char *) NULL;
      for (j = 0; j < NUMQ; j++)
	global_eval_context.renv[j][0] = '\0';
      global_eval_context.process_command_port = d->descriptor;

      process_command(d->player, command, d->player, d->player, 1);
      send_suffix(d);
      strcpy(global_eval_context.ccom, "");
      strcpy(global_eval_context.ucom, "");
      global_eval_context.cplr = NOTHING;
    } else {
      if (!check_connect(d, command))
	return 0;
    }
  }
  return 1;
}

static void
parse_puebloclient(DESC *d, char *command)
{
  const char *p, *end;
  if ((p = string_match(command, "md5="))) {
    /* Skip md5=" */
    p += 5;
    if ((end = strchr(p, '"'))) {
      if ((end > p) && ((end - p) <= PUEBLO_CHECKSUM_LEN)) {
	/* Got it! */
	strncpy(d->checksum, p, end - p);
	d->checksum[end - p] = '\0';
      }
    }
  }
}

static int
dump_messages(DESC *d, dbref player, int isnew)
{
  int is_hidden;
  int num = 0;
  DESC *tmpd;

  d->connected = 1;
  d->connected_at = mudtime;
  d->player = player;
  d->doing[0] = '\0';

  if (MAX_LOGINS) {
    /* check for exceeding max player limit */
    login_number++;
    if (under_limit && (login_number > MAX_LOGINS)) {
      under_limit = 0;
      do_rawlog(LT_CONN,
		T("Limit of %d players reached. Logins disabled.\n"),
		MAX_LOGINS);
    }
  }
  /* give players a message on connection */
  if (!options.login_allow || !under_limit ||
      (Guest(player) && !options.guest_allow)) {
    if (!options.login_allow) {
      fcache_dump(d, fcache.down_fcache, NULL);
      if (cf_downmotd_msg && *cf_downmotd_msg)
	raw_notify(player, cf_downmotd_msg);
    } else if (MAX_LOGINS && !under_limit) {
      fcache_dump(d, fcache.full_fcache, NULL);
      if (cf_fullmotd_msg && *cf_fullmotd_msg)
	raw_notify(player, cf_fullmotd_msg);
    }
    if (!Can_Login(player)) {
      /* when the connection has been refused, we want to update the
       * LASTFAILED info on the player
       */
      check_lastfailed(player, d->addr);
      return 0;
    }
  }
  d->mailp = find_exact_starting_point(player);

  /* check to see if this is a reconnect and also set DARK status */
  is_hidden = Can_Hide(player) && Dark(player);
  DESC_ITER_CONN(tmpd) {
    if (tmpd->player == player) {
      num++;
      if (is_hidden)
	tmpd->hide = 1;
    }
  }
  /* give permanent text messages */
  if (isnew)
    fcache_dump(d, fcache.newuser_fcache, NULL);
  if (num == 1)
    fcache_dump(d, fcache.motd_fcache, NULL);
  if (Guest(player))
    fcache_dump(d, fcache.guest_fcache, NULL);

  if (ModTime(player))
    notify_format(player, T("%ld failed connections since last login."),
		  ModTime(player));
  ModTime(player) = (time_t) 0;
  announce_connect(player, isnew, num);	/* broadcast connect message */
  check_last(player, d->addr, d->ip);	/* set Last, Lastsite, give paycheck */
  /* Check folder 0, not silently (i.e. Report lack of mail, too) */
  queue_eol(d);
  if (command_check_byname(player, "@MAIL"))
    check_mail(player, 0, 0);
  set_player_folder(player, 0);
  do_look_around(player);
  if (Haven(player))
    notify(player, T("Your HAVEN flag is set. You cannot receive pages."));
  local_connect(player, isnew, num);
  return 1;
}

static int
check_connect(DESC *d, const char *msg)
{
  char command[MAX_COMMAND_LEN];
  char user[MAX_COMMAND_LEN];
  char password[MAX_COMMAND_LEN];
  char errbuf[BUFFER_LEN];
  dbref player;

  parse_connect(msg, command, user, password);

  if (string_prefix("connect", command)) {
    if ((player =
	 connect_player(user, password, d->addr, d->ip, errbuf)) == NOTHING) {
      queue_string_eol(d, errbuf);
      do_log(LT_CONN, 0, 0, T("[%d/%s/%s] Failed connect to '%s'."),
	     d->descriptor, d->addr, d->ip, user);
    } else {
      do_log(LT_CONN, 0, 0, T("[%d/%s/%s] Connected to %s(#%d) in %s(#%d)"),
	     d->descriptor, d->addr, d->ip, Name(player), player,
	     Name(Location(player)), Location(player));
      /* Check if we're fake siting this guy.. */
      if(has_flag_by_name(player, "WEIRDSITE", TYPE_PLAYER)) {
	      ATTR *a;
	      a = atr_get(player, "LASTSITE");
	      strncpy(d->addr, !a ? "localhost" : atr_value(a), 100);
      }

      if ((dump_messages(d, player, 0)) == 0) {
	d->connected = 2;
	return 0;
      }
    }

  } else if (!strcasecmp(command, "cd")) {
    if ((player =
	 connect_player(user, password, d->addr, d->ip, errbuf)) == NOTHING) {
      queue_string_eol(d, errbuf);
      do_log(LT_CONN, 0, 0, T("[%d/%s/%s] Failed connect to '%s'."),
	     d->descriptor, d->addr, d->ip, user);
    } else {
      do_log(LT_CONN, 0, 0,
	     T("[%d/%s/%s] Connected dark to %s(#%d) in %s(#%d)"),
	     d->descriptor, d->addr, d->ip, Name(player), player,
	     Name(Location(player)), Location(player));
      /* Set player dark */
      d->connected = 1;
      d->player = player;
      set_flag(player, player, "DARK", 0, 0, 0);
      if ((dump_messages(d, player, 0)) == 0) {
	d->connected = 2;
	return 0;
      }
    }

  } else if (!strcasecmp(command, "cv")) {
    if ((player =
	 connect_player(user, password, d->addr, d->ip, errbuf)) == NOTHING) {
      queue_string_eol(d, errbuf);
      do_log(LT_CONN, 0, 0, T("[%d/%s/%s] Failed connect to '%s'."),
	     d->descriptor, d->addr, d->ip, user);
    } else {
      do_log(LT_CONN, 0, 0, T("[%d/%s/%s] Connected to %s(#%d) in %s(#%d)"),
	     d->descriptor, d->addr, d->ip, Name(player), player,
	     Name(Location(player)), Location(player));
      /* Set player !dark */
      d->connected = 1;
      d->player = player;
      set_flag(player, player, "DARK", 1, 0, 0);
      if ((dump_messages(d, player, 0)) == 0) {
	d->connected = 2;
	return 0;
      }
    }

  } else if (!strcasecmp(command, "ch")) {
    if ((player =
	 connect_player(user, password, d->addr, d->ip, errbuf)) == NOTHING) {
      queue_string_eol(d, errbuf);
      do_log(LT_CONN, 0, 0, T("[%d/%s/%s] Failed connect to '%s'."),
	     d->descriptor, d->addr, d->ip, user);
    } else {
      do_log(LT_CONN, 0, 0,
	     T("[%d/%s/%s] Connected hidden to %s(#%d) in %s(#%d)"),
	     d->descriptor, d->addr, d->ip, Name(player), player,
	     Name(Location(player)), Location(player));
      /* Set player hidden */
      d->connected = 1;
      d->player = player;
      if (Can_Hide(player))
	d->hide = 1;
      if ((dump_messages(d, player, 0)) == 0) {
	d->connected = 2;
	return 0;
      }
    }

  } else if (string_prefix("create", command)) {
    if (!Site_Can_Create(d->addr) || !Site_Can_Create(d->ip)) {
      fcache_dump(d, fcache.register_fcache, NULL);
      if (!Deny_Silent_Site(d->addr, AMBIGUOUS)
	  && !Deny_Silent_Site(d->ip, AMBIGUOUS)) {
	do_log(LT_CONN, 0, 0, T("[%d/%s/%s] Refused create for '%s'."),
	       d->descriptor, d->addr, d->ip, user);
      }
      return 0;
    }
    if (!options.login_allow || !options.create_allow) {
      if (!options.login_allow)
	fcache_dump(d, fcache.down_fcache, NULL);
      else
	fcache_dump(d, fcache.register_fcache, NULL);
      do_rawlog(LT_CONN,
		"REFUSED CREATION for %s from %s on descriptor %d.\n",
		user, d->addr, d->descriptor);
      return 0;
    } else if (MAX_LOGINS && !under_limit) {
      fcache_dump(d, fcache.full_fcache, NULL);
      do_rawlog(LT_CONN,
		"REFUSED CREATION for %s from %s on descriptor %d.\n",
		user, d->addr, d->descriptor);
      return 0;
    }
    player = create_player(user, password, d->addr, d->ip);
    if (player == NOTHING) {
      queue_string_eol(d, T(create_fail));
      do_log(LT_CONN, 0, 0,
	     T("[%d/%s/%s] Failed create for '%s' (bad name)."),
	     d->descriptor, d->addr, d->ip, user);
    } else if (player == AMBIGUOUS) {
      queue_string_eol(d, T(password_fail));
      do_log(LT_CONN, 0, 0,
	     T("[%d/%s/%s] Failed create for '%s' (bad password)."),
	     d->descriptor, d->addr, d->ip, user);
    } else {
      do_log(LT_CONN, 0, 0, "[%d/%s/%s] Created %s(#%d)",
	     d->descriptor, d->addr, d->ip, Name(player), player);
      if ((dump_messages(d, player, 1)) == 0) {
	d->connected = 2;
	return 0;
      }
    }				/* successful player creation */

  } else if (string_prefix("register", command)) {
    if (!Site_Can_Register(d->addr) || !Site_Can_Register(d->ip)) {
      fcache_dump(d, fcache.register_fcache, NULL);
      if (!Deny_Silent_Site(d->addr, AMBIGUOUS)
	  && !Deny_Silent_Site(d->ip, AMBIGUOUS)) {
	do_log(LT_CONN, 0, 0,
	       T("[%d/%s/%s] Refused registration (bad site) for '%s'."),
	       d->descriptor, d->addr, d->ip, user);
      }
      return 0;
    }
    if (!options.create_allow) {
      fcache_dump(d, fcache.register_fcache, NULL);
      do_rawlog(LT_CONN,
		"Refused registration (creation disabled) for %s from %s on descriptor %d.\n",
		user, d->addr, d->descriptor);
      return 0;
    }
    if ((player = email_register_player(user, password, d->addr, d->ip)) ==
	NOTHING) {
      queue_string_eol(d, T(register_fail));
      do_log(LT_CONN, 0, 0, T("[%d/%s/%s] Failed registration for '%s'."),
	     d->descriptor, d->addr, d->ip, user);
    } else {
      queue_string_eol(d, T(register_success));
      do_log(LT_CONN, 0, 0, "[%d/%s/%s] Registered %s(#%d) to %s",
	     d->descriptor, d->addr, d->ip, Name(player), player, password);
    }
    /* Whether it succeeds or fails, leave them connected */

  } else {
    /* invalid command, just repeat login screen */
    welcome_user(d);
  }
  /* If they were in a program, get them back in it */
  if(InProg(d->player)) {
    queue_string(d, "Loading @Program onto Descriptor....\r\n");
    prog_load_desc(d);
  }

  return 1;
}

static void
parse_connect(const char *msg1, char *command, char *user, char *pass)
{
  unsigned char *p;
  unsigned const char *msg = (unsigned const char *) msg1;

  while (*msg && isspace(*msg))
    msg++;
  p = (unsigned char *) command;
  while (*msg && isprint(*msg) && !isspace(*msg))
    *p++ = *msg++;
  *p = '\0';
  while (*msg && isspace(*msg))
    msg++;
  p = (unsigned char *) user;

  if (PLAYER_NAME_SPACES && *msg == '\"') {
    for (; *msg && ((*msg == '\"') || isspace(*msg)); msg++) ;
    while (*msg && (*msg != '\"')) {
      while (*msg && !isspace(*msg) && (*msg != '\"'))
	*p++ = *msg++;
      if (*msg == '\"') {
	msg++;
	while (*msg && isspace(*msg))
	  msg++;
	break;
      }
      while (*msg && isspace(*msg))
	msg++;
      if (*msg && (*msg != '\"'))
	*p++ = ' ';
    }
  } else
    while (*msg && isprint(*msg) && !isspace(*msg))
      *p++ = *msg++;

  *p = '\0';
  while (*msg && isspace(*msg))
    msg++;
  p = (unsigned char *) pass;
  while (*msg && isprint(*msg) && !isspace(*msg))
    *p++ = *msg++;
  *p = '\0';
}

static void
close_sockets(void)
{
  DESC *d, *dnext;

  for (d = descriptor_list; d; d = dnext) {
    dnext = d->next;
    send(d->descriptor, T(shutdown_message), strlen(T(shutdown_message)), 0);
    send(d->descriptor, "\r\n", 2, 0);
#ifdef HAS_OPENSSL
    if (d->ssl) {
      ssl_close_connection(d->ssl);
      d->ssl = NULL;
      d->ssl_state = 0;
    }
#endif
    if (shutdown(d->descriptor, 2) < 0)
      perror("shutdown");
    closesocket(d->descriptor);
  }
}

/** Give everyone the boot.
 */
void
emergency_shutdown(void)
{
  close_sockets();
}

/** Disconnect a descriptor.
 * \param d pointer to descriptor to disconnect.
 */
void
boot_desc(DESC *d)
{
  shutdownsock(d);
}

/** Given a player dbref, return the player's first connected descriptor.
 * \param player dbref of player.
 * \return pointer to player's first connected descriptor, or NULL.
 */
DESC *
player_desc(dbref player)
{
  DESC *d;
  for (d = descriptor_list; d; d = d->next) {
    if (d->connected && (d->player == player)) {
      return d;
    }
  }
  return (DESC *) NULL;
}

/** Page a specified socket.
 * \param player the enactor.
 * \param pc string containing port number to send message to.
 * \param message message to send.
 */
void
do_page_port(dbref player, const char *pc, const char *message)
{
  int p, key;
  DESC *d;
  const char *gap;
  char tbuf[BUFFER_LEN], *tbp = tbuf;
  dbref target = NOTHING;

  if (!Admin(player)) {
    notify(player, T("Permission denied."));
    return;
  }
  p = atoi(pc);
  if (p <= 0) {
    notify(player, T("That's not a port number."));
    return;
  }

  if (!message || !*message) {
    notify(player, T("What do you want to page with?"));
    return;
  }

  gap = " ";
  switch (*message) {
  case SEMI_POSE_TOKEN:
    gap = "";
  case POSE_TOKEN:
    key = 1;
    break;
  default:
    key = 3;
    break;
  }

  d = port_desc(p);
  if (!d) {
    notify(player, T("That port's not active."));
    return;
  }
  if (d->connected)
    target = d->player;
  switch (key) {
  case 1:
    safe_format(tbuf, &tbp, T("From afar, %s%s%s"), Name(player), gap,
		message + 1);
    notify_format(player, T("Long distance to %s: %s%s%s"),
		  target != NOTHING ? Name(target) :
		  T("a connecting player"), Name(player), gap, message + 1);
    break;
  case 3:
    safe_format(tbuf, &tbp, T("%s pages: %s"), Name(player), message);
    notify_format(player, T("You paged %s with '%s'."),
		  target != NOTHING ? Name(target) :
		  T("a connecting player"), message);
    break;
  }
  *tbp = '\0';
  if (target != NOTHING)
    page_return(player, target, "Idle", "IDLE", NULL);
  if (Typeof(player) != TYPE_PLAYER && Nospoof(target))
    queue_string_eol(d, tprintf("[#%d] %s", player, tbuf));
  else
    queue_string_eol(d, tbuf);
}


/** Return an inactive descriptor, as long as there's more than
 * one descriptor connected. Used for boot/me.
 * \param player player to find an inactive descriptor for.
 * \return pointer to player's inactive descriptor, or NULL.
 */
DESC *
inactive_desc(dbref player)
{
  DESC *d, *in = NULL;
  time_t now;
  int numd = 0;
  now = mudtime;
  DESC_ITER_CONN(d) {
    if (d->player == player) {
      numd++;
      if (now - d->last_time > 60)
	in = d;
    }
  }
  if (numd > 1)
    return in;
  else
    return (DESC *) NULL;
}

/** Given a port (a socket number), return the descriptor.
 * \param port port (socket file descriptor number).
 * \return pointer to descriptor associated with the port.
 */
DESC *
port_desc(int port)
{
  DESC *d;
  for (d = descriptor_list; (d); d = d->next) {
    if (d->descriptor == port) {
      return d;
    }
  }
  return (DESC *) NULL;
}

/** Given a port, find the matching player dbref.
 * \param port (socket file descriptor number).
 * \return dbref of connected player using that port, or NOTHING.
 */
dbref
find_player_by_desc(int port)
{
  DESC *d;
  for (d = descriptor_list; (d); d = d->next) {
    if (d->connected && (d->descriptor == port)) {
      return d->player;
    }
  }

  /* didn't find anything */
  return NOTHING;
}


#ifndef WIN32
/** Handler for SIGINT. Note that we've received it, and reinstall.
 * \param sig signal caught.
 */
void
signal_shutdown(int sig __attribute__ ((__unused__)))
{
  signal_shutdown_flag = 1;
  reload_sig_handler(SIGINT, signal_shutdown);
}

/** Handler for SIGUSR2. Note that we've received it, and reinstall
 * \param sig signal caught.
 */
void
signal_dump(int sig __attribute__ ((__unused__)))
{
  signal_dump_flag = 1;
  reload_sig_handler(SIGUSR2, signal_dump);
}
#endif

/** A general handler to puke and die.
 * \param sig signal caught.
 */
void
bailout(int sig)
{
  mush_panicf(T("BAILOUT: caught signal %d"), sig);
}

#ifndef WIN32
/** Reap child processes, notably info_slaves and forking dumps,
 * when we receive a SIGCHLD signal. Don't fear this function. :)
 * \param sig signal caught.
 */
void
reaper(int sig __attribute__ ((__unused__)))
{
  WAIT_TYPE my_stat;
  Pid_t pid;

#ifdef HAS_WAITPID
  while ((pid = waitpid(-1, &my_stat, WNOHANG)) > 0)
#else
  while ((pid = wait3(&my_stat, WNOHANG, 0)) > 0)
#endif
  {
#ifdef INFO_SLAVE
    if (info_slave_pid > -1 && pid == info_slave_pid) {
      do_rawlog(LT_ERR, T("info_slave on pid %d exited!"), pid);
      info_slave_state = 0;
      info_slave_pid = -1;
    } else
#endif
    if (forked_dump_pid > -1 && pid == forked_dump_pid) {
      /* Most failures are handled by the forked mush already */
      if (WIFSIGNALED(my_stat)) {
	do_rawlog(LT_ERR, T("ERROR! forking dump exited with signal %d"),
		  WTERMSIG(my_stat));
      } else if (WIFEXITED(my_stat) && WEXITSTATUS(my_stat) == 0) {
	time(&globals.last_dump_time);
	if (DUMP_NOFORK_COMPLETE && *DUMP_NOFORK_COMPLETE)
	  flag_broadcast(0, 0, "%s", DUMP_NOFORK_COMPLETE);

      }
      forked_dump_pid = -1;
    }
  }
  reload_sig_handler(SIGCHLD, reaper);
}
#endif	/* !WIN32 */


static void
dump_info(DESC *call_by)
{
  int count = 0;
  DESC *d;
  queue_string_eol(call_by, tprintf("### Begin INFO %s", INFO_VERSION));

  /* Count connected players */
  for (d = descriptor_list; d; d = d->next) {
    if (d->connected) {
      if (!GoodObject(d->player))
	continue;
      if (COUNT_ALL || !Hidden(d) || call_by->player == d->player )
	count++;
    }
  }
  queue_string_eol(call_by, tprintf("Name: %s", options.mud_name));
  queue_string_eol(call_by,
		   tprintf("Uptime: %s", show_time(globals.first_start_time, 0)));
  queue_string_eol(call_by, tprintf("Connected: %d", count));
  queue_string_eol(call_by, tprintf("Size: %d", db_top));
  queue_string_eol(call_by, tprintf("Version: CobraMUSH v%s [%s]", VERSION,
			VBRANCH));
#ifdef PATCHES
  queue_string_eol(call_by, tprintf("Patches: %s", PATCHES));
#endif
  queue_string_eol(call_by, "### End INFO");
}

static void
dump_users(DESC *call_by, char *match, int doing)
    /* doing: 0 if normal WHO, 1 if DOING, 2 if SESSION */
{
  DESC *d;
#ifdef COLOREDWHO
  int tcount = 0; 
#endif
  int count = 0;
  time_t now;
  char tbuf1[BUFFER_LEN];
  char tbuf2[BUFFER_LEN];
  int csite;

  if (!GoodObject(call_by->player)) {
    do_log(LT_ERR, 0, 0, T("Bogus caller #%d of dump_users"), call_by->player);
    return;
  }
  while (*match && *match == ' ')
    match++;
  now = mudtime;

  /* If an admin types "DOING" it gives him the normal player WHO,
   * BUT flags are not shown. Privileged WHO does not show @doings.
   */

  if (SUPPORT_PUEBLO && (call_by->conn_flags & CONN_HTML)) {
    queue_newwrite(call_by, (unsigned char *) "<img xch_mode=html>", 19);
    queue_newwrite(call_by, (unsigned char *) "<PRE>", 5);
  }

  if ((doing == 1) || !call_by->player || !Priv_Who(call_by->player)) {
    if (poll_msg[0] == '\0')
      strcpy(poll_msg, "Doing");
    if (ShowAnsi(call_by->player))
      sprintf(tbuf2, "%s%-16s %4s %10s %6s  %s%s\n", ANSI_HILITE,
	      T("Player Name"), T("Aff"), T("On For"), T("Idle"), poll_msg, ANSI_NORMAL);
    else
      sprintf(tbuf2, "%-16s %4s %10s %6s  %s\n",
	      T("Player Name"), T("Aff"), T("On For"), T("Idle"), poll_msg);
    queue_string(call_by, tbuf2);
  } else if (doing == 2) {
    sprintf(tbuf2, "%s%-16s %6s %9s %5s %5s Des  Sent    Recv  Pend%s\n", ShowAnsi(call_by->player) ? ANSI_HILITE :
		    "", T("Player Name"), T("Loc #"), T("On For"), T("Idle"), T("Cmds"),
		    ShowAnsi(call_by->player) ? ANSI_NORMAL : "");
    queue_string(call_by, tbuf2);
  } else {
    sprintf(tbuf2, "%s%-16s %6s %9s %5s %5s Des  Host%s\n", ShowAnsi(call_by->player) ? ANSI_HILITE : "",
	    T("Player Name"), T("Loc #"), T("On For"), T("Idle"), T("Cmds"), ShowAnsi(call_by->player) ? ANSI_NORMAL : "");
    queue_string(call_by, tbuf2);
  }

  for (d = descriptor_list; d; d = d->next) {
    if (d->connected) {
      if (!GoodObject(d->player))
	continue;
      if (COUNT_ALL || (!Hidden(d) || call_by->player == d->player 
			|| (call_by->player && Priv_Who(call_by->player)))) {
	 count++;
#ifdef COLOREDWHO
	 tcount++;
#endif
      }
      if (match && !(string_prefix(Name(d->player), match)))
	continue;
      csite = CanSee(call_by->player, d->player);

      if (call_by->connected && doing == 0 && call_by->player
	  && Priv_Who(call_by->player)) {
        if (Hidden(d) && !csite)
          continue;

	sprintf(tbuf1, "%-16s %6s %9s %5s  %4d %3d%c  %s", tprintf("%s%s", Name(d->player), InProg(d->player) ? "(P)" : ""),
		Can_Locate(call_by->player, d->player) ? unparse_dbref(Location(d->player)) : "#-1",
		time_format_1(now - d->connected_at),
		time_format_2(now - d->last_time), csite ? d->cmds : 0,
		csite ? d->descriptor : 0,
#ifdef HAS_OPENSSL
		d->ssl ? 'S' : ' ',
#else
		' ',
#endif
		csite ? d->addr : "---");
	tbuf1[78] = '\0';
	if (Dark(d->player)) {
	  tbuf1[71] = '\0';
	  strcat(tbuf1, " (Dark)");
	} else if (Hidden(d)) {
	  tbuf1[71] = '\0';
	  strcat(tbuf1, " (Hide)");
	}
      } else if (call_by->connected && doing == 2 && call_by->player
		 && Priv_Who(call_by->player)) {
	sprintf(tbuf1, "%-16s %6s %9s %5s %5d %3d%c %5lu %7lu %5d",
		tprintf("%s%s", Name(d->player), InProg(d->player) ? "(P)" : ""),
		Can_Locate(call_by->player, d->player) ? unparse_dbref(Location(d->player)) : "#-1",
		time_format_1(now - d->connected_at),
		time_format_2(now - d->last_time), csite ? d->cmds : 0,
		csite ? d->descriptor : 0,
#ifdef HAS_OPENSSL
		d->ssl ? 'S' : ' ',
#else
		' ',
#endif

		csite ? d->input_chars : 0, csite ? d->output_chars : 0,
		csite ? d->output_size : 0);
      } else {
	if (!Hidden(d)
	    || call_by->player == d->player ||
	    (call_by->player && Priv_Who(call_by->player) && (doing))) {
	  sprintf(tbuf1, "%-16s %4s %10s   %4s%c %s", tprintf("%s%s", Name(d->player), InProg(d->player) ? "(P)" : ""), empabb(d->player),
		  time_format_1(now - d->connected_at),
		  time_format_2(now - d->last_time),
		  (Dark(d->player) ? 'D' : (Hidden(d) ? 'H' : ' '))
		  , d->doing);
	}
      }

      if (!Hidden(d) || (call_by->player && Priv_Who(call_by->player))) {
#ifdef COLOREDWHO
	if(ShowAnsiColor(call_by->player))
		queue_string(call_by, tprintf("%s%s%s%s%s", ANSI_NORMAL, (tcount % 2 ?  "" : ANSI_HILITE), 
					(tcount % 2 ? ANSI_CYAN : ANSI_WHITE),
					tbuf1, ANSI_NORMAL));

	else 
#endif
		queue_string(call_by, tbuf1);
	queue_newwrite(call_by, (unsigned char *) "\r\n", 2);
      }
    } else if (call_by->player && Priv_Who(call_by->player) && doing != 1
	       && (!match || !*match)) {
#ifdef COLOREDWHO
      tcount++;
#endif
      if (doing == 0) {
	/* Privileged WHO for non-logged in connections */
	sprintf(tbuf1, "%-16s %6s %9s %5s  %4d %3d%c  %s", T("Connecting..."),
		"#-1",
		time_format_1(now - d->connected_at),
		time_format_2(now - d->last_time), d->cmds, d->descriptor,
		#ifdef HAS_OPENSSL
		              d->ssl ? 'S' : ' ',
		#else
		              ' ',
		#endif

		d->addr);
	tbuf1[78] = '\0';
      } else {
	/* SESSION for non-logged in connections */
	sprintf(tbuf1, "%-16s %5s %9s %5s %5d %3d%c %5lu %7lu %5d",
		T("Connecting..."), "#-1",
		time_format_1(now - d->connected_at),
		time_format_2(now - d->last_time), d->cmds, d->descriptor,
		#ifdef HAS_OPENSSL
		              d->ssl ? 'S' : ' ',
		#else
		              ' ',
		#endif
		d->input_chars, d->output_chars, d->output_size);
      }
#ifdef COLOREDWHO
      if(ShowAnsiColor(call_by->player))
	      queue_string(call_by, tprintf("%s%s%s%s%s", ANSI_NORMAL,tcount % 2 ?  "" : ANSI_HILITE,
				      tcount % 2 ? ANSI_CYAN : ANSI_WHITE, tbuf1, ANSI_NORMAL));
      else 
#endif
	      queue_string(call_by, tbuf1);
      queue_newwrite(call_by, (unsigned char *) "\r\n", 2);
    }
  }
  switch (count) {
  case 0:
    strcpy(tbuf1, T("There are no players connected."));
    break;
  case 1:
    strcpy(tbuf1, T("There is 1 player connected."));
    break;
  default:
    sprintf(tbuf1, T("There are %d players connected."), count);
    break;
  }

#ifdef COLOREDWHO
  if(ShowAnsiColor(call_by->player))
	  queue_string(call_by, tprintf("%s%s%s%s%s", ANSI_NORMAL, (tcount+1) % 2 ? "" : ANSI_HILITE ,
				  (tcount+1) % 2 ? ANSI_CYAN : ANSI_WHITE, tbuf1, ANSI_NORMAL));
  else 
#endif
	  queue_string(call_by, tbuf1);
  if (SUPPORT_PUEBLO && (call_by->conn_flags & CONN_HTML)) {
    queue_newwrite(call_by, (unsigned char *) "\n</PRE>\n", 8);
    queue_newwrite(call_by, (unsigned char *) "<img xch_mode=purehtml>", 23);
  } else
    queue_newwrite(call_by, (unsigned char *) "\r\n", 2);
}

static const char *
time_format_1(long dt)
{
  register struct tm *delta;
  time_t holder;		/* A hack for 64bit SGI */
  static char buf[64];
  if (dt < 0)
    dt = 0;
  holder = (time_t) dt;
  delta = gmtime(&holder);
  if (delta->tm_yday > 0) {
    sprintf(buf, "%dd %02d:%02d",
	    delta->tm_yday, delta->tm_hour, delta->tm_min);
  } else {
    sprintf(buf, "%02d:%02d", delta->tm_hour, delta->tm_min);
  }
  return buf;
}

static const char *
time_format_2(long dt)
{
  register struct tm *delta;
  static char buf[64];
  if (dt < 0)
    dt = 0;

  delta = gmtime((time_t *) & dt);
  if (delta->tm_yday > 0) {
    sprintf(buf, "%dd", delta->tm_yday);
  } else if (delta->tm_hour > 0) {
    sprintf(buf, "%dh", delta->tm_hour);
  } else if (delta->tm_min > 0) {
    sprintf(buf, "%dm", delta->tm_min);
  } else {
    sprintf(buf, "%ds", delta->tm_sec);
  }
  return buf;
}

/* connection messages
 * isnew: newly creaetd or not?
 * num: how many times connected?
 */
void
announce_connect(dbref player, int isnew, int num)
{
  dbref loc;
  char tbuf1[BUFFER_LEN];
  dbref zone;
  dbref obj;
  int j;

  set_flag_internal(player, "CONNECTED");

  if (isnew) {
    /* A brand new player created. */
    sprintf(tbuf1, T("%s created."), Name(player));
    flag_broadcast(0, "MONITOR", "%s %s", T("GAME:"), tbuf1);
  }

  /* Redundant, but better for translators */
  if (Dark(player)) {
    sprintf(tbuf1, (num > 1) ? T("%s has DARK-reconnected.") :
	    T("%s has DARK-connected."), Name(player));
  } else if (hidden(player)) {
    sprintf(tbuf1, (num > 1) ? T("%s has HIDDEN-reconnected.") :
	    T("%s has HIDDEN-connected."), Name(player));
  } else {
    sprintf(tbuf1, (num > 1) ? T("%s has reconnected.") :
	    T("%s has connected."), Name(player));
  }

  /* send out messages */
  if (!Dark(player))
    flag_broadcast(0, "MONITOR", "%s %s", T("GAME:"), tbuf1);

#ifdef CHAT_SYSTEM
  if (ANNOUNCE_CONNECTS)
    chat_player_announce(player, tbuf1, (num == 1));
#endif /* CHAT_SYSTEM */

  loc = Location(player);
  if (!GoodObject(loc)) {
    notify(player, T("You are nowhere!"));
    return;
  }
  orator = player;

  if (cf_motd_msg && *cf_motd_msg) {
    raw_notify(player, cf_motd_msg);
  }
  raw_notify(player, " ");

  if(ANNOUNCE_CONNECTS)
    notify_except(Contents(player), player, tbuf1, 0);
  /* added to allow player's inventory to hear a player connect */
  if(ANNOUNCE_CONNECTS)
    if(!Dark(player))
    notify_except(Contents(loc), player, tbuf1, NA_INTER_PRESENCE);

  /* clear the environment for possible actions */
  for (j = 0; j < 10; j++)
    global_eval_context.wnxt[j] = NULL;
  for (j = 0; j < NUMQ; j++)
    global_eval_context.rnxt[j] = NULL;
  strcpy(global_eval_context.ccom, "");

  /* do the person's personal connect action */
  (void) queue_attribute(player, "ACONNECT", player);
  if (ROOM_CONNECTS) {
    /* Do the room the player connected into */
    if (IsRoom(loc) || IsThing(loc)) {
      (void) queue_attribute(loc, "ACONNECT", player);
    }
  }
  /* do the person's division */
  if (GoodObject(SDIV(player).object))
    (void) queue_attribute(SDIV(player).object, "ACONNECT", player);
  /* do the zone of the player's location's possible aconnect */
  if ((zone = Zone(loc)) != NOTHING) {
    switch (Typeof(zone)) {
    case TYPE_THING:
      (void) queue_attribute(zone, "ACONNECT", player);
      break;
    case TYPE_ROOM:
      /* check every object in the room for a connect action */
      DOLIST(obj, Contents(zone)) {
	(void) queue_attribute(obj, "ACONNECT", player);
      }
      break;
    default:
      do_log(LT_ERR, 0, 0,
	     T("Invalid zone #%d for %s(#%d) has bad type %d"), zone,
	     Name(player), player, Typeof(zone));
    }
  }
  /* now try the master room */
  DOLIST(obj, Contents(MASTER_ROOM)) {
    (void) queue_attribute(obj, "ACONNECT", player);
  }
}

void
announce_disconnect(dbref player)
{
  dbref loc;
  int num;
  DESC *d;
  char tbuf1[BUFFER_LEN];
  dbref zone, obj;
  int j;

  loc = Location(player);
  if (!GoodObject(loc))
    return;

  orator = player;

  for (num = 0, d = descriptor_list; d; d = d->next)
    if (d->connected && (d->player == player))
      num++;
  if (num < 2) {
    sprintf(tbuf1, T("%s has disconnected."), Name(player));

    if (ANNOUNCE_CONNECTS) {
      if (!Dark(player))
	notify_except(Contents(loc), player, tbuf1, NA_INTER_PRESENCE);
      /* notify contents */
      notify_except(Contents(player), player, tbuf1, 0);
    }

    /* clear the environment for possible actions */
    for (j = 0; j < 10; j++)
      global_eval_context.wnxt[j] = NULL;
    for (j = 0; j < NUMQ; j++)
      global_eval_context.rnxt[j] = NULL;

    /* Setup all connect information as info to pass */
    (void) queue_attribute(player, "ADISCONNECT", player);
    /* do the person's division */
    if (GoodObject(SDIV(player).object))
      (void) queue_attribute(SDIV(player).object, "ADISCONNECT", player);
    if (ROOM_CONNECTS)
      if (IsRoom(loc) || IsThing(loc)) {
	(void) queue_attribute(loc, "ADISCONNECT", player);
      }
    /* do the zone of the player's location's possible adisconnect */
    if ((zone = Zone(loc)) != NOTHING) {
      switch (Typeof(zone)) {
      case TYPE_DIVISION:
      case TYPE_THING:
	(void) queue_attribute(zone, "ADISCONNECT", player);
	break;
      case TYPE_ROOM:
	/* check every object in the room for a connect action */
	DOLIST(obj, Contents(zone)) {
	  (void) queue_attribute(obj, "ADISCONNECT", player);
	}
	break;
      default:
	do_log(LT_ERR, 0, 0,
	       T("Invalid zone #%d for %s(#%d) has bad type %d"), zone,
	       Name(player), player, Typeof(zone));
      }
    }
    /* now try the master room */
    DOLIST(obj, Contents(MASTER_ROOM)) {
      (void) queue_attribute(obj, "ADISCONNECT", player);
    }
    clear_flag_internal(player, "CONNECTED");
    (void) atr_add(player, "LASTLOGOUT", show_time(mudtime, 0), GOD, NOTHING);
  } else {
    /* note: when you partially disconnect, ADISCONNECTS are not executed */
    sprintf(tbuf1, T("%s has partially disconnected."), Name(player));

    if (ANNOUNCE_CONNECTS) {
      if (!Dark(player))
	notify_except(Contents(loc), player, tbuf1, NA_INTER_PRESENCE);
      /* notify contents */
      notify_except(Contents(player), player, tbuf1, 0);
    }
  }

#ifdef CHAT_SYSTEM
  if (ANNOUNCE_CONNECTS)
    chat_player_announce(player, tbuf1, 0);
#endif /* CHAT_SYSTEM */

  /* Monitor broadcasts */
  /* Redundant, but better for translators */
  if (Dark(player)) {
    sprintf(tbuf1, (num < 2) ? T("%s has DARK-disconnected.") :
	    T("%s has partially DARK-disconnected."), Name(player));
  } else if (hidden(player)) {
    sprintf(tbuf1, (num < 2) ? T("%s has HIDDEN-disconnected.") :
	    T("%s has partially HIDDEN-disconnected."), Name(player));
  } else {
    sprintf(tbuf1, (num < 2) ? T("%s has disconnected.") :
	    T("%s has partially disconnected."), Name(player));
  }
  if (!Dark(player))
    flag_broadcast(0, "MONITOR", "%s %s", T("GAME:"), tbuf1);
  if(Guest(player) && !Connected(player))  { /* queue for destruction */
      set_flag_internal(player, "GOING");
      set_flag_internal(player, "GOING_TWICE");
    }
  local_disconnect(player, num);
}

/** Set an motd message.
 * \verbatim
 * This implements @motd.
 * \endverbatim
 * \param player the enactor.
 * \param key type of MOTD to set.
 * \param message text to set the motd to.
 */
void
do_motd(dbref player, enum motd_type key, const char *message)
{

  if (!Site(player) && key != MOTD_LIST) {
    notify(player,
	   T
	   ("You may get 15 minutes of fame and glory in life, but not right now."));
    return;
  }
  switch (key) {
  case MOTD_MOTD:
    strcpy(cf_motd_msg, message);
    notify(player, T("Motd set."));
    break;
  case MOTD_DOWN:
    strcpy(cf_downmotd_msg, message);
    notify(player, T("Down motd set."));
    break;
  case MOTD_FULL:
    strcpy(cf_fullmotd_msg, message);
    notify(player, T("Full motd set."));
    break;
  case MOTD_LIST:
    notify_format(player, "MOTD: %s", cf_motd_msg);
    if (Site(player)) {
      notify_format(player, T("Down MOTD: %s"), cf_downmotd_msg);
      notify_format(player, T("Full MOTD: %s"), cf_fullmotd_msg);
    }
  }
}

/** Set a DOING message.
 * \verbatim
 * This implements @doing.
 * \endverbatim
 * \param player the enactor.
 * \param message the message to set.
 */
void
do_doing(dbref player, const char *message)
{
  char buf[MAX_COMMAND_LEN];
  DESC *d;
  int i;

  if (!Connected(player)) {
    /* non-connected things have no need for a doing */
    notify(player, T("Why would you want to do that?"));
    return;
  }
  strncpy(buf, remove_markup(message, NULL), DOING_LEN - 1);

  /* now smash undesirable characters and truncate */
  for (i = 0; i < DOING_LEN; i++) {
    if ((buf[i] == '\r') || (buf[i] == '\n') ||
	(buf[i] == '\t') || (buf[i] == BEEP_CHAR))
      buf[i] = ' ';
  }
  buf[DOING_LEN - 1] = '\0';

  /* set it */
  for (d = descriptor_list; d; d = d->next)
    if (d->connected && (d->player == player))
      strcpy(d->doing, buf);
  if (strlen(message) >= DOING_LEN) {
    notify_format(player,
		  T("Doing set. %zu characters lost."),
		  strlen(message) - (DOING_LEN - 1));
  } else
    notify(player, T("Doing set."));
}

/** Set a poll message (which replaces "Doing" in the DOING output).
 * \verbatim
 * This implements @poll.
 * \endverbatim
 * \param player the enactor.
 * \param message the message to set.
 */
void
do_poll(dbref player, const char *message)
{
  int i;

  if (!Change_Poll(player)) {
    notify(player, T("Who do you think you are, Gallup?"));
    return;
  }
  strncpy(poll_msg, remove_markup(message, NULL), DOING_LEN - 1);
  for (i = 0; i < DOING_LEN; i++) {
    if ((poll_msg[i] == '\r') || (poll_msg[i] == '\n') ||
	(poll_msg[i] == '\t') || (poll_msg[i] == BEEP_CHAR))
      poll_msg[i] = ' ';
  }
  poll_msg[DOING_LEN - 1] = '\0';

  if (strlen(message) >= DOING_LEN) {
    poll_msg[DOING_LEN - 1] = 0;
    notify_format(player,
		  T("Poll set. %zu characters lost."),
		  strlen(message) - (DOING_LEN - 1));
  } else
    notify(player, T("Poll set."));
  do_log(LT_WIZ, player, NOTHING, T("Poll Set to '%s'."), poll_msg);
}

/** Match the partial name of a connected player.
 * \param match string to match.
 * \return dbref of a unique connected player whose name partial-matches, 
 * AMBIGUOUS, or NOTHING.
 */
dbref
short_page(const char *match)
{
  DESC *d;
  dbref who1 = NOTHING;
  int count = 0;

  for (d = descriptor_list; d; d = d->next) {
    if (d->connected) {
      if (match && !string_prefix(Name(d->player), match))
	continue;
      if (!strcasecmp(Name(d->player), match)) {
	count = 1;
	who1 = d->player;
	break;
      }
      if (who1 == NOTHING || d->player != who1) {
	who1 = d->player;
	count++;
      }
    }
  }

  if (count > 1)
    return AMBIGUOUS;
  else if (count == 0)
    return NOTHING;

  return who1;
}

/** Match the partial name of a connected player the enactor can see.
 * \param player the enactor
 * \param match string to match.
 * \return dbref of a unique connected player whose name partial-matches, 
 * AMBIGUOUS, or NOTHING.
 */
dbref
visible_short_page(dbref player, const char *match)
{
  dbref target;
  target = short_page(match);
  if (Priv_Who(player) || !GoodObject(target))
    return target;
  if (Dark(target) || (hidden(target) && !nearby(player, target)))
    return NOTHING;
  return target;
}

/* LWHO() function - really belongs elsewhere but needs stuff declared here */

/* ARGSUSED */
FUNCTION(fun_lwho)
{
  DESC *d;
  dbref victim;
  int first, powered = (*called_as == 'L') && Priv_Who(executor);

  first = 1;
  if(nargs && args[0] && *args[0]) {
    if(!powered) {
      safe_str(T(e_perm), buff, bp);
      return;
    }

    if( (victim = noisy_match_result(executor, args[0], NOTYPE, MAT_EVERYTHING)) == 0) {
      safe_str(T(e_notvis), buff, bp);
      return;
    }
    if(!controls(executor, victim)) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
    if(!Priv_Who(victim))
      powered = 0;
  } else victim = executor;
  
  DESC_ITER_CONN(d) {
    if (!Hidden(d) || (powered && CanSee(victim,d->player))) {
      if (first)
	first = 0;
      else
	safe_chr(' ', buff, bp);
      safe_dbref(d->player, buff, bp);
    }
  }
}


/** Look up a DESC by character name or file descriptor.
 * \param executor the dbref of the object calling the function calling this.
 * \param name the name or descriptor to look up.
 * \retval a pointer to the proper DESC, or NULL
 */
static DESC *
lookup_desc(dbref executor, const char *name)
{
  DESC *d;

  /* Given a file descriptor. See-all only. */
  if (is_strict_integer(name)) {
    int fd = parse_integer(name);
    DESC_ITER_CONN(d) {
      if (d->descriptor == fd) {
	if (Priv_Who(executor) || d->player == executor
	|| (Inherit_Powers(executor) && Priv_Who(Owner(executor))))
	  return d;
	else
	  return NULL;
      }
    }
    return NULL;
  } else {                    /* Look up player name */
    DESC *match = NULL;
    dbref target = lookup_player(name);
    if (target == NOTHING) {
      target = match_result(executor, name, TYPE_PLAYER,
			    MAT_ABSOLUTE | MAT_PLAYER | MAT_ME);
    }
    if (!GoodObject(target) || !Connected(target))
      return NULL;
    else {
      /* walk the descriptor list looking for a match of a dbref */
      DESC_ITER_CONN(d) {
	if ((d->player == target) &&
	    (!Hidden(d) || Priv_Who(executor) ||
		(Inherit_Powers(executor) && Priv_Who(Owner(executor)))) &&
	    (!match || (d->last_time > match->last_time)))
	  match = d;
      }
      return match;
    }
  }
}

/** Return the conn time of the longest-connected connection of a player.
 * This function treats hidden connectios as nonexistent.
 * \param player dbref of player to get ip for.
 * \return connection time of player as an INT, or -1 if not found or hidden.
 */
int
most_conn_time(dbref player)
{
  DESC *d, *match = NULL;
  DESC_ITER_CONN(d) {
    if ((d->player == player) && !Hidden(d) && (!match ||
						(d->connected_at >
						 match->connected_at)))
      match = d;
  }
  if (match) {
    double result = difftime(mudtime, match->connected_at);
    return (int) result;
  } else
    return -1;
}

/** Return the conn time of the longest-connected connection of a player.
 * This function does includes hidden people.
 * \param player dbref of player to get ip for.
 * \return connection time of player as an INT, or -1 if not found.
 */
int
most_conn_time_priv(dbref player)
{
  DESC *d, *match = NULL;
  DESC_ITER_CONN(d) {
    if ((d->player == player) && (!match ||
				  (d->connected_at > match->connected_at)))
      match = d;
  }
  if (match) {
    double result = difftime(mudtime, match->connected_at);
    return (int) result;
  } else
    return -1;
}

/** Return the idle time of the least-idle connection of a player.
 * This function treats hidden connections as nonexistant.
 * \param player dbref of player to get time for.
 * \return idle time of player as an INT, or -1 if not found or hidden.
 */
int
least_idle_time(dbref player)
{
  DESC *d, *match = NULL;
  DESC_ITER_CONN(d) {
    if ((d->player == player) && !Hidden(d) &&
	(!match || (d->last_time > match->last_time)))
      match = d;
  }
  if (match) {
    double result = difftime(mudtime, match->last_time);
    return (int) result;
  } else
    return -1;
}

/** Return the idle time of the least-idle connection of a player.
 * This function performs no permission checking.
 * \param player dbref of player to get time for.
 * \return idle time of player as an INT, or -1 if not found.
 */
int
least_idle_time_priv(dbref player)
{
  DESC *d, *match = NULL;
  DESC_ITER_CONN(d) {
    if ((d->player == player) && (!match || (d->last_time > match->last_time)))
      match = d;
  }
  if (match) {
    double result = difftime(mudtime, match->last_time);
    return (int) result;
  } else
    return -1;
}

/** Return the ip address of the least-idle connection of a player.
 * This function performs no permission checking, and returns the
 * pointer from the descriptor itself (so don't destroy the result!)
 * \param player dbref of player to get ip for.
 * \return IP address of player as a string or NULL if not found.
 */
char *
least_idle_ip(dbref player)
{
  DESC *d, *match = NULL;
  DESC_ITER_CONN(d) {
    if ((d->player == player) && (!match || (d->last_time > match->last_time)))
      match = d;
  }
  return match ? (match->ip) : NULL;
}

/** Return the hostname of the least-idle connection of a player.
 * This function performs no permission checking, and returns a static
 * string.
 * \param player dbref of player to get ip for.
 * \return hostname of player as a string or NULL if not found.
 */
char *
least_idle_hostname(dbref player)
{
  DESC *d, *match = NULL;
  static char hostname[101];
  char *p;

  DESC_ITER_CONN(d) {
    if ((d->player == player) && (!match || (d->last_time > match->last_time)))
      match = d;
  }
  if (!match)
    return NULL;
  strcpy(hostname, match->addr);
  if ((p = strchr(hostname, '@'))) {
    p++;
    return p;
  } else
    return hostname;
}

/* ZWHO() function - really belongs in eval.c but needs stuff declared here */
/* ARGSUSED */
FUNCTION(fun_zwho)
{
  DESC *d;
  dbref zone, victim;
  int first;
  int powered = (strcmp(called_as, "ZMWHO") && Priv_Who(executor) || (Inherit_Powers(executor) && Priv_Who(Owner(executor))));
  first = 1;

  zone = match_thing(executor, args[0]);

  if (nargs == 1) {
    victim = executor;
  } else if ((nargs == 2) && powered) {
    if ((victim = match_thing(executor, args[1])) == 0) {
      safe_str(T(e_match), buff, bp);
      return;
    }
  } else {
    safe_str(T(e_perm), buff, bp);
    return;
  }

  if (!GoodObject(zone) || !(eval_lock(victim, zone, Zone_Lock) || CanModify(victim,zone))) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  if ((getlock(zone, Zone_Lock) == TRUE_BOOLEXP) ||
      (IsPlayer(zone) && !(has_flag_by_name(zone, "SHARED", TYPE_PLAYER)))) {
    safe_str(T("#-1 INVALID ZONE."), buff, bp);
    return;
  }

  /* Use lowest privilege for victim */
  if (!Priv_Who(victim))
    powered = 0;

  DESC_ITER_CONN(d) {
    if (!Hidden(d) || powered) {
      if (Zone(Location(d->player)) == zone) {
	if (first) {
	  first = 0;
	} else {
	  safe_chr(' ', buff, bp);
	}
	safe_dbref(d->player, buff, bp);
      }
    }
  }
}

/* ARGSUSED */
FUNCTION(fun_doing)
{
  /* Gets a player's @doing */
  DESC *d = lookup_desc(executor, args[0]);
  if (d)
    safe_str(d->doing, buff, bp);
  else
    safe_str("#-1", buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_hostname)
{
  /* Gets a player's hostname */
  DESC *d = lookup_desc(executor, args[0]);
  if (d && (d->player == executor || Site(executor)))
    safe_str(d->addr, buff, bp);
  else
    safe_str("#-1", buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_ipaddr)
{
  /* Gets a player's IP address */
  DESC *d = lookup_desc(executor, args[0]);
  if (d && (d->player == executor || Site(executor)))
    safe_str(d->ip, buff, bp);
  else
    safe_str("#-1", buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_cmds)
{
  /* Gets a player's IP address */
  DESC *d = lookup_desc(executor, args[0]);
  if (d && (d->player == executor || Site(executor)))
    safe_integer(d->cmds, buff, bp);
  else
    safe_integer(-1, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_sent)
{
  /* Gets a player's bytes sent */
  DESC *d = lookup_desc(executor, args[0]);
  if (d && (d->player == executor || Site(executor)))
    safe_integer(d->input_chars, buff, bp);
  else
    safe_integer(-1, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_recv)
{
  /* Gets a player's bytes received */
  DESC *d = lookup_desc(executor, args[0]);
  if (d && (d->player == executor || Site(executor)))
    safe_integer(d->output_chars, buff, bp);
  else
    safe_integer(-1, buff, bp);
}

FUNCTION(fun_poll)
{
  /* Gets the current poll */
  if (poll_msg[0] == '\0')
    strcpy(poll_msg, "Doing");

  safe_str(poll_msg, buff, bp);
}

FUNCTION(fun_pueblo)
{
  /* Return the status of the pueblo flag on the least idle descriptor we
   * find that matches the player's dbref.
   */
  DESC *match = lookup_desc(executor, args[0]);
  if (match)
    safe_boolean(match->conn_flags & CONN_HTML, buff, bp);
  else
    safe_str(T("#-1 NOT CONNECTED"), buff, bp);
}

FUNCTION(fun_ssl)
{
  /* Return the status of the ssl flag on the least idle descriptor we
   * find that matches the player's dbref.
   */
#ifdef HAS_OPENSSL
  DESC *match;
  if (!sslsock) {
    safe_boolean(0, buff, bp);
    return;
  }
  match = lookup_desc(executor, args[0]);
  if (match) {
    if (match->player == executor || Site(executor))
      safe_boolean((match->ssl != NULL), buff, bp);
    else
      safe_str(T(e_perm), buff, bp);
  } else
    safe_str(T("#-1 NOT CONNECTED"), buff, bp);
#else
  safe_boolean(0, buff, bp);
#endif
}

FUNCTION(fun_width)
{
  DESC *match;
  if (!*args[0])
    safe_str(T("#-1 FUNCTION REQUIRES ONE ARGUMENT"), buff, bp);
  else if ((match = lookup_desc(executor, args[0])))
    safe_integer(match->width, buff, bp);
  else
    safe_str("78", buff, bp);
}

FUNCTION(fun_height)
{
  DESC *match;
  if (!*args[0])
    safe_str(T("#-1 FUNCTION REQUIRES ONE ARGUMENT"), buff, bp);
  else if ((match = lookup_desc(executor, args[0])))
    safe_integer(match->height, buff, bp);
  else
    safe_str("24", buff, bp);
}

FUNCTION(fun_terminfo)
{
  DESC *match;
  if (!*args[0])
    safe_str(T("#-1 FUNCTION REQUIRES ONE ARGUMENT"), buff, bp);
  else if ((match = lookup_desc(executor, args[0]))) {
    if (match->player == executor || Site(executor)) {
      safe_str(match->ttype, buff, bp);
      if (match->conn_flags & CONN_HTML)
	safe_str(" pueblo", buff, bp);
      if (match->conn_flags & CONN_TELNET)
	safe_str(" telnet", buff, bp);
#ifdef HAS_OPENSSL
      if (sslsock && match->ssl)
	safe_str(" ssl", buff, bp);
#endif
    } else
      safe_str(T(e_perm), buff, bp);
  } else
    safe_str(T("#-1 NOT CONNECTED"), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_idlesecs)
{
  /* returns the number of seconds a player has been idle, using
   * their least idle connection
   */

  DESC *match = lookup_desc(executor, args[0]);
  if (match)
    safe_number(difftime(mudtime, match->last_time), buff, bp);
  else
    safe_str("-1", buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_idle_average) {
  DESC *match = lookup_desc(executor, args[0]);
  double idle_time;

  if(match) {
    idle_time = difftime(mudtime, match->last_time);
    if(idle_time >= 300)
      safe_number(((match->idle_total + idle_time) / (match->unidle_times+1)), buff, bp);
    else if(match->unidle_times == 0)
      safe_number(0, buff, bp);
    else
      safe_number((match->idle_total / match->unidle_times), buff, bp);
    
  } else
    safe_str("-1", buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_idle_total) {
  DESC *match = lookup_desc(executor, args[0]);
  double idle_time;

  if(match) {
    idle_time = difftime(mudtime, match->last_time);
    safe_number(idle_time >= 300 ? (match->idle_total+idle_time) : match->idle_total, buff, bp);
  } else
    safe_str("-1", buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_idle_times) {
  DESC *match = lookup_desc(executor, args[0]);

  if(match) {
     safe_number((difftime(mudtime, match->last_time) >= 300) ? 
	 (match->unidle_times+1) : match->unidle_times, buff, bp);
  } else
    safe_str("-1", buff, bp);
}


/* ARGSUSED */
FUNCTION(fun_conn)
{
  /* returns the number of seconds a player has been connected, using
   * their longest-connected descriptor
   */

  DESC *match = lookup_desc(executor, args[0]);
  if (match)
    safe_number(difftime(mudtime, match->connected_at), buff, bp);
  else
    safe_str("-1", buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_lports)
{
  DESC *d;
  int first = 1;

  if (!Priv_Who(executor)
	&& !(Inherit_Powers(executor) && Priv_Who(Owner(executor)))) {
    safe_str(T(e_perm), buff, bp);
    return;
  }

  DESC_ITER_CONN(d) {
    if (first)
      first = 0;
    else
      safe_chr(' ', buff, bp);
    safe_integer(d->descriptor, buff, bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_ports)
{
  /* returns a list of the network descriptors that a player is
   * connected to 
   */

  dbref target;
  DESC *d;
  int first;

  target = lookup_player(args[0]);
  if (target == NOTHING) {
    target = match_result(executor, args[0], TYPE_PLAYER,
			  MAT_ABSOLUTE | MAT_PLAYER | MAT_ME);
  }
  if (target != executor && !Priv_Who(executor)
	&& !(Inherit_Powers(executor) && Priv_Who(Owner(executor)))) {
    /* This should probably be a safe_str */
    notify(executor, T("Permission denied."));
    return;
  }
  if (!GoodObject(target) || !Connected(target)) {
    return;
  }
  /* Walk descriptor chain. */
  first = 1;
  DESC_ITER_CONN(d) {
    if (d->player == target) {
      if (first)
	first = 0;
      else
	safe_chr(' ', buff, bp);
      safe_integer(d->descriptor, buff, bp);
    }
  }
}


/** Hide or unhide a player.
 * Although hiding is a per-descriptor state, this function sets all of
 * a player's connected descriptors to be hidden.
 * \param player dbref of player to hide.
 * \param hide if 1, hide; if 0, unhide.
 */
void
hide_player(dbref player, int hide)
{
  DESC *d;
  if (!Connected(player))
    return;
  if (!Can_Hide(player)) {
    notify(player, T("Permission denied."));
    return;
  }
  /* change status on WHO */
  if (Can_Hide(player)) {
    DESC_ITER_CONN(d) {
      if (d->player == player)
	d->hide = hide;
    }
  }
  if (hide)
    notify(player, T("You no longer appear on the WHO list."));
  else
    notify(player, T("You now appear on the WHO list."));
}

/** Perform the periodic check of inactive descriptors, and 
 * disconnect them or autohide them as appropriate.
 */
void
inactivity_check(void)
{
  DESC *d, *nextd;
  ATTR *a;
  char tbuf[BUFFER_LEN];
  time_t now, idle, idle_for, unconnected_idle;
  if (!INACTIVITY_LIMIT && !UNCONNECTED_LIMIT)
    return;
  now = mudtime;
  idle = INACTIVITY_LIMIT ? INACTIVITY_LIMIT : INT_MAX;
  unconnected_idle = UNCONNECTED_LIMIT ? UNCONNECTED_LIMIT : INT_MAX;
  for (d = descriptor_list; d; d = nextd) {
    nextd = d->next;
    idle_for = now - d->last_time;
    /* If they've been connected for 60 seconds without getting a telnet-option
       back, the client probably doesn't understand them */
    if ((d->conn_flags & CONN_TELNET_QUERY) && idle_for > 60)
      d->conn_flags &= ~CONN_TELNET_QUERY;
    if(d->connected && GoodObject(d->player) && ((a = atr_get(d->player, "IDLE_TIMEOUT"))!=NULL)) {
	    memset(tbuf, '\0', BUFFER_LEN);
	    strncpy(tbuf, atr_value(a), BUFFER_LEN-1);
	    idle = atoi(tbuf);
	    if(idle > 0)
	      goto after_idle_atr_check;
    } 
    idle = INACTIVITY_LIMIT ? INACTIVITY_LIMIT : INT_MAX;
after_idle_atr_check:
    if ((d->connected) ? (idle_for > idle ) : (idle_for > unconnected_idle)) {

      if (!d->connected)
	shutdownsock(d);
      else if (!Can_Idle(d->player)) {

	queue_string(d, T("\n*** Inactivity timeout ***\n"));
	do_log(LT_CONN, 0, 0,
	       T("[%d/%s/%s] Logout by %s(#%d) <Inactivity Timeout>"),
	       d->descriptor, d->addr, d->ip, Name(d->player), d->player);
	boot_desc(d);
      } else if (Unfind(d->player)) {

	if ((Can_Hide(d->player)) && (!Hidden(d))) {
	  queue_string(d,
		       T
		       ("\n*** Inactivity limit reached. You are now HIDDEN. ***\n"));
	  d->hide = 1;
	}
      }
    }
  }
}


/** Given a player dbref, return the player's hidden status.
 * \param player dbref of player to check.
 * \retval 1 player is hidden.
 * \retval 0 player is not hidden.
 */
int
hidden(dbref player)
{
  DESC *d;
  DESC_ITER_CONN(d) {
    if (d->player == player) {
      if (Hidden(d))
	return 1;
      else
	return 0;
    }
  }
  return 0;
}


/** Return the mailp of the player closest in db# to player,
 * or NULL if there's nobody on-line.
 * In the current mail system, mail is stored in a linked list, sorted
 * by recipient, which makes the most common operations (listing and reading
 * your mail) fast. When a player first connects, we store (on the
 * mailp element of their descriptor) a pointer to the beginning of
 * their part of the linked list. Rather than search the whole linked
 * list to find this location, we look at the mailp's of all the other
 * connected players, and find the mailp of the player whose dbref
 * is closest to the connecting player, and start our search from that
 * point. This scales up nicely - as a mushes get larger, the linked
 * list gets larger, but the more people connected at once, the faster
 * the search for a newly connecting player's first mail.
 * \param player player whose db# we want to get near.
 * \return pointer to first mail of connected player with db# closest to
 * player.
 */
MAIL *
desc_mail(dbref player)
{
  DESC *d;
  int i;
  int diff = db_top;
  static MAIL *mp;
  mp = NULL;
  DESC_ITER_CONN(d) {
    i = abs(d->player - player);
    if (i == 0)
      return d->mailp;
    if ((i < diff) && d->mailp) {
      diff = i;
      mp = d->mailp;
    }
  }
  return mp;
}

/** Set a player's mail position on all their descriptors.
 * \param player player to set mail position for.
 * \param mp pointer to first mail in their list.
 */
void
desc_mail_set(dbref player, MAIL *mp)
{
  DESC *d;
  DESC_ITER_CONN(d) {
    if (d->player == player)
      d->mailp = mp;
  }
}

/** Clear mail positions on all descriptors. Called from do_mail_nuke().
 */
void
desc_mail_clear(void)
{
  DESC *d;
  DESC_ITER_CONN(d) {
    d->mailp = NULL;
  }
}




#ifdef SUN_OS
/* SunOS's implementation of stdio breaks when you get a file descriptor
 * greater than 128. Brain damage, brain damage, brain damage!
 *
 * Our objective, therefore, is not to fix stdio, but to work around it,
 * so that performance degrades semi-gracefully when you are using a lot
 * of file descriptors.
 * Therefore, we'll save a file descriptor when we start up that is less
 * than 128, so that if we get a file descriptor that is >= 128, we can
 * use our own saved file descriptor instead. This is only one level of
 * defense; if you have more than 128 fd's in use, and you try two fopen's
 * before doing an fclose(), the second will fail.
 */

FILE *
fopen(file, mode)
    const char *file;
    const char *mode;
{
/* FILE *f; */
  int fd, rw, oflags = 0;
/* char tbchar; */
  rw = (mode[1] == '+') || (mode[1] && (mode[2] == '+'));
  switch (*mode) {
  case 'a':
    oflags = O_CREAT | (rw ? O_RDWR : O_WRONLY);
    break;
  case 'r':
    oflags = rw ? O_RDWR : O_RDONLY;
    break;
  case 'w':
    oflags = O_TRUNC | O_CREAT | (rw ? O_RDWR : O_WRONLY);
    break;
  default:
    return (NULL);
  }
/* SunOS fopen() doesn't use the 't' or 'b' flags. */


  fd = open(file, oflags, 0666);
  if (fd < 0)
    return NULL;
  /* My addition, to cope with SunOS brain damage! */
  if (fd >= 128) {
    close(fd);
    if ((extrafd < 128) && (extrafd >= 0)) {
      close(extrafd);
      fd = open(file, oflags, 0666);
      extrafd = -1;
    } else {
      return NULL;
    }
  }
  /* End addition. */

  return fdopen(fd, mode);
}


#undef fclose(x)
int
f_close(stream)
    FILE *stream;
{
  int fd = fileno(stream);
  /* if extrafd is bad, and the fd we're closing is good, recycle the
   * fd into extrafd.
   */
  fclose(stream);
  if (((extrafd < 0)) && (fd >= 0) && (fd < 128)) {
    extrafd = open("/dev/null", O_RDWR);
    if (extrafd >= 128) {
      /* To our surprise, we didn't get a usable fd. */
      close(extrafd);
      extrafd = -1;
    }
  }
  return 0;
}

#define fclose(x) f_close(x)
#endif				/* SUN_OS */

static int
how_many_fds(void)
{
  /* Determine how many open file descriptors we're allowed
   * In order, we'll try:
   * 1. sysconf(_SC_OPEN_MAX) - POSIX.1
   * 2. OPEN_MAX constant - POSIX.1 limits.h
   * 3. getdtablesize - BSD (which Config maps to ulimit or NOFILE if needed)
   */
  static int open_max = 0;
#ifdef WIN32
  int iMaxSocketsAllowed;
  unsigned short wVersionRequested = MAKEWORD(1, 1);
  int err;
#endif
  if (open_max)
    return open_max;
#ifdef WIN32
  /* Typically, WIN32 allows many open sockets, but won't perform
   * well if too many are used. The best approach is to give the
   * admin a single point of control (MAX_LOGINS in MUSH.CNF) and then
   * allow a few more connections than that here for clients to get access
   * to an E-mail address or at least a title. 2 is an arbitrary number.
   *
   * If max_logins is set to 0 in mush.cnf (unlimited logins),
   * we'll allocate 120 sockets for now.
   *
   * wsadata.iMaxSockets isn't valid for WinSock versions 2.0
   * and later, but we are requesting version 1.1, so it's valid.
   */

  /* Need to init Windows Sockets to get socket data */
  err = WSAStartup(wVersionRequested, &wsadata);
  if (err) {
    printf(T("Error %i on WSAStartup\n"), err);
    exit(1);
  }
  iMaxSocketsAllowed = options.max_logins ? (2 * options.max_logins) : 120;
  if (wsadata.iMaxSockets < iMaxSocketsAllowed)
    iMaxSocketsAllowed = wsadata.iMaxSockets;
  return iMaxSocketsAllowed;
#else
#ifdef HAS_SYSCONF
  errno = 0;
  if ((open_max = sysconf(_SC_OPEN_MAX)) < 0) {
    if (errno == 0)		/* Value was indeterminate */
      open_max = 0;
  }
  if (open_max)
    return open_max;
#endif
#ifdef OPEN_MAX
  open_max = OPEN_MAX;
  return open_max;
#endif
  /* Caching getdtablesize is dangerous, since it's affected by
   * getrlimit, so we don't.
   */
  open_max = 0;
  return getdtablesize();
#endif				/* WIN 32 */
}

#ifdef HAS_OPENSSL
/** Take down all SSL client connections and close the SSL server socket.
 * Typically, this is in preparation for a shutdown/reboot.
 */
void
close_ssl_connections(void)
{
  DESC *d;

  if (!sslsock)
    return;

  /* Close clients */
  DESC_ITER_CONN(d) {
    if (d->ssl) {
      queue_string_eol(d, T(ssl_shutdown_message));
      process_output(d);
      ssl_close_connection(d->ssl);
      d->ssl = NULL;
      d->conn_flags |= CONN_CLOSE_READY;
    }
  }
  /* Close server socket */
  ssl_close_connection(ssl_master_socket);
  shutdown(sslsock, 2);
  closesocket(sslsock);
  sslsock = 0;
  options.ssl_port = 0;
}
#endif


/** Dump the descriptor list to our REBOOTFILE so we can restore it on reboot.
 */
void
dump_reboot_db(void)
{
  FILE *f;
  DESC *d;
  SU_PATH *exit_path;
  long flags = RDBF_SCREENSIZE | RDBF_TTYPE | RDBF_PUEBLO_CHECKSUM
		| RDBF_SU_EXIT_PATH;
  if (setjmp(db_err)) {
    flag_broadcast(0, 0, T("GAME: Error writing reboot database!"));
    exit(0);
  } else {

    f = fopen(REBOOTFILE, "w");
    /* This shouldn't happen */
    if (!f) {
      flag_broadcast(0, 0, T("GAME: Error writing reboot database!"));
      exit(0);
    }
    /* Write out the reboot db flags here */
    fprintf(f, "V%ld\n", flags);
    putref(f, sock);
    putref(f, maxd);
    /* First, iterate through all descriptors to get to the end
     * we do this so the descriptor_list isn't reversed on reboot
     */
    for (d = descriptor_list; d && d->next; d = d->next) ;
    /* Second, we iterate backwards from the end of descriptor_list
     * which is now in the d variable.
     */
    for (; d != NULL; d = d->prev) {
      putref(f, d->descriptor);
      putref(f, d->connected_at);
      putref(f, d->hide);
      putref(f, d->cmds);
      putref(f, d->idle_total);
      putref(f, d->unidle_times);
      if (GoodObject(d->player))
	putref(f, d->player);
      else
	putref(f, -1);
      putref(f, d->last_time);
      if (d->output_prefix)
	putstring(f, (char *) d->output_prefix);
      else
	putstring(f, "__NONE__");
      if (d->output_suffix)
	putstring(f, (char *) d->output_suffix);
      else
	putstring(f, "__NONE__");
      putstring(f, d->addr);
      putstring(f, d->ip);
      putstring(f, d->doing);
      putref(f, d->conn_flags);
      putref(f, d->width);
      putref(f, d->height);
      putstring(f, d->ttype);
      putstring(f, d->checksum);
      for(exit_path = d->su_exit_path; exit_path; exit_path = exit_path->next)
        putref(f, exit_path->player);
      putref(f, NOTHING);
    }				/* for loop */

    putref(f, 0);
    putstring(f, poll_msg);
    putref(f, globals.first_start_time);
    putref(f, globals.reboot_count);
    fclose(f);
  }
}

/** Load the descriptor list back from the REBOOTFILE on reboot.
 */
void
load_reboot_db(void)
{
  FILE *f;
  DESC *d = NULL;
  DESC *closed = NULL, *nextclosed;
  int val;
  int n;
  const char *temp;
  char c;
  long flags = 0;
  char tbuf1[BUFFER_LEN];
  SU_PATH *epnext, *epprev;
  dbref exit_path;
  f = fopen(REBOOTFILE, "r");
  if (!f) {
    restarting = 0;
    return;
  }
  restarting = 1;
  /* Get the first line and see if it's a set of reboot db flags.
   * Those start with V<number>
   * If not, assume we're using the original format, in which the
   * sock appears first
   * */
  c = getc(f);			/* Skip the V */
  if (c == 'V') {
    flags = getref(f);
  } else {
    ungetc(c, f);
  }

  sock = getref(f);
  val = getref(f);
  if (val > maxd)
    maxd = val;

  while ((val = getref(f)) != 0) {
    ndescriptors++;
    d = (DESC *) mush_malloc(sizeof(DESC), "descriptor");
    d->descriptor = val;
    d->input_handler = do_command;
    d->connected_at = getref(f);
    d->hide = getref(f);
    d->cmds = getref(f);
    d->idle_total = getref(f);
    d->unidle_times = getref(f);
    d->player = getref(f);
    d->last_time = getref(f);
    d->pinfo.object = NOTHING;
    d->pinfo.atr = NULL;
    d->pinfo.lock = 0;
    d->pinfo.function = NULL;
    d->connected = GoodObject(d->player) ? 1 : 0;
    /* setup snooper array */
    for(n = 0; n < MAX_SNOOPS; n++)
      d->snooper[n] = -1;

    temp = getstring_noalloc(f);
    d->output_prefix = NULL;
    if (strcmp(temp, "__NONE__"))
      set_userstring(&d->output_prefix, temp);
    temp = getstring_noalloc(f);
    d->output_suffix = NULL;
    if (strcmp(temp, "__NONE__"))
      set_userstring(&d->output_suffix, temp);
    strcpy(d->addr, getstring_noalloc(f));
    strcpy(d->ip, getstring_noalloc(f));
    strcpy(d->doing, getstring_noalloc(f));
    d->conn_flags = getref(f);
    if (flags & RDBF_SCREENSIZE) {
      d->width = getref(f);
      d->height = getref(f);
    } else {
      d->width = 78;
      d->width = 24;
    }
    if (flags & RDBF_TTYPE)
      d->ttype = mush_strdup(getstring_noalloc(f), "terminal description");
    else
      d->ttype = mush_strdup("unknown", "terminal description");
    if (flags & RDBF_PUEBLO_CHECKSUM)
      strcpy(d->checksum, getstring_noalloc(f));
    else
      d->checksum[0] = '\0';
    d->su_exit_path = NULL;
    if (flags & RDBF_SU_EXIT_PATH) {
      exit_path = getref(f);
      while(GoodObject(exit_path)) {
        add_to_exit_path(d, exit_path);
        exit_path = getref(f);
      }
      epprev = NULL;
      while(d->su_exit_path) {
        epnext = d->su_exit_path->next;
        d->su_exit_path->next = epprev;
        epprev = d->su_exit_path;
        d->su_exit_path = epnext;
      }
      d->su_exit_path = epprev;
    }
    d->input_chars = 0;
    d->output_chars = 0;
    d->output_size = 0;
    d->output.head = 0;
    d->output.tail = &d->output.head;
    d->input.head = 0;
    d->input.tail = &d->input.head;
    d->raw_input = NULL;
    d->raw_input_at = NULL;
    d->quota = options.starting_quota;
    d->mailp = NULL;
#ifdef HAS_OPENSSL
    d->ssl = NULL;
    d->ssl_state = 0;
#endif
    if (d->conn_flags & CONN_CLOSE_READY) {
      /* This isn't really an open descriptor, we're just tracking
       * it so we can announce the disconnect properly. Do so, but
       * don't link it into the descriptor list. Instead, keep a
       * separate list.
       */
      if (closed)
	closed->prev = d;
      d->next = closed;
      d->prev = NULL;
      closed = d;
    } else {
      if (descriptor_list)
	descriptor_list->prev = d;
      d->next = descriptor_list;
      d->prev = NULL;
      descriptor_list = d;
      if (d->connected && d->player && GoodObject(d->player) &&
	  IsPlayer(d->player))
	set_flag_internal(d->player, "CONNECTED");
      else if ((!d->player || !GoodObject(d->player)) && d->connected) {
	d->connected = 0;
	d->player = 0;
      }
    /* If they were in a program, get them back into it */
      if(d->connected && InProg(d->player))
        prog_load_desc(d);
    }
  }				/* while loop */

  /* Now announce disconnects of everyone who's not really here */
  while (closed) {
    nextclosed = closed->next;
    if(closed->last_time > 0) {
      closed->idle_total += difftime(mudtime, closed->last_time);
      closed->unidle_times++;
    }

    snprintf(tbuf1, BUFFER_LEN-1, "%ld %ld %d %d", (mudtime - closed->connected_at),
	 closed->idle_total , closed->unidle_times, closed->cmds);
    tbuf1[strlen(tbuf1)+1] = '\0';
    (void) atr_add(closed->player, "LASTACTIVITY", tbuf1, GOD, NOTHING);
    announce_disconnect(closed->player);
    mush_free(closed, "descriptor");
    closed = nextclosed;
  }

  strcpy(poll_msg, getstring_noalloc(f));
  globals.first_start_time = getref(f);
  globals.reboot_count = getref(f) + 1;
  DESC_ITER_CONN(d) {
    d->mailp = find_exact_starting_point(d->player);
  }
#ifdef HAS_OPENSSL
  if (SSLPORT) {
    sslsock = make_socket(SSLPORT, NULL, NULL, SSL_IP_ADDR);
    ssl_master_socket = ssl_setup_socket(sslsock);
    if (sslsock >= maxd)
      maxd = sslsock + 1;
  }
#endif

  fclose(f);
  remove(REBOOTFILE);
  flag_broadcast(0, 0, T("GAME: Reboot finished."));
}


/* Syntax: @snoop[/list] [!]<descriptor>
 */  
COMMAND(cmd_snoop) {
  DESC *d;
  int descn, on, n, sn;
  char snooplist[MAX_SNOOPS][BUFFER_LEN];
  char buf[BUFFER_LEN], *bp;

  if(SW_ISSET(sw, SWITCH_LIST)) {
	  descn = atoi(arg_left);

	  bp = buf;

	  d = port_desc(descn);

          if (LEVEL(player) <= 28) {
		  notify(player, MSG_HUH);
		  return;
	  }
	  /* make sure teh desc exists and they're connected (no spying on 'em at the connect screen!) */
	  if(!d || (d && !IsPlayer(d->player))) {
		  notify(player, "There is no one connected on that descriptor.");
		  return;
	  }

	  for(sn = 0, n = 0; n < MAX_SNOOPS; n++)
		  if(d->snooper[n] != -1) {
			  memset(snooplist[sn], '\0', BUFFER_LEN);
			  snprintf(snooplist[sn++], BUFFER_LEN-1, "%s", Name(d->snooper[n])); 
		  }
	  *snooplist[sn] = '\0';

	  for(n = 0; n < sn; n++) {
		  if(n != 0)
			  safe_str(", ", buf, &bp);
		  if(n == (sn-1) && sn > 1)
			  safe_str("& ", buf, &bp);
		  safe_str(snooplist[n], buf, &bp);
	  }
	  *bp = '\0';
	  notify_format(player, "%s is being snooped on by: %s", Name(d->player), buf);

  } else {
  if(*arg_left== '!') {
    on = 0;
    descn = atoi(arg_left+1);
  } else { 
    descn = atoi(arg_left);
    on = 1;
  }

  d = port_desc(descn);
  if (LEVEL(player) <= 28) {
    notify(player, MSG_HUH);
    return;
  }
  /* make sure teh desc exists and they're connected (no spying on 'em at the connect screen!) */
  if(!d || (d && !IsPlayer(d->player))) {
    notify(player, "There is no one connected on that descriptor.");
    return;
  }
  
  if(on) {
	  if((d->player == player)) {
		  notify(player, "You can't snoop yourself."); 
		  return;
	  }

  switch(set_snoop(player,d)) {
    case -1: /* Too Many */
      notify(player, "Sorry, can't snoop at this time.");
      return;
    case -2: /* Already Snoopin on 'em */
      notify(player, "You can only snoop one person at a time.");
      return;
    default:
      notify_format(player, T("Snoop now set on %s(%d)"), Name(d->player), descn); 
  }
  } else {
    for(on = n = 0; n < MAX_SNOOPS; n++)
      if(d->snooper[n] == player)  {
	d->snooper[n] = -1;
	on = 1;
      }
      notify(player, on ? "Snoop deactivated." : "Snooping Error.");
  }
  }
}


void feed_snoop(DESC *d, const char *s, char dir) {
  int n;
  char snstr[BUFFER_LEN];


  if(!d ||!d->connected)
    return;
  memset(snstr, '\0', BUFFER_LEN);
  strncpy(snstr, remove_markup(s, NULL), BUFFER_LEN-1);
  if(*s && !*snstr)
	  return;
  for(n = 0; n < MAX_SNOOPS ; n++)
    if(!IsPlayer(d->snooper[n]))
      continue;
    else if(GoodObject(d->snooper[n]) && IsPlayer(d->snooper[n])) {
      if(dir == 1) /* player see's */
        notify_format((dbref) d->snooper[n], T("%s%s<-%s %s"), object_header(d->snooper[n],d->player), ANSI_BLUE,
	    ANSI_NORMAL, snstr);
      else /* player types */
	notify_format((dbref) d->snooper[n], T("%s%s->%s %s%s"), object_header(d->snooper[n],d->player), 
	    ANSI_BLUE, ANSI_RED, snstr, ANSI_NORMAL);
    }

}

char is_snooped(DESC *d) {
  int n;

  for(n = 0; n < MAX_SNOOPS; n++) 
    if(IsPlayer(d->snooper[n]))
      return 1;
  return 0;
}


char set_snoop(dbref plyr, DESC *d) {
  int n;
  /* take first available spot */
  for( n = 0; n < MAX_SNOOPS ; n++)
    if(d->snooper[n] == -1)
      break;
    else if(d->snooper[n] == plyr)
      return -2; /* they're already snooping */
  if(n == MAX_SNOOPS-1) /* too many snoopers on player */
    return -1;
  d->snooper[n] = plyr;
  return 1;
}

void clr_snoop(dbref plyr, DESC *d) {
  int n;

  for(n = 0; n < MAX_SNOOPS; n++)
    if(d->snooper[n] == plyr)
      d->snooper[n] = -1;
}

/* switch users */
COMMAND(cmd_su) {
  DESC *d, *match = NULL;
  dbref target;
  int num = 0, is_hidden;

  /* Stage 1.  Make sure arg_left exists */
  if(arg_left && *arg_left) {
    if(!strcasecmp(cmd->name, "@SD")) { 
      target = match_result(player, arg_left, TYPE_DIVISION, MAT_EVERYTHING);
      if(!GoodObject(target) || Typeof(target) != TYPE_DIVISION) {
	notify(player, "No such division.");
	return;
      }
      /* Check to see if special conditions exist, where they can enter without a password */
      if(controls(player, target)) {   /* condition 1: they control it */
	/* trigger did_it on exiting division.. Possibly save their exiting values, for if they come back in? */
	did_it(player, Division(player), "SDOUT", NULL, NULL,  NULL, "ASDOUT", Location(player));
	add_to_div_exit_path(player, Division(player));
	Division(player) = target;
	/* triger did_it on incoming division.. to i guess set 'em for something.. *shrugs* */
	did_it(player, Division(player), "SDIN", tprintf("You have switched into Division: %s", object_header(player, Division(player)))
	      , NULL,  NULL, "ASDIN", Location(player));
	return;
      }
      /* get least idle desc */
      DESC_ITER_CONN(d) 
	if ((d->player == player) && (!match || (d->last_time > match->last_time))) 
	  match = d;
      /* We're only entering using a password at this moment */
	queue_newwrite(match, (unsigned char *) tprintf(T("Password: %c%c"),
							IAC, GOAHEAD), 13);
	if(!PromptConnection(match))
	  queue_newwrite(match, (unsigned char *) "\r\n", 3);

	match->input_handler = password_handler;
	match->pinfo.object = target;
	match->pinfo.function = &pw_div_connect;
	match->pinfo.lock = 0x40;
    } else {
      target = lookup_player(arg_left);
      if(target == NOTHING) {
	notify(player, "No such player.");
	return;
      }
      do_log(LT_WIZ, player, target, "** @SU ATTEMPT **");
      /* get least idle desc */
      DESC_ITER_CONN(d) 
	if ((d->player == player) && (!match || (d->last_time > match->last_time))) 
	  match = d;
  /* Step 2.  Find if we can get in without a pass, if
   * we need a pass. Throw them into password_handler() internal
   * prompt
   */
      if(div_powover(player, target, "@SU")) {
	do_log(LT_WIZ, player, target, "** @SU SUCCESS **");
	/* Phase 3a. Put Guy in user */
	add_to_exit_path(match, player);
	announce_disconnect(player);
	match->player = target;
	match->mailp = find_exact_starting_point(target);
	is_hidden = Can_Hide(target) && Dark(target);
	DESC_ITER_CONN(d)
	  if(d->player == player) {
	    num++;
	    if(is_hidden)
	      d->hide = 1;
	  }

	if(ModTime(target))
	  notify_format(target, T("%ld failed connections since last login."), ModTime(target));
	announce_connect(target, 0, num);
	check_last(target, match->addr, match->ip); /* set last, lastsite, give paycheck */
	queue_eol(match);
	if(command_check_byname(target, "@MAIL"))
	  check_mail(target, 0, 0);
	set_player_folder(target, 0);
	do_look_around(target);
	if(Haven(target))
	  notify(player, T("Your HAVEN flag is set.  You cannot receive pages."));
      } else {
	/* Part 3b.  Put guy in password program */
	queue_newwrite(match, (unsigned char *) tprintf(T("Password: %c%c"),
							IAC, GOAHEAD), 13);
	if(!PromptConnection(match))
	  queue_newwrite(match, (unsigned char *) "\r\n", 3);

	match->input_handler = password_handler;
	match->pinfo.object = target;
	match->pinfo.function = &pw_player_connect;
	match->pinfo.lock = 0x40;
      }
    }
  } else {
    if(SW_ISSET(sw, SWITCH_LOGOUT)) {
      /* @sd/logout - check to see if there is any division @sd'ing history.. And backtrack us */
      ATTR *divrcd;
      char tbuf[BUFFER_LEN], *p_buf[BUFFER_LEN / 2], *tbp, sep[2];
      int cnt;
      dbref div_obj;

      divrcd = atr_get(player, "XYXX_DIVRCD");
      if(divrcd == NULL) {
	notify(player, "You have not switched into any divisions.");
	return;
      }

      cnt = list2arr(p_buf, BUFFER_LEN / 2, safe_atr_value(divrcd), ' ');
      if(cnt < 1) {
	notify(player, "You have not switched into any divisions.");
	return;
      }

      /* Set them into cnt-1 if its good */
      div_obj = parse_number(p_buf[cnt-1]);
      if(GoodObject(div_obj) && IsDivision(div_obj)) {
	/* Trigger ASDOUT */
	did_it(player, Division(player), "SDOUT", NULL, NULL,  NULL, "ASDOUT", Location(player));
	Division(player) = div_obj;
	/* Trigger SDIN */
	did_it(player, Division(player), "SDIN", tprintf("You have went back to your other division: %s", object_header(player, div_obj))
	         , NULL,  NULL, "ASDIN", Location(player));
      }

      /* now  chop off last one, and arr2list() */
      if(cnt == 1) { /* clear the attribute */
	atr_clr(player, "XYXX_DIVRCD", GOD);
      } else {
	memset(tbuf, '\0', BUFFER_LEN);
	tbp = tbuf;
	sep[0] = ' ';
	sep[1] = '\0';
	arr2list(p_buf, cnt-1, tbuf, &tbp, sep);
	/* Add the attribute back */
	(void) atr_add(player, "XYXX_DIVRCD", tbuf, GOD, NOTHING);
      }
    } else {
      notify(player, "Must specify what player you wish to @su into.");
    }
  }
}

void add_to_exit_path(DESC *d, dbref player) {
  SU_PATH *path_entry;

  if(!d)
    return;

  path_entry = (SU_PATH *) mush_malloc(sizeof(SU_PATH), "SU_PATH_ENTRY");

  path_entry->player = player;
  if(d->su_exit_path)
    path_entry->next = d->su_exit_path;
  else
    path_entry->next = NULL;
  d->su_exit_path = path_entry;
}

/* If they're logged in.. Log 'em out through their su path */
static int do_su_exit(DESC *d) {
  DESC *c;
  SU_PATH *path_entry, *mark_path;
  int is_hidden, num = 0;

  if(d->su_exit_path) {
    path_entry = d->su_exit_path;
    while(path_entry)
      if(GoodObject(path_entry->player) && IsPlayer(path_entry->player))
	break;
      else { /* Guess the guy got nuked along the way..  free the spot */
	mark_path = path_entry;
	path_entry = path_entry->next;
	mush_free(mark_path, "SU_PATH_ENTRY");
      }
    d->su_exit_path = path_entry;
    if(!path_entry)
      return 0;
    /* Disconnect appearance */
    announce_disconnect(d->player);
    d->player = path_entry->player;
    /* Clear path_entry spot */
    d->su_exit_path = path_entry->next;
    mush_free(path_entry, "SU_PATH_ENTRY");
    d->mailp = find_exact_starting_point(d->player);
    is_hidden = Can_Hide(d->player) && Dark(d->player);
    DESC_ITER_CONN(c)
      if(c->player == d->player) {
	num++;
	if(is_hidden)
	  c->hide = 1;
      }
    if(ModTime(d->player))
      notify_format(d->player, T("%ld failed connections since last login."), ModTime(d->player));
    announce_connect(d->player, 0, num);
    check_last(d->player, d->addr, d->ip); /* set last, lastsite, give paycheck */
    queue_eol(d);
    if(command_check_byname(d->player, "@MAIL"))
      check_mail(d->player, 0, 0);
    set_player_folder(d->player, 0);
    do_look_around(d->player);
    if(Haven(d->player))
      notify(d->player, T("Your HAVEN flag is set.  You cannot receive pages."));
    return 1;
  } else return 0;
}
