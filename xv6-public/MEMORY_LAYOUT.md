# xv6 Memory Layout and Page Table Structure

## Process Address Space Layout

```
Virtual Address Space (32-bit):

0xFFFFFFFF  +----------------------------+
            |   Device Space             |  (0xFE000000 onwards)
            |   (I/O, LAPIC, etc)        |
            +----------------------------+

0xE0000000  +----------------------------+
            |   Kernel Data & Heap       |  (Shared with all processes)
            +----------------------------+

0x80000000  +----------------------------+  KERNBASE
            |   Kernel Text & RO Data    |  (Shared with all processes)
            +----------------------------+

            |   (unmapped gap)           |

p->sz + 1   +----------------------------+  <-- Stack Guard Page
p->sz       +----------------------------+  <-- Top of Heap / Bottom of Stack
            |   STACK (grows downward)   |
            |   (expandable)             |
            |   ~~~~~~~~~~              |
            |   (heap grows upward)      |
            |   HEAP                     |
            +----------------------------+
            |   BSS + initialized data   |
            +----------------------------+
            |   Program text (.text)     |
            +----------------------------+
0x00000000  (start of user space)

```

## numvp() - Virtual Pages Count

Example: Process with sz = 8192 bytes (2 pages)

```
Virtual Pages Calculation:
- Process size: p->sz = 8192 bytes
- Rounded up: PGROUNDUP(8192) = 8192 (already page-aligned)
- Number of pages: 8192 / 4096 = 2 pages
- Add guard page: 2 + 1 = 3 virtual pages

Pages in address space:
  [0x0000 - 0x0FFF]        Page 0 - CODE
  [0x1000 - 0x1FFF]        Page 1 - DATA
  [0x2000 - 0x2FFF]        Page 2 - STACK GUARD (not mapped)
  
Result: numvp() = 3
```

## numpp() - Physical Pages Count

Walking the page table:

```
Process page directory (pgdir) at physical address:

pgdir[0]  ---> Physical Page Table 0
         |
         +---> Entry 0: PTE_P=1 --> Physical page 0x1000 (code)
         +---> Entry 1: PTE_P=1 --> Physical page 0x2000 (data)
         +---> Entry 2: PTE_P=0 --> NULL (guard page, not mapped)

Result: numpp() = 2 (two pages with PTE_P flag set)
```

### Page Table Walking Logic

```
for each virtual address a from 0 to p->sz:
  pte = walkpgdir(pgdir, a, 0)     // Get PTE for address a
  if (pte is valid and *pte & PTE_P):
    count++
    
   a += PGSIZE (move to next page)
```

## getptsize() - Page Table Pages Count

Page Directory structure:

```
Page Directory (1 page = 4KB):
+-------+--------+---+-----+---+
|  ...  | pgdir  | . | ... | . |  (1024 entries, each 4 bytes)
+-------+--------+---+-----+---+
  PDX=0   PDX=1   ... PDX=k ... (k = 1023)

Each PDE (Page Directory Entry):
- If (PDE & PTE_P): points to a Page Table page (count it)
- Else: no page table allocated for this range

Example: 
Process with 2 pages of user memory might need:
- 1 page directory
- 1 page table (can hold up to 1024 PTEs)
- Total: 2 pages for page table structure
```

### Counting Page Table Pages

```
countpagepages(pgdir):
  count = 1  // Start with page directory
  
  for i = 0 to 1023:  // NPDENTRIES = 1024
    if (pgdir[i] & PTE_P):  // If page table exists
      count++
      
  Result: count = 1 + number_of_allocated_page_tables
```

## PTE (Page Table Entry) Structure

32-bit x86 PTE:

```
31              12  11 10 9 8 7 6 5 4 3 2 1 0
+------------------+-----+---------+---------+
| Physical Page    |     | Unused  | Flags   |
| Address (20 bits)| U/S |         | P W U D |
+------------------+-----+---------+---------+

Flags:
- P (0x001): Present (1 = page is in memory)
- W (0x002): Writable (1 = can write to page)
- U (0x004): User-accessible (1 = user mode can access)
- D (0x008): Dirty (1 = page has been written)
- others: Not used in xv6
```

## Example: Process Creation and Memory Layout

### Initial state (fresh process):

