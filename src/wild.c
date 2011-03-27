/**
 * \file wild.c
 *
 * \brief Wildcard matching routings for PennMUSH
 *
 * Written by T. Alexander Popiel, 24 June 1993
 * Last modified by Javelin, 2002-2003
 *
 * Thanks go to Andrew Molitor for debugging
 * Thanks also go to Rich $alz for code to benchmark against
 *
 * Copyright (c) 1993,2000 by T. Alexander Popiel
 * This code is available under the terms of the GPL,
 * see http://www.gnu.org/copyleft/gpl.html for details.
 *
 * This code is included in PennMUSH under the PennMUSH
 * license by special dispensation from the author,
 * T. Alexander Popiel.  If you wish to include this
 * code in other packages, but find the GPL too onerous,
 * then please feel free to contact the author at
 * popiel@wolfskeep.com to work out agreeable terms.
 */

#include "config.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "copyrite.h"
#include "conf.h"
#include "case.h"
#include "externs.h"
#include "ansi.h"
#include "mymalloc.h"
#include "parse.h"
#include "pcre.h"
#include "confmagic.h"

/** Force a char to be lowercase */
#define FIXCASE(a) (DOWNCASE(a))
/** Check for equality of characters, maybe case-sensitive */
#define EQUAL(cs,a,b) ((cs) ? (a == b) : (FIXCASE(a) == FIXCASE(b)))
/** Check for inequality of characters, maybe case-sensitive */
#define NOTEQUAL(cs,a,b) ((cs) ? (a != b) : (FIXCASE(a) != FIXCASE(b)))
/** Maximum number of wildcarded arguments */
#define NUMARGS (10)

const unsigned char *tables = NULL;  /** Pointer to character tables */

static int wild1
  (const char *RESTRICT tstr, const char *RESTRICT dstr, int arg,
   char **wbuf, int *len, int cs, char **ary, int max);
static int wild(const char *RESTRICT s, const char *RESTRICT d, int p, int cs,
                char **ary, int max, char *buffer, int len);
static int check_literals(const char *RESTRICT tstr, const char *RESTRICT dstr,
                          int cs);
static char *strip_backslashes(const char *str);

/** Do a wildcard match, without remembering the wild data.
 *
 * This routine will cause crashes if fed NULLs instead of strings.
 *
 * \param tstr pattern to match against.
 * \param dstr string to check.
 * \retval 1 dstr matches the tstr pattern.
 * \retval 0 dstr does not match the tstr pattern.
 */
int
quick_wild(const char *RESTRICT tstr, const char *RESTRICT dstr)
{
  /* quick_wild_new does the real work, but before we call it, 
   * we do some sanity checking. 
   */
  if (!check_literals(tstr, dstr, 0))
    return 0;
  return quick_wild_new(tstr, dstr, 0);
}

/** Do a wildcard match, possibly case-sensitive, without memory.
 *
 * This probably crashes if fed NULLs instead of strings, too.
 *
 * \param tstr pattern to match against.
 * \param dstr string to check.
 * \param cs if 1, case-sensitive; if 0, case-insensitive.
 * \retval 1 dstr matches the tstr pattern.
 * \retval 0 dstr does not match the tstr pattern.
 */
int
quick_wild_new(const char *RESTRICT tstr, const char *RESTRICT dstr, int cs)
{
  while (*tstr != '*') {
    switch (*tstr) {
    case '?':
      /* Single character match.  Return false if at
       * end of data.
       */
      if (!*dstr)
        return 0;
      break;
    case '\\':
      /* Escape character.  Move up, and force literal
       * match of next character.
       */
      tstr++;
      /* FALL THROUGH */
    default:
      /* Literal character.  Check for a match.
       * If matching end of data, return true.
       */
      if (NOTEQUAL(cs, *dstr, *tstr))
        return 0;
      if (!*dstr)
        return 1;
    }
    tstr++;
    dstr++;
  }

  /* Skip over '*'. */
  tstr++;

  /* Return true on trailing '*'. */
  if (!*tstr)
    return 1;

  /* Skip over wildcards. */
  while ((*tstr == '?') || (*tstr == '*')) {
    if (*tstr == '?') {
      if (!*dstr)
        return 0;
      dstr++;
    }
    tstr++;
  }

  /* Skip over a backslash in the pattern string if it is there. */
  if (*tstr == '\\')
    tstr++;

  /* Return true on trailing '*'. */
  if (!*tstr)
    return 1;

  /* Scan for possible matches. */
  while (*dstr) {
    if (EQUAL(cs, *dstr, *tstr) && quick_wild_new(tstr + 1, dstr + 1, cs))
      return 1;
    dstr++;
  }
  return 0;
}

