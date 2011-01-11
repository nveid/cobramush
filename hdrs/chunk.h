/* This must be first, otherwise dbref will be undefined */
#include "mushtype.h"
#ifndef _CHUNK_H_
#define _CHUNK_H_

#undef LOG_CHUNK_STATS

typedef unsigned short u_int_16;
typedef unsigned int u_int_32;

typedef u_int_32 chunk_reference_t;
#define NULL_CHUNK_REFERENCE 0

chunk_reference_t chunk_create(unsigned char const *data, u_int_16 len,
                               unsigned char derefs);
void chunk_delete(chunk_reference_t reference);
u_int_16 chunk_fetch(chunk_reference_t reference,
                     unsigned char *buffer, u_int_16 buffer_len);
u_int_16 chunk_len(chunk_reference_t reference);
unsigned char chunk_derefs(chunk_reference_t reference);
void chunk_migration(int count, chunk_reference_t ** references);
int chunk_num_swapped(void);
void chunk_init(void);
enum chunk_stats_type { CSTATS_SUMMARY, CSTATS_REGIONG, CSTATS_PAGINGG,
  CSTATS_FREESPACEG, CSTATS_REGION, CSTATS_PAGING
};
void chunk_stats(dbref player, enum chunk_stats_type which);
void chunk_new_period(void);

int chunk_fork_file(void);
void chunk_fork_parent(void);
void chunk_fork_child(void);
void chunk_fork_done(void);

#endif                          /* _CHUNK_H_ */
