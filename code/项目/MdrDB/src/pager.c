/*
 * pager.c：页缓存、页的分配和事务。
 *
 * 第 0 页是文件头，B+ 树从第 1 页开始。文件头的布局：
 *   偏移 0，16 字节：魔数 "MdrDB format 1"，不足部分补 0
 *   偏移 16，4 字节：页大小
 *   偏移 20，4 字节：页数，包括第 0 页
 *   其余字节为 0
 *
 * 一个写事务的生命周期：
 *   pager_begin   创建日志，记下当前页数；
 *   pager_write   某页第一次被修改前，把原始内容追加到日志；
 *   （缓存满时，脏页可以在提交前写回文件，前提是日志已经同步）
 *   pager_commit  同步日志，写回所有脏页，同步数据库文件，删除日志；
 *   pager_rollback 丢弃缓存，用日志把文件恢复原样，删除日志。
 */
#include "pager.h"

#include "file.h"
#include "journal.h"

#include <stdlib.h>
#include <string.h>

#define HEADER_MAGIC_SIZE 16
#define HEADER_PAGE_SIZE_OFFSET 16
#define HEADER_PAGE_COUNT_OFFSET 20

static const char header_magic[HEADER_MAGIC_SIZE] = "MdrDB format 1";

typedef enum pager_state {
    PAGER_IDLE,    /* 没有写事务 */
    PAGER_WRITING, /* 写事务进行中，日志已打开 */
    PAGER_ERROR    /* 写事务中途出了 I/O 错误，只能回滚或关闭 */
} pager_state;

struct pager {
    db_file *file;
    char *journal_path;
    page *frames;
    size_t frame_count;
    uint64_t clock; /* 每取用一次页加 1，当作 LRU 的时间 */

    uint32_t page_count; /* 当前页数，含本事务新分配的页 */
    pager_state state;
    journal journal;
    uint32_t orig_page_count; /* 事务开始时的页数 */
    uint8_t *journaled;       /* 位图：哪些页的原始内容已经进了日志 */
    pager_stats stats;
};

static long page_offset(uint32_t pgno) { return (long)pgno * DB_PAGE_SIZE; }

static bool test_bit(const uint8_t *bits, uint32_t i) {
    return (bits[i / 8] >> (i % 8)) & 1u;
}

static void set_bit(uint8_t *bits, uint32_t i) {
    bits[i / 8] = (uint8_t)(bits[i / 8] | (1u << (i % 8)));
}

/*
 * 写事务中途的 I/O 错误意味着磁盘上的内容可能新旧混杂，继续读写只会
 * 更糟。进入错误状态后只允许回滚，回滚所需的日志还在磁盘上。
 */
static db_status fail(pager *p, db_status rc) {
    if (p->state == PAGER_WRITING) {
        p->state = PAGER_ERROR;
    }
    return rc;
}

static char *concat(const char *a, const char *b) {
    size_t la = strlen(a);
    size_t lb = strlen(b);
    char *s = malloc(la + lb + 1);
    if (s != NULL) {
        memcpy(s, a, la);
        memcpy(s + la, b, lb + 1);
    }
    return s;
}

static db_status read_page(pager *p, uint32_t pgno, uint8_t *data) {
    size_t n;
    db_status rc =
        file_read_at(p->file, page_offset(pgno), data, DB_PAGE_SIZE, &n);
    if (rc != DB_OK) {
        return rc;
    }
    /* 页号小于页数却读不满一页：文件被截短了。 */
    return n == DB_PAGE_SIZE ? DB_OK : DB_CORRUPT;
}

/*
 * 把脏页写回数据库文件。回滚日志的规则：一页的原始内容必须先到达
 * 日志，才能覆盖数据库文件里的这一页，所以写之前先同步日志。
 */
static db_status write_back(pager *p, page *pg) {
    db_status rc = journal_sync(&p->journal);
    if (rc == DB_OK) {
        rc = file_write_at(p->file, page_offset(pg->pgno), pg->data,
                           DB_PAGE_SIZE);
    }
    if (rc != DB_OK) {
        return fail(p, rc);
    }
    pg->dirty = false;
    return DB_OK;
}

