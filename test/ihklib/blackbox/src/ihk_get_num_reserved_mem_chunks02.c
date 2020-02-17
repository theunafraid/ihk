#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "mem.h"
#include "params.h"
#include "mod.h"

const char param[] = "num_chunks";
const char *values[] = {
	"NULL",
	"MemFree * 0.9",
};

int main(int argc, char **argv)
{
	int ret;
	int i;

	params_getopt(argc, argv);

	/* Precondition */
	ret = insmod();
	INTERR(ret, "insmod returned %d\n", ret);

	struct mems mems_input[] = {
		{
			.mem_chunks = NULL,
			.num_mem_chunks = 0,
		},
		{ 0 },
	};

	struct mems mems_after_reserve[2] = { 0 };
	int ret_expected[2] = { 0 };

	/* Activate and check */
	for (i = 0; i < 2; i++) {
		START("test-case: %s: %s\n", param, values[i]);

		if (i != 0) {
			ret = mems_reserve();
			INTERR(ret, "mems_reserve returned %d\n", ret);

			ret = mems_reserved(&mems_after_reserve[i]);
			INTERR(ret, "mems_reserved returned %d\n", ret);

			ret_expected[i] = mems_after_reserve[i].num_mem_chunks;
		}

		ret = ihk_get_num_reserved_mem_chunks(0);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);

		ret = mems_release();
		INTERR(ret, "mems_reserve returned %d\n", ret);
	}

	ret = 0;
 out:
	rmmod(0);
	return ret;
}

