/**
 * \file ihklib.c
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <bfd.h>
#include <inttypes.h>
#include <time.h>
#include <limits.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/eventfd.h>
#include <sys/time.h>
#include <linux/limits.h>
#include <sched.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <config.h>
#include <ihk/ihk_host_user.h>
#include <ihk/ihklib.h>
#include <ihk/ihklib_private.h>

int __argc;
char **__argv;

int loglevel = IHKLIB_LOGLEVEL_ERR;

//#define DEBUG

#ifdef DEBUG
#define dprintf(fmt, args...) do {	\
	printf(fmt, ##args);		\
} while (0)

#define dprintk(fmt, args...) do {					\
	char contents[4096 - 256];					\
	int fd;								\
	ssize_t len;							\
	ssize_t offset = 0;						\
	if (geteuid()) {						\
		break;							\
	}								\
	sprintf(contents, fmt, ##args);					\
	fd = open("/dev/kmsg", O_WRONLY);				\
	len = strlen(contents) + 1;					\
	while (offset < len) {						\
		offset += write(fd, contents + offset, len - offset);	\
	}								\
	close(fd);							\
} while (0)

#else
#define dprintf(fmt, args...) do {  } while (0)
#define dprintk(fmt, args...) do {  } while (0)
#endif

#define eprintf(fmt, args...) do {		\
	if (loglevel >= IHKLIB_LOGLEVEL_ERR) {	\
		fprintf(stderr, fmt, ##args);	\
	}					\
} while (0)


#define PHYSMEM_NAME_SIZE 32

#define CHKANDJUMP(cond, err, fmt, args...) do {	\
	if (cond) {					\
		ret = err;				\
		dprintf(fmt, ##args);			\
		goto out;				\
	}						\
} while(0)


struct namespace_file namespace_files[] = {
	{ .nstype = CLONE_NEWUSER,	.name = "ns/user", .fd = -1 },
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)
	{ .nstype = CLONE_NEWCGROUP,	.name = "ns/cgroup", .fd = -1 },
#endif
	{ .nstype = CLONE_NEWIPC,	.name = "ns/ipc", .fd = -1 },
	{ .nstype = CLONE_NEWUTS,	.name = "ns/uts", .fd = -1 },
	{ .nstype = CLONE_NEWNET,	.name = "ns/net", .fd = -1 },
	{ .nstype = CLONE_NEWPID,	.name = "ns/pid", .fd = -1 },
	{ .nstype = CLONE_NEWNS,	.name = "ns/mnt", .fd = -1 },
	{ .nstype = 0, .name = NULL, .fd = -1 }
};

struct ihklib_reserve_mem_conf reserve_mem_conf = {
	.total = 0,
	.variance_limit = 0,
	.min_chunk_size = PAGE_SIZE,
	.max_size_ratio_all = 100,
	.timeout = 30,
};

static int snprintf_realloc(char **str, size_t *size,
		size_t offset, const char *format, ...)
{
	int ret, needed;
	char *tmp;
	va_list ap;

	va_start(ap, format);
	while (0 <= (ret = vsnprintf(*str + offset, *size - offset,
				format, ap))
		  && *size - offset < (needed = ret + 1)) {
		va_end(ap);

		while (*size - offset < needed) {
			*size *= 2;
		}

		tmp = realloc(*str, *size);

		if (tmp) {
			*str = tmp;
		} else {
			free(*str);
			*str = NULL;
			return -1;
		}
		va_start(ap, format);
	}
	va_end(ap);

	return ret;
}

/* Return number of CPUs on success, negative on failure */
static int cpu_str2array(char *_cpu_list, int num_cpus, int *cpus)
{
	int ret = 0;
	int i;
	int cpu_rank = 0;
	char *cpu_list, *to_free = NULL;
	char *token, *minus;

	if (!_cpu_list) {
		/* nothing to do */
		ret = 0;
		goto out;
	}

	if (!(cpu_list = strdup(_cpu_list))) {
		ret = -errno;
		dprintf("%s: error: allocating cpu_list\n",
			__func__);
		goto out;
	}
	to_free = cpu_list;

	token = strsep(&cpu_list, ",");
	while (token) {
		if (*token == 0) {
			eprintf("%s: error: illegal expression: %s\n",
				__func__, _cpu_list); /* empty token */
			ret = -EINVAL;
			goto out;
		}
		if ((minus = strchr(token, '-'))) {
			int start, end;

			if (*(minus + 1) == 0) {
				eprintf("%s: error: illegal expression: %s\n",
					__func__, _cpu_list); /* empty token */
				ret = -EINVAL;
				goto out;
			}
			*minus = 0;
			start = atoi(token);
			end = atoi(minus + 1);
			for (i = start; i <= end; i++) {
				dprintf("%s: cpus[%d]=%d\n",
					__func__, cpu_rank, i);
				if (cpus && num_cpus > cpu_rank) {
					cpus[cpu_rank] = i;
				}
				cpu_rank++;
			}
		}
		else {
			dprintf("%s: cpus[%d]=%d\n",
				__func__, cpu_rank, atoi(token));
			if (cpus && num_cpus > cpu_rank) {
				cpus[cpu_rank] = atoi(token);
			}
			cpu_rank++;
		}
		token = strsep(&cpu_list, ",");
	}

	ret = cpu_rank;

 out:
	free(to_free);
	return ret;
}

int cpu_str2count(char *cpu_list)
{
	return cpu_str2array(cpu_list, 0, NULL);
}

