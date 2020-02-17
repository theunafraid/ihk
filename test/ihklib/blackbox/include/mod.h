#ifndef __INIT_FINI_H_INCLUDED__
#define __INIT_FINI_H_INCLUDED__

int insmod(uid_t uid, gid_t gid);
int rmmod(int verbose);
int kill_mcexec(void);

#endif
