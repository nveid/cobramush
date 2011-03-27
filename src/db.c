/**
 * \file db.c
 *
 * \brief Loading and saving the CobraMUSH object database.
 *
 *
 */

#include "copyrite.h"
#include "config.h"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#ifdef I_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif
#include <stdlib.h>
#include "conf.h"
#include "dbio.h"
#include "externs.h"
#include "mushdb.h"
#include "attrib.h"
#include "mymalloc.h"
#include "game.h"
#include "flags.h"
#include "lock.h"
#include "dbdefs.h"
#include "log.h"
#include "strtree.h"
#include "parse.h"
#include "privtab.h"
#include "htab.h"
#include "extmail.h"
#include "confmagic.h"

#ifdef WIN32
#pragma warning( disable : 4761)      /* disable warning re conversion */
#endif

#ifdef WIN32SERVICES
void shutdown_checkpoint(void);
#endif

/** Get a ref out of the database if a given db flag is set */
#define MAYBE_GET(f,x) \
        (indb_flags & (x)) ? getref(f) : 0

long indb_flags;		/**< What flags are in the db we read at startup? */
long flagdb_flags;		/**< What flags are in the flag db we read at startup? */

int loading_db = 0;   /**< Are we loading the database? */

char db_timestamp[100];	/**< Time the read database was saved. */

struct object *db = NULL; /**< The object db array */
dbref db_top = 0;	  /**< The number of objects in the db array */

dbref errobj;		  /**< Dbref of object on which an error has occurred */

int dbline = 0;		  /**< Line of the database file being read */
int use_flagfile = 0;    /**< Use seperate file for flags */

/** String that markes the end of dumps */
const char *EOD = "***END OF DUMP***\n";

#ifndef DB_INITIAL_SIZE
#define DB_INITIAL_SIZE 5000   /**< Initial size for db array */
#endif				/* DB_INITIAL_SIZE */

dbref db_size = DB_INITIAL_SIZE;  /**< Current size of db array */

HASHTAB htab_objdata;	      /**< Object data hash table */
HASHTAB htab_objdata_keys;    /**< Object data keys hash table */

static void db_grow(dbref newtop);

static void db_write_obj_basic(FILE * f, dbref i, struct object *o);
int db_paranoid_write_object(FILE * f, dbref i, int flag);
void putlocks(FILE * f, lock_list *l);
void getlocks(dbref i, FILE * f);
void get_new_locks(dbref i, FILE * f, int c);
void db_read_attrs(FILE * f, dbref i, int c);
int get_list(FILE * f, dbref i);
static unsigned char * getbytes(FILE * f);
void putbytes(FILE * f, unsigned char *lbytes, int byte_limit);
void db_free(void);
int load_flag_db(FILE *);
void db_write_flag_db(FILE *);
static void init_objdata_htab(int size, void(*free_data)(void*));
static void db_write_flags(FILE * f);
static void db_write_powers(FILE * f);
static dbref db_read_oldstyle(FILE * f);

StrTree object_names;	    /**< String tree of object names */
StrTree _clastmods;
extern StrTree atr_names;

void init_names(void);
void init_lmods(void);
void create_minimal_db(void);

extern struct db_stat_info current_state;

/** Initialize the name strtree.
 */
void
init_names(void)
{
  st_init(&object_names);
}

void init_lmods(void) {
  st_init(&_clastmods);
}

/** Set an object's name through the name strtree.
 * We maintain object names in a strtree because many objects have
 * the same name (cardinal exits, weapons and armor, etc.)
 * This function is used to set an object's name; if the name's already
 * in the strtree, we just get a pointer to it, saving memory.
 * (If not, we add it to the strtree and use that pointer).
 * \param obj dbref of object whose name is to be set.
 * \param newname name to set on the object, or NULL to clear the name.
 * \return object's new name, or NULL if none is given.
 */
const char *
set_name(dbref obj, const char *newname)
{
  /* if pointer not null unalloc it */
  if (Name(obj))
    st_delete(Name(obj), &object_names);
  if (!newname || !*newname)
    return NULL;
  Name(obj) = st_insert(newname, &object_names);
  return Name(obj);
}

/* Alot of things could of modified the samething.. */
const char *set_lmod(dbref obj, const char *lmod) {
  if(db[obj].lastmod)
    st_delete(db[obj].lastmod, &_clastmods);
  if(!lmod || !*lmod) {
    /* this can happen.. */
    LastMod(obj) = st_insert((const char *)"NONE", &_clastmods);
  } else LastMod(obj) = st_insert(lmod, &_clastmods);

  return db[obj].lastmod;
}

int db_init = 0;  /**< Has the db array been initialized yet? */

static void
db_grow(dbref newtop)
{
  struct object *newdb;
  dbref initialized;
  struct object *o;

  if (newtop > db_top) {
    initialized = db_top;
    current_state.total = newtop;
    current_state.garbage += newtop - db_top;
    db_top = newtop;
    if (!db) {
      /* make the initial one */
      db_size = (db_init) ? db_init : DB_INITIAL_SIZE;
      while (db_top > db_size)
        db_size *= 2;
      if ((db = (struct object *)
           malloc(db_size * sizeof(struct object))) == NULL) {
        do_rawlog(LT_ERR, "ERROR: out of memory while creating database!");
        abort();
      }
    }
    /* maybe grow it */
    if (db_top > db_size) {
      /* make sure it's big enough */
      while (db_top > db_size)
        db_size *= 2;
      if ((newdb = (struct object *)
           realloc(db, db_size * sizeof(struct object))) == NULL) {
        do_rawlog(LT_ERR, "ERROR: out of memory while extending database!");
        abort();
      }
      db = newdb;
    }
    while (initialized < db_top) {
      o = db + initialized;
      o->name = 0;
      o->list = 0;
      o->location = NOTHING;
      o->contents = NOTHING;
      o->exits = NOTHING;
      o->next = NOTHING;
      o->parent = NOTHING;
      o->locks = NULL;
      o->owner = GOD;
      o->zone = NOTHING;
      o->penn = 0;
      o->type = TYPE_GARBAGE;
      o->flags = NULL;
      o->division.dp_bytes = NULL;
      o->division.level = 1;
      o->division.object = -1;
      o->division.powergroups = NULL;
      o->warnings = 0;
      o->modification_time = o->creation_time = mudtime;
      o->lastmod = NULL;
      o->attrcount = 0;
      initialized++;
    }
  }
}

/** Allocate a new object structure.
 * This function allocates and returns a new object structure.
 * The caller must see that it gets appropriately typed and otherwise
 * initialized.
 * \return dbref of newly allocated object.
 */
dbref
new_object(void)
{
  dbref newobj;
  struct object *o;
  /* if stuff in free list use it */
  if ((newobj = free_get()) == NOTHING) {
    /* allocate more space */
    newobj = db_top;
    db_grow(db_top + 1);
  }
  /* clear it out */
  o = db + newobj;
  o->name = 0;
  o->list = 0;
  o->location = NOTHING;
  o->contents = NOTHING;
  o->exits = NOTHING;
  o->next = NOTHING;
  o->parent = NOTHING;
  o->locks = NULL;
  o->owner = GOD;
  o->zone = NOTHING;
  o->penn = 0;
  o->type = TYPE_GARBAGE;
  o->flags = new_flag_bitmask("FLAG");
  o->division.dp_bytes = NULL;
  o->division.level = 1;
  o->division.object = -1;
  o->division.powergroups = NULL;
  o->warnings = 0;
  o->modification_time = o->creation_time = mudtime;
  o->attrcount = 0;
  o->lastmod = NULL;
#ifdef RPMODE_SYS
  o->rplog.bufferq = NULL;
  o->rplog.status = 0;
#endif /* RPMODE_SYS */
  if (current_state.garbage)
    current_state.garbage--;
  return newobj;
}

/** Output a long int to a file.
 * \param f file pointer to write to.
 * \param ref value to write.
 */
void
putref(FILE * f, long int ref)
{
  OUTPUT(fprintf(f, "%ld\n", ref));
}

/** Output a string to a file.
 * This function writes a string to a file, double-quoted, 
 * appropriately escaping quotes and backslashes (the escape character).
 * \param f file pointer to write to.
 * \param s value to write.
 */
void
putstring(FILE * f, const char *s)
{
  OUTPUT(putc('"', f));
  while (*s) {
    switch (*s) {
    case '\\':
    case '"':
      OUTPUT(putc('\\', f));
      /* FALL THROUGH */
    default:
      OUTPUT(putc(*s, f));
    }
    s++;
  }
  OUTPUT(putc('"', f));
  OUTPUT(putc('\n', f));
}

/** Read a labeled entry from a database.
 * Labeled entries look like 'label entry', and are used
 * extensively in the current database format, and to a lesser
 * extent in older versions.
 * \param f the file to read from
 * \param label pointer to update to the address of a static
 * buffer containing the label that was read.
 * \param value pointer to update to the address of a static
 * buffer containing the value that was read.
 */
