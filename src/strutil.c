/**
 * \file strutil.c
 *
 * \brief String utilities for PennMUSH.
 *
 *
 */

#include "config.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include "copyrite.h"
#include "conf.h"
#include "case.h"
#include "ansi.h"
#include "pueblo.h"
#include "parse.h"
#include "externs.h"
#include "mymalloc.h"
#include "log.h"
#include "confmagic.h"

char *next_token(char *str, char sep);
int format_long(long val, char *buff, char **bp, int maxlen, int base);
static char *
mush_strndup(const char *src, size_t len, const char *check)
  __attribute_malloc__;

/* Duplicate the first len characters of s */
    static char *mush_strndup(const char *src, size_t len, const char *check)
{
  char *copy;
  size_t rlen = strlen(src);

  if (rlen < len)
    len = rlen;

  copy = mush_malloc(len + 1, check);
  if (copy) {
    memcpy(copy, src, len);
    copy[len] = '\0';
  }

  return copy;
}

/** Our version of strdup, with memory leak checking.
 * This should be used in preference to strdup, and in assocation
 * with mush_free().
 * \param s string to duplicate.
 * \param check label for memory checking.
 * \return newly allocated copy of s.
 */
char *
mush_strdup(const char *s, const char *check __attribute__ ((__unused__)))
{
  char *x;

#ifdef HAS_STRDUP
  x = strdup(s);
  if (x)
    add_check(check);
#else

  size_t len = strlen(s) + 1;
  x = mush_malloc(len, check);
  if (x)
    memcpy(x, s, len);
#endif
  return x;
}

/** Return the string chopped at lim characters.
 * \param str string to chop.
 * \param lim character at which to chop the string.
 * \return statically allocated buffer with chopped string.
 */
char *
chopstr(const char *str, size_t lim)
{
  static char tbuf1[BUFFER_LEN];
  if (strlen(str) <= lim)
    return (char *) str;
  strncpy(tbuf1, str, lim);
  tbuf1[lim] = '\0';
  return tbuf1;
}


#ifndef HAS_STRCASECMP
#ifndef WIN32
/** strcasecmp for systems without it.
 * \param s1 one string to compare.
 * \param s2 another string to compare.
 * \retval -1 s1 is less than s2.
 * \retval 0 s1 equals s2
 * \retval 1 s1 is greater than s2.
 */
int
strcasecmp(const char *s1, const char *s2)
{
  while (*s1 && *s2 && DOWNCASE(*s1) == DOWNCASE(*s2))
    s1++, s2++;

  return (DOWNCASE(*s1) - DOWNCASE(*s2));
}

/** strncasecmp for systems without it.
 * \param s1 one string to compare.
 * \param s2 another string to compare.
 * \param n number of characters to compare.
 * \retval -1 s1 is less than s2.
 * \retval 0 s1 equals s2
 * \retval 1 s1 is greater than s2.
 */
int
strncasecmp(const char *s1, const char *s2, size_t n)
{
  for (; 0 < n; ++s1, ++s2, --n)
    if (DOWNCASE(*s1) != DOWNCASE(*s2))
      return DOWNCASE(*s1) - DOWNCASE(*s2);
    else if (*s1 == 0)
      return 0;
  return 0;

}
#endif				/* !WIN32 */
#endif				/* !HAS_STRCASECMP */

/** Does string begin with prefix? 
 * This comparison is case-insensitive.
 * \param string to check.
 * \param prefix to check against.
 * \retval 1 string begins with prefix.
 * \retval 0 string does not begin with prefix.
 */
int
string_prefix(const char *RESTRICT string, const char *RESTRICT prefix)
{
  if (!string || !prefix)
    return 0;
  while (*string && *prefix && DOWNCASE(*string) == DOWNCASE(*prefix))
    string++, prefix++;
  return *prefix == '\0';
}

/** Match a substring at the start of a word in a string, case-insensitively.
 * \param src a string of words to match against.
 * \param sub a prefix to match against the start of a word in string.
 * \return pointer into src at the matched word, or NULL.
 */
const char *
string_match(const char *src, const char *sub)
{
  if (!src || !sub)
    return 0;

  if (*sub != '\0') {
    while (*src) {
      if (string_prefix(src, sub))
	return src;
      /* else scan to beginning of next word */
      while (*src && (isalpha((unsigned char) *src)
		      || isdigit((unsigned char) *src)))
	src++;
      while (*src && !isalpha((unsigned char) *src)
	     && !isdigit((unsigned char) *src))
	src++;
    }
  }
  return NULL;
}

/** Return an initial-cased version of a string in a static buffer.
 * \param s string to initial-case.
 * \return pointer to a static buffer containing the initial-cased version.
 */
char *
strinitial(const char *s)
{
  static char buf1[BUFFER_LEN];
  char *p;

  if (!s || !*s) {
    buf1[0] = '\0';
    return buf1;
  }
  strcpy(buf1, s);
  for (p = buf1; *p; p++)
    *p = DOWNCASE(*p);
  buf1[0] = UPCASE(buf1[0]);
  return buf1;
}

/** Return an uppercased version of a string in a static buffer.
 * \param s string to uppercase.
 * \return pointer to a static buffer containing the uppercased version.
 */
char *
strupper(const char *s)
{
  static char buf1[BUFFER_LEN];
  char *p;

  if (!s || !*s) {
    buf1[0] = '\0';
    return buf1;
  }
  strcpy(buf1, s);
  for (p = buf1; *p; p++)
    *p = UPCASE(*p);
  return buf1;
}

/** Return a lowercased version of a string in a static buffer.
 * \param s string to lowercase.
 * \return pointer to a static buffer containing the lowercased version.
 */
char *
strlower(const char *s)
{
  static char buf1[BUFFER_LEN];
  char *p;

  if (!s || !*s) {
    buf1[0] = '\0';
    return buf1;
  }
  strcpy(buf1, s);
  for (p = buf1; *p; p++)
    *p = DOWNCASE(*p);
  return buf1;
}

/** Modify a string in-place to uppercase.
 * \param s string to uppercase.
 * \return s, now modified to be all uppercase.
 */
