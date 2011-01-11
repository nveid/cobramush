/* prog.c - MUSH Program and Prompting Related Code - File created 3tpritnf/11/02
 *          Part of the CobraMUSH Server Distribution
 *
 */
#include "copyrite.h"
#include "config.h"
#ifdef I_STRING
#include <string.h>
#else
#include <strings.h>
#endif
#include "conf.h"
#include "externs.h"
#include "parse.h"
#include "htab.h"
#include "division.h"
#include "boolexp.h"
#include "command.h"
#include "cmds.h"
#include "confmagic.h"
#include "attrib.h"
#include "flags.h"
#include "mushdb.h"
#include "dbdefs.h"
#include "match.h"
#include "ansi.h"
#include "log.h"
#include "extmail.h"

/* External declarations. */

extern int process_output(DESC * d);
extern int queue_newwrite(DESC * d, const unsigned char *b, int n);
extern int queue_eol(DESC * d);

int password_handler(DESC *, char *);

/* Telnet codes */
#define IAC             255     /**< interpret as command */
#define GOAHEAD         249     /**< go ahead */

#define UNDEFINED_PROMPT "\x1B[1m>\x1b[0m"
#define PI_LOCK               0x1
#define PI_PROMPT             0x2
#define PI_INTERNAL_PROMPT    0x40

extern DESC *descriptor_list;

extern int do_command(DESC *, char *);
void prog_place(dbref player, dbref object, ATTR * patr, int lock);

/* SWITCHES- LOCK(requires power) QUIT */
COMMAND(cmd_prog)
{
  dbref target;
  dbref thing;
  char tbuf1[BUFFER_LEN], *tbuf2;
  char *s, *t, *tbp;
  ATTR *patr;
  DESC *d;
  int i, pflags = 0;
  char buf[BUFFER_LEN], *bp;
  char *key;

  if (!arg_left) {
    notify(player, "Invalid arguments.");
    return;
  }
  target = match_result(player, arg_left, TYPE_PLAYER, MAT_EVERYTHING);
  if ((target == NOTHING) || !Connected(target)) {
    notify(player, "Can't find that player.");
    return;
  }

  if (!CanProg(player, target)) {
    notify(player, "You can't place that player into a program.");
    return;
  }

  if (SW_ISSET(sw, SWITCH_LOCK)) {
    if (!CanProgLock(player, target)) {
      notify(player, "You can't lock that player into a program.");
      return;
    }
    pflags |= PI_LOCK;
  }

  DESC_ITER_CONN(d)
      if (d->player == target) {
    if ((InProg(d->player) && ((d->pinfo.lock & PI_LOCK) &&
                               !CanProgLock(player, d->player)))
        || (d->pinfo.lock & PI_INTERNAL_PROMPT)) {
      notify(player, "That player is currently locked in their program.");
      return;
    } else
      d->pinfo.lock = 0;
  }

  if (SW_ISSET(sw, SWITCH_QUIT)) {
    DESC_ITER_CONN(d) {
      if (d->player == target) {
        if (!d->connected) {
          notify(player, "Player not connected.");
          return;
        }
        d->pinfo.object = -1;
        d->pinfo.atr = NULL;
        d->pinfo.lock = 0;
        d->pinfo.function = NULL;
        d->input_handler = do_command;

        queue_newwrite(d, (unsigned char *) "\r\n", 2);
      }
    }
    twiddle_flag_internal("FLAG", target, "INPROGRAM", 1);
    atr_clr(target, "XY_PROGENV", GOD);
    if (Nospoof(target))
      notify_format(target, "%s took you out of your program.",
                    object_header(target, player));
    return;
  }

  s = strchr(arg_right, '/');

  if (!s) {
    thing = player;
    t = strchr(arg_right, ':');
    s = arg_right;
  } else {
    *s++ = '\0';
    thing = match_thing(player, arg_right);
    t = strchr(s, ':');
  }

  if (t != NULL)
    *t++ = '\0';

  if (thing == NOTHING) {
    notify(player, "Can't find that object");
    return;
  } else if (!controls(player, thing)) {
    notify(player, "Permission denied.");
    return;
  } /* get atr here into patr */
  else if ((patr = atr_get(thing, upcasestr(s))) == NULL) {
    notify(player, "No such attribute.");
    notify(player, s);
    return;
  } else if (!Can_Write_Attr(player, thing, patr)) {
    notify(player, "Permission denied.");
    return;
  }
  /* NOW.. Put them in the prog */
  notify_format(player, "You place %s into a program.",
                object_header(player, target));
  if (Nospoof(target))
    notify_format(target, "You have been placed into a program by %s.",
                  object_header(target, player));
  if (t) {
    pflags |= PI_PROMPT;
    do_rawlog(LT_ERR,
              "Bad Program Prompt.  Allowed (will be defunct as of CobraMUSH 1.0).  Suggest use @prompt: %d - %s",
              player, global_eval_context.ccom);
    notify_format(Owner(player),
                  "WARNING: %s is using bad prompt syntax with @program.  Refer to help @prompt.",
                  object_header(Owner(player), player));

    tooref = ooref, ooref = NOTHING;
    atr_add(target, "XY_PROGPROMPT", t, GOD, NOTHING);
    ooref = tooref;
    memset(buf, '\0', BUFFER_LEN);
    bp = buf;
  }
  /* Save Registers to XY_PROGENV */
  memset(tbuf1, '\0', BUFFER_LEN);
  tbp = tbuf1;
  for (i = 0; i < NUMQ; i++) {
    if (global_eval_context.renv[i][0]) {
      tbuf2 = str_escaped_chr(global_eval_context.renv[i], '|');
      safe_str(tbuf2, tbuf1, &tbp);
      mush_free((Malloc_t) tbuf2, "str_escaped_chr.buff");
    }
    safe_chr('|', tbuf1, &tbp);
  }
  for(key = hash_firstentry_key(&global_eval_context.namedregs); key; key = hash_nextentry_key(&global_eval_context.namedregs)) {
    safe_str(key, tbuf1, &tbp);
    safe_chr('|', tbuf1, &tbp);
    safe_str(get_namedreg(&global_eval_context.namedregs, key), tbuf1, &tbp);
    safe_chr('|', tbuf1, &tbp);
  }

  tbp--;
  *tbp = '\0';

  /* Now Save to XY_PROGENV */
  (void) atr_add(target, "XY_PROGENV", tbuf1, GOD, NOTHING);


  /* Place them into the actual program */

  prog_place(target, thing, patr, pflags);
}

