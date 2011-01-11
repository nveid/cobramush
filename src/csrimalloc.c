/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

#include "config.h"
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#ifdef I_FCNTL
#include <fcntl.h>
#endif
#ifdef I_SYS_FILE
#include <sys/file.h>
#endif
#ifdef I_SYS_MMAN
#include <sys/mman.h>
#endif
#ifdef I_SYS_STAT
#include <sys/stat.h>
#endif
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#include "conf.h"
#include "externs.h"
#include "mymalloc.h"
#include "csrimalloc.h"         /* Must be after mymalloc.h! */

#ifdef CSRI_TRACE
#undef malloc(x)
#undef calloc(x, n)
#undef realloc(p, x)
#undef memalign(x, n)
#undef valloc(x)
#undef emalloc(x)
#undef ecalloc(x, n)
#undef erealloc(p, x)
#undef strdup(p)
#undef strsave(p)
#undef cfree(p)
#undef free(p)
extern char *strdup _((const char *));
#endif


/*  Author: Mark Moraes <moraes@csri.toronto.edu> */
/*  Modified by Javelin to replace references to _mal_statsfile
 *  with stderr, to make glibc happy.
 */
#ifndef __DEFS_H__
#define __DEFS_H__
/* 
 *  This file includes all relevant include files, and defines various
 *  types and constants. It also defines lots of macros which operate on
 *  the malloc blocks and pointers, to try and keep the ugliness
 *  abstracted away from the source code.
 */

#include <errno.h>

#ifndef REGISTER
#ifdef __GNUC__
   /* gcc probably does better register allocation than I do */
#define REGISTER
#else
#define REGISTER register
#endif
#endif

#if defined(__STDC__)
#define proto(x)        x
#else
#define proto(x)        ()
#define const
#define volatile
#endif

#ifndef EXTERNS_H__
#define EXTERNS_H__

/* Lots of ugliness as we cope with non-standardness */

/* 
 *  Malloc definitions from General Utilities <stdlib.h>. Note that we
 *  disagree with Berkeley Unix on the return type of free/cfree.
 */
extern univptr_t malloc proto((size_t));
extern univptr_t calloc proto((size_t, size_t));
extern univptr_t realloc proto((univptr_t, size_t));
extern Free_t free proto((univptr_t));

/* General Utilities <stdlib.h> */

extern void abort proto((void));
extern void exit proto((int));
extern char *getenv proto((const char *));

/*
 *  Input/Output <stdio.h> Note we disagree with Berkeley Unix on
 *  sprintf().
 */

#if 0                           /* can't win with this one */
extern int sprintf proto((char *, const char *, ...));
#endif

extern int fputs proto((const char *, FILE *));
extern int fflush proto((FILE *));
#if 0
extern int setvbuf proto((FILE *, char *, int, memsize_t));
#endif


/* Character Handling: <string.h> */

#ifndef HAS_MEMSET
extern univptr_t memset proto((univptr_t, int, memsize_t));
#endif

#ifndef HAS_MEMCPY
extern univptr_t memcpy proto((univptr_t, const univptr_t, memsize_t));
#endif

#if 0
/* We'd better not have to do this - Javelin */
extern char *strcpy proto((char *, const char *));
extern memsize_t strlen proto((const char *));
#endif

/* UNIX -- unistd.h */
extern int write
proto((int /*fd */ , const char * /*buf */ , int /*nbytes */ ));
extern int open proto((const char * /*path */ , int /*flags */ , ...));

#define caddr_t char *
extern caddr_t sbrk proto((int));

#ifdef _SC_PAGESIZE             /* Solaris 2.x, SVR4? */
#define getpagesize()           sysconf(_SC_PAGESIZE)
#else                           /* ! _SC_PAGESIZE */
#ifdef _SC_PAGE_SIZE            /* HP, IBM */
#define getpagesize()   sysconf(_SC_PAGE_SIZE)
#else                           /* ! _SC_PAGE_SIZE */
#ifndef getpagesize
extern int getpagesize proto((void));
#endif                          /* getpagesize */
#endif                          /* _SC_PAGE_SIZE */
#endif                          /* _SC_PAGESIZE */

#ifdef _AIX                     /* IBM AIX doesn't declare sbrk, but is STDHEADERS. */
extern caddr_t sbrk proto((int));
#endif

/* Backwards compatibility with BSD/Sun -- these are going to vanish one day */
extern univptr_t valloc proto((size_t));
extern univptr_t memalign proto((size_t, size_t));
extern Free_t cfree proto((univptr_t));

/* Malloc definitions - my additions.  Yuk, should use malloc.h properly!!  */
extern univptr_t emalloc proto((size_t));
extern univptr_t ecalloc proto((size_t, size_t));
extern univptr_t erealloc proto((univptr_t, size_t));
extern char *strsave proto((const char *));
#ifdef CSRI_DEBUG
extern void mal_debug proto((int));
extern int mal_verify proto((int));
#endif
extern void mal_dumpleaktrace proto((FILE *));
extern void mal_heapdump proto((FILE *));
extern void mal_leaktrace proto((int));
extern void mal_mmap proto((char *));
extern void mal_sbrkset proto((int));
extern void mal_slopset proto((int));
#ifdef CSRI_PROFILESIZES
extern void mal_statsdump proto((FILE *));
#endif
extern void mal_trace proto((int));

/* Internal definitions */
extern int __nothing proto((void));
extern univptr_t _mal_sbrk proto((size_t));
extern univptr_t _mal_mmap proto((size_t));

#ifdef HAVE_MMAP
extern int madvise proto((caddr_t, size_t, int));
extern caddr_t mmap proto((caddr_t, size_t, int, int, int, off_t));
#endif

#endif  /* EXTERNS_H__ */                       /* Do not add anything after this line */

/* $Id: csrimalloc.c,v 1.2 2005-12-13 20:34:11 ari Exp $ */
#ifndef __ASSERT_H__
#define __ASSERT_H__
#ifdef CSRI_DEBUG
#define ASSERT(p, s) \
        ((void)(!(p)?__m_botch((s),"",(univptr_t)0,0,__FILE__,__LINE__):0))
#define ASSERT_SP(p, s, s2, sp) \
        ((void)(!(p)?__m_botch((s),(s2),(univptr_t)(sp),0,__FILE__,__LINE__):0))
#define ASSERT_EP(p, s, s2, ep) \
        ((void)(!(p)?__m_botch((s),(s2),(univptr_t)(ep),1,__FILE__,__LINE__):0))

extern int __m_botch
proto((const char *, const char *, univptr_t, int, const char *, int));
#else
#define ASSERT(p, s)
#define ASSERT_SP(p, s, s2, sp)
#define ASSERT_EP(p, s, s2, ep)
#endif
#endif  /* __ASSERT_H__ */                      /* Do not add anything after this line */
#ifndef EXIT_FAILURE
#define EXIT_FAILURE    1
#endif

#ifdef __STDC__
#include <limits.h>
#ifndef BITSPERBYTE
#define BITSPERBYTE     CHAR_BIT
#endif
#else
#include <values.h>
#endif

/* $Id: csrimalloc.c,v 1.2 2005-12-13 20:34:11 ari Exp $ */
#ifndef __ALIGN_H__
#define __ALIGN_H__
/* 
 * 'union word' must be aligned to the most pessimistic alignment needed
 * on the architecture (See align.h) since all pointers returned by
 * malloc and friends are really pointers to a Word. This is the job of
 * the 'foo' field in the union, which actually decides the size. (On
 * Sun3s, 1 int/long (4 bytes) is good enough, on Sun4s, 8 bytes are
 * necessary, i.e. 2 ints/longs)
 */
/*
 * 'union word' should also be the size necessary to ensure that an
 * sbrk() immediately following an sbrk(sizeof(union word) * n) returns
 * an address one higher than the first sbrk. i.e. contiguous space from
 * successive sbrks. (Most sbrks will do this regardless of the size -
 * Sun's doesnt) This is not vital - the malloc will work, but will not
 * be able to coalesce between sbrk'ed segments.
 */

#if defined(sparc) || defined(__sparc__) || defined(__sgi) || defined(__hpux)
/*
 * Sparcs require doubles to be aligned on double word, SGI R4000 based
 * machines are 64 bit, this is the conservative way out
 */
/* this will waste space on R4000s or the 64bit UltraSparc */
#define ALIGN long foo[2]
#define NALIGN 8
#endif

#if defined(__alpha)
/* 64 bit */
#define ALIGN long foo
#define NALIGN 8
#endif

/* This default seems to work on most 32bit machines */
#ifndef ALIGN
#define ALIGN long foo
#define NALIGN 4
#endif

/* Align with power of 2 */
#define SIMPLEALIGN(X, N) (univptr_t)(((size_t)((char *)(X) + (N-1))) & ~(N-1))

#if     NALIGN == 2 || NALIGN == 4 || NALIGN == 8 || NALIGN == 16
  /* if NALIGN is a power of 2, the next line will do ok */
#define ALIGNPTR(X) SIMPLEALIGN(X, NALIGN)
#else
  /* otherwise we need the generic version; hope the compiler isn't too smart */
static size_t _N = NALIGN;
#define ALIGNPTR(X) (univptr_t)((((size_t)((univptr_t)(X)+(NALIGN-1)))/_N)*_N)
#endif

/*
 * If your sbrk does not return blocks that are aligned to the
 * requirements of malloc(), as specified by ALIGN above, then you need
 * to define SBRKEXTRA so that it gets the extra memory needed to find
 * an alignment point. Not needed on Suns on which sbrk does return
 * properly aligned blocks. But on U*x Vaxen, A/UX and UMIPS at least,
 * you need to do this. It is safer to take the non Sun route (!! since
 * the non-sun part also works on Suns, maybe we should just remove the
 * Sun ifdef)
 */
#if ! (defined(sun) || defined(__sun__))
#define SBRKEXTRA               (sizeof(Word) - 1)
#define SBRKALIGN(x)    ALIGNPTR(x)
#else
#define SBRKEXTRA               0
#define SBRKALIGN(x)    (x)
#endif

#ifndef BITSPERBYTE
  /*
   * These values should work with any binary representation of integers
   * where the high-order bit contains the sign. Should be in values.h,
   * but just in case...
   */
  /* a number used normally for size of a shift */
#if gcos
#define BITSPERBYTE     9
#else                           /* ! gcos */
#define BITSPERBYTE     8
#endif                          /* gcos */
#endif                          /* BITSPERBYTE */

#ifndef BITS
#define BITS(type)      (BITSPERBYTE * (int) sizeof(type))
#endif                          /* BITS */

/* size_t with only the high-order bit turned on */
#define HIBITSZ (((size_t) 1) << (BITS(size_t) - 1))

#endif  /* __ALIGN_H__ */                       /* Do not add anything after this line */

/*
 * We assume that FREE is a 0 bit, and the tag for a free block, Or'ing the
 * tag with ALLOCED should turn on the high bit, And'ing with SIZEMASK
 * should turn it off.
 */

#define FREE            ((size_t) 0)
#define ALLOCED         (HIBITSZ)
#define SIZEMASK        (~HIBITSZ)

#define MAXBINS         8

#define DEF_SBRKUNITS   1024

union word {                    /* basic unit of storage */
  size_t size;                  /* size of this block + 1 bit status */
  union word *next;             /* next free block */
  union word *prev;             /* prev free block */
  univptr_t ptr;                /* stops lint complaining, keeps alignment */
  char c;
  int i;
  char *cp;
  char **cpp;
  int *ip;
  int **ipp;
   ALIGN;                       /* alignment stuff - wild fun */
};

typedef union word Word;

/*
 *  WARNING - Many of these macros are UNSAFE because they have multiple
 *  evaluations of the arguments. Use with care, avoid side-effects.
 */
/*
 * These macros define operations on a pointer to a block. The zero'th
 * word of a block is the size field, where the top bit is 1 if the
 * block is allocated. This word is also referred to as the starting tag.
 * The last word of the block is identical to the zero'th word of the
 * block and is the end tag.  IF the block is free, the second last word
 * is a pointer to the next block in the free list (a doubly linked list
 * of all free blocks in the heap), and the third from last word is a
 * pointer to the previous block in the free list.  HEADERWORDS is the
 * number of words before the pointer in the block that malloc returns,
 * TRAILERWORDS is the number of words after the returned block. Note
 * that the zero'th and the last word MUST be the boundary tags - this
 * is hard-wired into the algorithm. Increasing HEADERWORDS or
 * TRAILERWORDS suitably should be accompanied by additional macros to
 * operate on those words. The routines most likely to be affected are
 * malloc/realloc/free/memalign/mal_verify/mal_heapdump.
 */
/*
 * There are two ways we can refer to a block -- by pointing at the
 * start tag, or by pointing at the end tag. For reasons of efficiency
 * and performance, free blocks are always referred to by the end tag,
 * while allocated blocks are usually referred to by the start tag.
 * Accordingly, the following macros indicate the type of their argument
 * by using either 'p', 'sp', or 'ep' to indicate a pointer. 'p' means
 * the pointer could point at either the start or end tag. 'sp' means it
 * must point at a start tag for that macro to work correctly, 'ep'
 * means it must point at the end tag. Usually, macros operating on free
 * blocks (NEXT, PREV, VALID_PREV_PTR, VALID_NEXT_PTR) take 'ep', while
 * macros operating on allocated blocks (REALSIZE, MAGIC_PTR,
 * SET_REALSIZE) take 'sp'. The size field may be validated using either
 * VALID_START_SIZE_FIELD for an 'sp' or VALID_END_SIZE_FIELD for an
 * 'ep'.
 */
/*
 *  SIZE, SIZEFIELD and TAG are valid for allocated and free blocks,
 *  REALSIZE is valid for allocated blocks when debugging, and NEXT and
 *  PREV are valid for free blocks. We could speed things up by making
 *  the free list singly linked when not debugging - the double link are
 *  just so we can check for pointer validity. (PREV(NEXT(ep)) == ep and
 *  NEXT(PREV(ep)) == ep). FREESIZE is used only to get the size field
 *  from FREE blocks - in this implementation, free blocks have a tag
 *  bit of 0 so no masking is necessary to operate on the SIZEFIELD when
 *  a block is free. We take advantage of that as a minor optimization.
 */
