// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;

// Reference count for each physical page
// Indexed by physical page number (pa / PGSIZE)
// We need to track pages from 0 to PHYSTOP
#define MAX_PAGES ((PHYSTOP) / PGSIZE)
static int refcount[MAX_PAGES];
static struct spinlock refcount_lock;

// Track number of free pages
static int num_free_pages;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  initlock(&refcount_lock, "refcount");
  kmem.use_lock = 0;
  // Static arrays are zero-initialized by default, but explicitly zero for clarity
  // Only initialize the portion we'll actually use to avoid issues
  num_free_pages = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  struct run *r;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE) {
    // Static arrays are zero-initialized, so refcount is already 0
    // Don't need to explicitly set it here
    
    // Fill with junk to catch dangling refs.
    memset(p, 1, PGSIZE);
    
    if(kmem.use_lock)
      acquire(&kmem.lock);
    r = (struct run*)p;
    r->next = kmem.freelist;
    kmem.freelist = r;
    num_free_pages++;
    if(kmem.use_lock)
      release(&kmem.lock);
  }
}
// Get reference count for a physical page
int
getrefcount(uint pa)
{
  uint idx = pa / PGSIZE;
  if(idx >= MAX_PAGES)
    panic("getrefcount: index out of range");
  if(!kmem.use_lock)
    return 0; // Not tracking refcounts during early boot
  acquire(&refcount_lock);
  int count = refcount[idx];
  release(&refcount_lock);
  return count;
}

// Increment reference count for a physical page
void
incref(uint pa)
{
  uint idx = pa / PGSIZE;
  if(idx >= MAX_PAGES)
    panic("incref: index out of range");
  if(!kmem.use_lock)
    return; // Not tracking refcounts during early boot
  acquire(&refcount_lock);
  // Increment reference count for the child process
  // If refcount is 0, it means the page was allocated before tracking was enabled
  // In that case, initialize to 1 (parent) then increment to 2 (parent + child)
  // If refcount is 1 (from kalloc), increment to 2 (parent + child)
  if(refcount[idx] == 0) {
    // Page was never tracked - set to 1 (parent's reference)
    refcount[idx] = 1;
  }
  // Now increment for child's reference (makes it 2: parent + child)
  refcount[idx]++;
  release(&refcount_lock);
}

// Decrement reference count for a physical page
// Returns the new reference count
// If refcount is already 0, returns 0 (handles double-free gracefully)
int
decref(uint pa)
{
  uint idx = pa / PGSIZE;
  if(idx >= MAX_PAGES)
    panic("decref: index out of range");
  if(!kmem.use_lock)
    return 0; // Not tracking refcounts during early boot, assume can free
  acquire(&refcount_lock);
  if(refcount[idx] <= 0) {
    // Already zero - might be a double-free or page never allocated
    // Return 0 to indicate it's already free
    release(&refcount_lock);
    return 0;
  }
  refcount[idx]--;
  int count = refcount[idx];
  release(&refcount_lock);
  return count;
}

//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;
  uint pa = V2P(v);

  if((uint)v % PGSIZE || v < end || pa >= PHYSTOP)
    panic("kfree");

  // Decrement reference count (only if in valid range and after full init)
  uint idx = pa / PGSIZE;
  int ref = 0;
  if(idx < MAX_PAGES && kmem.use_lock) {
    // Only use refcount tracking after full initialization
    ref = decref(pa);
    // Only actually free if refcount is zero
    if(ref > 0)
      return;
  }
  
  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  num_free_pages++;
  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;
  uint pa;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    num_free_pages--;
  }
  if(kmem.use_lock)
    release(&kmem.lock);
  
  if(r) {
    pa = V2P((char*)r);
    // Set reference count to 1 for newly allocated page
    // Always set refcount if tracking is enabled (kmem.use_lock == 1)
    uint idx = pa / PGSIZE;
    if(idx < MAX_PAGES) {
      if(kmem.use_lock) {
        // Tracking is enabled - set refcount to 1
        acquire(&refcount_lock);
        // If refcount is already set (shouldn't happen for a free page), don't overwrite
        // But for a newly allocated page from freelist, it should be 0
        if(refcount[idx] == 0) {
          refcount[idx] = 1;
        }
        release(&refcount_lock);
      }
      // If tracking is not enabled yet, refcount stays 0 (will be handled by incref)
    }
  }
  
  return (char*)r;
}

// Get the number of free pages in the system
int
getNumFreePages(void)
{
  int count;
  if(kmem.use_lock)
    acquire(&kmem.lock);
  count = num_free_pages;
  if(kmem.use_lock)
    release(&kmem.lock);
  return count;
}