void
db_read_labeled_string(FILE * f, char **label, char **value)
{
  static char lbuf[BUFFER_LEN], vbuf[BUFFER_LEN];
  int c;
  char *p;

  *label = lbuf;
  *value = vbuf;

  /* invariant: we start at the beginning of a line. */

  dbline++;

  do {
    c = getc(f);
    while (isspace(c)) {
      if (c == '\n')
        dbline++;
      c = getc(f);
    }
    if (c == '#') {
      while ((c = getc(f)) != '\n' && c != EOF) {
        /* nothing */
      }
      if (c == '\n')
        dbline++;
    }
  } while (c != EOF && isspace(c));

  if (c == EOF) {
    do_rawlog(LT_ERR, T("DB: Unexpected EOF at line %d"), dbline);
    longjmp(db_err, 1);
  }

  /* invariant: we should have the first character of a label in 'c'. */

  p = lbuf;
  do {
    if (c != '_' && c != '-' && c != '!' && c != '.' && c != '>' && c != '<' && c != '#' &&     /* these really should only be first time */
        !isalnum(c)) {
      do_rawlog(LT_ERR, "DB: Illegal character '%c'(%d) in label, line %d",
                c, c, dbline);
      longjmp(db_err, 1);
    }
    safe_chr(c, lbuf, &p);
    c = getc(f);
  } while (c != EOF && !isspace(c));
  *p++ = '\0';
  if (p >= lbuf + BUFFER_LEN)
    do_rawlog(LT_ERR, "DB: warning: very long label, line %d", dbline);

  /* suck up separating whitespace */
  while (c != '\n' && c != EOF && isspace(c))
    c = getc(f);

  /* check for presence of a value, which we must have. */
  if (c == EOF || c == '\n') {
    if (c == EOF)
      do_rawlog(LT_ERR, T("DB: Unexpected EOF at line %d"), dbline);
    else
      do_rawlog(LT_ERR, T("DB: Missing value for '%s' at line %d"), lbuf,
                dbline);
    longjmp(db_err, 1);
  }

  /* invariant: we should have the first character of a value in 'c'. */

  p = vbuf;
  if (c == '"') {
    /* quoted string */
    int sline;
    sline = dbline;
    for (;;) {
      c = getc(f);
      if (c == '"')
        break;
      if (c == '\\')
        c = getc(f);
      if (c == EOF) {
        do_rawlog(LT_ERR, "DB: Unclosed quoted string starting on line %d",
                  sline);
        longjmp(db_err, 1);
      }
      if (c == '\0')
        do_rawlog(LT_ERR,
                  "DB: warning: null in quoted string, remainder lost, line %d",
                  dbline);
      if (c == '\n')
        dbline++;
      safe_chr(c, vbuf, &p);
    }
    do {
      c = getc(f);
      if (c != EOF && !isspace(c)) {
        do_rawlog(LT_ERR, "DB: Garbage after quoted string, line %d", dbline);
        longjmp(db_err, 1);
      }
    } while (c != '\n' && c != EOF);
  } else {
    /* non-quoted value */
    do {
      if (c != '_' && c != '-' && c != '!' && c != '.' &&
          c != '#' && !isalnum(c) && !isspace(c)) {
        do_rawlog(LT_ERR, "DB: Illegal character '%c'(%d) in value, line %d",
                  c, c, dbline);
        longjmp(db_err, 1);
      }
      safe_chr(c, vbuf, &p);
      c = getc(f);
    } while (c != EOF && c != '\n');
    if (c == '\n' && (p - vbuf >= 2) && (*(p - 2) == '\r')) {
      /* Oops, we read in \r\n at the end of this value. Drop the \r */
      p--;
      *(p - 1) = '\n';
    }
  }
  *p++ = '\0';
  if (p >= vbuf + BUFFER_LEN)
    do_rawlog(LT_ERR, "DB: warning: very long value, line %d", dbline);

  /* note no line increment for final newline because of initial increment */
}

/** Read a string with a given label.
 * If the label read is different than the one being checked, the 
 * database load will abort with an error.
 * \param f the file to read from.
 * \param label the label that should be read.
 * \param value pointer to update to the address of a static
 * buffer containing the value that was read.
 */
void
db_read_this_labeled_string(FILE * f, const char *label, char **value)
{
  char *readlabel;

  db_read_labeled_string(f, &readlabel, value);

  if (strcmp(readlabel, label)) {
    do_rawlog(LT_ERR,
              T("DB: error: Got label '%s', expected label '%s' at line %d"),
              readlabel, label, dbline);
    longjmp(db_err, 1);
  }
}

/** Read an integer with a given label.
 * If the label read is different than the one being checked, the 
 * database load will abort with an error.
 * \param f the file to read from.
 * \param label the label that should be read.
 * \param value pointer to update to the number that was read.
 */
void
db_read_this_labeled_number(FILE * f, const char *label, int *value)
{
  char *readlabel;
  char *readvalue;

  db_read_labeled_string(f, &readlabel, &readvalue);

  if (strcmp(readlabel, label)) {
    do_rawlog(LT_ERR,
              T("DB: error: Got label '%s', expected label '%s' at line %d"),
              readlabel, label, dbline);
    longjmp(db_err, 1);
  }

  *value = parse_integer(readvalue);
}

/** Read an integer and label.
 * \param f the file to read from.
 * \param label pointer to update to the address of a static
 * buffer containing the label that was read.
 * \param value pointer to update to the number that was read.
 */
void
db_read_labeled_number(FILE * f, char **label, int *value)
{
  char *readvalue;
  db_read_labeled_string(f, label, &readvalue);
  *value = parse_integer(readvalue);
}

/** Read a time_t with a given label.
 * If the label read is different than the one being checked, the
 * database load will abort with an error.
 * \param f the file to read from.
 * \param label the label that should be read.
 * \param value pointer to update to the time_t that was read.
 */
void
db_read_this_labeled_time_t(FILE * f, const char *label, time_t *value)
{
  char *readlabel;
  char *readvalue;

  db_read_labeled_string(f, &readlabel, &readvalue);

  if (strcmp(readlabel, label)) {
    do_rawlog(LT_ERR,
	      T("DB: error: Got label '%s', expected label '%s' at line %d"),
	      readlabel, label, dbline);
    longjmp(db_err, 1);
  }

  *value = (time_t) atoll(readvalue);
}

/** Read a time_t and label.
 * \param f the file to read from.
 * \param label pointer to update to the address of a static
 * buffer containing the label that was read.
 * \param value pointer to update to the time_t that was read.
 */
void
db_read_labeled_time_t(FILE * f, char **label, time_t *value)
{
  char *readvalue;
  db_read_labeled_string(f, label, &readvalue);
  *value = (time_t) atoll(readvalue);
}

/** Read a dbref with a given label.
 * If the label read is different than the one being checked, the 
 * database load will abort with an error.
 * \param f the file to read from.
 * \param label the label that should be read.
 * \param value pointer to update to the dbref that was read.
 */
void
db_read_this_labeled_dbref(FILE * f, const char *label, dbref *val)
{
  char *readlabel;
  char *readvalue;

  db_read_labeled_string(f, &readlabel, &readvalue);

  if (strcmp(readlabel, label)) {
    do_rawlog(LT_ERR,
              T("DB: error: Got label '%s', expected label '%s' at line %d"),
              readlabel, label, dbline);
    longjmp(db_err, 1);
  }
  *val = qparse_dbref(readvalue);
}

/** Read a dbref and label.
 * \param f the file to read from.
 * \param label pointer to update to the address of a static
 * buffer containing the label that was read.
 * \param value pointer to update to the dbref that was read.
 */
void
db_read_labeled_dbref(FILE * f, char **label, dbref *val)
{
  char *readvalue;
  db_read_labeled_string(f, label, &readvalue);
  *val = qparse_dbref(readvalue);
}

static void
db_write_label(FILE * f, char const *l)
{
  OUTPUT(fputs(l, f));
  OUTPUT(putc(' ', f));
}

void
db_write_labeled_string(FILE * f, char const *label, char const *value)
{
  db_write_label(f, label);
  putstring(f, value);
}

void
db_write_labeled_number(FILE * f, char const *label, int value)
{
  OUTPUT(fprintf(f, "%s %d\n", label, value));
}

void
db_write_labeled_time_t(FILE * f, char const *label, time_t value)
{
  OUTPUT(fprintf(f, "%s %zu\n", label, value));
}

void
db_write_labeled_dbref(FILE * f, char const *label, dbref value)
{
  OUTPUT(fprintf(f, "%s #%d\n", label, value));
}

/** Write a boolexp to a file in unparsed (text) form.
 * \param f file pointer to write to.
 * \param b pointer to boolexp to write.
 */
void
putboolexp(FILE * f, boolexp b)
{
  db_write_labeled_string(f, "  key", unparse_boolexp(GOD, b, UB_DBREF));
}

/** Write a list of locks to a file.
 * \param f file pointer to write to.
 * \param l pointer to lock_list to write.
 */
void
putlocks(FILE * f, lock_list *l)
{
  lock_list *ll;
  int count = 0;
  for (ll = l; ll; ll = ll->next)
    count++;
  db_write_labeled_number(f, "lockcount", count);
  for (ll = l; ll; ll = ll->next) {
    db_write_labeled_string(f, " type", ll->type);
    db_write_labeled_dbref(f, "  creator", L_CREATOR(ll));
    db_write_labeled_string(f, "  flags", lock_flags_long(ll));
    db_write_labeled_number(f, "  derefs", chunk_derefs(L_KEY(ll)));
    putboolexp(f, ll->key);
    /* putboolexp adds a '\n', so we won't. */
  }
}


/** Write out the basics of an object.
 * This function writes out the basic information associated with an
 * object - just about everything but the attributes.
 * \param f file pointer to write to.
 * \param i dbref of object to write.
 * \param o pointer to object to write.
 */
