#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/synch.h"

#include "filesys/file.h"
#include "filesys/filesys.h"
// #include "user/syscall.h"

typedef int pid_t;
#define PID_ERROR ((pid_t) -1)

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

void halt();
pid_t fork(struct intr_frame *f);

int wait(pid_t pid);
int open(const char *file);
void close(int fd);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);

int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);

int filesize(int fd);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void exit(int status);

struct file *get_file(int fd);
int add_file(struct file *file);
void remove_file(int fd);


/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */


struct lock filesys_lock;

void
syscall_init (void) {
	/* global lock for file operations */

	lock_init(&filesys_lock);

	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}


/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// /* Arguments: %rdi %rsi %rdx %r10 %r8 %r9 */
	// // TODO: Your implementation goes here.
	switch(f->R.rax){
		case SYS_HALT:
		{
			halt();
	
		}
		case SYS_EXIT:
		{
			int status = f->R.rdi;
			exit(status);
			
		}
		case SYS_FORK:
		{
			f->R.rax = fork(f);
			break;
		 }
		case SYS_EXEC:
		{
			const char *file = f->R.rdi;
			f->R.rax = exec(file);
			break;
		}
		case SYS_WAIT:
		{
			pid_t pid = f->R.rdi;
			f->R.rax = wait(pid);
			break;
		}
		case SYS_CREATE:
		{
			const char *file = f->R.rdi;
			unsigned int initial_size = f->R.rsi;

			f->R.rax = create(file, initial_size);
			break;
		}
		case SYS_REMOVE:
		{
			const char *file = f->R.rdi;
			f->R.rax = remove(file);
			break;
		}
		case SYS_OPEN:
		{
			const char *file = f->R.rdi;
			int fd = open(file);

			f->R.rax = fd;
			break;
		}
		case SYS_FILESIZE:
		{
			int fd = f->R.rdi;
			int file_size = filesize(fd);
			f->R.rax = file_size;
			break;
		}
		case SYS_READ:
		{
			int fd = f->R.rdi;
			void *buffer = f->R.rsi;
			unsigned int size = f->R.rdx;
			
			f->R.rax = read(fd, buffer, size);
			break;
		}
		case SYS_WRITE:
		{
			int fd = f->R.rdi;
			void *buffer = f->R.rsi;
			unsigned int size = f->R.rdx;

			f->R.rax = write(fd, buffer, size);
			break;
		
		}
		case SYS_SEEK:
		{
			int fd = f->R.rdi;
			unsigned position = f->R.rsi;
			seek(fd, position);
			break;
			
		}
		case SYS_TELL:
		{
			int fd = f->R.rdi;
			unsigned result = tell(fd);

			f->R.rax = result;
			break;
			
		}
		case SYS_CLOSE:
		{	
			int fd = f->R.rdi;
			close(fd);
			break;
		}
		default:
		{
			thread_exit();
		}

	// 	// case SYS_DUP2:                   /* Duplicate the file descriptor */
	// 	// {
	// 	// 	dup2();
	// 	// }

	}
	// printf ("system call!\n");
	// thread_exit ();
}



int add_file(struct file *file) {
	struct thread *cur = thread_current();
	struct file **fdt = cur->fdt;


	while ((cur->next_fd < FDCOUNT_LIMIT) && fdt[cur->next_fd])
	{
		cur->next_fd++;
	}
	if (cur->next_fd >= FDCOUNT_LIMIT)
		return -1;

	fdt[cur->next_fd] = file;
	
	return cur->next_fd;
}

void remove_file(int fd) {
	struct thread *cur = thread_current();
	if (fd < 0 || fd >= FDCOUNT_LIMIT)
		return;

	cur->fdt[fd] = NULL;
}

void validate_address(void *addr) {
	struct thread *t = thread_current();
	
	if (is_kernel_vaddr(addr) || (pml4_get_page (t->pml4, addr) == NULL)){
		exit(-1);
	}
}


void halt(){
	power_off();
}


void exit(int status) {
	thread_current()->exit_status = status;
	printf("%s: exit(%d)\n", thread_name(), status);
	thread_exit();
}

int wait (pid_t pid) {

	// find the thread corresponding to the child process ID and wait for it

	return process_wait(pid);

	// to exit using the semaphore as described in step 2
	// When the child thread exits, release the semaphore using sema_up()
	// Return the child process ID from the wait() system call.
}


pid_t fork(struct intr_frame *f) {
	const char *thread_name= f->R.rdi;
	tid_t tid;
	tid = process_fork(thread_name, f);
	
	return tid;
	
}

