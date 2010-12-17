/**
 * \file funstr.c
 *
 * \brief String functions for mushcode.
 *
 *
 */
#include "copyrite.h"

#include "config.h"
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <locale.h>
#include "conf.h"
#include "ansi.h"
#include "externs.h"
#include "case.h"
#include "match.h"
#include "parse.h"
#include "pueblo.h"
#include "attrib.h"
#include "flags.h"
#include "dbdefs.h"
#include "mushdb.h"
#include "htab.h"
#include "lock.h"
#include "confmagic.h"


#ifdef WIN32
#define LC_MESSAGES 6
#pragma warning( disable : 4761)	/* NJG: disable warning re conversion */
#endif

#ifdef __APPLE__
#define LC_MESSAGES 6
#endif

HASHTAB htab_tag;  /**< Hash table of safe html tags */

#define MAX_COLS 32  /**< Maximum number of columns for align() */
static int wraplen(char *str, int maxlen);
static int align_one_line(char *buff, char **bp, int ncols,
			  int cols[MAX_COLS], int calign[MAX_COLS],
			  char *ptrs[MAX_COLS], ansi_string *as[MAX_COLS],
			  int linenum, char *fieldsep, int fslen, char *linesep,
			  int lslen, char filler);
static int comp_gencomp(dbref exceutor, char *left, char *right, char *type);
void init_tag_hashtab(void);
void init_pronouns(void);

/** Return an indicator of a player's gender.
 * \param player player whose gender is to be checked.
 * \retval 0 neuter.
 * \retval 1 female.
 * \retval 2 male.
 * \retval 3 plural.
 */
int
get_gender(dbref player)
{
  ATTR *a;

  a = atr_get(player, "SEX");

  if (!a)
    return 0;

  switch (*atr_value(a)) {
  case 'T':
  case 't':
  case 'P':
  case 'p':
    return 3;
  case 'M':
  case 'm':
    return 2;
  case 'F':
  case 'f':
  case 'W':
  case 'w':
    return 1;
  default:
    return 0;
  }
}

char *subj[4];	/**< Subjective pronouns */
char *poss[4];	/**< Possessive pronouns */
char *obj[4];	/**< Objective pronouns */
char *absp[4];	/**< Absolute possessive pronouns */

/** Macro to set a pronoun entry based on whether we're translating or not */
#define SET_PRONOUN(p,v,u)  p = strdup((translate) ? (v) : (u))

/** Initialize the pronoun translation strings.
 * This function sets up the values of the arrays of subjective,
 * possessive, objective, and absolute possessive pronouns with
 * locale-appropriate values.
 */
void
init_pronouns(void)
{
  int translate = 0;
#ifdef HAS_SETLOCALE
  char *loc;
  if ((loc = setlocale(LC_MESSAGES, NULL))) {
    if (strcmp(loc, "C") && strncmp(loc, "en", 2))
      translate = 1;
  }
#endif
  SET_PRONOUN(subj[0], T("pronoun:neuter,subjective"), "it");
  SET_PRONOUN(subj[1], T("pronoun:feminine,subjective"), "she");
  SET_PRONOUN(subj[2], T("pronoun:masculine,subjective"), "he");
  SET_PRONOUN(subj[3], T("pronoun:plural,subjective"), "they");
  SET_PRONOUN(poss[0], T("pronoun:neuter,possessive"), "its");
  SET_PRONOUN(poss[1], T("pronoun:feminine,possessive"), "her");
  SET_PRONOUN(poss[2], T("pronoun:masculine,possessive"), "his");
  SET_PRONOUN(poss[3], T("pronoun:plural,possessive"), "their");
  SET_PRONOUN(obj[0], T("pronoun:neuter,objective"), "it");
  SET_PRONOUN(obj[1], T("pronoun:feminine,objective"), "her");
  SET_PRONOUN(obj[2], T("pronoun:masculine,objective"), "him");
  SET_PRONOUN(obj[3], T("pronoun:plural,objective"), "them");
  SET_PRONOUN(absp[0], T("pronoun:neuter,absolute possessive"), "its");
  SET_PRONOUN(absp[1], T("pronoun:feminine,absolute possessive"), "hers");
  SET_PRONOUN(absp[2], T("pronoun:masculine,absolute possessive"), "his");
  SET_PRONOUN(absp[3], T("pronoun:plural,absolute possessive "), "theirs");
}

#undef SET_PRONOUN