/* arg0 - player, 
 * arg 1 - prog thing or attribute
 * arg 2 - attribute, 
 * arg 3 - program prompt silently ignored and logged bad usage (until Cobra 1.0) 
 *       - **CORRECT USAGE** ProgLock 1 or 0
 * arg 4 - lock(1/0) logged bad usage (until Cobra 1.0) */
FUNCTION(fun_prog)
{
  ATTR *patr;
  DESC *d;
  char tbuf1[BUFFER_LEN], *tbuf2;
  char *tbp;
  dbref target, thing;
  char pflags = 0x0;
  int i;
  char *key;

  target = match_result(executor, args[0], TYPE_PLAYER, MAT_EVERYTHING);
  if (!GoodObject(target) || !Connected(target)) {
    safe_str("#-1 UNFINDABLE", buff, bp);
    return;
  }

  if (!CanProg(executor, target)) {
    safe_str("#-1 PERMISSION DENIED", buff, bp);
    return;
  }

  if (args[3] && is_integer(args[3]) && atoi(args[3])
      && !CanProgLock(executor, target)) {
    safe_str("#-1 PERMISSION DENIED", buff, bp);
    return;
  }

  DESC_ITER_CONN(d) {
    if (d->player == target) {
      if ((InProg(d->player) && ((d->pinfo.lock & PI_LOCK) &&
                                 !CanProgLock(executor, d->player)))
          || (d->pinfo.lock & PI_INTERNAL_PROMPT)) {
        safe_str("#-1 PERMISSION DENIED", buff, bp);
        return;
      } else
        d->pinfo.lock = 0;
    }
  }
  thing = match_thing(executor, args[1]);
  if (thing == NOTHING) {
    safe_str("#-1 THING UNFINDABLE", buff, bp);
    return;
  } else if (!controls(executor, thing)) {
    safe_str("#-1 PERMISSION DENIED", buff, bp);
    return;
  } /* get atr here into patr */
  else if ((patr = atr_get(thing, upcasestr(args[2]))) == NULL) {
    safe_str("#-1 NO SUCH ATTRIB", buff, bp);
    return;
  } else if (!Can_Write_Attr(executor, thing, patr)) {
    safe_str("#-1 PERMISSION DENIED", buff, bp);
    return;
  }
  if (nargs >= 4) {
    if (args[3] && is_integer(args[3]) && atoi(args[3]) == 1)
      pflags = PI_LOCK;
    else {
      do_rawlog(LT_ERR,
                "Bad Program Prompt, Silently Ignored. HINT: Use Prompt() : %d - %s",
                executor, global_eval_context.ccom);
    }
  }

  if (nargs >= 5) {
    if (args[4] && atoi(args[4]) == 1) {
      pflags = PI_LOCK;
      do_rawlog(LT_ERR, "Bad Proglock Usage format used on %d - %s",
                executor, global_eval_context.ccom);
    }
  }

  /* Save Registers to XY_PROGENV */
  memset(tbuf1, '\0', BUFFER_LEN);
  tbp = tbuf1;
  for (i = 0; i < NUMQ; i++) {
    if (global_eval_context.renv[i][0]) {
      tbuf2 = str_escaped_chr(global_eval_context.renv[i], '|');
      safe_str(tbuf2, tbuf1, &tbp);
      mush_free((Malloc_t) tbuf2, "str_escaped_chr.buff");
    }
    safe_chr('|', tbuf1, &tbp);
  }
  for(key = hash_firstentry_key(&global_eval_context.namedregs); key; key = hash_nextentry_key(&global_eval_context.namedregs)) {
    safe_str(key, tbuf1, &tbp);
    safe_chr('|', tbuf1, &tbp);
    safe_str(get_namedreg(&global_eval_context.namedregs, key), tbuf1, &tbp);
    safe_chr('|', tbuf1, &tbp);
  }


  tbp--;
  *tbp = '\0';
  /* Now Save to XY_PROGENV */
  (void) atr_add(target, "XY_PROGENV", tbuf1, GOD, NOTHING);



  prog_place(target, thing, patr, pflags);
  safe_chr('1', buff, bp);
}

