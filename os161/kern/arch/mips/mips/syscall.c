#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <machine/pcb.h>
#include <machine/spl.h>
#include <machine/trapframe.h>
#include <kern/callno.h>
#include <syscall.h>
#include <thread.h>
#include <addrspace.h>
#include <array.h>
#include <vfs.h>
#include "syscall.h"
#include <kern/unistd.h>
#include <clock.h>


#define MAX_PATH_LEN 128
#define MINBRKCHK -98304
#define MAXBRKCHK 98304
extern procContBlock * listProcesses[MAX_PID];
extern struct thread* curthread;

/*
 * System call handler.
 *
 * A pointer to the trapframe created during exception entry (in
 * exception.S) is passed in.
 *
 * The calling conventions for syscalls are as follows: Like ordinary
 * function calls, the first 4 32-bit arguments are passed in the 4
 * argument registers a0-a3. In addition, the system call number is
 * passed in the v0 register.
 *
 * On successful return, the return value is passed back in the v0
 * register, like an ordinary function call, and the a3 register is
 * also set to 0 to indicate success.
 *
 * On an error return, the error code is passed back in the v0
 * register, and the a3 register is set to 1 to indicate failure.
 * (Userlevel code takes care of storing the error code in errno and
 * returning the value -1 from the actual userlevel syscall function.
 * See src/lib/libc/syscalls.S and related files.)
 *
 * Upon syscall return the program counter stored in the trapframe
 * must be incremented by one instruction; otherwise the exception
 * return code will restart the "syscall" instruction and the system
 * call will repeat forever.
 *
 * Since none of the OS/161 system calls have more than 4 arguments,
 * there should be no need to fetch additional arguments from the
 * user-level stack.
 *
 * Watch out: if you make system calls that have 64-bit quantities as
 * arguments, they will get passed in pairs of registers, and not
 * necessarily in the way you expect. We recommend you don't do it.
 * (In fact, we recommend you don't use 64-bit quantities at all. See
 * arch/mips/include/types.h.)
 */

/* call functions based on the call number.  */
void
mips_syscall(struct trapframe *tf)
{
	int callno;
	int spl;
	int32_t retval;
	int err;

	assert(curspl==0);

	callno = tf->tf_v0; // the system call number is
 	//passed in the v0 register.

	/*
	 * Initialize retval to 0. Many of the system calls don't
	 * really return a value, just 0 for success and -1 on
	 * error. Since retval is the value returned on success,
	 * initialize it to 0 by default; thus it's not necessary to
	 * deal with it except for calls that return other values, 
	 * like write.
	 */

	retval = 0;

	switch (callno) {

		case SYS_reboot:
		err = sys_reboot(tf->tf_a0);
		break;

		case SYS_read:
		err = syscall_read(tf->tf_a0, tf->tf_a1, tf->tf_a2, &retval);
		break;

		case SYS_write:
		err = syscall_write(tf->tf_a0, tf->tf_a1, tf->tf_a2, &retval);
		break;

		case SYS_fork:
		err = syscall_fork(tf, &retval);
		break;

		case SYS_waitpid:
		err = syscall_waitpid(tf->tf_a0,(int)tf->tf_a1, tf->tf_a2, &retval);
		break;

		case SYS_getpid:
		spl = splhigh();
		err = syscall_getpid(&retval);
		splx(spl);
		break;

		case SYS__exit:
		err = syscall_exit(tf->tf_a0, &retval);
		break;

 		case SYS_execv:
 		spl = splhigh();
 		err = syscall_execv(tf->tf_a0, tf->tf_a1);
 		splx(spl);
 		break;

 		case SYS___time:
		err = syscall_time((userptr_t)tf->tf_a0,
				 (userptr_t)tf->tf_a1, &retval);
		break;

		case SYS_sbrk:
		err = syscall_sbrk(tf->tf_a0, &retval);
		break;

	    default:
		kprintf("Unknown syscall %d\n", callno);
		err = ENOSYS;
		break;
	}


	if (err) {
		/*
		 * Return the error code. This gets converted at
		 * userlevel to a return value of -1 and the error
		 * code in errno.
		 */
		tf->tf_v0 = err;
		tf->tf_a3 = 1;      /* signal an error */
	}
	else {
		/* Success. */
		tf->tf_v0 = retval;
		tf->tf_a3 = 0;      /* signal no error */
	}
	
	/*
	 * Now, advance the program counter, to avoid restarting
	 * the syscall over and over again.
	 */
	
	tf->tf_epc += 4;

	/* Make sure the syscall code didn't forget to lower spl */
	assert(curspl==0);

}

