/**
 * \file info_slave.c
 *
 * \brief The information slave process.
 *
 * When running PennMUSH under Unix, a second process (info_slave) is
 * started and the server farms out DNS and ident lookups to the
 * info_slave, and reads responses from the info_slave asynchronously. 
 * Communication between server and slave is by means of a local socket.
 *
 */
#include "copyrite.h"
#include "config.h"

#ifdef WIN32
#error "info_slave is not currently supported on Windows"
#endif

#include <stdio.h>
#include <stdlib.h>
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#ifdef I_SYS_SOCKET
#include <sys/socket.h>
#endif
#ifdef I_NETINET_IN
#include <netinet/in.h>
#endif
#include <netdb.h>
#include <ctype.h>
#include <string.h>
#ifdef I_UNISTD
#include <unistd.h>
#endif
#include <sys/uio.h>

#include "conf.h"
#include "externs.h"
#include "ident.h"
#include "mysocket.h"
#include "confmagic.h"

/* Duplicate these, rather than trying to include strutil.o... */
/** Arguments for functions that call APPEND_TO_BUF */
#define APPEND_ARGS int len, blen, clen
/** Add string c to buffer buff of max length mlen */
#define APPEND_TO_BUF(mlen) \
  /* Trivial cases */  \
  if (c[0] == '\0') \
    return 0; \
  /* The array is at least two characters long here */ \
  if (c[1] == '\0') \
    return safe_chr(c[0], buff, bp); \
  len = strlen(c); \
  blen = *bp - buff; \
  if (blen > (mlen)) \
    return len; \
  if ((len + blen) <= (mlen)) \
    clen = len; \
  else \
    clen = (mlen) - blen; \
  memcpy(*bp, c, clen); \
  *bp += clen; \
  return len - clen

#ifdef SAFE_CHR_FUNCTION
int
safe_chr(char c, char *buf, char **bufp)
{
  /* adds a character to a string, being careful not to overflow buffer */

  if ((*bufp - buf >= BUFFER_LEN - 1))
    return 1;

  *(*bufp)++ = c;
  return 0;
}
#endif

int
safe_str(const char *c, char *buff, char **bp)
{
  /* copies a string into a buffer, making sure there's no overflow. */
  APPEND_ARGS;

  if (!c || !*c)
    return 0;

  APPEND_TO_BUF(BUFFER_LEN);
}

#undef APPEND_ARGS
#undef APPEND_TO_BUF


int
main(int argc, char *argv[])
{
  int mush;
  int port;
  int fd;
  union sockaddr_u local, remote;
  static char buf[BUFFER_LEN];  /* overkill */
  char *bp;
  int len, size;
  IDENT *ident_result;
  char host[NI_MAXHOST];
  char lport[NI_MAXSERV];
  int use_ident, use_dns, timeout;
  socklen_t llen, rlen;
  struct iovec dat[3];

  if (argc < 2) {
    fprintf(stderr, "info_slave needs a port number!\n");
    return EXIT_FAILURE;
  }
  port = atoi(argv[1]);
  use_ident = 1;
  if (argc >= 3) {
    /* The second argument is -1 if we don't want ident used.
     * Anything else is the timeout. Default is 5 seconds.
     */
    use_ident = atoi(argv[2]);
  } else
    use_ident = 5;

  if (argc >= 4) {
    /* The third argument is 1 to do DNS lookups, 0 to not. */
    use_dns = atoi(argv[3]);
  } else
    use_dns = 1;

#ifdef HAS_SOCKETPAIR
  mush = port;                  /* We inherit open file descriptions and sockets from parent */
#else
  mush = make_socket_conn("127.0.0.1", NULL, 0, port, NULL);
  if (mush == -1) {             /* Couldn't connect */
    fprintf(stderr, "Couldn't connect to mush!\n");
    return EXIT_FAILURE;
  }
#endif
  /* yes, we are _blocking_ */

  for (;;) {
    /* grab a request */
    /* First, the address size. */

    len = read(mush, &rlen, sizeof rlen);
    if (len < (int) sizeof rlen) {
      perror("info_slave reading remote size (Did the mush crash?)");
      return EXIT_FAILURE;
    }
    /* Now the first address and len of the second. */
    dat[0].iov_base = (char *) &remote.data;
    dat[0].iov_len = rlen;
    dat[1].iov_base = (char *) &llen;
    dat[1].iov_len = sizeof llen;
    size = rlen + sizeof llen;
    len = readv(mush, dat, 2);
    if (len < size) {
      perror("info_slave reading remote sockaddr and local size");
      return EXIT_FAILURE;
    }

    /* Now the second address and fd. */
    dat[0].iov_base = (char *) &local.data;
    dat[0].iov_len = llen;
    dat[1].iov_base = (char *) &fd;
    dat[1].iov_len = sizeof fd;
    size = llen + sizeof fd;
    len = readv(mush, dat, 2);
    if (len < size) {
      perror("info_slave reading local sockaddr and fd");
      return EXIT_FAILURE;
    }

    if (!fd)
      /* MUSH aborted query part way through or only wrote a partial
       * packet */
      continue;

    bp = buf;
    if (getnameinfo(&remote.addr, rlen, host, sizeof host, NULL, 0,
                    NI_NUMERICHOST | NI_NUMERICSERV) != 0)
      safe_str("An error occured", buf, &bp);
    else
      safe_str(host, buf, &bp);
    safe_chr('^', buf, &bp);
    if (getnameinfo(&local.addr, llen, NULL, 0, lport, sizeof lport,
                    NI_NUMERICHOST | NI_NUMERICSERV) != 0)
      safe_str("An error occured", buf, &bp);
    else
      safe_str(lport, buf, &bp);
    safe_chr('^', buf, &bp);

    if (use_ident > 0) {
      timeout = use_ident;
      ident_result =
        ident_query(&local.addr, llen, &remote.addr, rlen, &timeout);
      if (ident_result && ident_result->identifier) {
        safe_str(ident_result->identifier, buf, &bp);
        safe_chr('@', buf, &bp);
      }
      if (ident_result)
        ident_free(ident_result);
    }
    if (use_dns) {
      if (getnameinfo(&remote.addr, rlen, host, sizeof host, NULL, 0,
                      NI_NUMERICSERV) != 0) {
        safe_str("An error occured", buf, &bp);
      } else {
        safe_str(host, buf, &bp);
      }
    } else
      safe_str(host, buf, &bp);
    *bp = '\0';
    size = strlen(buf);
    dat[0].iov_base = (char *) &fd;
    dat[0].iov_len = sizeof fd;
    dat[1].iov_base = (char *) &size;
    dat[1].iov_len = sizeof size;
    dat[2].iov_base = buf;
    dat[2].iov_len = size;
    len = writev(mush, dat, 3);
    size = dat[0].iov_len + dat[1].iov_len + dat[2].iov_len;
    if (len < size) {
      perror("info_slave write packet");
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}
