#include <limits.h>
#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "mem.h"
#include "params.h"
#include "mod.h"

const char *values[] = {
	"root",
};

int main(int argc, char **argv)
{
	int ret;
	int i;

	params_getopt(argc, argv);

	struct mems mems_input[1] = { 0 };

	/* Both Linux and McKernel cpus */
	for (i = 0; i < 1; i++) {
		ret = mems_ls(&mems_input[i]);
		INTERR(ret, "mems_ls returned %d\n", ret);
	}

	/* Spare two cpus for Linux */
	for (i = 0; i < 1; i++) {
		ret = mems_shift(&mems_input[i], 2);
		INTERR(ret, "mems_shift returned %d\n", ret);
	}

	int ret_expected[] = { 0 };

	struct mems *mems_expected[] = {
		&mems_input[0],
	};

	/* Precondition */
	ret = insmod(params.uid, params.gid);
	INTERR(ret, "insmod returned %d\n", ret);

	/* Activate and check */
	for (i = 0; i < 1; i++) {
		START("test-case: user privilege: %s\n", values[i]);

		ret = ihk_reserve_mem(0, mems_input[i].mem_chunks,
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
