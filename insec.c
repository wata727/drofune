#include <stdio.h>
#include <string.h>
#include "run.h"

const char *self;

int main (const int argc, const char **argv) {
  self = argv[0];
  if (argc < 2) {
    fprintf(stderr, "%s: must have command\n", self);
    return 1;
  }

  const char *command = argv[1];
  if (strcmp(command, "run") == 0) {
    return run();
  } else if (strcmp(command, "exec") == 0) {
    return 0;
  }

  fprintf(stderr, "%s: unknown command `%s`\n", self, command);
  return 1;
}
