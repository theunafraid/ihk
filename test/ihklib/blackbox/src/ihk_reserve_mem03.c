#include <limits.h>
#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "mem.h"
#include "params.h"
#include "mod.h"

const char *values[] = {
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

	/* All of McKernel CPUs */
	for (i = 0; i < 5; i++) {
		ret = mems_ls(&mems_input[i]);
		INTERR(ret, "mems_ls returned %d\n", ret);

		ret = mems_shift(&mems_input[i], 2);
		INTERR(ret, "mems_shift returned %d\n", ret);
	}

	int ret_expected[] = {
		  -ENOENT,
		  -ENOENT,
		  0,
		  -ENOENT,
		  -ENOENT,
		};

	struct mems *mems_expected[] = {
		  NULL, /* don't care */
		  NULL, /* don't care */
		  &mems_input[2],
		  NULL, /* don't care */
		  NULL, /* don't care */
		};

	/* Precondition */
	ret = insmod(params.uid, params.gid);
	INTERR(ret, "insmod returned %d\n", ret);

	/* Activate and check */
	for (i = 0; i < 5; i++) {
		START("test-case: dev_index: %s\n", values[i]);

		ret = ihk_reserve_mem(dev_index_input[i],
				      mems_input[i].mem_chunks,
				      mems_input[i].num_mem_chunks);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);

		if (mems_expected[i]) {
			ret = mems_check_reserved(mems_expected[i]);
			OKNG(ret == 0, "reserved as expected\n");

			/* Clean up */
			ret = ihk_release_mem(0, mems_input[i].mem_chunks,
					      mems_input[i].num_mem_chunks);
			INTERR(ret, "ihk_release_mem returned %d\n", ret);
		}
	}

	ret = 0;
 out:
	rmmod(0);
	return ret;
}
