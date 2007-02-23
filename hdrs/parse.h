/* parse.h - parser declarations and macros

 * Written by T. Alexander Popiel, 13 May 1995
 * Last modified by T. Alexander Popiel, 26 May 1995
 *
 * Copyright (c) 1995 by T. Alexander Popiel
 * See copyrite.h for details.
 */

#ifndef _PARSE_H_
#define _PARSE_H_

#include "copyrite.h"

#include "config.h"
#include <stdlib.h>
#include <limits.h>
#include <math.h>

#include "confmagic.h"

/* This is the type to be used for numbers which may be non-integral. */
#define HUGE_NVAL       HUGE_DOUBLE

/* These are some common error messages. */
extern char e_int[];		/* #-1 ARGUMENT MUST BE INTEGER */
extern char e_ints[];		/* #-1 ARGUMENTS MUST BE INTEGERS */
extern char e_uint[];		/* #-1 ARGUMENT MUST BE POSITIVE INTEGER */
extern char e_uints[];		/* #-1 ARGUMENTS MUST BE POSITIVE INTEGERS */
extern char e_num[];		/* #-1 ARGUMENT MUST BE NUMBER */
extern char e_nums[];		/* #-1 ARGUMENTS MUST BE NUMBERS */
extern char e_perm[];		/* #-1 PERMISSION DENIED */
extern char e_atrperm[];	/* #-1 NO PERMISSION TO GET ATTRIBUTE */
extern char e_match[];		/* #-1 NO MATCH */
extern char e_notvis[];		/* #-1 NO SUCH OBJECT VISIBLE */
extern char e_disabled[];	/* #-1 FUNCTION DISABLED */
extern char e_range[];		/* #-1 OUT OF RANGE */

/* The following routines all take strings as arguments, and return
 * data of the appropriate types.  
 */

extern int parse_boolean(char const *str);
extern dbref parse_dbref(char const *str);
extern dbref qparse_dbref(char const *str);
extern dbref parse_objid(char const *str);

#define parse_integer(str) strtol(str, NULL, 10)
#define parse_uinteger(str) strtoul(str, NULL, 10)

#define parse_number(str) strtod(str, NULL)



/* The following routines all take varoius arguments, and return
 * string representations of same.  The string representations
 * are stored in static buffers, so the next call to each function
 * will destroy any old string that was there.
 */

#define unparse_boolean(x) ((x) ? "1" : "0")

char *unparse_dbref(dbref num);
char *unparse_integer(int num);
char *unparse_uinteger(unsigned int num);
char *unparse_number(NVAL num);
char *unparse_types(int type);

/* The following routines all take strings as arguments, and return
 * true iff the string is a valid representation of the appropriate type.
 */
int is_dbref(char const *str);
int is_objid(char const *str);

int is_integer(char const *str);
int is_uinteger(char const *str);
int is_boolean(char const *str);

/* Split a sep-delimited string into individual elements */
extern int list2arr(char *r[], int max, char *list, char sep);
/* split up a sep-delimietd string into individual elements and acknowledge that the seperate character may be escaepd out */
extern int elist2arr(char *r[], int max, char *list, char sep);
/* The reverse */
extern void arr2list(char *r[], int max, char *list, char **lp, char *sep);




/* All function declarations follow the format: */
#ifndef HAVE_FUN_DEFINED
typedef struct fun FUN;
#define HAVE_FUN_DEFINED
#endif
/** Common declaration for softcode function implementations */
#define FUNCTION(fun_name) \
  /* ARGSUSED */ /* try to keep lint happy */ \
  void fun_name (FUN *fun, char *buff, char **bp, int nargs, char *args[], \
                   int arglens[], dbref executor, dbref caller, dbref enactor, \
                   char const *called_as, PE_Info *pe_info); \
  void fun_name(FUN *fun __attribute__ ((__unused__)), \
                char *buff __attribute__ ((__unused__)), \
                char **bp  __attribute__ ((__unused__)), \
                int nargs  __attribute__ ((__unused__)), \
                char *args[]  __attribute__ ((__unused__)), \
                int arglens[] __attribute__ ((__unused__)), \
                dbref executor  __attribute__ ((__unused__)), \
                dbref caller  __attribute__ ((__unused__)), \
                dbref enactor  __attribute__ ((__unused__)), \
                char const *called_as  __attribute__ ((__unused__)), \
                PE_Info *pe_info  __attribute__ ((__unused__)))

