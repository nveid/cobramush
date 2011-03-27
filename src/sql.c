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

static MYSQL_RES *sql_query(char *query_str, int *affected_rows);
static void free_sql_query(MYSQL_RES * qres);
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
  static time_t last_retry = 0;
  time_t curtime;

  /* Only retry at most once per minute. */
  curtime = time(NULL);
  if (curtime < (last_retry + 60))
    return 0;
  last_retry = curtime;

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

static MYSQL_RES *
sql_query(char *q_string, int *affected_rows)
{
  MYSQL_RES *qres;
  int fail;

  /* No affected rows by default */
  *affected_rows = -1;

  /* Make sure we have something to query, first. */
  if (!q_string || !*q_string)
    return NULL;

  /* If we have no connection, and we don't have auto-reconnect on
   * (or we try to auto-reconnect and we fail), return NULL.
   */

  if (!mysql_struct) {
    sql_init();
    if (!mysql_struct) {
      return NULL;
    }
  }

  /* Send the query. If it returns non-zero, we have an error. */
  fail = mysql_real_query(mysql_struct, q_string, strlen(q_string));
  if (fail && (mysql_errno(mysql_struct) == CR_SERVER_GONE_ERROR)) {
    /* If it's CR_SERVER_GONE_ERROR, the server went away.
     * Try reconnecting. */
    sql_init();
    if (mysql_struct)
      fail = mysql_real_query(mysql_struct, q_string, strlen(q_string));
  }
  /* If we still fail, it's an error. */
  if (fail) {
    return NULL;
    }

  /* Get the result */
  qres = mysql_use_result(mysql_struct);
  if (!qres) {
    if (!mysql_field_count(mysql_struct)) {
      /* We didn't expect data back, so see if we modified anything */
      *affected_rows = mysql_affected_rows(mysql_struct);
      return NULL;
    } else {
      /* Oops, we should have had data! */
      return NULL;
    }
  }
  return qres;
}

void
free_sql_query(MYSQL_RES * qres)
{
  MYSQL_ROW row_p;
  while ((row_p = mysql_fetch_row(qres)) != NULL) ;
  mysql_free_result(qres);
}

FUNCTION(fun_mapsql)
{
  MYSQL_RES *qres;
  MYSQL_ROW row_p;
  ufun_attrib ufun;
  char *wenv[10];
  char *osep = (char *) " ";
  int affected_rows;
  int rownum;
  char numbuff[20];
  int numfields;
  char rbuff[BUFFER_LEN];
  int funccount = 0;
  int do_fieldnames = 0;
  int i;
  MYSQL_FIELD *fields;

  if (!Sql_Ok(executor)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }

  if (!fetch_ufun_attrib(args[0], executor, &ufun, 1))
    return;

  if (nargs > 2) {
    /* we have an output separator in args[2]. */
    osep = args[2];
  }

  if (nargs > 3) {
    /* args[3] contains a boolean, if we should pass
     * the field names first. */
    do_fieldnames = parse_boolean(args[3]);
  }

  for (i = 0; i < 10; i++)
    wenv[i] = NULL;

  qres = sql_query(args[1], &affected_rows);

  if (!qres) {
    if (affected_rows >= 0) {
      notify_format(executor, "SQL: %d rows affected.", affected_rows);
    } else if (!mysql_struct) {
      notify(executor, "No SQL database connection.");
    } else {
      notify_format(executor, "SQL: Error: %s", mysql_error(mysql_struct));
      safe_str("#-1", buff, bp);
    }
    return;
  }

  /* Get results. A silent query (INSERT, UPDATE, etc.) will return NULL */
  numfields = mysql_num_fields(qres);

  if (do_fieldnames) {
    fields = mysql_fetch_fields(qres);
    strncpy(numbuff, unparse_integer(0), 20);
    wenv[0] = numbuff;
    for (i = 0; i < numfields && i < 9; i++) {
      wenv[i + 1] = fields[i].name;
    }
    if (call_ufun(&ufun, wenv, i + 1, rbuff, executor, enactor, pe_info))
      goto finished;
    safe_str(rbuff, buff, bp);
  }

  rownum = 0;
  while ((row_p = mysql_fetch_row(qres)) != NULL) {
    if (rownum++ > 0 || do_fieldnames) {
      safe_str(osep, buff, bp);
    }
    strncpy(numbuff, unparse_integer(rownum), 20);
    wenv[0] = numbuff;
    for (i = 0; (i < numfields) && (i < 9); i++) {
	wenv[i + 1] = row_p[i];
      if (!wenv[i + 1])
        wenv[i + 1] = (char *) "";
    }
    /* Now call the ufun. */
    if (call_ufun(&ufun, wenv, i + 1, rbuff, executor, enactor, pe_info))
      goto finished;
    if (safe_str(rbuff, buff, bp) && funccount == pe_info->fun_invocations)
      goto finished;
    funccount = pe_info->fun_invocations;
  }
finished:
  free_sql_query(qres);
}

