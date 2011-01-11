/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/* Modified by Alan Schwartz for PennMUSH.
 * Should be included after config.h
 */

#ifndef __CSRIMALLOC_H
#define __CSRIMALLOC_H

#include "config.h"
#define univptr_t               Malloc_t
#define memsize_t               size_t
#include "confmagic.h"

/*
 *  defined so users of new features of this malloc can #ifdef
 *  invocations of those features.
 */
#define CSRIMALLOC

#ifdef CSRI_TRACE
/* Tracing malloc definitions - helps find leaks */

extern univptr_t trace__malloc
_((size_t nbytes, const char *fname, int linenum));
extern univptr_t trace__calloc
_((size_t nelem, size_t elsize, const char *fname, int linenum));
extern univptr_t trace__realloc
_((univptr_t cp, size_t nbytes, const char *fname, int linenum));
extern univptr_t trace__valloc _((size_t size, const char *fname, int linenum));
extern univptr_t trace__memalign
_((size_t alignment, size_t size, const char *fname, int linenum));
extern univptr_t trace__emalloc
_((size_t nbytes, const char *fname, int linenum));
extern univptr_t trace__ecalloc
_((size_t nelem, size_t sz, const char *fname, int linenum));
extern univptr_t trace__erealloc
_((univptr_t ptr, size_t nbytes, const char *fname, int linenum));
extern char *trace__strdup _((const char *s, const char *fname, int linenum));
extern char *trace__strsave _((const char *s, const char *fname, int linenum));
extern void trace__free _((univptr_t cp, const char *fname, int linenum));
extern void trace__cfree _((univptr_t cp, const char *fname, int linenum));

#define malloc(x)               trace__malloc((x), __FILE__, __LINE__)
#define calloc(x, n)            trace__calloc((x), (n), __FILE__, __LINE__)
#define realloc(p, x)           trace__realloc((p), (x), __FILE__, __LINE__)
#define memalign(x, n)          trace__memalign((x), (n), __FILE__, __LINE__)
#define valloc(x)               trace__valloc((x), __FILE__, __LINE__)
#define emalloc(x)              trace__emalloc((x), __FILE__, __LINE__)
#define ecalloc(x, n)           trace__ecalloc((x), (n), __FILE__, __LINE__)
#define erealloc(p, x)          trace__erealloc((p), (x), __FILE__, __LINE__)
#define strdup(p)               trace__strdup((p), __FILE__, __LINE__)
#define strsave(p)              trace__strsave((p), __FILE__, __LINE__)
/* cfree and free are identical */
#define cfree(p)                trace__free((p), __FILE__, __LINE__)
#define free(p)                 trace__free((p), __FILE__, __LINE__)

#else                           /* CSRI_TRACE */

extern univptr_t malloc _((size_t nbytes));
extern univptr_t calloc _((size_t nelem, size_t elsize));
extern univptr_t realloc _((univptr_t cp, size_t nbytes));
extern univptr_t valloc _((size_t size));
extern univptr_t memalign _((size_t alignment, size_t size));
extern univptr_t emalloc _((size_t nbytes));
extern univptr_t ecalloc _((size_t nelem, size_t sz));
extern univptr_t erealloc _((univptr_t ptr, size_t nbytes));
extern char *strdup _((const char *s));
extern char *strsave _((const char *s));
extern Free_t free _((univptr_t cp));
extern Free_t cfree _((univptr_t cp));

#endif                          /* CSRI_TRACE */

extern void mal_debug _((int level));
extern void mal_dumpleaktrace _((FILE * fp));
extern void mal_heapdump _((FILE * fp));
extern void mal_leaktrace _((int value));
extern void mal_sbrkset _((int n));
extern void mal_slopset _((int n));
extern void mal_statsdump _((FILE * fp));
extern void mal_setstatsfile _((FILE * fp));
extern void mal_trace _((int value));
extern int mal_verify _((int fullcheck));
extern void mal_mmap _((char *fname));


/*
 *  You may or may not want this - In gcc version 1.30, on Sun3s running
 *  SunOS3.5, this works fine.
 */
#ifdef __GNUC__
#ifndef alloca
#define alloca(n) __builtin_alloca(n)
#endif
#endif                          /* __GNUC__ */
#ifdef sparc
#define alloca(n) __builtin_alloca(n)
#endif                          /* sparc */


#endif  /* __CSRIMALLOC_H__ */                  /* Do not add anything after this line */