static void
db_write_obj_basic(FILE * f, dbref i, struct object *o)
{
  db_write_labeled_string(f, "name", o->name);
  db_write_labeled_dbref(f, "location", o->location);
  db_write_labeled_dbref(f, "contents", o->contents);
  db_write_labeled_dbref(f, "exits", o->exits);
  db_write_labeled_dbref(f, "next", o->next);
  db_write_labeled_dbref(f, "parent", o->parent);
  putlocks(f, Locks(i));
  db_write_labeled_dbref(f, "owner", o->owner);
  db_write_labeled_dbref(f, "zone", o->zone);
  db_write_labeled_dbref(f, "division_object",  o->division.object);
  db_write_labeled_number(f, "level", o->division.level);
  db_write_labeled_number(f, "pennies", Pennies(i));
  db_write_labeled_number(f, "type", Typeof(i));
  db_write_labeled_string(f, "powergroup", (const char *) powergroups_list_on(i, 0));
  db_write_labeled_string(f, "powers", division_list_powerz(i, 0));
  db_write_labeled_string(f, "flags",
                       bits_to_string("FLAG", o->flags, GOD, NOTHING));
  db_write_labeled_string(f, "warnings", unparse_warnings(o->warnings));
  db_write_labeled_number(f, "created", (int) o->creation_time);
  db_write_labeled_number(f, "modified", (int) o->modification_time);
  if(o->lastmod && *o->lastmod)
   db_write_labeled_string(f, "lastmod", o->lastmod);
  else
    db_write_labeled_string(f, "lastmod", (const char *)"NONE");

}

/** Write out an object.
 * This function writes a single object out to a file.
 * \param f file pointer to write to.
 * \param i dbref of object to write.
 */
int
db_write_object(FILE * f, dbref i)
{
  struct object *o;
  ALIST *list;
  int count = 0;

  o = db + i;
  db_write_obj_basic(f, i, o);

  /* write the attribute list */

  /* Don't trust AttrCount(thing) for number of attributes to write. */
  for (list = o->list; list; list = AL_NEXT(list)) {
    if (AF_Nodump(list))
      continue;
    count++;
  }
  db_write_labeled_number(f, "attrcount", count);

  for (list = o->list; list; list = AL_NEXT(list)) {
    if (AF_Nodump(list))
      continue;
    db_write_labeled_string(f, " name", AL_NAME(list));
    db_write_labeled_dbref(f, "  owner", Owner(AL_CREATOR(list)));
    db_write_labeled_string(f, "  flags", atrflag_to_string(AL_FLAGS(list)));
    db_write_labeled_number(f, "  derefs", AL_DEREFS(list));
    db_write_labeled_string(f, "  writelock", unparse_boolexp(GOD, AL_WLock(list), UB_DBREF));
    db_write_labeled_number(f, "     derefs", chunk_derefs(AL_WLock(list)));
    db_write_labeled_string(f, "  readlock", unparse_boolexp(GOD, AL_RLock(list), UB_DBREF));
    db_write_labeled_number(f, "     derefs", chunk_derefs(AL_RLock(list)));
    db_write_labeled_time_t(f, "  modtime", AL_MODTIME(list));
    db_write_labeled_string(f, "  value", atr_value(list));
  }
  return 0;
}

/** Write out the object database to disk.
 * \verbatim
 * This function writes the databsae out to disk. The database
 * structure currently looks something like this:
 * +V<header line>
 * +FLAGS LIST
 * <flag data>
 * +POWERS LIST
 * <flag data>
 * ~<number of objects>
 * <object data>
 * \endverbatim
 * \param f file pointer to write to.
 * \param flag 0 for normal dump, DBF_PANIC for panic dumps.
 * \return the number of objects in the database (db_top)
 */
dbref
db_write(FILE * f, int flag)
{
  dbref i;
  int dbflag;

  /* print a header line to make a later conversion to 2.0 easier to do.
   * the odd choice of numbers is based on 256*x + 2 offset
   * The original PennMUSH had x=5 (chat) or x=6 (nochat), and Tiny expects
   * to deal with that. We need to use some extra flags as well, so
   * we may be adding to 5/6 as needed, using successive binary numbers.
   */
  dbflag = 5 + flag;
  dbflag += DBF_NO_CHAT_SYSTEM;
  dbflag += DBF_WARNINGS;
  dbflag += DBF_CREATION_TIMES;
  dbflag += DBF_SPIFFY_LOCKS;
  dbflag += DBF_NEW_STRINGS;
  /* Removed because CobraMUSH 0.7 and after assume DBF_TYPE_GARBAGE implies
   * a PennMUSH database that needs to be converted. */
  /* dbflag += DBF_TYPE_GARBAGE; */
  dbflag += DBF_SPLIT_IMMORTAL;
  dbflag += DBF_NO_TEMPLE;
  dbflag += DBF_LESS_GARBAGE;
  dbflag += DBF_AF_VISUAL;
  dbflag += DBF_VALUE_IS_COST;
  dbflag += DBF_LINK_ANYWHERE;
  dbflag += DBF_NO_STARTUP_FLAG;
  dbflag += DBF_AF_NODUMP;
  dbflag += DBF_NEW_FLAGS;
  dbflag += DBF_DIVISIONS;
  dbflag += DBF_LABELS;
  dbflag += DBF_NEW_ATR_LOCK;
  dbflag += DBF_ATR_MODTIME;

  OUTPUT(fprintf(f, "+V%d\n", dbflag * 256 + 2));
  db_write_labeled_string(f, "savedtime", show_time(mudtime, 1));

  OUTPUT(fprintf(f, "~%d\n", db_top));
  for (i = 0; i < db_top; i++) {
#ifdef WIN32
#ifndef __MINGW32__
    /* Keep the service manager happy */
    if (shutdown_flag && (i & 0xFF) == 0)
      shutdown_checkpoint();
#endif
#endif
    if (IsGarbage(i))
      continue;
    OUTPUT(fprintf(f, "!%d\n", i));
    db_write_object(f, i);
  }
  OUTPUT(fputs(EOD, f));
  return db_top;
}

static void
db_write_flags(FILE * f)
{
  OUTPUT(fprintf(f, "+FLAGS LIST\n"));
  flag_write_all(f, "FLAG");
}

static void
db_write_powers(FILE *f) {
  OUTPUT(fprintf(f, "+POWERS LIST\n"));
  power_write_all(f);
}


/** Write out an object, in paranoid fashion.
 * This function writes a single object out to a file in paranoid
 * mode, which warns about several potential types of corruption,
 * and can fix some of them.
 * \param f file pointer to write to.
 * \param i dbref of object to write.
 * \param flag 1 = debug, 0 = normal
 */
int
db_paranoid_write_object(FILE * f, dbref i, int flag)
{
  struct object *o;
  ALIST *list, *next;
  char name[BUFFER_LEN];
  char tbuf1[BUFFER_LEN];
  int err = 0;
  char *p;
  char lastp;
  dbref owner;
  int flags;
  int fixmemdb = 0;
  int count = 0;
  int attrcount = 0;

  o = db + i;
  db_write_obj_basic(f, i, o);
  /* fflush(f); */

  /* write the attribute list, scanning */
  for (list = o->list; list; list = AL_NEXT(list)) {
    if (AF_Nodump(list))
      continue;
    attrcount++;
  }

  db_write_labeled_number(f, "attrcount", attrcount);

  for (list = o->list; list; list = next) {
    next = AL_NEXT(list);
    if (AF_Nodump(list))
      continue;
    fixmemdb = err = 0;
    /* smash unprintable characters in the name, replace with ! */
    strcpy(name, AL_NAME(list));
    for (p = name; *p; p++) {
      if (!isprint((unsigned char) *p) || isspace((unsigned char) *p)) {
        *p = '!';
        fixmemdb = err = 1;
      }
    }
    if (err) {
      /* If name already exists on this object, try adding a
       * number to the end. Give up if we can't find one < 10000
       */
      if (atr_get_noparent(i, name)) {
        count = 0;
        do {
          name[BUFFER_LEN - 6] = '\0';
          sprintf(tbuf1, "%s%d", name, count);
          count++;
        } while (count < 10000 && atr_get_noparent(i, tbuf1));
        strcpy(name, tbuf1);
      }
      do_rawlog(LT_CHECK,
                T(" * Bad attribute name on #%d. Changing name to %s.\n"),
                i, name);
      err = 0;
    }
    /* check the owner */
    owner = AL_CREATOR(list);
    if (!GoodObject(owner)) {
      do_rawlog(LT_CHECK, T(" * Bad owner on attribute %s on #%d.\n"), name, i);
      owner = GOD;
      fixmemdb = 1;
    } else {
      owner = Owner(owner);
    }

    /* write that info out */
    db_write_labeled_string(f, " name", name);
    db_write_labeled_dbref(f, "  owner", owner);
    db_write_labeled_string(f, "  flags", atrflag_to_string(AL_FLAGS(list)));
    db_write_labeled_number(f, "  derefs", AL_DEREFS(list));

    /* now check the attribute */
    strcpy(tbuf1, atr_value(list));
    /* get rid of unprintables and hard newlines */
    lastp = '\0';
    for (p = tbuf1; *p; p++) {
      if (!isprint((unsigned char) *p)) {
        if (!isspace((unsigned char) *p)) {
          *p = '!';
          err = 1;
        }
      }
      lastp = *p;
    }
    if (err) {
      fixmemdb = 1;
      do_rawlog(LT_CHECK,
                T(" * Bad text in attribute %s on #%d. Changed to:\n"), name,
                i);
      do_rawlog(LT_CHECK, "%s\n", tbuf1);
    }
    db_write_labeled_string(f, "  value", tbuf1);
    if (flag && fixmemdb) {
      /* Fix the db in memory */
      flags = AL_FLAGS(list);
      atr_clr(i, AL_NAME(list), owner);
      (void) atr_add(i, name, tbuf1, owner, flags);
      list = atr_get_noparent(i, name);
      AL_FLAGS(list) = flags;
    }
  }
  return 0;
}



