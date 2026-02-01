
 

#include "i915_drv.h"
#include "intel_ddi.h"
#include "intel_ddi_buf_trans.h"
#include "intel_de.h"
#include "intel_display_types.h"
#include "intel_dp.h"
#include "intel_cx0_phy.h"

 
static const union intel_ddi_buf_trans_entry _hsw_trans_dp[] = {
	{ .hsw = { 0x00FFFFFF, 0x0006000E, 0x0 } },
	{ .hsw = { 0x00D75FFF, 0x0005000A, 0x0 } },
	{ .hsw = { 0x00C30FFF, 0x00040006, 0x0 } },
	{ .hsw = { 0x80AAAFFF, 0x000B0000, 0x0 } },
	{ .hsw = { 0x00FFFFFF, 0x0005000A, 0x0 } },
	{ .hsw = { 0x00D75FFF, 0x000C0004, 0x0 } },
	{ .hsw = { 0x80C30FFF, 0x000B0000, 0x0 } },
	{ .hsw = { 0x00FFFFFF, 0x00040006, 0x0 } },
	{ .hsw = { 0x80D75FFF, 0x000B0000, 0x0 } },
};

static const struct intel_ddi_buf_trans hsw_trans_dp = {
	.entries = _hsw_trans_dp,
	.num_entries = ARRAY_SIZE(_hsw_trans_dp),
};

static const union intel_ddi_buf_trans_entry _hsw_trans_fdi[] = {
	{ .hsw = { 0x00FFFFFF, 0x0007000E, 0x0 } },
	{ .hsw = { 0x00D75FFF, 0x000F000A, 0x0 } },
	{ .hsw = { 0x00C30FFF, 0x00060006, 0x0 } },
	{ .hsw = { 0x00AAAFFF, 0x001E0000, 0x0 } },
	{ .hsw = { 0x00FFFFFF, 0x000F000A, 0x0 } },
	{ .hsw = { 0x00D75FFF, 0x00160004, 0x0 } },
	{ .hsw = { 0x00C30FFF, 0x001E0000, 0x0 } },
	{ .hsw = { 0x00FFFFFF, 0x00060006, 0x0 } },
	{ .hsw = { 0x00D75FFF, 0x001E0000, 0x0 } },
};

static const struct intel_ddi_buf_trans hsw_trans_fdi = {
	.entries = _hsw_trans_fdi,
	.num_entries = ARRAY_SIZE(_hsw_trans_fdi),
};

static const union intel_ddi_buf_trans_entry _hsw_trans_hdmi[] = {
							 
	{ .hsw = { 0x00FFFFFF, 0x0006000E, 0x0 } },	 
	{ .hsw = { 0x00E79FFF, 0x000E000C, 0x0 } },	 
	{ .hsw = { 0x00D75FFF, 0x0005000A, 0x0 } },	 
	{ .hsw = { 0x00FFFFFF, 0x0005000A, 0x0 } },	 
	{ .hsw = { 0x00E79FFF, 0x001D0007, 0x0 } },	 
	{ .hsw = { 0x00D75FFF, 0x000C0004, 0x0 } },	 
	{ .hsw = { 0x00FFFFFF, 0x00040006, 0x0 } },	 
	{ .hsw = { 0x80E79FFF, 0x00030002, 0x0 } },	 
	{ .hsw = { 0x00FFFFFF, 0x00140005, 0x0 } },	 
	{ .hsw = { 0x00FFFFFF, 0x000C0004, 0x0 } },	 
	{ .hsw = { 0x00FFFFFF, 0x001C0003, 0x0 } },	 
	{ .hsw = { 0x80FFFFFF, 0x00030002, 0x0 } },	 
};

static const struct intel_ddi_buf_trans hsw_trans_hdmi = {
	.entries = _hsw_trans_hdmi,
	.num_entries = ARRAY_SIZE(_hsw_trans_hdmi),
	.hdmi_default_entry = 6,
};

static const union intel_ddi_buf_trans_entry _bdw_trans_edp[] = {
	{ .hsw = { 0x00FFFFFF, 0x00000012, 0x0 } },
	{ .hsw = { 0x00EBAFFF, 0x00020011, 0x0 } },
	{ .hsw = { 0x00C71FFF, 0x0006000F, 0x0 } },
	{ .hsw = { 0x00AAAFFF, 0x000E000A, 0x0 } },
	{ .hsw = { 0x00FFFFFF, 0x00020011, 0x0 } },
	{ .hsw = { 0x00DB6FFF, 0x0005000F, 0x0 } },
	{ .hsw = { 0x00BEEFFF, 0x000A000C, 0x0 } },
	{ .hsw = { 0x00FFFFFF, 0x0005000F, 0x0 } },
	{ .hsw = { 0x00DB6FFF, 0x000A000C, 0x0 } },
};

static const struct intel_ddi_buf_trans bdw_trans_edp = {
	.entries = _bdw_trans_edp,
	.num_entries = ARRAY_SIZE(_bdw_trans_edp),
};

static const union intel_ddi_buf_trans_entry _bdw_trans_dp[] = {
	{ .hsw = { 0x00FFFFFF, 0x0007000E, 0x0 } },
	{ .hsw = { 0x00D75FFF, 0x000E000A, 0x0 } },
	{ .hsw = { 0x00BEFFFF, 0x00140006, 0x0 } },
	{ .hsw = { 0x80B2CFFF, 0x001B0002, 0x0 } },
	{ .hsw = { 0x00FFFFFF, 0x000E000A, 0x0 } },
	{ .hsw = { 0x00DB6FFF, 0x00160005, 0x0 } },
	{ .hsw = { 0x80C71FFF, 0x001A0002, 0x0 } },
	{ .hsw = { 0x00F7DFFF, 0x00180004, 0x0 } },
	{ .hsw = { 0x80D75FFF, 0x001B0002, 0x0 } },
};

static const struct intel_ddi_buf_trans bdw_trans_dp = {
	.entries = _bdw_trans_dp,
	.num_entries = ARRAY_SIZE(_bdw_trans_dp),
};

static const union intel_ddi_buf_trans_entry _bdw_trans_fdi[] = {
	{ .hsw = { 0x00FFFFFF, 0x0001000E, 0x0 } },
	{ .hsw = { 0x00D75FFF, 0x0004000A, 0x0 } },
	{ .hsw = { 0x00C30FFF, 0x00070006, 0x0 } },
	{ .hsw = { 0x00AAAFFF, 0x000C0000, 0x0 } },
	{ .hsw = { 0x00FFFFFF, 0x0004000A, 0x0 } },
	{ .hsw = { 0x00D75FFF, 0x00090004, 0x0 } },
	{ .hsw = { 0x00C30FFF, 0x000C0000, 0x0 } },
	{ .hsw = { 0x00FFFFFF, 0x00070006, 0x0 } },
	{ .hsw = { 0x00D75FFF, 0x000C0000, 0x0 } },
};

static const struct intel_ddi_buf_trans bdw_trans_fdi = {
	.entries = _bdw_trans_fdi,
	.num_entries = ARRAY_SIZE(_bdw_trans_fdi),
};

static const union intel_ddi_buf_trans_entry _bdw_trans_hdmi[] = {
							 
	{ .hsw = { 0x00FFFFFF, 0x0007000E, 0x0 } },	 
	{ .hsw = { 0x00D75FFF, 0x000E000A, 0x0 } },	 
	{ .hsw = { 0x00BEFFFF, 0x00140006, 0x0 } },	 
	{ .hsw = { 0x00FFFFFF, 0x0009000D, 0x0 } },	 
	{ .hsw = { 0x00FFFFFF, 0x000E000A, 0x0 } },	 
	{ .hsw = { 0x00D7FFFF, 0x00140006, 0x0 } },	 
	{ .hsw = { 0x80CB2FFF, 0x001B0002, 0x0 } },	 
	{ .hsw = { 0x00FFFFFF, 0x00140006, 0x0 } },	 
	{ .hsw = { 0x80E79FFF, 0x001B0002, 0x0 } },	 
	{ .hsw = { 0x80FFFFFF, 0x001B0002, 0x0 } },	 
};

static const struct intel_ddi_buf_trans bdw_trans_hdmi = {
	.entries = _bdw_trans_hdmi,
	.num_entries = ARRAY_SIZE(_bdw_trans_hdmi),
	.hdmi_default_entry = 7,
};

 
static const union intel_ddi_buf_trans_entry _skl_trans_dp[] = {
	{ .hsw = { 0x00002016, 0x000000A0, 0x0 } },
	{ .hsw = { 0x00005012, 0x0000009B, 0x0 } },
	{ .hsw = { 0x00007011, 0x00000088, 0x0 } },
	{ .hsw = { 0x80009010, 0x000000C0, 0x1 } },
	{ .hsw = { 0x00002016, 0x0000009B, 0x0 } },
	{ .hsw = { 0x00005012, 0x00000088, 0x0 } },
	{ .hsw = { 0x80007011, 0x000000C0, 0x1 } },
	{ .hsw = { 0x00002016, 0x000000DF, 0x0 } },
	{ .hsw = { 0x80005012, 0x000000C0, 0x1 } },
};

static const struct intel_ddi_buf_trans skl_trans_dp = {
	.entries = _skl_trans_dp,
	.num_entries = ARRAY_SIZE(_skl_trans_dp),
};

 
static const union intel_ddi_buf_trans_entry _skl_u_trans_dp[] = {
	{ .hsw = { 0x0000201B, 0x000000A2, 0x0 } },
	{ .hsw = { 0x00005012, 0x00000088, 0x0 } },
	{ .hsw = { 0x80007011, 0x000000CD, 0x1 } },
	{ .hsw = { 0x80009010, 0x000000C0, 0x1 } },
	{ .hsw = { 0x0000201B, 0x0000009D, 0x0 } },
	{ .hsw = { 0x80005012, 0x000000C0, 0x1 } },
	{ .hsw = { 0x80007011, 0x000000C0, 0x1 } },
	{ .hsw = { 0x00002016, 0x00000088, 0x0 } },
	{ .hsw = { 0x80005012, 0x000000C0, 0x1 } },
};

static const struct intel_ddi_buf_trans skl_u_trans_dp = {
	.entries = _skl_u_trans_dp,
	.num_entries = ARRAY_SIZE(_skl_u_trans_dp),
};

 
static const union intel_ddi_buf_trans_entry _skl_y_trans_dp[] = {
	{ .hsw = { 0x00000018, 0x000000A2, 0x0 } },
	{ .hsw = { 0x00005012, 0x00000088, 0x0 } },
	{ .hsw = { 0x80007011, 0x000000CD, 0x3 } },
	{ .hsw = { 0x80009010, 0x000000C0, 0x3 } },
	{ .hsw = { 0x00000018, 0x0000009D, 0x0 } },
	{ .hsw = { 0x80005012, 0x000000C0, 0x3 } },
	{ .hsw = { 0x80007011, 0x000000C0, 0x3 } },
	{ .hsw = { 0x00000018, 0x00000088, 0x0 } },
	{ .hsw = { 0x80005012, 0x000000C0, 0x3 } },
};

