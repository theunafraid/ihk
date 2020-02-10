#include <errno.h>
#include <ihklib.h>
#include "util.h"
#include "okng.h"
#include "params.h"
#include "init_fini.h"
#include <unistd.h>

const char param[] = "/dev/mcd0";
const char *values[] = {
	"with no os instance",
	"with one os instance",
};

int main(int argc, char **argv)
{
	int ret;
	int i;

	params_getopt(argc, argv);

	ret = insmod(params.uid, params.gid);
	INTERR(ret, "insmod returned %d\n", ret);

	int ret_expected[2] = { 0, 1 };

	for (i = 0; i < 2; i++) {
		ret = ihk_get_num_os_instances(0);
		OKNG(ret == ret_expected[i],
		     "# of os instances: %d, expected: %d\n",
		     ret, ret_expected[i]);

		/* Precondition */
		if (i == 0) {
			ret = ihk_create_os(0);
			INTERR(ret, "ihk_create_os returned %d\n", ret);
		}
	}

out:
	rmmod(0);
	return ret;
}
