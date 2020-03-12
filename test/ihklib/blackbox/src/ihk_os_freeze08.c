#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <ihklib.h>
#include <ihk/ihklib_private.h>
#include "util.h"
#include "okng.h"
#include "cpu.h"
#include "mem.h"
#include "os.h"
#include "user.h"
#include "params.h"
#include "linux.h"

const char param[] = "LWK status";
const char *messages[] = {
	"IHK_STATUS_INACTIVE"
	" (before smp_ihk_os_boot or after smp_ihk_destroy_os)",
	"IHK_STATUS_RUNNING (after done_init)",
	"IHK_STATUS_SHUTDOWN (smp_ihk_os_shutdown -- smp_ihk_destroy_os)",
	"IHK_STATUS_PANIC",
	"IHK_STATUS_HUNGUP",
	"IHK_STATUS_FREEZING",
	"IHK_STATUS_FROZEN"
};

#define MAX_COUNT 10

static int user_poll_fifo(int fd_fifo)
{
	int ret;
	pid_t pid;
	int fd_poll = -1;
	int fd_event = -1;
	struct epoll_event event = { 0 };
	struct epoll_event events[1];
	int nfd;
	int i;
	int count = 0;

	fd_poll = epoll_create1(0);
	INTERR(fd_poll == -1, "epoll_create returned %d\n", errno);

	event.data.fd = fd_fifo;
	event.events = EPOLLIN;

	ret = epoll_ctl(fd_poll, EPOLL_CTL_ADD, fd_fifo, &event);
	INTERR(ret, "epoll_ctl returned %d\n", errno);

 redo:
	nfd = epoll_wait(fd_poll, events, 1,
			 1000 * MAX_COUNT * 0.3);

	if (nfd < 0) {
		int errno_save = errno;

		if (errno == EINTR) {
			goto redo;
		}

		printf("%s: epoll_wait returned %d\n",
		       __func__, errno_save);
		ret = -errno_save;
		goto out;
	}

	if (nfd == 0) {
		INFO("%s: epoll_wait timeout\n", __func__);

		ret = -ETIME;
		goto out;
	}

	for (i = 0; i < nfd; i++) {
		if (events[i].data.fd == fd_fifo) {
			int messages[MAX_COUNT];
			ssize_t ret;

			ret = read(events[i].data.fd, messages,
				   sizeof(int) * MAX_COUNT);

			if (ret == -1) {
				int errno_save = errno;

				printf("%s: read returned %d\n",
				       __func__, errno_save);
				ret = -errno_save;
				goto out;
			}

			if (ret == 0) {
				printf("%s: EOF detected\n",
				       __func__);
				goto out;
			}

			count += ret / sizeof(int);
		}
	}

	ret = count;
 out:
	if (fd_poll != -1) {
		close(fd_poll);
	}

	//INFO("%s: count: %d, ret %d\n", __func__, count, ret);
	return ret;
}

