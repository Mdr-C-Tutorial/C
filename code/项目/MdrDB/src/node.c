/*
 * node.c：节点页面的读写。布局见 node.h。
 */
#include "node.h"

#include <inttypes.h>
#include <string.h>

#define KIND_OFFSET 0
#define COUNT_OFFSET 2
#define CONTENT_OFFSET 4
#define LINK_OFFSET 8

/* C11：内容区起点用 16 位存放，页不能大于 65535 字节。 */
_Static_assert(DB_PAGE_SIZE <= 65535, "page offsets are stored in 16 bits");

static size_t content_start(const uint8_t *node) {
    return get_u16(node + CONTENT_OFFSET);
}

static uint8_t *pointer_slot(uint8_t *node, int i) {
    return node + NODE_HEADER_SIZE + CELL_POINTER_SIZE * (size_t)i;
}

static size_t pointer_at(const uint8_t *node, int i) {
    return get_u16(node + NODE_HEADER_SIZE + CELL_POINTER_SIZE * (size_t)i);
}

void node_init(uint8_t *node, node_kind kind) {
    memset(node, 0, DB_PAGE_SIZE);
    node[KIND_OFFSET] = (uint8_t)kind;
    put_u16(node + CONTENT_OFFSET, DB_PAGE_SIZE); /* 内容区为空 */
}

bool node_is_leaf(const uint8_t *node) {
    return node[KIND_OFFSET] == NODE_LEAF;
}

int node_cell_count(const uint8_t *node) {
    return get_u16(node + COUNT_OFFSET);
}

uint32_t node_link(const uint8_t *node) { return get_u32(node + LINK_OFFSET); }

void node_set_link(uint8_t *node, uint32_t pgno) {
    put_u32(node + LINK_OFFSET, pgno);
}

size_t node_free_space(const uint8_t *node) {
    size_t pointers_end =
        NODE_HEADER_SIZE + CELL_POINTER_SIZE * (size_t)node_cell_count(node);
    return content_start(node) - pointers_end;
}

bool node_has_room(const uint8_t *node, size_t cell_size) {
    return node_free_space(node) >= cell_size + CELL_POINTER_SIZE;
}

/* 叶子单元格的长度写在单元格里，内部单元格是定长的。 */
static size_t cell_size_at(const uint8_t *node, size_t offset) {
    if (node_is_leaf(node)) {
        return LEAF_CELL_HEADER_SIZE + get_u16(node + offset + 4);
    }
    return INTERNAL_CELL_SIZE;
}

const uint8_t *node_cell(const uint8_t *node, int i) {
    return node + pointer_at(node, i);
}

size_t node_cell_size(const uint8_t *node, int i) {
    return cell_size_at(node, pointer_at(node, i));
}

uint32_t node_key(const uint8_t *node, int i) {
    return get_u32(node_cell(node, i));
}

int node_search(const uint8_t *node, uint32_t key, bool *found) {
    int count = node_cell_count(node);
    int lo = 0;
    int hi = count;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (node_key(node, mid) < key) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    *found = lo < count && node_key(node, lo) == key;
    return lo;
}

void node_insert_cell(uint8_t *node, int i, const uint8_t *cell, size_t size) {
    int count = node_cell_count(node);
    size_t start = content_start(node) - size;
    memcpy(node + start, cell, size);
    /* 指针数组里第 i 项及之后的项后移一格，空出第 i 项。 */
    memmove(pointer_slot(node, i + 1), pointer_slot(node, i),
            CELL_POINTER_SIZE * (size_t)(count - i));
    put_u16(pointer_slot(node, i), (uint16_t)start);
    put_u16(node + COUNT_OFFSET, (uint16_t)(count + 1));
    put_u16(node + CONTENT_OFFSET, (uint16_t)start);
}

