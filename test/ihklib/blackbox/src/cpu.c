#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include "util.h"
#include "okng.h"
#include "input_vector.h"


int cpus_init(struct cpus *cpus, int ncpus)
{
	int ret;
	cpus->cpus = mmap(0, sizeof(int) * ncpus,
			  PROT_READ | PROT_WRITE,
			  MAP_ANONYMOUS | MAP_PRIVATE,
			  -1, 0);
	if (cpus->cpus == MAP_FAILED) {
		ret = -errno;
		goto out;
	}

	cpus->ncpus = ncpus;
	ret = 0;
 out:
	return ret;
}

int cpus_ls(struct cpus *cpus)
{
	char cmd[1024];
	FILE *fp;
	int ncpus;
	int ret;
	
	sprintf(cmd, "lscpu -p=cpu --online | awk '!/#/ { print $0; }'");
	//INFO("%s\n", cmd);
	fp = popen(cmd, "r");
	INTERR(fp == NULL, "%s failed\n", cmd);

	if (cpus->cpus == NULL) {
		ret = cpus_init(cpus, MAX_NUM_CPUS);
		if (ret != 0) {
			goto out;
		}
	}

	ncpus = 0;
	do {
		int id;
		
		ret = fscanf(fp, "%d", &id);
		if (ret == -1) {
			break;
			
		}

		if (ret != 1) {
			continue;
		}

		cpus->cpus[ncpus++] = id;
	} while (ret);

	cpus->cpus = mremap(cpus->cpus, sizeof(int) * cpus->ncpus,
			    sizeof(int) * ncpus, MREMAP_MAYMOVE);
	if (cpus->cpus == MAP_FAILED) {
		ret = -errno;
		goto out;
	}
	
	cpus->ncpus = ncpus;
	
	ret = 0;
 out:
	if (!fp) {
		fclose(fp);
	}
	return ret;
}

int cpus_push(struct cpus *cpus, int id)
{
	int ret;
	
	if (cpus->cpus == NULL) {
		ret = cpus_init(cpus, 1);
		if (ret != 0) {
			goto out;
		}
	} else {
		cpus->cpus = mremap(cpus->cpus, sizeof(int) * cpus->ncpus,
				    sizeof(int) * (cpus->ncpus + 1),
				    MREMAP_MAYMOVE);
		if (cpus->cpus == MAP_FAILED) {
			ret = -errno;
			goto out;
		}
	}
	
	cpus->cpus[cpus->ncpus] = id;
	cpus->ncpus++;

	ret = 0;
 out:
	return ret;
}

int cpus_pop(struct cpus *cpus)
{
	int ret;
	
	if (cpus->ncpus == 0 || cpus->cpus == NULL) {
		ret = 1;
		goto out;
	}
	
	cpus->cpus = mremap(cpus->cpus, sizeof(int) * cpus->ncpus,
			    sizeof(int) * (cpus->ncpus - 1),
			    MREMAP_MAYMOVE);
	if (cpus->cpus == MAP_FAILED) {
		ret = -errno;
		goto out;
	}

	cpus->ncpus--;

	ret = 0;
 out:
	return ret;
}

int cpus_shift(struct cpus *cpus, int n)
{
	int ret;
	
	if (cpus->ncpus < n || cpus->cpus == NULL) {
		ret = 1;
		goto out;
	}

	if (cpus->ncpus == n) {
		ret = munmap(cpus->cpus, sizeof(int) * cpus->ncpus);
		if (ret) {
			ret = -errno;
			goto out;
		}
		cpus->cpus = NULL;
		cpus->ncpus = 0;
		ret = 0;
		goto out;
	}
	
	memmove(cpus->cpus, cpus->cpus + n, sizeof(int) * (cpus->ncpus - n));
	
	cpus->cpus = mremap(cpus->cpus, sizeof(int) * cpus->ncpus,
			    sizeof(int) * (cpus->ncpus - n),
			    MREMAP_MAYMOVE);
	if (cpus->cpus == MAP_FAILED) {
		ret = -errno;
		goto out;
	}

	cpus->ncpus -= n;

	ret = 0;
 out:
	return ret;
}


void cpus_dump(struct cpus *cpus)
{
	int i;
	
	INFO("ncpus: %d\n", cpus->ncpus);

	if (cpus->cpus == NULL) {
		INFO("cpus->cpus is NULL\n");
		return;
	}
	
	for (i = 0; i < cpus->ncpus; i++) {
		INFO("cpus[%d]: %d\n", i, cpus->cpus[i]);
	}
}

int cpus_compare(struct cpus *result, struct cpus *expected)
{
	int i;

	if (expected == NULL) {
		return 0;
	}

	if (result->ncpus != expected->ncpus) {
		return 1;
	}
	
	for (i = 0; i < result->ncpus; i++) {
		if (result->cpus[i] != expected->cpus[i]) {
			return 1;
		}
	}
	return 0;
}
