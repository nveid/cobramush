/**
 * \file ident.c
 *
 * \brief High-level calls to the ident library.
 *
 * Author: Pär Emanuelsson <pell@lysator.liu.se>
 * Hacked by: Peter Eriksson <pen@lysator.liu.se>
 * 
 * Many changes by Shawn Wagner to be protocol independent
 * for PennMUSH
 */

#include "config.h"
#ifdef NeXT3
#include <libc.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#include <time.h>
#ifdef I_SYS_TIME
#include <sys/time.h>
#endif
#ifdef I_SYS_WAIT
#include <sys/wait.h>
#endif
#include <errno.h>
#ifndef WIN32
#ifdef I_SYS_SOCKET
#include <sys/socket.h>
#endif
#ifdef I_SYS_FILE
#include <sys/file.h>
#endif
#ifdef I_NETINET_IN
#include <netinet/in.h>
#else
#ifdef I_SYS_IN
#include <sys/in.h>
#endif
#endif
#ifdef I_ARPA_INET
#include <arpa/inet.h>
#endif
#include <netdb.h>
#endif                          /* WIN32 */

#ifdef I_UNISTD
#include <unistd.h>
#endif

#include "conf.h"
#include "externs.h"
#include "attrib.h"
#include "ident.h"
#include "mymalloc.h"
#include "mysocket.h"
#include "confmagic.h"

  /* Low-level calls and macros */

/** Structure to track an ident connection. */
typedef struct {
  int fd;               /**< file descriptor to read from. */
  char buf[IDBUFSIZE];  /**< buffer to hold ident data. */
} ident_t;


static ident_t *id_open(struct sockaddr *faddr,
                        socklen_t flen,
                        struct sockaddr *laddr, socklen_t llen, int *timeout);

static int id_query(ident_t * id,
                    struct sockaddr *laddr,
                    socklen_t llen,
                    struct sockaddr *faddr, socklen_t flen, int *timeout);

static int id_close(ident_t * id);

static int id_parse(ident_t * id, int *timeout, IDENT **ident);
static IDENT *ident_lookup(int fd, int *timeout);


/* Do a complete ident query and return result */
static IDENT *
ident_lookup(int fd, int *timeout)
{
  union sockaddr_u localaddr, remoteaddr;
  socklen_t llen, rlen, len;

  len = sizeof(remoteaddr);
  if (getpeername(fd, (struct sockaddr *) remoteaddr.data, &len) < 0)
    return 0;
  llen = len;

  len = sizeof(localaddr);
  if (getsockname(fd, (struct sockaddr *) localaddr.data, &len) < 0)
    return 0;
  rlen = len;

  return ident_query(&localaddr.addr, llen, &remoteaddr.addr, rlen, timeout);
}

/** Perform an ident query and return the result.
 * \param laddr local socket address data.
 * \param llen local socket address data length.
 * \param raddr remote socket address data.
 * \param rlen remote socket address data length.
 * \param timeout pointer to timeout value for query.
 * \return ident responses in IDENT pointer, or NULL.
 */
IDENT *
ident_query(struct sockaddr *laddr, socklen_t llen,
            struct sockaddr *raddr, socklen_t rlen, int *timeout)
{
  int res;
  ident_t *id;
  IDENT *ident = 0;

  if (timeout && *timeout < 0)
    *timeout = 0;

  id = id_open(raddr, rlen, laddr, llen, timeout);

  if (!id) {
#ifndef WIN32
    errno = EINVAL;
#endif
#ifdef DEBUG
    fprintf(stderr, "id_open failed.\n");
#endif
    return 0;
  }

  res = id_query(id, raddr, rlen, laddr, llen, timeout);

  if (res < 0) {
    id_close(id);
#ifdef DEBUG
    fprintf(stderr, "id_query failed.\n");
#endif
    return 0;
  }

  res = id_parse(id, timeout, &ident);

  if (res != 1) {
    id_close(id);
#ifdef DEBUG
    fprintf(stderr, "id_parse failed.\n");
#endif
    return 0;
  }
  id_close(id);

  return ident;                 /* At last! */
}

/** Perform an ident lookup and return the remote identifier as a
 * newly allocated string. This function allocates memory that
 * should be freed by the caller.
 * \param fd socket to use for ident lookup.
 * \param timeout pointer to timeout value for lookup.
 * \return allocated string containing identifier, or NULL.
 */
char *
ident_id(int fd, int *timeout)
{
  IDENT *ident;
  char *id = NULL;
  if (timeout && *timeout < 0)
    *timeout = 0;
  ident = ident_lookup(fd, timeout);
  if (ident && ident->identifier && *ident->identifier)
    id = strdup(ident->identifier);
  ident_free(ident);
  return id;
}

