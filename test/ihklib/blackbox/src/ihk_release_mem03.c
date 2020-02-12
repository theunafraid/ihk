#include <limits.h>
#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "mem.h"
#include "params.h"
#include "init_fini.h"

const char param[] = "dev_index";
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

	int ret_expected_reserve_mem[] = {
		  -ENOENT,
		  -ENOENT,
		  0,
		  -ENOENT,
		  -ENOENT,
		};

	int ret_expected_get_num_reserved_mems[] = {
		  -ENOENT,
		  -ENOENT,
		  mems_input[2].nmems,
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
		struct mems mems;

		START("test-case: : %s\n", values[i]);

		ret = ihk_reserve_mem(dev_index_input[i],
				      mems_input[i].mems, mems_input[i].nmems);
		INTERR(ret != ret_expected_reserve_mem[i],
		     "ihk_reserve_mem returned %d\n", ret);

		ret = ihk_get_num_reserved_mems(dev_index_input[i]);
		INTERR(ret != ret_expected_get_num_reserved_mems[i],
		     "ihk_get_num_reserved_mems returned %d\n", ret);

		if (!mems_expected[i]) {
			ret = mems_init(&mems, 1);
			INTERR(ret, "mems_init returned %d\n", ret);

			ret = ihk_query_mem(dev_index_input[i], mems.mems,
					    mems.nmems);
			OKNG(ret == ret_expected[i],
			     "return value: %d, expected: %d\n",
			     ret, ret_expected[i]);
		} else {
			mems.nmems = ret;

			ret = mems_init(&mems, mems.nmems);
			INTERR(ret, "mems_init returned %d\n", ret);

			ret = ihk_query_mem(dev_index_input[i], mems.mems,
					    mems.nmems);
			OKNG(ret == ret_expected[i],
			     "return value: %d, expected: %d\n",
			     ret, ret_expected[i]);

			ret = mems_compare(&mems, mems_expected[i]);
			OKNG(ret == 0, "query result matches input\n");

			/* Clean up */
			ret = ihk_release_mem(0, mems_input[i].mems,
					      mems_input[i].nmems);
			INTERR(ret, "ihk_release_mem returned %d\n", ret);
		}
	}

	ret = 0;
 out:
	rmmod(0);
	return ret;
}