/** Do an attribute name wildcard match.
 *
 * This probably crashes if fed NULLs instead of strings, too.
 * The special thing about this one is that ` doesn't match normal
 * wildcards; you have to use ** to match embedded `.  Also, patterns
 * ending in ` are treated as patterns ending in `*, and empty patterns
 * are treated as *.
 *
 * \param tstr pattern to match against.
 * \param dstr string to check.
 * \retval 1 dstr matches the tstr pattern.
 * \retval 0 dstr does not match the tstr pattern.
 */
int
atr_wild(const char *RESTRICT tstr, const char *RESTRICT dstr)
{
  int starcount;

  if (!*tstr)
    return !strchr(dstr, '`');

  while (*tstr != '*') {
    switch (*tstr) {
    case '?':
      /* Single character match.  Return false if at
       * end of data.
       */
      if (!*dstr || *dstr == '`')
        return 0;
      break;
    case '`':
      /* Delimiter match.  Special handling if at end of pattern. */
      if (*dstr != '`')
        return 0;
      if (!tstr[1])
        return !strchr(dstr + 1, '`');
      break;
    case '\\':
      /* Escape character.  Move up, and force literal
       * match of next character.
       */
      tstr++;
      /* FALL THROUGH */
    default:
      /* Literal character.  Check for a match.
       * If matching end of data, return true.
       */
      if (NOTEQUAL(0, *dstr, *tstr))
        return 0;
      if (!*dstr)
        return 1;
    }
    tstr++;
    dstr++;
  }

  /* Skip over '*'. */
  tstr++;
  starcount = 1;

  /* Skip over wildcards. */
  while (starcount < 2 && ((*tstr == '?') || (*tstr == '*'))) {
    if (*tstr == '?') {
      if (!*dstr || *dstr == '`')
        return 0;
      dstr++;
      starcount = 0;
    } else
      starcount++;
    tstr++;
  }

  /* Skip over long strings of '*'. */
  while (*tstr == '*')
    tstr++;

  /* Return true on trailing '**'. */
  if (!*tstr)
    return starcount == 2 || !strchr(dstr, '`');

  if (*tstr == '?') {
    /* Scan for possible matches. */
    while (*dstr) {
      if (*dstr != '`' && atr_wild(tstr + 1, dstr + 1))
        return 1;
      dstr++;
    }
  } else {
    /* Skip over a backslash in the pattern string if it is there. */
    if (*tstr == '\\')
      tstr++;

    /* Scan for possible matches. */
    while (*dstr) {
      if (EQUAL(0, *dstr, *tstr) && atr_wild(tstr + 1, dstr + 1))
	return 1;
      if (starcount < 2 && *dstr == '`')
	return 0;
      dstr++;
    }
  }
  return 0;
}

/* ---------------------------------------------------------------------------
 * wild1: INTERNAL: do a wildcard match, remembering the wild data.
 *
 * DO NOT CALL THIS FUNCTION DIRECTLY - DOING SO MAY RESULT IN
 * SERVER CRASHES AND IMPROPER ARGUMENT RETURN.
 *
 * Side Effect: this routine modifies the 'global_eval_context.wnxt' global variable,
 * and what it points to.
 */
