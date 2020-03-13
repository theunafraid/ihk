#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
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

const char param[] = "event type";
const char *values[] = {
	"# of SIMD, FP, scalar operations",
	"# of SVE operations",
	"# of WFI/WFE wait cycles",
	"# of read transactions from the CMG memory",
	"# of write transactions to the CMG memory",
};

/* See the following for the explanation of the following config values.
 * https://github.com/fujitsu/A64FX/blob/master/doc/A64FX_PMU_v1.1.pdf
 */

#define FP_FIXED_OPS_SPEC 0x80c1
/* Counts architecturally executed NEON and FP operations.  The
 * event counter is incremented by the specified number of elements for
 * NEON operations or by 1 for FP operations, and by twice
 * those amounts for operations that would also be counted by
 * FP_FMA_SPEC.
 */

#define FP_SCALE_OPS_SPEC 0x80c0
/* Counts architecturally executed SVE arithmetic operations. This
 * event counter is incremented by (128 / CSIZE) and by twice that amount
 * for operations that would also be counted by SVE_FP_FMA_SPEC.
 */

#define WFE_WFI_CYCLE 0x018e
/* Counts every cycle that the instruction unit is halted by the
 * WFE/WFI instruction.
 */

#define BUS_READ_TOTAL_MEM 0x0316
/* Counts read transactions from memory connected to the CMG It counts
 * all events caused in the measured CMG regardless of measured PE.
 */


#define BUS_WRITE_TOTAL_MEM 0x031e
/* Counts write transactions to memory connect to the CMG.  It counts
 * all events caused in the measured CMG regardless of measured PE.
 */

int main(int argc, char **argv)
{
	int ret;
	int i;
	FILE *fp = NULL;
	int excess;

	params_getopt(argc, argv);

	/* Precondition */
	ret = linux_insmod(0);
	INTERR(ret, "linux_insmod returned %d\n", ret);

	ret = cpus_reserve();
	INTERR(ret, "cpus_reserve returned %d\n", ret);

	/* First application NUMA-node */
	struct mems mems = { 0 };

	ret = mems_ls(&mems, "MemFree", 0.9);
	INTERR(ret, "mems_ls returned %d\n", ret);

	excess = mems.num_mem_chunks - 4;
	if (excess > 0) {
		ret = mems_shift(&mems, excess);
		INTERR(ret, "mems_shift returned %d\n", ret);
	}

	ret = mems_pop(&mems, mems.num_mem_chunks - 1);
	INTERR(ret, "mems_pop returned %d\n", ret);

	ret = ihk_reserve_mem(0, mems.mem_chunks,
			      mems.num_mem_chunks);
	INTERR(ret, "ihk_reserve_mem returned %d\n", ret);

	struct ihk_perf_event_attr attr_input[] = {
		{
		 .config = FP_FIXED_OPS_SPEC,
		 .disabled = 1,
		 .pinned = 0,
		 .exclude_user = 0,
		 .exclude_kernel = 1,
		 .exclude_hv = 1,
		 .exclude_idle = 0
		},
		{
		 .config = FP_SCALE_OPS_SPEC,
		 .disabled = 1,
		 .pinned = 0,
		 .exclude_user = 0,
		 .exclude_kernel = 1,
		 .exclude_hv = 1,
		 .exclude_idle = 0
		},
		{
		 .config = WFE_WFI_CYCLE,
		 .disabled = 1,
		 .pinned = 0,
		 .exclude_user = 0,
		 .exclude_kernel = 1,
		 .exclude_hv = 1,
		 .exclude_idle = 0
		},
		{
		 .config = BUS_READ_TOTAL_MEM,
		 .disabled = 1,
		 .pinned = 0,
		 .exclude_user = 0,
		 .exclude_kernel = 1,
		 .exclude_hv = 1,
		 .exclude_idle = 0
		},
		{
		 .config = BUS_WRITE_TOTAL_MEM,
		 .disabled = 1,
		 .pinned = 0,
		 .exclude_user = 0,
		 .exclude_kernel = 1,
		 .exclude_hv = 1,
		 .exclude_idle = 0
		}
	};

	int ret_expected[1] = { 1 };

	unsigned long count_expected[2] = { 1000000 };

	pid_t pid = -1;
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

		ret = ihk_os_setperfevent(0, attr_input, 5);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);

		ret = ihk_os_perfctl(0, PERF_EVENT_ENABLE);
		INTERR(ret, "PERF_EVENT_ENABLE returned %d\n", ret);

		ret = user_fork_exec("simd_sve_wfi_mem", &pid);
		INTERR(ret < 0, "user_fork_exec returned %d\n", ret);

		ret = waitpid(pid, &wstatus, 0);
		INTERR(ret < 0, "waitpid returned %d\n", errno);
		pid = -1;

		ret = ihk_os_perfctl(0, PERF_EVENT_DISABLE);
		INTERR(ret, "PERF_EVENT_DISABLE returned %d\n", ret);

		ret = ihk_os_getperfevent(0, counts, 1);
		INTERR(ret, "ihk_os_getperfevent returned %d\n",
		       ret);

		OKNG(counts[0] >= count_expected[i] &&
		     counts[0] < count_expected[i] * 1.1,
		     "event count (%ld) is within expected range\n",
		     counts[0]);

		ret = ihk_os_perfctl(0, PERF_EVENT_DISABLE);
		INTERR(ret, "PERF_EVENT_DISABLE returned %d\n", ret);

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
	if (pid != -1) {
		user_wait(&pid);
		linux_kill_mcexec();
	}
	if (ihk_get_num_os_instances(0)) {
		ihk_os_shutdown(0);
		cpus_os_release();
		mems_os_release();
		ihk_destroy_os(0, 0);
	}
	cpus_release();
	mems_release();
	linux_rmmod(0);

	return ret;
}
