#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "mem.h"
#include "params.h"
#include "mod.h"

const char *messages[] = {
	"before insmod",
	"after insmod",
};

int main(int argc, char **argv)
{
	int ret;
	int i;

	params_getopt(argc, argv);

	/* All of McKernel CPUs */
	struct mems mems_input_reserve_cpu[2] = { 0 };
	for (i = 1; i < 2; i++) {
		ret = mem_chunks_ls(&mems_input_reserve_cpu[i]);
		INTERR(ret, "mem_chunks_ls returned %d\n", ret);

		ret = mems_shift(&mems_input_reserve_cpu[i], 2);
		INTERR(ret, "mems_shift returned %d\n", ret);
	}

	struct mems mems_input[2] = { 0 };
	for (i = 1; i < 2; i++) {
		ret = mem_chunks_ls(&mems_input[i]);
		INTERR(ret, "mem_chunks_ls returned %d\n", ret);

		ret = mems_shift(&mems_input[i], 2);
		INTERR(ret, "mems_shift returned %d\n", ret);
	}

	int ret_expected_reserve_cpu[] = { -ENOENT, 0 };
	int ret_expected[] = { -ENOENT, 0 };

	struct mems mems_after_release[] = {
		 { 0 },
		 { .mem_chunks = NULL, .num_mem_chunks = 0 },
		};

	struct mems *mems_expected[] = {
		 NULL,
		 &mems_after_release[1],
		};

	/* Activate and check */
	for (i = 0; i < 2; i++) {
		START("test-case: /dev/mcd0: %s\n", messages[i]);

		ret = ihk_reserve_cpu(0, mems_input_reserve_cpu[i].mem_chunks,
				      mems_input_reserve_cpu[i].num_mem_chunks);
		INTERR(ret != ret_expected_reserve_cpu[i],
		       "ihk_reserve_cpu returned %d\n", ret);

		ret = ihk_release_cpu(0, mems_input[i].mem_chunks,
				      mems_input[i].num_mem_chunks);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);

		if (mem_chunks_expected[i]) {
			ret = mem_chunks_check_reserved(mems_expected[i]);
			OKNG(ret == 0, "reserved as expected\n");

			/* Clean up */
			ret = ihk_release_cpu(0, mems_after_release[i].mem_chunks,
					      mems_after_release[i].num_mem_chunks);
			INTERR(ret, "ihk_release_cpu returned %d\n", ret);
		}

		/* Precondition */
		if (i == 0) {
			ret = insmod(params.uid, params.gid);
			INTERR(ret == 0, "insmod returned %d\n", ret);
		}
	}

	ret = 0;
 out:
	rmmod(0);
	return ret;
}
