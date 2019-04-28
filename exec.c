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
  // Check if pid file already exists
  if (access("/var/run/drofune.pid", F_OK) != 0) {
    fprintf(stderr, "/var/run/drofune.pid: Container not running\n");
    return 1;
  }

  pid_t pid;
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

  int i;
  int namespaces_count = sizeof(namespaces) / sizeof(namespaces[0]);
  for (i = 0; i < namespaces_count; i++) {
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
