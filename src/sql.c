/**
 * \file sql.c
 *
 * \brief Code to support PennMUSH connection to SQL databases.
 *
 *
 */

#include "copyrite.h"
#include "config.h"
#ifdef I_UNISTD
#include <unistd.h>
#endif
#include <string.h>
#include "conf.h"
#ifdef HAS_MYSQL
#include <mysql/mysql.h>
#include <mysql/errmsg.h>
#endif
#include "externs.h"
#include "access.h"
#include "log.h"
#include "parse.h"
#include "boolexp.h"
#include "command.h"
#include "function.h"
#include "mushdb.h"
#include "dbdefs.h"
#include "conf.h"
#include "confmagic.h"

#ifdef HAS_MYSQL
static MYSQL *mysql_struct = NULL;
#define MYSQL_RETRY_TIMES 3

static int safe_sql_query(dbref player, char *q_string, char row_delim,
			  char field_delim, char *buff, char **bp);
void sql_timer();
static int sql_init(void);
#ifdef _SWMP_
void sql_startup();
void sqenv_clear(int);
static int sqllist_check(dbref thing, dbref player, char type, char end, char *str, int just_match);
int sql_env[2];
#endif 
static char sql_up = 0;

void
sql_shutdown(void)
{
  sql_up = 0;
  if (!mysql_struct)
    return;
  mysql_close(mysql_struct);
  mysql_struct = NULL;
}

void sql_startup() {
	sql_init();

	/* Delete All Entries in Auth_T Table to start with */
	if(mysql_struct) mysql_query(mysql_struct, "DELETE FROM auth_t");
	sql_up = 1;
#ifdef _SWMP_
	sql_env[0] = -1;
	sql_env[1] = -1;
#endif
}

static int
sql_init(void)
{
  int retries = MYSQL_RETRY_TIMES;

  /* If we are already connected, drop and retry the connection, in
   * case for some reason the server went away.
   */
  if (mysql_struct)
    sql_shutdown();

  if (!strcasecmp(SQL_PLATFORM, "mysql")) {
    while (retries && !mysql_struct) {
      /* Try to connect to the database host. If we have specified
       * localhost, use the Unix domain socket instead.
       */
      mysql_struct = mysql_init(NULL);

      if (!mysql_real_connect
	  (mysql_struct, SQL_HOST, SQL_USER, SQL_PASS, SQL_DB, 3306, 0, 0)) {
	do_rawlog(LT_ERR, "Failed mysql connection: %s\n",
		  mysql_error(mysql_struct));
	sql_shutdown();
	sleep(1);
      }
      retries--;
    }
  }

  if (mysql_struct)
    return 1;
  else
    return 0;
}

#define print_sep(s,b,p)  if (s) safe_chr(s,b,p)

static int
safe_sql_query(dbref player, char *q_string, char row_delim, char field_delim,
	       char *buff, char **bp)
{
  MYSQL_RES *qres;
  MYSQL_ROW row_p;
  int num_rows, got_rows, got_fields;
  int i, j;

  /* If we have no connection, and we don't have auto-reconnect on
   * (or we try to auto-reconnect and we fail), this is an error
   * generating a #-1. Notify the player, too, and set the return code.
   */

  if (!mysql_struct) {
    sql_init();
    if (!mysql_struct) {
      notify(player, "No SQL database connection.");
      if (buff)
	safe_str("#-1", buff, bp);
      return -1;
    }
  }
  if (!q_string || !*q_string)
    return 0;

  /* Send the query. */
  got_rows = mysql_real_query(mysql_struct, q_string, strlen(q_string));
  if (got_rows && (mysql_errno(mysql_struct) == CR_SERVER_GONE_ERROR)) {
    /* We got this error because the server died unexpectedly
     * and it shouldn't have. Try repeatedly to reconnect before
     * giving up and failing. This induces a few seconds of lag,
     * depending on number of retries
     */
    sql_init();
    if (mysql_struct)
      got_rows = mysql_real_query(mysql_struct, q_string, strlen(q_string));
  }
  if (got_rows) {
    notify(player, mysql_error(mysql_struct));
    if (buff)
      safe_str("#-1", buff, bp);
    return -1;
  }

  /* Get results. A silent query (INSERT, UPDATE, etc.) will return NULL */
  qres = mysql_store_result(mysql_struct);
  if (!qres) {
    if (!mysql_field_count(mysql_struct)) {
      /* We didn't expect data back, so see if we modified anything */
      num_rows = mysql_affected_rows(mysql_struct);
      notify_format(player, "SQL: %d rows affected.", num_rows);
      return 0;
    } else {
      /* Oops, we should have had data! */
      notify_format(player, "SQL: Error: %s", mysql_error(mysql_struct));
      return -1;
    }
  }

  /* At this point, we know we've done something like SELECT and 
   * we got results. But just in case...
   */
  got_rows = mysql_num_rows(qres);
  if (!got_rows)
    return 0;
  got_fields = mysql_num_fields(qres);

  /* Construct properly-delimited data. */
  if (buff) {
    for (i = 0; i < got_rows; i++) {
      if (i > 0) {
	print_sep(row_delim, buff, bp);
      }
      row_p = mysql_fetch_row(qres);
      if (row_p) {
	for (j = 0; j < got_fields; j++) {
	  if (j > 0) {
	    print_sep(field_delim, buff, bp);
	  }
	  if (row_p[j] && *row_p[j])
	    if (safe_str(row_p[j], buff, bp))
	      goto finished;	/* We filled the buffer, best stop */
	}
      }
    }
  } else {
    for (i = 0; i < got_rows; i++) {
      row_p = mysql_fetch_row(qres);
      if (row_p) {
	for (j = 0; j < got_fields; j++) {
	  if (row_p[j] && *row_p[j]) {
	    notify(player, tprintf("Row %d, Field %d: %s",
				   i + 1, j + 1, row_p[j]));
	  } else {
	    notify(player, tprintf("Row %d, Field %d: NULL", i + 1, j + 1));
	  }
	}
      } else {
	notify(player, tprintf("Row %d: NULL", i + 1));
      }
    }
  }

finished:
  mysql_free_result(qres);
  return 0;
}


