struct namespace {
  const char *name;
  int value;
};

int exec(char **commands, struct context ctx);
static int exec_process(char** commands, struct context ctx);
static pid_t enter_namespaces(char* pid);
static pid_t secure_enter_namespaces(char* pid);
static int enter_namespace(char* pid, struct namespace ns);
