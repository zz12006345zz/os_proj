#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <debug.h>
// #include "lib/user/syscall.h"
#include "pagedir.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "process.h"
#include "syscall.h"
#include <string.h>
#include "lib/kernel/hash.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "lib/kernel/hash.h"
#include "threads/synch.h"
#include "threads/malloc.h"

typedef int mapid_t;
typedef int pid_t;

static void syscall_handler (struct intr_frame *);
static bool user_ptr_valid(const void *ptr);
static void _halt(void) NO_RETURN;
/* system call function declaration*/
static int32_t _wait(pid_t pid);
static int32_t _write (int fd, const void *buffer, unsigned length);
static int32_t _read (int fd, void *buffer, unsigned length);
static bool _create (const char *file, unsigned initial_size);
static int _open (const char *file);
static void _close (int fd);
static int _filesize (int fd);
static pid_t _exec (const char *file);
static void _seek (int fd, unsigned position);
static unsigned _tell (int fd);
static bool _remove (const char *file);

/* static function */
void _release_all(struct thread* cur);
/* search fd is holded by current file, note fd must be local */
static struct list_elem* search(struct thread*cur, int fd);

typedef struct _file_descritor{
  int fd;
  struct file* handle;
  struct list_elem node;
}file_descritor;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t *stack_pointer = f->esp;

  if(!user_ptr_valid(f->esp)){
    _exit(-1);
  }
  switch (*(int*)(f->esp))
  {
  case SYS_HALT:
  {
    _halt();
    break;
  }
  case SYS_EXIT:
  {
    if(!user_ptr_valid(stack_pointer + 1)){
      _exit(-1);
    }
    _exit(*(int*)(stack_pointer + 1));
    break;
  }
  case SYS_EXEC:
  {
    user_ptr_valid(stack_pointer + 1);
    f->eax = _exec((char *)*(stack_pointer+1));
    break;
  }
  case SYS_WAIT:
  {
    user_ptr_valid(stack_pointer + 1);
    f->eax = _wait(*(int*)(stack_pointer + 1));
    break;
  }
  case SYS_CREATE:
  {
    /* code */
    user_ptr_valid(stack_pointer + 2);
    f->eax =  _create ((char *)*(stack_pointer+1), *(unsigned*)(stack_pointer+2));
    break;
  }
  case SYS_REMOVE:
  {
    user_ptr_valid(stack_pointer + 1);
    f->eax = _remove((char *)*(stack_pointer+1));
    break;
  }
  case SYS_OPEN:
  {
    user_ptr_valid(stack_pointer + 1);
    f->eax = _open ((char *)*(stack_pointer+1));
    break;
  }
  case SYS_FILESIZE:
  {
    user_ptr_valid(stack_pointer + 1);
    f->eax = _filesize (*(int *)(stack_pointer+1));
    break;
  }
  case SYS_READ:
  {
    user_ptr_valid(stack_pointer + 3);
    f->eax = _read (*(int*)(stack_pointer + 1), (void *) *(stack_pointer + 2), *(unsigned *)(stack_pointer + 3));
    break;
  }
  case SYS_WRITE:
  {
    user_ptr_valid(stack_pointer + 3);
    f->eax = _write (*(int*)(stack_pointer + 1), (void *) *(stack_pointer + 2), *(unsigned *)(stack_pointer + 3));
    break;
  }
  case SYS_SEEK:
  {
    user_ptr_valid(stack_pointer + 2);
    _seek(*(int*)(stack_pointer + 1),*(unsigned *)(stack_pointer + 2));
    break;
  }
  case SYS_TELL:
  {
    user_ptr_valid(stack_pointer + 1);
    f->eax = _tell(*(int*)(stack_pointer + 1));
    break;
  }
  case SYS_CLOSE:
  {
    user_ptr_valid(stack_pointer + 1);
    _close (*(int*)(stack_pointer + 1));
    break;
  }
  
  default:
    NOT_REACHED ();
    break;
  }
}

bool user_ptr_valid(const void *ptr){
  if(ptr >= PHYS_BASE || ptr == NULL){
    return false;
  }
  struct thread* th = thread_current();

  return pagedir_get_page(th->pagedir, ptr) != NULL;
}

void _halt(){
  shutdown_power_off();
}

void _exit(int status){
  struct thread* th = thread_current();
  struct exitS* ex = malloc(sizeof(struct exitS));
  ex->child = th->tid;
  ex->exit_status = status;
  list_push_back(&th->parent->exited_children, &ex->elem);
  list_remove(&th->child_elem);

  printf("%s: exit(%d)\n", th->name, status);
  file_close(th->prog_file);
  _release_all(th);
  // sema_up(&th->process_wait);

  thread_exit ();
}

