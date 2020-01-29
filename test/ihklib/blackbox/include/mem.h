#ifndef __MEM_H_INCLUDED__
#define __MEM_H_INCLUDED__

#include <ihklib.h>

#define MAX_NUM_MEM_CHUNKS 1024

struct mems {
	struct ihk_mem_chunk *mem_chunks;
	int num_mem_chunks;
};

int mems_init(struct mems *mems, int num_mem_chunks);
int mems_copy(struct mems *dst, struct mems *src);
int mems_ls(struct mems *mems, char *type, double ratio);
int mems_push(struct mems *mems, unsigned long size, int numa_node_number);
int mems_pop(struct mems *mems, int n);
int mems_shift(struct mems *mems, int n);
void mems_fill(struct mems *mems, unsigned long size);
void mems_dump(struct mems *mems);
void mems_dump_sum(struct mems *mems);
int mems_compare(struct mems *result, struct mems *expected, struct mems *margin);
int mems_check_reserved(struct mems *expected, struct mems *margin);
int mems_query_and_release(void);

#endif