/* 页框不多，顺序查找足够快。页框多起来时应当改用散列表。 */
static page *find_frame(pager *p, uint32_t pgno) {
    for (size_t i = 0; i < p->frame_count; i++) {
        page *pg = &p->frames[i];
        if (pg->in_use && pg->pgno == pgno) {
            return pg;
        }
    }
    return NULL;
}

/*
 * 找一个页框装新页：有空闲的就用空闲的，否则换出最久没用、也没被
 * 钉住的那一页。被换出的页若是脏的，要先写回文件。事务还没提交，
 * 修改就进了数据库文件，这是允许的：原始内容在日志里，随时能撤销。
 */
static db_status take_frame(pager *p, page **out) {
    page *victim = NULL;
    for (size_t i = 0; i < p->frame_count; i++) {
        page *pg = &p->frames[i];
        if (!pg->in_use) {
            victim = pg;
            break;
        }
        if (pg->pins == 0 &&
            (victim == NULL || pg->last_used < victim->last_used)) {
            victim = pg;
        }
    }
    if (victim == NULL) {
        return DB_CACHE_FULL;
    }
    if (victim->in_use && victim->dirty) {
        db_status rc = write_back(p, victim);
        if (rc != DB_OK) {
            return rc;
        }
        p->stats.spills++;
    }
    victim->in_use = false;
    *out = victim;
    return DB_OK;
}

db_status pager_get(pager *p, uint32_t pgno, page **out) {
    *out = NULL;
    if (p->state == PAGER_ERROR) {
        return DB_IO_ERROR;
    }
    if (pgno >= p->page_count) {
        return DB_CORRUPT; /* 指向文件末尾之外，只能来自损坏的数据 */
    }
    page *pg = find_frame(p, pgno);
    if (pg != NULL) {
        p->stats.hits++;
    } else {
        db_status rc = take_frame(p, &pg);
        if (rc != DB_OK) {
            return rc;
        }
        rc = read_page(p, pgno, pg->data);
        if (rc != DB_OK) {
            return rc;
        }
        p->stats.misses++;
        pg->pgno = pgno;
        pg->in_use = true;
        pg->dirty = false;
        pg->pins = 0;
    }
    pg->pins++;
    pg->last_used = ++p->clock;
    *out = pg;
    return DB_OK;
}

void pager_unpin(page *pg) {
    if (pg != NULL && pg->pins > 0) {
        pg->pins--;
    }
}

db_status pager_write(pager *p, page *pg) {
    if (p->state == PAGER_ERROR) {
        return DB_IO_ERROR;
    }
    if (p->state != PAGER_WRITING) {
        return DB_MISUSE;
    }
    /*
     * 事务开始后才分配的页（页号不小于 orig_page_count）没有需要保存
     * 的原始内容：回滚后页数恢复原值，这些页落在页数之外，自然作废。
     * 位图保证每页只记一次：同一页第二次修改时，日志里已经是它的原貌。
     */
    if (pg->pgno < p->orig_page_count && !test_bit(p->journaled, pg->pgno)) {
        db_status rc = journal_append(&p->journal, pg->pgno, pg->data);
        if (rc != DB_OK) {
            return fail(p, rc);
        }
        set_bit(p->journaled, pg->pgno);
        p->stats.journaled++;
    }
    pg->dirty = true;
    return DB_OK;
}

db_status pager_allocate(pager *p, page **out) {
    *out = NULL;
    if (p->state == PAGER_ERROR) {
        return DB_IO_ERROR;
    }
    if (p->state != PAGER_WRITING) {
        return DB_MISUSE;
    }
    if (p->page_count >= DB_MAX_PAGES) {
        return DB_FULL;
    }

    /* 页数记在文件头里，改它和改别的页一样：先 pager_write，后修改。 */
    page *header;
    db_status rc = pager_get(p, 0, &header);
    if (rc != DB_OK) {
        return rc;
    }
    rc = pager_write(p, header);
    if (rc != DB_OK) {
        pager_unpin(header);
        return rc;
    }
    uint32_t pgno = p->page_count;
    put_u32(header->data + HEADER_PAGE_COUNT_OFFSET, pgno + 1);
    pager_unpin(header);
    p->page_count = pgno + 1;

    /* 新页不用从文件读：它的内容从全零开始。 */
    page *pg;
    rc = take_frame(p, &pg);
    if (rc != DB_OK) {
        return rc;
    }
    memset(pg->data, 0, DB_PAGE_SIZE);
    pg->pgno = pgno;
    pg->in_use = true;
    pg->dirty = true;
    pg->pins = 1;
    pg->last_used = ++p->clock;
    *out = pg;
    return DB_OK;
}

