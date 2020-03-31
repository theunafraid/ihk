#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

struct test_driver_ioctl_arg {
	unsigned long addr;
	unsigned long val;
	unsigned long addr_ext;
	int cpu; /* id in McKernel */
};

int main(int argc, char **argv)
{
	int ret;
	int fd;
	int opt;
	int cpu_expected = -1; /* */
	int fail = 0;

	struct test_driver_ioctl_arg read_arg = { 0 };
	struct test_driver_ioctl_arg write_arg = { 0 };

	while ((opt = getopt(argc, argv, "a:c:")) != -1) {
		switch (opt) {
		case 'a':
			read_arg.addr_ext = atol(optarg);
			write_arg.addr_ext = atol(optarg);
			break;
		case 'c':
			cpu_expected = atoi(optarg);
			break;
		default: /* '?' */
			printf("unknown option %c\n", optopt);
			ret = -EINVAL;
			goto out;
		}
	}

	fd = open("/dev/test_driver", O_RDWR);
	if (fd == -1) {
		printf("[INTERR] open /dev/test_driver returned %d\n",
		       errno);
		ret = 1;
		goto out;
	}

	ret = ioctl(fd, 0, (unsigned long)&read_arg);
	if (ret) {
		printf("[INTERR] ioctl 1st read returned %d\n",
		       errno);
		ret = -errno;
		goto out;
	}

	if (cpu_expected != -1) {
		if (read_arg.cpu == cpu_expected) {
			printf("[  OK  ] ");
		} else {
			printf("[  NG  ] ");
			fail++;
		}
		printf("ikc-channel source cpu: returned: %d, expected: %d\n",
		       read_arg.cpu, cpu_expected);
	}

	write_arg.val = (read_arg.val ^ 0x1);
	ret = ioctl(fd, 1, (unsigned long)&write_arg);
	if (ret) {
		printf("[INTERR] ioctl write returned %d\n",
		       errno);
		ret = -errno;
		goto out;
	}

	ret = ioctl(fd, 0, (unsigned long)&read_arg);
	if (ret) {
		printf("[INTERR] ioctl 2nd read returned %d\n",
		       errno);
		ret = -errno;
		goto out;
	}

	if (read_arg.val == write_arg.val) {
		printf("[  OK  ] ");
	} else {
		printf("[  NG  ] ");
		fail++;
	}
	printf("read-modify-write\n");

	ret = fail == 0 ? 0 : 1;
 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}
