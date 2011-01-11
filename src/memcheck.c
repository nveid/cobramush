/**
 * \file memcheck.c
 *
 * \brief A simple memory allocation tracker for PennMUSH.
 *
 * This code isn't usually compiled in, but it's handy to debug
 * memory leaks sometimes.
 *
 *
 */
#include "config.h"
#include "conf.h"
#include "copyrite.h"

#include <stdlib.h>
#include <string.h>


#include "externs.h"
#include "dbdefs.h"
#include "mymalloc.h"
#include "log.h"
#include "confmagic.h"

typedef struct mem_check MEM;

/** A linked list for storing memory allocation counts */
struct mem_check {
  int ref_count;                /**< Number of allocations of this type. */
  MEM *next;                    /**< Pointer to next in linked list. */
  char ref_name[BUFFER_LEN];    /**< Name of this allocation type. */
};

static MEM *my_check = NULL;

/*** WARNING! DO NOT USE strcasecoll IN THESE FUNCTIONS OR YOU'LL CREATE
 *** AN INFINITE LOOP. DANGER, WILL ROBINSON!
 ***/

/** Add an allocation check.
 * \param ref type of allocation.
 */
void
add_check(const char *ref)
{
  MEM *loop, *newcheck, *prev = NULL;
  size_t reflen;
  int cmp;

  if (!options.mem_check)
    return;

  for (loop = my_check; loop; loop = loop->next) {
    cmp = strcasecmp(ref, loop->ref_name);
    if (cmp == 0) {
      loop->ref_count++;
      return;
    } else if (cmp < 0)
      break;
    prev = loop;
  }
  reflen = strlen(ref) + 1;
  newcheck = (MEM *) malloc(sizeof(MEM) - BUFFER_LEN + reflen);
  memcpy(newcheck->ref_name, ref, reflen);
  newcheck->ref_count = 1;
  newcheck->next = loop;
  if (prev)
    prev->next = newcheck;
  else
    my_check = newcheck;
  return;
}

/** Remove an allocation check.
 * \param ref type of allocation to remove.
 */
void
del_check(const char *ref)
{
  MEM *loop, *prev = NULL;
  int cmp;

  if (!options.mem_check)
    return;

  for (loop = my_check; loop; loop = loop->next) {
    cmp = strcasecmp(ref, loop->ref_name);
    if (cmp == 0) {
      loop->ref_count--;
      if (!loop->ref_count) {
        if (!prev)
          my_check = loop->next;
        else
          prev->next = loop->next;
        free(loop);
      }
      return;
    } else if (cmp < 0)
      break;
    prev = loop;
  }
  do_rawlog(LT_CHECK,
            T("ERROR: Tried deleting a check that was never added! :%s\n"),
            ref);
}

/** List allocations.
 * \param player the enactor.
 */
void
list_mem_check(dbref player)
{
  MEM *loop;

  if (!options.mem_check)
    return;
  for (loop = my_check; loop; loop = loop->next) {
    notify_format(player, "%s : %d", loop->ref_name, loop->ref_count);
  }
}

/** Log all allocations.
 */
void
log_mem_check(void)
{
  MEM *loop;

  if (!options.mem_check)
    return;
  do_rawlog(LT_CHECK, "MEMCHECK dump starts");
  for (loop = my_check; loop; loop = loop->next) {
    do_rawlog(LT_CHECK, "%s : %d", loop->ref_name, loop->ref_count);
  }
  do_rawlog(LT_CHECK, "MEMCHECK dump ends");
}
