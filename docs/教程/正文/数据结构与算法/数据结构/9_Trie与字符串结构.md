# 9. Trie 与字符串结构

在散列表中查找长度为 $L$ 的字符串虽然期望耗时 $O(L)$，但散列表无法高效回答“有多少个键以 `pre` 为前缀”或按字典序枚举某个前缀下的所有键。而在平衡二叉搜索树中查找字符串，每次结点比较耗时 $O(L)$，总查找耗时为 $O(L \log N)$。**字典树 (Trie / Prefix Tree)** 利用字符串之间的公共前缀组织树形分支，使精确查找、插入、删除和前缀统计的时间复杂度均严格为 $O(L)$，且与已存单词总数 $N$ 无关。

## 1. Trie 的结构与转移模型

设字符集为 $\Sigma$（例如小写英文字母集 $|\Sigma| = 26$，或任意字节序列 $|\Sigma| = 256$）。Trie 是一棵 $|\Sigma|$ 叉有根树：

- **根结点 (Root)** 不对应任何字符，代表长度为 $0$ 的空前缀 `""`；
- 从结点 $u$ 出发的第 $c$ 条分支指向子结点 `u->children[c]`，代表在当前前缀末尾追加字符 $c \in \Sigma$；
- 从根结点到任意结点 $v$ 的唯一路径上依次经过的字符序列，即为结点 $v$ 所对应的字符串前缀。

值得注意的是，在 C 语言中处理通用字节或字符序列时，接口应接收 `(const unsigned char *word, size_t length)` 二元组，而不是依赖结尾 `'\0'` 的 C 字符串。这样既能自然支持长度为 `0` 的空单词，也能以完全相同的方式处理含内嵌零字节的二进制键。

## 2. 结点状态设计：词尾标记、前缀计数与关联值

一棵功能完整的 Trie 结点通常在分支指针数组 `children[ALPHABET_SIZE]` 之外维护以下三类元数据中的若干项：

1. **词尾标记与关联值 (`is_word` / `has_value` 与 `value`)**：区分“某个前缀仅作为更长单词的中间路径存在”与“该前缀本身就是一个已插入的完整单词”。例如插入了 `"apple"` 但未插入 `"app"` 时，对应 `"app"` 的结点存在，但其 `is_word` 为 `false`。若将 `is_word` 旁附带一个 `uint64_t value`，Trie 就直接成为一个字符串词典。
2. **经过计数 (`prefix_count` / `pass_count`)**：记录当前字典中有多少个**不同的已插入单词**以该结点对应的串为前缀（含在该结点结束的单词）。
   - 仅当插入一个**此前不存在**的新单词时，沿途每个结点（含根结点与终点结点）的 `prefix_count` 才加 $1$；
   - 查询前缀 `P` 匹配的单词总数时，只需从根沿 `P` 走 $|P|$ 步到达结点 $u$（若中途遇到空指针则为 $0$），直接返回 `u->prefix_count` 即可，时间复杂度为 $O(|P|)$。

### 2.1 插入的异常安全与删除的尾部结点回收

在 C 语言实现 Trie 时，有两个工程细节尤为关键：

- **插入失败不留半条脏路径**：插入新单词前，必须先完整检查所有字符均属于合法字符集（如 `'a' <= word[i] && word[i] <= 'z'`）。在向下创建新结点的过程中，不应提前递增沿途已有结点的 `prefix_count`；或者一旦中途 `calloc` 分配失败，必须回收本次新分配的尾部结点链，保证原树状态与计数器丝毫不变。
- **删除单词后的无用结点清理**：当从词典中删除单词 `W` 时，首先确认 `W` 确实存在并将终点结点的 `is_word`（或 `has_value`）置为 `false`。随后在递归回溯（或利用显式栈回溯）的过程中，若某个子结点既不再是任何单词的词尾（`!child->is_word`），也没有任何非空孩子分支，则说明该子结点已不再被任何现存单词使用，应当立即 `free` 并将父结点对应的 `children[c]` 置回 `NULL`。

## 3. C 语言实现示例：支持前缀统计与删除回收的小写字母 Trie