```
Step 1: Process created with exec()
  - pgdir: new page directory allocated
  - sz: set to size of executable
  
Step 2: User code loaded
  - Pages for .text and .data mapped
  - walkpgdir() creates page table entries
  
Step 3: Stack initialized
  - Top of memory reserved for stack
  - Guard page at p->sz (left unmapped)
  
numvp() call:
  Count = PGROUNDUP(p->sz) / PGSIZE + 1
  
numpp() call:
  Walk pgdir, count all PTEs with PTE_P set
  Should equal numvp() initially
  
getptsize() call:
  Count = 1 (pgdir) + number of page tables
```

### After malloc():

```
Step 1: malloc(4096) called
  - growproc(4096) called
  - allocuvm() allocates new page
  - Page table entries created with PTE_P set
  - p->sz increased
  
Results:
  numvp() = higher (more virtual pages)
  numpp() = higher (more physical pages)
  getptsize() = might be same or higher
```

## Key Constants in xv6

```c
// From param.h
KSTACKSIZE = 4096      // Kernel stack per process (1 page)

// From mmu.h
PGSIZE = 4096          // Bytes per page
NPDENTRIES = 1024      // # entries in page directory
NPTENTRIES = 1024      // # PTEs per page table
PTE_P = 0x001          // Present bit
PTE_W = 0x002          // Writable bit
PTE_U = 0x004          // User bit
PTXSHIFT = 12          // Bit shift for page table index
PDXSHIFT = 22          // Bit shift for page directory index

// From memlayout.h
KERNBASE = 0x80000000  // Start of kernel virtual address
EXTMEM = 0x100000      // Start of extended memory
PHYSTOP = 0xE000000    // Top of physical memory

// Macros
PGROUNDUP(x)   = ((x) + PGSIZE - 1) & ~(PGSIZE - 1)
PGROUNDDOWN(x) = (x) & ~(PGSIZE - 1)
PDX(va)        = ((va) >> PDXSHIFT) & 0x3FF
PTX(va)        = ((va) >> PTXSHIFT) & 0x3FF
PTE_ADDR(pte)  = (pte) & ~0xFFF
```

## Relationship Between numvp(), numpp(), and getptsize()

```
Scenario 1: Initial Process (no demand paging)
  numvp() = numpp()        (virtual = physical)
  getptsize() = 1 or 2     (depends on process size)

Scenario 2: After malloc()
  numvp() = numpp()        (still equal, no demand paging)
  getptsize() = might stay same (if fits in existing tables)

Scenario 3: With demand paging (future enhancement)
  numvp() > numpp()        (more virtual than physical)
  Some pages in PT not allocated yet

Scenario 4: After fork()
  Child gets copy of parent's pages
  numvp(child) ≈ numvp(parent)
  getptsize(child) ≈ getptsize(parent)
```

## Common Page Table Scenarios

### Small process (< 4MB):
```
Virtual Addresses:
0x0 - 0xFFFFF: User space (1 entry in pgdir points to 1 page table)

pgdir structure:
[0] ---> Page Table 0 (handles 0x0 - 0x3FFFFF)
[1-1023] ---> (empty)

Result: getptsize() = 2 (1 pgdir + 1 page table)
```

### Medium process (4MB - 8MB):
```
pgdir structure:
[0] ---> Page Table 0
[1] ---> Page Table 1
[2-1023] ---> (empty)

Result: getptsize() = 3 (1 pgdir + 2 page tables)
```

### Large process (approaching 3GB limit):
```
pgdir structure:
[0-767] ---> Page Tables (handles up to 3GB)
[768-1023] ---> Kernel space (KERNBASE at 0x80000000)

Result: getptsize() = 768 or more
```

## Stack Guard Page Purpose

```
Normal stack:
  Top at p->sz (mapped, RW)
  Guard page at p->sz + 1 (NOT mapped)
  
If buffer overflow occurs:
  try to write to p->sz + 4092 (still in guard page range)
  Page fault! Process crashes with clear error
  
Without guard page:
  overflow writes to heap
  Subtle corruption, hard to debug
```

## Testing Checklist

- [ ] numvp() returns value ≥ 1
- [ ] numpp() returns value ≥ 1
- [ ] getptsize() returns value ≥ 2 (pgdir + at least 1 page table)
- [ ] numvp() ≥ numpp() (or equal with no demand paging)
- [ ] After malloc, at least one of the values increases
- [ ] After fork, child has similar values to parent
- [ ] Multiple calls return same value (stable)
- [ ] Very large malloc might increase all values