/* quitprog(player) */
FUNCTION(fun_quitprog)
{
  DESC *d;
  dbref target;

  target = match_result(executor, args[0], TYPE_PLAYER, MAT_EVERYTHING);

  if (target == NOTHING || !Connected(target)) {
    safe_str(e_notvis, buff, bp);
    return;
  }
  if (!CanProg(executor, target)) {
    safe_str(e_perm, buff, bp);
    return;
  }

  DESC_ITER_CONN(d)
      if (d->player == target) {
    if (d->pinfo.lock && !CanProgLock(executor, target)) {
      safe_str(e_perm, buff, bp);
      return;
    }
    d->pinfo.object = -1;
    d->pinfo.atr = NULL;
    d->pinfo.lock = 0;
    d->input_handler = do_command;
    queue_newwrite(d, (unsigned char *) tprintf("%c%c", IAC, GOAHEAD), 2);
  }
  atr_clr(target, "XY_PROGINFO", GOD);
  atr_clr(target, "XY_PROGPROMPT", GOD);
  atr_clr(target, "XY_PROGENV", GOD);
  twiddle_flag_internal("FLAG", target, "INPROGRAM", 1);
  safe_chr('1', buff, bp);

  if (Nospoof(target))
    notify_format(target, "%s took you out of your program.",
                  object_header(target, executor));

}