```c
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { ALPHA = 26 };

typedef struct TrieNode {
    struct TrieNode *children[ALPHA];
    size_t prefix_count;
    bool is_word;
} TrieNode;

static TrieNode *trie_node_new(void) {
    return (TrieNode *)calloc(1, sizeof(TrieNode));
}

static bool trie_has_children(const TrieNode *node) {
    for (size_t i = 0; i < ALPHA; ++i) {
        if (node->children[i] != NULL) {
            return true;
        }
    }
    return false;
}

static bool trie_insert_str(TrieNode *root, const char *s, bool *out_inserted) {
    const TrieNode *scan = root;
    for (size_t i = 0; s[i] != '\0'; ++i) {
        int idx = (unsigned char)s[i] - 'a';
        if (idx < 0 || idx >= ALPHA) {
            return false;
        }
        if (scan != NULL) {
            scan = scan->children[idx];
        }
    }
    if (scan != NULL && scan->is_word) {
        *out_inserted = false;
        return true;
    }

    TrieNode *cur = root;
    for (size_t i = 0; s[i] != '\0'; ++i) {
        int idx = (unsigned char)s[i] - 'a';
        if (cur->children[idx] == NULL) {
            cur->children[idx] = trie_node_new();
            if (cur->children[idx] == NULL) {
                return false;
            }
        }
        cur = cur->children[idx];
    }
    cur->is_word = true;

    cur = root;
    cur->prefix_count++;
    for (size_t i = 0; s[i] != '\0'; ++i) {
        cur = cur->children[(unsigned char)s[i] - 'a'];
        cur->prefix_count++;
    }
    *out_inserted = true;
    return true;
}

static size_t trie_count_prefix(const TrieNode *root, const char *prefix) {
    const TrieNode *cur = root;
    for (size_t i = 0; prefix[i] != '\0'; ++i) {
        int idx = (unsigned char)prefix[i] - 'a';
        if (idx < 0 || idx >= ALPHA || cur->children[idx] == NULL) {
            return 0;
        }
        cur = cur->children[idx];
    }
    return cur->prefix_count;
}

static void trie_free(TrieNode *node) {
    if (node == NULL) {
        return;
    }
    for (size_t i = 0; i < ALPHA; ++i) {
        trie_free(node->children[i]);
    }
    free(node);
}

int main(void) {
    TrieNode *root = trie_node_new();
    bool ins = false;
    trie_insert_str(root, "apple", &ins);
    trie_insert_str(root, "app", &ins);
    trie_insert_str(root, "apply", &ins);
    trie_insert_str(root, "app", &ins); // 重复插入不重复计数

    printf("prefix(\"app\") = %zu, prefix(\"appl\") = %zu, total = %zu\n",
           trie_count_prefix(root, "app"),
           trie_count_prefix(root, "appl"),
           trie_count_prefix(root, ""));
    trie_free(root);
    return 0;
}
```

运行结果：

::: terminal

prefix("app") = 3, prefix("appl") = 2, total = 3

:::

## 4. 习题

<Exercise id="31901" :d="3">

实现基于固定 26 路 ASCII 小写字母（`'a'` 到 `'z'`）分支数组的字典树（Trie / 前缀树）`LowerTrie`，支持单词插入、精确匹配以及前缀存在性检索：

```c
#include <stdbool.h>
#include <stddef.h>

typedef struct LowerTrie LowerTrie;

LowerTrie *lower_trie_create(void);
void lower_trie_destroy(LowerTrie *trie);
bool lower_trie_insert(LowerTrie *trie, const char *word, bool *out_inserted);
bool lower_trie_contains(const LowerTrie *trie, const char *word, bool *out_found);
bool lower_trie_starts_with(const LowerTrie *trie, const char *prefix, bool *out_starts);
```

要求：

