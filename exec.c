#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <math.h>

struct namespace {
  const char *name;
  int value;
};

static struct namespace namespaces[] = {
  {"pid", CLONE_NEWPID},
  {"uts", CLONE_NEWUTS},
  {"ipc", CLONE_NEWIPC},
  {"mnt", CLONE_NEWNS},
};

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
  free(path);
  return 0;
}

int exec(char **commands) {
  pid_t pid;
  char* target_pid = "19522";

  int i;
  for (i = 0; i < 4; i++) {
    struct namespace ns = namespaces[i];
    if (enter_namespace(target_pid, ns) < 0) {
      return 1;
    }

    if (ns.value == CLONE_NEWPID) {
      pid = fork();
      sleep(1);
      if (pid > 0) {
        break;
      }
    }
  }

  if (pid == 0) {
    execv(commands[0], commands);
    return 0;
  }

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
