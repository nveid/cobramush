/**
 * \file parse.c
 *
 * \brief The PennMUSH function/expression parser
 *
 * The most important function in this file is process_expression.
 *
 */

#include "copyrite.h"

#include "config.h"
#include <math.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>

#include "conf.h"
#include "externs.h"
#include "ansi.h"
#include "dbdefs.h"
#include "boolexp.h"
#include "function.h"
#include "case.h"
#include "match.h"
#include "mushdb.h"
#include "parse.h"
#include "attrib.h"
#include "pcre.h"
#include "flags.h"
#include "log.h"
#include "mymalloc.h"
#include "confmagic.h"

extern char *absp[], *obj[], *poss[], *subj[];  /* fundb.c */
extern int inum, inum_limit;
extern char *iter_rep[];
int global_fun_invocations;
int global_fun_recursions;
/* extern int re_subpatterns; */
/* extern int *re_offsets; */
/* extern char *re_from; */
extern sig_atomic_t cpu_time_limit_hit;
extern int cpu_limit_warning_sent;
extern int iter_break;

/** Structure for storing DEBUG output in a linked list */
struct debug_info {
  char *string;         /**< A DEBUG string */
  Debug_Info *prev;     /**< Previous node in the linked list */
  Debug_Info *next;     /**< Next node in the linked list */
};

FUNCTION_PROTO(fun_gfun);

#ifndef DOXYGEN_SHOULD_SKIP_THIS

/* Common error messages */
char e_int[] = "#-1 ARGUMENT MUST BE INTEGER";
char e_ints[] = "#-1 ARGUMENTS MUST BE INTEGERS";
char e_uint[] = "#-1 ARGUMENT MUST BE POSITIVE INTEGER";
char e_uints[] = "#-1 ARGUMENTS MUST BE POSITIVE INTEGERS";
char e_num[] = "#-1 ARGUMENT MUST BE NUMBER";
char e_nums[] = "#-1 ARGUMENTS MUST BE NUMBERS";
char e_invoke[] = "#-1 FUNCTION INVOCATION LIMIT EXCEEDED";
char e_call[] = "#-1 CALL LIMIT EXCEEDED";
char e_perm[] = "#-1 PERMISSION DENIED";
char e_atrperm[] = "#-1 NO PERMISSION TO GET ATTRIBUTE";
char e_match[] = "#-1 NO MATCH";
char e_notvis[] = "#-1 NO SUCH OBJECT VISIBLE";
char e_disabled[] = "#-1 FUNCTION DISABLED";
char e_range[] = "#-1 OUT OF RANGE";

#endif

#if 0
static void dummy_errors(void);

static void
dummy_errors()
{
  /* Just to make sure the error messages are in the translation
     tables. */
  char *temp;
  temp = T("#-1 ARGUMENT MUST BE INTEGER");
  temp = T("#-1 ARGUMENTS MUST BE INTEGERS");
  temp = T("#-1 ARGUMENT MUST BE POSITIVE INTEGER");
  temp = T("#-1 ARGUMENTS MUST BE POSITIVE INTEGERS");
  temp = T("#-1 ARGUMENT MUST BE NUMBER");
  temp = T("#-1 ARGUMENTS MUST BE NUMBERS");
  temp = T("#-1 FUNCTION INVOCATION LIMIT EXCEEDED");
  temp = T("#-1 CALL LIMIT EXCEEDED");
  temp = T("#-1 PERMISSION DENIED");
  temp = T("#-1 NO PERMISSION TO GET ATTRIBUTE");
  temp = T("#-1 NO MATCH");
  temp = T("#-1 NO SUCH OBJECT VISIBLE");
  temp = T("#-1 FUNCTION DISABLED");
  temp = T("#-1 OUT OF RANGE");
}

#endif

/** Given a string, parse out a dbref.
 * \param str string to parse.
 * \return dbref contained in the string, or NOTHING if not a valid dbref.
 */
dbref
parse_dbref(char const *str)
{
  /* Make sure string is strictly in format "#nnn".
   * Otherwise, possesives will be fouled up.
   */
  char const *p;
  dbref num;

  if (!str || (*str != NUMBER_TOKEN) || !*(str + 1))
    return NOTHING;
  for (p = str + 1; isdigit((unsigned char) *p); p++) {
  }
  if (*p)
    return NOTHING;

  num = atoi(str + 1);
  if (!GoodObject(num))
    return NOTHING;
  return num;
}

/** Version of parse_dbref() that doesn't do GoodObject checks */
dbref
qparse_dbref(const char *s)
{

  if (!s || (*s != NUMBER_TOKEN) || !*(s + 1))
    return NOTHING;
  return parse_integer(s + 1);
}


/** Given a string, parse out an object id or dbref.
 * \param str string to parse.
 * \return dbref of object referenced by string, or NOTHING if not a valid
 * string or not an existing dbref.
 */
dbref
parse_objid(char const *str)
{
  char *p;
  if ((p = strchr(str, ':'))) {
    char tbuf1[BUFFER_LEN];
    dbref it;
    /* A unique id, probably */
    strncpy(tbuf1, str, (p - str));
    tbuf1[p - str] = '\0';
    it = parse_dbref(tbuf1);
    if (GoodObject(it)) {
      time_t matchtime;
      p++;
      if (!is_strict_integer(p))
        return NOTHING;
      matchtime = parse_integer(p);
      return (CreTime(it) == matchtime) ? it : NOTHING;
    } else
      return NOTHING;
  } else
    return parse_dbref(str);
}


/** Given a string, parse out a boolean value.
 * The meaning of boolean is fuzzy. To TinyMUSH, any string that begins with
 * a non-zero number is true, and everything else is false.
 * To PennMUSH, negative dbrefs are false, non-negative dbrefs are true,
 * 0 is false, all other numbers are true, empty or blank strings are false,
 * and all other strings are true. 
 * \param str string to parse.
 * \retval 1 str represents a true value.
 * \retval 0 str represents a false value.
 */
