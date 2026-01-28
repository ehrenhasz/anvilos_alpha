

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN


typedef struct svstatus_t {
	uint64_t time_be64 PACKED;
	uint32_t time_nsec_be32 PACKED;
	uint32_t pid_le32 PACKED;
	uint8_t  paused;
	uint8_t  want; 
	uint8_t  got_term;
	uint8_t  run_or_finish;
} svstatus_t;
struct ERR_svstatus_must_be_20_bytes {
	char ERR_svstatus_must_be_20_bytes[sizeof(svstatus_t) == 20 ? 1 : -1];
};

POP_SAVED_FUNCTION_VISIBILITY
