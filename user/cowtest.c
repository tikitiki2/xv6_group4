#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096

int
main()
{
  char *p = sbrk(PGSIZE);  // Allocate one page
  if(p == (char*)-1) {
    printf("sbrk failed\n");
    exit(1);
  }

  // Write to the page
  *p = 'A';

  int pid = fork();
  if(pid < 0) {
    printf("fork failed\n");
    exit(1);
  }

  if(pid == 0) {
    // Child process
    printf("Child: original value at p: %c\n", *p);
    *p = 'B';  // This should trigger a page fault and copy-on-write
    printf("Child: new value at p: %c\n", *p);
    exit(0);
  } else {
    // Parent process
    wait(0);
    printf("Parent: value at p: %c\n", *p);  // Should still be 'A'
  }

  exit(0);
} 