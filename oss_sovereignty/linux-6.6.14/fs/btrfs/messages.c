

#include "fs.h"
#include "messages.h"
#include "discard.h"
#include "transaction.h"
#include "space-info.h"
#include "super.h"

#ifdef CONFIG_PRINTK

#define STATE_STRING_PREFACE	": state "
#define STATE_STRING_BUF_LEN	(sizeof(STATE_STRING_PREFACE) + BTRFS_FS_STATE_COUNT + 1)

 
static const char fs_state_chars[] = {
	[BTRFS_FS_STATE_REMOUNTING]		= 'M',
	[BTRFS_FS_STATE_RO]			= 0,
	[BTRFS_FS_STATE_TRANS_ABORTED]		= 'A',
	[BTRFS_FS_STATE_DEV_REPLACING]		= 'R',
	[BTRFS_FS_STATE_DUMMY_FS_INFO]		= 0,
	[BTRFS_FS_STATE_NO_CSUMS]		= 'C',
	[BTRFS_FS_STATE_LOG_CLEANUP_ERROR]	= 'L',
};

static void btrfs_state_to_string(const struct btrfs_fs_info *info, char *buf)
{
	unsigned int bit;
	bool states_printed = false;
	unsigned long fs_state = READ_ONCE(info->fs_state);
	char *curr = buf;

	memcpy(curr, STATE_STRING_PREFACE, sizeof(STATE_STRING_PREFACE));
	curr += sizeof(STATE_STRING_PREFACE) - 1;

	if (BTRFS_FS_ERROR(info)) {
		*curr++ = 'E';
		states_printed = true;
	}

	for_each_set_bit(bit, &fs_state, sizeof(fs_state)) {
		WARN_ON_ONCE(bit >= BTRFS_FS_STATE_COUNT);
		if ((bit < BTRFS_FS_STATE_COUNT) && fs_state_chars[bit]) {
			*curr++ = fs_state_chars[bit];
			states_printed = true;
		}
	}

	 
	if (!states_printed)
		curr = buf;

	*curr++ = 0;
}
#endif

 
const char * __attribute_const__ btrfs_decode_error(int errno)
{
	char *errstr = "unknown";

	switch (errno) {
	case -ENOENT:		 
		errstr = "No such entry";
		break;
	case -EIO:		 
		errstr = "IO failure";
		break;
	case -ENOMEM:		 
		errstr = "Out of memory";
		break;
	case -EEXIST:		 
		errstr = "Object already exists";
		break;
	case -ENOSPC:		 
		errstr = "No space left";
		break;
	case -EROFS:		 
		errstr = "Readonly filesystem";
		break;
	case -EOPNOTSUPP:	 
		errstr = "Operation not supported";
		break;
	case -EUCLEAN:		 
		errstr = "Filesystem corrupted";
		break;
	case -EDQUOT:		 
		errstr = "Quota exceeded";
		break;
	}

	return errstr;
}

 
__cold
void __btrfs_handle_fs_error(struct btrfs_fs_info *fs_info, const char *function,
		       unsigned int line, int errno, const char *fmt, ...)
{
	struct super_block *sb = fs_info->sb;
#ifdef CONFIG_PRINTK
	char statestr[STATE_STRING_BUF_LEN];
	const char *errstr;
#endif

#ifdef CONFIG_PRINTK_INDEX
	printk_index_subsys_emit(
		"BTRFS: error (device %s%s) in %s:%d: errno=%d %s", KERN_CRIT, fmt);
#endif

	 
	if (errno == -EROFS && sb_rdonly(sb))
		return;

#ifdef CONFIG_PRINTK
	errstr = btrfs_decode_error(errno);
	btrfs_state_to_string(fs_info, statestr);
	if (fmt) {
		struct va_format vaf;
		va_list args;

		va_start(args, fmt);
		vaf.fmt = fmt;
		vaf.va = &args;

		pr_crit("BTRFS: error (device %s%s) in %s:%d: errno=%d %s (%pV)\n",
			sb->s_id, statestr, function, line, errno, errstr, &vaf);
		va_end(args);
	} else {
		pr_crit("BTRFS: error (device %s%s) in %s:%d: errno=%d %s\n",
			sb->s_id, statestr, function, line, errno, errstr);
	}
#endif

	 
	WRITE_ONCE(fs_info->fs_error, errno);

	 
	if (!(sb->s_flags & SB_BORN))
		return;

	if (sb_rdonly(sb))
		return;

	btrfs_discard_stop(fs_info);

	 
	btrfs_set_sb_rdonly(sb);
	btrfs_info(fs_info, "forced readonly");
	 
}

