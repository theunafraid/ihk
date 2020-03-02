#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "mem.h"
#include "params.h"
#include "linux.h"

const char param[] =
	"all with different"
	" IHK_RESERVE_MEM_MIN_CHUNK_SIZE values";
const char *values[] = {
	"64 KiB",
	"4 MiB",
};

int main(int argc, char **argv)
{
	int ret;
	int i;

	params_getopt(argc, argv);

	/* Prepare one with NULL and zero-clear others */

	struct mems mems_input[2] = { 0 };

	for (i = 0; i < 2; i++) {
		int j;
		int excess;

		ret = mems_ls(&mems_input[i], "MemTotal", 1.0);
		INTERR(ret, "mems_ls returned %d\n", ret);

		excess = mems_input[i].num_mem_chunks - 4;
		if (excess > 0) {
			ret = mems_shift(&mems_input[i], excess);
			INTERR(ret, "mems_shift returned %d\n", ret);
		}

		/* All */
		for (j = 0; j < mems_input[i].num_mem_chunks; j++) {
			mems_input[i].mem_chunks[j].size = -1;
		}
	}

	int mem_conf_keys[2] = {
		IHK_RESERVE_MEM_MIN_CHUNK_SIZE,
		IHK_RESERVE_MEM_MIN_CHUNK_SIZE,
	};

	int mem_conf_values[2] = { 1UL << 16, 4UL << 20 };

	int ret_expected[2] = { 0, 0 };
	struct mems mems_free_on_reserve[2] = { 0 };

	struct mems mems_ratio[2] = { 0 };
	unsigned long mems_ratio_expected[2] = { 98, 95 };

	double ratios[2][MAX_NUM_MEM_CHUNKS] = { 0 };

	/* Precondition */
	ret = linux_insmod();
	INTERR(ret, "linux_insmod returned %d\n", ret);

	/* Warm up */
	for (i = 0; i < 2; i++) {
		ret = ihk_reserve_mem_conf(0, mem_conf_keys[i],
					   &mem_conf_values[i]);
		INTERR(ret, "ihk_reserve_mem_conf returned %d\n",
		       ret);

		ret = ihk_reserve_mem(0, mems_input[i].mem_chunks,
				      mems_input[i].num_mem_chunks);
		INTERR(ret, "ihk_reserve_mem returned %d\n",
		       ret);

		ret = mems_release();
		INTERR(ret, "mems_release returned %d\n", ret);
	}

	/* Activate and check */
	for (i = 0; i < 2; i++) {
		int excess;

		START("test-case: %s: %s\n", param, values[i]);

		ret = ihk_reserve_mem_conf(0, mem_conf_keys[i],
					   &mem_conf_values[i]);
		INTERR(ret, "ihk_reserve_mem_conf returned %d\n",
		       ret);

		ret = ihk_reserve_mem(0, mems_input[i].mem_chunks,
				      mems_input[i].num_mem_chunks);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);

		/* Scan Linux kmsg */
		ret = mems_free(&mems_free_on_reserve[i]);
		INTERR(ret, "mems_free returned %d\n", ret);

		excess = mems_free_on_reserve[i].num_mem_chunks - 4;
		if (excess > 0) {
			ret = mems_shift(&mems_free_on_reserve[i], excess);
			INTERR(ret, "mems_shift returned %d\n", ret);
		}

		/* Check memory size measured as the ratio of free memory */
		ret = mems_copy(&mems_ratio[i],
				&mems_free_on_reserve[i]);
		INTERR(ret, "mems_copy returned %d\n", ret);

		mems_fill(&mems_ratio[i], mems_ratio_expected[i]);

		ret = mems_check_ratio(&mems_free_on_reserve[i],
				       &mems_ratio[i], ratios[i]);
		if (i == 0) {
			OKNG(ret == 0, "ratio of reserved to NR_FREE_PAGES\n");
		}

		if (i == 1) {
			int j;
			int fail = 0;

			for (j = 0; j < MAX_NUM_MEM_CHUNKS; j++) {
				if (ratios[i - 1][j] > 0 &&
				    ratios[i][j] > ratios[i - 1][j]) {
					fail = 1;
				}
			}
			OKNG(fail == 0, "less memory reserved with"
			     " smaller MIN_CHUNK_SIZE\n");
		}

		/* Clean up */
		ret = mems_release();
		INTERR(ret, "mems_release returned %d\n", ret);
	}
	ret = 0;
 out:
	linux_rmmod(0);
	return ret;
}
