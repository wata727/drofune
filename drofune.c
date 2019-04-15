#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>
#include "run.h"
#include "exec.h"

const char *self;

struct context {};
static struct context context;

static struct argp_option options[] = {};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
  switch (key) {
    case ARGP_KEY_NO_ARGS:
      fprintf(stderr, "%s: must have commands\n", self);
      exit(1);
    default:
      return ARGP_ERR_UNKNOWN;
  }
}

static char args_doc[] = "COMMAND";

static char doc[] =                                     \
  "\nCOMMANDS:\n"                                       \
  "\trun     - run a new container\n"                   \
  "\texec    - exec a command in a running container\n" \
  ;

static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc, char **argv) {
  self = argv[0];
  argc--; argv++;
  if (argc < 1) {
    fprintf(stderr, "%s: must have command\n", self);
    return 1;
  }
  const char *command = argv[0];

  if (strcmp(command, "run") == 0) {
    int cmd_idx;
    argp_parse(&argp, argc, argv, ARGP_IN_ORDER, &cmd_idx, &context);

    char **commands = (char**)malloc(sizeof(char*) * (argc - cmd_idx));
    char *arg;
    int i;

    for (i = 0; i < argc - cmd_idx; i++) {
      arg = argv[cmd_idx + i];
      commands[i] = malloc(strlen(arg));
      strcpy(commands[i], arg);
    }
    commands[i] = NULL;

    return run(commands);
  } else if (strcmp(command, "exec") == 0) {
    int cmd_idx;
    argp_parse(&argp, argc, argv, ARGP_IN_ORDER, &cmd_idx, &context);

    char **commands = (char**)malloc(sizeof(char*) * (argc - cmd_idx));
    char *arg;
    int i;

    for (i = 0; i < argc - cmd_idx; i++) {
      arg = argv[cmd_idx + i];
      commands[i] = malloc(strlen(arg));
      strcpy(commands[i], arg);
    }
    commands[i] = NULL;

    return exec(commands);
  }

  fprintf(stderr, "%s: unknown command '%s'\n", self, command);
  return 1;
}