/** Free an IDENT structure and all elements.
 * \param id pointer to IDENT structure to free.
 */
void
ident_free(IDENT *id)
{
  if (!id)
    return;
  if (id->identifier)
    free(id->identifier);
  if (id->opsys)
    free(id->opsys);
  if (id->charset)
    free(id->charset);
  free(id);
}

/* id_open.c Establish/initiate a connection to an IDENT server
   **
   ** Author: Peter Eriksson <pen@lysator.liu.se>
   ** Fixes: Pär Emanuelsson <pell@lysator.liu.se> */



static ident_t *
id_open(struct sockaddr *faddr, socklen_t flen,
        struct sockaddr *laddr, socklen_t llen, int *timeout)
{
  ident_t *id;
  char host[NI_MAXHOST];
  union sockaddr_u myinterface;
  fd_set rs, ws, es;
  struct timeval to;
  int res;
#ifndef WIN32
  int tmperrno;
#endif

  if ((id = (ident_t *) malloc(sizeof(*id))) == 0)
    return 0;

  memset(id, 0, sizeof(ident_t));

  if (getnameinfo(faddr, flen, host, sizeof(host), NULL, 0,
                  NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
    free(id);
    return 0;
  }

  /* Make sure we connect from the right interface. Changing the pointer
     directly doesn't seem to work. So... */
  memcpy(&myinterface, laddr, llen);
  if (myinterface.addr.sa_family == AF_INET)
    ((struct sockaddr_in *) &myinterface.addr)->sin_port = 0;
#ifdef HAS_IPV6                 /* Bleah, I wanted to avoid stuff like this */
  else if (myinterface.addr.sa_family == AF_INET6)
    ((struct sockaddr_in6 *) &myinterface.addr)->sin6_port = 0;
#endif

  id->fd = make_socket_conn(host, &myinterface.addr, llen, IDPORT, timeout);

  if (id->fd < 0)               /* Couldn't connect to an ident server */
    goto ERROR_BRANCH;

  if (timeout) {
    time_t now, after;

    FD_ZERO(&rs);
    FD_ZERO(&ws);
    FD_ZERO(&es);
    FD_SET(id->fd, &rs);
    FD_SET(id->fd, &ws);
    FD_SET(id->fd, &es);
    to.tv_sec = *timeout;
    to.tv_usec = 0;
    now = time(NULL);
    if ((res = select(id->fd + 1, &rs, &ws, &es, &to)) < 0) {
#ifdef DEBUG
      perror("libident: select");
#endif
      goto ERROR_BRANCH;
    }
    after = time(NULL);
    *timeout -= after - now;
    *timeout = *timeout < 0 ? 0 : *timeout;

    if (res == 0) {
#ifndef WIN32
      errno = ETIMEDOUT;
#endif
      goto ERROR_BRANCH;
    }
    if (FD_ISSET(id->fd, &es))
      goto ERROR_BRANCH;

    if (!FD_ISSET(id->fd, &rs) && !FD_ISSET(id->fd, &ws))
      goto ERROR_BRANCH;
  }
  return id;

ERROR_BRANCH:
#ifndef WIN32
  tmperrno = errno;             /* Save, so close() won't erase it */
#endif
  closesocket(id->fd);
  free(id);
#ifndef WIN32
  errno = tmperrno;
#endif
  return 0;
}


/* id_close.c Close a connection to an IDENT server
   **
   ** Author: Peter Eriksson <pen@lysator.liu.se> */

static int
id_close(ident_t * id)
{
  int res;

  res = closesocket(id->fd);
  free(id);

  return res;
}


/* id_query.c Transmit a query to an IDENT server
   **
   ** Author: Peter Eriksson <pen@lysator.liu.se>
   ** Slight modifications by Alan Schwartz */


static int
id_query(ident_t * id, struct sockaddr *laddr, socklen_t llen,
         struct sockaddr *faddr, socklen_t flen, int *timeout)
{
  int res;
  char buf[80];
  char port[NI_MAXSERV];
  fd_set ws;
  struct timeval to;

  getnameinfo(laddr, llen, NULL, 0, port, sizeof(port),
              NI_NUMERICHOST | NI_NUMERICSERV);
  sprintf(buf, "%s , ", port);
  getnameinfo(faddr, flen, NULL, 0, port, sizeof(port),
              NI_NUMERICHOST | NI_NUMERICSERV);
  strncat(buf, port, sizeof(buf));
  strncat(buf, "\r\n", sizeof(buf));