/* All results are returned in buff, at the point *bp.  *bp is likely
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
 */

/* process_expression() evaluates expressions.  What a concept. */

int process_expression(char *buff, char **bp, char const **str,
		       dbref executor, dbref caller, dbref enactor,
		       int eflags, int tflags, PE_Info * pe_info);

/* buff is a pointer to a BUFFER_LEN string to contain the expression
 * result.  *bp is the point in buff at which the result should be written.
 * *bp will be updated to point one past the result of the expression,
 * and the result will _NOT_ be null-terminated.
 * For top-level calls to process_expression(), *bp should probably equal
 * buff.  For calls to process_expression() inside function implementations,
 * buff and bp should probably be the values passed into the implementation.
 *
 * *str is a pointer to a string containing the expression to evaluate.
 * *str will be updated to point at the terminator which caused return
 * from process_expression().  The string pointed to by *str will not
 * be modified.
 *
 * executor, caller, and enactor represent %!, %@, and %#, respectively.
 * No validity checking of any sort is done on these parameters, so please
 * be careful with them.
 *
 * eflags consists of one or more of the following evaluation flags:
 */

#define PE_NOTHING              0
#define PE_COMPRESS_SPACES      0x00000001
#define PE_STRIP_BRACES         0x00000002
#define PE_COMMAND_BRACES       0x00000004
#define PE_EVALUATE             0x00000010
#define PE_FUNCTION_CHECK       0x00000020
#define PE_FUNCTION_MANDATORY   0x00000040
#define PE_LITERAL              0x00000100
#define PE_DOLLAR               0x00000200
#define PE_DEBUG                0x00000400

#define PE_DEFAULT (PE_COMPRESS_SPACES | PE_STRIP_BRACES | \
                    PE_DOLLAR | PE_EVALUATE | PE_FUNCTION_CHECK)
#define PE_UDEFAULT (PE_COMPRESS_SPACES | PE_STRIP_BRACES | \
		     PE_EVALUATE | PE_FUNCTION_CHECK)

/* PE_COMPRESS_SPACES strips leading and trailing spaces, and reduces sets
 * of internal spaces to one space.
 *
 * PE_STRIP_BRACES strips off top-level braces.
 *
 * PE_COMMAND_BRACES strips off only completely enclosing braces,
 * suitable for trimming command lists given to noparse commands like
 * @switch or @break.
 *
 * PE_EVALUATE allows %-substitutions, []-evaluation, function evaluation,
 * and \-stripping.
 *
 * PE_FUNCTION_CHECK allows function evaluation.  Note that both PE_EVALUATE
 * and PE_FUNCTION_CHECK must be active for function evaluation to occur.
 *
 * PE_FUNCTION_MANDATORY causes an error to be reported if a function call
 * is attempted for a non-existant function.  Otherwise, the function call
 * is not evaluated, but rather treated as normal text.
 *
 * PE_LITERAL prevents { and [ from being recognized and causing recursion.
 *
 * PE_DEFAULT is the most commonly used set of flags, normally sufficient
 * for calls to process_expression().
 *
 * PE_UDEFAULT is PE_DEFAULT without PE_DOLLAR, intended for use in
 * calling attributes (via u(), mix, step, etc)
 *
 *
 * tflags consists of one or more of the following termination flags:
 */

#define PT_NOTHING      0
#define PT_BRACE        0x00000001
#define PT_BRACKET      0x00000002
#define PT_PAREN        0x00000004
#define PT_COMMA        0x00000008
#define PT_SEMI         0x00000010
#define PT_EQUALS       0x00000020
#define PT_SPACE        0x00000040

/* These represent '\0', '}', ']', ')', ',', ';', '=', and ' ', respectively.
 * If the character corresponding to a set flag is encountered, then
 * process_expression() will exit, with *str pointing at the terminating
 * charater.  '\0' is always a terminating character.
 *
 * PT_DEFAULT, below, is provided as syntactic sugar.
 */

#define PT_DEFAULT PT_NOTHING

/* pe_info is a pointer to a structure of internal state information
 * for process_expression().  Top-level calls to process_expression()
 * should pass a NULL as pe_info.  Calls to process_expression() from
 * function implementations should pass their pe_info as pe_info.
 * In no case should any other pe_info be passed to process_expression().
 */

/* For the cpu time limiting. From timer.c */
extern void start_cpu_timer(void);
extern void reset_cpu_timer(void);

#endif				/* !_PARSE_H_ */
