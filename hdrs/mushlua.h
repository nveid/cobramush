#ifndef _MUSHLUA_H_
#define _MUSHLUA_H_

void mush_lua_start(void);
void mush_lua_stop(void);
void luaopen_mush(lua_State *);

#endif /* _MUSHLUA_H_ */
