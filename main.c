#include <stdio.h>
#include <string.h>

int _start() {
  char source[] =
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

  char destination[201];

  size_t numBytes = strlen(source) + 1;

  memcpy(destination, source, numBytes);

  if (strcmp(source, destination) == 0) {
    printf("Memory copied successfully!\n");
  } else {
    printf("Memory copy failed.\n");
  }

  printf("Destination: %s\n", destination);

  return 0;
}
