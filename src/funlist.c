/**
 * \file funlist.c
 *
 * \brief List-handling functions for mushcode.
 *
 *
 */
#include "copyrite.h"

#include "config.h"
#include <string.h>
#include <ctype.h>
#include "ansi.h"
#include "conf.h"
#include "case.h"
#include "externs.h"
#include "parse.h"
#include "boolexp.h"
#include "function.h"
#include "mymalloc.h"
#include "pcre.h"
#include "match.h"
#include "attrib.h"
#include "dbdefs.h"
#include "flags.h"
#include "mushdb.h"
#include "lock.h"
#include "confmagic.h"

#define MAX_SORTSIZE (BUFFER_LEN / 2)  /**< Maximum number of elements to sort */

static char *next_token(char *str, char sep);
static char *autodetect_list(char **ptrs, int nptrs);
static char *get_list_type(char **args, int nargs,
			   int type_pos, char **ptrs, int nptrs);
static char *get_list_type_noauto(char **args, int nargs, int type_pos);
static int i_comp(const void *s1, const void *s2);
static int f_comp(const void *s1, const void *s2);
static int u_comp(const void *s1, const void *s2);
static int regrep_helper(dbref who, dbref what, dbref parent,
			 char const *name, ATTR *atr, void *args);
/** Type definition for a qsort comparison function */
typedef int (*comp_func) (const void *, const void *);
static void sane_qsort(void **array, int left, int right, comp_func compare);
enum itemfun_op { IF_DELETE, IF_REPLACE, IF_INSERT };
static void do_itemfuns(char *buff, char **bp, char *str, char *num,
			char *word, char *sep, enum itemfun_op flag);

char *iter_rep[MAX_ITERS];  /**< itext values */
int iter_place[MAX_ITERS];  /**< inum numbers */
int inum = 0;		    /**< iter depth */
int inum_limit = 0;	    /**< limit of iter depth */
int iter_break = 0; /**< iter break */
extern const unsigned char *tables;

static char *
next_token(char *str, char sep)
{
  /* move pointer to start of the next token */

  while (*str && (*str != sep))
    str++;
  if (!*str)
    return NULL;
  str++;
  if (sep == ' ') {
    while (*str == sep)
      str++;
  }
  return str;
}

/** Convert list to array.
 * Chops up a list of words into an array of words. The list is
 * destructively modified.
 * \param r pointer to array to store words.
 * \param max maximum number of words to split out.
 * \param list list of words as a string.
 * \param sep separator character between list items.
 * \return number of words split out.
 */
int
list2arr(char *r[], int max, char *list, char sep) {
  char *p, *lp;
  int i;
  int first;
  ansi_string *as;
  char *aptr;

  as = parse_ansi_string(list);
  aptr = as->text;

  aptr = trim_space_sep(aptr, sep);

  lp = list;
  p = split_token(&aptr, sep);
  first = 0;
  for (i = 0; p && (i < max); i++, p = split_token(&aptr, sep)) {
    r[i] = lp;
    safe_ansi_string(as, p - (as->text), strlen(p), list, &lp);
    *(lp++) = '\0';
  }
  free_ansi_string(as);
  return i;
}

/* convert a character into an array and acknowledge a seperator character that may be escaped */
/* String is not destruviely modified like list2arr does */
int elist2arr(char *r[], int max, char *list, char sep) {
   static char tbuf[BUFFER_LEN];
   char tbuf2[BUFFER_LEN];
   char *lp, *p, *bp, *cbufp;
   int i;

   memset(tbuf, '\0', BUFFER_LEN);
   cbufp = bp = tbuf;
   strcpy(tbuf2, list);
   lp = p = tbuf2;
   i = 0;

   /* Do first */
   while(p && *p && (i <= max)) {
     if(*p == '\\' && (p + 1)) {
       /* Remove This Char & let the next char in */
       *p++ = '\0';
       safe_str(lp, cbufp, &bp);
       lp = p;
     }
     if(*p == sep) {
       *p = '\0';
       if(lp != NULL) {
	 safe_str(lp, cbufp, &bp);
         *bp++ = '\0'; 
	 r[i++] = cbufp; 
	 cbufp = bp;
       } else r[i++] = NULL;
       lp = ++p;
       continue;
     }
     p++;
   }
   /* Insert our final one.. */

   if(lp != NULL) {
     safe_str(lp, cbufp, &bp);
     *bp = '\0';
     r[i] = cbufp;
   }

     
  return i+1;
}



/** Convert array to list.
 * Takes an array of words and concatenates them into a string,
 * using our safe string functions.
 * \param r pointer to array of words.
 * \param max maximum number of words to concatenate.
 * \param list string to fill with word list.
 * \param lp pointer into end of list.
 * \param sep string to use as separator between words.
 */
void
arr2list(char *r[], int max, char *list, char **lp, char *sep)
{
  int i;
  int seplen = 0;

  if (!max)
    return;

  if (sep && *sep)
    seplen = strlen(sep);

  safe_str(r[0], list, lp);
  for (i = 1; i < max; i++) {
    safe_strl(sep, seplen, list, lp);
    safe_str(r[i], list, lp);
  }
  **lp = '\0';
}

/* ARGSUSED */
FUNCTION(fun_munge)
{
  /* This is a function which takes three arguments. The first is
   * an obj-attr pair referencing a u-function to be called. The
   * other two arguments are lists. The first list is passed to the
   * u-function.  The second list is then rearranged to match the
   * order of the first list as returned from the u-function.
   * This rearranged list is returned by MUNGE.
   * A fourth argument (separator) is optional.
   */

  char list1[BUFFER_LEN], *lp, rlist[BUFFER_LEN], *rp;
  char **ptrs1, **ptrs2, **results;
  int i, j, nptrs1, nptrs2, nresults;
  dbref thing;
  ATTR *attrib;
  char sep, isep[2] = { '\0', '\0' }, *osep, osepd[2] = {
  '\0', '\0'};
  int first;
  char *uargs[2];

  if (!delim_check(buff, bp, nargs, args, 4, &sep))
    return;

  isep[0] = sep;
  if (nargs == 5)
    osep = args[4];
  else {
    osepd[0] = sep;
    osep = osepd;
  }

  /* find our object and attribute */
  parse_anon_attrib(executor, args[0], &thing, &attrib);
  if (!GoodObject(thing) || !attrib || !Can_Read_Attr(executor, thing, attrib)) {
    free_anon_attrib(attrib);
    return;
  }
  if (!CanEvalAttr(executor, thing, attrib)) {
    free_anon_attrib(attrib);
    return;
  }

  /* Copy the first list, since we need to pass it to two destructive
   * routines.
   */

  strcpy(list1, args[1]);

  /* Break up the two lists into their respective elements. */

  ptrs1 = (char **) mush_malloc(MAX_SORTSIZE * sizeof(char *), "ptrarray");
  ptrs2 = (char **) mush_malloc(MAX_SORTSIZE * sizeof(char *), "ptrarray");
  if (!ptrs1 || !ptrs2)
    mush_panic("Unable to allocate memory in fun_munge");
  nptrs1 = list2arr(ptrs1, MAX_SORTSIZE, args[1], sep);
  nptrs2 = list2arr(ptrs2, MAX_SORTSIZE, args[2], sep);

  if (nptrs1 != nptrs2) {
    safe_str(T("#-1 LISTS MUST BE OF EQUAL SIZE"), buff, bp);
    mush_free((Malloc_t) ptrs1, "ptrarray");
    mush_free((Malloc_t) ptrs2, "ptrarray");
    free_anon_attrib(attrib);
    return;
  }
  /* Call the user function */

  lp = list1;
  rp = rlist;
  uargs[0] = lp;
  uargs[1] = isep;
  do_userfn(rlist, &rp, thing, attrib, 2, uargs,
	    executor, caller, enactor, pe_info);
  *rp = '\0';

  /* Now that we have our result, put it back into array form. Search
   * through list1 until we find the element position, then copy the
   * corresponding element from list2.  Mark used elements with
   * NULL to handle duplicates
   */
  results = (char **) mush_malloc(MAX_SORTSIZE * sizeof(char *), "ptrarray");
  if (!results)
    mush_panic("Unable to allocate memory in fun_munge");
  nresults = list2arr(results, MAX_SORTSIZE, rlist, sep);

  first = 1;
  for (i = 0; i < nresults; i++) {
    for (j = 0; j < nptrs1; j++) {
      if (ptrs2[j] && !strcmp(results[i], ptrs1[j])) {
	if (first)
	  first = 0;
	else
	  safe_str(osep, buff, bp);
	safe_str(ptrs2[j], buff, bp);
	ptrs2[j] = NULL;
	break;
      }
    }
  }
  mush_free((Malloc_t) ptrs1, "ptrarray");
  mush_free((Malloc_t) ptrs2, "ptrarray");
  mush_free((Malloc_t) results, "ptrarray");
  free_anon_attrib(attrib);
}

/* ARGSUSED */
FUNCTION(fun_elements)
{
  /* Given a list and a list of numbers, return the corresponding
   * elements of the list. elements(ack bar eep foof yay,2 4) = bar foof
   * A separator for the first list is allowed.
   * This code modified slightly from the Tiny 2.2.1 distribution
   */
  int nwords, cur;
  char **ptrs;
  char *wordlist;
  char *s, *r, sep;
  char *osep, osepd[2] = { '\0', '\0' };

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  if (nargs == 4)
    osep = args[3];
  else {
    osepd[0] = sep;
    osep = osepd;
  }

  ptrs = (char **) mush_malloc(MAX_SORTSIZE * sizeof(char *), "ptrarray");
  wordlist = (char *) mush_malloc(BUFFER_LEN, "string");
  if (!ptrs || !wordlist)
    mush_panic("Unable to allocate memory in fun_elements");

  /* Turn the first list into an array. */
  strcpy(wordlist, args[0]);
  nwords = list2arr(ptrs, MAX_SORTSIZE, wordlist, sep);

  s = trim_space_sep(args[1], ' ');

  /* Go through the second list, grabbing the numbers and finding the
   * corresponding elements.
   */
  r = split_token(&s, ' ');
  cur = atoi(r) - 1;
  if ((cur >= 0) && (cur < nwords) && ptrs[cur]) {
    safe_str(ptrs[cur], buff, bp);
  }
  while (s) {
    r = split_token(&s, ' ');
    cur = atoi(r) - 1;
    if ((cur >= 0) && (cur < nwords) && ptrs[cur]) {
      safe_str(osep, buff, bp);
      safe_str(ptrs[cur], buff, bp);
    }
  }
  mush_free((Malloc_t) ptrs, "ptrarray");
  mush_free((Malloc_t) wordlist, "string");
}

/* ARGSUSED */
FUNCTION(fun_matchall)
{
  /* Check each word individually, returning the word number of all
   * that match. If none match, return an empty string.
   */

  int wcount;
  char *r, *s, *b, sep;
  char *osep, osepd[2] = { '\0', '\0' };

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  if (nargs == 4)
    osep = args[3];
  else {
    osepd[0] = sep;
    osep = osepd;
  }

  wcount = 1;
  s = trim_space_sep(args[0], sep);
  b = *bp;
  do {
    r = split_token(&s, sep);
    if (quick_wild(args[1], r)) {
      if (*bp != b)
	safe_str(osep, buff, bp);
      safe_integer(wcount, buff, bp);
    }
    wcount++;
  } while (s);
}

/* ARGSUSED */
FUNCTION(fun_graball)
{
  /* Check each word individually, returning all that match.
   * If none match, return an empty string.  This is to grab()
   * what matchall() is to match().
   */

  char *r, *s, *b, sep;
  char *osep, osepd[2] = { '\0', '\0' };

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  if (nargs == 4)
    osep = args[3];
  else {
    osepd[0] = sep;
    osep = osepd;
  }

  s = trim_space_sep(args[0], sep);
  b = *bp;
  do {
    r = split_token(&s, sep);
    if (quick_wild(args[1], r)) {
      if (*bp != b)
	safe_str(osep, buff, bp);
      safe_str(r, buff, bp);
    }
  } while (s);
}



