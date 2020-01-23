#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include "util.h"
#include "okng.h"
#include "init_fini.h"

int rmmod(int verbose)
{
	int ret;
	char cmd[1024];

	sprintf(cmd, "rmmod %s/kmod/mcctrl.ko", QUOTE(WITH_MCK));
	if (verbose)
		INFO("%s\n", cmd);
	ret = system(cmd);
	ret = WEXITSTATUS(ret);
	if (ret != 0) {
		INFO("%s returned %d\n", cmd, ret);
	}

	sprintf(cmd, "rmmod %s/kmod/ihk-%s.ko",
		QUOTE(WITH_MCK), QUOTE(BUILD_TARGET));
	if (verbose)
		INFO("%s\n", cmd);
	ret = system(cmd);
	ret = WEXITSTATUS(ret);
	if (ret != 0) {
		INFO("%s returned %d\n", cmd, ret);
	}

	sprintf(cmd, "rmmod %s/kmod/ihk.ko", QUOTE(WITH_MCK));
	if (verbose)
		INFO("%s\n", cmd);
	ret = system(cmd);
	ret = WEXITSTATUS(ret);
	if (ret != 0) {
		INFO("%s returned %d\n", cmd, ret);
	}

out:
	return ret;
}
