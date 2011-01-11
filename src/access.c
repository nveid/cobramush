/**
 * \file
 *
 * \brief Access control lists for PennMUSH.
 * \verbatim
 *
 * The file access.cnf in the game directory will control all 
 * access-related directives, replacing lockout.cnf and sites.cnf
 *
 * The format of entries in the file will be:
 *
 * wild-host-name    [!]option [!]option [!]option ... # comment
 *
 * A wild-host-name is a wildcard pattern to match hostnames with. 
 * The wildcard "*" will work like UNIX filename globbing, so
 * *.edu will match all sites with names ending in .edu, and
 * *.*.*.*.* will match all sites with 4 periods in their name.
 * 128.32.*.* will match all sites starting with 128.32 (UC Berkeley).
 * You can also use user@host to match specific users if you know that
 * the host is running ident and you trust its responses (nontrivial).
 *
 * The options that can be specified are:
 * *CONNECT              Allow connections to non-guest players
 * *GUEST                Allow connection to guests
 * *CREATE               Allow player creation at login screen
 * DEFAULT               All of the above
 * NONE                 None of the above
 * SUSPECT              Set all players connecting from the site suspect
 * REGISTER             Allow players to use the "register" connect command
 * DENY_SILENT          Don't log when someone's denied access from here
 * REGEXP               Treat the hostname pattern as a regular expression
 * *GOD                  God can connect from this pattern.
 * *DIRECTOR             Directors can connect from this pattern.
 * *ADMIN                Admins can connect from this pattern.
 *
 * Options that are *'d can be prefaced by a !, meaning "Don't allow".
 *
 * The file is parsed line-by-line in order. This makes it possible
 * to explicitly allow only certain sites to connect and deny all others,
 * or vice versa. Sites can only do the options that are specified
 * in the first line they match.
 *
 * If a site is listed in the file with no options at all, it is
 * disallowed from any access (treated as !CONNECT, basically)
 *
 * If a site doesn't match any line in the file, it is allowed any
 * toggleable access (treated as DEFAULT) but isn't SUSPECT or REGISTER.
 *
 * "make access" produces access.cnf from lockout.cnf/sites.cnf
 *
 * @sitelock'd sites appear after the line "@sitelock" in the file
 * Using @sitelock writes out the file.
 * 
 * \endverbatim
 */

#include "config.h"
#include "copyrite.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#include <fcntl.h>
#ifdef I_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif
#ifdef I_UNISTD
#include <unistd.h>
#endif
#include "conf.h"
#include "externs.h"
#include "access.h"
#include "mymalloc.h"
#include "match.h"
#include "parse.h"
#include "log.h"
#include "mushdb.h"
#include "dbdefs.h"
#include "flags.h"
#include "confmagic.h"


/** An access flag. */
typedef struct a_acsflag acsflag;
/** An access flag.
 * This structure is used to build a table of access control flags.
 */
struct a_acsflag {
  const char *name;             /**< Name of the access flag */
  int toggle;                   /**< Is this a negatable flag? */
  int flag;                     /**< Bitmask of the flag */
};
static acsflag acslist[] = {
  {"connect", 1, ACS_CONNECT},
  {"create", 1, ACS_CREATE},
  {"guest", 1, ACS_GUEST},
  {"default", 0, ACS_DEFAULT},
  {"register", 0, ACS_REGISTER},
  {"suspect", 0, ACS_SUSPECT},
  {"deny_silent", 0, ACS_DENY_SILENT},
  {"regexp", 0, ACS_REGEXP},
  {"god", 1, ACS_GOD},
  {"director", 1, ACS_DIRECTOR},
  {"admin", 1, ACS_ADMIN},
  {NULL, 0, 0}
};

static struct access *access_top;
static int add_access_node
  (const char *host, const dbref who, const int can, const int cant,
   const char *comment);
static void free_access_list(void);

static int
add_access_node(const char *host, const dbref who, const int can,
                const int cant, const char *comment)
{
  struct access *end;
  struct access *tmp;

  tmp = (struct access *) mush_malloc(sizeof(struct access), "struct_access");
  if (!tmp)
    return 0;
  tmp->who = who;
  tmp->can = can;
  tmp->cant = cant;
  strcpy(tmp->host, host);
  if (comment)
    strcpy(tmp->comment, comment);
  else
    tmp->comment[0] = '\0';
  tmp->next = NULL;

  if (!access_top) {
    /* Add to the beginning */
    access_top = tmp;
  } else {
    end = access_top;
    while (end->next)
      end = end->next;
    end->next = tmp;
  }

  return 1;
}