static const struct intel_ddi_buf_trans skl_y_trans_dp = {
	.entries = _skl_y_trans_dp,
	.num_entries = ARRAY_SIZE(_skl_y_trans_dp),
};

 
static const union intel_ddi_buf_trans_entry _kbl_trans_dp[] = {
	{ .hsw = { 0x00002016, 0x000000A0, 0x0 } },
	{ .hsw = { 0x00005012, 0x0000009B, 0x0 } },
	{ .hsw = { 0x00007011, 0x00000088, 0x0 } },
	{ .hsw = { 0x80009010, 0x000000C0, 0x1 } },
	{ .hsw = { 0x00002016, 0x0000009B, 0x0 } },
	{ .hsw = { 0x00005012, 0x00000088, 0x0 } },
	{ .hsw = { 0x80007011, 0x000000C0, 0x1 } },
	{ .hsw = { 0x00002016, 0x00000097, 0x0 } },
	{ .hsw = { 0x80005012, 0x000000C0, 0x1 } },
};

static const struct intel_ddi_buf_trans kbl_trans_dp = {
	.entries = _kbl_trans_dp,
	.num_entries = ARRAY_SIZE(_kbl_trans_dp),
};

 
static const union intel_ddi_buf_trans_entry _kbl_u_trans_dp[] = {
	{ .hsw = { 0x0000201B, 0x000000A1, 0x0 } },
	{ .hsw = { 0x00005012, 0x00000088, 0x0 } },
	{ .hsw = { 0x80007011, 0x000000CD, 0x3 } },
	{ .hsw = { 0x80009010, 0x000000C0, 0x3 } },
	{ .hsw = { 0x0000201B, 0x0000009D, 0x0 } },
	{ .hsw = { 0x80005012, 0x000000C0, 0x3 } },
	{ .hsw = { 0x80007011, 0x000000C0, 0x3 } },
	{ .hsw = { 0x00002016, 0x0000004F, 0x0 } },
	{ .hsw = { 0x80005012, 0x000000C0, 0x3 } },
};

static const struct intel_ddi_buf_trans kbl_u_trans_dp = {
	.entries = _kbl_u_trans_dp,
	.num_entries = ARRAY_SIZE(_kbl_u_trans_dp),
};

 
static const union intel_ddi_buf_trans_entry _kbl_y_trans_dp[] = {
	{ .hsw = { 0x00001017, 0x000000A1, 0x0 } },
	{ .hsw = { 0x00005012, 0x00000088, 0x0 } },
	{ .hsw = { 0x80007011, 0x000000CD, 0x3 } },
	{ .hsw = { 0x8000800F, 0x000000C0, 0x3 } },
	{ .hsw = { 0x00001017, 0x0000009D, 0x0 } },
	{ .hsw = { 0x80005012, 0x000000C0, 0x3 } },
	{ .hsw = { 0x80007011, 0x000000C0, 0x3 } },
	{ .hsw = { 0x00001017, 0x0000004C, 0x0 } },
	{ .hsw = { 0x80005012, 0x000000C0, 0x3 } },
};

static const struct intel_ddi_buf_trans kbl_y_trans_dp = {
	.entries = _kbl_y_trans_dp,
	.num_entries = ARRAY_SIZE(_kbl_y_trans_dp),
};

 
static const union intel_ddi_buf_trans_entry _skl_trans_edp[] = {
	{ .hsw = { 0x00000018, 0x000000A8, 0x0 } },
	{ .hsw = { 0x00004013, 0x000000A9, 0x0 } },
	{ .hsw = { 0x00007011, 0x000000A2, 0x0 } },
	{ .hsw = { 0x00009010, 0x0000009C, 0x0 } },
	{ .hsw = { 0x00000018, 0x000000A9, 0x0 } },
	{ .hsw = { 0x00006013, 0x000000A2, 0x0 } },
	{ .hsw = { 0x00007011, 0x000000A6, 0x0 } },
	{ .hsw = { 0x00000018, 0x000000AB, 0x0 } },
	{ .hsw = { 0x00007013, 0x0000009F, 0x0 } },
	{ .hsw = { 0x00000018, 0x000000DF, 0x0 } },
};

static const struct intel_ddi_buf_trans skl_trans_edp = {
	.entries = _skl_trans_edp,
	.num_entries = ARRAY_SIZE(_skl_trans_edp),
};

 
static const union intel_ddi_buf_trans_entry _skl_u_trans_edp[] = {
	{ .hsw = { 0x00000018, 0x000000A8, 0x0 } },
	{ .hsw = { 0x00004013, 0x000000A9, 0x0 } },
	{ .hsw = { 0x00007011, 0x000000A2, 0x0 } },
	{ .hsw = { 0x00009010, 0x0000009C, 0x0 } },
	{ .hsw = { 0x00000018, 0x000000A9, 0x0 } },
	{ .hsw = { 0x00006013, 0x000000A2, 0x0 } },
	{ .hsw = { 0x00007011, 0x000000A6, 0x0 } },
	{ .hsw = { 0x00002016, 0x000000AB, 0x0 } },
	{ .hsw = { 0x00005013, 0x0000009F, 0x0 } },
	{ .hsw = { 0x00000018, 0x000000DF, 0x0 } },
};

static const struct intel_ddi_buf_trans skl_u_trans_edp = {
	.entries = _skl_u_trans_edp,
	.num_entries = ARRAY_SIZE(_skl_u_trans_edp),
};

 
static const union intel_ddi_buf_trans_entry _skl_y_trans_edp[] = {
	{ .hsw = { 0x00000018, 0x000000A8, 0x0 } },
	{ .hsw = { 0x00004013, 0x000000AB, 0x0 } },
	{ .hsw = { 0x00007011, 0x000000A4, 0x0 } },
	{ .hsw = { 0x00009010, 0x000000DF, 0x0 } },
	{ .hsw = { 0x00000018, 0x000000AA, 0x0 } },
	{ .hsw = { 0x00006013, 0x000000A4, 0x0 } },
	{ .hsw = { 0x00007011, 0x0000009D, 0x0 } },
	{ .hsw = { 0x00000018, 0x000000A0, 0x0 } },
	{ .hsw = { 0x00006012, 0x000000DF, 0x0 } },
	{ .hsw = { 0x00000018, 0x0000008A, 0x0 } },
};

static const struct intel_ddi_buf_trans skl_y_trans_edp = {
	.entries = _skl_y_trans_edp,
	.num_entries = ARRAY_SIZE(_skl_y_trans_edp),
};

 
static const union intel_ddi_buf_trans_entry _skl_trans_hdmi[] = {
	{ .hsw = { 0x00000018, 0x000000AC, 0x0 } },
	{ .hsw = { 0x00005012, 0x0000009D, 0x0 } },
	{ .hsw = { 0x00007011, 0x00000088, 0x0 } },
	{ .hsw = { 0x00000018, 0x000000A1, 0x0 } },
	{ .hsw = { 0x00000018, 0x00000098, 0x0 } },
	{ .hsw = { 0x00004013, 0x00000088, 0x0 } },
	{ .hsw = { 0x80006012, 0x000000CD, 0x1 } },
	{ .hsw = { 0x00000018, 0x000000DF, 0x0 } },
	{ .hsw = { 0x80003015, 0x000000CD, 0x1 } },	 
	{ .hsw = { 0x80003015, 0x000000C0, 0x1 } },
	{ .hsw = { 0x80000018, 0x000000C0, 0x1 } },
};

static const struct intel_ddi_buf_trans skl_trans_hdmi = {
	.entries = _skl_trans_hdmi,
	.num_entries = ARRAY_SIZE(_skl_trans_hdmi),
	.hdmi_default_entry = 8,
};

 
static const union intel_ddi_buf_trans_entry _skl_y_trans_hdmi[] = {
	{ .hsw = { 0x00000018, 0x000000A1, 0x0 } },
	{ .hsw = { 0x00005012, 0x000000DF, 0x0 } },
	{ .hsw = { 0x80007011, 0x000000CB, 0x3 } },
	{ .hsw = { 0x00000018, 0x000000A4, 0x0 } },
	{ .hsw = { 0x00000018, 0x0000009D, 0x0 } },
	{ .hsw = { 0x00004013, 0x00000080, 0x0 } },
	{ .hsw = { 0x80006013, 0x000000C0, 0x3 } },
	{ .hsw = { 0x00000018, 0x0000008A, 0x0 } },
	{ .hsw = { 0x80003015, 0x000000C0, 0x3 } },	 
	{ .hsw = { 0x80003015, 0x000000C0, 0x3 } },
	{ .hsw = { 0x80000018, 0x000000C0, 0x3 } },
};

static const struct intel_ddi_buf_trans skl_y_trans_hdmi = {
	.entries = _skl_y_trans_hdmi,
	.num_entries = ARRAY_SIZE(_skl_y_trans_hdmi),
	.hdmi_default_entry = 8,
};

static const union intel_ddi_buf_trans_entry _bxt_trans_dp[] = {
						 
	{ .bxt = { 52,  0x9A, 0, 128, } },	 
	{ .bxt = { 78,  0x9A, 0, 85,  } },	 
	{ .bxt = { 104, 0x9A, 0, 64,  } },	 
	{ .bxt = { 154, 0x9A, 0, 43,  } },	 
	{ .bxt = { 77,  0x9A, 0, 128, } },	 
	{ .bxt = { 116, 0x9A, 0, 85,  } },	 
	{ .bxt = { 154, 0x9A, 0, 64,  } },	 
	{ .bxt = { 102, 0x9A, 0, 128, } },	 
	{ .bxt = { 154, 0x9A, 0, 85,  } },	 
	{ .bxt = { 154, 0x9A, 1, 128, } },	 
};

static const struct intel_ddi_buf_trans bxt_trans_dp = {
	.entries = _bxt_trans_dp,
	.num_entries = ARRAY_SIZE(_bxt_trans_dp),
};

static const union intel_ddi_buf_trans_entry _bxt_trans_edp[] = {
					 
	{ .bxt = { 26, 0, 0, 128, } },	 
	{ .bxt = { 38, 0, 0, 112, } },	 
	{ .bxt = { 48, 0, 0, 96,  } },	 
	{ .bxt = { 54, 0, 0, 69,  } },	 
	{ .bxt = { 32, 0, 0, 128, } },	 
	{ .bxt = { 48, 0, 0, 104, } },	 
	{ .bxt = { 54, 0, 0, 85,  } },	 
	{ .bxt = { 43, 0, 0, 128, } },	 
	{ .bxt = { 54, 0, 0, 101, } },	 
	{ .bxt = { 48, 0, 0, 128, } },	 
};

