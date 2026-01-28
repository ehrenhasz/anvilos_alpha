#ifndef HOST1X_HW_HOST1X05_CHANNEL_H
#define HOST1X_HW_HOST1X05_CHANNEL_H
static inline u32 host1x_channel_fifostat_r(void)
{
	return 0x0;
}
#define HOST1X_CHANNEL_FIFOSTAT \
	host1x_channel_fifostat_r()
static inline u32 host1x_channel_fifostat_cfempty_v(u32 r)
{
	return (r >> 11) & 0x1;
}
#define HOST1X_CHANNEL_FIFOSTAT_CFEMPTY_V(r) \
	host1x_channel_fifostat_cfempty_v(r)
static inline u32 host1x_channel_dmastart_r(void)
{
	return 0x14;
}
#define HOST1X_CHANNEL_DMASTART \
	host1x_channel_dmastart_r()
static inline u32 host1x_channel_dmaput_r(void)
{
	return 0x18;
}
#define HOST1X_CHANNEL_DMAPUT \
	host1x_channel_dmaput_r()
static inline u32 host1x_channel_dmaget_r(void)
{
	return 0x1c;
}
#define HOST1X_CHANNEL_DMAGET \
	host1x_channel_dmaget_r()
static inline u32 host1x_channel_dmaend_r(void)
{
	return 0x20;
}
#define HOST1X_CHANNEL_DMAEND \
	host1x_channel_dmaend_r()
static inline u32 host1x_channel_dmactrl_r(void)
{
	return 0x24;
}
#define HOST1X_CHANNEL_DMACTRL \
	host1x_channel_dmactrl_r()
static inline u32 host1x_channel_dmactrl_dmastop(void)
{
	return 1 << 0;
}
#define HOST1X_CHANNEL_DMACTRL_DMASTOP \
	host1x_channel_dmactrl_dmastop()
static inline u32 host1x_channel_dmactrl_dmastop_v(u32 r)
{
	return (r >> 0) & 0x1;
}
#define HOST1X_CHANNEL_DMACTRL_DMASTOP_V(r) \
	host1x_channel_dmactrl_dmastop_v(r)
static inline u32 host1x_channel_dmactrl_dmagetrst(void)
{
	return 1 << 1;
}
#define HOST1X_CHANNEL_DMACTRL_DMAGETRST \
	host1x_channel_dmactrl_dmagetrst()
static inline u32 host1x_channel_dmactrl_dmainitget(void)
{
	return 1 << 2;
}
#define HOST1X_CHANNEL_DMACTRL_DMAINITGET \
	host1x_channel_dmactrl_dmainitget()
static inline u32 host1x_channel_channelctrl_r(void)
{
	return 0x98;
}
#define HOST1X_CHANNEL_CHANNELCTRL \
	host1x_channel_channelctrl_r()
static inline u32 host1x_channel_channelctrl_kernel_filter_gbuffer_f(u32 v)
{
	return (v & 0x1) << 2;
}
#define HOST1X_CHANNEL_CHANNELCTRL_KERNEL_FILTER_GBUFFER(v) \
	host1x_channel_channelctrl_kernel_filter_gbuffer_f(v)
#endif
