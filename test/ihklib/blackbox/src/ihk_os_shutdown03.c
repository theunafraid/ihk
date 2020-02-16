#include <limits.h>
#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "mem.h"
#include "params.h"
#include "mod.h"

const char *messages[] = {
	"INT_MIN",
	"-1",
	"0",
	"1",
	"INT_MAX",
};

int main(int argc, char **argv)
{
	int ret;
	int i;

	params_getopt(argc, argv);

	int dev_index_input[] = {
		 INT_MIN,
		 -1,
		 0,
		 1,
		 INT_MAX
		};

	struct mems mems_input[5] = { 0 };

	/* All */
	for (i = 0; i < 5; i++) {
		ret = mem_chunks_ls(&mems_input[i]);
		INTERR(ret, "mem_chunks_ls returned %d\n", ret);

		ret = mems_shift(&mems_input[i], 2);
		INTERR(ret, "mems_shift returned %d\n", ret);
	}

	struct mems mems_after_release[5] = { 0 };

	/* All */
	for (i = 2; i < 3; i++) {
		ret = mem_chunks_ls(&mems_after_release[i]);
		INTERR(ret, "mem_chunks_ls returned %d\n", ret);

		ret = mems_shift(&mems_after_release[i], 2);
		INTERR(ret, "mems_shift returned %d\n", ret);
	}

	/* Empty */
	for (i = 2; i < 3; i++) {
		ret = mems_shift(&mems_after_release[i],
				 mems_after_release[i].num_mem_chunks);
		INTERR(ret, "mems_shift returned %d\n", ret);
	}

	int ret_expected_reserve_cpu[] = {
		  -ENOENT,
		  -ENOENT,
		  0,
		  -ENOENT,
		  -ENOENT,
		};

	int ret_expected[] = {
		  -ENOENT,
		  -ENOENT,
		  0,
		  -ENOENT,
		  -ENOENT,
		};

	struct mems *mems_expected[] = {
		 NULL,
		 NULL,
		  &mems_after_release[2],
		 NULL,
		 NULL,
		};

	/* Precondition */
	ret = insmod(params.uid, params.gid);
	INTERR(ret, "insmod returned %d\n", ret);

	/* Activate and check */
	for (i = 0; i < 5; i++) {
		START("test-case: dev_index: %s\n", messages[i]);

		ret = ihk_reserve_cpu(dev_index_input[i],
				      mems_input[i].mem_chunks, cpus_input[i].num_mem_chunks);
		INTERR(ret != ret_expected_reserve_cpu[i],
		     "ihk_reserve_cpu returned %d\n", ret);

		ret = ihk_release_cpu(dev_index_input[i],
				      mems_input[i].mem_chunks, cpus_input[i].num_mem_chunks);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);

		if (mem_chunks_expected[i]) {
			ret = mem_chunks_check_reserved(mems_expected[i]);
			OKNG(ret == 0, "released as expected\n");

			/* Clean up */
			ret = ihk_release_cpu(0, mems_after_release[i].mem_chunks,
					      mems_after_release[i].num_mem_chunks);
			INTERR(ret, "ihk_release_cpu returned %d\n", ret);
		}
	}

	ret = 0;
 out:
	rmmod(0);
	return ret;
}
