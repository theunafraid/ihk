#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "mem.h"
#include "params.h"
#include "mod.h"

int main(int argc, char **argv)
{
	int ret;
	int i;

	params_getopt(argc, argv);

	/* Parse additional options */
	int opt;

	while ((opt = getopt(argc, argv, "ir")) != -1) {
		switch (opt) {
		case 'i':
			/* Precondition */
			ret = insmod(params.uid, params.gid);
			INTERR(ret, "insmod returned %d\n", ret);
			exit(0);
			break;
		case 'r':
			/* Clean up */
			ret = rmmod(1);
			INTERR(ret, "rmmod returned %d\n", ret);
			exit(0);
			break;
		default: /* '?' */
			printf("unknown option %c\n", optopt);
			exit(1);
		}
	}

	const char *messages[] = {
		 "non-root",
		};

	struct mems mems_input[1] = { 0 };

	/* Both Linux and McKernel mem_chunks */
	for (i = 0; i < 1; i++) {
		ret = mem_chunks_ls(&mems_input[i]);
		INTERR(ret, "mem_chunks_ls returned %d\n", ret);
	}

	/* Spare two mem_chunks for Linux */
	for (i = 0; i < 1; i++) {
		ret = mems_shift(&mems_input[i], 2);
		INTERR(ret, "mems_shift returned %d\n", ret);
	}

	int ret_expected_reserve_cpu[] = { -EACCES };
	int ret_expected[] = { -EACCES };

	struct mems mems_after_release[1] = { 0 };

	/* Copy reserved */
	for (i = 0; i < 1; i++) {
		ret = mem_chunks_copy(&mems_after_release[i], &mems_input[i]);
		INTERR(ret, "mem_chunks_copy returned %d\n", ret);
	}

	/* Empty */
	ret = mems_shift(&mems_after_release[0],
			 mems_after_release[0].num_mem_chunks);
	INTERR(ret, "mems_shift returned %d\n", ret);

	struct mems *mems_expected[] = {
		 NULL, /* don't care */
		};

	/* Activate and check */
	for (i = 0; i < 1; i++) {
		START("test-case: user privilege: %s\n", messages[i]);

		ret = ihk_reserve_cpu(0, mems_input[i].mem_chunks,
				      mems_input[i].num_mem_chunks);
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
	}

	ret = 0;
 out:
	return ret;
}
