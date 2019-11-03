#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/spl.h>
#include <machine/tlb.h>
#include <vfs.h>
#include <elf.h>


/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground. You should replace all of this
 * code while doing the VM assignment. In fact, starting in that
 * assignment, this file is not included in your kernel!
 */

struct C_ENTRY* COREMAP;
size_t numberOfEntries;
size_t staticPages;

int init_vm = 0;
int found = 0;

extern struct thread* curthread;


void
vm_bootstrap(void)
{
	u_int32_t first_p_as;  
	u_int32_t last_p_as;
	u_int32_t new_beg;
	u_int32_t c_size;
	ram_getsize(&first_p_as, &last_p_as);

	numberOfEntries = (last_p_as - first_p_as)/ PAGE_SIZE;

	COREMAP = (struct C_ENTRY*)PADDR_TO_KVADDR(first_p_as);
	c_size = numberOfEntries * sizeof(struct C_ENTRY);
	new_beg = first_p_as + c_size; 

	int staticPages = (new_beg - first_p_as) / PAGE_SIZE + 1;
	int i;
	for (i = 0; i < numberOfEntries; ++i) {

		COREMAP[i].as = NULL;
		COREMAP[i].id = i;
		COREMAP[i].p_as = first_p_as + PAGE_SIZE * i;

		if(i > staticPages) {
			COREMAP[i].v_as = 0xDEADBEEF; 
			COREMAP[i].state = 0;
			COREMAP[i].length = 0;
		} 
		else {
			COREMAP[i].v_as = PADDR_TO_KVADDR(COREMAP[i].p_as);
			COREMAP[i].state = 1;
			COREMAP[i].length = 1; 
						
		}
	}

	init_vm = 1;
}

static
paddr_t
getppages(unsigned long npages)
{
	int spl;
	paddr_t addr;

	spl = splhigh();

	addr = ram_stealmem(npages);
	
	splx(spl);
	return addr;
}

vaddr_t allocate_one() {

	int freed_id = c_entry_freed_state();

	COREMAP[freed_id].as = curthread->t_vmspace;
	COREMAP[freed_id].state = 1; 
	COREMAP[freed_id].v_as = PADDR_TO_KVADDR(COREMAP[freed_id].p_as);
	COREMAP[freed_id].length = 1;

	return (COREMAP[freed_id].v_as);
}

