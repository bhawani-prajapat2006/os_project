# Memory Mapping (mmap) System Call Implementation for xv6

## Overview
This document describes the implementation of a simple memory mapping (mmap) system call in xv6. The implementation allows processes to expand their virtual address space on demand without immediately allocating physical memory. Physical pages are allocated later when they are actually accessed, triggering a page fault.

## Implementation Summary

### 1. System Call Number
- **File**: `syscall.h`
- **Change**: Added `#define SYS_mmap 26`
- Assigned sequential number following existing system calls

### 2. Page Table Entry Flag
- **File**: `mmu.h`
- **Change**: Added `#define PTE_M 0x100` for marking memory-mapped pages
- This flag enables support for optional extensions where mmap regions can be placed at arbitrary addresses

### 3. Process Structure Extension
- **File**: `proc.h`
- **Change**: Added two fields to `struct proc`:
  ```c
  uint mmap_start;  // Start of memory-mapped region
  uint mmap_end;    // End of memory-mapped region
  ```
- These fields track the virtual address range of memory-mapped memory for each process

### 4. Process Initialization
- **File**: `proc.c`
- **Function**: `allocproc()`
- **Change**: Initialize mmap fields when a new process is allocated:
  ```c
  p->mmap_start = 0;
  p->mmap_end = 0;
  ```

### 5. System Call Implementation
- **File**: `sysproc.c`
- **Function**: `sys_mmap(int n)`
- **Behavior**:
  - Takes one argument: number of bytes to map
  - Returns 0 if input is invalid (non-positive or not page-aligned)
  - Returns 0 if mmap regions are already mapped
  - Expands process size by n bytes WITHOUT allocating physical memory
  - Returns the starting virtual address of the new region
  - Sets `p->mmap_start` and `p->mmap_end` for later page fault handling

### 6. On-Demand Memory Allocation
- **File**: `vm.c`
- **Function**: `allocuvm_ondemand(pde_t *pgdir, uint va)`
- **Purpose**: Allocate and map a single physical page for a given virtual address
- **Called by**: Page fault handler in trap.c
- **Process**:
  1. Round down VA to page boundary
  2. Allocate physical page via `kalloc()`
  3. Clear the page with `memset()`
  4. Map virtual to physical address using `mappages()`
  5. Return 0 on success, -1 on error

### 7. Page Fault Handler
- **File**: `trap.c`
- **Case**: `T_PGFLT` (Page Fault trap)
- **Implementation**:
  - Retrieve faulting virtual address via `rcr2()`
  - Check if address is in the mmap region (`va >= mmap_start && va < mmap_end`)
  - If in mmap region:
    - Call `allocuvm_ondemand()` to allocate physical memory
    - Call `switchuvm()` to update TLB/CR3
    - Return to user code to retry the memory access
  - If not in mmap region:
    - Fall through to default trap handling (kills process)

### 8. System Call Dispatch
- **File**: `syscall.c`
- **Changes**:
  - Added extern declaration: `extern int sys_mmap(void);`
  - Added entry in syscalls table: `[SYS_mmap] sys_mmap,`

### 9. User-Space Interface
- **File**: `user.h`
  - Added function declaration: `int mmap(int);`
- **File**: `usys.S`
  - Added syscall stub: `SYSCALL(mmap)`

### 10. Fork Implementation
- **File**: `proc.c`
- **Function**: `fork()`
- **Change**: Copy mmap region information to child process:
  ```c
  np->mmap_start = curproc->mmap_start;
  np->mmap_end = curproc->mmap_end;
  ```
- The actual memory pages are copied by `copyuvm()` if they have been allocated (present flag set)

### 11. Test Program
- **File**: `mmaptest.c`
- **Added to**: Makefile (UPROGS list)
- **Features**:
  - Tests basic mmap allocation
  - Verifies virtual pages increase without physical page allocation
  - Tests on-demand page allocation via memory access
  - Tests multiple sequential mmap calls
  - Tests error cases (0 bytes, non-page-aligned)
  - Verifies written values are correctly stored and retrieved

