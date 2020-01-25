#include <sys/mman.h>
#include <ihklib.h>
#include "input_vector.h"
#include "okng.h"
#include "check.h"

int query_and_check(struct cpus *expected)
{
	int ret;
	struct cpus cpus;
	
	ret = ihk_get_num_reserved_cpus(0);
	INTERR(ret < 0, "ihk_get_num_reserved_cpus returned %d\n",
	       ret);
	cpus.ncpus = ret;
	INFO("# of reserved cpus: %d\n", cpus.ncpus);
	
	cpus.cpus = mmap(0, MAX_NUM_CPUS * sizeof(int),
			 PROT_READ | PROT_WRITE,
			 MAP_ANONYMOUS | MAP_PRIVATE,
			 -1, 0);
	INTERR(cpus.cpus == MAP_FAILED,
	       "mmap cpus.cpus failed\n");
	
	ret = ihk_query_cpu(0, cpus.cpus, cpus.ncpus);
	INTERR(ret < 0, "ihk_query_cpu returned %d\n",
	       ret);
		
	ret = cpus_compare(&cpus, expected);
	if (ret) {
		INFO("actual reservation:\n");
		cpus_dump(&cpus);
		INFO("expected reservation:\n");
		cpus_dump(expected);
	}

 out:
	return ret;
}

