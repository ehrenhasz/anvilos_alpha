

#define LZO1X
#undef LZO1Y

#undef assert

#define assert(v) ((void)0)

int lzo1x_1_compress(const uint8_t* src, unsigned src_len,
		uint8_t* dst, unsigned* dst_len,
		void* wrkmem);
int lzo1x_1_15_compress(const uint8_t* src, unsigned src_len,
		uint8_t* dst, unsigned* dst_len,
		void* wrkmem);
int lzo1x_999_compress_level(const uint8_t* in, unsigned in_len,
		uint8_t* out, unsigned* out_len,
		void* wrkmem,
		int compression_level);





int lzo1x_decompress_safe(const uint8_t* src, unsigned src_len,
		uint8_t* dst, unsigned* dst_len	);

#define LZO_E_OK                    0
#define LZO_E_ERROR                 (-1)
#define LZO_E_OUT_OF_MEMORY         (-2)    
#define LZO_E_NOT_COMPRESSIBLE      (-3)    
#define LZO_E_INPUT_OVERRUN         (-4)
#define LZO_E_OUTPUT_OVERRUN        (-5)
#define LZO_E_LOOKBEHIND_OVERRUN    (-6)
#define LZO_E_EOF_NOT_FOUND         (-7)
#define LZO_E_INPUT_NOT_CONSUMED    (-8)
#define LZO_E_NOT_YET_IMPLEMENTED   (-9)    


#define LZO_VERSION   0x2030
