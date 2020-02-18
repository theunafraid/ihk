#ifndef __USER_H_INCLUDED__
#define __USER_H_INCLUDED__

extern char **environ;

int user_fork_exec(char *filename, pid_t *pid);
int user_wait(pid_t *pid);

#endif
