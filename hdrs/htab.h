/* htab.h - Structures and declarations needed for table hashing */

#ifndef __HTAB_H
#define __HTAB_H

#define SOME_KEY_LEN 10

#define HTAB_UPSCALE   4
#define HTAB_DOWNSCALE 2

typedef struct hashentry HASHENT;
/** A hash table entry.
 */
struct hashentry {
  struct hashentry *next;	/**< Pointer to next entry */
  void *data;			/**< Data for this entry */
  /* int extra_size; */
  char key[SOME_KEY_LEN];	/**< Key for this entry */
};

#define HASHENT_SIZE (sizeof(HASHENT)-SOME_KEY_LEN)

typedef struct hashtable HASHTAB;
/** A hash table.
 */
struct hashtable {
  int hashsize;                 /**< Size of hash table */
  int mask;                     /**< Mask for entries in table */
  int entries;                  /**< Number of entries stored */
  HASHENT **buckets;            /**< Pointer to pointer to entries */
  int last_hval;                /**< State for hashfirst & hashnext. */
  HASHENT *last_entry;          /**< State for hashfirst & hashnext. */
  int entry_size;               /**< Size of each entry */
  void (*free_data) (void *);   /**< Function to call on data when deleting
                                   a entry. */
};

#define get_hashmask(x) hash_getmask(x)
#define hashinit(x,y, z) hash_init(x,y, z, NULL)
#define hashfind(key,tab) hash_value(hash_find(tab,key))
#define hashadd(key,data,tab) hash_add(tab,key,data, 0)
#define hashadds(key, data, tab, size) hash_add(tab, key, data, size)
#define hashdelete(key,tab) hash_delete(tab,hash_find(tab,key))
#define hashflush(tab, size) hash_flush(tab,size)
#define hashfree(tab) hash_flush(tab, 0)
extern int hash_getmask(int *size);
extern void hash_init(HASHTAB *htab, int size, int data_size, void (*)(void *));
extern HASHENT *hash_find(HASHTAB *htab, const char *key);
extern void *hash_value(HASHENT *entry);
extern char *hash_key(HASHENT *entry);
extern void hash_resize(HASHTAB *htab, int size);
extern int hash_add
  (HASHTAB *htab, const char *key, void *hashdata, int extra_size);
extern void hash_delete(HASHTAB *htab, HASHENT *entry);
extern void hash_flush(HASHTAB *htab, int size);
extern void *hash_firstentry(HASHTAB *htab);
extern void *hash_nextentry(HASHTAB *htab);
extern char *hash_firstentry_key(HASHTAB *htab);
extern char *hash_nextentry_key(HASHTAB *htab);
extern void hash_stats_header(dbref player);
extern void hash_stats(dbref player, HASHTAB *htab, const char *hashname);
#endif