static const struct intel_ddi_buf_trans bxt_trans_edp = {
	.entries = _bxt_trans_edp,
	.num_entries = ARRAY_SIZE(_bxt_trans_edp),
};

 
static const union intel_ddi_buf_trans_entry _bxt_trans_hdmi[] = {
						 
	{ .bxt = { 52,  0x9A, 0, 128, } },	 
	{ .bxt = { 52,  0x9A, 0, 85,  } },	 
	{ .bxt = { 52,  0x9A, 0, 64,  } },	 
	{ .bxt = { 42,  0x9A, 0, 43,  } },	 
	{ .bxt = { 77,  0x9A, 0, 128, } },	 
	{ .bxt = { 77,  0x9A, 0, 85,  } },	 
	{ .bxt = { 77,  0x9A, 0, 64,  } },	 
	{ .bxt = { 102, 0x9A, 0, 128, } },	 
	{ .bxt = { 102, 0x9A, 0, 85,  } },	 
	{ .bxt = { 154, 0x9A, 1, 128, } },	 
};

static const struct intel_ddi_buf_trans bxt_trans_hdmi = {
	.entries = _bxt_trans_hdmi,
	.num_entries = ARRAY_SIZE(_bxt_trans_hdmi),
	.hdmi_default_entry = ARRAY_SIZE(_bxt_trans_hdmi) - 1,
};

 
static const union intel_ddi_buf_trans_entry _icl_combo_phy_trans_dp_hbr2_edp_hbr3[] = {
							 
	{ .icl = { 0xA, 0x35, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xA, 0x4F, 0x37, 0x00, 0x08 } },	 
	{ .icl = { 0xC, 0x71, 0x2F, 0x00, 0x10 } },	 
	{ .icl = { 0x6, 0x7F, 0x2B, 0x00, 0x14 } },	 
	{ .icl = { 0xA, 0x4C, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xC, 0x73, 0x34, 0x00, 0x0B } },	 
	{ .icl = { 0x6, 0x7F, 0x2F, 0x00, 0x10 } },	 
	{ .icl = { 0xC, 0x6C, 0x3C, 0x00, 0x03 } },	 
	{ .icl = { 0x6, 0x7F, 0x35, 0x00, 0x0A } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
};

static const struct intel_ddi_buf_trans icl_combo_phy_trans_dp_hbr2_edp_hbr3 = {
	.entries = _icl_combo_phy_trans_dp_hbr2_edp_hbr3,
	.num_entries = ARRAY_SIZE(_icl_combo_phy_trans_dp_hbr2_edp_hbr3),
};

static const union intel_ddi_buf_trans_entry _icl_combo_phy_trans_edp_hbr2[] = {
							 
	{ .icl = { 0x0, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x8, 0x7F, 0x38, 0x00, 0x07 } },	 
	{ .icl = { 0x1, 0x7F, 0x33, 0x00, 0x0C } },	 
	{ .icl = { 0x9, 0x7F, 0x31, 0x00, 0x0E } },	 
	{ .icl = { 0x8, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x1, 0x7F, 0x38, 0x00, 0x07 } },	 
	{ .icl = { 0x9, 0x7F, 0x35, 0x00, 0x0A } },	 
	{ .icl = { 0x1, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x9, 0x7F, 0x38, 0x00, 0x07 } },	 
	{ .icl = { 0x9, 0x7F, 0x3F, 0x00, 0x00 } },	 
};

static const struct intel_ddi_buf_trans icl_combo_phy_trans_edp_hbr2 = {
	.entries = _icl_combo_phy_trans_edp_hbr2,
	.num_entries = ARRAY_SIZE(_icl_combo_phy_trans_edp_hbr2),
};

static const union intel_ddi_buf_trans_entry _icl_combo_phy_trans_hdmi[] = {
							 
	{ .icl = { 0xA, 0x60, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xB, 0x73, 0x36, 0x00, 0x09 } },	 
	{ .icl = { 0x6, 0x7F, 0x31, 0x00, 0x0E } },	 
	{ .icl = { 0xB, 0x73, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x6, 0x7F, 0x37, 0x00, 0x08 } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x6, 0x7F, 0x35, 0x00, 0x0A } },	 
};

static const struct intel_ddi_buf_trans icl_combo_phy_trans_hdmi = {
	.entries = _icl_combo_phy_trans_hdmi,
	.num_entries = ARRAY_SIZE(_icl_combo_phy_trans_hdmi),
	.hdmi_default_entry = ARRAY_SIZE(_icl_combo_phy_trans_hdmi) - 1,
};