/* Return number of CPUs on success, negative on failure */
int cpu_str2req(char *_cpu_list, int num_cpus, struct ihk_cpu_req *req)
{
	int ret = 0;

	if (!req) {
		eprintf("%s: error: invalid req pointer (NULL)\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	ret = cpu_str2array(_cpu_list, num_cpus, req->cpus);
	req->num_cpus = ret;

 out:
	return ret;
}

static int mem_str2array(char *mem_list, int *num_mem_chunks,
			struct ihk_mem_chunk *mem_chunks)
{
	int ret = 0;
	int mem_count = 0;
	char *chunk = mem_list;
	char *token = strsep(&chunk, ",");
	while (token != NULL) {
		if(*token == 0) {
			goto empty_mem;
		}
		char* cdr = token;
		token = strsep(&cdr, "@");
		if (mem_chunks && *num_mem_chunks > mem_count) {
			mem_chunks[mem_count].size = atol(token);
			if (cdr != NULL) {
				mem_chunks[mem_count].numa_node_number = atol(cdr);
			}
		}
		mem_count++;
	empty_mem:
		token = strsep(&chunk, ",");
	}

	ret = mem_count;

    return ret;
}

/* Return number of maps on success, negative on failure */
static int ikc_str2array(char *_ikc_list, int num_maps,
		int *src_cpus, int *dst_cpus)
{
	int ret = 0;
	int i;
	int token_cnt = 0, total_cnt = 0;
	int cpu_buf[IHK_MAX_NUM_CPUS] = {0};
	char *ikc_list, *to_free = NULL;
	char *token;

	if (!_ikc_list) {
		/* nothing to do */
		ret = 0;
		goto out;
	}

	if (!(ikc_list = strdup(_ikc_list))) {
		ret = -errno;
		dprintf("%s: error: allocating ikc_list\n",
			__func__);
		goto out;
	}
	to_free = ikc_list;

	token = strsep(&ikc_list, "+");
	while (token) {
		char *cpu_list;
		char *ikc_cpu;
		int dst_cpu;

		cpu_list = strsep(&token, ":");
		if (!cpu_list) {
			ret = -EINVAL;
			goto out;
		}

		token_cnt = cpu_str2array(cpu_list, IHK_MAX_NUM_CPUS, cpu_buf);

		ikc_cpu = strsep(&token, ":");
		if (!ikc_cpu) {
			ret = -EINVAL;
			goto out;
		}

		dst_cpu = atoi(ikc_cpu);

		/* Store IKC target CPU */
		for (i = 0; i < token_cnt; i++) {
			if (src_cpus && dst_cpus && num_maps > total_cnt) {
				src_cpus[total_cnt] = cpu_buf[i];
				dst_cpus[total_cnt] = dst_cpu;
			}
			total_cnt++;
		}

		token = strsep(&ikc_list, "+");
	}

	ret = total_cnt;

 out:
	free(to_free);
	return ret;
}

int ikc_str2count(char *_ikc_list)
{
	return ikc_str2array(_ikc_list, 0, NULL, NULL);
}

int ikc_str2req(char *_ikc_list, int num_cpus, struct ihk_ikc_req *req)
{
	int ret = 0;

	if (!req) {
		eprintf("%s: error: invalid req pointer (NULL)\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	ret = ikc_str2array(_ikc_list, num_cpus, req->src_cpus, req->dst_cpus);
	req->num_cpus = ret;

 out:
	return ret;
}


#define IHK_SMP_MEM_ALL	(-1UL)
static size_t ihk_memparse(char *token)
{
	size_t ret;
	char *endp = token + strlen(token) - 1;

	/* "all" or "ALL" indicates best effort allocation */
	if (!strcmp("all", token) || !strcmp("ALL", token)) {
		ret = IHK_SMP_MEM_ALL;
		goto out;
	}

	ret = atol(token);

	switch (*endp) {
	case 'e':
	case 'E':
		ret <<= 10;
	case 'p':
	case 'P':
		ret <<= 10;
	case 't':
	case 'T':
		ret <<= 10;
	case 'g':
	case 'G':
		ret <<= 10;
	case 'm':
	case 'M':
		ret <<= 10;
	case 'k':
	case 'K':
		ret <<= 10;
	default:
		// do nothing
		break;
	}

out:
	return ret;
}

/* Return number of MEM chunks on success, negative on failure */
int mem_str2req(char *_mem_list, int num_mem_chunks, struct ihk_mem_req *req)
{
	int ret = 0;
	int mem_count = 0;
	char *mem_list, *to_free = NULL;
	char *token, *cdr;

	if (!_mem_list) {
		/* nothing to do */
		ret = 0;
		goto out;
	}

	if (!(mem_list = strdup(_mem_list))) {
		ret = -errno;
		dprintf("%s: error: allocating mem_list\n",
			__func__);
		goto out;
	}
	to_free = mem_list;

	token = strsep(&mem_list, ",");
	while (token) {
		if (*token == 0) {
			eprintf("%s: error: illegal expression: %s\n",
				__func__, _mem_list); /* empty token */
			ret = -EINVAL;
			goto out;
		}
		cdr = token;
		token = strsep(&cdr, "@");
		if (req && num_mem_chunks > mem_count) {
			req->sizes[mem_count] = ihk_memparse(token);
			if (cdr != NULL) {
				req->numa_ids[mem_count] = atol(cdr);
			}
		}
		mem_count++;

		token = strsep(&mem_list, ",");
	}

	if (req) {
		req->num_chunks = mem_count;
	}

	ret = mem_count;

 out:
	free(to_free);
	return ret;
}

int mem_str2count(char *mem_list)
{
	return mem_str2req(mem_list, 0, NULL);
}

static char *cpu_array2str(int num_cpus, int *cpus)
{
	/* prev_cpu should be < -1 so that "if (prev_cpu != cpus[i] - 1)"
	 * won't misunderstand that the cursor is pointing to "0"
	 * following "-1".
	 */
	int i, prev_cpu = -10, in_seq = 0, n = 0;
	size_t buflen = 64;
	char *str = NULL;

	str = malloc(buflen);
	if (!str) {
		goto out;
	}

	memset(str, 0, buflen);

	for (i = 0; i < num_cpus; i++) {
		if (prev_cpu != cpus[i] - 1) {
			if (prev_cpu > 0) {
				n += snprintf_realloc(&str, &buflen, n,
					"%d,", prev_cpu);
			}
			in_seq = 0;
		}
		else {
			if (!in_seq) {
				n += snprintf_realloc(&str, &buflen, n,
					"%d-", prev_cpu);
				in_seq = 1;
			}
		}

		prev_cpu = cpus[i];
	}

	if (prev_cpu >= 0) {
		n += snprintf_realloc(&str, &buflen, n,
			"%d", prev_cpu);
	}

 out:
	return str;
}

char *cpu_req2str(struct ihk_cpu_req *req)
{
	return cpu_array2str(req->num_cpus, req->cpus);
}

static char *mem_array2str(int num_mem_chunks, size_t *sizes, int *numa_ids)
{
	int i, n = 0;
	size_t buflen = 64;
	char *str = NULL;

	str = malloc(buflen);
	if (!str) {
		goto out;
	}

	memset(str, 0, buflen);

	for (i = 0; i < num_mem_chunks; i++) {
		n += snprintf_realloc(&str, &buflen, n,
			"%lu@%d", sizes[i], numa_ids[i]);
		if (i != num_mem_chunks - 1) {
			n += snprintf_realloc(&str, &buflen, n, ",");
		}
	}

 out:
	return str;
}

char *mem_req2str(struct ihk_mem_req *req)
{
	return mem_array2str(req->num_chunks, req->sizes, req->numa_ids);
}

char *ikc_req2str(struct ihk_ikc_req *req)
{
	int i, src, dst, max_dst = -1, idx, n = 0;
	char *str = NULL;
	size_t buflen = 64;

	/* Sender-set (sset): Set of senders sharing the same destination */
	int *rank = NULL; /* Order in sender-set, indexed by IKC source CPU# */
	int *ikc_sset_sizes = NULL; /* Indexed by IKC destination CPU# */
	int **ikc_sset_members = NULL; /* Indexed by IKC destination CPU# */

	str = malloc(buflen);
	if (!str) {
		goto out;
	}

	memset(str, 0, buflen);

	rank = calloc(sizeof(int), IHK_MAX_NUM_CPUS);
	if (!rank) {
		eprintf("%s: error: allocating rank\n", __func__);
		goto out;
	}

	ikc_sset_sizes = calloc(sizeof(int), IHK_MAX_NUM_CPUS);
	if (!ikc_sset_sizes) {
		eprintf("%s: error: allocating num_ikc_ssets\n", __func__);
		goto out;
	}

	ikc_sset_members = calloc(sizeof(int *), IHK_MAX_NUM_CPUS);
	if (!ikc_sset_members) {
		eprintf("%s: error: allocating ikc_sset_members\n", __func__);
		goto out;
	}

	for (idx = 0; idx < req->num_cpus; idx++) {
		src = req->src_cpus[idx];
		dst = req->dst_cpus[idx];

		rank[src] = ikc_sset_sizes[dst];
		ikc_sset_sizes[dst]++;
		if (max_dst < dst) {
			max_dst = dst;
		}
	}

	for (idx = 0; idx < req->num_cpus; idx++) {
		src = req->src_cpus[idx];
		dst = req->dst_cpus[idx];

		if (!ikc_sset_members[dst]) {
			ikc_sset_members[dst] = calloc(sizeof(int),
					ikc_sset_sizes[dst]);
			if (!ikc_sset_members[dst]) {
				eprintf("%s: error: allocating ikc_sset_members\n",
					__func__);
				goto out;
			}
		}
		*(ikc_sset_members[dst] + rank[src]) = src;
	}

	for (dst = 0; dst < IHK_MAX_NUM_CPUS; dst++) {
		if (ikc_sset_sizes[dst] == 0) {
			continue;
		}

		for (i = 0; i < ikc_sset_sizes[dst]; i++) {
			n += snprintf_realloc(&str, &buflen, n,
				"%d", *(ikc_sset_members[dst] + i));
			if (i != ikc_sset_sizes[dst] - 1) {
				n += snprintf_realloc(&str, &buflen, n, ",");
			}
		}
		n += snprintf_realloc(&str, &buflen, n, ":%d", dst);
		if (dst != max_dst) {
			n += snprintf_realloc(&str, &buflen, n, "+");
		}
	}

	dprintf("get_ikc_map,query_res=%s\n", str);

out:
	if (ikc_sset_members) {
		for (dst = 0; dst < IHK_MAX_NUM_CPUS; ++dst) {
			free(ikc_sset_members[dst]);
		}
	}

	free(ikc_sset_members);
	free(ikc_sset_sizes);
	free(rank);

	return str;
}

static int ihklib_device_readable(int index)
{
	int ret;
	char fn[PATH_MAX];

	sprintf(fn, "/dev/mcd%d", index);
	ret = access(fn, R_OK);
	if (ret) {
		ret = -errno;
		dprintf("%s: error: access: path: %s, errno: %d\n",
			__func__, fn, -ret);
		goto out;
	}

	ret = 0;
 out:
	return ret;
}

int ihklib_device_open(int index)
{
	int ret;
	char fn[PATH_MAX];

	ret = ihklib_device_readable(index);
	if (ret) {
		dprintf("%s: error: ihklib_device_readable returned %d\n",
			__func__, ret);
		goto out;
	}

	sprintf(fn, "/dev/mcd%d", index);
	if ((ret = open(fn, O_RDONLY)) == -1) {
		ret = -errno;
		dprintf("%s: error: open %s: %s\n",
			__func__, fn, strerror(-ret));
		goto out;
	}

 out:
	return ret;
}

int ihk_reserve_cpu(int index, int* cpus, int num_cpus)
{
	int ret;
	struct ihk_ioctl_cpu_desc req = { 0 };
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	ret = ihklib_device_readable(index);
	if (ret) {
		goto out;
	}

	if (num_cpus < 0 || num_cpus > IHK_MAX_NUM_CPUS) {
		dprintf("%s: invalid number of cpus (%d)\n",
			__func__, num_cpus);
		ret = -EINVAL;
		goto out;
	}

	if (num_cpus != 0 && cpus == NULL) {
		ret = -EFAULT;
		goto out;
	}

	if (num_cpus == 0) {
		ret = 0;
		goto out;
	}

	req.cpus = cpus;
	req.num_cpus = num_cpus;

	if ((fd = ihklib_device_open(index)) < 0) {
		dprintf("%s: error: ihklib_device_open returned %d\n",
			__func__, fd);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_DEVICE_RESERVE_CPU, &req);
	if (ret) {
		ret = -errno;
		dprintf("%s: IHK_DEVICE_RESERVE_CPU returned %d\n",
			__func__, -ret);
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

int ihk_get_num_reserved_cpus(int index)
{
	int ret;
	int fd = -1;

	dprintk("%s: enter\n", __func__);
	if ((fd = ihklib_device_open(index)) < 0) {
		dprintf("%s: ihklib_device_open returned %d\n",
			__func__, fd);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_DEVICE_GET_NUM_CPUS);
	if (ret < 0) {
		ret = -errno;
		dprintf("%s: IHK_DEVICE_GET_NUM_CPUS returned %d\n",
			__func__, -ret);
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

int ihk_query_cpu(int index, int *cpus, int num_cpus)
{
	int ret;
	struct ihk_ioctl_cpu_desc req = { 0 };
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	ret = ihklib_device_readable(index);
	if (ret) {
		goto out;
	}

	if (num_cpus < 0 || num_cpus > IHK_MAX_NUM_CPUS) {
		dprintf("%s: invalid number of cpus (%d)\n",
			__func__, num_cpus);
		ret = -EINVAL;
		goto out;
	}

	if ((fd = ihklib_device_open(index)) < 0) {
		dprintf("%s: ihklib_device_open returned %d\n",
			__func__, fd);
		ret = fd;
		goto out;
	}

	if ((ret = ioctl(fd, IHK_DEVICE_GET_NUM_CPUS)) < 0) {
		ret = -errno;
		dprintf("%s: IHK_DEVICE_GET_NUM_CPUS returned %d\n",
			__func__, -ret);
		goto out;
	}

	if (ret != num_cpus) {
		dprintf("%s: error: actual # of cpus (%d) != requested (%d)\n",
			__func__, ret, num_cpus);
		ret = -EINVAL;
		goto out;
	}

	req.cpus = cpus;
	req.num_cpus = num_cpus;

	if ((ret = ioctl(fd, IHK_DEVICE_QUERY_CPU, &req))) {
		ret = -errno;
		dprintf("%s: error: IHK_DEVICE_QUERY_CPU returned %d\n",
			__func__, -ret);
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

int ihk_release_cpu(int index, int* cpus, int num_cpus)
{
	int ret;
	struct ihk_ioctl_cpu_desc req = { 0 };
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	ret = ihklib_device_readable(index);
	if (ret) {
		goto out;
	}

	if (num_cpus < 0 || num_cpus > IHK_MAX_NUM_CPUS) {
		dprintf("%s: invalid num_cpus: %d\n",
			__func__, num_cpus);
		ret = -EINVAL;
		goto out;
	}

	if (num_cpus != 0 && cpus == NULL) {
		ret = -EFAULT;
		goto out;
	}

	if (num_cpus == 0) {
		ret = 0;
		goto out;
	}

	req.cpus = cpus;
	req.num_cpus = num_cpus;

	if ((fd = ihklib_device_open(index)) < 0) {
		dprintf("%s: error: ihklib_device_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_DEVICE_RELEASE_CPU, &req);
	if (ret) {
		ret = -errno;
		dprintf("%s: IHK_DEVICE_RELEASE_CPU returned %d\n",
			__func__, -ret);
		goto out;
	}
 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

int ihk_reserve_mem_conf(int index, int key, void *value)
{
	int ret;

	ret = ihklib_device_readable(index);
	if (ret) {
		goto out;
	}

	switch (key) {
	case IHK_RESERVE_MEM_TOTAL:
		reserve_mem_conf.total = 1;
		reserve_mem_conf.variance_limit = *((int *)value);
		break;
	case IHK_RESERVE_MEM_MIN_CHUNK_SIZE:
		reserve_mem_conf.min_chunk_size = *((int *)value);
		break;
	case IHK_RESERVE_MEM_MAX_SIZE_RATIO_ALL:
		reserve_mem_conf.max_size_ratio_all = *((int *)value);
		break;
	case IHK_RESERVE_MEM_TIMEOUT:
		reserve_mem_conf.timeout = *((int *)value);
		break;
	default:
		ret = -EINVAL;
		goto out;
	}

	ret = 0;
 out:
	return ret;
}

int ihk_reserve_mem(int index, struct ihk_mem_chunk *mem_chunks,
		    int num_mem_chunks)
{
	int ret;
	int i;
	struct ihk_mem_req req = { 0 };
	int fd = -1;

	size_t total_requested = 0;
	int num_mem_chunks_reserved;
	struct ihk_mem_chunk *mem_chunks_reserved = NULL;
	size_t *reserved = NULL;
	int num_numa_nodes = 0;
	int num_numa_nodes_compensate = 0;
	int num_numa_nodes_release = 0;
	size_t ave_requested;
	size_t total_missing = 0, total_excess = 0;
	size_t ave_compensate;
	size_t total_missing2 = 0, total_excess2 = 0;
	size_t ave_compensate2;
	unsigned long min = (unsigned long)-1;
	unsigned long max = 0;
	unsigned long variance_limit;
	int release = 0;

	dprintk("%s: reserve_mem_conf.total=%d\n",
		__func__, reserve_mem_conf.total);

	ret = ihklib_device_readable(index);
	if (ret) {
		goto out;
	}

	if (num_mem_chunks < 0 || num_mem_chunks > IHK_MAX_NUM_MEM_CHUNKS) {
		dprintf("%s: error: invalid # of chunks (%d)\n",
			__func__, num_mem_chunks);
		ret = -EINVAL;
		goto out;
	}

	if (num_mem_chunks != 0 && mem_chunks == NULL) {
		ret = -EFAULT;
		goto out;
	}

	if (num_mem_chunks == 0) {
		ret = 0;
		goto out;
	}

	req.sizes = calloc(num_mem_chunks, sizeof(size_t));
	if (!req.sizes) {
		dprintf("%s: error: allocating req.sizes\n",
			__func__);
		ret = -ENOMEM;
		goto out;
	}

	req.numa_ids = calloc(num_mem_chunks, sizeof(int));
	if (!req.numa_ids) {
		dprintf("%s: error: allocating req.numa_ids\n",
			__func__);
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < num_mem_chunks; i++) {
		if (reserve_mem_conf.total) {
			req.sizes[i] = (size_t)IHK_SMP_MEM_ALL;
			total_requested += (size_t)mem_chunks[i].size;
		} else {
			req.sizes[i] = (size_t)mem_chunks[i].size;
		}
		req.numa_ids[i] = mem_chunks[i].numa_node_number;
	}
	req.num_chunks = num_mem_chunks;
	req.min_chunk_size = reserve_mem_conf.min_chunk_size;
	req.max_size_ratio_all = reserve_mem_conf.max_size_ratio_all;
	req.timeout = reserve_mem_conf.timeout;

	fd = ihklib_device_open(index);
	if (fd < 0) {
		ret = fd;
		printf("%s: ihklib_device_open returned %d\n",
		       __func__, fd);
		goto out;
	}

	ret = ioctl(fd, IHK_DEVICE_RESERVE_MEM, &req);
	if (ret != 0) {
		ret = -errno;
		dprintf("%s: IHK_DEVICE_RESERVE_MEM returned %d\n",
			__func__, -ret);
		goto out;
	}

	close(fd);

	if (reserve_mem_conf.total) {
		dprintk("%s: total requested: %ld\n",
			__func__, total_requested);

		num_mem_chunks_reserved =
			ihk_get_num_reserved_mem_chunks(index);
		mem_chunks_reserved = calloc(num_mem_chunks_reserved,
					  sizeof(struct ihk_mem_chunk));
		CHKANDJUMP(mem_chunks_reserved == NULL, -ENOMEM,
			   "failed to allocate mem_chunks_reserved\n");

		ret = ihk_query_mem(index, mem_chunks_reserved,
				    num_mem_chunks_reserved);
		CHKANDJUMP(ret, -EINVAL, "ihk_query_mem failed\n");

		reserved = calloc(IHK_MAX_NUM_NUMA_NODES, sizeof(size_t));
		CHKANDJUMP(reserved == NULL, -ENOMEM,
			   "failed to allocate reserved\n");

		for (i = 0; i < num_mem_chunks_reserved; i++) {
			reserved[mem_chunks_reserved[i].numa_node_number] +=
				mem_chunks_reserved[i].size;
		}

		for (i = 0; i < IHK_MAX_NUM_NUMA_NODES; i++) {
			if (reserved[i] == 0) {
				continue;
			}
			num_numa_nodes++;
		}

/* align reserve/release amount */
#define IHKLIB_RESERVE_AMOUNT_ALIGN (1UL << 20)

		/* round up not to release too much */
		ave_requested = ((total_requested / num_numa_nodes +
				  IHKLIB_RESERVE_AMOUNT_ALIGN - 1) /
				 IHKLIB_RESERVE_AMOUNT_ALIGN) *
			IHKLIB_RESERVE_AMOUNT_ALIGN;
		dprintk("%s: ave requested: %ld\n",
			__func__, ave_requested);

		/* Fill below-average-of-requested nodes upto the average */
		for (i = 0; i < IHK_MAX_NUM_NUMA_NODES; i++) {
			if (reserved[i] == 0) {
				continue;
			}
			dprintk("%s: node id: %d, reserved: %ld\n",
				__func__, i, reserved[i]);
			if (reserved[i] > ave_requested) {
				num_numa_nodes_compensate++;
				total_excess += reserved[i] - ave_requested;
			} else {
				total_missing += ave_requested - reserved[i];
			}
		}

		if (total_missing > total_excess) {
			dprintf("%s: error: "
				"sum of below-ave (%ld, %ld MiB) > "
				"sum of above-ave (%ld, %ld MiB)\n",
				__func__,
				total_missing, total_missing >> 20,
				total_excess, total_excess >> 20);
			release = 1;
			ret = -ENOMEM;
			goto out;
		}

		dprintk("%s: total missing: %ld\n",
			__func__, total_missing);

		req.sizes = calloc(IHK_MAX_NUM_NUMA_NODES, sizeof(size_t));
		CHKANDJUMP(req.sizes == NULL, -ENOMEM,
			   "failed to allocate torelease\n");

		req.numa_ids = calloc(IHK_MAX_NUM_NUMA_NODES, sizeof(int));
		CHKANDJUMP(req.numa_ids == NULL, -ENOMEM,
			   "failed to allocate torelease\n");

		/* round up not to release too much */
		ave_compensate = ((total_missing / num_numa_nodes_compensate +
				   IHKLIB_RESERVE_AMOUNT_ALIGN - 1) /
				  IHKLIB_RESERVE_AMOUNT_ALIGN) *
			IHKLIB_RESERVE_AMOUNT_ALIGN;
		dprintk("%s: ave compensate: %ld\n",
			__func__, ave_compensate);

		/* Fill below ave(requested + compensation),
		 * compensating nodes upto the average
		 */
		for (i = 0; i < IHK_MAX_NUM_NUMA_NODES; i++) {
			if (reserved[i] <= ave_requested) {
				continue;
			}

			if (reserved[i] > ave_requested + ave_compensate) {
				num_numa_nodes_release++;
				total_excess2 += reserved[i] - ave_requested -
					ave_compensate;
				dprintk("%s: above-ave-req+comp: node id: %d, reserved: %ld, ave requested: %ld, ave compensate: %ld, compensate2+=%ld\n",
					__func__, i, reserved[i],
					ave_requested, ave_compensate,
					reserved[i] - ave_requested -
					ave_compensate);
			} else {
				total_missing2 += ave_requested +
					ave_compensate - reserved[i];
				dprintk("%s: below-ave-req+comp: node id: %d, reserved: %ld, ave requested: %ld, ave compensate: %ld, missing2+=%ld\n",
					__func__, i, reserved[i],
					ave_requested, ave_compensate,
					ave_requested + ave_compensate -
					reserved[i]);
			}
		}

		dprintk("%s: total excess2: %ld, total missing2: %ld\n",
			__func__, total_excess2, total_missing2);

		/* round up not to release too much */
		ave_compensate2 =
			((total_missing2 / num_numa_nodes_release +
			  IHKLIB_RESERVE_AMOUNT_ALIGN - 1) /
			 IHKLIB_RESERVE_AMOUNT_ALIGN) *
			IHKLIB_RESERVE_AMOUNT_ALIGN;
		dprintk("%s: ave compensate2: %ld\n",
			__func__, ave_compensate2);

		/* above-average-of-requested-plus-compensation nodes
		 * can release the excess amount
		 */
		for (i = 0; i < IHK_MAX_NUM_NUMA_NODES; i++) {
			req.numa_ids[i] = i;

			if (reserved[i] > ave_requested +
			    ave_compensate + ave_compensate2) {
				req.sizes[i] = reserved[i] - ave_requested -
					ave_compensate - ave_compensate2;
				CHKANDJUMP(reserved[i] < ave_requested +
					   ave_compensate + ave_compensate2,
					   -EINVAL, "negative release size\n");
			} else {
				req.sizes[i] = 0;
			}

			if (req.sizes[i] != 0) {
				dprintk("%s: node id: %d, to-release: %ld\n",
					__func__, i, req.sizes[i]);
			}

			if (reserved[i] > 0 &&
			    reserved[i] - req.sizes[i] < min) {
				min = reserved[i] - req.sizes[i];
			}
			if (reserved[i] > 0 &&
			    reserved[i] - req.sizes[i] > max) {
				max = reserved[i] - req.sizes[i];
			}
		}

		variance_limit = ave_requested *
			reserve_mem_conf.variance_limit / 100;
		dprintk("%s: min: %ld, max: %ld, variance_limit: %ld\n",
			__func__, min, max, variance_limit);
		if (max - ave_requested > variance_limit ||
		    ave_requested - min > variance_limit) {
#ifdef DEBUG
			unsigned long max_ave = max - ave_requested;
			unsigned long ave_min = ave_requested - min;
#endif

			dprintf("%s: error: variance > limit, "
				"ave: %ld (%ld MiB), "
				"max - ave: %ld (%ld MiB), "
				"ave - min: %ld (%ld MiB), "
				"limit: %ld (%ld MiB)\n",
				__func__,
				ave_requested, ave_requested >> 20,
				max_ave, max_ave >> 20,
				ave_min, ave_min >> 20,
				variance_limit, variance_limit >> 20);

			release = 1;
			ret = -ENOMEM;
			goto out;
		}

		req.num_chunks = IHK_MAX_NUM_NUMA_NODES;

		fd = ihklib_device_open(index);
		if (fd < 0) {
			ret = fd;
			dprintf("%s: ihklib_device_open returned %d\n",
				__func__, fd);
		}

		ret = ioctl(fd, IHK_DEVICE_RELEASE_MEM_PARTIALLY, &req);
		if (ret != 0) {
			ret = -errno;
			dprintf("%s: IHK_DEVICE_RESERVE_MEM returned %d\n",
				__func__, -ret);
			goto out;
		}

		close(fd);
	}

	ret = 0;
out:
	if (release) {
		struct ihk_mem_chunk mem_chunks[1] = {
			{ .size = -1UL, .numa_node_number = 0 }
		};

		ihk_release_mem(index, mem_chunks, 1);
	}

	if (fd >= 0) {
		close(fd);
	}
	free(req.sizes);
	free(req.numa_ids);
	if (reserve_mem_conf.total) {
		free(mem_chunks_reserved);
		free(reserved);
	}
	return ret;
}

int ihk_get_num_reserved_mem_chunks(int index)
{
	int ret;
	int fd = -1;
	struct ihk_mem_req req = { 0 };

	dprintk("%s: enter\n", __func__);
	if ((fd = ihklib_device_open(index)) < 0) {
		eprintf("%s: error: ihklib_device_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	req.num_chunks = 0;   /* means only get num_reserved_mem_chunks */

	ret = ioctl(fd, IHK_DEVICE_QUERY_MEM, &req);
	if (ret) {
		ret = -errno;
		dprintf("%s: IHK_DEVICE_QUERY_MEM returned %d\n",
			__func__, -ret);
		goto out;
	}

	ret = req.num_chunks;

 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

int ihk_query_mem(int index, struct ihk_mem_chunk* mem_chunks, int _num_mem_chunks)
{
	int ret;
	int fd = -1;
	int i;
	int num_mem_chunks;
	struct ihk_mem_req req = { 0 };

	dprintk("%s: enter\n", __func__);
	ret = ihklib_device_readable(index);
	if (ret) {
		goto out;
	}

	if (_num_mem_chunks < 0 || _num_mem_chunks > IHK_MAX_NUM_MEM_CHUNKS) {
		dprintf("%s: error: invalid # of chunks (%d)\n",
			__func__, _num_mem_chunks);
		ret = -EINVAL;
		goto out;
	}

	if (_num_mem_chunks != 0 && mem_chunks == NULL) {
		ret = -EFAULT;
		goto out;
	}

	ret = ihk_get_num_reserved_mem_chunks(index);
	if (ret < 0) {
		dprintf("%s: error: ihk_get_num_reserved_mem_chunks"
			" returned %d\n",
			__func__, ret);
		goto out;
	}
	num_mem_chunks = ret;

	if (_num_mem_chunks != num_mem_chunks) {
		dprintf("%s: error: actual # of chunks (%d) !="
			" requested (%d)\n",
			__func__, num_mem_chunks, _num_mem_chunks);
		ret = -EINVAL;
		goto out;
	}

	req.sizes = calloc(num_mem_chunks, sizeof(size_t));
	if (!req.sizes) {
		dprintf("%s: error: allocating request sizes\n",
			__func__);
		ret = -ENOMEM;
		goto out;
	}

	req.numa_ids = calloc(num_mem_chunks, sizeof(int));
	if (!req.numa_ids) {
		dprintf("%s: error: allocating request numa_ids\n",
			__func__);
		ret = -ENOMEM;
		goto out;
	}

	req.num_chunks = num_mem_chunks;

	if ((fd = ihklib_device_open(index)) < 0) {
		dprintf("%s: error: ihklib_device_open returned %d\n",
			__func__, fd);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_DEVICE_QUERY_MEM, &req);
	if (ret) {
		ret = -errno;
		dprintf("%s: error: IHK_DEVICE_QUERY_MEM returned %d\n",
			__func__, -ret);
		goto out;
	}

	for (i = 0; i < num_mem_chunks; i++) {
		mem_chunks[i].size = req.sizes[i];
		mem_chunks[i].numa_node_number = req.numa_ids[i];
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	free(req.sizes);
	free(req.numa_ids);
	return ret;
}

int ihk_release_mem(int index, struct ihk_mem_chunk* mem_chunks, int num_mem_chunks)
{
	int ret, i;
	struct ihk_mem_req req = { 0 };
	int fd = -1;
	struct ihk_mem_chunk *query_mem_chunks = NULL;

	dprintk("%s: enter\n", __func__);

	ret = ihklib_device_readable(index);
	if (ret) {
		goto out;
	}

	if (num_mem_chunks < 0 || num_mem_chunks > IHK_MAX_NUM_MEM_CHUNKS) {
		dprintf("%s: error: invalid # of chunks (%d)\n",
			__func__, num_mem_chunks);
		ret = -EINVAL;
		goto out;
	}

	if (num_mem_chunks != 0 && mem_chunks == NULL) {
		ret = -EFAULT;
		goto out;
	}

	if (num_mem_chunks == 0) {
		ret = 0;
		goto out;
	};

	if (mem_chunks[0].size == IHK_SMP_MEM_ALL) {
		/* Special case for releasing all memory */
		num_mem_chunks = ihk_get_num_reserved_mem_chunks(index);
		query_mem_chunks = calloc(num_mem_chunks,
					  sizeof(struct ihk_mem_chunk));
		if (query_mem_chunks == NULL) {
			dprintf("%s: error: allocating memory chunks\n",
				__func__);
			ret = -ENOMEM;
			goto out;
		}

		ret = ihk_query_mem(index, query_mem_chunks, num_mem_chunks);
		if (ret) {
			dprintf("%s: error: ihk_query_mem returned %d\n",
				__func__, ret);
			goto out;
		}

		mem_chunks = query_mem_chunks;
	}

	req.sizes = calloc(num_mem_chunks, sizeof(size_t));
	if (!req.sizes) {
		dprintf("%s: error: allocating request sizes\n",
			__func__);
		ret = -ENOMEM;
		goto out;
	}

	req.numa_ids = calloc(num_mem_chunks, sizeof(int));
	if (!req.numa_ids) {
		dprintf("%s: error: allocating request numa_ids\n",
			__func__);
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < num_mem_chunks; i++) {
		req.sizes[i] = (size_t)mem_chunks[i].size;
		req.numa_ids[i] = mem_chunks[i].numa_node_number;
	}
	req.num_chunks = num_mem_chunks;

	if ((fd = ihklib_device_open(index)) < 0) {
		dprintf("%s: error: ihklib_device_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_DEVICE_RELEASE_MEM, &req);
	if (ret) {
		ret = -errno;
		dprintf("%s: error: IHK_OS_RELEASE_MEM returned %d\n",
			__func__, -ret);
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	free(query_mem_chunks);
	free(req.sizes);
	free(req.numa_ids);
	return ret;
}

/* Create OS and return OS index */
int ihk_create_os(int index)
{
	int ret;
	int fd = -1;

	dprintk("%s: enter\n", __func__);
	if ((fd = ihklib_device_open(index)) < 0) {
		dprintf("%s: error: ihklib_device_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_DEVICE_CREATE_OS, 0);
	if (ret < 0) {
		ret = -errno;
		dprintf("%s: error: IHK_DEVICE_CREATE_OS returned %d\n",
			__func__, -ret);
		goto out;
	}
 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

int ihk_get_num_os_instances(int index)
{
	int ret;
	DIR *dir = NULL;
	struct dirent *direp;
	int num_os_instances = 0;
	int fd = -1;

	dprintk("%s: enter\n", __func__);
	if ((fd = ihklib_device_open(index)) < 0) {
		dprintf("%s: error: ihklib_device_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	dir = opendir(PATH_DEV);
	if (dir == NULL) {
		ret = -errno;
		dprintf("%s: error: opendir returned %d\n",
			__func__, -ret);
		goto out;
	}

	while ((direp = readdir(dir))) {
		if ((strncmp(direp->d_name,"mcos",4) == 0)) {
			num_os_instances++;
		}
	}
	ret = num_os_instances;
 out:
	if (fd != -1) {
		close(fd);
	}
	if (dir) {
		closedir(dir);
	}
	return ret;
}

int ihk_get_os_instances(int index, int *indices, int _num_os_instances)
{
	int ret;
	DIR *dir = NULL;
	struct dirent *direp;
	int num_os_instances = 0;
	int num_mcos = 0;

	dprintk("%s: enter\n", __func__);
	ret = ihklib_device_readable(index);
	if (ret) {
		dprintf("%s: error: ihklib_device_readable returned %d\n",
			__func__, ret);
		goto out;
	}

	ret = ihk_get_num_os_instances(index);
	if (ret < 0) {
		dprintf("%s: error: ihk_get_num_os_instances returned %d\n",
			__func__, ret);
		goto out;
	}
	num_os_instances = ret;

	if (num_os_instances != _num_os_instances) {
		dprintf("%s: Actual # of OS instances (%d) != "
			"requested (%d)\n",
			__func__, num_os_instances, _num_os_instances);
		ret = -EINVAL;
		goto out;
	}

	dir = opendir(PATH_DEV);
	if (dir == NULL) {
		ret = -errno;
		dprintf("%s: error: opendir returned %d\n",
			__func__, -ret);
		goto out;
	}

	while ((direp = readdir(dir))) {
		if ((strncmp(direp->d_name, "mcos", 4) == 0)) {
			indices[num_mcos] = atoi(direp->d_name + 4);
			num_mcos++;
		}
	}

	ret = 0;
 out:
	if (dir) {
		closedir(dir);
	}
	return ret;
}

int ihk_destroy_os(int dev_index, int os_index)
{
	int ret;
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	fd = ihklib_device_open(dev_index);
	if (fd < 0) {
		dprintf("%s: error: ihklib_device_open returned %d\n",
			__func__, fd);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_DEVICE_DESTROY_OS, os_index);
	if (ret) {
		ret = -errno;
		dprintf("%s: error: IHK_DEVICE_DESTROY_OS returned %d\n",
			__func__, -ret);
		goto out;
	}
 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

static int ihklib_os_readable(int index)
{
	int ret;
	char fn[PATH_MAX];

	sprintf(fn, "/dev/mcos%d", index);
	ret = access(fn, R_OK);
	if (ret) {
		ret = -errno;
		dprintf("%s: error: access: path: %s, errno: %d\n",
			__func__, fn, -ret);
		goto out;
	}

 out:
	return ret;
}

int ihklib_os_open(int index)
{
	int ret;
	char fn[PATH_MAX];

	ret = ihklib_os_readable(index);
	if (ret) {
		dprintf("%s: error: ihklib_os_readable returned %d\n",
			__func__, ret);
		goto out;
	}

	sprintf(fn, "/dev/mcos%d", index);

	if ((ret = open(fn, O_RDONLY)) == -1) {
		ret = -errno;
		dprintf("%s: error: open %s: %s\n",
			__func__, fn, strerror(-ret));
		goto out;
	}

 out:
	return ret;
}

int ihk_os_assign_cpu(int index, int* cpus, int num_cpus)
{
	int ret;
	struct ihk_ioctl_cpu_desc req = { 0 };
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	ret = ihklib_os_readable(index);
	if (ret) {
		goto out;
	}

	if (num_cpus < 0 || num_cpus > IHK_MAX_NUM_CPUS) {
		dprintf("%s: error: invalid # of cpus (%d)\n",
			__func__, num_cpus);
		ret = -EINVAL;
		goto out;
	}

	if (num_cpus != 0 && cpus == NULL) {
		ret = -EFAULT;
		goto out;
	}

	if (num_cpus == 0) {
		ret = 0;
		goto out;
	}

	req.cpus = cpus;
	req.num_cpus = num_cpus;

	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open returned %d\n",
			__func__, fd);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_OS_ASSIGN_CPU, &req);
	if (ret) {
		ret = -errno;
		dprintf("%s: error: IHK_OS_ASSIGN_CPU returned %d\n",
			__func__, -ret);
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

int ihk_os_get_num_assigned_cpus(int index)
{
	int ret;
	int fd = -1;

	dprintk("%s: enter\n", __func__);
	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open returned %d\n",
			__func__, fd);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_OS_GET_NUM_CPUS);
	if (ret < 0) {
		dprintf("%s: error: IHK_OS_GET_NUM_CPUS returned %d\n",
			__func__, ret);
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

int ihk_os_query_cpu(int index, int *cpus, int num_cpus)
{
	int ret;
	struct ihk_ioctl_cpu_desc req = { 0 };
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	ret = ihklib_os_readable(index);
	if (ret) {
		goto out;
	}

	if (num_cpus < 0 || num_cpus > IHK_MAX_NUM_CPUS) {
		dprintf("%s: error: invalid # of cpus (%d)\n",
			__func__, num_cpus);
		ret = -EINVAL;
		goto out;
	}

	if (num_cpus != 0 && cpus == NULL) {
		ret = -EFAULT;
		goto out;
	}

	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	if ((ret = ioctl(fd, IHK_OS_GET_NUM_CPUS)) < 0) {
		ret = -errno;
		dprintf("%s: error: IHK_OS_GET_NUM_CPUS returned %d\n",
			__func__, -ret);
		goto out;
	}

	if (ret != num_cpus) {
		dprintf("%s: error: actual # of CPUs (%d) != requested (%d)\n",
			__func__, ret, num_cpus);
		ret = -EINVAL;
		goto out;
	}
	req.cpus = cpus;
	req.num_cpus = num_cpus;

	if ((ret = ioctl(fd, IHK_OS_QUERY_CPU, &req))) {
		ret = -errno;
		dprintf("%s: error: IHK_OS_QUERY_CPU returned %d\n",
			__func__, -ret);
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

int ihk_os_release_cpu(int index, int *cpus, int num_cpus)
{
	int ret;
	struct ihk_ioctl_cpu_desc req = { 0 };
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	ret = ihklib_os_readable(index);
	if (ret) {
		goto out;
	}

	if (num_cpus < 0 || num_cpus > IHK_MAX_NUM_CPUS) {
		dprintf("%s: error: invalid # of cpus (%d)\n",
			__func__, num_cpus);
		ret = -EINVAL;
		goto out;
	}

	if (num_cpus != 0 && cpus == NULL) {
		ret = -EFAULT;
		goto out;
	}

	if (num_cpus == 0) {
		ret = 0;
		goto out;
	}
	req.cpus = cpus;
	req.num_cpus = num_cpus;

	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open returned %d\n",
			__func__, fd);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_OS_RELEASE_CPU, &req);
	if (ret) {
		ret = -errno;
		dprintf("%s: error: IHK_OS_RELEASE_CPU returned %d\n",
			__func__, -ret);
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

int ihk_os_set_ikc_map(int index, struct ihk_ikc_cpu_map *map, int num_cpus)
{
	int ret, i;
	struct ihk_ioctl_ikc_desc req = { 0 };
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	ret = ihklib_os_readable(index);
	if (ret) {
		goto out;
	}

	if (num_cpus < 0 || num_cpus > IHK_MAX_NUM_CPUS) {
		dprintf("%s: error: invalid # of cpus (%d)\n",
			__func__, num_cpus);
		ret = -EINVAL;
		goto out;
	}

	if (num_cpus != 0 && map == NULL) {
		ret = -EFAULT;
		goto out;
	}

	ret = ihk_os_get_num_assigned_cpus(index);
	if (ret != num_cpus) {
		dprintf("%s: error: actual number of CPUs (%d) is"
			" different than requested (%d)\n",
			__func__, ret, num_cpus);
		ret = -EINVAL;
		goto out;
	}

	req.src_cpus = calloc(num_cpus, sizeof(int));
	if (!req.src_cpus) {
		dprintf("%s: error: allocating request src_cpus\n",
			__func__);
		ret = -ENOMEM;
		goto out;
	}

	req.dst_cpus = calloc(num_cpus, sizeof(int));
	if (!req.dst_cpus) {
		dprintf("%s: error: allocating request dst_cpuss\n",
			__func__);
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < num_cpus; i++) {
		req.src_cpus[i] = map[i].src_cpu;
		req.dst_cpus[i] = map[i].dst_cpu;
	}
	req.num_cpus = num_cpus;

	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_OS_SET_IKC_MAP, &req);
	if (ret) {
		ret = -errno;
		dprintf("%s: IHK_OS_SET_IKC_MAP returned %d\n",
			__func__, -ret);
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	free(req.src_cpus);
	free(req.dst_cpus);
	return ret;
}

int ihk_os_get_ikc_map(int index, struct ihk_ikc_cpu_map *map, int num_cpus)
{
	int ret, i;
	struct ihk_ioctl_ikc_desc req = { 0 };
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	ret = ihklib_os_readable(index);
	if (ret) {
		goto out;
	}

	if (num_cpus < 0 || num_cpus > IHK_MAX_NUM_CPUS) {
		dprintf("%s: error: invalid # of cpus (%d)\n",
			__func__, num_cpus);
		ret = -EINVAL;
		goto out;
	}

	if (num_cpus != 0 && map == NULL) {
		ret = -EFAULT;
		goto out;
	}

	ret = ihk_os_get_num_assigned_cpus(index);
	if (ret != num_cpus) {
		dprintf("%s: error: actual number of CPUs (%d) is"
			" different than requested (%d)\n",
			__func__, ret, num_cpus);
		ret = -EINVAL;
		goto out;
	}

	req.src_cpus = calloc(num_cpus, sizeof(int));
	if (!req.src_cpus) {
		dprintf("%s: error: allocating request src_cpus\n",
			__func__);
		ret = -ENOMEM;
		goto out;
	}

	req.dst_cpus = calloc(num_cpus, sizeof(int));
	if (!req.dst_cpus) {
		dprintf("%s: error: allocating request dst_cpuss\n",
			__func__);
		ret = -ENOMEM;
		goto out;
	}

	req.num_cpus = num_cpus;

	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_OS_GET_IKC_MAP, &req);
	if (ret) {
		ret = -errno;
		dprintf("%s: IHK_OS_GET_IKC_MAP returned %d\n",
			__func__, -ret);
		goto out;
	}

	for (i = 0; i < req.num_cpus; i++) {
		map[i].src_cpu = req.src_cpus[i];
		map[i].dst_cpu = req.dst_cpus[i];
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	free(req.src_cpus);
	free(req.dst_cpus);
	return ret;
}

int ihk_os_assign_mem(int index, struct ihk_mem_chunk *mem_chunks, int num_mem_chunks)
{
	int ret, i;
	struct ihk_mem_req req = { 0 };
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	ret = ihklib_os_readable(index);
	if (ret) {
		goto out;
	}

	if (num_mem_chunks < 0 || num_mem_chunks > IHK_MAX_NUM_MEM_CHUNKS) {
		dprintf("%s: error: invalid # of chunks (%d)\n",
			__func__, num_mem_chunks);
		ret = -EINVAL;
		goto out;
	}

	if (num_mem_chunks != 0 && mem_chunks == NULL) {
		ret = -EFAULT;
		goto out;
	}

	if (num_mem_chunks == 0) {
		ret = 0;
		goto out;
	};

	req.sizes = calloc(num_mem_chunks, sizeof(size_t));
	if (!req.sizes) {
		dprintf("%s: error: allocating request sizes\n",
			__func__);
		ret = -ENOMEM;
		goto out;
	}

	req.numa_ids = calloc(num_mem_chunks, sizeof(int));
	if (!req.numa_ids) {
		dprintf("%s: error: allocating request numa_ids\n",
			__func__);
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < num_mem_chunks; i++) {
		req.sizes[i] = (size_t)mem_chunks[i].size;
		req.numa_ids[i] = mem_chunks[i].numa_node_number;
	}
	req.num_chunks = num_mem_chunks;

	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open returned %d\n",
			__func__, fd);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_OS_ASSIGN_MEM, &req);
	if (ret) {
		ret = -errno;
		dprintf("%s: IHK_OS_ASSIGN_MEM returned %d\n",
			__func__, -ret);
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	free(req.sizes);
	free(req.numa_ids);
	return ret;
}

int ihk_os_get_num_assigned_mem_chunks(int index)
{
	int ret;
	struct ihk_mem_req req = { 0 };
	int fd = -1;

	dprintk("%s: enter\n", __func__);
	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	req.num_chunks = 0;   /* means only get num_chunks */

	ret = ioctl(fd, IHK_OS_QUERY_MEM, &req);
	if (ret) {
		ret = -errno;
		dprintf("%s: IHK_OS_QUERY_MEM returned %d\n",
			__func__, -ret);
		goto out;
	}

	ret = req.num_chunks;

 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

int ihk_os_query_mem(int index, struct ihk_mem_chunk *mem_chunks,
		     int _num_mem_chunks)
{
	int ret, i;
	int num_mem_chunks;
	struct ihk_mem_req req = { 0 };
	int fd = -1;

	dprintk("%s: enter\n", __func__);
	ret = ihklib_os_readable(index);
	if (ret) {
		goto out;
	}

	if (_num_mem_chunks < 0 || _num_mem_chunks > IHK_MAX_NUM_MEM_CHUNKS) {
		dprintf("%s: error: invalid # of chunks (%d)\n",
			__func__, _num_mem_chunks);
		ret = -EINVAL;
		goto out;
	}

	if (_num_mem_chunks != 0 && mem_chunks == NULL) {
		ret = -EFAULT;
		goto out;
	}

	ret = ihk_os_get_num_assigned_mem_chunks(index);
	if (ret < 0) {
		dprintf("%s: error: ihk_os_get_num_assigned_mem_chunks"
			" returned %d\n",
			__func__, ret);
		goto out;
	}
	num_mem_chunks = ret;

	if (_num_mem_chunks != num_mem_chunks) {
		dprintf("%s: error: actual # of chunks (%d) !="
			" requested (%d)\n",
			__func__, num_mem_chunks, _num_mem_chunks);
		ret = -EINVAL;
		goto out;
	}

	req.sizes = calloc(num_mem_chunks, sizeof(size_t));
	if (!req.sizes) {
		dprintf("%s: error: allocating request sizes\n",
			__func__);
		ret = -ENOMEM;
		goto out;
	}

	req.numa_ids = calloc(num_mem_chunks, sizeof(int));
	if (!req.numa_ids) {
		dprintf("%s: error: allocating request numa_ids\n",
			__func__);
		ret = -ENOMEM;
		goto out;
	}

	req.num_chunks = num_mem_chunks;

	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_OS_QUERY_MEM, &req);
	if (ret) {
		ret = -errno;
		dprintf("%s: error: IHK_OS_QUERY_MEM returned %d\n",
			__func__, -ret);
		goto out;
	}

	for (i = 0; i < num_mem_chunks; i++) {
		mem_chunks[i].size = req.sizes[i];
		mem_chunks[i].numa_node_number = req.numa_ids[i];
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	free(req.sizes);
	free(req.numa_ids);
	return ret;
}

int ihk_os_release_mem(int index, struct ihk_mem_chunk *mem_chunks,
		int num_mem_chunks)
{
	int ret, i;
	struct ihk_mem_req req = { 0 };
	int fd = -1;
	struct ihk_mem_chunk *query_mem_chunks = NULL;

	dprintk("%s: enter\n", __func__);

	ret = ihklib_os_readable(index);
	if (ret) {
		goto out;
	}

	if (num_mem_chunks < 0 || num_mem_chunks > IHK_MAX_NUM_MEM_CHUNKS) {
		dprintf("%s: error: invalid # of chunks (%d)\n",
			__func__, num_mem_chunks);
		ret = -EINVAL;
		goto out;
	}

	if (num_mem_chunks != 0 && mem_chunks == NULL) {
		ret = -EFAULT;
		goto out;
	}

	if (num_mem_chunks == 0) {
		ret = 0;
		goto out;
	};

	if (mem_chunks[0].size == IHK_SMP_MEM_ALL) {
		/* Special case for releasing all memory */
		num_mem_chunks = ihk_os_get_num_assigned_mem_chunks(index);
		query_mem_chunks = calloc(num_mem_chunks,
					  sizeof(struct ihk_mem_chunk));
		if (query_mem_chunks == NULL) {
			dprintf("%s: error: allocating memory chunks\n",
				__func__);
			ret = -ENOMEM;
			goto out;
		}

		ret = ihk_os_query_mem(index, query_mem_chunks, num_mem_chunks);
		if (ret) {
			dprintf("%s: error: ihk_os_query_mem returned %d\n",
				__func__, ret);
			goto out;
		}

		mem_chunks = query_mem_chunks;
	}

	req.sizes = calloc(num_mem_chunks, sizeof(size_t));
	if (!req.sizes) {
		dprintf("%s: error: allocating request sizes\n",
			__func__);
		ret = -ENOMEM;
		goto out;
	}

	req.numa_ids = calloc(num_mem_chunks, sizeof(int));
	if (!req.numa_ids) {
		eprintf("%s: error: allocating request numa_ids\n",
			__func__);
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < num_mem_chunks; i++) {
		req.sizes[i] = (size_t)mem_chunks[i].size;
		req.numa_ids[i] = mem_chunks[i].numa_node_number;
	}
	req.num_chunks = num_mem_chunks;

	if ((fd = ihklib_os_open(index)) < 0) {
		eprintf("%s: error: ihklib_os_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_OS_RELEASE_MEM, &req);
	if (ret) {
		ret = -errno;
		dprintf("%s: error: IHK_OS_RELEASE_MEM returned %d\n",
			__func__, -ret);
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	free(query_mem_chunks);
	free(req.sizes);
	free(req.numa_ids);
	return ret;
}

int ihk_os_get_eventfd(int index, int type)
{
	int fd = -1;
	int ret;
	struct ihk_os_ioctl_eventfd_desc desc;

	dprintk("%s: enter\n", __func__);
	memset(&desc, 0, sizeof(desc));

	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	switch (type) {
	case IHK_OS_EVENTFD_TYPE_OOM:
	case IHK_OS_EVENTFD_TYPE_STATUS:
	case IHK_OS_EVENTFD_TYPE_KMSG:
		break;
	default:
		dprintf("%s: error: unknown type: %d\n",
			__func__, type);
		ret = -EINVAL;
		goto out;
	}

	desc.fd = eventfd(0, 0);
	desc.type = type;

	ret = ioctl(fd, IHK_OS_REGISTER_EVENT, &desc);
	if (ret) {
		ret = -errno;
		dprintf("%s: error: IHK_OS_REGISTER_EVENT returned %d\n",
			__func__, -ret);
		goto out;
	}

	ret = desc.fd;
 out:
	if (fd != -1) {
		close(fd);
	}
	dprintk("%s: returning %d\n", __func__, ret);
	return ret;
}

int ihk_os_load(int index, char* fn)
{
	int ret;
	int fd = -1;

	dprintk("%s: enter\n", __func__);
	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	if (fn == NULL) {
		dprintf("%s: error: file name is NULL\n",
			__func__);
		ret = -EINVAL;
		goto out;
	}

	ret = ioctl(fd, IHK_OS_LOAD, (unsigned long)fn);
	if (ret) {
		ret = -errno;
		dprintf("%s: error: IHK_OS_LOAD returned %d\n",
			__func__, -ret);
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

int ihk_os_kargs(int index, char* kargs)
{
	int ret;
	int fd = -1;

	dprintk("%s: enter\n", __func__);
	if (kargs == NULL) {
		dprintf("%s: warning: kargs is NULL\n",
			__func__);
		ret = -EFAULT;
		goto out;
	}

	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_OS_SET_KARGS, kargs);
	if (ret) {
		ret = -errno;
		dprintf("%s: error: IHK_OS_SET_KARGS returned %d\n",
			__func__, -ret);
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

int ihk_os_boot(int index)
{
	int ret;
	int fd = -1;
	int i;

	dprintk("%s: enter\n", __func__);
	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open returned %d\n",
			__func__, fd);
		ret = fd;
		goto out;
	}

	if ((ret = ioctl(fd, IHK_OS_BOOT, 0)) == -1) {
		ret = -errno;
		dprintf("%s: error: IHK_OS_BOOT returned %d\n",
			__func__, -ret);
		goto out;
	}

	for (i = 0; i < 50; i++) { /* 10 second */
		ret = ioctl(fd, IHK_OS_STATUS);

		switch (ret) {
		case IHK_OS_STATUS_BOOTING:
		case IHK_OS_STATUS_BOOTED:
		case IHK_OS_STATUS_READY:
			usleep(200000);
			continue;
		default:
			break;
		}
	}

	if (ret == -1) {
		ret = -errno;
		dprintf("%s: error: IHK_OS_STATUS returned %d\n",
			__func__, -ret);
		goto out;
	}

	if (ret != IHK_OS_STATUS_RUNNING) {
		dprintf("%s: error: "
			"status didn't change to RUNNING (%d)\n",
			__func__, ret);
		ret = -EINVAL;
		goto out;
	}

	ret = 0;
 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

int ihk_os_shutdown(int index)
{
	int ret;
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	if ((fd = ihklib_os_open(index)) < 0) {
		eprintf("%s: error: ihklib_os_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_OS_SHUTDOWN, 0);
	if (ret) {
		ret = -errno;
		dprintf("%s: IHK_OS_SHUTDOWN returned %d\n",
			__func__, -ret);
		goto out;
	}
 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;

}

int ihk_os_get_status(int index)
{
	int ret;
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_OS_STATUS);
	if (ret < 0) {
		dprintf("%s: error: IHK_OS_STATUS: %d\n",
			__func__, ret);
		goto out;
	}

	switch (ret) {
	case IHK_OS_STATUS_NOT_BOOTED: /* before smp_ihk_os_boot or
					* after smp_ihk_destroy_os
					*/
		ret = IHK_STATUS_INACTIVE;
		break;
	case IHK_OS_STATUS_BOOTING:	/* smp_ihk_os_boot -- arch_init */
	case IHK_OS_STATUS_BOOTED:	/* arch_init -- arch_ready */
	case IHK_OS_STATUS_READY:	/* arch_ready -- done_init */
		ret = IHK_STATUS_BOOTING;
		break;
	case IHK_OS_STATUS_RUNNING:	/* after done_init */
		ret = IHK_STATUS_RUNNING;
		break;
	case IHK_OS_STATUS_SHUTDOWN:	/* smp_ihk_os_shutdown --
					 * smp_ihk_destroy_os
					 */
		ret = IHK_STATUS_SHUTDOWN;
		break;
	case IHK_OS_STATUS_FAILED:
		ret = IHK_STATUS_PANIC;
		break;
	case IHK_OS_STATUS_HUNGUP:
		ret = IHK_STATUS_HUNGUP;
		break;
	case IHK_OS_STATUS_FREEZING:
		ret = IHK_STATUS_FREEZING;
		break;
	case IHK_OS_STATUS_FROZEN:
		ret = IHK_STATUS_FROZEN;
		break;
	default:
		dprintf("%s: error: unknown os status: %d\n",
			__func__, ret);
		ret = -EINVAL;
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	dprintk("%s: returning %d\n", __func__, ret);
	return ret;
}

int ihk_os_get_kmsg_size(int index)
{
	int ret;
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	ret = IHK_KMSG_SIZE;

 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

int ihk_os_kmsg(int index, char* kmsg, ssize_t sz_kmsg)
{
	int ret;
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open returned %d\n",
			__func__, fd);
		ret = fd;
		goto out;
	}

	if (sz_kmsg != IHK_KMSG_SIZE) {
		dprintf("%s: error: invalid buffer size\n",
			__func__);
		ret = -EINVAL;
		goto out;
	}

	if (kmsg == NULL) {
		dprintf("%s: error: invalid buffer address\n",
			__func__);
		ret = -EFAULT;
		goto out;
	}

	ret = ioctl(fd, IHK_OS_READ_KMSG, (unsigned long)kmsg);
	if (ret < 0) {
		ret = -errno;
		dprintf("%s: IHK_OS_READ_KMSG returned %d\n",
			__func__, -ret);
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

int ihk_os_clear_kmsg(int index)
{
	int ret;
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_OS_CLEAR_KMSG, 0);
	if (ret) {
		ret = -errno;
		dprintf("%s: IHK_OS_CLEAR_KMSG returned %d\n",
			__func__, -ret);
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

int ihk_os_get_num_numa_nodes(int index)
{
	int ret;
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_OS_GET_NUM_NUMA_NODES);
	if (ret < 0) {
		ret = -errno;
		dprintf("%s: IHK_OS_GET_NUM_NUMA_NODES returned %d\n",
			__func__, -ret);
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

static int get_meminfo_path(char *path, int os_index, int node)
{
	return snprintf(path, PATH_MAX,
			"/sys/devices/virtual/mcos/mcos%d/"
			"sys/devices/system/node/node%d/meminfo",
			os_index, node);
}

int ihklib_os_query_mem_sysfs(int index, char *result, ssize_t sz_result,
			      const char *type)
{
	int ret;
	int node = 0;
	char path[PATH_MAX];
	int len = 0;
	struct stat sb;
	FILE *fp = NULL;

	dprintk("%s: enter\n", __func__);

	memset(result, 0, sz_result);

	get_meminfo_path(path, index, node);

	while (stat(path, &sb) != -1) {
		unsigned long free_kb = 0;
		char *line = NULL;
		size_t line_len;

		fp = fopen(path, "r");
		CHKANDJUMP(!fp, -1, "error: opening %s\n", path);

		while (getline(&line, &line_len, fp) != -1) {
			int scan_node;
			char scanfmt[1024];
			int scanfmt_len;

			scanfmt_len = snprintf(scanfmt, sizeof(scanfmt),
					       "Node %%d %s:%%16lu kB",
					       type);
			if (scanfmt_len >= sizeof(scanfmt)) {
				eprintf("%s: error: type string (%s) is too long\n",
					__func__, type);
				ret = -1;
				goto out;
			}

			if (sscanf(line, scanfmt,
				   &scan_node, &free_kb) == 2) {
				if (node > 0)
					len += snprintf(&result[len],
							sz_result - len, ",");

				len += snprintf(&result[len], sz_result - len,
						"%lu@%d",
						free_kb * 1024, node);
			}

			free(line);
			line = NULL;
		}

		fclose(fp);
		fp = NULL;

		++node;
		get_meminfo_path(path, index, node);
	}

	CHKANDJUMP(len == 0, -1, "%s not found\n", type);

	ret = 0;
out:
	if (fp) {
		fclose(fp);
	}
	return ret;
}

static int ihklib_os_query_mem(int index, unsigned long *result,
		 int num_numa_nodes, enum ihklib_os_query_mem_type type)
{
	int i, ret;
	char result_str[16 * IHK_MAX_NUM_NUMA_NODES];
	struct ihk_mem_chunk mem_chunks[IHK_MAX_NUM_NUMA_NODES];
	int num_mem_chunks = num_numa_nodes;
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	if ((fd = ihklib_os_open(index)) < 0) {
		eprintf("%s: error: ihklib_os_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	ret = ihklib_os_query_mem_sysfs(index, result_str,
					sizeof(result_str),
					ihklib_os_query_mem_type_str[type]);
	CHKANDJUMP(ret != 0, -EINVAL,
		   "ihklib_os_query_total_mem failed\n");

	memset(mem_chunks, 0, sizeof(mem_chunks));
	mem_str2array(result_str, &num_mem_chunks, mem_chunks);

	CHKANDJUMP(num_mem_chunks != num_numa_nodes, -EINVAL,
		   "actual number of NUMA nodes (%d) is different than requested (%d)\n",
		   num_mem_chunks, num_numa_nodes);

	for (i = 0; i < num_mem_chunks; i++) {
		CHKANDJUMP(mem_chunks[i].numa_node_number >= num_numa_nodes ||
			   mem_chunks[i].numa_node_number < 0, -EINVAL,
			   "NUMA node number out of range\n");
		result[mem_chunks[i].numa_node_number] = mem_chunks[i].size;
	}

	ret = 0;
 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

int ihk_os_query_total_mem(int index, unsigned long *result,
			   int num_numa_nodes)
{
	dprintk("%s: enter\n", __func__);
	return ihklib_os_query_mem(index, result, num_numa_nodes,
				   IHKLIB_OS_QUERY_MEM_TOTAL);
}

int ihk_os_query_free_mem(int index, unsigned long *result,
		      int num_numa_nodes)
{
	dprintk("%s: enter\n", __func__);
	return ihklib_os_query_mem(index, result, num_numa_nodes,
				   IHKLIB_OS_QUERY_MEM_FREE);
}

int ihk_os_get_num_pagesizes(int index)
{
	int ret;
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open returned %d\n",
			__func__, fd);
		ret = fd;
		goto out;
	}

	ret = IHK_MAX_NUM_PGSIZES;

 out:
	if (fd != -1) {
		close(fd);
	}
	dprintk("%s: returning %d\n", __func__, ret);
	return ret;
}

int ihk_os_get_pagesizes(int index, long *pgsizes, int num_pgsizes)
{
	int ret;
	int i;
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open\n",
			__func__);
		ret = fd;
		goto out;
	}

	if (!pgsizes) {
		ret = -EFAULT;
		goto out;
	}

	if (num_pgsizes != IHK_MAX_NUM_PGSIZES) {
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < num_pgsizes; i++) {
		pgsizes[i] = rusage_pgtype_to_pgsize((enum ihk_os_pgsize)i);
	}

	ret = 0;
 out:
	if (fd != -1) {
		close(fd);
	}
	dprintk("%s: returning %d\n", __func__, ret);
	return ret;
}

#ifdef ENABLE_RUSAGE
int ihk_os_getrusage(int index, struct ihk_os_rusage *rusage,
		     size_t size_rusage)
{
	int ret;
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	if (!rusage) {
		dprintf("%s: error: output buffer is NULL\n",
			__func__);
		ret = -EFAULT;
		goto out;
	}

	if (size_rusage != sizeof(struct ihk_os_rusage)) {
		dprintf("%s: error: size of output buffer is invalid\n",
			__func__);
		ret = -EINVAL;
		goto out;
	}

	struct mcctrl_ioctl_getrusage_desc desc = {
		.rusage = rusage,
		.size_rusage = size_rusage,
	};

	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open returned %d\n",
			__func__, fd);
		ret = fd;
		goto out;
	}

	ret = ioctl(fd, IHK_OS_GETRUSAGE, &desc);
	if (ret) {
		ret = -errno;
		dprintf("%s: IHK_OS_GETRUSAGE returned %d\n",
			__func__, -ret);
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	dprintk("%s: returning %d\n", __func__, ret);
	return ret;
}
#else
int ihk_os_getrusage(int index, struct ihk_os_rusage *rusage,
		     size_t size_rusage)
{
	dprintf("Specify --enable-rusage when configuring.\n");
	return -ENOSYS;
}
#endif

int ihk_os_setperfevent(int index, ihk_perf_event_attr *attr, int n)
{
	int ret;
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open returned %d\n",
			__func__, fd);
		ret = fd;
		goto out;
	}

	if (n <= 0) {
		dprintf("%s: invalid number(%d) of events\n",
			__func__, n);
		ret = -EINVAL;
		goto out;
	}

	ret = ioctl(fd, IHK_OS_AUX_PERF_NUM, n);
	if (ret) {
		ret = -errno;
		dprintf("%s: IHK_OS_AUX_PERF_NUM returned %d\n",
			__func__, -ret);
		goto out;
	}

	ret = ioctl(fd, IHK_OS_AUX_PERF_SET, attr);
	if (ret < 0) {
		ret = -errno;
		dprintf("%s: IHK_OS_AUX_PERF_SET returned %d\n",
			__func__, -ret);
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	dprintk("%s: returning %d\n", __func__, ret);
	return ret;
}

int ihk_os_perfctl(int index, int comm)
{
	int ret;
	int fd = -1;

	dprintk("%s: enter\n", __func__);
	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open returned %d\n",
			__func__, fd);
		ret = fd;
		goto out;
	}

	switch (comm) {
	case PERF_EVENT_ENABLE: /* start PA event */
		ret = ioctl(fd, IHK_OS_AUX_PERF_ENABLE, 0);
		break;
	case PERF_EVENT_DISABLE: /* stop PA event */
		ret = ioctl(fd, IHK_OS_AUX_PERF_DISABLE, 0);
		break;
	case PERF_EVENT_DESTROY: /* stop PA event and
				  * reset # of counters
				  */
		ret = ioctl(fd, IHK_OS_AUX_PERF_DESTROY, 0);
		break;
	default:
		ret = -EINVAL;
		goto out;
	}
	if (ret) {
		ret = -errno;
		dprintf("%s: IHK_OS_AUX_PERF_* returned %d\n",
			__func__, -ret);
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	dprintk("%s: returning %d\n", __func__, ret);
	return ret;
}

int ihk_os_getperfevent(int index, unsigned long *counter, int n)
{
	int ret;
	int fd = -1;

	dprintk("%s: enter\n", __func__);
	if ((fd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open returned %d\n",
			__func__, fd);
		ret = fd;
		goto out;
	}

	if (n <= 0) {
		dprintf("%s: invalid number(%d) of events\n",
			__func__, n);
		ret = -EINVAL;
		goto out;
	}

	ret = ioctl(fd, IHK_OS_AUX_PERF_GET, counter);
	if (ret) {
		ret = -errno;
		dprintf("%s: IHK_OS_AUX_PERF_GET returned %d\n",
			__func__, -ret);
		goto out;
	}

 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

int ihk_os_freeze(unsigned long *os_set, int n)
{
	int ret;
	int index;
	int fd = -1;

	dprintk("%s: enter\n", __func__);
	if (n <= 0) {
		dprintf("%s: invalid length of os bitset(%d)\n", __func__, n);
		ret = -EINVAL;
		goto out;
	}

	for (index = 0; index < n; index++) {
		if (*(os_set + index / 64) & (1ULL << (index % 64))) {
			if ((fd = ihklib_os_open(index)) < 0) {
				dprintf("%s: error: ihklib_os_open\n",
					__func__);
				ret = fd;
				goto out;
			}

			ret = ioctl(fd, IHK_OS_FREEZE, 0);
			if (ret) {
				ret = -errno;
				dprintf("%s: IHK_OS_FREEZE "
					"returned %d\n",
					__func__, -ret);
				goto out;
			}

			close(fd);
			fd = -1;
		}
	}
	ret = 0;
 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

int ihk_os_thaw(unsigned long *os_set, int n)
{
	int ret;
	int index;
	int fd = -1;

	dprintk("%s: enter\n", __func__);

	if (n <= 0) {
		dprintf("%s: invalid length of os bitset(%d)\n", __func__, n);
		ret = -EINVAL;
		goto out;
	}

	for (index = 0; index < n; index++) {
		if (*(os_set + index / 64) & (1ULL << (index % 64))) {
			if ((fd = ihklib_os_open(index)) < 0) {
				dprintf("%s: error: ihklib_os_open\n",
					__func__);
				ret = fd;
				goto out;
			}

			ret = ioctl(fd, IHK_OS_THAW, 0);
			if (ret) {
				ret = -errno;
				dprintf("%s: IHK_OS_THAW "
					"returned %d\n",
					__func__, -ret);
				goto out;
			}

			close(fd);
			fd = -1;
		}
	}
	ret = 0;
 out:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

#ifdef ENABLE_MEMDUMP
#include <bfd.h>
#include <inttypes.h>
#include <time.h>
#include <limits.h>
#include <pwd.h>

int ihk_os_makedumpfile(int index, char *dump_file, int dump_level, int interactive)
{
	int ret;
	static char hname[HOST_NAME_MAX+1];
	bfd *abfd = NULL;
	bfd_boolean ok;
	asection *scn;
	dumpargs_t args;
	unsigned long phys_size, phys_offset;
	int error, i;
	size_t bsize;
	void *buf = NULL;
	uintptr_t addr;
	size_t cpsize;
	time_t t;
	struct tm *tm;
	char *date;
	struct passwd *pw;
	dump_mem_chunks_t *mem_chunks;
	long mem_size;
	char *physmem_name_buf = NULL;
	char physmem_name[PHYSMEM_NAME_SIZE];
	int osfd = -1;
	char *token;

	dprintk("%s: enter\n", __func__);
	dprintf("%s: index=%d,dump_file=%s,dump_level=%d,interactive=%d\n",
		__func__, index, dump_file, dump_level, interactive);

	if ((osfd = ihklib_os_open(index)) < 0) {
		dprintf("%s: error: ihklib_os_open returned %d\n",
			__func__, osfd);
		ret = osfd;
		goto out;
	}

	ret = ihk_os_get_status(index);
	if (ret < 0) {
		dprintf("%s: ihk_os_get_status returned %d\n",
			__func__, ret);
		goto out;
	}

	if (ret == IHK_STATUS_INACTIVE) {
		ret = -EINVAL;
		goto out;
	}

	t = time(NULL);
	if (t == (time_t)-1) {
		ret = -errno;
		dprintf("%s: error: time returned %d\n",
			__func__, -ret);
		goto out;
	}

	tm = localtime(&t);
	if (!tm) {
		ret = -EINVAL;
		dprintf("%s: error: localtime failed\n",
			__func__);
		goto out;
	}

	error = gethostname(hname, sizeof(hname));
	if (error != 0) {
		ret = -errno;
		dprintf("%s: error: gethostname returned %d\n",
			__func__, -ret);
		goto out;
	}

	/* TODO: might be redundant */
	pw = getpwuid(getuid());
	if (pw == NULL) {
		ret = -errno;
		dprintf("%s: error: getpwuid returned %d\n",
			__func__, -ret);
		goto out;
	}

	args.cmd = DUMP_SET_LEVEL;
	args.level = dump_level;
	error = ioctl(osfd, IHK_OS_DUMP, &args);
	if (error != 0) {
		ret = -errno;
		dprintf("%s: error: DUMP_SET_LEVEL returned %d\n",
			__func__, -ret);
		goto out;
	}

	args.cmd = DUMP_NMI;
	error = ioctl(osfd, IHK_OS_DUMP, &args);
	if (error != 0) {
		ret = -errno;
		dprintf("%s: error: DUMP_NMI returned %d\n",
			__func__, -ret);
		goto out;
	}

	args.cmd = DUMP_QUERY_NUM_MEM_AREAS;
	args.size = 0;
	error = ioctl(osfd, IHK_OS_DUMP, &args);
	if (error != 0) {
		ret = -errno;
		dprintf("%s: error: "
			"DUMP_QUERY_NUM_MEM_AREAS returned %d\n",
			__func__, -ret);
		goto out;
	}

	mem_size = args.size;
	mem_chunks = malloc(mem_size);
	if (!mem_chunks) {
		ret = -ENOMEM;
		dprintf("%s: error: alloating mem_chunks\n",
			__func__);
		goto out;
	}

	memset(mem_chunks, 0, args.size);

	args.cmd = DUMP_QUERY_MEM_AREAS;
	args.buf = (void *)mem_chunks;
	error = ioctl(osfd, IHK_OS_DUMP, &args);
	if (error != 0) {
		ret = -errno;
		dprintf("%s: error: DUMP_QUERY_MEM_AREAS returned %d\n",
			__func__, -ret);
		goto out;
	}

	phys_size = 0;
	dprintf("%s: nr chunks: %d\n",
		__func__, mem_chunks->nr_chunks);
	for (i = 0; i < mem_chunks->nr_chunks; ++i) {
		dprintf("%s: 0x%lx:%lu\n",
				__FUNCTION__,
				mem_chunks->chunks[i].addr,
				mem_chunks->chunks[i].size);
		phys_size += mem_chunks->chunks[i].size;
	}

	bsize = 0x100000;
	buf = malloc(bsize);
	if (!buf) {
		ret = -ENOMEM;
		dprintf("%s: error: allocating buf\n", __func__);
		goto out;
	}

	bfd_init();

	if (dump_file == NULL) {
		ret = -EFAULT;
		goto out;
	}

	token = strrchr(dump_file, '/');
	if (token) {
		token[0] = 0;
		ret = access(dump_file, W_OK);
		if (ret) {
			ret = -errno;
			dprintf("%s: %s is inaccessible: %d\n",
				__func__, dump_file, -ret);
			token[0] = '/';
			goto out;
		}
		token[0] = '/';
	}

	abfd = bfd_fopen(dump_file, NULL, "w", -1);
	if (!abfd) {
		ret = -EINVAL;
		dprintf("%s: bfd_fopen failed: %s\n",
			__func__, bfd_errmsg(bfd_get_error()));
		goto out;
	}

	ok = bfd_set_format(abfd, bfd_object);
	if (!ok) {
		ret = -EINVAL;
		dprintf("%s: error: bfd_set_format: %s\n",
			__func__, bfd_errmsg(bfd_get_error()));
		goto out;
	}

	date = asctime(tm);
	if (date) {
		cpsize = strlen(date) - 1;	/* exclude trailing '\n' */
		scn = bfd_make_section_anyway(abfd, "date");
		if (!scn) {
			ret = -EINVAL;
			dprintf("%s: error: "
				"bfd_make_section_anyway(date): %s\n",
				__func__, bfd_errmsg(bfd_get_error()));
			goto out;
		}

		ok = bfd_set_section_size(abfd, scn, cpsize);
		if (!ok) {
			ret = -EINVAL;
			dprintf("%s: error: "
				"bfd_set_section_size: %s\n",
				__func__, bfd_errmsg(bfd_get_error()));
			goto out;
		}

		ok = bfd_set_section_flags(abfd, scn, SEC_HAS_CONTENTS);
		if (!ok) {
			ret = -EINVAL;
			dprintf("%s: error: "
				"bfd_set_section_flags: %s\n",
				__func__, bfd_errmsg(bfd_get_error()));
			goto out;
		}
	}
	error = gethostname(hname, sizeof(hname));
	if (!error) {
		cpsize = strlen(hname);
		scn = bfd_make_section_anyway(abfd, "hostname");
		if (!scn) {
			ret = -EINVAL;
			dprintf("%s: error: "
				"bfd_make_section_anyway(hostname): %s\n",
				__func__, bfd_errmsg(bfd_get_error()));
			goto out;
		}

		ok = bfd_set_section_size(abfd, scn, cpsize);
		if (!ok) {
			ret = -EINVAL;
			dprintf("%s: error: "
				"bfd_set_section_size: %s\n",
				__func__, bfd_errmsg(bfd_get_error()));
			goto out;
		}

		ok = bfd_set_section_flags(abfd, scn, SEC_HAS_CONTENTS);
		if (!ok) {
			ret = -EINVAL;
			dprintf("%s: error: "
				"bfd_set_section_flags: %s\n",
				__func__, bfd_errmsg(bfd_get_error()));
			goto out;
		}
	}
	pw = getpwuid(getuid());
	if (pw) {
		cpsize = strlen(pw->pw_name);
		scn = bfd_make_section_anyway(abfd, "user");
		if (!scn) {
			ret = -EINVAL;
			dprintf("%s: error: "
				"bfd_make_section_anyway(user): %s\n",
				__func__, bfd_errmsg(bfd_get_error()));
			goto out;
		}

		ok = bfd_set_section_size(abfd, scn, cpsize);
		if (!ok) {
			ret = -EINVAL;
			dprintf("%s: error: "
				"bfd_set_section_size: %s\n",
				__func__, bfd_errmsg(bfd_get_error()));
			goto out;
		}

		ok = bfd_set_section_flags(abfd, scn, SEC_HAS_CONTENTS);
		if (!ok) {
			ret = -EINVAL;
			dprintf("%s: error: "
				"bfd_set_section_flags: %s\n",
				__func__, bfd_errmsg(bfd_get_error()));
			goto out;
		}
	}

	/* Add section for physical memory chunks information */
	scn = bfd_make_section_anyway(abfd, "physchunks");
	if (!scn) {
		ret = -EINVAL;
		dprintf("%s: error: "
			"bfd_make_section_anyway(physchunks): %s\n",
			__func__, bfd_errmsg(bfd_get_error()));
		goto out;
	}

	ok = bfd_set_section_size(abfd, scn, mem_size);
	if (!ok) {
		ret = -EINVAL;
		dprintf("%s: error: "
			"bfd_set_section_size: %s\n",
			__func__, bfd_errmsg(bfd_get_error()));
		goto out;
	}

	ok = bfd_set_section_flags(abfd, scn, SEC_ALLOC|SEC_HAS_CONTENTS);
	if (!ok) {
		ret = -EINVAL;
		dprintf("%s: error: "
			"bfd_set_section_flags: %s\n",
			__func__, bfd_errmsg(bfd_get_error()));
		goto out;
	}

	for (i = 0; i < mem_chunks->nr_chunks; ++i) {

		physmem_name_buf = malloc(PHYSMEM_NAME_SIZE);
		memset(physmem_name_buf,0,PHYSMEM_NAME_SIZE);
		sprintf(physmem_name_buf, "physmem%d",i);

		/* Physical memory contents section */
		scn = bfd_make_section_anyway(abfd, physmem_name_buf);
		if (!scn) {
			ret = -EINVAL;
			dprintf("%s: error: "
				"bfd_make_section_anyway(physmem): %s\n",
				__func__, bfd_errmsg(bfd_get_error()));
			goto out;
		}

		if (interactive) {
			ok = bfd_set_section_size(abfd, scn, PAGE_SIZE);
		}
		else {
			ok = bfd_set_section_size(abfd, scn, mem_chunks->chunks[i].size);
		}
		if (!ok) {
			ret = -EINVAL;
			dprintf("%s: error: "
				"bfd_set_section_size: %s\n",
				__func__, bfd_errmsg(bfd_get_error()));
			goto out;
		}

		ok = bfd_set_section_flags(abfd, scn, SEC_ALLOC|SEC_HAS_CONTENTS);
		if (!ok) {
			ret = -EINVAL;
			dprintf("%s: error: "
				"bfd_set_section_flags: %s\n",
				__func__, bfd_errmsg(bfd_get_error()));
			goto out;
		}

		scn->vma = mem_chunks->chunks[i].addr;
	}

	scn = bfd_get_section_by_name(abfd, "date");
	if (scn) {
		ok = bfd_set_section_contents(abfd, scn, date, 0, scn->size);
		if (!ok) {
			ret = -EINVAL;
			dprintf("%s: error: "
				"bfd_set_section_contents(date): %s\n",
				__func__, bfd_errmsg(bfd_get_error()));
			goto out;
		}
	}

	scn = bfd_get_section_by_name(abfd, "hostname");
	if (scn) {
		ok = bfd_set_section_contents(abfd, scn, hname, 0, scn->size);
		if (!ok) {
			ret = -EINVAL;
			dprintf("%s: error: "
				"bfd_set_section_contents(hostname): %s\n",
				__func__, bfd_errmsg(bfd_get_error()));
			goto out;
		}
	}

	scn = bfd_get_section_by_name(abfd, "user");
	if (scn) {
		ok = bfd_set_section_contents(abfd, scn, pw->pw_name, 0, scn->size);
		if (!ok) {
			ret = -EINVAL;
			dprintf("%s: error: "
				"bfd_set_section_contents(user): %s\n",
				__func__, bfd_errmsg(bfd_get_error()));
			goto out;
		}
	}

	scn = bfd_get_section_by_name(abfd, "physchunks");
	if (scn) {
		ok = bfd_set_section_contents(abfd, scn, mem_chunks, 0, mem_size);
		if (!ok) {
			ret = -EINVAL;
			dprintf("%s: error: "
				"bfd_set_section_contents(physchunks): %s\n",
				__func__, bfd_errmsg(bfd_get_error()));
			goto out;
		}
	}

	if (interactive) {
		ret = 0;
		goto out;
	}

	for (i = 0; i < mem_chunks->nr_chunks; ++i) {

		phys_offset = 0;

		memset(physmem_name,0,sizeof(physmem_name));
		sprintf(physmem_name, "physmem%d",i);

		scn = bfd_get_section_by_name(abfd, physmem_name);
		if (!scn) {
			ret = -EINVAL;
			dprintf("%s: error: "
				"bfd_get_section_by_name(physmem_name): %s\n",
				__func__, bfd_errmsg(bfd_get_error()));
			goto out;
		}

		for (addr = mem_chunks->chunks[i].addr;
				addr < (mem_chunks->chunks[i].addr + mem_chunks->chunks[i].size);
				addr += cpsize) {

			cpsize = (mem_chunks->chunks[i].addr + mem_chunks->chunks[i].size) - addr;
			if (cpsize > bsize) {
				cpsize = bsize;
			}

			args.cmd = DUMP_READ;
			args.start = addr;
			args.size = cpsize;
			args.buf = buf;

			error = ioctl(osfd, IHK_OS_DUMP, &args);
			if (error != 0) {
				ret = -errno;
				dprintf("%s: error: DUMP_HEAD returned %d\n",
					__func__, -ret);
				goto out;
			}

			ok = bfd_set_section_contents(abfd, scn, buf, phys_offset, cpsize);
			if (!ok) {
				ret = -EINVAL;
				dprintf("%s: error: "
					"bfd_set_section_contents(physmem): %s\n",
					__func__, bfd_errmsg(bfd_get_error()));
				goto out;
			}

			phys_offset += cpsize;
		}
	}

	ret = 0;
 out:
	if (abfd) {
		ok = bfd_close(abfd);
		if (!ok) {
			ret = -EINVAL;
			dprintf("%s: error: bfd_close: %s\n",
				__func__, bfd_errmsg(bfd_get_error()));
		}
	}
	if (osfd >= 0) {
		error = close(osfd);
		if (error) {
			ret = -errno;
			dprintf("%s: error: close: %s\n",
				__func__, strerror(-ret));
		}
	}
	return ret;
}
#else /* ENABLE_MEMDUMP */
int ihk_os_makedumpfile(int index, char *dump_file, int dump_level, int interactive)
{
	dprintk("%s: enter\n", __func__);
	fprintf(stderr, "dump is not supported.\n");
	return -ENOSYS;
}
#endif /* ENABLE_MEMDUMP */

/*
 * Messages with level below or equal to loglevel
 * are printed out
 */
int ihk_set_loglevel(enum IHKLIB_LOGLEVEL level)
{
	dprintk("%s: enter\n", __func__);
	loglevel = level;
	return 0;
}