/** Write out the object database to disk, in paranoid mode.
 * \verbatim
 * This function writes the databsae out to disk, in paranoid mode. 
 * The database structure currently looks something like this:
 * +V<header line>
 * +FLAGS LIST
 * <flag data>
 * ~<number of objects>
 * <object data>
 * \endverbatim
 * \param f file pointer to write to.
 * \param flag 0 for normal paranoid dump, 1 for debug paranoid dump.
 * \return the number of objects in the database (db_top)
 */
dbref
db_paranoid_write(FILE * f, int flag)
{
  dbref i;
  int dbflag;

/* print a header line to make a later conversion to 2.0 easier to do.
 * the odd choice of numbers is based on 256*x + 2 offset
 */
  dbflag = 5;
  dbflag += DBF_NO_CHAT_SYSTEM;
  dbflag += DBF_WARNINGS;
  dbflag += DBF_CREATION_TIMES;
  dbflag += DBF_SPIFFY_LOCKS;
  dbflag += DBF_NEW_STRINGS;
  /* Removed because CobraMUSH 0.7 and after assume DBF_TYPE_GARBAGE implies
   * a PennMUSH database that needs to be converted. */
  /* dbflag += DBF_TYPE_GARBAGE; */
  dbflag += DBF_SPLIT_IMMORTAL;
  dbflag += DBF_NO_TEMPLE;
  dbflag += DBF_LESS_GARBAGE;
  dbflag += DBF_AF_VISUAL;
  dbflag += DBF_VALUE_IS_COST;
  dbflag += DBF_LINK_ANYWHERE;
  dbflag += DBF_NO_STARTUP_FLAG;
  dbflag += DBF_AF_NODUMP;
  dbflag += DBF_NEW_FLAGS;
  dbflag += DBF_DIVISIONS;
  dbflag += DBF_LABELS;

  do_rawlog(LT_CHECK, "PARANOID WRITE BEGINNING...\n");

  OUTPUT(fprintf(f, "+V%d\n", dbflag * 256 + 2));

  db_write_labeled_string(f, "savedtime", show_time(mudtime, 1));
  db_write_flags(f);
  db_write_powers(f);
  OUTPUT(fprintf(f, "~%d\n", db_top));

  /* write out each object */
  for (i = 0; i < db_top; i++) {
#ifdef WIN32SERVICES
    /* Keep the service manager happy */
    if (shutdown_flag && (i & 0xFF) == 0)
      shutdown_checkpoint();
#endif
    if (IsGarbage(i))
      continue;
    OUTPUT(fprintf(f, "!%d\n", i));
    db_paranoid_write_object(f, i, flag);
    /* print out a message every so many objects */
    if (i % globals.paranoid_checkpt == 0)
      do_rawlog(LT_CHECK, T("\t...wrote up to object #%d\n"), i);
  }
  OUTPUT(fputs(EOD, f));
  do_rawlog(LT_CHECK, T("\t...finished at object #%d\n"), i - 1);
  do_rawlog(LT_CHECK, "END OF PARANOID WRITE.\n");
  return db_top;
}


/** Read in a long int.
 * \param f file pointer to read from.
 * \return long int read.
 */
long int
getref(FILE * f)
{
  static char buf[BUFFER_LEN];
  if (!fgets(buf, sizeof(buf), f)) {
    do_rawlog(LT_ERR, T("Unexpected EOF at line %d"), dbline);
    longjmp(db_err, 1);
  }
  dbline++;
  return strtol(buf, NULL, 10);
}


/** Read in a string, into a static buffer.
 * This function reads a double-quoted escaped string of the form
 * written by putstring. The string is read into a static buffer
 * that is not allocated, so the return value must usually be copied
 * elsewhere.
 * \param f file pointer to read from.
 * \return pointer to static buffer containing string read.
 */
const char *
getstring_noalloc(FILE * f)
{
  static char buf[BUFFER_LEN];
  char *p;
  int c;

  p = buf;
  c = fgetc(f);
  if (c == EOF) {
    do_rawlog(LT_ERR, T("Unexpected EOF at line %d"), dbline);
    longjmp(db_err, 1);
  } else if (c != '"') {
    for (;;) {
      if ((c == '\0') || (c == EOF) ||
          ((c == '\n') && ((p == buf) || (p[-1] != '\r')))) {
        *p = '\0';
        if (c == '\n')
          dbline++;
        return buf;
      }
      safe_chr(c, buf, &p);
      c = fgetc(f);
    }
  } else {
    for (;;) {
      c = fgetc(f);
      if (c == '"') {
        /* It's a closing quote if it's followed by \r or \n */
        c = fgetc(f);
        if (c == '\r') {
          /* Get a possible \n, too */
          if ((c = fgetc(f)) != '\n')
            ungetc(c, f);
          else
            dbline++;
        } else if (c != '\n')
          ungetc(c, f);
        *p = '\0';
        return buf;
      } else if (c == '\\') {
        c = fgetc(f);
      }
      if ((c == '\0') || (c == EOF)) {
        *p = '\0';
        return buf;
      }
      safe_chr(c, buf, &p);
    }
  }
}

/** Read a boolexp from a file.
 * This function reads a boolexp from a file. It expects the format that
 * put_boolexp writes out.
 * \param f file pointer to read from.
 * \param type pointer to lock type being read.
 * \return pointer to boolexp read.
 */
boolexp
getboolexp(FILE * f, const char *type)
{
  char *val;
  db_read_this_labeled_string(f, "key", &val);
  return parse_boolexp(GOD, val, type);
}

extern PRIV lock_privs[];

/** Read locks for an object.
 * This function is used for DBF_SPIFFY_LOCKS to read a whole list
 * of locks from an object and set them.
 * \param i dbref of the object.
 * \param f file pointer to read from.
 * \param c number of locks, or -1 if not yet known.
 */
void
get_new_locks(dbref i, FILE * f, int c)
{
  char *val, *key;
  dbref creator;
  int flags;
  char type[BUFFER_LEN];
  boolexp b;
  int count = c, derefs = 0, found = 0;

  if (c < 0) {
    db_read_this_labeled_string(f, "lockcount", &val);
    count = parse_integer(val);
  }

  for (;;) {
    int ch;

    ch = fgetc(f);
    ungetc(ch, f);

    if (ch != ' ')
      break;

    found++;

    /* Name of the lock */
    db_read_this_labeled_string(f, "type", &val);
    strcpy(type, val);
    if(indb_flags & DBF_LABELS) {
      db_read_this_labeled_dbref(f, "creator", &creator);
      db_read_this_labeled_string(f, "flags", &val);
      flags = string_to_privs(lock_privs, val, 0);
      db_read_this_labeled_number(f, "derefs", &derefs);
    } else {
      db_read_this_labeled_number(f, "creator", &creator);
      db_read_this_labeled_number(f, "flags", &flags);
    }
    /* boolexp */
    db_read_this_labeled_string(f, "key", &key);
    b = parse_boolexp_d(GOD, key, type, derefs);
    add_lock_raw(creator, i, type, b, flags);
  }

  if (found != count)
    do_rawlog(LT_ERR,
              T
              ("WARNING: Actual lock count (%d) different from expected count (%d)."),
              found, count);

}


/** Read locks for an object.
 * This function is used for DBF_NEW_LOCKS to read a whole list
 * of locks from an object and set them. DBF_NEW_LOCKS aren't really
 * new any more, and get_new_locks() is probably being used instead of
 * this function.
 * \param i dbref of the object.
 * \param f file pointer to read from.
 */
void
getlocks(dbref i, FILE * f)
{
  /* Assumes it begins at the beginning of a line. */
  int c;
  boolexp b;
  char buf[BUFFER_LEN], *p;
  while ((c = getc(f)), c != EOF && c == '_') {
    p = buf;
    while ((c = getc(f)), c != EOF && c != '|') {
      *p++ = c;
    }
    *p = '\0';
    if (c == EOF || (p - buf == 0)) {
      do_rawlog(LT_ERR, T("ERROR: Invalid lock format on object #%d"), i);
      return;
    }
    b = getboolexp(f, buf);     /* Which will clobber a '\n' */
    if (b == TRUE_BOOLEXP) {
      /* getboolexp() would already have complained. */
      return;
    } else {
      add_lock_raw(Owner(i), i, buf, b, -1);
    }
  }
  ungetc(c, f);
  return;
}

void
putbytes(FILE * f, unsigned char *lbytes, int byte_limit) {
  int i;

  OUTPUT(putc('\"', f));

  for(i = 0; i < byte_limit; ++i) {
    switch(lbytes[i]) {
      case '\"':
      case '\\':
        OUTPUT(putc('\\', f));
      default:
        OUTPUT(putc(lbytes[i], f));
    }
  }

  OUTPUT(putc('\"', f));
  OUTPUT(putc('\n', f));
}