FUNCTION(fun_sql)
{
  MYSQL_RES *qres;
  MYSQL_ROW row_p;
  char *rowsep = (char *) " ";
  char *fieldsep = (char *) " ";
  int affected_rows;
  int rownum;
  int i;
  int numfields;

  if (!Sql_Ok(executor)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }

  if (nargs >= 2) {
    /* we have a row separator in args[2]. */
    rowsep = args[1];
  }

  if (nargs >= 3) {
    /* we have a field separator in args[3]. */
    fieldsep = args[2];
  }

  qres = sql_query(args[0], &affected_rows);

  if (!qres) {
    if (affected_rows >= 0) {
      notify_format(executor, "SQL: %d rows affected.", affected_rows);
    } else if (!mysql_struct) {
      notify(executor, "No SQL database connection.");
    } else {
      notify_format(executor, "SQL: Error: %s", mysql_error(mysql_struct));
      safe_str("#-1", buff, bp);
    }
    return;
  }

  /* Get results. A silent query (INSERT, UPDATE, etc.) will return NULL */
  numfields = mysql_num_fields(qres);

  rownum = 0;
  while ((row_p = mysql_fetch_row(qres)) != NULL) {
    if (rownum++ > 0) {
      safe_str(rowsep, buff, bp);
    }
    for (i = 0; i < numfields; i++) {
      if (i > 0) {
        if (safe_str(fieldsep, buff, bp))
          goto finished;
      }
      if (row_p[i] && *row_p[i])
        if (safe_str(row_p[i], buff, bp))
          goto finished;        /* We filled the buffer, best stop */
    }
  }
finished:
  free_sql_query(qres);
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
  MYSQL_RES *qres;
  MYSQL_ROW row_p;
  int affected_rows;
  int rownum;
  int numfields;
  char *cell;
  MYSQL_FIELD *fields;
  int i;

  qres = sql_query(arg_left, &affected_rows);

  if (!qres) {
    if (affected_rows >= 0) {
      notify_format(player, "SQL: %d rows affected.", affected_rows);
    } else if (!mysql_struct) {
      notify(player, "No SQL database connection.");
    } else {
      notify_format(player, "SQL: Error: %s", mysql_error(mysql_struct));
    }
    return;
  }

  /* Get results. A silent query (INSERT, UPDATE, etc.) will return NULL */
  numfields = mysql_num_fields(qres);
  fields = mysql_fetch_fields(qres);

  rownum = 0;
  while ((row_p = mysql_fetch_row(qres)) != NULL) {
    rownum++;
    if (numfields > 0) {
      for (i = 0; i < numfields; i++) {
        cell = row_p[i];
        notify_format(player, "Row %d, Field %s: %s",
                      rownum, fields[i].name, (cell && *cell) ? cell : "NULL");
      }
    } else
      notify_format(player, "Row %d: NULL", rownum);
  }
  free_sql_query(qres);
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

FUNCTION(fun_mapsql)
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
