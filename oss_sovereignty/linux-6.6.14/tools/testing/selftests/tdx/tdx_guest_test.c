
 

#include <sys/ioctl.h>

#include <errno.h>
#include <fcntl.h>

#include <linux/tdx-guest.h>
#include "../kselftest_harness.h"

#define TDX_GUEST_DEVNAME "/dev/tdx_guest"
#define HEX_DUMP_SIZE 8
#define DEBUG 0

 
struct tdreport_type {
	__u8 type;
	__u8 sub_type;
	__u8 version;
	__u8 reserved;
};

 
struct reportmac {
	struct tdreport_type type;
	__u8 reserved1[12];
	__u8 cpu_svn[16];
	__u8 tee_tcb_info_hash[48];
	__u8 tee_td_info_hash[48];
	__u8 reportdata[64];
	__u8 reserved2[32];
	__u8 mac[32];
};

 
struct td_info {
	__u8 attr[8];
	__u64 xfam;
	__u64 mrtd[6];
	__u64 mrconfigid[6];
	__u64 mrowner[6];
	__u64 mrownerconfig[6];
	__u64 rtmr[24];
	__u64 reserved[14];
};

 
struct tdreport {
	struct reportmac reportmac;
	__u8 tee_tcb_info[239];
	__u8 reserved[17];
	struct td_info tdinfo;
};

static void print_array_hex(const char *title, const char *prefix_str,
			    const void *buf, int len)
{
	int i, j, line_len, rowsize = HEX_DUMP_SIZE;
	const __u8 *ptr = buf;

	printf("\t\t%s", title);

	for (j = 0; j < len; j += rowsize) {
		line_len = rowsize < (len - j) ? rowsize : (len - j);
		printf("%s%.8x:", prefix_str, j);
		for (i = 0; i < line_len; i++)
			printf(" %.2x", ptr[j + i]);
		printf("\n");
	}

	printf("\n");
}

TEST(verify_report)
{
	struct tdx_report_req req;
	struct tdreport *tdreport;
	int devfd, i;

	devfd = open(TDX_GUEST_DEVNAME, O_RDWR | O_SYNC);
	ASSERT_LT(0, devfd);

	 
	for (i = 0; i < TDX_REPORTDATA_LEN; i++)
		req.reportdata[i] = i;

	 
	ASSERT_EQ(0, ioctl(devfd, TDX_CMD_GET_REPORT0, &req));

	if (DEBUG) {
		print_array_hex("\n\t\tTDX report data\n", "",
				req.reportdata, sizeof(req.reportdata));

		print_array_hex("\n\t\tTDX tdreport data\n", "",
				req.tdreport, sizeof(req.tdreport));
	}

	 
	tdreport = (struct tdreport *)req.tdreport;
	ASSERT_EQ(0, memcmp(&tdreport->reportmac.reportdata[0],
			    req.reportdata, sizeof(req.reportdata)));

	ASSERT_EQ(0, close(devfd));
}

TEST_HARNESS_MAIN