#define SIZEFIELD(p)            ((p)->size)
#define SIZE(p)                 ((size_t) (SIZEFIELD(p) & SIZEMASK))
#define ALLOCMASK(n)            ((n) | ALLOCED)
#define FREESIZE(p)             SIZEFIELD(p)
/*
 *  FREEMASK should be (n) & SIZEMASK, but since (n) will always have
 *  the hi bit off after the conversion from bytes requested by the user
 *  to words.
 */
#define FREEMASK(n)             (n)
#define TAG(p)                  (SIZEFIELD(p) & ~SIZEMASK)

#ifdef CSRI_DEBUG
#define REALSIZE(sp)            (((sp)+1)->size)
#endif                          /* CSRI_DEBUG */

#define NEXT(ep)                (((ep)-1)->next)
#define PREV(ep)                (((ep)-2)->prev)

/*
 *  HEADERWORDS is the real block header in an allocated block - the
 *  free block header uses extra words in the block itself
 */
#ifdef CSRI_DEBUG
#define HEADERWORDS             2       /* Start boundary tag + real size in bytes */
#else                           /* ! CSRI_DEBUG */
#define HEADERWORDS             1       /* Start boundary tag */
#endif                          /* CSRI_DEBUG */

#define TRAILERWORDS            1

#define FREEHEADERWORDS         1       /* Start boundary tag */

#define FREETRAILERWORDS        3       /* next and prev, end boundary tag */

#define ALLOC_OVERHEAD  (HEADERWORDS + TRAILERWORDS)
#define FREE_OVERHEAD   (FREEHEADERWORDS + FREETRAILERWORDS)

/*
 *  The allocator views memory as a list of non-contiguous arenas. (If
 *  successive sbrks() return contiguous blocks, they are colaesced into
 *  one arena - if a program never calls sbrk() other than malloc(),
 *  then there should only be one arena. This malloc will however
 *  happily coexist with other allocators calling sbrk() and uses only
 *  the blocks given to it by sbrk. It expects the same courtesy from
 *  other allocators. The arenas are chained into a linked list using
 *  the first word of each block as a pointer to the next arena. The
 *  second word of the arena, and the last word, contain fake boundary
 *  tags that are permanantly marked allocated, so that no attempt is
 *  made to coalesce past them. See the code in dumpheap for more info.
 */
#define ARENASTART              2       /* next ptr + fake start tag */

#ifdef CSRI_DEBUG
  /* 
   * 1 for prev link in free block - next link is absorbed by header
   * REALSIZE word
   */
#define FIXEDOVERHEAD           (1 + ALLOC_OVERHEAD)
#else                           /* ! CSRI_DEBUG */
  /* 1 for prev link, 1 for next link, + header and trailer */
#define FIXEDOVERHEAD           (2 + ALLOC_OVERHEAD)
#endif                          /* CSRI_DEBUG */

/* 
 * Check that pointer is safe to dereference i.e. actually points
 * somewhere within the heap and is properly aligned.
 */
#define PTR_IN_HEAP(p) \
        ((p) > _malloc_loword && (p) < _malloc_hiword && \
         ALIGNPTR(p) == ((univptr_t) (p)))

/* Check that the size field is valid */
#define VALID_START_SIZE_FIELD(sp) \
        (PTR_IN_HEAP((sp) + SIZE(sp) - 1) && \
         SIZEFIELD(sp) == SIZEFIELD((sp) + SIZE(sp) - 1))

#define VALID_END_SIZE_FIELD(ep) \
        (PTR_IN_HEAP((ep) - SIZE(ep) + 1) && \
         SIZEFIELD(ep) == SIZEFIELD((ep) - SIZE(ep) + 1))


#define ulong unsigned long

#ifdef CSRI_DEBUG
  /* 
   * Byte that is stored at the end of each block if the requested size
   * of the block is not a multiple of sizeof(Word). (If it is a multiple
   * of sizeof(Word), then we don't need to put the magic because the
   * endboundary tag will be corrupted and the tests that check the
   * validity of the boundary tag should detect it
   */
#ifndef u_char
#define u_char          unsigned char
#endif                          /* u_char */

#define MAGIC_BYTE              ((u_char) '\252')

  /* 
   * Check if size of the block is identical to requested size. Typical
   * tests will be of the form DONT_NEED_MAGIC(p) || something for
   * short-circuited protection, because blocks where DONT_NEED_MAGIC is
   * true will be tested for boundary tag detection so we don't need the
   * magic byte at the end.
   */
#define DONT_NEED_MAGIC(sp) \
        (REALSIZE(sp) == ((SIZE(sp) - ALLOC_OVERHEAD) * sizeof(Word)))

  /*
   * VALID_REALSIZE must not be used if either DONT_NEED_MAGIC or TOO_SMALL
   * are true
   */
#define VALID_REALSIZE(sp) \
        (REALSIZE(sp) < ((SIZE(sp) - ALLOC_OVERHEAD)*sizeof(Word)))

  /* Location of the magic byte */
#define MAGIC_PTR(sp)           ((u_char *)((sp) + HEADERWORDS) + REALSIZE(sp))

  /*
   * malloc code should only use the next three macros SET_REALSIZE,
   * VALID_MAGIC and TOO_SMALL, since they are the only ones which have
   * non-CSRI_DEBUG (null) alternatives
   */

  /* Macro sets the realsize of a block if necessary */
#define SET_REALSIZE(sp, n) \
        (TOO_SMALL(sp) ? 0 : \
         (REALSIZE(sp) = (n), \
          DONT_NEED_MAGIC(sp) || (*MAGIC_PTR(sp) = MAGIC_BYTE)))

  /* Macro tests that the magic byte is valid if it exists */
#define VALID_MAGIC(sp) \
        (TOO_SMALL(sp) || DONT_NEED_MAGIC(sp) || \
         (VALID_REALSIZE(sp) && (*MAGIC_PTR(sp) == MAGIC_BYTE)))

  /*
   * Bytes left over memalign may be too small to hold the real size, they
   * may be 1 word, i.e start and end tag stored in same word!
   */
#define TOO_SMALL(sp) (SIZE(sp) < ALLOC_OVERHEAD)

#else                           /* ! CSRI_DEBUG */
#define SET_REALSIZE(sp, n)
#define VALID_MAGIC(sp) (1)
#define TOO_SMALL(sp)           (0)
#endif                          /* CSRI_DEBUG */

/* 
 *  Check that a free list ptr points to a block with something pointing
 *  back. This is the only reason we use a doubly linked free list.
 */
#define VALID_NEXT_PTR(ep) (PTR_IN_HEAP(NEXT(ep)) && PREV(NEXT(ep)) == (ep))
#define VALID_PREV_PTR(ep) (PTR_IN_HEAP(PREV(ep)) && NEXT(PREV(ep)) == (ep))

/*
 *  quick bit-arithmetic to check a number (including 1) is a power of two.
 */
#define is_power_of_2(x) ((((x) - 1) & (x)) == 0)

/*
 *  An array to try and keep track of the distribution of sizes of
 *  malloc'ed blocks
 */
#ifdef CSRI_PROFILESIZES
#define MAXPROFILESIZE 2*1024
#define COUNTSIZE(nw) (_malloc_scount[((nw) < MAXPROFILESIZE) ? (nw) : 0]++)
#else
#define COUNTSIZE(nw)
#endif

#ifndef USESTDIO
  /* 
   * Much better not to use stdio - stdio calls malloc() and can
   * therefore cause problems when debugging heap corruption. However,
   * there's no guaranteed way to turn off buffering on a FILE pointer in
   * ANSI.  This is a vile kludge!
   */
#define fputs(s, f)             write(fileno(f), (s), strlen(s))
#define setvbuf(f, s, n, l)     __nothing()
#define fflush(f)               __nothing()
#endif                          /* USESTDIO */

#ifdef CSRI_TRACE
  /* 
   * Prints a trace string. For convenience, x is an
   * sprintf(_malloc_statsbuf, ...) or strcpy(_malloc_statsbuf, ...);
   * something which puts the appropriate text to be printed in
   * _malloc_statsbuf - ugly, but more convenient than making x a string.
   */
#define PRCSRI_TRACE(x) \
  if (_malloc_tracing) { \
        (void) x; \
        (void) fputs(_malloc_statsbuf, stderr); \
  } else \
        _malloc_tracing += 0
#else
#define PRCSRI_TRACE(x)
#endif

#ifdef CSRI_DEBUG
#define CHECKHEAP() \
  if (_malloc_debugging >= 2) \
        (void) mal_verify(_malloc_debugging >= 3); \
  else \
        _malloc_debugging += 0

#define CHECKFREEPTR(ep, msg) \
  if (_malloc_debugging >= 1) { \
        ASSERT_EP(PTR_IN_HEAP(ep), "pointer outside heap", (msg), (ep)); \
        ASSERT_EP(TAG(ep) == FREE, "already-allocated block", (msg), (ep)); \
        ASSERT_EP(VALID_END_SIZE_FIELD(ep), "corrupt block", msg, (ep)); \
        ASSERT_EP(VALID_NEXT_PTR(ep), "corrupt link to next free block", (msg), (ep)); \
        ASSERT_EP(VALID_PREV_PTR(ep), "corrupt link to previous free block", (msg), (ep)); \
  } else \
        _malloc_debugging += 0

#define CHECKALLOCPTR(sp, msg) \
  if (_malloc_debugging >= 1) { \
        ASSERT_SP(PTR_IN_HEAP(sp), "pointer outside heap", (msg), (sp)); \
        ASSERT_SP(TAG(sp) != FREE, "already-freed block", (msg), (sp)); \
        ASSERT_SP(VALID_START_SIZE_FIELD(sp), "corrupt block", (msg), (sp)); \
        ASSERT_SP(VALID_MAGIC(sp), "block with end overwritten", (msg), (sp)); \
  } else \
        _malloc_debugging += 0

#else                           /* !CSRI_DEBUG */
#define CHECKHEAP()
#define CHECKFREEPTR(ep, msg)
#define CHECKALLOCPTR(sp, msg)
#endif                          /* CSRI_DEBUG */

#define FREEMAGIC               '\125'

/*
 *  Memory functions but in words. We just call memset/memcpy, and hope
 *  that someone has optimized them. If you are on pure 4.2BSD, either
 *  redefine these in terms of bcopy/your own memcpy, or
 *  get the functions from one of 4.3src/Henry Spencer's strings
 *  package/C News src
 */
#define MEMSET(p, c, n) \
        (void) memset((univptr_t) (p), (c), (memsize_t) ((n) * sizeof(Word)))
#define MEMCPY(p1, p2, n) \
        (void) memcpy((univptr_t) (p1), (univptr_t) (p2), \
                                  (memsize_t) ((n) * sizeof(Word)))


#ifdef CSRI_DEBUG
#define DMEMSET(p, n)   MEMSET((p), FREEMAGIC, (n))
#else
#define DMEMSET(p, n)
#endif

/* 
 *  Thanks to Hugh Redelmeier for pointing out that an rcsid is "a
 *  variable accessed in a way not obvious to the compiler", so should
 *  be called volatile. Amazing - a use for const volatile...
 */
#ifndef RCSID                   /* define RCSID(x) to nothing if don't want the rcs headers */
#if defined(lint) || defined(__STRICT_ANSI__)
#define RCSID(x)
#else
#define RCSID(x)                static const volatile char *rcsid = x ;
#endif
#endif

#endif  /* __DEFS_H__ */                        /* Do not add anything after this line */

/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*
 *  All globals are names starting with _malloc, which should not clash
 *  with anything else.
 */
/*
 *  Remember to initialize the variable in globals.c if you want, and
 *  provide an alternative short name in globrename.h
 */
/* $Id: csrimalloc.c,v 1.2 2005-12-13 20:34:11 ari Exp $ */
#ifndef __GLOBALRENAME_H__
#define __GLOBALRENAME_H__
/*
 *  Renaming all external symbols that are internal to the malloc to be
 *  unique within 6 characters for machines whose linkers just can't keep
 *  up. We hope the cpp is smart enough - if not, get GNU cccp or the
 *  cpp that comes with X Windows Version 11 Release 3.
 */
#ifdef SHORTNAMES
#define _malloc_minchunk        __MAL1_minchunk

#define _malloc_rovers          __MAL2_rovers
#define _malloc_binmax          __MALH_binmax
#define _malloc_firstbin        __MALG_firstbin
#define _malloc_lastbin         __MAL3_lastbin
#define _malloc_hiword          __MAL4_hiword
#define _malloc_loword          __MAL5_loword

#define _malloc_sbrkunits       __MAL6_sbrkunits

#define _malloc_mem                     __MAL7_mem

#define _malloc_tracing         __MAL8_tracing
#define _malloc_statsbuf        __MALA_statsbuf

#define _malloc_leaktrace       __MALB_leaktrace

#ifdef CSRI_PROFILESIZES
#define _malloc_scount          __MALC_scount
#endif                          /* CSRI_PROFILESIZES */

#ifdef CSRI_DEBUG
/*
 *  0 or 1 means checking all pointers before using them. Reasonably
 *  thorough.  2 means check the entire heap on every call to
 *  malloc/free/realloc/memalign. (the rest call these)
 */
#define _malloc_debugging       __MALD_debugging
#endif                          /* CSRI_DEBUG */
#define _malloc_version         __MALE_version

#define _malloc_memfunc         __MALF_memfunc

#endif  /* SHORTNAMES */                        /* Do not add anything after this line */
#endif  /* __GLOBALRENAME_H__ */                         /* Do not add anything after this line */
const char *_malloc_version =
  "CSRI, University of Toronto Malloc Version 1.18alpha";

