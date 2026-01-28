#define LPFC_ATTR(name, defval, minval, maxval, desc) \
static uint lpfc_##name = defval;\
module_param(lpfc_##name, uint, S_IRUGO);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_param_init(name, defval, minval, maxval)
#define LPFC_ATTR_R(name, defval, minval, maxval, desc) \
static uint lpfc_##name = defval;\
module_param(lpfc_##name, uint, S_IRUGO);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_param_show(name)\
lpfc_param_init(name, defval, minval, maxval)\
static DEVICE_ATTR(lpfc_##name, S_IRUGO, lpfc_##name##_show, NULL)
#define LPFC_ATTR_RW(name, defval, minval, maxval, desc) \
static uint lpfc_##name = defval;\
module_param(lpfc_##name, uint, S_IRUGO);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_param_show(name)\
lpfc_param_init(name, defval, minval, maxval)\
lpfc_param_set(name, defval, minval, maxval)\
lpfc_param_store(name)\
static DEVICE_ATTR(lpfc_##name, S_IRUGO | S_IWUSR,\
		   lpfc_##name##_show, lpfc_##name##_store)
#define LPFC_BBCR_ATTR_RW(name, defval, minval, maxval, desc) \
static uint lpfc_##name = defval;\
module_param(lpfc_##name, uint, 0444);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_param_show(name)\
lpfc_param_init(name, defval, minval, maxval)\
lpfc_param_store(name)\
static DEVICE_ATTR(lpfc_##name, 0444 | 0644,\
		   lpfc_##name##_show, lpfc_##name##_store)
#define LPFC_ATTR_HEX_R(name, defval, minval, maxval, desc) \
static uint lpfc_##name = defval;\
module_param(lpfc_##name, uint, S_IRUGO);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_param_hex_show(name)\
lpfc_param_init(name, defval, minval, maxval)\
static DEVICE_ATTR(lpfc_##name, S_IRUGO, lpfc_##name##_show, NULL)
#define LPFC_ATTR_HEX_RW(name, defval, minval, maxval, desc) \
static uint lpfc_##name = defval;\
module_param(lpfc_##name, uint, S_IRUGO);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_param_hex_show(name)\
lpfc_param_init(name, defval, minval, maxval)\
lpfc_param_set(name, defval, minval, maxval)\
lpfc_param_store(name)\
static DEVICE_ATTR(lpfc_##name, S_IRUGO | S_IWUSR,\
		   lpfc_##name##_show, lpfc_##name##_store)
#define LPFC_VPORT_ATTR(name, defval, minval, maxval, desc) \
static uint lpfc_##name = defval;\
module_param(lpfc_##name, uint, S_IRUGO);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_vport_param_init(name, defval, minval, maxval)
#define LPFC_VPORT_ATTR_R(name, defval, minval, maxval, desc) \
static uint lpfc_##name = defval;\
module_param(lpfc_##name, uint, S_IRUGO);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_vport_param_show(name)\
lpfc_vport_param_init(name, defval, minval, maxval)\
static DEVICE_ATTR(lpfc_##name, S_IRUGO, lpfc_##name##_show, NULL)
#define LPFC_VPORT_ULL_ATTR_R(name, defval, minval, maxval, desc) \
static uint64_t lpfc_##name = defval;\
module_param(lpfc_##name, ullong, S_IRUGO);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_vport_param_show(name)\
lpfc_vport_param_init(name, defval, minval, maxval)\
static DEVICE_ATTR(lpfc_##name, S_IRUGO, lpfc_##name##_show, NULL)
#define LPFC_VPORT_ATTR_RW(name, defval, minval, maxval, desc) \
static uint lpfc_##name = defval;\
module_param(lpfc_##name, uint, S_IRUGO);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_vport_param_show(name)\
lpfc_vport_param_init(name, defval, minval, maxval)\
lpfc_vport_param_set(name, defval, minval, maxval)\
lpfc_vport_param_store(name)\
static DEVICE_ATTR(lpfc_##name, S_IRUGO | S_IWUSR,\
		   lpfc_##name##_show, lpfc_##name##_store)
#define LPFC_VPORT_ATTR_HEX_R(name, defval, minval, maxval, desc) \
static uint lpfc_##name = defval;\
module_param(lpfc_##name, uint, S_IRUGO);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_vport_param_hex_show(name)\
lpfc_vport_param_init(name, defval, minval, maxval)\
static DEVICE_ATTR(lpfc_##name, S_IRUGO, lpfc_##name##_show, NULL)
#define LPFC_VPORT_ATTR_HEX_RW(name, defval, minval, maxval, desc) \
static uint lpfc_##name = defval;\
module_param(lpfc_##name, uint, S_IRUGO);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_vport_param_hex_show(name)\
lpfc_vport_param_init(name, defval, minval, maxval)\
lpfc_vport_param_set(name, defval, minval, maxval)\
lpfc_vport_param_store(name)\
static DEVICE_ATTR(lpfc_##name, S_IRUGO | S_IWUSR,\
		   lpfc_##name##_show, lpfc_##name##_store)