char *
upcasestr(char *s)
{
  char *p;
  for (p = s; p && *p; p++)
    *p = UPCASE(*p);
  return s;
}

/** Safely add an accented string to a buffer.
 * \param base base string to which accents are applied.
 * \param tmplate accent template string.
 * \param len length of base (and tmplate).
 * \param buff pointer to buffer to store accented string.
 * \param bp pointer to pointer to insertion point in buff.
 * \retval 1 failed to store entire string.
 * \retval 0 success.
 */
int
safe_accent(const char *RESTRICT base, const char *RESTRICT tmplate, size_t len,
	    char *buff, char **bp)
{
  /* base and tmplate must be the same length */
  size_t n;
  unsigned char c;

  for (n = 0; n < len; n++) {
    switch (base[n]) {
    case 'A':
      switch (tmplate[n]) {
      case '`':
	c = 192;
	break;
      case '\'':
	c = 193;
	break;
      case '^':
	c = 194;
	break;
      case '~':
	c = 195;
	break;
      case ':':
	c = 196;
	break;
      case 'o':
	c = 197;
	break;
      case 'e':
      case 'E':
	c = 198;
	break;
      default:
	c = 'A';
      }
      break;
    case 'a':
      switch (tmplate[n]) {
      case '`':
	c = 224;
	break;
      case '\'':
	c = 225;
	break;
      case '^':
	c = 226;
	break;
      case '~':
	c = 227;
	break;
      case ':':
	c = 228;
	break;
      case 'o':
	c = 229;
	break;
      case 'e':
      case 'E':
	c = 230;
	break;
      default:
	c = 'a';
      }
      break;
    case 'C':
      if (tmplate[n] == ',')
	c = 199;
      else
	c = 'C';
      break;
    case 'c':
      if (tmplate[n] == ',')
	c = 231;
      else
	c = 'c';
      break;
    case 'E':
      switch (tmplate[n]) {
      case '`':
	c = 200;
	break;
      case '\'':
	c = 201;
	break;
      case '^':
	c = 202;
	break;
      case ':':
	c = 203;
	break;
      default:
	c = 'E';
      }
      break;
    case 'e':
      switch (tmplate[n]) {
      case '`':
	c = 232;
	break;
      case '\'':
	c = 233;
	break;
      case '^':
	c = 234;
	break;
      case ':':
	c = 235;
	break;
      default:
	c = 'e';
      }
      break;
    case 'I':
      switch (tmplate[n]) {
      case '`':
	c = 204;
	break;
      case '\'':
	c = 205;
	break;
      case '^':
	c = 206;
	break;
      case ':':
	c = 207;
	break;
      default:
	c = 'I';
      }
      break;
    case 'i':
      switch (tmplate[n]) {
      case '`':
	c = 236;
	break;
      case '\'':
	c = 237;
	break;
      case '^':
	c = 238;
	break;
      case ':':
	c = 239;
	break;
      default:
	c = 'i';
      }
      break;
    case 'N':
      if (tmplate[n] == '~')
	c = 209;
      else
	c = 'N';
      break;
    case 'n':
      if (tmplate[n] == '~')
	c = 241;
      else
	c = 'n';
      break;
    case 'O':
      switch (tmplate[n]) {
      case '`':
	c = 210;
	break;
      case '\'':
	c = 211;
	break;
      case '^':
	c = 212;
	break;
      case '~':
	c = 213;
	break;
      case ':':
	c = 214;
	break;
      default:
	c = 'O';
      }
      break;
    case 'o':
      switch (tmplate[n]) {
      case '&':
	c = 240;
	break;
      case '`':
	c = 242;
	break;
      case '\'':
	c = 243;
	break;
      case '^':
	c = 244;
	break;
      case '~':
	c = 245;
	break;
      case ':':
	c = 246;
	break;
      default:
	c = 'o';
      }
      break;
    case 'U':
      switch (tmplate[n]) {
      case '`':
	c = 217;
	break;
      case '\'':
	c = 218;
	break;
      case '^':
	c = 219;
	break;
      case ':':
	c = 220;
	break;
      default:
	c = 'U';
      }
      break;
    case 'u':
      switch (tmplate[n]) {
      case '`':
	c = 249;
	break;
      case '\'':
	c = 250;
	break;
      case '^':
	c = 251;
	break;
      case ':':
	c = 252;
	break;
      default:
	c = 'u';
      }
      break;
    case 'Y':
      if (tmplate[n] == '\'')
	c = 221;
      else
	c = 'Y';
      break;
    case 'y':
      if (tmplate[n] == '\'')
	c = 253;
      else if (tmplate[n] == ':')
	c = 255;
      else
	c = 'y';
      break;
    case '?':
      if (tmplate[n] == 'u')
	c = 191;
      else
	c = '?';
      break;
    case '!':
      if (tmplate[n] == 'u')
	c = 161;
      else
	c = '!';
      break;
    case '<':
      if (tmplate[n] == '"')
	c = 171;
      else
	c = '<';
      break;
    case '>':
      if (tmplate[n] == '"')
	c = 187;
      else
	c = '>';
      break;
    case 's':
      if (tmplate[n] == 'B')
	c = 223;
      else
	c = 's';
      break;
    case 'p':
      if (tmplate[n] == '|')
	c = 254;
      else
	c = 'p';
      break;
    case 'P':
      if (tmplate[n] == '|')
	c = 222;
      else
	c = 'P';
      break;
    case 'D':
      if (tmplate[n] == '-')
	c = 208;
      else
	c = 'D';
      break;
    default:
      c = base[n];
    }
    if (isprint(c)) {
      if (safe_chr((char) c, buff, bp))
	return 1;
    } else {
      if (safe_chr(base[n], buff, bp))
	return 1;
    }
  }
  return 0;
}


/** Define the args used in APPEND_TO_BUF */
#define APPEND_ARGS int len, blen, clen
/** Append a string c to the end of buff, starting at *bp.
 * This macro is used by the safe_XXX functions.
 */
