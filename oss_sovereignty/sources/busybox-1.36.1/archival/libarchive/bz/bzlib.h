








#define BZ_RUN               0
#define BZ_FLUSH             1
#define BZ_FINISH            2

#define BZ_OK                0
#define BZ_RUN_OK            1
#define BZ_FLUSH_OK          2
#define BZ_FINISH_OK         3
#define BZ_STREAM_END        4
#define BZ_SEQUENCE_ERROR    (-1)
#define BZ_PARAM_ERROR       (-2)
#define BZ_MEM_ERROR         (-3)
#define BZ_DATA_ERROR        (-4)
#define BZ_DATA_ERROR_MAGIC  (-5)
#define BZ_IO_ERROR          (-6)
#define BZ_UNEXPECTED_EOF    (-7)
#define BZ_OUTBUFF_FULL      (-8)
#define BZ_CONFIG_ERROR      (-9)

typedef struct bz_stream {
	void *state;
	char *next_in;
	char *next_out;
	unsigned avail_in;
	unsigned avail_out;
	
	unsigned long long total_out;
} bz_stream;



static void BZ2_bzCompressInit(bz_stream *strm, int blockSize100k);
static int BZ2_bzCompress(bz_stream *strm, int action);
#if ENABLE_FEATURE_CLEAN_UP
static void BZ2_bzCompressEnd(bz_stream *strm);
#endif




