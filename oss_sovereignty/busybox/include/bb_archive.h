#ifndef UNARCHIVE_H
#define UNARCHIVE_H 1
PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN
enum {
#if BB_BIG_ENDIAN
	COMPRESS_MAGIC = 0x1f9d,
	GZIP_MAGIC  = 0x1f8b,
	BZIP2_MAGIC = 256 * 'B' + 'Z',
	XZ_MAGIC1   = 256 * 0xfd + '7',
	XZ_MAGIC2   = 256 * (unsigned)(256 * (256 * 'z' + 'X') + 'Z') + 0,
	XZ_MAGIC1a  = 256 * (unsigned)(256 * (256 * 0xfd + '7') + 'z') + 'X',
	XZ_MAGIC2a  = 256 * 'Z' + 0,
#else
	COMPRESS_MAGIC = 0x9d1f,
	GZIP_MAGIC  = 0x8b1f,
	BZIP2_MAGIC = 'B' + 'Z' * 256,
	XZ_MAGIC1   = 0xfd + '7' * 256,
	XZ_MAGIC2   = 'z' + ('X' + ('Z' + 0 * 256) * 256) * 256,
	XZ_MAGIC1a  = 0xfd + ('7' + ('z' + 'X' * 256) * 256) * 256,
	XZ_MAGIC2a  = 'Z' + 0 * 256,
#endif
};
typedef struct file_header_t {
	char *name;
	char *link_target;
#if ENABLE_FEATURE_TAR_UNAME_GNAME
	char *tar__uname;
	char *tar__gname;
#endif
	off_t size;
	uid_t uid;
	gid_t gid;
	mode_t mode;
	time_t mtime;
	dev_t device;
} file_header_t;
struct hardlinks_t;
typedef struct archive_handle_t {
	unsigned ah_flags;
	int src_fd;
	char FAST_FUNC (*filter)(struct archive_handle_t *);
	llist_t *accept;
	llist_t *reject;
	llist_t *passed;
	file_header_t *file_header;
	llist_t *link_placeholders;
	void FAST_FUNC (*action_header)(const file_header_t *);
	void FAST_FUNC (*action_data)(struct archive_handle_t *);
	void FAST_FUNC (*seek)(int fd, off_t amount);
	off_t offset;
#if ENABLE_FEATURE_TAR_LONG_OPTIONS
	unsigned tar__strip_components;
#endif
#define PAX_NEXT_FILE 0
#define PAX_GLOBAL    1
#if ENABLE_TAR || ENABLE_DPKG || ENABLE_DPKG_DEB
	smallint tar__end;
# if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
	char* tar__longname;
	char* tar__linkname;
# endif
# if ENABLE_FEATURE_TAR_TO_COMMAND
	char* tar__to_command;
	const char* tar__to_command_shell;
# endif
# if ENABLE_FEATURE_TAR_SELINUX
	char* tar__sctx[2];
# endif
#endif
#if ENABLE_CPIO || ENABLE_RPM2CPIO || ENABLE_RPM
	uoff_t cpio__blocks;
	struct bb_uidgid_t cpio__owner;
	struct hardlinks_t *cpio__hardlinks_to_create;
	struct hardlinks_t *cpio__created_hardlinks;
#endif
#if ENABLE_DPKG || ENABLE_DPKG_DEB
	char *dpkg__buffer;
	char FAST_FUNC (*dpkg__action_data_subarchive)(struct archive_handle_t *);
	struct archive_handle_t *dpkg__sub_archive;
#endif
#if ENABLE_FEATURE_AR_CREATE
	const char *ar__name;
	struct archive_handle_t *ar__out;
#endif
#if ENABLE_FEATURE_AR_LONG_FILENAMES
	char *ar__long_names;
	unsigned ar__long_name_size;
#endif
} archive_handle_t;
#define ARCHIVE_RESTORE_DATE        (1 << 0)
#define ARCHIVE_CREATE_LEADING_DIRS (1 << 1)
#define ARCHIVE_UNLINK_OLD          (1 << 2)
#define ARCHIVE_EXTRACT_NEWER       (1 << 3)
#define ARCHIVE_DONT_RESTORE_OWNER  (1 << 4)
#define ARCHIVE_DONT_RESTORE_PERM   (1 << 5)
#define ARCHIVE_NUMERIC_OWNER       (1 << 6)
#define ARCHIVE_O_TRUNC             (1 << 7)
#define ARCHIVE_REMEMBER_NAMES      (1 << 8)
#if ENABLE_RPM
#define ARCHIVE_REPLACE_VIA_RENAME  (1 << 9)
#endif
#define TAR_BLOCK_SIZE 512
#define NAME_SIZE      100
#define NAME_SIZE_STR "100"
typedef struct tar_header_t {      
	char name[NAME_SIZE];      
	char mode[8];              
	char uid[8];               
	char gid[8];               
	char size[12];             
	char mtime[12];            
	char chksum[8];            
	char typeflag;             
	char linkname[NAME_SIZE];  
	char magic[8];             
	char uname[32];            
	char gname[32];            
	char devmajor[8];          
	char devminor[8];          
	char prefix[155];          
	char padding[12];          
} tar_header_t;
struct BUG_tar_header {
	char c[sizeof(tar_header_t) == TAR_BLOCK_SIZE ? 1 : -1];
};
void chksum_and_xwrite_tar_header(int fd, struct tar_header_t *hp) FAST_FUNC;
extern const char cpio_TRAILER[];
archive_handle_t *init_handle(void) FAST_FUNC;
char filter_accept_all(archive_handle_t *archive_handle) FAST_FUNC;
char filter_accept_list(archive_handle_t *archive_handle) FAST_FUNC;
char filter_accept_list_reassign(archive_handle_t *archive_handle) FAST_FUNC;
char filter_accept_reject_list(archive_handle_t *archive_handle) FAST_FUNC;
void unpack_ar_archive(archive_handle_t *ar_archive) FAST_FUNC;
void data_skip(archive_handle_t *archive_handle) FAST_FUNC;
void data_extract_all(archive_handle_t *archive_handle) FAST_FUNC;
void data_extract_to_stdout(archive_handle_t *archive_handle) FAST_FUNC;
void data_extract_to_command(archive_handle_t *archive_handle) FAST_FUNC;
void header_skip(const file_header_t *file_header) FAST_FUNC;
void header_list(const file_header_t *file_header) FAST_FUNC;
void header_verbose_list(const file_header_t *file_header) FAST_FUNC;
char get_header_ar(archive_handle_t *archive_handle) FAST_FUNC;
char get_header_cpio(archive_handle_t *archive_handle) FAST_FUNC;
char get_header_tar(archive_handle_t *archive_handle) FAST_FUNC;
char get_header_tar_gz(archive_handle_t *archive_handle) FAST_FUNC;
char get_header_tar_bz2(archive_handle_t *archive_handle) FAST_FUNC;
char get_header_tar_lzma(archive_handle_t *archive_handle) FAST_FUNC;
char get_header_tar_xz(archive_handle_t *archive_handle) FAST_FUNC;
void seek_by_jump(int fd, off_t amount) FAST_FUNC;
void seek_by_read(int fd, off_t amount) FAST_FUNC;
const char *strip_unsafe_prefix(const char *str) FAST_FUNC;
void create_or_remember_link(llist_t **link_placeholders,
		const char *target,
		const char *linkname,
		int hard_link) FAST_FUNC;
