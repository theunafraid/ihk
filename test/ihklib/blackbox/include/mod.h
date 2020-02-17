#ifndef __INIT_FINI_H_INCLUDED__
#define __INIT_FINI_H_INCLUDED__

int insmod(void);
int chmod(uid_t uid, gid_t git);
int rmmod(int verbose);
int kill_mcexec(void);

#endif
