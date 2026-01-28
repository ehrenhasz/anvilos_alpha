#include <ctype.h>
#include <libnvpair.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <libzfs.h>
#include <libzfs_core.h>
#include <sys/dmu.h>
#include <sys/zfs_ioctl.h>
#include "zstream.h"
int
zstream_do_token(int argc, char *argv[])
{
	char *resume_token = NULL;
	libzfs_handle_t *hdl;
	if (argc < 2) {
		(void) fprintf(stderr, "Need to pass the resume token\n");
		zstream_usage();
	}
	resume_token = argv[1];
	if ((hdl = libzfs_init()) == NULL) {
		(void) fprintf(stderr, "%s\n", libzfs_error_init(errno));
		return (1);
	}
	nvlist_t *resume_nvl =
	    zfs_send_resume_token_to_nvlist(hdl, resume_token);
	if (resume_nvl == NULL) {
		(void) fprintf(stderr,
		    "Unable to parse resume token: %s\n",
		    libzfs_error_description(hdl));
		libzfs_fini(hdl);
		return (1);
	}
	dump_nvlist(resume_nvl, 5);
	nvlist_free(resume_nvl);
	libzfs_fini(hdl);
	return (0);
}
