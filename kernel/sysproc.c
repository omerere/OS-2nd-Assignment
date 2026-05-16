#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// added the bellow

static uint lcg_state = 1;
static struct spinlock lcg_lock;

void lcg_init(void) {
  initlock(&lcg_lock, "lcg_lock");
}

// Raw kernel functions accessible to other kernel files
void lcg_srand(uint seed) {
  acquire(&lcg_lock);
  lcg_state = seed;
  release(&lcg_lock);
}

uint lcg_rand(void) {
  uint a = 1664525;
  uint b = 1013904223;
  uint res;

  acquire(&lcg_lock);
  lcg_state = a * lcg_state + b;
  res = lcg_state;
  release(&lcg_lock);

  return res;
}

// System call wrappers for userspace access
uint64
sys_lcg_srand(void)
{
  int seed;
  argint(0, &seed);
  lcg_srand((uint)seed);
  return 0;
}

uint64
sys_lcg_rand(void)
{
  return lcg_rand();
}

uint64 sys_setgid(void) { 
  int gid;
  argint(0, &gid);
  myproc()->gid = gid;
  return 0;
}

uint64 sys_getgid(void) {
  return myproc()->gid;
}

uint64 sys_israeli_create(void) {
  int favoritism;
  argint(0, &favoritism);
  return israeli_create(favoritism);
}

uint64 sys_israeli_acquire(void) {
  int lock_id;
  argint(0, &lock_id);
  return israeli_acquire(lock_id);
}

uint64 sys_israeli_release(void) {
  int lock_id;
  argint(0, &lock_id);
  return israeli_release(lock_id);
}

uint64 sys_israeli_destroy(void) {
  int lock_id;
  argint(0, &lock_id);
  return israeli_destroy(lock_id);
}

static int team_scores[10]; // Supports up to 10 teams

uint64 sys_race_init(void) {
  for(int i = 0; i < 10; i++) {
    team_scores[i] = 0;
  }
  return 0;
}

uint64 sys_race_inc(void) {
  int team_id;
  argint(0, &team_id);
  
  if(team_id >= 0 && team_id < 10) {
    team_scores[team_id]++;
    return team_scores[team_id];
  }
  return -1;
}

uint64 sys_race_get_max(void) {
  int max = 0;
  for(int i = 0; i < 10; i++) {
    if(team_scores[i] > max) {
      max = team_scores[i];
    }
  }
  return max;
}

uint64 sys_race_get_score(void)
{
  int team_id;
  argint(0, &team_id);
  if(team_id >= 0 && team_id < 10){
    return team_scores[team_id];
  }
  return -1;
}