#define APPEND_TO_BUF \
  /* Trivial cases */  \
  if (c[0] == '\0') \
    return 0; \
  /* The array is at least two characters long here */ \
  if (c[1] == '\0') \
    return safe_chr(c[0], buff, bp); \
  len = strlen(c); \
  blen = *bp - buff; \
  if (blen > (BUFFER_LEN - 1)) \
    return len; \
  if ((len + blen) <= (BUFFER_LEN - 1)) \
    clen = len; \
  else \
    clen = (BUFFER_LEN - 1) - blen; \
  memcpy(*bp, c, clen); \
  *bp += clen; \
  return len - clen


/** Safely store a formatted string into a buffer.
 * This is a better way to do safe_str(tprintf(fmt,...),buff,bp)
 * \param buff buffer to store formatted string.
 * \param bp pointer to pointer to insertion point in buff.
 * \param fmt format string.
 * \return number of characters left over, or 0 for success.
 */
int
safe_format(char *buff, char **bp, const char *RESTRICT fmt, ...)
{
  APPEND_ARGS;
#ifdef HAS_VSNPRINTF
  char c[BUFFER_LEN];
#else
  char c[BUFFER_LEN * 3];
#endif
  va_list args;

  va_start(args, fmt);

#ifdef HAS_VSNPRINTF
  vsnprintf(c, sizeof c, fmt, args);
#else
  vsprintf(c, fmt, args);
#endif
  c[BUFFER_LEN - 1] = '\0';
  va_end(args);

  APPEND_TO_BUF;
}

/** Safely store an integer into a buffer.
 * \param i integer to store.
 * \param buff buffer to store into.
 * \param bp pointer to pointer to insertion point in buff.
 * \return 0 on success, non-zero on failure.
 */
int
safe_integer(long i, char *buff, char **bp)
{
  return format_long(i, buff, bp, BUFFER_LEN, 10);
}

/** Safely store an unsigned integer into a buffer.
 * \param i integer to store.
 * \param buff buffer to store into.
 * \param bp pointer to pointer to insertion point in buff.
 * \return 0 on success, non-zero on failure.
 */
int
safe_uinteger(unsigned long i, char *buff, char **bp)
{
  return safe_str(unparse_uinteger(i), buff, bp);
}

/** Safely store an unsigned integer into a short buffer.
 * \param i integer to store.
 * \param buff buffer to store into.
 * \param bp pointer to pointer to insertion point in buff.
 * \return 0 on success, non-zero on failure.
 */
int
safe_integer_sbuf(long i, char *buff, char **bp)
{
  return format_long(i, buff, bp, SBUF_LEN, 10);
}

/** Safely store a dbref into a buffer.
 * Don't store partial dbrefs.
 * \param d dbref to store.
 * \param buff buffer to store into.
 * \param bp pointer to pointer to insertion point in buff.
 * \retval 0 success.
 * \retval 1 failure.
 */
int
safe_dbref(dbref d, char *buff, char **bp)
{
  char *saved = *bp;
  if (safe_chr('#', buff, bp)) {
    *bp = saved;
    return 1;
  }
  if (format_long(d, buff, bp, BUFFER_LEN, 10)) {
    *bp = saved;
    return 1;
  }
  return 0;
}


/** Safely store a number into a buffer.
 * \param n number to store.
 * \param buff buffer to store into.
 * \param bp pointer to pointer to insertion point in buff.
 * \retval 0 success.
 * \retval 1 failure.
 */
int
safe_number(NVAL n, char *buff, char **bp)
{
  const char *c;
  APPEND_ARGS;
  c = unparse_number(n);
  APPEND_TO_BUF;
}

/** Safely store a string into a buffer.
 * \param c string to store.
 * \param buff buffer to store into.
 * \param bp pointer to pointer to insertion point in buff.
 * \retval 0 success.
 * \retval 1 failure.
 */
int
safe_str(const char *c, char *buff, char **bp)
{
  APPEND_ARGS;
  if (!c || !*c)
    return 0;
  APPEND_TO_BUF;
}

/** Safely store a string into a buffer, quoting it if it contains a space.
 * \param c string to store.
 * \param buff buffer to store into.
 * \param bp pointer to pointer to insertion point in buff.
 * \retval 0 success.
 * \retval 1 failure.
 */
int
safe_str_space(const char *c, char *buff, char **bp)
{
  APPEND_ARGS;
  char *saved = *bp;

  if (!c || !*c)
    return 0;

  if (strchr(c, ' ')) {
    if (safe_chr('"', buff, bp) || safe_str(c, buff, bp) ||
	safe_chr('"', buff, bp)) {
      *bp = saved;
      return 1;
    }
    return 0;
  } else {
    APPEND_TO_BUF;
  }
}


/** Safely store a string of known length into a buffer
 * This is an optimization of safe_str for when we know the string's length.
 * \param s string to store.
 * \param len length of s.
 * \param buff buffer to store into.
 * \param bp pointer to pointer to insertion point in buff.
 * \retval 0 success.
 * \retval 1 failure.
 */
int
safe_strl(const char *s, int len, char *buff, char **bp)
{
  int blen, clen;

  if (!s || !*s)
    return 0;
  if (len == 1)
    return safe_chr(*s, buff, bp);

  blen = *bp - buff;
  if (blen > BUFFER_LEN - 2)
    return len;
  if ((len + blen) <= BUFFER_LEN - 2)
    clen = len;
  else
    clen = BUFFER_LEN - 2 - blen;
  memcpy(*bp, s, clen);
  *bp += clen;
  return len - clen;
}

/** Safely fill a string with a given character a given number of times.
 * \param x character to fill with.
 * \param n number of copies of character to fill in.
 * \param buff buffer to store into.
 * \param bp pointer to pointer to insertion point in buff.
 * \retval 0 success.
 * \retval 1 failure (filled to end of buffer, but more was requested).
 */
int
safe_fill(char x, size_t n, char *buff, char **bp)
{
  size_t blen;
  int ret = 0;

  if (n == 0)
    return 0;
  else if (n == 1)
    return safe_chr(x, buff, bp);

  if (n > BUFFER_LEN - 1)
    n = BUFFER_LEN - 1;

  blen = BUFFER_LEN - (*bp - buff) - 1;

  if (blen < n) {
    n = blen;
    ret = 1;
  }
  memset(*bp, x, n);
  *bp += n;

  return ret;
}

