		     +--------------------------+
       	     |		CSE 521             |
		     | PROJECT 2: USER PROGRAMS	|
		     | 	   DESIGN DOCUMENT     	|
		     +--------------------------+

---- GROUP ----

>> Fill in the names and email addresses of your group members.

Zhe Zhang <zzhang64@buffalo.edu>
Jie Lyu <jielv@buffalo.edu>
Sen Pan <senpan@buffalo.edu>

---- PRELIMINARIES ----

>> If you have any preliminary comments on your submission, notes for the
>> TAs, or extra credit, please give them here.

>> Please cite any offline or online sources you consulted while
>> preparing your submission, other than the Pintos documentation, course
>> text, lecture notes, and course staff.

			   ARGUMENT PASSING
			   ================

---- DATA STRUCTURES ----

>> A1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

```
/* recursive push char* to stack */
void _initial_stack(char **save_ptr, void **esp);
```

---- ALGORITHMS ----

>> A2: Briefly describe how you implemented argument parsing.  How do
>> you arrange for the elements of argv[] to be in the right order?
>> How do you avoid overflowing the stack page?

**A2.1** 
We implement argument parsing by modifying ```setup_stack```. In the function, firstly if successfully install a memory page, set ```esp``` to ```PHY_BASE```. Secondly, parse the cmd line and push arguments onto stack as described in A2.2. In third step, we test if padding is needed. Afterward, set pointers argv[i] pointing to the head of arguments parsed in the second step, including a dummy pointer. Finally, set up argc and set esp to return position, which is &argc - 4.

**A2.2** 
Use a recursive call ```_initial_stack``` to parse the cmd first, and then push argv[] into the arguments in reverse order. To be specific, in ```_initial_stack``` , the function will parse the cmd first, then recursively call ```_initial_stack```, in the end, the function will push argv[] onto stack.

After push all the arguments, scan the arguments from the `PHY_BASE-1` to user space, whenever we meet a '\0' whose position is ```i```, the push a pointer which is equivalent to ```i+1```.

**A2.3**  
We check the value of ```*esp``` in the end of ```setup_stack``` function, if it goes beyond PHY_BASE - PAGESIZE, then return false.

---- RATIONALE ----

>> A3: Why does Pintos implement strtok_r() but not strtok()?

**A3** 
1) the return value ```strtok``` is equal to the ```save_ptr``` in ```strtok_r```, but ```strtok_r``` return the head of current string, which is more flexible and easy to handle.

2) strtok uses a global varible, which is not thread safe.

>> A4: In Pintos, the kernel separates commands into a executable name
>> and arguments.  In Unix-like systems, the shell does this
>> separation.  Identify at least two advantages of the Unix approach.

**A4**
1) Easy implementation. String regex is much easier to be implemented using shell, prepossessing shell to check if there are any invalid expressions or characters.
2) Shell is in user space, if there are some exceptions in shell, it won't affect the user space.


			     SYSTEM CALLS
			     ============

---- DATA STRUCTURES ----

>> B1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

userprog/syscall.c
```
/* file descriptor, map fd to struct file* */
typedef struct _file_descritor{
  int fd;
  struct file* handle;
  struct list_elem node;
}file_descritor;

/* copy of lib/user/syscall.h just for convenience */
typedef int pid_t;
typedef int mapid_t;
```

threads/thread.h
```
struct thread {
    ...

    /* parent process */
    struct thread* parent; /* a children only has one parent */
    struct list children_list; /* parent may spawn multiple children */

    /* variable in thread->children_list */
    struct list_elem child_elem;
    /* deal with duplicated process_wait() */
    struct semaphore process_wait; // parent sema_down child. parent waits child's sema, child signals
    struct semaphore exec_sync; // parent sema_down parent. parent waits it's own sema, child signals
    struct list file_descriptors; /* store all the files opened by current prog, release them all when exit */
    ++ struct list exited_children; /* store all the status of forked children, get the status when exit */

    bool exit; // in process_execute() the child process started successfully? pass from child -> parent. Useful in process execute sync.
    int exit_status;// after child exit pass the status. child -> parent
    int internal_fd; // internal count of fd, increase by one after open a new file
    ++ struct list prog_file; /* to store the file currently opened by the user program, close when exit */
}

/* use a list to store all the child process and its exit status */
struct exitS{
    tid_t child;
    int exit_status;
    struct list_elem elem;
};
```
update: 
in struct thread
```
deleted:
-- bool waited;
-- tid_t child;

added:
++ struct file* prog_file;
++ struct list exited_children;
```