static int
wild1(const char *RESTRICT tstr, const char *RESTRICT dstr, int arg,
      char **wbuf, int *len, int cs, char **ary, int max)
{
  const char *datapos;
  int argpos, numextra;

  while (*tstr != '*') {
    switch (*tstr) {
    case '?':
      /* Single character match.  Return false if at
       * end of data.
       */
      if (!*dstr)
        return 0;

      if (*len >= 2) {
        ary[arg++] = *wbuf;
        *(*wbuf)++ = *dstr;
        *(*wbuf)++ = '\0';
        *len -= 2;
      }

      /* Jump to the fast routine if we can. */

      if (arg >= (int) max || *len < 2)
        return quick_wild_new(tstr + 1, dstr + 1, cs);
      break;
    case '\\':
      /* Escape character.  Move up, and force literal
       * match of next character.
       */
      tstr++;
      /* FALL THROUGH */
    default:
      /* Literal character.  Check for a match.
       * If matching end of data, return true.
       */
      if (NOTEQUAL(cs, *dstr, *tstr))
        return 0;
      if (!*dstr)
        return 1;
    }
    tstr++;
    dstr++;
  }

  /* If at end of pattern, slurp the rest, and leave. */
  if (!tstr[1]) {
    int tlen;
    tlen = strlen(dstr);
    if (tlen < *len) {
      ary[arg] = *wbuf;
      strcpy(*wbuf, dstr);
      *wbuf += tlen + 2;
      *len -= tlen + 1;
    }
    return 1;
  }
  /* Remember current position for filling in the '*' return. */
  datapos = dstr;
  argpos = arg;

  /* Scan forward until we find a non-wildcard. */
  do {
    if (argpos < arg) {
      /* Fill in arguments if someone put another '*'
       * before a fixed string.
       */
      if (*len >= 1) {
        ary[argpos++] = *wbuf;
        *(*wbuf)++ = '\0';
        *len -= 1;
      }

      /* Jump to the fast routine if we can. */
      if (argpos >= (int) max || *len < 2)
        return quick_wild_new(tstr, dstr, cs);

      /* Fill in any intervening '?'s */
      while (argpos < arg) {
        if (*len >= 2) {
          ary[argpos++] = *wbuf;
          *(*wbuf)++ = *datapos++;
          *(*wbuf)++ = '\0';
          *len -= 2;
        }

        /* Jump to the fast routine if we can. */
        if (argpos >= (int) max || *len < 1)
          return quick_wild_new(tstr, dstr, cs);
      }
    }
    /* Skip over the '*' for now... */
    tstr++;
    arg++;

    /* Skip over '?'s for now... */
    numextra = 0;
    while (*tstr == '?') {
      if (!*dstr)
        return 0;
      tstr++;
      dstr++;
      arg++;
      numextra++;
    }
  } while (*tstr == '*');

  /* Skip over a backslash in the pattern string if it is there. */
  if (*tstr == '\\')
    tstr++;

  /* Check for possible matches.  This loop terminates either at
   * end of data (resulting in failure), or at a successful match.
   */
  if (!*tstr)
    while (*dstr)
      dstr++;
  else {
    while (1) {
      if (EQUAL(cs, *dstr, *tstr) &&
          ((arg < (int) max) ? wild1(tstr, dstr, arg, wbuf, len, cs, ary, max)
           : quick_wild_new(tstr, dstr, cs)))
        break;
      if (!*dstr)
        return 0;
      dstr++;
    }
  }

  /* Found a match!  Fill in all remaining arguments.
   * First do the '*'...
   */
  {
    int datalen;
    datalen = (dstr - datapos) - numextra;
    if (datalen + 1 <= *len) {
      ary[argpos++] = *wbuf;
      strncpy(*wbuf, datapos, datalen);
      *wbuf += datalen;
      *(*wbuf)++ = '\0';
      *len -= datalen + 1;
      datapos = dstr - numextra;
    }
  }

  /* Fill in any trailing '?'s that are left. */
  while (numextra) {
    if (argpos >= (int) max || *len < 2)
      return 1;
    if (*len >= 2) {
      ary[argpos++] = *wbuf;
      *(*wbuf)++ = *datapos++;
      *(*wbuf)++ = '\0';
      *len -= 2;
      numextra--;
    }
  }

  /* It's done! */
  return 1;
}

/* ---------------------------------------------------------------------------
 * wild: do a wildcard match, remembering the wild data.
 *
 * This routine will cause crashes if fed NULLs instead of strings.
 *
 * This function may crash if malloc() fails.
 *
 * Side Effect: this routine modifies the 'global_eval_context.wnxt' global variable.
 */
