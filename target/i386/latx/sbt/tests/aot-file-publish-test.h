#ifndef LATX_AOT_FILE_PUBLISH_TEST_H
#define LATX_AOT_FILE_PUBLISH_TEST_H

int __real_fsync(int fd);
int __real_rename(const char *old_path, const char *new_path);
int __wrap_fsync(int fd);
int __wrap_rename(const char *old_path, const char *new_path);

#endif
