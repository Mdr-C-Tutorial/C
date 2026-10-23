/*
 * btree.c：B+ 树的查找、插入、删除、遍历和检查。
 *
 * 插入先从根下降到叶子，记下经过的路径。叶子装不下时一分为二，把
 * 分隔键插进父节点；父节点也装不下，就沿路径继续向上分裂。根装不下时
 * 树长高一层：根的内容搬到新页，根变成指向新页的内部节点。根的页号
 * 因此始终是 BTREE_ROOT，不需要另外记录。
 *
 * 删除只从叶子里去掉单元格，不合并页面，叶子可能变空。内部节点里的
 * 分隔键仍然正确地划分着键的范围，查找和插入不受影响。
 */
#include "btree.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

/* 从根到叶子的路径。插入引起分裂时，要沿着它逐层修改父节点。 */
typedef struct tree_path {
    int depth;                      /* 层数，pgno[depth - 1] 是叶子 */
    uint32_t pgno[BTREE_MAX_DEPTH]; /* 每一层经过的页 */
    int slot[BTREE_MAX_DEPTH];      /* 每一层走的是第几个孩子 */
} tree_path;

/*
 * 叶子单元格的负载不超过 BTREE_MAX_PAYLOAD。插入时检查过的数据一定
 * 满足这一条，损坏的文件却未必；分裂算法要靠它保证两半都装得下（见
 * split_point），所以取页时要与页内布局一起检查。
 */
static bool payloads_fit(const uint8_t *node) {
    if (!node_is_leaf(node)) {
        return true; /* 内部单元格是定长的 */
    }
    for (int i = 0; i < node_cell_count(node); i++) {
        size_t size;
        (void)leaf_payload(node, i, &size);
        if (size > BTREE_MAX_PAYLOAD) {
            return false;
        }
    }
    return true;
}

/* 取一页并确认它是格式正确的节点。文件可能损坏，先验证再解析。 */
static db_status load_node(pager *p, uint32_t pgno, page **out) {
    if (pgno == 0) {
        return DB_CORRUPT; /* 第 0 页是文件头，不会是树节点 */
    }
    db_status rc = pager_get(p, pgno, out);
    if (rc != DB_OK) {
        return rc;
    }
    if (!node_validate((*out)->data) || !payloads_fit((*out)->data)) {
        pager_unpin(*out);
        *out = NULL;
        return DB_CORRUPT;
    }
    return DB_OK;
}

/*
 * 在内部节点里为 key 选孩子。第 i 个孩子里的键都小于第 i 个分隔键，
 * 且不小于第 i - 1 个分隔键；等于分隔键的 key 属于它右边的孩子。
 */
static int child_slot(const uint8_t *node, uint32_t key) {
    bool found;
    int i = node_search(node, key, &found);
    return found ? i + 1 : i;
}

static db_status find_leaf(pager *p, uint32_t key, tree_path *path) {
    uint32_t pgno = BTREE_ROOT;
    for (int level = 0; level < BTREE_MAX_DEPTH; level++) {
        page *pg;
        db_status rc = load_node(p, pgno, &pg);
        if (rc != DB_OK) {
            return rc;
        }
        path->pgno[level] = pgno;
        if (node_is_leaf(pg->data)) {
            path->depth = level + 1;
            pager_unpin(pg);
            return DB_OK;
        }
        path->slot[level] = child_slot(pg->data, key);
        pgno = internal_child(pg->data, path->slot[level]);
        pager_unpin(pg);
    }
    return DB_CORRUPT; /* 树不可能这么深，多半是页之间形成了环 */
}

static uint32_t leaf_of(const tree_path *path) {
    return path->pgno[path->depth - 1];
}

db_status btree_create(pager *p) {
    page *root;
    db_status rc = pager_allocate(p, &root);
    if (rc != DB_OK) {
        return rc;
    }
    if (root->pgno == BTREE_ROOT) {
        node_init(root->data, NODE_LEAF);
    } else {
        rc = DB_MISUSE; /* 只能在只有文件头的新库上建树 */
    }
    pager_unpin(root);
    return rc;
}

