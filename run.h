int run(char **commands, struct context ctx);
static int init_process(char* dir, char** commands, struct context ctx);
static int syscall_clone(void);
static char* mount_rootfs(char* dir);
static int mount_devtmpfs(char *dir);
static int mount_procfs(char *dir);
static int chrootfs(char *dir);
static int pivot_rootfs(char *dir);