static const union intel_ddi_buf_trans_entry _ehl_combo_phy_trans_dp[] = {
							 
	{ .icl = { 0xA, 0x33, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xA, 0x47, 0x38, 0x00, 0x07 } },	 
	{ .icl = { 0xC, 0x64, 0x33, 0x00, 0x0C } },	 
	{ .icl = { 0x6, 0x7F, 0x2F, 0x00, 0x10 } },	 
	{ .icl = { 0xA, 0x46, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xC, 0x64, 0x37, 0x00, 0x08 } },	 
	{ .icl = { 0x6, 0x7F, 0x32, 0x00, 0x0D } },	 
	{ .icl = { 0xC, 0x61, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x6, 0x7F, 0x37, 0x00, 0x08 } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
};

static const struct intel_ddi_buf_trans ehl_combo_phy_trans_dp = {
	.entries = _ehl_combo_phy_trans_dp,
	.num_entries = ARRAY_SIZE(_ehl_combo_phy_trans_dp),
};

static const union intel_ddi_buf_trans_entry _ehl_combo_phy_trans_edp_hbr2[] = {
							 
	{ .icl = { 0x8, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x8, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x1, 0x7F, 0x3D, 0x00, 0x02 } },	 
	{ .icl = { 0xA, 0x35, 0x39, 0x00, 0x06 } },	 
	{ .icl = { 0x8, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x1, 0x7F, 0x3C, 0x00, 0x03 } },	 
	{ .icl = { 0xA, 0x35, 0x39, 0x00, 0x06 } },	 
	{ .icl = { 0x1, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xA, 0x35, 0x38, 0x00, 0x07 } },	 
	{ .icl = { 0xA, 0x35, 0x3F, 0x00, 0x00 } },	 
};

static const struct intel_ddi_buf_trans ehl_combo_phy_trans_edp_hbr2 = {
	.entries = _ehl_combo_phy_trans_edp_hbr2,
	.num_entries = ARRAY_SIZE(_ehl_combo_phy_trans_edp_hbr2),
};

static const union intel_ddi_buf_trans_entry _jsl_combo_phy_trans_edp_hbr[] = {
							 
	{ .icl = { 0x8, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x8, 0x7F, 0x38, 0x00, 0x07 } },	 
	{ .icl = { 0x1, 0x7F, 0x33, 0x00, 0x0C } },	 
	{ .icl = { 0xA, 0x35, 0x36, 0x00, 0x09 } },	 
	{ .icl = { 0x8, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x1, 0x7F, 0x38, 0x00, 0x07 } },	 
	{ .icl = { 0xA, 0x35, 0x35, 0x00, 0x0A } },	 
	{ .icl = { 0x1, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xA, 0x35, 0x38, 0x00, 0x07 } },	 
	{ .icl = { 0xA, 0x35, 0x3F, 0x00, 0x00 } },	 
};

static const struct intel_ddi_buf_trans jsl_combo_phy_trans_edp_hbr = {
	.entries = _jsl_combo_phy_trans_edp_hbr,
	.num_entries = ARRAY_SIZE(_jsl_combo_phy_trans_edp_hbr),
};

static const union intel_ddi_buf_trans_entry _jsl_combo_phy_trans_edp_hbr2[] = {
							 
	{ .icl = { 0x8, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x8, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x1, 0x7F, 0x3D, 0x00, 0x02 } },	 
	{ .icl = { 0xA, 0x35, 0x38, 0x00, 0x07 } },	 
	{ .icl = { 0x8, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x1, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xA, 0x35, 0x3A, 0x00, 0x05 } },	 
	{ .icl = { 0x1, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xA, 0x35, 0x38, 0x00, 0x07 } },	 
	{ .icl = { 0xA, 0x35, 0x3F, 0x00, 0x00 } },	 
};

static const struct intel_ddi_buf_trans jsl_combo_phy_trans_edp_hbr2 = {
	.entries = _jsl_combo_phy_trans_edp_hbr2,
	.num_entries = ARRAY_SIZE(_jsl_combo_phy_trans_edp_hbr2),
};

static const union intel_ddi_buf_trans_entry _dg1_combo_phy_trans_dp_rbr_hbr[] = {
							 
	{ .icl = { 0xA, 0x32, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xA, 0x48, 0x35, 0x00, 0x0A } },	 
	{ .icl = { 0xC, 0x63, 0x2F, 0x00, 0x10 } },	 
	{ .icl = { 0x6, 0x7F, 0x2C, 0x00, 0x13 } },	 
	{ .icl = { 0xA, 0x43, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xC, 0x60, 0x36, 0x00, 0x09 } },	 
	{ .icl = { 0x6, 0x7F, 0x30, 0x00, 0x0F } },	 
	{ .icl = { 0xC, 0x60, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x6, 0x7F, 0x37, 0x00, 0x08 } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
};

static const struct intel_ddi_buf_trans dg1_combo_phy_trans_dp_rbr_hbr = {
	.entries = _dg1_combo_phy_trans_dp_rbr_hbr,
	.num_entries = ARRAY_SIZE(_dg1_combo_phy_trans_dp_rbr_hbr),
};

static const union intel_ddi_buf_trans_entry _dg1_combo_phy_trans_dp_hbr2_hbr3[] = {
							 
	{ .icl = { 0xA, 0x32, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xA, 0x48, 0x35, 0x00, 0x0A } },	 
	{ .icl = { 0xC, 0x63, 0x2F, 0x00, 0x10 } },	 
	{ .icl = { 0x6, 0x7F, 0x2C, 0x00, 0x13 } },	 
	{ .icl = { 0xA, 0x43, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xC, 0x60, 0x36, 0x00, 0x09 } },	 
	{ .icl = { 0x6, 0x7F, 0x30, 0x00, 0x0F } },	 
	{ .icl = { 0xC, 0x58, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x6, 0x7F, 0x35, 0x00, 0x0A } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
};

static const struct intel_ddi_buf_trans dg1_combo_phy_trans_dp_hbr2_hbr3 = {
	.entries = _dg1_combo_phy_trans_dp_hbr2_hbr3,
	.num_entries = ARRAY_SIZE(_dg1_combo_phy_trans_dp_hbr2_hbr3),
};

static const union intel_ddi_buf_trans_entry _icl_mg_phy_trans_rbr_hbr[] = {
					 
	{ .mg = { 0x18, 0x00, 0x00 } },	 
	{ .mg = { 0x1D, 0x00, 0x05 } },	 
	{ .mg = { 0x24, 0x00, 0x0C } },	 
	{ .mg = { 0x2B, 0x00, 0x14 } },	 
	{ .mg = { 0x21, 0x00, 0x00 } },	 
	{ .mg = { 0x2B, 0x00, 0x08 } },	 
	{ .mg = { 0x30, 0x00, 0x0F } },	 
	{ .mg = { 0x31, 0x00, 0x03 } },	 
	{ .mg = { 0x34, 0x00, 0x0B } },	 
	{ .mg = { 0x3F, 0x00, 0x00 } },	 
};

static const struct intel_ddi_buf_trans icl_mg_phy_trans_rbr_hbr = {
	.entries = _icl_mg_phy_trans_rbr_hbr,
	.num_entries = ARRAY_SIZE(_icl_mg_phy_trans_rbr_hbr),
};

static const union intel_ddi_buf_trans_entry _icl_mg_phy_trans_hbr2_hbr3[] = {
					 
	{ .mg = { 0x18, 0x00, 0x00 } },	 
	{ .mg = { 0x1D, 0x00, 0x05 } },	 
	{ .mg = { 0x24, 0x00, 0x0C } },	 
	{ .mg = { 0x2B, 0x00, 0x14 } },	 
	{ .mg = { 0x26, 0x00, 0x00 } },	 
	{ .mg = { 0x2C, 0x00, 0x07 } },	 
	{ .mg = { 0x33, 0x00, 0x0C } },	 
	{ .mg = { 0x2E, 0x00, 0x00 } },	 
	{ .mg = { 0x36, 0x00, 0x09 } },	 
	{ .mg = { 0x3F, 0x00, 0x00 } },	 
};

static const struct intel_ddi_buf_trans icl_mg_phy_trans_hbr2_hbr3 = {
	.entries = _icl_mg_phy_trans_hbr2_hbr3,
	.num_entries = ARRAY_SIZE(_icl_mg_phy_trans_hbr2_hbr3),
};

static const union intel_ddi_buf_trans_entry _icl_mg_phy_trans_hdmi[] = {
					 
	{ .mg = { 0x1A, 0x0, 0x0 } },	 
	{ .mg = { 0x20, 0x0, 0x0 } },	 
	{ .mg = { 0x29, 0x0, 0x0 } },	 
	{ .mg = { 0x32, 0x0, 0x0 } },	 
	{ .mg = { 0x3F, 0x0, 0x0 } },	 
	{ .mg = { 0x3A, 0x0, 0x5 } },	 
	{ .mg = { 0x39, 0x0, 0x6 } },	 
	{ .mg = { 0x38, 0x0, 0x7 } },	 
	{ .mg = { 0x37, 0x0, 0x8 } },	 
	{ .mg = { 0x36, 0x0, 0x9 } },	 
};

static const struct intel_ddi_buf_trans icl_mg_phy_trans_hdmi = {
	.entries = _icl_mg_phy_trans_hdmi,
	.num_entries = ARRAY_SIZE(_icl_mg_phy_trans_hdmi),
	.hdmi_default_entry = ARRAY_SIZE(_icl_mg_phy_trans_hdmi) - 1,
};

static const union intel_ddi_buf_trans_entry _tgl_dkl_phy_trans_dp_hbr[] = {
					 
	{ .dkl = { 0x7, 0x0, 0x00 } },	 
	{ .dkl = { 0x5, 0x0, 0x05 } },	 
	{ .dkl = { 0x2, 0x0, 0x0B } },	 
	{ .dkl = { 0x0, 0x0, 0x18 } },	 
	{ .dkl = { 0x5, 0x0, 0x00 } },	 
	{ .dkl = { 0x2, 0x0, 0x08 } },	 
	{ .dkl = { 0x0, 0x0, 0x14 } },	 
	{ .dkl = { 0x2, 0x0, 0x00 } },	 
	{ .dkl = { 0x0, 0x0, 0x0B } },	 
	{ .dkl = { 0x0, 0x0, 0x00 } },	 
};

static const struct intel_ddi_buf_trans tgl_dkl_phy_trans_dp_hbr = {
	.entries = _tgl_dkl_phy_trans_dp_hbr,
	.num_entries = ARRAY_SIZE(_tgl_dkl_phy_trans_dp_hbr),
};

static const union intel_ddi_buf_trans_entry _tgl_dkl_phy_trans_dp_hbr2[] = {
					 
	{ .dkl = { 0x7, 0x0, 0x00 } },	 
	{ .dkl = { 0x5, 0x0, 0x05 } },	 
	{ .dkl = { 0x2, 0x0, 0x0B } },	 
	{ .dkl = { 0x0, 0x0, 0x19 } },	 
	{ .dkl = { 0x5, 0x0, 0x00 } },	 
	{ .dkl = { 0x2, 0x0, 0x08 } },	 
	{ .dkl = { 0x0, 0x0, 0x14 } },	 
	{ .dkl = { 0x2, 0x0, 0x00 } },	 
	{ .dkl = { 0x0, 0x0, 0x0B } },	 
	{ .dkl = { 0x0, 0x0, 0x00 } },	 
};

static const struct intel_ddi_buf_trans tgl_dkl_phy_trans_dp_hbr2 = {
	.entries = _tgl_dkl_phy_trans_dp_hbr2,
	.num_entries = ARRAY_SIZE(_tgl_dkl_phy_trans_dp_hbr2),
};

static const union intel_ddi_buf_trans_entry _tgl_dkl_phy_trans_hdmi[] = {
					 
	{ .dkl = { 0x7, 0x0, 0x0 } },	 
	{ .dkl = { 0x6, 0x0, 0x0 } },	 
	{ .dkl = { 0x4, 0x0, 0x0 } },	 
	{ .dkl = { 0x2, 0x0, 0x0 } },	 
	{ .dkl = { 0x0, 0x0, 0x0 } },	 
	{ .dkl = { 0x0, 0x0, 0x5 } },	 
	{ .dkl = { 0x0, 0x0, 0x6 } },	 
	{ .dkl = { 0x0, 0x0, 0x7 } },	 
	{ .dkl = { 0x0, 0x0, 0x8 } },	 
	{ .dkl = { 0x0, 0x0, 0xA } },	 
};

static const struct intel_ddi_buf_trans tgl_dkl_phy_trans_hdmi = {
	.entries = _tgl_dkl_phy_trans_hdmi,
	.num_entries = ARRAY_SIZE(_tgl_dkl_phy_trans_hdmi),
	.hdmi_default_entry = ARRAY_SIZE(_tgl_dkl_phy_trans_hdmi) - 1,
};

static const union intel_ddi_buf_trans_entry _tgl_combo_phy_trans_dp_hbr[] = {
							 
	{ .icl = { 0xA, 0x32, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xA, 0x4F, 0x37, 0x00, 0x08 } },	 
	{ .icl = { 0xC, 0x71, 0x2F, 0x00, 0x10 } },	 
	{ .icl = { 0x6, 0x7D, 0x2B, 0x00, 0x14 } },	 
	{ .icl = { 0xA, 0x4C, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xC, 0x73, 0x34, 0x00, 0x0B } },	 
	{ .icl = { 0x6, 0x7F, 0x2F, 0x00, 0x10 } },	 
	{ .icl = { 0xC, 0x6C, 0x3C, 0x00, 0x03 } },	 
	{ .icl = { 0x6, 0x7F, 0x35, 0x00, 0x0A } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
};

static const struct intel_ddi_buf_trans tgl_combo_phy_trans_dp_hbr = {
	.entries = _tgl_combo_phy_trans_dp_hbr,
	.num_entries = ARRAY_SIZE(_tgl_combo_phy_trans_dp_hbr),
};

static const union intel_ddi_buf_trans_entry _tgl_combo_phy_trans_dp_hbr2[] = {
							 
	{ .icl = { 0xA, 0x35, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xA, 0x4F, 0x37, 0x00, 0x08 } },	 
	{ .icl = { 0xC, 0x63, 0x2F, 0x00, 0x10 } },	 
	{ .icl = { 0x6, 0x7F, 0x2B, 0x00, 0x14 } },	 
	{ .icl = { 0xA, 0x47, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xC, 0x63, 0x34, 0x00, 0x0B } },	 
	{ .icl = { 0x6, 0x7F, 0x2F, 0x00, 0x10 } },	 
	{ .icl = { 0xC, 0x61, 0x3C, 0x00, 0x03 } },	 
	{ .icl = { 0x6, 0x7B, 0x35, 0x00, 0x0A } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
};

static const struct intel_ddi_buf_trans tgl_combo_phy_trans_dp_hbr2 = {
	.entries = _tgl_combo_phy_trans_dp_hbr2,
	.num_entries = ARRAY_SIZE(_tgl_combo_phy_trans_dp_hbr2),
};

static const union intel_ddi_buf_trans_entry _tgl_uy_combo_phy_trans_dp_hbr2[] = {
							 
	{ .icl = { 0xA, 0x35, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xA, 0x4F, 0x36, 0x00, 0x09 } },	 
	{ .icl = { 0xC, 0x60, 0x32, 0x00, 0x0D } },	 
	{ .icl = { 0xC, 0x7F, 0x2D, 0x00, 0x12 } },	 
	{ .icl = { 0xC, 0x47, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xC, 0x6F, 0x36, 0x00, 0x09 } },	 
	{ .icl = { 0x6, 0x7D, 0x32, 0x00, 0x0D } },	 
	{ .icl = { 0x6, 0x60, 0x3C, 0x00, 0x03 } },	 
	{ .icl = { 0x6, 0x7F, 0x34, 0x00, 0x0B } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
};

static const struct intel_ddi_buf_trans tgl_uy_combo_phy_trans_dp_hbr2 = {
	.entries = _tgl_uy_combo_phy_trans_dp_hbr2,
	.num_entries = ARRAY_SIZE(_tgl_uy_combo_phy_trans_dp_hbr2),
};

 
static const union intel_ddi_buf_trans_entry _tgl_combo_phy_trans_edp_hbr2_hobl[] = {
							 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
};

static const struct intel_ddi_buf_trans tgl_combo_phy_trans_edp_hbr2_hobl = {
	.entries = _tgl_combo_phy_trans_edp_hbr2_hobl,
	.num_entries = ARRAY_SIZE(_tgl_combo_phy_trans_edp_hbr2_hobl),
};

static const union intel_ddi_buf_trans_entry _rkl_combo_phy_trans_dp_hbr[] = {
							 
	{ .icl = { 0xA, 0x2F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xA, 0x4F, 0x37, 0x00, 0x08 } },	 
	{ .icl = { 0xC, 0x63, 0x2F, 0x00, 0x10 } },	 
	{ .icl = { 0x6, 0x7D, 0x2A, 0x00, 0x15 } },	 
	{ .icl = { 0xA, 0x4C, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xC, 0x73, 0x34, 0x00, 0x0B } },	 
	{ .icl = { 0x6, 0x7F, 0x2F, 0x00, 0x10 } },	 
	{ .icl = { 0xC, 0x6E, 0x3E, 0x00, 0x01 } },	 
	{ .icl = { 0x6, 0x7F, 0x35, 0x00, 0x0A } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
};

static const struct intel_ddi_buf_trans rkl_combo_phy_trans_dp_hbr = {
	.entries = _rkl_combo_phy_trans_dp_hbr,
	.num_entries = ARRAY_SIZE(_rkl_combo_phy_trans_dp_hbr),
};

static const union intel_ddi_buf_trans_entry _rkl_combo_phy_trans_dp_hbr2_hbr3[] = {
							 
	{ .icl = { 0xA, 0x35, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xA, 0x50, 0x38, 0x00, 0x07 } },	 
	{ .icl = { 0xC, 0x61, 0x33, 0x00, 0x0C } },	 
	{ .icl = { 0x6, 0x7F, 0x2E, 0x00, 0x11 } },	 
	{ .icl = { 0xA, 0x47, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xC, 0x5F, 0x38, 0x00, 0x07 } },	 
	{ .icl = { 0x6, 0x7F, 0x2F, 0x00, 0x10 } },	 
	{ .icl = { 0xC, 0x5F, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x6, 0x7E, 0x36, 0x00, 0x09 } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
};

static const struct intel_ddi_buf_trans rkl_combo_phy_trans_dp_hbr2_hbr3 = {
	.entries = _rkl_combo_phy_trans_dp_hbr2_hbr3,
	.num_entries = ARRAY_SIZE(_rkl_combo_phy_trans_dp_hbr2_hbr3),
};

static const union intel_ddi_buf_trans_entry _adls_combo_phy_trans_dp_hbr2_hbr3[] = {
							 
	{ .icl = { 0xA, 0x35, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xA, 0x4F, 0x37, 0x00, 0x08 } },	 
	{ .icl = { 0xC, 0x63, 0x31, 0x00, 0x0E } },	 
	{ .icl = { 0x6, 0x7F, 0x2C, 0x00, 0x13 } },	 
	{ .icl = { 0xA, 0x47, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xC, 0x63, 0x37, 0x00, 0x08 } },	 
	{ .icl = { 0x6, 0x73, 0x32, 0x00, 0x0D } },	 
	{ .icl = { 0xC, 0x58, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x6, 0x7F, 0x35, 0x00, 0x0A } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
};

static const struct intel_ddi_buf_trans adls_combo_phy_trans_dp_hbr2_hbr3 = {
	.entries = _adls_combo_phy_trans_dp_hbr2_hbr3,
	.num_entries = ARRAY_SIZE(_adls_combo_phy_trans_dp_hbr2_hbr3),
};

static const union intel_ddi_buf_trans_entry _adls_combo_phy_trans_edp_hbr2[] = {
							 
	{ .icl = { 0x9, 0x73, 0x3D, 0x00, 0x02 } },	 
	{ .icl = { 0x9, 0x7A, 0x3C, 0x00, 0x03 } },	 
	{ .icl = { 0x9, 0x7F, 0x3B, 0x00, 0x04 } },	 
	{ .icl = { 0x4, 0x6C, 0x33, 0x00, 0x0C } },	 
	{ .icl = { 0x2, 0x73, 0x3A, 0x00, 0x05 } },	 
	{ .icl = { 0x2, 0x7C, 0x38, 0x00, 0x07 } },	 
	{ .icl = { 0x4, 0x5A, 0x36, 0x00, 0x09 } },	 
	{ .icl = { 0x4, 0x57, 0x3D, 0x00, 0x02 } },	 
	{ .icl = { 0x4, 0x65, 0x38, 0x00, 0x07 } },	 
	{ .icl = { 0x4, 0x6C, 0x3A, 0x00, 0x05 } },	 
};

static const struct intel_ddi_buf_trans adls_combo_phy_trans_edp_hbr2 = {
	.entries = _adls_combo_phy_trans_edp_hbr2,
	.num_entries = ARRAY_SIZE(_adls_combo_phy_trans_edp_hbr2),
};

static const union intel_ddi_buf_trans_entry _adls_combo_phy_trans_edp_hbr3[] = {
							 
	{ .icl = { 0xA, 0x35, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xA, 0x4F, 0x37, 0x00, 0x08 } },	 
	{ .icl = { 0xC, 0x63, 0x31, 0x00, 0x0E } },	 
	{ .icl = { 0x6, 0x7F, 0x2C, 0x00, 0x13 } },	 
	{ .icl = { 0xA, 0x47, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xC, 0x63, 0x37, 0x00, 0x08 } },	 
	{ .icl = { 0x6, 0x73, 0x32, 0x00, 0x0D } },	 
	{ .icl = { 0xC, 0x58, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x6, 0x7F, 0x35, 0x00, 0x0A } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
};

static const struct intel_ddi_buf_trans adls_combo_phy_trans_edp_hbr3 = {
	.entries = _adls_combo_phy_trans_edp_hbr3,
	.num_entries = ARRAY_SIZE(_adls_combo_phy_trans_edp_hbr3),
};

static const union intel_ddi_buf_trans_entry _adlp_combo_phy_trans_dp_hbr[] = {
							 
	{ .icl = { 0xA, 0x35, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xA, 0x4F, 0x37, 0x00, 0x08 } },	 
	{ .icl = { 0xC, 0x71, 0x31, 0x00, 0x0E } },	 
	{ .icl = { 0x6, 0x7F, 0x2C, 0x00, 0x13 } },	 
	{ .icl = { 0xA, 0x4C, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xC, 0x73, 0x34, 0x00, 0x0B } },	 
	{ .icl = { 0x6, 0x7F, 0x2F, 0x00, 0x10 } },	 
	{ .icl = { 0xC, 0x7C, 0x3C, 0x00, 0x03 } },	 
	{ .icl = { 0x6, 0x7F, 0x35, 0x00, 0x0A } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
};

static const struct intel_ddi_buf_trans adlp_combo_phy_trans_dp_hbr = {
	.entries = _adlp_combo_phy_trans_dp_hbr,
	.num_entries = ARRAY_SIZE(_adlp_combo_phy_trans_dp_hbr),
};

static const union intel_ddi_buf_trans_entry _adlp_combo_phy_trans_dp_hbr2_hbr3[] = {
							 
	{ .icl = { 0xA, 0x35, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xA, 0x4F, 0x37, 0x00, 0x08 } },	 
	{ .icl = { 0xC, 0x71, 0x30, 0x00, 0x0F } },	 
	{ .icl = { 0x6, 0x7F, 0x2B, 0x00, 0x14 } },	 
	{ .icl = { 0xA, 0x4C, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xC, 0x73, 0x34, 0x00, 0x0B } },	 
	{ .icl = { 0x6, 0x7F, 0x30, 0x00, 0x0F } },	 
	{ .icl = { 0xC, 0x63, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x6, 0x7F, 0x38, 0x00, 0x07 } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
};

static const union intel_ddi_buf_trans_entry _adlp_combo_phy_trans_edp_hbr2[] = {
							 
	{ .icl = { 0x4, 0x50, 0x38, 0x00, 0x07 } },	 
	{ .icl = { 0x4, 0x58, 0x35, 0x00, 0x0A } },	 
	{ .icl = { 0x4, 0x60, 0x34, 0x00, 0x0B } },	 
	{ .icl = { 0x4, 0x6A, 0x32, 0x00, 0x0D } },	 
	{ .icl = { 0x4, 0x5E, 0x38, 0x00, 0x07 } },	 
	{ .icl = { 0x4, 0x61, 0x36, 0x00, 0x09 } },	 
	{ .icl = { 0x4, 0x6B, 0x34, 0x00, 0x0B } },	 
	{ .icl = { 0x4, 0x69, 0x39, 0x00, 0x06 } },	 
	{ .icl = { 0x4, 0x73, 0x37, 0x00, 0x08 } },	 
	{ .icl = { 0x4, 0x7A, 0x38, 0x00, 0x07 } },	 
};

static const union intel_ddi_buf_trans_entry _adlp_combo_phy_trans_dp_hbr2_edp_hbr3[] = {
							 
	{ .icl = { 0xA, 0x35, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xA, 0x4F, 0x37, 0x00, 0x08 } },	 
	{ .icl = { 0xC, 0x71, 0x30, 0x00, 0x0f } },	 
	{ .icl = { 0x6, 0x7F, 0x2B, 0x00, 0x14 } },	 
	{ .icl = { 0xA, 0x4C, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0xC, 0x73, 0x34, 0x00, 0x0B } },	 
	{ .icl = { 0x6, 0x7F, 0x30, 0x00, 0x0F } },	 
	{ .icl = { 0xC, 0x63, 0x3F, 0x00, 0x00 } },	 
	{ .icl = { 0x6, 0x7F, 0x38, 0x00, 0x07 } },	 
	{ .icl = { 0x6, 0x7F, 0x3F, 0x00, 0x00 } },	 
};

static const struct intel_ddi_buf_trans adlp_combo_phy_trans_dp_hbr2_hbr3 = {
	.entries = _adlp_combo_phy_trans_dp_hbr2_hbr3,
	.num_entries = ARRAY_SIZE(_adlp_combo_phy_trans_dp_hbr2_hbr3),
};

static const struct intel_ddi_buf_trans adlp_combo_phy_trans_edp_hbr3 = {
	.entries = _adlp_combo_phy_trans_dp_hbr2_edp_hbr3,
	.num_entries = ARRAY_SIZE(_adlp_combo_phy_trans_dp_hbr2_edp_hbr3),
};

static const struct intel_ddi_buf_trans adlp_combo_phy_trans_edp_up_to_hbr2 = {
	.entries = _adlp_combo_phy_trans_edp_hbr2,
	.num_entries = ARRAY_SIZE(_adlp_combo_phy_trans_edp_hbr2),
};

static const union intel_ddi_buf_trans_entry _adlp_dkl_phy_trans_dp_hbr[] = {
					 
	{ .dkl = { 0x7, 0x0, 0x01 } },	 
	{ .dkl = { 0x5, 0x0, 0x06 } },	 
	{ .dkl = { 0x2, 0x0, 0x0B } },	 
	{ .dkl = { 0x0, 0x0, 0x17 } },	 
	{ .dkl = { 0x5, 0x0, 0x00 } },	 
	{ .dkl = { 0x2, 0x0, 0x08 } },	 
	{ .dkl = { 0x0, 0x0, 0x14 } },	 
	{ .dkl = { 0x2, 0x0, 0x00 } },	 
	{ .dkl = { 0x0, 0x0, 0x0B } },	 
	{ .dkl = { 0x0, 0x0, 0x00 } },	 
};

static const struct intel_ddi_buf_trans adlp_dkl_phy_trans_dp_hbr = {
	.entries = _adlp_dkl_phy_trans_dp_hbr,
	.num_entries = ARRAY_SIZE(_adlp_dkl_phy_trans_dp_hbr),
};

static const union intel_ddi_buf_trans_entry _adlp_dkl_phy_trans_dp_hbr2_hbr3[] = {
					 
	{ .dkl = { 0x7, 0x0, 0x00 } },	 
	{ .dkl = { 0x5, 0x0, 0x04 } },	 
	{ .dkl = { 0x2, 0x0, 0x0A } },	 
	{ .dkl = { 0x0, 0x0, 0x18 } },	 
	{ .dkl = { 0x5, 0x0, 0x00 } },	 
	{ .dkl = { 0x2, 0x0, 0x06 } },	 
	{ .dkl = { 0x0, 0x0, 0x14 } },	 
	{ .dkl = { 0x2, 0x0, 0x00 } },	 
	{ .dkl = { 0x0, 0x0, 0x09 } },	 
	{ .dkl = { 0x0, 0x0, 0x00 } },	 
};

static const struct intel_ddi_buf_trans adlp_dkl_phy_trans_dp_hbr2_hbr3 = {
	.entries = _adlp_dkl_phy_trans_dp_hbr2_hbr3,
	.num_entries = ARRAY_SIZE(_adlp_dkl_phy_trans_dp_hbr2_hbr3),
};

static const union intel_ddi_buf_trans_entry _dg2_snps_trans[] = {
	{ .snps = { 25, 0, 0 } },	 
	{ .snps = { 32, 0, 6 } },	 
	{ .snps = { 35, 0, 10 } },	 
	{ .snps = { 43, 0, 17 } },	 
	{ .snps = { 35, 0, 0 } },	 
	{ .snps = { 45, 0, 8 } },	 
	{ .snps = { 48, 0, 14 } },	 
	{ .snps = { 47, 0, 0 } },	 
	{ .snps = { 55, 0, 7 } },	 
	{ .snps = { 62, 0, 0 } },	 
};

static const struct intel_ddi_buf_trans dg2_snps_trans = {
	.entries = _dg2_snps_trans,
	.num_entries = ARRAY_SIZE(_dg2_snps_trans),
	.hdmi_default_entry = ARRAY_SIZE(_dg2_snps_trans) - 1,
};

static const union intel_ddi_buf_trans_entry _dg2_snps_trans_uhbr[] = {
	{ .snps = { 62, 0, 0 } },	 
	{ .snps = { 55, 0, 7 } },	 
	{ .snps = { 50, 0, 12 } },	 
	{ .snps = { 44, 0, 18 } },	 
	{ .snps = { 35, 0, 21 } },	 
	{ .snps = { 59, 3, 0 } },	 
	{ .snps = { 53, 3, 6 } },	 
	{ .snps = { 48, 3, 11 } },	 
	{ .snps = { 42, 5, 15 } },	 
	{ .snps = { 37, 5, 20 } },	 
	{ .snps = { 56, 6, 0 } },	 
	{ .snps = { 48, 7, 7 } },	 
	{ .snps = { 45, 7, 10 } },	 
	{ .snps = { 39, 8, 15 } },	 
	{ .snps = { 48, 14, 0 } },	 
	{ .snps = { 45, 4, 4 } },	 
};

static const struct intel_ddi_buf_trans dg2_snps_trans_uhbr = {
	.entries = _dg2_snps_trans_uhbr,
	.num_entries = ARRAY_SIZE(_dg2_snps_trans_uhbr),
};

static const union intel_ddi_buf_trans_entry _mtl_c10_trans_dp14[] = {
	{ .snps = { 26, 0, 0  } },       
	{ .snps = { 33, 0, 6  } },       
	{ .snps = { 38, 0, 11 } },       
	{ .snps = { 43, 0, 19 } },       
	{ .snps = { 39, 0, 0  } },       
	{ .snps = { 45, 0, 7  } },       
	{ .snps = { 46, 0, 13 } },       
	{ .snps = { 46, 0, 0  } },       
	{ .snps = { 55, 0, 7  } },       
	{ .snps = { 62, 0, 0  } },       
};

static const struct intel_ddi_buf_trans mtl_c10_trans_dp14 = {
	.entries = _mtl_c10_trans_dp14,
	.num_entries = ARRAY_SIZE(_mtl_c10_trans_dp14),
	.hdmi_default_entry = ARRAY_SIZE(_mtl_c10_trans_dp14) - 1,
};

 
static const union intel_ddi_buf_trans_entry _mtl_c20_trans_dp14[] = {
	{ .snps = { 20, 0, 0  } },       
	{ .snps = { 24, 0, 4  } },       
	{ .snps = { 30, 0, 9  } },       
	{ .snps = { 34, 0, 14 } },       
	{ .snps = { 29, 0, 0  } },       
	{ .snps = { 34, 0, 5  } },       
	{ .snps = { 38, 0, 10 } },       
	{ .snps = { 36, 0, 0  } },       
	{ .snps = { 40, 0, 6  } },       
	{ .snps = { 48, 0, 0  } },       
};

 
static const union intel_ddi_buf_trans_entry _mtl_c20_trans_uhbr[] = {
	{ .snps = { 48, 0, 0 } },        
	{ .snps = { 43, 0, 5 } },        
	{ .snps = { 40, 0, 8 } },        
	{ .snps = { 37, 0, 11 } },       
	{ .snps = { 33, 0, 15 } },       
	{ .snps = { 46, 2, 0 } },        
	{ .snps = { 42, 2, 4 } },        
	{ .snps = { 38, 2, 8 } },        
	{ .snps = { 35, 2, 11 } },       
	{ .snps = { 33, 2, 13 } },       
	{ .snps = { 44, 4, 0 } },        
	{ .snps = { 40, 4, 4 } },        
	{ .snps = { 37, 4, 7 } },        
	{ .snps = { 33, 4, 11 } },       
	{ .snps = { 40, 8, 0 } },	 
	{ .snps = { 30, 2, 2 } },	 
};

 
static const union intel_ddi_buf_trans_entry _mtl_c20_trans_hdmi[] = {
	{ .snps = { 48, 0, 0 } },        
	{ .snps = { 38, 4, 6 } },        
	{ .snps = { 36, 4, 8 } },        
	{ .snps = { 34, 4, 10 } },       
	{ .snps = { 32, 4, 12 } },       
};

static const struct intel_ddi_buf_trans mtl_c20_trans_hdmi = {
	.entries = _mtl_c20_trans_hdmi,
	.num_entries = ARRAY_SIZE(_mtl_c20_trans_hdmi),
	.hdmi_default_entry = 0,
};

static const struct intel_ddi_buf_trans mtl_c20_trans_dp14 = {
	.entries = _mtl_c20_trans_dp14,
	.num_entries = ARRAY_SIZE(_mtl_c20_trans_dp14),
	.hdmi_default_entry = ARRAY_SIZE(_mtl_c20_trans_dp14) - 1,
};

static const struct intel_ddi_buf_trans mtl_c20_trans_uhbr = {
	.entries = _mtl_c20_trans_uhbr,
	.num_entries = ARRAY_SIZE(_mtl_c20_trans_uhbr),
};

bool is_hobl_buf_trans(const struct intel_ddi_buf_trans *table)
{
	return table == &tgl_combo_phy_trans_edp_hbr2_hobl;
}

static bool use_edp_hobl(struct intel_encoder *encoder)
{
	struct intel_dp *intel_dp = enc_to_intel_dp(encoder);
	struct intel_connector *connector = intel_dp->attached_connector;

	return connector->panel.vbt.edp.hobl && !intel_dp->hobl_failed;
}

static bool use_edp_low_vswing(struct intel_encoder *encoder)
{
	struct intel_dp *intel_dp = enc_to_intel_dp(encoder);
	struct intel_connector *connector = intel_dp->attached_connector;

	return connector->panel.vbt.edp.low_vswing;
}

static const struct intel_ddi_buf_trans *
intel_get_buf_trans(const struct intel_ddi_buf_trans *trans, int *num_entries)
{
	*num_entries = trans->num_entries;
	return trans;
}

static const struct intel_ddi_buf_trans *
hsw_get_buf_trans(struct intel_encoder *encoder,
		  const struct intel_crtc_state *crtc_state,
		  int *n_entries)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_ANALOG))
		return intel_get_buf_trans(&hsw_trans_fdi, n_entries);
	else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		return intel_get_buf_trans(&hsw_trans_hdmi, n_entries);
	else
		return intel_get_buf_trans(&hsw_trans_dp, n_entries);
}

static const struct intel_ddi_buf_trans *
bdw_get_buf_trans(struct intel_encoder *encoder,
		  const struct intel_crtc_state *crtc_state,
		  int *n_entries)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_ANALOG))
		return intel_get_buf_trans(&bdw_trans_fdi, n_entries);
	else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		return intel_get_buf_trans(&bdw_trans_hdmi, n_entries);
	else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_EDP) &&
		 use_edp_low_vswing(encoder))
		return intel_get_buf_trans(&bdw_trans_edp, n_entries);
	else
		return intel_get_buf_trans(&bdw_trans_dp, n_entries);
}

static int skl_buf_trans_num_entries(enum port port, int n_entries)
{
	 
	if (port == PORT_A || port == PORT_E)
		return min(n_entries, 10);
	else
		return min(n_entries, 9);
}

static const struct intel_ddi_buf_trans *
_skl_get_buf_trans_dp(struct intel_encoder *encoder,
		      const struct intel_ddi_buf_trans *trans,
		      int *n_entries)
{
	trans = intel_get_buf_trans(trans, n_entries);
	*n_entries = skl_buf_trans_num_entries(encoder->port, *n_entries);
	return trans;
}

static const struct intel_ddi_buf_trans *
skl_y_get_buf_trans(struct intel_encoder *encoder,
		    const struct intel_crtc_state *crtc_state,
		    int *n_entries)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		return intel_get_buf_trans(&skl_y_trans_hdmi, n_entries);
	else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_EDP) &&
		 use_edp_low_vswing(encoder))
		return _skl_get_buf_trans_dp(encoder, &skl_y_trans_edp, n_entries);
	else
		return _skl_get_buf_trans_dp(encoder, &skl_y_trans_dp, n_entries);
}

static const struct intel_ddi_buf_trans *
skl_u_get_buf_trans(struct intel_encoder *encoder,
		    const struct intel_crtc_state *crtc_state,
		    int *n_entries)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		return intel_get_buf_trans(&skl_trans_hdmi, n_entries);
	else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_EDP) &&
		 use_edp_low_vswing(encoder))
		return _skl_get_buf_trans_dp(encoder, &skl_u_trans_edp, n_entries);
	else
		return _skl_get_buf_trans_dp(encoder, &skl_u_trans_dp, n_entries);
}

