/* pueblo.h */

#ifndef __PUEBLO_H
#define __PUEBLO_H

/* Ok. The original idea for this came from seeing the Tiny patch for Pueblo. 
 * A few months later I felt the urge to add some pueblo thingies to Penn, 
 * and did so, though at a quite different level. This led to the 
 * discovery of the trouble with bsd.c, which also got partly rewritten.
 * The commands @emit/html and pemit_html() were added to have Tiny compability
 */

#include "conf.h"

#define PUEBLOBUFF \
       char pbuff[BUFFER_LEN]; \
       char *pp
#define PUSE \
       pp=pbuff
#define PEND \
       *pp=0;

#define notify_nopenter_by(t,a,b) notify_anything(t, na_one, &(a), NULL, NA_NOPENTER, b)
#define notify_nopenter(a,b) notify_nopenter_by(GOD, a, b)
#define notify_noenter_by(t,a,b) notify_anything(t, na_one, &(a), NULL, NA_NOENTER, b)
#define notify_noenter(a,b) notify_noenter_by(GOD, a, b)

#define tag_wrap(a,b,c) safe_tag_wrap(a,b,c,pbuff,&pp,NOTHING)
#define tag(a) safe_tag(a,pbuff,&pp)
#define tag_cancel(a) safe_tag_cancel(a,pbuff,&pp)

#define notify_pueblo(a,b) notify_anything(GOD, na_one, &(a), NULL, NA_PONLY | NA_NOPENTER | NA_NOLISTEN, b)

int safe_tag(char const *a_tag, char *buf, char **bp);
int safe_tag_cancel(char const *a_tag, char *buf, char **bp);
int safe_tag_wrap(char const *a_tag, char const *params,
                  char const *data, char *buf, char **bp, dbref player);

/* Please STAY SANE when modifying. 
 * Making this something like 'x' and 'y' is a BAD IDEA 
 */

#define TAG_START '\02'
#define TAG_END '\03'

/* Start & Cancel raw mud text */
/* Use this for when we go into pueblo displaying shit */
#define PUEBLO_CMT(buf,bp)  safe_str("</XCH_MUDTEXT>", buf, bp); \
			    safe_str("<IMG XCH_MODE=HTML>", buf, bp);

/* Use this when we go back to normal shit */
#define PUEBLO_SMT(buf, bp) safe_str("<IMG XCH_MODE=TEXT>", buf, bp);

#endif