db_status btree_find(pager *p, uint32_t key, uint8_t *payload, size_t *size) {
    tree_path path;
    db_status rc = find_leaf(p, key, &path);
    if (rc != DB_OK) {
        return rc;
    }
    page *leaf;
    rc = load_node(p, leaf_of(&path), &leaf);
    if (rc != DB_OK) {
        return rc;
    }
    bool found;
    int i = node_search(leaf->data, key, &found);
    if (!found) {
        rc = DB_NOT_FOUND;
    } else {
        const uint8_t *src = leaf_payload(leaf->data, i, size);
        if (*size > BTREE_MAX_PAYLOAD) {
            rc = DB_CORRUPT;
        } else {
            memcpy(payload, src, *size);
        }
    }
    pager_unpin(leaf);
    return rc;
}

/* ---------------------------------------------------------------- 插入 */

/* 节点放得下就把 cell 插到第 index 个位置，*done 置为 true。 */
static db_status insert_if_room(pager *p, uint32_t pgno, int index,
                                const uint8_t *cell, size_t size, bool *done) {
    *done = false;
    page *pg;
    db_status rc = load_node(p, pgno, &pg);
    if (rc != DB_OK) {
        return rc;
    }
    if (node_has_room(pg->data, size)) {
        rc = pager_write(p, pg);
        if (rc == DB_OK) {
            node_insert_cell(pg->data, index, cell, size);
            *done = true;
        }
    }
    pager_unpin(pg);
    return rc;
}

/*
 * 分裂用的临时区：节点原有的单元格加上新单元格，按键的顺序排好。
 * 合计可能超过一页，所以不能在页面里原地操作。
 */
typedef struct cell_list {
    int count;
    size_t used;
    uint16_t offset[NODE_MAX_CELLS + 1];
    uint16_t size[NODE_MAX_CELLS + 1];
    uint8_t bytes[2 * DB_PAGE_SIZE];
} cell_list;

static void cell_list_add(cell_list *list, const uint8_t *cell, size_t size) {
    memcpy(list->bytes + list->used, cell, size);
    list->offset[list->count] = (uint16_t)list->used;
    list->size[list->count] = (uint16_t)size;
    list->count++;
    list->used += size;
}

static void collect_cells(const uint8_t *node, int index, const uint8_t *cell,
                          size_t size, cell_list *list) {
    int count = node_cell_count(node);
    list->count = 0;
    list->used = 0;
    for (int i = 0; i <= count; i++) {
        if (i == index) {
            cell_list_add(list, cell, size);
        }
        if (i < count) {
            cell_list_add(list, node_cell(node, i), node_cell_size(node, i));
        }
    }
}

static uint32_t list_key(const cell_list *list, int i) {
    return get_u32(list->bytes + list->offset[i]);
}

/* 内部单元格里键后面是孩子页号。 */
static uint32_t list_child(const cell_list *list, int i) {
    return get_u32(list->bytes + list->offset[i] + 4);
}

/*
 * 选分裂点 m：左半拿前 m 个单元格，m 取使左半刚好达到总字节数一半的
 * 位置。单个单元格不超过可用空间的 1/4，可以证明两半都装得下。
 */
static int split_point(const cell_list *list) {
    size_t total = 0;
    for (int i = 0; i < list->count; i++) {
        total += list->size[i] + CELL_POINTER_SIZE;
    }
    size_t left = 0;
    int m = 0;
    while (2 * left < total) {
        left += list->size[m] + CELL_POINTER_SIZE;
        m++;
    }
    return m;
}

static void fill_node(uint8_t *node, node_kind kind, const cell_list *list,
                      int from, int to) {
    node_init(node, kind);
    for (int i = from; i < to; i++) {
        /* split_point 保证放得下；断言把这条推理写进代码。 */
        assert(node_has_room(node, list->size[i]));
        node_insert_cell(node, i - from, list->bytes + list->offset[i],
                         list->size[i]);
    }
}

/*
 * 把放不下新单元格的节点 pgno 一分为二：pgno 留左半，新页放右半。
 * *right_pgno 返回新页的页号，*separator 返回要插进父节点的分隔键。
 */
