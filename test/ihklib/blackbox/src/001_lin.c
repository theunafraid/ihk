#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "cpu.h"
#include "params.h"
#include "init_fini.h"

#define DEBUG

int main(int argc, char **argv)
{
	int ret, status;
	FILE *fp, *fp1, *fp2;
	char buf[65536], buf1[65536], buf2[65536];
	size_t nread;

	char cmd[1024];
	char fn[256];
	char kargs[256];

	int cpus[4];
	int num_cpus;

	struct ihk_mem_chunk mem_chunks[4];
	int num_mem_chunks;
	int indices[2];
	int num_os_instances;
	ssize_t kmsg_size;
	struct ihk_ikc_cpu_map ikc_map[2];
	int num_numa_nodes;
	unsigned long memfree[4];
	int num_pgsizes;
	long pgsizes[3];
	struct ihk_os_rusage rusage;
	char *retstr;
	int boot_shutdown = 0;
	int mcexec_shutdown = 0;
	int ikc_map_by_func = 0;

	int i;
	
	params_getopt(argc, argv);

	
	exit(0);
	
	// get # of reserved cpus
	num_cpus = ihk_get_num_reserved_cpus(0);
	//printf("num_cpus=%d\n", num_cpus);
	OKNG(num_cpus < 0, "ihk_get_num_reserved_cpu (1)\n");

	// get reserved cpus
	ret = ihk_query_cpu(0, cpus, 1);
	OKNG(ret != 0, "ihk_query_cpu (1)\n");

	// release cpu
	cpus[0] = 1;
	num_cpus = 1;
	ret = ihk_release_cpu(0, cpus, num_cpus);
	OKNG(ret != 0, "ihk_release_cpu (1)\n");

	/* Expected to succeed */
	ret = insmod(params.uid, params.gid);
	NG(ret == 0, "insmod returned %d\n", ret);

	// reserve cpu
	cpus[0] = 3;
	cpus[1] = 1;
	num_cpus = 2;
	ret = ihk_reserve_cpu(0, cpus, num_cpus);
	OKNG(ret == 0, "ihk_reserve_cpu\n");

	// get # of reserved cpus
	num_cpus = ihk_get_num_reserved_cpus(0);
	OKNG(num_cpus == 2, "ihk_get_num_reserved_cpu (2)\n");

	// get reserved cpus. Note that cpu# is sorted in ihk.
	ret = ihk_query_cpu(0, cpus, num_cpus);
	OKNG(ret == 0 &&
	     cpus[0] == 1 &&
	     cpus[1] == 3, "ihk_query_cpu (2)\n");

	// release cpu
	cpus[0] = 1;
	num_cpus = 1;
	ret = ihk_release_cpu(0, cpus, num_cpus);
	OKNG(ret == 0, "ihk_release_cpu (2)\n");

	// get # of reserved cpus
	num_cpus = ihk_get_num_reserved_cpus(0);
	OKNG(num_cpus == 1, "ihk_get_num_reserved_cpu (3)\n");

	// get reserved cpus
	ret = ihk_query_cpu(0, cpus, num_cpus);
	OKNG(ret == 0 &&
	     cpus[0] == 3, "ihk_query_cpu (3)\n");

	// reserve cpu
	cpus[0] = 1;
	num_cpus = 1;
	ret = ihk_reserve_cpu(0, cpus, num_cpus);
	OKNG(ret == 0, "ihk_reserve_cpu\n");

	// get # of reserved cpus
	num_cpus = ihk_get_num_reserved_cpus(0);
	OKNG(num_cpus == 2, "ihk_get_num_reserved_cpu (3)\n");

	// get reserved cpus. Note that cpu# is sorted in ihk.
	ret = ihk_query_cpu(0, cpus, num_cpus);
	OKNG(ret == 0 &&
	     cpus[0] == 1 &&
	     cpus[1] == 3, "ihk_query_cpu (4)\n");

#if 1
	// shutdown
	// usleep(250*1000); // Wait for nothing is in-flight
 shutdown:
	ret = ihk_os_shutdown(0);
	OKNG(ret == 0, "ihk_os_shutdown (2)\n");

	// get status. Note that the smp_ihk_os_shutdown() transitions
	// smp-x86 status to BUILTIN_OS_STATUS_SHUTDOWN
	// and smp_ihk_os_query_status() transitions os status to
	// IHK_OS_STATUS_NOT_BOOTED.
	ret = ihk_os_get_status(0);
	OKNG(ret == IHK_STATUS_SHUTDOWN ||
	     ret == IHK_STATUS_INACTIVE,
	     "ihk_os_get_status (5) returned %d\n", ret);

	// get status
	sprintf(cmd, "%s/sbin/ihkosctl 0 get status 2>&1", QUOTE(WITH_MCK));
	fp = popen(cmd, "r");
	nread = fread(buf, 1, sizeof(buf), fp);
	buf[nread] = 0;
	OKNG(strstr(buf, "SHUTDOWN") != NULL ||
	     strstr(buf, "INACTIVE") != NULL,
	     "ihkconfig 0 get status (5) returned:\n%s\n", buf);
#endif

	// destroy os
	usleep(250*1000); // Wait for nothing is in-flight
	ret = ihk_destroy_os(0, 0);
	OKNG(ret == 0, "ihk_destroy_os (4)\n");

	// get # of OS instances
	num_os_instances = ihk_get_num_os_instances(0);
	OKNG(num_os_instances == 0, "ihk_get_num_os_instances (3)\n");

	// get OS instances
	ret = ihk_get_os_instances(0, indices, num_os_instances);
	OKNG(ret == 0, "ihk_get_os_instances (3)\n");

	// get os_instances
	sprintf(cmd, "%s/sbin/ihkconfig 0 get os_instances", QUOTE(WITH_MCK));
	fp = popen(cmd, "r");
	nread = fread(buf, 1, sizeof(buf), fp);
	buf[nread] = 0;
	OKNG(strstr(buf, "0") == NULL,
	     "ihkconfig 0 get os_instances (4) returned:\n%s\n", buf);


	ret = rmmod(0);
	NG(ret == 0, "rmmod returned %d\n", ret);
	
	printf("[INFO] All tests finished\n");
	ret = 0;

 out:
	return ret;
}