static int
wild(const char *RESTRICT s, const char *RESTRICT d, int p, int cs,
     char **ary, int max, char *buffer, int len)
{
  /* Do fast match to see if pattern matches. If yes, do it again,
     remembering this time.. */
  while ((*s != '*') && (*s != '?')) {
    if (*s == '\\')
      s++;
    if (NOTEQUAL(cs, *d, *s))
      return 0;
    if (!*d)
      return 1;
    s++;
    d++;
  }

  /* Do sanity check */
  if (!check_literals(s, d, cs))
    return 0;

  /* Do the match. */
  return wild1(s, d, p, &buffer, &len, cs, ary, max);
}
 

/** Wildcard match, possibly case-sensitive, and remember the wild data
 *  in matches, storing them in data.
 * This routine will cause crashes if fed NULLs instead of strings.
 *
 * \param s pattern to match against.
 * \param d string to check.
 * \param cs if 1, case-sensitive; if 0, case-insensitive.
 * \param ary An array to store the grabs in
 * \param max Number of elements ary can hold
 * \param data Buffer used to hold the matches. The elements of ary
 *    are set to pointers into this  buffer.
 * \param len The number of bytes in data. Twice the length of d should
 *    be enough.
 * \retval 1 d matches s.
 * \retval 0 d doesn't match s.
 */
int
wild_match_case_r(const char *RESTRICT s, const char *RESTRICT d, int cs,
                  char **matches, int nmatches, char *data, int len)
{
  int n;

  for (n = 0; n < nmatches; n++)
    matches[n] = NULL;

  return wild(s, d, 0, cs, matches, nmatches, data, len);
}

/** Regexp match, possibly case-sensitive, and remember matched subexpressions.
 *
 * This routine will cause crashes if fed NULLs instead of strings.
 *
 * \param s regexp to match against.
 * \param d string to check.
 * \param cs if 1, case-sensitive; if 0, case-insensitive
 * \param matches array to store matched subexpressions in
 * \param nmatches the size of the matches array
 * \param data buffer space to copy matches into. The elements of
 *   array point into here
 * \param len The size of data
 * \retval 1 d matches s
 * \retval 0 d doesn't match s
 */
int
regexp_match_case_r(const char *RESTRICT s, const char *RESTRICT val, int cs,
                    char **matches, int nmatches, char *data, int len)
{
  pcre *re;
  int i;
  const char *errptr;
  const char *d;
  size_t delenn;
  int erroffset;
  int offsets[99];
  int subpatterns;

  for (i = 0; i < nmatches; i++)
    matches[i] = NULL;

  if ((re = pcre_compile(s, (cs ? 0 : PCRE_CASELESS), &errptr, &erroffset,
                         tables)) == NULL) {
    /*
     * This is a matching error. We have an error message in
     * errptr that we can ignore, since we're doing
     * command-matching.
     */
    return 0;
  }
  add_check("pcre");
  d = remove_markup(val, &delenn);
  /* 
   * Now we try to match the pattern. The relevant fields will
   * automatically be filled in by this.
   */
  if ((subpatterns = pcre_exec(re, NULL, d, delenn - 1, 0, 0, offsets, 99))
      < 0) {
    mush_free(re, "pcre");
    return 0;
  }
  /* If we had too many subpatterns for the offsets vector, set the number
   * to 1/3 of the size of the offsets vector
   */
  if (subpatterns == 0)
    subpatterns = 33;

  /*
   * Now we fill in our args vector. Note that in regexp matching,
   * 0 is the entire string matched, and the parenthesized strings
   * go from 1 to 9. We DO PRESERVE THIS PARADIGM, for consistency
   * with other languages.
   */

  for (i = 0; i < nmatches && i < subpatterns && len > 1; i++) {
    int sublen;
  
    sublen = pcre_copy_substring(d, offsets, subpatterns, (int) i, data, len);

    if (sublen < 0)
      break;

    matches[i] = data;
    data += sublen + 2;
    len -= sublen + 2;
  }

  mush_free(re, "pcre");
  return 1;
}