>> B2: Describe how file descriptors are associated with open files.
>> Are file descriptors unique within the entire OS or just within a
>> single process?

**B2.1**
We use ```file_descritor``` to store the fd and ```struct file```, and store the structure in link list in ```struct thread::file_descriptors```.
If we want to find fd, we first scan the ```file_descriptors``` and if the ```fd``` in ```file_descriptors``` is the same as fd we query, then we find the answer. If we reach the end, then there are not matched open files.

Originally, we think a hash map would be a better choice to implement the mapping operation, but we failed to use hash map due to it takes too much memory space, which will corrupt the thread structure.
Besides, due to limited memory space in pintos, the hash map actually only uses 4 buckets, and a link list in each bucket, which does not improve runtime much. So, using a link list in thread would be fine.

**B2.2**
In our implementation, fd is unique only within a single process.

---- ALGORITHMS ----

>> B3: Describe your code for reading and writing user data from the
>> kernel.

**B3.1 reading**
    1) Check the validation of buffer pointer.
    2) If fd is ```STDOUT_FILENO```, then return -1; if fd is ```STDIN_FILENO```, then use std in;
    3) Fetch current thread, and scan ```file_descriptors``` in thread structure. If find matched fd, then return the ```struct list_elem node```, else return NULL.
    4) If node is NULL, return -1.
    5) If node not NULL, call ```file_read``` and return.

**B3.2 writing**
    1) Check the validation of buffer pointer.
    2) If fd is ```STDOUT_FILENO```, then use std out; if fd is ```STDIN_FILENO```, then return -1;
    3) Fetch current thread, and scan ```file_descriptors``` in thread structure. If find matched fd, then return the ```struct list_elem node```, else return NULL.
    4) If node is NULL, return -1.
    5) If node not NULL, call ```file_write``` and return.

>> B4: Suppose a system call causes a full page (4,096 bytes) of data
>> to be copied from user space into the kernel.  What is the least
>> and the greatest possible number of inspections of the page table
>> (e.g. calls to pagedir_get_page()) that might result?  What about
>> for a system call that only copies 2 bytes of data?  Is there room
>> for improvement in these numbers, and how much?

**B4**
The least number of calling pagedir_get_page is 1 if we alloc a new page to save the data.
The most number of calling pagedir_get_page is 2, if the current page is partially used.
We can alloc a new page to save each full-page data, but this will leave some holes in memory.

>> B5: Briefly describe your implementation of the "wait" system call
>> and how it interacts with process termination.

**B5**
1) Save each child thread in a link list in the parent thread.
2) When calling child process, find if the child process with the very pid exists. If not, there may be two possibilities.
    2.1 The child process already exited, before calling wait.
    2.2 The pid does not exists at all.
    To deal with the different cases, if a child process exit normally, it will modify ```tid_t child``` and ```int exit_status``` in the parent process. Then the parent process will check if ```current->child``` and ```tid_t child_tid``` matches to distinguishes two different cases.
3) Check if parent process is already waiting for the child process.(This might not be needed, since wait is a blocked call, we will remove this part later)
4) ```sema_try_down``` or ```sema_down``` ```child->process_wait```. We use ```sema_try_down``` just in case the child process has already exited and called sema_up before, so check this before calling ```sema_down```.
5) If a child process exits, it will sema_up ```current->process_wait```, to wake up parent process after setting parent's ```exit_status```.
6) return current->exit_status.

>> B6: Any access to user program memory at a user-specified address
>> can fail due to a bad pointer value.  Such accesses must cause the
>> process to be terminated.  System calls are fraught with such
>> accesses, e.g. a "write" system call requires reading the system
>> call number from the user stack, then each of the call's three
>> arguments, then an arbitrary amount of user memory, and any of
>> these can fail at any point.  This poses a design and
>> error-handling problem: how do you best avoid obscuring the primary
>> function of code in a morass of error-handling?  Furthermore, when
>> an error is detected, how do you ensure that all temporarily
>> allocated resources (locks, buffers, etc.) are freed?  In a few
>> paragraphs, describe the strategy or strategies you adopted for
>> managing these issues.  Give an example.

