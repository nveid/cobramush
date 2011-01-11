/* Prefix-matched-key lookups. */

#ifndef PTAB_H
#define PTAB_H

struct ptab_entry;
/** Prefix table.
 * This structure represents a prefix table. In a prefix table, 
 * data is looked up by the best matching prefix of the given key.
 */
typedef struct ptab {
  int state;                    /**< Internal table state */
  int len;                      /**< Table size */
  int maxlen;                   /**< Maximum table size */
  int current;                  /**< Internal table state */
  struct ptab_entry **tab;      /**< Pointer to array of entries */
} PTAB;


void ptab_init(PTAB *);
void ptab_free(PTAB *);
void *ptab_find(PTAB *, const char *);
void *ptab_find_exact(PTAB *, const char *);
void ptab_delete(PTAB *, const char *);
void ptab_start_inserts(PTAB *);
void ptab_end_inserts(PTAB *);
void ptab_insert(PTAB *, const char *, void *);
void ptab_stats_header(dbref);
void ptab_stats(dbref, PTAB *, const char *);
void *ptab_firstentry_new(PTAB *, char *key);
void *ptab_nextentry_new(PTAB *, char *key);
#define ptab_firstentry(x) ptab_firstentry_new(x,NULL)
#define ptab_nextentry(x) ptab_nextentry_new(x,NULL)

#endif                          /* PTAB_H */
