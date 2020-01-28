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

	const char *messages[] =
		{
		 "before insmod",
		 "after insmod",
		};
	
	struct mems mems_input[2] = { 0 };
	for (i = 0; i < 2; i++) { 
		ret = mems_ls(&mems_input[i], "MemFree", 0.9);
		INTERR(ret, "mems_ls returned %d\n", ret);
	}

	int ret_expected[] = { -ENOENT, 0 };
	struct mems *mems_expected[] = { NULL, &mems_input[1] };

	/* Activate and check */
	for (i = 0; i < 2; i++) {
		START("test-case: /dev/mcd0: %s\n", messages[i]);

		mems_dump(&mems_input[i]);
		ret = ihk_reserve_mem(0, mems_input[i].mem_chunks,
				      mems_input[i].num_mem_chunks);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);
		
		if (mems_expected[i]) {
			mems_dump_sum(&mems_input[i]);

			ret = mems_check_reserved(mems_expected[i]);
			OKNG(ret == 0, "reserved as expected\n");

			/* Clean up */
			ret = ihk_release_mem(0, mems_input[i].mem_chunks,
					      mems_input[i].num_mem_chunks);
			INTERR(ret != 0, "ihk_release_mem returned %d\n", ret);
		}
		
		/* Precondition */
		if (i == 0) {
			ret = insmod(params.uid, params.gid);
			NG(ret == 0, "insmod returned %d\n", ret);
		}
	}

	ret = 0;
 out:
	rmmod(0);
	return ret;
}
