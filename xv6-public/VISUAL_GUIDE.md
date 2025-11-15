# Visual Guide: Page Table Structures and Counting

## What is numvp()? (Virtual Pages)

### Visual Example: Simple Process

```
Scenario: Process with 4KB code + 4KB data (sz = 8192 bytes)

Virtual Address Space:
┌─────────────────────────────────────────┐
│                                         │
│            KERNEL (0x80000000+)         │  ← KERNBASE
│                                         │
├─────────────────────────────────────────┤
│                                         │
│             (unmapped gap)              │
│                                         │
├─────────────────────────────────────────┤
│ p->sz + 4096: 0x3000                    │  ← Stack Guard Page (VIRTUAL)
│              (NOT MAPPED - not in PT)   │    NOT counted by numpp()
├─────────────────────────────────────────┤    BUT counted by numvp()
│ p->sz: 0x2000                           │  ← End of data
│      (this is where stack/heap starts)  │
├─────────────────────────────────────────┤
│ 0x1000 - 0x1FFF: DATA (4KB)             │  ← Physical page mapped
├─────────────────────────────────────────┤
│ 0x0000 - 0x0FFF: CODE (4KB)             │  ← Physical page mapped
└─────────────────────────────────────────┘

Calculation:
  p->sz = 8192 bytes = 2 pages
  PGROUNDUP(8192) = 8192 (already aligned)
  numvp = 8192 / 4096 + 1 = 2 + 1 = 3

Result: numvp() returns 3
  - Page 0 (code) - VIRTUAL and PHYSICAL
  - Page 1 (data) - VIRTUAL and PHYSICAL  
  - Page 2 (guard) - VIRTUAL only, NOT PHYSICAL
```

### Key Point About Stack Guard

```
Guard Page Purpose:

WITHOUT Guard Page:
  [Heap] ← sbrk() expands heap here
    ↑
  [Stack] ← stack grows down
  
  If stack overflows:
    Silently overwrites heap
    Subtle corruption, very hard to debug!

WITH Guard Page:
  [Heap]
    ↑
  [UNMAPPED PAGE] ← Guard page
    ↑
  [Stack]
  
  If stack overflows:
    Immediately hits unmapped page
    Page fault! Program crashes with clear error
    Stack overflow caught early!

The guard page is:
  - Allocated virtually (counts toward address space)
  - Not allocated physically (saves RAM)
  - Not accessible (read or write causes fault)
```

### Formula for numvp()

```
numvp() = (PGROUNDUP(p->sz) / PGSIZE) + 1
            └──────────┬──────────────┘   └─ Stack guard
                      ├─ Rounds up to page boundary
                      └─ Divide by 4096 to get pages
```

## What is numpp()? (Physical Pages)

### Visual Example: Page Table Walking

```
Process Page Directory (at some physical address, e.g., 0x1000):

pgdir[0]:   0x2000 | PTE_P=1 | PTE_U=1
            │
            └────→ Page Table 0 (points to physical page 0x2000)

Page Table 0:
  Entry 0: 0x10000 | PTE_P=1  ← Physical page 0x10000 has CODE
  Entry 1: 0x11000 | PTE_P=1  ← Physical page 0x11000 has DATA
  Entry 2: 0x00000 | PTE_P=0  ← No physical page (guard page)
  Entry 3: 0x00000 | PTE_P=0
  ...
  Entry 1023: (empty)

numpp() Counting Process:

for (a = 0x0; a < p->sz; a += 4096) {
  pte = walkpgdir(pgdir, a, 0)
  if (pte != 0 && (*pte & PTE_P) != 0) {
    count++  ← Increment for each present page
  }
}

Walkthrough:
  a = 0x0000: pte points to page table entry 0
              Entry 0: 0x10000 | PTE_P=1  → COUNT IT (count=1)
  
  a = 0x1000: pte points to page table entry 1
              Entry 1: 0x11000 | PTE_P=1  → COUNT IT (count=2)
  
  a = 0x2000: Stop! (a >= p->sz which is 0x2000)

Result: numpp() returns 2 (only pages with PTE_P set)
```

### Important Distinction

```
numvp() vs numpp():

Virtual Pages (numvp()):
  ✓ Include unmapped guard page
  ✓ PGROUNDUP(sz) / PGSIZE + 1
  ✓ Logical address space
  
Physical Pages (numpp()):
  ✗ Exclude guard page (PTE_P not set)
  ✓ Count only PTE_P=1 entries
  ✓ Actual RAM allocated
  
In xv6 initially:
  numvp() = numpp() (because no demand paging)
  
With demand paging (later):
  numvp() > numpp() (some pages not paged in yet)
```

## What is getptsize()? (Page Table Size)

