#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include "util.h"
#include "okng.h"
#include "linux.h"

int linux_insmod(void)
{
	int ret;
	char cmd[1024];

	sprintf(cmd, "insmod %s/kmod/ihk.ko", QUOTE(WITH_MCK));
	INFO("%s\n", cmd);
	ret = system(cmd);
	ret = WEXITSTATUS(ret);
	INTERR(ret, "%s returned %d\n", cmd, ret);

	sprintf(cmd,
		"insmod %s/kmod/ihk-%s.ko ihk_start_irq=240 ihk_ikc_irq_core=0",
		QUOTE(WITH_MCK), QUOTE(BUILD_TARGET));
	INFO("%s\n", cmd);
	ret = system(cmd);
	ret = WEXITSTATUS(ret);
	INTERR(ret, "%s returned %d\n", cmd, ret);

	sprintf(cmd, "insmod %s/kmod/mcctrl.ko", QUOTE(WITH_MCK));
	INFO("%s\n", cmd);
	ret = system(cmd);
	ret = WEXITSTATUS(ret);
	INTERR(ret, "%s returned %d\n", cmd, ret);

out:
	return ret;
}

int linux_chmod(uid_t uid, gid_t gid)
{
	int ret;
	char cmd[1024];

	sprintf(cmd, "chmod og+rw /dev/mcos*", uid, gid);
	INFO("%s\n", cmd);
	ret = system(cmd);
	ret = WEXITSTATUS(ret);
	INTERR(ret, "%s returned %d\n", cmd, ret);
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

		sprintf(cmd, "rmmod %s/kmod/%s-%s.ko",
			QUOTE(WITH_MCK), name,
			QUOTE(BUILD_TARGET));
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
	int wstatus;

	sprintf(cmd, "pidof mcexec | xargs -r kill -9");
	ret = system(cmd);
	wstatus = WEXITSTATUS(ret);
	INFO("kill mcexec returned %d\n", wstatus);

	return wstatus;
}
