# Drofune

Drofune is an insecure container runtime implementation. Drofune(泥舟) means sinking ship.

## Why the implementation is insecure?

This project's goal is NOT to replace container runtimes such as [runc](https://github.com/opencontainers/runc) or [crun](https://github.com/giuseppe/crun) (Therefore, naturally, it doesn't conform to the OCI runtime specification). Its goal is to learn about why container runtimes are secure or insecure. Drofune is a minimal implementation of a container runtime, and has some vulnerabilities **intentionally**.

These behaviors can be easily switched by some flags. While reviewing the implementation, you can learn what it takes to secure a container runtime.

## Specification

### Requirements

- Linux kernel => 3.18 (OverlayFS is required)
- x86_64 (shellcode depends on the architecture)

As a matter of fact, I have not confirmed that it works in all environments. If you have an environment that doesn't work, please send us a patch. The following is the environment I confirmed:

```
% uname -a
Linux pasocon 4.15.0-47-generic #50-Ubuntu SMP Wed Mar 13 10:44:52 UTC 2019 x86_64 x86_64 x86_64 GNU/Linux
```

### Namespaces

The following namespaces are isolated:

- IPC namespaces
- Mount namespaces
- PID namespaces
- UTS namespaces

As you can see, networks and cgroups are not isolated (user namespaces are not isolated by default in Docker). Therefore, although it is possible to attack hosts from these attack vectors, this project ignores them to simplify the implementation (patches are welcome).

### Linux Capabilities

By default, processes inside the container have all capabilities. The `--drop-caps` option allows you to limit the capabilities to the Docker default.

### Seccomp

runc (crun) uses [Seccomp](https://www.kernel.org/doc/Documentation/prctl/seccomp_filter.txt) for system call filtering, but Drofune doesn't use it. Therefore, there is a vulnerability that cannot be prevented, but we ignore it here (patches are welcome).

### Filesystem

The file system uses [OverlayFS](https://www.kernel.org/doc/Documentation/filesystems/overlayfs.txt) to reproduce the file system equivalent to the host inside a container.

## Usage

There are only two things you can do. The one is to run a new container, the other one is entering the existing container.

```
% drofune --help
Usage: drofune [OPTION...] COMMAND [OPTION...]

COMMANDS:
        run     - run a new container
        exec    - exec a command in a running container

  -?, --help                 Give this help list
      --usage                Give a short usage messag
```

All commands require root permissions to work with namespaces.

### Run a new container

```
% drofune run --help
Usage: run [OPTION...]

      --drop-caps            drop capabilities like Docker
      --pivot-root           use pivot_root(2) instead of chroot(2)
  -?, --help                 Give this help list
      --usage                Give a short usage message
```

The following is an example of starting a container and invoking Bash shell:

```
% drofune run /bin/bash
root@pasocon:/#
```

Note that only one container can be running. Attempting to start the second container results in an error.

### Exec a command in a running container

```
% drofune exec --help
Usage: exec [OPTION...]

      --clone-binary         run with cloned binary
      --drop-caps            drop capabilities like Docker
      --secure-join          fork after entering all namespaces
  -?, --help                 Give this help list
      --usage                Give a short usage message
```

The following is an example of entering the running container and invoking Bash shell:

```
% drofune exec /bin/bash
root@pasocon:/#
```

Note that an error occurs when the container is not running.

## How to test vulnerabilities

Exploits for testing vulnerabilities are available. See [exploits](exploits).

## References

This project refers to the following great projects. In particular, [MINCS](https://github.com/mhiramat/mincs) was very helpful for the basic implementation.

- [mhiramat/mincs](https://github.com/mhiramat/mincs)
- [opencontainers/runc](https://github.com/opencontainers/runc)
- [giuseppe/crun](https://github.com/giuseppe/crun)

## Author

[Kazuma Watanabe](https://github.com/wata727)
