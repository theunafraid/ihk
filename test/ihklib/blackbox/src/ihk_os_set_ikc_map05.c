#include <limits.h>
#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "cpu.h"
#include "mem.h"
#include "os.h"
#include "params.h"
#include "linux.h"

const char param[] = "user privilege";
const char *values[] = {
	"root"
};

int main(int argc, char **argv)
{
	int ret;
	int i;

	params_getopt(argc, argv);

	/* Precondition */
	ret = linux_insmod(0);
	INTERR(ret, "linux_insmod returned %d\n", ret);

	ret = cpus_reserve();
	INTERR(ret, "cpus_reserve returned %d\n", ret);

	ret = mems_reserve();
	INTERR(ret, "mems_reserve returned %d\n", ret);

	struct ikc_cpu_map map_input[1] = { 0 };
	struct ikc_cpu_map map_after_set[1] = { 0 };

	for (i = 0; i < 1; i++) {
		ret = ikc_cpu_map_2toN(&map_input[i]);
		INTERR(ret, "ikc_cpu_map_2toN returned %d\n", ret);
		
		ret = ikc_cpu_map_2toN(&map_after_set[i]);
		INTERR(ret, "ikc_cpu_map_2toN returned %d\n", ret);
	}
	
	int ret_expected[] = { 0 };
	struct ikc_cpu_map *map_expected[] = {
		&map_after_set[0],
	};

	/* Activate and check */
	for (i = 0; i < 1; i++) {
		START("test-case: %s: %s\n", param, values[i]);
		
		ret = ihk_create_os(0);
		INTERR(ret, "ihk_create_os returned %d\n", ret);

		ret = cpus_os_assign();
		INTERR(ret, "cpus_os_assign returned %d\n", ret);

		ret = mems_os_assign();
		INTERR(ret, "mems_os_assign returned %d\n", ret);

		ret = ihk_os_set_ikc_map(0, map_input[i].map,
				map_input[i].ncpus);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);
	
		ret = os_load();
		INTERR(ret, "os_load returned %d\n", ret);

		ret = os_kargs();
		INTERR(ret, "os_kargs returned %d\n", ret);

		ret = ihk_os_boot(0);
		INTERR(ret, "ihk_os_boot returned %d\n", ret);
	
		if (map_expected[i]) {
			ret = ikc_cpu_map_check_channels(map_input[i].ncpus);
			OKNG(ret == map_expected[i]->ncpus,
				"check_ncpus: returned: %d expected: %d\n",
				ret, map_expected[i]->ncpus);

			ret = ikc_cpu_map_check(map_expected[i]);
			OKNG(ret == 0, "map set as expected\n");
		}
		
		ret = ihk_os_shutdown(0);
		INTERR(ret, "ihk_os_shutdown returned %d\n", ret);
		
		ret = os_wait_for_status(IHK_STATUS_INACTIVE);
		INTERR(ret, "os status didn't change to %d\n",
		       IHK_STATUS_INACTIVE);
		
		ret = os_wait_for_status(IHK_STATUS_INACTIVE);
		INTERR(ret, "os_wait_for_status returned %d\n", ret);
		
		ret = cpus_os_release();
		INTERR(ret, "cpus_os_release returned %d\n", ret);

		ret = mems_os_release();
		INTERR(ret, "mems_os_release returned %d\n", ret);

		ret = ihk_destroy_os(0, 0);
		INTERR(ret, "ihk_destroy_os returned %d\n", ret);

	}

	ret = 0;
 out:
	ihk_os_shutdown(0);
	mems_release();
	cpus_release();
	linux_rmmod(0);
	
	return ret;
}
