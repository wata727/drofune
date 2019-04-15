#define _GNU_SOURCE
#include <stdio.h>
#include <sched.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

int exec(char **commands) {
  int fd = open("/proc/32038/ns/pid", O_RDONLY);
  if (fd < 0) {
    perror("open");
    return 1;
  }
  if (setns(fd, CLONE_NEWPID) < 0) {
    perror("setns");
    return 1;
  }
  fd = open("/proc/32038/ns/mnt", O_RDONLY);
  if (fd < 0) {
    perror("open");
    return 1;
  }
  if (setns(fd, CLONE_NEWNS) < 0) {
    perror("setns");
    return 1;
  }

  pid_t pid = fork();
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