/* ARGSUSED */
FUNCTION(fun_fold)
{
  /* iteratively evaluates an attribute with a list of arguments and
   * optional base case. With no base case, the first list element is
   * passed as %0, and the second as %1. The attribute is then evaluated
   * with these args. The result is then used as %0, and the next arg as
   * %1. Repeat until no elements are left in the list. The base case 
   * can provide a starting point.
   */

  ufun_attrib ufun;
  char *cp;
  char *wenv[2];
  char sep;
  int funccount, per;
  char base[BUFFER_LEN];
  char result[BUFFER_LEN];

  if (!delim_check(buff, bp, nargs, args, 4, &sep))
    return;

  if (!fetch_ufun_attrib(args[0], executor, &ufun, 1))
    return;

  cp = args[1];

  /* If we have three or more arguments, the third one is the base case */
  if (nargs >= 3) {
    strncpy(base, args[2], BUFFER_LEN);
  } else {
    strncpy(base, split_token(&cp, sep), BUFFER_LEN);
  }
  wenv[0] = base;
  wenv[1] = split_token(&cp, sep);

  call_ufun(&ufun, wenv, 2, result, executor, enactor, pe_info);

  strncpy(base, result, BUFFER_LEN);

  funccount = pe_info->fun_invocations;

  /* handle the rest of the cases */
  while (cp && *cp) {
    wenv[1] = split_token(&cp, sep);
    per = call_ufun(&ufun, wenv, 2, result, executor, enactor, pe_info);
    if (per || (pe_info->fun_invocations >= FUNCTION_LIMIT &&
		pe_info->fun_invocations == funccount && !strcmp(base, result)))
      break;
    funccount = pe_info->fun_invocations;
    strcpy(base, result);
  }
  safe_str(base, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_itemize)
{
  /* Called in one of two ways:
   * itemize(<list>[,<delim>[,<conjunction>[,<punctuation>]]])
   * elist(<list>[,<conjunction> [,<delim> [,<output delim> [,<punctuation>]]]])
   * Either way, it takes the elements of list and:
   *  If there's just one, returns it.
   *  If there's two, returns <e1> <conjunction> <e2>
   *  If there's >2, returns <e1><punc> <e2><punc> ... <conjunction> <en>
   * Default <conjunction> is "and", default punctuation is ","
   */
  const char *outsep = " ";
  char sep = ' ';
  const char *lconj = "and";
  const char *punc = ",";
  char *cp;
  char *word, *nextword;
  int pos;

  if (strcmp(called_as, "ELIST") == 0) {
    /* elist ordering */
    if (!delim_check(buff, bp, nargs, args, 3, &sep))
      return;
    if (nargs > 1)
      lconj = args[1];
    if (nargs > 3)
      outsep = args[3];
    if (nargs > 4)
      punc = args[4];
  } else {
    /* itemize ordering */
    if (!delim_check(buff, bp, nargs, args, 2, &sep))
      return;
    if (nargs > 2)
      lconj = args[2];
    if (nargs > 3)
      punc = args[3];
  }
  cp = trim_space_sep(args[0], sep);
  pos = 1;
  word = split_token(&cp, sep);
  while (word) {
    nextword = split_token(&cp, sep);
    safe_itemizer(pos, !(nextword), punc, lconj, outsep, buff, bp);
    safe_str(word, buff, bp);
    pos++;
    word = nextword;
  }
}


/* ARGSUSED */
FUNCTION(fun_filter)
{
  /* take a user-def function and a list, and return only those elements
   * of the list for which the function evaluates to 1.
   */

  ufun_attrib ufun;
  char result[BUFFER_LEN];
  char *cp;
  char *wenv[1];
  char sep;
  int first;
  int check_bool = 0;
  int funccount;
  char *osep, osepd[2] = { '\0', '\0' };

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  osepd[0] = sep;
  osep = (nargs >= 4) ? args[3] : osepd;

  if (strcmp(called_as, "FILTERBOOL") == 0)
    check_bool = 1;

  /* find our object and attribute */
  if (!fetch_ufun_attrib(args[0], executor, &ufun, 1))
    return;

  /* Go through each argument */
  cp = trim_space_sep(args[1], sep);
  first = 1;
  funccount = pe_info->fun_invocations;
  while (cp && *cp) {
    wenv[0] = split_token(&cp, sep);
    if (call_ufun(&ufun, wenv, 1, result, executor, enactor, pe_info))
      break;
    if ((check_bool == 0)
	? (*result == '1' && *(result + 1) == '\0')
	: parse_boolean(result)) {
      if (first)
	first = 0;
      else
	safe_str(osep, buff, bp);
      safe_str(wenv[0], buff, bp);
    }
    /* Can't do *bp == oldbp like in all the others, because bp might not
     * move even when not full, if one of the list elements is null and
     * we have a null separator. */
    if (*bp == (buff + BUFFER_LEN - 1) && pe_info->fun_invocations == funccount)
      break;
    funccount = pe_info->fun_invocations;
  }
}

/* ARGSUSED */
FUNCTION(fun_shuffle)
{
  /* given a list of words, randomize the order of words. 
   * We do this by taking each element, and swapping it with another
   * element with a greater array index (thus, words[0] can be swapped
   * with anything up to words[n], words[5] with anything between
   * itself and words[n], etc.
   * This is relatively fast - linear time - and reasonably random.
   * Will take an optional delimiter argument.
   */

  char *words[BUFFER_LEN / 2];
  int n, i, j;
  char sep;
  char *osep, osepd[2] = { '\0', '\0' };

  if (!delim_check(buff, bp, nargs, args, 2, &sep))
    return;

  if (nargs == 3)
    osep = args[2];
  else {
    osepd[0] = sep;
    osep = osepd;
  }

  /* split the list up, or return if the list is empty */
  if (!*args[0])
    return;
  n = list2arr(words, BUFFER_LEN / 2, args[0], sep);

  /* shuffle it */
  for (i = 0; i < n; i++) {
    char *tmp;
    j = get_random_long(i, n - 1);
    tmp = words[j];
    words[j] = words[i];
    words[i] = tmp;
  }

  arr2list(words, n, buff, bp, osep);
}

typedef enum {
  L_NUMERIC,
  L_FLOAT,
  L_ALPHANUM,
  L_DBREF
} ltype;

static char *
autodetect_list(char *ptrs[], int nptrs)
{
  char * sort_type;
  ltype lt;
  int i;


  lt = L_NUMERIC;
  sort_type = NUMERIC_LIST;

  for (i = 0; i < nptrs; i++) {
    switch (lt) {
    case L_NUMERIC:
      if (!is_strict_integer(ptrs[i])) {
	/* If it's not an integer, see if it's a floating-point number */
	if (is_strict_number(ptrs[i])) {
	  lt = L_FLOAT;
	  sort_type = FLOAT_LIST;
	} else if (i == 0) {

	  /* If we get something non-numeric, switch to an
	   * alphanumeric guess, unless this is the first
	   * element and we have a dbref.
	   */
	  if (is_objid(ptrs[i])) {
	    lt = L_DBREF;
	    sort_type = DBREF_LIST;
	  } else
	    return ALPHANUM_LIST;
	}
      }
      break;
    case L_FLOAT:
      if (!is_strict_number(ptrs[i]))
	return ALPHANUM_LIST;
      break;
    case L_DBREF:
      if (!is_objid(ptrs[i]))
	return ALPHANUM_LIST;
      break;
    default:
      return ALPHANUM_LIST;
    }
  }
  return sort_type;
}


typedef struct sort_record s_rec;

typedef int (*qsort_func) (const void *, const void *);
typedef void (*makerecord) (s_rec *, dbref player, char *sortflags);

#define GENRECORD(x) void x(s_rec *rec,dbref player,char *sortflags); \
  void x(s_rec *rec, \
    dbref player __attribute__ ((__unused__)), \
    char *sortflags __attribute__ ((__unused__)))

/** Sorting strings by different values. We store both the string and
 * its 'key' to sort by. Sort of a hardcode munge.
 */
struct sort_record {
  char *ptr;	 /**< NULL except for sortkey */
  char *val;	 /**< The string this is */
  dbref db;	 /**< dbref (default 0, bad is -1) */
  char *str;	 /**< string comparisons */
  int num;	 /**< integer comparisons */
  NVAL numval;	 /**< float comparisons */
  int freestr;	 /**< free str on completion */
};

/* Compare(r,x,y) {
 *   if (x->db < 0 && y->db < 0)
 *     return 0;  // Garbage is identical.
 *   if (x->db < 0)
 *     return 2;  // Garbage goes last.
 *   if (y->db < 0)
 *     return -2; // Garbage goes last.
 *   if (r < 0)
 *     return -2; // different
 *   if (r > 0)
 *     return 2;  // different
 *   if (x->db < y->db)
 *     return -1; // similar
 *   if (y->db < x-db)
 *     return 1;  // similar
 *   return 0;    // identical
 * }
 */

/* If I could, I'd let sort() toss out non-existant dbrefs
 * Instead, sort stuffs them into a jumble at the end. */

#define Compare(r,x,y) \
        ((x->db < 0 || y->db < 0) ? \
           ((x->db < 0 && y->db < 0) ? 0 : (x->db < 0 ? 2 : -2)) \
           : ((r != 0) ? (r < 0 ? -2 : 2) \
             : (x->db == y->db ? 0 : (x->db < y->db ? -1 : 1)) \
           ) \
         )

static int
s_comp(const void *s1, const void *s2)
{
  const s_rec *sr1 = (const s_rec *) s1;
  const s_rec *sr2 = (const s_rec *) s2;
  int res = 0;
  res = strcoll(sr1->str, sr2->str);
  return Compare(res, sr1, sr2);
}

static int
si_comp(const void *s1, const void *s2)
{
  const s_rec *sr1 = (const s_rec *) s1;
  const s_rec *sr2 = (const s_rec *) s2;
  int res = 0;
  res = strcasecoll(sr1->str, sr2->str);
  return Compare(res, sr1, sr2);
}

static int
i_comp(const void *s1, const void *s2)
{
  const s_rec *sr1 = (const s_rec *) s1;
  const s_rec *sr2 = (const s_rec *) s2;
  int res = 0;
  res = sr1->num - sr2->num;
  return Compare(res, sr1, sr2);
}

static int
f_comp(const void *s1, const void *s2)
{
  const s_rec *sr1 = (const s_rec *) s1;
  const s_rec *sr2 = (const s_rec *) s2;
  NVAL res = 0;
  res = sr1->numval - sr2->numval;
  return Compare(res, sr1, sr2);
}

GENRECORD(gen_alphanum)
{
  size_t len;
  if (strchr(rec->val, ESC_CHAR)) {
    rec->str = mush_strdup(remove_markup(rec->val, &len), "genrecord");
    rec->freestr = 1;
  } else {
    rec->str = rec->val;
  }
}

GENRECORD(gen_dbref)
{
  rec->num = qparse_dbref(rec->val);
}

GENRECORD(gen_num)
{
  rec->num = parse_integer(rec->val);
}

GENRECORD(gen_float)
{
  rec->numval = parse_number(rec->val);
}

#define RealGoodObject(x) (GoodObject(x) && !IsGarbage(x))

GENRECORD(gen_db_name)
{
  rec->str = (char *) "";
  if (RealGoodObject(rec->db))
    rec->str = (char *) Name(rec->db);
}

GENRECORD(gen_db_idle)
{
  rec->num = -1;
  if (RealGoodObject(rec->db)) {
    if (Priv_Who(player))
      rec->num = least_idle_time_priv(rec->db);
    else
      rec->num = least_idle_time(rec->db);
  }
}

GENRECORD(gen_db_conn)
{
  rec->num = -1;
  if (RealGoodObject(rec->db)) {
    if (Priv_Who(player))
      rec->num = most_conn_time_priv(rec->db);
    else
      rec->num = most_conn_time(rec->db);
  }
}

GENRECORD(gen_db_ctime)
{
  if (RealGoodObject(rec->db))
    rec->num = CreTime(rec->db);
}

GENRECORD(gen_db_owner)
{
  if (RealGoodObject(rec->db))
    rec->num = Owner(rec->db);
}

GENRECORD(gen_db_loc)
{
  rec->num = -1;
  if (RealGoodObject(rec->db) && Can_Locate(player, rec->db)) {
    rec->num = Location(rec->db);
  }
}

GENRECORD(gen_db_attr)
{
  /* Eek, I hate dealing with memory issues. */

  static char *defstr = (char *) "";
  const char *ptr;

  rec->str = defstr;
  if (RealGoodObject(rec->db) && sortflags && *sortflags &&
      (ptr = do_get_attrib(player, rec->db, sortflags)) != NULL) {
    rec->str = mush_strdup(ptr, "genrecord");
    rec->freestr = 1;
  }
}

typedef struct _list_type_list_ {
  char *name;
  makerecord make_record;
  qsort_func sorter;
  int isdbs;
} list_type_list;

char ALPHANUM_LIST[] = "A";
char INSENS_ALPHANUM_LIST[] = "I";
char DBREF_LIST[] = "D";
char NUMERIC_LIST[] = "N";
char FLOAT_LIST[] = "F";
char DBREF_NAME_LIST[] = "NAME";
char DBREF_NAMEI_LIST[] = "NAMEI";
char DBREF_IDLE_LIST[] = "IDLE";
char DBREF_CONN_LIST[] = "CONN";
char DBREF_CTIME_LIST[] = "CTIME";
char DBREF_OWNER_LIST[] = "OWNER";
char DBREF_LOCATION_LIST[] = "LOC";
char DBREF_ATTR_LIST[] = "ATTR";
char DBREF_ATTRI_LIST[] = "ATTRI";
char *UNKNOWN_LIST = NULL;

list_type_list ltypelist[] = {
  /* List type name,            recordmaker,    comparer, dbrefs? */
  {ALPHANUM_LIST, gen_alphanum, s_comp, 0},
  {INSENS_ALPHANUM_LIST, gen_alphanum, si_comp, 0},
  {DBREF_LIST, gen_dbref, i_comp, 0},
  {NUMERIC_LIST, gen_num, i_comp, 0},
  {FLOAT_LIST, gen_float, f_comp, 0},
  {DBREF_NAME_LIST, gen_db_name, s_comp, 1},
  {DBREF_NAMEI_LIST, gen_db_name, si_comp, 1},
  {DBREF_IDLE_LIST, gen_db_idle, i_comp, 1},
  {DBREF_CONN_LIST, gen_db_conn, i_comp, 1},
  {DBREF_CTIME_LIST, gen_db_ctime, i_comp, 1},
  {DBREF_OWNER_LIST, gen_db_owner, i_comp, 1},
  {DBREF_LOCATION_LIST, gen_db_loc, i_comp, 1},
  {DBREF_ATTR_LIST, gen_db_attr, s_comp, 1},
  {DBREF_ATTRI_LIST, gen_db_attr, si_comp, 1},
  /* This stops the loop, so is default */
  {NULL, gen_alphanum, s_comp, 0}
};

char *
get_list_type(char *args[], int nargs, int type_pos, char *ptrs[], int nptrs)
{
  static char stype[BUFFER_LEN];
  int i;
  char *str;
  if (nargs >= type_pos) {
    str = args[type_pos - 1];
    if (*str) {
      strcpy(stype, str);
      str = strchr(stype, ':');
      if (str)
	*(str++) = '\0';
      for (i = 0; ltypelist[i].name && strcasecmp(ltypelist[i].name, stype);
	   i++) ;
      /* return ltypelist[i].name; */
      if (ltypelist[i].name) {
	return args[type_pos - 1];
      }
    }
  }
  return autodetect_list(ptrs, nptrs);
}

char *
get_list_type_noauto(char *args[], int nargs, int type_pos)
{
  static char stype[BUFFER_LEN];
  int i;
  char *str;
  if (nargs >= type_pos) {
    str = args[type_pos - 1];
    if (*str) {
      strcpy(stype, str);
      str = strchr(stype, ':');
      if (str)
	*(str++) = '\0';
      for (i = 0; ltypelist[i].name && strcasecmp(ltypelist[i].name, stype);
	   i++) ;
      /* return ltypelist[i].name; */
      return args[type_pos - 1];
    }
  }
  return UNKNOWN_LIST;
}


static dbref ucomp_executor, ucomp_caller, ucomp_enactor;
static char ucomp_buff[BUFFER_LEN];
static PE_Info *ucomp_pe_info;

static int
u_comp(const void *s1, const void *s2)
{
  char result[BUFFER_LEN], *rp;
  char const *tbuf;
  int n;

  /* Our two arguments are passed as %0 and %1 to the sortby u-function. */

  /* Note that this function is for use in conjunction with our own
   * sane_qsort routine, NOT with the standard library qsort!
   */
  global_eval_context.wenv[0] = (char *) s1;
  global_eval_context.wenv[1] = (char *) s2;

  /* Run the u-function, which should return a number. */

  tbuf = ucomp_buff;
  rp = result;
  if (process_expression(result, &rp, &tbuf,
			 ucomp_executor, ucomp_caller, ucomp_enactor,
			 PE_DEFAULT, PT_DEFAULT, ucomp_pe_info))
    return 0;
  n = parse_integer(result);

  return n;
}

/** A generic comparer routine to compare two values of any sort type.
 * \param
 */
int
gencomp(dbref player, char *a, char *b, char *sort_type)
{
  char *ptr;
  int i, len;
  int result;
  s_rec s1, s2;
  ptr = NULL;
  if (!sort_type) {
    /* Advance i to the default */
    for (i = 0; ltypelist[i].name; i++) ;
  } else if ((ptr = strchr(sort_type, ':'))) {
    len = ptr - sort_type;
    ptr += 1;
    if (!*ptr)
      ptr = NULL;
    for (i = 0;
	 ltypelist[i].name && strncasecmp(ltypelist[i].name, sort_type, len);
	 i++) ;
  } else {
    for (i = 0; ltypelist[i].name && strcasecmp(ltypelist[i].name, sort_type);
	 i++) ;
  }
  s1.freestr = s2.freestr = 0;
  if (ltypelist[i].isdbs) {
    s1.db = parse_dbref(a);
    s2.db = parse_dbref(b);
    if (!RealGoodObject(s1.db))
      s1.db = NOTHING;
    if (!RealGoodObject(s2.db))
      s2.db = NOTHING;
  } else {
    s1.db = s2.db = 0;
  }

  s1.val = a;
  s2.val = b;
  ltypelist[i].make_record(&s1, player, ptr);
  ltypelist[i].make_record(&s2, player, ptr);
  result = ltypelist[i].sorter((const void *) &s1, (const void *) &s2);
  if (s1.freestr)
    mush_free(s1.str, "genrecord");
  if (s2.freestr)
    mush_free(s2.str, "genrecord");
  return result;
}

/** A generic sort routine to sort several different
 * types of arrays, in place.
 * \param player the player executing the sort.
 * \param s the array to sort.
 * \param n number of elements in array s
 * \param sort_type the string that describes the sort type.
 */

void
do_gensort(dbref player, char *keys[], char *strs[], int n, char *sort_type)
{
  char *ptr;
  static char stype[BUFFER_LEN];
  int i, sorti;
  s_rec *sp;
  ptr = NULL;
  if (!sort_type || !*sort_type) {
    /* Advance sorti to the default */
    for (sorti = 0; ltypelist[sorti].name; sorti++) ;
  } else if (strchr(sort_type, ':') != NULL) {
    strcpy(stype, sort_type);
    ptr = strchr(stype, ':');
    *(ptr++) = '\0';
    if (!*ptr)
      ptr = NULL;
    for (sorti = 0;
	 ltypelist[sorti].name && strcasecmp(ltypelist[sorti].name, stype);
	 sorti++) ;
  } else {
    for (sorti = 0;
	 ltypelist[sorti].name && strcasecmp(ltypelist[sorti].name, sort_type);
	 sorti++) ;
  }
  sp = (s_rec *) mush_malloc(n * sizeof(s_rec), "do_gensort");
  for (i = 0; i < n; i++) {
    sp[i].val = keys[i];
    sp[i].freestr = 0;
    sp[i].db = 0;
    if (strs) {
      sp[i].ptr = strs[i];
    } else {
      sp[i].ptr = NULL;
    }
    sp[i].str = NULL;
    if (ltypelist[sorti].isdbs) {
      sp[i].db = parse_objid(keys[i]);
      if (!RealGoodObject(sp[i].db))
	sp[i].db = NOTHING;
    }
    ltypelist[sorti].make_record(&(sp[i]), player, ptr);
  }
  qsort((void *) sp, n, sizeof(s_rec), ltypelist[sorti].sorter);

  for (i = 0; i < n; i++) {
    keys[i] = sp[i].val;
    if (strs) {
      strs[i] = sp[i].ptr;
    }
    if (sp[i].freestr)
      mush_free(sp[i].str, "genrecord");
  }
  mush_free((Malloc_t) sp, "do_gensort");
}

/* ARGSUSED */
FUNCTION(fun_sort)
{
  char *ptrs[MAX_SORTSIZE];
  int nptrs;
  char *sort_type;
  char sep;
  char outsep[BUFFER_LEN];

  if (!nargs || !*args[0])
    return;

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  if (nargs < 4) {
    outsep[0] = sep;
    outsep[1] = '\0';
  } else
    strcpy(outsep, args[3]);

  nptrs = list2arr(ptrs, MAX_SORTSIZE, args[0], sep);
  sort_type = get_list_type(args, nargs, 2, ptrs, nptrs);
  do_gensort(executor, ptrs, NULL, nptrs, sort_type);
  arr2list(ptrs, nptrs, buff, bp, outsep);
}

static void
sane_qsort(void *array[], int left, int right, comp_func compare)
{
  /* Andrew Molitor's qsort, which doesn't require transitivity between
   * comparisons (essential for preventing crashes due to boneheads
   * who write comparison functions where a > b doesn't mean b < a).
   */
  /* Actually, this sort doesn't require commutivity.
   * Sorting doesn't make sense without transitivity...
   */

  int i, last;
  void *tmp;

loop:
  if (left >= right)
    return;

  /* Pick something at random at swap it into the leftmost slot   */
  /* This is the pivot, we'll put it back in the right spot later */

  i = get_random_long(left, right);
  tmp = array[i];
  array[i] = array[left];
  array[left] = tmp;

  last = left;
  for (i = left + 1; i <= right; i++) {

    /* Walk the array, looking for stuff that's less than our */
    /* pivot. If it is, swap it with the next thing along     */

    if (compare(array[i], array[left]) < 0) {
      last++;
      if (last == i)
	continue;

      tmp = array[last];
      array[last] = array[i];
      array[i] = tmp;
    }
  }

  /* Now we put the pivot back, it's now in the right spot, we never */
  /* need to look at it again, trust me.                             */

  tmp = array[last];
  array[last] = array[left];
  array[left] = tmp;

  /* At this point everything underneath the 'last' index is < the */
  /* entry at 'last' and everything above it is not < it.          */

  if ((last - left) < (right - last)) {
    sane_qsort(array, left, last - 1, compare);
    left = last + 1;
    goto loop;
  } else {
    sane_qsort(array, last + 1, right, compare);
    right = last - 1;
    goto loop;
  }
}

/* ARGSUSED */
FUNCTION(fun_sortkey)
{
  char *ptrs[MAX_SORTSIZE];
  char *keys[MAX_SORTSIZE];
  int nptrs;
  char *sort_type;
  char sep;
  char outsep[BUFFER_LEN];
  int i;
  char tbuff[BUFFER_LEN];
  char *tp;
  char const *cp;
  char result[BUFFER_LEN];
  char *rp;
  ATTR *attrib;
  dbref thing;

  /* sortkey(attr,list,sort_type,delim,osep) */

  if (!nargs || !*args[0] || !*args[1])
    return;

  if (!delim_check(buff, bp, nargs, args, 4, &sep))
    return;

  if (nargs < 5) {
    outsep[0] = sep;
    outsep[1] = '\0';
  } else
    strcpy(outsep, args[4]);

  /* Find object and attribute to get sortby function from. */
  parse_anon_attrib(executor, args[0], &thing, &attrib);
  if (!GoodObject(thing) || !attrib || !Can_Read_Attr(executor, thing, attrib)) { 
    free_anon_attrib(attrib);
    return;
  }
  if (!CanEvalAttr(executor, thing, attrib)) {
    free_anon_attrib(attrib);
    return;
  }
  tp = tbuff;
  safe_str(atr_value(attrib), tbuff, &tp);
  *tp = '\0';

  nptrs = list2arr(ptrs, MAX_SORTSIZE, args[1], sep);

  /* Now we make a list of keys */
  for (i = 0; i < nptrs; i++) {
    global_eval_context.wenv[0] = (char *) ptrs[i];
    rp = result;
    cp = tbuff;
    process_expression(result, &rp, &cp,
		       thing, executor, enactor,
		       PE_DEFAULT, PT_DEFAULT, pe_info);
    *rp = '\0';
    keys[i] = mush_strdup(result, "sortkey");
  }

  sort_type = get_list_type(args, nargs, 3, keys, nptrs);
  do_gensort(executor, keys, ptrs, nptrs, sort_type);
  arr2list(ptrs, nptrs, buff, bp, outsep);
  for (i = 0; i < nptrs; i++) {
    mush_free(keys[i], "sortkey");
  }
}

/* ARGSUSED */
FUNCTION(fun_sortby)
{
  char *ptrs[MAX_SORTSIZE], *tptr[10];
  char *up, sep;
  int nptrs;
  dbref thing;
  ATTR *attrib;
  char *osep, osepd[2] = { '\0', '\0' };

  if (!nargs || !*args[0])
    return;

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  if (nargs == 4)
    osep = args[3];
  else {
    osepd[0] = sep;
    osep = osepd;
  }

  /* Find object and attribute to get sortby function from. */
  parse_anon_attrib(executor, args[0], &thing, &attrib);
  if (!GoodObject(thing) || !attrib || !Can_Read_Attr(executor, thing, attrib)) {
    free_anon_attrib(attrib);
    return;
  }
  if (!CanEvalAttr(executor, thing, attrib)) {
    free_anon_attrib(attrib);
    return;
  }
  up = ucomp_buff;
  safe_str(atr_value(attrib), ucomp_buff, &up);
  *up = '\0';

  ucomp_executor = thing;
  ucomp_caller = executor;
  ucomp_enactor = enactor;
  ucomp_pe_info = pe_info;

  save_global_env("sortby", tptr);

  /* Split up the list, sort it, reconstruct it. */
  nptrs = list2arr(ptrs, MAX_SORTSIZE, args[1], sep);
  if (nptrs > 1)		/* pointless to sort less than 2 elements */
    sane_qsort((void *) ptrs, 0, nptrs - 1, u_comp);

  arr2list(ptrs, nptrs, buff, bp, osep);

  restore_global_env("sortby", tptr);
  free_anon_attrib(attrib);
}

/* ARGSUSED */
FUNCTION(fun_setinter)
{
  char sep;
  char **a1, **a2;
  int n1, n2, x1, x2, val;
  char *sort_type = ALPHANUM_LIST;
  int osepl = 0;
  char *osep = NULL, osepd[2] = { '\0', '\0' };

  /* if no lists, then no work */
  if (!*args[0] && !*args[1])
    return;

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  a1 = (char **) mush_malloc(MAX_SORTSIZE * sizeof(char *), "ptrarray");
  a2 = (char **) mush_malloc(MAX_SORTSIZE * sizeof(char *), "ptrarray");
  if (!a1 || !a2)
    mush_panic("Unable to allocate memory in fun_setinter");

  /* make arrays out of the lists */
  n1 = list2arr(a1, MAX_SORTSIZE, args[0], sep);
  n2 = list2arr(a2, MAX_SORTSIZE, args[1], sep);

  if (nargs < 4) {
    osepd[0] = sep;
    osep = osepd;
    if (sep)
      osepl = 1;
  } else if (nargs == 4) {
    sort_type = get_list_type_noauto(args, nargs, 4);
    if (sort_type == UNKNOWN_LIST) {
      sort_type = ALPHANUM_LIST;
      osep = args[3];
      osepl = arglens[3];
    } else {
      osepd[0] = sep;
      osep = osepd;
      if (sep)
	osepl = 1;
    }
  } else if (nargs == 5) {
    sort_type = get_list_type(args, nargs, 4, a1, n1);
    osep = args[4];
    osepl = arglens[4];
  }
  /* sort each array */
  do_gensort(executor, a1, NULL, n1, sort_type);
  do_gensort(executor, a2, NULL, n2, sort_type);

  /* get the first value for the intersection, removing duplicates */
  x1 = x2 = 0;
  while ((val = gencomp(executor, a1[x1], a2[x2], sort_type))) {
    if (val < 0) {
      x1++;
      if (x1 >= n1) {
	mush_free((Malloc_t) a1, "ptrarray");
	mush_free((Malloc_t) a2, "ptrarray");
	return;
      }
    } else {
      x2++;
      if (x2 >= n2) {
	mush_free((Malloc_t) a1, "ptrarray");
	mush_free((Malloc_t) a2, "ptrarray");
	return;
      }
    }
  }
  safe_str(a1[x1], buff, bp);
  while (!gencomp(executor, a1[x1], a2[x2], sort_type)) {
    x1++;
    if (x1 >= n1) {
      mush_free((Malloc_t) a1, "ptrarray");
      mush_free((Malloc_t) a2, "ptrarray");
      return;
    }
  }

  /* get values for the intersection, until at least one list is empty */
  while ((x1 < n1) && (x2 < n2)) {
    while ((val = gencomp(executor, a1[x1], a2[x2], sort_type))) {
      if (val < 0) {
	x1++;
	if (x1 >= n1) {
	  mush_free((Malloc_t) a1, "ptrarray");
	  mush_free((Malloc_t) a2, "ptrarray");
	  return;
	}
      } else {
	x2++;
	if (x2 >= n2) {
	  mush_free((Malloc_t) a1, "ptrarray");
	  mush_free((Malloc_t) a2, "ptrarray");
	  return;
	}
      }
    }
    safe_strl(osep, osepl, buff, bp);
    safe_str(a1[x1], buff, bp);
    while (!gencomp(executor, a1[x1], a2[x2], sort_type)) {
      x1++;
      if (x1 >= n1) {
	mush_free((Malloc_t) a1, "ptrarray");
	mush_free((Malloc_t) a2, "ptrarray");
	return;
      }
    }
  }
  mush_free((Malloc_t) a1, "ptrarray");
  mush_free((Malloc_t) a2, "ptrarray");
}

/* ARGSUSED */
FUNCTION(fun_setunion)
{
  char sep;
  char **a1, **a2;
  int n1, n2, x1, x2, val;
  int lastx1, lastx2, found;
  char *sort_type = ALPHANUM_LIST;
  int osepl = 0;
  char *osep = NULL, osepd[2] = { '\0', '\0' };

  /* if no lists, then no work */
  if (!*args[0] && !*args[1])
    return;

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  a1 = (char **) mush_malloc(MAX_SORTSIZE * sizeof(char *), "ptrarray");
  a2 = (char **) mush_malloc(MAX_SORTSIZE * sizeof(char *), "ptrarray");
  if (!a1 || !a2)
    mush_panic("Unable to allocate memory in fun_diff");

  /* make arrays out of the lists */
  n1 = list2arr(a1, MAX_SORTSIZE, args[0], sep);
  n2 = list2arr(a2, MAX_SORTSIZE, args[1], sep);

  if (nargs < 4) {
    osepd[0] = sep;
    osep = osepd;
    if (sep)
      osepl = 1;
  } else if (nargs == 4) {
    sort_type = get_list_type_noauto(args, nargs, 4);
    if (sort_type == UNKNOWN_LIST) {
      sort_type = ALPHANUM_LIST;
      osep = args[3];
      osepl = arglens[3];
    } else {
      osepd[0] = sep;
      osep = osepd;
      if (sep)
	osepl = 1;
    }
  } else if (nargs == 5) {
    sort_type = get_list_type(args, nargs, 4, a1, n1);
    osep = args[4];
    osepl = arglens[4];
  }
  /* sort each array */
  do_gensort(executor, a1, NULL, n1, sort_type);
  do_gensort(executor, a2, NULL, n2, sort_type);

  /* get values for the union, in order, skipping duplicates */
  lastx1 = lastx2 = -1;
  found = x1 = x2 = 0;
  if (n1 == 1 && !*a1[0])
    n1 = 0;
  if (n2 == 1 && !*a2[0])
    n2 = 0;
  while ((x1 < n1) || (x2 < n2)) {
    /* If we've already copied off something from a1, and our current
     * look at a1 is the same element, or we've copied from a2 and
     * our current look at a1 is the same element, skip forward in a1.
     */
    if (x1 < n1 && lastx1 >= 0) {
      val = gencomp(executor, a1[lastx1], a1[x1], sort_type);
      if (val == 0) {
	x1++;
	continue;
      }
    }
    if (x1 < n1 && lastx2 >= 0) {
      val = gencomp(executor, a2[lastx2], a1[x1], sort_type);
      if (val == 0) {
	x1++;
	continue;
      }
    }
    if (x2 < n2 && lastx1 >= 0) {
      val = gencomp(executor, a1[lastx1], a2[x2], sort_type);
      if (val == 0) {
	x2++;
	continue;
      }
    }
    if (x2 < n2 && lastx2 >= 0) {
      val = gencomp(executor, a2[lastx2], a2[x2], sort_type);
      if (val == 0) {
	x2++;
	continue;
      }
    }
    if (x1 >= n1) {
      /* Just copy off the rest of a2 */
      if (x2 < n2) {
	if (found)
	  safe_strl(osep, osepl, buff, bp);
	safe_str(a2[x2], buff, bp);
	lastx2 = x2;
	x2++;
	found = 1;
      }
    } else if (x2 >= n2) {
      /* Just copy off the rest of a1 */
      if (x1 < n1) {
	if (found)
	  safe_strl(osep, osepl, buff, bp);
	safe_str(a1[x1], buff, bp);
	lastx1 = x1;
	x1++;
	found = 1;
      }
    } else {
      /* At this point, we're merging. Take the lower of the two. */
      val = gencomp(executor, a1[x1], a2[x2], sort_type);
      if (val <= 0) {
	if (found)
	  safe_strl(osep, osepl, buff, bp);
	safe_str(a1[x1], buff, bp);
	lastx1 = x1;
	x1++;
	found = 1;
      } else {
	if (found)
	  safe_strl(osep, osepl, buff, bp);
	safe_str(a2[x2], buff, bp);
	lastx2 = x2;
	x2++;
	found = 1;
      }
    }
  }
  mush_free((Malloc_t) a1, "ptrarray");
  mush_free((Malloc_t) a2, "ptrarray");
}

/* ARGSUSED */
FUNCTION(fun_setdiff)
{
  char sep;
  char **a1, **a2;
  int n1, n2, x1, x2, val;
  char *sort_type = ALPHANUM_LIST;
  int osepl = 0;
  char *osep = NULL, osepd[2] = { '\0', '\0' };

  /* if no lists, then no work */
  if (!*args[0] && !*args[1])
    return;

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  a1 = (char **) mush_malloc(MAX_SORTSIZE * sizeof(char *), "ptrarray");
  a2 = (char **) mush_malloc(MAX_SORTSIZE * sizeof(char *), "ptrarray");
  if (!a1 || !a2)
    mush_panic("Unable to allocate memory in fun_diff");

  /* make arrays out of the lists */
  n1 = list2arr(a1, MAX_SORTSIZE, args[0], sep);
  n2 = list2arr(a2, MAX_SORTSIZE, args[1], sep);

  if (nargs < 4) {
    osepd[0] = sep;
    osep = osepd;
    if (sep)
      osepl = 1;
  } else if (nargs == 4) {
    sort_type = get_list_type_noauto(args, nargs, 4);
    if (sort_type == UNKNOWN_LIST) {
      sort_type = ALPHANUM_LIST;
      osep = args[3];
      osepl = arglens[3];
    } else {
      osepd[0] = sep;
      osep = osepd;
      if (sep)
	osepl = 1;
    }
  } else if (nargs == 5) {
    sort_type = get_list_type(args, nargs, 4, a1, n1);
    osep = args[4];
    osepl = arglens[4];
  }

  /* sort each array */
  do_gensort(executor, a1, NULL, n1, sort_type);
  do_gensort(executor, a2, NULL, n2, sort_type);

  /* get the first value for the difference, removing duplicates */
  x1 = x2 = 0;
  while ((val = gencomp(executor, a1[x1], a2[x2], sort_type)) >= 0) {
    if (val > 0) {
      x2++;
      if (x2 >= n2)
	break;
    }
    if (!val) {
      x1++;
      if (x1 >= n1) {
	mush_free((Malloc_t) a1, "ptrarray");
	mush_free((Malloc_t) a2, "ptrarray");
	return;
      }
    }
  }
  safe_str(a1[x1], buff, bp);
  do {
    x1++;
    if (x1 >= n1) {
      mush_free((Malloc_t) a1, "ptrarray");
      mush_free((Malloc_t) a2, "ptrarray");
      return;
    }
  } while (!gencomp(executor, a1[x1], a1[x1 - 1], sort_type));

  /* get values for the difference, until at least one list is empty */
  while (x2 < n2) {
    if ((val = gencomp(executor, a1[x1], a2[x2], sort_type)) < 0) {
      safe_strl(osep, osepl, buff, bp);
      safe_str(a1[x1], buff, bp);
    }
    if (val <= 0) {
      do {
	x1++;
	if (x1 >= n1) {
	  mush_free((Malloc_t) a1, "ptrarray");
	  mush_free((Malloc_t) a2, "ptrarray");
	  return;
	}
      } while (!gencomp(executor, a1[x1], a1[x1 - 1], sort_type));
    }
    if (val >= 0)
      x2++;
  }

  /* empty out remaining values, still removing duplicates */
  while (x1 < n1) {
    safe_strl(osep, osepl, buff, bp);
    safe_str(a1[x1], buff, bp);
    do {
      x1++;
    } while ((x1 < n1) && !gencomp(executor, a1[x1], a1[x1 - 1], sort_type));
  }
  mush_free((Malloc_t) a1, "ptrarray");
  mush_free((Malloc_t) a2, "ptrarray");
}

#define CACHE_SIZE 8  /**< Maximum size of the lnum cache */

FUNCTION(fun_unique)
{
  char sep;
  char **a1, **a2;
  int n1, x1, x2;
  char *sort_type = ALPHANUM_LIST;
  int osepl = 0;
  char *osep = NULL, osepd[2] = { '\0', '\0' };

  /* if no lists, then no work */
  if (!*args[0])
    return;

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  a1 = (char **) mush_malloc(MAX_SORTSIZE * sizeof(char *), "ptrarray");

  if (!a1)
    mush_panic("Unable to allocate memory in fun_unique");

  /* make array out of the list */
  n1 = list2arr(a1, MAX_SORTSIZE, args[0], sep);

  a2 = mush_malloc(n1 * sizeof(char *), "ptrarray");
  if (!a2)
    mush_panic("Unable to allocate memory in fun_unique");

  if (nargs >= 2)
    sort_type = get_list_type_noauto(args, nargs, 2);

  if (sort_type == UNKNOWN_LIST)
    sort_type = ALPHANUM_LIST;

  if (nargs < 4) {
    osepd[0] = sep;
    osep = osepd;
    if (sep)
      osepl = 1;
  } else if (nargs == 4) {
    osep = args[3];
    osepl = arglens[3];
  }


  a2[0] = a1[0];
  for (x1 = x2 = 1; x1 < n1; x1++) {
    if (gencomp(executor, a1[x1], a2[x2 - 1], sort_type) == 0)
      continue;
    a2[x2] = a1[x1];
    x2++;
  }

  for (x1 = 0; x1 < x2; x1++) {
    if (x1 > 0)
      safe_strl(osep, osepl, buff, bp);
    safe_str(a2[x1], buff, bp);
  }

  mush_free(a1, "ptrarray");
  mush_free(a2, "ptrarray");

}

/* ARGSUSED */
FUNCTION(fun_lnum)
{
  NVAL j;
  NVAL start;
  NVAL end;
  int istart, iend, k;
  char const *osep = " ";
  static NVAL cstart[CACHE_SIZE];
  static NVAL cend[CACHE_SIZE];
  static char csep[CACHE_SIZE][BUFFER_LEN];
  static char cresult[CACHE_SIZE][BUFFER_LEN];
  static int cpos;
  char *cp;

  if (!is_number(args[0])) {
    safe_str(T(e_num), buff, bp);
    return;
  }
  end = parse_number(args[0]);
  if (nargs > 1) {
    if (!is_number(args[1])) {
      safe_str(T(e_num), buff, bp);
      return;
    }
    start = end;
    end = parse_number(args[1]);
    if ((start == 0) && (end == 0)) {
      safe_str("0", buff, bp);	/* Special case - lnum(0,0) -> 0 */
      return;
    }
  } else {
    if (end == 0)
      return;			/* Special case - lnum(0) -> blank string */
    else if (end == 1) {
      safe_str("0", buff, bp);	/* Special case - lnum(1) -> 0 */
      return;
    }
    end--;
    if (end < 0) {
      safe_str(T("#-1 NUMBER OUT OF RANGE"), buff, bp);
      return;
    }
    start = 0;
  }
  if (nargs > 2) {
    osep = args[2];
  }
  for (k = 0; k < CACHE_SIZE; k++) {
    if (cstart[k] == start && cend[k] == end && !strcmp(csep[k], osep)) {
      safe_str(cresult[k], buff, bp);
      return;
    }
  }
  cpos = (cpos + 1) % CACHE_SIZE;
  cstart[cpos] = start;
  cend[cpos] = end;
  strcpy(csep[cpos], osep);
  cp = cresult[cpos];

  istart = (int) start;
  iend = (int) end;
  if (istart == start && iend == end) {
    safe_integer(istart, cresult[cpos], &cp);
    if (istart <= iend) {
      for (k = istart + 1; k <= iend; k++) {
	safe_str(osep, cresult[cpos], &cp);
	if (safe_integer(k, cresult[cpos], &cp))
	  break;
      }
    } else {
      for (k = istart - 1; k >= iend; k--) {
	safe_str(osep, cresult[cpos], &cp);
	if (safe_integer(k, cresult[cpos], &cp))
	  break;
      }
    }
  } else {
    safe_number(start, cresult[cpos], &cp);
    if (start <= end) {
      for (j = start + 1; j <= end; j++) {
	safe_str(osep, cresult[cpos], &cp);
	if (safe_number(j, cresult[cpos], &cp))
	  break;
      }
    } else {
      for (j = start - 1; j >= end; j--) {
	safe_str(osep, cresult[cpos], &cp);
	if (safe_number(j, cresult[cpos], &cp))
	  break;
      }
    }
  }
  *cp = '\0';

  safe_str(cresult[cpos], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_first)
{
  /* read first word from a string */

  char *p;
  char sep;

  if (!*args[0])
    return;

  if (!delim_check(buff, bp, nargs, args, 2, &sep))
    return;

  p = trim_space_sep(args[0], sep);
  safe_str(split_token(&p, sep), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_randword)
{
  char *s, *r;
  char sep;
  int word_count, word_index;

  if (!*args[0])
    return;

  if (!delim_check(buff, bp, nargs, args, 2, &sep))
    return;

  s = trim_space_sep(args[0], sep);
  word_count = do_wordcount(s, sep);
  word_index = get_random_long(0, word_count - 1);

  /* Go to the start of the token we're interested in. */
  while (word_index && s) {
    s = next_token(s, sep);
    word_index--;
  }

  if (!s || !*s)		/* ran off the end of the string */
    return;

  /* Chop off the end, and copy. No length checking needed. */
  r = s;
  if (s && *s)
    (void) split_token(&s, sep);
  safe_str(r, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_rest)
{
  char *p;
  char sep;

  if (!*args[0])
    return;

  if (!delim_check(buff, bp, nargs, args, 2, &sep))
    return;

  p = trim_space_sep(args[0], sep);
  (void) split_token(&p, sep);
  safe_str(p, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_last)
{
  /* read last word from a string */

  char *p, *r;
  char sep;

  if (!*args[0])
    return;

  if (!delim_check(buff, bp, nargs, args, 2, &sep))
    return;

  p = trim_space_sep(args[0], sep);
  if (!(r = strrchr(p, sep)))
    r = p;
  else
    r++;
  safe_str(r, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_grab)
{
  /* compares two strings with possible wildcards, returns the
   * word matched. Based on the 2.2 version of this function.
   */

  char *r, *s, sep;

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  /* Walk the wordstring, until we find the word we want. */
  s = trim_space_sep(args[0], sep);
  do {
    r = split_token(&s, sep);
    if (quick_wild(args[1], r)) {
      safe_str(r, buff, bp);
      return;
    }
  } while (s);
}

/* ARGSUSED */
FUNCTION(fun_namegraball)
{
  /* Given a list of dbrefs and a string, it matches the
   * name of the dbrefs against the string.
   * grabnameall(#1 #2 #3,god) -> #1
   */

  char *r, *s, sep;
  dbref victim;
  dbref absolute;
  int first = 1;

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  absolute = parse_dbref(args[1]);
  if (!RealGoodObject(absolute))
    absolute = NOTHING;

  if (*args[1]) {
    s = trim_space_sep(args[0], sep);
    do {
      r = split_token(&s, sep);
      victim = parse_dbref(r);
      if (!RealGoodObject(victim))
	continue;               /* Don't bother with garbage */
      if (!(string_match(Name(victim), args[1]) || (absolute == victim)))
	continue;
      if (!can_interact(victim, executor, INTERACT_MATCH))
	continue;
      /* It matches, and is interact-able */
      if (!first)
	safe_chr(sep, buff, bp);
      safe_str(r, buff, bp);
      first = 0;
    } while (s);
  } else {
    /* Pull out all good objects (those that _have_ names) */
    s = trim_space_sep(args[0], sep);
    do {
      r = split_token(&s, sep);
      victim = parse_dbref(r);
      if (!RealGoodObject(victim))
	continue;		/* Don't bother with garbage */
      if (!can_interact(victim, executor, INTERACT_MATCH))
	continue;
      /* It's real, and is interact-able */
      if (!first)
	safe_chr(sep, buff, bp);
      safe_str(r, buff, bp);
      first = 0;
    } while (s);
  }
}

/* ARGSUSED */
FUNCTION(fun_namegrab)
{
  /* Given a list of dbrefs and a string, it matches the
   * name of the dbrefs against the string.
   */

  char *r, *s, sep;
  dbref victim;
  dbref absolute;
  char *exact_res, *res;

  exact_res = res = NULL;

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  absolute = parse_dbref(args[1]);
  if (!RealGoodObject(absolute))
    absolute = NOTHING;

  /* Walk the wordstring, until we find the word we want. */
  s = trim_space_sep(args[0], sep);
  do {
    r = split_token(&s, sep);
    victim = parse_dbref(r);
    if (!RealGoodObject(victim))
      continue;                       /* Don't bother with garbage */
    /* Dbref match has top priority */
    if ((absolute == victim) && can_interact(victim, executor, INTERACT_MATCH)) {
      safe_str(r, buff, bp);
      return;
    }
    /* Exact match has second priority */
    if (!exact_res && !strcasecmp(Name(victim), args[1]) &&
      can_interact(victim, executor, INTERACT_MATCH)) {
      exact_res = r;
    }
    /* Non-exact match. */
    if (!res && string_match(Name(victim), args[1]) &&
      can_interact(victim, executor, INTERACT_MATCH)) {
      res = r;
    }
  } while (s);
  if (exact_res)
    safe_str(exact_res, buff, bp);
  else if (res)
    safe_str(res, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_match)
{
  /* compares two strings with possible wildcards, returns the
   * word position of the match. Based on the 2.0 version of this
   * function.
   */

  char *s, *r;
  char sep;
  int wcount = 1;

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  /* Walk the wordstring, until we find the word we want. */
  s = trim_space_sep(args[0], sep);
  do {
    r = split_token(&s, sep);
    if (quick_wild(args[1], r)) {
      safe_integer(wcount, buff, bp);
      return;
    }
    wcount++;
  } while (s);
  safe_chr('0', buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_wordpos)
{
  int charpos, i;
  char *cp, *tp, *xp;
  char sep;

  if (!is_integer(args[1])) {
    safe_str(T(e_int), buff, bp);
    return;
  }
  charpos = parse_integer(args[1]);
  cp = args[0];
  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  if ((charpos <= 0) || ((size_t) charpos > strlen(cp))) {
    safe_str("#-1", buff, bp);
    return;
  }
  tp = cp + charpos - 1;
  cp = trim_space_sep(cp, sep);
  xp = split_token(&cp, sep);
  for (i = 1; xp; i++) {
    if (tp < (xp + strlen(xp)))
      break;
    xp = split_token(&cp, sep);
  }
  safe_integer(i, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_extract)
{
  char sep = ' ';
  int start = 1, len = 1;
  char *s, *r;

  if (!is_integer(args[1]) || !is_integer(args[2])) {
    safe_str(T(e_ints), buff, bp);
    return;
  }
  s = args[0];

  if (nargs > 1) {
    if (!is_integer(args[1])) {
      safe_str(T(e_ints), buff, bp);
      return;
    }
    start = parse_integer(args[1]);
  }
  if (nargs > 2) {
    if (!is_integer(args[2])) {
      safe_str(T(e_ints), buff, bp);
      return;
    }
    len = parse_integer(args[2]);
  }
  if ((nargs > 3) && (!delim_check(buff, bp, nargs, args, 4, &sep)))
    return;

  if ((start < 1) || (len < 1))
    return;

  /* Go to the start of the token we're interested in. */
  start--;
  s = trim_space_sep(s, sep);
  while (start && s) {
    s = next_token(s, sep);
    start--;
  }

  if (!s || !*s)		/* ran off the end of the string */
    return;

  /* Find the end of the string that we want. */
  r = s;
  len--;
  while (len && s) {
    s = next_token(s, sep);
    len--;
  }

  /* Chop off the end, and copy. No length checking needed. */
  if (s && *s)
    (void) split_token(&s, sep);
  safe_str(r, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_cat)
{
  int i;

  safe_strl(args[0], arglens[0], buff, bp);
  for (i = 1; i < nargs; i++) {
    safe_chr(' ', buff, bp);
    safe_strl(args[i], arglens[i], buff, bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_remove)
{
  char sep;

  /* zap word from string */

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;
  if (strchr(args[1], sep)) {
    safe_str(T("#-1 CAN ONLY DELETE ONE ELEMENT"), buff, bp);
    return;
  }
  safe_str(remove_word(args[0], args[1], sep), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_items)
{
  /* the equivalent of WORDS for an arbitrary separator */
  /* This differs from WORDS in its treatment of the space
   * separator.
   */

  char *s = args[0];
  char c = *args[1];
  int count = 1;

  if (c == '\0')
    c = ' ';

  while ((s = strchr(s, c))) {
    count++;
    s++;
  }

  safe_integer(count, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_element)
{
  /* the equivalent of MEMBER for an arbitrary separator */
  /* This differs from MEMBER in its use of quick_wild()
   * instead of strcmp().
   */

  char *s, *t;
  char c;
  int el;

  c = *args[2];

  if (c == '\0')
    c = ' ';
  if (strchr(args[1], c)) {
    safe_str(T("#-1 CAN ONLY TEST ONE ELEMENT"), buff, bp);
    return;
  }
  s = args[0];
  el = 1;

  do {
    t = s;
    s = seek_char(t, c);
    if (*s)
      *s++ = '\0';
    if (quick_wild(args[1], t)) {
      safe_integer(el, buff, bp);
      return;
    }
    el++;
  } while (*s);

  safe_chr('0', buff, bp);	/* no match */
}

/* ARGSUSED */
FUNCTION(fun_index)
{
  /* more or less the equivalent of EXTRACT for an arbitrary separator */
  /* This differs from EXTRACT in its handling of space separators. */

  int start, end;
  char c;
  char *s, *p;

  if (!is_integer(args[2]) || !is_integer(args[3])) {
    safe_str(T(e_ints), buff, bp);
    return;
  }
  s = args[0];
  c = *args[1];
  if (!c)
    c = ' ';

  start = parse_integer(args[2]);
  end = parse_integer(args[3]);

  if ((start < 1) || (end < 1) || (*s == '\0'))
    return;

  /* move s to the start of the item we want */
  while (--start) {
    if (!(s = strchr(s, c)))
      return;
    s++;
  }

  /* skip just spaces, not tabs or newlines, since people may MUSHcode things
   * like "%r%tPolgara %r%tDurnik %r%tJavelin"
   */
  while (*s == ' ')
    s++;
  if (!*s)
    return;

  /* now figure out where to end the string */
  p = s + 1;
  /* we may already be pointing to a separator */
  if (*s == c)
    end--;
  while (end--)
    if (!(p = strchr(p, c)))
      break;
    else
      p++;

  if (p)
    p--;
  else
    p = s + strlen(s);

  /* trim trailing spaces (just true spaces) */
  while ((p > s) && (p[-1] == ' '))
    p--;
  *p = '\0';

  safe_str(s, buff, bp);
}

/** Functions that operate on items - delete, replace, insert.
 * \param buff return buffer.
 * \param bp pointer to insertion point in buff.
 * \param str original string.
 * \param num string containing the element number to operate on.
 * \param word string to insert/delete/replace.
 * \param sep separator string.
 * \param flag operation to perform: IF_DELETE, IF_REPLACE, IF_INSERT
 */
static void
do_itemfuns(char *buff, char **bp, char *str, char *num, char *word,
	    char *sep, enum itemfun_op flag)
{
  char c;
  int el, count, len = -1;
  char *sptr, *eptr;

  if (!is_integer(num)) {
    safe_str(T(e_int), buff, bp);
    return;
  }
  el = parse_integer(num);

  /* figure out the separator character */
  if (sep && *sep)
    c = *sep;
  else
    c = ' ';

  /* we can't remove anything before the first position */
  if ((el < 1 && flag != IF_INSERT) || el == 0) {
    safe_str(str, buff, bp);
    return;
  }
  if (el < 0) {
    sptr = str + strlen(str);
    eptr = sptr;
  } else {
    sptr = str;
    eptr = strchr(sptr, c);
  }
  count = 1;

  /* go to the correct item in the string */
  if (el < 0) {			/* if using insert() with a negative insertion param */
    /* count keeps track of the number of words from the right
     * of the string.  When count equals the correct position, then
     * sptr will point to the count'th word from the right, or
     * a null string if the  word being added will be at the end of
     * the string.
     * eptr is just a helper.  */
    for (len = strlen(str); len >= 0 && count < abs(el); len--, eptr--) {
      if (*eptr == c)
	count++;
      if (count == abs(el)) {
	sptr = eptr + 1;
	break;
      }
    }
  } else {
    /* Loop invariant: if sptr and eptr are not NULL, eptr points to
     * the count'th instance of c in str, and sptr is the beginning of
     * the count'th item. */
    while (eptr && (count < el)) {
      sptr = eptr + 1;
      eptr = strchr(sptr, c);
      count++;
    }
  }

  if ((!eptr || len < 0) && (count < abs(el))) {
    /* we've run off the end of the string without finding anything */
    safe_str(str, buff, bp);
    return;
  }
  /* now find the end of that element */
  if ((el < 0 && *eptr) || (el > 0 && sptr != str))
    sptr[-1] = '\0';

  switch (flag) {
  case IF_DELETE:
    /* deletion */
    if (!eptr) {		/* last element in the string */
      if (el != 1)
	safe_str(str, buff, bp);
    } else if (sptr == str) {	/* first element in the string */
      eptr++;			/* chop leading separator */
      safe_str(eptr, buff, bp);
    } else {
      safe_str(str, buff, bp);
      safe_str(eptr, buff, bp);
    }
    break;
  case IF_REPLACE:
    /* replacing */
    if (!eptr) {		/* last element in string */
      if (el != 1) {
	safe_str(str, buff, bp);
	safe_chr(c, buff, bp);
      }
      safe_str(word, buff, bp);
    } else if (sptr == str) {	/* first element in string */
      safe_str(word, buff, bp);
      safe_str(eptr, buff, bp);
    } else {
      safe_str(str, buff, bp);
      safe_chr(c, buff, bp);
      safe_str(word, buff, bp);
      safe_str(eptr, buff, bp);
    }
    break;
  case IF_INSERT:
    /* insertion */
    if (sptr == str) {		/* first element in string */
      safe_str(word, buff, bp);
      safe_chr(c, buff, bp);
      safe_str(str, buff, bp);
    } else {
      safe_str(str, buff, bp);
      safe_chr(c, buff, bp);
      safe_str(word, buff, bp);
      if (sptr && *sptr) {	/* Don't add an osep to the end of the list */
	safe_chr(c, buff, bp);
	safe_str(sptr, buff, bp);
      }
    }
    break;
  }
}


/* ARGSUSED */
FUNCTION(fun_ldelete)
{
  /* delete a word at position X of a list */

  do_itemfuns(buff, bp, args[0], args[1], NULL, args[2], IF_DELETE);
}

/* ARGSUSED */
FUNCTION(fun_replace)
{
  /* replace a word at position X of a list */

  do_itemfuns(buff, bp, args[0], args[1], args[2], args[3], IF_REPLACE);
}

/* ARGSUSED */
FUNCTION(fun_insert)
{
  /* insert a word at position X of a list */

  do_itemfuns(buff, bp, args[0], args[1], args[2], args[3], IF_INSERT);
}

/* ARGSUSED */
FUNCTION(fun_member)
{
  char *s, *t;
  char sep;
  int el;

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  if (strchr(args[1], sep)) {
    safe_str(T("#-1 CAN ONLY TEST ONE ELEMENT"), buff, bp);
    return;
  }
  s = trim_space_sep(args[0], sep);
  el = 1;

  do {
    t = split_token(&s, sep);
    if (!strcmp(args[1], t)) {
      safe_integer(el, buff, bp);
      return;
    }
    el++;
  } while (s);

  safe_chr('0', buff, bp);	/* not found */
}

/* ARGSUSED */
FUNCTION(fun_before)
{
  char *p;

  if (!*args[1])
    p = strchr(args[0], ' ');
  else
    p = strstr(args[0], args[1]);
  if (p) {
    safe_strl(args[0], p - args[0], buff, bp);
  } else
    safe_strl(args[0], arglens[0], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_after)
{
  char *p;

  if (!*args[1]) {
    args[1][0] = ' ';
    args[1][1] = '\0';
    arglens[1] = 1;
  }
  p = strstr(args[0], args[1]);
  if (p)
    safe_str(p + arglens[1], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_revwords)
{
  char **words;
  int count;
  char sep;
  char *osep, osepd[2] = { '\0', '\0' };

  if (!delim_check(buff, bp, nargs, args, 2, &sep))
    return;

  if (nargs == 3)
    osep = args[2];
  else {
    osepd[0] = sep;
    osep = osepd;
  }

  words = (char **) mush_malloc(sizeof(char *) * BUFFER_LEN, "wordlist");

  count = list2arr(words, BUFFER_LEN, args[0], sep);
  if (count == 0) {
    mush_free((Malloc_t) words, "wordlist");
    return;
  }

  safe_str(words[--count], buff, bp);
  while (count) {
    safe_str(osep, buff, bp);
    safe_str(words[--count], buff, bp);
  }
  mush_free((Malloc_t) words, "wordlist");
}

/* ARGSUSED */
FUNCTION(fun_words)
{
  char sep;

  if (!delim_check(buff, bp, nargs, args, 2, &sep))
    return;
  safe_integer(do_wordcount(trim_space_sep(args[0], sep), sep), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_splice)
{
  /* like MERGE(), but does it for a word */

  char *s0, *s1, *s2;
  char *p0, *p1;
  char sep;

  if (!delim_check(buff, bp, nargs, args, 4, &sep))
    return;

  s0 = trim_space_sep(args[0], sep);
  s1 = trim_space_sep(args[1], sep);
  s2 = trim_space_sep(args[2], sep);

  /* length checks */
  if (!*args[2]) {
    safe_str(T("#-1 NEED A WORD"), buff, bp);
    return;
  }
  if (do_wordcount(s2, sep) != 1) {
    safe_str(T("#-1 TOO MANY WORDS"), buff, bp);
    return;
  }
  if (do_wordcount(s0, sep) != do_wordcount(s1, sep)) {
    safe_str(T("#-1 NUMBER OF WORDS MUST BE EQUAL"), buff, bp);
    return;
  }
  /* loop through the two lists */
  p0 = split_token(&s0, sep);
  p1 = split_token(&s1, sep);
  safe_str(strcmp(p0, s2) ? p0 : p1, buff, bp);
  while (s0) {
    p0 = split_token(&s0, sep);
    p1 = split_token(&s1, sep);
    safe_chr(sep, buff, bp);
    safe_str(strcmp(p0, s2) ? p0 : p1, buff, bp);
  }
}


FUNCTION(fun_break) {
  int i;

  if(!args[0] || !*args[0])
	i = 0;

  if(i != 0 && !is_strict_integer(args[0])) {
	safe_str(T(e_int), buff, bp);
	return;
   }

   if(i != 0) i = parse_integer(args[0]);

   if(i < 0 || i >= inum || (inum - i) <= inum_limit) {
	safe_str(T("#-1 ARGUMENT OUT OF RANGE"), buff, bp);
	return;
   }

   iter_break = i+1;
}

/* ARGSUSED */
FUNCTION(fun_iter)
{
  /* Based on the TinyMUSH 2.0 code for this function. Please note that
   * arguments to this function are passed _unparsed_.
   */
  /* Actually, this code has changed so much that the above comment
   * isn't really true anymore. - Talek, 18 Oct 2000
   */

  char sep;
  char *outsep, *list;
  char *tbuf1, *tbuf2, *lp;
  char const *sp;
  int *place;
  int funccount;
  char *oldbp;
  const char *replace[2];


  if (inum >= MAX_ITERS) {
    safe_str(T("#-1 TOO MANY ITERS"), buff, bp);
    return;
  }

  if (nargs >= 3) {
    /* We have a delimiter. We've got to parse the third arg in place */
    char insep[BUFFER_LEN];
    char *isep = insep;
    const char *arg3 = args[2];
    process_expression(insep, &isep, &arg3, executor, caller, enactor,
		       PE_DEFAULT, PT_DEFAULT, pe_info);
    *isep = '\0';
    strcpy(args[2], insep);
  }
  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  outsep = (char *) mush_malloc(BUFFER_LEN, "string");
  list = (char *) mush_malloc(BUFFER_LEN, "string");
  if (!outsep || !list)
    mush_panic("Unable to allocate memory in fun_iter");
  if (nargs < 4)
    strcpy(outsep, " ");
  else {
    const char *arg4 = args[3];
    char *osep = outsep;
    process_expression(outsep, &osep, &arg4, executor, caller, enactor,
		       PE_DEFAULT, PT_DEFAULT, pe_info);
    *osep = '\0';
  }
  lp = list;
  sp = args[0];
  process_expression(list, &lp, &sp, executor, caller, enactor,
		     PE_DEFAULT, PT_DEFAULT, pe_info);
  *lp = '\0';
  lp = trim_space_sep(list, sep);
  if (!*lp) {
    mush_free((Malloc_t) outsep, "string");
    mush_free((Malloc_t) list, "string");
    return;
  }

  inum++;
  place = &iter_place[inum];
  *place = 0;
  funccount = pe_info->fun_invocations;
  oldbp = *bp;
  while (lp) {
    if (*place) {
      safe_str(outsep, buff, bp);
    }
    *place = *place + 1;
    iter_rep[inum] = tbuf1 = split_token(&lp, sep);
    replace[0] = tbuf1;
    replace[1] = unparse_integer(*place);
    tbuf2 = replace_string2(standard_tokens, replace, args[1]);
    sp = tbuf2;
    if (process_expression(buff, bp, &sp, executor, caller, enactor,
			   PE_DEFAULT, PT_DEFAULT, pe_info))
      break;
    if (*bp == (buff + BUFFER_LEN - 1) && pe_info->fun_invocations == funccount)
      break;
    funccount = pe_info->fun_invocations;
    oldbp = *bp;
    mush_free((Malloc_t) tbuf2, "replace_string.buff");
    if(iter_break > 0) { 
      iter_break--;
      break;
    }
  }
  *place = 0;
  iter_rep[inum] = NULL;
  inum--;
  mush_free((Malloc_t) outsep, "string");
  mush_free((Malloc_t) list, "string");
}

/* ARGSUSED */
FUNCTION(fun_ilev)
{
  safe_integer(inum - 1, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_itext)
{
  int i;

  if (!is_strict_integer(args[0])) {
    safe_str(T(e_int), buff, bp);
    return;
  }
  i = parse_integer(args[0]);

  if (i < 0 || i >= inum || (inum - i) <= inum_limit) {
    safe_str(T("#-1 ARGUMENT OUT OF RANGE"), buff, bp);
    return;
  }

  safe_str(iter_rep[inum - i], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_inum)
{
  int i;

  if (!is_strict_integer(args[0])) {
    safe_str(T(e_int), buff, bp);
    return;
  }
  i = parse_integer(args[0]);

  if (i < 0 || i >= inum || (inum - i) <= inum_limit) {
    safe_str(T("#-1 ARGUMENT OUT OF RANGE"), buff, bp);
    return;
  }

  safe_number(iter_place[inum - i], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_step)
{
  /* Like map, but passes up to 10 elements from the list at a time in %0-%9
   * If the attribute is not found, null is returned, NOT an error.
   * This function takes delimiters.
   */

  dbref thing;
  ATTR *attrib;
  char *preserve[10];
  char const *ap;
  char *asave, *lp;
  char sep;
  int n;
  int step;
  int funccount;
  char *oldbp;
  char *osep, osepd[2] = { '\0', '\0' };
  int pe_flags = PE_DEFAULT;

  if (!is_integer(args[2])) {
    safe_str(T(e_int), buff, bp);
    return;
  }

  step = parse_integer(args[2]);

  if (step < 1 || step > 10) {
    safe_str(T("#-1 STEP OUT OF RANGE"), buff, bp);
    return;
  }

  if (!delim_check(buff, bp, nargs, args, 4, &sep))
    return;

  if (nargs == 5)
    osep = args[4];
  else {
    osepd[0] = sep;
    osep = osepd;
  }

  lp = trim_space_sep(args[1], sep);
  if (!*lp)
    return;

  /* find our object and attribute */
  parse_anon_attrib(executor, args[0], &thing, &attrib);
  if (!GoodObject(thing) || !attrib || !Can_Read_Attr(executor, thing, attrib)) {
    free_anon_attrib(attrib);
    return;
  }
  if (!CanEvalAttr(executor, thing, attrib)) {
    free_anon_attrib(attrib);
    return;
  }
  if (AF_Debug(attrib))
    pe_flags |= PE_DEBUG;

  asave = safe_atr_value(attrib);

  /* save our stack */
  save_global_env("step", preserve);

  for (n = 0; n < step; n++) {
    global_eval_context.wenv[n] = split_token(&lp, sep);
    if (!lp) {
      n++;
      break;
    }
  }
  for (; n < 10; n++)
    global_eval_context.wenv[n] = NULL;

  ap = asave;
  process_expression(buff, bp, &ap, thing, executor, enactor,
		     pe_flags, PT_DEFAULT, pe_info);
  oldbp = *bp;
  funccount = pe_info->fun_invocations;
  while (lp) {
    safe_str(osep, buff, bp);
    for (n = 0; n < step; n++) {
      global_eval_context.wenv[n] = split_token(&lp, sep);
      if (!lp) {
	n++;
	break;
      }
    }
    for (; n < 10; n++)
      global_eval_context.wenv[n] = NULL;
    ap = asave;
    if (process_expression(buff, bp, &ap, thing, executor, enactor,
			   pe_flags, PT_DEFAULT, pe_info))
      break;
    if (*bp == (buff + BUFFER_LEN - 1) && pe_info->fun_invocations == funccount)
      break;
    oldbp = *bp;
    funccount = pe_info->fun_invocations;
  }

  free((Malloc_t) asave);
  free_anon_attrib(attrib);
  restore_global_env("step", preserve);
}

/* ARGSUSED */
FUNCTION(fun_map)
{
  /* Like iter(), but calls an attribute with list elements as %0 instead.
   * If the attribute is not found, null is returned, NOT an error.
   * This function takes delimiters.
   */

  ufun_attrib ufun;
  char *lp;
  char *wenv[2];
  char place[16];
  int placenr = 1;
  char sep;
  int funccount;
  char *osep, osepd[2] = { '\0', '\0' };
  char rbuff[BUFFER_LEN];

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  osepd[0] = sep;
  osep = (nargs >= 4) ? args[3] : osepd;

  lp = trim_space_sep(args[1], sep);
  if (!*lp)
    return;

  if (!fetch_ufun_attrib(args[0], executor, &ufun, 1))
    return;

  strcpy(place, "1");

  /* Build our %0 args */
  wenv[0] = split_token(&lp, sep);
  wenv[1] = place;

  call_ufun(&ufun, wenv, 2, rbuff, executor, enactor, pe_info);
  funccount = pe_info->fun_invocations;
  safe_str(rbuff, buff, bp);
  while (lp) {
    safe_str(osep, buff, bp);
    strcpy(place, unparse_integer(++placenr));
    wenv[0] = split_token(&lp, sep);

    if (call_ufun(&ufun, wenv, 2, rbuff, executor, enactor, pe_info))
      break;
    safe_str(rbuff, buff, bp);
    if (*bp == (buff + BUFFER_LEN - 1) && pe_info->fun_invocations == funccount)
      break;
    funccount = pe_info->fun_invocations;
  }
}


/* ARGSUSED */
FUNCTION(fun_mix)
{
  /* Like map(), but goes through lists, passing them as %0 and %1.. %9.
   * If the attribute is not found, null is returned, NOT an error.
   * This function takes delimiters.
   */

  ufun_attrib ufun;
  char rbuff[BUFFER_LEN];
  char *lp[10];
  char *list[10];
  char sep;
  int funccount;
  int n;
  int lists, words;
  int first = 1;

  if (nargs > 3) {		/* Last arg must be the delimiter */
    n = nargs;
    lists = nargs - 2;
  } else {
    n = 4;
    lists = 2;
  }

  if (!delim_check(buff, bp, nargs, args, n, &sep))
    return;

  for (n = 0; n < lists; n++)
    lp[n] = trim_space_sep(args[n + 1], sep);

  /* find our object and attribute */
  if (!fetch_ufun_attrib(args[0], executor, &ufun, 1))
    return;

  first = 1;
  while (1) {
    words = 0;
    for (n = 0; n < lists; n++) {
      if (lp[n] && *lp[n]) {
	list[n] = split_token(&lp[n], sep);
	if (list[n])
	  words++;
      } else {
	list[n] = NULL;
      }
    }
    if (!words)
      return;
    if (first)
      first = 0;
    else
      safe_chr(sep, buff, bp);
    funccount = pe_info->fun_invocations;
    call_ufun(&ufun, list, lists, rbuff, executor, enactor, pe_info);
    safe_str(rbuff, buff, bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_table)
{
  /* TABLE(list, field_width, line_length, delimiter, output sep)
   * Given a list, produce a table (a column'd list)
   * Optional parameters: field width, line length, delimiter, output sep
   * Number of columns = line length / (field width+1)
   */
  size_t line_length = 78;
  size_t field_width = 10;
  size_t col = 0;
  size_t offset, col_len;
  char sep, osep, *cp, *t;
  ansi_string *as;

  if (!delim_check(buff, bp, nargs, args, 5, &osep))
    return;
  if ((nargs == 5) && !*args[4])
    osep = 0;

  if (!delim_check(buff, bp, nargs, args, 4, &sep))
    return;

  if (nargs > 2) {
    if (!is_integer(args[2])) {
      safe_str(T(e_ints), buff, bp);
      return;
    }
    line_length = parse_integer(args[2]);
    if (line_length < 2)
      line_length = 2;
  }
  if (nargs > 1) {
    if (!is_integer(args[1])) {
      safe_str(T(e_ints), buff, bp);
      return;
    }
    field_width = parse_integer(args[1]);
    if (field_width < 1)
      field_width = 1;
    if (field_width >= BUFFER_LEN)
      field_width = BUFFER_LEN - 1;
  }
  if (field_width >= line_length)
    field_width = line_length - 1;

  /* Split out each token, truncate/pad it to field_width, and pack
   * it onto the line. When the line would go over line_length,
   * send a return
   */

  as = parse_ansi_string(args[0]);

  cp = trim_space_sep(as->text, sep);
  if (!*cp) {
    free_ansi_string(as);
    return;
  }

  t = split_token(&cp, sep);
  offset = t - &as->text[0];
  col_len = strlen(t);
  if (col_len > field_width)
    col_len = field_width;
  safe_ansi_string(as, offset, col_len, buff, bp);
  if (safe_fill(' ', field_width - col_len, buff, bp)) {
    free_ansi_string(as);
    return;
  }
  col = field_width + !!osep;
  while (cp) {
    col += field_width + !!osep;
    if (col > line_length) {
      if (NEWLINE_ONE_CHAR)
	safe_str("\n", buff, bp);
      else
	safe_str("\r\n", buff, bp);
      col = field_width + !!osep;
    } else {
      if (osep)
	safe_chr(osep, buff, bp);
    }
    t = split_token(&cp, sep);
    if (!t)
      break;
    offset = t - &as->text[0];
    col_len = strlen(t);
    if (col_len > field_width)
      col_len = field_width;
    safe_ansi_string(as, offset, col_len, buff, bp);
    if (safe_fill(' ', field_width - col_len, buff, bp))
      break;
  }
  free_ansi_string(as);
}

/* In the following regexp functions, we use pcre_study to potentially
 * make pcre_exec faster. If pcre_study() can't help, it returns right
 * away, and if it can, the savings in the actual matching are usually
 * worth it.  Ideally, all compiled regexps and their study patterns
 * should be cached somewhere. Especially nice for patterns in the
 * master room. Just need to come up with a good caching algorithm to
 * use. Easiest would be a hashtable that's just cleared every
 * dbck_interval seconds. Except some benchmarking showed that compiling
 * patterns is faster than I thought it'd be, so this is low priority.
 */

/* string, regexp, replacement string. Acts like sed or perl's s///g,
//with an ig version */
FUNCTION(fun_regreplace)
{
  pcre *re;
  pcre_extra *study = NULL;
  const char *errptr;
  int subpatterns;
  int offsets[99];
  int erroffset;
  const char *r, *obp;
  char *start, *oldbp;
  char tbuf[BUFFER_LEN], *tbp;
  char abuf[BUFFER_LEN], *abp;
  char prebuf[BUFFER_LEN], *prep;
  char postbuf[BUFFER_LEN], *postp;
  pcre *old_re_code;
  int flags = 0, all = 0, match_offset = 0, len, funccount;
  int i;

  int old_re_subpatterns;
  int *old_re_offsets;
  char *old_re_from;

  old_re_subpatterns = global_eval_context.re_subpatterns;
  old_re_offsets = global_eval_context.re_offsets;
  old_re_from = global_eval_context.re_from;

  if (called_as[strlen(called_as) - 1] == 'I')
    flags = PCRE_CASELESS;

  if (string_prefix(called_as, "REGEDITALL"))
    all = 1;

  abp = abuf;
  r = args[0];
  process_expression(abuf, &abp, &r, executor, caller, enactor, PE_DEFAULT,
		     PT_DEFAULT, pe_info);
  *abp = '\0';

  postp = postbuf;
  safe_str(abuf, postbuf, &postp);
  *postp = '\0';

  for (i = 1; i < nargs - 1; i += 2) {
    /* old postbuf is new prebuf */
    prep = prebuf;
    safe_str(postbuf, prebuf, &prep);
    *prep = '\0';
    postp = postbuf;
    *postp = '\0';
    tbp = tbuf;
    r = args[i];
    process_expression(tbuf, &tbp, &r, executor, caller, enactor, PE_DEFAULT,
		       PT_DEFAULT, pe_info);
    *tbp = '\0';

    if ((re = pcre_compile(tbuf, flags, &errptr, &erroffset, tables)) == NULL) {
      /* Matching error. */
      safe_str(T("#-1 REGEXP ERROR: "), buff, bp);
      safe_str(errptr, buff, bp);
      return;
    }
    add_check("pcre");
    /* If we're going to match the pattern multiple times, let's
       study it. */
    if (all) {
      study = pcre_study(re, 0, &errptr);
      if (errptr != NULL) {
	mush_free((Malloc_t) re, "pcre");
	safe_str(T("#-1 REGEXP ERROR: "), buff, bp);
	safe_str(errptr, buff, bp);
	return;
      }
      if (study != NULL)
	add_check("pcre.extra");
    }
    len = strlen(prebuf);
    start = prebuf;
    subpatterns = pcre_exec(re, study, prebuf, len, 0, 0, offsets, 99);

    /* Match wasn't found... we're done */
    if (subpatterns < 0) {
      safe_str(prebuf, postbuf, &postp);
      mush_free((Malloc_t) re, "pcre");
      if (study)
	mush_free((Malloc_t) study, "pcre.extra");
      continue;
    }

    funccount = pe_info->fun_invocations;
    oldbp = postp;

    do {
      /* Copy up to the start of the matched area */
      char tmp = prebuf[offsets[0]];
      prebuf[offsets[0]] = '\0';
      safe_str(start, postbuf, &postp);
      prebuf[offsets[0]] = tmp;

      /* Now copy in the replacement, putting in captured sub-expressions */
      obp = args[i + 1];
      global_eval_context.re_code = re;
      global_eval_context.re_from = prebuf;
      global_eval_context.re_offsets = offsets;
      global_eval_context.re_subpatterns = subpatterns;
      process_expression(postbuf, &postp, &obp, executor, caller, enactor,
			 PE_DEFAULT | PE_DOLLAR, PT_DEFAULT, pe_info);
      if ((*bp == (buff + BUFFER_LEN - 1))
	  && (pe_info->fun_invocations == funccount))
	break;

      oldbp = postp;
      funccount = pe_info->fun_invocations;

      start = prebuf + offsets[1];
      match_offset = offsets[1];
      /* Make sure we advance at least 1 char */
      if (offsets[0] == match_offset)
	match_offset++;
    } while (all && match_offset < len && (subpatterns =
					   pcre_exec(re, study, prebuf, len,
						     match_offset, 0, offsets,
						     99)) >= 0);


    /* Now copy everything after the matched bit */
    safe_str(start, postbuf, &postp);
    *postp = '\0';
    mush_free((Malloc_t) re, "pcre");
    if (study)
      mush_free((Malloc_t) study, "pcre.extra");

    global_eval_context.re_code = old_re_code;
    global_eval_context.re_offsets = old_re_offsets;
    global_eval_context.re_subpatterns = old_re_subpatterns;
    global_eval_context.re_from = old_re_from;
  }

  safe_str(postbuf, buff, bp);
}

/** array of indexes for %q registers during regexp matching */
extern signed char qreg_indexes[UCHAR_MAX + 1];

FUNCTION(fun_regmatch)
{
/* ---------------------------------------------------------------------------
 * fun_regmatch Return 0 or 1 depending on whether or not a regular
 * expression matches a string. If a third argument is specified, dump
 * the results of a regexp pattern match into a set of r()-registers.
 *
 * regmatch(string, pattern, list of registers)
 * Registers are by position (old way) or name:register (new way)
 *
 */
  int i, nqregs, curq;
  char *qregs[NUMQ];
  pcre *re;
  const char *errptr;
  int erroffset;
  int offsets[99];
  int subpatterns;
  int flags = 0;
  int qindex;

  if (strcmp(called_as, "REGMATCHI") == 0)
    flags = PCRE_CASELESS;

  if (nargs == 2) {		/* Don't care about saving sub expressions */
    safe_boolean(quick_regexp_match(args[1], args[0], flags ? 0 : 1), buff, bp);
    return;
  }

  if ((re = pcre_compile(args[1], flags, &errptr, &erroffset, tables)) == NULL) {
    /* Matching error. */
    safe_str(T("#-1 REGEXP ERROR: "), buff, bp);
    safe_str(errptr, buff, bp);
    return;
  }
  add_check("pcre");
  subpatterns = pcre_exec(re, NULL, args[0], arglens[0], 0, 0, offsets, 99);
  safe_integer(subpatterns >= 0, buff, bp);

  /* We need to parse the list of registers.  Anything that we don't parse
   * is assumed to be -1.  If we didn't match, or the match went wonky,
   * then set the register to empty.  Otherwise, fill the register with
   * the subexpression.
   */
  if (subpatterns == 0)
    subpatterns = 33;
  nqregs = list2arr(qregs, NUMQ, args[2], ' ');
  for (i = 0; i < nqregs; i++) {
    char *regname;
    char *named_subpattern = NULL;
    int subpattern = 0;
    if ((regname = strchr(qregs[i], ':'))) {
      /* subexpr:register */
      *regname++ = '\0';
      if (is_strict_integer(qregs[i]))
	subpattern = parse_integer(qregs[i]);
      else
	named_subpattern = qregs[i];
    } else {
      /* Get subexpr by position in list */
      subpattern = i;
      regname = qregs[i];
    }

    if (regname && regname[0] && !regname[1] &&
	((qindex = qreg_indexes[(unsigned char) regname[0]]) != -1))
      curq = qindex;
    else
      curq = -1;
    if (curq < 0 || curq >= NUMQ)
      continue;

    if (subpatterns < 0)
      global_eval_context.renv[curq][0] = '\0';
    else if (named_subpattern)
      pcre_copy_named_substring(re, args[0], offsets, subpatterns,
				named_subpattern,
				global_eval_context.renv[curq], BUFFER_LEN);
    else
      pcre_copy_substring(args[0], offsets, subpatterns, subpattern,
			  global_eval_context.renv[curq], BUFFER_LEN);
  }
  mush_free((Malloc_t) re, "pcre");
}


/** Structure to hold data for regrep */
struct regrep_data {
  pcre *re;		/**< Pointer to compiled regular expression */
  pcre_extra *study;	/**< Pointer to studied data about re */
  char *buff;		/**< Buffer to store regrep results */
  char **bp;		/**< Pointer to address of insertion point in buff */
  int first;		/**< Is this the first match or a later match? */
};

/* Like grep(), but using a regexp pattern. This same function handles
 *  both regrep and regrepi. */
FUNCTION(fun_regrep)
{
  struct regrep_data reharg;
  const char *errptr;
  int erroffset;
  int flags = 0;

  dbref it = match_thing(executor, args[0]);
  reharg.first = 0;
  if (it == NOTHING || it == AMBIGUOUS) {
    safe_str(T(e_notvis), buff, bp);
    return;
  }
  /* make sure there's an attribute and a pattern */
  if (!*args[1]) {
    safe_str(T("#-1 NO SUCH ATTRIBUTE"), buff, bp);
    return;
  }
  if (!*args[2]) {
    safe_str(T("#-1 INVALID GREP PATTERN"), buff, bp);
    return;
  }

  if (strcmp(called_as, "REGREPI") == 0)
    flags = PCRE_CASELESS;

  if ((reharg.re = pcre_compile(args[2], flags,
				&errptr, &erroffset, tables)) == NULL) {
    /* Matching error. */
    safe_str(T("#-1 REGEXP ERROR: "), buff, bp);
    safe_str(errptr, buff, bp);
    return;
  }
  add_check("pcre");

  reharg.study = pcre_study(reharg.re, 0, &errptr);
  if (errptr != NULL) {
    safe_str(T("#-1 REGEXP ERROR: "), buff, bp);
    safe_str(errptr, buff, bp);
    mush_free(reharg.re, "pcre");
    return;
  }
  if (reharg.study)
    add_check("pcre.extra");

  reharg.buff = buff;
  reharg.bp = bp;

  atr_iter_get(executor, it, args[1], 0, regrep_helper, (void *) &reharg);
  mush_free(reharg.re, "pcre");
  if (reharg.study)
    mush_free(reharg.study, "pcre.extra");
}

static int
regrep_helper(dbref who __attribute__ ((__unused__)),
	      dbref what __attribute__ ((__unused__)),
	      dbref parent __attribute__ ((__unused__)),
	      char const *name __attribute__ ((__unused__)),
	      ATTR *atr, void *args)
{
  struct regrep_data *reharg = args;
  char const *str;
  int offsets[99];

  str = atr_value(atr);
  if (pcre_exec(reharg->re, reharg->study, str, strlen(str), 0, 0, offsets, 99)
      >= 0) {
    if (reharg->first != 0)
      safe_chr(' ', reharg->buff, reharg->bp);
    else
      reharg->first = 1;
    safe_str(AL_NAME(atr), reharg->buff, reharg->bp);
    return 1;
  } else
    return 0;
}

/* Like grab, but with a regexp pattern. This same function handles
 *  regrab(), regraball(), and the case-insenstive versions. */
FUNCTION(fun_regrab)
{
  char *r, *s, *b, sep;
  pcre *re;
  pcre_extra *study;
  const char *errptr;
  int erroffset;
  int offsets[99];
  int flags = 0, all = 0;
  char *osep, osepd[2] = { '\0', '\0' };

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  if (nargs == 4)
    osep = args[3];
  else {
    osepd[0] = sep;
    osep = osepd;
  }

  s = trim_space_sep(args[0], sep);
  b = *bp;

  if (strrchr(called_as, 'I'))
    flags = PCRE_CASELESS;

  if (string_prefix(called_as, "REGRABALL"))
    all = 1;

  if ((re = pcre_compile(args[1], flags, &errptr, &erroffset, tables)) == NULL) {
    /* Matching error. */
    safe_str(T("#-1 REGEXP ERROR: "), buff, bp);
    safe_str(errptr, buff, bp);
    return;
  }
  add_check("pcre");

  study = pcre_study(re, 0, &errptr);
  if (errptr != NULL) {
    safe_str(T("#-1 REGEXP ERROR: "), buff, bp);
    safe_str(errptr, buff, bp);
    mush_free(re, "pcre");
    return;
  }
  if (study)
    add_check("pcre.extra");

  do {
    r = split_token(&s, sep);
    if (pcre_exec(re, study, r, strlen(r), 0, 0, offsets, 99) >= 0) {
      if (all && *bp != b)
	safe_str(osep, buff, bp);
      safe_str(r, buff, bp);
      if (!all)
	break;
    }
  } while (s);

  mush_free((Malloc_t) re, "pcre");
  if (study)
    mush_free((Malloc_t) study, "pcre.extra");
}

FUNCTION(fun_apply)
{
  char sep;
  int count;
  char *arr[10];
  ufun_attrib ufun;
  char rbuff[BUFFER_LEN];
  OOREF_DECL;

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  ENTER_OOREF;

  count = list2arr(arr, 10, args[1], sep);

  if (!fetch_ufun_attrib(args[0], executor, &ufun, 1)) {
    safe_str(T(ufun.errmess), buff, bp);
    LEAVE_OOREF;
    return;
  }

  call_ufun(&ufun, arr, count, rbuff, executor, enactor, pe_info);

  LEAVE_OOREF;

  safe_str(rbuff, buff, bp);
}

