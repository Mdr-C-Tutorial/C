# 9. Trie 与字符串结构

## 习题

<Exercise id="30013" :d="4" :w="4">

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

<Exercise id="30014" :d="5" :w="4">

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
