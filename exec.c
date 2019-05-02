#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "drofune.h"
#include "exec.h"
#include "cloned_binary.h"
#include "drop_caps.h"

// List of namespaces. The order is important for exploits.
// As it joins the namespace sequentially from the top, it is easier to attack if the PID namespace is on the top.
static struct namespace namespaces[] = {
  {"pid", CLONE_NEWPID},
  {"uts", CLONE_NEWUTS},
  {"ipc", CLONE_NEWIPC},
  {"mnt", CLONE_NEWNS},
};

int exec(char **commands, struct context ctx) {
  // At first glance, you may not understand why it needs this implementation.
  // However, it allows access to the host binary from within the container through /proc/pid/exe if do not clone the binary.
  // See exploits/CVE-2019-5736
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
  pid_t pid;
  if (ctx.secure_join) {
    pid = secure_enter_namespaces(target_pid);
  } else {
    pid = enter_namespaces(target_pid);
  }
  if (pid < 0) return 1;

  // Exec commands in the container.
  if (pid == 0)
    return exec_process(commands, ctx);

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

static int exec_process(char** commands, struct context ctx) {
  // Drop capabilities to prevent dangerous operations from the container.
  if (ctx.drop_caps) {
    if (drop_caps() < 0) {
      perror("drop capabilities");
      return 1;
    }
  }

  if (execv(commands[0], commands) < 0) {
    perror("execv");
    return 1;
  }
  return 0;
}

// Join container's namespaces in an unsafe way.
static pid_t enter_namespaces(char* target_pid) {
  int i;
  int namespaces_count = sizeof(namespaces) / sizeof(namespaces[0]);
  pid_t pid;

  for (i = 0; i < namespaces_count; i++) {
    struct namespace ns = namespaces[i];
    if (enter_namespace(target_pid, ns) < 0) {
      return -1;
    }

    // Certainly, you need to fork to enter the PID namespace, but you MUST NOT fork before entering all namespaces.
    // For sleep(3), assume another long process. This is necessary for an attack vector.
    // See exploits/insecure_join
    if (ns.value == CLONE_NEWPID) {
      pid = fork();
      sleep(1);
      if (pid > 0) {
        return pid;
      }
    }
  }

  return pid;
}

// Join container's namespaces in a safe way.
static pid_t secure_enter_namespaces(char* target_pid) {
  int i;
  int namespaces_count = sizeof(namespaces) / sizeof(namespaces[0]);
  pid_t pid;

  for (i = 0; i < namespaces_count; i++) {
    struct namespace ns = namespaces[i];
    if (enter_namespace(target_pid, ns) < 0) {
      return -1;
    }
  }

  // You should fork after entering all namespaces.
  pid = fork();
  sleep(1);

  return pid;
}

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