int main(int argc, char **argv)
{
	int ret;
	int i, j;
	pid_t pid_status = -1;
	pid_t pid_count = -1;
	int fd_fifo;
	int fd;
	char *fn;

	params_getopt(argc, argv);

	/* Parse additional options */
	int opt;

	while ((opt = getopt(argc, argv, "f:")) != -1) {
		switch (opt) {
		case 'f':
			fn = optarg;
			break;
		default: /* '?' */
			printf("unknown option %c\n", optopt);
			exit(1);
		}
	}

	enum ihklib_os_status target_status[] = {
		IHK_STATUS_INACTIVE,
		IHK_STATUS_RUNNING,
		IHK_STATUS_SHUTDOWN, /* shutting-down */
		IHK_STATUS_PANIC,
		IHK_STATUS_HUNGUP,
		IHK_STATUS_FREEZING,
		IHK_STATUS_FROZEN,
	};

	int ret_expected[] = {
		-EINVAL,
		0,
		-EINVAL,
		-EINVAL,
		-EINVAL,
		-EBUSY,
		-EBUSY,
	};

	/* Precondition */
	ret = linux_insmod(0);
	INTERR(ret, "linux_insmod returned %d\n", ret);

	ret = cpus_reserve();
	INTERR(ret, "cpus_reserve returned %d\n", ret);

	ret = mems_reserve();
	INTERR(ret, "mems_reserve returned %d\n", ret);

	unsigned long os_set[1] = { 1 };

	/* Activate and check */
	for (i = 0; i < 7; i++) {
		int word = 1;

		START("test-case: %s: %s\n", param, messages[i]);

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

		switch (target_status[i]) {
		case IHK_STATUS_INACTIVE:
			break;
		case IHK_STATUS_RUNNING:
		case IHK_STATUS_SHUTDOWN:
		case IHK_STATUS_PANIC:
		case IHK_STATUS_HUNGUP:
		case IHK_STATUS_FREEZING:
		case IHK_STATUS_FROZEN:
			ret = ihk_os_boot(0);
			INTERR(ret, "ihk_os_boot returned %d\n", ret);
			break;
		default:
			break;
		}

		switch (target_status[i]) {
		case IHK_STATUS_SHUTDOWN:
			pid_status = fork();
			if (!pid_status) {
				ret = ihk_os_shutdown(0);
				if (ret) {
					printf("child: ihk_os_shutdown "
					       "returned %d\n", ret);
				}
				exit(ret);
			}
			break;
		case IHK_STATUS_PANIC:
			ret = user_fork_exec("panic", &pid_status);
			INTERR(ret < 0, "user_fork_exec returned %d\n");
			break;
		case IHK_STATUS_HUNGUP:
			ret = user_fork_exec("hungup", &pid_status);
			INTERR(ret < 0, "user_fork_exec returned %d\n");

			/* wait until McKernel start ihk_mc_delay_us() */
			usleep(0.25 * 1000000);

			fd = ihklib_os_open(0);
			INTERR(fd < 0, "ihklib_os_open returned %d\n",
			       fd);

			ioctl(fd, IHK_OS_DETECT_HUNGUP);
			usleep(0.25 * 1000000);
			ioctl(fd, IHK_OS_DETECT_HUNGUP);

			close(fd);
			break;
		case IHK_STATUS_FREEZING:
		case IHK_STATUS_FROZEN:
			ihk_os_freeze(os_set, sizeof(unsigned long) * 8);
			break;
		default:
			break;
		}

		/* wait until os status changes to the target status */
		ret = os_wait_for_status(target_status[i]);
		INTERR(ret, "os status didn't change to %d\n",
		       target_status[i]);

		if (target_status[i] == IHK_STATUS_RUNNING) {
			char cmd[4096];

			fd_fifo = open(fn, O_RDWR);
			INTERR(fd_fifo == -1, "open returned %d\n", errno);

			sprintf(cmd, "count %s", fn);
			ret = user_fork_exec(cmd, &pid_count);
			INTERR(ret < 0, "user_fork_exec returned %d\n", ret);

			/* Start couting */
			ret = write(fd_fifo, &word, sizeof(int));
			INTERR(ret != sizeof(int),
			       "write returned %d\n", errno);

			/* Wait until few messages sent from child */
			usleep(2000000);
		}

		INFO("trying to freeze...\n");

		ret = ihk_os_freeze(os_set, sizeof(unsigned long) * 8);
		OKNG(ret == ret_expected[i],
		     "return value: %d, expected: %d\n",
		     ret, ret_expected[i]);

		if (target_status[i] == IHK_STATUS_RUNNING) {
			int count = 0;
			int wstatus;
			char exit_status;

			/* Consume messages sent before getting
			 * frozen
			 */
			while ((ret = user_poll_fifo(fd_fifo)) > 0) {
				count += ret;
				INFO("count: %d\n", count);
			}

			/* epoll on pipe should eventually time out */
			OKNG(ret == -ETIME, "target process gets silent "
			     "as expected, ret: %d\n", ret);

			INFO("trying to thaw...\n");
			ret = ihk_os_thaw(os_set, sizeof(unsigned long) * 8);
			INTERR(ret, "ihk_os_thaw returned %d\n", ret);

			/* Consume remaining messages */
			while ((ret = user_poll_fifo(fd_fifo)) > 0) {
				count += ret;
				INFO("count: %d\n", count);
			}
			OKNG(ret == -ETIME && count == MAX_COUNT,
			     "all messages received\n");

			ret = waitpid(pid_count, &wstatus, 0);
			exit_status = WEXITSTATUS(wstatus);
			INTERR(ret < 0 || exit_status != 0,
			       "waitpid returned %d, exit status: %d\n",
			       errno, exit_status);
			pid_count = -1;

			close(fd_fifo);
		}

		/* Clean up */

		/* Wait until status stabilizes */
		switch (target_status[i]) {
		case IHK_STATUS_SHUTDOWN:
			ret = os_wait_for_status(IHK_STATUS_INACTIVE);
			INTERR(ret, "os_wait_for_status returned %d\n", ret);
			break;
		case IHK_STATUS_FREEZING:
			ret = os_wait_for_status(IHK_STATUS_FROZEN);
			INTERR(ret, "os_wait_for_status returned %d\n", ret);
			break;
		default:
			break;
		}

		/* Thaw when frozen */
		if (ihk_os_get_status(0) == IHK_STATUS_FROZEN) {
			ret = ihk_os_thaw(os_set, sizeof(unsigned long) * 8);
			INTERR(ret, "ihk_os_thaw returned %d\n", ret);
		}

		switch (target_status[i]) {
		case IHK_STATUS_SHUTDOWN:
		case IHK_STATUS_HUNGUP:
		case IHK_STATUS_PANIC:
			ret = user_wait(&pid_status);
			INTERR(ret, "user_wait returned %d\n", ret);
			break;
		default:
			break;
		}

		linux_kill_mcexec();

		if (ihk_os_get_status(0) != IHK_STATUS_INACTIVE) {
			ret = ihk_os_shutdown(0);
			INTERR(ret, "ihk_os_shutdown returned %d\n", ret);

			ret = os_wait_for_status(IHK_STATUS_INACTIVE);
			INTERR(ret, "os_wait_for_status returned %d\n", ret);
		}

		ret = cpus_os_release();
		INTERR(ret, "cpus_os_release returned %d\n", ret);

		ret = mems_os_release();
		INTERR(ret, "mems_os_release returned %d\n", ret);

		ret = ihk_destroy_os(0, 0);
		INTERR(ret, "ihk_destroy_os returned %d\n", ret);
	}

	ret = 0;
 out:
	if (pid_status > 0) {
		user_wait(&pid_status);
	}

	if (ihk_get_num_os_instances(0)) {
		unsigned long os_set[1] = { 1 };

		switch (ihk_os_get_status(0)) {
		case IHK_STATUS_SHUTDOWN:
			os_wait_for_status(IHK_STATUS_INACTIVE);
			break;
		case IHK_STATUS_FREEZING:
			os_wait_for_status(IHK_STATUS_FROZEN);
			break;
		default:
			break;
		}

		if (ihk_os_get_status(0) == IHK_STATUS_FROZEN) {
			ihk_os_thaw(os_set, sizeof(unsigned long) * 8);
		}

		if (pid_count > 0) {
			user_wait(&pid_count);
		}

		if (pid_status > 0) {
			user_wait(&pid_status);
		}

		linux_kill_mcexec();

		if (ihk_os_get_status(0) != IHK_STATUS_INACTIVE) {
			ihk_os_shutdown(0);
			os_wait_for_status(IHK_STATUS_INACTIVE);
		}

		ihk_destroy_os(0, 0);
	}
	cpus_release();
	mems_release();
	linux_rmmod(0);

	return ret;
}

