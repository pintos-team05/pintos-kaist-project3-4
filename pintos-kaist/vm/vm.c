/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}


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
	struct page *page = spt_find_page(spt, upage);
	if (page == NULL) { 

		/* TODO: Create the page, fetch the initializer according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		/* TODO: Insert the page into the spt. */
		struct page *page_new = (struct page *)malloc(sizeof(struct page));
		// page->va = palloc_get_page(PAL_USER);
		if (page_new != NULL){
			switch (VM_TYPE(type)) {
				case VM_ANON:
					page_new->frame = NULL;
					uninit_new(page_new, upage, init, type, aux, &anon_initializer);
					break;
				case VM_FILE:
					page_new->frame = NULL;
					uninit_new(page_new, upage, init, type, aux, &file_backed_initializer);
					break;
				default:
				    free(page_new);
					return false;
			}
			// page->va = upage;
			return spt_insert_page(spt, page_new);
		}
	}
	return false;  // true ? or false ?
	
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = calloc(1, sizeof(struct page));

	struct page p;
	struct hash_elem *e;

	/* TODO: Fill this function. */
	p.va = pg_round_down(va);
	e = hash_find(&spt->pages, &p.hash_elem);
	if (e != NULL){
		page = hash_entry(e, struct page, hash_elem);
		return page;
	}
	else{
		free(page);
		return NULL;
	}
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	// 1. spt에서 page->addr이 존재하는지 검사
	// 2. pages hash에 page 삽입
	if (hash_insert(&spt->pages, &page->hash_elem) == NULL){
		succ = true;
	}

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
	struct frame *frame = calloc(1, sizeof(struct frame));
	// struct frame *frame = palloc_get_page(PAL_USER);
	/* TODO: Fill this function. */
	frame->kva = palloc_get_page(PAL_USER);
	frame->page = NULL;

	
	if (frame->kva == NULL){
		PANIC("todo"); // swap in / out
	}
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

void validate_address2(void *addr) {
	struct thread *t = thread_current();
	if (is_kernel_vaddr(addr) || (pml4_get_page(t->pml4, addr)) == NULL)
		exit(-1);
}


/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {

	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	
	/* TODO: Validate the fault */
	if (is_kernel_vaddr (addr))
		return false;

	page = spt_find_page(spt, pg_round_down(addr));
	
	if (page == NULL){
		return false;
	}
	
	if (page->frame == NULL){
		/* The page has not been initialized yet */
		// obtain the base address of the page containing the faulted virtual address
		if (!vm_do_claim_page(page))
			return false;
	}
	if (pml4_get_page(thread_current()->pml4, pg_round_down(addr)) == NULL)
		return false;

	// validate_address2(addr);
	


	// page->uninit.init(page, page->uninit.aux);
	// load segment;
	
	/* TODO: Your code goes here */

	return true;
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

	struct thread *t = thread_current();

	/* TODO: Fill this function */
	page = spt_find_page(&t->spt, va);
	if(page != NULL)	
		return vm_do_claim_page (page);
	else
		return false;
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	bool success = install_page(page->va, frame->kva, 1);
	// pml4_set_page(&thread_current()->pml4, page, frame, 1);

	if (!success) {
	/* If page table entry insertion fails, free the frame and remove links */
		frame->page = NULL;
		page->frame = NULL;
		// vm_free_frame(frame);
		return false;
	}
	
	return swap_in (page, frame->kva);
}


/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
	const struct page *p = hash_entry (p_, struct page, hash_elem);
	return hash_bytes (&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED) {
	const struct page *a = hash_entry (a_, struct page, hash_elem);
	const struct page *b = hash_entry (b_, struct page, hash_elem);

	return a->va < b->va;
}


/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {

	hash_init(&spt->pages, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {

	struct hash_iterator i;
	hash_first(&i, &src->pages);
	while (hash_next (&i)){

		struct page *parent_page = hash_entry (hash_cur (&i), struct page, hash_elem);
		enum vm_type type_parent = page_get_type(parent_page);
		
		switch (VM_TYPE (parent_page->operations->type)) {

			case VM_UNINIT:
				vm_alloc_page(type_parent, parent_page->va, true);
				break;
			case VM_ANON:
				vm_alloc_page(type_parent, parent_page->va, true);
				struct page *child_page = spt_find_page(dst, parent_page->va);

				if (child_page == NULL)
					return false;

				if (!vm_do_claim_page(child_page))
					return false; 

				memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);
				break;
			case VM_FILE:
				break;
			default:
				break;
			}
				
		}
	return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	// struct hash_iterator i;
	// hash_first(&i, &spt->pages);
	// while (hash_next (&i)){
		
	// }

}
