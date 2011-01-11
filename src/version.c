/* This file defines the @version command. It's all by itself because
 * we want to rebuild this file at every compilation, so that the
 * BUILDDATE is correct
 */
#include "config.h"
#include "copyrite.h"

#ifdef I_STRING
#include <string.h>
#else
#include <strings.h>
#endif
#include <time.h>
#include "conf.h"

#include "externs.h"
#include "version.h"
#include "patches.h"
#ifndef WIN32
#include "buildinf.h"
#endif
#include "confmagic.h"

void do_version _((dbref player));

void
do_version(player)
    dbref player;
{
  char buff[BUFFER_LEN];

  notify_format(player, T("You are connected to %s"), MUDNAME);

  strcpy(buff, ctime(&globals.start_time));
  buff[strlen(buff) - 1] = '\0';        /* eat the newline */
  notify_format(player, T("Last restarted: %s"), buff);

  notify_format(player, "CobraMUSH v%s [%s]", VERSION, VBRANCH);
#ifdef PATCHES
  notify_format(player, "Patches: %s", PATCHES);
#endif
#ifdef WIN32
  notify_format(player, T("Build date: %s"), __DATE__);
#else
  notify_format(player, T("Build date: %s"), BUILDDATE);
  notify_format(player, T("Compiler: %s"), COMPILER);
  notify_format(player, T("Compilation flags: %s"), CCFLAGS);
  notify_format(player, T("Malloc package: %d"), MALLOC_PACKAGE);
#endif
}