/** Read the access.cnf file.
 * Initialize the access rules linked list and read in the access.cnf file.
 * Return 1 if successful, 0 if not
 */
int
read_access_file(void)
{
  FILE *fp;
  char buf[BUFFER_LEN];
  char *p;
  int can, cant;
  int retval;
  dbref who;
  char *comment;

  if (access_top) {
    /* We're reloading the file, so we've got to delete any current 
     * entries
     */
    free_access_list();
  }
  access_top = NULL;
  /* Be sure we have a file descriptor */
  release_fd();
  fp = fopen(ACCESS_FILE, FOPEN_READ);
  if (!fp) {
    do_log(LT_ERR, GOD, GOD, T("No %s file found."), ACCESS_FILE);
    retval = 0;
  } else {
    do_rawlog(LT_ERR, "Reading %s", ACCESS_FILE);
    while (fgets(buf, BUFFER_LEN, fp)) {
      /* Strip end of line if it's \r\n or \n */
      if ((p = strchr(buf, '\r')))
        *p = '\0';
      else if ((p = strchr(buf, '\n')))
        *p = '\0';
      /* Find beginning of line; ignore blank lines */
      p = buf;
      if (*p && isspace((unsigned char) *p))
        p++;
      if (*p && *p != '#') {
        can = cant = 0;
        comment = NULL;
        /* Is this the @sitelock entry? */
        if (!strncasecmp(p, "@sitelock", 9)) {
          if (!add_access_node("@sitelock", AMBIGUOUS, ACS_SITELOCK, 0, ""))
            do_log(LT_ERR, GOD, GOD, T("Failed to add sitelock node!"));
        } else {
          if ((comment = strchr(p, '#'))) {
            *comment++ = '\0';
            while (*comment && isspace((unsigned char) *comment))
              comment++;
          }
          /* Move past the host name */
          while (*p && !isspace((unsigned char) *p))
            p++;
          if (*p)
            *p++ = '\0';
          if (!parse_access_options(p, &who, &can, &cant, NOTHING))
            /* Nothing listed, so assume we can't do anything! */
            cant = ACS_DEFAULT;
          if (!add_access_node(buf, who, can, cant, comment))
            do_log(LT_ERR, GOD, GOD, T("Failed to add access node!"));
        }
      }
    }
    retval = 1;
    fclose(fp);
  }
  reserve_fd();
  return retval;
}

/** Write the access.cnf file.
 * Writes out the access.cnf file from the linked list
 */
void
write_access_file(void)
{
  FILE *fp;
  char tmpf[BUFFER_LEN];
  struct access *ap;
  acsflag *c;

  sprintf(tmpf, "%s.tmp", ACCESS_FILE);
  /* Be sure we have a file descriptor */
  release_fd();
  fp = fopen(tmpf, FOPEN_WRITE);
  if (!fp) {
    do_log(LT_ERR, GOD, GOD, T("Unable to open %s."), tmpf);
  } else {
    for (ap = access_top; ap; ap = ap->next) {
      if (strcmp(ap->host, "@sitelock") == 0) {
        fprintf(fp, "@sitelock\n");
        continue;
      }
      fprintf(fp, "%s %d ", ap->host, ap->who);
      switch (ap->can) {
      case ACS_SITELOCK:
        break;
      case ACS_DEFAULT:
        fprintf(fp, "DEFAULT ");
        break;
      default:
        for (c = acslist; c->name; c++)
          if (ap->can & c->flag)
            fprintf(fp, "%s ", c->name);
        break;
      }
      switch (ap->cant) {
      case ACS_DEFAULT:
        fprintf(fp, "NONE ");
        break;
      default:
        for (c = acslist; c->name; c++)
          if (c->toggle && (ap->cant & c->flag))
            fprintf(fp, "!%s ", c->name);
        break;
      }
      if (ap->comment && *ap->comment)
        fprintf(fp, "# %s\n", ap->comment);
      else
        fprintf(fp, "\n");
    }
    fclose(fp);
#ifdef WIN32
    unlink(ACCESS_FILE);
#endif
    rename(tmpf, ACCESS_FILE);
  }
  reserve_fd();
  return;
}

