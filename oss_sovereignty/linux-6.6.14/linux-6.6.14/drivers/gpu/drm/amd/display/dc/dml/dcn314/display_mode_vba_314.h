#ifndef __DML314_DISPLAY_MODE_VBA_H__
#define __DML314_DISPLAY_MODE_VBA_H__
void dml314_recalculate(struct display_mode_lib *mode_lib);
void dml314_ModeSupportAndSystemConfigurationFull(struct display_mode_lib *mode_lib);
double dml314_CalculateWriteBackDISPCLK(
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
#endif  
