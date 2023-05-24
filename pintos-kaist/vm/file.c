/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
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
	
	struct page *p = page;

	if (p == NULL)
		return false;
	
	struct file *file = p->map_file;
	if (file == NULL)
		return false;

	file_seek(file, p->offset);
	file_read(file, kva, PGSIZE);
	
	pml4_set_page(thread_current()->pml4, page->va, kva, page->writable);

	if(lock_held_by_current_thread(&swap_lock))
		lock_release(&swap_lock);
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;

	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *p = page;

	if (p == NULL)
		return false;
	
	struct file *file = p->map_file;
	if (file == NULL)
	{
		pml4_clear_page(thread_current()->pml4, p->va);
		palloc_free_page(p->frame->kva);
		free(p->frame);
		return true;
	}

	if(pml4_is_dirty (thread_current()->pml4, p->va))
	{
		file_seek(file, p->offset);
		file_write(file, p->frame->kva, PGSIZE);
		pml4_set_dirty(thread_current()->pml4, p->va, false);
	}

	pml4_clear_page(thread_current()->pml4, p->va);
	palloc_free_page(p->frame->kva);
	free(p->frame);
	p->frame = NULL;
	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	// do_munmap(page->va);
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *p = page;

	if (p == NULL)
		return;
	
	struct file *file = p->map_file;
	if (file == NULL)
		return;

	int temp;
	if(pml4_is_dirty (thread_current()->pml4, p->va))
	{
		file_seek(file, p->offset);
		temp = file_write(file, p->frame->kva, PGSIZE);
		pml4_set_dirty(thread_current()->pml4, p->va, false);
	}
	p->va = NULL;
	p->map_file = NULL;
	hash_delete(&spt->pages, &p->hash_elem);
	pml4_clear_page(thread_current()->pml4, p->va);
	// free(file);
	file_close(file);
	// printf("\n----------------------------------\n");
	// printf("\n%s\n", (char*)p->frame->kva);
	// printf("\n----------------------------------\n");
}
static bool
lazy_load_segment_file (struct page *page, void *aux) {
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
	struct load_info *info = (struct load_info *)aux;
	
	struct file *file = info->file;
	off_t ofs = info->ofs;
	page->offset = ofs;

	uint8_t *upage = info->upage;
	uint32_t read_bytes = info->read_bytes; 
	uint32_t zero_bytes =info->zero_bytes; 
	bool writable = info->writable;
	
	file_seek(file, ofs);
	uint8_t *kpage = (uint8_t *)page->frame->kva;

	/* Do calculate how to fill this page.
	* We will read PAGE_READ_BYTES bytes from FILE
	* and zero the final PAGE_ZERO_BYTES bytes. */
	size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
	size_t page_zero_bytes = PGSIZE - page_read_bytes;

	/* Get a page of memory. */
	if (kpage == NULL)
	{
		free(info);
		return false;
	}

	/* Load this page. */
	if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
		palloc_free_page (kpage);
		free(info);
		return false;
	}
	memset (kpage + page_read_bytes, 0, page_zero_bytes);
	free(info);

	struct thread *t = thread_current ();
	pml4_set_page (t->pml4, upage, kpage, writable);
	return true;
}
/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {

	uint32_t read_bytes = file_length(file) < length? file_length(file): length;
	uint32_t zero_bytes = (read_bytes%PGSIZE)== 0 ? 0 : PGSIZE - (read_bytes%PGSIZE);
	off_t ofs = offset;
	uint8_t *upage = addr;

	// ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	// ASSERT (pg_ofs (upage) == 0);
	// ASSERT (ofs % PGSIZE == 0);
	struct supplemental_page_table *spt = &thread_current ()->spt;
	
	while (read_bytes > 0 || zero_bytes > 0) {
		struct page *p = spt_find_page(spt, upage);
		if (p != NULL)
			return NULL;
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		void *aux = NULL;

		struct load_info *info = (struct load_info *)calloc(1, sizeof(struct load_info));
		info->file = file_reopen(file);
		info->ofs = ofs;
		info->upage = upage;
		info->read_bytes = page_read_bytes;
		info->zero_bytes = page_zero_bytes;
		info->writable = writable;
		aux = info;
		if (!vm_alloc_page_with_initializer (VM_FILE, upage,
					writable, lazy_load_segment_file, aux))
		{
			free(info);
			return NULL;
		}
		struct page *page_added = spt_find_page(spt, upage);
		page_added->map_file = info->file;
		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
		ofs += page_read_bytes;
	}
	return addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *p = spt_find_page(spt, addr);
	if (p == NULL)
		return;
	
	struct file *file = p->map_file;
	struct inode *inode = file_get_inode(file);
	while (1)	
	{
		// printf("idx = %d  \n", iidx++);
		file = p->map_file;
		if (file == NULL)
			return;

		if (file_get_inode(file) != inode)
			return;

		if(pml4_is_dirty (thread_current()->pml4, p->va))
		{
			file_seek(file, p->offset);
			file_write(file, p->frame->kva, PGSIZE);
			pml4_set_dirty(thread_current()->pml4, p->va, false);
		}
		hash_delete(&spt->pages, &p->hash_elem);
		pml4_clear_page(thread_current()->pml4, p->va);
		p->va = NULL;
		p->map_file = NULL;
		free(file);
		addr += PGSIZE;
		p = spt_find_page(spt, addr);
		if (p == NULL)
			return;
	}
	// printf("reached at end of do_mumap\n");
}
