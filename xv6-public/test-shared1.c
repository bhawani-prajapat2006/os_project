#include "types.h"
#include "user.h"

int main(int argc, char *argv[]) 
{ 
  uint *ap = (uint *) mapshared();
  int pid = fork();

  if(pid == 0) {
    sleep(1); //wait for parent
    uint *ac = (uint *) getshared();
    printf(1, "child %d\n", *ac);
    *ac = 53;
    sleep(10);
  }

  else {
    *ap = 42;
    sleep(5);
    printf(1, "parent %d\n", *ap);
    if(unmapshared() < 0)
      printf(1, "could not unmap shared page\n");
    wait();
  }
  
  exit();
}
