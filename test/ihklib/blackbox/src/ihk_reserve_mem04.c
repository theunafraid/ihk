#include <limits.h>
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

	const char *messages[] =
		{
		 "INT_MIN",
		 "-1",
		 "0",
		 "all",
		 "all + 1",
		 "all - 1",
		 "INT_MAX",
		};
	
	struct mems cpu_inputs[7] = { 0 };

	/* Both Linux and McKernel cpus */
	for (i = 0; i < 7; i++) { 
		ret = cpus_ls(&cpu_inputs[i]);
		INTERR(ret, "cpus_ls returned %d\n", ret);
	}

	/* Plus one */
	ret = cpus_push(&cpu_inputs[4], cpu_inputs[4].num_mem_chunks);
	INTERR(ret, "cpus_push returned %d\n", ret);

	/* Minus one */
	ret = cpus_pop(&cpu_inputs[5], 1);
	INTERR(ret, "cpus_pop returned %d\n", ret);

	/* Spare two cpus for Linux */
	for (i = 0; i < 7; i++) { 
		ret = cpus_shift(&cpu_inputs[i], 2);
		INTERR(ret, "cpus_shift returned %d\n", ret);
	}

	cpu_inputs[0].num_mem_chunks = INT_MIN;
	cpu_inputs[1].num_mem_chunks = -1;
	cpu_inputs[2].num_mem_chunks = 0;
	cpu_inputs[6].num_mem_chunks = INT_MAX;

	int ret_expected[] =
		{
		  -EINVAL,
		  -EINVAL,
		  0,
		  0,
		  -EINVAL,
		  0,
		  -EINVAL,
		};
	
	struct mems *cpus_expected[] = 
		{
		  NULL, /* don't care */
		  NULL, /* don't care */
		  NULL, /* don't care */
		  &cpu_inputs[3],
		  NULL, /* don't care */
		  &cpu_inputs[5],
		  NULL, /* don't care */
		};
	
	/* Precondition */
	ret = insmod(params.uid, params.gid);
	INTERR(ret != 0, "insmod returned %d\n", ret);

	/* Activate and check */
	for (i = 0; i < 7; i++) {
		START("test-case: num_cpus: %s\n", messages[i]);

		ret = ihk_reserve_mem(0,
				      cpu_inputs[i].mem_chunks, cpu_inputs[i].num_mem_chunks);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);
		
		if (cpus_expected[i]) {
			ret = cpus_check_reserved(cpus_expected[i]);
			OKNG(ret == 0, "reserved as expected\n");

			/* Clean up */
			ret = ihk_release_mem(0, cpu_inputs[i].mem_chunks,
					      cpu_inputs[i].num_mem_chunks);
			INTERR(ret != 0, "ihk_release_mem returned %d\n", ret);
		}
	}

	ret = 0;
 out:
	rmmod(0);
	return ret;
}
