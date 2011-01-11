/*
 * cron.c - A cron daemon for CobraMUSH
 * Written originally by grapenut (Jeremy Ritter)  used with permission
 * for the CobraMUSH distribution.
 */

#include "copyrite.h"
#include "config.h"
#include "conf.h"
#include "externs.h"
#include "time.h"
#include "parse.h"
#include "privtab.h"
#include "attrib.h"
#include "lock.h"
#include "log.h"
#include "dbdefs.h"
#include "match.h"
#include "cron.h"

#include <ctype.h>
#ifdef I_STRING
#include <string.h>
#endif

#ifdef MUSHCRON

HASHTAB crontab;

enum SPEC_TYPE {
  CS_MINUTE = 0,
  CS_HOUR = 1,
  CS_DAY = 2,
  CS_MONTH = 3,
  CS_WDAY = 4,
  CS_TASK = 5
};

struct def_crontab default_cron_table[] = {
  {"HOURLY", CF_COMMAND,  "0 * * * *", GOD, CRON_GLOBAL, "HOURLY"},
  {"DAILY", CF_COMMAND,  "0 0 * * *", GOD, CRON_GLOBAL, "DAILY"},
  {NULL, 0, NULL, 0, 0, NULL}
};

static PRIV cron_type_table[] = {
  {"Command", 'C', CF_COMMAND, CF_COMMAND},
  {"Function", 'F', CF_FUNCTION, CF_FUNCTION},
  {"Halt", 'H', CF_HALT, CF_HALT},
  {NULL, '\0', 0, 0}
};

static NVALUE month_table[] = {
  {"Jan", CM_JANUARY},
  {"Feb", CM_FEBRUARY},
  {"Mar", CM_MARCH},
  {"Apr", CM_APRIL},
  {"May", CM_MAY},
  {"Jun", CM_JUNE},
  {"Jul", CM_JULY},
  {"Aug", CM_AUGUST},
  {"Sep", CM_SEPTEMBER},
  {"Oct", CM_OCTOBER},
  {"Nov", CM_NOVEMBER},
  {"Dec", CM_DECEMBER},
  {NULL, 0}
};

static NVALUE_ALIAS month_alias_table[] = {
  {"January", "Jan"},
  {"February", "Feb"},
  {"March", "Mar"},
  {"April", "Apr"},
  {"June", "Jun"},
  {"July", "Jul"},
  {"August", "Aug"},
  {"September", "Sep"},
  {"October", "Oct"},
  {"November", "Nov"},
  {"December", "Dec"},
  {NULL, NULL}
};

static NVALUE wday_table[] = {
  {"Mon", CD_MONDAY},
  {"Tue", CD_TUESDAY},
  {"Wed", CD_WEDNESDAY},
  {"Thu", CD_THURSDAY},
  {"Fri", CD_FRIDAY},
  {"Sat", CD_SATURDAY},
  {"Sun", CD_SUNDAY},
  {NULL, 0}
};

static NVALUE_ALIAS wday_alias_table[] = {
  {"Monday", "Mon"},
  {"Tuesday", "Tue"},
  {"Wednesday", "Wed"},
  {"Thursday", "Thu"},
  {"Friday", "Fri"},
  {"Saturday", "Sat"},
  {"Sunday", "Sun"},
  {NULL, NULL}
};

extern time_t mudtime;


