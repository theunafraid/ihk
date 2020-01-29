#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "cpu.h"
#include "params.h"
#include "init_fini.h"

int main(int argc, char **argv)
{
	int ret;
	int i;
	
	params_getopt(argc, argv);

	/* Prepare one with NULL and zero-clear others */

	const char *messages[] =
		{
		 "NULL",
		 "reserved",
		 "reserved + 1",
		 "reserved - 1",
		};
	
	struct cpus cpu_inputs_reserve_cpu[4] = { 0 };

	/* All of McKernel CPUs */
	for (i = 0; i < 4; i++) { 
		ret = cpus_ls(&cpu_inputs_reserve_cpu[i]);
		INTERR(ret, "cpus_ls returned %d\n", ret);

		/* Spare two cpus for Linux */
		ret = cpus_shift(&cpu_inputs_reserve_cpu[i], 2);
		INTERR(ret, "cpus_shift returned %d\n", ret);
	}
	
	struct cpus cpu_inputs[4] = {
				     { .ncpus = 1, .cpus = NULL },
				     { 0 },
				     { 0 },
				     { 0 },
	};

	/* All of McKernel CPUs */
	for (i = 1; i < 4; i++) { 
		ret = cpus_ls(&cpu_inputs[i]);
		INTERR(ret, "cpus_ls returned %d\n", ret);
	}

	ret = cpus_push(&cpu_inputs[2], cpu_inputs[2].ncpus);
	INTERR(ret, "cpus_push returned %d\n", ret);
	
	ret = cpus_pop(&cpu_inputs[3], 1);
	INTERR(ret, "cpus_pop returned %d\n", ret);
	
	for (i = 1; i < 4; i++) { 
		/* Spare two cpus for Linux */
		ret = cpus_shift(&cpu_inputs[i], 2);
		INTERR(ret, "cpus_shift returned %d\n", ret);
	}
			
	int ret_expected_reserve_cpu[4] = { 0 };

	int ret_expected[] =
		{
		  -EFAULT,
		  0,
		  -EINVAL,
		  0
		};

	struct cpus cpus_after_release[4] = { 0 };

	/* All of McKernel CPUs */
	for (i = 0; i < 4; i++) { 
		ret = cpus_ls(&cpus_after_release[i]);
		INTERR(ret, "cpus_ls returned %d\n", ret);

		/* Spare two cpus for Linux */
		ret = cpus_shift(&cpus_after_release[i], 2);
		INTERR(ret, "cpus_shift returned %d\n", ret);
	}

	/* Empty */
	ret = cpus_shift(&cpus_after_release[1], cpus_after_release[1].ncpus);
	INTERR(ret, "cpus_shift returned %d\n", ret);

	/* Last one */
	ret = cpus_shift(&cpus_after_release[3], cpus_after_release[3].ncpus - 1);
	INTERR(ret, "cpus_shift returned %d\n", ret);

	struct cpus *cpus_expected[] = 
		{
		  &cpus_after_release[0],
		  &cpus_after_release[1],
		  &cpus_after_release[2],
		  &cpus_after_release[3],
		};

	/* Precondition */
	ret = insmod(params.uid, params.gid);
	INTERR(ret != 0, "insmod returned %d\n", ret);
	
	/* Activate and check */
	for (i = 0; i < 4; i++) {
		START("test-case: cpus array passed: %s\n", messages[i]);
		
		ret = ihk_reserve_cpu(0, cpu_inputs_reserve_cpu[i].cpus,
				      cpu_inputs_reserve_cpu[i].ncpus);
		INTERR(ret != ret_expected_reserve_cpu[i],
		     "ihk_reserve_cpu returned %d\n", ret);

		ret = ihk_release_cpu(0, cpu_inputs[i].cpus,
				      cpu_inputs[i].ncpus);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);
		
		ret = cpus_check_reserved(cpus_expected[i]);
		OKNG(ret == 0, "released as expected\n");
		
		/* Clean up */
		if (cpus_after_release[i].ncpus > 0) {
			ret = ihk_release_cpu(0, cpus_after_release[i].cpus,
					      cpus_after_release[i].ncpus);
			INTERR(ret != 0, "ihk_release_cpu returned %d\n", ret);
		}
	}

	ret = 0;
 out:
	rmmod(0);
	return ret;
}

