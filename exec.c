#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/sendfile.h>
#include <math.h>
#include "drofune.h"
#include "cloned_binary.h"

struct namespace {
  const char *name;
  int value;
};

// List of namespaces. The order is important for exploits.
// As it joins the namespace sequentially from the top, it is easier to attack if the PID namespace is on the top.
static struct namespace namespaces[] = {
  {"pid", CLONE_NEWPID},
  {"uts", CLONE_NEWUTS},
  {"ipc", CLONE_NEWIPC},
  {"mnt", CLONE_NEWNS},
};

// Enter container's namespace.
// Open the file descriptor referring a namespace and call setns(2).
static int enter_namespace(char* pid, struct namespace ns) {
  int len = 10 + strlen(pid) + 3 + 1;
  char *path = malloc(len);
  if (path == NULL) {
    perror("malloc namespace path");
    return -1;
  }
  snprintf(path, len, "/proc/%s/ns/%s", pid, ns.name);

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    perror("open ns fd");
    return -1;
  }
  if (setns(fd, ns.value) < 0) {
    perror("setns");
    return -1;
  }
  if (close(fd) < 0) {
    perror("close ns fd");
    return -1;
  }
  free(path);
  return 0;
}

int exec(char **commands, struct context ctx) {
  if (ctx.clone_binary) {
    if (ensure_cloned_binary() < 0) {
      perror("ensure_clone_binary");
      return 1;
    }
  }

  // Check if pid file already exists.
  if (access("/var/run/drofune.pid", F_OK) != 0) {
    fprintf(stderr, "/var/run/drofune.pid: Container not running\n");
    return 1;
  }

  // Read container process pid from pid file.
  char target_pid[16];
  FILE *fp = fopen("/var/run/drofune.pid", "r");
  if (fp == NULL) {
    perror("open file file");
    return 1;
  }
  if (fgets(target_pid, 16, fp) == NULL) {
    perror("read pid file");
    return 1;
  }
  if (fclose(fp) == EOF) {
    perror("close pid file");
    return 1;
  }

  // Join container's namespaces.
  int i;
  int namespaces_count = sizeof(namespaces) / sizeof(namespaces[0]);
  pid_t pid;
  for (i = 0; i < namespaces_count; i++) {
    struct namespace ns = namespaces[i];
    if (enter_namespace(target_pid, ns) < 0) {
      return 1;
    }

    // Oh no! Certainly, you need to fork to enter the PID namespace, but you MUST NOT fork before entering all namespaces.
    // For sleep(3), assume another long process. This is necessary for an attack vector.
    if (ns.value == CLONE_NEWPID && !ctx.secure_join) {
      pid = fork();
      sleep(1);
      if (pid > 0) {
        break;
      }
    }
  }

  // You should fork after entering all namespaces.
  if (ctx.secure_join) {
    pid = fork();
    sleep(1);
  }

  // Exec commands in the container.
  if (pid == 0) {
    execv(commands[0], commands);
    return 0;
  }

  // Here is the parent process.
  // Wait for the process to exit.
  int status;
  if (waitpid(pid, &status, 0) < 0) {
    perror("waitpid");
    return 1;
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return 1;
}