unsigned char *
getbytes(FILE * f) {
  static unsigned char byte_buf[BUFFER_LEN];
  unsigned char *bp;
  int i;

  memset(byte_buf, 0, BUFFER_LEN);
  bp = byte_buf;
  (void) fgetc(f);	/* Skip the leading " */
  for(;;) {
    i = fgetc(f);
    if(i == '\"') {
      if((i = fgetc(f)) != '\n')
        ungetc(i, f);
      break;
    } else if(i == '\\') {
      i = fgetc(f);
    }
    if(i == EOF)
      break;
    safe_chr(i, byte_buf, &bp);
  }
  return byte_buf;
}

/** Free the entire database.
 * This function frees the name, attributes, and locks on every object
 * in the database, and then free the entire database structure and
 * resets db_top.
 */
void
db_free(void)
{
  dbref i;

  if (db) {

    for (i = 0; i < db_top; i++) {
      set_name(i, NULL);
      set_lmod(i, NULL);
      atr_free_all(i);
      free_locks(Locks(i));
    }

    free((char *) db);
    db = NULL;
    db_init = db_top = 0;
  }
}

/** Read an attribute list for an object from a file
 * \param f file pointer to read from.
 * \param i dbref for the attribute list.
 */
int
get_list(FILE * f, dbref i)
{
  int c;
  char *p, *q;
  char tbuf1[BUFFER_LEN + 150];
  int flags;
  int count = 0;
  unsigned char derefs;

  List(i) = NULL;
  tbuf1[0] = '\0';
  while (1)
    switch (c = getc(f)) {
    case ']':                  /* new style attribs, read name then value */
      /* Using getstring_noalloc here will cause problems with attribute
         names starting with ". This is probably a better fix than just
         disallowing " in attribute names. */
      fgets(tbuf1, BUFFER_LEN + 150, f);
      if (!(p = strchr(tbuf1, '^'))) {
        do_rawlog(LT_ERR, T("ERROR: Bad format on new attributes. object #%d"),
                  i);
        return -1;
      }
      *p++ = '\0';
      if (!(q = strchr(p, '^'))) {
        do_rawlog(LT_ERR,
                  T("ERROR: Bad format on new attribute %s. object #%d"),
                  tbuf1, i);
        return -1;
      }
      *q++ = '\0';
      flags = atoi(q);
      /* Remove obsolete AF_NUKED flag just in case */
      flags &= ~AF_NUKED;
      if (!(indb_flags & DBF_AF_VISUAL)) {
	/* Remove AF_ODARK flag. If it wasn't there, set AF_VISUAL */
	if (!(flags & AF_ODARK))
	  flags |= AF_VISUAL;
	flags &= ~AF_ODARK;
      }
      /* Read in the deref count for the attribute, or set it to 0 if not
         present. */
      q = strchr(q, '^');
      if (q++)
	derefs = atoi(q);
      else
	derefs = 0;
      /* We add the attribute assuming that atoi(p) is an ok dbref
       * since we haven't loaded the whole db and can't really tell
       * if it is or not. We'll fix this up at the end of the load 
       */
      atr_new_add(i, tbuf1, getstring_noalloc(f), atoi(p), flags, derefs, TRUE_BOOLEXP, TRUE_BOOLEXP, 0);
      count++;
      /* Check removed for atoi(q) == 0  (which results in NOTHING for that
       * parameter, and thus no flags), since this eliminates 'visual'
       * attributes (which, if not built-in attrs, have a flag val of 0.)
       */
      break;
    case '>':                  /* old style attribs, die noisily */
      do_rawlog(LT_ERR, T("ERROR: old-style attribute format in object %d"), i);
      return -1;
      break;
    case '<':                  /* end of list */
      if ('\n' != getc(f)) {
        do_rawlog(LT_ERR, T("ERROR: no line feed after < on object %d"), i);
        return -1;
      }
      return count;
    default:
      if (c == EOF) {
        do_rawlog(LT_ERR, T("ERROR: Unexpected EOF on file."));
        return -1;
      }
      do_rawlog(LT_ERR,
                T
                ("ERROR: Bad character %c (%d) in attribute list on object %d"),
                c, c, i);
      do_rawlog(LT_ERR,
                T("  (expecting ], >, or < as first character of the line.)"));
      if (*tbuf1)
        do_rawlog(LT_ERR, T("  Last attribute read was: %s"), tbuf1);
      else
        do_rawlog(LT_ERR, T("  No attributes had been read yet."));
      return -1;
    }
}

extern PRIV attr_privs_view[];

void
db_read_attrs(FILE * f, dbref i, int count)
{
   char name[ATTRIBUTE_NAME_LIMIT + 1];
   char l_key[BUFFER_LEN];
   char value[BUFFER_LEN + 1];
   boolexp w_lock, r_lock;
   time_t modtime;
   dbref owner;
   int derefs = 0, lock_derefs = 0;
   int flags;
   char *tmp;
   int found = 0;

   List(i) = NULL;

   for(;;) {
     int c;
 
     c = fgetc(f);
     ungetc(c, f);
     
     if (c != ' ')
       break;
 
     found++;
 
     db_read_this_labeled_string(f, "name", &tmp);
     strcpy(name, tmp);
     db_read_this_labeled_dbref(f, "owner", &owner);
     db_read_this_labeled_string(f, "flags", &tmp);
     flags = string_to_privs(attr_privs_view, tmp, 0);
     db_read_this_labeled_number(f, "derefs", &derefs);

     if(HAS_COBRADBFLAG(indb_flags,DBF_NEW_ATR_LOCK)) {
       db_read_this_labeled_string(f, "writelock", &tmp);
       strcpy(l_key, tmp);
       db_read_this_labeled_number(f, "derefs", &lock_derefs);
       w_lock = parse_boolexp_d(GOD, l_key, (char *) "ATTR", lock_derefs);
       
       db_read_this_labeled_string(f, "readlock", &tmp);
       strcpy(l_key, tmp);
       db_read_this_labeled_number(f, "derefs", &lock_derefs);
       r_lock = parse_boolexp_d(GOD, l_key, (char *) "ATTR", lock_derefs);

     }  else {
       w_lock = TRUE_BOOLEXP;
       r_lock = TRUE_BOOLEXP;
     }

     if (HAS_COBRADBFLAG(indb_flags, DBF_ATR_MODTIME)) {
       db_read_this_labeled_time_t(f, "modtime", &modtime);
     } else {
       modtime = CreTime(i);
     }

     db_read_this_labeled_string(f, "value", &tmp);
     strcpy(value, tmp);
     atr_new_add(i, name, value, owner, flags, derefs, w_lock, r_lock, modtime);
   }
   if (found != count) 
    do_rawlog(LT_ERR,
	      T("WARNING: Actual attribute count (%d) different than expected count (%d)."),
		found, count);

}

/* Load Seperate Flag Database
 */

int load_flag_db(FILE *f) {
  int c;
  char *tmp;

  loading_db = 1;
  
  if((c = fgetc(f)) != '+' && (c = fgetc(f)) != '+') {
    do_rawlog(LT_ERR, T("Flag database does not start with a version string"));
    return -1;
  }

 
  flagdb_flags = ((getref(f) - 2) / 256) - 5;
  db_read_this_labeled_string(f, "savedtime", &tmp); /* We don't use this info anywhere 'yet' */
  do_rawlog(LT_ERR, T("Loading flag databased saved on %s UTC"), tmp);

  while((c = fgetc(f)) != EOF) {
    if(c == '+') {
	c = fgetc(f);
	 if(c == 'F') {
	   (void) getstring_noalloc(f);
	   flag_read_all(f, "FLAG");
	 } else if(c == 'P'){
	   (void) getstring_noalloc(f);
	   powers_read_all(f);
	 }else {
	   do_rawlog(LT_ERR, T("Unrecgonized database format"));
	   return -1;
	 }
    } else if(c == '*'){
      char buff[80];
      ungetc('*', f);
      fgets(buff, sizeof buff, f);
      if(strcmp(buff, EOD) != 0){
	do_rawlog(LT_ERR, T("ERROR: No end of dump entry."));
	return -1;
      } else {
	do_rawlog(LT_ERR, T("READING: done"));
	break;
      }
    } else {
      do_rawlog(LT_ERR, T("Unrecognized database format"));
      return -1;
    }
  }
  loading_db = 0;
  return 0;
}

void db_write_flag_db(FILE *f) {
  int flags;

  /* Write out db flags */
  flags = 5;
  flags += FLAG_DBF_CQUOTA_RENAME;
  /* */

  OUTPUT(fprintf(f, "+V%ld\n", flagdb_flags * 256 + 2));
  db_write_labeled_string(f, "savedtime", show_time(mudtime, 1));
  db_write_flags(f);
  db_write_powers(f);
  OUTPUT(fputs(EOD, f));
}



/** Read a non-labeled database from a file.
 * \param f the file to read from
 * \return number of objects in the database
 */

