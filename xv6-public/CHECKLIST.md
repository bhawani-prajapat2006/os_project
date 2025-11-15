# Implementation Checklist and Verification Guide

## Pre-Implementation Checklist

- [ ] Backup current files (optional but recommended)
- [ ] Read through IMPLEMENTATION_GUIDE.md completely
- [ ] Review MEMORY_LAYOUT.md to understand page tables
- [ ] Review CODE_REFERENCE.md for exact code snippets
- [ ] Understand what each syscall should return
- [ ] Understand the difference between VP, PP, and PTSIZE

## Implementation Checklist

### Step 1: syscall.h - Add System Call Numbers
- [ ] Open `/home/sakti/qemu/xv6-public/syscall.h`
- [ ] Find the line with `#define SYS_hello  22`
- [ ] Add three new lines:
  - [ ] `#define SYS_numvp  23`
  - [ ] `#define SYS_numpp  24`
  - [ ] `#define SYS_getptsize  25`
- [ ] Save file
- [ ] Verify no compilation errors: `make clean && make 2>&1 | head -20`

### Step 2: vm.c - Add Helper Functions
- [ ] Open `/home/sakti/qemu/xv6-public/vm.c`
- [ ] Find the end of the file (before blank pages)
- [ ] Add `countppages()` function:
  - [ ] Copy exact function from CODE_REFERENCE.md
  - [ ] Verify function signature: `uint countppages(pde_t *pgdir, uint sz)`
  - [ ] Check for typos in walkpgdir call
  - [ ] Verify PTE_P flag check syntax
- [ ] Add `countpagepages()` function:
  - [ ] Copy exact function from CODE_REFERENCE.md
  - [ ] Verify function signature: `uint countpagepages(pde_t *pgdir)`
  - [ ] Check NPDENTRIES loop is correct
  - [ ] Verify PTE_P flag check syntax
- [ ] Save file
- [ ] Verify no compilation errors

### Step 3: sysproc.c - Add Syscall Handlers
- [ ] Open `/home/sakti/qemu/xv6-public/sysproc.c`
- [ ] Find `sys_hello()` function at the end
- [ ] Add `sys_numvp()` function:
  - [ ] Get process pointer with `myproc()`
  - [ ] Calculate PGROUNDUP(p->sz) / PGSIZE
  - [ ] Add 1 for guard page
  - [ ] Return total
- [ ] Add `sys_numpp()` function:
  - [ ] Get process pointer with `myproc()`
  - [ ] Call `countppages(p->pgdir, p->sz)`
  - [ ] Return result
- [ ] Add `sys_getptsize()` function:
  - [ ] Get process pointer with `myproc()`
  - [ ] Call `countpagepages(p->pgdir)`
  - [ ] Return result
- [ ] Save file
- [ ] Verify no compilation errors

### Step 4: syscall.c - Register Syscalls (Part A)
- [ ] Open `/home/sakti/qemu/xv6-public/syscall.c`
- [ ] Find section with `extern int sys_hello(void);`
- [ ] Add three lines after it:
  - [ ] `extern int sys_numvp(void);`
  - [ ] `extern int sys_numpp(void);`
  - [ ] `extern int sys_getptsize(void);`
- [ ] Save and verify no errors

### Step 5: syscall.c - Register Syscalls (Part B)
- [ ] Open `/home/sakti/qemu/xv6-public/syscall.c` (already open from Step 4)
- [ ] Find `static int (*syscalls[])(void)` array
- [ ] Find the last entry: `[SYS_hello]   sys_hello,`
- [ ] Add three new entries after it:
  - [ ] `[SYS_numvp]     sys_numvp,`
  - [ ] `[SYS_numpp]     sys_numpp,`
  - [ ] `[SYS_getptsize] sys_getptsize,`
- [ ] Verify closing `};` is still there
- [ ] Save file
- [ ] Verify no compilation errors

### Step 6: user.h - Add User Declarations
- [ ] Open `/home/sakti/qemu/xv6-public/user.h`
- [ ] Find section with system call declarations
- [ ] Find line `int hello(void);`
- [ ] Add three lines after it:
  - [ ] `int numvp(void);`
  - [ ] `int numpp(void);`
  - [ ] `int getptsize(void);`
