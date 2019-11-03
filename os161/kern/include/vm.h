#ifndef _VM_H_
#define _VM_H_

#include <machine/vm.h>
#include <thread.h>
/*
 * VM system-related definitions.
 *
 * You'll probably want to add stuff here.
 */

struct C_ENTRY {
	int id;	
	struct addrspace* as;
	int state;  // freed = 0, fixed = 1, dirty = 2
	paddr_t p_as; 
	vaddr_t v_as; 
	int length; 
};

/* Fault-type arguments to vm_fault() */
#define VM_FAULT_READ        0    /* A read was attempted */
#define VM_FAULT_WRITE       1    /* A write was attempted */
#define VM_FAULT_READONLY    2    /* A write to a readonly page was attempted*/

/* Initialization function */
void vm_bootstrap(void);

/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress);
void vm_fault_stack(int faulttype, vaddr_t faultaddress, int *retval);
void vm_fault_heap(int faulttype, vaddr_t faultaddress, int *retval, struct addrspace* as);


/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(int numberOfPages);
void free_kpages(vaddr_t addr);

#define FL_PN 0xffc00000 
#define SL_PN 0x003ff000	
#define PAGE_FRAME 0xfffff000	

vaddr_t allocate_one();
vaddr_t allocate_multiple(int numberOfPages);
u_int32_t* retEntry(struct thread* addrspace_owner, vaddr_t va);
paddr_t  load_seg(int id, struct addrspace* as, vaddr_t v_as);
int fix_faults (vaddr_t faultaddress, unsigned int permissions);
void check_levels(vaddr_t faultaddress, paddr_t* paddr);
paddr_t alloc_page_userspace(struct addrspace * as, vaddr_t v_as);
int c_entry_freed_state();

#endif /* _VM_H_ */