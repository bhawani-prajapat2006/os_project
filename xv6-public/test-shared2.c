#include "types.h"      
#include "user.h"

int main(int argc, char *argv[]) 
{ 
  uint *ap = (uint *) mapshared();
  int pid = fork();

  if(pid == 0) {
    sleep(2); //wait for parent to write
    uint *ac = (uint *) getshared();
    printf(1, "child %d\n", *ac);
    *ac = 53;
    sleep(10);
    printf(1, "child again %d\n", *ac);
    *ac = 54;
    sleep(20); //sleep to give parent time to unmap
  }

  else {
    *ap = 42;
    sleep(5); //wait for child to reply
    printf(1, "parent %d\n", *ap);
    *ap = 43; //write again
    sleep(10);
    printf(1, "parent again %d\n", *ap);
    if(unmapshared() < 0)
      printf(1, "could not unmap shared page\n");
    wait();
  }
  
  exit();
}
