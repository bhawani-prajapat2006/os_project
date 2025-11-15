# Quick Start Guide - 5 Minute Overview

## What You Need to Do

Implement 3 new system calls in xv6:

1. **numvp()** - Return number of virtual pages
2. **numpp()** - Return number of physical pages  
3. **getptsize()** - Return size of page table

## The 8 Changes Required

### 1. syscall.h - Add 3 lines
```c
#define SYS_numvp      23
#define SYS_numpp      24
#define SYS_getptsize  25
```

### 2. vm.c - Add 2 functions (~40 lines)

Function 1 - Count physical pages:
```c
uint countppages(pde_t *pgdir, uint sz)
{
  pte_t *pte;
  uint a, count;
  count = 0;
  a = 0;
  while(a < sz && a < KERNBASE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(pte != 0 && (*pte & PTE_P) != 0)
      count++;
    a += PGSIZE;
  }
  return count;
}
```

Function 2 - Count page table pages:
```c
uint countpagepages(pde_t *pgdir)
{
  uint i, count;
  count = 1;  // page directory
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P)
      count++;
  }
  return count;
}
```

### 3. sysproc.c - Add 3 functions (~35 lines)

```c
int sys_numvp(void)
{
  struct proc *p = myproc();
  return PGROUNDUP(p->sz) / PGSIZE + 1;
}

int sys_numpp(void)
{
  struct proc *p = myproc();
  return countppages(p->pgdir, p->sz);
}

int sys_getptsize(void)
{
  struct proc *p = myproc();
  return countpagepages(p->pgdir);
}
```

### 4. syscall.c - Add extern declarations
```c
extern int sys_numvp(void);
extern int sys_numpp(void);
extern int sys_getptsize(void);
```

### 5. syscall.c - Add to array
```c
[SYS_numvp]     sys_numvp,
[SYS_numpp]     sys_numpp,
[SYS_getptsize] sys_getptsize,
```

### 6. user.h - Add declarations
```c
int numvp(void);
int numpp(void);
int getptsize(void);
```

### 7. usys.S - Add stubs
```asm
SYSCALL(numvp)
SYSCALL(numpp)
SYSCALL(getptsize)
```

### 8. Makefile - Add to UPROGS
Add `_memtest\` to the UPROGS list

## Create Test Program

File: `memtest.c`

```c
#include "types.h"
#include "stat.h"
#include "user.h"

int main(void)
{
  printf(1, "numvp: %d\n", numvp());
  printf(1, "numpp: %d\n", numpp());
  printf(1, "getptsize: %d\n", getptsize());
  
  char *ptr = malloc(4096);
  
  printf(1, "After malloc:\n");
  printf(1, "numvp: %d\n", numvp());
  printf(1, "numpp: %d\n", numpp());
  printf(1, "getptsize: %d\n", getptsize());
  
  exit();
}
```

## Build and Test

```bash
cd /home/sakti/qemu/xv6-public
make clean
make
make qemu
# In xv6 shell:
$ memtest
```

## Expected Output

```
numvp: 2
numpp: 2
getptsize: 2 pages
After malloc:
numvp: 3
numpp: 3
getptsize: 2 pages
```

## Key Formulas

**numvp()**: `PGROUNDUP(p->sz) / PGSIZE + 1`
- Count pages up to process size
- Add 1 for stack guard page

**numpp()**: Walk page table, count entries with PTE_P flag set
- Only count pages actually mapped in memory
- Stop at KERNBASE

**getptsize()**: 1 (page directory) + number of valid page tables
- Scan page directory
- Count entries with PTE_P flag

## What Each Returns

| Function | Example | Meaning |
|----------|---------|---------|
| numvp() | 3 | 3 virtual pages (includes guard) |
| numpp() | 2 | 2 physical pages (no guard) |
| getptsize() | 2 | 2 pages for page tables (1 dir + 1 PT) |

## Important Constants

- `PGSIZE` = 4096 (bytes per page)
- `NPDENTRIES` = 1024 (entries in page directory)
- `PTE_P` = 0x001 (present bit in PTE)
- `KERNBASE` = 0x80000000 (kernel space start)

## Common Mistakes to Avoid

1. ❌ Forget to add syscall number in syscall.h
2. ❌ Forget to register in syscalls array in syscall.c
3. ❌ Wrong function names (must match exactly)
4. ❌ Forget to check PTE_P flag
5. ❌ Don't include PGSIZE constant
6. ❌ Stop at wrong boundary (should be KERNBASE, not p->sz)

## Verification Steps

```bash
# 1. Check for compilation errors
cd /home/sakti/qemu/xv6-public
make clean && make 2>&1 | grep -i error

# 2. Check kernel created
ls -l kernel

# 3. Check test program will build
grep _memtest Makefile

# 4. Run qemu
make qemu

# 5. In qemu:
$ memtest
```

## How to Verify Correct Output

- [ ] numvp >= 1 ✓
- [ ] numpp >= 1 ✓
- [ ] getptsize >= 2 ✓
- [ ] numpp <= numvp ✓
- [ ] After malloc, values increase or stay same ✓

## If Something Goes Wrong

**Compilation fails**:
- Check for typos in function names
- Verify all #includes are present
- Check syntax of new functions

**Program not found**:
- Add _memtest to Makefile UPROGS
- Recompile with `make clean && make`

**Returns wrong values**:
- Add debug cprintf() statements
- Check walkpgdir() is working
- Verify loop boundaries

**Syscall returns -1**:
- Check syscall.h number is 23-25
- Verify registered in syscall.c array
- Check function signature matches

## For More Details

Read these documents in order:
1. IMPLEMENTATION_GUIDE.md - Full explanation
2. CODE_REFERENCE.md - All code snippets
3. MEMORY_LAYOUT.md - Visual diagrams
4. CHECKLIST.md - Step-by-step checklist

## Done!

Once memtest works:
- ✓ All three syscalls implemented
- ✓ Understand page tables
- ✓ Know virtual vs physical memory
- ✓ Can extend xv6 in future

