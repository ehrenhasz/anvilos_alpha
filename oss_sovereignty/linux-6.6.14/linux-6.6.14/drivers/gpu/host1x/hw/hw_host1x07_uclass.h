#ifndef HOST1X_HW_HOST1X07_UCLASS_H
#define HOST1X_HW_HOST1X07_UCLASS_H
static inline u32 host1x_uclass_incr_syncpt_r(void)
{
	return 0x0;
}
#define HOST1X_UCLASS_INCR_SYNCPT \
	host1x_uclass_incr_syncpt_r()
static inline u32 host1x_uclass_incr_syncpt_cond_f(u32 v)
{
	return (v & 0xff) << 10;
}
#define HOST1X_UCLASS_INCR_SYNCPT_COND_F(v) \
	host1x_uclass_incr_syncpt_cond_f(v)
static inline u32 host1x_uclass_incr_syncpt_indx_f(u32 v)
{
	return (v & 0x3ff) << 0;
}
#define HOST1X_UCLASS_INCR_SYNCPT_INDX_F(v) \
	host1x_uclass_incr_syncpt_indx_f(v)
static inline u32 host1x_uclass_wait_syncpt_r(void)
{
	return 0x8;
}
#define HOST1X_UCLASS_WAIT_SYNCPT \
	host1x_uclass_wait_syncpt_r()
static inline u32 host1x_uclass_wait_syncpt_indx_f(u32 v)
{
	return (v & 0xff) << 24;
}
#define HOST1X_UCLASS_WAIT_SYNCPT_INDX_F(v) \
	host1x_uclass_wait_syncpt_indx_f(v)
static inline u32 host1x_uclass_wait_syncpt_thresh_f(u32 v)
{
	return (v & 0xffffff) << 0;
}
#define HOST1X_UCLASS_WAIT_SYNCPT_THRESH_F(v) \
	host1x_uclass_wait_syncpt_thresh_f(v)
static inline u32 host1x_uclass_wait_syncpt_base_r(void)
{
	return 0x9;
}
#define HOST1X_UCLASS_WAIT_SYNCPT_BASE \
	host1x_uclass_wait_syncpt_base_r()
static inline u32 host1x_uclass_wait_syncpt_base_indx_f(u32 v)
{
	return (v & 0xff) << 24;
}
#define HOST1X_UCLASS_WAIT_SYNCPT_BASE_INDX_F(v) \
	host1x_uclass_wait_syncpt_base_indx_f(v)
static inline u32 host1x_uclass_wait_syncpt_base_base_indx_f(u32 v)
{
	return (v & 0xff) << 16;
}
#define HOST1X_UCLASS_WAIT_SYNCPT_BASE_BASE_INDX_F(v) \
	host1x_uclass_wait_syncpt_base_base_indx_f(v)
static inline u32 host1x_uclass_wait_syncpt_base_offset_f(u32 v)
{
	return (v & 0xffff) << 0;
}
#define HOST1X_UCLASS_WAIT_SYNCPT_BASE_OFFSET_F(v) \
	host1x_uclass_wait_syncpt_base_offset_f(v)
static inline u32 host1x_uclass_load_syncpt_base_r(void)
{
	return 0xb;
}
#define HOST1X_UCLASS_LOAD_SYNCPT_BASE \
	host1x_uclass_load_syncpt_base_r()
static inline u32 host1x_uclass_load_syncpt_base_base_indx_f(u32 v)
{
	return (v & 0xff) << 24;
}
#define HOST1X_UCLASS_LOAD_SYNCPT_BASE_BASE_INDX_F(v) \
	host1x_uclass_load_syncpt_base_base_indx_f(v)
static inline u32 host1x_uclass_load_syncpt_base_value_f(u32 v)
{
	return (v & 0xffffff) << 0;
}
#define HOST1X_UCLASS_LOAD_SYNCPT_BASE_VALUE_F(v) \
	host1x_uclass_load_syncpt_base_value_f(v)
static inline u32 host1x_uclass_incr_syncpt_base_base_indx_f(u32 v)
{
	return (v & 0xff) << 24;
}
#define HOST1X_UCLASS_INCR_SYNCPT_BASE_BASE_INDX_F(v) \
	host1x_uclass_incr_syncpt_base_base_indx_f(v)
static inline u32 host1x_uclass_incr_syncpt_base_offset_f(u32 v)
{
	return (v & 0xffffff) << 0;
}
#define HOST1X_UCLASS_INCR_SYNCPT_BASE_OFFSET_F(v) \
	host1x_uclass_incr_syncpt_base_offset_f(v)
static inline u32 host1x_uclass_indoff_r(void)
{
	return 0x2d;
}
#define HOST1X_UCLASS_INDOFF \
	host1x_uclass_indoff_r()
static inline u32 host1x_uclass_indoff_indbe_f(u32 v)
{
	return (v & 0xf) << 28;
}
#define HOST1X_UCLASS_INDOFF_INDBE_F(v) \
	host1x_uclass_indoff_indbe_f(v)
static inline u32 host1x_uclass_indoff_autoinc_f(u32 v)
{
	return (v & 0x1) << 27;
}
#define HOST1X_UCLASS_INDOFF_AUTOINC_F(v) \
	host1x_uclass_indoff_autoinc_f(v)
static inline u32 host1x_uclass_indoff_indmodid_f(u32 v)
{
	return (v & 0xff) << 18;
}
#define HOST1X_UCLASS_INDOFF_INDMODID_F(v) \
	host1x_uclass_indoff_indmodid_f(v)
static inline u32 host1x_uclass_indoff_indroffset_f(u32 v)
{
	return (v & 0xffff) << 2;
}
#define HOST1X_UCLASS_INDOFF_INDROFFSET_F(v) \
	host1x_uclass_indoff_indroffset_f(v)
static inline u32 host1x_uclass_indoff_rwn_read_v(void)
{
	return 1;
}
#define HOST1X_UCLASS_INDOFF_INDROFFSET_F(v) \
	host1x_uclass_indoff_indroffset_f(v)
static inline u32 host1x_uclass_load_syncpt_payload_32_r(void)
{
	return 0x4e;
}
#define HOST1X_UCLASS_LOAD_SYNCPT_PAYLOAD_32 \
	host1x_uclass_load_syncpt_payload_32_r()
static inline u32 host1x_uclass_wait_syncpt_32_r(void)
{
	return 0x50;
}
#define HOST1X_UCLASS_WAIT_SYNCPT_32 \
	host1x_uclass_wait_syncpt_32_r()
#endif
