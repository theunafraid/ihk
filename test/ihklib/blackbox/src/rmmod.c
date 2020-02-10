#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
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
		INFO("%s Failed\n", cmd);
		goto out;
	}
	ret = fscanf(st, "%d\n", &count);
	pclose(st);

out:
	return count;
}

int rmmod(int verbose)
{
	int ret;
	char cmd[1024];
	char name[1024];

	if (mod_loaded("mcctrl")) {

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
	if (mod_loaded(name)) {

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

	if (mod_loaded("ihk")) {
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