### Visual Example: Page Table Structure

```
Complete Page Table Hierarchy:

Memory Layout (32-bit x86):
┌─────────────────────────────────────────┐
│  Page Directory (1 page = 4096 bytes)   │  ← This page gets counted
│  1024 entries, each 4 bytes             │
│  [pde_0][pde_1][pde_2]...[pde_1023]    │
└─────────────────────────────────────────┘
         │        │       │
         ├─→ [Page Table 0]  ← This page gets counted
         ├─→ [Page Table 1]  ← This page gets counted
         └─→ [Page Table 2]  ← This page gets counted

Specific Example (small process):

pgdir (counts as 1 page):
 [0] → points to PT0 (valid entry, PTE_P=1)
 [1] → points to PT1 (valid entry, PTE_P=1)
 [2] → NULL (invalid entry, PTE_P=0)
 [3-1023] → NULL (invalid entries, PTE_P=0)

PT0 (counts as 1 page):
 [0-1023] → Page table entries for virtual addresses 0x0 - 0x3FFFFF

PT1 (counts as 1 page):
 [0-1023] → Page table entries for virtual addresses 0x400000 - 0x7FFFFF

PT2 might not exist yet (if not allocated)

Total page table size: 1 (pgdir) + 2 (PT0, PT1) = 3 pages
```

### Counting Algorithm

```
countpagepages(pgdir):

count = 1  ← Start with page directory itself

for (i = 0; i < NPDENTRIES; i++) {  ← NPDENTRIES = 1024
  if (pgdir[i] & PTE_P) {           ← Check present bit on PDE
    count++                         ← Each valid entry = 1 page table
  }
}

return count

Visual Walkthrough:

Loop iteration 0: pgdir[0] & PTE_P = 1 (has PT)     → count = 2
Loop iteration 1: pgdir[1] & PTE_P = 1 (has PT)     → count = 3
Loop iteration 2: pgdir[2] & PTE_P = 0 (no PT)      → count = 3
Loop iteration 3: pgdir[3] & PTE_P = 0 (no PT)      → count = 3
...
Loop iteration 1023: pgdir[1023] & PTE_P = 0 (no PT) → count = 3

Result: getptsize() = 3
```

### Memory Efficiency

```
Page Table Memory Usage:

Small process (1-4MB):
  Needs: 1 page directory + 1 page table
  Size: 2 pages × 4KB = 8 KB
  Efficiency: 8KB overhead for up to 4MB address space
  
Medium process (4-1024MB):
  Needs: 1 page directory + multiple page tables
  Example: 1 pgdir + 100 PT pages = 101 pages
  Size: 101 × 4KB = 404 KB
  
Kernel space:
  Needs: 1 page directory + ~768 page tables (for 3GB kernel space)
  Size: 768 KB just for page table structure
```

## Dynamic Behavior: Before and After malloc()

### Scenario 1: Initial Process

```
Initial State (after exec, before any malloc):

Virtual Addresses:
  0x0 - 0x0FFF: CODE (from executable)
  0x1000 - 0x1FFF: DATA (from executable)
  (rest unmapped until stack)

Page Directory:
  pgdir[0] → points to PT0 (for 0x0 - 0x3FFFFF)
  pgdir[1-1023] → invalid

numvp() = PGROUNDUP(0x2000) / 4096 + 1 = 2 + 1 = 3
numpp() = 2 (code + data pages have PTE_P set)
getptsize() = 2 (1 pgdir + 1 PT)
```

### Scenario 2: After malloc(8192)

```
After malloc(8192):

Virtual Addresses:
  0x0 - 0x0FFF: CODE
  0x1000 - 0x1FFF: DATA
  0x2000 - 0x2FFF: MALLOC SPACE (new page)
  0x3000 - 0x3FFF: MALLOC SPACE (new page)
  (rest unmapped until stack)

Process size now: p->sz = 0x4000 (16384 bytes)

Page Directory:
  pgdir[0] → points to PT0 (same as before)

PT0 now has:
  Entry 0: 0x10000 | PTE_P=1 (CODE)
  Entry 1: 0x11000 | PTE_P=1 (DATA)
  Entry 2: 0x12000 | PTE_P=1 (MALLOC 1)
  Entry 3: 0x13000 | PTE_P=1 (MALLOC 2)
  Entry 4-1023: 0x00000 | PTE_P=0

numvp() = PGROUNDUP(0x4000) / 4096 + 1 = 4 + 1 = 5
numpp() = 4 (4 pages with PTE_P set)
getptsize() = 2 (still 1 pgdir + 1 PT, didn't need new PT)
```

### Scenario 3: After Many malloc() Calls (4MB+ total)