int 
syscall_read(int fd, char* buf, size_t buflen, int32_t* retval)
{
	if (fd != STDIN_FILENO){
		return EBADF;
	}
	char kernloc;
	int copyCheck;
	kernloc = getch();
	copyCheck = copyout(&kernloc, (userptr_t) buf, 1);
	if(copyCheck != 0)
		return EFAULT;
	*retval = 1;
	return 0;	
}



int 
syscall_write(int fd, char* buf, size_t nbytes, int* retval) {
	int spl = splhigh();
	int copyCheck;
  	if(fd != STDOUT_FILENO && fd != STDERR_FILENO){
  		splx(spl);
  		return EBADF;
  	}
	char *kernloc = kmalloc(sizeof(char) * (nbytes));
	copyCheck = copyin((const_userptr_t)buf, kernloc, nbytes);
	if(copyCheck != 0){
		splx(spl);
		return EFAULT;
	}
	kernloc[nbytes] = '\0';
	int i = 0;
	for(i; i < nbytes; i++){
		putch(kernloc[i]);
	}
	*retval = nbytes;
	kfree(kernloc);
	splx(spl);
	return 0;
}

int
md_forkentry(void* tf, unsigned long vmspace)
{
	struct trapframe child;
	struct trapframe *parent= (struct trapframe*) tf;
	assert(parent != NULL);

	parent->tf_epc += 4;
	parent->tf_v0 = 0;
	parent->tf_a3 = 0;

	curthread->t_vmspace = (struct addrspace*)vmspace;
	assert(curthread->t_vmspace != NULL);
	as_activate(curthread->t_vmspace);

	child = *parent;
	mips_usermode(&child);

	panic("switching to user mode returned\n");
	return 0;
}

int 
syscall_fork(struct trapframe *tfptr, int *retval){
	int spl = splhigh();
	struct addrspace * childVM = NULL;

	int result = as_copy(curthread->t_vmspace, &childVM);
	if(result){
		splx(spl);
		return result;
	}

	struct  trapframe* childTF = kmalloc(sizeof(struct trapframe));
	if (childTF == NULL) {
		splx(spl);
		return ENOMEM;
	}	
	*childTF = *tfptr; 

	struct thread *child = kmalloc(sizeof(struct thread));
	result =  thread_fork(curthread->t_name, (void*)childTF, (unsigned long)childVM, md_forkentry, &child);

	assert(child != NULL);
	*retval = child->pID;
	splx(spl);
	return 0;

}

int 
syscall_getpid(int32_t *retval){
	int spl = splhigh();
	splx(spl);
	*retval = curthread->pID;
	return 0;
}

int 
syscall_waitpid(int pID, int status, int options, int32_t *retval) {
	*retval = pID; 
	int spl = splhigh();
	int result;
	int* status_ptr = &status;
	if (status_ptr == NULL) {
		splx(spl);
		return EFAULT;
	}

	if(options != 0){
		splx(spl);
		return EINVAL;
	}

	if(listProcesses[curthread->pID] == NULL){
		splx(spl);
		return EFAULT;
	}

	if (pID < MIN_PID || pID > MAX_PID) {
		splx(spl);
		return EINVAL;
	}

	if (pID == curthread->pID) {
		splx(spl);
		return EINVAL;
	}

	if(listProcesses[pID] == NULL){
			// panic("Process ID Does not exist");
		splx(spl);
		return EINVAL;
		}



	if(listProcesses[pID]->parentID != curthread->pID){
		splx(spl);
		return EINVAL;
	}


	if(listProcesses[pID]->exited == 1){ 
		assert(listProcesses[pID] != NULL);
		*status_ptr = listProcesses[pID]->exitCode;
		processRemove(pID);
		*retval = pID;
		
		splx(spl);
		return 0;
	}
	else{ 

		assert(listProcesses[pID] != NULL);

			while(listProcesses[pID]->exited != 1){
				thread_sleep(pID);
			}
		
		thread_wakeup_1(pID);
		*status_ptr = listProcesses[pID]->exitCode;
		processRemove(pID);
		
		splx(spl);
		
		return 0;
	}
}

int 
syscall_exit(int exitcode, int32_t *retval) {
	
	int spl = splhigh();
	int current_pid = curthread->pID;

	int j = 0;
	for(j; j < MAX_PID; j++) { 
		if(listProcesses[j] != NULL){
			if(listProcesses[j]->parentID == current_pid)
			{
				listProcesses[j]->exitCode = exitcode;
				listProcesses[j]->parentID = 1;
			}
		}
	}

	// *retval = 0;
	splx(spl);
	thread_exit(); 
}


