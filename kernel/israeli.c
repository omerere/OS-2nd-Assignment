#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

// External functions
extern uint lcg_rand(void);           // From Task 0
extern int get_gid_by_pid(int pid);   // From proc.c helper

// 1. Data Structure 
struct israeli_lock {
  int active;           
  int locked;           
  int favoritism;       
  int owner_gid;        
  
  int queue[16];        // Fixed-size array 
  int q_head;           
  int q_size;           
  
  uint internal_lock;   // For atomic operations 
};

// Global array of 15 locks 
struct israeli_lock ilocks[15];

// 2. Concurrency Helpers 
void acquire_internal(uint *lk) {
  while(__sync_lock_test_and_set(lk, 1) != 0)
    ;
  __sync_synchronize();
}

void release_internal(uint *lk) {
  __sync_synchronize();
  __sync_lock_release(lk);
}

// 3. System Calls
void israeli_init(void) {
  for(int i = 0; i < 15; i++) {
    ilocks[i].active = 0;
    ilocks[i].internal_lock = 0;
  }
}

int israeli_create(int favoritism) {
  if (favoritism < 0 || favoritism > 100) return -1;
  
  for(int i = 0; i < 15; i++) {
    acquire_internal(&ilocks[i].internal_lock);
    if(ilocks[i].active == 0) {
      ilocks[i].active = 1;
      ilocks[i].locked = 0;
      ilocks[i].favoritism = favoritism;
      ilocks[i].q_head = 0;
      ilocks[i].q_size = 0;
      ilocks[i].owner_gid = -1;
      release_internal(&ilocks[i].internal_lock);
      return i; 
    }
    release_internal(&ilocks[i].internal_lock);
  }
  return -1; 
}

int israeli_destroy(int lock_id) {
  if (lock_id < 0 || lock_id >= 15) return -1;
  
  acquire_internal(&ilocks[lock_id].internal_lock);
  if (ilocks[lock_id].active == 0) {
    release_internal(&ilocks[lock_id].internal_lock);
    return -1;
  }
  ilocks[lock_id].active = 0;
  release_internal(&ilocks[lock_id].internal_lock);
  return 0;
}

int israeli_acquire(int lock_id) {
  if (lock_id < 0 || lock_id >= 15) return -1;
  struct proc *p = myproc();
  
  acquire_internal(&ilocks[lock_id].internal_lock);
  if (ilocks[lock_id].active == 0) {
    release_internal(&ilocks[lock_id].internal_lock);
    return -1;
  }

  // Everyone must enter the queue, no bypassing 
  if (ilocks[lock_id].q_size < 16) {
    int tail = (ilocks[lock_id].q_head + ilocks[lock_id].q_size) % 16;
    ilocks[lock_id].queue[tail] = p->pid;
    ilocks[lock_id].q_size++;
  }
  release_internal(&ilocks[lock_id].internal_lock);

  // Yield loop 
  while (1) {
    acquire_internal(&ilocks[lock_id].internal_lock);
    
    if (ilocks[lock_id].active == 0) {
      release_internal(&ilocks[lock_id].internal_lock);
      return -1; // Bail out gracefully
    }

    // Acquire if lock is free AND we are the first in line
    if (ilocks[lock_id].locked == 0 && ilocks[lock_id].queue[ilocks[lock_id].q_head] == p->pid) {
      ilocks[lock_id].locked = 1;
      ilocks[lock_id].owner_gid = p->gid;
      
      // Remove ourselves from the front of the queue
      ilocks[lock_id].q_head = (ilocks[lock_id].q_head + 1) % 16;
      ilocks[lock_id].q_size--;
      
      release_internal(&ilocks[lock_id].internal_lock);
      return 0; 
    }
    
    release_internal(&ilocks[lock_id].internal_lock);
    yield(); // CPU given up, no busy waiting
  }
}

int israeli_release(int lock_id) {
  if (lock_id < 0 || lock_id >= 15) return -1;
  
  acquire_internal(&ilocks[lock_id].internal_lock);
  if (ilocks[lock_id].active == 0 || ilocks[lock_id].locked == 0) {
    release_internal(&ilocks[lock_id].internal_lock);
    return -1;
  }

  int G = ilocks[lock_id].owner_gid; 
  int c = ilocks[lock_id].favoritism; 
  
  ilocks[lock_id].locked = 0; 

  // Lock policy execution 
  if (ilocks[lock_id].q_size > 0) {
    int first_match_idx = -1;
    
    // Step 1: Identify earliest waiting process with gid == G
    for (int i = 0; i < ilocks[lock_id].q_size; i++) {
      int idx = (ilocks[lock_id].q_head + i) % 16;
      int waiting_pid = ilocks[lock_id].queue[idx];
      
      if (get_gid_by_pid(waiting_pid) == G) { 
        first_match_idx = idx; 
        break; 
      }
    }

    int target_idx = ilocks[lock_id].q_head; // Default to standard FIFO

    // Step 2: Probability check
    if (first_match_idx != -1) {
      if ((lcg_rand() % 100) < c) { 
        target_idx = first_match_idx; 
      }
    }
    
    // Step 3: If favoritism applied, shift the favored process to the front
    // This perfectly preserves the strict FIFO order for everyone else.
    if (target_idx != ilocks[lock_id].q_head) {
        int favored_pid = ilocks[lock_id].queue[target_idx];
        int curr = target_idx;
        
        // Slide everyone else back one spot in the circular buffer
        while (curr != ilocks[lock_id].q_head) {
            int prev = (curr - 1 + 16) % 16;
            ilocks[lock_id].queue[curr] = ilocks[lock_id].queue[prev];
            curr = prev;
        }
        // Place the favored process at the very front
        ilocks[lock_id].queue[ilocks[lock_id].q_head] = favored_pid;
    }
  }

  release_internal(&ilocks[lock_id].internal_lock);
  return 0;
}