static db_status split_node(pager *p, uint32_t pgno, int index,
                            const uint8_t *cell, size_t size,
                            uint32_t *right_pgno, uint32_t *separator) {
    page *left;
    page *right;
    db_status rc = load_node(p, pgno, &left);
    if (rc != DB_OK) {
        return rc;
    }
    rc = pager_write(p, left);
    if (rc == DB_OK) {
        rc = pager_allocate(p, &right);
    }
    if (rc != DB_OK) {
        pager_unpin(left);
        return rc;
    }

    cell_list list;
    collect_cells(left->data, index, cell, size, &list);
    int m = split_point(&list);
    uint32_t link = node_link(left->data);
    if (node_is_leaf(left->data)) {
        /* 叶子：右半的第一个键复制一份上去当分隔键，这一行仍留在右叶。 */
        fill_node(left->data, NODE_LEAF, &list, 0, m);
        fill_node(right->data, NODE_LEAF, &list, m, list.count);
        node_set_link(right->data, link);       /* 右叶接替原来的右邻 */
        node_set_link(left->data, right->pgno); /* 左叶的右邻是新叶 */
    } else {
        /* 内部节点：第 m 个单元格整个移进父节点，它的孩子归左半。 */
        fill_node(left->data, NODE_INTERNAL, &list, 0, m);
        node_set_link(left->data, list_child(&list, m));
        fill_node(right->data, NODE_INTERNAL, &list, m + 1, list.count);
        node_set_link(right->data, link);
    }
    *separator = list_key(&list, m);
    *right_pgno = right->pgno;
    pager_unpin(right);
    pager_unpin(left);
    return DB_OK;
}

/*
 * 根装不下时先让树长高一层：根的全部内容搬到新页，根变成没有键、
 * 只有一个孩子（就是新页）的内部节点。*child_pgno 返回新页的页号。
 */
static db_status grow_root(pager *p, uint32_t *child_pgno) {
    page *root;
    page *child;
    db_status rc = load_node(p, BTREE_ROOT, &root);
    if (rc != DB_OK) {
        return rc;
    }
    rc = pager_write(p, root);
    if (rc == DB_OK) {
        rc = pager_allocate(p, &child);
    }
    if (rc == DB_OK) {
        memcpy(child->data, root->data, DB_PAGE_SIZE);
        node_init(root->data, NODE_INTERNAL);
        node_set_link(root->data, child->pgno);
        *child_pgno = child->pgno;
        pager_unpin(child);
    }
    pager_unpin(root);
    return rc;
}

static db_status set_child(pager *p, uint32_t pgno, int slot, uint32_t child) {
    page *pg;
    db_status rc = load_node(p, pgno, &pg);
    if (rc != DB_OK) {
        return rc;
    }
    rc = pager_write(p, pg);
    if (rc == DB_OK) {
        internal_set_child(pg->data, slot, child);
    }
    pager_unpin(pg);
    return rc;
}

/* 把 cell 插入路径第 level 层节点的第 index 个位置，必要时向上分裂。 */
static db_status insert_cell(pager *p, const tree_path *path, int level,
                             int index, const uint8_t *cell, size_t size) {
    bool done;
    db_status rc =
        insert_if_room(p, path->pgno[level], index, cell, size, &done);
    if (rc != DB_OK || done) {
        return rc;
    }

    uint32_t node = path->pgno[level];
    uint32_t parent = BTREE_ROOT;
    int slot = 0;
    if (level == 0) {
        rc = grow_root(p, &node); /* 之后根就是 node 的父节点 */
    } else {
        parent = path->pgno[level - 1];
        slot = path->slot[level - 1];
    }
    uint32_t right = 0;
    uint32_t separator = 0;
    if (rc == DB_OK) {
        rc = split_node(p, node, index, cell, size, &right, &separator);
    }
    /*
     * 父节点里原来指向 node 的位置改指右半，再在它前面插入
     * (separator, node)：
     *     ... [slot] -> node ...
     *  => ... (separator, node) [slot + 1] -> right ...
     * 先改指针再插入：插入若引起父节点分裂，这个指针会随单元格一起搬走。
     */
    if (rc == DB_OK) {
        rc = set_child(p, parent, slot, right);
    }
    if (rc != DB_OK) {
        return rc;
    }
    uint8_t parent_cell[INTERNAL_CELL_SIZE];
    internal_cell_build(parent_cell, separator, node);
    /* level 为 0 时，父节点是刚长高的根，里面是空的，一定插得进去。 */
    int parent_level = level > 0 ? level - 1 : 0;
    return insert_cell(p, path, parent_level, slot, parent_cell,
                       sizeof parent_cell);
}

