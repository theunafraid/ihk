/* bootparam.h COPYRIGHT FUJITSU LIMITED 2015-2017 */
#ifndef HEADER_BUILTIN_BOOTPARAM_H
#define HEADER_BUILTIN_BOOTPARAM_H

#define SMP_MAX_CPUS 512

#define __NCOREBITS  (sizeof(long) * 8)   /* bits per mask */
#define CORE_SET(n, p) \
	((p).set[(n)/__NCOREBITS] |= ((long)1 << ((n) % __NCOREBITS)))
#define CORE_CLR(n, p) \
	((p).set[(n)/__NCOREBITS] &= ~((long)1 << ((n) % __NCOREBITS)))
#define CORE_ISSET(n, p) \
	(((p).set[(n)/__NCOREBITS] & ((long)1 << ((n) % __NCOREBITS)))?1:0)
#define CORE_ZERO(p)      memset(&(p).set, 0, sizeof((p).set))

#ifndef __ASSEMBLY__
struct smp_coreset {
	unsigned long set[SMP_MAX_CPUS / __NCOREBITS];
};

static inline int CORE_ISSET_ANY(struct smp_coreset *p)
{
	int     i;

	for(i = 0; i < SMP_MAX_CPUS / __NCOREBITS; i++)
		if(p -> set[i])
			return 1;
	return 0;
}

struct ihk_smp_boot_param_cpu {
	int numa_id;
	int hw_id;
	int linux_cpu_id;
};

struct ihk_smp_boot_param_memory_chunk {
	unsigned long start, end;
	int numa_id;
};

#define IHK_SMP_MEMORY_TYPE_DRAM          0x01
#define IHK_SMP_MEMORY_TYPE_HBM           0x02

struct ihk_smp_boot_param_numa_node {
	int type;
	int linux_numa_id;
};

/*
 * smp_boot_param holds various boot time arguments.
 * The layout in the memory is the following:
 * [[struct smp_boot_param]
 * [struct ihk_smp_boot_param_cpu] ... [struct ihk_smp_boot_param_cpu]
 * [struct ihk_smp_boot_param_numa_node] ...
 * [struct ihk_smp_boot_param_numa_node]
 * [struct ihk_smp_boot_param_memory_chunk] ...
 * [struct ihk_smp_boot_param_memory_chunk]],
 * where the number of CPUs, the number of numa nodes and
 * the number of memory ranges are determined by the nr_cpus,
 * nr_numa_nodes and nr_memory_chunks fields, respectively.
 */
struct smp_boot_param {
	/*
	 * [start, end] covers all assigned ranges, including holes
	 * in between so that a straight mapping can be set up at boot time,
	 * but actual valid memory ranges are described in the
	 * ihk_smp_boot_param_memory_chunk structures.
	 */
	unsigned long start, end;

	/* End address of the memory chunk on which kernel sections 
	   are loaded, used for boundary check in early_alloc_pages(). */
	unsigned long bootstrap_mem_end;

	unsigned long status;
	unsigned long msg_buffer;
	unsigned long msg_buffer_size;
	unsigned long mikc_queue_recv, mikc_queue_send;

	unsigned long dma_address;
	unsigned long ident_table;
	unsigned long ns_per_tsc;
	unsigned long boot_sec;
	unsigned long boot_nsec;
	unsigned int ihk_ikc_irq;
	unsigned int ihk_ikc_irq_apicid;
	char kernel_args[256];
	int nr_cpus;
	int nr_numa_nodes;
	int nr_memory_chunks;
};
#endif /* !__ASSEMBLY__ */

#endif