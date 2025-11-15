# Code Implementation Reference

This document provides exact code snippets for each file that needs modification.

## File 1: syscall.h - Add System Call Numbers

**Location**: `/home/sakti/qemu/xv6-public/syscall.h`

**Current content** (last few lines):
```c
#define SYS_link   19
#define SYS_mkdir  20
#define SYS_close  21
#define SYS_hello  22
```

**What to add** (after SYS_hello):
```c
#define SYS_numvp  23
#define SYS_numpp  24
#define SYS_getptsize  25
```

**Result** (complete file should look like):
```c
// System call numbers
#define SYS_fork    1
#define SYS_exit    2
#define SYS_wait    3
#define SYS_pipe    4
#define SYS_read    5
#define SYS_kill    6
#define SYS_exec    7
#define SYS_fstat   8
#define SYS_chdir   9
#define SYS_dup    10
#define SYS_getpid 11
#define SYS_sbrk   12
#define SYS_sleep  13
#define SYS_uptime 14
#define SYS_open   15
#define SYS_write  16
#define SYS_mknod  17
#define SYS_unlink 18
#define SYS_link   19
#define SYS_mkdir  20
#define SYS_close  21
#define SYS_hello  22
#define SYS_numvp  23
#define SYS_numpp  24
#define SYS_getptsize  25
```

---

## File 2: vm.c - Add Helper Functions

**Location**: `/home/sakti/qemu/xv6-public/vm.c`

**Add at the end** (before the blank pages section):

```c
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
```

**Explanation of countppages()**:
- Takes page directory pointer and size limit
- Loops from address 0 to sz, incrementing by PGSIZE (4096)
- For each address, calls walkpgdir() to get the PTE
- If PTE exists (pte != 0) AND has present bit set (*pte & PTE_P)
- Increments counter
- Returns total count of physical pages

**Explanation of countpagepages()**:
- Takes page directory pointer
- Initializes count to 1 (for page directory itself)
- Loops through all 1024 page directory entries
- Each entry that has PTE_P set represents one allocated page table page
- Increments counter for each present entry
- Returns total count of page table pages

---

## File 3: sysproc.c - Add Syscall Handlers

**Location**: `/home/sakti/qemu/xv6-public/sysproc.c`

**Add at the end** (after the sys_hello function):

```c
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
```

**Explanation of sys_numvp()**:
1. Get current process: `myproc()`
2. Calculate: PGROUNDUP(p->sz) rounds up to next page boundary
3. Divide by PGSIZE to get number of pages
4. Add 1 for the unmapped stack guard page
5. Return total

**Explanation of sys_numpp()**:
1. Get current process
2. Call helper function countppages with page directory and size
3. Helper walks page table and counts entries with PTE_P flag
4. Return count

**Explanation of sys_getptsize()**:
1. Get current process
2. Call helper function countpagepages with page directory
3. Helper counts page directory (1) + all allocated page tables
4. Return count

---

## File 4: syscall.c - Register Syscalls

**Location**: `/home/sakti/qemu/xv6-public/syscall.c`

### Change 1: Add extern declarations

**Find this section**:
```c
extern int sys_chdir(void);
extern int sys_close(void);
// ... other extern declarations ...
extern int sys_hello(void);
```

**Add these lines** (after sys_hello):
```c
extern int sys_numvp(void);
extern int sys_numpp(void);
extern int sys_getptsize(void);
```

### Change 2: Register in syscalls array

**Find this section**:
```c
static int (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
// ... other syscalls ...
[SYS_close]   sys_close,
[SYS_hello]   sys_hello,
};
```

**Add these lines** (before the closing brace, after sys_hello):
```c
[SYS_numvp]     sys_numvp,
[SYS_numpp]     sys_numpp,
[SYS_getptsize] sys_getptsize,
```

---

## File 5: user.h - Add User Declarations

**Location**: `/home/sakti/qemu/xv6-public/user.h`

**Find this section**:
```c
// system calls
int fork(void);
int exit(void) __attribute__((noreturn));
// ... other syscall declarations ...
int uptime(void);
int hello(void);
```

**Add these lines** (after hello):
```c
int numvp(void);
int numpp(void);
int getptsize(void);
```

---

## File 6: usys.S - Add Syscall Stubs

**Location**: `/home/sakti/qemu/xv6-public/usys.S`

**What to add** (at the end, before any closing directives):

This file typically uses a macro SYSCALL that generates the actual INT instruction.

Add these lines:
```asm
SYSCALL(numvp)
SYSCALL(numpp)
SYSCALL(getptsize)
```

If the file doesn't have a SYSCALL macro, look at existing syscalls and follow the same pattern.

---

## File 7: memtest.c - Create Test Program

**Create new file**: `/home/sakti/qemu/xv6-public/memtest.c`