db_status btree_insert(pager *p, uint32_t key, const uint8_t *payload,
                       size_t size) {
    if (size > BTREE_MAX_PAYLOAD) {
        return DB_TOO_LONG;
    }
    tree_path path;
    db_status rc = find_leaf(p, key, &path);
    if (rc != DB_OK) {
        return rc;
    }
    page *leaf;
    rc = load_node(p, leaf_of(&path), &leaf);
    if (rc != DB_OK) {
        return rc;
    }
    bool found;
    int index = node_search(leaf->data, key, &found);
    pager_unpin(leaf);
    if (found) {
        return DB_DUPLICATE; /* 这时还没有修改任何页 */
    }
    uint8_t cell[LEAF_CELL_HEADER_SIZE + BTREE_MAX_PAYLOAD];
    size_t cell_size = leaf_cell_build(cell, key, payload, size);
    return insert_cell(p, &path, path.depth - 1, index, cell, cell_size);
}

/* ---------------------------------------------------------------- 删除 */

db_status btree_delete(pager *p, uint32_t key) {
    tree_path path;
    db_status rc = find_leaf(p, key, &path);
    if (rc != DB_OK) {
        return rc;
    }
    page *leaf;
    rc = load_node(p, leaf_of(&path), &leaf);
    if (rc != DB_OK) {
        return rc;
    }
    bool found;
    int index = node_search(leaf->data, key, &found);
    if (!found) {
        rc = DB_NOT_FOUND;
    } else {
        rc = pager_write(p, leaf);
        if (rc == DB_OK) {
            node_remove_cell(leaf->data, index);
        }
    }
    pager_unpin(leaf);
    return rc;
}

/* ---------------------------------------------------------------- 游标 */

/*
 * 让游标停在一个实际存在的单元格上。当前叶子走完了就沿 link 去右边的
 * 叶子；删除不合并页面，链上可能有空叶子，所以要循环。
 *
 * 文件损坏时，叶子链可能绕回前面的叶子，遍历就永远走不完。两项检查
 * 保证它一定结束：连续走过的空叶子不能比总页数还多；每走一步，停下
 * 时的键都必须比上一个键大。
 */
static db_status settle(btree_cursor *c) {
    uint32_t steps = 0;
    for (;;) {
        page *pg;
        db_status rc = load_node(c->pager, c->leaf, &pg);
        if (rc != DB_OK) {
            return rc;
        }
        bool leaf = node_is_leaf(pg->data);
        int count = node_cell_count(pg->data);
        uint32_t next = node_link(pg->data);
        uint32_t key = c->index < count ? node_key(pg->data, c->index) : 0;
        pager_unpin(pg);
        if (!leaf) {
            return DB_CORRUPT;
        }
        if (c->index < count) {
            if (c->valid && key <= c->key) {
                return DB_CORRUPT; /* 键没有变大：叶子链接错了或成了环 */
            }
            c->key = key;
            c->valid = true;
            return DB_OK;
        }
        if (next == 0) {
            c->valid = false;
            return DB_OK;
        }
        if (++steps >= pager_page_count(c->pager)) {
            return DB_CORRUPT; /* 走过的叶子比总页数还多：链表成了环 */
        }
        c->leaf = next;
        c->index = 0;
    }
}

db_status btree_seek(pager *p, uint32_t key, btree_cursor *c) {
    c->pager = p;
    c->key = 0;
    c->valid = false;
    tree_path path;
    db_status rc = find_leaf(p, key, &path);
    if (rc != DB_OK) {
        return rc;
    }
    page *leaf;
    rc = load_node(p, leaf_of(&path), &leaf);
    if (rc != DB_OK) {
        return rc;
    }
    bool found;
    c->leaf = leaf_of(&path);
    c->index = node_search(leaf->data, key, &found);
    pager_unpin(leaf);
    return settle(c);
}

db_status btree_next(btree_cursor *c) {
    if (!c->valid) {
        return DB_MISUSE;
    }
    c->index++;
    return settle(c);
}

