




























#include <linux/bpf.h>
#include <linux/input.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "bpf_util.h"
#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "testing_helpers.h"

int main(int argc, char **argv)
{
	struct bpf_object *obj;
	int ret, lircfd, progfd, inputfd;
	int testir1 = 0x1dead;
	int testir2 = 0x20101;
	u32 prog_ids[10], prog_flags[10], prog_cnt;

	if (argc != 3) {
		printf("Usage: %s /dev/lircN /dev/input/eventM\n", argv[0]);
		return 2;
	}

	ret = bpf_prog_test_load("test_lirc_mode2_kern.bpf.o",
				 BPF_PROG_TYPE_LIRC_MODE2, &obj, &progfd);
	if (ret) {
		printf("Failed to load bpf program\n");
		return 1;
	}

	lircfd = open(argv[1], O_RDWR | O_NONBLOCK);
	if (lircfd == -1) {
		printf("failed to open lirc device %s: %m\n", argv[1]);
		return 1;
	}

	 
	ret = bpf_prog_detach2(progfd, lircfd, BPF_LIRC_MODE2);
	if (ret != -1 || errno != ENOENT) {
		printf("bpf_prog_detach2 not attached should fail: %m\n");
		return 1;
	}

	inputfd = open(argv[2], O_RDONLY | O_NONBLOCK);
	if (inputfd == -1) {
		printf("failed to open input device %s: %m\n", argv[1]);
		return 1;
	}

	prog_cnt = 10;
	ret = bpf_prog_query(lircfd, BPF_LIRC_MODE2, 0, prog_flags, prog_ids,
			     &prog_cnt);
	if (ret) {
		printf("Failed to query bpf programs on lirc device: %m\n");
		return 1;
	}

	if (prog_cnt != 0) {
		printf("Expected nothing to be attached\n");
		return 1;
	}

	ret = bpf_prog_attach(progfd, lircfd, BPF_LIRC_MODE2, 0);
	if (ret) {
		printf("Failed to attach bpf to lirc device: %m\n");
		return 1;
	}

	 
	ret = write(lircfd, &testir1, sizeof(testir1));
	if (ret != sizeof(testir1)) {
		printf("Failed to send test IR message: %m\n");
		return 1;
	}

	struct pollfd pfd = { .fd = inputfd, .events = POLLIN };
	struct input_event event;

	for (;;) {
		poll(&pfd, 1, 100);

		 
		ret = read(inputfd, &event, sizeof(event));
		if (ret != sizeof(event)) {
			printf("Failed to read decoded IR: %m\n");
			return 1;
		}

		if (event.type == EV_MSC && event.code == MSC_SCAN &&
		    event.value == 0xdead) {
			break;
		}
	}

	 
	ret = write(lircfd, &testir2, sizeof(testir2));
	if (ret != sizeof(testir2)) {
		printf("Failed to send test IR message: %m\n");
		return 1;
	}

	for (;;) {
		poll(&pfd, 1, 100);

		 
		ret = read(inputfd, &event, sizeof(event));
		if (ret != sizeof(event)) {
			printf("Failed to read decoded IR: %m\n");
			return 1;
		}

		if (event.type == EV_REL && event.code == REL_Y &&
		    event.value == 1 ) {
			break;
		}
	}

	prog_cnt = 10;
	ret = bpf_prog_query(lircfd, BPF_LIRC_MODE2, 0, prog_flags, prog_ids,
			     &prog_cnt);
	if (ret) {
		printf("Failed to query bpf programs on lirc device: %m\n");
		return 1;
	}

	if (prog_cnt != 1) {
		printf("Expected one program to be attached\n");
		return 1;
	}

	 
	ret = bpf_prog_detach2(progfd, lircfd, BPF_LIRC_MODE2);
	if (ret) {
		printf("bpf_prog_detach2: returned %m\n");
		return 1;
	}

	return 0;
}