- [ ] Save file
- [ ] Verify no compilation errors

### Step 7: usys.S - Add Assembly Stubs
- [ ] Open `/home/sakti/qemu/xv6-public/usys.S`
- [ ] Look for existing SYSCALL macro usages
- [ ] Find a good place to add new syscalls (typically at end)
- [ ] Add three lines:
  - [ ] `SYSCALL(numvp)`
  - [ ] `SYSCALL(numpp)`
  - [ ] `SYSCALL(getptsize)`
- [ ] Save file
- [ ] Verify no compilation errors

### Step 8: Create memtest.c - Test Program
- [ ] Create new file `/home/sakti/qemu/xv6-public/memtest.c`
- [ ] Copy test program from CODE_REFERENCE.md
- [ ] Verify includes are correct
- [ ] Check printf statements for syntax
- [ ] Verify malloc calls
- [ ] Check exit() is called at end
- [ ] Save file

### Step 9: Makefile - Add Test Program
- [ ] Open `/home/sakti/qemu/xv6-public/Makefile`
- [ ] Find `UPROGS=\` section
- [ ] Find line with `_hello_test\` or similar
- [ ] Add `_memtest\` to the list in alphabetical order
  - [ ] Should be between `_mkdir\` and `_rm\` typically
- [ ] Verify backslash at end of line (except last)
- [ ] Save file

## Compilation Verification

Run these commands to verify everything compiles:

```bash
cd /home/sakti/qemu/xv6-public
make clean
make 2>&1 | tee build.log
```

- [ ] Compilation completes without errors
- [ ] Kernel is created: `ls -l kernel` shows file
- [ ] File system image created: `ls -l fs.img` shows file
- [ ] memtest is compiled: grep memtest build.log shows output

## Runtime Testing

### Test 1: Basic Functionality
```bash
cd /home/sakti/qemu/xv6-public
make qemu
# In xv6 shell:
$ memtest
```

Expected output includes:
- [ ] "Virtual Pages (numvp):" followed by a number
- [ ] "Physical Pages (numpp):" followed by a number
- [ ] "Page Table Size (getptsize):" followed by a number
- [ ] All numbers are positive (≥ 1)
- [ ] "Initial values look reasonable" message

### Test 2: Value Sanity Checks

From the output, verify:
- [ ] `numvp() ≥ 1` (at least the executable itself)
- [ ] `numpp() ≥ 1` (at least one page mapped)
- [ ] `getptsize() ≥ 2` (page dir + at least 1 page table)
- [ ] `numvp() ≥ numpp()` (virtual ≥ physical in xv6)
- [ ] `getptsize() ≤ 1024` (max 1024 page tables in 32-bit arch)

### Test 3: After malloc

From memtest output, check:
- [ ] After malloc(4096), `numvp()` increases or stays same
- [ ] After malloc(4096), `numpp()` increases or stays same
- [ ] After malloc(4096), `getptsize()` increases or stays same

### Test 4: Repeated Calls

- [ ] Run memtest multiple times: `memtest && memtest`
- [ ] First and second output should be identical
- [ ] No crashes or panics

### Test 5: Direct Shell Tests

In xv6 shell, try these commands:
```
$ cd /tmp
$ test.c  (create and run test programs)
```

Or create simple test programs:

**test1.c**:
```c
#include "types.h"
#include "user.h"

int main() {
  printf(1, "numvp: %d\n", numvp());
  exit();
}
```

Compile and test:
- [ ] `gcc -c test1.c` (creates object file)
- [ ] Run the test executable
- [ ] Should print "numvp: <number>"

## Verification Script

Run this series of tests:

```bash
#!/bin/bash
cd /home/sakti/qemu/xv6-public

echo "=== COMPILATION TEST ==="
make clean > /dev/null 2>&1
if make 2>&1 | grep -q "error:"; then
  echo "✗ Compilation failed"
  exit 1
else
  echo "✓ Compilation successful"
