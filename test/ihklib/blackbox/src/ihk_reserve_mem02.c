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

	/* Prepare one with NULL and zero-clear others */

	const char param[] = "mem_chunks";
	const char *values[] =
		{
		 "NULL",
		 "MemFree * 0.9",
		};
	
	struct mems mems_input[2] = {
				     { .num_mem_chunks = 1, .mem_chunks = NULL },
				     { 0 },
	};
	
	for (i = 1; i < 2; i++) { 
		ret = mems_ls(&mems_input[i], "MemFree", 0.9);
		INTERR(ret, "mems_ls returned %d\n", ret);
	}

	int ret_expected[] = { -EFAULT, 0 };
	struct mems *mems_expected[] = { NULL, &mems_input[1] };

	/* Precondition */
	ret = insmod(params.uid, params.gid);
	INTERR(ret != 0, "insmod returned %d\n", ret);
	
	/* Activate and check */
	for (i = 0; i < 2; i++) {
		START("test-case: %s: %s\n", param, values[i]);
		
		ret = ihk_reserve_mem(0, mems_input[i].mem_chunks, mems_input[i].num_mem_chunks);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);

		if (mems_expected[i]) {
			mems_dump(&mems_input[i]);

			ret = mems_check_reserved(mems_expected[i], 4UL << 20);
			OKNG(ret == 0, "reserved as expected\n");
			
			/* Clean up */
			ret = mems_query_and_release();
			INTERR(ret != 0, "mems_query_and_release returned %d\n", ret);
		}
	}

	ret = 0;
 out:
	rmmod(0);
	return ret;
}