/*
 *  _malloc_minchunk is the minimum number of units that a block can be
 *  cut down to.  If the difference between the required block size, and
 *  the first available block is greater than _malloc_minchunk, the
 *  block is chopped into two pieces, else the whole block is returned.
 *  The larger this is, the less fragmentation there will be, but the
 *  greater the waste. The actual minimum size of a block is therefore
 *  _malloc_minchunk*sizeof(Word) This consists of one word each for the
 *  boundary tags, one for the next and one for the prev pointers in a
 *  free block.
 */
size_t _malloc_minchunk = FIXEDOVERHEAD;

/*
 * _malloc_rovers is the array of pointers into that each point 'someplace'
 * into a list of free blocks smaller than a specified size. We start our
 * search for a block from _malloc_rovers, thus starting the search at a
 * different place everytime, rather than at the start of the list.  This
 * improves performance considerably, sez Knuth
 */
Word *_malloc_rovers[MAXBINS + 1] = { NULL };
const size_t _malloc_binmax[MAXBINS] = {
  8, 16, 32, 64, 128, 256, 512, 1024
};
int _malloc_firstbin = 0;
int _malloc_lastbin = 0;
Word *_malloc_hiword = NULL;
Word *_malloc_loword = NULL;

/* 
 *  _malloc_sbrkunits is the multiple of sizeof(Word) to actually use in
 *  sbrk() calls - _malloc_sbrkunits should be large enough that sbrk
 *  isn't called too often, but small enough that any program that
 *  mallocs a few bytes doesn't end up being very large. I've set it to
 *  1K resulting in a sbrk'ed size of 8K. This is (coincidentally!) the
 *  pagesize on Suns.  I think that this seems a reasonable number for
 *  modern programs that malloc heavily. For small programs, you may
 *  want to set it to a lower number.
 */
size_t _malloc_sbrkunits = DEF_SBRKUNITS;

Word *_malloc_mem = NULL;

/*
 *  Do not call any output routine other than fputs() - use sprintf() if
 *  you want to format something before printing. We don't want stdio
 *  calling malloc() if we can help it
 */
int _malloc_tracing = 0;        /* No tracing */
char _malloc_statsbuf[128];

int _malloc_leaktrace = 0;

#ifdef CSRI_PROFILESIZES
int _malloc_scount[MAXPROFILESIZE];
#endif                          /* CSRI_PROFILESIZES */

#ifdef CSRI_DEBUG
/*
 *  0 or 1 means checking all pointers before using them. Reasonably
 *  thorough.  2 means check the entire heap on every call to
 *  malloc/free/realloc/memalign. (the rest call these)
 */
int _malloc_debugging = 0;
#endif                          /* CSRI_DEBUG */

univptr_t (*_malloc_memfunc) proto((size_t)) = _mal_sbrk;

#ifndef __GLOBALS_H__
#define __GLOBALS_H__
/*
 *  Remember to initialize the variable in globals.c if you want, and
 *  provide an alternative short name in globrename.h
 */

extern
size_t _malloc_minchunk;

extern Word *_malloc_rovers[];
extern const
size_t _malloc_binmax[];
extern int
 _malloc_firstbin;
extern int
 _malloc_lastbin;
extern Word *_malloc_hiword;
extern Word *_malloc_loword;

extern
size_t _malloc_sbrkunits;

extern Word *_malloc_mem;

extern int
 _malloc_tracing;               /* No tracing */
extern char
 _malloc_statsbuf[];

extern int
 _malloc_leaktrace;

#ifdef CSRI_PROFILESIZES
extern int
 _malloc_scount[];
#endif                          /* CSRI_PROFILESIZES */

#ifdef CSRI_DEBUG
/*
 *  0 or 1 means checking all pointers before using them. Reasonably
 *  thorough.  2 means check the entire heap on every call to
 *  malloc/free/realloc/memalign. (the rest call these)
 */
extern int
 _malloc_debugging;
#endif                          /* CSRI_DEBUG */

extern
univptr_t (*_malloc_memfunc) proto((size_t));

extern int
__m_prblock proto((univptr_t, int, FILE *));

#endif  /* __GLOBALS_H__ */                     /* Do not add anything after this line */

/*
   ** sptree.h:  The following type declarations provide the binary tree
   **  representation of event-sets or priority queues needed by splay trees
   **
   **  assumes that data and datb will be provided by the application
   **  to hold all application specific information
   **
   **  assumes that key will be provided by the application, comparable
   **  with the compare function applied to the addresses of two keys.
 */

#ifndef SPTREE_H
#define SPTREE_H

typedef struct _spblk {
  struct _spblk *leftlink;
  struct _spblk *rightlink;
  struct _spblk *uplink;

  univptr_t
   key;                         /* formerly time/timetyp */
  univptr_t
   data;                        /* formerly aux/auxtype */
  univptr_t
   datb;
} SPBLK;

typedef struct {
  SPBLK *root;                  /* root node */

  /* Statistics, not strictly necessary, but handy for tuning  */

  int
   lookups;                     /* number of splookup()s */
  int
   lkpcmps;                     /* number of lookup comparisons */

  int
   enqs;                        /* number of spenq()s */
  int
   enqcmps;                     /* compares in spenq */

  int
   splays;
  int
   splayloops;

} SPTREE;

#if defined(__STDC__)
#define __proto(x)      x
#else
#define __proto(x)      ()
#endif

/* sptree.c */
/* init tree */
extern SPTREE *__spinit __proto((void));
/* find key in a tree */
extern SPBLK *__splookup __proto((univptr_t, SPTREE *));
/* enter an item, allocating or replacing */
extern SPBLK *__spadd __proto((univptr_t, univptr_t, univptr_t, SPTREE *));
/* scan forward through tree */
extern void
__spscan __proto((void (*)__proto((SPBLK *)), SPBLK *, SPTREE *));
/* return tree statistics */
extern char *__spstats __proto((SPTREE *));
/* delete node from tree */
extern void
__spdelete __proto((SPBLK *, SPTREE *));

#undef __proto

#endif                          /* SPTREE_H */

#ifndef __CSRI_TRACE_H__
#define __CSRI_TRACE_H__
extern void
__m_install_record proto((univptr_t, const char *));
extern void
__m_delete_record proto((univptr_t));

#define RECORD_FILE_AND_LINE(addr, fname, linenum) \
        if (_malloc_leaktrace) { \
                (void) sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum); \
                __m_install_record(addr, _malloc_statsbuf); \
        } else \
                _malloc_leaktrace += 0

#define DELETE_RECORD(addr) \
        if (_malloc_leaktrace) \
                __m_delete_record(addr); \
        else \
                _malloc_leaktrace += 0

#endif  /* __CSRI_TRACE_H__ */                  /* Do not add anything after this line */

#include "confmagic.h"

#ifdef CSRI_TRACE
/* Tracing malloc definitions - helps find leaks */
univptr_t
trace__malloc _((size_t nbytes, const char *fname, int linenum));
univptr_t
trace__calloc _((size_t nelem, size_t elsize, const char *fname, int linenum));
univptr_t
trace__realloc _((univptr_t cp, size_t nbytes, const char *fname, int linenum));
univptr_t trace__valloc _((size_t size, const char *fname, int linenum));
univptr_t
 trace__memalign
_((size_t alignment, size_t size, const char *fname, int linenum));
univptr_t trace__emalloc _((size_t nbytes, const char *fname, int linenum));
univptr_t
trace__ecalloc _((size_t nelem, size_t sz, const char *fname, int linenum));
univptr_t
 trace__erealloc
_((univptr_t ptr, size_t nbytes, const char *fname, int linenum));
char *trace__strdup _((const char *s, const char *fname, int linenum));
char *trace__strsave _((const char *s, const char *fname, int linenum));
void
trace__free _((univptr_t cp, const char *fname, int linenum));
void
trace__cfree _((univptr_t cp, const char *fname, int linenum));
#else                           /* CSRI_TRACE */
univptr_t
malloc _((size_t nbytes));
univptr_t
calloc _((size_t nelem, size_t elsize));
univptr_t
realloc _((univptr_t cp, size_t nbytes));
univptr_t
valloc _((size_t size));
univptr_t
memalign _((size_t alignment, size_t size));
univptr_t
emalloc _((size_t nbytes));
univptr_t
ecalloc _((size_t nelem, size_t sz));
univptr_t
erealloc _((univptr_t ptr, size_t nbytes));
Free_t free _((univptr_t cp));
Free_t cfree _((univptr_t cp));
#endif                          /* CSRI_TRACE */

int
 __m_botch
_((const char *s1, const char *s2, univptr_t p,
   int is_end_ptr, const char *filename, int linenumber));
void
__m_prnode _((SPBLK * spblk));
void
mal_contents _((FILE * fp));
#ifdef CSRI_DEBUG
void
mal_debug _((int level));
int
mal_verify _((int fullcheck));
#endif
void
mal_dumpleaktrace _((FILE * fp));
void
mal_heapdump _((FILE * fp));
void
mal_leaktrace _((int value));
void
mal_sbrkset _((int n));
void
mal_slopset _((int n));
#ifdef CSRI_PROFILESIZES
void
mal_statsdump _((FILE * fp));
#endif
void
mal_trace _((int value));
void
mal_mmap _((char *fname));

#ifdef CSRI_TRACE

univptr_t
trace__emalloc(nbytes, fname, linenum)
    size_t nbytes;
    const char *fname;
    int
     linenum;
{
  univptr_t cp;

  PRCSRI_TRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
  cp = emalloc(nbytes);
  RECORD_FILE_AND_LINE(cp, fname, linenum);
  return (cp);
}


univptr_t
trace__erealloc(ptr, nbytes, fname, linenum)
    univptr_t
     ptr;
    size_t nbytes;
    const char *fname;
    int
     linenum;
{
  univptr_t cp;

  PRCSRI_TRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
  cp = erealloc(ptr, nbytes);
  RECORD_FILE_AND_LINE(cp, fname, linenum);
  return (cp);
}

univptr_t
trace__ecalloc(nelem, sz, fname, linenum)
    size_t nelem, sz;
    const char *fname;
    int
     linenum;
{
  univptr_t cp;

  PRCSRI_TRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
  cp = ecalloc(nelem, sz);
  RECORD_FILE_AND_LINE(cp, fname, linenum);
  return (cp);
}


/*
 * These are the wrappers around malloc for detailed tracing and leak
 * detection.  Allocation routines call RECORD_FILE_AND_LINE to record the
 * filename/line number keyed on the block address in the splay tree,
 * de-allocation functions call DELETE_RECORD to delete the specified block
 * address and its associated file/line from the splay tree.
 */
/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY */

univptr_t
trace__malloc(nbytes, fname, linenum)
    size_t nbytes;
    const char *fname;
    int
     linenum;
{
  univptr_t cp;

  PRCSRI_TRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
  cp = malloc(nbytes);
  RECORD_FILE_AND_LINE(cp, fname, linenum);
  return (cp);
}

/* ARGSUSED if CSRI_TRACE is not defined */
void
trace__free(cp, fname, linenum)
    univptr_t cp;
    const char *fname;
    int linenum;
{
  PRCSRI_TRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
  DELETE_RECORD(cp);
  free(cp);
}

univptr_t
trace__realloc(cp, nbytes, fname, linenum)
    univptr_t
     cp;
    size_t nbytes;
    const char *fname;
    int
     linenum;
{
  univptr_t old;

  PRCSRI_TRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
  old = cp;
  cp = realloc(cp, nbytes);
  if (old != cp) {
    DELETE_RECORD(old);
    RECORD_FILE_AND_LINE(cp, fname, linenum);
  }
  return (cp);
}

univptr_t
trace__calloc(nelem, elsize, fname, linenum)
    size_t nelem, elsize;
    const char *fname;
    int
     linenum;
{
  univptr_t cp;

  PRCSRI_TRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
  cp = calloc(nelem, elsize);
  RECORD_FILE_AND_LINE(cp, fname, linenum);
  return (cp);
}

/* ARGSUSED if CSRI_TRACE is not defined */
void
trace__cfree(cp, fname, linenum)
    univptr_t cp;
    const char *fname;
    int linenum;
{
  PRCSRI_TRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
  DELETE_RECORD(cp);
  /* No point calling cfree() - it just calls free() */
  free(cp);
}

/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY */



univptr_t
trace__memalign(alignment, size, fname, linenum)
    size_t alignment, size;
    const char *fname;
    int
     linenum;
{
  univptr_t cp;

  PRCSRI_TRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
  cp = memalign(alignment, size);
  RECORD_FILE_AND_LINE(cp, fname, linenum);
  return (cp);
}

univptr_t
trace__valloc(size, fname, linenum)
    size_t size;
    const char *fname;
    int
     linenum;
{
  univptr_t cp;

  PRCSRI_TRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
  cp = valloc(size);
  RECORD_FILE_AND_LINE(cp, fname, linenum);
  return (cp);
}

/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY */

char *
trace__strdup(s, fname, linenum)
    const char *s;
    const char *fname;
    int linenum;
{
  char *cp;

  PRCSRI_TRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
  cp = (char *) strdup(s);
  RECORD_FILE_AND_LINE((univptr_t) cp, fname, linenum);
  return (cp);
}

char *
trace__strsave(s, fname, linenum)
    const char *s;
    const char *fname;
    int linenum;
{
  char *cp;

  PRCSRI_TRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
  cp = strsave(s);
  RECORD_FILE_AND_LINE((univptr_t) cp, fname, linenum);
  return (cp);
}
#endif                          /* CSRI_TRACE */
int
__nothing()
{
  return 0;
}

/*
 * Simple botch routine - writes directly to stderr.  CAREFUL -- do not use
 * printf because of the vile hack we use to redefine fputs with write for
 * normal systems (i.e not super-pure ANSI)!
 */
