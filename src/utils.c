/**
 * \file utils.c
 *
 * \brief Utility functions for PennMUSH.
 *
 *
 */

#include "copyrite.h"
#include "config.h"

#include <stdio.h>
#include <limits.h>
#ifdef sgi
#include <math.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#ifdef I_SYS_STAT
#include <sys/stat.h>
#endif
#include <fcntl.h>
#ifdef I_UNISTD
#include <unistd.h>
#endif
#ifdef WIN32
#include <wtypes.h>
#include <winbase.h>		/* For GetCurrentProcessId() */
#endif
#include "conf.h"

#include "match.h"
#include "externs.h"
#include "mushdb.h"
#include "mymalloc.h"
#include "log.h"
#include "flags.h"
#include "dbdefs.h"
#include "attrib.h"
#include "parse.h"
#include "lock.h"
#include "confmagic.h"

dbref find_entrance(dbref door);
void initialize_mt(void);
static unsigned long genrand_int32(void);
static void init_genrand(unsigned long);
static void init_by_array(unsigned long *, int);
extern int local_can_interact_first(dbref from, dbref to, int type);
extern int local_can_interact_last(dbref from, dbref to, int type);

/** A malloc wrapper that tracks type of allocation.
 * This should be used in preference to malloc() when possible,
 * to enable memory leak tracing with MEM_CHECK.
 * \param size bytes to allocate.
 * \param check string to label allocation with.
 * \return allocated block of memory or NULL.
 */
Malloc_t
mush_malloc(size_t size, const char *check)
{
  Malloc_t ptr;
  add_check(check);
  ptr = malloc(size);
  if (ptr == NULL)
    do_log(LT_ERR, 0, 0, "mush_malloc failed to malloc %ld bytes for %s",
	   size, check);
  return ptr;
}

/** A free wrapper that tracks type of allocation.
 * If mush_malloc() gets the memory, mush_free() should free it
 * to enable memory leak tracing with MEM_CHECK.
 * \param ptr pointer to block of member to free.
 * \param check string to label allocation with.
 */
void
mush_free(Malloc_t RESTRICT ptr, const char *RESTRICT check
	  __attribute__ ((__unused__)))
{
  del_check(check);
  free(ptr);
  return;
}


/** Parse object/attribute strings into components.
 * This function takes a string which is of the format obj/attr or attr,
 * and returns the dbref of the object, and a pointer to the attribute.
 * If no object is specified, then the dbref returned is the player's.
 * str is destructively modified. This function is probably underused.
 * \param player the default object.
 * \param str the string to parse.
 * \param thing pointer to dbref of object parsed out of string.
 * \param attrib pointer to pointer to attribute structure retrieved.
 */
void
parse_attrib(dbref player, char *str, dbref *thing, ATTR **attrib)
{
  char *name;

  /* find the object */

  if ((name = strchr(str, '/')) != NULL) {
    *name++ = '\0';
    *thing = noisy_match_result(player, str, NOTYPE, MAT_EVERYTHING);
  } else {
    name = str;
    *thing = player;
  }

  /* find the attribute */
  *attrib = (ATTR *) atr_get(*thing, upcasestr(name));
}

/** Parse an attribute or anonymous attribute into dbref and pointer.
 * This function takes a string which is of the format #lambda/code, 
 * <obj>/<attr> or <attr>,  and returns the dbref of the object, 
 * and a pointer to the attribute.
 * \param player the executor, for permissions checks.
 * \param str string to parse.
 * \param thing pointer to address to return dbref parsed, or NOTHING
 * if none could be parsed.
 * \param attrib pointer to address to return ATTR * of attrib parsed,
 * or NULL if none could be parsed.
 */
