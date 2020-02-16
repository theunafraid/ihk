#include <limits.h>
#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "cpu.h"
#include "params.h"
#include "mod.h"

int main(int argc, char **argv)
{
	int ret;
	int i;

	params_getopt(argc, argv);

	const char *values[] = {
		 "root",
		};

	struct cpus cpus_input[1] = { 0 };

	/* Both Linux and McKernel cpus */
	for (i = 0; i < 1; i++) {
		ret = cpus_reserved(&cpus_input[i]);
		INTERR(ret, "cpus_reserved returned %d\n", ret);
	}

	/* Spare two cpus for Linux */
	for (i = 0; i < 1; i++) {
		ret = cpus_shift(&cpus_input[i], 2);
		INTERR(ret, "cpus_shift returned %d\n", ret);
	}

	int ret_expected[] = { 0 };

	struct cpus *cpus_expected[] = {
		 &cpus_input[0],
		};

	/* Precondition */
	ret = insmod(params.uid, params.gid);
	INTERR(ret, "insmod returned %d\n", ret);

	/* Activate and check */
	for (i = 0; i < 1; i++) {
		START("test-case: user privilege: %s\n", values[i]);

		ret = ihk_os_assign_cpu(0, cpus_input[i].cpus, cpus_input[i].ncpus);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);

		if (cpus_expected[i]) {
			ret = cpus_check_assigned(cpus_expected[i]);
			OKNG(ret == 0, "assigned as expected\n");

			/* Clean up */
			ret = ihk_os_release_cpu(0, cpus_input[i].cpus,
					      cpus_input[i].ncpus);
			INTERR(ret, "ihk_os_release_cpu returned %d\n", ret);
		}
	}

	ret = 0;
 out:
	rmmod(0);
	return ret;
}
