#define _GNU_SOURCE
#include <stdio.h>
#include <sched.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <signal.h>
#include <fcntl.h>

int run() {
  pid_t pid = (int)syscall(__NR_clone, CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD, NULL);
  if (pid < 0) {
    perror("clone");
    return 1;
  }

  if (pid == 0) {
    int ret;
    int oldrootfd = open("/", O_DIRECTORY | O_RDONLY);
    int newrootfd = open("/", O_DIRECTORY | O_RDONLY);

    if (oldrootfd < 0) {
      perror("open");
      return 1;
    }
    if (newrootfd < 0) {
      perror("open");
      return 1;
    }
    ret = fchdir(newrootfd);
    if (ret < 0) {
      perror("fchdir");
      return 1;
    }
    ret = syscall(__NR_pivot_root, ".", ".");
    if (ret < 0) {
      perror("pivot_root");
      return 1;
    }
    ret = fchdir(oldrootfd);
    if (ret < 0) {
      perror("fchdir");
      return 1;
    }
    ret = mount("none", ".", NULL, MS_REC | MS_PRIVATE, NULL);
    if (ret < 0) {
      perror("mount");
      return 1;
    }
    ret = umount2 (".", MNT_DETACH);
    if (ret < 0) {
      perror("umount2");
      return 1;
    }
    ret = chdir ("/");
    if (ret < 0) {
      perror("chdir");
      return 1;
    }

    char *argv[16];

    argv[0] = "/bin/bash";
    execv(argv[0], argv);
    return 0;
  }

  sleep(60);
  return 0;
}