void
parse_anon_attrib(dbref player, char *str, dbref *thing, ATTR **attrib)
{

  if (string_prefix(str, "#lambda/")) {
    unsigned char *t;
    str += 8;
    if (!*str) {
      *attrib = NULL;
      *thing = NOTHING;
    } else {
      *attrib = mush_malloc(sizeof(ATTR), "anon_attr");
      AL_CREATOR(*attrib) = player;
      AL_NAME(*attrib) = strdup("#lambda");
      t = compress(str);
      (*attrib)->data = chunk_create(t, (u_int_16) u_strlen(t), 0);
      AL_RLock(*attrib) = AL_WLock(*attrib) = TRUE_BOOLEXP;
      AL_FLAGS(*attrib) = AF_ANON;
      AL_NEXT(*attrib) = NULL;
      *thing = player;
    }
    return;
  }
  parse_attrib(player, str, thing, attrib);
}

/** Free the memory allocated for an anonymous attribute.
 * \param attrib pointer to attribute.
 */
void
free_anon_attrib(ATTR *attrib)
{
  if (attrib && (AL_FLAGS(attrib) & AF_ANON)) {
    free((char *) AL_NAME(attrib));
    chunk_delete(attrib->data);
    mush_free(attrib, "anon_attr");
  }
}

/** Given an attribute [<object>/]<name> pair (which may include #lambda),
 * fetch its value, owner (thing), and pe_flags, and store in the struct
 * pointed to by ufun
 */
int 
fetch_ufun_attrib(char *attrname, dbref executor, ufun_attrib * ufun,
		  int accept_lambda)
{ 
  ATTR *attrib;
  dbref thing;
  int pe_flags = PE_UDEFAULT;
    
  if (!ufun)
    return 0;		/* We should never NOT receive a ufun. */
  ufun->errmess = (char *) "";
    
  /* find our object and attribute */
  if (accept_lambda) {
    parse_anon_attrib(executor, attrname, &thing, &attrib);
  } else {
    parse_attrib(executor, attrname, &thing, &attrib);
  }

  /* Is it valid? */
  if (!GoodObject(thing)) {
    ufun->errmess = (char *) "#-1 INVALID OBJECT";
    free_anon_attrib(attrib);
    return 0;
  } else if (!attrib) {
    ufun->contents[0] = '\0';
    ufun->thing = thing;
    ufun->pe_flags = pe_flags;
    free_anon_attrib(attrib);
    return 1;
  } else if (!Can_Read_Attr(executor, thing, attrib)) {
    ufun->errmess = e_atrperm;
    free_anon_attrib(attrib);
    return 0;
  }
    
  /* Can we evaluate it? */
  if (!CanEvalAttr(executor, thing, attrib)) {
    ufun->errmess = e_perm;
    free_anon_attrib(attrib);
    return 0;
  } 
    
  /* DEBUG attributes */
  if (AF_Debug(attrib))
    pe_flags |= PE_DEBUG;
    
  /* Populate the ufun object */
  strncpy(ufun->contents, atr_value(attrib), BUFFER_LEN);
  ufun->thing = thing;
  ufun->pe_flags = pe_flags;
  
  /* Cleanup */
  free_anon_attrib(attrib);
    
  /* We're good */
  return 1;
}

/** Given a ufun, executor, enactor, PE_Info, and arguments for %0-%9,
 * call the ufun with appropriate permissions on values given for
 * wenv_args. The value returned is stored in the buffer pointed to
 * by retval, if given.
 * \param ufun The ufun_attrib that was initialized by fetch_ufun_attrib
 * \param wenv_args An array of string values for global_eval_context.wenv
 * \param wenv_argc The number of wenv args to use.
 * \param ret If desired, a pointer to a buffer in which the results
 * of the process_expression are stored in.
 * \param executor The executor.
 * \param enactor The enactor.
 * \param pe_info The pe_info passed to the FUNCTION
 * \retval 0 success
 * \retval 1 process_expression failed. (CPU time limit)
 */
