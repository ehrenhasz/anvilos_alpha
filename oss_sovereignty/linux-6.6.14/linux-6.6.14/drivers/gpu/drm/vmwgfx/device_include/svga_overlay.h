#ifndef _SVGA_OVERLAY_H_
#define _SVGA_OVERLAY_H_
#include "svga_reg.h"
#if defined __cplusplus
extern "C" {
#endif
#define VMWARE_FOURCC_YV12 0x32315659
#define VMWARE_FOURCC_YUY2 0x32595559
#define VMWARE_FOURCC_UYVY 0x59565955
typedef enum {
	SVGA_OVERLAY_FORMAT_INVALID = 0,
	SVGA_OVERLAY_FORMAT_YV12 = VMWARE_FOURCC_YV12,
	SVGA_OVERLAY_FORMAT_YUY2 = VMWARE_FOURCC_YUY2,
	SVGA_OVERLAY_FORMAT_UYVY = VMWARE_FOURCC_UYVY,
} SVGAOverlayFormat;
#define SVGA_VIDEO_COLORKEY_MASK 0x00ffffff
#define SVGA_ESCAPE_VMWARE_VIDEO 0x00020000
#define SVGA_ESCAPE_VMWARE_VIDEO_SET_REGS 0x00020001
#define SVGA_ESCAPE_VMWARE_VIDEO_FLUSH 0x00020002
typedef struct SVGAEscapeVideoSetRegs {
	struct {
		uint32 cmdType;
		uint32 streamId;
	} header;
	struct {
		uint32 registerId;
		uint32 value;
	} items[1];
} SVGAEscapeVideoSetRegs;
typedef struct SVGAEscapeVideoFlush {
	uint32 cmdType;
	uint32 streamId;
} SVGAEscapeVideoFlush;
#pragma pack(push, 1)
typedef struct {
	uint32 command;
	uint32 overlay;
} SVGAFifoEscapeCmdVideoBase;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct {
	SVGAFifoEscapeCmdVideoBase videoCmd;
} SVGAFifoEscapeCmdVideoFlush;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct {
	SVGAFifoEscapeCmdVideoBase videoCmd;
	struct {
		uint32 regId;
		uint32 value;
	} items[1];
} SVGAFifoEscapeCmdVideoSetRegs;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct {
	SVGAFifoEscapeCmdVideoBase videoCmd;
	struct {
		uint32 regId;
		uint32 value;
	} items[SVGA_VIDEO_NUM_REGS];
} SVGAFifoEscapeCmdVideoSetAllRegs;
#pragma pack(pop)
#if defined __cplusplus
}
#endif
#endif
