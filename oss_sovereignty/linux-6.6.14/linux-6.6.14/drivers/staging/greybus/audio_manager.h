#ifndef _GB_AUDIO_MANAGER_H_
#define _GB_AUDIO_MANAGER_H_
#include <linux/kobject.h>
#include <linux/list.h>
#define GB_AUDIO_MANAGER_NAME "gb_audio_manager"
#define GB_AUDIO_MANAGER_MODULE_NAME_LEN 64
#define GB_AUDIO_MANAGER_MODULE_NAME_LEN_SSCANF "63"
struct gb_audio_manager_module_descriptor {
	char name[GB_AUDIO_MANAGER_MODULE_NAME_LEN];
	int vid;
	int pid;
	int intf_id;
	unsigned int ip_devices;
	unsigned int op_devices;
};
struct gb_audio_manager_module {
	struct kobject kobj;
	struct list_head list;
	int id;
	struct gb_audio_manager_module_descriptor desc;
};
int gb_audio_manager_add(struct gb_audio_manager_module_descriptor *desc);
int gb_audio_manager_remove(int id);
void gb_audio_manager_remove_all(void);
struct gb_audio_manager_module *gb_audio_manager_get_module(int id);
void gb_audio_manager_put_module(struct gb_audio_manager_module *module);
int gb_audio_manager_dump_module(int id);
void gb_audio_manager_dump_all(void);
#endif  
