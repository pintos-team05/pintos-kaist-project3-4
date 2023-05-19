/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
// project 3 add header
#include "../include/lib/kernel/hash.h"
#include "../include/userprog/process.h"
#include "../include/threads/mmu.h"
#include "../include/vm/uninit.h"
#include "../include/vm/anon.h"
#include "../include/lib/string.h"
// project 3 add header

//project 3 add function_prototype
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
struct page *page_lookup (struct supplemental_page_table *spt, const void *address);
//project 3 add function_prototype

/* Initializes the vi rtual memory subsystem by invoking each subsystem's
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
// vm_alloc_page_with_initializer (VM_ANON, upage, writable, lazy_load_segment, aux))
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)
	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check whether the upage is already occupied or not. */
	if (spt_find_page (spt, pg_round_down(upage)) == NULL) {
		struct page * new_page = calloc(1, sizeof(struct page));
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		// (tmp_pseudo)
		if (new_page == NULL) {
			return false;
		}
		if (type == VM_ANON) {
			uninit_new(new_page, upage , init , VM_ANON, aux, anon_initializer);
		}
		else if (type == VM_FILE) {
			uninit_new(new_page, upage , init , VM_FILE, aux, file_backed_initializer);	
		}
		else {
			palloc_free_page(new_page);
			goto err;
		}
		new_page->writable = writable;
		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt, new_page);
		// (tmp_pseudo)
	}	
	return false; // ??? docs QNA 에 따르면 false, 생각은 true;
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */
	page = page_lookup(spt, pg_round_down(va));
	if (page == NULL) {
		return NULL;
	}
	else {
		return page;
	}
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED, struct page *page UNUSED) {
	int succ = false;
	struct hash_elem *h_elem;
	/* TODO: Fill this function. */
	h_elem = hash_insert(&spt->hash_table, &page->hash_elem);
	if (h_elem == NULL) {
		return succ = true;
	}
	else {
		return succ;
	}
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
	// page-linear 는 calloc 으로바꾸면 됨. palloc 안됨.
	struct frame *frame = calloc(1, sizeof(struct frame));
	/* TODO: Fill this function. */
	frame->kva = palloc_get_page(PAL_USER);
	frame->page = NULL;
	if (frame == NULL) {
		PANIC("todo");
	}
	
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	// struct page *page = palloc_get_page(PAL_USER);
	vm_alloc_page(VM_ANON,addr,true);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED= &thread_current ()->spt;
	struct page *page = NULL;
	struct thread *th = thread_current();
	uintptr_t rsp = f->rsp;
	if (!user) {
		rsp = th->th_rsp;
	} 
	if (is_kernel_vaddr(addr)) {
		return false;
	}
	if (KERN_BASE > addr && USER_STACK < addr) {
		return false;
	}
	if ((rsp < addr || rsp-8 == addr) && (USER_STACK - (1<<20) <= addr)) {
		vm_stack_growth(pg_round_down(addr));
		/* TODO: Validate the fault */
		// check bad ptr;
		// check not_present;
		if ((page = spt_find_page(spt, pg_round_down(addr))) == NULL) {
			return false;
		}
		if (page->frame == NULL) {
			if (!vm_do_claim_page(page)) {
				return false;
			}
		}
		if (pml4_get_page(thread_current()->pml4, pg_round_down(addr)) == NULL) {
			return false;
		}
		// testcase mmap-ro
		if (!not_present) {
			return false;
		}
		/* TODO: Your code goes here */
	}
	else {
		/* TODO: Validate the fault */
		// check bad ptr;
		// check not_present;
		if ((page = spt_find_page(spt, pg_round_down(addr))) == NULL) {
			return false;
		}
		if (page->frame == NULL) {
			if (!vm_do_claim_page(page)) {
				return false;
			}
		}
		if (pml4_get_page(thread_current()->pml4, pg_round_down(addr)) == NULL) {
			return false;
		}
		if (!not_present) {
			return false;
		}
		/* TODO: Your code goes here */
	}
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
	struct thread *th = thread_current();
	/* TODO: Fill this function */
	page = spt_find_page(&th->spt, pg_round_down(va));
	if (page == NULL) {
		return false;
	}
	return vm_do_claim_page (page);
}
// project 3 add to copy function
static bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}
// project 3 add to copy function
/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	bool success = install_page(page->va, frame->kva, page->writable);
	if (success == NULL) {
		return false;
	}
	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->hash_table, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
		struct hash_iterator i;
		hash_first (&i, &src->hash_table);
		while (hash_next(&i)) {
			struct page *page_parent = hash_entry (i.elem,struct page, hash_elem);
			enum vm_type type_parent = page_get_type(page_parent);
			switch (VM_TYPE(page_parent->operations->type))
			{
			case VM_UNINIT:
				vm_alloc_page(type_parent, page_parent->va, page_parent->writable);
				break;
			case VM_ANON:
				vm_alloc_page(type_parent, page_parent->va, page_parent->writable);
				struct page *page_child_anon = spt_find_page(dst, page_parent->va);
				if (page_child_anon == NULL) {
					return false;
				}
				if (!vm_do_claim_page(page_child_anon)) {
					return false;
				}
				memcpy(page_child_anon->frame->kva, page_parent->frame->kva, PGSIZE);
				break;
			case VM_FILE:
				vm_alloc_page(type_parent, page_parent->va, page_parent->writable);
				struct page *page_child_file = spt_find_page(dst, page_parent->va);
				if (page_child_file == NULL) {
					return false;
				}
				if (!vm_do_claim_page(page_child_file)) {
					return false;
				}
				if (page_parent->file.file != NULL){
					page_child_file->file.file = file_duplicate(page_parent->file.file);
				}
				memcpy(page_child_file->frame->kva, page_parent->frame->kva, PGSIZE);
				page_child_file->file.file = file_duplicate(page_parent->file.file);
				break;
			default:
				break;
			}
		}
	return true;
}


void
destroyer (struct hash_elem *hash_elem, void *aux) {
	struct page *p = hash_entry (hash_elem, struct page, hash_elem);
	destroy(p);
}
/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	// hash_destroy(&spt->hash_table, destroyer);
	hash_clear(&spt->hash_table, destroyer);
	
}


//project 3 add function

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

/* Returns the page containing the given virtual address, or a null pointer if no such page exists. */
struct page *
page_lookup (struct supplemental_page_table *spt, const void *address) {
	struct page p;
	struct hash_elem *e;

	p.va = address;
	// hash_elem 에 우리가 원하는 address 만 들어있어도 찾아낼 수 있음.
	e = hash_find (&spt->hash_table, &p.hash_elem);
	return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

//project 3 add function