static dbref
db_read_oldstyle(FILE * f)
{
  int c, opbits;
  dbref i;
  struct object *o;
  int temp = 0;
  time_t temp_time = 0;
  div_pbits old_dp_bytes;

  for (i = 0;; i++) {
    /* Loop invariant: we always begin at the beginning of a line. */
    errobj = i;
    c = getc(f);
    switch (c) {
      /* make sure database is at least this big *1.5 */
    case '~':
      db_init = (getref(f) * 3) / 2;
      init_objdata_htab(db_init, NULL);
      break;
      /* Use the MUSH 2.0 header stuff to see what's in this db */
    case '+':
      c = getc(f);		/* Skip the V */
      if (c == 'F') {
	(void) getstring_noalloc(f);
	flag_read_all(f, "FLAG");
      } else {
	do_rawlog(LT_ERR, T("Unrecognized database format!"));
	return -1;
      }
      break;
      /* old fashioned database */
    case '#':
    case '&':			/* zone oriented database */
      do_rawlog(LT_ERR, T("ERROR: old style database."));
      return -1;
      break;
      /* new database */
    case '!':			/* non-zone oriented database */
      /* make space */
      i = getref(f);
      db_grow(i + 1);
      /* read it in */
      o = db + i;
      set_name(i, getstring_noalloc(f));
      o->location = getref(f);
      o->contents = getref(f);
      o->exits = getref(f);
      o->next = getref(f);
      o->parent = getref(f);
      o->locks = NULL;
      get_new_locks(i, f, -1);
      o->owner = getref(f);
      o->zone = getref(f);
      if (indb_flags & DBF_DIVISIONS) {
        o->division.object = getref(f);
        o->division.level = getref(f);
	/* This is the spot for poweres.. read it to NULL ville */
	getref(f);
	/* Read Old Style and convert */
        old_dp_bytes = getbytes(f);
	o->division.dp_bytes = (div_pbits) convert_old_cobra_powers(old_dp_bytes);
      } else {
        o->division.object = -1;
        o->division.level = 1;
        o->division.dp_bytes = NULL;
      }
      s_Pennies(i, getref(f));
      if (indb_flags & DBF_NEW_FLAGS) {
	o->type = getref(f);
	o->flags = string_to_bits("FLAG", getstring_noalloc(f));
      } else {
	int old_flags, old_toggles;
	old_flags = getref(f);
	old_toggles = getref(f);
	if ((o->type = type_from_old_flags(old_flags)) < 0) {
	  do_rawlog(LT_ERR, T("Unable to determine type of #%d\n"), i);
	  return -1;
	}
	o->flags =
	  flags_from_old_flags(old_flags, old_toggles, o->type);
      }

      /* We need to have flags in order to do this right, which is why
       * we waited until now
       */
      switch (Typeof(i)) {
      case TYPE_PLAYER:
	current_state.players++;
	current_state.garbage--;
	break;
      case TYPE_THING:
	current_state.things++;
	current_state.garbage--;
	break;
      case TYPE_EXIT:
	current_state.exits++;
	current_state.garbage--;
	break;
      case TYPE_ROOM:
	current_state.rooms++;
	current_state.garbage--;
	break;
      case TYPE_DIVISION:
        current_state.divisions++;
        current_state.garbage--;
        break;
      }

      if (IsPlayer(i) && (strlen(o->name) > (Size_t) PLAYER_NAME_LIMIT)) {
	char buff[BUFFER_LEN + 1];	/* The name plus a NUL */
	strncpy(buff, o->name, PLAYER_NAME_LIMIT);
	buff[PLAYER_NAME_LIMIT] = '\0';
	set_name(i, buff);
	do_rawlog(LT_CHECK,
		  T(" * Name of #%d is longer than the maximum, truncating.\n"),
		  i);
      } else if (!IsPlayer(i) && (strlen(o->name) > OBJECT_NAME_LIMIT)) {
	char buff[OBJECT_NAME_LIMIT + 1];	/* The name plus a NUL */
	strncpy(buff, o->name, OBJECT_NAME_LIMIT);
	buff[OBJECT_NAME_LIMIT] = '\0';
	set_name(i, buff);
	do_rawlog(LT_CHECK,
		  T(" * Name of #%d is longer than the maximum, truncating.\n"),
		  i);
      }

      if (!(indb_flags & DBF_VALUE_IS_COST) && IsThing(i))
	s_Pennies(i, (Pennies(i) + 1) * 5);
      if(!(indb_flags & DBF_DIVISIONS)) {
        opbits = getref(f);	/* save old pows to c to pass to conversion after object is read*/
	convert_object_powers(i, opbits);
      }

      /* Remove the STARTUP and ACCESSED flags */
      if (!(indb_flags & DBF_NO_STARTUP_FLAG)) {
	clear_flag_internal(i, "STARTUP");
	clear_flag_internal(i, "ACCESSED");
      }

      /* Clear the GOING flags. If it was scheduled for destruction
       * when the db was saved, it gets a reprieve.
       */
      if(!Guest(i)) {
        clear_flag_internal(i, "GOING");
        clear_flag_internal(i, "GOING_TWICE");
      }

      /* If there are channels in the db, read 'em in */
      /* We don't support this anymore, so we just discard them */
      if (!(indb_flags & DBF_NO_CHAT_SYSTEM))
	temp = getref(f);
      else
	temp = 0;

      /* If there are warnings in the db, read 'em in */
      temp = MAYBE_GET(f, DBF_WARNINGS);
      o->warnings = temp;
      /* If there are creation times in the db, read 'em in */
      temp_time = MAYBE_GET(f, DBF_CREATION_TIMES);
      if (temp_time)
	o->creation_time = (time_t) temp_time;
      else
	o->creation_time = mudtime;
      temp_time = MAYBE_GET(f, DBF_CREATION_TIMES);
      if (temp_time || IsPlayer(i))
	o->modification_time = (time_t) temp_time;
      else
	o->modification_time = o->creation_time;

      /* read attribute list for item */
      if ((o->attrcount = get_list(f, i)) < 0) {
	do_rawlog(LT_ERR, T("ERROR: bad attribute list object %d"), i);
	return -1;
      }
      if (!(indb_flags & DBF_AF_NODUMP)) {
	/* Clear QUEUE and SEMAPHORE attributes */
	atr_clr(i, "QUEUE", GOD);
	atr_clr(i, "SEMAPHORE", GOD);
      }
      /* check to see if it's a player */
      if (IsPlayer(i)) {
	add_player(i);
	clear_flag_internal(i, "CONNECTED");
      }
      break;

    case '*':
      {
	char buff[80];
	ungetc('*', f);
	fgets(buff, sizeof buff, f);
	if (strcmp(buff, EOD) != 0) {
	  do_rawlog(LT_ERR, T("ERROR: No end of dump after object #%d"), i - 1);
	  return -1;
	} else {
	  if(!(indb_flags & DBF_DIVISIONS) || (indb_flags & DBF_TYPE_GARBAGE)) {
	    dbref master_division;
	      /* Final Step of DB Conversion
	       * We're gonna first Create the Master Division
	       * Second we're gonna loop through the database
	       * set all players except unregistered players to the master division
	       * and set all levels appropriate
	       **/
	    i = GOD;
	    master_division = new_object();
	    set_name(master_division, "Master Division");
	    Type(master_division) = TYPE_DIVISION;
	    PUSH(master_division, Contents(i));
	    Owner(master_division) = i;
	    CreTime(master_division) = ModTime(master_division) = mudtime;
	    atr_new_add(master_division, "DESCRIBE",
		"This is the master division that comes before all divisions.", i,
		AF_VISUAL | AF_NOPROG | AF_PREFIXMATCH, 1, TRUE_BOOLEXP, TRUE_BOOLEXP, mudtime);
	    current_state.divisions++;
	    SLEVEL(master_division) =  LEVEL_DIRECTOR;
	    SLEVEL(i) = LEVEL_GOD;
	    /* Division & God Setup now run through the database & set everyitng else up */
	    for(i = 0; i < db_top; i++)
	    {
	      if(has_flag_by_name(i, "UNREGISTERED", TYPE_PLAYER)){
		SLEVEL(i) = LEVEL_UNREGISTERED;
		clear_flag_internal(i, "UNREGISTERED");
	      } else if(i != master_division) {
		SDIV(i).object = master_division;
		if(!has_flag_by_name(i, "WIZARD", NOTYPE) &&
		    !has_flag_by_name(i, "ROYALTY", NOTYPE) && SLEVEL(i) != -500) {
		  SLEVEL(i) = LEVEL_PLAYER;
		} else if(SLEVEL(i) == -500) {
		  SLEVEL(i) = LEVEL_GUEST;
		} else { /* They look like wizard or royalty.. Set 'em to 29 */
		  SLEVEL(i) = LEVEL_DIRECTOR;
		}
	      }
	    }
	       	do_rawlog(LT_ERR, T("DB Conversion complete"));
	  }
	  do_rawlog(LT_ERR, "READING: done");
	  loading_db = 0;
	  fix_free_list();
	  dbck();
	  log_mem_check();
	  return db_top;
	}
      }
    default:
      do_rawlog(LT_ERR, T("ERROR: failed object %d"), i);
      return -1;
    }
  }
}

/** Read the object database from a file.
 * This function reads the entire database from a file. See db_write()
 * for some notes about the expected format.
 * \param f file pointer to read from.
 * \return number of objects in the database.
 */
