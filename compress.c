#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <input_file> <bash_file>\n", argv[0]);
    return EXIT_FAILURE;
  }

  const char *input_file = argv[1];
  const char *bash_file = argv[2];
  FILE *file = fopen(input_file, "rb");
  if (file == NULL) {
    perror("Error opening input file");
    return EXIT_FAILURE;
  }

  unsigned char byte;
  size_t index = 0;

  FILE *ofile = fopen(bash_file, "a");
  while (fread(&byte, sizeof(byte), 1, file) == 1) {
    if (byte != 0) {
      char command[50];
      snprintf(command, sizeof(command), "memwritebyte %zu %u", index, byte);
      fprintf(ofile, "%s\n", command);
    }
    index++;
  }

  fclose(file);
  fclose(ofile);
  return EXIT_SUCCESS;
}
