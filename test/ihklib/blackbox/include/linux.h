#ifndef __LINUX_H_INCLUDED__
#define __LINUX_H_INCLUDED__

int linux_insmod(void);
int linux_chmod(uid_t uid, gid_t git);
int linux_rmmod(int verbose);
int linux_kill_mcexec(void);

#endif