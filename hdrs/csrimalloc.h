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

univptr_t trace__malloc(size_t nbytes, const char *fname, int linenum);
univptr_t trace__calloc
  (size_t nelem, size_t elsize, const char *fname, int linenum);
univptr_t trace__realloc
  (univptr_t cp, size_t nbytes, const char *fname, int linenum);
univptr_t trace__valloc(size_t size, const char *fname, int linenum);
univptr_t trace__memalign
  (size_t alignment, size_t size, const char *fname, int linenum);
univptr_t trace__emalloc(size_t nbytes, const char *fname, int linenum);
univptr_t trace__ecalloc
  (size_t nelem, size_t sz, const char *fname, int linenum);
univptr_t trace__erealloc
  (univptr_t ptr, size_t nbytes, const char *fname, int linenum);
char *trace__strdup(const char *s, const char *fname, int linenum);
char *trace__strsave(const char *s, const char *fname, int linenum);
void trace__free(univptr_t cp, const char *fname, int linenum);
void trace__cfree(univptr_t cp, const char *fname, int linenum);

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

univptr_t malloc(size_t nbytes);
univptr_t calloc(size_t nelem, size_t elsize);
univptr_t realloc(univptr_t cp, size_t nbytes);
univptr_t valloc(size_t size);
univptr_t memalign(size_t alignment, size_t size);
univptr_t emalloc(size_t nbytes);
univptr_t ecalloc(size_t nelem, size_t sz);
univptr_t erealloc(univptr_t ptr, size_t nbytes);
char *strdup(const char *s);
char *strsave(const char *s);
void free(univptr_t cp);
void cfree(univptr_t cp);

#endif                          /* CSRI_TRACE */

void mal_debug(int level);
void mal_dumpleaktrace(FILE * fp);
void mal_heapdump(FILE * fp);
void mal_leaktrace(int value);
void mal_sbrkset(int n);
void mal_slopset(int n);
void mal_statsdump(FILE * fp);
void mal_setstatsfile(FILE * fp);
void mal_trace(int value);
int mal_verify(int fullcheck);
void mal_mmap(char *fname);


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


#endif  /* __CSRIMALLOC_H__ */                        /* Do not add anything after this line */