static const struct intel_ddi_buf_trans *
skl_get_buf_trans(struct intel_encoder *encoder,
		  const struct intel_crtc_state *crtc_state,
		  int *n_entries)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		return intel_get_buf_trans(&skl_trans_hdmi, n_entries);
	else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_EDP) &&
		 use_edp_low_vswing(encoder))
		return _skl_get_buf_trans_dp(encoder, &skl_trans_edp, n_entries);
	else
		return _skl_get_buf_trans_dp(encoder, &skl_trans_dp, n_entries);
}

static const struct intel_ddi_buf_trans *
kbl_y_get_buf_trans(struct intel_encoder *encoder,
		    const struct intel_crtc_state *crtc_state,
		    int *n_entries)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		return intel_get_buf_trans(&skl_y_trans_hdmi, n_entries);
	else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_EDP) &&
		 use_edp_low_vswing(encoder))
		return _skl_get_buf_trans_dp(encoder, &skl_y_trans_edp, n_entries);
	else
		return _skl_get_buf_trans_dp(encoder, &kbl_y_trans_dp, n_entries);
}

static const struct intel_ddi_buf_trans *
kbl_u_get_buf_trans(struct intel_encoder *encoder,
		    const struct intel_crtc_state *crtc_state,
		    int *n_entries)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		return intel_get_buf_trans(&skl_trans_hdmi, n_entries);
	else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_EDP) &&
		 use_edp_low_vswing(encoder))
		return _skl_get_buf_trans_dp(encoder, &skl_u_trans_edp, n_entries);
	else
		return _skl_get_buf_trans_dp(encoder, &kbl_u_trans_dp, n_entries);
}