int
__m_botch(s1, s2, p, is_end_ptr, filename, linenumber)
    const char *s1, *s2;
    univptr_t p;
    const char *filename;
    int linenumber, is_end_ptr;
{
  static char linebuf[32];      /* Enough for a BIG linenumber! */
  static int notagain = 0;

  if (notagain == 0) {
    (void) fflush(stderr);
    (void) sprintf(linebuf, "%d: ", linenumber);
    (void) fputs("memory corruption found, file ", stderr);
    (void) fputs(filename, stderr);
    (void) fputs(", line ", stderr);
    (void) fputs(linebuf, stderr);
    (void) fputs(s1, stderr);
    if (s2 && *s2) {
      (void) fputs(" ", stderr);
      (void) fputs(s2, stderr);
    }
    (void) fputs("\n", stderr);
    (void) __m_prblock(p, is_end_ptr, stderr);
    /*
     * In case stderr is buffered and was written to before we
     * tried to unbuffer it
     */
    (void) fflush(stderr);
    notagain++;                 /* just in case abort() tries to cleanup */
    abort();
  }
  return 0;                     /* SHOULDNTHAPPEN */
}

/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY */



int
__m_prblock(p, is_end_ptr, fp)
    univptr_t p;
    int is_end_ptr;
    FILE *fp;
{
  Word *blk = (Word *) p;
  Word *blkend;
  ulong tag;
  ulong blksize;
  char buf[512];                /* long enough for the sprintfs below */

  if (blk == NULL)
    return 0;

  if (!PTR_IN_HEAP(blk)) {
    sprintf(buf, "  ** pointer 0x%lx not in heap\n", (ulong) blk);
    fputs(buf, fp);
    return 0;
  }
  blksize = SIZE(blk);
  tag = TAG(blk);
  if (is_end_ptr) {
    blkend = blk;
    blk -= blksize - 1;
  } else {
    blkend = blk + blksize - 1;
  }
  (void) sprintf(buf, "  %s blk: 0x%lx to 0x%lx, %lu (0x%lx) words",
                 tag == FREE ? "Free" : "Allocated", (ulong) blk,
                 (ulong) blkend, blksize, blksize);
  (void) fputs(buf, fp);
  if (is_end_ptr && !PTR_IN_HEAP(blk)) {
    sprintf(buf, "  ** start pointer 0x%lx not in heap\n", (ulong) blk);
    fputs(buf, fp);
    return 0;
  }
  if (!is_end_ptr && !PTR_IN_HEAP(blkend)) {
    sprintf(buf, "  ** end pointer 0x%lx not in heap\n", (ulong) blk);
    fputs(buf, fp);
    return 0;
  }
  if (tag == FREE) {
#ifdef CSRI_DEBUG
    int i;
#endif
    int n;
    char *cp;

    (void) sprintf(buf, " next=0x%lx, prev=0x%lx\n",
                   (ulong) NEXT(blkend), (ulong) PREV(blkend));
    (void) fputs(buf, fp);
    /* Make sure free block is filled with FREEMAGIC */
    n = (blksize - FREE_OVERHEAD) * sizeof(Word);
    cp = (char *) (blk + FREEHEADERWORDS);
#ifdef CSRI_DEBUG
    for (i = 0; i < n; i++, cp++) {
      if (*cp != FREEMAGIC) {
        (void) fputs("  ** modified after free().\n", fp);
        break;
      }
    }
#endif
  } else {
#ifdef CSRI_DEBUG
    if (!TOO_SMALL(blk)) {
      (void) sprintf(buf, " really %lu bytes\n", (ulong) REALSIZE(blk));
      (void) fputs(buf, fp);
    } else {
      (void) fputs("\n", fp);
    }
#else
    (void) fputs("\n", fp);
#endif
  }
  if (TAG(blk) == FREE) {
    if (!VALID_NEXT_PTR(blkend))
      (void) fputs("  ** bad next pointer\n", fp);
    if (!VALID_PREV_PTR(blkend))
      (void) fputs("  ** bad prev pointer\n", fp);
  } else {
    if (!VALID_MAGIC(blk))
      (void) fputs("  ** end of block overwritten\n", fp);
  }
  if (!VALID_START_SIZE_FIELD(blk)) {
    sprintf(buf, "  ** bad size field: tags = 0x%x, 0x%x\n",
            (unsigned int) SIZEFIELD(blk), (unsigned int) SIZEFIELD(blkend));
    (void) fputs(buf, fp);
    return 0;
  }
  (void) fflush(fp);
  return 1;
}

/*
 * Similar to malloc_verify except that it prints the heap as it goes along.
 * The only ASSERTs in this routine are those that would cause it to wander
 * off into the sunset because of corrupt tags.
 */
void
mal_heapdump(fp)
    FILE *fp;
{
  REGISTER Word *ptr;
  REGISTER Word *blk;
  REGISTER Word *blkend;
  int i;
  char buf[512];                /* long enough for the sprintfs below */

  if (_malloc_loword == NULL) { /* Nothing malloc'ed yet */
    (void) fputs("Null heap - nothing malloc'ed yet\n", fp);
    return;
  }
  (void) fputs("Heap printout:\n", fp);
  (void) fputs("Free list rover pointers:\n", fp);
  sprintf(buf, "  First non-null bin is %d\n", _malloc_firstbin);
  (void) fputs(buf, fp);
  for (i = 0; i < MAXBINS; i++) {
    if ((ptr = _malloc_rovers[i]) == NULL)
      continue;
    (void) sprintf(buf, "  %d: 0x%lx\n", i, (ulong) ptr);
    (void) fputs(buf, fp);
    if (!PTR_IN_HEAP(ptr))
      (void) fputs("  ** not in heap\n", fp);
    if (!VALID_END_SIZE_FIELD(ptr))
      (void) fputs("  ** bad end size field\n", fp);
    if (!VALID_NEXT_PTR(ptr))
      (void) fputs("  ** bad next pointer\n", fp);
    if (!VALID_PREV_PTR(ptr))
      (void) fputs("  ** bad prev pointer\n", fp);
  }
  if (_malloc_rovers[MAXBINS] != NULL) {
    (void) sprintf(buf, "  ** rover terminator is 0x%lx, fixing\n",
                   (ulong) _malloc_rovers[MAXBINS]);
    (void) fputs(buf, fp);
    _malloc_rovers[MAXBINS] = NULL;
  }
  for (ptr = _malloc_mem; ptr != NULL; ptr = ptr->next) {
    /* print the arena */
    (void) sprintf(buf, "Arena from 0x%lx to 0x%lx, %lu (0x%lx) words\n",
                   (ulong) ptr, (ulong) (ptr + SIZE(ptr + 1)),
                   (ulong) SIZE(ptr + 1) + 1, (ulong) SIZE(ptr + 1) + 1);
    (void) fputs(buf, fp);
    (void) sprintf(buf, "Next arena is 0x%lx\n", (ulong) ptr->next);
    (void) fputs(buf, fp);
    (void) fflush(fp);
    ASSERT(SIZEFIELD(ptr + 1) == SIZEFIELD(ptr + SIZE(ptr + 1)),
           "mal_dumpheap: corrupt malloc arena");
    blkend = ptr + SIZE(ptr + 1);
    for (blk = ptr + ARENASTART; blk < blkend; blk += SIZE(blk)) {
      if (!__m_prblock((univptr_t) blk, 0, fp)) {
        __m_botch("mal_dumpheap: corrupt block", "",
                  (univptr_t) 0, 0, __FILE__, __LINE__);
      }
    }
  }
  (void) fputs("==============\n", fp);
  (void) fflush(fp);
}

/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY */



/*
 *  malloc which dies if it can't allocate enough storage.
 */
univptr_t
emalloc(nbytes)
    size_t nbytes;
{
  univptr_t cp = malloc(nbytes);

  if (cp == 0) {
    (void) fputs("No more memory for emalloc\n", stderr);
#ifdef CSRI_DEBUG
    (void) fflush(stderr);
    abort();
#else
    exit(EXIT_FAILURE);
#endif
  }
  return (cp);
}

/*
 *  realloc which dies if it can't allocate enough storage.
 */
univptr_t
erealloc(ptr, nbytes)
    univptr_t
     ptr;
    size_t nbytes;
{
  univptr_t cp = realloc(ptr, nbytes);

  if (cp == 0) {
    (void) fputs("No more memory for erealloc\n", stderr);
#ifdef CSRI_DEBUG
    (void) fflush(stderr);
    abort();
#else
    exit(EXIT_FAILURE);
#endif
  }
  return (cp);
}

/*
 *  calloc which dies if it can't allocate enough storage.
 */
univptr_t
ecalloc(nelem, sz)
    size_t nelem, sz;
{
  size_t nbytes = nelem * sz;
  univptr_t cp = emalloc(nbytes);

  (void) memset((univptr_t) cp, 0, (memsize_t) nbytes);
  return (cp);
}

/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY */



/* gets memory from the system via the sbrk() system call.  Most Un*xes */
univptr_t
_mal_sbrk(nbytes)
    size_t nbytes;
{
  static char *lastsbrk;
  char *p;

  p = (char *) sbrk((int) nbytes);
  /*
   * Some old SVR3 apparently have a kernel bug: after several sbrk
   * calls, sbrk suddenly starts returning values lower than the ones it
   * returned before.  Yeesh!
   */
  if (nbytes > 0) {
    ASSERT(p > lastsbrk,
           "system call error? sbrk returned value lower than previous calls");
    lastsbrk = p;
  }
  return p;
}

/*
 * gets memory from the system via mmaping a file.  This was written for SunOS
 * versions greater than 4.0.  The filename is specified by the environment
 * variable CSRIMALLOC_MMAPFILE or by the call to mal_mmapset().  Using this
 * instead of sbrk() has the advantage of bypassing the swap system, allowing
 * processes to run with huge heaps even on systems configured with small swap
 * space.
 */
static char *mmap_filename;

#ifdef HAVE_MMAP
/* Sun gets size_t wrong, and these follow, thanks to my #defines! */
#undef caddr_t
#undef size_t
#undef u_char

univptr_t
_mal_mmap(nbytes)
    size_t nbytes;
{
  static struct {
    int i_fd;
    caddr_t i_data;
    caddr_t i_end;
    size_t i_size;
    size_t i_alloced;
  } mmf;
  struct stat stbuf;

  if (mmf.i_data != NULL) {
    /* Already initialized & mmaped the file */
    univptr_t p = mmf.i_data + mmf.i_alloced;

    if ((char *) p + nbytes > mmf.i_end) {
      errno = ENOMEM;
      return (univptr_t) -1;
    }
    mmf.i_alloced += nbytes;
    return p;
  }
  /*
   * This code is run the first time the function is called, it opens
   * the file and mmaps the
   */
  if (mmap_filename == NULL) {
    mmap_filename = getenv("CSRIMALLOC_MMAPFILE");
    if (mmap_filename == NULL) {
      errno = ENOMEM;
      return (univptr_t) -1;
    }
  }
  mmf.i_fd = open(mmap_filename, O_RDWR, 0666);
  if (mmf.i_fd < 0 || fstat(mmf.i_fd, &stbuf) < 0)
    return (univptr_t) -1;
  if (stbuf.st_size < nbytes) {
    errno = ENOMEM;
    return (univptr_t) -1;
  }
  mmf.i_size = stbuf.st_size;
  mmf.i_data = mmap((caddr_t) 0, mmf.i_size, PROT_READ | PROT_WRITE,
                    MAP_SHARED, mmf.i_fd, (off_t) 0);
  if (mmf.i_data == (caddr_t) - 1)
    return (univptr_t) -1;
  mmf.i_end = mmf.i_data + mmf.i_size;
  mmf.i_alloced = nbytes;
  /* Advise vm system of random access pattern */
  (void) madvise(mmf.i_data, mmf.i_size, MADV_RANDOM);
  return mmf.i_data;
}
#else                           /* !HAVE_MMAP */
univptr_t
_mal_mmap(nbytes)
    size_t nbytes __attribute__ ((__unused__));
{
  return (univptr_t) -1;
}

#endif                          /* HAVE_MMAP */

void
mal_mmap(fname)
    char *fname;
{
  _malloc_memfunc = _mal_mmap;
  mmap_filename = fname;
}

/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY */



/* 
 *  These routines provide an interface for tracing memory leaks. The
 *  user can turn on leak tracing at any time by calling
 *  mal_leaktrace(1), after which every block allocated by
 *  _malloc()/_calloc()/_realloc()/_valloc()/_memalign() has a string
 *  (containing the filename and linenumber of the routine invoking it)
 *  stored in a database. When _free()/_cfree() is called on that block,
 *  the record is deleted from the database. The user can call
 *  mal_dumpleaktrace() to show the list of blocks allocated, and
 *  where they were allocated. The location of leaks can usually be
 *  detected from this.
 */
/*
 *  The tree implementation used to store the blocks is a splay-tree,
 *  using an implementation in C by Dave Brower (daveb@rtech.uucp),
 *  translated from Douglas Jones' original Pascal. However, any data
 *  structure that permits insert(), delete() and traverse()/apply() of
 *  key, value pairs should be suitable. Only this file needs to be
 *  changed.
 */
static SPTREE *sp = NULL;

/*
 *  num is a sequence number, incremented for ever block. min_num gets
 *  set to num after every dumpleaktrace - subsequent dumps do not print
 *  any blocks with sequence numbers less than min_num
 */
static unsigned long min_num = 0;
static unsigned long num = 0;

/*
 * These are used by mal_contents to count number of allocated blocks and the
 * number of bytes allocated.  Better way to do this is to walk the heap
 * rather than scan the splay tree.
 */
static unsigned long nmallocs;
static unsigned long global_nbytes;

static FILE *dumpfp = NULL;

/* 
 *  Turns recording of FILE and LINE number of each call to
 *  malloc/free/realloc/calloc/cfree/memalign/valloc on (if value != 0)
 *  or off, (if value == 0)
 */
void
mal_leaktrace(value)
    int value;
{
  _malloc_leaktrace = (value != 0);
  if (sp == NULL)
    sp = __spinit();
}

/*
 *  The routine which actually does the printing. I know it is silly to
 *  print address in decimal, but sort doesn't read hex, so sorting the
 *  printed data by address is impossible otherwise. Urr. The format is
 *              FILE:LINE: sequence_number address_in_decimal (address_in_hex)
 */
