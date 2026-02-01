
 

#include <linux/firmware.h>
#include "../firmware.h"

 
#ifdef CONFIG_FW_LOADER

struct builtin_fw {
	char *name;
	void *data;
	unsigned long size;
};

extern struct builtin_fw __start_builtin_fw[];
extern struct builtin_fw __end_builtin_fw[];

static bool fw_copy_to_prealloc_buf(struct firmware *fw,
				    void *buf, size_t size)
{
	if (!buf)
		return true;
	if (size < fw->size)
		return false;
	memcpy(buf, fw->data, fw->size);
	return true;
}

 
bool firmware_request_builtin(struct firmware *fw, const char *name)
{
	struct builtin_fw *b_fw;

	if (!fw)
		return false;

	for (b_fw = __start_builtin_fw; b_fw != __end_builtin_fw; b_fw++) {
		if (strcmp(name, b_fw->name) == 0) {
			fw->size = b_fw->size;
			fw->data = b_fw->data;
			return true;
		}
	}

	return false;
}
EXPORT_SYMBOL_NS_GPL(firmware_request_builtin, TEST_FIRMWARE);

 
bool firmware_request_builtin_buf(struct firmware *fw, const char *name,
				  void *buf, size_t size)
{
	if (!firmware_request_builtin(fw, name))
		return false;

	return fw_copy_to_prealloc_buf(fw, buf, size);
}

bool firmware_is_builtin(const struct firmware *fw)
{
	struct builtin_fw *b_fw;

	for (b_fw = __start_builtin_fw; b_fw != __end_builtin_fw; b_fw++)
		if (fw->data == b_fw->data)
			return true;

	return false;
}

#endif