db_status pager_begin(pager *p) {
    if (p->state == PAGER_ERROR) {
        return DB_IO_ERROR;
    }
    if (p->state != PAGER_IDLE) {
        return DB_MISUSE;
    }
    p->journaled = calloc(p->page_count / 8 + 1, 1);
    if (p->journaled == NULL) {
        return DB_NO_MEMORY;
    }
    db_status rc = journal_create(&p->journal, p->journal_path);
    if (rc != DB_OK) {
        free(p->journaled);
        p->journaled = NULL;
        return rc;
    }
    p->orig_page_count = p->page_count;
    p->state = PAGER_WRITING;
    return DB_OK;
}

static void end_transaction(pager *p) {
    free(p->journaled);
    p->journaled = NULL;
    p->state = PAGER_IDLE;
}

db_status pager_commit(pager *p) {
    if (p->state == PAGER_ERROR) {
        return DB_IO_ERROR;
    }
    if (p->state != PAGER_WRITING) {
        return DB_MISUSE;
    }
    for (size_t i = 0; i < p->frame_count; i++) {
        page *pg = &p->frames[i];
        if (pg->in_use && pg->dirty) {
            db_status rc = write_back(p, pg);
            if (rc != DB_OK) {
                return rc;
            }
        }
    }
    db_status rc = file_sync(p->file);
    if (rc != DB_OK) {
        return fail(p, rc);
    }
    /*
     * 删除日志就是提交点（commit point）。删除之前崩溃，下次打开时
     * 日志还在，事务被撤销；删除之后，事务才算生效。
     */
    journal_close(&p->journal);
    rc = file_remove(p->journal_path);
    if (rc != DB_OK) {
        return fail(p, rc);
    }
    end_transaction(p);
    return DB_OK;
}

/*
 * 用日志把数据库文件恢复成事务开始前的样子，然后删除日志。恢复写完
 * 并同步之后才删日志：在这之前崩溃，下次打开会再回放一遍。回放只是把
 * 同样的原始页再写一次，重复执行没有害处。
 */
static db_status replay_journal(pager *p, uint32_t *restored) {
    db_status rc = journal_playback(p->journal_path, p->file, restored);
    if (rc == DB_OK) {
        rc = file_sync(p->file);
    }
    if (rc == DB_OK) {
        rc = file_remove(p->journal_path);
    }
    return rc;
}

static db_status load_header(pager *p) {
    uint8_t data[DB_PAGE_SIZE];
    db_status rc = read_page(p, 0, data);
    if (rc != DB_OK) {
        return rc;
    }
    uint32_t page_size = get_u32(data + HEADER_PAGE_SIZE_OFFSET);
    uint32_t count = get_u32(data + HEADER_PAGE_COUNT_OFFSET);
    if (memcmp(data, header_magic, HEADER_MAGIC_SIZE) != 0 ||
        page_size != DB_PAGE_SIZE || count < 1 || count > DB_MAX_PAGES) {
        return DB_CORRUPT;
    }
    p->page_count = count;
    return DB_OK;
}

db_status pager_rollback(pager *p) {
    if (p->state == PAGER_IDLE) {
        return DB_MISUSE;
    }
    if (pager_pinned(p) > 0) {
        return DB_MISUSE; /* 还有人拿着页的指针，不能把缓存清掉 */
    }
    journal_close(&p->journal);
    /*
     * 缓存里的页要么改过，要么可能是"换出后又读回来"的修改后版本，
     * 一律丢弃，之后以恢复好的文件为准。
     */
    for (size_t i = 0; i < p->frame_count; i++) {
        p->frames[i].in_use = false;
        p->frames[i].dirty = false;
    }
    uint32_t restored;
    db_status rc = replay_journal(p, &restored);
    if (rc == DB_OK) {
        rc = load_header(p);
    }
    if (rc != DB_OK) {
        p->state = PAGER_ERROR;
        return rc;
    }
    end_transaction(p);
    return DB_OK;
}

