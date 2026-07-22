#ifndef QEMU_PATH_H
#define QEMU_PATH_H

void init_paths(const char *prefix);
const char *path(const char *pathname);
void path_fork_start(void);
void path_fork_end(int child);

#endif
