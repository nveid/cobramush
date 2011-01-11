/* ansi.h */

/* ANSI control codes for various neat-o terminal effects

 * Some older versions of Ultrix don't appear to be able to
 * handle these escape sequences. If lowercase 'a's are being
 * stripped from @doings, and/or the output of the ANSI flag
 * is screwed up, you have the Ultrix problem.
 *
 * To fix the ANSI problem, try replacing the '\x1B' with '\033'.
 * To fix the problem with 'a's, replace all occurrences of '\a'
 * in the code with '\07'.
 *
 */

#ifndef __ANSI_H
#define __ANSI_H

#define ANSI_BLACK_V    (30)
#define ANSI_RED_V      (31)
#define ANSI_GREEN_V    (32)
#define ANSI_YELLOW_V   (33)
#define ANSI_BLUE_V     (34)
#define ANSI_MAGENTA_V  (35)
#define ANSI_CYAN_V     (36)
#define ANSI_WHITE_V    (37)

#define BEEP_CHAR     '\a'
#define ESC_CHAR      '\x1B'

#define ANSI_BEGIN   "\x1B["

#define ANSI_NORMAL   "\x1B[0m"

#define ANSI_HILITE   "\x1B[1m"
#define ANSI_INVERSE  "\x1B[7m"
#define ANSI_BLINK    "\x1B[5m"
#define ANSI_UNDERSCORE "\x1B[4m"

#define ANSI_INV_BLINK         "\x1B[7;5m"
#define ANSI_INV_HILITE        "\x1B[1;7m"
#define ANSI_BLINK_HILITE      "\x1B[1;5m"
#define ANSI_INV_BLINK_HILITE  "\x1B[1;5;7m"

/* Foreground colors */

#define ANSI_BLACK      "\x1B[30m"
#define ANSI_RED        "\x1B[31m"
#define ANSI_GREEN      "\x1B[32m"
#define ANSI_YELLOW     "\x1B[33m"
#define ANSI_BLUE       "\x1B[34m"
#define ANSI_MAGENTA    "\x1B[35m"
#define ANSI_CYAN       "\x1B[36m"
#define ANSI_WHITE      "\x1B[37m"

/* Background colors */

#define ANSI_BBLACK     "\x1B[40m"
#define ANSI_BRED       "\x1B[41m"
#define ANSI_BGREEN     "\x1B[42m"
#define ANSI_BYELLOW    "\x1B[43m"
#define ANSI_BBLUE      "\x1B[44m"
#define ANSI_BMAGENTA   "\x1B[45m"
#define ANSI_BCYAN      "\x1B[46m"
#define ANSI_BWHITE     "\x1B[47m"

#define ANSI_END        "m"

#endif                          /* __ANSI_H */
