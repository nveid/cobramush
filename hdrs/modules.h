/* CobraMUSH Module System based upon cross-platform libldl */
#ifndef _MODULES_H_
#define _MODULES_H_

struct module_entry_t {
  char *name; /* Name of module */
  char *info; /* Short Description */
  float version; /* Version of Module */
  int (*load)(struct module_entry_t *); /* Load Function */
  int (*unload)(struct module_entry_t *); /* Unload function */
  void *handle;
  struct module_entry_t *next;
};

struct module_entry_t *module_entry_add(char *name);
int module_entry_del(struct module_entry_t *entry);
int modules_init(void);
int module_open(char *path, char *name);
int module_close(struct module_entry_t *m);
int modules_shutdown(void);


/* Iterate through Module List */
#define MODULE_ITER(m) for(m = module_list ; m != NULL ; m = m->next)
/* Check and Free */
#define MODULE_CAF(mem,check)  if(mem != NULL) \
				  mush_free(mem, check);
/* Retrieve Module Function */
#define MODULE_FUNC(h, m, func, ...) \
  if((h = lt_dlsym(m, func))) { \
    h(__VA_ARGS__); \
  }
#define MODULE_FUNC_NOARGS(h, m, func) \
  if((h = lt_dlsym(m, func))) { \
    h(); \
  }

#define MODULE_FUNCRET(m, func) lt_dlsym(m, func);


#endif /* _MODULES_H_ */
