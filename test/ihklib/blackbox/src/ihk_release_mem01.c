#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "mem.h"
#include "params.h"
#include "init_fini.h"

const char param[] = "/dev/mcd0";
const char *values[] = {
	"before insmod",
	"after insmod",
};

int main(int argc, char **argv)
{
	int ret;
	int i;

	params_getopt(argc, argv);

	/* Precondition */
	ret = insmod(params.uid, params.gid);
	INTERR(ret, "insmod returned %d\n", ret);
	
	ret = mems_reserve();
	INTERR(ret, "mems_reserve returned %d\n", ret);
	
	struct mems mems_input[2] = { 0 };
	
	int ret_expected[] = { 0, -ENOENT };

	/* Activate and check */
	for (i = 0; i < 2; i++) {
		
		ret = mems_reserved(&mems_input[i]);
		INTERR(ret, "mems_reserved returned %d\n", ret);

		if ( i == 1 ) {
			ret = rmmod(0);
			INTERR(ret, "rmmod returned %d\n", ret);
		}

		START("test-case: %s: %s\n", param, values[i]);

		ret = ihk_release_mem(0, mems_input[i].mem_chunks,
				      mems_input[i].num_mem_chunks);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);

		if (i == 0) {
			ret = mems_reserve();
			INTERR(ret, "mems_reserve returned %d\n", ret);
		}
	}

	ret = 0;
 out:
	rmmod(0);
	return ret;
}