int exec(const char* file) {
	validate_address(file);
	return process_exec(file);

}


/* Get file pointer for given file descriptor */
struct file *get_file(int fd) {
	struct file **fdt = thread_current()->fdt;
	if (fd < 0 || fd >= FDCOUNT_LIMIT)
	{
		return NULL;
	}
	return fdt[fd];
}

bool create(const char *file, unsigned initial_size) {
	bool success;
	if(file == NULL){
		exit(-1);
	}
	success = filesys_create(file, initial_size);
	return success;
}


bool remove(const char *file) {
	bool success;
	validate_address(file);
	if(file == NULL) {
		exit(-1);
	}
	success = filesys_remove(file);
	return success;
}


/* open file corresponds to path in "file" */
int open(const char *file) { 
	validate_address(file);
	/* Acquire global lock */
	lock_acquire(&filesys_lock);

	struct thread *t = thread_current();
	if (strcmp(file, "") == 0){
		lock_release(&filesys_lock);
		return -1;
	}
	struct file *file_ptr = filesys_open(file);

	if (file_ptr == NULL) {
		lock_release(&filesys_lock);
		return -1;
	}

	/* Add file to file descriptor table */
	int fd = add_file(file_ptr);
	
	if (fd == -1){
		file_close(file_ptr);
	}
	
	/* Release global lock */
	lock_release(&filesys_lock);

	return fd;
}

/* close file corresponds to file descriptor fd */
void close(int fd) {
	/* Acquire global lock */
	lock_acquire(&filesys_lock);
	struct file *file_ptr = thread_current()->fdt[fd];

	if (file_ptr == NULL){
		lock_release(&filesys_lock);
		return -1;
	}

	file_close(file_ptr);
	remove_file(fd);
	

	lock_release(&filesys_lock);
	return 0;
}

int read(int fd, void *buffer, unsigned size) {
	// Read size bytes from the file open as fd into buffer.
	// Return the number of bytes actually read (0 at end of file), or -1 if fails
	// if fd is 0, it reads from keyboard using input_getc(), 
	// otherwise reads from file using file_read() function

	validate_address(buffer);
	lock_acquire(&filesys_lock);
	off_t read_size = 0;
	char *read_buffer = (char *)buffer;

	/* STDIN */
	if (fd == 0) {
		while (read_size < size) {
			read_buffer[read_size] = input_getc();
			if (read_buffer[read_size] == '\n'){
				break;
			}
			read_size++;
		}
		read_buffer[read_size]='\0';
		lock_release(&filesys_lock);
		return read_size;
	}

	else{
		struct file *file_ptr = get_file(fd);
		if (file_ptr == NULL){
			lock_release(&filesys_lock);
			return -1;
		}
		read_size = file_read(file_ptr, read_buffer, size);
		lock_release(&filesys_lock);
		return read_size;
	}
}

int write(int fd, const void *buffer, unsigned size) {
	// Write size bytes from buffer to the open file fd.
	// Returns the number of bytes actually written.
	// If fd is 1, it write to the console using putbuf(), 
	// otherwise write to the file using file_write() function
	validate_address(buffer);
	lock_acquire(&filesys_lock);
	off_t written_size = 0;
	char *write_buffer = (char *)buffer;

	/* STDOUT */
	if (fd == 1) {
		putbuf(write_buffer, size);
		lock_release(&filesys_lock);
		return size;
	}
	else {
		struct file *file_ptr = get_file(fd);
		if (file_ptr == NULL){
			lock_release(&filesys_lock);
			return -1;
		}
		written_size = file_write(file_ptr, write_buffer, size);
		lock_release(&filesys_lock);
		return written_size;
	}
}

int filesize(int fd) {
	if (fd < 2 || fd > FDCOUNT_LIMIT){
		return -1;
	}
	struct file *file_ptr = get_file(fd);
	if (file_ptr == NULL){
		return -1;
	}
	return file_length(file_ptr);
}

/* Changes the next byte to be read or written in open file fd to position */
void seek(int fd, unsigned position) {
	if (fd < 2 || fd > FDCOUNT_LIMIT){
		return;
	}
	struct file *file_ptr = get_file(fd);
	
	if (file_ptr == NULL){
		return;
	}
	file_seek(file_ptr, position);
}

unsigned tell(int fd) {
	if (fd < 2 || fd > FDCOUNT_LIMIT){
		return;
	}
	struct file *file_ptr = get_file(fd);

	return file_tell(file_ptr);
}