  if (timeout) {
    time_t now, after;
    FD_ZERO(&ws);
    FD_SET(id->fd, &ws);
    to.tv_sec = *timeout;
    to.tv_usec = 0;
    now = time(NULL);
    if ((res = select(id->fd + 1, NULL, &ws, NULL, &to)) < 0)
      return -1;
    after = time(NULL);
    *timeout -= after - now;
    *timeout = *timeout < 0 ? 0 : *timeout;
    if (res == 0) {
#ifndef WIN32
      errno = ETIMEDOUT;
#endif
      return -1;
    }
  }
  /* Used to ignore SIGPIPE here, but we already ignore it anyways. */
  res = send(id->fd, buf, strlen(buf), 0);

  return res;
}


/* id_parse.c Receive and parse a reply from an IDENT server
   **
   ** Author: Peter Eriksson <pen@lysator.liu.se>
   ** Fiddling: Pär Emanuelsson <pell@lysator.liu.se> */

static char *
xstrtok(char *RESTRICT cp, const char *RESTRICT cs, char *RESTRICT dc)
{
  static char *bp = 0;

  if (cp)
    bp = cp;

  /*
   ** No delimitor cs - return whole buffer and point at end
   */
  if (!cs) {
    while (*bp)
      bp++;
    return NULL;
  }
  /*
   ** Skip leading spaces
   */
  while (isspace((unsigned char) *bp))
    bp++;

  /*
   ** No token found?
   */
  if (!*bp)
    return NULL;

  cp = bp;
  bp += strcspn(bp, cs);
  /*  while (*bp && !strchr(cs, *bp))
     bp++;
   */
  /* Remove trailing spaces */
  *dc = *bp;
  for (dc = bp - 1; dc > cp && isspace((unsigned char) *dc); dc--) ;
  *++dc = '\0';

  bp++;

  return cp;
}


static int
id_parse(ident_t * id, int *timeout, IDENT **ident)
{
  char c, *cp, *tmp_charset;
  fd_set rs;
  int res = 0, lp, fp;
  size_t pos;
  struct timeval to;

#ifndef WIN32
  errno = 0;
#endif

  tmp_charset = 0;

  if (!id || !ident)
    return -1;

  *ident = malloc(sizeof(IDENT));

  if (!*ident)
    return -1;

  memset(*ident, 0, sizeof(IDENT));

  pos = strlen(id->buf);

  if (timeout) {
    time_t now, after;
    FD_ZERO(&rs);
    FD_SET(id->fd, &rs);
    to.tv_sec = *timeout;
    to.tv_usec = 0;
    now = time(NULL);
    if ((res = select(id->fd + 1, &rs, NULL, NULL, &to)) < 0)
      return -1;
    after = time(NULL);
    *timeout -= after - now;
    *timeout = *timeout < 0 ? 0 : *timeout;
    if (res == 0) {
#ifndef WIN32
      errno = ETIMEDOUT;
#endif
      return -1;
    }
  }
  while (pos < sizeof(id->buf) &&
         (res = recv(id->fd, id->buf + pos, 1, 0)) == 1 && id->buf[pos] != '\n')
    pos++;
  if (res < 0)
    return -1;

  if (res == 0) {
#ifndef WIN32
    errno = ENOTCONN;
#endif
    return -1;
  }
  if (id->buf[pos] != '\n') {
    return 0;
  }
  id->buf[pos++] = '\0';

  /* Get first field (<lport> , <fport>) */
  cp = xstrtok(id->buf, ":", &c);
  if (!cp) {
    return -2;
  }

  if ((res = sscanf(cp, " %d , %d", &lp, &fp)) != 2) {
    (*ident)->identifier = strdup(cp);
    return -2;
  }
  /* Get second field (USERID or ERROR) */
  cp = xstrtok(NULL, ":", &c);
  if (!cp) {
    return -2;
  }
  if (strcmp(cp, "ERROR") == 0) {
    cp = xstrtok(NULL, "\n\r", &c);
    if (!cp)
      return -2;

    (*ident)->identifier = strdup(cp);

    return 2;
  } else if (strcmp(cp, "USERID") == 0) {
    /* Get first subfield of third field <opsys> */
    cp = xstrtok(NULL, ",:", &c);
    if (!cp) {
      return -2;
    }
    (*ident)->opsys = strdup(cp);

    /* We have a second subfield (<charset>) */
    if (c == ',') {
      cp = xstrtok(NULL, ":", &c);
      if (!cp)
        return -2;

      tmp_charset = cp;

      (*ident)->charset = strdup(cp);

      /* We have even more subfields - ignore them */
      if (c == ',')
        xstrtok(NULL, ":", &c);
    }
    if (tmp_charset && strcmp(tmp_charset, "OCTET") == 0)
      cp = xstrtok(NULL, NULL, &c);
    else
      cp = xstrtok(NULL, "\n\r", &c);

    (*ident)->identifier = strdup(cp);
    return 1;
  } else {
    (*ident)->identifier = strdup(cp);
    return -3;
  }
}
