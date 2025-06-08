// #include "doom/doomtype.h"
// #include "lib/syscalls.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char sys_char();
void _start() {
  while (true) {
    char key = sys_char();
    if (key == 0)
      continue;
    printf("this is from puts(%c)\n", key);
  }
  printf("hi!\n");
  exit(1);
}
