#define _GNU_SOURCE
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

int main(int argc, char **argv)
{
	int ret, i, opt;
	char *fn_in = NULL, *fn_out = NULL;
	int message = 1;
	int fd_in = -1, fd_out = -1;
	void *test_array = NULL;
	unsigned long size;

	while ((opt = getopt(argc, argv, "i:o:s:")) != -1) {
		switch (opt) {
		case 'i':
			fn_in = optarg;
			break;
		case 'o':
			fn_out = optarg;
			break;
		case 's': /* MiB */
			size = atol(optarg) << 20;
			break;
		default:
			printf("unknown option %c\n", optopt);
			ret = -EINVAL;
			goto out;
		}
	}

	fd_in = open(fn_in, O_RDWR);
	if (fd_in == -1) {
		int errno_save = errno;

		printf("%s:%d open %s returned %d\n",
		       __FILE__, __LINE__, fn_in, errno);
		ret = -errno_save;
		goto out;
	}

	fd_out = open(fn_out, O_RDWR);
	if (fd_out == -1) {
		int errno_save = errno;

		printf("%s:%d open %s returned %d\n",
		       __FILE__, __LINE__, fn_out, errno);
		ret = -errno_save;
		goto out;
	}

	/* Let parent take stat */
	ret = write(fd_out, &message, sizeof(int));
	if (ret != sizeof(int)) {
		int errno_save = errno;

		printf("%s: write returned %d, errno: %d\n",
		       __FILE__, ret, errno);
		ret = ret >= 0 ? ret : -errno_save;
		goto sync_out;
	}

	/* Wait until parent takes reference stat */
	ret = read(fd_in, &message, sizeof(int));
	if (ret != sizeof(int)) {
		int errno_save = errno;

		printf("%s: read returned %d, errno: %d\n",
		       __FILE__, ret, errno);
		ret = ret >= 0 ? ret : -errno_save;
		goto sync_out;
	}
	
	test_array = mmap(0, size,
			  PROT_READ | PROT_WRITE,
			  MAP_ANONYMOUS | MAP_PRIVATE,
			  -1, 0);
	if (test_array == MAP_FAILED) {
		int errno_save = errno;
		
		printf("%s: error: mmap %ld bytes failed\n",
		       __FILE__, size);
		ret = -errno_save;
		goto sync_out;
	}
	memset(test_array, 0xaa, size);

sync_out:
	/* Let parent take stat */
	ret = write(fd_out, &message, sizeof(int));
	if (ret != sizeof(int)) {
		int errno_save = errno;

		printf("%s: write returned %d, errno: %d\n",
		       __FILE__, ret, errno);
		ret = ret >= 0 ? ret : -errno_save;
		goto out;
	}

	/* Wait until parent takes usage stat */
	ret = read(fd_in, &message, sizeof(int));
	if (ret != sizeof(int)) {
		int errno_save = errno;

		printf("%s: read returned %d, errno: %d\n",
		       __FILE__, ret, errno);
		ret = ret >= 0 ? ret : -errno_save;
		goto out;
	}

	ret = 0;

out:
	free(test_array);
	return ret;
}
