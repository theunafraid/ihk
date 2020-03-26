#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "cpu.h"
#include "mem.h"
#include "os.h"
#include "params.h"
#include "linux.h"

const char param[] = "Path of dumpfiles";
const char *values[] = {
	"NULL",
	"valid path",
	"path with invalid directory",
	"inaccessible path"
};

int main(int argc, char **argv)
{
	int ret;
	int i;
	char *fn;
	int opt;

	params_getopt(argc, argv);

	while ((opt = getopt(argc, argv, "f:")) != -1) {
		switch (opt) {
		case 'f':
			fn = optarg;
			break;
		default: /* '?' */
			printf("unknown option %c\n", optopt);
			exit(1);
		}
	}

	char *dumpfile_input[4] = {
		NULL,
		fn,
		"/path/to/nonexistence",
		NULL
	};

	int ret_expected[4] = {
		-EINVAL,
		0,
		-ENOENT,
		-EACCES,
	};

	int ret_access_expected[4] = {
		-1,
		0
	};

	/* Precondition */
	ret = linux_insmod(0);
	INTERR(ret, "linux_insmod returned %d\n", ret);

	ret = cpus_reserve();
	INTERR(ret, "cpus_reserve returned %d\n", ret);

	ret = mems_reserve();
	INTERR(ret, "mems_reserve returned %d\n", ret);

	/* Activate and check */
	for (i = 0; i < 4; i++) {
		START("test-case: %s: %s\n", param, values[i]);

		ret = ihk_create_os(0);
		INTERR(ret, "ihk_create_os returned %d\n", ret);

		ret = cpus_os_assign();
		INTERR(ret, "cpus_os_assign returned %d\n", ret);

		ret = mems_os_assign();
		INTERR(ret, "mems_os_assign returned %d\n", ret);

		ret = os_load();
		INTERR(ret, "os_load returned %d\n", ret);

		ret = os_kargs();
		INTERR(ret, "os_kargs returned %d\n", ret);

		/* Precondition */
		if (i == 1) {
			ret = ihk_os_boot(0);
			INTERR(ret, "ihk_os_boot returned %d\n", ret);

			ret = os_wait_for_status(IHK_STATUS_RUNNING);
			INTERR(ret, "os status didn't change to %d\n",
			       IHK_STATUS_RUNNING);
		}

		ret = ihk_os_makedumpfile(0, dumpfile_input, 24, 0);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);

		ret = access(dumpfile_input, F_OK);
		OKNG(ret == ret_access_expected[i],
			"dumpfile created as expected\n");

		/* Clean up */
		if ((ihk_os_get_status(0)) == IHK_STATUS_RUNNING) {
			ret = unlink(dumpfile_input);
			INTERR(ret, "unlink returned %d\n", ret);

			ret = ihk_os_shutdown(0);
			INTERR(ret, "ihk_os_shutdown returned %d\n", ret);

			ret = os_wait_for_status(IHK_STATUS_INACTIVE);
			INTERR(ret, "os status didn't change to %d\n",
			       IHK_STATUS_INACTIVE);
		}

		ret = cpus_os_release();
		INTERR(ret, "cpus_os_release returned %d\n", ret);

		ret = mems_os_release();
		INTERR(ret, "mems_os_release returned %d\n", ret);

		ret = ihk_destroy_os(0, 0);
		INTERR(ret, "ihk_destroy_os returned %d\n", ret);
	}

	ret = 0;
 out:
	if (!(access(dumpfile_input, F_OK))) {
		unlink(dumpfile_input);
	}
	if (ihk_get_num_os_instances(0)) {
		ihk_os_shutdown(0);
		os_wait_for_status(IHK_STATUS_INACTIVE);
		cpus_os_release();
		mems_os_release();
		ihk_destroy_os(0, 0);
	}
	cpus_release();
	mems_release();
	linux_rmmod(1);

	return ret;
}
