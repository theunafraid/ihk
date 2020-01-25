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
		 "all",
		 "all + 1",
		 "all - 1",
		};
	
	struct cpus cpu_inputs[] = 
		{
		 {
		  .cpus = NULL,
		  .ncpus = 1,
		 },
		 { 0 },
		 { 0 },
		 { 0 },
		};

	/* All of McKernel CPUs */
	for (i = 1; i < 4; i++) { 
		ret = cpus_ls(&cpu_inputs[i]);
		INTERR(ret, "cpus_ls returned %d\n", ret);
	}

	/* Plus one */
	ret = cpus_push(&cpu_inputs[2], cpu_inputs[2].ncpus);
	INTERR(ret, "cpus_push returned %d\n", ret);

	/* Minus one */
	ret = cpus_pop(&cpu_inputs[3]);
	INTERR(ret, "cpus_pop returned %d\n", ret);

	for (i = 1; i < 4; i++) {
		/* Spare two cpus for Linux */
		ret = cpus_unshift(&cpu_inputs[i], 2);
		INTERR(ret, "cpus_unshift returned %d\n", ret);
	}
	
	int ret_expected[] = {
		  -EFAULT,
		  0,
		  -EINVAL,
		  0,
	};
	struct cpus *cpus_expected[] = 
		{
		  NULL, /* don't care */
		  &cpu_inputs[1],
 		  NULL, /* don't care */
		  &cpu_inputs[3],
		};

	/* Precondition */
	ret = insmod(params.uid, params.gid);
	INTERR(ret != 0, "insmod returned %d\n", ret);
	
	/* Activate and check */
	for (i = 0; i < 4; i++) {
		INFO("test-case: cpus: %s\n", messages[i]);
		
		ret = ihk_reserve_cpu(0, cpu_inputs[i].cpus, cpu_inputs[i].ncpus);
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
	}

	ret = 0;
 out:
	rmmod(0);
	return ret;
}

