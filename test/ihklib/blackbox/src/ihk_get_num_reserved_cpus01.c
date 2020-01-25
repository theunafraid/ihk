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

	const char *messages[] =
		{
		 "before insmod",
		 "after insmod",
		};
	
	/* All of McKernel CPUs */
	struct cpus cpu_inputs[2] = { 0 };
	for (i = 1; i < 2; i++) { 
		ret = cpus_ls(&cpu_inputs[i]);
		INTERR(ret, "cpus_ls returned %d\n", ret);

		ret = cpus_unshift(&cpu_inputs[i], 2);
		INTERR(ret, "cpus_unshift returned %d\n", ret);
	}
	
	int ret_expected_reserve_cpu[] = { -ENOENT, 0 };
	int ret_expected[] = { -ENOENT, cpu_inputs[1].ncpus };
	struct cpus *cpus_expected[] = { NULL, &cpu_inputs[1] };

	/* Activate and check */
	for (i = 0; i < 2; i++) {
		INFO("test-case: /dev/mcd0: %s\n", messages[i]);

		ret = ihk_reserve_cpu(0, cpu_inputs[i].cpus, cpu_inputs[i].ncpus);
		INTERR(ret != ret_expected_reserve_cpu[i],
		       "ihk_reserve_cpu returned %d\n", ret);

		ret = ihk_get_num_reserved_cpus(0);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);
		
		if (cpus_expected[i]) {
			ret = query_and_check(cpus_expected[i]);
			OKNG(ret == 0, "reserved as expected\n");
			
			/* Clean up */
			ret = ihk_release_cpu(0, cpu_inputs[i].cpus,
					      cpu_inputs[i].ncpus);
			INTERR(ret != 0, "ihk_release_cpu returned %d\n", ret);
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
