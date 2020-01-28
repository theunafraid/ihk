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

	/* Prepare one with NULL and zero-clear others */

	const char *messages[] =
		{
		 "NULL",
		 "# of entries: # of reserved",
		};
	
	struct cpus cpu_inputs_reserve_cpu[2] = { 0 };

	/* All of McKernel CPUs */
	for (i = 0; i < 2; i++) { 
		ret = cpus_ls(&cpu_inputs_reserve_cpu[i]);
		INTERR(ret, "cpus_ls returned %d\n", ret);

		/* Spare two cpus for Linux */
		ret = cpus_shift(&cpu_inputs_reserve_cpu[i], 2);
		INTERR(ret, "cpus_shift returned %d\n", ret);
	}
	
	struct cpus cpu_inputs[2] = { 0 };

	ret = cpus_init(&cpu_inputs[1], cpu_inputs_reserve_cpu[1].ncpus);
	INTERR(ret, "cpus_init returned %d\n", ret);
			
	int ret_expected_reserve_cpu[] = { 0, 0 };

	int ret_expected_get_num_reserved_cpus[] =
		{
		 cpu_inputs_reserve_cpu[0].ncpus,
		 cpu_inputs_reserve_cpu[1].ncpus,
		};
	
	int ret_expected[] =
		{
		  -EINVAL,
		  0,
		};

	struct cpus *cpus_expected[] = 
		{
		  NULL, /* don't care */
		  &cpu_inputs_reserve_cpu[1],
		};

	/* Precondition */
	ret = insmod(params.uid, params.gid);
	INTERR(ret != 0, "insmod returned %d\n", ret);
	
	/* Activate and check */
	for (i = 0; i < 2; i++) {
		START("test-case: cpus array passed: %s\n", messages[i]);
		
		ret = ihk_reserve_cpu(0, cpu_inputs_reserve_cpu[i].cpus,
				      cpu_inputs_reserve_cpu[i].ncpus);
		INTERR(ret != ret_expected_reserve_cpu[i],
		     "ihk_reserve_cpu returned %d\n", ret);

		ret = ihk_get_num_reserved_cpus(0);
		INTERR(ret != ret_expected_get_num_reserved_cpus[i],
		     "ihk_get_num_reserved_cpus returned %d\n", ret);

		ret = ihk_query_cpu(0, cpu_inputs[i].cpus,
				    cpu_inputs[i].ncpus);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);
		
		if (cpus_expected[i]) {
			ret = cpus_compare(&cpu_inputs[i], cpus_expected[i]);
			OKNG(ret == 0, "query result matches input\n");
		}
		
		/* Clean up */
		ret = ihk_release_cpu(0, cpu_inputs_reserve_cpu[i].cpus,
				      cpu_inputs_reserve_cpu[i].ncpus);
		INTERR(ret != 0, "ihk_release_cpu returned %d\n", ret);
	}

	ret = 0;
 out:
	rmmod(0);
	return ret;
}

