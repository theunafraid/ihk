#ifndef __INPUT_VECTOR_H_INCLUDED__
#define __INPUT_VECTOR_H_INCLUDED__

#define MAX_NUM_CPUS 272

struct cpus {
	int *cpus;
	int ncpus;
};

int cpus_ls(struct cpus *cpus);
int cpus_push(struct cpus *cpus, int id);
int cpus_pop(struct cpus *cpus);
int cpus_unshift(struct cpus *cpus, int n);
void cpus_dump(struct cpus *cpus);
int cpus_compare(struct cpus *cpus_result, struct cpus *cpus_expected);

#endif