#undef APPEND_ARGS
#undef APPEND_TO_BUF

/* skip_space and seek_char are essentially right out of the 2.0 code */

/** Return a pointer to the next non-space character in a string, or NULL.
 * We return NULL if given a null string or a string with only spaces.
 * \param s string to search for non-spaces.
 * \return pointer to next non-space character in s.
 */
char *
skip_space(const char *s)
{
  char *c = (char *) s;
  while (c && *c && isspace((unsigned char) *c))
    c++;
  return c;
}

/** Return a pointer to next char in s which matches c, or to the terminating
 * null at the end of s.
 * \param s string to search.
 * \param c character to search for.
 * \return pointer to next occurence of c or to the end of s.
 */
char *
seek_char(const char *s, char c)
{
  char *p = (char *) s;
  while (p && *p && (*p != c))
    p++;
  return p;
}

/** Unsigned char version of strlen.
 * \param s string.
 * \return length of s.
 */
int
u_strlen(const unsigned char *s)
{
  return strlen((const char *) s);
}

/** Unsigned char version of strcpy. Equally dangerous.
 * \param target destination for copy.
 * \param source string to copy.
 * \return pointer to copy.
 */
unsigned char *
u_strcpy(unsigned char *target, const unsigned char *source)
{
  return (unsigned char *) strcpy((char *) target, (const char *) source);
}

/** Search for all copies of old in string, and replace each with newbit.
 * The replaced string is returned, newly allocated.
 * \param old string to find.
 * \param newbit string to replace old with.
 * \param string string to search for old in.
 * \return allocated string with replacements performed.
 */
char *
replace_string(const char *RESTRICT old, const char *RESTRICT newbit,
	       const char *RESTRICT string)
{
  char *result, *r;
  size_t len, newlen;

  r = result = mush_malloc(BUFFER_LEN, "replace_string.buff");
  if (!result)
    mush_panic(T("Couldn't allocate memory in replace_string!"));

  len = strlen(old);
  newlen = strlen(newbit);

  while (*string) {
    char *s = strstr(string, old);
    if (s) {			/* Match found! */
      safe_strl(string, s - string, result, &r);
      safe_strl(newbit, newlen, result, &r);
      string = s + len;
    } else {
      safe_str(string, result, &r);
      break;
    }
  }
  *r = '\0';
  return result;
}

char *
str_escaped_chr(const char *RESTRICT string, char escape_chr)
{
  const char *p;
  char *result, *r;

  r = result = mush_malloc(BUFFER_LEN, "str_escaped_chr.buff");
  if (!result)
    mush_panic(T("Couldn't allocate memory in replace_string!"));

  p = string;
  while(*p) {
    if(*p == '\\')
      *r++ = '\\';
    else if(*p == escape_chr)
      *r++ = '\\';
    *r++ = *p++;
  }


  *r = '\0';
  return result;
}


/** Standard replacer tokens for text and position */
const char *standard_tokens[2] = { "##", "#@" };

/* Replace two tokens in a string at once. All-around better than calling
 * replace_string() twice
 */
/** Search for all copies of two old strings, and replace each with a 
 * corresponding newbit.
 * The replaced string is returned, newly allocated.
 * \param old array of two strings to find.
 * \param newbits array of two strings to replace old with.
 * \param string string to search for old.
 * \return allocated string with replacements performed.
 */
char *
replace_string2(const char *old[2], const char *newbits[2],
		const char *RESTRICT string)
{
  char *result, *rp;
  char firsts[3] = { '\0', '\0', '\0' };
  size_t oldlens[2], newlens[2];

  if (!string)
    return NULL;

  rp = result = mush_malloc(BUFFER_LEN, "replace_string.buff");
  if (!result)
    mush_panic(T("Couldn't allocate memory in replace_string2!"));

  firsts[0] = old[0][0];
  firsts[1] = old[1][0];

  oldlens[0] = strlen(old[0]);
  oldlens[1] = strlen(old[1]);
  newlens[0] = strlen(newbits[0]);
  newlens[1] = strlen(newbits[1]);

  while (*string) {
    size_t skip = strcspn(string, firsts);
    if (skip) {
      safe_strl(string, skip, result, &rp);
      string += skip;
    }
    if(*string) {
	    if (strncmp(string, old[0], oldlens[0]) == 0) {	/* Copy the first */
		    safe_strl(newbits[0], newlens[0], result, &rp);
		    string += oldlens[0];
	    } else if (strncmp(string, old[1], oldlens[1]) == 0) {	/* The second */
		    safe_strl(newbits[1], newlens[1], result, &rp); 
		    string += oldlens[1]; 
	    } else {
		    safe_chr(*string, result, &rp);
		    string++;
	    }
    }
  }

  *rp = '\0';
  return result;

}

/** Given a string and a separator, trim leading and trailing spaces
 * if the separator is a space. This destructively modifies the string.
 * \param str string to trim.
 * \param sep separator character.
 * \return pointer to (trimmed) string.
 */
char *
trim_space_sep(char *str, char sep)
{
  /* Trim leading and trailing spaces if we've got a space separator. */

  char *p;

  if (sep != ' ')
    return str;
  /* Skip leading spaces */
  str += strspn(str, " ");
  for (p = str; *p; p++) ;
  /* And trailing */
  for (p--; (p > str) && (*p == ' '); p--) ;
  p++;
  *p = '\0';
  return str;
}

/** Find the start of the next token in a string.
 * If the separator is a space, we magically skip multiple spaces.
 * \param str the string.
 * \param sep the token separator character.
 * \return pointer to start of next token in string.
 */
char *
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

/** Split out the next token from a string, destructively modifying it.
 * As usually, if the separator is a space, we skip multiple spaces.
 * The string's address is update to be past the token, and the token
 * is returned. This code from TinyMUSH 2.0.
 * \param sp pointer to string to split from.
 * \param sep token separator.
 * \return pointer to token, now null-terminated.
 */
char *
split_token(char **sp, char sep)
{
  char *str, *save;

  save = str = *sp;
  if (!str) {
    *sp = NULL;
    return NULL;
  }
  while (*str && (*str != sep))
    str++;
  if (*str) {
    *str++ = '\0';
    if (sep == ' ') {
      while (*str == sep)
	str++;
    }
  } else {
    str = NULL;
  }
  *sp = str;
  return save;
}

