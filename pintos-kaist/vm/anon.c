/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/mmu.h"
#include "bitmap.h"
#define BLOCK_SECTOR_SIZE 512
#define SECTORS_PER_PAGE 8
static struct bitmap *swap_table;
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
	/* data structure to manage free and used areas in the swap disk */
	/* swap area will be also managed at the granularity of PGSIZE */
	swap_disk = disk_get(1, 1); // assuming swap disk nmber is 1
	swap_table = bitmap_create(disk_size(swap_disk));
	if (swap_table == NULL){
		return;
	}
	bitmap_set_all(swap_table, false);

}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	/* TODO : add some informations for swapping */
	struct anon_page *anon_page = &page->anon;

	/* TODO : Initialize additional fields in the anon_page struct */

	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {

	/* TODO : implement swap in */
	/* read content from swap disk into memory */
	/* swap table should be updated. */

	struct anon_page *anon_page = &page->anon;
	uint64_t *pml4 = thread_current()->pml4;
	bitmap_scan_and_flip(swap_table, page->start_slot_num, 8, true);

	disk_sector_t sector = page->start_slot_num;
	
	void *dst = kva; 
	// lock_acquire(&thread_current()->disk_lock);
	for (int i=0; i <SECTORS_PER_PAGE; i++){
		disk_read(swap_disk, sector + i, dst);
		dst += BLOCK_SECTOR_SIZE;
	}
	// lock_release(&thread_current()->disk_lock);
	pml4_set_page(pml4, page->va, kva, page->writable);
	page->start_slot_num = 0;
	
	// printf("anon_swap in done!!!!!!\n");
	// if (lock_held_by_current_thread (&swap_lock)) 
	// lock_release(&thread_current()->swap_lock);

	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	/* TODO : implement swap out */
	/* implement this before implementing swap_in */
	/* copy data from memory to swap disk */
	/* 1. find available swap slot by searching the swap table */
	/* 2. copy the page into slot */
	/* 3. the location of data should be in page structure */
	/* If there is no slot available, it could occur kernel panic */

	struct anon_page *anon_page = &page->anon;
	disk_sector_t slot = bitmap_scan_and_flip(swap_table, 0, SECTORS_PER_PAGE, false);
	if (slot == BITMAP_ERROR){
		PANIC("No swap slots avilable");
	}
	void *src = page->frame->kva; 
	// lock_acquire(&thread_current()->disk_lock);
	for (int i=0; i < SECTORS_PER_PAGE; i++) {
		disk_write(swap_disk, slot+i, src);
		src += BLOCK_SECTOR_SIZE;
	}
	// lock_release(&thread_current()->disk_lock);
	page->start_slot_num = slot;

	uint64_t *pml4 = thread_current()->pml4;
	pml4_clear_page(pml4, page->va);

	palloc_free_page(page->frame->kva);
	free(page->frame);
	page->frame = NULL;
	// printf("free physical frame done!!! \n");

	return true;
	
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	// free(anon_page);
	// palloc_free_page(page);
}
