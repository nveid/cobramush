#ifndef __CRON_H
#define __CRON_H

#include "conf.h"
#include "externs.h"
#include "command.h"

#define CRON_SPEC_SEP           ' '
#define CRON_NUM_SEP            ','
#define CRON_SKIP_SEP           '/'
#define CRON_RANGE_SEP          '-'
#define CRON_WILDCARD           '*'

#define CRON_MINUTE_MAX         59
#define CRON_HOUR_MAX           23
#define CRON_DAY_MAX            30
#define CRON_MONTH_MAX          11
#define CRON_WDAY_MAX           6

#define CRON_GLOBAL             -1

#define CRON_NAME_LEN           32
#define CRON_FORMAT_LEN         128

#define CF_COMMAND              0x1
#define CF_FUNCTION             0x2
#define CF_HALT                 0x4

#define CRON_Command(job)       ((job)->type & CF_COMMAND)
#define CRON_Function(job)      ((job)->type & CF_FUNCTION)
#define CRON_Halt(job)          ((job)->type & CF_HALT)

#define CF_DEFAULT              (0)

#define CM_JANUARY              0
#define CM_FEBRUARY             1
#define CM_MARCH                2
#define CM_APRIL                3
#define CM_MAY                  4
#define CM_JUNE                 5
#define CM_JULY                 6
#define CM_AUGUST               7
#define CM_SEPTEMBER            8
#define CM_OCTOBER              9
#define CM_NOVEMBER             10
#define CM_DECEMBER             11

#define CD_SUNDAY               0
#define CD_MONDAY               1
#define CD_TUESDAY              2
#define CD_WEDNESDAY            3
#define CD_THURSDAY             4
#define CD_FRIDAY               5
#define CD_SATURDAY             6

typedef struct named_value NVALUE;
typedef struct named_value_alias NVALUE_ALIAS;
typedef struct CRONSPEC cronspec;
typedef struct CRONJOB cronjob;

struct named_value
{
  const char *name;
  int value;
};

struct named_value_alias
{
  const char *alias;
  const char *name;
};

struct CRONSPEC
{
  long long minute;
  long hour;
  long day;
  short month;
  char wday;
};

struct CRONJOB
{
  char name[CRON_NAME_LEN];
  long int type;
  cronspec spec;
  char format[CRON_FORMAT_LEN];
  dbref owner;
  dbref object;
  char attrib[ATTRIBUTE_NAME_LIMIT];
};

struct def_crontab  {
  const char *name;
  long int type;
  const char *format;
  dbref owner;
  dbref object;
  const char *attribute;
};

int start_cron(void);
int run_cron(void);

NVALUE *get_nvalue(NVALUE *table, const char *name);
NVALUE *get_nvalue_by_alias(NVALUE *table, NVALUE_ALIAS *alias, const char *name);

cronspec current_spec(void);
int cmpspec(cronspec a, cronspec b);

long long num_to_bits(unsigned int num);
long long parse_spec(const char *str, unsigned int maximum, NVALUE *table, NVALUE_ALIAS *alias, char *format, char **fp);
char *show_spec(cronspec *spec);

cronjob *new_job(void);
int free_job(cronjob *job);
cronjob *make_job(dbref owner, const char *name, const char *str);
void do_list_cronjobs(dbref player);

extern HASHTAB crontab;

extern COMMAND(cmd_cron);

#endif /* __CRON_H */
