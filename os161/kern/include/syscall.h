#ifndef _SYSCALL_H_
#define _SYSCALL_H_

/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */

typedef struct curTime {
	time_t seconds;
	unsigned long nanoseconds;
} curTime;

int sys_reboot(int code);
int syscall_read(int fd, char* buf, size_t buflen, int32_t *retval);
int syscall_write(int fd, char* c, size_t size, int32_t* retval);
int syscall_fork(struct trapframe *, int32_t *retval);
int syscall_waitpid(int childPID, int status, int options, int32_t *retval);
int syscall_getpid(int32_t *retval);
int syscall__exit(int , int32_t *retval);
// int sys_execv(char *progname, char **args);
int syscall_execv(const char *prog_path, char **args);
int syscall_sbrk(int incr, int32_t* retval);
int syscall_time(time_t *seconds, unsigned long *nanoseconds, int *retval);
#endif /* _SYSCALL_H_ */