FUNCTION(fun_sql)
{
  char row_delim, field_delim;

  if (!Sql_Ok(executor)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }

  if (nargs >= 2) {
    /* we have a delimiter in args[2]. Got to parse it */
    char insep[BUFFER_LEN];
    char *isep = insep;
    const char *arg2 = args[1];
    process_expression(insep, &isep, &arg2, executor, caller, enactor,
		       PE_DEFAULT, PT_DEFAULT, pe_info);
    *isep = '\0';
    strcpy(args[1], insep);
  }

  if (nargs >= 3) {
    /* we have a delimiter in args[3]. Got to parse it */
    char insep[BUFFER_LEN];
    char *isep = insep;
    const char *arg3 = args[2];
    process_expression(insep, &isep, &arg3, executor, caller, enactor,
		       PE_DEFAULT, PT_DEFAULT, pe_info);
    *isep = '\0';
    strcpy(args[2], insep);
  }

  if (!delim_check(buff, bp, nargs, args, 2, &row_delim))
    return;
  if (nargs < 3)
    field_delim = ' ';
  else if (!delim_check(buff, bp, nargs, args, 3, &field_delim))
    return;

  safe_sql_query(executor, args[0], row_delim, field_delim, buff, bp);
}


FUNCTION(fun_sql_escape)
{
  char bigbuff[BUFFER_LEN * 2 + 1];

  if (!Sql_Ok(executor)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }

  if (!args[0] || !*args[0])
    return;

  if (!mysql_struct) {
    sql_init();
    if (!mysql_struct) {
      notify(executor, "No SQL database connection.");
      safe_str("#-1", buff, bp);
      return;
    }
  }
  if (mysql_real_escape_string(mysql_struct, bigbuff, args[0], strlen(args[0]))
      < BUFFER_LEN)
    safe_str(bigbuff, buff, bp);
  else
    safe_str("#-1 TOO LONG", buff, bp);
}


