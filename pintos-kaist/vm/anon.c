/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "lib/kernel/bitmap.h"

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
	disk_sector_t size = disk_size(swap_disk);
	swap_table = bitmap_create(size);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	// printf("page sector num in swap in= %d\n", page->start_sector_num);
	for(int i = 0; i< 8; i++)
	{
		disk_read(swap_disk, page->start_sector_num + i, kva + (i*DISK_SECTOR_SIZE));
	}
	// printf("disk In sector num: %d\n", page->start_sector_num);
	bitmap_scan_and_flip(swap_table, page->start_sector_num, 8, true);
	page->start_sector_num = 0;
	pml4_set_page(thread_current()->pml4, page->va, kva, page->writable);

	if(lock_held_by_current_thread(&swap_lock))
		lock_release(&swap_lock);
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	int start_slot_num = bitmap_scan_and_flip(swap_table, 0, 8, false);
	// printf("page sector num in swap out= %d\n", start_slot_num);
	if(start_slot_num == BITMAP_ERROR)
		PANIC("Swap slop is full!\n");
	page->start_sector_num = start_slot_num;
	// printf("disk Out sector num: %d\n", page->start_sector_num);
	for(int i = 0; i < 8; i++)
	{
		disk_write(swap_disk, start_slot_num + i, (void*)page->frame->kva + (i*DISK_SECTOR_SIZE));
	}
	
	pml4_clear_page(thread_current()->pml4, page->va);
	palloc_free_page(page->frame->kva);
	free(page->frame);
	page->frame = NULL;
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	// palloc_free_page(page);
	// free(&page->anon);
	memset(anon_page, 0, sizeof(struct anon_page));
}