void
__m_prnode(spblk)
    SPBLK *spblk;
{
  if ((unsigned long) spblk->datb < min_num)
    return;
  (void) sprintf(_malloc_statsbuf, "%s%8lu %8lu(0x%08lx)\n",
                 (char *) spblk->data, (unsigned long) spblk->datb,
                 (unsigned long) spblk->key, (unsigned long) spblk->key);
  (void) fputs(_malloc_statsbuf, dumpfp);
}

/*
 *  Dumps all blocks which have been recorded.
 */
void
mal_dumpleaktrace(fp)
    FILE *fp;
{
  dumpfp = fp;
  __spscan(__m_prnode, (SPBLK *) NULL, sp);
  (void) fflush(dumpfp);
  min_num = num;
}

/*
 *  Inserts a copy of a string keyed by the address addr into the tree
 *  that stores the leak trace information. The string is presumably of
 *  the form "file:linenumber:". It also stores a sequence number that
 *  gets incremented with each call to this routine.
 */
void
__m_install_record(addr, s)
    univptr_t addr;
    const char *s;
{
  num++;
  (void) __spadd(addr, strsave(s), (char *) num, sp);
}

/* Deletes the record keyed by addr if it exists */
void
__m_delete_record(addr)
    univptr_t addr;
{
  SPBLK *result;

  if ((result = __splookup(addr, sp)) != NULL) {
    free(result->data);
    result->data = 0;
    __spdelete(result, sp);
  }
}

static void __m_count _((SPBLK * spblk));

static void
__m_count(spblk)
    SPBLK *spblk;
{
  Word *p;

  nmallocs++;
  p = (Word *) spblk->key;
  p -= HEADERWORDS;

  CHECKALLOCPTR(p, "when checking in __m_count");
  global_nbytes += SIZE(p) * sizeof(Word);
  return;
}

void
mal_contents(fp)
    FILE *fp;
{
  nmallocs = 0;
  global_nbytes = 0;
  __spscan(__m_count, (SPBLK *) NULL, sp);
  (void) sprintf(_malloc_statsbuf,
                 "%% %lu bytes %lu mallocs %p vm\n",
                 global_nbytes, nmallocs, sbrk(0));
  (void) fputs(_malloc_statsbuf, fp);
  (void) fflush(fp);
}

/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY */


/*
 * GETBIN, UNLINK, LINK and CARVE are free-list maintenance macros used in
 * several places.  A free-list is a doubly-linked list of non-contiguous
 * blocks, marked by boundary tags that indicate the size.
 */

/* GETBIN returns a number such that i <= _malloc_binmax[bin] */
#define GETBIN(i)   \
        (((i) <= _malloc_binmax[3]) ? \
                (((i) <= _malloc_binmax[1]) ? \
                        (((i) <= _malloc_binmax[0]) ? 0 : 1) \
                : \
                        (((i) <= _malloc_binmax[2]) ? 2 : 3) \
                ) \
        : \
                (((i) <= _malloc_binmax[5]) ? \
                        (((i) <= _malloc_binmax[4]) ? 4 : 5) \
                : \
                        (((i) <= _malloc_binmax[6]) ? 6 : 7) \
                ) \
        )

/* UNLINK removes the block 'ep' from the free list 'epbin' */
#define UNLINK(ep, epbin) \
        { \
                REGISTER Word *epnext = NEXT(ep); \
                if (ep == epnext) { \
                        _malloc_rovers[epbin] = NULL; \
                        if (_malloc_firstbin == epbin) \
                                while (! _malloc_rovers[_malloc_firstbin] && \
                                       _malloc_firstbin < MAXBINS-1) \
                                        _malloc_firstbin++; \
                } else { \
                        REGISTER Word *epprev = PREV(ep); \
                        NEXT(epprev) = epnext; \
                        PREV(epnext) = epprev; \
                        if (ep == _malloc_rovers[epbin]) \
                                _malloc_rovers[epbin] = epprev; \
                } \
        }

/*
 * LINK adds the block 'ep' (psize words) to the free list 'epbin',
 * immediately after the block pointed to by that bin's rover.
 */
#define LINK(ep, epsize, epbin) \
        { \
                REGISTER Word *epprev; \
                REGISTER Word *eprover = _malloc_rovers[epbin]; \
                \
                if (eprover == NULL) { \
                        _malloc_rovers[epbin] = eprover = epprev = ep; \
                        if (_malloc_firstbin > epbin) \
                                _malloc_firstbin = epbin; \
                } else { \
                        CHECKFREEPTR(eprover, "while checking rover"); \
                        epprev = PREV(eprover); \
                } \
                NEXT(ep) = eprover; \
                PREV(eprover) = ep; \
                NEXT(epprev) = ep; \
                PREV(ep) = epprev; /* PREV(eprover) */\
                SIZEFIELD(ep) = SIZEFIELD(ep-epsize+1) = FREEMASK(epsize); \
        }

#define CARVE(ep, epsize, epbin, reqsize) \
         { \
                REGISTER size_t eprest = epsize - reqsize; \
                int newepbin; \
                \
                if (eprest >= _malloc_minchunk) { \
                        newepbin = (int)GETBIN(eprest); \
                        if (newepbin != epbin) { \
                                UNLINK(ep, epbin); \
                                LINK(ep, eprest, newepbin); \
                        } else { \
                                SIZEFIELD(ep) = SIZEFIELD(ep-eprest+1) \
                                        = FREEMASK(eprest); \
                        } \
                } else { \
                        /* alloc the entire block */ \
                        UNLINK(ep, epbin); \
                        reqsize = epsize; \
                } \
         }

static int grabhunk _((size_t));

static int
grabhunk(nwords)
    size_t nwords;
{
  univptr_t cp;
  size_t morecore;
  Word *ptr;
  size_t sbrkwords;
  size_t blksize;
  static char *spare;
  static int nspare;

  /* 
   *  two words for fake boundary tags for the entire block, and one
   *  for the next ptr of the block.
   */
#define EXCESS 3
  sbrkwords = (size_t) (((nwords + EXCESS) / _malloc_sbrkunits + 1) *
                        _malloc_sbrkunits);
  morecore = sbrkwords * sizeof(Word) + SBRKEXTRA;
  if ((cp = (*_malloc_memfunc) (morecore)) == (univptr_t) -1)
    return (0);
  /* 
   * Should first GUARANTEE that what sbrk returns is aligned to
   * Word boundaries - see align.h. Unfortunately, to guarantee
   * that the pointer returned by sbrk is aligned on a word
   * boundary, we must ask for sizeof(Word) -1 extra bytes, since
   * we have no guarantee what other sbrk'ed blocks exist. (Sun
   * sbrk always returns an aligned value, that is another story!)
   * We use spare and nspare to keep track of the bytes wasted, so
   * that we can try and reuse them later. If no other sbrk()s are
   * called, then nspare rotates through the values 3, 2, 1, 0,
   * and the first branch of the if() is always taken.
   */
  if ((spare + nspare) == (char *) cp) {
    ptr = (Word *) SBRKALIGN(spare);
    morecore += nspare;
    sbrkwords = morecore / sizeof(Word);
  } else {
    ptr = (Word *) SBRKALIGN(cp);
    morecore -= (char *) ptr - (char *) cp;
  }
  spare = (char *) (ptr + sbrkwords);
  nspare = (morecore - sbrkwords * sizeof(Word));
  PRCSRI_TRACE(sprintf(_malloc_statsbuf, "sbrk %lu\n",
                       (ulong) sbrkwords * sizeof(Word)));

  /*
   * If the new chunk adjoins _malloc_hiword, then _malloc_hiword
   * need not be a fake boundary tag any longer, (its a real one) and
   * the higher end of the block we sbrk'ed is the fake tag.  So we
   * tag it appropriately, make the start of the block point to the
   * old _malloc_hiword, and free it.  If we aren't next to
   * _malloc_hiword, then someone else sbrk'ed in between, so we
   * can't coalesce over the boundary anyway, in which case we just
   * change _malloc_hiword to be in the new sbrk'ed block without
   * damaging the old one. And we free the block.
   */
  if (ptr != _malloc_hiword + 1 || _malloc_mem == NULL) {
    /* Non-contiguous sbrk'ed block, or first sbrk we've done. */
    /* 
     * First push this block on the stack of non-contiguous blocks
     * we've sbrked. !! For real paranoia, we'd also check
     * _malloc_mem...
     */
    REGISTER Word *tmp = _malloc_mem;

    _malloc_mem = ptr;
    ptr->next = tmp;
    ptr++;
    sbrkwords--;

    _malloc_hiword = ptr;
    if (_malloc_loword == NULL || _malloc_loword > ptr) {
      /* First time - set lower bound. */
      PRCSRI_TRACE(sprintf(_malloc_statsbuf, "heapstart 0x%lx\n", (ulong) ptr));
      _malloc_loword = ptr;
    }
    /*
     * Fake boundary tags to indicate the ends of an arena.
     * Since they are marked as allocated, no attempt will be
     * made to coalesce before or after them.
     */
    SIZEFIELD(ptr) = ALLOCED | sbrkwords;
    _malloc_hiword += sbrkwords - 1;
    PRCSRI_TRACE(sprintf(_malloc_statsbuf, "heapend 0x%lx\n",
                         (ulong) _malloc_hiword));
    SIZEFIELD(_malloc_hiword) = ALLOCED | sbrkwords;

    /* * Subtract 2 for the special arena end tags. */
    sbrkwords -= 2;
    ptr++;
    DMEMSET(ptr + FREEHEADERWORDS, sbrkwords - FREE_OVERHEAD);
    ptr = _malloc_hiword - 1;
    _malloc_lastbin = (int) GETBIN(sbrkwords);
    LINK(ptr, sbrkwords, _malloc_lastbin)
      _malloc_rovers[_malloc_lastbin] = ptr;
    while (_malloc_rovers[_malloc_firstbin] == NULL &&
           _malloc_firstbin < MAXBINS - 1)
      _malloc_firstbin++;
    return (1);
  }
  /*
   * If we get here, then the sbrked chunk is contiguous, so we fake
   * up the boundary tags and size to look like an allocated block
   * and then call free()
   */
  ptr--;
  blksize = SIZE(ptr) + sbrkwords;
  SIZEFIELD(ptr) = ALLOCMASK(sbrkwords);
  _malloc_hiword += sbrkwords;
  SIZEFIELD(_malloc_hiword - 1) = SIZEFIELD(ptr);
  /* Update special arena end tags of the memory chunk */
  SIZEFIELD(_malloc_hiword) = ALLOCMASK(blksize);
  SIZEFIELD(_malloc_hiword - blksize + 1) = ALLOCMASK(blksize);
  SET_REALSIZE(ptr, (sbrkwords - ALLOC_OVERHEAD) * sizeof(Word));
  free((univptr_t) (ptr + HEADERWORDS));
  return (1);
}

univptr_t
malloc(nbytes)
    size_t nbytes;
{
  REGISTER Word *start, *search = NULL;
  REGISTER Word *p;
  REGISTER size_t required;
  REGISTER size_t searchsize;
  int bin;

#ifdef SVID_MALLOC_0
  /* SVID requires that malloc(0) return NULL, ick! */
  if (nbytes == 0) {
    errno = EINVAL;
    return (NULL);
  }
#endif                          /* SVID_MALLOC_0 */

  required = ALLOC_OVERHEAD + (nbytes + sizeof(Word) - 1) / sizeof(Word);
  if (required < (size_t) _malloc_minchunk)
    required = _malloc_minchunk;

  searchsize = 0;
  bin = (int) GETBIN(required);
  if (bin < _malloc_firstbin)
    bin = _malloc_firstbin;
  /* typically, we expect to execute this loop only once */
  while (searchsize < required && bin < MAXBINS) {
    if ((search = _malloc_rovers[bin++]) == NULL) {
      continue;
    }
    if (search == _malloc_hiword - 1) {
      /* avoid final "wilderness" block */
      CHECKFREEPTR(search, "while checking \"wilderness\" in malloc()");
      search = NEXT(search);
    }
    start = search;
    do {
      CHECKFREEPTR(search, "while searching in malloc()");
      searchsize = FREESIZE(search);
      if (searchsize >= required) {
        break;
      } else {
        search = NEXT(search);
      }
    } while (search != start);
  }

  if (searchsize < required) {
    if (grabhunk(required) == 0) {
      errno = ENOMEM;
      return (NULL);
    }
    /*
     * We made sure in grabhunk() or free() that
     * _malloc_rovers[lastbin] is pointing to the newly sbrked
     * (and freed) block.
     */
    bin = _malloc_lastbin;
    search = _malloc_rovers[bin];
    searchsize = FREESIZE(search);
  } else if (bin > 0) {
    bin--;
  }
  CARVE(search, searchsize, bin, required);
  p = search - searchsize + 1;
  SIZEFIELD(p) = SIZEFIELD(p + required - 1) = ALLOCMASK(required);
  PRCSRI_TRACE(sprintf
               (_malloc_statsbuf, "+ %lu %lu 0x%lx\n", (ulong) nbytes,
                (ulong) (required - ALLOC_OVERHEAD) * sizeof(Word),
                (ulong) (p + HEADERWORDS)));
  COUNTSIZE(required);
  SET_REALSIZE(p, nbytes);
  return ((univptr_t) (p + HEADERWORDS));
}