#ifdef FORCE_IPV4
static char *
ip4_to_ip6(const char *addr)
{
  static char tbuf1[BUFFER_LEN];
  char *bp;
  bp = tbuf1;
  safe_format(tbuf1, &bp, "::ffff:%s", addr);
  *bp = '\0';
  return tbuf1;
}
#endif

/** Decide if a host can access someway.
 * \param hname a host or user+host pattern.
 * \param flag the access type we're testing.
 * \param who the player attempting access.
 * \retval 1 access permitted.
 * \retval 0 access denied.
 * \verbatim
 * Given a hostname and a flag decide if the host can do it.
 * Here's how it works:
 * We run the linked list and take the first match.
 *  (If the hostname is user@host, we try to match both user@host
 *   and just host to each line in the file.)
 * If we make a match, and the line tells us whether the site can/can't
 *   do the action, we're done.
 * Otherwise, we assume that the host can do any toggleable option
 *   (can create, connect, guest), and don't have any special
 *   flags (can't register, isn't suspect)
 * \endverbatim
 */
int
site_can_access(const char *hname, int flag, dbref who)
{
  struct access *ap;
  acsflag *c;
  char *p;

  if (!hname || !*hname)
    return 0;

  if ((p = strchr(hname, '@')))
    p++;

  for (ap = access_top; ap; ap = ap->next) {
    if (!(ap->can & ACS_SITELOCK)
        && ((ap->can & ACS_REGEXP)
            ? (regexp_match_case(ap->host, hname, 0)
               || (p && regexp_match_case(ap->host, p, 0))
#ifdef FORCE_IPV4
               || regexp_match_case(ip4_to_ip6(ap->host), hname, 0)
               || (p && regexp_match_case(ip4_to_ip6(ap->host), p, 0))
#endif
            )
            : (quick_wild(ap->host, hname)
               || (p && quick_wild(ap->host, p))
#ifdef FORCE_IPV4
               || quick_wild(ip4_to_ip6(ap->host), hname)
               || (p && quick_wild(ip4_to_ip6(ap->host), p))
#endif
            ))
        && (ap->who == AMBIGUOUS || ap->who == who)) {
      /* Got one */
      if (flag & ACS_CONNECT) {
        if ((ap->cant & ACS_GOD) && God(who))   /* God can't connect from here */
          return 0;
        else if ((ap->cant & ACS_DIRECTOR) && Director(who))
          /* Directors can't connect from here */
          return 0;
        else if ((ap->cant & ACS_ADMIN) && Admin(who))
          /* Admins can't connect from here */
          return 0;
      }
      if (ap->cant && ((ap->cant & flag) == flag))
        return 0;
      if (ap->can && (ap->can & flag))
        return 1;

      /* Hmm. We don't know if we can or not, so continue */
      break;
    }
  }

  /* Flag was neither set nor unset. If the flag was a toggle,
   * then the host can do it. If not, the host can't */
  for (c = acslist; c->name; c++) {
    if (flag & c->flag)
      return c->toggle ? 1 : 0;
  }
  /* Should never reach here, but just in case */
  return 1;
}


/** Return the first access rule that matches a host.
 * \param hname a host or user+host pattern.
 * \param who the player attempting access.
 * \param rulenum pointer to rule position.
 * \return pointer to first matching access rule or NULL.
 */
struct access *
site_check_access(const char *hname, dbref who, int *rulenum)
{
  struct access *ap;
  char *p;

  *rulenum = 0;
  if (!hname || !*hname)
    return 0;

  if ((p = strchr(hname, '@')))
    p++;

  for (ap = access_top; ap; ap = ap->next) {
    (*rulenum)++;
    if (!(ap->can & ACS_SITELOCK)
        && ((ap->can & ACS_REGEXP)
            ? (regexp_match_case(ap->host, hname, 0)
               || (p && regexp_match_case(ap->host, p, 0))
#ifdef FORCE_IPV4
               || regexp_match_case(ip4_to_ip6(ap->host), hname, 0)
               || (p && regexp_match_case(ip4_to_ip6(ap->host), p, 0))
#endif
            )
            : (quick_wild(ap->host, hname)
               || (p && quick_wild(ap->host, p))
#ifdef FORCE_IPV4
               || quick_wild(ip4_to_ip6(ap->host), hname)
               || (p && quick_wild(ip4_to_ip6(ap->host), p))
#endif
            ))
        && (ap->who == AMBIGUOUS || ap->who == who)) {
      /* Got one */
      return ap;
    }
  }
  return NULL;
}