/** Count the number of tokens in a string.
 * \param str string to count.
 * \param sep token separator.
 * \return number of tokens in str.
 */
int
do_wordcount(char *str, char sep)
{
  int n;

  if (!*str)
    return 0;
  for (n = 0; str; str = next_token(str, sep), n++) ;
  return n;
}

/** A version of strlen that ignores ansi and HTML sequences.
 * \param p string to get length of.
 * \return length of string p, not including ansi/html sequences.
 */
int
ansi_strlen(const char *p)
{
  int i = 0;

  if (!p)
    return 0;

  while (*p) {
    if (*p == ESC_CHAR) {
      while ((*p) && (*p != 'm'))
	p++;
    } else if (*p == TAG_START) {
      while ((*p) && (*p != TAG_END))
	p++;
    } else {
      i++;
    }
    p++;
  }
  return i;
}

/** Returns the apparent length of a string, up to numchars visible 
 * characters. The apparent length skips over nonprinting ansi and
 * tags.
 * \param p string.
 * \param numchars maximum size to report.
 * \return apparent length of string.
 */
int
ansi_strnlen(const char *p, size_t numchars)
{
  int i = 0;

  if (!p)
    return 0;
  while (*p && numchars > 0) {
    if (*p == ESC_CHAR) {
      while ((*p) && (*p != 'm')) {
	p++;
	i++;
      }
    } else if (*p == TAG_START) {
      while ((*p) && (*p != TAG_END)) {
	p++;
	i++;
      }
    } else
      numchars--;
    i++;
    p++;
  }
  return i;
}

/** Given a string, a word, and a separator, remove first occurence
 * of the word from the string. Destructive.
 * \param list a string containing a separated list.
 * \param word a word to remove from the list.
 * \param sep the separator between list items.
 * \return pointer to static buffer containing list without first occurence
 * of word.
 */
char *
remove_word(char *list, char *word, char sep)
{
  char *sp;
  char *bp;
  static char buff[BUFFER_LEN];

  bp = buff;
  sp = split_token(&list, sep);
  if (!strcmp(sp, word)) {
    sp = split_token(&list, sep);
    safe_str(sp, buff, &bp);
  } else {
    safe_str(sp, buff, &bp);
    while (list && strcmp(sp = split_token(&list, sep), word)) {
      safe_chr(sep, buff, &bp);
      safe_str(sp, buff, &bp);
    }
  }
  while (list) {
    sp = split_token(&list, sep);
    safe_chr(sep, buff, &bp);
    safe_str(sp, buff, &bp);
  }
  *bp = '\0';
  return buff;
}

/** Return the next name in a list. A name may be a single word, or
 * a quoted string. This is used by things like page/list. The list's
 * pointer is advanced to the next name in the list.
 * \param head pointer to pointer to string of names.
 * \return pointer to static buffer containing next name.
 */
char *
next_in_list(const char **head)
{
  int paren = 0;
  static char buf[BUFFER_LEN];
  char *p = buf;

  while (**head == ' ')
    (*head)++;

  if (**head == '"') {
    (*head)++;
    paren = 1;
  }

  /* Copy it char by char until you hit a " or, if not in a
   * paren, a space
   */
  while (**head && (paren || (**head != ' ')) && (**head != '"')) {
    safe_chr(**head, buf, &p);
    (*head)++;
  }

  if (paren && **head)
    (*head)++;

  safe_chr('\0', buf, &p);
  return buf;

}

/** Strip all ansi and html markup from a string. As a side effect,
 * stores the length of the stripped string in a provided address.
 * NOTE! Length returned is length *including* the terminating NULL,
 * because we usually memcpy the result.
 * \param orig string to strip.
 * \param s_len address to store length of stripped string, if provided.
 * \return pointer to static buffer containing stripped string.
 */
char *
remove_markup(const char *orig, size_t * s_len)
{
  static char buff[BUFFER_LEN];
  char *bp = buff;
  const char *q;
  size_t len = 0;

  if (!orig) {
    if (s_len)
      *s_len = 0;
    return NULL;
  }

  for (q = orig; *q;) {
    switch (*q) {
    case ESC_CHAR:
      /* Skip over ansi */
      while (*q && *q++ != 'm') ;
      break;
    case TAG_START:
      /* Skip over HTML */
      while (*q && *q++ != TAG_END) ;
      break;
    default:
      safe_chr(*q++, buff, &bp);
      len++;
    }
  }
  *bp = '\0';
  if (s_len)
    *s_len = len + 1;
  return buff;
}


/** Safely append an int to a string. Returns a true value on failure.
 * This will someday take extra arguments for use with our version 
 * of snprintf. Please try not to use it.
 * maxlen = total length of string.
 * buf[maxlen - 1] = place where \0 will go.
 * buf[maxlen - 2] = last visible character.
 * \param val value to append.
 * \param buff string to append to.
 * \param bp pointer to pointer to insertion point in buff.
 * \param maxlen total length of string. 
 * \param base the base to render the number in.
 */
