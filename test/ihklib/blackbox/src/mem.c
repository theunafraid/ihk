#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "util.h"
#include "okng.h"
#include "mem.h"


int mems_init(struct mems *mems, int num_mem_chunks)
{
	int ret;
	mems->mem_chunks = mmap(0,
				sizeof(struct ihk_mem_chunk) * num_mem_chunks,
			  PROT_READ | PROT_WRITE,
			  MAP_ANONYMOUS | MAP_PRIVATE,
			  -1, 0);
	if (mems->mem_chunks == MAP_FAILED) {
		ret = -errno;
		goto out;
	}

	mems->num_mem_chunks = num_mem_chunks;
	ret = 0;
 out:
	return ret;
}

int mems_copy(struct mems *dst, struct mems *src)
{
	int ret;

	if (dst->mem_chunks) {
		dst->mem_chunks = mremap(dst->mem_chunks,
					 sizeof(struct ihk_mem_chunk) *
					 dst->num_mem_chunks,
					 sizeof(struct ihk_mem_chunk) *
					 src->num_mem_chunks,
					 MREMAP_MAYMOVE);
		dst->num_mem_chunks = src->num_mem_chunks;
	} else {
		ret = mems_init(dst, src->num_mem_chunks);
		if (ret) {
			goto out;
		}
	}
	
	memcpy(dst->mem_chunks, src->mem_chunks,
	       sizeof(struct ihk_mem_chunk) * src->num_mem_chunks);

	ret = 0;
 out:
	return ret;
}

/* type: "MemTotal" or "MemFree" */
int mems_ls(struct mems *mems, char *type, double ratio)
{
	int ret;
	DIR *dp = NULL;
	FILE *fp = NULL;
	struct dirent *entp;
	int max_numa_node_number = -1;
	
	if (mems->mem_chunks == NULL) {
		ret = mems_init(mems, MAX_NUM_MEM_CHUNKS);
		if (ret != 0) {
			goto out;
		}
	}

	dp = opendir("/sys/devices/system/node/");
	if (dp == NULL) {
		ret = -errno;
		goto out;
	}
	
	entp = readdir(dp);
	while (entp) {
		char cmd[4096];
		unsigned long memfree;
		int numa_node_number;

		ret = strncmp(entp->d_name,"node",4);
		if (ret) {
			goto next;
		}

		numa_node_number = atoi(entp->d_name + 4);
		printf("%s: numa_node_number: %d\n",
		       __func__, numa_node_number);

		if (numa_node_number > max_numa_node_number) {
			max_numa_node_number = numa_node_number;
		}
		
		sprintf(cmd, "grep %s /sys/devices/system/node/%s/meminfo | awk '{ print $4; }'",
			type, entp->d_name);

		fp = popen(cmd, "r");
		if (fp == NULL) {
			ret = -errno;
			goto out;
		}

		ret = fscanf(fp, "%ld kb", &memfree);
		printf("%s: memfree: %ld\n", __func__, memfree);
		if (ret == EOF) {
			ret = -errno;
			goto out;
		}

		fclose(fp);
		fp = NULL;

#define RESERVE_MEM_GRANULE (1024UL * 1024 * 4)
		mems->mem_chunks[numa_node_number].size =
			((unsigned long)(memfree * 1024 * ratio) &
			 ~(RESERVE_MEM_GRANULE - 1));
		mems->mem_chunks[numa_node_number].numa_node_number =
			numa_node_number;
	next:
		entp = readdir(dp);
	}
	mems->mem_chunks = mremap(mems->mem_chunks,
				  sizeof(struct ihk_mem_chunk) *
				  mems->num_mem_chunks,
				  sizeof(struct ihk_mem_chunk) *
				  max_numa_node_number + 1,
				  MREMAP_MAYMOVE);
	if (mems->mem_chunks == MAP_FAILED) {
		int errno_save = errno;

		printf("%s: mremap returned %d\n", __func__, errno);
		ret = -errno;
		goto out;
	}

 	mems->num_mem_chunks = max_numa_node_number + 1;
	ret = 0;
 out:
	if (fp) {
		fclose(fp);
	}
	
	if (dp) {
		closedir(dp);
	}

	return ret;
}

int mems_push(struct mems *mems, unsigned long size, int numa_node_number)
{
	int ret;
	
	if (mems->mem_chunks == NULL) {
		ret = mems_init(mems, 1);
		if (ret != 0) {
			goto out;
		}
	} else {
		mems->mem_chunks = mremap(mems->mem_chunks,
					  sizeof(struct ihk_mem_chunk) *
					  mems->num_mem_chunks,
					  sizeof(struct ihk_mem_chunk) *
					  (mems->num_mem_chunks + 1),
					  MREMAP_MAYMOVE);
		if (mems->mem_chunks == MAP_FAILED) {
			ret = -errno;
			goto out;
		}
	}
	
	mems->mem_chunks[mems->num_mem_chunks].size = size;
	mems->mem_chunks[mems->num_mem_chunks].numa_node_number =
		numa_node_number;
	mems->num_mem_chunks++;

	ret = 0;
 out:
	return ret;
}

