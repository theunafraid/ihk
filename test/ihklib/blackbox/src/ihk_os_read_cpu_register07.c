#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "mem.h"
#include "cpu.h"
#include "os.h"
#include "params.h"
#include "linux.h"
#include "user.h"
#include "perf.h"

const char param[] = "addr_ext";
const char *values[] = {
	"IMP_PF_CTRL_EL1",
};

#define Op0_shift	19
#define Op0_mask	0x3
#define Op1_shift	16
#define Op1_mask	0x7
#define CRn_shift	12
#define CRn_mask	0xf
#define CRm_shift	8
#define CRm_mask	0xf
#define Op2_shift	5
#define Op2_mask	0x7

#define sys_reg(op0, op1, crn, crm, op2)	       \
	(((op0) << Op0_shift) | ((op1) << Op1_shift) | \
	 ((crn) << CRn_shift) | ((crm) << CRm_shift) | \
	 ((op2) << Op2_shift))

#define IMP_PF_CTRL_EL1				sys_reg(3, 0, 11, 4, 0)

int main(int argc, char **argv)
{
	int ret;
	int i;
	int fd = -1;
	char fn[4096];

	params_getopt(argc, argv);

	/* Precondition */
	ret = linux_insmod(0);
	INTERR(ret, "linux_insmod returned %d\n", ret);

	sprintf(fn, "%s/kmod/test_driver.ko", QUOTE(CMAKE_INSTALL_PREFIX));
	ret = _linux_insmod(fn);
	INTERR(ret, "linux_insmod_test_driver returned %d\n", ret);

	ret = cpus_reserve();
	INTERR(ret, "cpus_reserve returned %d\n", ret);

	ret = mems_reserve();
	INTERR(ret, "mems_reserve returned %d\n", ret);

	/* Activate and check */
	for (i = 0; i < 1; i++) {
		int errno_save;
		int ncpu;
		char cmd[4096];
		unsigned long counts[1];
		int wstatus;

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

		ret = ihk_os_boot(0);
		INTERR(ret, "ihk_os_boot returned %d\n", ret);

		sprintf(cmd, "%s/bin/mcexec "
			"%s/bin/read_write_cpu_register -c 0 -a %d",
			QUOTE(WITH_MCK), QUOTE(CMAKE_INSTALL_PREFIX),
			IMP_PF_CTRL_EL1);
		ret = system(cmd);
		wstatus = WEXITSTATUS(ret);

		INTERR(ret < 0 || wstatus != 0,
		       "system returned %d or invalid exit status: %d\n",
		       errno, wstatus);

		ret = linux_kill_mcexec();
		INTERR(ret, "linux_kill_mcexec returned %d\n", ret);

		system("/home/takagi/project/os/install/sbin/ihkosctl 0 kmsg");

		ret = ihk_os_shutdown(0);
		INTERR(ret, "ihk_os_shutdown returned %d\n", ret);

		ret = os_wait_for_status(IHK_STATUS_INACTIVE);
		INTERR(ret, "os status didn't change to %d\n",
		       IHK_STATUS_INACTIVE);

		ret = cpus_os_release();
		INTERR(ret, "cpus_os_release returned %d\n", ret);

		ret = mems_os_release();
		INTERR(ret, "mems_os_release returned %d\n", ret);

		ret = ihk_destroy_os(0, 0);
		INTERR(ret, "ihk_destroy_os returned %d\n", ret);
	}

	ret = 0;
 out:
	system("/home/takagi/project/os/install/sbin/ihkosctl 0 kmsg");

	linux_kill_mcexec();

	if (fd != -1) {
		close(fd);
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
	_linux_rmmod("test_driver");
	linux_rmmod(0);

	return ret;
}