static const struct intel_ddi_buf_trans *
kbl_get_buf_trans(struct intel_encoder *encoder,
		  const struct intel_crtc_state *crtc_state,
		  int *n_entries)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		return intel_get_buf_trans(&skl_trans_hdmi, n_entries);
	else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_EDP) &&
		 use_edp_low_vswing(encoder))
		return _skl_get_buf_trans_dp(encoder, &skl_trans_edp, n_entries);
	else
		return _skl_get_buf_trans_dp(encoder, &kbl_trans_dp, n_entries);
}

static const struct intel_ddi_buf_trans *
bxt_get_buf_trans(struct intel_encoder *encoder,
		  const struct intel_crtc_state *crtc_state,
		  int *n_entries)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		return intel_get_buf_trans(&bxt_trans_hdmi, n_entries);
	else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_EDP) &&
		 use_edp_low_vswing(encoder))
		return intel_get_buf_trans(&bxt_trans_edp, n_entries);
	else
		return intel_get_buf_trans(&bxt_trans_dp, n_entries);
}

static const struct intel_ddi_buf_trans *
icl_get_combo_buf_trans_dp(struct intel_encoder *encoder,
			   const struct intel_crtc_state *crtc_state,
			   int *n_entries)
{
	return intel_get_buf_trans(&icl_combo_phy_trans_dp_hbr2_edp_hbr3,
				   n_entries);
}

