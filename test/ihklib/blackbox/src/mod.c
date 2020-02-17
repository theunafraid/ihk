#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include "util.h"
#include "okng.h"
#include "init_fini.h"

int insmod(uid_t uid, gid_t gid)
{
	int ret;
	char cmd[1024];

	sprintf(cmd, "insmod %s/kmod/ihk.ko", QUOTE(WITH_MCK));
	INFO("%s\n", cmd);
	ret = system(cmd);
	ret = WEXITSTATUS(ret);
	NG(ret == 0, "%s returned %d\n", cmd, ret);

	sprintf(cmd,
		"insmod %s/kmod/ihk-%s.ko ihk_start_irq=240 ihk_ikc_irq_core=0",
		QUOTE(WITH_MCK), QUOTE(BUILD_TARGET));
	INFO("%s\n", cmd);
	ret = system(cmd);
	ret = WEXITSTATUS(ret);
	NG(ret == 0, "%s returned %d\n", cmd, ret);

#if 0
	sprintf(cmd, "chown %d:%d /dev/mcd*", uid, gid);
	INFO("%s\n", cmd);
	ret = system(cmd);
	ret = WEXITSTATUS(ret);
	NG(ret == 0, "%s returned %d\n", cmd, ret);
#endif

	sprintf(cmd, "insmod %s/kmod/mcctrl.ko", QUOTE(WITH_MCK));
	INFO("%s\n", cmd);
	ret = system(cmd);
	ret = WEXITSTATUS(ret);
	NG(ret == 0, "%s returned %d\n", cmd, ret);

out:
	return ret;
}
