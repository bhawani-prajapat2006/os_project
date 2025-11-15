# FAQ - Frequently Asked Questions

## Understanding the Task

### Q1: What is a system call?
**A**: A system call is a request from user-space program to the kernel to perform a privileged operation. In xv6, syscalls are invoked with the INT instruction and dispatched based on a number stored in EAX.

### Q2: Why are we implementing three separate syscalls?
**A**: Each syscall serves a different purpose:
- **numvp()**: Measures virtual address space (what the process claims)
- **numpp()**: Measures physical memory usage (what actually exists in RAM)
- **getptsize()**: Measures page table overhead (how much memory is used for bookkeeping)

### Q3: What's the difference between virtual and physical pages?
**A**: 
- **Virtual pages**: Logical address space the process sees (0 to KERNBASE)
- **Physical pages**: Actual RAM pages allocated to the process
- Without demand paging: VP = PP (usually)
- With demand paging: VP > PP (some pages not yet paged in)

### Q4: What is a page?
**A**: A page is a fixed-size unit of memory (4096 bytes in xv6). Virtual addresses are divided into page-sized chunks for mapping to physical memory.

---

## Implementation Questions

### Q5: How do I add a new system call?
**A**: Four steps:
1. Add syscall number to syscall.h
2. Write handler function in syscall.c (or sysproc.c)
3. Add extern declaration and register in syscalls array in syscall.c
4. Add declaration to user.h so user programs can call it

### Q6: What does "add syscall stub in usys.S" mean?
**A**: usys.S contains assembly code that generates the INT instruction. The SYSCALL macro creates this stub automatically. You just need to add the macro call like `SYSCALL(numvp)`.

### Q7: Why do we need to modify so many files?
**A**: Each file has a specific role:
- syscall.h: Defines syscall numbers
- sysproc.c: Implements syscall logic
- syscall.c: Registers syscalls
- user.h: Declares syscalls for user programs
- usys.S: Generates INT stubs
- vm.c: Page table helper functions

### Q8: Can I skip any of the files?
**A**: No. Each file is necessary:
- Skip syscall.h: Syscall won't have a number
- Skip sysproc.c: No handler implementation
- Skip syscall.c: Won't be registered in dispatch table
- Skip user.h: User programs can't call it
- Skip usys.S: INT instruction not generated
- Skip vm.c helpers: countppages won't exist

---

## Code Implementation Questions

### Q9: What does walkpgdir() do?
**A**: It walks the page table hierarchy and returns a pointer to the PTE (Page Table Entry) for a given virtual address. Format:
```c
pte_t *pte = walkpgdir(pgdir, virtual_addr, 0);
// Returns: pointer to PTE, or 0 if not found
// Third parameter: 1=allocate if missing, 0=just lookup
```

### Q10: What's the PTE_P flag?
**A**: The Present bit (0x001) in a PTE. If set, the PTE points to a valid physical page. If not set, the page is not currently in memory (or guard page).

### Q11: Why do we add 1 for the stack guard page in numvp()?
**A**: The stack guard page is:
- Located at p->sz (the byte after process size)
- NOT physically allocated (not in RAM)
- Not included in walking the page table
- But still part of the virtual address space (hence counted in numvp)

### Q12: What's the difference between countppages() and countpagepages()?
**A**:
- **countppages()**: Counts USER PAGES that are physically allocated
  - Walks address space from 0 to p->sz
  - Counts PTEs with PTE_P set
  
- **countpagepages()**: Counts PAGE TABLE PAGES
  - Walks page directory entries
  - Counts PDEs with PTE_P set
  - Returns 1 (directory) + number of page tables

### Q13: How many pages can a page table handle?
**A**: One page table page can handle 1024 PTEs (page table entries), which maps 1024 × 4096 = 4MB of address space.

---

## Testing Questions

### Q14: What output should I expect?
**A**: For a typical small program:
```
numvp: 2-3        (small initial program)
numpp: 2-3        (same, since xv6 has no demand paging)
getptsize: 2      (1 page directory + 1 page table)
```

After malloc(4096):
```
numvp: 3-4        (increased)
numpp: 3-4        (increased)
getptsize: 2      (same, if fits in existing table)
```

### Q15: Why might getptsize() not change after malloc?
**A**: If the malloc'd page fits in the existing page table (same 4MB region), no new page table page needs to be allocated. Getptsize only increases when we need to allocate a new page table page.