void create_links_from_list(llist_t *list) FAST_FUNC;
void data_align(archive_handle_t *archive_handle, unsigned boundary) FAST_FUNC;
const llist_t *find_list_entry(const llist_t *list, const char *filename) FAST_FUNC;
const llist_t *find_list_entry2(const llist_t *list, const char *filename) FAST_FUNC;
char *unpack_bz2_data(const char *packed, int packed_len, int unpacked_len) FAST_FUNC;
typedef struct transformer_state_t {
	smallint signature_skipped;  
	IF_DESKTOP(long long) int FAST_FUNC (*xformer)(struct transformer_state_t *xstate);
	USE_FOR_NOMMU(const char *xformer_prog;)
	int      src_fd;
	int      dst_fd;
	size_t   mem_output_size_max;  
	size_t   mem_output_size;
	char     *mem_output_buf;
	off_t    bytes_out;
	off_t    bytes_in;   
	uint32_t crc32;
	time_t   mtime;      
	union {              
		uint8_t b[8];
		uint16_t b16[4];
		uint32_t b32[2];
	} magic;
} transformer_state_t;
void init_transformer_state(transformer_state_t *xstate) FAST_FUNC;
ssize_t transformer_write(transformer_state_t *xstate, const void *buf, size_t bufsize) FAST_FUNC;
ssize_t xtransformer_write(transformer_state_t *xstate, const void *buf, size_t bufsize) FAST_FUNC;
int check_signature16(transformer_state_t *xstate, unsigned magic16) FAST_FUNC;
IF_DESKTOP(long long) int inflate_unzip(transformer_state_t *xstate) FAST_FUNC;
IF_DESKTOP(long long) int unpack_Z_stream(transformer_state_t *xstate) FAST_FUNC;
IF_DESKTOP(long long) int unpack_gz_stream(transformer_state_t *xstate) FAST_FUNC;
IF_DESKTOP(long long) int unpack_bz2_stream(transformer_state_t *xstate) FAST_FUNC;
IF_DESKTOP(long long) int unpack_lzma_stream(transformer_state_t *xstate) FAST_FUNC;
IF_DESKTOP(long long) int unpack_xz_stream(transformer_state_t *xstate) FAST_FUNC;
char* append_ext(char *filename, const char *expected_ext) FAST_FUNC;
int bbunpack(char **argv,
		IF_DESKTOP(long long) int FAST_FUNC (*unpacker)(transformer_state_t *xstate),
		char* FAST_FUNC (*make_new_name)(char *filename, const char *expected_ext),
		const char *expected_ext
) FAST_FUNC;
#define BBUNPK_OPTSTR "cfkvq"
#define BBUNPK_OPTSTRLEN  5
#define BBUNPK_OPTSTRMASK ((1 << BBUNPK_OPTSTRLEN) - 1)
enum {
	BBUNPK_OPT_STDOUT     = 1 << 0,
	BBUNPK_OPT_FORCE      = 1 << 1,
	BBUNPK_OPT_KEEP       = 1 << 2,
	BBUNPK_OPT_VERBOSE    = 1 << 3,
	BBUNPK_OPT_QUIET      = 1 << 4,
	BBUNPK_OPT_DECOMPRESS = 1 << 5,
	BBUNPK_OPT_TEST       = 1 << 6,
	BBUNPK_SEAMLESS_MAGIC = (1 << 31) * ENABLE_ZCAT * SEAMLESS_COMPRESSION,
};
void check_errors_in_children(int signo);
#if BB_MMU
void fork_transformer(int fd,
	int signature_skipped,
	IF_DESKTOP(long long) int FAST_FUNC (*transformer)(transformer_state_t *xstate)
) FAST_FUNC;
#define fork_transformer_with_sig(fd, transformer, transform_prog) fork_transformer((fd), 0, (transformer))
#define fork_transformer_with_no_sig(fd, transformer)              fork_transformer((fd), 1, (transformer))
#else
void fork_transformer(int fd, const char *transform_prog) FAST_FUNC;
#define fork_transformer_with_sig(fd, transformer, transform_prog) fork_transformer((fd), (transform_prog))
#endif
POP_SAVED_FUNCTION_VISIBILITY
#endif