int
format_long(long val, char *buff, char **bp, int maxlen, int base)
{
  char stack[128];		/* Even a negative 64 bit number will only be 21
				   digits or so max. This should be plenty of
				   buffer room. */
  char *current;
  int size = 0, neg = 0;
  ldiv_t r;
  const char *digits = "0123456789abcdefghijklmnopqrstuvwxyz";

  /* Sanity checks */
  if (!bp || !buff || !*bp)
    return 1;
  if (*bp - buff >= maxlen - 1)
    return 1;

  if (base < 2)
    base = 2;
  else if (base > 36)
    base = 36;

  if (val < 0) {
    neg = 1;
    val = -val;
    if (val < 0) {
      /* -LONG_MIN == LONG_MIN on 2's complement systems. Take the
         easy way out since this value is rarely encountered. */
      switch (base) {
      case 10:
	return safe_format(buff, bp, "%ld", val);
      case 16:
	return safe_format(buff, bp, "%lx", val);
      case 8:
	return safe_format(buff, bp, "%lo", val);
      default:
	/* Weird base /and/ LONG_MIN. Fix someday. */
	return 0;
      }
    }

  }

  current = stack + sizeof(stack);

  /* Take the rightmost digit, and push it onto the stack, then
   * integer divide by 10 to get to the next digit. */
  r.quot = val;
  do {
    /* ldiv(x, y) does x/y and x%y at the same time (both of
     * which we need).
     */
    r = ldiv(r.quot, base);
    *(--current) = digits[r.rem];
  } while (r.quot);

  /* Add the negative sign if needed. */
  if (neg)
    *(--current) = '-';

  /* The above puts the number on the stack.  Now we need to put
   * it in the buffer.  If there's enough room, use Duff's Device
   * for speed, otherwise do it one char at a time.
   */

  size = stack + sizeof(stack) - current;

  /* if (size < (int) ((buff + maxlen - 1) - *bp)) { */
  if (((int) (*bp - buff)) + size < maxlen - 2) {
    switch (size % 8) {
    case 0:
      while (current < stack + sizeof(stack)) {
	*((*bp)++) = *(current++);
    case 7:
	*((*bp)++) = *(current++);
    case 6:
	*((*bp)++) = *(current++);
    case 5:
	*((*bp)++) = *(current++);
    case 4:
	*((*bp)++) = *(current++);
    case 3:
	*((*bp)++) = *(current++);
    case 2:
	*((*bp)++) = *(current++);
    case 1:
	*((*bp)++) = *(current++);
      }
    }
  } else {
    while (current < stack + sizeof(stack)) {
      if (*bp - buff >= maxlen - 1) {
	return 1;
      }
      *((*bp)++) = *(current++);
    }
  }

  return 0;
}

#if defined(HAS_STRXFRM) && !defined(WIN32)
/** A locale-sensitive strncmp.
 * \param s1 first string to compare.
 * \param s2 second string to compare.
 * \param t number of characters to compare.
 * \retval -1 s1 collates before s2.
 * \retval 0 s1 collates the same as s2.
 * \retval 1 s1 collates after s2.
 */
int
strncoll(const char *s1, const char *s2, size_t t)
{
  char *d1, *d2, *ns1, *ns2;
  int result;
  size_t s1_len, s2_len;

  ns1 = mush_malloc(t + 1, "string");
  ns2 = mush_malloc(t + 1, "string");
  memcpy(ns1, s1, t);
  ns1[t] = '\0';
  memcpy(ns2, s2, t);
  ns2[t] = '\0';
  s1_len = strxfrm(NULL, ns1, 0) + 1;
  s2_len = strxfrm(NULL, ns2, 0) + 1;

  d1 = mush_malloc(s1_len + 1, "string");
  d2 = mush_malloc(s2_len + 1, "string");
  (void) strxfrm(d1, ns1, s1_len);
  (void) strxfrm(d2, ns2, s2_len);
  result = strcmp(d1, d2);
  mush_free(d1, "string");
  mush_free(d2, "string");
  return result;
}

/** A locale-sensitive strcasecmp.
 * \param s1 first string to compare.
 * \param s2 second string to compare.
 * \retval -1 s1 collates before s2.
 * \retval 0 s1 collates the same as s2.
 * \retval 1 s1 collates after s2.
 */
int
strcasecoll(const char *s1, const char *s2)
{
  char *d1, *d2;
  int result;
  size_t s1_len, s2_len;

  s1_len = strxfrm(NULL, s1, 0) + 1;
  s2_len = strxfrm(NULL, s2, 0) + 1;

  d1 = mush_malloc(s1_len, "string");
  d2 = mush_malloc(s2_len, "string");
  (void) strxfrm(d1, strupper(s1), s1_len);
  (void) strxfrm(d2, strupper(s2), s2_len);
  result = strcmp(d1, d2);
  mush_free(d1, "string");
  mush_free(d2, "string");
  return result;
}

/** A locale-sensitive strncasecmp.
 * \param s1 first string to compare.
 * \param s2 second string to compare.
 * \param t number of characters to compare.
 * \retval -1 s1 collates before s2.
 * \retval 0 s1 collates the same as s2.
 * \retval 1 s1 collates after s2.
 */
int
strncasecoll(const char *s1, const char *s2, size_t t)
{
  char *d1, *d2, *ns1, *ns2;
  int result;
  size_t s1_len, s2_len;

  ns1 = mush_malloc(t + 1, "string");
  ns2 = mush_malloc(t + 1, "string");
  memcpy(ns1, s1, t);
  ns1[t] = '\0';
  memcpy(ns2, s2, t);
  ns2[t] = '\0';
  s1_len = strxfrm(NULL, ns1, 0) + 1;
  s2_len = strxfrm(NULL, ns2, 0) + 1;

  d1 = mush_malloc(s1_len, "string");
  d2 = mush_malloc(s2_len, "string");
  (void) strxfrm(d1, strupper(ns1), s1_len);
  (void) strxfrm(d2, strupper(ns2), s2_len);
  result = strcmp(d1, d2);
  mush_free(d1, "string");
  mush_free(d2, "string");
  return result;
}
#endif				/* HAS_STRXFRM && !WIN32 */

/** Return a string pointer past any ansi/html markup at the start.
 * \param p a string.
 * \return pointer to string after any initial ansi/html markup.
 */
char *
skip_leading_ansi(const char *p)
{
  if (!p)
    return NULL;
  while (*p == ESC_CHAR || *p == TAG_START) {
    if (*p == ESC_CHAR) {
      while (*p && *p != 'm')
	p++;
    } else {			/* TAG_START */
      while (*p && *p != TAG_END)
	p++;
    }
    if (*p)
      p++;
  }
  return (char *) p;

}

/** Convert a string into an ansi_string.
 * This takes a string that may contain ansi/html markup codes and
 * converts it to an ansi_string structure that separately stores
 * the plain string and the markup codes for each character.
 * \param src string to parse.
 * \return pointer to an ansi_string structure representing the src string.
 */
