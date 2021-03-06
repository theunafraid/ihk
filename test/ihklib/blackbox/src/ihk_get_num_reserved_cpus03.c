#include <limits.h>
#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "cpu.h"
#include "params.h"
#include "linux.h"

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

	struct cpus cpus_input[5] = {{ 0 }};

	/* All of McKernel CPUs */
	for (i = 0; i < 5; i++) {
		ret = cpus_ls(&cpus_input[i]);
		INTERR(ret, "cpus_ls returned %d\n", ret);

		ret = cpus_shift(&cpus_input[i], 2);
		INTERR(ret, "cpus_shift returned %d\n", ret);
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
		  cpus_input[2].ncpus,
		  -ENOENT,
		  -ENOENT,
		};

	struct cpus *cpus_expected[] = {
		  NULL, /* don't care */
		  NULL, /* don't care */
		  &cpus_input[2],
		  NULL, /* don't care */
		  NULL, /* don't care */
		};

	/* Precondition */
	ret = linux_insmod(0);
	INTERR(ret, "linux_insmod returned %d\n", ret);

	/* Activate and check */
	for (i = 0; i < 5; i++) {
		START("test-case: dev_index: %s\n", values[i]);

		ret = ihk_reserve_cpu(dev_index_input[i],
				      cpus_input[i].cpus, cpus_input[i].ncpus);
		INTERR(ret != ret_expected_reserve_cpu[i],
		     "ihk_reserve_cpu returned %d\n", ret);

		ret = ihk_get_num_reserved_cpus(dev_index_input[i]);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);

		if (cpus_expected[i]) {
			ret = cpus_check_reserved(cpus_expected[i]);
			OKNG(ret == 0, "reserved as expected\n");

			/* Clean up */
			ret = ihk_release_cpu(0, cpus_input[i].cpus,
					      cpus_input[i].ncpus);
			INTERR(ret, "ihk_release_cpu returned %d\n", ret);
		}
	}

	ret = 0;
 out:
	linux_rmmod(0);
	return ret;
}
