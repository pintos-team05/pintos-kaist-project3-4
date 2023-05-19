/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "lib/kernel/hash.h"
#include <string.h>

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		// struct page *page_new = palloc_get_page(0);
		struct page *page_new = (struct page *)malloc(sizeof(struct page));
		if(page_new == NULL) 
			goto err;
		
		switch (VM_TYPE(type))
		{
		case VM_ANON:
			/* anon_initializer */
			uninit_new(page_new, upage, init, type, aux, anon_initializer);
			break;
		case VM_FILE:
			/* file_backed_initializer */
			uninit_new(page_new, upage, init, type, aux, file_backed_initializer);
			break;
		default:
			break;
		}
		/* TODO: Insert the page into the spt. */
		return spt_insert_page (spt, page_new);
	}
	return false;
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */
	struct page p;
	struct hash_elem *e;

	p.va = pg_round_down(va);
	e = hash_find(&spt->pages, &p.hash_elem);
	
	if (e == NULL)
		return NULL;
	
	return hash_entry(e, struct page, hash_elem);
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	struct hash_elem *result = hash_insert(&spt->pages, &page->hash_elem);
	if (result == NULL)
		succ = true;
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	frame = (struct frame*)malloc(sizeof(struct frame));
	frame->kva = palloc_get_page(PAL_USER);
	frame->page = NULL;
	/* TODO: swap_out */
	if (frame->kva == NULL)
	{
		PANIC("Todo: swapping");
	}
	
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	struct thread *curr = thread_current ();
	// void *rsp = pg_round_down(curr->rsp_user);
	void *new_stack_bottom = (void *) ((uint8_t *) pg_round_down(addr));

	vm_alloc_page(VM_ANON, new_stack_bottom, true);
	bool success = vm_claim_page(new_stack_bottom);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	/* May be need to handler more case */
	/* page fault caused by stack pointer */
	struct thread *curr = thread_current ();
	/* The maximum stack size of project3 is a 1MB */
	if (user && ((void*)f->rsp < addr || (void*)f->rsp -8 == addr))
	{
		if(addr >= (USER_STACK - PGSIZE*256) && addr <= USER_STACK)
		{
			curr->rsp_user = f->rsp;
			vm_stack_growth(addr);
			return true;
		}
	}
	else if(!user && ((void*)curr->rsp_user < addr || (void*)curr->rsp_user -8 == addr))
	{
		if(addr >= (USER_STACK - PGSIZE*256) && addr <= USER_STACK)
		{
			vm_stack_growth(addr);
			return true;
		}
	}

	page = spt_find_page (spt, addr);
	if(page == NULL || (is_kernel_vaddr(addr)&& user))
		return false;
	
	if (page->frame == NULL)
	{
		if (!vm_do_claim_page(page))
			return false;
	}
	if((pml4_get_page (thread_current ()->pml4, addr) == NULL))
		return false;

	if (!not_present)
		return false;

	return true;
	// return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	struct thread *t = thread_current();
	page = spt_find_page(&t->spt, va);
	
	if (page == NULL)
		return NULL;

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	/* Is address validation need? */

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	struct thread *curr = thread_current();
	if (!pml4_set_page(curr->pml4, page->va, frame->kva, true))
		return false;

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	/* Project 3  */
	hash_init(&spt->pages, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	struct hash_iterator i;

   hash_first (&i, src);
   while (hash_next (&i))
   {
   		struct page *page_parent = hash_entry (hash_cur (&i), struct page, hash_elem);
		enum vm_type type_parent = page_get_type (page_parent);
		int ty = VM_TYPE (page_parent->operations->type);
		struct page *page_child;
		struct frame *frame_child;
		switch (ty)
		{
		case VM_UNINIT:
			vm_alloc_page(type_parent, page_parent->va, true);
			/* Need to be consider the init function and aux variable */
			break;
		case VM_ANON:
			vm_alloc_page(type_parent, page_parent->va, true);
			page_child = spt_find_page(dst, page_parent->va);
			
			if (page_child == NULL)
				return NULL;
			
			if(!vm_do_claim_page(page_child))
				return false;
			
			frame_child = page_child->frame;
			memcpy(frame_child->kva, page_parent->frame->kva, PGSIZE);
			break;
		case VM_FILE:
			vm_alloc_page(type_parent, page_parent->va, true);
			page_child = spt_find_page(dst, page_parent->va);
			
			if (page_child == NULL)
				return NULL;
			
			if(!vm_do_claim_page(page_child))
				return false;
			
			frame_child = page_child->frame;
			if(page_parent->map_file != NULL)
			{
				page_child->map_file = file_duplicate(page_child->map_file);
				page_child->offset = page_parent->offset;
			}
			memcpy(frame_child->kva, page_parent->frame->kva, PGSIZE);
			break;
		default:
			break;
		}
   }
   return true;
}
void destroyer (struct hash_elem *target_e, void *aux)
{
	struct page *target = hash_entry(target_e, struct page, hash_elem);
	destroy(target); 
	// palloc_free_page(target);
}
/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	if (spt==NULL)
		return;
	
	// hash_destroy(&spt->pages, destroyer);
	hash_clear(&spt->pages, destroyer);

}

/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}
/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->va < b->va;
}