int
start_cron(void)
{
  int i;
  char tmp_format[BUFFER_LEN];
  char *fp_h, *fp_t;
  cronjob *job;
  long long spec;
  enum SPEC_TYPE which;


  hashinit(&crontab, 32, sizeof(cronjob));

  /* Load Default CronTable */
  for(i = 0; default_cron_table[i].name != NULL; i++) {
     job = new_job();
     if(!job)
       break;
     job->owner = default_cron_table[i].owner;
     job->object = default_cron_table[i].object;
     job->type = default_cron_table[i].type;
     strcpy(job->name, default_cron_table[i].name);
     strcpy(job->format, default_cron_table[i].format);
     strcpy(job->attrib, default_cron_table[i].attribute);
     memset(tmp_format, '\0', BUFFER_LEN);
     strcpy(tmp_format,  job->format);

    which = CS_MINUTE;
    fp_h = fp_t = tmp_format;

    while(fp_h && which < CS_TASK ) {
       if(fp_t && (fp_t = strchr(fp_h, ' '))) 
           *fp_t++ = '\0';
      
       switch(which) {
           case CS_MINUTE:
             spec = parse_spec((const char *) fp_h, CRON_MINUTE_MAX, NULL, NULL, NULL, NULL);
             memcpy(&job->spec.minute, &spec, sizeof(job->spec.minute));
             break;
           case CS_HOUR:
             spec = parse_spec((const char *) fp_h, CRON_HOUR_MAX, NULL, NULL, NULL, NULL);
             memcpy(&job->spec.hour, &spec, sizeof(job->spec.hour));
             break;
           case CS_DAY:
             spec = parse_spec((const char *) fp_h, CRON_DAY_MAX, NULL, NULL, NULL, NULL);
             memcpy(&job->spec.day, &spec, sizeof job->spec.day);
             break;
           case CS_MONTH:
             spec = parse_spec((const char *) fp_h, CRON_MONTH_MAX, month_table, month_alias_table, NULL, NULL);
             memcpy(&job->spec.month, &spec, sizeof(job->spec.month));
             break;
           case CS_WDAY:
             spec = parse_spec((const char *) fp_h, CRON_DAY_MAX, wday_table, wday_alias_table, NULL, NULL);
             memcpy(&job->spec.wday, &spec, sizeof(job->spec.wday));
             break;
       }
       which++;
       fp_h = fp_t;
    }
    hashadd(job->name, (void *) job, &crontab);     
  }
  return 1;
}

NVALUE *
get_nvalue(NVALUE *table, const char *name)
{
  NVALUE *n;
  for (n = table; n && n->name; n++) {
    if (!strcasecmp(n->name, name)) {
      return n;
    }
  }
  return NULL;
}

NVALUE *
get_nvalue_by_alias(NVALUE *table, NVALUE_ALIAS *alias, const char *name)
{
  NVALUE *n;
  NVALUE_ALIAS *a;

  n = get_nvalue(table, name);
  if (n)
    return n;
  for (a = alias; a && a->alias; a++) {
    if (!strcasecmp(a->alias, name)) {
      n = get_nvalue(table, a->name);
      if (!n)
        continue;
      return n;
    }
  }
  return NULL;
}

NVALUE *
get_nvalue_by_value(NVALUE *table, int value)
{
  NVALUE *n;
  for (n = table; n && n->name; n++) {
    if (n->value == value) {
      return n;
    }
  }
  return NULL;
}

cronspec
current_spec(void)
{
  cronspec spec;
  struct tm *current_time = localtime(&mudtime);

  spec.minute = (typeof(spec.minute)) 1 << current_time->tm_min;
  spec.hour = (typeof(spec.hour)) 1 << current_time->tm_hour;
  spec.day = (typeof(spec.day)) 1 << (current_time->tm_mday - 1);
  spec.month = (typeof(spec.month)) 1 << current_time->tm_mon;
  spec.wday = (typeof(spec.wday)) 1 << current_time->tm_wday;

  return spec;
}

int
cmpspec(cronspec a, cronspec b)
{
  if (a.minute & b.minute &&
      a.hour & b.hour &&
      a.day & b.day &&
      a.month & b.month &&
      a.wday & b.wday)
    return 1;
  return 0;
}

int
run_cron(void)
{
  cronspec spec;
  cronjob *job;
  char buff[BUFFER_LEN];
  char *bp;
  const char *sp;
  dbref start, end;
  ATTR *a;
  char *s;
  int stop = 0;
  

  if (mudtime % 60)
    return 0;


  spec = current_spec();
  
  for (job = (cronjob *) hash_firstentry(&crontab); job; job = (cronjob *) hash_nextentry(&crontab)) {
    if (cmpspec(spec, job->spec)) {
      if (CRON_Halt(job)) {
        continue;
      }
      if (!GoodObject(job->object)) {
        start = 0;
        end = db_top;
      } else {
        start = job->object;
        end = job->object;
      }
      do {
        /* Job owner must control object to do it */
        if(controls(job->owner,start)) {
          if (CRON_Command(job)) {
            queue_attribute(start, job->attrib, job->owner);
          } else if (CRON_Function(job)) {
            /* TODO: Review this section & possibly rewrite to make sure its secure. */
            bp = buff;
            a = (ATTR *) atr_get(start, job->attrib);
            if (!a) {
              start++;
              continue;
            }
            s = (char *) safe_atr_value(a);
            if (!s) {
              start++;
              continue;
            }
            sp = s;
            process_expression(buff, &bp, &sp, start, start, start, PE_DEFAULT, PT_DEFAULT, NULL);
            *bp = '\0';
            free(s);
          } else {
            stop = 1;
            continue;
          }
        }
        start++;
      } while (start < end && !stop);
    }
  }

  return 1;
}