dbref
db_read(FILE * f)
{
  int c;
  dbref i = 0;
  char *tmp;
  struct object *o;
  div_pbits div_powers;
  /* Changed because CobraMUSH 0.7 and after assume DBF_TYPE_GARBAGE implies
   * a PennMUSH database that needs to be converted. */
  /* int minimum_flags =
    DBF_NEW_STRINGS | DBF_TYPE_GARBAGE | DBF_SPLIT_IMMORTAL | DBF_NO_TEMPLE |
    DBF_SPIFFY_LOCKS; */

  int minimum_flags =
    DBF_NEW_STRINGS | DBF_SPLIT_IMMORTAL | DBF_NO_TEMPLE | DBF_SPIFFY_LOCKS;

  log_mem_check();

  loading_db = 1;

  clear_players();
  db_free();
  indb_flags = 1;

  c = fgetc(f);
  if (c != '+') {
    do_rawlog(LT_ERR, T("Database does not start with a version string"));
    return -1;
  }
  c = fgetc(f);
  if (c != 'V') {
    do_rawlog(LT_ERR, T("Database does not start with a version string"));
    return -1;
  }
  indb_flags = ((getref(f) - 2) / 256) - 5;
  /* give us some brains.. for if we're using the flagfile or not */
  if(!use_flagfile)
    flagdb_flags = indb_flags;
  /* if you want to read in an old-style database, use an earlier
   * patchlevel to upgrade.
   */
  if (((indb_flags & minimum_flags) != minimum_flags) ||
      (indb_flags & DBF_NO_POWERS)) {
    do_rawlog(LT_ERR, T("ERROR: Old database without required dbflags."));
    return -1;
  }

  if (!(indb_flags & DBF_LABELS))
    return db_read_oldstyle(f);

  db_read_this_labeled_string(f, "savedtime", &tmp);
  strcpy(db_timestamp, tmp);

  do_rawlog(LT_ERR, T("Loading database saved on %s UTC"), db_timestamp);

  while ((c = fgetc(f)) != EOF) {
    switch (c) {
    case '+':
      c = fgetc(f);
      if (c == 'F') {
	(void) getstring_noalloc(f);
	flag_read_all(f, "FLAG");
      } else if (c == 'P') {
	(void) getstring_noalloc(f);
	if(!(indb_flags & DBF_TYPE_GARBAGE))
	  powers_read_all(f);
	else {
	  if(ps_tab._Read_Powers_ == 0)
	    init_powers();
	  flag_read_all(f, NULL);
	}
      } else {
	do_rawlog(LT_ERR, T("Unrecognized database format!"));
	return -1;
      }
      break;
    case '~':
      db_init = (getref(f) * 3) / 2;
      init_objdata_htab(db_init, NULL);
      break;
    case '!':
      /* Read an object */
      {
	char *label, *value;
	/* Thre should be an entry in the enum and following table and
	   switch for each top-level label associated with an
	   object. Not finding a label is not an error; the default
	   set in new_object() is used. Finding a label not listed
	   below is an error. */
	enum known_labels {
	  LBL_NAME, LBL_LOCATION, LBL_CONTENTS, LBL_EXITS,
	  LBL_NEXT, LBL_PARENT, LBL_LOCKS, LBL_OWNER, LBL_ZONE,
	  LBL_PENNIES, LBL_TYPE, LBL_FLAGS, LBL_POWERGROUPS, LBL_POWERS, LBL_WARNINGS,
	  LBL_CREATED, LBL_MODIFIED, LBL_ATTRS, LBL_ERROR, LBL_LEVEL,
	  LBL_DIVOBJ, LBL_PWRLVL, LBL_LMOD
	};
	struct label_table {
	  const char *label;
	  enum known_labels tag;
	};
	struct label_table fields[] = {
	  {"name", LBL_NAME},
	  {"location", LBL_LOCATION},
	  {"contents", LBL_CONTENTS},
	  {"exits", LBL_EXITS},
	  {"next", LBL_NEXT},
	  {"parent", LBL_PARENT},
	  {"lockcount", LBL_LOCKS},
	  {"owner", LBL_OWNER},
	  {"zone", LBL_ZONE},
	  {"pennies", LBL_PENNIES},
	  {"type", LBL_TYPE},
	  {"flags", LBL_FLAGS},
	  {"powers", LBL_POWERS},
	  {"warnings", LBL_WARNINGS},
	  {"created", LBL_CREATED},
	  {"modified", LBL_MODIFIED},
	  {"attrcount", LBL_ATTRS},
	  {"division_object", LBL_DIVOBJ},
	  {"level", LBL_LEVEL},
	  {"powerlevel", LBL_PWRLVL},
	  {"powergroup", LBL_POWERGROUPS},
	  {"lastmod", LBL_LMOD},
	   /* Add new label types here. */
	  {NULL, LBL_ERROR}
	}, *entry;
	enum known_labels the_label;

	i = getref(f);
	db_grow(i + 1);
	o = db + i;
	while (1) {
	  c = fgetc(f);
	  ungetc(c, f);
	  /* At the start of another object or the EOD marker */
	  if (c == '!' || c == '*')
	    break;
	  db_read_labeled_string(f, &label, &value);
	  the_label = LBL_ERROR;
	  /* Look up the right enum value in the label table */
	  for (entry = fields; entry->label; entry++) {
	    if (strcmp(entry->label, label) == 0) {
	      the_label = entry->tag;
	      break;
	    }
	  }
	  switch (the_label) {
	  case LBL_NAME:
	    set_name(i, value);
	    break;
	  case LBL_LOCATION:
	    o->location = qparse_dbref(value);
	    break;
	  case LBL_CONTENTS:
	    o->contents = qparse_dbref(value);
	    break;
	  case LBL_EXITS:
	    o->exits = qparse_dbref(value);
	    break;
	  case LBL_NEXT:
	    o->next = qparse_dbref(value);
	    break;
	  case LBL_PARENT:
	    o->parent = qparse_dbref(value);
	    break;
	  case LBL_LOCKS:
	    get_new_locks(i, f, parse_integer(value));
	    break;
	  case LBL_LEVEL:
	    o->division.level = parse_integer(value); 
	    break;
	  case LBL_PWRLVL:
	    /* Do nothing with this value */
	    break;
	  case LBL_DIVOBJ:
	    o->division.object = qparse_dbref(value);
	    break;
	  case LBL_OWNER:
	    o->owner = qparse_dbref(value);
	    break;
	  case LBL_ZONE:
	    o->zone = qparse_dbref(value);
	    break;
	  case LBL_PENNIES:
	    s_Pennies(i, parse_integer(value));
	    break;
	  case LBL_TYPE:
	    o->type = parse_integer(value);
	    switch (Typeof(i)) {
	    case TYPE_PLAYER:
	      current_state.players++;
	      current_state.garbage--;
	      break;
	    case TYPE_DIVISION:
	      current_state.divisions++;
	      current_state.garbage--;
	      break;
	    case TYPE_THING:
	      current_state.things++;
	      current_state.garbage--;
	      break;
	    case TYPE_EXIT:
	      current_state.exits++;
	      current_state.garbage--;
	      break;
	    case TYPE_ROOM:
	      current_state.rooms++;
	      current_state.garbage--;
	      break;
	    }
	    break;
	  case LBL_FLAGS:
	    o->flags = string_to_bits("FLAG", value);
	    /* Clear the GOING flags. If it was scheduled for destruction
	     * when the db was saved, it gets a reprieve.
	     */
	    clear_flag_internal(i, "GOING");
	    clear_flag_internal(i, "GOING_TWICE");
	    break;
	  case LBL_POWERS:
	      div_powers = string_to_dpbits(value);
	      if(power_is_zero(div_powers, DP_BYTES) == 0) {
		o->division.dp_bytes = NULL;
		mush_free(div_powers, "POWER_SPOT");
	      } else o->division.dp_bytes =  div_powers;
	    break;
	  case LBL_POWERGROUPS:
	    powergroup_db_set(NOTHING, i, value, 0);
	    break;
	  case LBL_WARNINGS:
	    o->warnings = parse_warnings(NOTHING, value);
	    break;
	  case LBL_CREATED:
	    o->creation_time = (time_t) parse_integer(value);
	    break;
	  case LBL_MODIFIED:
	    o->modification_time = (time_t) parse_integer(value);
	    break;
	  case LBL_LMOD:
	    db[i].lastmod = NULL;
	    set_lmod(i, value);
	    break;
	  case LBL_ATTRS:
	    {
	      int attrcount = parse_integer(value);
	      db_read_attrs(f, i, attrcount);
	    }
	    break;
	  case LBL_ERROR:
	  default:
	    do_rawlog(LT_ERR, T("Unrecognized field '%s' in object #%d"),
		      label, i);
	    return -1;
	  }
	}
	if (IsPlayer(i) && (strlen(o->name) > (size_t) PLAYER_NAME_LIMIT)) {
	  char buff[BUFFER_LEN + 1];	/* The name plus a NUL */
	  strncpy(buff, o->name, PLAYER_NAME_LIMIT);
	  buff[PLAYER_NAME_LIMIT] = '\0';
	  set_name(i, buff);
	  do_rawlog(LT_CHECK,
		    T
		    (" * Name of #%d is longer than the maximum, truncating.\n"),
		    i);
	} else if (!IsPlayer(i) && (strlen(o->name) > OBJECT_NAME_LIMIT)) {
	  char buff[OBJECT_NAME_LIMIT + 1];	/* The name plus a NUL */
	  strncpy(buff, o->name, OBJECT_NAME_LIMIT);
	  buff[OBJECT_NAME_LIMIT] = '\0';
	  set_name(i, buff);
	  do_rawlog(LT_CHECK,
		    T
		    (" * Name of #%d is longer than the maximum, truncating.\n"),
		    i);
	}
	if (IsPlayer(i)) {
	  add_player(i);
	  clear_flag_internal(i, "CONNECTED");
	}
      }
      break;
    case '*':
      {
	char buff[80];
	ungetc('*', f);
	fgets(buff, sizeof buff, f);
	if (strcmp(buff, EOD) != 0) {
	  do_rawlog(LT_ERR, T("ERROR: No end of dump after object #%d"), i - 1);
	  return -1;
	} else {
	  loading_db = 0;
	  log_mem_check();
	  fix_free_list();
	  dbck();
	  log_mem_check();
	  do_rawlog(LT_ERR, "READING: done");
	  return db_top;
	}
      }
    default:
      do_rawlog(LT_ERR, T("ERROR: failed object %d"), i);
      return -1;
    }
  }
  return -1;
}

