#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "util.h"
#include "okng.h"
#include "user.h"

int user_fork_exec(char *filename, pid_t *pid)
{
	int ret;
	char cmd[4096];
	char *argv[2] = { 0 };
	int status;

	ret = fork();
	if (ret == 0) {
		sprintf(cmd, "%s/bin/mcexec %s/bin/%s",
			QUOTE(WITH_MCK),
			QUOTE(CMAKE_INSTALL_PREFIX),
			filename);
		ret = system(cmd);
		status = WEXITSTATUS(ret);
		INFO("%s: child exited with status of %d\n",
		     __func__, status);
		exit(status);
#if 0
		argv[0] = cmd;
		ret = execve(argv[0], argv, environ);
		if (ret) {
			printf("%s:%s execve: errno: %d, cmd: %s\n",
			       __FILE__, __LINE__, errno, cmd);
			exit(errno);
		}
#endif
	}

	*pid = ret;
 out:
	return ret;
}

int user_wait(pid_t *pid)
{
	int ret;
	int i;
	int wstatus;

	for (i = 0; i < 2; i++) {
		ret = waitpid(*pid, &wstatus, WNOHANG);
		if (ret > 0) {
			if (ret != *pid) {
				printf("%s:%s waitpid returned %d\n",
				       __FILE__, __LINE__, ret);
				ret = -EINVAL;
				goto out;
			}
			INFO("process with pid %d exited with status %d\n",
			     *pid, WEXITSTATUS(wstatus));
			*pid = -1;
			ret = 0;
			goto out;
		}

		if (ret == 0) {
			kill(*pid, 9);
			continue;
		}

		if (ret < 0) {
			int errno_save = errno;

			printf("%s:%s waitpid: errno: %d\n",
			       __FILE__, __LINE__, errno_save);
			ret = -errno_save;
			goto out;
		}
	}
 out:
	return ret;
}
