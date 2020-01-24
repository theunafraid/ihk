#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <ihk/ihklib.h>
#include "util.h"
#include "okng.h"
#include "init_fini.h"
#include "input_vector.h"

#define DEBUG

static int query_and_check(struct cpu_expected *expected)
{
	int ret;
	struct cpus cpus;
	int ncpus;
	
	ret = ihk_get_num_reserved_cpus(0);
	INTERR(ret < 0, "ihk_get_num_reserved_cpus returned %d\n",
	       ret);
	ncpus = ret;
	INFO("# of reserved cpus: %d\n", ncpus);
	
	cpus.cpus = mmap(0, MAX_NUM_CPUS * sizeof(int),
			 PROT_READ | PROT_WRITE,
			 MAP_ANONYMOUS | MAP_PRIVATE,
			 -1, 0);
	INTERR(cpus.cpus == MAP_FAILED,
	       "mmap cpus.cpus failed\n");
	
	ret = ihk_query_cpu(0, cpus.cpus, ncpus);
	INTERR(ret < 0, "ihk_query_cpu returned %d\n",
	       ret);
		
	ret = cpus_compare(&cpus, expected->cpus);
	if (ret) {
		INFO("actual reservation:\n");
		cpus_dump(&cpus);
		INFO("expected reservation:\n");
		cpus_dump(expected->cpus);
	}
	OKNG(ret == 0, "ihk_reserve_cpu: comparing reservation result\n");
}

static int device_existence(void)
{
	/* Generate test vector */
	struct cpus cpu_inputs[2];

	/* Set of lscpu and plus/minus one element */
	for (i = 1; i < 4; i++) { 
		ret = cpus_ls(&cpu_inputs[i]);
		INTERR(ret, "cpus_ls returned %d\n", ret);

		/* Spare two cpus for Linux */
		ret = cpus_unshift(&cpu_inputs[i], 2);
		INTERR(ret, "cpus_unshift returned %d\n", ret);
	}
	
	struct cpu_expected cpu_expected[2] = 
		{
		 [0] =
		 {
		  .ret = -EINVAL,
		  .cpus = NULL, /* don't care */
		 },
		 [1] =
		 {
		  .ret = 0,
		  .cpus = &cpu_inputs[1],
		 },
		};

	/* Call the target */
	struct cpus cpus;
	int ncpus;
	
	for (i = 0; i < 2; i++) {
		ret = ihk_reserve_cpu(0, cpu_inputs[i].cpus, cpu_inputs[i].ncpus);
		OKNG(ret == cpu_expected[i].ret,
		     "ihk_reserve_cpu: return value: %d, expected: %d\n",
		     ret, cpu_expected[i].ret);
		
		if (i == 1) {
			ret = query_and_check(&cpu_expected);
			OKNG(ret == 0, "ihk_reserve_cpu: query_and_check\n");
		}
		
		if (i == 0) {
			ret = insmod(uid, gid);
			NG(ret == 0, "insmod returned %d\n", ret);
		}
	}
}

static int reserve_cpu(void)
{
	/* Generate test vector */
#define CPUS_VLEN 5

	/* Prepare one having NULL and clear others */
	struct cpus cpu_inputs[CPUS_VLEN] = 
		{
		 [0] =
		 {
		  .cpus = NULL,
		  .ncpus = 1,
		 },
		 [2] = { 0 },
		 [3] = { 0 },
		 [4] = { 0 },
		};

	/* All cpus */
	for (i = 1; i < 4; i++) { 
		ret = cpus_ls(&cpu_inputs[i]);
		INTERR(ret, "cpus_ls returned %d\n", ret);
	}

	ret = cpus_push(&cpu_inputs[2]);
	INTERR(ret, "cpus_push returned %d\n", ret);

	ret = cpus_pop(&cpu_inputs[3]);
	INTERR(ret, "cpus_pop returned %d\n", ret);

	for (i = 1; i < 4; i++) {
		/* Spare two cpus for Linux */
		ret = cpus_unshift(&cpu_inputs[i], 2);
		INTERR(ret, "cpus_unshift returned %d\n", ret);
	}
	
	struct cpu_expected cpu_expected[CPUS_VLEN] = 
		{
		 [0] =
		 {
		  .ret = -EFAULT,
		  .cpus = NULL, /* don't care */
		 },
		 [1] =
		 {
		  .ret = 0,
		  .cpus = &cpu_inputs[1],
		 },
		 [2] =
		 {
		  .ret = -EINVAL,
		  .cpus = NULL,
		 },
		 [3] =
		 {
		  .ret = 0,
		  .cpus = &cpu_inputs[3],
		 },
		};

	/* Call the target */
	for (i = 0; i < CPUS_VLEN; i++) {
		if (i == 1) {
			cpus_dump(&cpu_inputs[i]);
		}

		ret = ihk_reserve_cpu(0, cpu_inputs[i].cpus, cpu_inputs[i].ncpus);
		OKNG(ret == cpu_expected[i].ret,
		     "ihk_reserve_cpu: return value: %d, expected: %d\n",
		     ret, cpu_expected[i].ret);

		ret = query_and_check(&cpu_expected[i]);
		OKNG(ret == 0, "ihk_reserve_cpu: query_and_check\n");
	}
}

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
	uid_t uid;
	gid_t gid;
	int opt;

	int i;
	
	while ((opt = getopt(argc, argv, "bxmu:g:")) != -1) {
		switch (opt) {
		case 'b':
			boot_shutdown = 1;
			break;
		case 'x':
			mcexec_shutdown = 1;
			break;
		case 'm':
			ikc_map_by_func = 1;
			break;
		case 'u':
			uid = atoi(optarg);
			break;
		case 'g':
			gid = atoi(optarg);
			break;
		default: /* '?' */
			printf("unknown option %c\n", optopt);
			exit(1);
		}
	}

	/* device file existent / non-existent case */
	ret = device_existence();
	OKNG(ret == 0, "ihk_reserve_cpu: device non-existent case\n");

	
	ret = reserve_cpu();
	NG(ret == 0, "reserve_cpu returned %d\n", ret);
	
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
	ret = insmod(uid, gid);
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

#if 1
	// destroy os
	usleep(250*1000); // Wait for nothing is in-flight
	ret = ihk_destroy_os(0, 0);
	OKNG(ret == 0, "ihk_destroy_os (4)\n");
#else
	// destroy os
	usleep(250*1000); // Wait for nothing is in-flight
	sprintf(cmd, "%s/sbin/ihkconfig 0 destroy 0 2>&1", QUOTE(WITH_MCK));
	fp = popen(cmd, "r");
	nread = fread(buf, 1, sizeof(buf), fp);
	buf[nread] = 0;
	OKNG(strstr(buf, "rror") == NULL,
	     "ihkconfig 0 destroy 0 returned:\n%s\n", buf);
#endif

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


	ret = rmmod();
	NG(ret == 0, "rmmod returned %d\n", ret);
	
	printf("[INFO] All tests finished\n");
	ret = 0;

 out:
	return ret;
}
