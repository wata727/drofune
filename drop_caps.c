#include <stdio.h>
#include <sys/prctl.h>

// See https://github.com/torvalds/linux/blob/v4.15/include/uapi/linux/capability.h
#define CAP_DAC_READ_SEARCH 2
#define CAP_LINUX_IMMUTABLE 9
#define CAP_NET_BROADCAST   11
#define CAP_NET_ADMIN       12
#define CAP_IPC_LOCK        14
#define CAP_IPC_OWNER       15
#define CAP_SYS_MODULE      16
#define CAP_SYS_RAWIO       17
#define CAP_SYS_CHROOT      18
#define CAP_SYS_PTRACE      19
#define CAP_SYS_PACCT       20
#define CAP_SYS_ADMIN       21
#define CAP_SYS_BOOT        22
#define CAP_SYS_NICE        23
#define CAP_SYS_RESOURCE    24
#define CAP_SYS_TIME        25
#define CAP_SYS_TTY_CONFIG  26
#define CAP_LEASE           28
#define CAP_AUDIT_CONTROL   30
#define CAP_MAC_OVERRIDE    32
#define CAP_MAC_ADMIN       33
#define CAP_SYSLOG          34
#define CAP_WAKE_ALARM      35
#define CAP_BLOCK_SUSPEND   36

int drop_caps(void) {
  // These are the capabilities which are not granted by default on Docker.
  // See https://docs.docker.com/engine/reference/run/#runtime-privilege-and-linux-capabilities
  int caps[] = {
    CAP_LINUX_IMMUTABLE,
    CAP_NET_BROADCAST,
    CAP_NET_ADMIN,
    CAP_IPC_LOCK,
    CAP_IPC_OWNER,
    CAP_SYS_MODULE,
    CAP_SYS_RAWIO,
    CAP_SYS_PTRACE,
    CAP_SYS_PACCT,
    CAP_SYS_ADMIN,
    CAP_SYS_BOOT,
    CAP_SYS_NICE,
    CAP_SYS_RESOURCE,
    CAP_SYS_TIME,
    CAP_SYS_TTY_CONFIG,
    CAP_LEASE,
    CAP_AUDIT_CONTROL,
    CAP_MAC_OVERRIDE,
    CAP_MAC_ADMIN,
    CAP_SYSLOG,
    CAP_WAKE_ALARM,
    CAP_BLOCK_SUSPEND,
  };

  int cap_size = sizeof(caps) / sizeof(int);
  int i;
  for (i = 0; i < cap_size; i++) {
    if (prctl(PR_CAPBSET_DROP, caps[i], 0, 0, 0) < 0) {
      perror("drop capability");
      return -1;
    }
  }

  // Drop SYS_CHROOT capability to prevent chroot jailbreak.
  // This capability is enabled by default on Docker because it uses pivor_root(2).
  // See exploits/chroot_jailbreak
  if (prctl(PR_CAPBSET_DROP, CAP_SYS_CHROOT, 0, 0, 0) < 0) {
    perror("drop capability");
    return -1;
  }

  return 0;
}
