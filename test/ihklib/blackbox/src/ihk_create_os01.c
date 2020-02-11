#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "params.h"
#include "init_fini.h"
#include <unistd.h>

const char param[] = "/dev/mcd0";
const char *values[] = {
	"No /dev/mcd0 exists"
	"/dev/mcd0 exists",
};

int main(int argc, char **argv)
{
	int ret;
	int i;

	params_getopt(argc, argv);

	int ret_expected[2] = { -ENOENT, 0 };
	int ret_expected_os_instances[2] = { -ENOENT, 1 };

	for (i = 0; i < 2; i++) {
		ret = ihk_create_os(0);
		OKNG(ret == ret_expected[i],
		     "return value (os index when positive): %d, expected: %d\n",
		     ret, ret_expected[i]);

		if (ret_expected[i] == 0) {
			ret = ihk_get_num_os_instances(0);
			OKNG(ret == ret_expected_os_instances[i],
			     "# of os instances: %d, expected: %d\n",
			     ret, ret_expected_os_instances[i]);
		}

		/* Precondition */
		if (i == 0) {
			ret = insmod(params.uid, params.gid);
			INTERR(ret, "insmod returned %d\n", ret);
		}
	}

out:
	rmmod(0);
	return ret;
}

