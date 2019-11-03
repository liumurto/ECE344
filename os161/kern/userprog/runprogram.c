/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <thread.h>
#include <curthread.h>
#include <vm.h>
#include <vfs.h>
#include <test.h>

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char *progname, char **args, unsigned long nargs	)
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	vaddr_t  *stackLoc[16];
	int result;
	int wordsInCopy;

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, &v);
	if (result) {
		return result;
	}

	/* We should be a new thread. */
	assert(curthread->t_vmspace == NULL);

	/* Create a new address space. */
	curthread->t_vmspace = as_create();
	if (curthread->t_vmspace==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Activate it. */
	as_activate(curthread->t_vmspace);

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_vmspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		return result;
	}
	//stuff for argument handling
	int i;	
	args[nargs] = NULL;
	for(i = nargs - 1; i >= 0; i--){
		int copylen = strlen(args[i]);	
		wordsInCopy = (int)4*((copylen/4)+1);
		stackptr -= wordsInCopy;
		copyout(args[i], stackptr, wordsInCopy);
		kprintf(stackptr);
		stackLoc[i]=stackptr;
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