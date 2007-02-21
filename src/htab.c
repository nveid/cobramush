/**
 * \file htab.c
 *
 * \brief Hashtable routines.
 * This code is largely ripped out of TinyMUSH 2.2.5, with tweaks
 * to make it Penn-compatible by Trivian.
 *
 *
 */

#include "config.h"
#include "copyrite.h"
#include <string.h>
#include "conf.h"
#include "externs.h"

#include "htab.h"
#include "mymalloc.h"
#include "confmagic.h"

HASHENT *hash_new(HASHTAB *htab, const char *key);
static int hash_val(register const char *k, int mask);

/* ---------------------------------------------------------------------------
 * hash_val: Compute hash value of a string for a hash table.
 */
/*#define NEW_HASH_FUN /**/
#ifdef NEW_HASH_FUN

/* This hash function adapted from http://burtleburtle.net/bob/hash/evahash.html */

typedef unsigned long int u4;	/**< unsigned 4-byte type */
typedef unsigned char u1;	/**< unsigned 1-byte type */

/* The mixing step */
#define mix(a,b,c) \
{ \
  a=a-b;  a=a-c;  a=a^(c>>13); \
  b=b-c;  b=b-a;  b=b^(a<<8);  \
  c=c-a;  c=c-b;  c=c^(b>>13); \
  a=a-b;  a=a-c;  a=a^(c>>12); \
  b=b-c;  b=b-a;  b=b^(a<<16); \
  c=c-a;  c=c-b;  c=c^(b>>5);  \
  a=a-b;  a=a-c;  a=a^(c>>3);  \
  b=b-c;  b=b-a;  b=b^(a<<10); \
  c=c-a;  c=c-b;  c=c^(b>>15); \
}

/* The whole new hash function */
static int
hash_val(register const char *k, int mask)
{
  register u4 a, b, c;		/* the internal state */
  u4 len, length;		/* how many key bytes still need mixing */
  static u4 initval = 5432;	/* the previous hash, or an arbitrary value */

  /* Set up the internal state */
  length = len = strlen(k);
  a = b = 0x9e3779b9;		/* the golden ratio; an arbitrary value */
  c = initval;			/* variable initialization of internal state */

   /*---------------------------------------- handle most of the key */
  while (len >= 12) {
    a = a + (k[0] + ((u4) k[1] << 8) + ((u4) k[2] << 16) + ((u4) k[3] << 24));
    b = b + (k[4] + ((u4) k[5] << 8) + ((u4) k[6] << 16) + ((u4) k[7] << 24));
    c = c + (k[8] + ((u4) k[9] << 8) + ((u4) k[10] << 16) + ((u4) k[11] << 24));
    mix(a, b, c);
    k = k + 12;
    len = len - 12;
  }

   /*------------------------------------- handle the last 11 bytes */
  c = c + length;
  switch (len) {		/* all the case statements fall through */
  case 11:
    c = c + ((u4) k[10] << 24);
  case 10:
    c = c + ((u4) k[9] << 16);
  case 9:
    c = c + ((u4) k[8] << 8);
    /* the first byte of c is reserved for the length */
  case 8:
    b = b + ((u4) k[7] << 24);
  case 7:
    b = b + ((u4) k[6] << 16);
  case 6:
    b = b + ((u4) k[5] << 8);
  case 5:
    b = b + k[4];
  case 4:
    a = a + ((u4) k[3] << 24);
  case 3:
    a = a + ((u4) k[2] << 16);
  case 2:
    a = a + ((u4) k[1] << 8);
  case 1:
    a = a + k[0];
    /* case 0: nothing left to add */
  }
  mix(a, b, c);
   /*-------------------------------------------- report the result */
  return c & mask;
}


#else				/* NEW_HASH_FUN */
/** Compute a hash value for mask-style hashing.
 * Given a null key, return 0. Otherwise, add up the numeric value
 * of all the characters and return the sum modulo the size of the
 * hash table.
 * \param key key to hash.
 * \param hashmask hash table size to use as modulus.
 * \return hash value.
 */