### Q16: What if the values seem wrong?
**A**: Possible causes:
- PTE_P flag checking is wrong
- Loop boundaries incorrect (should stop at KERNBASE)
- PGROUNDUP calculation wrong
- off-by-one error in counting

### Q17: Can I test each syscall individually?
**A**: Yes. Create separate test programs:
```c
// test_numvp.c
#include "user.h"
int main() {
  printf(1, "numvp: %d\n", numvp());
  exit();
}
```

---

## Compilation Questions

### Q18: What does "undefined reference to 'sys_numvp'" mean?
**A**: The compiler can find the declaration but not the definition. Causes:
- Missing `extern int sys_numvp(void);` in syscall.c
- Missing implementation in sysproc.c
- Typo in function name (doesn't match)

### Q19: What does "numvp: not found" mean (in shell)?
**A**: The syscall exists but user program can't call it. Causes:
- Missing `int numvp(void);` in user.h
- Missing `SYSCALL(numvp)` in usys.S
- Typo in name

### Q20: How do I debug compilation errors?
**A**: 
```bash
make clean
make 2>&1 | tee build.log
grep error build.log
# Shows file:line: error: message
```

Then look at that line and surrounding context.

### Q21: Should I recompile after every change?
**A**: Yes. Make tracks dependencies, so:
```bash
make        # Only recompiles changed files
make clean  # Start fresh if confused
make        # Full rebuild
```

---

## Debugging Questions

### Q22: How do I add debug output?
**A**: Use cprintf() in kernel code, printf() in user code:

```c
// In kernel (sysproc.c, vm.c):
cprintf("numvp: calculated as %d\n", num_pages);

// In user program (memtest.c):
printf(1, "About to call numvp()\n");
```

### Q23: My syscall returns -1 (error). Why?
**A**: Possible causes:
- Function returns -1 by mistake
- Syscall number not registered
- Syscall array bounds exceeded
- Function crashes before returning

Check:
1. Does your function return an int?
2. Is it registered in syscalls array at correct index?
3. Does the array have enough space?

### Q24: My program crashes (segfault). Why?
**A**: Possible causes:
- Dereferencing null pointer (pte == 0)
- Array index out of bounds
- Invalid memory access

Always check for null pointers:
```c
if (pte != 0 && (*pte & PTE_P) != 0) {
  // pte is valid, safe to dereference
}
```

### Q25: Values change between calls. Is that normal?
**A**: Depends:
- **Normal**: If malloc() or sbrk() called between calls
- **Abnormal**: If called twice with no memory allocation
  - Could indicate bug in counting
  - Or system activity modifying process

---

## Conceptual Questions

### Q26: Why is there a stack guard page?
**A**: To detect stack overflow:
- If stack grows too much, it hits the unmapped page
- Causes immediate page fault
- Program crashes with clear error
- Without it: stack overwrites heap (hard to debug)

### Q27: Why count page table pages separately?
**A**: To measure kernel bookkeeping overhead:
- User is interested in: "How much memory does my program use?"
- This includes: user pages + page table pages
- getptsize() lets you measure overhead
- Useful for optimization and understanding

### Q28: Why do we need both numvp() and numpp()?
**A**: For different purposes:
- **numvp()**: Maximum committed address space
- **numpp()**: Actual RAM in use
- Difference allows studying demand paging (future)
- Useful for memory management analysis

### Q29: What is KERNBASE?
**A**: The start of kernel virtual address space (0x80000000). All addresses at or above this are kernel space. User programs should never see data there, so we stop counting at KERNBASE.

### Q30: What is PGSIZE?
**A**: Page size (4096 bytes, 4KB). All memory operations happen on page boundaries. PGSIZE is used for alignment and counting.

---

## Advanced Questions

### Q31: What about future demand paging?
**A**: Once implemented:
- User pages might not be physically present yet
- numvp() would stay same
- numpp() would be less than numvp()
- Your code should already support this!

### Q32: Can I optimize page table walking?
**A**: Technically yes, but:
- Current implementation is simple and correct
- Optimization would require careful thought
- Better to get it working first

### Q33: What if I want to count kernel pages too?
**A**: Task says "user part of address space", so:
- Stop at KERNBASE (don't count kernel pages)
- Task specifically asks for this

### Q34: Can syscalls fail?
**A**: Yes, they can return -1 on error:
- Our syscalls shouldn't fail (just count)
- But other syscalls (open, read) return -1 on error
- User program should check return value

### Q35: How do syscalls handle invalid arguments?
**A**: Use argint(), argstr(), argptr() to fetch arguments:
```c
int sys_mysyscall(void) {
  int arg1;
  char *arg2;
  
  if (argint(0, &arg1) < 0)
    return -1;  // Invalid argument
  if (argstr(1, &arg2) < 0)
    return -1;  // Invalid string
    
  // Use arg1 and arg2
}
```

Our syscalls take no arguments, so no validation needed.

---

## Performance Questions

### Q36: Are these syscalls slow?
**A**: No:
- numvp(): Simple arithmetic, O(1)
- numpp(): Walks all user pages, O(p->sz)
- getptsize(): Walks page directory (1024 entries), O(1)

For typical xv6 processes, all are very fast.

### Q37: Should I cache the results?
**A**: No:
- Results can change after malloc
- Caching would give stale values
- Kernel should compute fresh values each time

---

## Integration Questions

### Q38: Will my syscalls break other programs?
**A**: No:
- You're only adding new syscalls
- Existing syscall numbers unchanged
- Existing programs unaffected

### Q39: Can I call other syscalls from mine?
**A**: Theoretically yes, but:
- Our syscalls don't need to
- Generally not recommended
- Could cause issues if not careful

### Q40: What happens if program calls invalid syscall?
**A**: In syscall handler:
```c
if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
  // Valid syscall
} else {
  cprintf("unknown sys call\n");
  curproc->tf->eax = -1;  // Return -1
}
```

Invalid syscalls return -1.

---

## Common Pitfalls

### Q41: "off-by-one error" - What's that?
**A**: Counting one too many or one too few:
- PGROUNDUP(x)/PGSIZE might be off-by-one
- Loop boundary might be off-by-one
- Array indexing might be off-by-one
- Always verify edge cases

### Q42: "Loops forever" - Why?
**A**: Infinite loop causes:
- Loop condition always true
- a += PGSIZE but a never reaches sz
- Boundary condition never met

Fix: Verify loop bounds and increment.

### Q43: "Returns wrong value" - How to fix?
**A**: Debug systematically:
1. Add cprintf to show intermediate values
2. Trace through algorithm by hand
3. Compare with expected value
4. Fix the discrepancy

### Q44: "Works sometimes, fails other times" - What's going on?
**A**: Race condition or uninitialized value:
- Process state changing between calls
- Uninitialized variables
- Timing dependent behavior

Fixes: Initialize variables, trace carefully.

### Q45: "Crashes after a while" - Why?
**A**: Memory corruption or stack overflow:
- Off-by-one error corrupting memory
- Array bounds exceeded
- Stack smashed by recursive loop

Fixes: Add bounds checking, verify array sizes.

---

## Getting Help

### Q46: Where's the xv6 documentation?
**A**: In the xv6 book:
- Available online free
- Ch. 2 (Virtual Memory) most relevant
- Also see comments in xv6 source code

### Q47: How do I read error messages?
**A**: Compiler error format:
```
filename.c:line_number: error: message
```

- Go to that file and line
- Read message carefully
- Usually very descriptive

### Q48: Can I look at other syscalls for examples?
**A**: Yes! Examples:
- **sys_sbrk()**: Similar to ours, no argument validation
- **sys_getpid()**: Simple, just returns value
- **sys_fork()**: Complex, shows calling kernel functions

Look in sysproc.c for examples.

### Q49: What if I still can't figure it out?
**A**: 
1. Read the documentation files provided
2. Check CODE_REFERENCE.md for exact code
3. Follow CHECKLIST.md step-by-step
4. Add cprintf debug statements
5. Trace through algorithm manually

### Q50: How do I know when I'm done?
**A**: Success criteria:
- [ ] All three syscalls work
- [ ] memtest produces output
- [ ] Values make sense
- [ ] malloc() changes values
- [ ] Other programs still work
- [ ] You understand the concepts

---

## Final Tips

**Remember**: 
- xv6 is educational - code is simple and readable
- Don't overcomplicate things
- Test incrementally (don't add all at once)
- Use debug output liberally
- Read error messages carefully
- Reference documentation frequently
- Ask yourself: "Does this make sense?"

Good luck with your implementation! 🎉