Free_t
free(cp)
    univptr_t
     cp;
{
  /* 
   * This is where the boundary tags come into their own. The
   * boundary tag guarantees a constant time insert with full
   * coalescing (the time varies slightly for the four case possible,
   * but still, essentially a very fast free.
   */
  /*
   *  P0 is the block being freed. P1 is the pointer to the block
   *  before the block being freed, and P2 is the block after it.
   *  We can either coalesce with P1, P2, both, or neither
   */
  REGISTER Word *p0, *p1, *p2;
  REGISTER size_t sizep0;
  int bin, oldbin = -1;

  if (cp == NULL)
#ifdef INT_FREE
    return 1;
#else
    return;
#endif

  p0 = (Word *) cp;
  p0 -= HEADERWORDS;

  CHECKALLOCPTR(p0, "passed to free()");
  /* With debugging, the CHECKALLOCPTR would have already aborted */
  if (TAG(p0) == FREE) {
    errno = EINVAL;
#ifdef INT_FREE
    return 0;
#else
    return;
#endif
  }
  /*
   * clear the entire block that used to be p0's, just in case
   * someone tries to refer to it or anything in it again.  We leave
   * the end tags alone for now - we'll smash them individually
   * depending on the way p0 merges with p1 and/or p2.
   */
  sizep0 = SIZE(p0);
  DMEMSET(p0 + FREEHEADERWORDS, sizep0 - FREE_OVERHEAD);
  PRCSRI_TRACE(sprintf(_malloc_statsbuf, "- %lu 0x%lx\n",
                       (ulong) (sizep0 - ALLOC_OVERHEAD) * sizeof(Word),
                       (ulong) (p0 + HEADERWORDS)));

  p1 = p0 - 1;
  /*
   * p0 now points to the end of the block -- we start treating it as
   * a free block
   */
  p0 += sizep0 - 1;
  p2 = p0 + 1;

  /*
   * We can't match the SIZEFIELDs of p1/p2 with p1/p2 + SIZE(p1/p2)
   * -1 because they might be a fake tag to indicate the bounds of
   * the arena. Further, we should only check p1 if p0-1 is not the
   * _malloc_loword or an arena bound - else p1 is probably not a
   * valid pointer. If tag p0-1 is allocated, then it could be an
   * arena bound.
   */

  if (TAG(p2) == FREE) {
    /*
     * Aha - block p2 (physically after p0) is free.  Merging
     * p0 with p2 merely means increasing p2's size to
     * incorporate p0 - no other pointer shuffling needed.
     * We'll move it to the right free-list later, if necessary.
     */
    p2 += FREESIZE(p2) - 1;
    oldbin = (int) GETBIN(FREESIZE(p2));
    CHECKFREEPTR(p2, "while checking p2 in free()");
    sizep0 += FREESIZE(p2);
    SIZEFIELD(p2 - sizep0 + 1) = SIZEFIELD(p2) = FREEMASK(sizep0);
    /*  Smash p0's old end tag and p2's old start tag. */
    DMEMSET(p0 - FREETRAILERWORDS + 1, FREETRAILERWORDS + FREEHEADERWORDS);
    p0 = p2;                    /* p0 just vanished - became part of p2 */
  }
  if (TAG(p1) == FREE) {
    /*
     * The block p1 (physically precedes p0 in memory) is free.
     * We grow p0 backward to absorb p1 and delete p1 from its
     * free list, since it no longer exists.
     */
    CHECKFREEPTR(p1, "while checking p1 in free()");
    sizep0 += FREESIZE(p1);
    bin = (int) GETBIN(FREESIZE(p1));
    UNLINK(p1, bin);
    SIZEFIELD(p0 - sizep0 + 1) = SIZEFIELD(p0) = FREEMASK(sizep0);
    /*
     * smash the free list pointers in p1 (SIZE, NEXT, PREV) to
     * make sure no one refers to them again. We cannot smash
     * the start boundary tag because it becomes the start tag
     * for the new block.  Also trash p0's start tag.
     */
    DMEMSET(p1 - FREETRAILERWORDS + 1, FREETRAILERWORDS + FREEHEADERWORDS);
  }
  bin = (int) GETBIN(sizep0);
  if (oldbin != bin) {
    /*
     * If we're here, it means block P0 needs to be inserted in
     * the correct free list, either because it didn't merge
     * with anything, or because it merged with p1 so we
     * deleted p1, or it merged with p2 and grew out p2's
     * existing free-list.
     */
    if (oldbin >= 0) {
      /* merged with P2, still in P2's free-list */
      UNLINK(p0, oldbin);
    }
    LINK(p0, sizep0, bin);
    _malloc_lastbin = bin;
    _malloc_rovers[bin] = p0;
  }
  CHECKHEAP();
#ifdef INT_FREE
  return 0;
#else
  return;
#endif
}


/*
 *  WARNING: This realloc() IS *NOT* upwards compatible with the
 *  convention that the last freed block since the last malloc may be
 *  realloced. Allegedly, this was because the old free() didn't
 *  coalesce blocks, and reallocing a freed block would perform the
 *  compaction. Yuk!
 */
univptr_t
realloc(cp, nbytes)
    univptr_t
     cp;
    size_t nbytes;
{
  REGISTER Word *p0 = (Word *) cp;
  REGISTER Word *p1;
  univptr_t tmp;
  REGISTER size_t required;
  REGISTER size_t sizep0;
  int bin;

  if (p0 == NULL)
    return (malloc(nbytes));

  if (nbytes == 0) {
    free(cp);
    return (NULL);
  }
  required = ALLOC_OVERHEAD + (nbytes + sizeof(Word) - 1) / sizeof(Word);
  if (required < (size_t) _malloc_minchunk)
    required = _malloc_minchunk;

  p0 -= HEADERWORDS;

  CHECKALLOCPTR(p0, "passed to realloc()");
  /* With debugging, the CHECKALLOCPTR would have already aborted */
  if (TAG(p0) == FREE) {
    errno = EINVAL;
    return (NULL);
  }
  sizep0 = SIZE(p0);
  if (sizep0 >= required) {
    /* Shrinking the block */
    size_t after = sizep0 - required;

    SET_REALSIZE(p0, nbytes);
    if (after < _malloc_minchunk) {
      /*
       * Not enough to free what's left so we return the
       * block intact - print no-op for neatness in
       * trace output.
       */
      PRCSRI_TRACE(strcpy(_malloc_statsbuf, "no-op\n"));
      return (cp);
    }
    SIZEFIELD(p0 + required - 1) = SIZEFIELD(p0) = ALLOCMASK(required);
    p0 += required;
    /*
     * We free what's after the block - mark it alloced and
     * throw it to free() to figure out whether to merge it
     * with what follows...
     */
    SIZEFIELD(p0 + after - 1) = SIZEFIELD(p0) = ALLOCMASK(after);
    SET_REALSIZE(p0, (after - ALLOC_OVERHEAD) * sizeof(Word));
    free((univptr_t) (p0 + HEADERWORDS));
    return (cp);
  }
  /*
   * If we get here, then we are growing the block p0 to something
   * bigger.
   */
  p1 = p0 + sizep0;
  required -= sizep0;
  if (TAG(p1) != FREE || FREESIZE(p1) < required) {
    /* Have to do it the hard way: block after us cannot be used */
    tmp = malloc(nbytes);
    if (tmp != NULL) {
      MEMCPY(tmp, cp, ((SIZE(p0) - ALLOC_OVERHEAD)));
      free(cp);
    }
    return (tmp);
  }
  /*
   * block after us is free and big enough to provide the required
   * space, so we grow into that block.
   */
  p1 += FREESIZE(p1) - 1;
  CHECKFREEPTR(p1, "while checking p1 in realloc()");
  bin = (int) GETBIN(FREESIZE(p1));
  CARVE(p1, FREESIZE(p1), bin, required);
  sizep0 += required;
  SIZEFIELD(p0) = SIZEFIELD(p0 + sizep0 - 1) = ALLOCMASK(sizep0);
  SET_REALSIZE(p0, nbytes);
  PRCSRI_TRACE(sprintf(_malloc_statsbuf, "++ %lu %lu 0x%lx\n",
                       (ulong) nbytes,
                       (ulong) (sizep0 - ALLOC_OVERHEAD) * sizeof(Word),
                       (ulong) cp));
  CHECKHEAP();
  return (cp);
}



/*
 *  !! Given what we know about alignment, we should be able to do better
 *  than memset and set words. Hopefully memset has been tuned.
 */
univptr_t
calloc(nelem, elsize)
    size_t nelem, elsize;
{
  REGISTER size_t nbytes = nelem * elsize;
  REGISTER univptr_t cp = malloc(nbytes);

  if (cp)
    (void) memset((univptr_t) cp, 0, (memsize_t) nbytes);
  return (cp);
}


/*
 *  Why would anyone want this.... ?
 */
Free_t
cfree(cp)
    univptr_t
     cp;
{
#ifdef INT_FREE
  return free(cp);
#else
  free(cp);
  return;
#endif
}

/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY */



/* 
 * !! memalign may leave small (< _malloc_minchunk) blocks as garbage.
 * Not worth fixing now -- I've only seen two applications call valloc()
 * or memalign(), and they do it only once in their life.
 */
/* 
 * This is needed to be compatible with Sun mallocs - Dunno how many
 * programs need it - the X server sure does... Returns a block 'size'
 * bytes long, such that the address is a multiple of 'alignment'.
 * (alignment MUST be a power of 2). This routine is possibly more
 * convoluted than free() - certainly uglier. Since it is rarely called
 * - possibly once in a program, it should be ok.  Since this is called
 * from valloc() which is usually needed in conjunction with
 * mmap()/munmap(), note the comment in the Sun manual page about
 * freeing segments of size 128K and greater. Ugh.
 */
univptr_t
memalign(alignment, size)
    size_t alignment, size;
{
  univptr_t cp;
  univptr_t addr;
  REGISTER Word *p0, *p1;
  REGISTER size_t before, after;
  size_t blksize;
#ifdef CSRI_DEBUG
  int tmp_debugging = _malloc_debugging;
#endif                          /* CSRI_DEBUG */

  if (alignment < sizeof(int) || !is_power_of_2(alignment) || size == 0) {
    errno = EINVAL;
    return (NULL);
  }
  if (alignment < sizeof(Word))
    return (malloc(size));      /* We guarantee this alignment anyway */
  /*
   * Life starts to get complicated - need to get a block large
   * enough to hold a block 'size' long, starting on an 'alignment'
   * boundary
   */
  if ((cp = malloc((size_t) (size + alignment - 1))) == NULL)
    return (NULL);
  addr = SIMPLEALIGN(cp, alignment);
  /*
   * This is all we really need - can go back now, except that we
   * might be wasting 'alignment - 1' bytes, which can be large since
   * this junk is usually called to align with things like pagesize.
   * So we try to push any free space before 'addr' and after 'addr +
   * size' back on the free list by making the memaligned chunk
   * ('addr' to 'addr + size') a block, and then doing stuff with the
   * space left over - either making them free blocks or coalescing
   * them whichever way is simplest. This usually involves making
   * them look like allocated blocks and calling free() which has all
   * the code to deal with this, and should do it reasonably fast.
   */
  p0 = (Word *) cp;
  p0 -= HEADERWORDS;
  /*
   * p0 now points to the word tag starting the block which we got
   * from malloc. This remains invariant from now on - p1 is our
   * temporary pointer
   */
  p1 = (Word *) addr;
  p1 -= HEADERWORDS;
  blksize = (size + sizeof(Word) - 1) / sizeof(Word);
  before = p1 - p0;
  after = SIZE(p0) - ALLOC_OVERHEAD - blksize - before;
  /*
   *  p1 now points to the word before addr - this is going to be the
   *  start of the memaligned block
   */
  if (after < _malloc_minchunk) {
    /*
     * We merge the extra space after the memaligned block into
     * it since that space isn't enough for a separate block.
     * Note that if the block after the one that malloc
     * returned is free, we might be able to merge the space
     * into that block even if it is too small - unfortunately,
     * free() won't accept a block of this size, and I don't
     * want to do that code here, so we'll just let it go to
     * waste in the memaligned block. !! fix later, maybe
     */
    blksize += after;
    after = 0;
  }
  /*
   * We mark the newly carved memaligned block p1 as alloced. addr is
   * (p1 + 1) which is the address we'll return
   */
  SIZEFIELD(p1) = ALLOCMASK(blksize + ALLOC_OVERHEAD);
  SIZEFIELD(p1 + blksize + ALLOC_OVERHEAD - 1) = SIZEFIELD(p1);
  SET_REALSIZE(p1, size);
  if (after > 0) {
    /* We can now free the block after the memaligned block. */
    p1 += blksize + ALLOC_OVERHEAD;     /* SIZE(p1) */
    /*
     * p1 now points to the space after the memaligned block. we
     * fix the size, mark it alloced, and call free - the block
     * after this may be free, which isn't simple to coalesce - let
     * free() do it.
     */
    SIZEFIELD(p1) = ALLOCMASK(after);
    SIZEFIELD(p1 + after - 1) = SIZEFIELD(p1);
    SET_REALSIZE(p1, (after - ALLOC_OVERHEAD) * sizeof(Word));
#ifdef CSRI_DEBUG
    /*
     * Full heap checking will break till we finish memalign
     * because the tags aren't all correct yet, but we still
     * call free().
     */
    _malloc_debugging = 0;
#endif                          /* CSRI_DEBUG */
    free((univptr_t) (p1 + HEADERWORDS));
  }
  if (addr != cp) {
    /*
     *  If what's 'before' is large enough to be freed, add p0 to
     *  free list after changing its size to just consist of the
     *  space before the memaligned block, also setting the
     *  alloced flag. Then call free() -- may merge with preceding
     *  block. (block after it is the memaligned block)
     */
    /*
     * Else the space before the block is too small to form a
     * free block, and the preceding block isn't free, so we
     * aren't touching it. Theoretically, we could put it in
     * the preceding alloc'ed block, but there are painful
     * complications if this is the start of the arena. We
     * pass, but MUST mark it as allocated. This sort of garbage
     * can split up the arena -- fix later with special case
     * maybe?!!
     */
    p1 = p0;
    SIZEFIELD(p1) = ALLOCMASK(before);
    SIZEFIELD(p1 + before - 1) = SIZEFIELD(p1);
    if (!TOO_SMALL(p1)) {
      SET_REALSIZE(p1, (before - ALLOC_OVERHEAD) * sizeof(Word));
    }
    if (before >= FREE_OVERHEAD) {
      free(cp);
    }
  }
#ifdef CSRI_DEBUG
  _malloc_debugging = tmp_debugging;
#endif                          /* CSRI_DEBUG */
  return (addr);
}

/* Just following the Sun manual page here */
univptr_t
valloc(size)
    size_t size;
{
  static size_t pagesz = 0;

  if (pagesz == 0)
    pagesz = (size_t) getpagesize();
  return (memalign(pagesz, size));
}

/* Set various malloc options */
/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY */


/* 
 *  Sets debugging level - level 0 and 1 both perform normal checking -
 *  making sure a pointer is valid before it is used for any heap data,
 *  and doing consistency checking on any block it touches while it
 *  works. Level 2 asks for a mal_verify() on every malloc(), free() or
 *  realloc(), thus checking the entire heap's pointers for consistency.
 *  Level 3 makes mal_verify() check that all free blocks contain a
 *  magic pattern that is put into a free block when it is freed.
 */
#ifdef CSRI_DEBUG
void
mal_debug(level)
    int level;
{
  if (level < 0 || level > 3) {
    return;
  }
  _malloc_debugging = level;
}
#endif                          /* CSRI_DEBUG */

/*
 *  Allows you to control the number of system calls made, which might
 *  be helpful in a program allocating a lot of memory - call this once
 *  to set a number big enough to contain all the allocations. Or for
 *  very little allocation, so that you don't get a huge space just
 *  because you alloc'e a couple of strings
 */
void
mal_sbrkset(n)
    int n;
{
  if (n < (int) (_malloc_minchunk * sizeof(Word))) {
    /* sbrk'ing anything less than a Word isn't a great idea. */
    return;
  }
  _malloc_sbrkunits = (n + sizeof(Word) - 1) / sizeof(Word);
  return;
}

/* 
 *  Since the minimum size block allocated is sizeof(Word)*_malloc_minchunk,
 *  adjusting _malloc_minchunk is one way to control
 *  memory fragmentation, and if you do a lot of mallocs and frees of
 *  objects that have a similar size, then a good way to speed things up
 *  is to set _malloc_minchunk such that the minimum size block covers
 *  most of the objects you allocate
 */
void
mal_slopset(n)
    int n;
{
  if (n < 0) {
    return;
  }
  _malloc_minchunk = (n + sizeof(Word) - 1) / sizeof(Word) + FIXEDOVERHEAD;
  return;
}

/*
 *  Turns tracing on (if value != 0) or off, (if value == 0)
 */
void
mal_trace(value)
    int value;
{
  if (value) {
    /* 
     *  Write something to the stats file so stdio can initialize
     *  its buffers i.e. call malloc() at least once while tracing
     *  is off, if the unbuffering failed.
     */
    (void) fputs("Malloc tracing starting\n", stderr);
    _malloc_tracing = 1;
    if (_malloc_loword != NULL) {
      /*
       * malloc happened before tracing turned on, so make
       * sure we print the heap start for xmem analysis.
       */
      PRCSRI_TRACE(sprintf(_malloc_statsbuf, "heapstart 0x%lx\n",
                           (ulong) _malloc_loword));
    }
  } else {
    /* For symmetry */
    (void) fputs("Malloc tracing stopped\n", stderr);
    _malloc_tracing = 0;
  }
  (void) fflush(stderr);
}

/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY */

/*
 *  Dumps the distribution of allocated sizes we've gathered so far
 */
#ifdef CSRI_PROFILESIZES
void
mal_statsdump(fp)
    FILE *fp;
{
  int i;
  char buf[128];

  for (i = 1; i < MAXPROFILESIZE; i++) {
    if (_malloc_scount[i] > 0) {
      (void) sprintf(buf, "%lu: %lu\n", (ulong) i * sizeof(Word),
                     (ulong) _malloc_scount[i]);
      (void) fputs(buf, fp);
      _malloc_scount[i] = 0;
    }
  }
  if (_malloc_scount[0] > 0) {
    (void) sprintf(buf, ">= %lu: %lu\n",
                   (ulong) MAXPROFILESIZE * sizeof(Word),
                   (ulong) _malloc_scount[0]);
    (void) fputs(buf, fp);
    _malloc_scount[0] = 0;
  }
  (void) fflush(fp);
}
#endif                          /* CSRI_PROFILESIZES */

/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY */






/* 
 *  makes a copy of a null terminated string in malloc'ed storage. Dies
 *  if enough memory isn't available or there is a malloc error
 */
char *
strsave(s)
    const char *s;
{
  if (s)
    return (strcpy((char *) emalloc((size_t) (strlen(s) + 1)), s));
  else
    return ((char *) NULL);
}

/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY */


/*
 *  Goes through the entire heap checking all pointers, tags for
 *  consistency. Should catch most casual heap corruption (overwriting
 *  the end of a malloc'ed chunk, etc..) Nonetheless, heap corrupters
 *  tend to be devious and ingenious in ways they corrupt heaps (Believe
 *  me, I know:-). We should probably do the same thing if CSRI_DEBUG is not
 *  defined, but return 0 instead of aborting. If fullcheck is non-zero,
 *  it also checks that free blocks contain the magic pattern written
 *  into them when they were freed to make sure the program is not still
 *  trying to access those blocks.
 */
#ifdef CSRI_DEBUG
int
mal_verify(fullcheck)
    int fullcheck;
{
  REGISTER Word *ptr, *p, *blk, *blkend;
  int i;

  if (_malloc_loword == NULL)   /* Nothing malloc'ed yet */
    return (0);

  for (i = 0; i < MAXBINS; i++) {
    ptr = _malloc_rovers[i];
    if (ptr != NULL) {
      ASSERT(i >= _malloc_firstbin, "firstbin too high");
      CHECKFREEPTR(ptr, "when verifying ROVER pointer");
    }
  }
  ASSERT(_malloc_rovers[MAXBINS] == NULL, "ROVER terminator not NULL");
  for (ptr = _malloc_mem; ptr != NULL; ptr = ptr->next) {
    /*
     *  Check arena bounds - not same as checking block tags,
     *  despite similar appearance of the test
     */
    ASSERT(SIZEFIELD(ptr + 1) == SIZEFIELD(ptr + SIZE(ptr + 1)),
           "corrupt malloc arena");
    blkend = ptr + SIZE(ptr + 1);
    for (blk = ptr + ARENASTART; blk < blkend; blk += SIZE(blk)) {
      ASSERT_SP(PTR_IN_HEAP(blk), "corrupt pointer", "", blk);
      ASSERT_SP(VALID_START_SIZE_FIELD(blk), "corrupt SIZE field", "", blk);
      if (TAG(blk) == FREE) {
        p = blk + FREESIZE(blk) - 1;
        ASSERT_EP(VALID_NEXT_PTR(p), "corrupt NEXT pointer", "", p);
        ASSERT_EP(VALID_PREV_PTR(p), "corrupt PREV pointer", "", p);
        if (fullcheck) {
          /* Make sure all free blocks are filled with FREEMAGIC */
          int n;
          char *cp;

          n = (SIZE(blk) - FREE_OVERHEAD) * sizeof(Word);
          cp = (char *) (blk + FREEHEADERWORDS);
          for (i = 0; i < n; i++, cp++) {
            ASSERT_SP(*cp == FREEMAGIC,
                      "free block modified after free()", "", blk);
          }
        }
      } else {
        ASSERT_SP(VALID_MAGIC(blk), "overwritten end of block", "", blk);
      }
    }
  }
  return (0);
}
#endif                          /* CSRI_DEBUG */

/*
 *  This file contains a few splay tree routines snarfed from David
 *  Brower's package, with globals renamed to keep them internal to the
 *  malloc, and not clash with similar routines that the application may
 *  use. The comments have been left with the original names - most of
 *  the renaming just involved prepending an __ before the name -
 *  spinstall got remapped to __spadd. Function prototypes added for
 *  external declarations. - Mark Moraes.
 */
/*
 * spdaveb.c -- daveb's new splay tree functions.
 *
 * The functions in this file provide an interface that is nearly
 * the same as the hash library I swiped from mkmf, allowing
 * replacement of one by the other.  Hey, it worked for me!
 *
 * splookup() -- given a key, find a node in a tree.
 * spinstall() -- install an item in the tree, overwriting existing value.
 * spfhead() -- fast (non-splay) find the first node in a tree.
 * spscan() -- forward scan tree from the head.
 * spfnext() -- non-splaying next.
 * spstats() -- make char string of stats for a tree.
 *
 * Written by David Brower, daveb@rtech.uucp 1/88. (now daveb@acm.org)
 */
/*LINTLIBRARY */

/* we keep a private free list of these blocks. */
#define MAXFREESPBLKS       1024

#if defined(__STDC__) && defined(ANSI_TYPES)
#endif


#define COMPARE(a,b)    (((char *) (a)) - ((char *) (b)))


/* insert item into the tree */
static SPBLK *spenq proto((SPBLK *, SPTREE *));
/* return and remove lowest item in subtree */
static SPBLK *spdeq proto((SPBLK **));
/* reorganize tree */
static void splay proto((SPBLK *, SPTREE *));
/* fast non-splaying head */
static SPBLK *spfhead proto((SPTREE *));
/* fast non-splaying next */
static SPBLK *spfnext proto((SPBLK *));

/* USER SUPPLIED! */

extern univptr_t emalloc proto((size_t));

#if 0
static SPBLK *
spmalloc(q)
    REGISTER SPTREE *q;
{
  REGISTER SPBLK *p = q->qfree;
  REGISTER int i;

  if (p != NULL) {
    q->qfree = p->leftlink;
    p->leftlink = 0;
    q->nfree--;
    return p;
  }
  return (SPBLK *) emalloc(sizeof(SPBLK));
}

static void
spfree(n, q)
    REGISTER SPBLK *n;
    REGISTER SPTREE *q;
{
  if (q->nfree > MAXFREESPBLKS) {
    (void) free((univptr_t) n);
  } else {
    n->leftlink = q->qfree;
    q->qfree = n;
    q->nfree++;
    n->rightlink = n->uplink = 0;
    n->key = n->data = n->datb = 0;
  }
}
#else
#define spmalloc(q) ((SPBLK *) emalloc(sizeof(SPBLK)))
#define spfree(n,q) ((void) free((univptr_t)n))
#endif


/*----------------
 *
 * splookup() -- given key, find a node in a tree.
 *
 *      Splays the found node to the root.
 */
SPBLK *
__splookup(key, q)
    REGISTER univptr_t key;
    REGISTER SPTREE *q;

{
  REGISTER SPBLK *n;
  REGISTER int Sct;
  REGISTER int c;

  /* find node in the tree */
  n = q->root;
  c = ++(q->lkpcmps);
  q->lookups++;
  while (n && (Sct = COMPARE(key, n->key))) {
    c++;
    n = (Sct < 0) ? n->leftlink : n->rightlink;
  }
  q->lkpcmps = c;

  /* reorganize tree around this node */
  if (n != NULL)
    splay(n, q);

  return (n);
}



/*----------------
 *
 * spinstall() -- install an entry in a tree, overwriting any existing node.
 *
 *      If the node already exists, replace its contents.
 *      If it does not exist, then allocate a new node and fill it in.
 */

SPBLK *
__spadd(key, data, datb, q)
    REGISTER univptr_t key;
    REGISTER univptr_t data;
    REGISTER univptr_t datb;
    REGISTER SPTREE *q;

{
  REGISTER SPBLK *n;

  if (NULL == (n = __splookup(key, q))) {
    n = spmalloc(q);
    n->key = (univptr_t) key;
    n->leftlink = NULL;
    n->rightlink = NULL;
    n->uplink = NULL;
    (void) spenq(n, q);
  }
  n->data = data;
  n->datb = datb;

  return (n);
}




/*----------------
 *
 * spfhead() -- return the "lowest" element in the tree.
 *
 *      returns a reference to the head event in the event-set q.
 *      avoids splaying but just searches for and returns a pointer to
 *      the bottom of the left branch.
 */
static SPBLK *
spfhead(q)
    REGISTER SPTREE *q;

{
  REGISTER SPBLK *x;

  if (NULL != (x = q->root))
    while (x->leftlink != NULL)
      x = x->leftlink;

  return (x);

}                               /* spfhead */



/*----------------
 *
 * spscan() -- apply a function to nodes in ascending order.
 *
 *      if n is given, start at that node, otherwise start from
 *      the head.
 */

void
__spscan(f, n, q)
    REGISTER void (*f) (SPBLK *);
    REGISTER SPBLK *n;
    REGISTER SPTREE *q;
{
  REGISTER SPBLK *x;

  for (x = n != NULL ? n : spfhead(q); x != NULL; x = spfnext(x))
    (*f) (x);
}


/*----------------
 *
 * spfnext() -- fast return next higer item in the tree, or NULL.
 *
 *      return the successor of n in q, represented as a splay tree.
 *      This is a fast (on average) version that does not splay.
 */
static SPBLK *
spfnext(n)
    REGISTER SPBLK *n;

{
  REGISTER SPBLK *next;
  REGISTER SPBLK *x;

  /* a long version, avoids splaying for fast average,
   * poor amortized bound
   */

  if (n == NULL)
    return (n);

  x = n->rightlink;
  if (x != NULL) {
    while (x->leftlink != NULL)
      x = x->leftlink;
    next = x;
  } else {                      /* x == NULL */
    x = n->uplink;
    next = NULL;
    while (x != NULL) {
      if (x->leftlink == n) {
        next = x;
        x = NULL;
      } else {
        n = x;
        x = n->uplink;
      }
    }
  }

  return (next);

}                               /* spfnext */


char *
__spstats(q)
    SPTREE *q;
{
  static char buf[128];
  float llen;
  float elen;
  float sloops;

  if (q == NULL)
    return ((char *) "");

  llen = q->lookups ? (float) q->lkpcmps / q->lookups : 0;
  elen = q->enqs ? (float) q->enqcmps / q->enqs : 0;
  sloops = q->splays ? (float) q->splayloops / q->splays : 0;

  (void) sprintf(buf, "f(%d %4.2f) i(%d %4.2f) s(%d %4.2f)",
                 q->lookups, llen, q->enqs, elen, q->splays, sloops);

  return buf;
}

