/**
 * \file rob.c
 *
 * \brief Kill and give.
 *
 * This file is called rob.c for historical reasons, and one day it'll
 * probably get folded into some other file.
 *
 *
 */

#include "config.h"
#include "copyrite.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "conf.h"
#include "externs.h"
#include "mushdb.h"
#include "attrib.h"
#include "match.h"
#include "parse.h"
#include "flags.h"
#include "log.h"
#include "lock.h"
#include "dbdefs.h"
#include "game.h"
#include "confmagic.h"
#include "case.h"

static void do_give_to(dbref player, char *arg, int silent);

/** The give command.
 * \param player the enactor/giver.
 * \param recipient name of object to receive.
 * \param amnt name of object to be transferred, or amount of pennies.
 * \param silent if 1, hush the usual messages.
 */
void
do_give(dbref player, char *recipient, char *amnt, int silent)
{
  dbref who;
  int amount;
  char tbuf1[BUFFER_LEN];
  char *bp;

  /* If we have a recipient, but no amnt, try parsing for
   * 'give <amnt> to <recipient>' instead of 'give <recipient>=<amount>'
   */
  if (recipient && *recipient && (!amnt || !*amnt)) {
    do_give_to(player, recipient, silent);
    return;
  }

  /* check recipient */
  switch (who =
          match_result(player, recipient, TYPE_PLAYER,
                       MAT_NEAR_THINGS | MAT_ENGLISH)) {
  case NOTHING:
    notify(player, T("Give to whom?"));
    return;
  case AMBIGUOUS:
    notify(player, T("I don't know who you mean!"));
    return;
  }

  /* Can't give to garbage... */
  if (IsGarbage(who)) {
    notify(player, T("Give to whom?"));
    return;
  }

  if (!is_strict_integer(amnt)) {
    dbref thing;
    switch (thing =
            match_result(player, amnt, TYPE_THING,
                          MAT_POSSESSION | MAT_ENGLISH)) {
    case NOTHING:
        notify(player, T("You don't have that!"));
      return;
    case AMBIGUOUS:
      notify(player, T("I don't know which you mean!"));
      return;
    default:
      /* if you can give yourself, that's like "enter". since we
       * do no lock check with give, we shouldn't be able to
       * do this.
       */
      if (thing == player) {
        notify(player, T("You can't give yourself away!"));
        return;
      }
      /* Don't give things to themselves. */
      if (thing == who) {
        notify(player, T("You can't give an object to itself!"));
        return;
      }
      if (!eval_lock(player, thing, Give_Lock)) {
        notify(player, T("You can't give that away."));
        return;
      }
      if (Mobile(thing) && (EnterOk(who) || controls(player, who))) {
        moveto(thing, who);

        /* Notify the giver with their GIVE message */
        bp = tbuf1;
        safe_format(tbuf1, &bp, T("You gave %s to %s."), Name(thing),
                    Name(who));
        *bp = '\0';
        did_it_with(player, player, "GIVE", tbuf1, "OGIVE", NULL,
                    "AGIVE", NOTHING, thing, who, NA_INTER_SEE);

        /* Notify the object that it's been given */
        notify_format(thing, T("%s gave you to %s."), Name(player), Name(who));

        /* Recipient gets success message on thing and receive on self */
        did_it(who, thing, "SUCCESS", NULL, "OSUCCESS", NULL, "ASUCCESS",
               NOTHING);
        bp = tbuf1;
        safe_format(tbuf1, &bp, T("%s gave you %s."), Name(player),
                    Name(thing));
        *bp = '\0';
        did_it_with(who, who, "RECEIVE", tbuf1, "ORECEIVE", NULL,
                    "ARECEIVE", NOTHING, thing, player, NA_INTER_SEE);
      } else
        notify(player, T("Permission denied."));
    }
    return;
  }
  amount = parse_integer(amnt);
  /* do amount consistency check */
  if (Pennies(who) + amount > Max_Pennies(who))
    amount = Max_Pennies(who) - Pennies(who);
  if (amount < 0 && !(MoneyAdmin(player) && controls(player, who))) {
    notify(player, T("What is this, a holdup?"));
    return;
  } else if (amount == 0) {
    notify_format(player,
                  T("You must specify a positive number of %s."), MONIES);
    return;
  }
  if (MoneyAdmin(player) && (amount < 0) && (Pennies(who) + amount < 0))
     amount = -Pennies(who);
  /* try to do the give */
  if (!MoneyAdmin(player) && !payfor(player, amount)) {
    notify_format(player, T("You don't have that many %s to give!"), MONIES);
  } else {
    char *pay_env[10] = { NULL };
    char paid[SBUF_LEN], *pb;

    pay_env[0] = pb = paid;

    /* objects work differently */
    if (IsThing(who)) {
      /* give pennies to an object */
      int cost = 0;
      ATTR *a;
      char *preserveq[NUMQ];
      char *preserves[10];
      char fbuff[BUFFER_LEN];
      char *fbp, *asave;
      char const *ap;

      a = atr_get(who, "COST");
      if (!a) {
        /* No cost attribute */
        notify_format(player, T("%s refuses your money."), Name(who));
        giveto(player, amount);
        return;
      }
      save_global_regs("give_save", preserveq);
      save_global_env("give_save", preserves);
      asave = safe_atr_value(a);
      ap = asave;
      fbp = fbuff;
      safe_integer_sbuf(amount, paid, &pb);
      *pb = '\0';
      global_eval_context.wenv[0] = paid;
      process_expression(fbuff, &fbp, &ap, who, player, player,
                         PE_DEFAULT, PT_DEFAULT, NULL);
      *fbp = '\0';
      free((Malloc_t) asave);
      restore_global_regs("give_save", preserveq);
      restore_global_env("give_save", preserves);
      if (amount < (cost = atoi(fbuff))) {
        notify(player, T("Feeling poor today?"));
        giveto(player, amount);
        return;
      }
      if (cost < 0)
        return;
      if ((amount - cost) > 0) {
        notify_format(player, T("You get %d in change."), amount - cost);
      } else {
        notify_format(player, T("You paid %d %s."), amount,
                      ((amount == 1) ? MONEY : MONIES));
      }
      giveto(player, amount - cost);
      giveto(who, cost);
      pb = paid;
      safe_integer_sbuf(cost, paid, &pb);
      *pb = '\0';
      real_did_it(player, who, "PAYMENT", NULL, "OPAYMENT", NULL, "APAYMENT",
                  NOTHING, pay_env, NA_INTER_SEE);
      return;
    } else {
      /* give pennies to a player */
      if (amount > 0) {
        notify_format(player,
                      T("You give %d %s to %s."), amount,
                      ((amount == 1) ? MONEY : MONIES), Name(who));
      } else {
        notify_format(player, T("You took %d %s from %s!"), abs(amount),
                      ((abs(amount) == 1) ? MONEY : MONIES), Name(who));
      }
      if (IsPlayer(who) && !silent) {
        if (amount > 0) {
          notify_format(who, T("%s gives you %d %s."), Name(player),
                        amount, ((amount == 1) ? MONEY : MONIES));
        } else {
          notify_format(who, T("%s took %d %s from you!"), Name(player),
                        abs(amount), ((abs(amount) == 1) ? MONEY : MONIES));
        }
      }
      giveto(who, amount);
      safe_integer_sbuf(amount, paid, &pb);
      *pb = '\0';
      real_did_it(player, who, "PAYMENT", NULL, "OPAYMENT", NULL, "APAYMENT",
                  NOTHING, pay_env, NA_INTER_SEE);
    }
  }
}