COMMAND(cmd_prompt)
{
  dbref who;
  DESC *d;
  char *prompt;
  char *tmp;

  switch (who =
          match_result(player, arg_left, NOTYPE,
                       MAT_OBJECTS | MAT_HERE | MAT_CONTAINER)) {
  case NOTHING:
    notify(player, T("I don't see that here."));
    break;
  case AMBIGUOUS:
    notify(player, T("I don't know who you mean!"));
    break;
  default:
    if (!CanProg(player, who)) {
      notify_format(player,
                    T("I'm sorry, but %s wishes to be left alone now."),
                    Name(who));
      return;
    }
    if (Typeof(who) != TYPE_PLAYER) {
      notify_format(player, T("I'm sorry, but %s cannot receive prompts."),
                    Name(who));
      return;
    }
    if (!InProg(who)) {
      notify_format(player, T("I'm sorry, but %s is not in a @program."),
                    Name(who));
      return;
    }
    if (arg_right && *arg_right) {
      prompt = arg_right;
      /* Save the prompt as well */
      tooref = ooref, ooref = NOTHING;
      atr_add(who, "XY_PROGPROMPT", arg_right, GOD, NOTHING);
      ooref = tooref;
    } else
      prompt = (char *) UNDEFINED_PROMPT;

    DESC_ITER_CONN(d) {
      if (d->player == who) {
        if (PromptConnection(d)) {
          if (ShowAnsiColor(d->player))
            tmp = tprintf("%s %c%c", prompt, IAC, GOAHEAD);
          else
            tmp =
                tprintf("%s %c%c", remove_markup(prompt, NULL), IAC,
                        GOAHEAD);
        } else {
          if (ShowAnsiColor(d->player))
            tmp = tprintf("%s \r\n", prompt);
          else
            tmp = tprintf("%s \r\n", remove_markup(prompt, NULL));
        }
        queue_newwrite(d, (unsigned char *) tmp, strlen(tmp));
      }
    }
  }

}

FUNCTION(fun_prompt)
{
  dbref who;
  DESC *d;
  char *prompt;
  char *tmp;

  switch (who =
          match_result(executor, args[0], NOTYPE,
                       MAT_OBJECTS | MAT_HERE | MAT_CONTAINER)) {
  case NOTHING:
    safe_str(T(e_match), buff, bp);
    break;
  case AMBIGUOUS:
    safe_str(T("#-2 AMBIGUOUS MATCH"), buff, bp);
    break;
  default:
    if (!CanProg(executor, who)) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
    if (Typeof(who) != TYPE_PLAYER) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
    if (!InProg(who)) {
      safe_str(T("#-1 NOT IN @PROGRAM"), buff, bp);
      return;
    }
    if (args[1])
      prompt = args[1];
    else
      prompt = (char *) UNDEFINED_PROMPT;

    DESC_ITER_CONN(d) {
      if (d->player == who) {
        if (PromptConnection(d)) {
          if (ShowAnsiColor(who))
            tmp = tprintf("%s %c%c", prompt, IAC, GOAHEAD);
          else
            tmp =
                tprintf("%s %c%c", remove_markup(prompt, NULL), IAC,
                        GOAHEAD);
        } else {
          if (ShowAnsiColor(who))
            tmp = tprintf("%s \r\n", prompt);
          else
            tmp = tprintf("%s \r\n", remove_markup(prompt, NULL));
        }
        queue_newwrite(d, (unsigned char *) tmp, strlen(tmp));
      }
    }
  }
}

/* eventually write core_commands() for holding QUIT LOGOUT WHO & other
 * core commands for when locked
 */
int
prog_handler(DESC * d, char *input)
{
  ATTR *a;
  char buf[BUFFER_LEN];
  char *tbuf, *bp;
  char *p_buf[NUMQ];
  int rcnt, i;
  char *key;

  if (!strcmp(input, "IDLE"))
    return 1;

  /* handle escaped commandz */
  if (d->pinfo.lock) {
    if (!strcmp(input, "|QUIT")) {
      return 0;
    } else if (!strcmp(input, "|LOGOUT")) {
      return -1;
    }
  } else if (*input == '|' && *(input + 1) != '\0') {
    return do_command(d, input + 1);
  }
  (d->cmds)++;
  d->last_time = mudtime;
  /* First Load Any Q Registers if we have 'em */
  a = atr_get(d->player, "XY_PROGENV");
  if (a) {
    rcnt = elist2arr(p_buf, NUMQ, safe_atr_value(a), '|');

    for (i = 0; i < NUMQ && i < rcnt; i++)
      if (p_buf[i] && strlen(p_buf[i]) > 0) {
        strcpy(global_eval_context.renv[i], p_buf[i]);
        global_eval_context.rnxt[i] = global_eval_context.renv[i];
      }

    for (; i < (rcnt - 1); i+= 2) {
      if (p_buf[i] && strlen(p_buf[i]) > 0 && p_buf[i+1] && strlen(p_buf[i+1]) > 0) {
        set_namedreg(&global_eval_context.namedregs, p_buf[i], p_buf[i+1]);
        set_namedreg(&global_eval_context.namedregsnxt, p_buf[i], p_buf[i+1]);
      }
    }
  }
  strcpy(buf, atr_value(d->pinfo.atr));
  global_eval_context.wnxt[0] = input;
  parse_que(d->pinfo.object, buf, d->player);
  /* Save Registers to XY_PROGENV */
  memset(buf, '\0', BUFFER_LEN);
  bp = buf;
  for (i = 0; i < NUMQ; i++) {
    if (global_eval_context.renv[i][0]) {
      tbuf = str_escaped_chr(global_eval_context.renv[i], '|');
      safe_str(tbuf, buf, &bp);
      mush_free((Malloc_t) tbuf, "str_escaped_chr.buff");
    }
    safe_chr('|', buf, &bp);
  }
  for(key = hash_firstentry_key(&global_eval_context.namedregs); key; key = hash_nextentry_key(&global_eval_context.namedregs)) {
    safe_str(key, buf, &bp);
    safe_chr('|', buf, &bp);
    safe_str(get_namedreg(&global_eval_context.namedregs, key), buf, &bp);
    safe_chr('|', buf, &bp);
  }
  bp--;
  *bp = '\0';
  /* Now Save to XY_PROGENV */
  (void) atr_add(d->player, "XY_PROGENV", buf, GOD, NOTHING);

  return 1;
}