long long
num_to_bits(unsigned int num)
{
  return (long long) 1 << num;
}

long long
parse_spec(const char *str, unsigned int maximum, NVALUE *table, NVALUE_ALIAS *alias, char *format, char **fp)
{
  char c;
  int x;
  int num;
  int range;
  int skip;
  int iter;
  char *sp;
  char *endptr;
  long long spec = 0;
  char buff[BUFFER_LEN];
  char *bp;
  NVALUE *nv;
  
  if (fp) {
    c = **fp;
    **fp = '\0';
  }
  if (fp)
    **fp = c;

  bp = buff;
  safe_str(str, buff, &bp);
  *bp = '\0';
  bp = buff;

  do {
    num = -1;
    range = -1;
    skip = 1;
    
    sp = split_token(&bp, CRON_NUM_SEP);
    if (!sp || !*sp)
      break;
    
    endptr = strchr(sp, CRON_SKIP_SEP);
    if (endptr) {
      *endptr++ = '\0';
      if (*endptr && isdigit(*endptr)) {
        skip = strtol(endptr, NULL, 10);
      }
    }
    
    endptr = strchr(sp, CRON_RANGE_SEP);
    if (endptr) {
      *endptr++ = '\0';
      if (*endptr) {
        if (isdigit(*endptr)) {
          range = strtol(endptr, NULL, 10);
        } else if ((nv = get_nvalue_by_alias(table, alias, endptr))) {
          range = nv->value;
        }
      }
    }
    
    if (isdigit(*sp)) {
      num = strtol(sp, NULL, 10);
    } else if (*sp == CRON_WILDCARD) {
      num = 0;
      range = maximum;
    } else if ((nv = get_nvalue_by_alias(table, alias, sp))) {
      num = nv->value;
    } else
      continue;
    
    if (num < 0 || num > maximum)
      continue;
    
    if (range < 0) {
      range = num;
    } else if (range > maximum) {
      range = maximum;
    }
    
    if (skip < 1 || skip > maximum)
      continue;

    if (format) {
      if (num == 0 && range == maximum) {
        safe_chr(CRON_WILDCARD, format, fp);
      } else {

        nv = get_nvalue_by_value(table, num);
        if (nv)
          safe_str(nv->name, format, fp);
        else
          safe_integer(num, format, fp);

        if (range != num) {
            safe_chr(CRON_RANGE_SEP, format, fp);

            nv = get_nvalue_by_value(table, range);
            if (nv)
              safe_str(nv->name, format, fp);
            else
              safe_integer(range, format, fp);
            
        }
      }
      if (skip > 1) {
        safe_chr(CRON_SKIP_SEP, format, fp);
        safe_integer(skip, format, fp);
      }
    }

    iter = 1;
    do {
      if (iter == 1)
        spec |= num_to_bits(num);
      iter++;
      num++;
      if (num > maximum && range < maximum)
        num = 0;
      if (iter > skip)
        iter = 1;
    } while (num <= range);
    if (bp)
      safe_chr(CRON_NUM_SEP, format, fp);
  } while (bp);

  bp = buff;
  for (x = sizeof(long long) * 8 - 1; x > -1; x--) {
    if (spec & ((long long) 1 << x)) {
      safe_integer(x, buff, &bp);
      if ((x + 1) > -1)
        safe_chr(' ', buff, &bp);
    }
  }
  *bp = '\0';
  bp = buff;

  
  return spec;

}

char *
show_spec(cronspec *spec)
{
  static char buff[BUFFER_LEN];
  char *bp;
  int x;

  if (!spec)
    return NULL;
  
  bp = buff;

  for (x = 0; x <= CRON_MINUTE_MAX; x++) {
    if (spec->minute & ((typeof(spec->minute)) 1 << x))
      safe_integer(1, buff, &bp);
    else
      safe_integer(0, buff, &bp);
  }

  safe_chr('\n', buff, &bp);

  for (x = 0; x <= CRON_HOUR_MAX; x++) {
    if (spec->hour & ((typeof(spec->hour)) 1 << x))
      safe_integer(1, buff, &bp);
    else
      safe_integer(0, buff, &bp);
  }

  safe_chr('\n', buff, &bp);

  for (x = 0; x <= CRON_DAY_MAX; x++) {
    if (spec->day & ((typeof(spec->day)) 1 << x))
      safe_integer(1, buff, &bp);
    else
      safe_integer(0, buff, &bp);
  }

  safe_chr('\n', buff, &bp);

  for (x = 0; x <= CRON_MONTH_MAX; x++) {
    if (spec->month & ((typeof(spec->month)) 1 << x))
      safe_integer(1, buff, &bp);
    else
      safe_integer(0, buff, &bp);
  }

  safe_chr('\n', buff, &bp);
  
  for (x = 0; x <= CRON_WDAY_MAX; x++) {
    if (spec->wday & ((typeof(spec->wday)) 1 << x))
      safe_integer(1, buff, &bp);
    else
      safe_integer(0, buff, &bp);
  }

  *bp = '\0';

  return buff;
}


