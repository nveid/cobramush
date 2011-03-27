/**
 * \file dbio.h
 *
 * \brief header files for functions for reading/writing database files
 */

#ifndef __DBIO_H
#define __DBIO_H

#include <setjmp.h>
#include <stdio.h>

extern jmp_buf db_err;

/** Run a function, and jump if error */
#define OUTPUT(fun) do { if ((fun) < 0) longjmp(db_err, 1); } while (0)


/* Output */
extern void putref(FILE * f, long int ref);
extern void putstring(FILE * f, const char *s);
extern void db_write_labeled_string(FILE * f, char const *label,
                                    char const *value);
extern void db_write_labeled_number(FILE * f, char const *label, int value);
extern void db_write_labeled_dbref(FILE * f, char const *label, dbref value);
extern void db_write_flag_db(FILE *);

extern dbref db_write(FILE * f, int flag);
extern int db_paranoid_write(FILE * f, int flag);

/* Input functions */
extern const char *getstring_noalloc(FILE * f);
extern long getref(FILE * f);
extern void db_read_this_labeled_string(FILE * f, const char *label,
                                        char **val);
extern void db_read_labeled_string(FILE * f, char **label, char **val);
extern void db_read_this_labeled_number(FILE * f, const char *label, int *val);
extern void db_read_labeled_number(FILE * f, char **label, int *val);
extern void db_read_this_labeled_dbref(FILE * f, const char *label, dbref *val);
extern void db_read_labeled_dbref(FILE * f, char **label, dbref *val);
extern int load_flag_db(FILE *);

extern void init_postconvert();

extern dbref db_read(FILE * f);

#endif
