/* CobraMUSH Module/Plugin System - File created 3/31/11
 *
 * Our Approach shall be one using the libtool library, mostly for the fact 
 * that is a cross-platform designed library.
 */

#include <unistd.h>
#include <string.h>
#include <ltdl.h>

#include "conf.h"
#include "externs.h"
#include "command.h"
#include "switches.h"
#include "mushtype.h"
#include "log.h"
#include "modules.h"

struct module_entry_t *module_list = NULL;

/* Ads a module entry and returns */
struct module_entry_t *module_entry_add(char *name) {
  struct module_entry_t *entry;

  entry = (struct module_entry_t *) mush_malloc(sizeof(struct module_entry_t), "MODULE_ENTRY");

  if(!entry) {
    return NULL; /* Should really panic here.. */
  }

  entry->name = mush_strdup(name, "MODULE_ENTRY.NAME");
  entry->info = NULL;
  entry->load = NULL;
  entry->unload = NULL;
  entry->next = module_list;
  module_list = entry;

  return entry;
}

/* removes a module entry from the list */
int module_entry_del(struct module_entry_t *entry) {
  struct module_entry_t *m, *e = NULL;

  if(module_list == entry)
    e = module_list;
  else MODULE_ITER(m)
    if(m->next != NULL && m->next == entry) {
      e = m;
      m->next = m->next->next;
      break;
    }

  if(e == NULL)
    return 0;
  
  MODULE_CAF(e->name, "MODULE_ENTRY.NAME");
  MODULE_CAF(e->info, "MODULE_ENTRY.INFO");
  MODULE_CAF(e, "MODULE_ENTRY");

  return 1;
}

/* Initialize Module Interface */
int modules_init() {
  return lt_dlinit(); /* Initialize libltdl. 0 is good */
}

/* Load Module name */
int module_open(char *path, char *name) {
  char file[BUFFER_LEN];
  int (*module_loader)(struct module_entry_t *);
  struct module_entry_t *module;
  void *handle;

  memset(file, '\0', BUFFER_LEN);
  snprintf(file, BUFFER_LEN, "%s/%s", path, name);

  handle = lt_dlopen(file);

  if(!handle) {
    do_rawlog(LT_ERR, "Error Loading Module: %s | %s ", file, lt_dlerror());
    return 0;
  }

  /* Some OSes may need symbols to be prefixed with _.. Will need to look into autoconfig code for this */
  module_loader = MODULE_FUNCRET(handle, "module_load");

  if(!module_loader) {
    do_rawlog(LT_ERR, "Error Loading Module: Could not call module_load | %s", file);
    return 0;
  }

  /* Add the module to the linked list */
  module = module_entry_add(name);
  if(module == NULL) {
    return 0;
  }
  module->handle = handle;
  module->load = module_loader;
  module->unload = MODULE_FUNCRET(handle, "module_unload");
  
  /* Grab info and version from module & put it in */
  /* Success.. Call the module */
  return module_loader(module);
}

/* Unload one Module */
int module_close(struct module_entry_t *m) {
  int ret;

  ret =  m->unload(m); /* first run requierd unload code */
  
  if(!ret)
    return 0;

  /* next unload the modules for real */
  ret = lt_dlclose(m->handle);

  if(ret != 0) {
    do_rawlog(LT_ERR, "Could not unload module: %s/%d | %s", __FILE__, __LINE__, lt_dlerror());
    return 0;
  }

  if(!module_entry_del(m)) {
    do_rawlog(LT_ERR, "Could not unload module: %s/%d ", __FILE__, __LINE__);
    return 0;
  }

  return 1;
}

/* Unload all modules */
int modules_shutdown() {
  struct module_entry_t *m;

  MODULE_ITER(m)
    m->unload(m);

  return lt_dlexit(); /* 0 is good */
}

/* MUSH Module Interface Portion... Much of this section will probably actually move to a module itself and become a resident module */

  /* command will be restricted to Director player in nature from the command file */
COMMAND(cmd_module) {
  struct module_entry_t *m;
  char tbuf1[BUFFER_LEN];
  int ret;

  if(SW_ISSET(sw, SWITCH_LOAD)) {
    if(!arg_left) {
      notify(player, "Requires at least one argument.");
      return;
    }
    /* Load a Module By name in arg_left */
    (void) getcwd(tbuf1, BUFFER_LEN);
    ret = module_open(tbuf1, arg_left);
    if(ret) 
      notify(player, "module successfully loaded.");
    else
      notify(player, "module could not load.");

  } else if(module_list == NULL) {
    notify_format(player, "Search-path: %s", lt_dlgetsearchpath());
    notify(player, "No modules loaded.");
    return;
  } else if(SW_ISSET(sw, SWITCH_UNLOAD)) {
    /* This is after the no modules loaded one.. because you have to modules loaded in order to unload something */
    if(!arg_left) {
      notify(player, "Reqiures at least one argument.");
      return;
    }
    MODULE_ITER(m)
      if(!strcasecmp(arg_left, m->name)) {
	if(module_close(m))
	  notify(player, "unloaded module");
	else
	  notify(player, "could not unload module");
	return;
      }
  } else {
  /* No Switches.. Just view modules loaded */
   MODULE_ITER(m)  {
    notify_format(player, "Module-Name: %s", m->name);
   }
  }
}