int
hash_val(const char *key, int hashmask)
{
  int hash = 0;
  const char *sp;

  if (!key || !*key)
    return 0;
  for (sp = key; *sp; sp++)
    hash = (hash << 5) + hash + *sp;
  return (hash & hashmask);
}
#endif				/* NEW_HASH_FUN */

/* ----------------------------------------------------------------------
 * hash_getmask: Get hash mask for mask-style hashing.
 */

/** Get the hash mask for mask-style hashing.
 * Given the data size, return closest power-of-two less than that size.
 * \param size data size.
 * \return hash mask.
 */
int
hash_getmask(int *size)
{
  int tsize;

  if (!size || !*size)
    return 0;

  for (tsize = 1; tsize < *size; tsize = tsize << 1) ;
  *size = tsize;
  return tsize - 1;
}

/** Initialize a hashtable.
 * \param htab pointer to hash table to initialize.
 * \param size size of hashtable.
 * \param data_size size of an individual datum to store in the table.
 */
void
hash_init(HASHTAB *htab, int size, int data_size)
{
  int i;

  htab->mask = get_hashmask(&size);
  htab->hashsize = size;
  htab->entries = 0;
  htab->buckets = mush_malloc(size * sizeof(HASHENT *), "hash_buckets");
  for (i = 0; i < size; i++)
    htab->buckets[i] = NULL;

  htab->entry_size = data_size;
}

/** Return a hashtable entry given a key.
 * \param htab pointer to hash table to search.
 * \param key key to look up in the table.
 * \return pointer to hash table entry for given key.
 */
HASHENT *
hash_find(HASHTAB *htab, const char *key)
{
  int hval, cmp;
  HASHENT *hptr;

  if (!htab->buckets)
    return NULL;

  hval = hash_val(key, htab->mask);
  for (hptr = htab->buckets[hval]; hptr != NULL; hptr = hptr->next) {
    cmp = strcmp(key, hptr->key);
    if (cmp == 0) {
      return hptr;
    } else if (cmp < 0)
      break;
  }
  return NULL;
}

/** Return the value stored in a hash entry.
 * \param entry pointer to a hash table entry.
 * \return generic pointer to the stored value.
 */
void *
hash_value(HASHENT *entry)
{
  if (entry)
    return entry->data;
  else
    return NULL;
}

/** Return the key stored in a hash entry.
 * \param entry pointer to a hash table entry.
 * \return pointer to the stored key.
 */
char *
hash_key(HASHENT *entry)
{
  if (entry)
    return entry->key;
  else
    return NULL;
}

/** Resize a hash table.
 * \param htab pointer to hashtable.
 * \param size new size.
 */
void
hash_resize(HASHTAB *htab, int size)
{
  int i;
  HASHENT **oldarr;
  HASHENT **newarr;
  HASHENT *hent, *nent, *curr, *old;
  int hval;
  int osize;
  int mask;

  /* We don't want hashes outside these limits */
  if ((size < (1 << 4)) || (size > (1 << 20)))
    return;

  /* Save the old data we need */
  osize = htab->hashsize;
  oldarr = htab->buckets;

  mask = htab->mask = get_hashmask(&size);

  if (size == htab->hashsize)
    return;

  htab->hashsize = size;
  newarr =
    (HASHENT **) mush_malloc(size * sizeof(struct hashentry *), "hash_buckets");
  htab->buckets = newarr;
  for (i = 0; i < size; i++)
    newarr[i] = NULL;

  for (i = 0; i < osize; i++) {
    hent = oldarr[i];
    while (hent) {
      nent = hent->next;
      hval = hash_val(hent->key, mask);
      for (curr = newarr[hval], old = NULL; curr; old = curr, curr = curr->next) {
	if (strcmp(curr->key, hent->key) > 0)
	  break;
      }
      if (old) {
	old->next = hent;
	hent->next = curr;
      } else {
	hent->next = newarr[hval];
	newarr[hval] = hent;
      }
      hent = nent;
    }
  }
  mush_free(oldarr, "hash_buckets");

  return;
}