```
After malloc() for > 4MB total:

Virtual Addresses now span across multiple 4MB regions:
  0x0 - 0x3FFFFF: Region 0 (covered by PT0)
  0x400000 - 0x7FFFFF: Region 1 (covered by PT1, if allocated)

Page Directory now has:
  pgdir[0] → points to PT0 (for 0x0 - 0x3FFFFF)
  pgdir[1] → points to PT1 (for 0x400000 - 0x7FFFFF)
  pgdir[2-1023] → invalid

PT0 and PT1 each have many PTE_P=1 entries

numvp() = larger (more virtual pages)
numpp() = larger (more physical pages)
getptsize() = 3 (1 pgdir + 2 PTs)
```

## Comparison Table

```
┌─────────────────┬─────────────────┬──────────────────┬──────────────┐
│ Measurement     │ Counts What     │ Example Value    │ Notes        │
├─────────────────┼─────────────────┼──────────────────┼──────────────┤
│ numvp()         │ Virtual pages   │ 3                │ Includes     │
│ (Virtual Pages) │ including guard │ (code+data+guard)│ guard page   │
│                 │ page            │                  │              │
├─────────────────┼─────────────────┼──────────────────┼──────────────┤
│ numpp()         │ Physical pages  │ 2                │ Only pages   │
│ (Physical Pages)│ with PTE_P set  │ (code+data only) │ actually     │
│                 │                 │                  │ allocated    │
├─────────────────┼─────────────────┼──────────────────┼──────────────┤
│ getptsize()     │ Page table      │ 2                │ pgdir (1) +  │
│ (PT Size)       │ pages including │ (1 pgdir +       │ page tables  │
│                 │ directory       │ 1 PT)            │ (1)          │
└─────────────────┴─────────────────┴──────────────────┴──────────────┘
```

## Expected Ranges

```
Typical Values for xv6 Processes:

Small shell command:
  numvp() = 3-5 pages
  numpp() = 3-5 pages
  getptsize() = 2 pages

Medium program:
  numvp() = 10-50 pages
  numpp() = 10-50 pages
  getptsize() = 2-3 pages

Large program:
  numvp() = 100+ pages
  numpp() = 100+ pages
  getptsize() = 3+ pages

Absolute limits (32-bit):
  numvp() ≤ (KERNBASE / PGSIZE) = 3GB / 4KB ≈ 786,432 pages
  numpp() ≤ PHYSTOP / PGSIZE = 224MB / 4KB ≈ 57,344 pages
  getptsize() ≤ NPDENTRIES + 1 = 1025 pages
               (but usually much less)
```

## Stack Guard Page in Action

```
Memory Layout with Guard Page:

High Address (0xFFFFFFFF)
    │
    ├─ Kernel Space (0x80000000+)
    │
    ├─ Unmapped Region
    │
    ├─ Address: p->sz + 4096
    ├─ [GUARD PAGE - UNMAPPED]  ← numpp() doesn't count this
    ├─ [               ]         ← numvp() DOES count this
    │
    ├─ Address: p->sz
    ├─ [Stack starts here, grows downward]
    ├─ [Stack Frame 1]
    ├─ [Stack Frame 2]
    ├─ [Stack usage pattern ↓]
    │
    ├─ [Heap, grows upward ↑]
    ├─ [Data + BSS]
    ├─ [Code]
    │
Low Address (0x00000000)

Stack Overflow Test:
  void recursive(int n) {
    char buffer[4096];
    if (n > 0) recursive(n-1);  // Keep growing stack
  }
  
  After many recursive calls:
    - Stack grows down toward guard page
    - Hits unmapped guard page
    - Segmentation fault (as intended!)
    - Program terminates cleanly
```

## Debugging Table

```
If you see unexpected values:

Value too LOW:
  numvp() < 1?
    → Process size calculation wrong
    → Check p->sz is correctly set
  
  numpp() < 1?
    → No pages marked present in page table
    → Check walkpgdir() returns valid pointers
    → Check PTE_P flag checking logic
  
  getptsize() < 2?
    → Not counting page directory
    → Or loop through PDEs not working
    → Check NPDENTRIES value

Value too HIGH:
  numvp() > 3GB worth of pages?
    → Calculation overflow or wrong
    → Check for off-by-one errors
  
  numpp() > numvp()?
    → Physical can't exceed virtual!
    → Check PTE_P flag isn't being confused
    → Check boundary checks (a < sz < KERNBASE)
  
  getptsize() > 1024?
    → Can't have more than 1024 page tables
    → Check loop bounds (i < NPDENTRIES)
    → Check PTE_P flag logic

Value changes unexpectedly:
  Call syscall twice and get different values?
    → Something is modifying process memory
    → Stack or heap activity between calls
    → Normal if malloc() or sbrk() called
```

