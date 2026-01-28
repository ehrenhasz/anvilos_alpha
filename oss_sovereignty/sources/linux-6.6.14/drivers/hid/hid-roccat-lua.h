
#ifndef __HID_ROCCAT_LUA_H
#define __HID_ROCCAT_LUA_H





#include <linux/types.h>

enum {
	LUA_SIZE_CONTROL = 8,
};

enum lua_commands {
	LUA_COMMAND_CONTROL = 3,
};

struct lua_device {
	struct mutex lua_lock;
};

#endif
