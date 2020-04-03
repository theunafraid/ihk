#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "mckernel.h"

/* double precision scalar add */
#define fadd10			\
	"fadd d0, d0, d0\n"	\
	"faddd d0, d0, d0\n"	\
	"faddd d0, d0, d0\n"	\
	"faddd d0, d0, d0\n"	\
	"faddd d0, d0, d0\n"	\
	"faddd d0, d0, d0\n"	\
	"faddd d0, d0, d0\n"	\
	"faddd d0, d0, d0\n"	\
	"faddd d0, d0, d0\n"	\
	"faddd d0, d0, d0\n"

#define fadd100 \
	fadd10 fadd10 fadd10 fadd10 fadd10 \
	fadd10 fadd10 fadd10 fadd10 fadd10

#define fadd1000 \
	fadd100 fadd100 fadd100 fadd100 fadd100 \
	fadd100 fadd100 fadd100 fadd100 fadd100

#define fadd1000000 do {		\
	int i;				\
					\
	for (i = 0; i < 1000; i++) {	\
		asm volatile(		\
			     fadd1000	\
			     :		\
			     :		\
			     :  "q0");	\
	}				\
} while (0)

/* x = a * b + c
   taken from https://developer.arm.com/docs/100891/0606/coding-considerations/embedding-sve-assembly-code-directly-into-c-and-c-code
 */
unsigned long fmla(double *x, double *a, double *b, double *c, unsigned long n)
{
	unsigned long i;

	asm (
	     "whilelo p0.d, %[i], %[n]         \n"
	     "1:                                           \n"
	     "ld1d z0.d, p0/z, [%[a], %[i], lsl #3]  \n"
	     "ld1d z1.d, p0/z, [%[b], %[i], lsl #3]  \n"
	     "ld1d z2.d, p0/z, [%[c], %[i], lsl #3]  \n"
	     "fmla z2.d, p0/m, z0.d, z1.d            \n"
	     "st1d z2.d, p0, [%[x], %[i], lsl #3]    \n"
	     "uqincd %[i]                            \n"
	     "whilelo p0.d, %[i], %[n]               \n"
	     "b.any 1b"
	     : [i] "=&r" (i)
	     : "[i]" (0),
	       [x] "r" (x),
	       [a] "r" (a),
	       [b] "r" (b),
	       [c] "r" (c),
	       [n] "r" (n)
	     : "memory", "cc", "p0", "z0", "z1", "z2");

	return i;
}

#define NDOUBLES (1UL << 17)
#define SZSWEEP (4UL << 30)

int wfe(void)
{
	int ret;
	pid_t pid;

	pid = fork();
	if (pid == 0) {
		asm("wfe" : : :);
		exit(0);
	} else {
		int wstatus, status;

		nop1000000;
		nop1000000;

		asm("sev" : : :);

		ret = waitpid(pid, &wstatus, 0);
		if (ret < 0) {
			int errno_save = errno;

			printf("%s:%s waitpid: errno: %d\n",
			       __FILE__, __LINE__, errno_save);
			ret = -errno_save;
			goto out;
		}

		status = WEXITSTATUS(wstatus);
		printf("[ INFO ] %s: child process exited with status %d\n",
		       __FILE__, status);
		if (status != 0) {
			ret = status;
			goto out;
		}
	}

	ret = 0;
 out:
	return ret;
}

#define MYMMAP(buf, size) do { 			\
	buf = mmap(0, size, \
		 PROT_READ | PROT_WRITE,	     \
		 MAP_ANONYMOUS | MAP_PRIVATE,	     \
		 -1, 0);			     \
	if (buf == MAP_FAILED) { \
		printf("%s: error: allocating memory\n",  \
		       __FILE__); \
		ret = -ENOMEM; \
		goto out; \
	} \
} while (0)

int main(int argc, char **argv)
{
	int ret, i, opt;
	char *fn_in = NULL, *fn_out = NULL;
	int message = 1;
	int fd_in = -1, fd_out = -1;
	double *x, *a, *b, *c;
	char *mem;

	while ((opt = getopt(argc, argv, "i:o:s:")) != -1) {
		switch (opt) {
		case 'i':
			fn_in = optarg;
			break;
		case 'o':
			fn_out = optarg;
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

	/* prepare memory */
	MYMMAP(x, NDOUBLES * sizeof(double));
	MYMMAP(a, NDOUBLES * sizeof(double));
	MYMMAP(b, NDOUBLES * sizeof(double));
	MYMMAP(c, NDOUBLES * sizeof(double));

	mem = mmap(0, SZSWEEP,
		   PROT_READ | PROT_WRITE,
		   MAP_ANONYMOUS | MAP_PRIVATE,
		   -1, 0);
	if (mem == MAP_FAILED) {
		printf("%s: error: allocating memory\n",
		       __FILE__);
		ret = -ENOMEM;
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

	/* 4 MiB read and write */
	for (i = 0; i < NDOUBLES; i++) {
		a[i] = drand48();
		b[i] = drand48();
		c[i] = drand48();
	}

	/* 128 thousand SVE mutliply-adds */
	fmla(x, a, b, c, NDOUBLES);

	/* 4 million double precision scalar adds */
	fadd1000000;
	fadd1000000;
	fadd1000000;
	fadd1000000;

	/* 2 million wfe stall cycles */
	wfe();

	/* # of read transactions:
	 *    4 GiB / 8 MiB * (# of cache blocks)
	 * # of write transactions:
	 *    (4 GiB / 8 MiB - 1) * (# of cache blocks)
	 */
	for (i = 0; i < SZSWEEP; i++) {
		mem[i] = lrand48() & 255;
	}

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
	munmap(x, NDOUBLES * sizeof(double));
	munmap(a, NDOUBLES * sizeof(double));
	munmap(b, NDOUBLES * sizeof(double));
	munmap(c, NDOUBLES * sizeof(double));
	munmap(mem, SZSWEEP);

	return ret;
}
