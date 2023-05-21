/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
//project 3 add header
#include "../include/userprog/process.h"
#include "../include/threads/mmu.h"
#include "../include/userprog/syscall.h"
//project 3 add header

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	// pml4 함수들에 & 를 넣음..
	file_seek(page->file.file , page->offset);
	if (pml4_is_dirty(thread_current()->pml4, page->va)) {
		lock_acquire(&filesys_lock);
		file_write(page->file.file, page->frame->kva, PGSIZE);
		lock_release(&filesys_lock);
		pml4_set_dirty(thread_current()->pml4, page->va,0);
	}
	pml4_clear_page(thread_current()->pml4, page->va);
}
bool
lazy_load_segment_file (struct page *page, void *aux) {
	// vm_claim_page(page);
	struct off_f *file_aux = aux;
	struct file *file = file_aux->file;
    off_t ofs = file_aux->ofs;
	page->offset = ofs;
    uint8_t *upage = file_aux-> upage;
    uint32_t read_bytes = file_aux->read_bytes;
    uint32_t zero_bytes = file_aux->zero_bytes;
    bool writable = file_aux->writable;

	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
	// 기존 load_segment 일부 발췌
	size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
	size_t page_zero_bytes = PGSIZE - page_read_bytes;
	file_seek (file, ofs);
	/* Get a page of memory. */
	uint8_t *kpage = page->frame->kva;
	if (kpage == NULL) {
		// ++
		free(file_aux);
		// ++
		return false;
	}
	/* Load this page. */
	if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
		palloc_free_page (kpage);
		// ++
		free(file_aux);
		// ++

		return false;
	}
	memset (kpage + page_read_bytes, 0, page_zero_bytes);
	// ++
	free(file_aux);
	// ++
	return true;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) {
	struct supplemental_page_table *spt = &thread_current()->spt;
	// ++

	// ++

	// if (addr == NULL || length <= 0) {
	// 	return NULL;
	// }
	void *start_addr = addr;
	// load_segment 참고 +++
	uint32_t read_bytes = length < file_length(file) ? length : file_length(file);
	uint32_t zero_bytes = (read_bytes%PGSIZE) == 0 ? 0 : PGSIZE - (read_bytes%PGSIZE);
	while (read_bytes > 0 || zero_bytes > 0) {
		struct page *page = spt_find_page(spt, addr);
		if (page != NULL) {
			return NULL;
		}
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct off_f *aux = (struct off_f *)calloc(1, sizeof(struct off_f));
		// 승훈 참고 +++ testcase close, remove 통과 - file 을 close 하고 다시 접근함. so, duplicate 필요.
		aux->file = file_duplicate(file);
		// 승훈 참고 +++
		aux->ofs = offset;
		aux->upage = addr;
		aux->read_bytes = page_read_bytes;
		aux->zero_bytes = page_zero_bytes;
		aux->writable = writable;
		if (!vm_alloc_page_with_initializer (VM_FILE, addr, writable, lazy_load_segment_file, aux)) {
			free(aux);
			return false;
		}
		
		// struct page *page = spt_find_page(spt, addr);
		// // ++
		// if(page == NULL) {
		// 	return NULL;
		// }
		// ++
		struct page *page_added = spt_find_page(spt, addr);
		page_added->file.file = aux->file;
		// page->file.file = aux->file;
		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;
		offset += page_read_bytes;
	}
	return start_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page *page = spt_find_page(spt, addr);
	if (page == NULL) {
		return;
	}
	struct file *file = page->file.file;

	off_t origin_ofs = file_tell(file);
	off_t offset = 0;
	int length = PGSIZE < file_length(file) ? PGSIZE : file_length(file);
	while (length > 0) {
		file = page->file.file;
		file_seek(file, page->offset);
		if (pml4_is_dirty(thread_current()->pml4, page->va)) {
			lock_acquire(&filesys_lock);
			file_write(file , page->frame->kva, PGSIZE);
			lock_release(&filesys_lock);
			pml4_set_dirty(thread_current()->pml4, page->va,0);
		}
		pml4_clear_page(thread_current()->pml4, page->va);
		hash_delete(&spt->hash_table, &page->hash_elem);
		// ++
		free(file);
		// ++

		length -= PGSIZE;
		addr += PGSIZE;
		offset += PGSIZE;
		// 도영 참고 +++
		page = spt_find_page(spt, addr);
		if (page == NULL) {
			return;
		}
		// 도영 참고 +++
	}
	// offset 처음 값으로 다시 돌려놓기.!
	file_seek(file, origin_ofs);
}