/** Display an access rule.
 * \param ap pointer to access rule.
 * \param rulenum access rule's number in the list.
 * \param who unused.
 * \param buff buffer to store output.
 * \param bp pointer into buff.
 * This function provides an appealing display of an access rule
 * in the list.
 */
int
format_access(struct access *ap, int rulenum,
              dbref who __attribute__ ((__unused__)), char *buff, char **bp)
{
  if (ap) {
    safe_format(buff, bp, T("Matched line %d: %s %s"), rulenum, ap->host,
                (ap->can & ACS_REGEXP) ? "(regexp)" : "");
    safe_chr('\n', buff, bp);
    safe_format(buff, bp, T("Comment: %s"), ap->comment);
    safe_chr('\n', buff, bp);
    safe_str(T("Connections allowed by: "), buff, bp);
    if (ap->cant & ACS_CONNECT)
      safe_str(T("No one"), buff, bp);
    else if (ap->cant & ACS_ADMIN)
      safe_str(T("All but admin"), buff, bp);
    else if (ap->cant & ACS_DIRECTOR)
      safe_str(T("All but directors"), buff, bp);
    else if (ap->cant & ACS_GOD)
      safe_str(T("All but God"), buff, bp);
    else
      safe_str(T("All"), buff, bp);
    safe_chr('\n', buff, bp);
    if (ap->cant & ACS_GUEST)
      safe_str(T("Guest connections are NOT allowed"), buff, bp);
    else
      safe_str(T("Guest connections are allowed"), buff, bp);
    safe_chr('\n', buff, bp);
    if (ap->cant & ACS_CREATE)
      safe_str(T("Creation is NOT allowed"), buff, bp);
    else
      safe_str(T("Creation is allowed"), buff, bp);
    safe_chr('\n', buff, bp);
    if (ap->can & ACS_REGISTER)
      safe_str(T("Email registration is allowed"), buff, bp);
    if (ap->can & ACS_SUSPECT)
      safe_str(T("Players connecting are set SUSPECT"), buff, bp);
    if (ap->can & ACS_DENY_SILENT)
      safe_str(T("Denied connections are not logged"), buff, bp);
  } else {
    safe_str(T("No matching access rule"), buff, bp);
  }
  return 0;
}


/** Add an access rule to the linked list.
 * \param player enactor.
 * \param host host pattern to add.
 * \param who player to which rule applies, or AMBIGUOUS.
 * \param can flags of allowed actions.
 * \param cant flags of disallowed actions.
 * \retval 1 success.
 * \retval 0 failure.
 * \verbatim
 * This function adds an access rule after the @sitelock entry.
 * If there is no @sitelock entry, add one to the end of the list
 * and then add the entry.
 * Build an appropriate comment based on the player and date
 * \endverbatim
 */
int
add_access_sitelock(dbref player, const char *host, dbref who, int can,
                    int cant)
{
  struct access *end;
  struct access *tmp;

  tmp = (struct access *) mush_malloc(sizeof(struct access), "struct_access");
  if (!tmp)
    return 0;

  tmp->who = who;
  tmp->can = can;
  tmp->cant = cant;
  strcpy(tmp->host, host);
  sprintf(tmp->comment, "By %s(#%d) on %s", Name(player), player,
          show_time(mudtime, 0));
  tmp->next = NULL;

  if (!access_top) {
    /* Add to the beginning, but first add a sitelock marker */
    if (!add_access_node("@sitelock", AMBIGUOUS, ACS_SITELOCK, 0, ""))
      return 0;
    access_top->next = tmp;
  } else {
    end = access_top;
    while (end->next && end->can != ACS_SITELOCK)
      end = end->next;
    /* Now, either we're at the sitelock or the end */
    if (end->can != ACS_SITELOCK) {
      /* We're at the end and there's no sitelock marker. Add one */
      if (!add_access_node("@sitelock", AMBIGUOUS, ACS_SITELOCK, 0, ""))
        return 0;
      end = end->next;
    } else {
      /* We're in the middle, so be sure we keep the list linked */
      tmp->next = end->next;
    }
    end->next = tmp;
  }
  return 1;
}

/** Remove an access rule from the linked list.
 * \param pattern access rule host pattern to match.
 * \return number of rule removed.
 * \verbatim
 * This function removes an access rule from the list.
 * Only rules that appear after the "@sitelock" rule can be
 * removed with this function.
 * \endverbatim
 */