int
call_ufun(ufun_attrib * ufun, char **wenv_args, int wenv_argc, char *ret,
	  dbref executor, dbref enactor, PE_Info * pe_info)   
{
  char rbuff[BUFFER_LEN];
  char *rp;
  char *old_wenv[10];
  int old_args = 0;
  int i;
  int pe_ret;
  char const *ap;

  int old_re_subpatterns;
  int *old_re_offsets;
  char *old_re_from;

  old_re_subpatterns = global_eval_context.re_subpatterns;
  old_re_offsets = global_eval_context.re_offsets;
  old_re_from = global_eval_context.re_from;

  /* Make sure we have a ufun first */
  if (!ufun)
    return 1;

  /* If the user doesn't care about the return of the expression,
   * then use our own rbuff.
   */
  if (!ret)
    ret = rbuff;
  rp = ret;

  for (i = 0; i < wenv_argc; i++) {
    old_wenv[i] = global_eval_context.wenv[i];
    global_eval_context.wenv[i] = wenv_args[i];
  }
  for (; i < 10; i++) {
    old_wenv[i] = global_eval_context.wenv[i];
    global_eval_context.wenv[i] = NULL;
  }

  /* Set all the regexp patterns to NULL so they are not
   * propogated */
  global_eval_context.re_subpatterns = -1;
  global_eval_context.re_offsets = NULL;
  global_eval_context.re_from = NULL;

  /* And now, make the call! =) */
  if (pe_info) {
    old_args = pe_info->arg_count;
    pe_info->arg_count = wenv_argc;
  }

  ap = ufun->contents;
  pe_ret = process_expression(ret, &rp, &ap, ufun->thing, executor,
			      enactor, ufun->pe_flags, PT_DEFAULT, pe_info);
  *rp = '\0';

  /* Restore the old wenv */
  for (i = 0; i < 10; i++) {
    global_eval_context.wenv[i] = old_wenv[i];
  }
  if (pe_info) {
    pe_info->arg_count = old_args;
  }

  /* Restore regexp patterns */
  global_eval_context.re_offsets = old_re_offsets;
  global_eval_context.re_subpatterns = old_re_subpatterns;
  global_eval_context.re_from = old_re_from;

  return pe_ret;
}

/** Given an exit, find the room that is its source through brute force.
 * This is used in pathological cases where the exit's own source
 * element is invalid.
 * \param door dbref of exit to find source of.
 * \return dbref of exit's source room, or NOTHING.
 */
dbref
find_entrance(dbref door)
{
  dbref room;
  dbref thing;
  for (room = 0; room < db_top; room++)
    if (IsRoom(room)) {
      thing = Exits(room);
      while (thing != NOTHING) {
	if (thing == door)
	  return room;
	thing = Next(thing);
      }
    }
  return NOTHING;
}

/** Remove the first occurence of what in chain headed by first.
 * This works for contents and exit chains.
 * \param first dbref of first object in chain.
 * \param what dbref of object to remove from chain.
 * \return new head of chain.
 */
dbref
remove_first(dbref first, dbref what)
{
  dbref prev;
  /* special case if it's the first one */
  if (first == what) {
    return Next(first);
  } else {
    /* have to find it */
    DOLIST(prev, first) {
      if (Next(prev) == what) {
	Next(prev) = Next(what);
	return first;
      }
    }
    return first;
  }
}

/** Is an object on a chain?
 * \param thing object to look for.
 * \param list head of chain to search.
 * \retval 1 found thing on list.
 * \retval 0 did not find thing on list.
 */
int
member(dbref thing, dbref list)
{
  DOLIST(list, list) {
    if (list == thing)
      return 1;
  }

  return 0;
}


/** Is an object inside another, at any level of depth?
 * That is, we check if disallow is inside of from, i.e., if 
 * loc(disallow) = from, or loc(loc(disallow)) = from, etc., with a 
 * depth limit of 50.
 * Despite the name of this function, it's not recursive any more.
 * \param disallow interior object to check.
 * \param from check if disallow is inside of this object.
 * \param count depths of nesting checked so far.
 * \retval 1 disallow is inside of from.
 * \retval 0 disallow is not inside of from.
 */
int
recursive_member(dbref disallow, dbref from, int count)
{
  do {
    /* The end of the location chain. This is a room. */
    if (!GoodObject(disallow) || IsRoom(disallow))
      return 0;

    if (from == disallow)
      return 1;

    disallow = Location(disallow);
    count++;
  } while (count <= 50);

  return 1;
}

