/*
 * This file was produced by running metaconfig and is intended to be included
 * after config.h and after all the other needed includes have been dealt with.
 *
 * This file may be empty, and should not be edited. Rerun metaconfig instead.
 * If you wish to get rid of this magic, remove this file and rerun metaconfig
 * without the -M option.
 *
 *  $Id: confmagic.h,v 1.1.1.1 2004-06-06 20:32:51 ari Exp $
 */

#ifndef _confmagic_h_
#define _confmagic_h_

#ifndef HAS_BCOPY
#ifndef bcopy
#define bcopy(s,d,l) memcpy((d),(s),(l))
#endif
#endif

#ifndef HAS_BZERO
#ifndef bzero
#define bzero(s,l) memset((s),0,(l))
#endif
#endif

/* If your system doesn't have the crypt(3) DES encryption code,
 * (which isn't exportable from the U.S.), then don't encrypt
 */
#ifndef HAS_CRYPT
#define crypt(s,t) (s)
#endif

#ifdef HAS_HUGE_VAL
#define HUGE_DOUBLE	HUGE_VAL
#else
#ifdef HAS_HUGE
#define HUGE_DOUBLE	HUGE
#else
#ifdef HAS_MAXDOUBLE
#define HUGE_DOUBLE	MAXDOUBLE
#else
#define HUGE_DOUBLE	2000000000
#endif
#endif
#endif
#ifdef HAS_INT_MAX
#define HUGE_INT	INT_MAX
#else
#ifdef HAS_MAXINT
#define HUGE_INT	MAXINT
#else
#define HUGE_INT	2000000000
#endif
#endif

#ifndef HAS_MEMCPY
#ifndef memcpy
#define memcpy(d,s,l) bcopy((s),(d),(l))
#endif
#endif

#ifndef HAS_MEMMOVE
#ifndef memmove
#define memmove(d,s,l) bcopy((s),(d),(l))
#endif
#endif

#ifndef HAS_RANDOM
#ifndef random
#ifdef HAS_LRAND48
#define random lrand48
#define srandom srand48
#else
#ifdef HAS_RAND
#define random rand
#define srandom srand
#endif
#endif
#endif
#endif

/*
#ifndef HAS_SIGCHLD
#define SIGCHLD	SIGCLD
#endif

#ifndef HAS_SIGCLD
#define SIGCLD	SIGCHLD
#endif
*/

#ifndef HAS_INDEX
#ifndef index
#define index strchr
#endif
#endif

#ifndef HAS_STRCHR
#ifndef strchr
#define strchr index
#endif
#endif

#ifndef HAS_INDEX
#ifndef rindex
#define rindex strrchr
#endif
#endif

#ifndef HAS_STRCHR
#ifndef strrchr
#define strrchr rindex
#endif
#endif

#ifndef HAS_STRCOLL
#undef strcoll
#define strcoll strcmp
#endif

#if !defined(WIN32) && !defined(HAS_STRXFRM)
#define strncoll strncmp
#define strncasecoll strncasecmp
#define strcasecoll strcasecmp
#endif

#endif