cronjob *
new_job(void)
{
  return (cronjob *) mush_malloc(sizeof(cronjob), "CRONJOB");
}

int
free_job(cronjob *job)
{
  if (job) {
    hashdelete(job->name, &crontab);
    mush_free(job, "CRONJOB");
  }
  return 1;
}

cronjob *
make_job(dbref owner, const char *name, const char *str)
{
  cronjob *job;
  enum SPEC_TYPE which;
  char *fp;
  char *sp;
  char *tp;
  char *bp;
  char buff[BUFFER_LEN];
  long long spec;
  
  if (!name || !str || !GoodObject(owner))
    return NULL;

  job = (cronjob *) hashfind(strupper(name), &crontab);
  if (!job) {
    job = new_job();
    if (!job) {
      notify(owner, "@CRON: ERROR: Unable to allocate cron-job.");
      return NULL;
    }
  }

  job->owner = owner;
  fp = job->format;

  bp = job->name;
  safe_strl(strupper(name), 32, job->name, &bp);
  *bp = '\0';

  bp = buff;
  safe_str(str, buff, &bp);
  *bp = '\0';
  bp = buff;
  
  which = CS_MINUTE;
  
  do {
    sp = split_token(&bp, CRON_SPEC_SEP);
    if (!sp || !*sp)
      break;
    switch (which) {
      case CS_MINUTE:
        spec = parse_spec((const char *) sp, CRON_MINUTE_MAX, NULL, NULL, job->format, &fp);
        memcpy(&job->spec.minute, &spec, sizeof(job->spec.minute));
        break;
      case CS_HOUR:
        safe_chr(CRON_SPEC_SEP, job->format, &fp);
        spec = parse_spec((const char *) sp, CRON_HOUR_MAX, NULL, NULL, job->format, &fp);
        memcpy(&job->spec.hour, &spec, sizeof(job->spec.hour));
        break;
      case CS_DAY:
        safe_chr(CRON_SPEC_SEP, job->format, &fp);
        spec = parse_spec((const char *) sp, CRON_DAY_MAX, NULL, NULL, job->format, &fp);
        memcpy(&job->spec.day, &spec, sizeof(job->spec.day));
        break;
      case CS_MONTH:
        safe_chr(CRON_SPEC_SEP, job->format, &fp);
        spec = parse_spec((const char *) sp, CRON_MONTH_MAX, month_table, month_alias_table, job->format, &fp);
        memcpy(&job->spec.month, &spec, sizeof(job->spec.month));
        break;
      case CS_WDAY:
        safe_chr(CRON_SPEC_SEP, job->format, &fp);
        spec = parse_spec((const char *) sp, CRON_WDAY_MAX, wday_table, wday_alias_table, job->format, &fp);
        memcpy(&job->spec.wday, &spec, sizeof(job->spec.wday));
        break;
      default:
        continue;
    }
    which++;
  } while (bp && which < CS_TASK);

  if(!bp) {
    free_job(job);
    notify(owner, "@CRON: Invalid job format.");
    return NULL;
  }

  sp = strchr(bp, '/');
  if (!sp) {
    free_job(job);
    notify(owner, "@CRON: Invalid job format.");
    return NULL;
  }
  *sp++ = '\0';
  
  if (!strcasecmp(bp, "GLOBAL")) {
    if(!Admin(owner)) {  /* Allow Admin and higher to set the global crons */
      notify(owner, "@CRON: Permission denied.");
      return NULL;
    }

    job->object = NOTHING;
  } else {
    job->object = noisy_match_result(owner, bp, NOTYPE, MAT_EVERYTHING);
    if (!GoodObject(job->object)) {
      free_job(job);
      return NULL;
    } else if(!controls(owner, job->object)) {
      notify(owner, "@CRON: Permission denied.");
      return NULL;
    }
  }

  tp = job->attrib;
  safe_str(strupper(sp), job->attrib, &tp);
  *tp = '\0';

  *fp = '\0';

  hashadd(job->name, (void *) job, &crontab);

  return job;
}

