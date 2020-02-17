#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <ihklib.h>
#include <ihk/ihklib_private.h>
#include "util.h"
#include "okng.h"
#include "cpu.h"
#include "mem.h"
#include "os.h"
#include "user.h"
#include "params.h"
#include "mod.h"

const char param[] = "interrupt enabled / disabled";
const char *messages[] = {
	"interrupt enabled",
	"interrupt disabled"
};

int main(int argc, char **argv)
{
	int ret;
	int i, j;
	pid_t pid = -1;

	params_getopt(argc, argv);

	int ret_expected[] = {
		0,
		0,
	};

	enum ihklib_os_status status_expected[] = {
		IHK_STATUS_INACTIVE,
		IHK_STATUS_INACTIVE,
	};

	/* Precondition */
	ret = insmod(params.uid, params.gid);
	INTERR(ret, "insmod returned %d\n", ret);

	ret = cpus_reserve();
	INTERR(ret, "cpus_reserve returned %d\n", ret);

	ret = mems_reserve();
	INTERR(ret, "mems_reserve returned %d\n", ret);

	/* Activate and check */
	for (i = 0; i < 2; i++) {
		START("test-case: %s: %s\n", param, messages[i]);

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

		ret = ihk_os_boot(0);
		INTERR(ret, "ihk_os_boot returned %d\n", ret);

		if (i == 1) {
			ret = user_fork_exec("delay_with_interrupt_disabled",
					     &pid);
			INTERR(ret < 0, "user_fork_exec returned %d\n");
		}
		usleep(1000000);

		INFO("trying to shutdown os\n");
		ret = ihk_os_shutdown(0);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);

		/* check if os status changed to the expected one */
		os_wait_for_status(status_expected[i]);
		ret = ihk_os_get_status(0);
		OKNG(ret == status_expected[i],
		     "status: %d, expected: %d\n",
		     ret, status_expected[i]);

		/* Clean up */
		if (i == 1) {
			user_wait(&pid);
			kill_mcexec();
		}

		ret = cpus_os_release();
		INTERR(ret, "cpus_os_assign returned %d\n", ret);

		ret = mems_os_release();
		INTERR(ret, "mems_os_assign returned %d\n", ret);

		ret = ihk_destroy_os(0, 0);
		INTERR(ret, "ihk_destroy_os returned %d\n", ret);
	}

	ret = 0;
 out:
	if (pid > 0) {
		user_wait(&pid);
	}

	if (ihk_get_num_os_instances(0)) {
		ihk_destroy_os(0, 0);
	}
	cpus_release();
	mems_release();
	rmmod(1);

	return ret;
}

