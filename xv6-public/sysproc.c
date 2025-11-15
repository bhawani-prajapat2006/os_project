#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

// Forward declarations for helper functions in vm.c
extern uint countppages(pde_t*, uint);
extern uint countpagepages(pde_t*);

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int sys_hello(void)
{
  cprintf("hello from kernal\n");
  return 0;
}

// Return the number of virtual pages in the user part of the address space
// This includes all pages from 0 to process size, plus the stack guard page
int
sys_numvp(void)
{
  struct proc *p = myproc();
  uint num_pages;

  // Number of pages up to process size
  num_pages = PGROUNDUP(p->sz) / PGSIZE;
  
  // Add 1 for the stack guard page (the page just above the stack)
  // The stack guard page is at p->sz
  num_pages += 1;

  return num_pages;
}

// Return the number of physical pages allocated in the user part of the address space
// This counts all valid page table entries with the present bit set
int
sys_numpp(void)
{
  struct proc *p = myproc();
  
  return countppages(p->pgdir, p->sz);
}

// Return the size of the page table in terms of pages
// This includes the page directory and all allocated page table pages
int
sys_getptsize(void)
{
  struct proc *p = myproc();
  
  return countpagepages(p->pgdir);
}

// Memory map system call - expand virtual address space without allocating physical memory
// Takes one argument: number of bytes to map
// Returns starting virtual address of the new memory region, or 0 on error
int
sys_mmap(void)
{
  int n;
  struct proc *p = myproc();
  uint new_addr;
  
  // Get the argument (number of bytes to map)
  if(argint(0, &n) < 0)
    return 0;
  
  // Validate input: must be positive and multiple of PGSIZE
  if(n <= 0 || (n % PGSIZE) != 0)
    return 0;
  
  // Check if sz + n would exceed KERNBASE
  if(p->sz + n >= KERNBASE)
    return 0;
  
  // Store the starting address before incrementing
  new_addr = p->sz;
  
  // Expand process size WITHOUT allocating physical memory
  // The page fault handler in trap.c will allocate pages on demand
  // when the user accesses them
  p->sz += n;
  
  // Update mmap region bounds to track all mapped memory
  // (This allows the page fault handler to know if an address
  // should be on-demand allocated)
  if(p->mmap_start == 0)
    p->mmap_start = new_addr;
  p->mmap_end = p->sz;
  
  return new_addr;
}

// Prototypes for VM helpers defined in vm.c
extern int mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm);
extern int find_shared(pde_t *pgdir, uint sz);
extern int unmap_shared(pde_t *pgdir, uint sz);

// Create a shared page at the end of the calling process's user address
// space. Returns the starting virtual address on success, 0 on failure.
int
sys_mapshared(void)
{
  struct proc *p = myproc();
  char *addr;
  char *mem;

  // Map at end of address space
  addr = (char*)p->sz;
  if((uint)addr + PGSIZE >= KERNBASE)
    return 0;

  mem = kalloc();
  if(mem == 0)
    return 0;
  memset(mem, 0, PGSIZE);

  if(mappages(p->pgdir, addr, PGSIZE, V2P(mem), PTE_W|PTE_U|PTE_S) < 0){
    kfree(mem);
    return 0;
  }

  p->sz += PGSIZE;
  return (int)addr;
}

// Return the virtual address of the shared page in this process, or 0 if none
int
sys_getshared(void)
{
  struct proc *p = myproc();
  int vaddr = find_shared(p->pgdir, p->sz);
  if(vaddr < 0)
    return 0;
  return vaddr;
}

// Unmap the shared page from this process and free its physical memory.
// Returns 0 on success, -1 on error.
int
sys_unmapshared(void)
{
  struct proc *p = myproc();
  int vaddr = unmap_shared(p->pgdir, p->sz);
  if(vaddr < 0)
    return -1;
  // If the shared page was at the end of the address space, shrink sz
  if((uint)vaddr + PGSIZE == p->sz)
    p->sz -= PGSIZE;
  return 0;
}

int
sys_getNumFreePages(void)
{
  return getNumFreePages();
}