#define JobObject_Str(x)   GoodObject(x) ? tprintf("#%d", x) : "Global"

void
do_list_cronjobs(dbref player)
{
  cronjob *job;
  int jobs;

  notify_format(player, "%-16s %-10s %-10s %s", "Name", "Owner", "Object", "Attribute");
  notify(player,        "-----------------------------------------------------------");
  for (job = hash_firstentry(&crontab), jobs = 0; job; job = hash_nextentry(&crontab), jobs++) {
    if (CanSee(player, job->owner))
      notify_format(player, "%-16s %-10d %-10s %s", job->name, job->owner, JobObject_Str(job->object), job->attrib);
  }
  if (jobs < 1)
    notify(player, "@CRON: There are no cronjobs.");
}

COMMAND(cmd_cron) {
  cronjob *job;
  if (SW_ISSET(sw, SWITCH_ADD)) {
    long int type = 0;
    if (!*arg_left || !*arg_right) {
      notify(player, "@CRON: You must supply a job name and spec.");
      return;
    }

    /* Do make_job parsing here */
    
    if (!(job = make_job(player, (const char *) arg_left, (const char *) arg_right)))
      return;
    
    notify_format(player, "@CRON: Added job %s that runs %s/%s at %s.", job->name, GoodObject(job->object) ? tprintf("#%d", job->object) : "GLOBAL", job->attrib, job->format);

    if (SW_ISSET(sw, SWITCH_COMMANDS)) {
      type |= CF_COMMAND;
    }

    if (SW_ISSET(sw, SWITCH_FUNCTIONS)) {
      notify(player, "DEV: Function Crons are Disabled for the time being while going a security review.");
      return;
    /*  type |= CF_FUNCTION; */
    }
    
    if (CRON_Command(job) && CRON_Function(job)) {
      notify(player, "@CRON: A job may not run as both a command and a function.");
    } else {
      notify_format(player, "@CRON: Job type set to %s.", privs_to_string(cron_type_table, type));
      job->type = type;
    }
    
    if (!CRON_Command(job) && !CRON_Function(job)) {
      notify(player, "@CRON: You must use @CRON/SET to specify whether it is a Command or Function.");
    }
    return;

  } else if (SW_ISSET(sw, SWITCH_SET)) {
    long int oldtype;
    long int newtype;
    
    if (!*arg_left) {
      notify(player, "@CRON: You must supply a job name.");
      return;
    }
    
    job = hashfind(strupper((const char *) arg_left), &crontab);
    if (!job) {
      notify(player, "@CRON: That is not a valid cronjob.");
      return;
    }
    
    if (!*arg_right) {
      notify(player, "@CRON: You must supply a toggle string.");
      return;
    }
    
    oldtype = job->type;
    newtype = string_to_privs(cron_type_table, arg_right, job->type);
    
    if (newtype & CF_COMMAND && newtype & CF_FUNCTION) {
      notify(player, "@CRON: A job may not run as both a command and a function.");
      return;
    }
    
    job->type = newtype;
    newtype = oldtype ^ job->type;
    notify_format(player, "@CRON: %s%s %s.", privs_to_string(cron_type_table, newtype),
                  (oldtype & newtype) ? " removed from" : ((job->type & newtype) ?
                                                           " set on" :
                                                           "No flags changed on"),
                  job->name);
    return;
  } else if (SW_ISSET(sw, SWITCH_DELETE)) {
    if (!*arg_left) {
      notify(player, "@CRON: You must supply a job name.");
      return;
    }

    job = hashfind(strupper((const char *) arg_left), &crontab);
    if (!job) {
      notify(player, "@CRON: That is not a valid cronjob.");
      return;
    }
    notify_format(player, "@CRON: Deleted job '%s'.", job->name);
    free_job(job);
    return;

  } else {
  
    if (!*arg_left) {
       do_list_cronjobs(player);
    } else {
      job = hashfind(strupper((const char *) arg_left), &crontab);
      if (!job) {
        notify(player, "@CRON: That is not a valid cronjob.");
        return;
      }
      notify_format(player, "NAME: %-35s TYPE: %s\nATTR: %6s/%-26s SPEC: %s", job->name, privs_to_string(cron_type_table, job->type),
                    GoodObject(job->object) ? tprintf("#%d", job->object) : "GLOBAL", job->attrib, job->format);
      /* More or Less Debug Information? */
     notify(player, show_spec(&job->spec));
  
      return;
    }
    
    return;
  }
}

#endif /* MUSHCRON */

