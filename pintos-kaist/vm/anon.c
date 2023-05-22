/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
// project 3 add header
#include <string.h>
#include "../include/lib/kernel/bitmap.h"
#include "../include/threads/mmu.h"
// project 3 add header

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);
	swap_table = bitmap_create(disk_size(swap_disk));
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	for (int i = 0 ; i <8 ; i++) {
		disk_read(swap_disk, page->start_swap_no+i, kva+(DISK_SECTOR_SIZE*i));
	}
	bitmap_scan_and_flip(swap_table, page->start_swap_no, 8, true);
	// printf("start_swap_num_in : %d\n", page->start_swap_no);
	page->start_swap_no = 0;
	// printf("1111\n");
	uint64_t *pml4 = thread_current()->pml4;
	pml4_set_page(pml4, page->va, page->frame->kva, page->writable);
	// printf("2222\n");
	// lock holder 확인하고 하기
	// if (!lock_held_by_current_thread(&swap_lock)) {
	// 	return false;
	// }
	// lock_release(&swap_lock);
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	size_t start_swap_num = bitmap_scan_and_flip(swap_table, 0, 8, false);
	// printf("start_swap_num_out : %d\n", start_swap_num);
	for (int i = 0; i < 8 ; i++) {
		disk_write(swap_disk, start_swap_num+i, page->frame->kva + (DISK_SECTOR_SIZE*i));
	}
	// printf("disk_write success\n");
	page->start_swap_no = start_swap_num;
	// 승훈 참고
	uint64_t pml4 = thread_current()->pml4;
	pml4_clear_page(pml4, page->va);
	// 승훈 참고
	palloc_free_page(page->frame->kva);
	free(page->frame);
	page->frame = NULL;
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
