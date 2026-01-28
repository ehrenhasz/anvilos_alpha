#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <libzutil.h>
#include <string.h>
boolean_t
zfs_isnumber(const char *str)
{
	if (!*str)
		return (B_FALSE);
	for (; *str; str++)
		if (!(isdigit(*str) || (*str == '.')))
			return (B_FALSE);
	if (str[strlen(str) - 1] == '.') {
		return (B_FALSE);
	}
	return (B_TRUE);
}
void
zfs_nicenum_format(uint64_t num, char *buf, size_t buflen,
    enum zfs_nicenum_format format)
{
	uint64_t n = num;
	int index = 0;
	const char *u;
	const char *units[3][7] = {
	    [ZFS_NICENUM_1024] = {"", "K", "M", "G", "T", "P", "E"},
	    [ZFS_NICENUM_BYTES] = {"B", "K", "M", "G", "T", "P", "E"},
	    [ZFS_NICENUM_TIME] = {"ns", "us", "ms", "s", "?", "?", "?"}
	};
	const int units_len[] = {[ZFS_NICENUM_1024] = 6,
	    [ZFS_NICENUM_BYTES] = 6,
	    [ZFS_NICENUM_TIME] = 4};
	const int k_unit[] = {	[ZFS_NICENUM_1024] = 1024,
	    [ZFS_NICENUM_BYTES] = 1024,
	    [ZFS_NICENUM_TIME] = 1000};
	double val;
	if (format == ZFS_NICENUM_RAW) {
		snprintf(buf, buflen, "%llu", (u_longlong_t)num);
		return;
	} else if (format == ZFS_NICENUM_RAWTIME && num > 0) {
		snprintf(buf, buflen, "%llu", (u_longlong_t)num);
		return;
	} else if (format == ZFS_NICENUM_RAWTIME && num == 0) {
		snprintf(buf, buflen, "%s", "-");
		return;
	}
	while (n >= k_unit[format] && index < units_len[format]) {
		n /= k_unit[format];
		index++;
	}
	u = units[format][index];
	if ((format == ZFS_NICENUM_TIME) && (num == 0)) {
		(void) snprintf(buf, buflen, "-");
	} else if ((index == 0) || ((num %
	    (uint64_t)powl(k_unit[format], index)) == 0)) {
		(void) snprintf(buf, buflen, "%llu%s", (u_longlong_t)n, u);
	} else {
		int i;
		for (i = 2; i >= 0; i--) {
			val = (double)num /
			    (uint64_t)powl(k_unit[format], index);
			if (format == ZFS_NICENUM_TIME) {
				if (snprintf(buf, buflen, "%d%s",
				    (unsigned int) floor(val), u) <= 5)
					break;
			} else {
				if (snprintf(buf, buflen, "%.*f%s", i,
				    val, u) <= 5)
					break;
			}
		}
	}
}
void
zfs_nicenum(uint64_t num, char *buf, size_t buflen)
{
	zfs_nicenum_format(num, buf, buflen, ZFS_NICENUM_1024);
}
void
zfs_nicetime(uint64_t num, char *buf, size_t buflen)
{
	zfs_nicenum_format(num, buf, buflen, ZFS_NICENUM_TIME);
}
void
zfs_niceraw(uint64_t num, char *buf, size_t buflen)
{
	zfs_nicenum_format(num, buf, buflen, ZFS_NICENUM_RAW);
}
void
zfs_nicebytes(uint64_t num, char *buf, size_t buflen)
{
	zfs_nicenum_format(num, buf, buflen, ZFS_NICENUM_BYTES);
}
