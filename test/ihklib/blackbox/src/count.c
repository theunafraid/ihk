#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#define MAX_COUNT 10

int main(int argc, char **argv)
{
	int ret;
	int fd = -1;
	int message;
	int i;

	fd = open(argv[1], O_RDWR);
	if (fd == -1) {
		int errno_save = errno;

		printf("%s: open returned %d\n", __FILE__, errno);
		ret = -errno_save;
		goto out;
	}

	/* Wait until parent gets ready */
	ret = read(fd, &message, sizeof(int));
	if (ret == -1) {
		int errno_save = errno;

		printf("%s: read returned %d\n", __FILE__, errno);
		ret = -errno_save;
		goto out;
	}
	if (ret == 0) {
		printf("%s: EOF detected %d\n", __FILE__, errno);
		ret = -EINVAL;
		goto out;
	}

	printf("[ INFO ] count: start sending message...\n", i);

	for (i = 0; i < MAX_COUNT; i++) {
		printf("[ INFO ] count: sending message #%d\n", i);

		ret = write(fd, &message, sizeof(int));
		if (ret == -1) {
			int errno_save = errno;

			printf("%s: read returned %d\n", __FILE__, errno);
			ret = -errno_save;
			goto out;
		}

		usleep(1000000);
	}

	ret = 0;
 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}