/** the buy command
 * \param player the enactor/buyer
 * \param item the item to buy
 * \param from who to buy it from
 * \param cost the cost
 */
void
do_buy(dbref player, char *item, char *from, int price)
{
  dbref vendor;
  char *prices;
  char *plus;
  char *cost;
  char finditem[BUFFER_LEN];
  char buycost[BUFFER_LEN];
  int boughtit;
  int len;
  char buff[BUFFER_LEN], *bp;
  char obuff[BUFFER_LEN];
  char *r[BUFFER_LEN / 2];
  char *c[BUFFER_LEN / 2];
  char *buy_env[10] = { NULL };
  int affordable;
  int costcount, ci;
  int count, i;
  int low, high;                /* lower bound, upper bound of cost */
  ATTR *a;

  if (!GoodObject(Location(player)))
    return;

  vendor = Contents(Location(player));
  if (vendor == player)
    vendor = Next(player);

  if (from != NULL && *from) {
    switch (vendor =
            match_result(player, from, TYPE_PLAYER,
                         MAT_NEAR_THINGS | MAT_ENGLISH)) {
    case NOTHING:
      notify(player, T("Buy from whom?"));
      return;
    case AMBIGUOUS:
      notify(player, T("I don't know who you mean!"));
      return;
    }
    if (vendor == player) {
      notify(player, T("You can't buy from yourself!"));
      return;
    }
  } else if (vendor == NOTHING) {
    notify(player, T("There's nobody here to buy things from."));
    return;
  } else {
    from = NULL;
  }

  if (!item || !*item || !(item = trim_space_sep(item, ' '))) {
    notify(player, T("Buy what?"));
    return;
  }

  bp = finditem;
  safe_str(item, finditem, &bp);
  safe_chr(':', finditem, &bp);
  *bp = '\0';
  for (bp = strchr(finditem, ' '); bp; bp = strchr(bp, ' '))
    *bp = '_';

  len = strlen(finditem);

  /* Scan pricelists */
  boughtit = -1;
  affordable = 1;
  do {
    a = atr_get(vendor, "PRICELIST");
    if (!a)
      continue;
    /* atr_value uses a static buffer, so we'll take advantage of that */
    prices = atr_value(a);
    upcasestr(prices);
    count = list2arr(r, BUFFER_LEN / 2, atr_value(a), ' ');
    if (!count)
      continue;
    for (i = 0; i < count; i++) {
      if (!strncasecmp(finditem, r[i], len)) {
        /* Check cost */
        cost = r[i] + len;
        if (!*cost)
          continue;
        costcount = list2arr(c, BUFFER_LEN / 2, cost, ',');
        for (ci = 0; ci < costcount; ci++) {
          cost = c[ci];
          /* Formats:
           * 10,2000+,10-100
           */
          if ((plus = strchr(cost, '-'))) {
            *(plus++) = '\0';
            if (!is_strict_integer(cost))
              continue;
            if (!is_strict_integer(plus))
              continue;
            low = parse_integer(cost);
            high = parse_integer(plus);
            if (price < 0) {
              boughtit = low;
            } else if (price >= low && price <= high) {
              boughtit = price;
            }
          } else if ((plus = strchr(cost, '+'))) {
            *(plus++) = '\0';
            if (!is_strict_integer(cost))
              continue;
            low = parse_integer(cost);
            if (price < 0) {
              boughtit = low;
            } else if (price > low) {
              boughtit = price;
            }
          } else if (is_strict_integer(cost)) {
            low = parse_integer(cost);
            if (price < 0) {
              boughtit = low;
            } else if (low == price) {
              boughtit = price;
            }
          } else {
            continue;
          }
          if (boughtit >= 0) {
            if (!payfor(player, boughtit)) {
              affordable = 0;
              boughtit = 0;
              continue;
            }
            bp = strchr(finditem, ':');
            if (bp)
              *bp = '\0';
            for (bp = finditem; *bp; bp++)
              *bp = DOWNCASE(*bp);
            bp = buff;
            safe_format(buff, &bp, "You buy a %s from %s.",
                        finditem, Name(vendor));
            *bp = '\0';
            bp = obuff;
            safe_format(obuff, &bp, "buys a %s from %s.",
                        finditem, Name(vendor));
            buy_env[0] = finditem;
            buy_env[1] = buycost;
            bp = buycost;
            safe_integer(boughtit, buycost, &bp);
            *bp = '\0';
            real_did_it(player, vendor, "BUY", buff, "OBUY", obuff, "ABUY",
                        NOTHING, buy_env, NA_INTER_SEE);
            return;
          }
        }
      }
    }
  } while (!from && ((vendor = Next(vendor)) != NOTHING));

  if (price >= 0) {
    if (!from) {
      notify(player, T("I can't find that item with that price here."));
    } else {
      notify_format(player, T("%s isn't selling that item for that price"),
                    Name(vendor));
    }
  } else if (affordable) {
    if (!from) {
      notify(player, T("I can't find that item here."));
    } else {
      notify_format(player, T("%s isn't selling that item."), Name(vendor));
    }
  } else {
    notify(player, T("You can't afford that."));
  }
}