vaddr_t allocate_multiple(int numberOfPages) {


	int cont = 1;
	int i; 

	for (i = 0; i < numberOfEntries - 1; i++){

		if (cont >= numberOfPages) break;
		if ((COREMAP[i].state == 0) && (COREMAP[i + 1].state == 0)) {
			cont++;
		} else {
			cont = 1;
		}
	}

	int start = i - numberOfPages + 1;
	if (cont >= numberOfPages) {
		int j;
		for (j = start; j < numberOfPages + start; j++){
			COREMAP[j].as = curthread->t_vmspace;
			COREMAP[j].state = 1;
			COREMAP[j].v_as = PADDR_TO_KVADDR(COREMAP[j].p_as);
			COREMAP[j].length = numberOfPages; 
		}

		return PADDR_TO_KVADDR(COREMAP[start].p_as);

	} else {
		int found = 0;
		int continous = 0;
		for (i = 0; i < numberOfEntries; i++) {
			if (continous >= numberOfPages) {
				found = 1;
				break;
			}
			if(COREMAP[i].state == 1) {
				continous = 0;
				continue;
			} else {
				continous ++;
			}
		}
		if(!found) {
			return NULL;
		}
		int starting_frame = i - numberOfPages + 1;

		for (i = starting_frame; i < numberOfPages + starting_frame; i++) {
			COREMAP[i].as = curthread->t_vmspace;
			COREMAP[i].state = 1;
			COREMAP[i].v_as = PADDR_TO_KVADDR(COREMAP[i].p_as);
			COREMAP[i].length = numberOfPages; 
		}
		return PADDR_TO_KVADDR(COREMAP[starting_frame].p_as);
	}
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t 
alloc_kpages(int numberOfPages)
{	
	int spl;
	spl = splhigh();

	if(init_vm == 1){
		vaddr_t	ret;
		if(numberOfPages == 1)
			ret = allocate_one();
		else
			ret = allocate_multiple(numberOfPages);

		splx(spl);
		return ret; 	
	} 
	else {
		vaddr_t vaddr = getppages(numberOfPages);
		splx(spl);
		return PADDR_TO_KVADDR(vaddr);
	}
}

void 
free_kpages(vaddr_t addr)
{	
	int spl;
	spl = splhigh();

	int i = 0;
	for (i = 0; i < numberOfEntries; i++) {
		if (PADDR_TO_KVADDR(COREMAP[i].p_as) == addr) {
			int numberOfEntriesToFree = COREMAP[i].length;
			int j;
			for (j = 0; j < numberOfEntriesToFree; j++) {
				COREMAP[j + i].v_as = 0xDEADBEEF;
				COREMAP[j + i].state = 0;
				COREMAP[j + i].length = 0;
			}
			splx(spl);
			return;
		}
	}
}


u_int32_t* retEntry (struct thread* addrspace_owner, vaddr_t va){

	int pt_l1_index = (va & FL_PN) >> 22; 
	int pt_l2_index = (va & SL_PN) >> 12;
	struct as_pagetable* lvl2_ptes = addrspace_owner->t_vmspace->as_ptes[pt_l1_index];

	if (lvl2_ptes == NULL) 
		return NULL;
	else 
		return &(lvl2_ptes->PTE[pt_l2_index]);
}


int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	found = 0;
	struct addrspace *as;
	int spl;
	int err; 
	spl = splhigh();

	faultaddress &= PAGE_FRAME; 

	if(faulttype == VM_FAULT_READONLY){
		splx(spl);
		return EFAULT;
	}
	else if(faulttype != VM_FAULT_READ && faulttype != VM_FAULT_WRITE){
		splx(spl);
		return EINVAL;
	}
	as = curthread->t_vmspace;

	if (as == NULL) {
		/*
		 * No address space set up. This is probably a kernel
		 * fault early in boot. Return EFAULT so as to panic
		 * instead of getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	unsigned int permissions = 0;
	vaddr_t bottom_vm, top_vm;

	int i = 0;
	for (i; i < array_getnum(as->as_regions); i++) {

		struct as_region * cur = array_getguy(as->as_regions, i);
		bottom_vm = cur->bottom_vm;
		top_vm = bottom_vm + cur->npages * PAGE_SIZE;

		if(faultaddress >= bottom_vm && faultaddress < top_vm){
			found = 1;
			permissions = (cur->region_permis);
			err = fix_faults(faultaddress, permissions); 
			splx(spl);
			return err;
		}
	}

	//check stack if not found
	if(!found){
		vm_fault_stack(faulttype, faultaddress, &err);
		if(found){
			splx(spl);
			return err;
		}
	}
	// check heap if not found
	if(!found){
		vm_fault_heap(faulttype, faultaddress, &err, as);
		if(found){
			splx(spl);
			return err;
		}
	}
	// cannot find anywhere, return EFAULT

	splx(spl);
	return EFAULT;
}

void vm_fault_stack(int faulttype, vaddr_t faultaddress, int* retval){
	unsigned int permissions = 0;
	vaddr_t bottom_vm;
	vaddr_t top_vm;
	top_vm = USERSTACK;

	bottom_vm = top_vm - 24 * PAGE_SIZE; 
	if(faultaddress >= bottom_vm && faultaddress < top_vm){
		found = 1;
		permissions = 6;
		*retval = fix_faults(faultaddress, permissions);
	}
}

void vm_fault_heap(int faulttype, vaddr_t faultaddress, int* retval, struct addrspace* as){
	unsigned int permissions = 0;
	vaddr_t bottom_vm;
	vaddr_t top_vm;

	bottom_vm = as->sheap;
	top_vm = as->eheap;
	if(faultaddress >= bottom_vm && faultaddress < top_vm){
		found = 1;
		permissions = 6;
		*retval = fix_faults(faultaddress, permissions);	
	}
}

int fix_faults(vaddr_t faultaddress, unsigned int permissions) {

	int spl = splhigh();
	vaddr_t vaddr;
	paddr_t paddr;
 
 	//function to see if second level page table exists or not, handles accordingly
 	check_levels(faultaddress, &paddr);

	//load into TLB
	if (permissions & PF_W) {
		paddr |= TLBLO_DIRTY;  
	}
	
	u_int32_t tlb_end, tlb_start;
	int k = 0;	
	for(k; k< NUM_TLB; k++){
		TLB_Read(&tlb_end, &tlb_start, k);
		// skip valid ones
		if(tlb_start & TLBLO_VALID){
			continue;
		}
		//fill first empty one
		tlb_end = faultaddress;
		tlb_start = paddr | TLBLO_VALID; 
		TLB_Write(tlb_end, tlb_start, k);
		splx(spl);
		return 0;
	}
	// no invalid ones => pick entry and random and expel it
	tlb_end = faultaddress;
	tlb_start = paddr | TLBLO_VALID;
	TLB_Random(tlb_end, tlb_start);
	splx(spl);
	return 0;
}

void check_levels(vaddr_t faultaddress, paddr_t* paddr){

	int pt_l1_index = (faultaddress & FL_PN) >> 22; 
	int pt_l2_index = (faultaddress & SL_PN) >> 12;
	// check if the 2nd level page table exists
	struct as_pagetable *lvl2_ptes = curthread->t_vmspace->as_ptes[pt_l1_index];

	if(lvl2_ptes != NULL) {
	
		u_int32_t *pte = &(lvl2_ptes->PTE[pt_l2_index]);

		if (*pte & PTE_PRESENT) {
			// page is present in physical memory
			*paddr = *pte & PAGE_FRAME; 
		} 
		else {
			 if (*pte) { 
			 	int freed_id = c_entry_freed_state();
				*paddr = load_seg(freed_id, curthread->t_vmspace, faultaddress);

			 } else {
				// page does not exist
				*paddr = alloc_page_userspace(NULL, faultaddress);
				}
			// now update the PTE
			*pte &= 0x00000fff;
			*pte |= *paddr;
	    	*pte |= PTE_PRESENT;
		}
	} else {

		// If second page table doesn't exist, create one
		curthread->t_vmspace->as_ptes[pt_l1_index] = kmalloc(sizeof(struct as_pagetable));
		lvl2_ptes = curthread->t_vmspace->as_ptes[pt_l1_index];

		int i = 0;
		for (i; i < PT_SIZE; i++) {
			lvl2_ptes->PTE[i] = 0;
		}
	    // allocate a page and do the mapping
	    *paddr = alloc_page_userspace(NULL, faultaddress);
		
	    u_int32_t* pte = retEntry(curthread, faultaddress); 

		
		*pte &= 0x00000fff;
	    *pte |= PTE_PRESENT;
	    *pte |= *paddr;
	}
}

int c_entry_freed_state() {

	int id = -1;
	int i; 
	for (i = 0; i < numberOfEntries; i++) {
		if (COREMAP[i].state == 0){
			id = i;
			break;
		}
	}
	return id;
}

void create_level_2(vaddr_t faultaddress, u_int32_t* pte, struct thread* thread){

}
paddr_t alloc_page_userspace(struct addrspace * as, vaddr_t v_as) {

	int freed_id = c_entry_freed_state();

	if(as == NULL)
		COREMAP[freed_id].as = curthread->t_vmspace;
	else 
		COREMAP[freed_id].as = as;

	COREMAP[freed_id].state = 2;
	COREMAP[freed_id].v_as = v_as;
	COREMAP[freed_id].length = 1;

	return COREMAP[freed_id].p_as;
}

paddr_t load_seg(int id, struct addrspace* as, vaddr_t v_as) {

	COREMAP[id].state = 2; 
	COREMAP[id].as = as;
	COREMAP[id].v_as = v_as;
	COREMAP[id].length = 1;

	return COREMAP[id].p_as;
}