/* 新库的文件头：魔数、页大小、页数 1，其余字节为 0。 */
static void fresh_header(uint8_t *data) {
    memset(data, 0, DB_PAGE_SIZE);
    memcpy(data, header_magic, HEADER_MAGIC_SIZE);
    put_u32(data + HEADER_PAGE_SIZE_OFFSET, DB_PAGE_SIZE);
    put_u32(data + HEADER_PAGE_COUNT_OFFSET, 1);
}

/*
 * 新数据库：直接写出文件头。文件里还没有数据，不需要日志保护；写到
 * 一半被打断，下次打开时 open_file 认得出来，会重写一遍。
 */
static db_status init_header(pager *p) {
    uint8_t data[DB_PAGE_SIZE];
    fresh_header(data);
    db_status rc = file_write_at(p->file, 0, data, DB_PAGE_SIZE);
    if (rc == DB_OK) {
        rc = file_sync(p->file);
    }
    if (rc == DB_OK) {
        p->page_count = 1;
    }
    return rc;
}

static db_status open_file(pager *p, const char *path, uint32_t *recovered) {
    db_status rc = file_exists(path) ? file_open(path, &p->file)
                                     : file_create(path, &p->file);
    if (rc != DB_OK) {
        return rc;
    }
    uint8_t data[DB_PAGE_SIZE];
    uint8_t fresh[DB_PAGE_SIZE];
    size_t n;
    rc = file_read_at(p->file, 0, data, DB_PAGE_SIZE, &n);
    if (rc != DB_OK) {
        return rc;
    }
    fresh_header(fresh);
    bool has_journal = file_exists(p->journal_path);
    if (n < DB_PAGE_SIZE && memcmp(data, fresh, n) == 0) {
        /*
         * 不足一页，已有的字节又都与新库的文件头相同：上次新建数据库时，
         * 文件头还没写完进程就中断了（空文件是 n 为 0 的情形）。库里还
         * 没有数据，重写文件头即可。旁边若有日志，只能是与它无关的旧文件。
         */
        if (has_journal) {
            rc = file_remove(p->journal_path);
        }
        return rc == DB_OK ? init_header(p) : rc;
    }
    if (has_journal) {
        /* 热日志（hot journal）：上次有事务没提交就中断了。 */
        rc = replay_journal(p, recovered);
        if (rc != DB_OK) {
            return rc;
        }
    }
    return load_header(p);
}

db_status pager_open(const char *path, size_t frames, pager **out,
                     uint32_t *recovered) {
    *out = NULL;
    *recovered = 0;
    if (frames < PAGER_MIN_FRAMES) {
        frames = PAGER_MIN_FRAMES;
    }
    pager *p = calloc(1, sizeof *p);
    if (p == NULL) {
        return DB_NO_MEMORY;
    }
    db_status rc = DB_NO_MEMORY;
    p->journal_path = concat(path, "-journal");
    p->frames = calloc(frames, sizeof *p->frames);
    p->frame_count = frames;
    if (p->journal_path != NULL && p->frames != NULL) {
        rc = open_file(p, path, recovered);
    }
    if (rc != DB_OK) {
        pager_close(p);
        return rc;
    }
    *out = p;
    return DB_OK;
}

void pager_close(pager *p) {
    if (p == NULL) {
        return;
    }
    if (p->state != PAGER_IDLE) {
        /* 回滚失败也不要紧：日志留在磁盘上，下次打开时恢复。 */
        (void)pager_rollback(p);
    }
    journal_close(&p->journal);
    file_close(p->file);
    free(p->journaled);
    free(p->frames);
    free(p->journal_path);
    free(p);
}

bool pager_in_transaction(const pager *p) { return p->state != PAGER_IDLE; }

uint32_t pager_page_count(const pager *p) { return p->page_count; }

size_t pager_frame_count(const pager *p) { return p->frame_count; }

size_t pager_pinned(const pager *p) {
    size_t n = 0;
    for (size_t i = 0; i < p->frame_count; i++) {
        if (p->frames[i].in_use && p->frames[i].pins > 0) {
            n++;
        }
    }
    return n;
}

pager_stats pager_get_stats(const pager *p) { return p->stats; }
