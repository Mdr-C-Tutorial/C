#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "sqlite3.h"
#include "vfslog.h"

/* 每个被打开的文件对应一个 log_file：真实文件对象紧跟在结构体后面。 */
typedef struct log_file {
    sqlite3_file base;
    sqlite3_file *real;
    struct vfslog *owner;
    const char *name;
} log_file;

/* 垫片本身的状态：真实 VFS、输出文件、自己的 io_methods。 */
typedef struct vfslog {
    sqlite3_vfs base;
    sqlite3_vfs *root;
    FILE *out;
    sqlite3_io_methods methods;
} vfslog;

/* 只保留路径的最后一段，日志里看着干净。 */
static const char *file_tail(const char *path) {
    const char *tail = path;

    while (*path != '\0') {
        if (*path == '/' || *path == '\\') {
            tail = path + 1;
        }
        path++;
    }
    return tail;
}

static void log_printf(vfslog *p, const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vfprintf(p->out, fmt, ap);
    va_end(ap);
    fflush(p->out);
}

static int log_close(sqlite3_file *file) {
    log_file *p = (log_file *)file;

    log_printf(p->owner, "close        %s\n", p->name);
    return p->real->pMethods->xClose(p->real);
}

static int log_read(sqlite3_file *file, void *buf, int amount, sqlite3_int64 offset) {
    log_file *p = (log_file *)file;
    int rc = p->real->pMethods->xRead(p->real, buf, amount, offset);

    log_printf(p->owner, "read   %-10s ofst=%-8lld n=%-5d -> %d\n", p->name,
               (long long)offset, amount, rc);
    return rc;
}

static int log_write(sqlite3_file *file, const void *buf, int amount,
                     sqlite3_int64 offset) {
    log_file *p = (log_file *)file;
    int rc = p->real->pMethods->xWrite(p->real, buf, amount, offset);

    log_printf(p->owner, "write  %-10s ofst=%-8lld n=%-5d -> %d\n", p->name,
               (long long)offset, amount, rc);
    return rc;
}

static int log_truncate(sqlite3_file *file, sqlite3_int64 size) {
    log_file *p = (log_file *)file;

    log_printf(p->owner, "truncate     %-10s size=%lld\n", p->name, (long long)size);
    return p->real->pMethods->xTruncate(p->real, size);
}

static int log_sync(sqlite3_file *file, int flags) {
    log_file *p = (log_file *)file;

    log_printf(p->owner, "sync         %-10s flags=0x%x\n", p->name, flags);
    return p->real->pMethods->xSync(p->real, flags);
}

static int log_file_size(sqlite3_file *file, sqlite3_int64 *size) {
    log_file *p = (log_file *)file;
    int rc = p->real->pMethods->xFileSize(p->real, size);

    log_printf(p->owner, "filesize     %-10s -> %d size=%lld\n", p->name, rc,
               (long long)*size);
    return rc;
}

static int log_lock(sqlite3_file *file, int level) {
    log_file *p = (log_file *)file;
    int rc = p->real->pMethods->xLock(p->real, level);

    log_printf(p->owner, "lock         %-10s level=%d -> %d\n", p->name, level, rc);
    return rc;
}

static int log_unlock(sqlite3_file *file, int level) {
    log_file *p = (log_file *)file;

    return p->real->pMethods->xUnlock(p->real, level);
}

static int log_check_reserved_lock(sqlite3_file *file, int *out) {
    log_file *p = (log_file *)file;

    return p->real->pMethods->xCheckReservedLock(p->real, out);
}

static int log_file_control(sqlite3_file *file, int op, void *arg) {
    log_file *p = (log_file *)file;

    return p->real->pMethods->xFileControl(p->real, op, arg);
}

static int log_sector_size(sqlite3_file *file) {
    log_file *p = (log_file *)file;

    return p->real->pMethods->xSectorSize(p->real);
}

static int log_device_characteristics(sqlite3_file *file) {
    log_file *p = (log_file *)file;

    return p->real->pMethods->xDeviceCharacteristics(p->real);
}

static int log_shm_map(sqlite3_file *file, int region, int size, int extend,
                       void volatile **out) {
    log_file *p = (log_file *)file;

    log_printf(p->owner, "shm_map      %-10s region=%d size=%d\n", p->name, region, size);
    return p->real->pMethods->xShmMap(p->real, region, size, extend, out);
}

static int log_shm_lock(sqlite3_file *file, int offset, int n, int flags) {
    log_file *p = (log_file *)file;

    return p->real->pMethods->xShmLock(p->real, offset, n, flags);
}

static void log_shm_barrier(sqlite3_file *file) {
    log_file *p = (log_file *)file;

    p->real->pMethods->xShmBarrier(p->real);
}

static int log_shm_unmap(sqlite3_file *file, int delete_flag) {
    log_file *p = (log_file *)file;

    return p->real->pMethods->xShmUnmap(p->real, delete_flag);
}

