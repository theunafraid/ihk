#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define MAX_NUM_CHUNKS (1UL<<10)

enum ihk_os_pgsize {
	IHK_OS_PGSIZE_4KB,
	IHK_OS_PGSIZE_64KB,
	IHK_OS_PGSIZE_2MB,
	IHK_OS_PGSIZE_32MB,
	IHK_OS_PGSIZE_1GB,
	IHK_OS_PGSIZE_16GB,
	IHK_OS_PGSIZE_512MB,
	IHK_OS_PGSIZE_4TB,
	IHK_MAX_NUM_PGSIZES
};

int main(int argc, char **argv)
{
	int ret;
	int i;
	char *mem[IHK_MAX_NUM_PGSIZES][MAX_NUM_CHUNKS];
	int message;
	int fd_in, fd_out;

	fd_in = open(argv[1], O_RDWR);
	if (fd_in == -1) {
		int errno_save = errno;

		printf("%s: open returned %d\n", __FILE__, errno);
		ret = -errno_save;
		goto out;
	}

	fd_out = open(argv[2], O_RDWR);
	if (fd_out == -1) {
		int errno_save = errno;

		printf("%s: open returned %d\n", __FILE__, errno);
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
		goto out;
	}

	/* Wait until parent takes reference stat */
	ret = read(fd_in, &message, sizeof(int));
	if (ret != sizeof(int)) {
		int errno_save = errno;

		printf("%s: read returned %d, errno: %d\n",
		       __FILE__, ret, errno);
		ret = ret >= 0 ? ret : -errno_save;
		goto out;
	}

	system("printf '[ INFO ] '; "
	       "grep MemUsed /sys/devices/system/node/node0/meminfo");

	/* 64 KiB * 1024 */
	for (i = 0; i < (1UL << 10); i++) {
		mem[IHK_OS_PGSIZE_64KB][i] = mmap(0,
						  PAGE_SIZE,
						  PROT_READ | PROT_WRITE,
						  MAP_ANONYMOUS | MAP_PRIVATE,
						  -1, 0);
		if (mem[IHK_OS_PGSIZE_64KB][i] == MAP_FAILED) {
			ret = -errno;
			goto out;
		}
		memset(mem[IHK_OS_PGSIZE_64KB][i], 0xff, PAGE_SIZE);
	}

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

	system("printf '[ INFO ] '; "
	       "grep MemUsed /sys/devices/system/node/node0/meminfo");

	ret = 0;
 out:
	return ret;
}
