#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// Find the virtual address of the first shared page (PTE_S) in the
// user portion of the page table up to size 'sz'. Returns the virtual
// address on success or -1 on failure.
int
find_shared(pde_t *pgdir, uint sz)
{
  pte_t *pte;
  uint a;

  a = 0;
  while(a < sz && a < KERNBASE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(pte && (*pte & PTE_S)){
      return (int)a;
    }
    a += PGSIZE;
  }
  return -1;
}

// Unmap and free the shared page (if mapped) in the page table up to size 'sz'.
// Returns the virtual address that was unmapped or -1 on failure.
int
unmap_shared(pde_t *pgdir, uint sz)
{
  pte_t *pte;
  uint a, pa;

  a = 0;
  while(a < sz && a < KERNBASE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(pte && (*pte & PTE_S)){
      if((*pte & PTE_P) != 0){
        pa = PTE_ADDR(*pte);
        if(pa == 0)
          panic("kfree");
        kfree((char*)P2V(pa));
      }
      *pte = 0;
      return (int)a;
    }
    a += PGSIZE;
  }
  return -1;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;

  if((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0) {
      freevm(pgdir);
      return 0;
    }
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  if(p == 0)
    panic("switchuvm: no process");
  if(p->kstack == 0)
    panic("switchuvm: no kstack");
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  if((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;

  if(newsz >= KERNBASE)
    return 0;
  if(newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      // If this is a shared page, do not free it here - it will be
      // freed explicitly by unmapshared() to avoid double-free when
      // multiple processes reference the same physical page.
      if(!(*pte & PTE_S)){
        char *v = P2V(pa);
        kfree(v);
      }
      *pte = 0;
    }
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
  uint i;

  if(pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, KERNBASE, 0);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte, *parent_pte;
  uint pa, i, flags;

  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    
    // If this page is marked shared, map the same physical page
    // into the child's address space (do not allocate/copy).
    if(flags & PTE_S) {
      // map same physical address, preserving flags
      if(mappages(d, (void*)i, PGSIZE, pa, flags) < 0) {
        goto bad;
      }
    } else {
      // CoW: Share the page instead of copying
      // Remove write permission from both parent and child, but keep other flags (PTE_U, etc.)
      flags = flags & ~PTE_W;  // Remove write bit, preserve other flags
      
      // Increment reference count BEFORE mapping child (so refcount is correct)
      // This ensures refcount is 2 (parent=1, child=1) after fork
      incref(pa);
      
      // Map the same physical page to child (read-only)
      if(mappages(d, (void*)i, PGSIZE, pa, flags) < 0) {
        goto bad;
      }
      
      // Make parent's page read-only too
      // Note: pte and parent_pte point to the same entry, but we use parent_pte for clarity
      parent_pte = walkpgdir(pgdir, (void*)i, 0);
      if(parent_pte == 0)
        panic("copyuvm: parent pte should exist");
      // Update parent PTE: keep physical address, use new flags (read-only), ensure PTE_P is set
      *parent_pte = pa | flags | PTE_P;
    }
  }
  
  // Flush TLB for parent process (since we modified its page table)
  lcr3(V2P(pgdir));
  
  return d;

bad:
  freevm(d);
  return 0;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

// Count the number of physical pages allocated in the user part of the address space
// Returns the number of valid page table entries with PTE_P flag set
uint
countppages(pde_t *pgdir, uint sz)
{
  pte_t *pte;
  uint a, count;

  count = 0;
  a = 0;
  while(a < sz && a < KERNBASE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(pte != 0 && (*pte & PTE_P) != 0){
      count++;
    }
    a += PGSIZE;
  }
  return count;
}

// Count the number of page table pages used by a process
// This includes the page directory and all inner page table pages
uint
countpagepages(pde_t *pgdir)
{
  uint i, count;
  pde_t pde;

  count = 1;  // Count the page directory itself
  for(i = 0; i < NPDENTRIES; i++){
    pde = pgdir[i];
    if(pde & PTE_P){
      // This page table exists, count it
      count++;
    }
  }
  return count;
}

// Allocate a single page of physical memory for the given virtual address
// Used for on-demand memory allocation during page faults
// Returns 0 on success, -1 on error
int
allocuvm_ondemand(pde_t *pgdir, uint va)
{
  char *mem;
  
  // Round down to page boundary
  va = PGROUNDDOWN(va);
  
  // Allocate physical memory
  mem = kalloc();
  if(mem == 0){
    cprintf("allocuvm_ondemand out of memory\n");
    return -1;
  }
  
  // Clear the page
  memset(mem, 0, PGSIZE);
  
  // Map the physical page to the virtual address
  if(mappages(pgdir, (char*)va, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
    cprintf("allocuvm_ondemand mapping failed\n");
    kfree(mem);
    return -1;
  }
  
  return 0;
}

// Handle copy-on-write page fault
// Returns 0 on success, -1 on error
int
cowfault(pde_t *pgdir, uint va)
{
  pte_t *pte;
  uint pa;
  char *mem;
  int ref;
  
  // Round down to page boundary
  va = PGROUNDDOWN(va);
  
  // Get the page table entry
  pte = walkpgdir(pgdir, (char*)va, 0);
  if(pte == 0)
    return -1;
  
  if(!(*pte & PTE_P))
    return -1;
  
  // Check if this is a write fault on a read-only page (CoW)
  // If the page is writable, it's not a CoW fault
  if(*pte & PTE_W)
    return -1;
  
  pa = PTE_ADDR(*pte);
  ref = getrefcount(pa);
  
  // For a CoW page after fork, refcount should be >= 2 (parent + child)
  // However, if refcount is 0 or 1, it means:
  // - 0: Page was allocated before refcount tracking, or tracking failed
  // - 1: Only one process references it (shouldn't happen for CoW, but handle it)
  // In both cases, we can just make it writable without copying
  if(ref < 2) {
    // Not shared (or tracking issue) - just make it writable
    *pte = pa | PTE_FLAGS(*pte) | PTE_W | PTE_P;
    // Flush TLB
    lcr3(V2P(pgdir));
    return 0;
  }
  
  // ref >= 2: Multiple processes share this page, need to copy
  // Allocate new page
  mem = kalloc();
  if(mem == 0)
    return -1;
  
  // Copy content from original page
  memmove(mem, (char*)P2V(pa), PGSIZE);
  
  // Decrement reference count for original page
  decref(pa);
  
  // Map new page with write permission
  *pte = V2P(mem) | (PTE_FLAGS(*pte) | PTE_W) | PTE_P;
  
  // Flush TLB
  lcr3(V2P(pgdir));
  
  return 0;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