static int log_open(sqlite3_vfs *vfs, sqlite3_filename name, sqlite3_file *file,
                    int flags, int *out_flags) {
    vfslog *owner = (vfslog *)vfs->pAppData;
    log_file *p = (log_file *)file;
    int rc;

    p->real = (sqlite3_file *)&p[1];
    p->owner = owner;
    p->name = name ? file_tail(name) : "(temp)";
    rc = owner->root->xOpen(owner->root, name, p->real, flags, out_flags);
    if (p->real->pMethods != NULL) {
        file->pMethods = &owner->methods;
    }
    log_printf(owner, "open         %-10s flags=0x%x -> %d\n", p->name, flags, rc);
    return rc;
}

static int log_delete(sqlite3_vfs *vfs, const char *name, int sync_dir) {
    vfslog *owner = (vfslog *)vfs->pAppData;

    log_printf(owner, "delete       %s\n", file_tail(name));
    return owner->root->xDelete(owner->root, name, sync_dir);
}

static int log_access(sqlite3_vfs *vfs, const char *name, int flags, int *out) {
    vfslog *owner = (vfslog *)vfs->pAppData;

    return owner->root->xAccess(owner->root, name, flags, out);
}

static int log_full_pathname(sqlite3_vfs *vfs, const char *name, int n, char *out) {
    vfslog *owner = (vfslog *)vfs->pAppData;

    return owner->root->xFullPathname(owner->root, name, n, out);
}

static void *log_dl_open(sqlite3_vfs *vfs, const char *name) {
    vfslog *owner = (vfslog *)vfs->pAppData;

    return owner->root->xDlOpen(owner->root, name);
}

static void log_dl_error(sqlite3_vfs *vfs, int n, char *msg) {
    vfslog *owner = (vfslog *)vfs->pAppData;

    owner->root->xDlError(owner->root, n, msg);
}

static void (*log_dl_sym(sqlite3_vfs *vfs, void *handle, const char *sym))(void) {
    vfslog *owner = (vfslog *)vfs->pAppData;

    return owner->root->xDlSym(owner->root, handle, sym);
}

static void log_dl_close(sqlite3_vfs *vfs, void *handle) {
    vfslog *owner = (vfslog *)vfs->pAppData;

    owner->root->xDlClose(owner->root, handle);
}

static int log_randomness(sqlite3_vfs *vfs, int n, char *out) {
    vfslog *owner = (vfslog *)vfs->pAppData;

    return owner->root->xRandomness(owner->root, n, out);
}

static int log_sleep(sqlite3_vfs *vfs, int microseconds) {
    vfslog *owner = (vfslog *)vfs->pAppData;

    return owner->root->xSleep(owner->root, microseconds);
}

static int log_current_time(sqlite3_vfs *vfs, double *out) {
    vfslog *owner = (vfslog *)vfs->pAppData;

    return owner->root->xCurrentTime(owner->root, out);
}

static int log_get_last_error(sqlite3_vfs *vfs, int n, char *msg) {
    vfslog *owner = (vfslog *)vfs->pAppData;

    return owner->root->xGetLastError(owner->root, n, msg);
}

static int log_current_time_int64(sqlite3_vfs *vfs, sqlite3_int64 *out) {
    vfslog *owner = (vfslog *)vfs->pAppData;

    return owner->root->xCurrentTimeInt64(owner->root, out);
}

int vfslog_register(const char *name, FILE *out, int make_default) {
    static vfslog owner;
    sqlite3_vfs *root = sqlite3_vfs_find(NULL);

    if (root == NULL) {
        return SQLITE_ERROR;
    }
    owner.root = root;
    owner.out = out;
    owner.base.iVersion = 2;
    owner.base.szOsFile = (int)sizeof(log_file) + root->szOsFile;
    owner.base.mxPathname = root->mxPathname;
    owner.base.pNext = NULL;
    owner.base.zName = name;
    owner.base.pAppData = &owner;
    owner.base.xOpen = log_open;
    owner.base.xDelete = log_delete;
    owner.base.xAccess = log_access;
    owner.base.xFullPathname = log_full_pathname;
    owner.base.xDlOpen = log_dl_open;
    owner.base.xDlError = log_dl_error;
    owner.base.xDlSym = log_dl_sym;
    owner.base.xDlClose = log_dl_close;
    owner.base.xRandomness = log_randomness;
    owner.base.xSleep = log_sleep;
    owner.base.xCurrentTime = log_current_time;
    owner.base.xGetLastError = log_get_last_error;
    owner.base.xCurrentTimeInt64 = log_current_time_int64;

    owner.methods.iVersion = 2;
    owner.methods.xClose = log_close;
    owner.methods.xRead = log_read;
    owner.methods.xWrite = log_write;
    owner.methods.xTruncate = log_truncate;
    owner.methods.xSync = log_sync;
    owner.methods.xFileSize = log_file_size;
    owner.methods.xLock = log_lock;
    owner.methods.xUnlock = log_unlock;
    owner.methods.xCheckReservedLock = log_check_reserved_lock;
    owner.methods.xFileControl = log_file_control;
    owner.methods.xSectorSize = log_sector_size;
    owner.methods.xDeviceCharacteristics = log_device_characteristics;
    owner.methods.xShmMap = log_shm_map;
    owner.methods.xShmLock = log_shm_lock;
    owner.methods.xShmBarrier = log_shm_barrier;
    owner.methods.xShmUnmap = log_shm_unmap;

    return sqlite3_vfs_register(&owner.base, make_default);
}
