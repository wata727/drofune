#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>
#include "run.h"

const char *self;

struct context {};
static struct context context;

static struct argp_option options[] = {};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
  switch (key) {
    case ARGP_KEY_NO_ARGS:
      fprintf(stderr, "%s: must have command\n", self);
      exit(1);
    default:
      return ARGP_ERR_UNKNOWN;
  }
}

static char args_doc[] = "COMMAND [OPTION...]";

static char doc[] =                                     \
  "\nCOMMANDS:\n"                                       \
  "\trun     - run a new container\n"                   \
  "\texec    - exec a command in a running container\n" \
  ;

static struct argp argp = { options, parse_opt, args_doc, doc };

int main(const int argc, char **argv) {
  self = argv[0];
  int cmd_idx;

  argp_parse(&argp, argc, argv, ARGP_IN_ORDER, &cmd_idx, &context);

  const char *command = argv[cmd_idx];
  if (strcmp(command, "run") == 0) {
    return run();
  } else if (strcmp(command, "exec") == 0) {
    return 0;
  }

  fprintf(stderr, "%s: unknown command '%s'\n", self, command);
  return 1;
}