int
parse_boolean(char const *str)
{
  if (TINY_BOOLEANS) {
    return (atoi(str) ? 1 : 0);
  } else {
    /* Turn a string into a boolean value.
     * All negative dbrefs are false, all non-negative dbrefs are true.
     * Zero is false, all other numbers are true.
     * Empty (or space only) strings are false, all other strings are true.
     */
    /* Null strings are false */
    if (!str || !*str)
      return 0;
    /* Negative dbrefs are false - actually anything starting #-,
     * which will also cover our error messages. */
    if (*str == '#' && *(str + 1) && (*(str + 1) == '-'))
      return 0;
    /* Non-zero numbers are true, zero is false */
    if (is_strict_number(str))
      return parse_number(str) != 0;    /* avoid rounding problems */
    /* Skip blanks */
    while (*str == ' ')
      str++;
    /* If there's any non-blanks left, it's true */
    return *str != '\0';        /* force to 1 or 0 */
  }
}

/** Is a string a boolean value?
 * To TinyMUSH, any integer is a boolean value. To PennMUSH, any
 * string at all is boolean.
 * \param str string to check.
 * \retval 1 string is a valid boolean.
 * \retval 0 string is not a valid boolean.
 */
int
is_boolean(char const *str)
{
  if (TINY_BOOLEANS)
    return is_integer(str);
  else
    return 1;
}

/** Is a string a dbref?
 * A dbref is a string starting with a #, optionally followed by a -,
 * and then followed by at least one digit, and nothing else.
 * \param str string to check.
 * \retval 1 string is a dbref.
 * \retval 0 string is not a dbref.
 */
int
is_dbref(char const *str)
{
  if (!str || (*str != NUMBER_TOKEN) || !*(str + 1))
    return 0;
  if (*(str + 1) == '-') {
    str++;
  }
  for (str++; isdigit((unsigned char) *str); str++) {
  }
  return !*str;
}

/** Is a string an objid?
 * An objid is a string starting with a #, optionally followed by a -,
 * and then followed by at least one digit, then optionally followed 
 * by a : and at least one digit, and nothing else.
 * In regex: ^#-?\d+(:\d+)?$
 * \param str string to check.
 * \retval 1 string is an objid
 * \retval 0 string is not an objid.
 */
int
is_objid(char const *str)
{
  static pcre *re = NULL;
  const char *errptr;
  int erroffset;

  if(!str)
    return 0;
  if (!re)
    re = pcre_compile("^#-?\\d+(?::\\d+)?$", 0, &errptr, &erroffset, NULL);
  return (pcre_exec(re, NULL, str, strlen(str), 0, 0, NULL, 0) >= 0);
}

/** Is string an integer?
 * To TinyMUSH, any string is an integer. To PennMUSH, a string that
 * passes strtol is an integer, and a blank string is an integer
 * if NULL_EQ_ZERO is turned on.
 * \param str string to check.
 * \retval 1 string is an integer.
 * \retval 0 string is not an integer.
 */
int
is_integer(char const *str)
{
  char *end;

  /* If we're emulating Tiny, anything is an integer */
  if (TINY_MATH)
    return 1;
  if (!str)
    return 0;
  while (isspace((unsigned char) *str))
    str++;
  if (*str == '\0')
    return NULL_EQ_ZERO;
  errno = 0;
  strtol(str, &end, 10);
  if (errno == ERANGE || *end != '\0')
    return 0;
  return 1;
}

/** Is string an uinteger?
 * To TinyMUSH, any string is an uinteger. To PennMUSH, a string that
 * passes strtoul is an uinteger, and a blank string is an uinteger
 * if NULL_EQ_ZERO is turned on.
 * \param str string to check.
 * \retval 1 string is an uinteger.
 * \retval 0 string is not an uinteger.
 */
int
is_uinteger(char const *str)
{
  char *end;

  /* If we're emulating Tiny, anything is an integer */
  if (TINY_MATH)
    return 1;
  if (!str)
    return 0;
  /* strtoul() accepts negative numbers, so we still have to do this check */
  while (isspace((unsigned char) *str))
    str++;
  if (*str == '\0')
    return NULL_EQ_ZERO;
  if (!(isdigit((unsigned char) *str) || *str == '+'))
    return 0;
  errno = 0;
  strtoul(str, &end, 10);
  if (errno == ERANGE || *end != '\0')
    return 0;
  return 1;
}

/** Is string a number by the strict definition?
 * A strict number is a non-null string that passes strtod.
 * \param str string to check.
 * \retval 1 string is a strict number.
 * \retval 0 string is not a strict number.
 */
int
is_strict_number(char const *str)
{
  char *end;
  NVAL val;
  if (!str)
    return 0;
  errno = 0;
  val = strtod(str, &end);
  if (errno == ERANGE || *end != '\0')
    return 0;
  return end > str;
}

/** Is string a number that isn't inf or nan?
 * \param num NVAL
 * \retval 1 num is a good number.
 * \retval 0 num is not a good number.
 */
int
is_good_number(NVAL val)
{
  char numbuff[128];
  char *p;
  snprintf(numbuff, 128, "%f", val);
  p = numbuff;
  /* Negative? */
  if (*p == '-')
    p++;
  /* Must start with a digit. */
  if (!*p || !isdigit((unsigned char) *p))
    return 0;
  return 1;
}

/** Is string an integer by the strict definition?
 * A strict integer is a non-null string that passes strtol.
 * \param str string to check.
 * \retval 1 string is a strict integer.
 * \retval 0 string is not a strict integer.
 */
int
is_strict_integer(char const *str)
{
  char *end;
  int val;
  if (!str)
    return 0;
  errno = 0;
  val = strtol(str, &end, 10);
  if (errno == ERANGE || *end != '\0')
    return 0;
  return end > str;
}

/** Is string a number?
 * To TinyMUSH, any string is a number. To PennMUSH, a strict number is
 * a number, and a blank string is a number if NULL_EQ_ZERO is turned on.
 * \param str string to check.
 * \retval 1 string is a number.
 * \retval 0 string is not a number.
 */
int
is_number(char const *str)
{
  /* If we're emulating Tiny, anything is a number */
  if (TINY_MATH)
    return 1;
  while (isspace((unsigned char) *str))
    str++;
  if (*str == '\0')
    return NULL_EQ_ZERO;
  return is_strict_number(str);
}

/* Table of interesting characters for process_expression() */
extern char active_table[UCHAR_MAX + 1];
/* Indexes of valid q-regs into the global_eval_context.renv array. -1 is error. */
extern signed char qreg_indexes[UCHAR_MAX + 1];

#ifdef WIN32
#pragma warning( disable : 4761)        /* NJG: disable warning re conversion */
#endif


