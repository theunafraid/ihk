#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include "util.h"
#include "okng.h"
#include "init_fini.h"

static int mod_loaded(const char *expected) {
	int count = 0; 
	int ret = 0;
	FILE *st = NULL;
	char cmd[1024];
	char module[64];

	sprintf(cmd, "lsmod | awk '{print $1}' | grep %s", expected);
	
	if ((st = popen(cmd, "r")) == NULL) {
		INFO("%s Failed\n", cmd);
		goto out;
	}
	while ((fscanf(st, "%s", module)) > 0) {
		if (strcmp(expected, module) == 0) {
			ret = 1;
			break;
		}
		count = strlen(module);
		fseek(st, count, SEEK_CUR);
	}	
	
	pclose(st);
out:	
	return ret;
}

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