COMMAND (cmd_sql) {
  safe_sql_query(player, arg_left, ' ', ' ', NULL, NULL);
}
#ifdef _SWMP_ /* SWM Protocol */
/* Do secondly checks on Authentication Table & Query Tables */
void sql_timer() {
	/* query variables */
	MYSQL_RES *qres, *qres2;
	MYSQL_ROW row_p, row_p2;
	int num_rows = 0 , got_rows = 0, got_fields = 0;
	int got_fields2 = 0, got_rows2 = 0;
	int i, j;
	char *str;
	char buf[BUFFER_LEN];
	dbref player;

	qres = NULL;
	qres2 = NULL;

	if(!sql_up || !GoodObject(SQLCMD_MasterRoom))
		return;

        if(!mysql_struct) {
                if((mudtime % 60) == 0) { /* If its down, try every minute to get it back up. */
                        sql_init();
			if(!mysql_struct)
				return;
		} else return;
        }

	/* Before we do anything lets delete old shit */

        mysql_query(mysql_struct, tprintf("DELETE FROM auth_t WHERE last < %d", mudtime-1800)); 

	/* We're connected.. Check Authentication Table */
	got_rows = mysql_query(mysql_struct, "SELECT * FROM auth_t WHERE authcode=0"); 

	if(got_rows != 0) {
	  do_log(LT_ERR, 0, 0, "SQL-> (%s:%d) %s", __FILE__, __LINE__, mysql_error(mysql_struct));
	  goto query_table;
	}

	qres = mysql_store_result(mysql_struct);
     if (qres == NULL) {
        /* Oops, we should have had data! */
        if(!mysql_field_count(mysql_struct))
	  goto query_table;
     }

     got_rows = mysql_num_rows(qres);
     if(got_rows < 1) 
	     goto query_table;

     got_fields = mysql_num_fields(qres);

     if(got_fields < 6 || got_fields > 6) {
       do_log(LT_ERR, 0, 0, "SQL 'auth_t' table %s fields.", got_fields < 6 ? "not enough" : "too many");
       goto query_table;
     }

     for(i = 0, row_p = mysql_fetch_row(qres)  ; i < got_rows ; i++, row_p = mysql_fetch_row(qres)) {
	       if((player = lookup_player(row_p[4])) == NOTHING || 
	       	   !Site_Can_Connect((const char *) row_p[1],player ) ||
	       	   !password_check(player, (const char *)row_p[5])) {
			     /* Mark this row as bad auth, can't find user */
		 j =  -1;
	       } else j = 1;

	       if( mysql_query(mysql_struct, 
	       	   tprintf("UPDATE auth_t SET authcode=%d, pass=\"LOGGEDIN\", user=\"%d\" WHERE last = %d AND id = %d", 
	       	     j, player, atoi(row_p[3]), atoi(row_p[0]))) != 0) 
		 do_log(LT_ERR, 0, 0, "SQL-> (%s/%d) %s ", __FILE__, __LINE__, mysql_error(mysql_struct));
     }


query_table:
     if(qres != NULL) mysql_free_result(qres);

     /* Check Query Table */
     got_rows = mysql_query(mysql_struct, "SELECT * FROM query_t WHERE io=1");

     if(got_rows != 0) {
       do_log(LT_ERR, 0, 0, "SQL -> %d/%s : %s", __LINE__, __FILE__, mysql_error(mysql_struct));
       goto st_finish;
     }

      qres = mysql_store_result(mysql_struct);

      if(qres == NULL) {
	do_log(LT_ERR, 0 , 0, "SQL: Error(%s:%d): %s", __FILE__, __LINE__, mysql_error(mysql_struct));
	return;
      }

      got_rows = mysql_num_rows(qres);

      if(got_rows < 1)
	goto st_finish;
      got_fields = mysql_num_fields(qres);
      if(!got_fields)
	goto st_finish;
      if(got_fields < 4 || got_fields > 4) {
	do_log(LT_ERR, 0, 0, "SQL 'query_t' table %s fields.", got_fields < 4 ? "not enough" : "too many");
	goto st_finish;
      }

      for(i = 0, row_p = mysql_fetch_row(qres); i < got_rows && row_p != NULL ; i++, row_p = mysql_fetch_row(qres)) {
	sql_env[0] =  atoi(row_p[0]);
	sql_env[1] = atoi(row_p[1]);
	/* Load AUTHID we're assosciated with */
	got_rows2 = mysql_query(mysql_struct, 
	    tprintf("SELECT * FROM auth_t WHERE id = %d", atoi(row_p[1])));
	if(got_rows2 != 0) {
	  /* Update Return Result As No Authorized user logged in */
	  mysql_query(mysql_struct, tprintf("UPDATE query_t SET io=0, query=\"%s\" WHERE id = %d", mysql_error(mysql_struct),atoi(row_p[0]) ));
	  continue;

	}
	qres2 = mysql_store_result(mysql_struct);

	/* Not in auth_t table */
	if(qres2 == NULL) {
	  mysql_query(mysql_struct, tprintf("UPDATE query_t SET io=0, query=\"%s\" WHERE id = %d", mysql_error(mysql_struct), atoi(row_p[0]) ));
	  continue;
	}

	got_rows2 = mysql_num_rows(qres2);

        if(got_rows2 < 1) {
          /* Update Return Result As No Authorized user logged in */
	  mysql_free_result(qres2);
          mysql_query(mysql_struct, tprintf("UPDATE query_t SET io=0, query=\"not logged in\" WHERE id = %d", atoi(row_p[0]) ));
          continue;
        }
	/* Fucked up auth_t Table*/
	if(got_rows > 1) {
	  mysql_free_result(qres2);
	  mysql_query(mysql_struct, tprintf("UPDATE query_t SET io=0, query=\"ERROR: Identical User IDs logged in.\" WHERE id = %d", atoi(row_p[0]) ));
	  continue;
	}
	row_p2 = mysql_fetch_row(qres2);

	player = atoi(row_p2[4]);
	/* They Aren't Real */
	if(!GoodObject(player) || !IsPlayer(player)) {
	  mysql_free_result(qres2);
	  mysql_query(mysql_struct, tprintf("UPDATE query_t SET io=0, query=\"ERROR: NonExistent Player ID logged into.\" WHERE id = %d", atoi(row_p[0]) ));
	  /* Delete Auth Table ID Entry , they shouldn't be there*/
	  mysql_query(mysql_struct, tprintf("DELETE FROM auth_t WHERE user=\"%d\"", player));
	  /* Delete Us Too */
	  continue;

	}

	memset(buf, '\0', BUFFER_LEN);
	strncpy(buf, row_p[3], BUFFER_LEN-1);
	j = sqllist_check(Contents(SQLCMD_MasterRoom), player, '$', ':', buf, 0);
   //     j = list_check(Contents(SQLCMD_MasterRoom), player, '$', ':', buf, 0);
	if(j < 1) {
	  mysql_free_result(qres2);
	  mysql_query(mysql_struct, tprintf("UPDATE query_t SET query=\"-1\", io=0 WHERE id = %d", atoi(row_p[0]) ));
	  continue;
	}
	mysql_free_result(qres2);

	sql_env[0] =  -1;
	sql_env[1] =  -1;
      }
      sql_env[0] =  -1;
      sql_env[1] = -1;
st_finish:
     if(qres != NULL) mysql_free_result(qres);
     if(qres2 != NULL) mysql_free_result(qres2);
}

