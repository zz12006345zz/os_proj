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

static struct list_elem* search(struct thread*cur, int fd);

// static struct hash file_descriptors;
// static int descriptor_CNT;
// static struct lock descriptor_lock;

// typedef struct _myhash {
//   int key;
//   struct hash_elem node;
// } myhash_helper;

// typedef struct _my_file_entry{
//   int key;
//   struct hash_elem node;
//   tid_t tid;
//   struct file * file_descriptor;
// }my_entry;

// static unsigned my_hash (const struct hash_elem *e, void *aux UNUSED){
//   return hash_entry(e, myhash_helper, node)->key;
// } 

// static bool my_hash_comp (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
//   return hash_entry(a, myhash_helper, node)->key < hash_entry(b, myhash_helper, node)->key;
// }
typedef struct _file_descritor{
  int fd;
  struct file* handle;
  struct list_elem node;
}file_descritor;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  // hash_init(&file_descriptors, my_hash, my_hash_comp, NULL);
  // descriptor_CNT = 2;
  // lock_init(&descriptor_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t *stack_pointer = f->esp;
  // printf ("system call!\n");
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
  // th->parent->exit_status = status;
  // th->parent->child = th->tid;
  // th->exit = true;
  struct exitS* ex = malloc(sizeof(struct exitS));
  ex->child = th->tid;
  ex->exit_status = status;
  list_push_back(&th->parent->exited_children, &ex->elem);
  list_remove(&th->child_elem);

  printf("%s: exit(%d)\n", th->name, status);
  file_close(th->prog_file);
  sema_up(&th->process_wait);

  thread_exit ();
}

int32_t _write (int fd, const void *buffer, unsigned length){
  // printf("write %zu\n", length);
  if(!user_ptr_valid(buffer)){
    _exit(-1);
  }
  // if(length > PGSIZE){
  //   length = PGSIZE;
  // }
  
  if(fd == STDIN_FILENO){
    return -1;
  }else if(fd == STDOUT_FILENO){
    putbuf(buffer, length);
    return length;
  }

  // myhash_helper helper;
  // helper.key = fd;
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

  // myhash_helper helper;
  // helper.key = fd;
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
  // printf("%s, %zu\n",file,initial_size);
  return filesys_create(file,initial_size);
}

int _open (const char *file){
  // printf("%s\n",file);
  if(!user_ptr_valid(file)){
    _exit(-1);
  }
  if(strcmp(file, "") == 0){
    return -1;
  }
  // printf("1\n");
  
  struct file* f_internal = filesys_open(file);
  if(f_internal == NULL){
    return -1;
  }
  // printf("2\n");
  // printf("%d\n", sizeof(file_descritor));
  file_descritor *file_entry = malloc(sizeof(file_descritor));
  struct thread* cur = thread_current();
  
  file_entry->handle = f_internal;
  file_entry->fd = cur->internal_fd++;
  list_push_back(&cur->file_descriptors, &file_entry->node);
  // file_entry->tid = thread_current()->tid;
  // lock_acquire(&descriptor_lock);
  // file_entry->key = descriptor_CNT++;
  // hash_insert(&file_descriptors, &file_entry->node);
  // lock_release(&descriptor_lock);

  return file_entry->fd;
}

void _close (int fd){
  // printf("close %d\n",fd);
  // myhash_helper helper;
  // helper.key = fd;
  // struct hash_elem* file_entry = hash_delete(&file_descriptors,&helper.node);
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

int _filesize (int fd){
  // myhash_helper helper;
  // helper.key = fd;
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
  // printf("exec: %s\n",file);
  
  // printf("exec return %d\n",ret);
  

  // int i = 0;
  // while(file[i] != ' ' && file[i] != '\0'){
  //   i++;
  // }
  // struct dir *dir = dir_open_root ();
  // struct inode *inode = NULL;
  // char *real_name = malloc(sizeof(char)* (i + 2));
  // strlcpy(real_name, file, i+1);

  // if (dir != NULL)
  //   dir_lookup (dir, real_name, &inode);
  // dir_close (dir);

  // if(inode == NULL){
  //   return -1;
  // }
  // printf("start %s\n", real_name);
  pid_t ret = process_execute(file);
  // free(real_name);
  return ret;
}

int32_t _wait(pid_t pid){
  int32_t ret = process_wait(pid);
  // printf("wait ret: %d \n", ret);
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