/* ARGSUSED */
FUNCTION(fun_isword)
{
  /* is every character a letter? */
  char *p;
  if (!args[0] || !*args[0]) {
    safe_chr('0', buff, bp);
    return;
  }
  for (p = args[0]; *p; p++) {
    if (!isalpha((unsigned char) *p)) {
      safe_chr('0', buff, bp);
      return;
    }
  }
  safe_chr('1', buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_capstr)
{
  char *p;
  p = skip_leading_ansi(args[0]);
  if (!p) {
    safe_strl(args[0], arglens[0], buff, bp);
    return;
  } else if (p != args[0]) {
    char x = *p;
    *p = '\0';
    safe_strl(args[0], p - args[0], buff, bp);
    *p = x;
  }
  if (*p) {
    safe_chr(UPCASE(*p), buff, bp);
    p++;
  }
  if (*p)
    safe_str(p, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_art)
{
  /* checks a word and returns the appropriate article, "a" or "an" */
  char c;
  char *p = skip_leading_ansi(args[0]);

  if (!p) {
    safe_chr('a', buff, bp);
    return;
  }
  c = tolower(*p);
  if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u')
    safe_str("an", buff, bp);
  else
    safe_chr('a', buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_subj)
{
  dbref thing;

  thing = match_thing(executor, args[0]);
  if (thing == NOTHING) {
    safe_str(T(e_match), buff, bp);
    return;
  }
  safe_str(subj[get_gender(thing)], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_poss)
{
  dbref thing;

  thing = match_thing(executor, args[0]);
  if (thing == NOTHING) {
    safe_str(T(e_match), buff, bp);
    return;
  }
  safe_str(poss[get_gender(thing)], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_obj)
{
  dbref thing;

  thing = match_thing(executor, args[0]);
  if (thing == NOTHING) {
    safe_str(T(e_match), buff, bp);
    return;
  }
  safe_str(obj[get_gender(thing)], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_aposs)
{
  dbref thing;

  thing = match_thing(executor, args[0]);
  if (thing == NOTHING) {
    safe_str(T(e_match), buff, bp);
    return;
  }
  safe_str(absp[get_gender(thing)], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_alphamax)
{
  char amax[BUFFER_LEN];
  char *c;
  int j, m = 0;
  size_t len;

  c = remove_markup(args[0], &len);
  memcpy(amax, c, len);
  for (j = 1; j < nargs; j++) {
    c = remove_markup(args[j], &len);
    if (strcoll(amax, c) < 0) {
      memcpy(amax, c, len);
      m = j;
    }
  }
  safe_strl(args[m], arglens[m], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_alphamin)
{
  char amin[BUFFER_LEN];
  char *c;
  int j, m = 0;
  size_t len;

  c = remove_markup(args[0], &len);
  memcpy(amin, c, len);
  for (j = 1; j < nargs; j++) {
    c = remove_markup(args[j], &len);
    if (strcoll(amin, c) > 0) {
      memcpy(amin, c, len);
      m = j;
    }
  }
  safe_strl(args[m], arglens[m], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_strlen)
{
  safe_integer(ansi_strlen(args[0]), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_mid)
{
  ansi_string *as;
  int pos, len;

  if (!is_integer(args[1]) || !is_integer(args[2])) {
    safe_str(T(e_ints), buff, bp);
    return;
  }

  as = parse_ansi_string(args[0]);
  pos = parse_integer(args[1]);
  len = parse_integer(args[2]);

  if (pos < 0) {
    safe_str(T(e_range), buff, bp);
    free_ansi_string(as);
    return;
  }

  if (len < 0) {
    pos = pos + len + 1;
    if (pos < 0)
      pos = 0;
    len = -len;
  }

  safe_ansi_string(as, pos, len, buff, bp);
  free_ansi_string(as);
}

/* ARGSUSED */
FUNCTION(fun_left)
{
  int len;
  ansi_string *as;

  if (!is_integer(args[1])) {
    safe_str(T(e_int), buff, bp);
    return;
  }
  len = parse_integer(args[1]);

  if (len < 0) {
    safe_str(T(e_range), buff, bp);
    return;
  }

  as = parse_ansi_string(args[0]);
  safe_ansi_string(as, 0, len, buff, bp);
  free_ansi_string(as);
}

/* ARGSUSED */
FUNCTION(fun_right)
{
  int len;
  ansi_string *as;

  if (!is_integer(args[1])) {
    safe_str(T(e_int), buff, bp);
    return;
  }
  len = parse_integer(args[1]);

  if (len < 0) {
    safe_str(T(e_range), buff, bp);
    return;
  }

  as = parse_ansi_string(args[0]);
  if ((size_t) len > as->len)
    safe_strl(args[0], arglens[0], buff, bp);
  else
    safe_ansi_string(as, as->len - len, as->len, buff, bp);
  free_ansi_string(as);
}

/* ARGSUSED */
FUNCTION(fun_strinsert)
{
  /* Insert a string into another */
  ansi_string *as;
  int pos;

  if (!is_integer(args[1])) {
    safe_str(e_int, buff, bp);
    return;
  }

  pos = parse_integer(args[1]);
  if (pos < 0) {
    safe_str(T(e_range), buff, bp);
    return;
  }

  as = parse_ansi_string(args[0]);

  if ((size_t) pos > as->len) {
    /* Fast special case - concatenate args[2] to args[0] */
    safe_strl(args[0], arglens[0], buff, bp);
    safe_strl(args[2], arglens[0], buff, bp);
    free_ansi_string(as);
    return;
  }

  safe_ansi_string(as, 0, pos, buff, bp);
  safe_strl(args[2], arglens[2], buff, bp);
  safe_ansi_string(as, pos, as->len, buff, bp);
  free_ansi_string(as);

}

/* ARGSUSED */
FUNCTION(fun_delete)
{
  ansi_string *as;
  int pos, pos2, num;

  if (!is_integer(args[1]) || !is_integer(args[2])) {
    safe_str(T(e_ints), buff, bp);
    return;
  }

  pos = parse_integer(args[1]);
  num = parse_integer(args[2]);

  if (pos < 0) {
    safe_str(T(e_range), buff, bp);
    return;
  }

  as = parse_ansi_string(args[0]);

  if ((size_t) pos > as->len || num == 0) {
    safe_strl(args[0], arglens[0], buff, bp);
    free_ansi_string(as);
    return;
  }

  if (num < 0) {
    pos2 = pos + 1;
    pos = pos2 + num;
    if (pos < 0)
      pos = 0;
  } else
    pos2 = pos + num;

  safe_ansi_string(as, 0, pos, buff, bp);
  safe_ansi_string(as, pos2, as->len, buff, bp);
  free_ansi_string(as);
}

/* ARGSUSED */
FUNCTION(fun_strreplace)
{
  ansi_string *as, *anew;
  int start, len, end;

  if (!is_integer(args[1]) || !is_integer(args[2])) {
    safe_str(T(e_ints), buff, bp);
    return;
  }

  start = parse_integer(args[1]);
  len = parse_integer(args[2]);

  if (start < 0 || len < 0) {
    safe_str(T(e_range), buff, bp);
    return;
  }

  as = parse_ansi_string(args[0]);
  anew = parse_ansi_string(args[3]);

  safe_ansi_string(as, 0, start, buff, bp);
  safe_ansi_string(anew, 0, anew->len, buff, bp);

  end = start + len;

  if ((size_t) end < as->len)
    safe_ansi_string(as, end, as->len - end, buff, bp);

  free_ansi_string(as);
  free_ansi_string(anew);

}

static int
comp_gencomp(dbref executor, char *left, char *right, char *type)
{
  int c;
  c = gencomp(executor, left, right, type);
  return (c > 0 ? 1 : (c < 0 ? -1 : 0));
}

/* ARGSUSED */
FUNCTION(fun_comp)
{
  char type = 'A';

  if (nargs == 3 && !(args[2] && *args[2])) {
    safe_str(T("#-1 INVALID THIRD ARGUMENT"), buff, bp);
    return;
  } else if (nargs == 3) {
    type = toupper(*args[2]);
  }

  switch (type) {
  case 'A':			/* Case-sensitive lexicographic */
    {
      char left[BUFFER_LEN], right[BUFFER_LEN], *l, *r;
      size_t llen, rlen;
      l = remove_markup(args[0], &llen);
      memcpy(left, l, llen);
      r = remove_markup(args[1], &rlen);
      memcpy(right, r, rlen);
      safe_integer(comp_gencomp(executor, left, right, ALPHANUM_LIST), buff,
		   bp);
      return;
    }
  case 'I':			/* Case-insensitive lexicographic */
    {
      char left[BUFFER_LEN], right[BUFFER_LEN], *l, *r;
      size_t llen, rlen;
      l = remove_markup(args[0], &llen);
      memcpy(left, l, llen);
      r = remove_markup(args[1], &rlen);
      memcpy(right, r, rlen);
      safe_integer(comp_gencomp(executor, left, right, INSENS_ALPHANUM_LIST),
		   buff, bp);
      return;
    }
  case 'N':			/* Integers */
    if (!is_strict_integer(args[0]) || !is_strict_integer(args[1])) {
      safe_str(T(e_ints), buff, bp);
      return;
    }
    safe_integer(comp_gencomp(executor, args[0], args[1], NUMERIC_LIST), buff,
		 bp);
    return;
  case 'F':
    if (!is_strict_number(args[0]) || !is_strict_number(args[1])) {
      safe_str(T(e_nums), buff, bp);
      return;
    }
    safe_integer(comp_gencomp(executor, args[0], args[1], FLOAT_LIST), buff,
		 bp);
    return;
  case 'D':
    {
      dbref a, b;
      a = parse_objid(args[0]);
      b = parse_objid(args[1]);
      if (a == NOTHING || b == NOTHING) {
	safe_str(T("#-1 INVALID DBREF"), buff, bp);
	return;
      }
      safe_integer(comp_gencomp(executor, args[0], args[1], DBREF_LIST), buff,
		   bp);
      return;
    }
  default:
    safe_str(T("#-1 INVALID THIRD ARGUMENT"), buff, bp);
    return;
  }
}

/* ARGSUSED */
FUNCTION(fun_pos)
{
  char tbuf[BUFFER_LEN];
  char *pos;
  size_t len;

  pos = remove_markup(args[1], &len);
  memcpy(tbuf, pos, len);
  pos = strstr(tbuf, remove_markup(args[0], NULL));
  if (pos)
    safe_integer(pos - tbuf + 1, buff, bp);
  else
    safe_str("#-1", buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_lpos)
{
  char *pos;
  char c = ' ';
  size_t n, len;
  int first = 1;

  if (args[1][0])
    c = args[1][0];

  pos = remove_markup(args[0], &len);
  for (n = 0; n < len; n++)
    if (pos[n] == c) {
      if (first)
	first = 0;
      else
	safe_chr(' ', buff, bp);
      safe_integer(n, buff, bp);
    }
}


/* ARGSUSED */
FUNCTION(fun_strmatch)
{
  char tbuf[BUFFER_LEN];
  char *t;
  size_t len;
  /* matches a wildcard pattern for an _entire_ string */

  t = remove_markup(args[0], &len);
  memcpy(tbuf, t, len);
  safe_boolean(quick_wild(remove_markup(args[1], NULL), tbuf), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_strcat)
{
  int j;

  for (j = 0; j < nargs; j++)
    safe_strl(args[j], arglens[j], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_flip)
{
  ansi_string *as;
  as = parse_ansi_string(args[0]);
  flip_ansi_string(as);
  safe_ansi_string(as, 0, as->len, buff, bp);
  free_ansi_string(as);
}

/* ARGSUSED */
FUNCTION(fun_merge)
{
  /* given s1, s2, and a list of characters, for each character in s1,
   * if the char is in the list, replace it with the corresponding
   * char in s2.
   */

  char *str, *rep;
  char matched[UCHAR_MAX + 1];

  /* do length checks first */
  if (arglens[0] != arglens[1]) {
    safe_str(T("#-1 STRING LENGTHS MUST BE EQUAL"), buff, bp);
    return;
  }

  memset(matched, 0, sizeof matched);

  /* find the characters to look for */
  if (!args[2] || !*args[2])
    matched[(unsigned char) ' '] = 1;
  else {
    unsigned char *p;
    for (p = (unsigned char *) args[2]; p && *p; p++)
      matched[*p] = 1;
  }

  /* walk strings, copy from the appropriate string */
  for (str = args[0], rep = args[1]; *str && *rep; str++, rep++) {
    *str = matched[(unsigned char) *str] ? *rep : *str;
  }
  safe_str(args[0], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_tr)
{
  /* given str, s1, s2, for each character in str, if the char
   * is in s1, replace it with the char at the same index in s2.
   */

  char charmap[256];
  char instr[BUFFER_LEN], outstr[BUFFER_LEN];
  char rawstr[BUFFER_LEN];
  char *ip, *op;
  size_t i, len;
  char *c;
  ansi_string *as;

  /* No ansi allowed in find or replace lists */
  c = remove_markup(args[1], &len);
  memcpy(rawstr, c, len);

  /* do length checks first */

  for (i = 0; i < 256; i++) {
    charmap[i] = (char) i;
  }

  ip = instr;
  op = outstr;

  for (i = 0; i < len; i++) {
    safe_chr(rawstr[i], instr, &ip);
    /* Handle a range of characters */
    if (i != len - 1 && rawstr[i + 1] == '-' && i != len - 2) {
      int dir, sentinel, cur;

      if (rawstr[i] < rawstr[i + 2])
	dir = 1;
      else
	dir = -1;

      sentinel = rawstr[i + 2] + dir;
      cur = rawstr[i] + dir;

      while (cur != sentinel) {
	safe_chr((char) cur, instr, &ip);
	cur += dir;
      }
      i += 2;
    }
  }

  c = remove_markup(args[2], &len);
  memcpy(rawstr, c, len);
  for (i = 0; i < len; i++) {
    safe_chr(rawstr[i], outstr, &op);
    /* Handle a range of characters */
    if (i != len - 1 && rawstr[i + 1] == '-' && i != len - 2) {
      int dir, sentinel, cur;

      if (rawstr[i] < rawstr[i + 2])
	dir = 1;
      else
	dir = -1;

      sentinel = rawstr[i + 2] + dir;
      cur = rawstr[i] + dir;

      while (cur != sentinel) {
	safe_chr((char) cur, outstr, &op);
	cur += dir;
      }
      i += 2;
    }
  }

  if ((ip - instr) != (op - outstr)) {
    safe_str(T("#-1 STRING LENGTHS MUST BE EQUAL"), buff, bp);
    return;
  }

  len = ip - instr;

  for (i = 0; i < len; i++)
    charmap[(unsigned char) instr[i]] = outstr[i];

  /* walk the string, translating characters */
  as = parse_ansi_string(args[0]);
  populate_codes(as);
  len = as->len;
  for (i = 0; i < len; i++) {
    as->text[i] = charmap[(unsigned char) as->text[i]];
  }
  safe_ansi_string(as, 0, as->len, buff, bp);
  free_ansi_string(as);
}

/* ARGSUSED */
FUNCTION(fun_lcstr)
{
  char *p, *y;
  p = args[0];
  while (*p) {
    y = skip_leading_ansi(p);
    if (y != p) {
      char t;
      t = *y;
      *y = '\0';
      safe_str(p, buff, bp);
      *y = t;
      p = y;
    }
    if (*p) {
      safe_chr(DOWNCASE(*p), buff, bp);
      p++;
    }
  }
}

/* ARGSUSED */
FUNCTION(fun_ucstr)
{
  char *p, *y;
  p = args[0];
  while (*p) {
    y = skip_leading_ansi(p);
    if (y != p) {
      char t;
      t = *y;
      *y = '\0';
      safe_str(p, buff, bp);
      *y = t;
      p = y;
    }
    if (*p) {
      safe_chr(UPCASE(*p), buff, bp);
      p++;
    }
  }
}

/* ARGSUSED */
FUNCTION(fun_repeat)
{
  int times;
  char *ap;

  if (!is_integer(args[1])) {
    safe_str(T(e_int), buff, bp);
    return;
  }
  times = parse_integer(args[1]);
  if (times < 0) {
    safe_str(T("#-1 ARGUMENT MUST BE NON-NEGATIVE INTEGER"), buff, bp);
    return;
  }
  if (!*args[0])
    return;

  /* Special-case repeating one character */
  if (arglens[0] == 1) {
    safe_fill(args[0][0], times, buff, bp);
    return;
  }

  /* Do the repeat in O(lg n) time. */
  /* This takes advantage of the fact that we're given a BUFFER_LEN
   * buffer for args[0] that we are free to trash.  Huzzah! */
  ap = args[0] + arglens[0];
  while (times) {
    if (times & 1) {
      if (safe_strl(args[0], arglens[0], buff, bp) != 0)
	break;
    }
    safe_str(args[0], args[0], &ap);
    *ap = '\0';
    arglens[0] = strlen(args[0]);
    times = times >> 1;
  }
}

/* ARGSUSED */
FUNCTION(fun_scramble)
{
  int n, i, j;
  ansi_string *as;

  if (!*args[0])
    return;

  as = parse_ansi_string(args[0]);
  populate_codes(as);
  n = as->len;
  for (i = 0; i < n; i++) {
    char t, *tcode;
    j = get_random_long(i, n - 1);
    t = as->text[j];
    as->text[j] = as->text[i];
    as->text[i] = t;
    tcode = as->codes[j];
    as->codes[j] = as->codes[i];
    as->codes[i] = tcode;
  }
  safe_ansi_string(as, 0, as->len, buff, bp);
  free_ansi_string(as);
}

/* ARGSUSED */
FUNCTION(fun_ljust)
{
  /* pads a string with trailing blanks (or other fill character) */

  size_t spaces, len;
  char sep;

  if (!is_uinteger(args[1])) {
    safe_str(T(e_uint), buff, bp);
    return;
  }
  len = ansi_strlen(args[0]);
  spaces = parse_uinteger(args[1]);
  if (spaces >= BUFFER_LEN)
    spaces = BUFFER_LEN - 1;

  if (len >= spaces) {
    safe_strl(args[0], arglens[0], buff, bp);
    return;
  }
  spaces -= len;

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  safe_strl(args[0], arglens[0], buff, bp);
  safe_fill(sep, spaces, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_rjust)
{
  /* pads a string with leading blanks (or other fill character) */

  size_t spaces, len;
  char sep;

  if (!is_uinteger(args[1])) {
    safe_str(T(e_uint), buff, bp);
    return;
  }
  len = ansi_strlen(args[0]);
  spaces = parse_uinteger(args[1]);
  if (spaces >= BUFFER_LEN)
    spaces = BUFFER_LEN - 1;

  if (len >= spaces) {
    safe_strl(args[0], arglens[0], buff, bp);
    return;
  }
  spaces -= len;

  if (!delim_check(buff, bp, nargs, args, 3, &sep))
    return;

  safe_fill(sep, spaces, buff, bp);
  safe_strl(args[0], arglens[0], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_center)
{
  /* pads a string with leading blanks (or other fill string) */
  size_t width, len, lsp, rsp, filllen;
  int fillq, fillr, i;
  char fillstr[BUFFER_LEN], *fp;
  ansi_string *as;

  if (!is_uinteger(args[1])) {
    safe_str(T(e_uint), buff, bp);
    return;
  }
  width = parse_uinteger(args[1]);
  len = ansi_strlen(args[0]);
  if (len >= width) {
    safe_strl(args[0], arglens[0], buff, bp);
    return;
  }
  lsp = rsp = (width - len) / 2;
  rsp += (width - len) % 2;
  if (lsp >= BUFFER_LEN)
    lsp = rsp = BUFFER_LEN - 1;

  if ((!args[2] || !*args[2]) && (!args[3] || !*args[3])) {
    /* Fast case for default fill with spaces */
    safe_fill(' ', lsp, buff, bp);
    safe_strl(args[0], arglens[0], buff, bp);
    safe_fill(' ', rsp, buff, bp);
    return;
  }

  /* args[2] contains the possibly ansi, multi-char fill string */
  filllen = ansi_strlen(args[2]);
  if (!filllen) {
    safe_str(T("#-1 FILL ARGUMENT MAY NOT BE ZERO-LENGTH"), buff, bp);
    return;
  }
  as = parse_ansi_string(args[2]);
  fillq = lsp / filllen;
  fillr = lsp % filllen;
  fp = fillstr;
  for (i = 0; i < fillq; i++)
    safe_ansi_string(as, 0, as->len, fillstr, &fp);
  safe_ansi_string(as, 0, fillr, fillstr, &fp);
  *fp = '\0';
  free_ansi_string(as);
  safe_str(fillstr, buff, bp);
  safe_strl(args[0], arglens[0], buff, bp);
  /* If we have args[3], that's the right-side fill string */
  if (nargs > 3) {
    if (args[3] && *args[3]) {
      filllen = ansi_strlen(args[3]);
      if (!filllen) {
	safe_str(T("#-1 FILL ARGUMENT MAY NOT BE ZERO-LENGTH"), buff, bp);
	return;
      }
      as = parse_ansi_string(args[3]);
      fillq = rsp / filllen;
      fillr = rsp % filllen;
      fp = fillstr;
      for (i = 0; i < fillq; i++)
      safe_ansi_string(as, 0, as->len, fillstr, &fp);
      safe_ansi_string(as, 0, fillr, fillstr, &fp);
      *fp = '\0';
      free_ansi_string(as);
      safe_str(fillstr, buff, bp);
    } else {
      /* Null args[3], fill right side with spaces */
      safe_fill(' ', rsp, buff, bp);
    }
    return;
  }
  /* No args[3], so we flip args[2] */
  filllen = ansi_strlen(args[2]);
  as = parse_ansi_string(args[2]);
  fillq = rsp / filllen;
  fillr = rsp % filllen;
  fp = fillstr;
  for (i = 0; i < fillq; i++)
    safe_ansi_string(as, 0, as->len, fillstr, &fp);
  safe_ansi_string(as, 0, fillr, fillstr, &fp);
  *fp = '\0';
  free_ansi_string(as);
  as = parse_ansi_string(fillstr);
  flip_ansi_string(as);
  safe_ansi_string(as, 0, as->len, buff, bp);
  free_ansi_string(as);
}

/* ARGSUSED */
FUNCTION(fun_foreach)
{
  /* Like map(), but it operates on a string, rather than on a list,
   * calling a user-defined function for each character in the string.
   * No delimiter is inserted between the results.
   */

  dbref thing;
  ATTR *attrib;
  char const *ap, *lp;
  char *asave, cbuf[2];
  char *tptr[2];
  char place[SBUF_LEN];
  int placenr = 0;
  int funccount;
  char *oldbp;
  char start, end;
  char letters[BUFFER_LEN];
  size_t len;

  if (nargs >= 3) {
    if (!delim_check(buff, bp, nargs, args, 3, &start))
      return;
  }

  if (nargs == 4) {
    if (!delim_check(buff, bp, nargs, args, 4, &end))
      return;
  } else {
    end = '\0';
  }

  /* find our object and attribute */
  parse_anon_attrib(executor, args[0], &thing, &attrib);
  if (!GoodObject(thing) || !attrib || !Can_Read_Attr(executor, thing, attrib)) {
    free_anon_attrib(attrib);
    return;
  }
  strcpy(place, "0");
  asave = safe_atr_value(attrib);

  /* save our stack */
  tptr[0] = global_eval_context.wenv[0];
  tptr[1] = global_eval_context.wenv[1];
  global_eval_context.wenv[1] = place;

  ap = remove_markup(args[1], &len);
  memcpy(letters, ap, len);

  lp = trim_space_sep(letters, ' ');
  if (nargs >= 3) {
    char *tmp = strchr(lp, start);

    if (!tmp) {
      safe_str(lp, buff, bp);
      free((Malloc_t) asave);
      free_anon_attrib(attrib);
      global_eval_context.wenv[1] = tptr[1];
      return;
    }
    oldbp = place;
    placenr = (tmp + 1) - lp;
    safe_integer_sbuf(placenr, place, &oldbp);
    oldbp = *bp;

    *tmp = '\0';
    safe_str(lp, buff, bp);
    lp = tmp + 1;
  }

  cbuf[1] = '\0';
  global_eval_context.wenv[0] = cbuf;
  oldbp = *bp;
  funccount = pe_info->fun_invocations;
  while (*lp && *lp != end) {
    *cbuf = *lp++;
    ap = asave;
    if (process_expression(buff, bp, &ap, thing, executor, enactor,
			   PE_DEFAULT, PT_DEFAULT, pe_info))
      break;
    if (*bp == oldbp && pe_info->fun_invocations == funccount)
      break;
    oldbp = place;
    safe_integer_sbuf(++placenr, place, &oldbp);
    *oldbp = '\0';
    oldbp = *bp;
    funccount = pe_info->fun_invocations;
  }
  if (*lp)
    safe_str(lp + 1, buff, bp);
  free((Malloc_t) asave);
  free_anon_attrib(attrib);
  global_eval_context.wenv[0] = tptr[0];
  global_eval_context.wenv[1] = tptr[1];
}

extern char escaped_chars[UCHAR_MAX + 1];
extern char escaped_chars_s[UCHAR_MAX +1];

/* ARGSUSED */
FUNCTION(fun_decompose)
{
  /* This function simply returns a decompose'd version of
   * the included string, such that
   * s(decompose(str)) == str, down to the last space, tab,
   * and newline. Except for ansi. */
  safe_str(decompose_str(args[0]), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_secure)
{
  /* this function smashes all occurences of "unsafe" characters in a string.
   * "unsafe" characters are defined by the escaped_chars table.
   * these characters get replaced by spaces
   */
  unsigned char *p;

  for (p = (unsigned char *) args[0]; *p; p++)
    if (escaped_chars_s[*p])
      *p = ' ';

  safe_strl(args[0], arglens[0], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_escape)
{
  unsigned char *s;

  if (arglens[0]) {
    safe_chr('\\', buff, bp);
    for (s = (unsigned char *) args[0]; *s; s++) {
      if ((s != (unsigned char *) args[0]) && escaped_chars[*s])
	safe_chr('\\', buff, bp);
      safe_chr(*s, buff, bp);
    }
  }
}

/* ARGSUSED */
FUNCTION(fun_trim)
{
  /* Similar to squish() but it doesn't trim spaces in the center, and
   * takes a delimiter argument and trim style.
   */

  char *p, *q, sep;
  int trim;
  int trim_style_arg, trim_char_arg;

  /* Alas, PennMUSH and TinyMUSH used different orders for the arguments.
   * We'll give the users an option about it
   */
  if (!strcmp(called_as, "TRIMTINY")) {
    trim_style_arg = 2;
    trim_char_arg = 3;
  } else if (!strcmp(called_as, "TRIMPENN")) {
    trim_style_arg = 3;
    trim_char_arg = 2;
  } else if (TINY_TRIM_FUN) {
    trim_style_arg = 2;
    trim_char_arg = 3;
  } else {
    trim_style_arg = 3;
    trim_char_arg = 2;
  }

  if (!delim_check(buff, bp, nargs, args, trim_char_arg, &sep))
    return;

  /* If a trim style is provided, it must be the third argument. */
  if (nargs >= trim_style_arg) {
    switch (DOWNCASE(*args[trim_style_arg - 1])) {
    case 'l':
      trim = 1;
      break;
    case 'r':
      trim = 2;
      break;
    default:
      trim = 3;
      break;
    }
  } else
    trim = 3;

  /* We will never need to check for buffer length overrunning, since
   * we will always get a smaller string. Thus, we can copy at the
   * same time we skip stuff.
   */

  /* If necessary, skip over the leading stuff. */
  p = args[0];
  if (trim != 2) {
    while (*p == sep)
      p++;
  }
  /* Cut off the trailing stuff, if appropriate. */
  if ((trim != 1) && (*p != '\0')) {
    q = args[0] + arglens[0] - 1;
    while ((q > p) && (*q == sep))
      q--;
    q[1] = '\0';
  }
  safe_str(p, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_lit)
{
  /* Just returns the argument, literally */
  safe_strl(args[0], arglens[0], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_squish)
{
  /* zaps leading and trailing spaces, and reduces other spaces to a single
   * space. This only applies to the literal space character, and not to
   * tabs, newlines, etc.
   * We do not need to check for buffer length overflows, since we're
   * never going to end up with a longer string.
   */

  char *tp;
  char sep;

  /* Figure out the character to squish */
  if (!delim_check(buff, bp, nargs, args, 2, &sep))
    return;

  /* get rid of trailing spaces first, so we don't have to worry about
   * them later.
   */
  tp = args[0] + arglens[0] - 1;
  while ((tp > args[0]) && (*tp == sep))
    tp--;
  tp[1] = '\0';

  for (tp = args[0]; *tp == sep; tp++)	/* skip leading spaces */
    ;

  while (*tp) {
    safe_chr(*tp, buff, bp);
    if (*tp == sep)
      while (*tp == sep)
	tp++;
    else
      tp++;
  }
}

/* ARGSUSED */
FUNCTION(fun_space)
{
  int s;

  if (!is_uinteger(args[0])) {
    safe_str(T(e_uint), buff, bp);
    return;
  }
  s = parse_integer(args[0]);
  safe_fill(' ', s, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_beep)
{
  int k;

  /* this function prints 1 to 5 beeps. The alert character '\a' is
   * an ANSI C invention; non-ANSI-compliant implementations may ignore
   * the '\' character and just print an 'a', or do something else nasty,
   * so we define it to be something reasonable in ansi.h.
   */

  if (nargs) {
    if (!is_integer(args[0])) {
      safe_str(T(e_int), buff, bp);
      return;
    }
    k = parse_integer(args[0]);
  } else
    k = 1;

  if (!Admin(executor) || (k <= 0) || (k > 5)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  safe_fill(BEEP_CHAR, k, buff, bp);
}

/** Initialize the html tag hash table with all the safe tags from HTML 4.0 */
void
init_tag_hashtab(void)
{
  static char dummy = 1;
  int i = 0;
  char tbuf[32];
  char *tbp;
  static const char *safetags[] = { "A", "B", "I", "U", "STRONG", "EM",
    "ADDRESS", "BLOCKQUOTE", "CENTER", "DEL", "DIV",
    "H1", "H2", "H3", "H4", "H5", "H6", "HR", "INS",
    "P", "PRE", "DIR", "DL", "DT", "DD", "LI", "MENU", "OL", "UL",
    "TABLE", "CAPTION", "COLGROUP", "COL", "THEAD", "TFOOT",
    "TBODY", "TR", "TD", "TH",
    "BR", "FONT", "IMG", "SPAN", "SUB", "SUP",
    "ABBR", "ACRONYM", "CITE", "CODE", "DFN", "KBD", "SAMP", "VAR",
    "BIG", "S", "SMALL", "STRIKE", "TT",
    NULL
  };
  hashinit(&htab_tag, 64, 1);

  while(safetags[i]) {
    memset(tbuf, '\0', 32);
    tbp = tbuf;
    hashadd(safetags[i], (void *) &dummy, &htab_tag);
    safe_format( tbuf, &tbp, "/%s", safetags[i] );
    hashadd(tbuf, (void *) &dummy, &htab_tag);
    i++;
  }
}

FUNCTION(fun_ord)
{
  char *m;
  size_t len = 0;
  if (!args[0] || !args[0][0]) {
    safe_str(T("#-1 FUNCTION EXPECTS ONE CHARACTER"), buff, bp);
    return;
  }
  m = remove_markup(args[0], &len);

  if (len != 2)			/* len includes trailing nul */
    safe_str(T("#-1 FUNCTION EXPECTS ONE CHARACTER"), buff, bp);
  else if (isprint((unsigned char) *m))
    safe_integer((unsigned char) *m, buff, bp);
  else
    safe_str(T("#-1 UNPRINTABLE CHARACTER"), buff, bp);
}

FUNCTION(fun_chr)
{
  int c;

  if (!is_integer(args[0])) {
    safe_str(T(e_uint), buff, bp);
    return;
  }
  c = parse_integer(args[0]);
  if (c < 0 || c > UCHAR_MAX)
    safe_str(T("#-1 THIS ISN'T UNICODE"), buff, bp);
  else if (isprint(c))
    safe_chr(c, buff, bp);
  else
    safe_str(T("#-1 UNPRINTABLE CHARACTER"), buff, bp);

}

FUNCTION(fun_accent)
{
  if (arglens[0] != arglens[1]) {
    safe_str(T("#-1 STRING LENGTHS MUST BE EQUAL"), buff, bp);
    return;
  }
  safe_accent(args[0], args[1], arglens[0], buff, bp);
}

FUNCTION(fun_stripaccents)
{
  int n;
  for (n = 0; n < arglens[0]; n++) {
    if (accent_table[(unsigned char) args[0][n]].base)
      safe_str(accent_table[(unsigned char) args[0][n]].base, buff, bp);
    else
      safe_chr(args[0][n], buff, bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_html)
{
  if (!Site(executor))
    safe_str(T(e_perm), buff, bp);
  else
    safe_tag(args[0], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_tag)
{
  int i;
  if (!Site(executor) && !hash_find(&htab_tag, strupper(args[0])))
    safe_str("#-1", buff, bp);
  else {
    safe_chr(TAG_START, buff, bp);
    safe_strl(args[0], arglens[0], buff, bp);
    for (i = 1; i < nargs; i++) {
      if (ok_tag_attribute(executor, args[i])) {
	safe_chr(' ', buff, bp);
	safe_strl(args[i], arglens[i], buff, bp);
      }
    }
    safe_chr(TAG_END, buff, bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_endtag)
{
  if (!Site(executor) && !hash_find(&htab_tag, strupper(args[0])))
    safe_str("#-1", buff, bp);
  else
    safe_tag_cancel(args[0], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_tagwrap)
{
  if (!Site(executor) && !hash_find(&htab_tag, strupper(args[0])))
    safe_str("#-1", buff, bp);
  else {
    if (nargs == 2)
      safe_tag_wrap(args[0], NULL, args[1], buff, bp, executor);
    else
      safe_tag_wrap(args[0], args[1], args[2], buff, bp, executor);
  }
}

#define COL_FLASH       (1)	/**< ANSI flash attribute bit */
#define COL_HILITE      (2)	/**< ANSI hilite attribute bit */
#define COL_INVERT      (4)	/**< ANSI inverse attribute bit */
#define COL_UNDERSCORE  (8)	/**< ANSI underscore attribute bit */

#define VAL_FLASH       (5)	/**< ANSI flag attribute value */
#define VAL_HILITE      (1)	/**< ANSI hilite attribute value */
#define VAL_INVERT      (7)	/**< ANSI inverse attribute value */
#define VAL_UNDERSCORE  (4)	/**< ANSI underscore attribute value */

#define COL_BLACK       (30)	/**< ANSI color black */
#define COL_RED         (31)	/**< ANSI color red */
#define COL_GREEN       (32)	/**< ANSI color green */
#define COL_YELLOW      (33)	/**< ANSI color yellow */
#define COL_BLUE        (34)	/**< ANSI color blue */
#define COL_MAGENTA     (35)	/**< ANSI color magenta */
#define COL_CYAN        (36)	/**< ANSI color cyan */
#define COL_WHITE       (37)	/**< ANSI color white */

/** The ansi attributes associated with a character. */
typedef struct {
  char flags;		/**< Ansi text attributes */
  char fore;		/**< Ansi foreground color */
  char back;		/**< Ansi background color */
} ansi_data;

static void dump_ansi_codes(ansi_data * ad, char *buff, char **bp);

/** If we're adding y to x, do we need to add z as well? */
#define EDGE_UP(x,y,z)  (((y) & (z)) && !((x) & (z)))

static void
dump_ansi_codes(ansi_data * ad, char *buff, char **bp)
{
  static ansi_data old_ad = { 0, 0, 0 };
  int f = 0;

  if ((old_ad.fore && !ad->fore)
      || (old_ad.back && !ad->back)
      || ((old_ad.flags & ad->flags) != old_ad.flags)) {
    safe_str(ANSI_NORMAL, buff, bp);
    old_ad.flags = 0;
    old_ad.fore = 0;
    old_ad.back = 0;
  }

  if ((old_ad.fore == ad->fore)
      && (old_ad.back == ad->back)
      && (old_ad.flags == ad->flags))
    /* If nothing has changed, don't bother doing anything.
     * This stops the entirely pointless \e[m being generated. */
    return;

  safe_str(ANSI_BEGIN, buff, bp);

  if (EDGE_UP(old_ad.flags, ad->flags, COL_FLASH)) {
    if (f++)
      safe_chr(';', buff, bp);
    safe_integer(VAL_FLASH, buff, bp);
  }

  if (EDGE_UP(old_ad.flags, ad->flags, COL_HILITE)) {
    if (f++)
      safe_chr(';', buff, bp);
    safe_integer(VAL_HILITE, buff, bp);
  }

  if (EDGE_UP(old_ad.flags, ad->flags, COL_INVERT)) {
    if (f++)
      safe_chr(';', buff, bp);
    safe_integer(VAL_INVERT, buff, bp);
  }

  if (EDGE_UP(old_ad.flags, ad->flags, COL_UNDERSCORE)) {
    if (f++)
      safe_chr(';', buff, bp);
    safe_integer(VAL_UNDERSCORE, buff, bp);
  }

  if (ad->fore != old_ad.fore) {
    if (f++)
      safe_chr(';', buff, bp);
    safe_integer(ad->fore, buff, bp);
  }

  if (ad->back != old_ad.back) {
    if (f++)
      safe_chr(';', buff, bp);
    safe_integer(ad->back + 10, buff, bp);
  }

  safe_str(ANSI_END, buff, bp);

  old_ad = *ad;

}


/* ARGSUSED */
FUNCTION(fun_ansi)
{
  static char tbuff[BUFFER_LEN];
  static ansi_data stack[1024] = { {0, 0, 0} }, *sp = stack;
  char const *arg0, *arg1;
  char *tbp;

  tbp = tbuff;
  arg0 = args[0];
  process_expression(tbuff, &tbp, &arg0, executor, caller, enactor,
		     PE_DEFAULT, PT_DEFAULT, pe_info);
  *tbp = '\0';

  sp[1] = sp[0];
  sp++;

  for (tbp = tbuff; *tbp; tbp++) {
    switch (*tbp) {
    case 'n':			/* normal */
      sp->flags = 0;
      sp->fore = 0;
      sp->back = 0;
      break;
    case 'f':			/* flash */
      sp->flags |= COL_FLASH;
      break;
    case 'h':			/* hilite */
      sp->flags |= COL_HILITE;
      break;
    case 'i':			/* inverse */
      sp->flags |= COL_INVERT;
      break;
    case 'u':			/* underscore */
      sp->flags |= COL_UNDERSCORE;
      break;
    case 'F':			/* flash */
      sp->flags &= ~COL_FLASH;
      break;
    case 'H':			/* hilite */
      sp->flags &= ~COL_HILITE;
      break;
    case 'I':			/* inverse */
      sp->flags &= ~COL_INVERT;
      break;
    case 'U':			/* underscore */
      sp->flags &= ~COL_UNDERSCORE;
      break;
    case 'b':			/* blue fg */
      sp->fore = COL_BLUE;
      break;
    case 'c':			/* cyan fg */
      sp->fore = COL_CYAN;
      break;
    case 'g':			/* green fg */
      sp->fore = COL_GREEN;
      break;
    case 'm':			/* magenta fg */
      sp->fore = COL_MAGENTA;
      break;
    case 'r':			/* red fg */
      sp->fore = COL_RED;
      break;
    case 'w':			/* white fg */
      sp->fore = COL_WHITE;
      break;
    case 'x':			/* black fg */
      sp->fore = COL_BLACK;
      break;
    case 'y':			/* yellow fg */
      sp->fore = COL_YELLOW;
      break;
    case 'B':			/* blue bg */
      sp->back = COL_BLUE;
      break;
    case 'C':			/* cyan bg */
      sp->back = COL_CYAN;
      break;
    case 'G':			/* green bg */
      sp->back = COL_GREEN;
      break;
    case 'M':			/* magenta bg */
      sp->back = COL_MAGENTA;
      break;
    case 'R':			/* red bg */
      sp->back = COL_RED;
      break;
    case 'W':			/* white bg */
      sp->back = COL_WHITE;
      break;
    case 'X':			/* black bg */
      sp->back = COL_BLACK;
      break;
    case 'Y':			/* yellow bg */
      sp->back = COL_YELLOW;
      break;
    }
  }

  dump_ansi_codes(sp, buff, bp);

  arg1 = args[1];
  process_expression(buff, bp, &arg1, executor, caller, enactor,
		     PE_DEFAULT, PT_DEFAULT, pe_info);

  dump_ansi_codes(--sp, buff, bp);

}

/* ARGSUSED */
FUNCTION(fun_stripansi)
{
  /* Strips ANSI codes away from a given string of text. Starts by
   * finding the '\x' character and stripping until it hits an 'm'.
   */

  char *cp;

  cp = remove_markup(args[0], NULL);
  safe_str(cp, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_edit)
{
  int i;
  char *f, *r, *raw;
  ansi_string *prebuf;
  char postbuf[BUFFER_LEN], lastbuf[BUFFER_LEN], *postp;
  size_t rlen, flen;

  prebuf = parse_ansi_string(args[0]);
  raw = args[0];
  for (i = 1; i < nargs - 1; i += 2) {

    postp = postbuf;
    f = args[i];		/* find this */
    r = args[i + 1];		/* replace it with this */
    flen = arglens[i];
    rlen = arglens[i + 1];

    /* Check for nothing to avoid infinite loop */
    if (!*f && !*r)
      continue;

    if (flen == 1 && *f == '$') {
      /* append */
      safe_str(raw, postbuf, &postp);
      safe_strl(r, rlen, postbuf, &postp);
    } else if (flen == 1 && *f == '^') {
      /* prepend */
      safe_strl(r, rlen, postbuf, &postp);
      safe_str(raw, postbuf, &postp);
    } else if (!*f) {
      /* insert between every character */
      size_t last;
      safe_strl(r, rlen, postbuf, &postp);
      for (last = 0; last < prebuf->len; last++) {
	safe_ansi_string(prebuf, last, 1, postbuf, &postp);
	safe_strl(r, rlen, postbuf, &postp);
      }
    } else {
      char *p;
      size_t last = 0;

      while (last < prebuf->len && (p = strstr(prebuf->text + last, f)) != NULL) {
	safe_ansi_string(prebuf, last, p - (prebuf->text + last),
			 postbuf, &postp);
	safe_strl(r, rlen, postbuf, &postp);
	last = p - prebuf->text + flen;
      }
      if (last < prebuf->len)
	safe_ansi_string(prebuf, last, prebuf->len, postbuf, &postp);
    }
    *postp = '\0';
    free_ansi_string(prebuf);
    prebuf = parse_ansi_string(postbuf);
    strcpy(lastbuf, postbuf);
    raw = lastbuf;
  }
  safe_ansi_string(prebuf, 0, prebuf->len, buff, bp);
  free_ansi_string(prebuf);
}

FUNCTION(fun_brackets)
{
  char *str;
  int rbrack, lbrack, rbrace, lbrace, lcurl, rcurl;

  lcurl = rcurl = rbrack = lbrack = rbrace = lbrace = 0;
  str = args[0];		/* The string to count the brackets in */
  while (*str) {
    switch (*str) {
    case '[':
      lbrack++;
      break;
    case ']':
      rbrack++;
      break;
    case '(':
      lbrace++;
      break;
    case ')':
      rbrace++;
      break;
    case '{':
      lcurl++;
      break;
    case '}':
      rcurl++;
      break;
    default:
      break;
    }
    str++;
  }
  safe_format(buff, bp, "%d %d %d %d %d %d", lbrack, rbrack,
	      lbrace, rbrace, lcurl, rcurl);
}


/* Returns the length of str up to the first return character, 
 * or else the last space, or else 0.
 */
static int
wraplen(char *str, int maxlen)
{
  const int length = strlen(str);
  int i = 0;

  if (length <= maxlen) {
    /* Find the first return char
     * so %r will not mess with any alignment
     * functions.
     */
    while (i < length) {
      if ((str[i] == '\n') || (str[i] == '\r'))
	return i;
      i++;
    }
    return length;
  }

  /* Find the first return char
   * so %r will not mess with any alignment
   * functions.
   */
  while (i <= maxlen + 1) {
    if ((str[i] == '\n') || (str[i] == '\r'))
      return i;
    i++;
  }

  /* No return char was found. Now 
   * find the last space in str.
   */
  while (str[maxlen] != ' ' && maxlen > 0)
    maxlen--;

  return (maxlen ? maxlen : -1);
}

/** The integer in string a will be stored in v, 
 * if a is not an integer then d (efault) is stored in v. 
 */
#define initint(a, v, d) \
  do \
   if (arglens[a] == 0) { \
      v = d; \
   } else { \
     if (!is_integer(args[a])) { \
        safe_str(T(e_int), buff, bp); \
        return; \
     } \
     v = parse_integer(args[a]); \
  } \
 while (0)

FUNCTION(fun_wrap)
{
/*  args[0]  =  text to be wrapped (required)
 *  args[1]  =  line width (width) (required)
 *  args[2]  =  width of first line (width1st)
 *  args[3]  =  output delimiter (btwn lines)
 */

  char *pstr;			/* start of string */
  ansi_string *as;
  const char *pend;		/* end of string */
  int linewidth, width1st, width;
  int linenr = 0;
  const char *linesep;
  int ansiwidth, ansilen;

  if (!args[0] || !*args[0])
    return;

  initint(1, width, 72);
  width1st = width;
  if (nargs > 2)
    initint(2, width1st, width);
  if (nargs > 3)
    linesep = args[3];
  else if (NEWLINE_ONE_CHAR)
    linesep = "\n";
  else
    linesep = "\r\n";

  if (width < 2 || width1st < 2) {
    safe_str(T("#-1 WIDTH TOO SMALL"), buff, bp);
    return;
  }

  as = parse_ansi_string(args[0]);
  pstr = as->text;
  pend = as->text + as->len;

  linewidth = width1st;
  while (pstr < pend) {
    if (linenr++ == 1)
      linewidth = width;
    if ((linenr > 1) && linesep && *linesep)
      safe_str(linesep, buff, bp);

    ansiwidth = ansi_strnlen(pstr, linewidth);
    ansilen = wraplen(pstr, ansiwidth);

    if (ansilen < 0) {
      /* word doesn't fit on one line, so cut it */
      safe_ansi_string2(as, pstr - as->text, ansiwidth - 1, buff, bp);
      safe_chr('-', buff, bp);
      pstr += ansiwidth - 1;	/* move to start of next line */
    } else {
      /* normal line */
      safe_ansi_string2(as, pstr - as->text, ansilen, buff, bp);
      if (pstr[ansilen] == '\r')
	++ansilen;
      pstr += ansilen + 1;	/* move to start of next line */
    }
  }
  free_ansi_string(as);
}

#undef initint

#define AL_LEFT 1    /**< Align left */
#define AL_RIGHT 2   /**< Align right */
#define AL_CENTER 3  /**< Align center */
#define AL_REPEAT 4  /**< Repeat column */

static int
align_one_line(char *buff, char **bp, int ncols,
	       int cols[MAX_COLS], int calign[MAX_COLS], char *ptrs[MAX_COLS],
	       ansi_string *as[MAX_COLS],
	       int linenum, char *fieldsep, int fslen,
	       char *linesep, int lslen, char filler)
{
  static char line[BUFFER_LEN];
  static char segment[BUFFER_LEN];
  char *sp;
  char *ptr, *tptr;
  char *lp;
  char *lastspace;
  int i, j;
  int len;
  int cols_done;
  int skipspace;

  lp = line;
  memset(line, filler, BUFFER_LEN);
  cols_done = 0;
  for (i = 0; i < ncols; i++) {
    if (!ptrs[i] || !*ptrs[i]) {
      if (calign[i] & AL_REPEAT) {
	ptrs[i] = as[i]->text;
      } else {
	lp += cols[i];
	if (i < (ncols - 1) && fslen)
	  safe_str(fieldsep, line, &lp);
	cols_done++;
	continue;
      }
    }
    if (calign[i] & AL_REPEAT) {
      cols_done++;
    }
    for (len = 0, ptr = ptrs[i], lastspace = NULL; len < cols[i]; ptr++, len++) {
      if ((!*ptr) || (*ptr == '\n'))
	break;
      if (isspace((unsigned char) *ptr)) {
	lastspace = ptr;
      }
    }
    skipspace = 0;
    sp = segment;
    if (!*ptr) {
      if (len > 0) {
	safe_ansi_string2(as[i], ptrs[i] - (as[i]->text), len, segment, &sp);
      }
      ptrs[i] = ptr;
    } else if (*ptr == '\n') {
      for (tptr = ptr;
	   *tptr && tptr >= ptrs[i] && isspace((unsigned char) *tptr); tptr--) ;
      len = (tptr - ptrs[i]) + 1;
      if (len > 0) {
	safe_ansi_string2(as[i], ptrs[i] - (as[i]->text), len, segment, &sp);
      }
      ptrs[i] = ptr + 1;
      ptr = tptr;
    } else if (lastspace) {
      ptr = lastspace;
      skipspace = 1;
      for (tptr = ptr;
	   *tptr && tptr >= ptrs[i] && isspace((unsigned char) *tptr); tptr--) ;
      len = (tptr - ptrs[i]) + 1;
      if (len > 0) {
	safe_ansi_string2(as[i], ptrs[i] - (as[i]->text), len, segment, &sp);
      }
      ptrs[i] = lastspace;
    } else {
      if (len > 0) {
	safe_ansi_string2(as[i], ptrs[i] - (as[i]->text), len, segment, &sp);
      }
      ptrs[i] = ptr;
    }
    *sp = '\0';

    if ((calign[i] & 3) == AL_LEFT) {
      safe_str(segment, line, &lp);
      lp += cols[i] - len;
    } else if ((calign[i] & 3) == AL_RIGHT) {
      lp += cols[i] - len;
      safe_str(segment, line, &lp);
    } else if ((calign[i] & 3) == AL_CENTER) {
      j = cols[i] - len;
      lp += j >> 1;
      safe_str(segment, line, &lp);
      lp += (j >> 1) + (j & 1);
    }
    if ((lp - line) > BUFFER_LEN)
      lp = (line + BUFFER_LEN - 1);
    if (i < (ncols - 1) && fslen)
      safe_str(fieldsep, line, &lp);
    if (skipspace)
      for (;
	   *ptrs[i] && (*ptrs[i] != '\n') && isspace((unsigned char) *ptrs[i]);
	   ptrs[i]++) ;
  }
  if (cols_done == ncols)
    return 0;
  *lp = '\0';
  if (linenum > 0 && lslen > 0)
    safe_str(linesep, buff, bp);
  safe_str(line, buff, bp);
  return 1;
}


FUNCTION(fun_align)
{
  int nline;
  char *ptr;
  int ncols;
  int i;
  static int cols[MAX_COLS];
  static int calign[MAX_COLS];
  static ansi_string *as[MAX_COLS];
  static char *ptrs[MAX_COLS];
  char filler;
  char *fieldsep;
  int fslen;
  char *linesep;
  int lslen;

  filler = ' ';
  fieldsep = (char *) " ";
  linesep = (char *) "\n";

  /* Get column widths */
  ncols = 0;
  for (ptr = args[0]; *ptr; ptr++) {
    while (isspace((unsigned char) *ptr))
      ptr++;
    if (*ptr == '>') {
      calign[ncols] = AL_RIGHT;
      ptr++;
    } else if (*ptr == '-') {
      calign[ncols] = AL_CENTER;
      ptr++;
    } else if (*ptr == '<') {
      calign[ncols] = AL_LEFT;
      ptr++;
    } else if (isdigit((unsigned char) *ptr)) {
      calign[ncols] = AL_LEFT;
    } else {
      safe_str(T("#-1 INVALID ALIGN STRING"), buff, bp);
      return;
    }
    for (i = 0; *ptr && isdigit((unsigned char) *ptr); ptr++) {
      i *= 10;
      i += *ptr - '0';
    }
    if (*ptr == '.') {
      calign[ncols] |= AL_REPEAT;
      ptr++;
    }
    cols[ncols++] = i;
    if (!*ptr)
      break;
  }

  for (i = 0; i < ncols; i++) {
    if (cols[i] < 1) {
      safe_str(T("#-1 CANNOT HAVE COLUMN OF SIZE 0"), buff, bp);
      return;
    }
  }

  if (ncols < 1) {
    safe_str(T("#-1 NOT ENOUGH COLUMNS FOR ALIGN"), buff, bp);
    return;
  }
  if (ncols > MAX_COLS) {
    safe_str(T("#-1 TOO MANY COLUMNS FOR ALIGN"), buff, bp);
    return;
  }
  if (nargs < (ncols + 1) || nargs > (ncols + 4)) {
    safe_str(T("#-1 INVALID NUMBER OF ARGUMENTS TO ALIGN"), buff, bp);
    return;
  }
  if (nargs >= (ncols + 2)) {
    if (!args[ncols + 1] || strlen(args[ncols + 1]) > 1) {
      safe_str(T("#-1 FILLER MUST BE ONE CHARACTER"), buff, bp);
      return;
    }
    if (*args[ncols + 1]) {
      filler = *(args[ncols + 1]);
    }
  }
  if (nargs >= (ncols + 3)) {
    fieldsep = args[ncols + 2];
  }
  if (nargs >= (ncols + 4)) {
    linesep = args[ncols + 3];
  }

  fslen = strlen(fieldsep);
  lslen = strlen(linesep);

  for (i = 0; i < MAX_COLS; i++) {
    as[i] = NULL;
  }
  for (i = 0; i < ncols; i++) {
    as[i] = parse_ansi_string(args[i + 1]);
    ptrs[i] = as[i]->text;
  }

  nline = 0;
  while (1) {
    if (!align_one_line(buff, bp, ncols, cols, calign, ptrs,
			as, nline++, fieldsep, fslen, linesep, lslen, filler))
      break;
  }
  **bp = '\0';
  for (i = 0; i < ncols; i++) {
    free_ansi_string(as[i]);
    ptrs[i] = as[i]->text;
  }
  return;
}

FUNCTION(fun_speak)
{
  ufun_attrib transufun;
  ufun_attrib nullufun;
  dbref speaker;
  char *speaker_str;
  char *open, *close;
  int transform = 0, null = 0, say = 0;
  char *wenv[3];
  int funccount;
  int fragment = 0;
  char *say_string;
  char *string;
  char rbuff[BUFFER_LEN];
  OOREF_DECL;

  speaker = match_thing(executor, args[0]);
  if (speaker == NOTHING || speaker == AMBIGUOUS) {
    safe_str(T(e_match), buff, bp);
    return;
  }

  speaker_str = unparse_dbref(speaker);

  if (!args[1] || !*args[1])
    return;

  string = args[1];

  if (nargs > 2 && *args[2] != '\0' && *args[2] != ' ')
    say_string = args[2];
  else
    say_string = (char *) "says,";

  ENTER_OOREF;

  if (nargs > 3) {
    if (args[3] != '\0') {
      /* we have a transform attr */
      transform = 1;
      if (!fetch_ufun_attrib(args[3], executor, &transufun, 1)) {
	safe_str(T(e_atrperm), buff, bp);
        LEAVE_OOREF;
	return;
      }
      if (nargs > 4) {
	if (args[4] != '\0') {
	  /* we have an attr to use when transform returns an empty string */
	  null = 1;
	  if (!fetch_ufun_attrib(args[4], executor, &nullufun, 1)) {
	    safe_str(T(e_atrperm), buff, bp);
            LEAVE_OOREF;
	    return;
	  }
	}
      }
    }
  }

  if (nargs < 6 || args[5] == '\0')
    open = (char *) "\"";
  else
    open = args[5];
  if (nargs < 7 || args[6] == '\0')
    close = open;
  else
    close = args[6];

  switch (*string) {
  case ':':
    safe_str(Name(speaker), buff, bp);
    string++;
    if (*string == ' ') {
      /* semipose it instead */
      while (*string == ' ')
	string++;
    } else
      safe_chr(' ', buff, bp);
    break;
  case ';':
    string++;
    safe_str(Name(speaker), buff, bp);
    if (*string == ' ') {
      /* pose it instead */
      safe_chr(' ', buff, bp);
      while (*string == ' ')
	string++;
    }
    break;
  case '|':
    string++;
    break;
  case '"':
    if (CHAT_STRIP_QUOTE)
      string++;
  default:
    say = 1;
    break;
  }

  if (!transform || (!say && !strstr(string, open))) {
    /* nice and easy */
    if (say)
      safe_format(buff, bp, "%s %s \"%s\"", Name(speaker), say_string, string);
    else
      safe_str(string, buff, bp);
    LEAVE_OOREF;
    return;
  }


  if (say && !strstr(string, close)) {
    /* the whole string has to be transformed */
    wenv[0] = string;
    wenv[1] = speaker_str;
    wenv[2] = unparse_integer(fragment);
    if (call_ufun(&transufun, wenv, 3, rbuff, executor, enactor, pe_info)) {
      LEAVE_OOREF;
      return;
    }
    if (strlen(rbuff) > 0) {
      safe_format(buff, bp, "%s %s %s", Name(speaker), say_string, rbuff);
      LEAVE_OOREF;
      return;
    } else if (null == 1) {
      wenv[0] = speaker_str;
      wenv[1] = unparse_integer(fragment);
      if (call_ufun(&nullufun, wenv, 2, rbuff, executor, enactor, pe_info)) {
        LEAVE_OOREF;
	return;
      }
      safe_str(rbuff, buff, bp);
      LEAVE_OOREF;
      return;
    }
  } else {
    /* only transform portions of string between open and close */
    char *speech;
    int indx;
    int finished = 0;
    int delete = 0;

    if (say) {
      safe_str(Name(speaker), buff, bp);
      safe_chr(' ', buff, bp);
      safe_str(say_string, buff, bp);
      safe_chr(' ', buff, bp);
    }
    funccount = pe_info->fun_invocations;
    while (!finished && ((say && fragment == 0 && (speech = string))
			 || (speech = strstr(string, open)))) {
      fragment++;
      indx = string - speech;
      if (indx < 0)
	indx *= -1;
      if (string != NULL && strlen(string) > 0 && indx > 0)
	safe_strl(string, indx, buff, bp);
      if (!say || fragment > 1)
	speech = speech + strlen(open);	/* move past open char */
      /* find close-char */
      string = strstr(speech, close);
      if (!string || !(string = string + strlen(close))) {
	/* no close char, or nothing after it; we're at the end! */
	finished = 1;
      }
      delete = (string == NULL ? strlen(speech) : strlen(speech) -
		(strlen(string) + strlen(close)));
      speech = chopstr(speech, delete);
      wenv[0] = speech;
      wenv[1] = speaker_str;
      wenv[2] = unparse_integer(fragment);
      if (call_ufun(&transufun, wenv, 3, rbuff, executor, enactor, pe_info))
	break;
      if (*bp == (buff + BUFFER_LEN - 1) &&
	  pe_info->fun_invocations == funccount)
	break;
      funccount = pe_info->fun_invocations;
      if ((null == 1) && (strlen(rbuff) == 0)) {
	wenv[0] = speaker_str;
	wenv[1] = unparse_integer(fragment);
	if (call_ufun(&nullufun, wenv, 2, rbuff, executor, enactor, pe_info))
	  break;
      }
      if (strlen(rbuff) > 0) {
	safe_str(rbuff, buff, bp);
      }
      if (*bp == (buff + BUFFER_LEN - 1) &&
	  pe_info->fun_invocations == funccount)
	break;
    }
    if (string != NULL && strlen(string) > 0) {
      safe_str(string, buff, bp);	/* remaining string (not speech, so not t) */
    }
  }

  LEAVE_OOREF;
}
