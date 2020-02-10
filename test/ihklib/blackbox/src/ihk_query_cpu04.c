#include <limits.h>
#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "cpu.h"
#include "params.h"
#include "init_fini.h"

const char *messages[] = {
	"INT_MIN",
	"-1",
	"0",
	"# of reserved",
	"# of reserved + 1",
	"# of reserved - 1",
	"INT_MAX",
};

int main(int argc, char **argv)
{
	int ret;
	int i;

	params_getopt(argc, argv);

	struct cpus cpus_input_reserve_cpu[7] = { 0 };

	/* Both Linux and McKernel cpus */
	for (i = 0; i < 7; i++) {
		ret = cpus_ls(&cpus_input_reserve_cpu[i]);
		INTERR(ret, "cpus_ls returned %d\n", ret);

		/* Spare two cpus for Linux */
		ret = cpus_shift(&cpus_input_reserve_cpu[i], 2);
		INTERR(ret, "cpus_shift returned %d\n", ret);
	}

	struct cpus cpus_input[] = {
		 { .ncpus = INT_MIN },
		 { .ncpus = -1 },
		 { .ncpus = 0 },
		 { .ncpus = cpus_input_reserve_cpu[3].ncpus },
		 { .ncpus = cpus_input_reserve_cpu[4].ncpus + 1 },
		 { .ncpus = cpus_input_reserve_cpu[5].ncpus - 1 },
		 { .ncpus = INT_MAX },
		};

	for (i = 3; i < 6; i++) {
		ret = cpus_init(&cpus_input[1],
				cpus_input_reserve_cpu[1].ncpus);
		INTERR(ret, "cpus_init returned %d\n", ret);
	}

	int ret_expected_reserve_cpu[7] = { 0 };
	int ret_expected_get_num_reserved_cpus[7] = { 0 };
	int ret_expected[] = {
		 -EINVAL,
		 -EINVAL,
		 -EINVAL,
		 0,
		 -EINVAL,
		 -EINVAL,
		 -EINVAL,
		};

	struct cpus *cpus_expected[] = {
		  NULL, /* don't care */
		  NULL, /* don't care */
		  NULL, /* don't care */
		  &cpus_input_reserve_cpu[3],
		  NULL, /* don't care */
		  NULL, /* don't care */
		  NULL, /* don't care */
		};

	/* Precondition */
	ret = insmod(params.uid, params.gid);
	INTERR(ret, "insmod returned %d\n", ret);

	/* Activate and check */
	for (i = 0; i < 7; i++) {
		START("test-case: num_cpus: %s\n", messages[i]);

		ret = ihk_reserve_cpu(0, cpus_input_reserve_cpu[i].cpus,
				      cpus_input_reserve_cpu[i].ncpus);
		INTERR(ret != ret_expected_reserve_cpu[i],
		     "ihk_reserve_cpu returned %d\n", ret);

		ret = ihk_get_num_reserved_cpus(0);
		INTERR(ret != ret_expected_get_num_reserved_cpus[i],
		     "ihk_get_num_reserved_cpus returned %d\n", ret);

		ret = ihk_query_cpu(0, cpus_input[i].cpus,
				    cpus_input[i].ncpus);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);

		if (cpus_expected[i]) {
			ret = cpus_compare(&cpus_input[i], cpus_expected[i]);
			OKNG(ret == 0, "query result matches input\n");
		}

		/* Clean up */
		ret = ihk_release_cpu(0, cpus_input_reserve_cpu[i].cpus,
				      cpus_input_reserve_cpu[i].ncpus);
		INTERR(ret, "ihk_release_cpu returned %d\n", ret);
	}

	ret = 0;
 out:
	rmmod(0);
	return ret;
}