1. 单词仅包含 ASCII 小写字母 `'a'` 到 `'z'`，以空字符 `'\0'` 结尾。若字符非法或指针参数为 `NULL`，操作返回 `false` 且不修改输出对象与树结构。
2. 每个树节点维护大小为 26 的子指针静态数组 `children[26]` 以及词尾布尔标记 `is_word`。
3. `lower_trie_insert` 将新单词录入树中：若该单词此前已存在，设置 `*out_inserted = false`；若为全新录入，设置 `*out_inserted = true`。中途若发生内存分配失败，树结构必须保持一致性。
4. `lower_trie_contains` 进行单词全字匹配；`lower_trie_starts_with` 检验是否存在以 `prefix` 为前缀的任何有效单词。
5. 检索与插入的时间复杂度严格由字符串长度 $O(L)$ 决定，与树中已存储的单词总数 $N$ 完全无关。

</Exercise>

<Exercise id="31902" :d="3">

当字符集扩大为全量 256 字节（甚至 Unicode 字符集）时，若每个节点仍分配大小为 256 的静态指针数组，将引发巨大的内存浪费（每个节点占用 $256 \times 8 = 2048$ 字节，其中绝大多数槽位为空指针）。实现基于**动态子节点单向链表**的稀疏字典树 `SparseTrie`：

```c
#include <stdbool.h>
#include <stddef.h>

typedef struct SparseTrie SparseTrie;

SparseTrie *sparse_trie_create(void);
void sparse_trie_destroy(SparseTrie *trie);
bool sparse_trie_insert(SparseTrie *trie, const unsigned char *bytes, size_t len, bool *out_inserted);
bool sparse_trie_contains(const SparseTrie *trie, const unsigned char *bytes, size_t len, bool *out_found);
```

要求：

1. 能够存储包含任意 256 种字节（$[0, 255]$）的任意二进制数据序列，允许字节值为 `0x00`。
2. 每个树节点严禁分配固定大小的 256 槽位数组，维护动态单向链表 `ChildLink`，仅为真实存在的分支分配子节点与关联指针。
3. `sparse_trie_insert` 将长度为 `len` 的字节串插入树中，若已存在设置 `*out_inserted = false`；若出现新分支则动态追加链接节点。
4. `sparse_trie_contains` 进行精确字节串匹配查询。
5. 从空间占用与查找效率两个维度，深入量化对比“固定静态数组 Trie”与“动态链表 Trie”的工程权衡。

</Exercise>

<Exercise id="31903" :d="4">

在字典树上实现单词前缀出现频次的高效聚合统计，并支持包含单个字符通配符 `.`（匹配任意一个有效小写字母）的模糊模式串检索：

```c
#include <stdbool.h>
#include <stddef.h>

typedef struct WildcardTrie WildcardTrie;

WildcardTrie *wildcard_trie_create(void);
void wildcard_trie_destroy(WildcardTrie *trie);
bool wildcard_trie_insert(WildcardTrie *trie, const char *word);
size_t wildcard_trie_prefix_count(const WildcardTrie *trie, const char *prefix);
bool wildcard_trie_search(const WildcardTrie *trie, const char *pattern);
```

要求：

1. 单词由 ASCII 小写字母 `'a'` 到 `'z'` 组成。每个节点维护字段 `prefix_count`，记录有多少个已插入单词的前缀经过该节点。
2. `wildcard_trie_insert`：将新单词插入树中，沿途每个节点的 `prefix_count` 均自增 1，词尾节点标记为单词结尾。
3. `wildcard_trie_prefix_count`：在严格 $O(|prefix|)$ 时间内直接返回以 `prefix` 为前缀的已录入单词总数；若前缀在树中不存在则返回 0。
4. `wildcard_trie_search`：模式串 `pattern` 中除小写字母外还可包含字符 `.`（点号）。点号可通配任意一个存在的分支。利用深度优先回溯遍历（DFS）进行多路分支探索，若存在任何一条完全匹配的单词路径则返回 `true`。
5. 说明深度优先搜索在通配符递归剪枝中的终止条件与正确性保障。

</Exercise>

<Exercise id="31904" :d="4">

给定一个包含非负 32 位整数的无序数组，求数组中任意两个数按位异或（XOR）运算所能达到的最大值。实现基于二进制位分支的 **01-Trie（位前缀树）**，将暴力枚举的二次方时间复杂度 $O(n^2)$ 降低至线性时间 $O(32 \cdot n) = O(n)$：

