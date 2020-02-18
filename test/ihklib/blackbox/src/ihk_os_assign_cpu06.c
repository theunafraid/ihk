#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "cpu.h"
#include "params.h"
#include "mod.h"

const char param[] = "user privilege";
const char *values[] = {
	"non-root",
};
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
			ret = insmod();
			INTERR(ret, "insmod returned %d\n", ret);

			ret = cpus_reserve();
			INTERR(ret, "cpus_reserve returned %d\n", ret);

			ret = ihk_create_os(0);
			INTERR(ret, "ihk_create_os returned %d\n", ret);

			/* make /dev/mcos0 accessible to non-root */
			ret = mod_chmod(params.uid, params.gid);
			INTERR(ret, "mod_chmod returned %d\n", ret);

			exit(0);
			break;
		case 'r':
			/* Clean up */
			ret = cpus_release();
			INTERR(ret, "cpus_release returned %d\n", ret);

			ret = rmmod(1);
			INTERR(ret, "rmmod returned %d\n", ret);
			exit(0);
			break;
		default: /* '?' */
			printf("unknown option %c\n", optopt);
			exit(1);
		}
	}

	struct cpus cpus_input[1] = { 0 };
	int ret_expected[] = { -EPERM };

	struct cpus *cpus_expected[] = {
		 NULL, /* don't care */
	};

	/* Activate and check */
	for (i = 0; i < 1; i++) {
		START("test-case: %s: %s\n", param, values[i]);

		/* cpus are dummy because we can't query */
		ret = cpus_push(&cpus_input[i], 0);
		INTERR(ret, "cpus_push returned %d\n", ret);

		ret = ihk_os_assign_cpu(0, cpus_input[i].cpus, 
				cpus_input[i].ncpus);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);
	}

	ret = 0;
 out:
	return ret;
}
