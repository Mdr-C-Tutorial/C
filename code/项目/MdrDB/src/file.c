/*
 * file.c：用 stdio 实现文件层，外加故障注入。
 */
#include "file.h"

#include <stdio.h>
#include <stdlib.h>

struct db_file {
    FILE *fp;
};

/* 故障注入的状态是全局的：真实的崩溃会同时影响进程打开的所有文件。 */
static struct {
    bool armed;
    long writes_left;
    fault_mode mode;
    bool crashed;
    long write_count;
} fault;

void fault_arm(long writes_allowed, fault_mode mode) {
    fault.armed = true;
    fault.writes_left = writes_allowed;
    fault.mode = mode;
    fault.crashed = false;
}

void fault_disarm(void) {
    fault.armed = false;
    fault.crashed = false;
}

bool fault_crashed(void) { return fault.crashed; }

long fault_write_count(void) { return fault.write_count; }

/* 每次写操作开始前调用。返回 true 表示这一次就是崩溃点。 */
static bool at_crash_point(void) {
    if (fault.armed) {
        if (fault.writes_left == 0) {
            return true;
        }
        fault.writes_left--;
    }
    fault.write_count++;
    return false;
}

static void crash(void) {
    if (fault.mode == FAULT_EXIT) {
        fputs("fault: simulated crash\n", stderr);
        /* _Exit 不执行 atexit 注册的函数，像崩溃一样突然结束进程。 */
        _Exit(EXIT_FAILURE);
    }
    fault.crashed = true;
}

static db_status wrap(FILE *fp, db_file **out) {
    if (fp == NULL) {
        return DB_IO_ERROR;
    }
    /*
     * 关掉 stdio 的缓冲，每次 fwrite 都立即交给操作系统。页本来就是
     * 整块读写，缓冲省不了几次系统调用；更重要的是，数据到达操作系统
     * 的顺序从此与代码中写的顺序一致，"在第 n 次写入时崩溃"才名副其实。
     */
    if (setvbuf(fp, NULL, _IONBF, 0) != 0) {
        fclose(fp);
        return DB_IO_ERROR;
    }
    db_file *f = malloc(sizeof *f);
    if (f == NULL) {
        fclose(fp);
        return DB_NO_MEMORY;
    }
    f->fp = fp;
    *out = f;
    return DB_OK;
}

db_status file_open(const char *path, db_file **out) {
    *out = NULL;
    return wrap(fopen(path, "rb+"), out);
}

db_status file_create(const char *path, db_file **out) {
    *out = NULL;
    if (fault.crashed) {
        return DB_IO_ERROR;
    }
    if (at_crash_point()) {
        crash();
        return DB_IO_ERROR;
    }
    return wrap(fopen(path, "wb+"), out);
}

void file_close(db_file *f) {
    if (f != NULL) {
        fclose(f->fp);
        free(f);
    }
}

db_status file_read_at(db_file *f, long offset, void *buf, size_t size,
                       size_t *nread) {
    *nread = 0;
    if (fseek(f->fp, offset, SEEK_SET) != 0) {
        return DB_IO_ERROR;
    }
    size_t n = fread(buf, 1, size, f->fp);
    if (n < size && ferror(f->fp)) {
        clearerr(f->fp);
        return DB_IO_ERROR;
    }
    *nread = n;
    return DB_OK;
}

db_status file_write_at(db_file *f, long offset, const void *buf, size_t size) {
    if (fault.crashed) {
        return DB_IO_ERROR;
    }
    bool crash_here = at_crash_point();
    /* 崩溃点上的写只写进前一半，模拟断电时只写进一部分的撕裂写。 */
    size_t n = crash_here ? size / 2 : size;
    if (fseek(f->fp, offset, SEEK_SET) != 0 || fwrite(buf, 1, n, f->fp) != n) {
        clearerr(f->fp);
        return DB_IO_ERROR;
    }
    if (crash_here) {
        crash();
        return DB_IO_ERROR;
    }
    return DB_OK;
}

db_status file_sync(db_file *f) {
    if (fault.crashed) {
        return DB_IO_ERROR;
    }
    /*
     * fflush 只保证数据交给了操作系统：进程崩溃不会丢，断电仍可能丢。
     * 要抵御断电，这里还得调用 fsync（POSIX）或 FlushFileBuffers
     * （Windows），它们不属于标准 C，见 README 的扩展任务。
     */
    return fflush(f->fp) == 0 ? DB_OK : DB_IO_ERROR;
}

bool file_exists(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return false;
    }
    fclose(fp);
    return true;
}

db_status file_remove(const char *path) {
    if (fault.crashed) {
        return DB_IO_ERROR;
    }
    if (at_crash_point()) {
        crash();
        return DB_IO_ERROR;
    }
    return remove(path) == 0 ? DB_OK : DB_IO_ERROR;
}