HASHENT *
hash_new(HASHTAB *htab, const char *key)
{
  int hval;
  size_t keylen;
  HASHENT *hptr, *curr, *old;

  hptr = hash_find(htab, key);
  if (hptr)
    return hptr;

  if (htab->entries > (htab->hashsize * HTAB_UPSCALE))
    hash_resize(htab, htab->hashsize << 1);

  hval = hash_val(key, htab->mask);
  htab->entries++;
  keylen = strlen(key) + 1;
  hptr = (HASHENT *) mush_malloc(HASHENT_SIZE + keylen, "hash_entry");
  memcpy(hptr->key, key, keylen);
  hptr->data = NULL;

  if (!htab->buckets[hval] || strcmp(key, htab->buckets[hval]->key) < 0) {
    hptr->next = htab->buckets[hval];
    htab->buckets[hval] = hptr;
    return hptr;
  }

  /* Insert in sorted order. There's always at least one item in 
     the chain already at this point. */
  old = htab->buckets[hval];
  for (curr = old->next; curr; old = curr, curr = curr->next) {
    /* Comparison will never be 0 because hash_add checks to see
       if the entry is already present. */
    if (strcmp(key, curr->key) < 0) {	/* Insert before curr */
      old->next = hptr;
      hptr->next = curr;
      return hptr;
    }
  }

  /* If we get here, we reached the end of the chain */
  old->next = hptr;
  hptr->next = NULL;

  return hptr;
}

/** Add an entry to a hash table.
 * \param htab pointer to hash table.
 * \param key key string to store data under.
 * \param hashdata void pointer to data to be stored.
 * \param extra_size unused.
 * \retval -1 failure.
 * \retval 0 success.
 */
int
hash_add(HASHTAB *htab, const char *key, void *hashdata,
	 int extra_size __attribute__ ((__unused__)))
{
  HASHENT *hptr;

  if (hash_find(htab, key) != NULL) {
    return -1;
  }

  hptr = hash_new(htab, key);

  if (!hptr)
    return -1;

  hptr->data = hashdata;
  /*      hptr->extra_size = extra_size; */
  return 0;
}

/** Delete an entry in a hash table.
 * \param htab pointer to hash table.
 * \param entry pointer to hash entry to delete (and free).
 */
void
hash_delete(HASHTAB *htab, HASHENT *entry)
{
  int hval;
  HASHENT *hptr, *last;

  if (!entry)
    return;

  hval = hash_val(entry->key, htab->mask);
  last = NULL;
  for (hptr = htab->buckets[hval]; hptr; last = hptr, hptr = hptr->next) {
    if (entry == hptr) {
      if (last == NULL)
	htab->buckets[hval] = hptr->next;
      else
	last->next = hptr->next;
      mush_free(hptr, "hash_entry");
      htab->entries--;
      return;
    }
  }

  if (htab->entries < (htab->hashsize * HTAB_DOWNSCALE))
    hash_resize(htab, htab->hashsize >> 1);
}

/** Flush a hash table, freeing all entries.
 * \param htab pointer to a hash table.
 * \param size size of hash table.
 */
void
hash_flush(HASHTAB *htab, int size)
{
  HASHENT *hent, *thent;
  int i;

  if (htab->buckets) {
    for (i = 0; i < htab->hashsize; i++) {
      hent = htab->buckets[i];
      while (hent != NULL) {
	thent = hent;
	hent = hent->next;
	mush_free(thent, "hash_entry");
      }
      htab->buckets[i] = NULL;
    }
  }
  if (size == 0) {
    mush_free(htab->buckets, "hash_buckets");
    htab->buckets = NULL;
  } else if (size != htab->hashsize) {
    if (htab->buckets)
      mush_free(htab->buckets, "hash_buckets");
    hashinit(htab, size, htab->entry_size);
  } else {
    htab->entries = 0;
  }
}

/** Return the first entry of a hash table.
 * This function is used with hash_nextentry() to iterate through a 
 * hash table.
 * \param htab pointer to hash table.
 * \return first hash table entry.
 */
void *
hash_firstentry(HASHTAB *htab)
{
  int hval;

  for (hval = 0; hval < htab->hashsize; hval++)
    if (htab->buckets[hval]) {
      htab->last_hval = hval;
      htab->last_entry = htab->buckets[hval];
      return htab->buckets[hval]->data;
    }
  return NULL;
}

