/**
 * \file mymalloc.c
 *
 * \brief Malloc wrapper file.
 *
 * A wrapper for the various malloc package options. See options.h
 * for descriptions of each.
 * Each package's source code must include config.h, mymalloc.h,
 * and confmagic.h.
 */
#include "config.h"
#include "options.h"
#include "confmagic.h"

/* An undefined or illegal MALLOC_PACKAGE is treated as system 
 * malloc, because we're such nice forgiving people
 */
#if (MALLOC_PACKAGE == 1)
#include "csrimalloc.c"
#elif (MALLOC_PACKAGE == 2)
#include "csrimalloc.c"
#elif (MALLOC_PACKAGE == 5)
#include "gmalloc.c"
#endif