## How It Works

### Virtual Address Space Growth (sys_mmap)
```
Before: [heap]-----[stack_guard]
After:  [heap][mmap_region]--[stack_guard]
        ^    ^              ^
        |    mmap_start     mmap_end
```

### Memory Allocation Flow
1. **User calls**: `addr = mmap(4096);`
2. **Kernel increases process size** but does NOT allocate physical memory
3. **Kernel returns** starting address
4. **Virtual page count increases**, physical page count stays same
5. **User accesses** `*(int*)addr = 42;`
6. **Page fault occurs** because page is not present
7. **Page fault handler** allocates one physical page and maps it
8. **Instruction retries** and write succeeds
9. **Physical page count increases** by 1

### On-Demand Allocation Benefits
- Memory is only allocated when actually needed
- Saves physical memory for processes that map but don't use all pages
- Allows mapping of large regions with minimal overhead
- Multiple processes can map different subsets of their regions

## Key Design Decisions

### 1. No PTE_M Flag Required for Basic Version
- The basic implementation doesn't require marking mmap pages with PTE_M
- Pages are identified as memory-mapped by checking against mmap_start/mmap_end ranges
- PTE_M flag is provided for optional extension support

### 2. Single mmap Call Per Process
- The current implementation only allows one mmap region per process
- Check `if(p->mmap_end != 0)` ensures only one call succeeds
- Extension could use a list/array to support multiple regions

### 3. Address Space Limitations
- mmap region is placed right after the program break (p->sz)
- Prevents overlap with stack and kernel space
- Check `p->sz + n >= KERNBASE` prevents kernel space invasion

### 4. Process Boundaries
- Parent's mmap information is copied to child during fork
- Both parent and child have same mmap_start and mmap_end
- Physical pages are only copied if already present in parent

## Testing

### Running the Test Program
```bash
# In xv6 shell
mmaptest
```

### Expected Output
- Virtual pages increase after mmap() call
- Physical pages don't increase until memory is accessed
- Physical pages allocated on-demand as memory is accessed
- Values written to mapped memory are correctly stored and retrieved
- Error cases (0 bytes, non-page-aligned) correctly return 0

## Extension: Support for Arbitrary Address Mapping

The implementation supports an optional extension to map memory at arbitrary addresses:

### Required Changes for Full Extension
1. Add `PTE_M` flag support (already implemented)
2. Accept second argument for start address in sys_mmap
3. Validate address is between program break and KERNBASE
4. Ensure no existing mappings at requested address
5. Modify countppages and countpagepages to count PTE_M pages
6. Modify copyuvm to copy memory-mapped pages from parent

### Benefits
- Allows creating sparse address spaces
- Useful for memory-mapped I/O and large data structures
- More flexible memory management

## Files Modified

1. `syscall.h` - Added SYS_mmap constant
2. `mmu.h` - Added PTE_M flag definition
3. `proc.h` - Added mmap_start and mmap_end fields
4. `proc.c` - Initialize mmap fields in allocproc(), copy in fork()
5. `sysproc.c` - Implemented sys_mmap()
6. `vm.c` - Implemented allocuvm_ondemand()
7. `trap.c` - Added page fault handler for T_PGFLT
8. `syscall.c` - Added sys_mmap to dispatch table
9. `user.h` - Added mmap declaration
10. `usys.S` - Added mmap syscall stub
11. `mmaptest.c` - Created comprehensive test program
12. `Makefile` - Added mmaptest to UPROGS

## Conclusion

This implementation provides a functional memory mapping system call that:
- ✓ Allocates virtual address space on demand
- ✓ Allocates physical memory only when accessed
- ✓ Properly handles page faults
- ✓ Updates TLB after allocation
- ✓ Supports process forking with mmap regions
- ✓ Validates input parameters
- ✓ Provides comprehensive test coverage
