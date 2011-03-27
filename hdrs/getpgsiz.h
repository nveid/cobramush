#ifndef __GETPGSIZ_H
#define __GETPGSIZ_H

#ifndef HAS_GETPAGESIZE
#ifdef PAGESIZE_VALUE
#define getpagesize() PAGESIZE_VALUE
#else
#ifndef WIN32
#ifdef I_SYS_PARAM
#include <sys/param.h>
#endif
#endif
#ifdef I_SYS_PAGE
#include <sys/page.h>
#endif

#ifdef EXEC_PAGESIZE
#define getpagesize() EXEC_PAGESIZE
#else
#ifdef NBPG
#define getpagesize() NBPG * CLSIZE
#ifndef CLSIZE
#define CLSIZE 1
#endif                          /* no CLSIZE */
#else                           /* no NBPG */
#ifdef NBPC
#define getpagesize() NBPC
#else                           /* no NBPC either? Bummer */
#ifdef PAGESIZE
#define getpagesize() PAGESIZE
#else                           /* Sigh. Time for a total guess. */
#define getpagesize() 1024
#endif                          /* no PAGESIZE */
#endif                          /* no NBPC */
#endif                          /* no NBPG */
#endif                          /* no EXEC_PAGESIZE */
#endif                          /* no PAGESIZE_VALUE */
#endif                          /* not HAS_GETPAGESIZE */

#endif                          /* __GETPGSIZ_H */
