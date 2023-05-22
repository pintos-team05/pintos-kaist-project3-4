#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
// add header
#include "../include/devices/disk.h"
// add header
struct page;
enum vm_type;
static struct disk *swap_disk;
struct anon_page {
	disk_sector_t capacity;
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
