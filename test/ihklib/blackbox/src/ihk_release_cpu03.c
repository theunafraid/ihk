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

	const char *messages[] =
		{
		 "INT_MIN",
		 "-1",
		 "0",
		 "1",
		 "INT_MAX",
		};
	
	int dev_index_inputs[] =
		{
		 INT_MIN,
		 -1,
		 0,
		 1,
		 INT_MAX
		};

	struct cpus cpu_inputs[5] = { 0 };

	/* All of McKernel CPUs */
	for (i = 0; i < 5; i++) { 
		ret = cpus_ls(&cpu_inputs[i]);
		INTERR(ret, "cpus_ls returned %d\n", ret);

		ret = cpus_shift(&cpu_inputs[i], 2);
		INTERR(ret, "cpus_shift returned %d\n", ret);
	}

	int ret_expected_reserve_cpu[] =
		{
		  -ENOENT,
		  -ENOENT,
		  0,
		  -ENOENT,
		  -ENOENT,
		};

	int ret_expected_get_num_reserved_cpus[] =
		{
		  -ENOENT,
		  -ENOENT,
		  cpu_inputs[2].ncpus,
		  -ENOENT,
		  -ENOENT,
		};

	int ret_expected[] =
		{
		  -ENOENT,
		  -ENOENT,
		  0,
		  -ENOENT,
		  -ENOENT,
		};

	struct cpus *cpus_expected[] = 
		{
		  NULL, /* don't care */
		  NULL, /* don't care */
		  &cpu_inputs[2],
		  NULL, /* don't care */
		  NULL, /* don't care */
		};
	
	/* Precondition */
	ret = insmod(params.uid, params.gid);
	INTERR(ret != 0, "insmod returned %d\n", ret);

	/* Activate and check */
	for (i = 0; i < 5; i++) {
		struct cpus cpus;

		INFO("test-case: dev_index: %s\n", messages[i]);

		ret = ihk_reserve_cpu(dev_index_inputs[i],
				      cpu_inputs[i].cpus, cpu_inputs[i].ncpus);
		INTERR(ret != ret_expected_reserve_cpu[i],
		     "ihk_reserve_cpu returned %d\n", ret);

		ret = ihk_get_num_reserved_cpus(dev_index_inputs[i]);
		INTERR(ret != ret_expected_get_num_reserved_cpus[i],
		     "ihk_get_num_reserved_cpus returned %d\n", ret);

		if (!cpus_expected[i]) {
			ret = cpus_init(&cpus, 1);
			INTERR(ret != 0, "cpus_init returned %d\n", ret);
			
			ret = ihk_query_cpu(dev_index_inputs[i], cpus.cpus,
					    cpus.ncpus);
			OKNG(ret == ret_expected[i],
			     "return value: %d, expected: %d\n",
			     ret, ret_expected[i]);
		} else {
			cpus.ncpus = ret;
			
			ret = cpus_init(&cpus, cpus.ncpus);
			INTERR(ret != 0, "cpus_init returned %d\n", ret);
			
			ret = ihk_query_cpu(dev_index_inputs[i], cpus.cpus,
					    cpus.ncpus);
			OKNG(ret == ret_expected[i],
			     "return value: %d, expected: %d\n",
			     ret, ret_expected[i]);

			ret = cpus_compare(&cpus, cpus_expected[i]);
			OKNG(ret == 0, "query result matches input\n");

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
