#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include "utils.h"

// Create a new process in new namespaces.
// This separates only mount, UTS, IPC and PID, except network, cgroup, user namespaces.
int syscall_clone(void) {
  int pid = (int)syscall(__NR_clone, CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD, NULL);
  if (pid < 0) {
    perror("clone");
    return pid;
  }
  return pid;
}

// Mount the host's root directory as rootfs.
// Use overlayfs to not propagate changes inside the container to the host.
char* mount_rootfs(char* dir) {
  // Container's root directory
  char *rd = concat(dir, "/root");
  // Upper layer directory of overlayfs
  char *ud = concat(dir, "/storage");
  // Working directory of overlayfs
  char *wd = concat(dir, "/work");
  if (rd == NULL || ud == NULL || wd == NULL)
    return NULL;

  if (mkdir(rd, 0700) < 0) {
    perror("mkdir rootdir");
    return NULL;
  }
  if (mkdir(ud, 0700) < 0) {
    perror("mkdir upperdir");
    return NULL;
  }
  if (mkdir(wd, 0700) < 0) {
    perror("mkdir workdir");
    return NULL;
  }

  size_t len = 9 + strlen(ud) + 20 + strlen(wd) + 1;
  char *data = malloc(len);
  if (data == NULL) {
    perror("malloc overlayfs data");
    return NULL;
  }
  snprintf(data, len, "upperdir=%s,lowerdir=/,workdir=%s", ud, wd);

  if (mount("overlayfs", rd, "overlay", MS_MGC_VAL, data) < 0) {
    perror("mount overlayfs");
    return NULL;
  }
  return rd;
}

// TODO: pseudo devtmpfs
int mount_devtmpfs(char *dir) {
  char *dev = concat(dir, "/dev");
  if (mount("devtmpfs", dev, "devtmpfs", MS_MGC_VAL, "") < 0) {
    perror("mount devtmpfs");
    return -1;
  }
  return 0;
}

// TODO: comments
int mount_procfs(char *dir) {
  if (mount("proc", "/proc", "proc", MS_MGC_VAL, "ro,nosuid,nodev,noexec") < 0) {
    perror("mount /proc ro");
    return -1;
  }
  char *proc = concat(dir, "/proc");
  if (mount("proc", proc, "proc", MS_MGC_VAL, "rw,nosuid,nodev,noexec,relatime") < 0) {
    perror("mount /proc rw");
    return -1;
  }
  char *proc_sys = concat(dir, "/proc/sys");
  if (mount("/proc/sys", proc_sys, NULL, MS_BIND, NULL) < 0) {
    perror("mount /proc/sys");
    return -1;
  }
  char *proc_sysrq_trigger = concat(dir, "/proc/sysrq-trigger");
  if (mount("/proc/sysrq-trigger", proc_sysrq_trigger, NULL, MS_BIND, NULL) < 0) {
    perror("mount /proc/sysrq-trigger");
    return -1;
  }
  char *proc_irq = concat(dir, "/proc/irq");
  if (mount("/proc/irq", proc_irq, NULL, MS_BIND, NULL) < 0) {
    perror("mount /proc/irq");
    return -1;
  }
  char *proc_bus = concat(dir, "/proc/bus");
  if (mount("/proc/bus", proc_bus, NULL, MS_BIND, NULL) < 0) {
    perror("mount /proc/bus");
    return -1;
  }
  char *sys = concat(dir, "/sys");
  if (mount("/sys", sys, NULL, MS_BIND, NULL) < 0) {
    perror("mount /sys");
    return -1;
  }
  return 0;
}

// TODO: comments
int change_rootfs(char *dir) {
  if (chdir(dir) < 0) {
    perror("chdir pivot_root");
    return 1;
  }
  if (mkdir(".orig", 0700) < 0) {
    perror("mkdir pivot_root");
    return 1;
  }
  if (syscall(__NR_pivot_root, ".", ".orig") < 0) {
    perror("pivot_root");
    return 1;
  }
  if (mount(NULL, "/.orig", NULL, MS_PRIVATE | MS_REC, NULL) < 0) {
    perror("mount pivot_root");
    return 1;
  }
  if (umount2("/.orig", MNT_DETACH) < 0) {
    perror("umount2 pivot_root");
    return 1;
  }
  if (chroot(".") < 0) {
    perror("chroot");
    return 1;
  }
  return 0;
}

int init_process(char* dir) {
  // Although the mount point is separated, the file system still points to the host.
  // As a first step, make the mount point private in order to not propagate mount events.
  if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) < 0) {
    perror("mount private");
    return 1;
  }

  // Mount the host's root directory on the working directory.
  char *rd = mount_rootfs(dir);
  if (rd == NULL)
    return 1;

  // Mount a new /dev directory. TODO: why?
  if (mount_devtmpfs(rd) < 0)
    return 1;

  // Mount a new /proc directory in order to hide outside pids.
  if (mount_procfs(rd) < 0)
    return 1;

  // TODO: Mount as new /tmp directory

  // Change the current rootfs of the process.
  // After that, the host's file system cannot be seen from this process.
  if (change_rootfs(rd) < 0)
    return 1;

  char *const argv[] = {
    "/bin/bash",
    NULL,
  };
  execv(argv[0], argv);
  return 0;
}

int run(void) {
  // Make a temporary directory as a working directory.
  // All processes in the container run on this directory.
  char template[] = "/tmp/insec-XXXXXX";
  char *dir = mkdtemp(template);
  if (dir == NULL) {
    perror("mkdtemp");
    return 1;
  }

  // Create a container init process.
  // Note that the init process starts on the host.
  pid_t pid = syscall_clone();
  if (pid < 0)
    return 1;

  // syscall_clone() behaves like fork(2).
  // If the pid is zero, the process has run in new namespaces.
  if (pid == 0)
    return init_process(dir);

  // Here is the parent process.
  // Wait for the init process to exit.
  int status;
  if (waitpid(pid, &status, 0) < 0) {
    perror("waitpid");
    return 1;
  }
  // Clean up the working directory.
  if (rm_r(dir) < 0)
    return 1;
  // If the exit status is obtained, it will be returned.
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return 1;
}
