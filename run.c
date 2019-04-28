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
// This isolates only PID, UTS, IPC and mount, except network, cgroup, user namespaces.
static int syscall_clone(void) {
  int pid = (int)syscall(__NR_clone, CLONE_NEWPID  | CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWNS | SIGCHLD, NULL);
  if (pid < 0) {
    perror("clone");
    return pid;
  }
  return pid;
}

// Mount the host's root directory as rootfs.
// Use overlayfs to not propagate changes inside the container to the host.
static char* mount_rootfs(char* dir) {
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
  free(data);
  return rd;
}

static int mount_devtmpfs(char *dir) {
  char *dev = concat(dir, "/dev");
  if (dev == NULL)
    return -1;

  if (mount("devtmpfs", dev, "devtmpfs", MS_MGC_VAL, "") < 0) {
    perror("mount devtmpfs");
    return -1;
  }
  return 0;
}

// Mount the host's procfs to the passed directory as read-only.
static int mount_procfs(char *dir) {
  char *proc = concat(dir, "/proc");
  char *proc_sys = concat(dir, "/proc/sys");
  char *proc_sysrq_trigger = concat(dir, "/proc/sysrq-trigger");
  char *proc_irq = concat(dir, "/proc/irq");
  char *proc_bus = concat(dir, "/proc/bus");
  char *sys = concat(dir, "/sys");
  if (proc == NULL || proc_sys == NULL || proc_sysrq_trigger == NULL || proc_irq == NULL || proc_bus == NULL || sys == NULL)
    return -1;

  // Mount the host's procfs read-only in order not to be written via bind mounted directory.
  // The current mount point is isolated and private, so there is no impact on the host.
  if (mount("proc", "/proc", "proc", MS_MGC_VAL, "ro,nosuid,nodev,noexec") < 0) {
    perror("mount /proc ro");
    return -1;
  }
  // Mount a new procfs to the new rootfs.
  if (mount("proc", proc, "proc", MS_MGC_VAL, "rw,nosuid,nodev,noexec,relatime") < 0) {
    perror("mount /proc rw");
    return -1;
  }
  // Bind mount the host's some procfs and sysfs to the new rootfs.
  if (mount("/proc/sys", proc_sys, NULL, MS_BIND, NULL) < 0) {
    perror("mount /proc/sys");
    return -1;
  }
  if (mount("/proc/sysrq-trigger", proc_sysrq_trigger, NULL, MS_BIND, NULL) < 0) {
    perror("mount /proc/sysrq-trigger");
    return -1;
  }
  if (mount("/proc/irq", proc_irq, NULL, MS_BIND, NULL) < 0) {
    perror("mount /proc/irq");
    return -1;
  }
  if (mount("/proc/bus", proc_bus, NULL, MS_BIND, NULL) < 0) {
    perror("mount /proc/bus");
    return -1;
  }
  if (mount("/sys", sys, NULL, MS_BIND, NULL) < 0) {
    perror("mount /sys");
    return -1;
  }
  return 0;
}

static int change_rootfs(char *dir) {
  if (chdir(dir) < 0) {
    perror("chdir pivot_root");
    return -1;
  }
  if (mkdir(".orig", 0700) < 0) {
    perror("mkdir pivot_root");
    return -1;
  }
  if (syscall(__NR_pivot_root, ".", ".orig") < 0) {
    perror("pivot_root");
    return -1;
  }
  // Mount the put_old directory private.
  if (mount(NULL, "/.orig", NULL, MS_PRIVATE | MS_REC, NULL) < 0) {
    perror("mount pivot_root");
    return -1;
  }
  // Perform a lazy unmount the put_old directory.
  // This will completely hide the host's filesystem from the new root directory.
  if (umount2("/.orig", MNT_DETACH) < 0) {
    perror("umount2 pivot_root");
    return -1;
  }
  // chroot(2) changes the running executable, which is necessary for the old root directory should be unmounted.
  if (chroot(".") < 0) {
    perror("chroot");
    return -1;
  }
  return 0;
}

static int init_process(char* dir, char** commands) {
  // Although the mount point is isolated, the filesystem still points to the host.
  // As a first step, make the mount point private in order to not propagate mount events.
  if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) < 0) {
    perror("mount private");
    return 1;
  }

  // Mount the host's root directory on the working directory.
  char *rd = mount_rootfs(dir);
  if (rd == NULL)
    return 1;

  // Mount a new /dev directory.
  if (mount_devtmpfs(rd) < 0)
    return 1;

  // Mount a new /proc directory in order to hide outside pids.
  if (mount_procfs(rd) < 0)
    return 1;

  // Change the current rootfs of the process.
  // After that, the host's filesystem cannot be seen from this process.
  if (change_rootfs(rd) < 0)
    return 1;

  execv(commands[0], commands);
  return 0;
}

int run(char **commands) {
  // Check if pid file already exists.
  if (access("/var/run/drofune.pid", F_OK) != -1) {
    fprintf(stderr, "/var/run/drofune.pid: Container already exists\n");
    return 1;
  }

  // Make a temporary directory as a working directory.
  // All processes in the container run on this directory.
  char template[] = "/tmp/drofune-XXXXXX";
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
    return init_process(dir, commands);

  // Here is the parent process.
  // Create and write pid file.
  FILE *fp = fopen("/var/run/drofune.pid", "w");
  if (fp == NULL) {
    perror("open pid file");
    return 1;
  }
  if (fprintf(fp, "%d", pid) < 0) {
    perror("write pid file");
    return 1;
  }
  if (fclose(fp) == EOF) {
    perror("close pid file");
    return 1;
  }

  // Wait for the init process to exit.
  int status;
  if (waitpid(pid, &status, 0) < 0) {
    perror("waitpid");
    return 1;
  }
  // Clean up the working directory.
  if (rm_r(dir) < 0)
    return 1;
  if (unlink("/var/run/drofune.pid") < 0)
    return 1;
  // If the exit status is obtained, it will be returned.
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return 1;
}
