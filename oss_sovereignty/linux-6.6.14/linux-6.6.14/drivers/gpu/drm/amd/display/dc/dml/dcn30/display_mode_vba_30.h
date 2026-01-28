#ifndef __DML30_DISPLAY_MODE_VBA_H__
#define __DML30_DISPLAY_MODE_VBA_H__
void dml30_recalculate(struct display_mode_lib *mode_lib);
void dml30_ModeSupportAndSystemConfigurationFull(struct display_mode_lib *mode_lib);
double dml30_CalculateWriteBackDISPCLK(
		enum source_format_class WritebackPixelFormat,
		double PixelClock,
		double WritebackHRatio,
		double WritebackVRatio,
		unsigned int WritebackHTaps,
		unsigned int WritebackVTaps,
		long   WritebackSourceWidth,
		long   WritebackDestinationWidth,
		unsigned int HTotal,
		unsigned int WritebackLineBufferSize);
void dml30_CalculateBytePerPixelAnd256BBlockSizes(
		enum source_format_class SourcePixelFormat,
		enum dm_swizzle_mode SurfaceTiling,
		unsigned int *BytePerPixelY,
		unsigned int *BytePerPixelC,
		double       *BytePerPixelDETY,
		double       *BytePerPixelDETC,
		unsigned int *BlockHeight256BytesY,
		unsigned int *BlockHeight256BytesC,
		unsigned int *BlockWidth256BytesY,
		unsigned int *BlockWidth256BytesC);
#endif  