int
remove_access_sitelock(const char *pattern)
{
  struct access *ap, *next, *prev = NULL;
  int n = 0;

  /* We only want to be able to delete entries added with @sitelock */
  for (ap = access_top; ap; ap = ap->next)
    if (strcmp(ap->host, "@sitelock") == 0) {
      prev = ap;
      ap = ap->next;
      break;
    }

  while (ap) {
    next = ap->next;
    if (strcasecmp(pattern, ap->host) == 0) {
      n++;
      mush_free(ap, "struct_access");
      if (prev)
        prev->next = next;
      else
        access_top = next;
    } else {
      prev = ap;
    }
    ap = next;
  }

  return n;
}

/* Free the entire access list */
static void
free_access_list()
{
  struct access *ap, *next;
  ap = access_top;
  while (ap) {
    next = ap->next;
    mush_free((Malloc_t) ap, "struct_access");
    ap = next;
  }
  access_top = NULL;
}


/** Display the access list.
 * \param player enactor.
 * Sends the complete access list to the player.
 */
void
do_list_access(dbref player)
{
  struct access *ap;
  acsflag *c;
  char flaglist[BUFFER_LEN];
  int rulenum = 0;
  char *bp;

  for (ap = access_top; ap; ap = ap->next) {
    rulenum++;
    if (ap->can != ACS_SITELOCK) {
      bp = flaglist;
      for (c = acslist; c->name; c++) {
        if (c->flag == ACS_DEFAULT)
          continue;
        if (ap->can & c->flag) {
          safe_chr(' ', flaglist, &bp);
          safe_str(c->name, flaglist, &bp);
        }
        if (c->toggle && (ap->cant & c->flag)) {
          safe_chr(' ', flaglist, &bp);
          safe_chr('!', flaglist, &bp);
          safe_str(c->name, flaglist, &bp);
        }
      }
      *bp = '\0';
      notify_format(player,
                    "%3d SITE: %-20s  DBREF: %-6s FLAGS:%s", rulenum,
                    ap->host, unparse_dbref(ap->who), flaglist);
      notify_format(player, " COMMENT: %s", ap->comment);
    } else {
      notify(player,
             T
             ("---- @sitelock will add sites immediately below this line ----"));
    }

  }
}

/** Parse access options into fields.
 * \param opts access options to read from.
 * \param who pointer to player to whom rule applies, or AMBIGUOUS.
 * \param can pointer to flags of allowed actions.
 * \param cant pointer to flags of disallowed actions.
 * \param player enactor.
 * \return number of options successfully parsed.
 * Parse options and return the appropriate can and cant bits.
 * Return the number of options successfully parsed.
 * This makes a copy of the options string, so it's not modified.
 */
int
parse_access_options(const char *opts, dbref *who, int *can, int *cant,
                     dbref player)
{
  char myopts[BUFFER_LEN];
  char *p;
  char *w;
  acsflag *c;
  int found, totalfound, first;

  if (!opts || !*opts)
    return 0;
  strcpy(myopts, opts);
  totalfound = 0;
  first = 1;
  if (who)
    *who = AMBIGUOUS;
  p = trim_space_sep(myopts, ' ');
  while ((w = split_token(&p, ' '))) {
    found = 0;

    if (first && who) {         /* Check for a character */
      first = 0;
      if (is_integer(w)) {      /* We have a dbref */
        *who = parse_integer(w);
        if (*who != AMBIGUOUS && !GoodObject(*who))
          *who = AMBIGUOUS;
        continue;
      }
    }

    if (*w == '!') {
      /* Found a negated warning */
      w++;
      for (c = acslist; c->name; c++) {
        if (c->toggle && !strncasecmp(w, c->name, strlen(c->name))) {
          *cant |= c->flag;
          found++;
        }
      }
    } else {
      /* None is special */
      if (!strncasecmp(w, "NONE", 4)) {
        *cant = ACS_DEFAULT;
        found++;
      } else {
        for (c = acslist; c->name; c++) {
          if (!strncasecmp(w, c->name, strlen(c->name))) {
            *can |= c->flag;
            found++;
          }
        }
      }
    }
    /* At this point, we haven't matched any warnings. */
    if (!found) {
      if (GoodObject(player))
        notify_format(player, T("Unknown access option: %s"), w);
      else
        do_log(LT_ERR, GOD, GOD, T("Unknown access flag: %s"), w);
    } else {
      totalfound += found;
    }
  }
  return totalfound;
}
