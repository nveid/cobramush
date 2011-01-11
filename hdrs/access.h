#ifndef __ACCESS_H
#define __ACCESS_H

/** Access information for a host-pattern.
 * This structure holds access information for a given host-pattern.
 * It's organized into a linked list of access rules.
 */
struct access {
  char host[BUFFER_LEN];        /**< The host pattern */
  char comment[BUFFER_LEN];     /**< A comment about the rule */
  dbref who;                    /**< Who created this rule if sitelock used */
  int can;                      /**< Bitflags of what the host can do */
  int cant;                     /**< Bitflags of what the host can't do */
  struct access *next;          /**< Pointer to next rule in the list */
};


/* These flags are can/can't - a site may or may not be allowed to do them */
#define ACS_CONNECT     0x1     /* Connect to non-guests */
#define ACS_CREATE      0x2     /* Create new players */
#define ACS_GUEST       0x4     /* Connect to guests */
#define ACS_REGISTER    0x8     /* Site can use the 'register' command */
/* These flags are set in the 'can' bit, but they mark special processing */
#define ACS_SITELOCK    0x10    /* Marker for where to insert @sitelock */
#define ACS_SUSPECT     0x20    /* All players from this site get SUSPECT */
#define ACS_DENY_SILENT 0x40    /* Don't log failed attempts */
#define ACS_REGEXP      0x80    /* Treat the host pattern as a regexp */

#define ACS_GOD         0x100   /* God can connect from this site */
#define ACS_DIRECTOR    0x200   /* Directors can connect from this site */
#define ACS_ADMIN       0x400   /* Admins can connect from this site */

/* This is the usual default access */
#define ACS_DEFAULT             (ACS_CONNECT|ACS_CREATE|ACS_GUEST)

/* Usefile macros */

#define Site_Can_Connect(hname, who)  site_can_access(hname,ACS_CONNECT, who)
#define Site_Can_Create(hname)  site_can_access(hname,ACS_CREATE, AMBIGUOUS)
#define Site_Can_Guest(hname)  site_can_access(hname,ACS_GUEST, AMBIGUOUS)
#define Site_Can_Register(hname)  site_can_access(hname,ACS_REGISTER, AMBIGUOUS)
#define Deny_Silent_Site(hname, who) site_can_access(hname,ACS_DENY_SILENT, who)
#define Suspect_Site(hname, who)  site_can_access(hname,ACS_SUSPECT, who)
#define Forbidden_Site(hname)  (!site_can_access(hname,ACS_DEFAULT, AMBIGUOUS))

/* Public functions */
int read_access_file(void);
void write_access_file(void);
int site_can_access(const char *hname, int flag, dbref who);
struct access *site_check_access(const char *hname, dbref who, int *rulenum);
int format_access(struct access *ap, int rulenum,
                  dbref who
                  __attribute__ ((__unused__)), char *buff, char **bp);
int add_access_sitelock(dbref player, const char *host, dbref who, int can,
                        int cant);
int remove_access_sitelock(const char *pattern);
void do_list_access(dbref player);
int parse_access_options
  (const char *opts, dbref *who, int *can, int *cant, dbref player);

#endif                          /* __ACCESS_H */
