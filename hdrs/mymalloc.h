/* A wrapper header that sets up the proper defines based on
 * options.h's MALLOC_PACKAGE
 */

#ifndef _MYMALLOC_H
#define _MYMALLOC_H

#include "options.h"

#if (MALLOC_PACKAGE == 1)
#define CSRI
#elif (MALLOC_PACKAGE == 2)
#include <stdlib.h>
#define CSRI
#define CSRI_TRACE
#define CSRI_PROFILESIZES
#define CSRI_DEBUG
#include "csrimalloc.h"
#endif

#endif                          /* _MYMALLOC_H */
