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
  malloc(4096);
  
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
  malloc(8192);
  
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