int
sqllist_check(dbref thing, dbref player, char type, char end, char *str,
           int just_match)
{
  int match = 0;

  while (thing != NOTHING) {
    if (atr_comm_match(thing, player, type, end, str, just_match, NULL, NULL, NULL))
      match = 1;
    else 
	match = 0;
    
    thing = Next(thing);
  }
  return (match);
}


/* SQ_Respond(<response>) - the way commands queued will respond to this type of query */
FUNCTION(fun_sq_respond) {
  MYSQL_RES *qres;
  int got_rows;

  
  /* SQL Environment should be set to use this function, if not deny usage */
  if(sql_env[0] == -1 || sql_env[1] == -1) {
    safe_str("#-1 NOTHING TO RESPOND TO", buff, bp);
    return;
  }

  if(!sql_up) {
    safe_str("#-1 SQL Server Down", buff, bp);
    return;
  }

  if(!mysql_struct) { 
    sql_init();
    if(!mysql_struct) {
      safe_str("#-1 SQL Server Down", buff, bp);
      return;
    }
  }

  /* For some reason we're having some unfreed result here.. */
  mysql_free_result(mysql_store_result(mysql_struct)); 
   got_rows = mysql_query(mysql_struct, tprintf("SELECT * FROM query_t WHERE id = %d", sql_env[0])); 
  if(got_rows != 0) {
    safe_str(tprintf("#-1 %s", mysql_error(mysql_struct)), buff, bp);
    return;
  }
  mysql_free_result(mysql_store_result(mysql_struct));

  /* Ok.. We do Exist, now Update */
  
  got_rows = 
    mysql_query(mysql_struct, tprintf("UPDATE query_t SET io = 0, query = \"%s\" WHERE id = %d", args[0], sql_env[0])); 

  if(got_rows != 0) {
    safe_format(buff, bp, "#-1 %s", mysql_error(mysql_struct));
    return;
  }
 safe_chr('1', buff, bp);
}

void sqenv_clear(int id) {

  sql_env[0] = -1;
  sql_env[0] = -1;

  if(!mysql_struct) {
    sql_init();
    if(!mysql_struct)
      return;
  }

  mysql_query(mysql_struct,  tprintf("UPDATE query_t SET io = 0 WHERE id = %d", id));
}

#else
FUNCTION(fun_sq_respond) {
}

void sql_timer() {
}
#endif

#else

/* Oops, no SQL */

FUNCTION(fun_sql)
{
  safe_str(T(e_disabled), buff, bp);
}

FUNCTION(fun_sql_escape)
{
  safe_str(T(e_disabled), buff, bp);
}

/* Do secondly checks on Authentication Table & Query Tables */
void sql_timer() {
	return;
}

void
sql_shutdown(void)
{
  return;
}

#endif