/** Return the first key of a hash table.
 * This function is used with hash_nextentry_key() to iterate through a 
 * hash table.
 * \param htab pointer to hash table.
 * \return first hash table key.
 */
char *
hash_firstentry_key(HASHTAB *htab)
{
  int hval;

  for (hval = 0; hval < htab->hashsize; hval++)
    if (htab->buckets[hval]) {
      htab->last_hval = hval;
      htab->last_entry = htab->buckets[hval];
      return htab->buckets[hval]->key;
    }
  return NULL;
}

/** Return the next entry of a hash table.
 * This function is used with hash_firstentry() to iterate through a 
 * hash table. hash_firstentry() must be called before calling
 * this function.
 * \param htab pointer to hash table.
 * \return next hash table entry.
 */
void *
hash_nextentry(HASHTAB *htab)
{
  int hval;
  HASHENT *hptr;

  hval = htab->last_hval;
  hptr = htab->last_entry;
  if (hptr->next) {
    htab->last_entry = hptr->next;
    return hptr->next->data;
  }
  hval++;
  while (hval < htab->hashsize) {
    if (htab->buckets[hval]) {
      htab->last_hval = hval;
      htab->last_entry = htab->buckets[hval];
      return htab->buckets[hval]->data;
    }
    hval++;
  }
  return NULL;
}

/** Return the next key of a hash table.
 * This function is used with hash_firstentry{,_key}() to iterate through a 
 * hash table. hash_firstentry{,_key}() must be called before calling
 * this function.
 * \param htab pointer to hash table.
 * \return next hash table key.
 */
char *
hash_nextentry_key(HASHTAB *htab)
{
  int hval;
  HASHENT *hptr;

  hval = htab->last_hval;
  hptr = htab->last_entry;
  if (hptr->next) {
    htab->last_entry = hptr->next;
    return hptr->next->key;
  }
  hval++;
  while (hval < htab->hashsize) {
    if (htab->buckets[hval]) {
      htab->last_hval = hval;
      htab->last_entry = htab->buckets[hval];
      return htab->buckets[hval]->key;
    }
    hval++;
  }
  return NULL;
}

/** Display a header for a stats listing.
 * \param player player to notify with header.
 */
void
hash_stats_header(dbref player)
{
  notify_format(player,
		"Table      Buckets Entries LChain  ECh  1Ch  2Ch  3Ch 4+Ch  AvgCh ~Memory");
}

/** Display stats on a hashtable.
 * \param player player to notify with stats.
 * \param htab pointer to the hash table.
 * \param hname name of the hash table.
 */
void
hash_stats(dbref player, HASHTAB *htab, const char *hname)
{
  int longest = 0, n;
  int lengths[5];
  double chainlens = 0.0;
  double totchains = 0.0;
  unsigned int bytes = 0;

  if (!htab || !hname)
    return;

  for (n = 0; n < 5; n++)
    lengths[n] = 0;
  bytes += sizeof(HASHTAB);
  bytes += htab->entry_size * htab->entries;
  if (htab->buckets) {
    bytes += HASHENT_SIZE * htab->hashsize;
    for (n = 0; n < htab->hashsize; n++) {
      int chain = 0;
      HASHENT *b;
      if (htab->buckets[n]) {
	for (b = htab->buckets[n]; b; b = b->next) {
	  chain++;
	  bytes += strlen(b->key) + 1 /* + b->extra_size */ ;
	}
	if (chain > longest)
	  longest = chain;
      }
      lengths[(chain > 4) ? 4 : chain]++;
      chainlens += chain;
    }
  }
  for (n = 1; n < 5; n++)
    totchains += lengths[n];

  notify_format(player,
		"%-10s %7d %7d %6d %4d %4d %4d %4d %4d %6.3f %7u", hname,
		htab->hashsize, htab->entries, longest, lengths[0], lengths[1],
		lengths[2], lengths[3], lengths[4],
		totchains > 0 ? chainlens / totchains : 0.0, bytes);
}
