#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>
#include "drofune.h"
#include "run.h"
#include "exec.h"

const char *self;
static struct context context;

enum {
  OPTION_SECURE_JOIN = 1000,
  OPTION_CLONE_BINARY,
  OPTION_PIVOT_ROOT,
  OPTION_DROP_CAPS,
};

static struct argp_option run_options[] = {
  {"pivot-root", OPTION_PIVOT_ROOT, 0, 0, "use pivot_root(2) instead of chroot(2)"},
  {"drop-caps", OPTION_DROP_CAPS, 0, 0, "drop capabilities like Docker"},
  { 0 }
};

static struct argp_option exec_options[] = {
  {"secure-join", OPTION_SECURE_JOIN, 0, 0, "fork after entering all namespaces"},
  {"clone-binary", OPTION_CLONE_BINARY, 0, 0, "run with cloned binary"},
  {"drop-caps", OPTION_DROP_CAPS, 0, 0, "drop capabilities like Docker"},
  { 0 }
};

static error_t parse_global_opt(int key, char *arg, struct argp_state *state) {
  switch (key) {
    case ARGP_KEY_NO_ARGS:
      fprintf(stderr, "%s: must have commands\n", self);
      exit(1);
    default:
      return ARGP_ERR_UNKNOWN;
  }

  return 0;
}

static error_t parse_run_opt(int key, char *arg, struct argp_state *state) {
  switch (key) {
    case OPTION_PIVOT_ROOT:
      context.pivot_root = 1;
      break;
    case OPTION_DROP_CAPS:
      context.drop_caps = 1;
      break;
    case ARGP_KEY_NO_ARGS:
      fprintf(stderr, "%s: must have commands\n", self);
      exit(1);
    default:
      return ARGP_ERR_UNKNOWN;
  }

  return 0;
}

static error_t parse_exec_opt(int key, char *arg, struct argp_state *state) {
  switch (key) {
    case OPTION_SECURE_JOIN:
      context.secure_join = 1;
      break;
    case OPTION_CLONE_BINARY:
      context.clone_binary = 1;
      break;
    case OPTION_DROP_CAPS:
      context.drop_caps = 1;
      break;
    case ARGP_KEY_NO_ARGS:
      fprintf(stderr, "%s: must have commands\n", self);
      exit(1);
    default:
      return ARGP_ERR_UNKNOWN;
  }

  return 0;
}

static char args_doc[] = "COMMAND [OPTION...]";

static char doc[] =                                     \
  "\nCOMMANDS:\n"                                       \
  "\trun     - run a new container\n"                   \
  "\texec    - exec a command in a running container\n" \
  ;

static struct argp global_argp = { 0, parse_global_opt, args_doc, doc };
static struct argp run_argp = { run_options, parse_run_opt, 0, 0 };
static struct argp exec_argp = { exec_options, parse_exec_opt, 0, 0 };

static char** get_commands(int argc, char **argv, int cmd_idx) {
  char **commands = (char**)malloc(sizeof(char*) * (argc - cmd_idx));
  char *arg;
  int i;

  for (i = 0; i < argc - cmd_idx; i++) {
    arg = argv[cmd_idx + i];
    commands[i] = malloc(strlen(arg));
    strcpy(commands[i], arg);
  }
  commands[i] = NULL;

  return commands;
}

int main(int argc, char **argv) {
  self = argv[0];
  int cmd_idx;
  argp_parse(&global_argp, argc, argv, ARGP_IN_ORDER, &cmd_idx, 0);
  char *command = argv[cmd_idx];
  argc = argc - cmd_idx; argv = argv + cmd_idx;

  if (strcmp(command, "run") == 0) {
    argp_parse(&run_argp, argc, argv, ARGP_IN_ORDER, &cmd_idx, &context);
    char **commands = get_commands(argc, argv, cmd_idx);
    return run(commands, context);
  } else if (strcmp(command, "exec") == 0) {
    argp_parse(&exec_argp, argc, argv, ARGP_IN_ORDER, &cmd_idx, &context);
    char **commands = get_commands(argc, argv, cmd_idx);
    return exec(commands, context);
  }

  fprintf(stderr, "%s: unknown command '%s'\n", self, command);
  return 1;
}
