/* rplog.c -> @rplog by Nveid.
 * 01/29/05 - RLB - Created File and began work on RPLog Code
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

#define RPBUF_SIZE 1
#define RPBUF_MAXSIZE 20
#define RPRESET_TIME 86400 /* How long it'll be till statuses get reset */

#ifdef RPMODE_SYS
static void rplog_status(dbref room, char status);
void init_rplogs();
void rplog_room(dbref room, dbref player, char *str);
static void rplog_recall(dbref player, dbref room, char quiet, int lines);
static int expire_bufferq(BUFFERQ *,time_t);
void rplog_shutdown();


void init_rplogs() {
  dbref current;

  for(current = 0 ; current < db_top ; current++)
    if(has_flag_by_name(current, "ICFUNCS", TYPE_ROOM)) {
	db[current].rplog.bufferq = allocate_bufferq(RPBUF_SIZE); 
	db[current].rplog.status = 0;
    } else { 
      db[current].rplog.bufferq = NULL;
    }
}

void rplog_init_room(dbref room) {
  db[room].rplog.bufferq = allocate_bufferq(RPBUF_SIZE);
  db[room].rplog.status = 0;
}

void  rplog_shutdown_room(dbref room) {
  if(db[room].rplog.bufferq)
   free_bufferq(db[room].rplog.bufferq); 
}

void rplog_room(dbref room, dbref player, char *str) {
  int lines; 

  /* FIXME: i'm doing this all wrong num_buffered has nothing to do with
   * bufferq_lines.. perhaps create a dynamic add_to_bufferq allocating
   * fucker thingie. */

  if(db[room].rplog.bufferq && has_flag_by_name(room, "ICFUNCS", TYPE_ROOM)) {
    if(db[room].rplog.status == 1) { /* Check Size.. We might need to make it bigger */
      if(db[room].rplog.bufferq->num_buffered == (lines = 
	    bufferq_lines(db[room].rplog.bufferq)) && lines < RPBUF_MAXSIZE )
	reallocate_bufferq(db[room].rplog.bufferq,lines+1); 
    }
    add_to_bufferq(db[room].rplog.bufferq, 0, player, str); 
  }
}

/* 1- Preserve logs past scroll out
 * 0- Reset Room
 */
static void rplog_status(dbref room, char status) {
  if(!db[room].rplog.bufferq)
    return;
  db[room].rplog.status = status;
  if(status == 0)  /* expire old message and reset status */
    expire_bufferq(db[room].rplog.bufferq, (mudtime-(60*20)));
}


void rplog_reset() {
  int i;
  char *bq;
  time_t timeof;
  if((mudtime % 3600)==0) { /* reset all things in a combat status 
			     * that have been ina  combat status over 24 hours */
    for(i = 0; i < db_top; i++)
      if(IsGarbage(i))
	continue;
      else if(db[i].rplog.bufferq != NULL) {
	if(db[i].rplog.status == 1) {
	  bq = db[i].rplog.bufferq->buffer;
	  bq += sizeof(int) + sizeof(dbref) + sizeof(int);
	  /* K.. we're pointed here.. make sure we're not greater than the buffer end though... */
	  if(bq >= db[i].rplog.bufferq->buffer_end) /* OOo... we are, nothings ever been done here.. So lets continue */
	    continue;
	  memcpy(&timeof, bq, sizeof(time_t)); 
	  if(timeof < (mudtime-RPRESET_TIME)) {
	    /* make sure its small.. */
	    reallocate_bufferq(db[i].rplog.bufferq,  RPBUF_SIZE);
	    expire_bufferq(db[i].rplog.bufferq, (time_t) (mudtime - 1200));
	  }
	} else { /* Otherwise just do message expirations */
	  expire_bufferq(db[i].rplog.bufferq, (time_t) (mudtime - 1200));
	}
      }
  }
}