ansi_string *
parse_ansi_string(const char *src)
{
  ansi_string *data;
  char *y, *current = NULL;
  size_t p = 0;

  if (!src)
    return NULL;

  data = mush_malloc(sizeof *data, "ansi_string");
  if (!data)
    return NULL;

  data->len = ansi_strlen(src);

  while (*src) {
    y = skip_leading_ansi(src);
    if (y != src) {
      if (current)
	mush_free(current, "markup_codes");
      current = mush_strndup(src, y - src, "markup_codes");
      src = y;
    }
    if (current)
      data->codes[p] = mush_strdup(current, "markup_codes");
    else
      data->codes[p] = NULL;
    data->text[p] = *src;
    if (*src)
      src++;
    p++;
  }
  data->text[p] = '\0';

  while (p <= data->len) {
    data->codes[p] = NULL;
    p++;
  }

  if (current)
    mush_free(current, "markup_codes");

  return data;
}


/** Fill up an ansi_string with codes so that when a code starts it
 * applies to all the following characters until there's a new code.
 * \param as pointer to an ansi_string to populate codes in.
 */
void
populate_codes(ansi_string *as)
{
  size_t p;
  char *current = NULL;

  if (!as)
    return;

  for (p = 0; p < as->len; p++)
    if (as->codes[p]) {
      if (current)
	mush_free(current, "markup_codes");
      current = mush_strdup(as->codes[p], "markup_codes");
    } else {
      if (!current)
	current = mush_strdup(ANSI_NORMAL, "markup_codes");
      as->codes[p] = mush_strdup(current, "markup_codes");
    }
  if (current)
    mush_free(current, "markup_codes");
}

/** Strip out codes from an ansi_string, leaving in only the codes where
 * they change.
 * \param as pointer to an ansi_string.
 */
void
depopulate_codes(ansi_string *as)
{
  size_t p, m;
  int normal = 1;

  if (!as)
    return;

  for (p = 0; p <= as->len; p++) {
    if (as->codes[p]) {
      if (normal) {
	if (strcmp(as->codes[p], ANSI_NORMAL) == 0) {
	  mush_free(as->codes[p], "markup_codes");
	  as->codes[p] = NULL;
	  continue;
	} else {
	  normal = 0;
	}
      }

      m = p;
      for (p++; p < as->len; p++) {
	if (as->codes[p] && strcmp(as->codes[p], as->codes[m]) == 0) {
	  mush_free(as->codes[p], "markup_codes");
	  as->codes[p] = NULL;
	} else {
	  p--;
	  break;
	}
      }
    }
  }
}

/** Reverse an ansi string, preserving its ansification.
 * This function destructively modifies the ansi_string passed.
 * \param as pointer to an ansi string.
 */
void
flip_ansi_string(ansi_string *as)
{
  int p, n;

  populate_codes(as);

  for (p = 0, n = as->len - 1; p < n; p++, n--) {
    char *tcode;
    char t;

    tcode = as->codes[p];
    t = as->text[p];
    as->codes[p] = as->codes[n];
    as->text[p] = as->text[n];
    as->codes[n] = tcode;
    as->text[n] = t;
  }
}


static int is_ansi_code(const char *s);
static int is_start_html_code(const char *s) __attribute__ ((__unused__));
static int is_end_html_code(const char *s);
/** Is s a string that signifies the end of ANSI codes? */
#define is_end_ansi_code(s) (!strcmp((s),ANSI_NORMAL))


static int
is_ansi_code(const char *s)
{
  return s && *s == ESC_CHAR;
}

static int
is_start_html_code(const char *s)
{
  return s && *s == TAG_START && *(s + 1) != '/';
}

static int
is_end_html_code(const char *s)
{
  return s && *s == TAG_START && *(s + 1) == '/';
}

/** Free an ansi_string.
 * \param as pointer to ansi_string to free.
 */
void
free_ansi_string(ansi_string *as)
{
  int p;

  if (!as)
    return;
  for (p = as->len; p >= 0; p--) {
    if (as->codes[p])
      mush_free(as->codes[p], "markup_codes");
  }
  mush_free(as, "ansi_string");
}

/** Safely append an ansi_string into a buffer as a real string.
 * \param as pointer to ansi_string to append.
 * \param start position in as to start copying from.
 * \param len length in characters to copy from as.
 * \param buff buffer to insert into.
 * \param bp pointer to pointer to insertion point of buff.
 * \retval 0 success.
 * \retval 1 failure.
 */
int safe_ansi_string(ansi_string *as, size_t start, size_t len, char *buff, char **bp) {
  int p, q;
  int in_ansi = 0;
  int in_html = 0;

  if (!as)
    return 1;

  depopulate_codes(as);

  if (start > as->len || len == 0 || as->len == 0)
    return safe_str("", buff, bp);

  /* Find the starting codes by working our way backward until we
   * reach some opening codes, and then working our way back from there
   * until we hit a non-opening code or non-code 
   */
  p = start;
  while ((p >= 0) && (as->codes[p] == NULL))
    p--;
  /* p is now either <0 or pointing to a code */
  if ((p >= 0) && !is_end_html_code(as->codes[p]) &&
      !is_end_ansi_code(as->codes[p])) {
    /* p is now pointing to a starting code */
    q = p;
    while ((q >= 0) && as->codes[q] && !is_end_html_code(as->codes[q]) &&
	   !is_end_ansi_code(as->codes[q])) {
      if (is_ansi_code(as->codes[q]))
	in_ansi = 1;
      else if (is_start_html_code(as->codes[q]))
	in_html++;
      q--;
    }
    /* p is now pointing to the first starting code, and we know if we're
     * in ansi, html, or both. We also know how many html tags have been
     * opened.
     */
  }

  /* Copy the text. The right thing to do now would be to have a stack
   * of open html tags and clear in_html once all of the tags have
   * been closed. We don't quite do that, alas.
   */
  for (p = (int) start; p < (int) (start + len) && p < (int) as->len; p++) {
    if (as->codes[p]) {
      if (safe_str(as->codes[p], buff, bp))
      return 1;
      if (is_end_ansi_code(as->codes[p]))
      in_ansi = 0;
      else if (is_ansi_code(as->codes[p]))
      in_ansi = 1;
      if (is_end_html_code(as->codes[p]))
      in_html--;
      else if (is_start_html_code(as->codes[p]))
      in_html++;
    }
    if (safe_chr(as->text[p], buff, bp))
      return 1;
  }

  /* Output (only) closing codes if needed. */
  while (p <= (int) as->len) {
    if (!in_ansi && !in_html)
      break;
    if (as->codes[p]) {
      if (is_end_ansi_code(as->codes[p])) {
      in_ansi = 0;
      if (safe_str(as->codes[p], buff, bp))
        return 1;
      } else if (is_end_html_code(as->codes[p])) {
      in_html--;
      if (safe_str(as->codes[p], buff, bp))
        return 1;
      }
    }
    p++;
  }
  if (in_ansi)
    safe_str(ANSI_NORMAL, buff, bp);
  return 0;
}

