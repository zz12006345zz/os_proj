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
static int32_t _wait(pid_t pid);
static int32_t _write (int fd, const void *buffer, unsigned length);
static int32_t _read (int fd, void *buffer, unsigned length);
static bool _create (const char *file, unsigned initial_size);
static int _open (const char *file);
static void _close (int fd);
static int _filesize (int fd);
static pid_t _exec (const char *file);
static struct hash file_descriptors;
static int descriptor_CNT;
static struct lock descriptor_lock;

typedef struct _myhash {
  int key;
  struct hash_elem node;
} myhash_helper;

typedef struct _my_file_entry{
  int key;
  struct hash_elem node;
  struct file * file_descriptor;
}my_entry;

static unsigned my_hash (const struct hash_elem *e, void *aux UNUSED){
  return hash_entry(e, myhash_helper, node)->key;
} 

static bool my_hash_comp (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
  return hash_entry(a, myhash_helper, node)->key < hash_entry(b, myhash_helper, node)->key;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  hash_init(&file_descriptors, my_hash, my_hash_comp, NULL);
  descriptor_CNT = 2;
  lock_init(&descriptor_lock);
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
    /* code */
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
    /* code */
    break;
  }
  case SYS_TELL:
  {
    /* code */
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
  th->parent->exit_status = status;
  th->exit = true;
  list_remove(&th->child_elem);
  printf("%s: exit(%d)\n", th->name, status);
  sema_up(&th->process_wait);

  thread_exit ();
}

int32_t _write (int fd, const void *buffer, unsigned length){
  if(!user_ptr_valid(buffer)){
    _exit(-1);
  }
  if(length > PGSIZE){
    length = PGSIZE;
  }
  
  if(fd == STDIN_FILENO){
    return -1;
  }else if(fd == STDOUT_FILENO){
    putbuf(buffer, length);
    return length;
  }

  myhash_helper helper;
  helper.key = fd;
  struct hash_elem* file_node = hash_find(&file_descriptors,&helper.node);
  if(file_node == NULL){// file not found
    return -1;
  }
  struct file* f = hash_entry(file_node, my_entry, node)->file_descriptor;
  return file_write(f, buffer, length);
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


  myhash_helper helper;
  helper.key = fd;
  struct hash_elem* file_node = hash_find(&file_descriptors,&helper.node);
  if(file_node == NULL){// file not found
    return -1;
  }
  struct file* f = hash_entry(file_node, my_entry, node)->file_descriptor;
  return file_read(f, buffer, length);
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
  my_entry *file_entry = malloc(sizeof(my_entry));
  file_entry->file_descriptor = f_internal;

  lock_acquire(&descriptor_lock);
  file_entry->key = descriptor_CNT++;
  hash_insert(&file_descriptors, &file_entry->node);
  lock_release(&descriptor_lock);

  return file_entry->key;
}

void _close (int fd){
  // printf("close %d\n",fd);
  myhash_helper helper;
  helper.key = fd;
  struct hash_elem* file_entry = hash_delete(&file_descriptors,&helper.node);
  if(file_entry){
    // printf("delete!\n");
    my_entry *f = hash_entry(file_entry, my_entry, node);
    file_close(f->file_descriptor);
    free(f);
  }
}

int _filesize (int fd){
  myhash_helper helper;
  helper.key = fd;
  struct hash_elem* file_node = hash_find(&file_descriptors,&helper.node);
  if(file_node == NULL){// file not found
    return -1;
  }
  return file_length(hash_entry(file_node, my_entry, node)->file_descriptor);
}

pid_t _exec (const char *file){
  if(strcmp(file, "") == 0){
    return -1;
  }
  // printf("exec: %s\n",file);
  
  // printf("exec return %d\n",ret);
  struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;

  int i = 0;
  while(file[i] != ' ' && file[i] != '\0'){
    i++;
  }

  char *real_name = malloc(sizeof(char)* (i + 2));
  strlcpy(real_name, file, i+1);
  // printf("name :%s\n",real_name);

  if (dir != NULL)
    dir_lookup (dir, real_name, &inode);
  dir_close (dir);

  if(inode == NULL){
    return -1;
  }
  pid_t ret = process_execute(file);
  free(real_name);
  return ret;
}

int32_t _wait(pid_t pid){
  int32_t ret = process_wait(pid);
  // printf("wait ret: %d \n", ret);
  return ret;
}