/** Regexp match, possibly case-sensitive, and with no memory.
 *
 * This routine will cause crashes if fed NULLs instead of strings.
 *
 * \param s regexp to match against.
 * \param d string to check.
 * \param cs if 1, case-sensitive; if 0, case-insensitive.
 * \retval 1 d matches s.
 * \retval 0 d doesn't match s.
 */
int
quick_regexp_match(const char *RESTRICT s, const char *RESTRICT d, int cs)
{
  pcre *re;
  const char *sptr;
  size_t slen;
  const char *errptr;
  int erroffset;
  int offsets[99];
  int r;
  int flags = 0;                /* There's a PCRE_NO_AUTO_CAPTURE flag to turn all raw
                                   ()'s into (?:)'s, which would be nice to use,
                                   except that people might use backreferences in
                                   their patterns. Argh. */

  if (!cs)
    flags |= PCRE_CASELESS;

  if ((re = pcre_compile(s, flags, &errptr, &erroffset, tables)) == NULL) {
    /*
     * This is a matching error. We have an error message in
     * errptr that we can ignore, since we're doing
     * command-matching.
     */
    return 0;
  }
  add_check("pcre");
  sptr = remove_markup(d, &slen);
  /* 
   * Now we try to match the pattern. The relevant fields will
   * automatically be filled in by this.
   */
  r = pcre_exec(re, NULL, sptr, slen - 1, 0, 0, offsets, 99); 

  mush_free(re, "pcre");

  return r >= 0;
}


/** Either an order comparison or a wildcard match with no memory.
 *
 *
 * \param s pattern to match against.
 * \param d string to check.
 * \param cs if 1, case-sensitive; if 0, case-insensitive.
 * \retval 1 d matches s.
 * \retval 0 d doesn't match s.
 */
int
local_wild_match_case(const char *RESTRICT s, const char *RESTRICT d, int cs)
{
  if (s && *s) {
    switch (*s) {
    case '>':
      s++;
      if (is_number(s) && is_number(d))
        return (parse_number(s) < parse_number(d));
      else
        return (strcoll(s, d) < 0);
    case '<':
      s++;
      if (is_number(s) && is_number(d))
        return (parse_number(s) > parse_number(d));
      else
        return (strcoll(s, d) > 0);
    default:
      return quick_wild_new(s, d, cs);
    }
  } else
    return (!d || !*d) ? 1 : 0;
}

/** Does a string contain a wildcard character (* or ?)?
 * Not used by the wild matching routines, but suitable for outside use.
 * \param s string to check.
 * \retval 1 s contains a * or ?
 * \retval 0 s does not contain a * or ?
 */
int
wildcard(const char *s)
{
  if (strchr(s, '*') || strchr(s, '?'))
    return 1;
  return 0;
}

static int
check_literals(const char *RESTRICT tstr, const char *RESTRICT dstr, int cs)
{
  /* Every literal string in tstr must appear, in order, in dstr,
   * or no match can happen. That is, tstr is the pattern and dstr
   * is the string-to-match
   */
  char tbuf1[BUFFER_LEN];
  char dbuf1[BUFFER_LEN];
  const char delims[] = "?*";
  char *sp, *dp;
  mush_strncpy(dbuf1, dstr, BUFFER_LEN);
  mush_strncpy(tbuf1, strip_backslashes(tstr), BUFFER_LEN);
  if (!cs) {
    upcasestr(tbuf1);
    upcasestr(dbuf1);
  }
  dp = dbuf1;
  sp = strtok(tbuf1, delims);
  while (sp) {
    if (!dp)
      return 0;
    if (!(dp = strstr(dp, sp)))
      return 0;
    dp += strlen(sp);
    sp = strtok(NULL, delims);
  }
  return 1;
}


static char *
strip_backslashes(const char *str)
{
  /* Remove backslashes from a string, and return it in a static buffer */
  static char buf[BUFFER_LEN];
  int i = 0;

  while (*str && (i < BUFFER_LEN - 1)) {
    if (*str == '\\' && *(str + 1))
      str++;
    buf[i++] = *str++;
  }
  buf[i] = '\0';
  return buf;
}
