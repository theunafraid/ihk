#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "mckernel.h"

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
	int ret;
	double *x, *a, *b, *c;
	int i;
	char *mem;

	/* 4 MiB read and write */
	MYMMAP(x, NDOUBLES * sizeof(double));
	MYMMAP(a, NDOUBLES * sizeof(double));
	MYMMAP(b, NDOUBLES * sizeof(double));
	MYMMAP(c, NDOUBLES * sizeof(double));

	for (i = 0; i < NDOUBLES; i++) {
		a[i] = drand48();
		b[i] = drand48();
		c[i] = drand48();
	}
	
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

	/* 128 thousand SVE mutliply-adds */
	fmla(x, a, b, c, NDOUBLES);

	/* 4 million NEON 32-bit integer adds */
	//vadd1000000;

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

	ret = 0;
 out:
	return ret;
}
