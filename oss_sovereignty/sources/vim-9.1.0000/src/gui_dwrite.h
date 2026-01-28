


#ifndef GUI_DWRITE_H
#define GUI_DWRITE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DWriteContext DWriteContext;

typedef struct DWriteRenderingParams {
    float gamma;
    float enhancedContrast;
    float clearTypeLevel;
    
    int pixelGeometry;
    
    int renderingMode;
    
    int textAntialiasMode;
} DWriteRenderingParams;

void DWrite_Init(void);
void DWrite_Final(void);

DWriteContext *DWriteContext_Open(void);
void DWriteContext_BindDC(DWriteContext *ctx, HDC hdc, const RECT *rect);
void DWriteContext_SetFont(DWriteContext *ctx, HFONT hFont);
void DWriteContext_DrawText(
	DWriteContext *ctx,
	const WCHAR *text,
	int len,
	int x,
	int y,
	int w,
	int h,
	int cellWidth,
	COLORREF color,
	UINT fuOptions,
	const RECT *lprc,
	const INT *lpDx);
void DWriteContext_FillRect(DWriteContext *ctx, const RECT *rc, COLORREF color);
void DWriteContext_DrawLine(DWriteContext *ctx, int x1, int y1, int x2, int y2,
	COLORREF color);
void DWriteContext_SetPixel(DWriteContext *ctx, int x, int y, COLORREF color);
void DWriteContext_Scroll(DWriteContext *ctx, int x, int y, const RECT *rc);
void DWriteContext_Flush(DWriteContext *ctx);
void DWriteContext_Close(DWriteContext *ctx);

void DWriteContext_SetRenderingParams(
	DWriteContext *ctx,
	const DWriteRenderingParams *params);

DWriteRenderingParams *DWriteContext_GetRenderingParams(
	DWriteContext *ctx,
	DWriteRenderingParams *params);

#ifdef __cplusplus
}
#endif
#endif
