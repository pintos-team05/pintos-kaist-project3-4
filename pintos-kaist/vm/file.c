/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "threads/mmu.h"
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
	// 1. file length 찾고, file length가 mapping된 mmaped area 크기만큼 file write 해 준 뒤
	// 2. file close 
	// struct file_page *file_page UNUSED = &page->file;
	// if (page == NULL)
	// 	return;

	// struct supplemental_page_table *spt = &thread_current()->spt;
	// uint64_t *pml4 = thread_current()->pml4;


	
	// void *mmaped_ptr = page->va;

	// struct file *mmaped_file = page->mmaped_file;
	// uint32_t length = file_length(mmaped_file);
	// uint32_t num_pages = (length % PGSIZE == 0) ? (int) length/PGSIZE : (int)(length/PGSIZE + 1);
	// uint32_t offset = 0;

	// while (num_pages != 0) {
	// 	struct page *mmaped_page = spt_find_page(spt, mmaped_ptr);
	// 	file_seek(mmaped_file, offset);
	// 	if(pml4_is_dirty(pml4, mmaped_ptr)){
	// 		if (!file_write(mmaped_file, mmaped_page->frame->kva, PGSIZE))
	// 			return;
	// 	}

	// 	num_pages -= 1;
	// 	hash_delete(&spt->pages, &mmaped_page->hash_elem);
	// 	mmaped_page->va = NULL;
	// 	mmaped_ptr += PGSIZE;
	// 	offset += PGSIZE;
	// }
	// file_close(mmaped_file);

}


static bool
lazy_load_segment_filebacked (struct page *page, void *aux) {

	struct lazy_load_info *info_filebacked = (struct lazy_load_info*) aux;
	struct file *file = info_filebacked->file;
	off_t ofs = info_filebacked->ofs;
	uint8_t *upage = info_filebacked->upage;
	uint32_t page_read_bytes = info_filebacked->read_bytes;
	uint32_t page_zero_bytes = info_filebacked->zero_bytes;
	bool writable = info_filebacked->writable;

	uint8_t *kpage = page->frame->kva;
	uint64_t *pml4 = thread_current()->pml4;

	struct thread *cur = thread_current();
	struct file **fdt = cur->fdt;

	// int fd = 2;
	// for (; fd<FDCOUNT_LIMIT; fd++){
	// 	if (fdt[fd] == file)
	// 		break;
	// }
	// if (fd == FDCOUNT_LIMIT)
	// 	return false;

	if (kpage == NULL)
		return false;
		
	file_seek(file, ofs);
	
	if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
		free(info_filebacked);
		palloc_free_page (kpage);
		return false;
	}

	memset (kpage + page_read_bytes, 0, page_zero_bytes);
	pml4_set_page(pml4, upage, kpage, writable);

	free(info_filebacked);

	return true;
	
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */

}


/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
		
		// length : 4096, file_length(file) : 794

		uint32_t read_bytes = (length > file_length(file)) ? file_length(file): length;
		uint32_t zero_bytes = (read_bytes % PGSIZE) == 0 ? 0 : PGSIZE - (read_bytes % PGSIZE);
		void *start_addr = addr;

		while (read_bytes > 0 || zero_bytes > 0) {
			if (spt_find_page(&thread_current()->spt, addr) != NULL)
				return NULL;

			size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
			size_t page_zero_bytes = PGSIZE - page_read_bytes;

			void *aux = NULL;
			struct lazy_load_info *info_filebacked = (struct lazy_load_info *)malloc(sizeof(struct lazy_load_info));
			if (info_filebacked == NULL) return NULL;

			info_filebacked->file = file_duplicate(file);
			info_filebacked->ofs = offset;
			info_filebacked->upage = addr;
			info_filebacked->read_bytes = page_read_bytes;
			info_filebacked->zero_bytes = page_zero_bytes;
			info_filebacked->writable = writable;
			aux = info_filebacked;

			if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment_filebacked, aux)){
				free(info_filebacked);
				return NULL;
			}

			struct page *page = spt_find_page(&thread_current()->spt, addr);
			page->mmaped_file = file_duplicate(file);

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
	uint64_t *pml4 = thread_current()->pml4;
	struct page *page = spt_find_page(spt, addr);
	if (page == NULL)
		return;

	
	void *mmaped_ptr = addr;

	struct file *mmaped_file = page->mmaped_file;
	int offset_org = file_tell(mmaped_file);
	uint32_t length = file_length(mmaped_file);
	uint32_t num_pages = (length % PGSIZE == 0) ? (int) length/PGSIZE : (int)(length/PGSIZE + 1);
	uint32_t offset = 0;

	
	while (num_pages != 0) {
		struct page *mmaped_page = spt_find_page(spt, mmaped_ptr);
		file_seek(mmaped_file, offset);
		if(pml4_is_dirty(pml4, addr)){
			if (!file_write(mmaped_file, mmaped_page->frame->kva, PGSIZE))
				return;
		}

		num_pages -= 1;
		hash_delete(&spt->pages, &mmaped_page->hash_elem);
		mmaped_page->va = NULL;
		mmaped_ptr += PGSIZE;
		offset += PGSIZE;
	}

	file_seek(mmaped_file, offset_org);
	
}
