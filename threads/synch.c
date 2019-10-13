/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#define HOLDER (lock->holder)

bool donate_compare(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED){
  return (list_entry(a, struct thread, elem)->priority + list_entry(a, struct thread, elem)->donated) 
            > (list_entry(b, struct thread, elem)->priority + list_entry(b, struct thread, elem)->donated);
}

bool lock_compare(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED){
  return list_entry(a, struct lock, lock_elem)->highest_priority > list_entry(b, struct lock, lock_elem)->highest_priority;
}
/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
    {
      // list_push_back (&sema->waiters, &thread_current ()->elem);
      list_insert_ordered(&sema->waiters, &thread_current ()->elem, donate_compare ,NULL);
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (!list_empty (&sema->waiters)) {
    list_sort(&sema->waiters, priority_comp, NULL);
    struct thread *th = list_entry (list_pop_front (&sema->waiters),
                                struct thread, elem);
    thread_unblock (th);
  }
    
  sema->value++;
  intr_set_level (old_level);
  /* when we wake up a new thread, check if the new thread has higher priority */
  try_preempt();
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);
  lock->donation = 0;
}

static int Donator_Forward(struct lock* lock, int donation){
  if(!HOLDER->donee){
    HOLDER->priority += donation;
    lock->donation += donation;
    return donation;
  }
  int extra_donate = HOLDER->priority + donation - HOLDER->donee->holder->priority;
  if(extra_donate > 0){
    Donator_Forward(HOLDER->donee, extra_donate);
    lock->donation += donation;
    HOLDER->priority += donation - extra_donate;
  }else{
    HOLDER->priority += donation;
    lock->donation += donation;
  }

  return donation;
}