**B6.1**
1) We check the validation of the pointers passed from user space each time kernel receives a pointers from ```esp``` in the syscall_handler, besides we will also check the validation of a ```char *``` pointer.
If the pointer is in kernel space (i.e. >= PHY_BASE) or the pointer is NULL, or the pointer comes from unreferenced page, then call ```exit(-1)```.

2) (updated) Our program currently does not acquired lock when implementing in syscall.c and process.c, so no need to release any locks when exits. But we do call malloc, when calling ```exit``` and ```open```, in this cases, we use a list to store all opened files and exited children. The program will release all these resoureses when calling ```_release_all()``` or ```_close_all()```.

3) To release opened files when the program exits unexpectedly, each time when the program opens a file, save the ```file_descriptor``` in the link list in thread structure.    When close the file, search corresponding fd and remove this ```file_descriptor```.
When call ```exit```, release all the file descriptor in ```struct thread::file_descriptors```, which is the ```void _close_all(struct thread* cur)``` function in ```syscall.c```.

---- SYNCHRONIZATION ----

>> B7: The "exec" system call returns -1 if loading the new executable
>> fails, so it cannot return before the new executable has completed
>> loading.  How does your code ensure this?  How is the load
>> success/failure status passed back to the thread that calls "exec"?

**B7**
1) In ```process_execute```, after executing ```thread_create```, sema_down parent process.
    ```
    tid = thread_create (fn_copy, PRI_DEFAULT, start_process, fn_copy);
    // wait until we know whether child process has successfully started
    sema_down(&cur->exec_sync);
    ```
2) In child process ```start_process```, after calling ```load```, and change the ```parent->exit``` variable in parent, then call ```sema_up(&parent->exec_sync)``` to wake up parent process.

3) Parent process is able to know whether the child process has started successfully by examining ```cur->exit``` variable, since ```exec``` is a synchronized call, it is safe to do this.


>> B8: Consider parent process P with child process C.  How do you
>> ensure proper synchronization and avoid race conditions when P
>> calls wait(C) before C exits?  After C exits?  How do you ensure
>> that all resources are freed in each case?  How about when P
>> terminates without waiting, before C exits?  After C exits?  Are
>> there any special cases?

**B8.1 avoid race conditions and release resources**
    Already explained in **B5** and **B6**.

**B8.2 P terminates without waiting**
    1. Since if a parent process must wait for its child process to get start.
    (updated)
    If it is killed, (++) we use a list to store all those threads which call exit, and the parent thread traverse the list to find a matched thread. If no child is matched, return -1.
    2. P terminates without waiting after C exits.
    This is the normal case, we do not need to take extra effort handle this case.
    3. P terminates without waiting before C exits. ( currented not implemented in the code.)
    In this case, the children list should be freed. 
    The children don need to set their parent to NULL, we can pass some extra information to let the children to know their parent's status. Children continues to execute.

---- RATIONALE ----

>> B9: Why did you choose to implement access to user memory from the
>> kernel in the way that you did?

Because pintos manual says it's an easier way to handle invalid pointers. And it turns out to be straightforward and easy to implement.

>> B10: What advantages or disadvantages can you see to your design
>> for file descriptors?

Advantages:
1) thread safty, including fd counter and link list.
2) thread is aware of all of it's file descriptor.

Disadvantages:
1) link list is slow, each time closing a file, it will take O(n) times to release corresponding file descriptor.


>> B11: The default tid_t to pid_t mapping is the identity mapping.
>> If you changed it, what advantages are there to your approach?

We didn't change it. We think it is reasonable to remain the relation since it's quite easy to implement and enough to handle the test case. Since all the usrprog in test cases is non-multithreading based.


			   SURVEY QUESTIONS
			   ================

Answering these questions is optional, but it will help us improve the
course in future quarters.  Feel free to tell us anything you
want--these questions are just to spur your thoughts.  You may also
choose to respond anonymously in the course evaluations at the end of
the quarter.

>> In your opinion, was this assignment, or any one of the three problems
>> in it, too easy or too hard?  Did it take too long or too little time?

>> Did you find that working on a particular part of the assignment gave
>> you greater insight into some aspect of OS design?

>> Is there some particular fact or hint we should give students in
>> future quarters to help them solve the problems?  Conversely, did you
>> find any of our guidance to be misleading?

>> Do you have any suggestions for the TAs to more effectively assist
>> students, either for future quarters or the remaining projects?

>> Any other comments?
