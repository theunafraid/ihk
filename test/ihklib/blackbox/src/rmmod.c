#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include "util.h"
#include "okng.h"
#include "init_fini.h"

static int mod_loaded(const char *name)
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

int rmmod(int verbose)
{
	int ret;
	char cmd[1024];
	char name[1024];

	ret = mod_loaded("mcctrl");
	INTERR(ret < 0, "mod_loaded mcctrl returned %d\n", ret);

	if (ret == 1) {
		sprintf(cmd, "rmmod %s/kmod/mcctrl.ko", QUOTE(WITH_MCK));
		if (verbose)
			INFO("%s\n", cmd);
		ret = system(cmd);
		ret = WEXITSTATUS(ret);
		if (ret != 0) {
			INFO("%s returned %d\n", cmd, ret);
		}
	}

	sprintf(name, "ihk_%s", QUOTE(KMOD_POSTFIX));
	ret = mod_loaded(name);
	INTERR(ret < 0, "mod_loaded %s returned %d\n",
	       name, ret);

	if (ret == 1) {
		sprintf(cmd, "rmmod %s/kmod/ihk-%s.ko",
			QUOTE(WITH_MCK), QUOTE(BUILD_TARGET));
		if (verbose)
			INFO("%s\n", cmd);
		ret = system(cmd);
		ret = WEXITSTATUS(ret);
		if (ret != 0) {
			INFO("%s returned %d\n", cmd, ret);
		}
	}

	ret = mod_loaded("ihk");
	INTERR(ret < 0, "mod_loaded ihk returned %d\n", ret);

	if (ret == 1) {
		sprintf(cmd, "rmmod %s/kmod/ihk.ko", QUOTE(WITH_MCK));
		if (verbose)
			INFO("%s\n", cmd);
		ret = system(cmd);
		ret = WEXITSTATUS(ret);
		if (ret != 0) {
			INFO("%s returned %d\n", cmd, ret);
		}
	}
out:
	return ret;
}