int32_t _write (int fd, const void *buffer, unsigned length){
  if(!user_ptr_valid(buffer)){
    _exit(-1);
  }
  
  if(fd == STDIN_FILENO){
    return -1;
  }else if(fd == STDOUT_FILENO){
    putbuf(buffer, length);
    return length;
  }

  struct thread* cur = thread_current();
  struct list_elem* file_entry = search(cur, fd);
  if(file_entry == NULL){// file not found
    return -1;
  }

  return file_write(list_entry(file_entry, file_descritor, node)->handle , buffer, length);
}

int32_t _read (int fd, void *buffer, unsigned length){
  if(!user_ptr_valid(buffer)){
    _exit(-1);
  }
  if (fd == STDIN_FILENO)
  {
    unsigned i;
    uint8_t* local_buffer = (uint8_t *) buffer;
    for (i = 0; i < length; i++)
    {
      local_buffer[i] = input_getc();
    }
    return length;
  }else if(fd == STDOUT_FILENO){
    return -1;
  }

  struct thread* cur = thread_current();
  struct list_elem* file_entry = search(cur, fd);
  if(file_entry == NULL){// file not found
    return -1;
  }

  return file_read(list_entry(file_entry, file_descritor, node)->handle, buffer, length);
}

bool _create (const char *file, unsigned initial_size){
  if(!user_ptr_valid(file)){
    _exit(-1);
  }

  if(strcmp(file, "") == 0){
    return false;
  }

  return filesys_create(file,initial_size);
}

int _open (const char *file){
  if(!user_ptr_valid(file)){
    _exit(-1);
  }
  if(strcmp(file, "") == 0){
    return -1;
  }
  
  struct file* f_internal = filesys_open(file);
  if(f_internal == NULL){
    return -1;
  }

  file_descritor *file_entry = malloc(sizeof(file_descritor));
  struct thread* cur = thread_current();
  
  file_entry->handle = f_internal;
  file_entry->fd = cur->internal_fd++;
  list_push_back(&cur->file_descriptors, &file_entry->node);

  return file_entry->fd;
}

void _close (int fd){
  struct thread* cur = thread_current();
  struct list_elem* file_entry = search(cur, fd);
  if(file_entry){
    list_remove(file_entry);
    file_descritor *f = list_entry(file_entry, file_descritor, node);
    file_close(f->handle);
    free(f);
  }
}

void _close_all(struct thread* cur){
  struct list_elem* it = list_begin(&cur->file_descriptors);
  file_descritor* file_entry;
  while(!list_empty(&cur->file_descriptors)){
    file_entry = list_entry(it, file_descritor, node); 
    it = list_remove(it);
    file_close(file_entry->handle);
    free(file_entry);
  }
}

void _release_all(struct thread* cur){
  struct list_elem* it = list_begin(&cur->exited_children);
  struct exitS* e_entry = NULL;
  while(!list_empty(&cur->exited_children)){
    e_entry = list_entry(it, struct exitS, elem); 
    it = list_remove(it);
    free(e_entry);
  }
  // it = list_begin(&cur->acquired_locks);
  // struct acquired_lock* l = NULL;
  // while(!list_empty(&cur->acquired_locks)){
  //   l = list_entry(it, struct acquired_lock, elem); 
  //   it = list_remove(it);
  //   free(l);
  // }
}

int _filesize (int fd){
  struct thread* cur = thread_current();
  struct list_elem* file_entry = search(cur, fd);
  if(file_entry == NULL){// file not found
    return -1;
  }

  return file_length(list_entry(file_entry, file_descritor, node)->handle);
}

pid_t _exec (const char *file){
  if(strcmp(file, "") == 0){
    return -1;
  }

  pid_t ret = process_execute(file);
  return ret;
}

int32_t _wait(pid_t pid){
  int32_t ret = process_wait(pid);

  return ret;
}

struct list_elem* search(struct thread*cur, int fd){
  for(struct list_elem* it = list_begin(&cur->file_descriptors); it!= list_end(&cur->file_descriptors); it = list_next(it)){
    if(list_entry(it, file_descritor, node)->fd == fd){
      return it;
    }
  }
  return NULL;
}

void _seek (int fd, unsigned position){
  struct list_elem* file_entry = search(thread_current(), fd);
  if(file_entry == NULL){// file not found
    return;
  }
  file_seek(list_entry(file_entry, file_descritor, node)->handle, position);
}

unsigned _tell (int fd){
  struct list_elem* file_entry = search(thread_current(), fd);
  if(file_entry == NULL){// file not found
    return 0;
  }
  return file_tell(list_entry(file_entry, file_descritor, node)->handle);
}

bool _remove (const char *file){
  return filesys_remove(file);
}