/** Function and other substitution evaluation.
 * This is the PennMUSH function/expression parser. Big stuff.
 *
 * All results are returned in buff, at the point *bp. bp is likely
 * not equal to buff, so make no assumptions about writing at the
 * start of the buffer.  *bp must be updated to point at the next
 * place to be filled (ala safe_str() and safe_chr()).  Be very
 * careful about not overflowing buff; use of safe_str() and safe_chr()
 * for all writes into buff is highly recommended.
 * 
 * nargs is the count of the number of arguments passed to the function,
 * and args is an array of pointers to them.  args will have at least
 * nargs elements, or 10 elements, whichever is greater.  The first ten
 * elements are initialized to NULL for ease of porting functions from
 * the old style, but relying on such is considered bad form.
 * The argument strings are stored in BUFFER_LEN buffers, but reliance
 * on that size is also considered bad form.  The argument strings may
 * be modified, but modifying the pointers to the argument strings will
 * cause crashes. 
 *
 * executor corresponds to %!, the object invoking the function.
 * caller   corresponds to %@, the last object to do a U() or similar.
 * enactor  corresponds to %#, the object that started the whole mess.
 * Note that fun_ufun() and similar must swap around these parameters
 * in calling process_expression(); no checks are made in the parser
 * itself to maintain these values.
 *
 * called_as contains a pointer to the name of the function called
 * (taken from the function table).  This may be used to distinguish
 * multiple functions which use the same C function for implementation.
 *
 * pe_info holds context information used by the parser.  It should
 * be passed untouched to process_expression(), if it is called.
 * pe_info should be treated as a black box; its structure and contents
 * may change without notice.
 *
 * Normally, p_e() returns 0. It returns 1 upon hitting the CPU time limit.
 *
 * \param buff buffer to store returns of parsing.
 * \param bp pointer to pointer into buff marking insert position.
 * \param str string to parse.
 * \param executor dbref of the object invoking the function.
 * \param caller dbref of  the last object to use u()
 * \param enactor dbref of the enactor.
 * \param eflags flags to control what is evaluated.
 * \param tflags flags to control what terminates an expression.
 * \param pe_info pointer to parser context data.
 * \retval 0 success.
 * \retval 1 CPU time limit exceeded.
 */