COMMAND(cmd_rplog) {
  dbref room;
  char quiet = 0;
  int lines;


  if(arg_left && *arg_left) {
    room = noisy_match_result(player, arg_left, TYPE_ROOM, MAT_EVERYTHING);
    if(room == NOTHING)
      return;
    if(!IsRoom(room)) {
      notify(player, "Must be a room.");
      return;
    }
  } else room = absolute_room(player);

  if(!GoodObject(room)) {
    notify(player, "Can't find your absolute location.");
    return;
  }

  if(!has_flag_by_name(room, "ICFUNCS", TYPE_ROOM)) {
    notify(player, "You must use this command from an IC Room.");
    return;
  }

  if(SW_ISSET(sw, SWITCH_COMBAT)) {
    rplog_status(room, 1);
    notify(player, "Combat Logging Triggered.");
    return;
  }

  if(SW_ISSET(sw, SWITCH_RESET)) {
    rplog_status(room, 0);
    notify(player, "Room Resetted.");
    return;
  }

  if(SW_ISSET(sw, SWITCH_QUIET))
    quiet = 1;
  if(arg_left)
    lines = parse_integer(arg_left);
  else 
    lines = 0;
  if(db[room].rplog.status)
    rplog_recall(player, room, quiet, lines);
  else
    notify(player, "The room must be in a combat status to view the buffer.");
}

FUNCTION(fun_crplog) {
  dbref room;
  if(!args[0])
    room = absolute_room(executor);
  else
    room = parse_dbref(args[0]);
  if(!GoodObject(room)) {
    safe_str("#-1 INVALID OBJECT", buff, bp);
    return;
  }

  if(!has_flag_by_name(room, "ICFUNCS", TYPE_ROOM)) {
    safe_str("#-1 NOT AN IC ROOM", buff, bp);
    return;
  }

  safe_chr(( db[room].rplog.status ? '1' : '0' ), buff, bp);
}

/* View Buffered Room & what not */
static void rplog_recall(dbref player, dbref room, char quiet, int lines) {
  int num_lines;
  time_t timestamp;
  char *stamp, *buf, *p = NULL;
  int skip;
  dbref speaker;
  int type;

  if(!GoodObject(player) || !GoodObject(room)) {
    return;
  }

  if(!IsRoom(room)) {
    notify(player, "You have to be a room to use this.");
    return;
  }

  if(isempty_bufferq(db[room].rplog.bufferq)) {
      notify(player, "Nothing to see");
      return;
  }

  num_lines = lines > 0 ? lines : db[room].rplog.bufferq->num_buffered;

  skip = BufferQNum(db[room].rplog.bufferq) - num_lines;

  do {
    buf = iter_bufferq(db[room].rplog.bufferq, &p, &speaker, &type, &timestamp);
    if (skip <= 0) {
      if (buf) {
        if (Nospoof(player) && GoodObject(speaker)) {
          char *nsmsg =
            ns_esnotify(speaker, na_one, &player, Paranoid(player) ? 1 : 0);
          if (quiet)
            notify_format(player, "%s %s", nsmsg, buf);
          else {
            stamp = show_time(timestamp, 0);
            notify_format(player, "[%s] %s %s", stamp, nsmsg, buf);
          }
          mush_free(nsmsg, "string");
        } else {
          if (quiet)
            notify(player, buf);
          else {
            stamp = show_time(timestamp, 0);
            notify_format(player, "[%s] %s", stamp, buf);
          }
	}
      }
    }
    skip--;
  } while(buf);

}

void rplog_shutdown(void) {
  dbref cur_obj;

  for(cur_obj = 0 ; cur_obj < db_top ; cur_obj++)
    if(has_flag_by_name(cur_obj, "ICFUNCS", TYPE_ROOM) 
	&& (db[cur_obj].rplog.bufferq)) {
      free_bufferq(db[cur_obj].rplog.bufferq);
      db[cur_obj].rplog.bufferq = NULL;
      db[cur_obj].rplog.status = 0;
    }
}

/* This is going to be a cool trick... go through this bufferq & expire old messages */
static int expire_bufferq(BUFFERQ *bq, time_t time) {
  /* First.. Loop though all the messages to find the time of the message we're expiring */
  char *p = bq->buffer, *w;
  time_t time_at;
  int size, jump;
  int skipped = 0;

  while((p < bq->buffer_end)) {
    memcpy(&size, p, sizeof(size));
    jump = size + BUFFERQLINEOVERHEAD + 1;
    w = p;
    w += sizeof(size) + sizeof(dbref) + sizeof(int);
    memcpy(&time_at, w, sizeof(time_t));
    if((time_at < time))
      p += jump;
    else
      break;
    skipped++;
  }
  if(p < bq->buffer_end)
    memmove(bq->buffer, p , bq->buffer_end - p);
  bq->buffer_end -= (p - bq->buffer);
  bq->num_buffered -= skipped;
  return skipped;
}
#endif /* RPMODE_SYS */
