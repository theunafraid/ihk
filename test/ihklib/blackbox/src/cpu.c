#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/mman.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "cpu.h"


int cpus_init(struct cpus *cpus, int ncpus)
{
	int ret;

	if (ncpus == 0) {
		ret = 0;
		goto out;
	}

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

int cpus_copy(struct cpus *dst, struct cpus *src)
{
	int ret;

	if (dst->cpus) {
		dst->cpus = mremap(dst->cpus,
				   sizeof(int) * dst->ncpus,
				   sizeof(int) * src->ncpus,
				   MREMAP_MAYMOVE);
		dst->ncpus = src->ncpus;
	} else {
		ret = cpus_init(dst, src->ncpus);
		if (ret) {
			goto out;
		}
	}

	memcpy(dst->cpus, src->cpus, sizeof(int) * src->ncpus);

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
	if (fp == NULL) {
		ret = -errno;
		goto out;
	}

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
		if (ret == -1)
			break;

		if (ret != 1)
			continue;

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
	if (fp) {
		fclose(fp);
	}
	return ret;
}

int cpus_max_id(struct cpus *cpus)
{
	int i;
	int max = INT_MIN;

	for (i = 0; i < cpus->ncpus; i++) {
		if (cpus->cpus[i] > max) {
			max = cpus->cpus[i];
		}
	}

	return max;
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

int cpus_pop(struct cpus *cpus, int n)
{
	int ret;

	if (cpus->ncpus < n || cpus->cpus == NULL) {
		ret = 1;
		goto out;
	}

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

int cpus_shift(struct cpus *cpus, int n)
{
	int ret;

	if (cpus->ncpus < n || cpus->cpus == NULL) {
		ret = -EINVAL;
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

	if (result == NULL && expected == NULL) {
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

int cpus_check_reserved(struct cpus *expected)
{
	int ret;
	struct cpus cpus = { 0 };

	ret = ihk_get_num_reserved_cpus(0);
	INTERR(ret < 0, "ihk_get_num_reserved_cpus returned %d\n",
	       ret);
	INFO("# of reserved cpus: %d\n", ret);

	if (ret > 0) {
		ret = cpus_init(&cpus, ret);
		INTERR(ret, "cpus_init returned %d\n", ret);

		ret = ihk_query_cpu(0, cpus.cpus, cpus.ncpus);
		INTERR(ret < 0, "ihk_query_cpu returned %d\n",
		       ret);
	}

	ret = cpus_compare(&cpus, expected);
	if (ret) {
		INFO("actual reservation:\n");
		cpus_dump(&cpus);
		INFO("expected reservation:\n");
		cpus_dump(expected);
	}

 out:
	return ret;
}

int cpus_reserved(struct cpus *cpus)
{
	int ret;

	ret = ihk_get_num_reserved_cpus(0);
	INTERR(ret < 0, "ihk_get_num_reserved_cpus returned %d\n",
	       ret);
	INFO("# of reserved cpus: %d\n", ret);

	if (ret > 0) {
		ret = cpus_init(cpus, ret);
		INTERR(ret, "cpus_init returned %d\n", ret);

		ret = ihk_query_cpu(0, cpus->cpus, cpus->ncpus);
		INTERR(ret < 0, "ihk_query_cpu returned %d\n",
		       ret);
	}

	ret = 0;
 out:
	return ret;
}

int cpus_check_assigned(struct cpus *expected)
{
	int ret;
	struct cpus cpus = { 0 };

	ret = ihk_os_get_num_assigned_cpus(0);
	INTERR(ret < 0, "ihk_os_get_num_assigned_cpus returned %d\n",
	       ret);
	INFO("# of assigned cpus: %d\n", ret);

	if (ret > 0) {
		ret = cpus_init(&cpus, ret);
		INTERR(ret, "cpus_init returned %d\n", ret);

		ret = ihk_os_query_cpu(0, cpus.cpus, cpus.ncpus);
		INTERR(ret < 0, "ihk_os_query_cpu returned %d\n",
		       ret);
	}

	ret = cpus_compare(&cpus, expected);
	if (ret) {
		INFO("actual reservation:\n");
		cpus_dump(&cpus);
		INFO("expected reservation:\n");
		cpus_dump(expected);
	}

 out:
	return ret;
}

int cpus_reserve(void)
{
	int ret;
	struct cpus cpus = { 0 };

	ret = cpus_ls(&cpus);
	INTERR(ret, "cpus_ls returned %d\n", ret);

	ret = cpus_shift(&cpus, 2);
	INTERR(ret, "cpus_shift returned %d\n", ret);

	ret = ihk_reserve_cpu(0, cpus.cpus, cpus.ncpus);
	INTERR(ret, "ihk_reserve_cpu returned %d\n", ret);

	ret = 0;
 out:
	return ret;
}

int cpus_release(void)
{
	int ret;
	struct cpus cpus = { 0 };

	ret = ihk_os_get_num_assigned_cpus(0);
	INTERR(ret < 0, "ihk_os_get_num_assigned_cpus returned %d\n",
	       ret);

	if (ret > 0) {
		ret = cpus_init(&cpus, ret);
		INTERR(ret, "cpus_init returned %d\n", ret);

		ret = ihk_os_query_cpu(0, cpus.cpus, cpus.ncpus);
		INTERR(ret, "ihk_os_query_cpu returned %d\n",
		       ret);
	}

	ret = ihk_release_cpu(0, cpus.cpus, cpus.ncpus);
	INTERR(ret, "ihk_os_release_cpu returned %d\n", ret);

	ret = 0;
 out:
	return ret;
}

/* lscpu and shift */
int cpus_os_assign(void)
{
	int ret;
	struct cpus cpus = {0};

	ret = cpus_reserved(&cpus);
	INTERR(ret, "cpus_reserved returned %d\n", ret);

	ret = ihk_os_assign_cpu(0, cpus.cpus, ret);
	INTERR(ret, "ihk_os_assign_cpu returned %d\n", ret);

	ret = 0;
out:
	return ret;
}

/* query and release */
int cpus_os_release(void)
{
	int ret;
	struct cpus cpus = {0};

	ret = ihk_os_get_num_assigned_cpus(0);
	INTERR(ret < 0, "ihk_os_get_num_assigned_cpus returned %d\n",
	       ret);

	if (ret > 0) {
		ret = cpus_init(&cpus, ret);
		INTERR(ret, "cpus_init returned %d\n", ret);

		ret = ihk_os_query_cpu(0, cpus.cpus, cpus.ncpus);
		INTERR(ret, "ihk_os_query_cpu returned %d\n",
		       ret);

		ret = ihk_os_release_cpu(0, cpus.cpus, cpus.ncpus);
		INTERR(ret, "ihk_os_release_cpu returned %d\n",
		       ret);
	}

	ret = 0;
out:
	return ret;
}