#ifdef CONFIG_PRINTK
static const char * const logtypes[] = {
	"emergency",
	"alert",
	"critical",
	"error",
	"warning",
	"notice",
	"info",
	"debug",
};

 
static struct ratelimit_state printk_limits[] = {
	RATELIMIT_STATE_INIT(printk_limits[0], DEFAULT_RATELIMIT_INTERVAL, 100),
	RATELIMIT_STATE_INIT(printk_limits[1], DEFAULT_RATELIMIT_INTERVAL, 100),
	RATELIMIT_STATE_INIT(printk_limits[2], DEFAULT_RATELIMIT_INTERVAL, 100),
	RATELIMIT_STATE_INIT(printk_limits[3], DEFAULT_RATELIMIT_INTERVAL, 100),
	RATELIMIT_STATE_INIT(printk_limits[4], DEFAULT_RATELIMIT_INTERVAL, 100),
	RATELIMIT_STATE_INIT(printk_limits[5], DEFAULT_RATELIMIT_INTERVAL, 100),
	RATELIMIT_STATE_INIT(printk_limits[6], DEFAULT_RATELIMIT_INTERVAL, 100),
	RATELIMIT_STATE_INIT(printk_limits[7], DEFAULT_RATELIMIT_INTERVAL, 100),
};

void __cold _btrfs_printk(const struct btrfs_fs_info *fs_info, const char *fmt, ...)
{
	char lvl[PRINTK_MAX_SINGLE_HEADER_LEN + 1] = "\0";
	struct va_format vaf;
	va_list args;
	int kern_level;
	const char *type = logtypes[4];
	struct ratelimit_state *ratelimit = &printk_limits[4];

#ifdef CONFIG_PRINTK_INDEX
	printk_index_subsys_emit("%sBTRFS %s (device %s): ", NULL, fmt);
#endif

	va_start(args, fmt);

	while ((kern_level = printk_get_level(fmt)) != 0) {
		size_t size = printk_skip_level(fmt) - fmt;

		if (kern_level >= '0' && kern_level <= '7') {
			memcpy(lvl, fmt,  size);
			lvl[size] = '\0';
			type = logtypes[kern_level - '0'];
			ratelimit = &printk_limits[kern_level - '0'];
		}
		fmt += size;
	}

	vaf.fmt = fmt;
	vaf.va = &args;

	if (__ratelimit(ratelimit)) {
		if (fs_info) {
			char statestr[STATE_STRING_BUF_LEN];

			btrfs_state_to_string(fs_info, statestr);
			_printk("%sBTRFS %s (device %s%s): %pV\n", lvl, type,
				fs_info->sb->s_id, statestr, &vaf);
		} else {
			_printk("%sBTRFS %s: %pV\n", lvl, type, &vaf);
		}
	}

	va_end(args);
}
#endif

#if BITS_PER_LONG == 32
void __cold btrfs_warn_32bit_limit(struct btrfs_fs_info *fs_info)
{
	if (!test_and_set_bit(BTRFS_FS_32BIT_WARN, &fs_info->flags)) {
		btrfs_warn(fs_info, "reaching 32bit limit for logical addresses");
		btrfs_warn(fs_info,
"due to page cache limit on 32bit systems, btrfs can't access metadata at or beyond %lluT",
			   BTRFS_32BIT_MAX_FILE_SIZE >> 40);
		btrfs_warn(fs_info,
			   "please consider upgrading to 64bit kernel/hardware");
	}
}

void __cold btrfs_err_32bit_limit(struct btrfs_fs_info *fs_info)
{
	if (!test_and_set_bit(BTRFS_FS_32BIT_ERROR, &fs_info->flags)) {
		btrfs_err(fs_info, "reached 32bit limit for logical addresses");
		btrfs_err(fs_info,
"due to page cache limit on 32bit systems, metadata beyond %lluT can't be accessed",
			  BTRFS_32BIT_MAX_FILE_SIZE >> 40);
		btrfs_err(fs_info,
			   "please consider upgrading to 64bit kernel/hardware");
	}
}
#endif

 
__cold
void __btrfs_panic(struct btrfs_fs_info *fs_info, const char *function,
		   unsigned int line, int errno, const char *fmt, ...)
{
	char *s_id = "<unknown>";
	const char *errstr;
	struct va_format vaf = { .fmt = fmt };
	va_list args;

	if (fs_info)
		s_id = fs_info->sb->s_id;

	va_start(args, fmt);
	vaf.va = &args;

	errstr = btrfs_decode_error(errno);
	if (fs_info && (btrfs_test_opt(fs_info, PANIC_ON_FATAL_ERROR)))
		panic(KERN_CRIT "BTRFS panic (device %s) in %s:%d: %pV (errno=%d %s)\n",
			s_id, function, line, &vaf, errno, errstr);

	btrfs_crit(fs_info, "panic in %s:%d: %pV (errno=%d %s)",
		   function, line, &vaf, errno, errstr);
	va_end(args);
	 
}
