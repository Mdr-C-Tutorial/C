/* 9.8 习题 50048 的起手代码：在主机上模拟页目录、进程与线程的地址空间。
 *
 * 编译：gcc -std=c17 -Wall -Wextra addrspace_starter.c -o addrspace
 *
 * 需要补全 pd_create、pd_map、pd_translate、pd_clone、pd_switch 与
 * addr_read32、addr_write32。模拟约定：
 *   - 物理内存有 NPAGES 个 4 KiB 页框，页框号 0 保留不用；
 *   - 页目录的每一项（PDE）保存页表的页框号，0 表示没有页表；
 *   - 页表项（PTE）低 12 位是标志，高位是页框号，格式与 x86 一致：
 *     bit0 存在、bit1 可写、bit2 用户态；
 *   - 内核区间 [0xC0000000, 0xFFFFFFFF) 在所有进程里映射到同一批页框，
 *     克隆页目录时这些 PDE 直接复制，不复制页表内容；
 *   - 用户区间的页表在克隆时要新建页表并复制页框内容（与 clone_directory 一致）。
 *
 * 不得用 malloc 之外的动态分配模拟页框；物理内存用一个静态二维数组。
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PAGE_SIZE 4096
#define NPAGES 32
#define KERNEL_BASE 0xC0000000u
#define USER_BASE 0x08000000u

#define PTE_PRESENT 0x1u
#define PTE_WRITABLE 0x2u
#define PTE_USER 0x4u

static uint8_t phys[NPAGES][PAGE_SIZE];
static int frame_used[NPAGES];
static uint32_t next_frame = 1; /* 0 号页框保留 */

struct page_table {
    uint32_t pte[1024];
};

struct page_directory {
    uint32_t pde[1024]; /* 页表的页框号，0 表示不存在 */
};

static struct page_directory *current_pd;

static uint32_t alloc_frame(void) {
    for (uint32_t i = next_frame; i < NPAGES; i++) {
        if (!frame_used[i]) {
            frame_used[i] = 1;
            next_frame = i + 1;
            memset(phys[i], 0, PAGE_SIZE);
            return i;
        }
    }
    return 0;
}

/* PDE 里存的是页框号，不是物理地址：0 表示没有页表。 */
static __attribute__((unused)) struct page_table *table_of(struct page_directory *d, uint32_t va, int make) {
    uint32_t idx = va >> 22;
    if (d->pde[idx] == 0 && make) {
        uint32_t f = alloc_frame();
        if (f == 0) {
            return NULL;
        }
        d->pde[idx] = f;
    }
    if (d->pde[idx] == 0) {
        return NULL;
    }
    return (struct page_table *)phys[d->pde[idx]];
}

/* TODO */
struct page_directory *pd_create(void) {
    return NULL;
}

/* TODO：把 va 所在的页映射到 frame，标志为 flags；需要时建页表。成功返回 0。 */
int pd_map(struct page_directory *d, uint32_t va, uint32_t frame, uint32_t flags) {
    (void)d;
    (void)va;
    (void)frame;
    (void)flags;
    return -1;
}

/* TODO：查页表。命中时把物理地址写入 *pa、PTE 低 12 位写入 *flags，返回 0；
 * 页表不存在或 PTE 的 present 位为 0 时返回 -1。 */
int pd_translate(struct page_directory *d, uint32_t va, uint32_t *pa, uint32_t *flags) {
    (void)d;
    (void)va;
    (void)pa;
    (void)flags;
    return -1;
}

/* TODO：深拷贝用户页表，内核区间的 PDE 直接共享。 */
struct page_directory *pd_clone(const struct page_directory *src) {
    (void)src;
    return NULL;
}

/* TODO：切换当前页目录。 */
void pd_switch(struct page_directory *d) {
    (void)d;
}

/* TODO：按当前页目录读一个 32 位字，失败返回 -1。 */
int addr_read32(uint32_t va, uint32_t *out) {
    (void)va;
    (void)out;
    return -1;
}

/* TODO：按当前页目录写一个 32 位字，失败返回 -1。 */
int addr_write32(uint32_t va, uint32_t value) {
    (void)va;
    (void)value;
    return -1;
}

static void report(const char *tag, uint32_t va) {
    uint32_t pa = 0, flags = 0, val = 0;
    int ok = pd_translate(current_pd, va, &pa, &flags);
    int rd = addr_read32(va, &val);
    printf("%-6s va=0x%08X %s", tag, va, ok == 0 ? "mapped" : "FAULT");
    if (ok == 0) {
        printf(" pa=0x%08X frame=%u flags=0x%X", pa, pa / PAGE_SIZE, flags);
    }
    if (rd == 0) {
        printf(" value=0x%08X", val);
    } else {
        printf(" read=FAULT");
    }
    printf("\n");
}

int main(void) {
    struct page_directory *kernel = pd_create();
    if (kernel == NULL) {
        printf("pd_create failed\n");
        return 1;
    }

    uint32_t kframe0 = alloc_frame();
    uint32_t kframe1 = alloc_frame();
    pd_map(kernel, KERNEL_BASE + 0x1000, kframe0, PTE_PRESENT | PTE_WRITABLE);
    pd_map(kernel, KERNEL_BASE + 0x2000, kframe1, PTE_PRESENT | PTE_WRITABLE);

    struct page_directory *a = pd_clone(kernel);
    struct page_directory *b = pd_clone(kernel);
    uint32_t fa = alloc_frame();
    uint32_t fb = alloc_frame();

    pd_map(a, USER_BASE, fa, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    pd_map(b, USER_BASE, fb, PTE_PRESENT | PTE_WRITABLE | PTE_USER);

    pd_switch(a);
    addr_write32(USER_BASE, 0x1111);
    report("A", USER_BASE);
    report("A", KERNEL_BASE + 0x1000);

    pd_switch(b);
    addr_write32(USER_BASE, 0x2222);
    report("B", USER_BASE);
    report("B", KERNEL_BASE + 0x1000);

    pd_switch(a);
    report("A", USER_BASE);
    report("A+thr", USER_BASE); /* A 与线程共用页目录，读到同一个值 */

    addr_write32(USER_BASE, 0x3333);
    report("A+thr", USER_BASE);

    pd_switch(b);
    report("B", USER_BASE);
    report("B", 0x09000000);
    report("B", USER_BASE + PAGE_SIZE);

    printf("frames used:");
    for (int i = 0; i < NPAGES; i++) {
        if (frame_used[i]) printf(" %d", i);
    }
    printf("\n");
    return 0;
}