fi

if [ ! -f kernel ]; then
  echo "✗ kernel file not created"
  exit 1
else
  echo "✓ kernel file created"
fi

if [ ! -f fs.img ]; then
  echo "✗ fs.img not created"
  exit 1
else
  echo "✓ fs.img created"
fi

echo ""
echo "=== BUILD VERIFICATION ==="
echo "✓ All files compiled successfully"
echo "✓ Ready for qemu testing"
```

Save as `verify.sh` and run:
```bash
chmod +x verify.sh
./verify.sh
```

## Troubleshooting Guide

### Issue: "undefined reference to 'sys_numvp'"

**Cause**: syscall.c doesn't have extern declaration

**Fix**:
1. Open syscall.c
2. Check for: `extern int sys_numvp(void);`
3. If missing, add it after sys_hello

**Verify**: `grep sys_numvp syscall.c` should show:
```
extern int sys_numvp(void);
[SYS_numvp]     sys_numvp,
```

### Issue: "numvp: not found" in xv6 shell

**Cause**: user.h missing declaration OR usys.S missing SYSCALL macro

**Fix**:
1. Check user.h: `grep numvp user.h` should show `int numvp(void);`
2. Check usys.S: `grep SYSCALL usys.S | grep numvp` should show entry
3. If missing, add them and recompile

### Issue: syscall returns -1 (error)

**Cause**: Syscall number not registered in array OR handler function missing

**Fix**:
1. In syscall.c, verify: `[SYS_numvp]     sys_numvp,` exists in array
2. Count array entries - should have exactly 25 (if starting at 1)
3. Check that array indices match syscall.h defines

### Issue: "getptsize() = 1" (seems wrong)

**Cause**: countpagepages() not counting page tables correctly

**Fix**:
1. Check vm.c function: should start count at 1
2. Loop through all NPDENTRIES entries
3. Check PTE_P flag on pgdir entries (not on actual data)

### Issue: Compilation says "unknown function SYSCALL"

**Cause**: usys.S syntax wrong

**Fix**:
1. Look at existing SYSCALL usage in usys.S
2. Follow exact same format
3. Make sure no typos in macro name

### Issue: memtest.c won't compile

**Cause**: Wrong includes or syntax error

**Fix**:
1. Verify includes: types.h, stat.h, user.h, fcntl.h
2. Check printf syntax: `printf(1, "format", args)`
3. Verify malloc returns char* not void*

## Performance Verification

After successful compilation and runtime testing:

- [ ] No kernel panics during memtest
- [ ] No hang (memtest completes in < 5 seconds)
- [ ] All three syscalls work consistently
- [ ] Can run memtest multiple times without issues
- [ ] Other xv6 programs still work (ls, cat, etc.)

## Final Checklist

Before declaring success:

- [ ] All 8 files modified/created
- [ ] Compilation completes without errors or warnings (except for pre-existing ones)
- [ ] memtest runs and shows output
- [ ] Values make sense (positive, reasonable ranges)
- [ ] Can run memtest multiple times
- [ ] Other xv6 programs still work
- [ ] Read MEMORY_LAYOUT.md to understand the output
- [ ] Tested allocation scenarios (malloc before/after)
- [ ] Understand relationship between VP, PP, and PTSIZE

## Documentation Review

- [ ] Read and understand IMPLEMENTATION_GUIDE.md
- [ ] Read and understand MEMORY_LAYOUT.md
- [ ] Read and understand CODE_REFERENCE.md
- [ ] Understand why each function is implemented the way it is
- [ ] Can explain what each syscall returns
- [ ] Can explain what each helper function does
- [ ] Understand xv6 page table structure

## Learning Outcomes

After successful implementation, you should understand:

- [ ] How xv6 system calls work
- [ ] How page tables are structured in x86
- [ ] Relationship between virtual and physical memory
- [ ] How to add new syscalls to xv6
- [ ] How malloc affects address space
- [ ] Purpose of stack guard page
- [ ] How to walk page tables programmatically
- [ ] Page table entry flags (PTE_P, etc.)