int mems_pop(struct mems *mems, int n)
{
	int ret;
	
	if (mems->num_mem_chunks < n || mems->mem_chunks == NULL) {
		ret = 1;
		goto out;
	}
	
	mems->mem_chunks = mremap(mems->mem_chunks,
				  sizeof(struct ihk_mem_chunk) *
				  mems->num_mem_chunks,
				  sizeof(struct ihk_mem_chunk) *
				  (mems->num_mem_chunks - n),
				  MREMAP_MAYMOVE);
	if (mems->mem_chunks == MAP_FAILED) {
		ret = -errno;
		goto out;
	}

	mems->num_mem_chunks -= n;

	ret = 0;
 out:
	return ret;
}

int mems_shift(struct mems *mems, int n)
{
	int ret;
	
	if (mems->num_mem_chunks < n || mems->mem_chunks == NULL) {
		ret = 1;
		goto out;
	}

	if (mems->num_mem_chunks == n) {
		ret = munmap(mems->mem_chunks,
			     sizeof(struct ihk_mem_chunk) *
			     mems->num_mem_chunks);
		if (ret) {
			ret = -errno;
			goto out;
		}
		mems->mem_chunks = NULL;
		mems->num_mem_chunks = 0;
		ret = 0;
		goto out;
	}
	
	memmove(mems->mem_chunks, mems->mem_chunks + n,
		sizeof(struct ihk_mem_chunk) * (mems->num_mem_chunks - n));
	
	mems->mem_chunks = mremap(mems->mem_chunks,
				  sizeof(struct ihk_mem_chunk) *
				  mems->num_mem_chunks,
				  sizeof(struct ihk_mem_chunk) *
				  (mems->num_mem_chunks - n),
				  MREMAP_MAYMOVE);
	if (mems->mem_chunks == MAP_FAILED) {
		ret = -errno;
		goto out;
	}
	
	mems->num_mem_chunks -= n;

	ret = 0;
 out:
	return ret;
}

void mems_dump(struct mems *mems)
{
	int i;
	
	INFO("num_mem_chunks: %d\n", mems->num_mem_chunks);

	if (mems->mem_chunks == NULL) {
		INFO("mems->mem_chunks is NULL\n");
		return;
	}
	
	for (i = 0; i < mems->num_mem_chunks; i++) {
		INFO("mem_chunks[%d]: size: %ld, numa_node_number: %d\n",
		     i, mems->mem_chunks[i].size,
		     mems->mem_chunks[i].numa_node_number);
	}
}

static void mems_sum(struct mems *mems, struct ihk_mem_chunk *sum)
{
	int i;

	memset(sum, 0, sizeof(struct ihk_mem_chunk) * MAX_NUM_MEM_CHUNKS);

	for (i = 0; i < mems->num_mem_chunks; i++) {
		sum[mems->mem_chunks[i].numa_node_number].size +=
			mems->mem_chunks[i].size;
	}
}

void mems_dump_sum(struct mems *mems) {
	int i;
	struct ihk_mem_chunk sum[MAX_NUM_MEM_CHUNKS] = { 0 };

	mems_sum(mems, sum);
	
	for (i = 0; i < MAX_NUM_MEM_CHUNKS; i++) {
		if (sum[i].size) {
			INFO("size: %ld, numa_node_number: %d\n",
			     sum[i].size,
			     sum[i].numa_node_number);
		}
	}
}

int mems_compare(struct mems *result, struct mems *expected)
{
	int i;
	struct ihk_mem_chunk sum_result[MAX_NUM_MEM_CHUNKS] = { 0 };
	struct ihk_mem_chunk sum_expected[MAX_NUM_MEM_CHUNKS] = { 0 };
	
	if (expected == NULL) {
		return 0;
	}

	mems_sum(result, sum_result);
	mems_sum(expected, sum_expected);

	for (i = 0; i < MAX_NUM_MEM_CHUNKS; i++) {
		if (sum_result[i].size != sum_expected[i].size) {
			return 1;
		}
	}

	return 0;
}

int mems_check_reserved(struct mems *expected)
{
	int ret;
	struct mems mems;
	
	ret = ihk_get_num_reserved_mem_chunks(0);
	INTERR(ret < 0, "ihk_get_num_reserved_mem_chunks returned %d\n",
	       ret);
	
	ret = mems_init(&mems, ret);
	INTERR(ret != 0, "cpus_init returned %d\n", ret);
	
	ret = ihk_query_mem(0, mems.mem_chunks, mems.num_mem_chunks);
	INTERR(ret < 0, "ihk_query_cpu returned %d\n",
	       ret);
		
	ret = mems_compare(&mems, expected);
	if (ret) {
		INFO("actual reservation:\n");
		mems_dump_sum(&mems);
		INFO("expected reservation:\n");
		mems_dump_sum(expected);
	}

 out:
	return ret;
}
