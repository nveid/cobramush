/* transition.c
 *
 * This utility will aid in a transition from a CobraMUSH running in one
 * directory to one running in a different directory, without disconnecting
 * users.
 *
 * To use this:
 *   Shell:
 *     gcc -o transition transition.c -DTARGET="/path/to/new/game/directory"
 *     cp transition /path/to/old/game/directory
 *     cd /path/to/old/game/directory
 *     rm netmush
 *     ln -s transition netmush
 *   Game:
 *     @shutdown/reboot
 *   Shell:
 *     cp reboot.db /path/to/new/game/directory
 *     cp data/* /path/to/new/game/directory/data
 *   Make sure that the new game directory is ready to run.  This includes
 *   a working netmush link and a suitably matched mush.cnf.
 *   Shell:
 *     kill -USR1 pid_of_this_program  (use -USR2 to cancel, but first
 *       replace the netmush link with one to netmud)
 *   When the restart is complete, check the game to be sure that everything
 *   worked.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef TARGET
#error Compile with -DTARGET="/path/to/new/game/directory"
#endif

#define RDBF_SCREENSIZE 0x1
#define RDBF_TTYPE 0x2
#define RDBF_PUEBLO_CHECKSUM 0x4
#define RDBF_SU_EXIT_PATH 0x10000

#define BUFFER_LEN 1024

struct desc {
  struct desc *next;
  int fd;
};

int sock;
int maxd;
struct desc *dlist;
int signaled;
int start_new;

int load_reboot_db(void);
void signal_usr1(int);
void signal_usr2(int);
int getref(FILE *);
const char *getstring_noalloc(FILE *);
int safe_chr(char, char *, char **);
void tell_one_guy(int, const char *);
void tell_everyone(const char *);
void deal_with_everyone(void);

int main(int argc, char *argv[]) {
  struct desc *cur;

  signaled = 0;
  start_new = 0;
  signal(SIGUSR1, signal_usr1);
  signal(SIGUSR2, signal_usr2);

  if(!load_reboot_db()) {
    fprintf(stderr, "unable to load db\n");
    exit(1);
  }

  printf("MUSH Server Transition Utility\n\n");

  printf("sock = %d\nmaxd = %d\nfds =", sock, maxd);
  for(cur = dlist; cur; cur = cur->next)
    printf(" %d", cur->fd);
  printf("\n");

  printf("To trigger the action, kill -USR1 %d\n", getpid());
  printf("This will chdir to %s and run netmush mush.cnf\n", TARGET);
  printf("You will need to copy reboot.db into the new location for this to work\n");
  printf("To cancel the action and run netmush mush.cnf in the current\ndirectory, kill -USR2 %d\n", getpid());
  printf("Of course, you will want to make sure that netmush points to a MUSH binary\ninstead of to this program before you do that.\n");

  tell_everyone("GAME: Beginning transition to new server\n");

  while(!signaled)
    deal_with_everyone();

  if(start_new) {
    tell_everyone("GAME: Starting new server\n");
    chdir(TARGET);
  } else {
    tell_everyone("GAME: Operation cancelled.  The old server will be restarted.\n");
  }

  execl("netmush", "netmush", "mush.cnf", NULL);
}

void tell_one_guy(int fd, const char *str) {
  write(fd, str, strlen(str));
}

void tell_everyone(const char *str) {
  struct desc *cur;

  for(cur = dlist; cur; cur = cur->next)
    tell_one_guy(cur->fd, str);
}

void deal_with_everyone(void) {
  fd_set inset;
  struct timeval timeout;
  struct desc *cur;

  FD_ZERO(&inset);
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  for(cur = dlist; cur; cur = cur->next)
    FD_SET(cur->fd, &inset);

  select(maxd, &inset, NULL, NULL, &timeout);

  for(cur = dlist; cur; cur = cur->next)
    if(FD_ISSET(cur->fd, &inset))
      tell_one_guy(cur->fd, "GAME: Server transition in progress, input ignored\n");
}

void signal_usr1(int sig __attribute__ ((__unused__))) {
  signaled = 1;
  start_new = 1;
}

void signal_usr2(int sig __attribute__ ((__unused__))) {
  signaled = 1;
}

int load_reboot_db(void) {
  FILE *f;
  struct desc *cur;
  int val;
  char c;
  long flags = 0, connflags;
  int exit_path;

  f = fopen("reboot.db", "r");
  if (!f)
    return 0;

  c = getc(f);                  /* Skip the V */
  if (c == 'V') {
    flags = getref(f);
  } else {
    fclose(f);
    return 0;
  }

  maxd = 0;
  sock = getref(f);
  val = getref(f);
  if (val > maxd)
    maxd = val;

  dlist = NULL;
  while ((val = getref(f)) != 0) {
    cur = (struct desc *) malloc(sizeof(struct desc));

    cur->fd = val;
    getref(f); /* connected_at */
    getref(f); /* hide */
    getref(f); /* cmds */
    getref(f); /* idle_total */
    getref(f); /* unidle_times */
    getref(f); /* player */
    getref(f); /* last_time */
    getstring_noalloc(f); /* output_prefix */
    getstring_noalloc(f); /* output_suffix */
    getstring_noalloc(f); /* addr */
    getstring_noalloc(f); /* ip */
    getstring_noalloc(f); /* doing */
    connflags = getref(f);
    if(flags & RDBF_SCREENSIZE) {
      getref(f); /* width */
      getref(f); /* height */
    }
    if(flags & RDBF_TTYPE)
      getstring_noalloc(f); /* ttype */
    if(flags & RDBF_PUEBLO_CHECKSUM)
      getstring_noalloc(f); /* checksum */
    if(flags & RDBF_SU_EXIT_PATH)
      for(exit_path = getref(f); exit_path >= 0; exit_path = getref(f))
        ;

    cur->next = dlist;
    dlist = cur;
  }

  fclose(f);

  return 1;
}

int getref(FILE *f) {
  static char buf[BUFFER_LEN];
  if (!fgets(buf, sizeof(buf), f)) {
    fprintf(stderr, "unexpected eof\n");
    exit(1);
  }
  return strtol(buf, NULL, 10);
}

const char *getstring_noalloc(FILE *f) {
  static char buf[BUFFER_LEN];
  char *p;
  int c;

  p = buf;
  c = fgetc(f);
  if (c == EOF) {
    fprintf(stderr, "unexpected eof\n");
    exit(1);
  } else if (c != '"') {
    for (;;) {
      if ((c == '\0') || (c == EOF) ||
          ((c == '\n') && ((p == buf) || (p[-1] != '\r')))) {
        *p = '\0';
        return buf;
      }
      safe_chr(c, buf, &p);
      c = fgetc(f);
    }
  } else {
    for (;;) {
      c = fgetc(f);
      if (c == '"') {
        /* It's a closing quote if it's followed by \r or \n */
        c = fgetc(f);
        if (c == '\r') {
          /* Get a possible \n, too */
          if ((c = fgetc(f)) != '\n')
            ungetc(c, f);
        } else if (c != '\n')
          ungetc(c, f);
        *p = '\0';
        return buf;
      } else if (c == '\\') {
        c = fgetc(f);
      }
      if ((c == '\0') || (c == EOF)) {
        *p = '\0';
        return buf;
      }
      safe_chr(c, buf, &p);
    }
  }
}

int safe_chr(char c, char *buf, char **bufp) {
  /* adds a character to a string, being careful not to overflow buffer */

  if ((*bufp - buf >= BUFFER_LEN - 1))
    return 1;

  *(*bufp)++ = c;
  return 0;
}