db_status btree_read(const btree_cursor *c, uint32_t *key, uint8_t *payload,
                     size_t *size) {
    if (!c->valid) {
        return DB_MISUSE;
    }
    page *leaf;
    db_status rc = load_node(c->pager, c->leaf, &leaf);
    if (rc != DB_OK) {
        return rc;
    }
    if (!node_is_leaf(leaf->data) || c->index >= node_cell_count(leaf->data)) {
        rc = DB_MISUSE; /* 游标创建之后树被修改过 */
    } else {
        *key = node_key(leaf->data, c->index);
        const uint8_t *src = leaf_payload(leaf->data, c->index, size);
        if (*size > BTREE_MAX_PAYLOAD) {
            rc = DB_CORRUPT;
        } else {
            memcpy(payload, src, *size);
        }
    }
    pager_unpin(leaf);
    return rc;
}

/* ---------------------------------------------------------------- 检查 */

typedef struct check_state {
    pager *pager;
    uint8_t *seen;      /* 位图：已经访问过的页 */
    uint32_t visited;   /* 访问过的页数 */
    int leaf_level;     /* 第一个叶子所在的层，-1 表示还没遇到 */
    uint32_t last_leaf; /* 按键的顺序，上一个访问的叶子 */
    uint32_t last_link; /* 上一个叶子记录的右邻 */
    btree_stats stats;
    char *message;
    size_t message_size;
} check_state;

static db_status corrupt(check_state *s, uint32_t pgno, const char *what) {
    snprintf(s->message, s->message_size, "page %" PRIu32 ": %s", pgno, what);
    return DB_CORRUPT;
}

/* 单元格之间不能重叠：按字节建一张占用表，逐个标记。 */
static bool cells_overlap(const uint8_t *node) {
    bool used[DB_PAGE_SIZE] = {false};
    for (int i = 0; i < node_cell_count(node); i++) {
        size_t start = (size_t)(node_cell(node, i) - node);
        size_t end = start + node_cell_size(node, i);
        for (size_t b = start; b < end; b++) {
            if (used[b]) {
                return true;
            }
            used[b] = true;
        }
    }
    return false;
}

static const char *node_problem(const uint8_t *node, uint64_t low,
                                uint64_t high) {
    if (!node_validate(node)) {
        return "bad page layout";
    }
    if (!payloads_fit(node)) {
        return "payload is too long";
    }
    if (cells_overlap(node)) {
        return "cells overlap";
    }
    int count = node_cell_count(node);
    if (!node_is_leaf(node) && count == 0) {
        return "internal node has no keys";
    }
    for (int i = 0; i < count; i++) {
        uint32_t key = node_key(node, i);
        if (key < low || key >= high) {
            return "key outside the range given by the parent";
        }
        if (i > 0 && key <= node_key(node, i - 1)) {
            return "keys are not in ascending order";
        }
    }
    return NULL;
}

/*
 * 检查以 pgno 为根的子树：其中的键都落在 [low, high) 内。high 用 64 位
 * 存放，UINT32_MAX + 1 表示没有上界。
 */
