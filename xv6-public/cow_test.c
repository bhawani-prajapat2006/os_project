#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

// Helper
void sep(char *msg) {
  printf(1, "\n===== %s =====\n", msg);
}

//
// TEST 1 — Basic CoW fork
//
void test1() {
  sep("TEST 1: fork uses almost no pages");

  int before = getNumFreePages();
  int pid = fork();

  if(pid == 0) {
    printf(1, "Child sees %d free\n", getNumFreePages());
    exit();
  }

  wait();
  int after = getNumFreePages();

  printf(1, "Free before = %d\n", before);
  printf(1, "Free after  = %d\n", after);

  if(before - after > 20)
    printf(1, "❌ FAIL: fork consumed too many pages\n");
  else
    printf(1, "✅ PASS\n");
}

//
// TEST 2 — Write triggers COW
//
void test2() {
  sep("TEST 2: write triggers COW");

  char *p = sbrk(4096);
  p[0] = 'A';

  int before = getNumFreePages();
  int pid = fork();

  if(pid == 0) {
    int mid = getNumFreePages();
    p[0] = 'B';
    int after = getNumFreePages();
    printf(1, "Child: before=%d mid=%d after=%d\n", before, mid, after);
    exit();
  }

  wait();
  printf(1, "Parent sees free=%d\n", getNumFreePages());
}

//
// TEST 3 — Multi-child COW; verify COW happens but no leaks
//
void test3() {
  sep("TEST 3: multi-fork COW");

  char *p = sbrk(4096);
  p[0] = 1;

  int before = getNumFreePages();
  int n = 5;

  for(int i=0;i<n;i++){
    int pid = fork();
    if(pid == 0){
      int pre = getNumFreePages();
      p[0] = i+2;    // must COW here
      int post = getNumFreePages();
      printf(1, "Child %d: allocated %d pages\n", i, pre - post);
      exit();
    }
  }

  for(int i=0;i<n;i++) wait();

  int after = getNumFreePages();
  printf(1, "Free before=%d after=%d diff=%d\n", before, after, before-after);

  // diff should be 0: no leaks
  if(before == after)
    printf(1, "✅ PASS (COW happened & no leaks)\n");
  else
    printf(1, "❌ FAIL: page leak = %d\n", before - after);
}

//
// TEST 4 — Parent must COW after child COW
//
void test4() {
  sep("TEST 4: parent COW after child COW");

  char *p = sbrk(4096);
  p[0] = 'X';

  int before = getNumFreePages();
  int pid = fork();

  if(pid == 0){
    // child COW
    int pre = getNumFreePages();
    p[0] = 'C';
    int post = getNumFreePages();
    printf(1, "Child COW used %d pages\n", pre-post);
    exit();
  }

  wait();

  // Parent write should COW only if refcount > 1 (rare after child exit).
  int pre = getNumFreePages();
  p[0] = 'P';
  int post = getNumFreePages();

  printf(1, "Parent COW used %d pages\n", pre-post);

  // Now check leak
  int after = getNumFreePages();

  if(before == after)
    printf(1, "✅ PASS (COW correct, no leaks)\n");
  else
    printf(1, "❌ FAIL: leak = %d pages\n", before - after);
}

//
// TEST 5 — Illegal memory access
//
void test5() {
  sep("TEST 5: illegal page fault handling");

  int pid = fork();
  if(pid == 0){
    int *x = (int*)0xFFFFFFFF;
    *x = 10;
    printf(1,"❌ FAIL: child survived illegal write\n");
    exit();
  }

  int ret = wait();
  printf(1, "Parent alive, child exit code=%d\n", ret);
  printf(1, "If parent is alive → PASS\n");
}

//
// TEST 6 — Large COW (10 pages)
//
void test6() {
  sep("TEST 6: large memory region COW");

  int N = 4096 * 10;
  char *p = sbrk(N);

  for(int i=0;i<N;i+=4096) p[i] = 'A';

  int before = getNumFreePages();
  int pid = fork();

  if(pid == 0){
    for(int i=0;i<N;i+=4096){
      int pre = getNumFreePages();
      p[i] = 'B'; // COW
      int post = getNumFreePages();
      printf(1, "Child wrote page: allocated %d\n", pre - post);
    }
    exit();
  }

  wait();

  int after = getNumFreePages();
  printf(1, "Free before=%d after=%d diff=%d\n", before, after, before-after);

  if(before == after)
    printf(1, "✅ PASS (COW correct + no leaks)\n");
  else
    printf(1, "❌ FAIL: leaked %d pages\n", before-after);
}

int main() {
  test1();
  test2();
  test3();
  test4();
  test5();
  test6();

  printf(1,"\nALL TESTS DONE\n");
  exit();
}