/* Check password of entering pinfo.object */
int
password_handler(DESC * d, char *input)
{
  int r;

  if (!password_check(d->pinfo.object, input)) {
    d->input_handler = do_command;
    queue_newwrite(d, (unsigned char *) "Invalid password.\r\n", 19);
    do_log(LT_WIZ, d->player, d->pinfo.object, "** @SU FAIL **");
    d->pinfo.object = NOTHING;
    d->pinfo.function = NULL;
    d->pinfo.lock = 0;
    return 1;
  }

  r = d->pinfo.function(d, input);
  d->pinfo.function = NULL;
  d->pinfo.lock = 0;
  return r;
}


int
pw_div_connect(DESC * d, char *input __attribute__ ((__unused__)))
{
  /* Division @su should be backlogged through an internal attribute. */
  add_to_div_exit_path(d->player, Division(d->player));
  /* Trigger SDOUT first */
  did_it(d->player, Division(d->player), "SDOUT", NULL, NULL, NULL,
         "ASDOUT", Location(d->player));
  Division(d->player) = d->pinfo.object;
  /* Now Trigger Sdin */
  did_it(d->player, Division(d->player), "SDIN",
         tprintf("You have switched into Division: %s",
                 object_header(d->player, Division(d->player)))
         , NULL, NULL, "ASDIN", Location(d->player));

  d->pinfo.object = NOTHING;
  d->input_handler = do_command;
  return 1;
}

int
pw_player_connect(DESC * d, char *input __attribute__ ((__unused__)))
{
  DESC *match;
  int num = 0, is_hidden;

  do_log(LT_WIZ, d->player, d->pinfo.object, "** @SU SUCCESS **");
  add_to_exit_path(d, d->player);
  announce_disconnect(d->player);
  d->player = d->pinfo.object;
#ifdef USE_MAILER
  d->mailp = (MAIL *) find_exact_starting_point(d->pinfo.object);
#endif
  /* We're good @su him */
  is_hidden = Can_Hide(d->pinfo.object) && Dark(d->pinfo.object);
  DESC_ITER_CONN(match)
      if (match->player == d->player) {
    num++;
    if (is_hidden)
      match->hide = 1;
  }
  if (ModTime(d->pinfo.object))
    notify_format(d->pinfo.object,
                  T("%ld failed connections since last login."),
                  ModTime(d->pinfo.object));
  announce_connect(d->pinfo.object, 0, num);
  check_last(d->pinfo.object, d->addr, d->ip);  /* set last, lastsite, give paycheck */
  queue_eol(d);
#ifdef USE_MAILER
  if (command_check_byname(d->pinfo.object, "@MAIL"))
    check_mail(d->pinfo.object, 0, 0);
  set_player_folder(d->pinfo.object, 0);
#endif
  do_look_around(d->pinfo.object);
  if (Haven(d->pinfo.object))
    notify(d->player,
           T("Your HAVEN flag is set. You cannot receive pages."));
  d->pinfo.object = NOTHING;
  d->input_handler = do_command;
  return 1;
}

