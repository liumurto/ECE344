#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
//#include <bitmap.h>
#include <machine/tlb.h>
#include <elf.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */
extern size_t numberOfEntries;
extern struct C_ENTRY* COREMAP;

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	as->as_regions = array_create();
	as->sheap = 0;
	as->eheap = 0;

	int i;
	for (i = 0; i < PT_SIZE; i++){
		as->as_ptes[i] = NULL;
	}

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	int spl = splhigh();
	struct addrspace *newas;

	newas = as_create();
	if (newas == NULL) {
		return ENOMEM;
	}

	as_copy_regions(newas, old);

	as_copy_heap(newas, old);

	as_copy_pte(newas, old);

	*ret = newas;
	splx(spl);
	return 0;
}

void as_copy_regions(struct addrspace *newas, struct addrspace *source){
	unsigned int i;
	for (i = 0; i < array_getnum(source->as_regions); i++) {
		struct as_region* temp = kmalloc(sizeof(struct as_region));
		*temp = *((struct as_region*)array_getguy(source->as_regions, i));
		array_add(newas->as_regions, temp);
	}
}

void as_copy_heap(struct addrspace *newas, struct addrspace *source){
	newas->sheap = source->sheap;
	newas->eheap = source->eheap;
	newas->permissions = source->permissions;
}

void as_copy_pte(struct addrspace *newas, struct addrspace *source){
	int i;
	for (i = 0; i < PT_SIZE; i++) {
		if(source->as_ptes[i] != NULL) {
			newas->as_ptes[i] = (struct as_pagetable*)kmalloc(sizeof(struct as_pagetable));

			struct as_pagetable *src = source->as_ptes[i];
			struct as_pagetable *copy = newas->as_ptes[i];
			int j;
			for (j = 0; j < PT_SIZE; j++) {
				
				copy->PTE[j] = 0;
				if(src->PTE[j] & PTE_PRESENT) {
			
					paddr_t src_paddr = (src->PTE[j] & PAGE_FRAME);
					vaddr_t dest_vaddr = (i << 22) + (j << 12);
			
					paddr_t dest_paddr = alloc_page_userspace(newas, dest_vaddr);

					memmove((void *) PADDR_TO_KVADDR(dest_paddr),
					(const void*)PADDR_TO_KVADDR(src_paddr), PAGE_SIZE) ;
				
					copy->PTE[j] |= dest_paddr;
					copy->PTE[j] |= PTE_PRESENT;

				}  
				else {
					//page not present, do nothing
					copy->PTE[j] = 0;
				}
			}
		} 
		else {
			newas->as_ptes[i] = NULL;
		} 
	}
}

void
as_destroy(struct addrspace *as)
{
	int spl = splhigh();
	int i = 0;
	//free all COREMAP entries
	for (i; i < numberOfEntries; i++) {
		if(COREMAP[i].state != 0 && COREMAP[i].as == as){
			COREMAP[i].as = NULL;
			COREMAP[i].v_as = 0;
			COREMAP[i].state = 0;
			COREMAP[i].length = 0;
		}
	}

	array_destroy(as->as_regions);

	for(i = 0; i < PT_SIZE; i++) {
		if(as->as_ptes[i] != NULL)
			kfree(as->as_ptes[i]);
	}

	kfree(as);
	splx(spl);
	return;
}

void
as_activate(struct addrspace *as)
{
	int i;
	int spl;

	(void)as; // suppress warning until code gets written

	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		TLB_Write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	size_t npages; 

	sz += vaddr & 0x00000fff;	 
	vaddr &= PAGE_FRAME;


	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;


	struct as_region *new_region = kmalloc(sizeof(struct as_region));
	new_region->bottom_vm = vaddr;
	new_region->npages = npages;

	new_region->region_permis = 0;
	new_region->region_permis = (readable | writeable | executable);
	array_add(as->as_regions, new_region);


	if(array_getnum(as->as_regions) == 2){
		as->sheap = vaddr + npages * PAGE_SIZE;
		as->eheap = as->sheap;
	}
	return 0;
}

int
as_prepare_load(struct addrspace *as)
{

	struct as_region* text = (struct as_region*)array_getguy(as->as_regions, 0); 

	as->permissions = text->region_permis;

	text->region_permis |= (PF_R | PF_W);


	return 0;
}

int
as_complete_load(struct addrspace *as)
{

	struct as_region* text = (struct as_region*)array_getguy(as->as_regions, 0); 

	text->region_permis = as->permissions;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

	(void)as;
	*stackptr = USERSTACK;

	return 0;
}