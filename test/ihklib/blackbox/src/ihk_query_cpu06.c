#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "input_vector.h"
#include "params.h"
#include "init_fini.h"
#include "check.h"

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
			INTERR(ret != 0, "insmod returned %d\n", ret);
			exit(0);
			break;
		case 'r':
			/* Clean up */
			ret = rmmod(1);
			INTERR(ret != 0, "rmmod returned %d\n", ret);
			exit(0);
			break;
		default: /* '?' */
			printf("unknown option %c\n", optopt);
			exit(1);
		}
	}

	const char *messages[] =
		{
		 "non-root",
		};
	
	struct cpus cpu_inputs_reserve_cpu[1] = { 0 };

	/* Both Linux and McKernel cpus */
	for (i = 0; i < 1; i++) { 
		ret = cpus_ls(&cpu_inputs_reserve_cpu[i]);
		INTERR(ret, "cpus_ls returned %d\n", ret);
	}

	/* Spare two cpus for Linux */
	for (i = 0; i < 1; i++) { 
		ret = cpus_shift(&cpu_inputs_reserve_cpu[i], 2);
		INTERR(ret, "cpus_shift returned %d\n", ret);
	}

	struct cpus cpu_inputs[] = { 0 };
	ret = cpus_init(&cpu_inputs[0], cpu_inputs_reserve_cpu[0].ncpus);
	INTERR(ret, "cpus_init returned %d\n", ret);

	int ret_expected_reserve_cpu[] = { -EACCES };
	int ret_expected[] = { -EACCES };
	
	struct cpus *cpus_expected[] = 
		{
		 NULL, /* don't care */
		};
	
	/* Activate and check */
	for (i = 0; i < 1; i++) {
		INFO("test-case: user priviledge: %s\n", messages[i]);

		ret = ihk_reserve_cpu(0, cpu_inputs[i].cpus, cpu_inputs[i].ncpus);
		INTERR(ret != ret_expected_reserve_cpu[i],
		     "ihk_reserve_cpu returned %d\n", ret);

		ret = ihk_query_cpu(0, cpu_inputs[i].cpus,
				    cpu_inputs[i].ncpus);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);

		if (cpus_expected[i]) {
			ret = check_reserved_cpu(cpus_expected[i]);
			OKNG(ret == 0, "reserved as expected\n");
			
			/* Clean up */
			ret = ihk_release_cpu(0, cpu_inputs_reserve_cpu[i].cpus,
					      cpu_inputs_reserve_cpu[i].ncpus);
			INTERR(ret != 0, "ihk_release_cpu returned %d\n", ret);
		}
	}

	ret = 0;
 out:
	return ret;
}