```c
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct BitTrie BitTrie;

BitTrie *bit_trie_create(void);
void bit_trie_destroy(BitTrie *t);
bool bit_trie_insert(BitTrie *t, uint32_t val);
uint32_t bit_trie_query_max_xor(const BitTrie *t, uint32_t val);
bool find_maximum_xor_pair(const uint32_t *nums, size_t count, uint32_t *out_max_xor);
```

要求：

1. 01-Trie 的字母表大小固定为 2（分支仅包含二进制位 `0` 与 `1`）。
2. 树的高度严格固定为 32 层。插入数值时，必须从最高有效位（第 31 位 MSB）向下逐位扫描至最低位（第 0 位 LSB）。
3. `bit_trie_query_max_xor` 采用贪心策略：对于待查询数 `val` 的当前第 $i$ 位二进制位 $b \in \{0, 1\}$，优先尝试走向相反位 $1 - b$ 的分支；若相反分支存在，异或结果的第 $i$ 位可置为 1；否则只能顺着相同位 $b$ 分支下行。
4. `find_maximum_xor_pair` 处理输入数组：当 `count < 2` 或指针非法时返回 `false`。
5. 详细推导为什么贪心选择“从最高位向最低位优先走相反分支”能够严格保证得到全局最大的异或结果。

</Exercise>

<Exercise id="31905" :d="5">

给定一个非负整数数组，求解其所有非空连续子数组中，元素异或和（Subarray XOR Sum）的最大值。结合异或前缀和（Prefix XOR）的数学性质与 01-Trie 检索，实现在 $O(n)$ 时间内求解该问题：

```c
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool max_subarray_xor(const uint32_t *nums, size_t count, uint32_t *out_max_xor);
```

要求：

1. 必须将任意连续区间 $[l, r]$ 的异或和：
   $$\text{XOR}(l, r) = \bigoplus_{i=l}^r nums[i]$$
   通过异或前缀和数学恒等式转化为两个前缀异或值之间的异或运算。
2. 内部构建 01-Trie，支持 32 位无符号整数的高位贪心查询。
3. 必须在插入任何前缀之前，首先向 01-Trie 中插入初始哨兵值 $0$（对应空前缀）；解释若缺少前缀 0 会导致何种边缘区间计算错误。
4. 算法时间复杂度严格为 $O(32 \cdot n) = O(n)$，空间复杂度严格为 $O(32 \cdot n) = O(n)$。若输入数组为空或指针参数非法，返回 `false`。

</Exercise>

<Exercise id="31906" :d="5">

实现将单分支链条压缩为字符串标签的紧凑字典树——**基数树（Radix Tree / 压缩前缀树 / Compact Trie）**：

```c
#include <stdbool.h>
#include <stddef.h>

typedef struct RadixTree RadixTree;

RadixTree *radix_tree_create(void);
void radix_tree_destroy(RadixTree *tree);
bool radix_tree_insert(RadixTree *tree, const char *word);
bool radix_tree_contains(const RadixTree *tree, const char *word);
```

要求：

1. 树中的每条边不再只保存单一字符，而是保存一个以 `'\0'` 结尾的动态字符串标签 `label`。
2. 插入新单词时，若与某条既有边的标签发生部分匹配，必须执行边分裂（Edge Splitting）：
   - 将公共前缀部分留在原边，目标指向新分裂节点；
   - 将旧边的剩余后缀剥离为一条新边挂在分裂节点之下；
   - 将待插入单词的剩余后缀作为另一条分支挂在分裂节点之下。
3. `radix_tree_contains` 沿边标签前缀逐段匹配，检索单词是否存在。
4. 析构时必须递归释放所有节点及其持有的动态字符串标签，杜绝内存泄漏。
5. 阐述 Linux 内核页缓存（Page Cache）、IP 路由表查找（LPM）等工业系统为什么普遍采用基数树而非普通 Trie。

</Exercise>
