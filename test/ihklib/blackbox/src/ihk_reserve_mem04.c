#include <limits.h>
#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "mem.h"
#include "params.h"
#include "init_fini.h"

int main(int argc, char **argv)
{
	int ret;
	int i;

	params_getopt(argc, argv);

	const char *values[] = {
		 "INT_MIN",
		 "-1",
		 "0",
		 "all",
		 "all + 1",
		 "all - 1",
		 "INT_MAX",
		};

	struct mems mems_input[7] = { 0 };

	/* Both Linux and McKernel cpus */
	for (i = 0; i < 7; i++) {
		ret = mems_ls(&mems_input[i]);
		INTERR(ret, "mems_ls returned %d\n", ret);
	}

	/* Plus one */
	ret = mems_push(&mems_input[4], mems_input[4].num_mem_chunks);
	INTERR(ret, "mems_push returned %d\n", ret);

	/* Minus one */
	ret = mems_pop(&mems_input[5], 1);
	INTERR(ret, "mems_pop returned %d\n", ret);

	/* Spare two cpus for Linux */
	for (i = 0; i < 7; i++) {
		ret = mems_shift(&mems_input[i], 2);
		INTERR(ret, "mems_shift returned %d\n", ret);
	}

	mems_input[0].num_mem_chunks = INT_MIN;
	mems_input[1].num_mem_chunks = -1;
	mems_input[2].num_mem_chunks = 0;
	mems_input[6].num_mem_chunks = INT_MAX;

	int ret_expected[] = {
		  -EINVAL,
		  -EINVAL,
		  0,
		  0,
		  -EINVAL,
		  0,
		  -EINVAL,
		};

	struct mems *mems_expected[] = {
		  NULL, /* don't care */
		  NULL, /* don't care */
		  NULL, /* don't care */
		  &mems_input[3],
		  NULL, /* don't care */
		  &mems_input[5],
		  NULL, /* don't care */
		};

	/* Precondition */
	ret = insmod(params.uid, params.gid);
	INTERR(ret != 0, "insmod returned %d\n", ret);

	/* Activate and check */
	for (i = 0; i < 7; i++) {
		START("test-case: num_cpus: %s\n", values[i]);

		ret = ihk_reserve_mem(0,
				      mems_input[i].mem_chunks, mems_input[i].num_mem_chunks);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);

		if (mems_expected[i]) {
			ret = mems_check_reserved(mems_expected[i]);
			OKNG(ret == 0, "reserved as expected\n");

			/* Clean up */
			ret = ihk_release_mem(0, mems_input[i].mem_chunks,
					      mems_input[i].num_mem_chunks);
			INTERR(ret != 0, "ihk_release_mem returned %d\n", ret);
		}
	}

	ret = 0;
 out:
	rmmod(0);
	return ret;
}
