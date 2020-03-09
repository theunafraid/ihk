#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include "util.h"
#include "okng.h"
#include "linux.h"
#include <sys/stat.h>
#include <ihklib.h>

int linux_insmod(int verbose)
{
	int ret;
	char cmd[1024];

	sprintf(cmd, "insmod %s/kmod/ihk.ko", QUOTE(WITH_MCK));
	if (verbose)
		INFO("%s\n", cmd);
	ret = system(cmd);
	ret = WEXITSTATUS(ret);
	INTERR(ret, "%s returned %d\n", cmd, ret);

	sprintf(cmd,
		"insmod %s/kmod/ihk-%s.ko ihk_start_irq=240 ihk_ikc_irq_core=0",
		QUOTE(WITH_MCK), QUOTE(BUILD_TARGET));
	if (verbose)
		INFO("%s\n", cmd);
	ret = system(cmd);
	ret = WEXITSTATUS(ret);
	INTERR(ret, "%s returned %d\n", cmd, ret);

	sprintf(cmd, "insmod %s/kmod/mcctrl.ko", QUOTE(WITH_MCK));
	if (verbose)
		INFO("%s\n", cmd);
	ret = system(cmd);
	ret = WEXITSTATUS(ret);
	INTERR(ret, "%s returned %d\n", cmd, ret);

out:
	return ret;
}

int linux_chmod(int dev_index)
{
	int i, ret = 0;
	int num_os_instances;
	int *os_indices = NULL;

	ret = ihk_get_num_os_instances(dev_index);
	INTERR(ret < 0,
		"ihk_get_num_os_instances failed with errno %d\n", errno);
	num_os_instances = ret;

	os_indices = calloc(num_os_instances, sizeof(int));
	INTERR(!os_indices, "calloc failed with errno: %d\n", errno);

	ret = ihk_get_os_instances(dev_index, os_indices, num_os_instances);
	INTERR(ret, "ihk_get_os_instances failed with errno: %d\n", errno);

	for (i = 0; i < num_os_instances; i++) {
		char os_filename[4096];

		sprintf(os_filename, "/dev/mcos%d", os_indices[i]);
		ret = chmod(os_filename, S_IRGRP | S_IWGRP |
					S_IROTH | S_IWOTH);
		INTERR(ret, "chmod failed with errno; %d\n", errno);
	}

	ret = 0;
out:
	return ret;
}

static int linux_lsmod(const char *name)
{
	int count = 0;
	int ret;
	FILE *st = NULL;
	char cmd[1024];

	sprintf(cmd, "lsmod | cut -d' ' -f1 | grep -c -x %s", name);

	if ((st = popen(cmd, "r")) == NULL) {
		int errno_save = errno;

		dprintf("%s: error: popen returned %d\n",
			__func__, errno_save);
		ret = -errno_save;
		goto out;
	}

	ret = fscanf(st, "%d\n", &count);

	if (ret == 0) {
		dprintf("%s: error: fscanf returned zero\n",
			__func__);
		ret = -EINVAL;
		goto out;
	}

	if (ret == -1) {
		int errno_save = errno;

		dprintf("%s: error: fscanf returned %d\n",
			__func__, errno_save);
		ret = -errno_save;
		goto out;
	}

	ret = count;
 out:
	if (st) {
		pclose(st);
	}

	return ret;
}

int linux_rmmod(int verbose)
{
	int ret;
	char cmd[1024];
	char name[1024];

	sprintf(name, "mcctrl");
	ret = linux_lsmod(name);
	if (ret < 0) {
		printf("%s: error: linux_lsmod %s returned %d\n",
		       __func__, name, ret);
		goto out;
	} else if (ret == 0) {
		INFO("warning: %s is not loaded\n", name);
	} else {
		INFO("trying to rmmod %s.ko...\n", name);

		sprintf(cmd, "rmmod %s/kmod/%s.ko",
			QUOTE(WITH_MCK), name);
		if (verbose)
			INFO("%s\n", cmd);
		ret = system(cmd);
		ret = WEXITSTATUS(ret);
		if (ret != 0) {
			INFO("%s returned %d\n", cmd, ret);
		}
	}

	sprintf(name, "ihk_%s", QUOTE(KMOD_POSTFIX));
	ret = linux_lsmod(name);
	if (ret < 0) {
		printf("%s: error: linux_lsmod %s returned %d\n",
		       __func__, name, ret);
		goto out;
	} else if (ret == 0) {
		INFO("warning: %s is not loaded\n", name);
	} else {
		INFO("trying to rmmod %s.ko...\n", name);

		sprintf(name, "ihk-%s", QUOTE(BUILD_TARGET));
		sprintf(cmd, "rmmod %s/kmod/%s.ko",
			QUOTE(WITH_MCK), name);
		if (verbose)
			INFO("%s\n", cmd);
		ret = system(cmd);
		ret = WEXITSTATUS(ret);
		if (ret != 0) {
			INFO("%s returned %d\n", cmd, ret);
		}
	}

	sprintf(name, "ihk");
	ret = linux_lsmod(name);

	if (ret < 0) {
		printf("%s: error: linux_lsmod %s returned %d\n",
		       __func__, name, ret);
		goto out;
	} else if (ret == 0) {
		INFO("warning: %s is not loaded\n", name);
	} else {
		INFO("trying to rmmod %s.ko...\n", name);

		sprintf(cmd, "rmmod %s/kmod/%s.ko",
			QUOTE(WITH_MCK), name);
		if (verbose)
			INFO("%s\n", cmd);
		ret = system(cmd);
		ret = WEXITSTATUS(ret);
		if (ret != 0) {
			INFO("%s returned %d\n", cmd, ret);
		}
	}

	ret = 0;
out:
	return ret;
}

int linux_kill_mcexec(void)
{
	int ret;
	char cmd[1024];
	int pid;
	FILE *fp = NULL;
	int killing = 0;

	while (1) {
		sprintf(cmd, "pidof mcexec | awk '{ print $1 }'");

		if ((fp = popen(cmd, "r")) == NULL) {
			int errno_save = errno;

			dprintf("%s: error: popen returned %d\n",
				__func__, errno_save);
			ret = -errno_save;
			goto out;
		}

		ret = fscanf(fp, "%d\n", &pid);
		if (ret == EOF || ret == 0) {
			ret = 0;
			goto out;
		}

		if (killing) {
			goto next;
		}

		if (ret == 1) {
			INFO("killing %d...\n", pid);
			ret = kill(pid, 9);
			if (ret) {
				int errno_save = errno;

				dprintf("%s: error: kill returned %d\n",
					__func__, errno_save);
				ret = -errno_save;
				goto out;
			}
			killing = 1;
		}
	next:
		pclose(fp);
		fp = NULL;
	}
 out:
	if (fp) {
		pclose(fp);
	}
	return ret;
}