/** Safely append an ansi_string into a buffer as a real string,
 * with extra copying of starting tags (for wrap()/align()).
 * \param as pointer to ansi_string to append.
 * \param start position in as to start copying from.
 * \param len length in characters to copy from as.
 * \param buff buffer to insert into.
 * \param bp pointer to pointer to insertion point of buff.
 * \retval 0 success.
 * \retval 1 failure.
 */
int safe_ansi_string2(ansi_string *as, size_t start, size_t len, char *buff, char **bp) {
   int p, q;
  int in_ansi = 0;
  int in_html = 0;

  if (!as)
    return 1;

  depopulate_codes(as);

  if (start > as->len || len == 0 || as->len == 0)
    return safe_str("", buff, bp);

  /* Find the starting codes by working our way backward until we
   * reach some opening codes, and then working our way back from there
   * until we hit a non-opening code or non-code
   */
  p = start;
  while ((p >= 0) && (as->codes[p] == NULL))
    p--;
  /* p is now either <0 or pointing to a code */
  if ((p >= 0) && !is_end_html_code(as->codes[p]) &&
      !is_end_ansi_code(as->codes[p])) {
    /* p is now pointing to a starting code */
    q = p;
    while ((q >= 0) && as->codes[q] && !is_end_html_code(as->codes[q]) &&
         !is_end_ansi_code(as->codes[q])) {
      if (is_ansi_code(as->codes[q]))
      in_ansi = 1;
      else if (is_start_html_code(as->codes[q]))
      in_html++;
      q--;
    }
    /* p is now pointing to the first starting code, and we know if we're
     * in ansi, html, or both. We also know how many html tags have been
     * opened.
     */

    /* Except there's this one problem - Now we know it, we weren't
     * doing anything with it.
     */
    for (q = q + 1; q <= p; q++) {
      if (safe_str(as->codes[q], buff, bp))
	return 1;
    }
  }

  /* Copy the text. The right thing to do now would be to have a stack
   * of open html tags and clear in_html once all of the tags have
   * been closed. We don't quite do that, alas.
   */
  for (p = (int) start; p < (int) (start + len) && p < (int) as->len; p++) {
    if (as->codes[p]) {
      if (safe_str(as->codes[p], buff, bp))
	return 1;
      if (is_end_ansi_code(as->codes[p]))
	in_ansi = 0;
      else if (is_ansi_code(as->codes[p]))
	in_ansi = 1;
      if (is_end_html_code(as->codes[p]))
	in_html--;
      else if (is_start_html_code(as->codes[p]))
	in_html++;
    }
    if (safe_chr(as->text[p], buff, bp))
      return 1;
  }

  /* Output (only) closing codes if needed. */
  while (p <= (int) as->len) {
    if (!in_ansi && !in_html)
      break;
    if (as->codes[p]) {
      if (is_end_ansi_code(as->codes[p])) {
	in_ansi = 0;
	if (safe_str(as->codes[p], buff, bp))
	  return 1;
      } else if (is_end_html_code(as->codes[p])) {
	in_html--;
	if (safe_str(as->codes[p], buff, bp))
	  return 1;
      }
    }
    p++;
  }
  if (in_ansi)
    safe_str(ANSI_NORMAL, buff, bp);
  return 0;
}

/** Safely append a list item to a buffer, possibly with punctuation
 * and conjunctions.
 * Given the current item number in a list, whether it's the last item
 * in the list, the list's output separator, a conjunction,
 * and a punctuation mark to use between items, store the appropriate
 * inter-item stuff into the given buffer safely.
 * \param cur_num current item number of the item to append.
 * \param done 1 if this is the final item.
 * \param delim string to insert after most items (comma).
 * \param conjoin string to insert before last time ("and").
 * \param space output delimiter.
 * \param buff buffer to append to.
 * \param bp pointer to pointer to insertion point in buff.
 */
void
safe_itemizer(int cur_num, int done, const char *delim, const char *conjoin,
	      const char *space, char *buff, char **bp)
{
  /* We don't do anything if it's the first one */
  if (cur_num == 1)
    return;
  /* Are we done? */
  if (done) {
    /* if so, we need a [<delim>]<space><conj> */
    if (cur_num >= 3)
      safe_str(delim, buff, bp);
    safe_str(space, buff, bp);
    safe_str(conjoin, buff, bp);
  } else {
    /* if not, we need just <delim> */
    safe_str(delim, buff, bp);
  }
  /* And then we need another <space> */
  safe_str(space, buff, bp);

}

/** Return a stringified time in a static buffer
 * Just like ctime() except without the trailing newlines.
 * \param t the time to format.
 * \param utc true if the time should be displayed in UTC, 0 for local time zone.
 * \return a pointer to a static buffer with the stringified time.
 */
char *
show_time(time_t t, int utc)
{
  struct tm *when;

  if (utc)
    when = gmtime(&t);
  else
    when = localtime(&t);

  return show_tm(when);
}

/** Return a stringified time in a static buffer
 * Just like asctime() except without the trailing newlines.
 * \param when the time to format.
 * \return a pointer to a static buffer with the stringified time.
 */
char *
show_tm(struct tm *when)
{
  static char buffer[BUFFER_LEN];
  int p;

  if (!when)
    return NULL;

  strcpy(buffer, asctime(when));

  p = strlen(buffer) - 1;
  if (buffer[p] == '\n')
    buffer[p] = '\0';

  if (buffer[8] == ' ')
    buffer[8] = '0';

  return buffer;
}