/* load program for connecting player */
void
prog_load_desc(DESC * d)
{
  ATTR *a;
  char buf[BUFFER_LEN];
  char buf2[BUFFER_LEN];
  char *p, *t;
  char wprmpt[2];
  dbref obj;

  t = buf;

  a = atr_get(d->player, "XY_PROGINFO");
  if (!a)
    return;
  /* OBJECT ATTRIBUTE PROGLOCK */
  strcpy(buf, atr_value(a));

  p = split_token(&t, ' ');
  /* OBJECT */
  obj = (dbref) parse_number(p);
  if (!GoodObject(obj)) {       /* reset their prog status */
    twiddle_flag_internal("FLAG", obj, "INPROGRAM", 1);
    atr_clr(d->player, "XY_PROGINFO", GOD);
    atr_clr(d->player, "XY_PROGPROMPT", GOD);
    return;
  }
  p = split_token(&t, ' ');
  /* ATTRIBUTE */
  strcpy(buf2, p);
  a = atr_get(obj, buf2);
  if (!a)
    return;
  d->pinfo.object = obj;
  d->pinfo.atr = a;
  p = split_token(&t, ' ');
  d->pinfo.lock = atoi(p);
  d->input_handler = prog_handler;
  memset(wprmpt, '\0', 2);
  if ((a = atr_get(d->player, "XY_PROGPROMPT"))) {
    /* program prompt */
    char rbuf[BUFFER_LEN], *tbp, *rbp;
    strcpy(buf, atr_value(a));
    tbp = buf;
    rbp = rbuf;
    memset(rbuf, '\0', BUFFER_LEN);
    process_expression(rbuf, &rbp, (const char **) &tbp, d->pinfo.object,
                       d->pinfo.object, d->pinfo.object, PE_DEFAULT,
                       PT_DEFAULT, (PE_Info *) NULL);

    *rbp = '\0';
    if (PromptConnection(d)) {
      if (ShowAnsiColor(d->player))
        snprintf(buf, BUFFER_LEN - 1, "%s %c%c", rbuf, IAC, GOAHEAD);
      else
        snprintf(buf, BUFFER_LEN - 1, "%s %c%c",
                 remove_markup(rbuf, NULL), IAC, GOAHEAD);
    } else {
      if (ShowAnsiColor(d->player))
        snprintf(buf, BUFFER_LEN - 1, "%s\r\n", rbuf);
      else
        snprintf(buf, BUFFER_LEN - 1, "%s\r\n", remove_markup(rbuf, NULL));
    }
  } else {
    if (PromptConnection(d)) {
      if (ShowAnsi(d->player))
        snprintf(buf, BUFFER_LEN - 1, "%s>%s %c%c", ANSI_HILITE,
                 ANSI_NORMAL, IAC, GOAHEAD);
      else
        snprintf(buf, BUFFER_LEN - 1, "> %c%c", IAC, GOAHEAD);
    } else {
      if (ShowAnsi(d->player))
        snprintf(buf, BUFFER_LEN - 1, "%s>%s\r\n", ANSI_HILITE,
                 ANSI_NORMAL);
      else
        snprintf(buf, BUFFER_LEN - 1, ">\r\n");
    }
  }
  queue_newwrite(d, (unsigned char *) buf, strlen(buf));
  process_output(d);
}

void
prog_place(dbref player, dbref object, ATTR * patr, int lock)
{
  DESC *d;
  char str[BUFFER_LEN];

  twiddle_flag_internal("FLAG", player, "INPROGRAM", 0);
  tooref = ooref, ooref = NOTHING;
  sprintf(str, "%d %s %d", object, patr->name, (lock & PI_LOCK) ? 1 : 0);
  if (!(lock & PI_PROMPT)) {
    atr_clr(player, "XY_PROGPROMPT", GOD);
  }
  (void) atr_add(player, "XY_PROGINFO", str, GOD, NOTHING);
  ooref = tooref;
  DESC_ITER_CONN(d)
      if (d->player == player) {
    d->pinfo.lock = lock;
    prog_load_desc(d);
  }
}
