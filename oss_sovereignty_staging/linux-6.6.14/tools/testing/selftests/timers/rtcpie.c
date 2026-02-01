
 

#include <stdio.h>
#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include "../kselftest.h"

 
static const char default_rtc[] = "/dev/rtc0";

int main(int argc, char **argv)
{
	int i, fd, retval, irqcount = 0;
	unsigned long tmp, data, old_pie_rate;
	const char *rtc = default_rtc;
	struct timeval start, end, diff;

	switch (argc) {
	case 2:
		rtc = argv[1];
		break;
	case 1:
		fd = open(default_rtc, O_RDONLY);
		if (fd == -1) {
			printf("Default RTC %s does not exist. Test Skipped!\n", default_rtc);
			exit(KSFT_SKIP);
		}
		close(fd);
		break;
	default:
		fprintf(stderr, "usage:  rtctest [rtcdev] [d]\n");
		return 1;
	}

	fd = open(rtc, O_RDONLY);

	if (fd ==  -1) {
		perror(rtc);
		exit(errno);
	}

	 
	retval = ioctl(fd, RTC_IRQP_READ, &old_pie_rate);
	if (retval == -1) {
		 
		if (errno == EINVAL) {
			fprintf(stderr, "\nNo periodic IRQ support\n");
			goto done;
		}
		perror("RTC_IRQP_READ ioctl");
		exit(errno);
	}
	fprintf(stderr, "\nPeriodic IRQ rate is %ldHz.\n", old_pie_rate);

	fprintf(stderr, "Counting 20 interrupts at:");
	fflush(stderr);

	 
	for (tmp=2; tmp<=64; tmp*=2) {

		retval = ioctl(fd, RTC_IRQP_SET, tmp);
		if (retval == -1) {
			 
			if (errno == EINVAL) {
				fprintf(stderr,
					"\n...Periodic IRQ rate is fixed\n");
				goto done;
			}
			perror("RTC_IRQP_SET ioctl");
			exit(errno);
		}

		fprintf(stderr, "\n%ldHz:\t", tmp);
		fflush(stderr);

		 
		retval = ioctl(fd, RTC_PIE_ON, 0);
		if (retval == -1) {
			perror("RTC_PIE_ON ioctl");
			exit(errno);
		}

		for (i=1; i<21; i++) {
			gettimeofday(&start, NULL);
			 
			retval = read(fd, &data, sizeof(unsigned long));
			if (retval == -1) {
				perror("read");
				exit(errno);
			}
			gettimeofday(&end, NULL);
			timersub(&end, &start, &diff);
			if (diff.tv_sec > 0 ||
			    diff.tv_usec > ((1000000L / tmp) * 1.10)) {
				fprintf(stderr, "\nPIE delta error: %ld.%06ld should be close to 0.%06ld\n",
				       diff.tv_sec, diff.tv_usec,
				       (1000000L / tmp));
				fflush(stdout);
				exit(-1);
			}

			fprintf(stderr, " %d",i);
			fflush(stderr);
			irqcount++;
		}

		 
		retval = ioctl(fd, RTC_PIE_OFF, 0);
		if (retval == -1) {
			perror("RTC_PIE_OFF ioctl");
			exit(errno);
		}
	}

done:
	ioctl(fd, RTC_IRQP_SET, old_pie_rate);

	fprintf(stderr, "\n\n\t\t\t *** Test complete ***\n");

	close(fd);

	return 0;
}