/** Is an object or its location unfindable?
 * \param thing object to check.
 * \retval 1 object or location is unfindable.
 * \retval 0 neither object nor location is unfindable.
 */
int
unfindable(dbref thing)
{
  int count = 0;
  do {
    if (!GoodObject(thing))
      return 0;
    if (Unfind(thing))
      return 1;
    if (IsRoom(thing))
      return 0;
    thing = Location(thing);
    count++;
  } while (count <= 50);
  return 0;
}


/** Reverse the order of a dbref chain.
 * \param list dbref at the head of the chain.
 * \return dbref at the head of the reversed chain.
 */
dbref
reverse(dbref list)
{
  dbref newlist;
  dbref rest;
  newlist = NOTHING;
  while (list != NOTHING) {
    rest = Next(list);
    PUSH(list, newlist);
    list = rest;
  }
  return newlist;
}


#define N 624 /**< PRNG constant */

/* We use the Mersenne Twister PRNG. It's quite good as PRNGS go,
 * much better than the typical ones provided in system libc's.
 *
 * The following two functions are based on the reference implementation,
 * with changes in the seeding function to use /dev/urandom as a seed
 * if possible.
 *
 * The Mersenne Twister homepage is:
 *  http://www.math.keio.ac.jp/~matumoto/emt.html
 *
 * You can get the reference code there.
 */


/** Wrapper to choose a seed and initialize the Mersenne Twister PRNG. */
void
initialize_mt(void)
{
#ifdef HAS_DEV_URANDOM
  int fd;
  unsigned long buf[N];

  fd = open("/dev/urandom", O_RDONLY);
  if (fd >= 0) {
    int r = read(fd, buf, sizeof buf);
    close(fd);
    if (r <= 0) {
      do_rawlog(LT_ERR,
		"Couldn't read from /dev/urandom! Resorting to normal seeding method.");
    } else {
      do_rawlog(LT_ERR, "Seeded RNG from /dev/urandom");
      init_by_array(buf, r / sizeof(unsigned long));
      return;
    }
  } else
    do_rawlog(LT_ERR,
	      "Couldn't open /dev/urandom to seed random number generator. Resorting to normal seeding method.");

#endif
  /* Default seeder. Pick a seed that's fairly random */
#ifdef WIN32
  init_genrand(GetCurrentProcessId() | (time(NULL) << 16));
#else
  init_genrand(getpid() | (time(NULL) << 16));
#endif
}


/* A C-program for MT19937, with initialization improved 2002/1/26.*/
/* Coded by Takuji Nishimura and Makoto Matsumoto.                 */

/* Before using, initialize the state by using init_genrand(seed)  */
/* or init_by_array(init_key, key_length).                         */

/* This library is free software.                                  */
/* This library is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of  */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.            */

/* Copyright (C) 1997, 2002 Makoto Matsumoto and Takuji Nishimura. */
/* Any feedback is very welcome.                                   */
/* http://www.math.keio.ac.jp/matumoto/emt.html                    */
/* email: matumoto@math.keio.ac.jp                                 */

/* Period parameters */
#define M 397  /**< PRNG constant */
#define MATRIX_A 0x9908b0dfUL	/**< PRNG constant vector a */
#define UPPER_MASK 0x80000000UL	/**< PRNG most significant w-r bits */
#define LOWER_MASK 0x7fffffffUL	/**< PRNG least significant r bits */

static unsigned long mt[N];	/* the array for the state vector  */
static int mti = N + 1;		/* mti==N+1 means mt[N] is not initialized */

/** initializes mt[N] with a seed.
 * \param a seed value.
 */
static void
init_genrand(unsigned long s)
{
  mt[0] = s & 0xffffffffUL;
  for (mti = 1; mti < N; mti++) {
    mt[mti] = (1812433253UL * (mt[mti - 1] ^ (mt[mti - 1] >> 30)) + mti);
    /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
    /* In the previous versions, MSBs of the seed affect   */
    /* only MSBs of the array mt[].                        */
    /* 2002/01/09 modified by Makoto Matsumoto             */
    mt[mti] &= 0xffffffffUL;
    /* for >32 bit machines */
  }
}