static const struct intel_ddi_buf_trans *
icl_get_combo_buf_trans_edp(struct intel_encoder *encoder,
			    const struct intel_crtc_state *crtc_state,
			    int *n_entries)
{
	if (crtc_state->port_clock > 540000) {
		return intel_get_buf_trans(&icl_combo_phy_trans_dp_hbr2_edp_hbr3,
					   n_entries);
	} else if (use_edp_low_vswing(encoder)) {
		return intel_get_buf_trans(&icl_combo_phy_trans_edp_hbr2,
					   n_entries);
	}

	return icl_get_combo_buf_trans_dp(encoder, crtc_state, n_entries);
}

static const struct intel_ddi_buf_trans *
icl_get_combo_buf_trans(struct intel_encoder *encoder,
			const struct intel_crtc_state *crtc_state,
			int *n_entries)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		return intel_get_buf_trans(&icl_combo_phy_trans_hdmi, n_entries);
	else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_EDP))
		return icl_get_combo_buf_trans_edp(encoder, crtc_state, n_entries);
	else
		return icl_get_combo_buf_trans_dp(encoder, crtc_state, n_entries);
}

static const struct intel_ddi_buf_trans *
icl_get_mg_buf_trans_dp(struct intel_encoder *encoder,
			const struct intel_crtc_state *crtc_state,
			int *n_entries)
{
	if (crtc_state->port_clock > 270000) {
		return intel_get_buf_trans(&icl_mg_phy_trans_hbr2_hbr3,
					   n_entries);
	} else {
		return intel_get_buf_trans(&icl_mg_phy_trans_rbr_hbr,
					   n_entries);
	}
}

static const struct intel_ddi_buf_trans *
icl_get_mg_buf_trans(struct intel_encoder *encoder,
		     const struct intel_crtc_state *crtc_state,
		     int *n_entries)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		return intel_get_buf_trans(&icl_mg_phy_trans_hdmi, n_entries);
	else
		return icl_get_mg_buf_trans_dp(encoder, crtc_state, n_entries);
}

static const struct intel_ddi_buf_trans *
ehl_get_combo_buf_trans_edp(struct intel_encoder *encoder,
			    const struct intel_crtc_state *crtc_state,
			    int *n_entries)
{
	if (crtc_state->port_clock > 270000)
		return intel_get_buf_trans(&ehl_combo_phy_trans_edp_hbr2, n_entries);
	else
		return intel_get_buf_trans(&icl_combo_phy_trans_edp_hbr2, n_entries);
}

static const struct intel_ddi_buf_trans *
ehl_get_combo_buf_trans(struct intel_encoder *encoder,
			const struct intel_crtc_state *crtc_state,
			int *n_entries)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		return intel_get_buf_trans(&icl_combo_phy_trans_hdmi, n_entries);
	else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_EDP) &&
		 use_edp_low_vswing(encoder))
		return ehl_get_combo_buf_trans_edp(encoder, crtc_state, n_entries);
	else
		return intel_get_buf_trans(&ehl_combo_phy_trans_dp, n_entries);
}

static const struct intel_ddi_buf_trans *
jsl_get_combo_buf_trans_edp(struct intel_encoder *encoder,
			    const struct intel_crtc_state *crtc_state,
			    int *n_entries)
{
	if (crtc_state->port_clock > 270000)
		return intel_get_buf_trans(&jsl_combo_phy_trans_edp_hbr2, n_entries);
	else
		return intel_get_buf_trans(&jsl_combo_phy_trans_edp_hbr, n_entries);
}

static const struct intel_ddi_buf_trans *
jsl_get_combo_buf_trans(struct intel_encoder *encoder,
			const struct intel_crtc_state *crtc_state,
			int *n_entries)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		return intel_get_buf_trans(&icl_combo_phy_trans_hdmi, n_entries);
	else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_EDP) &&
		 use_edp_low_vswing(encoder))
		return jsl_get_combo_buf_trans_edp(encoder, crtc_state, n_entries);
	else
		return intel_get_buf_trans(&icl_combo_phy_trans_dp_hbr2_edp_hbr3, n_entries);
}

static const struct intel_ddi_buf_trans *
tgl_get_combo_buf_trans_dp(struct intel_encoder *encoder,
			   const struct intel_crtc_state *crtc_state,
			   int *n_entries)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);

	if (crtc_state->port_clock > 270000) {
		if (IS_TIGERLAKE_UY(dev_priv)) {
			return intel_get_buf_trans(&tgl_uy_combo_phy_trans_dp_hbr2,
						   n_entries);
		} else {
			return intel_get_buf_trans(&tgl_combo_phy_trans_dp_hbr2,
						   n_entries);
		}
	} else {
		return intel_get_buf_trans(&tgl_combo_phy_trans_dp_hbr,
					   n_entries);
	}
}

static const struct intel_ddi_buf_trans *
tgl_get_combo_buf_trans_edp(struct intel_encoder *encoder,
			    const struct intel_crtc_state *crtc_state,
			    int *n_entries)
{
	if (crtc_state->port_clock > 540000) {
		return intel_get_buf_trans(&icl_combo_phy_trans_dp_hbr2_edp_hbr3,
					   n_entries);
	} else if (use_edp_hobl(encoder)) {
		return intel_get_buf_trans(&tgl_combo_phy_trans_edp_hbr2_hobl,
					   n_entries);
	} else if (use_edp_low_vswing(encoder)) {
		return intel_get_buf_trans(&icl_combo_phy_trans_edp_hbr2,
					   n_entries);
	}

	return tgl_get_combo_buf_trans_dp(encoder, crtc_state, n_entries);
}

static const struct intel_ddi_buf_trans *
tgl_get_combo_buf_trans(struct intel_encoder *encoder,
			const struct intel_crtc_state *crtc_state,
			int *n_entries)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		return intel_get_buf_trans(&icl_combo_phy_trans_hdmi, n_entries);
	else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_EDP))
		return tgl_get_combo_buf_trans_edp(encoder, crtc_state, n_entries);
	else
		return tgl_get_combo_buf_trans_dp(encoder, crtc_state, n_entries);
}

static const struct intel_ddi_buf_trans *
dg1_get_combo_buf_trans_dp(struct intel_encoder *encoder,
			   const struct intel_crtc_state *crtc_state,
			   int *n_entries)
{
	if (crtc_state->port_clock > 270000)
		return intel_get_buf_trans(&dg1_combo_phy_trans_dp_hbr2_hbr3,
					   n_entries);
	else
		return intel_get_buf_trans(&dg1_combo_phy_trans_dp_rbr_hbr,
					   n_entries);
}

static const struct intel_ddi_buf_trans *
dg1_get_combo_buf_trans_edp(struct intel_encoder *encoder,
			    const struct intel_crtc_state *crtc_state,
			    int *n_entries)
{
	if (crtc_state->port_clock > 540000)
		return intel_get_buf_trans(&icl_combo_phy_trans_dp_hbr2_edp_hbr3,
					   n_entries);
	else if (use_edp_hobl(encoder))
		return intel_get_buf_trans(&tgl_combo_phy_trans_edp_hbr2_hobl,
					   n_entries);
	else if (use_edp_low_vswing(encoder))
		return intel_get_buf_trans(&icl_combo_phy_trans_edp_hbr2,
					   n_entries);
	else
		return dg1_get_combo_buf_trans_dp(encoder, crtc_state, n_entries);
}

static const struct intel_ddi_buf_trans *
dg1_get_combo_buf_trans(struct intel_encoder *encoder,
			const struct intel_crtc_state *crtc_state,
			int *n_entries)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		return intel_get_buf_trans(&icl_combo_phy_trans_hdmi, n_entries);
	else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_EDP))
		return dg1_get_combo_buf_trans_edp(encoder, crtc_state, n_entries);
	else
		return dg1_get_combo_buf_trans_dp(encoder, crtc_state, n_entries);
}

static const struct intel_ddi_buf_trans *
rkl_get_combo_buf_trans_dp(struct intel_encoder *encoder,
			   const struct intel_crtc_state *crtc_state,
			   int *n_entries)
{
	if (crtc_state->port_clock > 270000)
		return intel_get_buf_trans(&rkl_combo_phy_trans_dp_hbr2_hbr3, n_entries);
	else
		return intel_get_buf_trans(&rkl_combo_phy_trans_dp_hbr, n_entries);
}

static const struct intel_ddi_buf_trans *
rkl_get_combo_buf_trans_edp(struct intel_encoder *encoder,
			    const struct intel_crtc_state *crtc_state,
			    int *n_entries)
{
	if (crtc_state->port_clock > 540000) {
		return intel_get_buf_trans(&icl_combo_phy_trans_dp_hbr2_edp_hbr3,
					   n_entries);
	} else if (use_edp_hobl(encoder)) {
		return intel_get_buf_trans(&tgl_combo_phy_trans_edp_hbr2_hobl,
					   n_entries);
	} else if (use_edp_low_vswing(encoder)) {
		return intel_get_buf_trans(&icl_combo_phy_trans_edp_hbr2,
					   n_entries);
	}

	return rkl_get_combo_buf_trans_dp(encoder, crtc_state, n_entries);
}