int
process_expression(char *buff, char **bp, char const **str,
                   dbref executor, dbref caller, dbref enactor,
                   int eflags, int tflags, PE_Info * pe_info)
{
  int debugging = 0, made_info = 0;
  char *debugstr = NULL, *sourcestr = NULL;
  char *realbuff = NULL, *realbp = NULL;
  int gender = -1;
  int inum_this;
  char *startpos = *bp;
  int had_space = 0;
  char temp[3];
  int temp_eflags;
  int old_iter_limit;
  int qindex;
  int e_len;
  int retval = 0;
  const char *e_msg;

  if (!buff || !bp || !str || !*str)
    return 0;
  if (cpu_time_limit_hit) {
    if (!cpu_limit_warning_sent) {
      cpu_limit_warning_sent = 1;
      /* Can't just put #-1 CPU USAGE EXCEEDED in buff here, because
       * it might never get displayed.
       */
      if (!Quiet(enactor))
        notify(enactor, T("CPU usage exceeded."));
      do_rawlog(LT_TRACE,
                T
                ("CPU time limit exceeded. enactor=#%d executor=#%d caller=#%d code=%s"),
                enactor, executor, caller, *str);
    }
    return 1;
  }
  if (Halted(executor))
    eflags = PE_NOTHING;
  if (eflags & PE_COMPRESS_SPACES)
    while (**str == ' ')
      (*str)++;
  if (!*str)
    return 0;

  if (!pe_info) {
    old_iter_limit = inum_limit;
    inum_limit = inum;
    made_info = 1;
    pe_info = (PE_Info *) mush_malloc(sizeof(PE_Info),
                                      "process_expression.pe_info");
    pe_info->fun_invocations = 0;
    pe_info->fun_depth = 0;
    pe_info->nest_depth = 0;
    pe_info->call_depth = 0;
    pe_info->debug_strings = NULL;
    pe_info->arg_count = 0;
  } else {
    old_iter_limit = -1;
  }

  /* If we've been asked to evaluate, log the expression if:
   * (a) the last thing we logged wasn't an expression, and
   * (b) this expression isn't a substring of the last thing we logged
   */
  if ((eflags & PE_EVALUATE) &&
      ((last_activity_type() != LA_PE) || !strstr(last_activity(), *str))) {
    log_activity(LA_PE, executor, *str);
  }

  if (eflags != PE_NOTHING) {
    if (((*bp) - buff) > (BUFFER_LEN - SBUF_LEN)) {
      realbuff = buff;
      realbp = *bp;
      buff = (char *) mush_malloc(BUFFER_LEN,
                                  "process_expression.buffer_extension");
      *bp = buff;
      startpos = buff;
    }
  }

  if (CALL_LIMIT && (pe_info->call_depth++ > CALL_LIMIT)) {
    e_msg = T(e_call);
    e_len = strlen(e_msg);
    if ((buff + e_len > *bp) || strcmp(e_msg, *bp - e_len))
      safe_str(e_msg, buff, bp);
    goto exit_sequence;
  }


  if (eflags != PE_NOTHING) {
    debugging = (Debug(executor) || (eflags & PE_DEBUG))
      && (Connected(Owner(executor)) || atr_get(executor, "DEBUGFORWARDLIST"));
    if (debugging) {
      int j;
      char *debugp;
      char const *mark;
      Debug_Info *node;

      debugstr = (char *) mush_malloc(BUFFER_LEN,
                                      "process_expression.debug_source");
      debugp = debugstr;
      safe_dbref(executor, debugstr, &debugp);
      safe_chr('!', debugstr, &debugp);
      for (j = 0; j <= pe_info->nest_depth; j++)
        safe_chr(' ', debugstr, &debugp);
      sourcestr = debugp;
      mark = *str;
      process_expression(debugstr, &debugp, str,
                         executor, caller, enactor,
                         PE_NOTHING, tflags, pe_info);
      *str = mark;
      if (eflags & PE_COMPRESS_SPACES)
        while ((debugp > sourcestr) && (debugp[-1] == ' '))
          debugp--;
      *debugp = '\0';
      node = (Debug_Info *) mush_malloc(sizeof(Debug_Info),
                                        "process_expression.debug_node");
      node->string = debugstr;
      node->prev = pe_info->debug_strings;
      node->next = NULL;
      if (node->prev)
        node->prev->next = node;
      pe_info->debug_strings = node;
      pe_info->nest_depth++;
    }
  }

  /* Only strip command braces if the first character is a brace. */
  if (**str != '{')
    eflags &= ~PE_COMMAND_BRACES;

  for (;;) {
     if(iter_break > 0) {
	while(*str && **str)
	  (*str)++;
	goto exit_sequence;
     }
    /* Find the first "interesting" character */
    {
      char const *pos;
      int len, len2;
      /* Inlined strcspn() equivalent, to save on overhead and portability */
      pos = *str;
      while (!active_table[*(unsigned char const *) *str])
        (*str)++;
      /* Inlined safe_str(), since the source string
       * may not be null terminated */
      len = *str - pos;
      len2 = BUFFER_LEN - 1 - (*bp - buff);
      if (len > len2)
        len = len2;
      if (len >= 0) {
        memcpy(*bp, pos, len);
        *bp += len;
      }
    }

    switch (**str) {
      /* Possible terminators */
    case '}':
      if (tflags & PT_BRACE)
	goto exit_sequence;
      break;
    case ']':
      if (tflags & PT_BRACKET)
	goto exit_sequence;
      break;
    case ')':
      if (tflags & PT_PAREN)
	goto exit_sequence;
      break;
    case ',':
      if (tflags & PT_COMMA)
	goto exit_sequence;
      break;
    case ';':
      if (tflags & PT_SEMI)
	goto exit_sequence;
      break;
    case '=':
      if (tflags & PT_EQUALS)
	goto exit_sequence;
      break;
    case ' ':
      if (tflags & PT_SPACE)
	goto exit_sequence;
      break;
    case '\0':
      goto exit_sequence;
    }


    switch (**str) {
    case 0x1B:			/* ANSI escapes. */
      /* Skip over until the 'm' that matches the end. */
      for (; *str && **str && **str != 'm'; (*str)++)
	safe_chr(**str, buff, bp);
      if (*str && **str) {
	safe_chr(**str, buff, bp);
	(*str)++;
      }
      break;
    case '$':			/* Dollar subs for regedit() */
      if ((eflags & (PE_DOLLAR | PE_EVALUATE)) == (PE_DOLLAR | PE_EVALUATE) &&
	  global_eval_context.re_subpatterns >= 0 &&
 	  global_eval_context.re_offsets != NULL &&
 	  global_eval_context.re_from != NULL) {
	char obuf[BUFFER_LEN];
	int p = -1;
        char subspace[BUFFER_LEN];
        char *named_substring = NULL;

        obuf[0] = '\0';
	(*str)++;
	/* Check the first two characters after the $ for a number */
	if (isdigit((unsigned char) **str)) {
	  p = **str - '0';
	  (*str)++;
	} else if (**str == '<') {
	  char *nbuf = subspace;
	  (*str)++;
	  for (; *str && **str && **str != '>'; (*str)++)
	    safe_chr(**str, subspace, &nbuf);
	  *nbuf = '\0';
	  if (*str && **str)
	    (*str)++;
	  if (is_strict_integer(subspace))
	    p = abs(parse_integer(subspace));
	  else
	    named_substring = subspace;
	} else {
	  safe_chr('$', buff, bp);
	  break;
	}

	if (named_substring) {
	  pcre_copy_named_substring(global_eval_context.re_code,
				    global_eval_context.re_from,
				    global_eval_context.re_offsets,
				    global_eval_context.re_subpatterns,
				    named_substring, obuf, BUFFER_LEN);
	} else {
	  pcre_copy_substring(global_eval_context.re_from,
			      global_eval_context.re_offsets,
			      global_eval_context.re_subpatterns,
			      p, obuf, BUFFER_LEN);
	}
	safe_str(obuf, buff, bp);
      } else {
	safe_chr('$', buff, bp);
	(*str)++;
      }
      break;
    case '%':			/* Percent substitutions */
      if (!(eflags & PE_EVALUATE) || (*bp - buff >= BUFFER_LEN - 1)) {
	/* peak -- % escapes (at least) one character */
	char savec;

	safe_chr('%', buff, bp);
	(*str)++;
	savec = **str;
	if (!savec)
	  goto exit_sequence;
	safe_chr(savec, buff, bp);
	(*str)++;
	switch (savec) {
        case '<':
          savec = **str;
          if (!savec)
            goto exit_sequence;
          for (savec = **str; savec && savec != '>'; savec = **str) {
            safe_chr(savec, buff, bp);
            (*str)++;
          }
          if(!savec)
            goto exit_sequence;
          safe_chr(savec, buff, bp);
          (*str)++;
          break;
	case 'Q':
	case 'q':
          savec = **str;
          if (!savec)
            goto exit_sequence;
          safe_chr(savec, buff, bp);
          (*str)++;
          if (savec == '<') {
            for (savec = **str; savec && savec != '>'; savec = **str) {
              safe_chr(savec, buff, bp);
              (*str)++;
            }
            if(!savec)
              goto exit_sequence;
            safe_chr(savec, buff, bp);
            (*str)++;
          }
          break;
	case 'V':
	case 'v':
	case 'W':
	case 'w':
	case 'X':
	case 'x':
	  /* These sequences escape two characters */
	  savec = **str;
	  if (!savec)
	    goto exit_sequence;
	  safe_chr(savec, buff, bp);
	  (*str)++;
	}
	break;
      } else {
	char savec, nextc;
	char *savepos;
	ATTR *attrib;

	(*str)++;
	savec = **str;
	if (!savec) {
	  /* Line ended in %, so treat it as a literal */
	  safe_chr('%', buff, bp);
	  goto exit_sequence;
	}
	savepos = *bp;
	(*str)++;

	switch (savec) {
	case '%':		/* %% - a real % */
	  safe_chr('%', buff, bp);
	  break;
	case ' ':		/* "% " for more natural typing */
	  safe_str("% ", buff, bp);
	  break;
	case '!':		/* executor dbref */
	  safe_dbref(executor, buff, bp);
	  break;
	case '@':		/* caller dbref */
	  safe_dbref(caller, buff, bp);
	  break;
	case '#':		/* enactor dbref */
	  safe_dbref(enactor, buff, bp);
	  break;
	case ':':		/* enactor unique id */
	  safe_dbref(enactor, buff, bp);
	  safe_chr(':', buff, bp);
	  safe_integer(CreTime(enactor), buff, bp);
	  break;
	case '?':		/* function limits */
	  if (pe_info) {
	    safe_integer(pe_info->fun_invocations, buff, bp);
	    safe_chr(' ', buff, bp);
	    safe_integer(pe_info->fun_depth, buff, bp);
	  } else {
	    safe_str("0 0", buff, bp);
	  }
	  break;
	case '~':		/* enactor accented name */
	  safe_str(accented_name(enactor), buff, bp);
	  break;
	case '+':		/* argument count */
	  if (pe_info)
	    safe_integer(pe_info->arg_count, buff, bp);
	  else
	    safe_integer(0, buff, bp);
	  break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':		/* positional argument */
	  if (global_eval_context.wenv[savec - '0'])
	    safe_str(global_eval_context.wenv[savec - '0'], buff, bp);
	  break;
	case 'A':
	case 'a':		/* enactor absolute possessive pronoun */
	  if (gender < 0)
	    gender = get_gender(enactor);
	  safe_str(absp[gender], buff, bp);
	  break;
	case 'B':
	case 'b':		/* blank space */
	  safe_chr(' ', buff, bp);
	  break;
	case 'C':
	case 'c':		/* command line */
	  safe_str(global_eval_context.ccom, buff, bp);
	  break;
	case 'I':
	case 'i':
	  nextc = **str;
	  if (!nextc)
	    goto exit_sequence;
	  (*str)++;
	  if (!isdigit((unsigned char) nextc)) {
	    safe_str(T(e_int), buff, bp);
	    break;
	  }
	  inum_this = nextc - '0';
	  if (inum_this < 0 || inum_this >= inum
	      || (inum - inum_this) <= inum_limit) {
	    safe_str(T("#-1 ARGUMENT OUT OF RANGE"), buff, bp);
	  } else {
	    safe_str(iter_rep[inum - inum_this], buff, bp);
	  }
	  break;
	case 'U':
	case 'u':
	  safe_str(global_eval_context.ucom, buff, bp);
	  break;
	case 'L':
	case 'l':		/* enactor location dbref */
	  /* The security implications of this have
	   * already been talked to death.  Deal. */
	  safe_dbref(Location(enactor), buff, bp);
	  break;
	case 'N':
	case 'n':		/* enactor name */
	  safe_str(Name(enactor), buff, bp);
	  break;
	case 'O':
	case 'o':		/* enactor objective pronoun */
	  if (gender < 0)
	    gender = get_gender(enactor);
	  safe_str(obj[gender], buff, bp);
	  break;
	case 'P':
	case 'p':		/* enactor possessive pronoun */
	  if (gender < 0)
	    gender = get_gender(enactor);
	  safe_str(poss[gender], buff, bp);
	  break;
        case '<':
	  if (!**str)
	    goto exit_sequence;
          {
            const char *tmp;
            char atrname[BUFFER_LEN];
            ATTR *atr;

            for(tmp = *str; *tmp && *tmp != '>'; tmp++)
              ;
            if(!*tmp || tmp == *str) {
              (*str)--;
              goto exit_sequence;
            }
            strncpy(atrname, *str, tmp - *str);
            atrname[tmp - *str] = '\0';

            atr = atr_get(executor, strupper(atrname));
            if(atr)
              safe_str(atr_value(atr), buff, bp);
            *str = tmp + 1;
          }
          break;
	case 'Q':
	case 'q':		/* temporary storage */
	  nextc = **str;
	  if (!nextc)
	    goto exit_sequence;
	  (*str)++;
          if (nextc == '<') {
            const char *tmp;
            char regname[BUFFER_LEN];
            for(tmp = *str; *tmp && *tmp != '>'; tmp++)
              ;
            if(!*tmp || tmp == *str) {
              (*str)--;
              goto exit_sequence;
            }
            strncpy(regname, *str, tmp - *str);
            regname[tmp - *str] = '\0';
            safe_str(get_namedreg(&global_eval_context.namedregs, regname), buff, bp);
            *str = tmp + 1;
          } else {
	    if ((qindex = qreg_indexes[(unsigned char) nextc]) == -1)
	      break;
	    if (global_eval_context.renv[qindex])
	      safe_str(global_eval_context.renv[qindex], buff, bp);
          }
	  break;
	case 'R':
	case 'r':		/* newline */
	  if (NEWLINE_ONE_CHAR)
	    safe_chr('\n', buff, bp);
	  else
	    safe_str("\r\n", buff, bp);
	  break;
	case 'S':
	case 's':		/* enactor subjective pronoun */
	  if (gender < 0)
	    gender = get_gender(enactor);
	  safe_str(subj[gender], buff, bp);
	  break;
	case 'T':
	case 't':		/* tab */
	  safe_chr('\t', buff, bp);
	  break;
	case 'V':
	case 'v':
	case 'W':
	case 'w':
	case 'X':
	case 'x':		/* attribute substitution */
	  nextc = **str;
	  if (!nextc)
	    goto exit_sequence;
	  (*str)++;
	  temp[0] = UPCASE(savec);
	  temp[1] = UPCASE(nextc);
	  temp[2] = '\0';
	  attrib = atr_get(executor, temp);
	  if (attrib)
	    safe_str(atr_value(attrib), buff, bp);
	  break;
        case 'z':
        case 'Z':
          nextc = **str;
          (*str)++;
          if (!nextc) {
            goto exit_sequence;
          } else {
            switch (nextc) {
            case 'h':       /* hilite */
              safe_str(ANSI_HILITE, buff, bp);
              break;
            case 'i':       /* inverse */
              safe_str(ANSI_INVERSE, buff, bp);
              break;
            case 'f':       /* flash */
              safe_str(ANSI_BLINK, buff, bp);
              break;
            case 'u':       /* underscore */
              safe_str(ANSI_UNDERSCORE, buff, bp);
              break;
            case 'n':       /* normal */
              safe_str(ANSI_NORMAL, buff, bp);
              break;
            case 'x':       /* black fg */
              safe_str(ANSI_BLACK, buff, bp);
              break;
            case 'r':       /* red fg */
              safe_str(ANSI_RED, buff, bp);
              break;
            case 'g':       /* green fg */
              safe_str(ANSI_GREEN, buff, bp);
              break;
            case 'y':       /* yellow fg */
              safe_str(ANSI_YELLOW, buff, bp);
              break;
            case 'b':       /* blue fg */
              safe_str(ANSI_BLUE, buff, bp);
              break;
            case 'm':       /* magenta fg */
              safe_str(ANSI_MAGENTA, buff, bp);
              break;
            case 'c':       /* cyan fg */
              safe_str(ANSI_CYAN, buff, bp);
              break;
            case 'w':       /* white fg */
              safe_str(ANSI_WHITE, buff, bp);
              break;
            case 'X':       /* black bg */
              safe_str(ANSI_BBLACK, buff, bp);
              break;
            case 'R':       /* red bg */
              safe_str(ANSI_BRED, buff, bp);
              break;
            case 'G':       /* green bg */
              safe_str(ANSI_BGREEN, buff, bp);
              break;
            case 'Y':       /* yellow bg */
              safe_str(ANSI_BYELLOW, buff, bp);
              break;
            case 'B':       /* blue bg */
              safe_str(ANSI_BBLUE, buff, bp);
              break;
            case 'M':       /* magenta bg */
              safe_str(ANSI_BMAGENTA, buff, bp);
              break;
            case 'C':       /* cyan bg */
              safe_str(ANSI_BCYAN, buff, bp);
              break;
            case 'W':       /* white bg */
              safe_str(ANSI_BWHITE, buff, bp);
              break;
            }
            break;
          }
	default:		/* just copy */
	  safe_chr(savec, buff, bp);
	}

	if (isupper((unsigned char) savec))
	  *savepos = UPCASE(*savepos);
      }
      break;
    case '{':			/* "{}" parse group; recurse with no function check */
      if (CALL_LIMIT && (pe_info->call_depth > CALL_LIMIT)) {
	(*str)++;
	break;
      }
      if (eflags & PE_LITERAL) {
	safe_chr('{', buff, bp);
	(*str)++;
	break;
      }
      if (!(eflags & (PE_STRIP_BRACES | PE_COMMAND_BRACES)))
	safe_chr('{', buff, bp);
      (*str)++;
      if (process_expression(buff, bp, str,
			     executor, caller, enactor,
			     eflags & PE_COMMAND_BRACES
			     ? (eflags & ~PE_COMMAND_BRACES)
			     : (eflags &
				~(PE_STRIP_BRACES | PE_FUNCTION_CHECK)),
			     PT_BRACE, pe_info)) {
	retval = 1;
	break;
      }

      if (**str == '}') {
	if (!(eflags & (PE_STRIP_BRACES | PE_COMMAND_BRACES)))
	  safe_chr('}', buff, bp);
	(*str)++;
      }
      /* Only strip one set of braces for commands */
      eflags &= ~PE_COMMAND_BRACES;
      break;
    case '[':			/* "[]" parse group; recurse with mandatory function check */
      if (CALL_LIMIT && (pe_info->call_depth > CALL_LIMIT)) {
	(*str)++;
	break;
      }
      if (eflags & PE_LITERAL) {
	safe_chr('[', buff, bp);
	(*str)++;
	break;
      }
      if (!(eflags & PE_EVALUATE)) {
	safe_chr('[', buff, bp);
	temp_eflags = eflags & ~PE_STRIP_BRACES;
      } else
	temp_eflags = eflags | PE_FUNCTION_CHECK | PE_FUNCTION_MANDATORY;
      (*str)++;
      if (process_expression(buff, bp, str,
			     executor, caller, enactor,
			     temp_eflags, PT_BRACKET, pe_info)) {
	retval = 1;
	break;
      }
      if (**str == ']') {
	if (!(eflags & PE_EVALUATE))
	  safe_chr(']', buff, bp);
	(*str)++;
      }
      break;
    case '(':			/* Function call */
      if (CALL_LIMIT && (pe_info->call_depth > CALL_LIMIT)) {
	(*str)++;
	break;
      }
      (*str)++;
      if (!(eflags & PE_EVALUATE) || !(eflags & PE_FUNCTION_CHECK)) {
	safe_chr('(', buff, bp);
	if (**str == ' ') {
	  safe_chr(**str, buff, bp);
	  (*str)++;
	}
	if (process_expression(buff, bp, str,
			       executor, caller, enactor,
			       eflags & ~PE_STRIP_BRACES, PT_PAREN, pe_info))
	  retval = 1;
	if (**str == ')') {
	  if (eflags & PE_COMPRESS_SPACES && (*str)[-1] == ' ')
	    safe_chr(' ', buff, bp);
	  safe_chr(')', buff, bp);
	  (*str)++;
	}
	break;
      } else {
	char *sargs[10];
	char **fargs;
	int sarglens[10];
	int *arglens;
	int args_alloced;
	int nfargs;
	int j;
	static char name[BUFFER_LEN];
	char *sp, *tp;
	FUN *fp;
	int temp_tflags;
	int denied;

	fargs = sargs;
	arglens = sarglens;
	for (j = 0; j < 10; j++) {
	  fargs[j] = NULL;
	  arglens[j] = 0;
	}
	args_alloced = 10;
	eflags &= ~PE_FUNCTION_CHECK;
	/* Get the function name */
	for (sp = startpos, tp = name; sp < *bp; sp++)
	  safe_chr(UPCASE(*sp), name, &tp);
	*tp = '\0';
	fp = func_hash_lookup(name);
	if (!fp) {
	  if (eflags & PE_FUNCTION_MANDATORY) {
	    *bp = startpos;
	    safe_str(T("#-1 FUNCTION ("), buff, bp);
	    safe_str(name, buff, bp);
	    safe_str(") NOT FOUND", buff, bp);
	    if (process_expression(name, &tp, str,
				   executor, caller, enactor,
				   PE_NOTHING, PT_PAREN, pe_info))
	      retval = 1;
	    if (**str == ')')
	      (*str)++;
	    break;
	  }
	  safe_chr('(', buff, bp);
	  if (**str == ' ') {
	    safe_chr(**str, buff, bp);
	    (*str)++;
	  }
	  if (process_expression(buff, bp, str, executor, caller, enactor,
				 eflags, PT_PAREN, pe_info)) {
	    retval = 1;
	    break;
	  }
	  if (**str == ')') {
	    if (eflags & PE_COMPRESS_SPACES && (*str)[-1] == ' ')
	      safe_chr(' ', buff, bp);
	    safe_chr(')', buff, bp);
	    (*str)++;
	  }
	  break;
	}
	*bp = startpos;

	/* Check for the invocation limit */
	if ((pe_info->fun_invocations >= FUNCTION_LIMIT) ||
	    (global_fun_invocations >= FUNCTION_LIMIT * 5)) {
	  e_msg = T(e_invoke);
	  e_len = strlen(e_msg);
	  if ((buff + e_len > *bp) || strcmp(e_msg, *bp - e_len))
	    safe_str(e_msg, buff, bp);
	  if (process_expression(name, &tp, str,
				 executor, caller, enactor,
				 PE_NOTHING, PT_PAREN, pe_info))
	    retval = 1;
	  if (**str == ')')
	    (*str)++;
	  break;
	}
	/* Check for the recursion limit */
	if ((pe_info->fun_depth + 1 >= RECURSION_LIMIT) ||
	    (global_fun_recursions + 1 >= RECURSION_LIMIT * 5)) {
	  safe_str(T("#-1 FUNCTION RECURSION LIMIT EXCEEDED"), buff, bp);
	  if (process_expression(name, &tp, str,
				 executor, caller, enactor,
				 PE_NOTHING, PT_PAREN, pe_info))
	    retval = 1;
	  if (**str == ')')
	    (*str)++;
	  break;
	}
	/* Get the arguments */
	temp_eflags = (eflags & ~PE_FUNCTION_MANDATORY)
	  | PE_COMPRESS_SPACES | PE_EVALUATE | PE_FUNCTION_CHECK;
	switch (fp->flags & FN_ARG_MASK) {
	case FN_LITERAL:
	  temp_eflags |= PE_LITERAL;
	  /* FALL THROUGH */
	case FN_NOPARSE:
	  temp_eflags &= ~(PE_COMPRESS_SPACES | PE_EVALUATE |
			   PE_FUNCTION_CHECK);
	  break;
	}
	denied = !check_func(executor, fp);
	if (denied)
	  temp_eflags &=
	    ~(PE_COMPRESS_SPACES | PE_EVALUATE | PE_FUNCTION_CHECK);
	temp_tflags = PT_COMMA | PT_PAREN;
	nfargs = 0;
	do {
	  char *argp;
	  if ((fp->maxargs < 0) && ((nfargs + 1) >= -fp->maxargs))
	    temp_tflags = PT_PAREN;
	  if (nfargs >= args_alloced) {
	    char **nargs;
	    int *narglens;
	    nargs = (char **) mush_malloc((nfargs + 10) * sizeof(char *),
					  "process_expression.function_arglist");
	    narglens = (int *) mush_malloc((nfargs + 10) * sizeof(int),
					   "process_expression.function_arglens");
	    for (j = 0; j < nfargs; j++) {
	      nargs[j] = fargs[j];
	      narglens[j] = arglens[j];
	    }
	    if (fargs != sargs)
	      mush_free((Malloc_t) fargs,
			"process_expression.function_arglist");
	    if (arglens != sarglens)
	      mush_free((Malloc_t) arglens,
			"process_expression.function_arglens");
	    fargs = nargs;
	    arglens = narglens;
	    args_alloced += 10;
	  }
	  fargs[nfargs] = (char *) mush_malloc(BUFFER_LEN,
					       "process_expression.function_argument");
	  argp = fargs[nfargs];
	  if (process_expression(fargs[nfargs], &argp, str,
				 executor, caller, enactor,
				 temp_eflags, temp_tflags, pe_info)) {
	    retval = 1;
	    nfargs++;
	    goto free_func_args;
	  }
	  *argp = '\0';
	  arglens[nfargs] = argp - fargs[nfargs];
	  (*str)++;
	  nfargs++;
	} while ((*str)[-1] == ',');
	if ((*str)[-1] != ')')
	  (*str)--;
	/* See if this function is enabled */
	/* Can't do this check earlier, because of possible side effects
	 * from the functions.  Bah. */
	if (denied) {
	  if (fp->flags & FN_DISABLED)
	    safe_str(T(e_disabled), buff, bp);
	  else
	    safe_str(T(e_perm), buff, bp);
	  goto free_func_args;
	} else {
	  /* If we have the right number of args, eval the function.
	   * Otherwise, return an error message.
	   * Special case: zero args is recognized as one null arg.
	   */
	  if ((fp->minargs == 0 || (fp->minargs == 1 && (fp->flags & FN_ONEARG))) && (nfargs == 1) && (!*fargs[0] || arglens[0]== 0)) {
	    mush_free((Malloc_t) fargs[0],
		      "process_expression.function_argument");
	    fargs[0] = NULL;
	    arglens[0] = 0;
	    nfargs = 0;
	  }
	  if ((nfargs < fp->minargs) || (nfargs > abs(fp->maxargs))) {
	    safe_str(T("#-1 FUNCTION ("), buff, bp);
	    safe_str(fp->name, buff, bp);
	    safe_str(") EXPECTS ", buff, bp);
	    if (fp->minargs == abs(fp->maxargs)) {
	      safe_integer(fp->minargs, buff, bp);
	    } else if ((fp->minargs + 1) == abs(fp->maxargs)) {
	      safe_integer(fp->minargs, buff, bp);
	      safe_str(" OR ", buff, bp);
	      safe_integer(abs(fp->maxargs), buff, bp);
	    } else if (fp->maxargs == INT_MAX) {
	      safe_str("AT LEAST ", buff, bp);
	      safe_integer(fp->minargs, buff, bp);
	    } else {
	      safe_str("BETWEEN ", buff, bp);
	      safe_integer(fp->minargs, buff, bp);
	      safe_str(" AND ", buff, bp);
	      safe_integer(abs(fp->maxargs), buff, bp);
	    }
	    safe_str(" ARGUMENTS BUT GOT ", buff, bp);
	    safe_integer(nfargs, buff, bp);
	  } else {
	    global_fun_recursions++;
	    pe_info->fun_depth++;
	    if (fp->flags & FN_BUILTIN) {
	      global_fun_invocations++;
	      pe_info->fun_invocations++;
	      fp->where.fun(fp, buff, bp, nfargs, fargs, arglens, executor,
			    caller, enactor, fp->name, pe_info);
	      if (fp->flags & FN_LOGARGS) {
		char logstr[BUFFER_LEN];
		char *logp;
		int logi;
		logp = logstr;
		safe_str(fp->name, logstr, &logp);
		safe_chr('(', logstr, &logp);
		for (logi = 0; logi < nfargs; logi++) {
		  safe_str(fargs[logi], logstr, &logp);
		  if (logi + 1 < nfargs)
		    safe_chr(',', logstr, &logp);
		}
		safe_chr(')', logstr, &logp);
		*logp = '\0';
		do_log(LT_CMD, executor, caller, "%s", logstr);
	      } else if (fp->flags & FN_LOGNAME)
		do_log(LT_CMD, executor, caller, "%s()", fp->name);
	    } else {
	      dbref thing;
	      ATTR *attrib;
	      global_fun_invocations++;
	      pe_info->fun_invocations++;
	      thing = userfn_tab[fp->where.offset].thing;
	      attrib = atr_get(thing, userfn_tab[fp->where.offset].name);
	      if (!attrib) {
		do_rawlog(LT_ERR,
			  T("ERROR: @function (%s) without attribute (#%d/%s)"),
			  fp->name, thing, userfn_tab[fp->where.offset].name);
		safe_str("#-1 @FUNCTION (", buff, bp);
		safe_str(fp->name, buff, bp);
		safe_str(") MISSING ATTRIBUTE (", buff, bp);
		safe_dbref(thing, buff, bp);
		safe_chr('/', buff, bp);
		safe_str(userfn_tab[fp->where.offset].name, buff, bp);
		safe_chr(')', buff, bp);
	      } else { 
		char *preserve[NUMQ];
		dbref local_ooref;
		if (fp->flags & FN_LOCALIZE)
		  save_global_regs("@function.save", preserve);
		/* Temporarily change ooref */
		local_ooref = ooref;
		ooref = attrib->creator;
		do_userfn(buff, bp, thing, attrib, nfargs, fargs,
			  executor, caller, enactor, pe_info);
		ooref = local_ooref;
		if (fp->flags & FN_LOCALIZE)
		  restore_global_regs("@function.save", preserve);
	      }
	    }
	    pe_info->fun_depth--;
	    global_fun_recursions--;
	  }
	}
	/* Free up the space allocated for the args */
      free_func_args:
	for (j = 0; j < nfargs; j++)
	  if (fargs[j])
	    mush_free((Malloc_t) fargs[j],
		      "process_expression.function_argument");
	if (fargs != sargs)
	  mush_free((Malloc_t) fargs, "process_expression.function_arglist");
	if (arglens != sarglens)
	  mush_free((Malloc_t) arglens, "process_expression.function_arglens");
      }
      break;
      /* Space compression */
    case ' ':
      had_space = 1;
      safe_chr(' ', buff, bp);
      (*str)++;
      if (eflags & PE_COMPRESS_SPACES) {
        while (**str == ' ')
          (*str)++;
      } else
        while (**str == ' ') {
          safe_chr(' ', buff, bp);
          (*str)++;
        }
      break;
      /* Escape charater */
    case '\\':
      if (!(eflags & PE_EVALUATE))
        safe_chr('\\', buff, bp);
      (*str)++;
      if (!**str)
        goto exit_sequence;
      /* FALL THROUGH */
      /* Basic character */
    default:
      safe_chr(**str, buff, bp);
      (*str)++;
      break;
    }
  }

exit_sequence:
  if (eflags != PE_NOTHING) {
    if ((eflags & PE_COMPRESS_SPACES) && had_space &&
        ((*str)[-1] == ' ') && ((*bp)[-1] == ' '))
      (*bp)--;
    if (debugging) {
      pe_info->nest_depth--;
      **bp = '\0';
      if (strcmp(sourcestr, startpos)) {
        static char dbuf[BUFFER_LEN];
        char *dbp;
        if (pe_info->debug_strings) {
          while (pe_info->debug_strings->prev)
            pe_info->debug_strings = pe_info->debug_strings->prev;
          while (pe_info->debug_strings->next) {
            dbp = dbuf;
            dbuf[0] = '\0';
            safe_format(dbuf, &dbp, "%s :", pe_info->debug_strings->string);
            *dbp = '\0';
            if (Connected(Owner(executor)))
              raw_notify(Owner(executor), dbuf);
            notify_list(executor, executor, "DEBUGFORWARDLIST", dbuf,
                        NA_NOLISTEN | NA_NOPREFIX);
            pe_info->debug_strings = pe_info->debug_strings->next;
            mush_free((Malloc_t) pe_info->debug_strings->prev,
                      "process_expression.debug_node");
          }
          mush_free((Malloc_t) pe_info->debug_strings,
                    "process_expression.debug_node");
          pe_info->debug_strings = NULL;
        }
        dbp = dbuf;
        dbuf[0] = '\0';
        safe_format(dbuf, &dbp, "%s => %s", debugstr, startpos);
        *dbp = '\0';
        if (Connected(Owner(executor)))
          raw_notify(Owner(executor), dbuf);
        notify_list(executor, executor, "DEBUGFORWARDLIST", dbuf,
                    NA_NOLISTEN | NA_NOPREFIX);
      } else {
        Debug_Info *node;
        node = pe_info->debug_strings;
        if (node) {
          pe_info->debug_strings = node->prev;
          if (node->prev)
            node->prev->next = NULL;
          mush_free((Malloc_t) node, "process_expression.debug_node");
        }
      }
      mush_free((Malloc_t) debugstr, "process_expression.debug_source");
    }
    if (realbuff) {
      **bp = '\0';
      *bp = realbp;
      safe_str(buff, realbuff, bp);
      mush_free((Malloc_t) buff, "process_expression.buffer_extension");
    }
  }
  /* Once we cross call limit, we stay in error */
  if (pe_info && CALL_LIMIT && pe_info->call_depth <= CALL_LIMIT)
    pe_info->call_depth--;
  if (made_info)
    mush_free((Malloc_t) pe_info, "process_expression.pe_info");
  if (old_iter_limit != -1) {
    inum_limit = old_iter_limit;
  }
  return retval;
}

#ifdef WIN32
#pragma warning( default : 4761)        /* NJG: enable warning re conversion */
#endif
