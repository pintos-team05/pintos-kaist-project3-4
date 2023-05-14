// #define VM
#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "filesys/off_t.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

void push_args(char **argv, int argc, struct intr_frame *if_);

struct aux {
	struct file *file; 
	off_t ofs;
	// uint8_t *page;
	uint32_t read_bytes; 
	uint32_t zero_bytes; 
	// bool writable;
};
#endif /* userprog/process.h */