void node_remove_cell(uint8_t *node, int i) {
    int count = node_cell_count(node);
    size_t start = content_start(node);
    size_t offset = pointer_at(node, i);
    size_t size = node_cell_size(node, i);

    /*
     * 让内容区保持连续：把地址低于被删单元格的内容整体后移 size 字节，
     * 再给指向这些内容的指针加上 size。空闲空间因此总是一整块，插入时
     * 不用整理碎片。代价是每次删除都要搬动数据，一页之内，搬得起。
     */
    memmove(node + start + size, node + start, offset - start);
    memset(node + start, 0, size);
    memmove(pointer_slot(node, i), pointer_slot(node, i + 1),
            CELL_POINTER_SIZE * (size_t)(count - i - 1));
    memset(pointer_slot(node, count - 1), 0, CELL_POINTER_SIZE);
    for (int k = 0; k < count - 1; k++) {
        size_t p = pointer_at(node, k);
        if (p < offset) {
            put_u16(pointer_slot(node, k), (uint16_t)(p + size));
        }
    }
    put_u16(node + COUNT_OFFSET, (uint16_t)(count - 1));
    put_u16(node + CONTENT_OFFSET, (uint16_t)(start + size));
}

size_t leaf_cell_build(uint8_t *cell, uint32_t key, const uint8_t *payload,
                       size_t size) {
    put_u32(cell, key);
    put_u16(cell + 4, (uint16_t)size);
    if (size > 0) {
        memcpy(cell + LEAF_CELL_HEADER_SIZE, payload, size);
    }
    return LEAF_CELL_HEADER_SIZE + size;
}

const uint8_t *leaf_payload(const uint8_t *node, int i, size_t *size) {
    const uint8_t *cell = node_cell(node, i);
    *size = get_u16(cell + 4);
    return cell + LEAF_CELL_HEADER_SIZE;
}

void internal_cell_build(uint8_t *cell, uint32_t key, uint32_t child) {
    put_u32(cell, key);
    put_u32(cell + 4, child);
}

uint32_t internal_child(const uint8_t *node, int i) {
    if (i == node_cell_count(node)) {
        return node_link(node);
    }
    return get_u32(node_cell(node, i) + 4);
}

void internal_set_child(uint8_t *node, int i, uint32_t child) {
    if (i == node_cell_count(node)) {
        node_set_link(node, child);
    } else {
        put_u32(node + pointer_at(node, i) + 4, child);
    }
}

/*
 * 检查页头和每个单元格都在页内，内容区紧凑。磁盘上的字节不可信：
 * B+ 树每取一页都先过这一关，之后的解析才不会越界。
 */
bool node_validate(const uint8_t *node) {
    if (node[KIND_OFFSET] != NODE_LEAF && node[KIND_OFFSET] != NODE_INTERNAL) {
        return false;
    }
    int count = node_cell_count(node);
    size_t start = content_start(node);
    size_t pointers_end = NODE_HEADER_SIZE + CELL_POINTER_SIZE * (size_t)count;
    if (pointers_end > start || start > DB_PAGE_SIZE) {
        return false;
    }
    size_t header =
        node_is_leaf(node) ? LEAF_CELL_HEADER_SIZE : INTERNAL_CELL_SIZE;
    size_t used = 0;
    for (int i = 0; i < count; i++) {
        size_t offset = pointer_at(node, i);
        if (offset < start || offset + header > DB_PAGE_SIZE) {
            return false;
        }
        size_t size = cell_size_at(node, offset);
        if (offset + size > DB_PAGE_SIZE) {
            return false;
        }
        used += size;
    }
    /* 内容区紧凑：所有单元格加起来正好填满 [start, 页尾)。 */
    return used == DB_PAGE_SIZE - start;
}

/*
 * 页内的偏移和长度都小于 65536，转成 unsigned 打印。不用 %zu：
 * MinGW 的 gcc 按微软 C 运行库的规则检查格式串，会对 %zu 报警告。
 */
void node_print(const uint8_t *node, uint32_t pgno, FILE *out) {
    bool leaf = node_is_leaf(node);
    int count = node_cell_count(node);
    fprintf(out, "page %" PRIu32 ": %s\n", pgno, leaf ? "leaf" : "internal");
    fprintf(
        out, "  cells %d, free bytes %u, content start %u, %s %" PRIu32 "\n",
        count, (unsigned)node_free_space(node), (unsigned)content_start(node),
        leaf ? "next leaf" : "right child", node_link(node));
    for (int i = 0; i < count; i++) {
        fprintf(out, "  cell %d: offset %u, size %u, key %" PRIu32, i,
                (unsigned)pointer_at(node, i),
                (unsigned)node_cell_size(node, i), node_key(node, i));
        if (!leaf) {
            fprintf(out, ", child %" PRIu32, internal_child(node, i));
        }
        fputc('\n', out);
    }
}
