/* privtab.h */
/* Defines a privilege table entry for general use */

#ifndef __PRIVTAB_H
#define __PRIVTAB_H

#include "copyrite.h"
#include "config.h"
#include "confmagic.h"


typedef struct priv_info PRIV;
/** Privileges.
 * This structure represents a privilege and its associated data.
 * Privileges tables are used to provide a unified way to parse
 * a string of restrictions into a bitmask.
 */
struct priv_info {
  const char *name;	/**< Name of the privilege */
  char letter;		/**< One-letter abbreviation */
  long int bits_to_set;	/**< Bitflags required to set this privilege */
  long int bits_to_show;	/**< Bitflags required to see this privilege */
};

#define PrivName(x)     ((x)->name)
#define PrivChar(x)     ((x)->letter)
#define PrivSetBits(x)  ((x)->bits_to_set)
#define PrivShowBits(x) ((x)->bits_to_show)

extern int string_to_privs(PRIV *table, const char *str, long int origprivs);
extern int letter_to_privs(PRIV *table, const char *str, long int origprivs);
extern const char *privs_to_string(PRIV *table, int privs);
extern const char *privs_to_letters(PRIV *table, int privs);

#endif				/* __PRIVTAB_H */
