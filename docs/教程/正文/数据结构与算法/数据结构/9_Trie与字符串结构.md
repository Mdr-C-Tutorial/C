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

<Exercise id="31901" :d="4" :w="4">

实现支持前缀计数的字节 Trie。为控制接口范围，单词只包含 ASCII 小写字母 `a` 到 `z`，不包含结尾空字节：

```c
#include <stdbool.h>
#include <stddef.h>

typedef struct LowerTrie LowerTrie;

LowerTrie *lower_trie_create(void);
void lower_trie_destroy(LowerTrie *trie);
bool lower_trie_insert(
    LowerTrie *trie,
    const unsigned char *word,
    size_t length,
    bool *out_inserted
);
bool lower_trie_contains(
    const LowerTrie *trie,
    const unsigned char *word,
    size_t length,
    bool *out_found
);
bool lower_trie_count_prefix(
    const LowerTrie *trie,
    const unsigned char *prefix,
    size_t length,
    size_t *out_count
);
```

要求：

1. `insert` 复制并拥有节点存储；重复单词成功返回并设置 `*out_inserted = false`。
2. 每个节点维护经过该节点的单词数量，使前缀查询时间复杂度为 $O(|prefix|)$。
3. 字符非法、参数无效、分配失败或大小计算溢出时返回 `false`；失败时不得修改 Trie 和输出对象。
4. 空单词可以插入并参与计数；长度为 0 时对应指针可以为 `NULL`。
5. `destroy(NULL)` 安全，必须释放所有节点。

</Exercise>

<Exercise id="31902" :d="5" :w="4">

在 Trie 上实现一个整数词典：每个单词关联一个 `uint64_t` 值，重复插入应更新旧值：

```c
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct LowerDictionary LowerDictionary;

LowerDictionary *lower_dictionary_create(void);
void lower_dictionary_destroy(LowerDictionary *dictionary);
bool lower_dictionary_put(
    LowerDictionary *dictionary,
    const unsigned char *word,
    size_t length,
    uint64_t value,
    bool *out_replaced
);
bool lower_dictionary_get(
    const LowerDictionary *dictionary,
    const unsigned char *word,
    size_t length,
    uint64_t *out_value
);
bool lower_dictionary_remove(
    LowerDictionary *dictionary,
    const unsigned char *word,
    size_t length,
    bool *out_removed
);
```

要求：

1. 键仍限于 ASCII 小写字母；词典拥有节点，删除后应回收不再需要的尾部节点。
2. `put` 的新键插入失败时不得留下半条路径；更新已有键时不得改变节点结构。
3. `get` 找不到键时成功返回并将 `*out_value` 保持不变；参数无效或字符非法时返回 `false`。
4. `remove` 删除不存在的键时成功返回并设置 `*out_removed = false`；所有输出对象在失败时保持不变。
5. 查找、插入和删除的时间复杂度为 $O(|word|)$。

</Exercise>
