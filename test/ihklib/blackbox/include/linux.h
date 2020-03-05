#ifndef __LINUX_H_INCLUDED__
#define __LINUX_H_INCLUDED__

int linux_insmod(void);
int linux_chmod(int dev_index);
int linux_rmmod(int verbose);
int linux_kill_mcexec(void);

#endif