static void
init_objdata_htab(int size, void (*free_data) (void *))
{
  if (size < 128)
    size = 128;
  hash_init(&htab_objdata, size, 4, free_data);
  hashinit(&htab_objdata_keys, 8, 32);
}

void init_postconvert() {
  dbref master_division, i;

	  if((!(indb_flags & DBF_DIVISIONS) || (indb_flags & DBF_TYPE_GARBAGE)) && CreTime(0) != globals.first_start_time ) {
	    do_rawlog(LT_ERR, "Beginning Pennmush to CobraMUSH database conversion process.");
	      /* Final Step of DB Conversion
	       * We're gonna first Create the Master Division
	       * Second we're gonna loop through the database
	       * set all players except unregistered players to the master division
	       * and set all levels appropriate
	       **/
	    i = GOD;
	    master_division = new_object();
	    set_name(master_division, "Master Division");
	    Type(master_division) = TYPE_DIVISION;
	    PUSH(master_division, Contents(i));
	    Owner(master_division) = i;
	    CreTime(master_division) = ModTime(master_division) = mudtime;
	    Location(master_division) = i;
	    atr_new_add(master_division, "DESCRIBE",
		"This is the master division that comes before all divisions.", i,
		AF_VISUAL | AF_NOPROG | AF_PREFIXMATCH, 1, TRUE_BOOLEXP, TRUE_BOOLEXP, mudtime);
	    current_state.divisions++;
	    SLEVEL(master_division) =  LEVEL_DIRECTOR;
	    SLEVEL(i) = LEVEL_GOD;
	    /* Division & God Setup now run through the database & set everyitng else up */
	    for(i = 0; i < db_top; i++)
	    {
	      if(IsGarbage(i))
		continue;
	      if(has_flag_by_name(i, "UNREGISTERED", TYPE_PLAYER)){
		SLEVEL(i) = LEVEL_UNREGISTERED;
		clear_flag_internal(i, "UNREGISTERED");
	      } else if(i != master_division) {
		SDIV(i).object = master_division;
		if(!has_flag_by_name(i, "WIZARD", NOTYPE) &&
		    !has_flag_by_name(i, "ROYALTY", NOTYPE) && SLEVEL(i) != -500) {
		  SLEVEL(i) = LEVEL_PLAYER;
		} else if(SLEVEL(i) == -500) {
		  SLEVEL(i) = LEVEL_GUEST;
		} else if(has_flag_by_name(i, "WIZARD", NOTYPE)) { /* They look like wizard or royalty.. Set 'em up right */
		  SLEVEL(i) = LEVEL_DIRECTOR;
		  powergroup_db_set(GOD, i, "WIZARD", 1);
		} else if(has_flag_by_name(i, "ROYALTY", NOTYPE)) {
		  SLEVEL(i) = LEVEL_ADMIN;
		  powergroup_db_set(GOD, i, "ROYALTY", 1);
		}
	      }
	    }
	       	do_rawlog(LT_ERR, T("DB Conversion complete"));
	  }

}

/** Add data to the object data hashtable. 
 * This hash table is typically used to store transient object data
 * that is built at database load and isn't saved to disk, but it
 * can be used for other purposes as well - it's a good general
 * tool for hackers who want to add their own data to objects.
 * This function adds data to the hashtable.
 * \param thing dbref of object to associate the data with.
 * \param keybase base string for type of data.
 * \param data pointer to the data to store.
 * \return data passed in.
 */
void *
set_objdata(dbref thing, const char *keybase, void *data)
{
  hashdelete(tprintf("%s_#%d", keybase, thing), &htab_objdata);
  if (data) {
    if (hashadd(tprintf("%s_#%d", keybase, thing), data, &htab_objdata) < 0)
      return NULL;
    if (hash_find(&htab_objdata_keys, keybase) == NULL) {
      char *newkey = strdup(keybase);
      hashadd(keybase, (void *) &newkey, &htab_objdata_keys);
    }
  }
  return data;
}

/** Retrieve data from the object data hashtable.
 * \param thing dbref of object data is associated with.
 * \param keybase base string for type of data.
 * \return data stored for that object and keybase, or NULL.
 */
void *
get_objdata(dbref thing, const char *keybase)
{
  return hashfind(tprintf("%s_#%d", keybase, thing), &htab_objdata);
}

/** Clear all of an object's data from the object data hashtable.
 * This function clears any data associated with a given object
 * that's in the object data hashtable (under any keybase).
 * It's used before we free the object.
 * \param thing dbref of object data is associated with.
 */
void
clear_objdata(dbref thing)
{
  char *p;
  for (p = (char *) hash_firstentry(&htab_objdata_keys);
       p; p = (char *) hash_nextentry(&htab_objdata_keys)) {
    set_objdata(thing, p, NULL);
  }
}

/** Create a basic 3-object (Start Room, God, Master Room) database. */
void
create_minimal_db(void)
{
  dbref start_room, god, master_room, master_division;

  int desc_flags = AF_VISUAL | AF_NOPROG | AF_PREFIXMATCH;

  start_room = new_object();	/* #0 */
  god = new_object();		/* #1 */
  master_room = new_object();	/* #2 */
  master_division = new_object(); /* #3 */

  init_objdata_htab(DB_INITIAL_SIZE, NULL);

  set_name(start_room, "Room Zero");
  Type(start_room) = TYPE_ROOM;
  Flags(start_room) = string_to_bits("FLAG", "LINK_OK");
  atr_new_add(start_room, "DESCRIBE", "You are in Room Zero.", GOD, desc_flags,
	      1, TRUE_BOOLEXP, TRUE_BOOLEXP, mudtime);
  CreTime(start_room) = ModTime(start_room) = mudtime;
  current_state.rooms++;

  set_name(god, "One");
  Type(god) = TYPE_PLAYER;
  Location(god) = start_room;
  Home(god) = start_room;
  Owner(god) = god;
  CreTime(god) = mudtime;
  ModTime(god) = (time_t) 0;
  SDIV(god).object = master_division;
  add_lock(god, god, Basic_Lock, parse_boolexp(god, "=me", Basic_Lock), -1);
  add_lock(god, god, Enter_Lock, parse_boolexp(god, "=me", Enter_Lock), -1);
  add_lock(god, god, Use_Lock, parse_boolexp(god, "=me", Use_Lock), -1);
  atr_new_add(god, "DESCRIBE", "You see Number One.", god, desc_flags, 1, TRUE_BOOLEXP, TRUE_BOOLEXP, mudtime);
#ifdef USE_MAILER
  atr_new_add(god, "MAILCURF", "0", god, AF_LOCKED | AF_NOPROG, 1, TRUE_BOOLEXP, TRUE_BOOLEXP, mudtime);
  add_folder_name(god, 0, "inbox");
#endif
  PUSH(god, Contents(start_room));
  add_player(god);
  s_Pennies(god, START_BONUS);
  local_data_create(god);
  current_state.players++;

  set_name(master_room, "Master Room");
  Type(master_room) = TYPE_ROOM;
  Flags(master_room) = string_to_bits("FLAG", "FLOATING");
  Owner(master_room) = god;
  CreTime(master_room) = ModTime(master_room) = mudtime;
  atr_new_add(master_room, "DESCRIBE",
	      "This is the master room. Any exit in here is considered global. The same is true for objects with $-commands placed here.",
	      god, desc_flags, 1, TRUE_BOOLEXP, TRUE_BOOLEXP, mudtime);
  current_state.rooms++;
/* Master Division */
  Location(master_division) = god;
  Home(master_division) = god;
  set_name(master_division, "Master Division");
  Type(master_division) = TYPE_DIVISION; 
  PUSH(master_division, Contents(god));
  Owner(master_division) = god;
  CreTime(master_division) = ModTime(master_division) = mudtime; 
  atr_new_add(master_division, "DESCRIBE", 
      "This is the master division that comes before all divisions.", god, desc_flags, 1, TRUE_BOOLEXP, TRUE_BOOLEXP, mudtime);
  current_state.divisions++;
  /* division stuff */
  SDIV(start_room).object = master_division;
  SDIV(master_room).object = master_division;
  SLEVEL(god) = LEVEL_GOD;
  SLEVEL(start_room) = LEVEL_SYSBUILDER;
  SLEVEL(master_room) = LEVEL_SYSCODER;
  SLEVEL(master_division) = LEVEL_DIRECTOR;
}
