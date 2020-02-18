#include <limits.h>
#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "cpu.h"
#include "mem.h"
#include "os.h"
#include "params.h"
#include "mod.h"

const char param[] = "os status";
const char *values[] = {
	"before boot",
	"after boot",
};

int main(int argc, char **argv)
{
	int ret;
	int i;

	params_getopt(argc, argv);

	/* Precondition */
	ret = insmod();
	INTERR(ret, "insmod returned %d\n", ret);

	ret = cpus_reserve();
	INTERR(ret, "cpus_reserve returned %d\n", ret);

	ret = mems_reserve();
	INTERR(ret, "mems_reserve returned %d\n", ret);

	ret = ihk_create_os(0);
	INTERR(ret, "ihk_create_os returned %d\n", ret);

	ret = mems_os_assign();
	INTERR(ret, "mems_os_assign returned %d\n", ret);

	struct cpus cpus_input[2] = { 0 };
	struct cpus cpus_after_assign[2] = { 0 };

	/* All */
	for (i = 0; i < 2; i++) {
		ret = cpus_reserved(&cpus_input[i]);
		INTERR(ret, "cpus_reserved returned %d\n", ret);

		ret = cpus_reserved(&cpus_after_assign[i]);
		INTERR(ret, "cpus_reserved returned %d\n", ret);
	}

	int ret_expected[] = {
		0,
		-EBUSY,
	};

	struct cpus *cpus_expected[] = {
		&cpus_after_assign[0],
		&cpus_after_assign[1],
	};

	/* Activate and check */
	for (i = 0; i < 2; i++) {
		START("test-case: %s: %s\n", param, values[i]);

		/* Precondition */
		if (i == 1) {
			/* Note that os need cpu to boot */
			ret = ihk_os_assign_cpu(0, cpus_input[i].cpus,
						cpus_input[i].ncpus);
			INTERR(ret, "ihk_os_assign_cpu returned %d\n", ret);

			ret = os_load();
			INTERR(ret, "os_load returned %d\n", ret);

			ret = os_kargs();
			INTERR(ret, "os_kargs returned %d\n", ret);

			ret = ihk_os_boot(0);
			INTERR(ret, "ihk_os_boot returned %d\n", ret);
		}

		ret = ihk_os_assign_cpu(0, cpus_input[i].cpus,
				cpus_input[i].ncpus);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);

		if (cpus_expected[i]) {
			ret = cpus_check_assigned(cpus_expected[i]);
			OKNG(ret == 0, "released as expected\n");
		}

		/* Clean up */
		if (i == 1) {
			ret = ihk_os_shutdown(0);
			INTERR(ret, "ihk_os_boot returned %d\n", ret);
		}

		ret = cpus_os_release();
		INTERR(ret, "cpus_os_release returned %d\n", ret);
	}

	ret = 0;
 out:
	mems_os_release();
	ihk_destroy_os(0, 0);
	cpus_release();
	mems_release();
	rmmod(0);

	return ret;
}