void s_Pennies(dbref thing, int amnt) {
        if(amnt > HUGE_INT)
                Pennies(thing) = (int) HUGE_INT;
        else if(amnt < 0)
                Pennies(thing) = 0;
        else Pennies(thing) = amnt;
}


/** The other syntax of the give command.
 * \param player the enactor/giver.
 * \param arg "something to someone".
 * \param silent if 1, hush the usual messages.
 */
static void
do_give_to(dbref player, char *arg, int silent)
{
  char *s;

  /* Parse out the object and recipient */
  upcasestr(arg);
  s = (char *) string_match(arg, "TO ");
  if (!s) {
    notify(player, T("Did you want to give something *to* someone?"));
    return;
  }
  while ((s > arg) && isspace(*(s - 1))) {
    s--;
  }
  if (s == arg) {
    notify(player, T("Give what?"));
    return;
  }
  *s++ = '\0';
  s = (char *) string_match(s, "TO ");
  s += 3;
  while (*s && isspace(*s))
    s++;
  if (!*s) {
    notify(player, T("Give to whom?"));
    return;
  }
  /* At this point, 'arg' is the object, and 's' is the recipient.
   * But be double-safe to be sure we don't loop.
   */
  if (!*arg || !*s) {
    notify(player, T("I don't know what you mean."));
    return;
  }
  do_give(player, s, arg, silent);
  return;
}