static db_status check_node(check_state *s, uint32_t pgno, int level,
                            uint64_t low, uint64_t high) {
    if (level >= BTREE_MAX_DEPTH) {
        return corrupt(s, pgno, "tree is too deep");
    }
    if (pgno == 0 || pgno >= pager_page_count(s->pager)) {
        return corrupt(s, pgno, "page number out of range");
    }
    if ((s->seen[pgno / 8] >> (pgno % 8)) & 1u) {
        return corrupt(s, pgno, "page is referenced twice");
    }
    s->seen[pgno / 8] = (uint8_t)(s->seen[pgno / 8] | (1u << (pgno % 8)));
    s->visited++;

    page *pg;
    db_status rc = pager_get(s->pager, pgno, &pg);
    if (rc != DB_OK) {
        return rc;
    }
    const char *problem = node_problem(pg->data, low, high);
    bool leaf = node_is_leaf(pg->data);
    int count = node_cell_count(pg->data);
    uint32_t link = node_link(pg->data);
    pager_unpin(pg);
    if (problem != NULL) {
        return corrupt(s, pgno, problem);
    }

    if (leaf) {
        if (s->leaf_level < 0) {
            s->leaf_level = level;
        } else if (level != s->leaf_level) {
            return corrupt(s, pgno, "leaves are at different depths");
        }
        /* 叶子是按键的顺序访问的，叶子链必须与这个顺序一致。 */
        if (s->last_leaf != 0 && s->last_link != pgno) {
            return corrupt(s, s->last_leaf, "leaf chain skips a leaf");
        }
        s->last_leaf = pgno;
        s->last_link = link;
        s->stats.leaves++;
        s->stats.keys += (uint32_t)count;
        return DB_OK;
    }

    s->stats.internals++;
    for (int i = 0; i <= count; i++) {
        /* 递归期间不钉住本页，每个孩子都重新取一次，通常命中缓存。 */
        rc = pager_get(s->pager, pgno, &pg);
        if (rc != DB_OK) {
            return rc;
        }
        uint32_t child = internal_child(pg->data, i);
        uint64_t child_low = i == 0 ? low : node_key(pg->data, i - 1);
        uint64_t child_high = i == count ? high : node_key(pg->data, i);
        pager_unpin(pg);
        rc = check_node(s, child, level + 1, child_low, child_high);
        if (rc != DB_OK) {
            return rc;
        }
    }
    return DB_OK;
}

db_status btree_check(pager *p, btree_stats *stats, char *message,
                      size_t message_size) {
    check_state s;
    memset(&s, 0, sizeof s);
    s.pager = p;
    s.leaf_level = -1;
    s.message = message;
    s.message_size = message_size;
    message[0] = '\0';
    uint32_t pages = pager_page_count(p);
    s.seen = calloc(pages / 8 + 1, 1);
    if (s.seen == NULL) {
        return DB_NO_MEMORY;
    }
    db_status rc = check_node(&s, BTREE_ROOT, 0, 0, (uint64_t)UINT32_MAX + 1);
    if (rc == DB_OK && s.last_link != 0) {
        rc = corrupt(&s, s.last_leaf, "last leaf points to another leaf");
    }
    if (rc == DB_OK && s.visited != pages - 1) {
        /* 除文件头外每一页都属于这棵树：MdrDB 从不释放页。 */
        snprintf(message, message_size,
                 "%" PRIu32 " pages are not part of the tree",
                 pages - 1 - s.visited);
        rc = DB_CORRUPT;
    }
    free(s.seen);
    if (rc == DB_OK) {
        s.stats.height = (uint32_t)s.leaf_level + 1;
        *stats = s.stats;
    }
    return rc;
}

/* ---------------------------------------------------------------- 打印 */

static db_status print_node(pager *p, uint32_t pgno, int level, FILE *out) {
    if (level >= BTREE_MAX_DEPTH) {
        return DB_CORRUPT;
    }
    page *pg;
    db_status rc = load_node(p, pgno, &pg);
    if (rc != DB_OK) {
        return rc;
    }
    bool leaf = node_is_leaf(pg->data);
    int count = node_cell_count(pg->data);
    fprintf(out, "%*s%s %" PRIu32 ": %d key%s", level * 2, "",
            leaf ? "leaf" : "internal", pgno, count, count == 1 ? "" : "s");
    if (leaf && count > 0) {
        fprintf(out, " (%" PRIu32 "..%" PRIu32 ")", node_key(pg->data, 0),
                node_key(pg->data, count - 1));
    }
    fputc('\n', out);
    pager_unpin(pg);

    for (int i = 0; !leaf && i <= count; i++) {
        rc = load_node(p, pgno, &pg);
        if (rc != DB_OK) {
            return rc;
        }
        uint32_t child = internal_child(pg->data, i);
        uint32_t key = i < count ? node_key(pg->data, i) : 0;
        pager_unpin(pg);
        rc = print_node(p, child, level + 1, out);
        if (rc != DB_OK) {
            return rc;
        }
        if (i < count) {
            fprintf(out, "%*skey %" PRIu32 "\n", (level + 1) * 2, "", key);
        }
    }
    return DB_OK;
}

db_status btree_print(pager *p, FILE *out) {
    return print_node(p, BTREE_ROOT, 0, out);
}
