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

	const char *messages[] = {
		 "INT_MIN",
		 "-1",
		 "0",
		 "all",
		 "all + 1",
		 "all - 1",
		 "INT_MAX",
		};

	struct cpus cpus_input[7] = { 0 };

	/* Both Linux and McKernel cpus */
	for (i = 0; i < 7; i++) {
		ret = cpus_ls(&cpus_input[i]);
		INTERR(ret, "cpus_ls returned %d\n", ret);
	}

	/* Plus one */
	ret = cpus_push(&cpus_input[4],
			cpus_max_id(&cpus_input[4]) + 1);
	INTERR(ret, "cpus_push returned %d\n", ret);

	/* Minus one */
	ret = cpus_pop(&cpus_input[5], 1);
	INTERR(ret, "cpus_pop returned %d\n", ret);

	/* Spare two cpus for Linux */
	for (i = 0; i < 7; i++) {
		ret = cpus_shift(&cpus_input[i], 2);
		INTERR(ret, "cpus_shift returned %d\n", ret);
	}

	cpus_input[0].ncpus = INT_MIN;
	cpus_input[1].ncpus = -1;
	cpus_input[2].ncpus = 0;
	cpus_input[6].ncpus = INT_MAX;

	int ret_expected[] = {
		  -EINVAL,
		  -EINVAL,
		  0,
		  0,
		  -EINVAL,
		  0,
		  -EINVAL,
		};

	struct cpus *cpus_expected[] = {
		  NULL, /* don't care */
		  NULL, /* don't care */
		  NULL, /* don't care */
		  &cpus_input[3],
		  NULL, /* don't care */
		  &cpus_input[5],
		  NULL, /* don't care */
		};

	/* Precondition */
	ret = insmod();
	INTERR(ret, "insmod returned %d\n", ret);

	/* Activate and check */
	for (i = 0; i < 7; i++) {
		START("test-case: num_cpus: %s\n", messages[i]);

		ret = ihk_reserve_cpu(0,
				      cpus_input[i].cpus, cpus_input[i].ncpus);
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
	rmmod(0);
	return ret;
}