/** initialize by an array with array-length
 * \param init_key the array for initializing keys 
 * \param key_length the array's length 
 */
static void
init_by_array(unsigned long init_key[], int key_length)
{
  int i, j, k;
  init_genrand(19650218UL);
  i = 1;
  j = 0;
  k = (N > key_length ? N : key_length);
  for (; k; k--) {
    mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1664525UL))
      + init_key[j] + j;	/* non linear */
    mt[i] &= 0xffffffffUL;	/* for WORDSIZE > 32 machines */
    i++;
    j++;
    if (i >= N) {
      mt[0] = mt[N - 1];
      i = 1;
    }
    if (j >= key_length)
      j = 0;
  }
  for (k = N - 1; k; k--) {
    mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1566083941UL))
      - i;			/* non linear */
    mt[i] &= 0xffffffffUL;	/* for WORDSIZE > 32 machines */
    i++;
    if (i >= N) {
      mt[0] = mt[N - 1];
      i = 1;
    }
  }

  mt[0] = 0x80000000UL;		/* MSB is 1; assuring non-zero initial array */
}

/* generates a random number on [0,0xffffffff]-interval */
static unsigned long
genrand_int32(void)
{
  unsigned long y;
  static unsigned long mag01[2] = { 0x0UL, MATRIX_A };
  /* mag01[x] = x * MATRIX_A  for x=0,1 */

  if (mti >= N) {		/* generate N words at one time */
    int kk;

    if (mti == N + 1)		/* if init_genrand() has not been called, */
      init_genrand(5489UL);	/* a default initial seed is used */

    for (kk = 0; kk < N - M; kk++) {
      y = (mt[kk] & UPPER_MASK) | (mt[kk + 1] & LOWER_MASK);
      mt[kk] = mt[kk + M] ^ (y >> 1) ^ mag01[y & 0x1UL];
    }
    for (; kk < N - 1; kk++) {
      y = (mt[kk] & UPPER_MASK) | (mt[kk + 1] & LOWER_MASK);
      mt[kk] = mt[kk + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
    }
    y = (mt[N - 1] & UPPER_MASK) | (mt[0] & LOWER_MASK);
    mt[N - 1] = mt[M - 1] ^ (y >> 1) ^ mag01[y & 0x1UL];

    mti = 0;
  }

  y = mt[mti++];

  /* Tempering */
  y ^= (y >> 11);
  y ^= (y << 7) & 0x9d2c5680UL;
  y ^= (y << 15) & 0xefc60000UL;
  y ^= (y >> 18);

  return y;
}


/** Get a uniform random long between low and high values, inclusive.
 * Based on MUX's RandomINT32()
 * \param low lower bound for random number.
 * \param high upper bound for random number.
 * \return random number between low and high, or 0 or -1 for error.
 */
long
get_random_long(long low, long high)
{
  unsigned long x, n, n_limit;

  /* Validate parameters */
  if (high < low) {
    return 0;
  } else if (high == low) {
    return low;
  }

  x = high - low;
  if (LONG_MAX < x) {
    return -1;
  }
  x++;

  /* We can now look for an random number on the interval [0,x-1].
     //

     // In order to be perfectly conservative about not introducing any
     // further sources of statistical bias, we're going to call getrand()
     // until we get a number less than the greatest representable
     // multiple of x. We'll then return n mod x.
     //
     // N.B. This loop happens in randomized constant time, and pretty
     // damn fast randomized constant time too, since
     //
     //      P(UINT32_MAX_VALUE - n < UINT32_MAX_VALUE % x) < 0.5, for any x.
     //
     // So even for the least desireable x, the average number of times
     // we will call getrand() is less than 2.
   */

  n_limit = ULONG_MAX - (ULONG_MAX % x);

  do {
    n = genrand_int32();
  } while (n >= n_limit);

  return low + (n % x);
}

/** Return an object's alias. We expect a valid object.
 * \param it dbref of object.
 * \return object's complete alias.
 */
char *
fullalias(dbref it)
{
  static char n[BUFFER_LEN];  /* STATIC */
  ATTR *a = atr_get_noparent(it, "ALIAS");

  if (!a)
    return '\0';

  strncpy(n, atr_value(a), BUFFER_LEN - 1);
  n[BUFFER_LEN - 1] = '\0';

  return n;
}

/** Return only the first component of an object's alias. We expect
 * a valid object.
 * \param it dbref of object.
 * \return object's short alias.
 */
char *
shortalias(dbref it)
{
  static char n[BUFFER_LEN];  /* STATIC */
  char *s;

  s = fullalias(it);
  if (!(s && *s))
    return '\0';

  strncpy(n, s, BUFFER_LEN - 1);
  n[BUFFER_LEN - 1] = '\0';
  if ((s = strchr(n, ';')))
    *s = '\0';

  return n;
}

/** Return an object's name, but for exits, return just the first
 * component. We expect a valid object.
 * \param it dbref of object.
 * \return object's short name.
 */
char *
shortname(dbref it)
{
  static char n[BUFFER_LEN];	/* STATIC */
  char *s;

  strncpy(n, Name(it), BUFFER_LEN - 1);
  n[BUFFER_LEN - 1] = '\0';
  if (IsExit(it)) {
    if ((s = strchr(n, ';')))
      *s = '\0';
  }
  return n;
}

/** Return the absolute room (outermost container) of an object.
 * Return  NOTHING if it's in an invalid object or in an invalid
 * location or AMBIGUOUS if there are too many containers.
 * \param it dbref of object.
 * \return absolute room of object, NOTHING, or AMBIGUOUS.
 */
dbref
absolute_room(dbref it)
{
  int rec = 0;
  dbref room;
  if (!GoodObject(it))
    return NOTHING;
  room = Location(it);
  if (!GoodObject(room))
    return NOTHING;
  while (!IsRoom(room)) {
    room = Location(room);
    rec++;
    if (rec > 20)
      return AMBIGUOUS;
  }
  return room;
}


/** Can one object interact with/perceive another in a given way?
 * This funtion checks to see if 'to' can perceive something from
 * 'from'. The types of interactions currently supported include:
 * INTERACT_SEE (will light rays from 'from' reach 'to'?), INTERACT_HEAR
 * (will sound from 'from' reach 'to'?), INTERACT_MATCH (can 'to'
 * match the name of 'from'?), and INTERACT_PRESENCE (will the arrival/
 * departure/connection/disconnection/growing ears/losing ears of 
 * 'from' be noticed by 'to'?).
 * \param from object of interaction.
 * \param to subject of interaction, attempting to interact with from.
 * \param type type of interaction.
 * \retval 1 to can interact with from in this way.
 * \retval 0 to can not interact with from in this way.
 */
int
can_interact(dbref from, dbref to, int type)
{
  int lci;

  /* This shouldn't even be checked for rooms and garbage, but we're
   * paranoid. Trying to stop interaction with yourself will not work 99%
   * of the time, so we don't allow it anyway. */
  if (IsGarbage(from) || IsGarbage(to))
    return 0;

  if ((from == to) || IsRoom(from) || IsRoom(to))
    return 1;

  /* This function can override standard checks! */
  lci = local_can_interact_first(from, to, type);
  if (lci != NOTHING)
    return lci;

  /* Standard checks */

  /* If it's an audible message, it must pass your Interact_Lock
   * (or be from a privileged speaker)
   */
  if ((type == INTERACT_HEAR) && !Pass_Interact_Lock(from, to))
    return 0;

  /* You can interact with the object you are in or any objects
   * you're holding.
   * You can interact with objects you control, but not
   * specifically the other way around
   */
  if ((from == Location(to)) || (to == Location(from)) || controls(to, from))
    return 1;

  lci = local_can_interact_last(from, to, type);
  if (lci != NOTHING)
    return lci;

  return 1;
}

#ifdef KNOW_SYS
/* know system core code */
/* Design in Simple:  
 * In an IC zone, or in a 'fixed' status.  People will only see others in there @XY_KNOW attribute
 * If they are not in that attribute, their @RACE will show in place following a - & the occurance
 * of that race in the room.
 */

/* checks if p1 knows p2.
 * If no, returns 0
 * If yes, returns 1
 */
char check_know(dbref p1, dbref p2) {
	ATTR *a;
	dbref num;
	char *b, *s, *e;
	char di = 0;  /* variable set when in dbref isolating */

	/* Quick Check first */
	if(p1 == p2)
		return 1;
	/* load @XY_KNOW attribute into the register, & check for p2 in it */

	a = atr_get(p1, "XY_KNOW");
	if(!a) /* they know no one */
		return 0;
	b = s = e = safe_atr_value(a);
	if(!s)
		return 0;

	/* isolate each dbref in the attribute & check if p2 matches anywhere */
	while(*s && *e) 
		if(di) {
			if(*e && e[1] != '\0' && !isspace(*e)) {
				e++;
				continue;
			}
			if(isspace(*e))
			  *e = '\0';
			num = parse_dbref(s);
			if(num == p2) { /* FOUND 'EM! */ 
				mush_free(b, "ATRFREE");
				return 1;
			}
			if(e[1] != '\0') {
				di = 0;
				s = e++;  
				s++;
			} else break;
		} else if(*s == '#') 
			di = 1;
		else if(isspace(*s))
			s++, e++;  
	mush_free(b,"ATRFREE");
	return 0;
}


#define ROCC_LIMIT 25
/* search which occ 'player' is in the room for their race & attach it to @RACE */
/* *sigh* This function does alot of shit for something that is called alot.
 * Best not to use the know system on a slow machine or a machine with alot
 * of overhead
 */
const char *know_name_qk(dbref player) {
	static char final_buffer[ROCC_LIMIT];
	ATTR *a, *ao;
	char *race, *ro, *p;
	dbref spot;
	/* to navigate backwards through content list */
	struct object_ptr {
		dbref cur;
		struct object_ptr *back;
	} *optr_add,*optr_nav;
	int occ = 0;

        /* ONLY PLAYERS! OR WE CRASH! */

        if(Typeof(player) != TYPE_PLAYER)
	   return shortname(player);

	/* initialize variables */
	memset(final_buffer, '\0', ROCC_LIMIT);
	p = final_buffer;
	spot = Contents(Location(player));


	/* Isolate player Race. */
	a = atr_get(player, "RACE");
	if(!a) 
		race = strdup(DEF_RACE_NAME);
	 else race = safe_atr_value(a);

	safe_str(race, final_buffer, &p);

	/* Now we have the race, loop through location contents to see which 'occurance' player
	 * is of that race
	 */
	/* first lets make our backwrds list */
	optr_nav = optr_add = NULL;
	DOLIST(spot,spot) {
		if(!IsPlayer(spot))
			continue;
		optr_nav = optr_add;
		optr_add = mush_malloc(sizeof(struct object_ptr *) , "OPTR_ADD");
		if(!optr_add)
		  mush_panic("Can't Allocate OPTR_ADD");
		if(optr_nav == NULL) {
			optr_add->back = NULL;
		} else optr_add->back = optr_nav;
		optr_add->cur = spot;
	}

	/* now loop through our backwards list */
	for(optr_nav = optr_add; optr_nav ; optr_nav = optr_nav->back) {
		/* grab race */
		ao = atr_get(optr_nav->cur, "RACE");
		if(!ao)
			ro = strdup(DEF_RACE_NAME); /* It's your average no race dude */
		else ro = safe_atr_value(ao);
		if(!strcasecmp(race, ro)) { 
			occ++;
			if(optr_nav->cur == player) break;
		}

		free(ro); 
	}
	/* attach occ to final_buffer */
	safe_chr('-', final_buffer, &p);
	safe_number(occ, final_buffer, &p);

         
	/* free up & return */
	for(optr_nav = optr_add;;optr_nav = optr_add) {
		if(!optr_nav)
		  break;
		optr_add = optr_add->back;
		mush_free(optr_nav, "OPTR_ADD");
	}

	free(race);
	free(ro);
	*p = '\0';
	return final_buffer;
	
}
#endif
