/* Code to mangle pathnames into those matching a given prefix.
   eg. open("/lib/foo.so") => open("/usr/gnemul/i386-linux/lib/foo.so");

   The assumption is that this area does not change.
*/
#include "qemu/osdep.h"
#include <sys/param.h>
#include <dirent.h>
#include "qemu/cutils.h"
#include "qemu/path.h"
#include "qemu/thread.h"

static const char *base;
static GHashTable *hash;
static QemuMutex lock;

#ifdef CONFIG_LATX
static bool path_is_proc_namespace(const char *name)
{
    static const char proc_root[] = "/proc";
    const char *component;
    size_t length = sizeof(proc_root) - 1;

    if (strncmp(name, proc_root, length) ||
        (name[length] && name[length] != '/')) {
        return false;
    }

    component = name + length;
    while (*component) {
        component += strspn(component, "/");
        if (component[0] == '.' && component[1] == '.' &&
            (component[2] == '\0' || component[2] == '/')) {
            return false;
        }
        component += strcspn(component, "/");
    }

    return true;
}
#endif

void init_paths(const char *prefix)
{
    if (prefix[0] == '\0' || !strcmp(prefix, "/")) {
        return;
    }

    if (prefix[0] == '/') {
        base = g_strdup(prefix);
    } else {
        char *cwd = g_get_current_dir();
        base = g_build_filename(cwd, prefix, NULL);
        g_free(cwd);
    }

    hash = g_hash_table_new(g_str_hash, g_str_equal);
    qemu_mutex_init(&lock);
}

static bool path_has_prefix(const char *name)
{
    if (!base || !name || name[0] != '/' || name[1] == '\0') {
        return false;
    }

#ifdef CONFIG_LATX
    /* Keep live procfs visible even when the runtime contains /proc. */
    if (path_is_proc_namespace(name)) {
        return false;
    }
#endif

    return true;
}

char *path_get_prefixed(const char *name)
{
    if (!path_has_prefix(name)) {
        return g_strdup(name);
    }

    return g_build_filename(base, name, NULL);
}

/* Look for path in emulation dir, otherwise return name. */
const char *path(const char *name)
{
    gpointer key, value;
    const char *ret;

    /* Only do absolute paths: quick and dirty, but should mostly be OK.  */
    if (!path_has_prefix(name)) {
        return name;
    }

    qemu_mutex_lock(&lock);

    /* Have we looked up this file before?  */
    if (g_hash_table_lookup_extended(hash, name, &key, &value)) {
        ret = value ? value : name;
    } else {
        char *save = g_strdup(name);
        char *full = path_get_prefixed(name);

        /* Look for the path; record the result, pass or fail.  */
        if (access(full, F_OK) == 0) {
            /* Exists.  */
            g_hash_table_insert(hash, save, full);
            ret = full;
        } else {
            /* Does not exist.  */
            g_free(full);
            g_hash_table_insert(hash, save, NULL);
            ret = name;
        }
    }

    qemu_mutex_unlock(&lock);
    return ret;
}

/* Keep a child from inheriting a path-cache lock held by another thread. */
void path_fork_start(void)
{
    if (base) {
        qemu_mutex_lock(&lock);
    }
}

void path_fork_end(int child)
{
    if (!base) {
        return;
    }

    if (child) {
        qemu_mutex_init(&lock);
    } else {
        qemu_mutex_unlock(&lock);
    }
}
