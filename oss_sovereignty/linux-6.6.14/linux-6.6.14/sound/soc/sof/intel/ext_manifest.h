#ifndef __INTEL_CAVS_EXT_MANIFEST_H__
#define __INTEL_CAVS_EXT_MANIFEST_H__
#include <sound/sof/ext_manifest.h>
enum sof_cavs_config_elem_type {
	SOF_EXT_MAN_CAVS_CONFIG_EMPTY		= 0,
	SOF_EXT_MAN_CAVS_CONFIG_CAVS_LPRO	= 1,
	SOF_EXT_MAN_CAVS_CONFIG_OUTBOX_SIZE	= 2,
	SOF_EXT_MAN_CAVS_CONFIG_INBOX_SIZE	= 3,
};
struct sof_ext_man_cavs_config_data {
	struct sof_ext_man_elem_header hdr;
	struct sof_config_elem elems[];
} __packed;
#endif  