static const struct intel_ddi_buf_trans *
rkl_get_combo_buf_trans(struct intel_encoder *encoder,
			const struct intel_crtc_state *crtc_state,
			int *n_entries)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		return intel_get_buf_trans(&icl_combo_phy_trans_hdmi, n_entries);
	else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_EDP))
		return rkl_get_combo_buf_trans_edp(encoder, crtc_state, n_entries);
	else
		return rkl_get_combo_buf_trans_dp(encoder, crtc_state, n_entries);
}

static const struct intel_ddi_buf_trans *
adls_get_combo_buf_trans_dp(struct intel_encoder *encoder,
			    const struct intel_crtc_state *crtc_state,
			    int *n_entries)
{
	if (crtc_state->port_clock > 270000)
		return intel_get_buf_trans(&adls_combo_phy_trans_dp_hbr2_hbr3, n_entries);
	else
		return intel_get_buf_trans(&tgl_combo_phy_trans_dp_hbr, n_entries);
}

static const struct intel_ddi_buf_trans *
adls_get_combo_buf_trans_edp(struct intel_encoder *encoder,
			     const struct intel_crtc_state *crtc_state,
			     int *n_entries)
{
	if (crtc_state->port_clock > 540000)
		return intel_get_buf_trans(&adls_combo_phy_trans_edp_hbr3, n_entries);
	else if (use_edp_hobl(encoder))
		return intel_get_buf_trans(&tgl_combo_phy_trans_edp_hbr2_hobl, n_entries);
	else if (use_edp_low_vswing(encoder))
		return intel_get_buf_trans(&adls_combo_phy_trans_edp_hbr2, n_entries);
	else
		return adls_get_combo_buf_trans_dp(encoder, crtc_state, n_entries);
}

static const struct intel_ddi_buf_trans *
adls_get_combo_buf_trans(struct intel_encoder *encoder,
			 const struct intel_crtc_state *crtc_state,
			 int *n_entries)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		return intel_get_buf_trans(&icl_combo_phy_trans_hdmi, n_entries);
	else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_EDP))
		return adls_get_combo_buf_trans_edp(encoder, crtc_state, n_entries);
	else
		return adls_get_combo_buf_trans_dp(encoder, crtc_state, n_entries);
}

static const struct intel_ddi_buf_trans *
adlp_get_combo_buf_trans_dp(struct intel_encoder *encoder,
			    const struct intel_crtc_state *crtc_state,
			    int *n_entries)
{
	if (crtc_state->port_clock > 270000)
		return intel_get_buf_trans(&adlp_combo_phy_trans_dp_hbr2_hbr3, n_entries);
	else
		return intel_get_buf_trans(&adlp_combo_phy_trans_dp_hbr, n_entries);
}

static const struct intel_ddi_buf_trans *
adlp_get_combo_buf_trans_edp(struct intel_encoder *encoder,
			     const struct intel_crtc_state *crtc_state,
			     int *n_entries)
{
	if (crtc_state->port_clock > 540000) {
		return intel_get_buf_trans(&adlp_combo_phy_trans_edp_hbr3,
					   n_entries);
	} else if (use_edp_hobl(encoder)) {
		return intel_get_buf_trans(&tgl_combo_phy_trans_edp_hbr2_hobl,
					   n_entries);
	} else if (use_edp_low_vswing(encoder)) {
		return intel_get_buf_trans(&adlp_combo_phy_trans_edp_up_to_hbr2,
					   n_entries);
	}

	return adlp_get_combo_buf_trans_dp(encoder, crtc_state, n_entries);
}

static const struct intel_ddi_buf_trans *
adlp_get_combo_buf_trans(struct intel_encoder *encoder,
			 const struct intel_crtc_state *crtc_state,
			 int *n_entries)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		return intel_get_buf_trans(&icl_combo_phy_trans_hdmi, n_entries);
	else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_EDP))
		return adlp_get_combo_buf_trans_edp(encoder, crtc_state, n_entries);
	else
		return adlp_get_combo_buf_trans_dp(encoder, crtc_state, n_entries);
}

static const struct intel_ddi_buf_trans *
tgl_get_dkl_buf_trans_dp(struct intel_encoder *encoder,
			 const struct intel_crtc_state *crtc_state,
			 int *n_entries)
{
	if (crtc_state->port_clock > 270000) {
		return intel_get_buf_trans(&tgl_dkl_phy_trans_dp_hbr2,
					   n_entries);
	} else {
		return intel_get_buf_trans(&tgl_dkl_phy_trans_dp_hbr,
					   n_entries);
	}
}

static const struct intel_ddi_buf_trans *
tgl_get_dkl_buf_trans(struct intel_encoder *encoder,
		      const struct intel_crtc_state *crtc_state,
		      int *n_entries)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		return intel_get_buf_trans(&tgl_dkl_phy_trans_hdmi, n_entries);
	else
		return tgl_get_dkl_buf_trans_dp(encoder, crtc_state, n_entries);
}

static const struct intel_ddi_buf_trans *
adlp_get_dkl_buf_trans_dp(struct intel_encoder *encoder,
			  const struct intel_crtc_state *crtc_state,
			  int *n_entries)
{
	if (crtc_state->port_clock > 270000) {
		return intel_get_buf_trans(&adlp_dkl_phy_trans_dp_hbr2_hbr3,
					   n_entries);
	} else {
		return intel_get_buf_trans(&adlp_dkl_phy_trans_dp_hbr,
					   n_entries);
	}
}

static const struct intel_ddi_buf_trans *
adlp_get_dkl_buf_trans(struct intel_encoder *encoder,
		       const struct intel_crtc_state *crtc_state,
		       int *n_entries)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		return intel_get_buf_trans(&tgl_dkl_phy_trans_hdmi, n_entries);
	else
		return adlp_get_dkl_buf_trans_dp(encoder, crtc_state, n_entries);
}

static const struct intel_ddi_buf_trans *
dg2_get_snps_buf_trans(struct intel_encoder *encoder,
		       const struct intel_crtc_state *crtc_state,
		       int *n_entries)
{
	if (intel_crtc_has_dp_encoder(crtc_state) &&
	    intel_dp_is_uhbr(crtc_state))
		return intel_get_buf_trans(&dg2_snps_trans_uhbr, n_entries);
	else
		return intel_get_buf_trans(&dg2_snps_trans, n_entries);
}

static const struct intel_ddi_buf_trans *
mtl_get_cx0_buf_trans(struct intel_encoder *encoder,
		      const struct intel_crtc_state *crtc_state,
		      int *n_entries)
{
	struct drm_i915_private *i915 = to_i915(encoder->base.dev);
	enum phy phy = intel_port_to_phy(i915, encoder->port);

	if (intel_crtc_has_dp_encoder(crtc_state) && crtc_state->port_clock >= 1000000)
		return intel_get_buf_trans(&mtl_c20_trans_uhbr, n_entries);
	else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI) && !(intel_is_c10phy(i915, phy)))
		return intel_get_buf_trans(&mtl_c20_trans_hdmi, n_entries);
	else if (!intel_is_c10phy(i915, phy))
		return intel_get_buf_trans(&mtl_c20_trans_dp14, n_entries);
	else
		return intel_get_buf_trans(&mtl_c10_trans_dp14, n_entries);
}

void intel_ddi_buf_trans_init(struct intel_encoder *encoder)
{
	struct drm_i915_private *i915 = to_i915(encoder->base.dev);
	enum phy phy = intel_port_to_phy(i915, encoder->port);

	if (DISPLAY_VER(i915) >= 14) {
		encoder->get_buf_trans = mtl_get_cx0_buf_trans;
	} else if (IS_DG2(i915)) {
		encoder->get_buf_trans = dg2_get_snps_buf_trans;
	} else if (IS_ALDERLAKE_P(i915)) {
		if (intel_phy_is_combo(i915, phy))
			encoder->get_buf_trans = adlp_get_combo_buf_trans;
		else
			encoder->get_buf_trans = adlp_get_dkl_buf_trans;
	} else if (IS_ALDERLAKE_S(i915)) {
		encoder->get_buf_trans = adls_get_combo_buf_trans;
	} else if (IS_ROCKETLAKE(i915)) {
		encoder->get_buf_trans = rkl_get_combo_buf_trans;
	} else if (IS_DG1(i915)) {
		encoder->get_buf_trans = dg1_get_combo_buf_trans;
	} else if (DISPLAY_VER(i915) >= 12) {
		if (intel_phy_is_combo(i915, phy))
			encoder->get_buf_trans = tgl_get_combo_buf_trans;
		else
			encoder->get_buf_trans = tgl_get_dkl_buf_trans;
	} else if (DISPLAY_VER(i915) == 11) {
		if (IS_PLATFORM(i915, INTEL_JASPERLAKE))
			encoder->get_buf_trans = jsl_get_combo_buf_trans;
		else if (IS_PLATFORM(i915, INTEL_ELKHARTLAKE))
			encoder->get_buf_trans = ehl_get_combo_buf_trans;
		else if (intel_phy_is_combo(i915, phy))
			encoder->get_buf_trans = icl_get_combo_buf_trans;
		else
			encoder->get_buf_trans = icl_get_mg_buf_trans;
	} else if (IS_GEMINILAKE(i915) || IS_BROXTON(i915)) {
		encoder->get_buf_trans = bxt_get_buf_trans;
	} else if (IS_COMETLAKE_ULX(i915) || IS_COFFEELAKE_ULX(i915) || IS_KABYLAKE_ULX(i915)) {
		encoder->get_buf_trans = kbl_y_get_buf_trans;
	} else if (IS_COMETLAKE_ULT(i915) || IS_COFFEELAKE_ULT(i915) || IS_KABYLAKE_ULT(i915)) {
		encoder->get_buf_trans = kbl_u_get_buf_trans;
	} else if (IS_COMETLAKE(i915) || IS_COFFEELAKE(i915) || IS_KABYLAKE(i915)) {
		encoder->get_buf_trans = kbl_get_buf_trans;
	} else if (IS_SKYLAKE_ULX(i915)) {
		encoder->get_buf_trans = skl_y_get_buf_trans;
	} else if (IS_SKYLAKE_ULT(i915)) {
		encoder->get_buf_trans = skl_u_get_buf_trans;
	} else if (IS_SKYLAKE(i915)) {
		encoder->get_buf_trans = skl_get_buf_trans;
	} else if (IS_BROADWELL(i915)) {
		encoder->get_buf_trans = bdw_get_buf_trans;
	} else if (IS_HASWELL(i915)) {
		encoder->get_buf_trans = hsw_get_buf_trans;
	} else {
		MISSING_CASE(INTEL_INFO(i915)->platform);
	}
}
