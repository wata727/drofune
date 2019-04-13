#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

char* concat(const char *s1, const char *s2) {
  char *ret = malloc(strlen(s1) + strlen(s2) + 1);
  if (ret == NULL) {
    perror("malloc");
    exit(1);
  }
  strcpy(ret, s1);
  strcat(ret, s2);
  return ret;
}

int rm_r(const char *path) {
  DIR *d = opendir(path);
  size_t path_len = strlen(path);
  if (d == NULL) {
    perror("opendir");
    return -1;
  }

  struct dirent *dir;
  errno = 0;
  while ((dir = readdir(d)) != NULL) {
    if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")) {
      continue;
    }
    size_t len = path_len + strlen(dir->d_name) + 2;
    char *buf = malloc(len);
    if (buf == NULL) {
      perror("malloc");
      return -1;
    }
    struct stat statbuf;
    snprintf(buf, len, "%s/%s", path, dir->d_name);
    if (!stat(buf, &statbuf)){
      if (S_ISDIR(statbuf.st_mode)) {
        if (rm_r(buf) < 0) {
          return -1;
        }
      } else {
        if (unlink(buf) < 0) {
          perror("unlink");
          return -1;
        }
      }
    }
    free(buf);
  }
  if (errno) {
    perror("readdir");
    return -1;
  }
  if (closedir(d) < 0) {
    perror("closedir");
    return -1;
  }
  if (rmdir(path) < 0) {
    perror("rmdir");
    return -1;
  }
  return 0;
}