**Complete file content**:
```c
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int
main(int argc, char *argv[])
{
  int vp, pp, pts;
  int vp2, pp2, pts2;
  int i;
  
  printf(1, "=== Memory Info Test ===\n");
  printf(1, "\nInitial state:\n");
  
  // Test the three new syscalls
  vp = numvp();
  pp = numpp();
  pts = getptsize();
  
  printf(1, "Virtual Pages (numvp): %d\n", vp);
  printf(1, "Physical Pages (numpp): %d\n", pp);
  printf(1, "Page Table Size (getptsize): %d pages\n", pts);
  
  // Check sanity
  if(vp >= 1 && pp >= 1 && pts >= 2) {
    printf(1, "✓ Initial values look reasonable\n");
  } else {
    printf(1, "✗ Values seem wrong!\n");
  }
  
  // Test 1: Allocate some memory
  printf(1, "\nTest 1: After malloc(4096):\n");
  char *ptr1 = malloc(4096);
  
  vp2 = numvp();
  pp2 = numpp();
  pts2 = getptsize();
  
  printf(1, "Virtual Pages: %d (was %d)\n", vp2, vp);
  printf(1, "Physical Pages: %d (was %d)\n", pp2, pp);
  printf(1, "Page Table Size: %d pages (was %d)\n", pts2, pts);
  
  if(vp2 >= vp && pp2 >= pp) {
    printf(1, "✓ Values increased or stayed same\n");
  } else {
    printf(1, "✗ Values should not decrease!\n");
  }
  
  // Test 2: Allocate more memory
  printf(1, "\nTest 2: After malloc(8192):\n");
  char *ptr2 = malloc(8192);
  
  int vp3 = numvp();
  int pp3 = numpp();
  int pts3 = getptsize();
  
  printf(1, "Virtual Pages: %d (was %d)\n", vp3, vp2);
  printf(1, "Physical Pages: %d (was %d)\n", pp3, pp2);
  printf(1, "Page Table Size: %d pages (was %d)\n", pts3, pts2);
  
  // Test 3: Multiple allocations
  printf(1, "\nTest 3: Multiple small allocations:\n");
  for(i = 0; i < 10; i++) {
    char *p = malloc(1024);
    if(p == 0) {
      printf(1, "malloc failed at iteration %d\n", i);
      break;
    }
  }
  
  int vp4 = numvp();
  int pp4 = numpp();
  int pts4 = getptsize();
  
  printf(1, "Virtual Pages: %d\n", vp4);
  printf(1, "Physical Pages: %d\n", pp4);
  printf(1, "Page Table Size: %d pages\n", pts4);
  
  printf(1, "\n=== Test Complete ===\n");
  
  exit();
}
```

**How to compile**:
1. Add `memtest` to the `UPROGS` variable in the Makefile
2. Run `make clean && make`
3. The test program will be included in the xv6 image

---

## File 8: Makefile - Add Test Program

**Location**: `/home/sakti/qemu/xv6-public/Makefile`

**Find this section**:
```makefile
UPROGS=\
	_cat\
	_echo\
	_forktest\
	_grep\
	_init\
	_kill\
	_ln\
	_ls\
	_mkdir\
	_rm\
	_sh\
	_stressfs\
	_usertests\
	_wc\
	_zombie\
```

**Add** `_memtest\` to the list (in alphabetical order):
```makefile
UPROGS=\
	_cat\
	_echo\
	_forktest\
	_grep\
	_init\
	_kill\
	_ln\
	_ls\
	_memtest\
	_mkdir\
	_rm\
	_sh\
	_stressfs\
	_usertests\
	_wc\
	_zombie\
```

---

## Summary of Changes

| File | Change Type | Lines to Add |
|------|-------------|--------------|
| syscall.h | Add | 3 defines |
| vm.c | Add | 2 functions (~40 lines) |
| sysproc.c | Add | 3 functions (~35 lines) |
| syscall.c | Add | 6 lines (externs + array entries) |
| user.h | Add | 3 declarations |
| usys.S | Add | 3 SYSCALL macros |
| memtest.c | Create | New file (~90 lines) |
| Makefile | Add | 1 entry |

---

## Testing Steps

1. **Compile**:
   ```bash
   cd /home/sakti/qemu/xv6-public
   make clean
   make
   ```

2. **Run xv6**:
   ```bash
   make qemu
   ```

3. **In xv6 shell**:
   ```
   $ memtest
   ```

4. **Expected output**:
   ```
   === Memory Info Test ===

   Initial state:
   Virtual Pages (numvp): 2
   Physical Pages (numpp): 2
   Page Table Size (getptsize): 2 pages
   ✓ Initial values look reasonable

   Test 1: After malloc(4096):
   Virtual Pages: 3 (was 2)
   Physical Pages: 3 (was 2)
   Page Table Size: 2 pages (was 2)
   ✓ Values increased or stayed same
   ...
   ```

---

## Troubleshooting

### Compilation Errors

**Error**: `undefined reference to 'sys_numvp'`
- **Solution**: Make sure you added extern declarations in syscall.c

**Error**: `numvp is not defined`
- **Solution**: Make sure you added declarations to user.h

**Error**: `'countppages' undeclared`
- **Solution**: Make sure you added the helper function to vm.c

### Runtime Issues

**Problem**: Syscall returns -1 (error)
- Check that syscall number is registered in syscall.c array
- Verify function signatures match

**Problem**: memtest program not found
- Add `_memtest` to Makefile UPROGS list
- Run `make clean && make`

**Problem**: Values don't seem right
- Print debug information in your syscalls
- Use `printf()` to log PTE checks
- Verify PGROUNDUP calculation

---

## Debugging Tips

Add debug output in vm.c:
```c
uint
countppages(pde_t *pgdir, uint sz)
{
  // Add this for debugging:
  cprintf("countppages: checking up to sz=%d\n", sz);
  
  pte_t *pte;
  uint a, count;

  count = 0;
  a = 0;
  while(a < sz && a < KERNBASE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(pte != 0 && (*pte & PTE_P) != 0){
      count++;
      // cprintf("  Page at %x is present\n", a);
    }
    a += PGSIZE;
  }
  cprintf("countppages: found %d pages\n", count);
  return count;
}
```

Then remove debug output after testing passes.