/*
   spaux.c:  This code implements the following operations on an event-set
   or priority-queue implemented using splay trees:

   spdelete( n, q )             n is removed from q.

   In the above, n and np are pointers to single items (type
   SPBLK *); q is an event-set (type SPTREE *),
   The type definitions for these are taken
   from file sptree.h.  All of these operations rest on basic
   splay tree operations from file sptree.c.

   The basic splay tree algorithms were originally presented in:

   Self Adjusting Binary Trees,
   by D. D. Sleator and R. E. Tarjan,
   Proc. ACM SIGACT Symposium on Theory
   of Computing (Boston, Apr 1983) 235-245.

   The operations in this package supplement the operations from
   file splay.h to provide support for operations typically needed
   on the pending event set in discrete event simulation.  See, for
   example,

   Introduction to Simula 67,
   by Gunther Lamprecht, Vieweg & Sohn, Braucschweig, Wiesbaden, 1981.
   (Chapter 14 contains the relevant discussion.)

   Simula Begin,
   by Graham M. Birtwistle, et al, Studentlitteratur, Lund, 1979.
   (Chapter 9 contains the relevant discussion.)

   Many of the routines in this package use the splay procedure,
   for bottom-up splaying of the queue.  Consequently, item n in
   delete and item np in all operations listed above must be in the
   event-set prior to the call or the results will be
   unpredictable (eg:  chaos will ensue).

   Note that, in all cases, these operations can be replaced with
   the corresponding operations formulated for a conventional
   lexicographically ordered tree.  The versions here all use the
   splay operation to ensure the amortized bounds; this usually
   leads to a very compact formulation of the operations
   themselves, but it may slow the average performance.

   Alternative versions based on simple binary tree operations are
   provided (commented out) for head, next, and prev, since these
   are frequently used to traverse the entire data structure, and
   the cost of traversal is independent of the shape of the
   structure, so the extra time taken by splay in this context is
   wasted.

   This code was written by:
   Douglas W. Jones with assistance from Srinivas R. Sataluri

   Translated to C by David Brower, daveb@rtech.uucp

   Thu Oct  6 12:11:33 PDT 1988 (daveb) Fixed spdeq, which was broken
   handling one-node trees.  I botched the pascal translation of
   a VAR parameter.  Changed interface, so callers must also be
   corrected to pass the node by address rather than value.
   Mon Apr  3 15:18:32 PDT 1989 (daveb)
   Apply fix supplied by Mark Moraes <moraes@csri.toronto.edu> to
   spdelete(), which dropped core when taking out the last element
   in a subtree -- that is, when the right subtree was empty and
   the leftlink was also null, it tried to take out the leftlink's
   uplink anyway.
 */
/*----------------
 *
 * spdelete() -- Delete node from a tree.
 *
 *      n is deleted from q; the resulting splay tree has been splayed
 *      around its new root, which is the successor of n
 *
 */
void
__spdelete(n, q)
    REGISTER SPBLK *n;
    REGISTER SPTREE *q;

{
  REGISTER SPBLK *x;

  splay(n, q);
  x = spdeq(&q->root->rightlink);
  if (x == NULL) {              /* empty right subtree */
    q->root = q->root->leftlink;
    if (q->root)
      q->root->uplink = NULL;
  } else {                      /* non-empty right subtree */
    x->uplink = NULL;
    x->leftlink = q->root->leftlink;
    x->rightlink = q->root->rightlink;
    if (x->leftlink != NULL)
      x->leftlink->uplink = x;
    if (x->rightlink != NULL)
      x->rightlink->uplink = x;
    q->root = x;
  }
  spfree(n, q);
}                               /* spdelete */


/*

 *  sptree.c:  The following code implements the basic operations on
 *  an event-set or priority-queue implemented using splay trees:
 *
 *  SPTREE *spinit( compare )   Make a new tree
 *  SPBLK *spenq( n, q )        Insert n in q after all equal keys.
 *  SPBLK *spdeq( np )          Return first key under *np, removing it.
 *  void splay( n, q )          n (already in q) becomes the root.
 *
 *  In the above, n points to an SPBLK type, while q points to an
 *  SPTREE.
 *
 *  The implementation used here is based on the implementation
 *  which was used in the tests of splay trees reported in:
 *
 *    An Empirical Comparison of Priority-Queue and Event-Set Implementations,
 *      by Douglas W. Jones, Comm. ACM 29, 4 (Apr. 1986) 300-311.
 *
 *  The changes made include the addition of the enqprior
 *  operation and the addition of up-links to allow for the splay
 *  operation.  The basic splay tree algorithms were originally
 *  presented in:
 *
 *      Self Adjusting Binary Trees,
 *              by D. D. Sleator and R. E. Tarjan,
 *                      Proc. ACM SIGACT Symposium on Theory
 *                      of Computing (Boston, Apr 1983) 235-245.
 *
 *  The enq and enqprior routines use variations on the
 *  top-down splay operation, while the splay routine is bottom-up.
 *  All are coded for speed.
 *
 *  Written by:
 *    Douglas W. Jones
 *
 *  Translated to C by:
 *    David Brower, daveb@rtech.uucp
 *
 * Thu Oct  6 12:11:33 PDT 1988 (daveb) Fixed spdeq, which was broken
 *      handling one-node trees.  I botched the pascal translation of
 *      a VAR parameter.
 */
/*----------------
 *
 * spinit() -- initialize an empty splay tree
 *
 */
SPTREE *
__spinit()
{
  REGISTER SPTREE *q;

  q = (SPTREE *) emalloc(sizeof(*q));

  q->lookups = 0;
  q->lkpcmps = 0;
  q->enqs = 0;
  q->enqcmps = 0;
  q->splays = 0;
  q->splayloops = 0;
  q->root = NULL;
  return (q);
}

/*----------------
 *
 *  spenq() -- insert item in a tree.
 *
 *  put n in q after all other nodes with the same key; when this is
 *  done, n will be the root of the splay tree representing q, all nodes
 *  in q with keys less than or equal to that of n will be in the
 *  left subtree, all with greater keys will be in the right subtree;
 *  the tree is split into these subtrees from the top down, with rotations
 *  performed along the way to shorten the left branch of the right subtree
 *  and the right branch of the left subtree
 */
static SPBLK *
spenq(n, q)
    REGISTER SPBLK *n;
    REGISTER SPTREE *q;
{
  REGISTER SPBLK *left;         /* the rightmost node in the left tree */
  REGISTER SPBLK *right;        /* the leftmost node in the right tree */
  REGISTER SPBLK *next;         /* the root of the unsplit part */
  REGISTER SPBLK *temp;

  REGISTER univptr_t key;

  q->enqs++;
  n->uplink = NULL;
  next = q->root;
  q->root = n;
  if (next == NULL) {           /* trivial enq */
    n->leftlink = NULL;
    n->rightlink = NULL;
  } else {                      /* difficult enq */
    key = n->key;
    left = n;
    right = n;

    /* n's left and right children will hold the right and left
       splayed trees resulting from splitting on n->key;
       note that the children will be reversed! */

    q->enqcmps++;
    if (COMPARE(next->key, key) > 0)
      goto two;

  one:                          /* assert next->key <= key */

    do {                        /* walk to the right in the left tree */
      temp = next->rightlink;
      if (temp == NULL) {
        left->rightlink = next;
        next->uplink = left;
        right->leftlink = NULL;
        goto done;              /* job done, entire tree split */
      }
      q->enqcmps++;
      if (COMPARE(temp->key, key) > 0) {
        left->rightlink = next;
        next->uplink = left;
        left = next;
        next = temp;
        goto two;               /* change sides */
      }
      next->rightlink = temp->leftlink;
      if (temp->leftlink != NULL)
        temp->leftlink->uplink = next;
      left->rightlink = temp;
      temp->uplink = left;
      temp->leftlink = next;
      next->uplink = temp;
      left = temp;
      next = temp->rightlink;
      if (next == NULL) {
        right->leftlink = NULL;
        goto done;              /* job done, entire tree split */
      }
      q->enqcmps++;

    } while (COMPARE(next->key, key) <= 0);     /* change sides */

  two:                          /* assert next->key > key */

    do {                        /* walk to the left in the right tree */
      temp = next->leftlink;
      if (temp == NULL) {
        right->leftlink = next;
        next->uplink = right;
        left->rightlink = NULL;
        goto done;              /* job done, entire tree split */
      }
      q->enqcmps++;
      if (COMPARE(temp->key, key) <= 0) {
        right->leftlink = next;
        next->uplink = right;
        right = next;
        next = temp;
        goto one;               /* change sides */
      }
      next->leftlink = temp->rightlink;
      if (temp->rightlink != NULL)
        temp->rightlink->uplink = next;
      right->leftlink = temp;
      temp->uplink = right;
      temp->rightlink = next;
      next->uplink = temp;
      right = temp;
      next = temp->leftlink;
      if (next == NULL) {
        left->rightlink = NULL;
        goto done;              /* job done, entire tree split */
      }
      q->enqcmps++;

    } while (COMPARE(next->key, key) > 0);      /* change sides */

    goto one;

  done:                 /* split is done, branches of n need reversal */

    temp = n->leftlink;
    n->leftlink = n->rightlink;
    n->rightlink = temp;
  }

  return (n);

}                               /* spenq */


/*----------------
 *
 *  spdeq() -- return and remove head node from a subtree.
 *
 *  remove and return the head node from the node set; this deletes
 *  (and returns) the leftmost node from q, replacing it with its right
 *  subtree (if there is one); on the way to the leftmost node, rotations
 *  are performed to shorten the left branch of the tree
 */
static SPBLK *
spdeq(np)
    SPBLK **np;                 /* pointer to a node pointer */

{
  REGISTER SPBLK *deq;          /* one to return */
  REGISTER SPBLK *next;         /* the next thing to deal with */
  REGISTER SPBLK *left;         /* the left child of next */
  REGISTER SPBLK *farleft;      /* the left child of left */
  REGISTER SPBLK *farfarleft;   /* the left child of farleft */

  if (np == NULL || *np == NULL) {
    deq = NULL;
  } else {
    next = *np;
    left = next->leftlink;
    if (left == NULL) {
      deq = next;
      *np = next->rightlink;

      if (*np != NULL)
        (*np)->uplink = NULL;

    } else
      for (;;) {                /* left is not null */
        /* next is not it, left is not NULL, might be it */
        farleft = left->leftlink;
        if (farleft == NULL) {
          deq = left;
          next->leftlink = left->rightlink;
          if (left->rightlink != NULL)
            left->rightlink->uplink = next;
          break;
        }
        /* next, left are not it, farleft is not NULL, might be it */
        farfarleft = farleft->leftlink;
        if (farfarleft == NULL) {
          deq = farleft;
          left->leftlink = farleft->rightlink;
          if (farleft->rightlink != NULL)
            farleft->rightlink->uplink = left;
          break;
        }
        /* next, left, farleft are not it, rotate */
        next->leftlink = farleft;
        farleft->uplink = next;
        left->leftlink = farleft->rightlink;
        if (farleft->rightlink != NULL)
          farleft->rightlink->uplink = left;
        farleft->rightlink = left;
        left->uplink = farleft;
        next = farleft;
        left = farfarleft;
      }
  }

  return (deq);

}                               /* spdeq */


/*----------------
 *
 *  splay() -- reorganize the tree.
 *
 *  the tree is reorganized so that n is the root of the
 *  splay tree representing q; results are unpredictable if n is not
 *  in q to start with; q is split from n up to the old root, with all
 *  nodes to the left of n ending up in the left subtree, and all nodes
 *  to the right of n ending up in the right subtree; the left branch of
 *  the right subtree and the right branch of the left subtree are
 *  shortened in the process
 *
 *  this code assumes that n is not NULL and is in q; it can sometimes
 *  detect n not in q and complain
 */

static void
splay(n, q)
    REGISTER SPBLK *n;
    SPTREE *q;

{
  REGISTER SPBLK *up;           /* points to the node being dealt with */
  REGISTER SPBLK *prev;         /* a descendent of up, already dealt with */
  REGISTER SPBLK *upup;         /* the parent of up */
  REGISTER SPBLK *upupup;       /* the grandparent of up */
  REGISTER SPBLK *left;         /* the top of left subtree being built */
  REGISTER SPBLK *right;        /* the top of right subtree being built */

  left = n->leftlink;
  right = n->rightlink;
  prev = n;
  up = prev->uplink;

  q->splays++;

  while (up != NULL) {
    q->splayloops++;

    /* walk up the tree towards the root, splaying all to the left of
       n into the left subtree, all to right into the right subtree */

    upup = up->uplink;
    if (up->leftlink == prev) { /* up is to the right of n */
      if (upup != NULL && upup->leftlink == up) {       /* rotate */
        upupup = upup->uplink;
        upup->leftlink = up->rightlink;
        if (upup->leftlink != NULL)
          upup->leftlink->uplink = upup;
        up->rightlink = upup;
        upup->uplink = up;
        if (upupup == NULL)
          q->root = up;
        else if (upupup->leftlink == upup)
          upupup->leftlink = up;
        else
          upupup->rightlink = up;
        up->uplink = upupup;
        upup = upupup;
      }
      up->leftlink = right;
      if (right != NULL)
        right->uplink = up;
      right = up;

    } else {                    /* up is to the left of n */
      if (upup != NULL && upup->rightlink == up) {      /* rotate */
        upupup = upup->uplink;
        upup->rightlink = up->leftlink;
        if (upup->rightlink != NULL)
          upup->rightlink->uplink = upup;
        up->leftlink = upup;
        upup->uplink = up;
        if (upupup == NULL)
          q->root = up;
        else if (upupup->rightlink == upup)
          upupup->rightlink = up;
        else
          upupup->leftlink = up;
        up->uplink = upupup;
        upup = upupup;
      }
      up->rightlink = left;
      if (left != NULL)
        left->uplink = up;
      left = up;
    }
    prev = up;
    up = upup;
  }

#ifdef SPLAYCSRI_DEBUG
  if (q->root != prev) {
/*      fprintf(stderr, " *** bug in splay: n not in q *** " ); */
    abort();
  }
#endif

  n->leftlink = left;
  n->rightlink = right;
  if (left != NULL)
    left->uplink = n;
  if (right != NULL)
    right->uplink = n;
  q->root = n;
  n->uplink = NULL;

}                               /* splay */