//lock->donation = right - left
//delta lock->donation = (expected-current )
static void Donator_Backword(struct thread* th, int expected, int delta_left){
  if(list_empty(&th->donator_locks)){
    // printf("return %s, %d\n",th->name, expected);
    th->priority = expected;
  }else{
    struct lock* fisrt_lock = list_entry(list_front(&th->donator_locks), struct lock, lock_elem);
    struct thread *first_thread = list_entry(list_front(&fisrt_lock->semaphore.waiters), struct thread, elem);
    // printf("delta left %d,%d, return %d\n",delta_left - expected + th->priority,fisrt_lock->donation,delta_left);
    Donator_Backword(first_thread, th->priority, delta_left - expected + th->priority);
    fisrt_lock->donation += expected - th->priority - delta_left;
    th->priority = expected;
  }
  // printf("priority %s %d\n", th->name, th->priority);
}
/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

  struct thread* th = thread_current();
  
  if(lock->holder == NULL){
    sema_down (&lock->semaphore);
    lock->holder = thread_current ();
    return;
  }

  if(th->priority > HOLDER->priority){
    if(list_empty(&HOLDER->donator_locks)){
      // no other donator
      th->donee = lock;
      lock->highest_priority = th->priority;
      int donate = Donator_Forward(lock, th->priority - HOLDER->priority);
      // printf("donate %d\n",donate);
      // lock->donation = th->priority - HOLDER->priority;
      // HOLDER->priority = th->priority;
      // HOLDER->owned_lock++;
      list_push_back(&HOLDER->donator_locks, &lock->lock_elem);
      // printf("sb2");
      th->donated += donate;
      th->priority -= donate;
    }else{
      // current donation is from other lock or from current lock
      if(list_front(&HOLDER->donator_locks) == &lock->lock_elem){
        // donation is from the current lock , the first elem in semaphore.waiters
        // printf("sb2\n");
        struct thread *first_thread = list_entry(list_front(&lock->semaphore.waiters), struct thread, elem);
        int donation = th->priority - first_thread->priority; // first_thread->priority + lock->donation = HOLDER->priority( after donation )
        first_thread->donated -= lock->donation;
        first_thread->priority += lock->donation;
        first_thread->donee = NULL;
        lock->donation = donation;
        lock->highest_priority = th->priority;

        th->donee = lock;
        HOLDER->priority = th->priority;
        // check the holder should donate forward -- nested donate
        th->donated += lock->donation;
        th->priority -= lock->donation;
      }else{
        // donation is from another lock, hold and donate
        lock->highest_priority = th->priority;
        lock->donation = th->priority - HOLDER->priority + list_entry(list_front(&HOLDER->donator_locks), struct lock, lock_elem)->donation;
        list_insert_ordered(&HOLDER->donator_locks, &lock->lock_elem, lock_compare, NULL);

        HOLDER->priority = th->priority;
        th->donee = lock;

        th->priority -= lock->donation;
        th->donated += lock->donation;
      }
    }
  }else{
    // maybe we still needs donate
    if(list_empty(&HOLDER->donator_locks)){
      int to_donate = th->priority - HOLDER->priority + list_entry(list_front(&HOLDER->donator_locks), struct lock, lock_elem)->donation;
      if(to_donate > 0){
        if(lock->donation > 0){
          // once donated, but not in the contribute to the donation right now
          if(to_donate > lock->donation){
            // i want to donate more!
            // semaphore first elem -- do not donate
            struct thread *first_thread = list_entry(list_front(&lock->semaphore.waiters), struct thread, elem);
            first_thread->donated -= lock->donation;
            first_thread->priority += lock->donation;
            lock->donation = to_donate;
            // insert again
            list_remove(&lock->lock_elem);
            list_insert_ordered(&th->donator_locks, &lock->lock_elem, lock_compare, NULL);
            th->priority -= to_donate;
            th->donee = lock;
            th->donated += to_donate;
          }
        }else{
          // not donated yet
          lock->donation = to_donate;
          list_insert_ordered(&th->donator_locks, &lock->lock_elem, lock_compare, NULL);
          th->donated += lock->donation;
          th->donee = lock;
          th->priority -= lock->donation;
        }
      }
    }
  }

  //donate before block
  sema_down (&lock->semaphore);
  lock->holder = thread_current ();
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  struct thread* th = thread_current();
  if(lock->donation != 0 && !list_empty(&lock->semaphore.waiters)){
    // time to return!
    struct thread *first_thread = list_entry(list_front(&lock->semaphore.waiters), struct thread, elem);
    // first_thread->donated -= lock->donation;
    first_thread->donee = NULL;
    // printf("first th prio:%d\n",first_thread->priority);
    /* return donation */
    bool current_donator = false;
    if(list_entry(&lock->lock_elem, struct lock, lock_elem) == list_entry(list_front(&th->donator_locks), struct lock, lock_elem)){
      current_donator = true;
      Donator_Backword(first_thread, th->priority, lock->donation);
    }else{
      Donator_Backword(first_thread, first_thread->priority + lock->donation, lock->donation);
    }
    
    list_remove(&lock->lock_elem);
    // printf("return!3%s\n",thread_name());
    if(!list_empty(&th->donator_locks)) {    
      // set current lock's donation to zero
      if(current_donator){
        // printf("11\n");
        int temp = lock->donation - list_entry(list_front(&th->donator_locks), struct lock, lock_elem)->donation;
        lock->donation = 0;
        lock->holder = NULL;
        sema_up (&lock->semaphore);
        th->priority -= temp;
      }else{
        // printf("33\n");
        lock->donation = 0;
        lock->holder = NULL;
        sema_up (&lock->semaphore);
      }
      try_preempt();
    }else{
      // no more donations, free the lock
      // printf("22\n");
      int temp = lock->donation;
      lock->donation = 0;
      lock->holder = NULL;
      sema_up (&lock->semaphore);
      th->priority -= temp;
      if(th->priority_delayed){
        th->priority = th->priority_aysnc; 
        th->priority_delayed = false;
      }
      try_preempt();
    }
  }else{
    /* Q: why we can't move this sentence upward? */
    lock->holder = NULL;
    sema_up (&lock->semaphore);
  }
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem 
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
  };

/* list_elem a, b is semaphore_elem->elem
   it is guranteed that at least a thread struct is in semaphore_elem.semaphore.waiters
*/
bool comp_priority_cond(const struct list_elem *a, const struct list_elem *b, void *current_priority){
  // TODO assert()
  // struct semaphore_elem* s = list_entry(b, struct semaphore_elem, elem);
  // struct list_elem *e = list_front(&(list_entry(b, struct semaphore_elem, elem))->semaphore.waiters);
  return (*(int *)current_priority) > list_entry((list_front(&(list_entry(b, struct semaphore_elem, elem))->semaphore.waiters)), struct thread, elem)->priority;
}

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
  // list_push_back (&cond->waiters, &waiter.elem);
  list_insert_ordered(&cond->waiters, &waiter.elem, comp_priority_cond ,(void *)(&thread_current()->priority));
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)) {
    // struct semaphore_elem *sm = list_entry (list_pop_front (&cond->waiters),struct semaphore_elem, elem);
    // struct list_elem *e = list_front(&sm->semaphore.waiters);
    // struct thread*th = list_entry(e,struct thread, elem);
    // printf("%d\n",th->priority);
    // sema_up(&sm->semaphore);
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
  }
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}