int syscall_execv(const char *prog_path, char **args) {
	int result,check;
	int nargs;
	size_t bufflen;

	// the prog_path is in user space, we need to copy it into kernel space
	if(prog_path == NULL) {
		return EINVAL;
	}
	if(args == NULL){
		return EFAULT;
	}

	// arguments badcall check
	char** argcheck= (char**) kmalloc(sizeof(char**));
	result = copyin((const_userptr_t)args, argcheck, sizeof(char *));
	if(result) {
		return EFAULT;
	}

	// check program name
	char* program = (char *) kmalloc(MAX_PATH_LEN);
	if(program == NULL){
		return ENOMEM;
	}
	check = copyinstr((userptr_t)prog_path, program, MAX_PATH_LEN, &bufflen);
	if (check) {
		return check;
	}

	if (bufflen == 1) {
		return EINVAL;
	}

	kfree(argcheck);

	// count the number of arguments
	char* nargscounter;
	int i = 0;
	while(1) {
		result = copyin((userptr_t)&(args[i]), &nargscounter, (sizeof(userptr_t)));
		if (result) {
		//always want to check if the copy failed or not, returns 0 if it works 
		//should probably free any kmallocs before returning
			return EFAULT;
		}
		
		if (nargscounter == NULL) break;

		i++;		
	}	nargs = i;

	char** argv;
	argv = (char**)kmalloc(sizeof(char*)*(nargs+1));

	if(argv == NULL){
		return ENOMEM;
	}

	for(i = 0; i < nargs; i++){
		argv[i] = (char*)kmalloc(sizeof(char)*MAX_PATH_LEN);
		result = copyinstr((userptr_t)args[i],argv[i],MAX_PATH_LEN,&bufflen);
		if(result){
			return result;
		}
	}
	argv[nargs] = NULL;
	// ********************** runprogram starts****************

	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	vaddr_t  *stackLoc[16];
	int wordsInCopy;

	result = vfs_open(program, O_RDONLY, &v);

	if (result) {
		return result;
	}

	// destroy the old addrspace
	if(curthread->t_vmspace != NULL){
		as_destroy(curthread->t_vmspace);
		curthread->t_vmspace = NULL;
	}

	assert(curthread->t_vmspace == NULL);

	// Create a new address space. 
	curthread->t_vmspace = as_create();
	if (curthread->t_vmspace==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	// Activate it. 
	as_activate(curthread->t_vmspace);

	// Load the executable. 
	result = load_elf(v, &entrypoint); // Load an ELF executable user program into the current address space and
	// returns the entry point (initial PC) for the program in ENTRYPOINT.
	if (result) {
		vfs_close(v);
		return result;
	}
	vfs_close(v);

	result = as_define_stack(curthread->t_vmspace, &stackptr);
	if (result) {
		// thread_exit destroys curthread->t_vmspace 
		return result;
	}

	//stuff for argument handling	
	int j;
	for(j = nargs - 1; j >= 0; j--){
		int copylen = strlen(argv[j]);	
		wordsInCopy = (int)4*((copylen/4)+1);
		stackptr -= wordsInCopy;
		copyout(argv[j], stackptr, wordsInCopy);
		stackLoc[j]=stackptr;
	}

	for(i = nargs; i >= 0; i--){
		stackptr -= 4;
		copyout(&stackLoc[i], stackptr, sizeof(void*));
	}
	/* Warp to user mode. */
	md_usermode(nargs/*argc*/, stackptr /*userspace addr of argv*/,
		    stackptr, entrypoint);
	
	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;
}


int
syscall_time(time_t *seconds, unsigned long *nanoseconds, int *retval)
{
	int result;
	struct curTime tv;
	
	gettime(&tv.seconds,&tv.nanoseconds);

	if(seconds != NULL){
		result = copyout(&tv.seconds,seconds,sizeof(time_t));
		if (result) {
			return EFAULT;
		}
	}

	if(nanoseconds != NULL){
		result = copyout(&tv.nanoseconds,nanoseconds,sizeof(unsigned long));
		if (result) {
			return EFAULT;
		}
	}

	*retval = tv.seconds;

	return 0;
}

int syscall_sbrk(int incr, int32_t* retval) {

	if(incr < (MINBRKCHK)){
		return EINVAL;
	}

	if(incr > (MAXBRKCHK)){
		return ENOMEM;
	}

	struct addrspace* as = curthread->t_vmspace;
	if (as->eheap + incr < as->sheap) {
		return EINVAL;
	}

	if (as->eheap + incr >= USERSTACK - MAXBRKCHK){
		return ENOMEM;
	}

	*retval = as->eheap;
	as->eheap += incr;
	return 0;
}