# 有机物碳氢计数

## 题目

有机物的命名有一套完整的规则。以下是烃的部分命名规则：

1. 甲烷到癸烷的名称分别为：`methane` `ethane` `propane` `butane` `pentane` `hexane` `heptane` `octane` `nonane` `decane`。
2. 烷基的名称是将对应烷烃词尾的 `ane` 替换为 `yl`。同理，烯烃替换成 `ene`，炔烃替换成 `yne`，烯基替换成 `enyl`，炔基替换成 `ynyl`。例如：乙基 `ethyl`，乙烯 `ethene`，乙炔 `ethyne`，乙烯基 `ethenyl`，乙炔基 `ethynyl`。
3. 苯，萘，蒽，菲的名称分别为：`benzene`, `naphthalene`, `anthracene`, `phenanthrene`。例如，甲基乙苯 `methylethylbenzene`，9-甲基菲 `9-methylphenanthrene`。
4. “环”用 `cyclo` 开头，例如：环丙烷 `cyclopropane`, 环丁烷 `cyclobutane`。
5. 标识取代基个数的“二 三 四 五 六”分别用 `di`，`tri`，`tetra`，`penta`，`hexa` 开头。例如：1-2-3-三甲苯 `1,2,3-trimethylbenzene`。

设计程序，输入有机物名称，输出其碳原子和氢原子个数。

例：输入 `benzene`，输出 `6 6`；输入 `2-methylpropane`，输出 `4 10`；输入 `4,5-dipropynylnaphthalene`，输出 `16 10`；输入 `2,3,5-trimethyl-4-propyloctane`，输出 `14 30`。

保证有机物名称满足上述规则。

## 答案

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct  {
    char* name;
    int carbon_count;
} Prefix;

typedef struct {
    char* name;
    int carbon_count;
    int hydrogen_count;
} Aromatic;

typedef struct {
    char* name;
    int value;
} Multiplier;

bool ends_with(const char* str, const char* suffix);
bool is_digit_string(const char* str);
int get_prefix_carbon(const char* prefix);
void process_substituents(const char* sub_str, int* carbon_count, int* hydrogen_count, int* num_substitutions);
void count_atoms(const char* organic_name, int* carbon_count, int* hydrogen_count);

Prefix prefixes[] = {
    {"meth", 1},
    {"eth", 2},
    {"prop", 3},
    {"but", 4},
    {"pent", 5},
    {"hex", 6},
    {"hept", 7},
    {"oct", 8},
    {"non", 9},
    {"dec", 10},
    {NULL, 0}
};

Aromatic aromatics[] = {
    {"benzene", 6, 6},
    {"naphthalene", 10, 8},
    {"anthracene", 14, 10},
    {"phenanthrene", 14, 10},
    {NULL, 0, 0}
};

Multiplier multipliers[] = {
    {"di", 2},
    {"tri", 3},
    {"tetra", 4},
    {"penta", 5},
    {"hexa", 6},
    {NULL, 0}
};

bool ends_with(const char* str, const char* suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (str_len < suffix_len) {
        return false;
    }
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

bool is_digit_string(const char* str) {
    while (*str) {
        if (!isdigit((unsigned char)*str)) {
            return false;
        }
        str++;
    }
    return true;
}

int get_prefix_carbon(const char* prefix) {
    for (int i = 0; prefixes[i].name != NULL; i++) {
        if (strcmp(prefixes[i].name, prefix) == 0) {
            return prefixes[i].carbon_count;
        }
    }
    return 0;
}

void process_substituents(const char* sub_str, int* carbon_count, int* hydrogen_count, int* num_substitutions) {
    char* temp = malloc(strlen(sub_str) + 1);
    if (temp == NULL) {
        return;
    }
    strcpy(temp, sub_str);
    char* token = strtok(temp, "-");
    while (token != NULL) {
        if (!is_digit_string(token) && strchr(token, ',') == NULL) {
            int multiplier = 1;
            char* sub_name = token;
            for (int i = 0; multipliers[i].name != NULL; i++) {
                size_t mult_len = strlen(multipliers[i].name);
                if (strncmp(token, multipliers[i].name, mult_len) == 0) {
                    multiplier = multipliers[i].value;
                    sub_name = token + mult_len;
                    break;
                }
            }
            const char* suffixes[] = {"yl", "enyl", "ynyl"};
            int suffix_type = -1;
            for (int s = 0; s < 3; s++) {
                if (ends_with(sub_name, suffixes[s])) {
                    suffix_type = s;
                    break;
                }
            }
            if (suffix_type != -1) {
                size_t suffix_len = strlen(suffixes[suffix_type]);
                size_t prefix_len = strlen(sub_name) - suffix_len;
                char sub_prefix[20];
                if (prefix_len >= sizeof(sub_prefix)) {
                    token = strtok(NULL, "-");
                    continue;
                }
                strncpy(sub_prefix, sub_name, prefix_len);
                sub_prefix[prefix_len] = '\0';
                int sub_carbons = get_prefix_carbon(sub_prefix);
                if (sub_carbons > 0) {
                    *carbon_count += sub_carbons * multiplier;
                    if (suffix_type == 0) { // yl
                        *hydrogen_count += (2 * sub_carbons + 1) * multiplier;
                    } else if (suffix_type == 1) { // enyl
                        *hydrogen_count += (2 * sub_carbons - 1) * multiplier;
                    } else if (suffix_type == 2) { // ynyl
                        *hydrogen_count += (2 * sub_carbons - 3) * multiplier;
                    }
                    *num_substitutions += multiplier;
                }
            }
        }
        token = strtok(NULL, "-");
    }
    free(temp);
}

void count_atoms(const char* organic_name, int* carbon_count, int* hydrogen_count) {
    *carbon_count = 0;
    *hydrogen_count = 0;
    int num_substitutions = 0;
    size_t org_len = strlen(organic_name);
    // check for aromatic
    for (int i = 0; aromatics[i].name != NULL; i++) {
        const char* aromatic_name = aromatics[i].name;
        size_t aromatic_len = strlen(aromatic_name);
        if (org_len >= aromatic_len && ends_with(organic_name, aromatic_name)) {
            *carbon_count = aromatics[i].carbon_count;
            *hydrogen_count = aromatics[i].hydrogen_count;
            size_t sub_len = org_len - aromatic_len;
            char* sub_str = malloc(sub_len + 1);
            if (sub_str == NULL) {
                return;
            }
            strncpy(sub_str, organic_name, sub_len);
            sub_str[sub_len] = '\0';
            process_substituents(sub_str, carbon_count, hydrogen_count, &num_substitutions);
            free(sub_str);
            *hydrogen_count -= num_substitutions;
            return;
        }
    }
    // not aromatic, check for non-aromatic
    const char* suffixes[] = {"ane", "ene", "yne"};
    for (int s = 0; s < 3; s++) {
        const char* suffix = suffixes[s];
        for (int p = 0; prefixes[p].name != NULL; p++) {
            const char* prefix = prefixes[p].name;
            // check cyclo first
            char base_candidate2[50];
            snprintf(base_candidate2, sizeof(base_candidate2), "cyclo%s%s", prefix, suffix);
            size_t candidate2_len = strlen(base_candidate2);
            if (org_len >= candidate2_len && ends_with(organic_name, base_candidate2)) {
                int base_carbons = prefixes[p].carbon_count;
                *carbon_count = base_carbons;
                if (s == 0) { // ane
                    *hydrogen_count = 2 * base_carbons;
                } else if (s == 1) { // ene
                    *hydrogen_count = 2 * base_carbons - 2;
                } else if (s == 2) { // yne
                    *hydrogen_count = 2 * base_carbons - 4;
                }
                size_t sub_len = org_len - candidate2_len;
                char* sub_str = malloc(sub_len + 1);
                if (sub_str == NULL) {
                    return;
                }
                strncpy(sub_str, organic_name, sub_len);
                sub_str[sub_len] = '\0';
                process_substituents(sub_str, carbon_count, hydrogen_count, &num_substitutions);
                free(sub_str);
                *hydrogen_count -= num_substitutions;
                return;
            }
            // check non-cyclo
            char base_candidate1[50];
            snprintf(base_candidate1, sizeof(base_candidate1), "%s%s", prefix, suffix);
            size_t candidate1_len = strlen(base_candidate1);
            if (org_len >= candidate1_len && ends_with(organic_name, base_candidate1)) {
                int base_carbons = prefixes[p].carbon_count;
                *carbon_count = base_carbons;
                if (s == 0) { // ane
                    *hydrogen_count = 2 * base_carbons + 2;
                } else if (s == 1) { // ene
                    *hydrogen_count = 2 * base_carbons;
                } else if (s == 2) { // yne
                    *hydrogen_count = 2 * base_carbons - 2;
                }
                size_t sub_len = org_len - candidate1_len;
                char* sub_str = malloc(sub_len + 1);
                if (sub_str == NULL) {
                    return;
                }
                strncpy(sub_str, organic_name, sub_len);
                sub_str[sub_len] = '\0';
                process_substituents(sub_str, carbon_count, hydrogen_count, &num_substitutions);
                free(sub_str);
                *hydrogen_count -= num_substitutions;
                return;
            }
        }
    }
    // if no base found
    *carbon_count = 0;
    *hydrogen_count = 0;
}

int main() {
    const char* test_cases[] = {
        "benzene",
        "2-methylpropane",
        "methylpropane",
        "2,3,5-trimethyl-4-propyloctane",
        "dimethylbutane"
    };
    int num_tests = sizeof(test_cases) / sizeof(test_cases[0]);
    for (int i = 0; i < num_tests; i++) {
        int carbon, hydrogen;
        count_atoms(test_cases[i], &carbon, &hydrogen);
        printf("%s: %d %d\n", test_cases[i], carbon, hydrogen);
    } 
    return 0;
}
```

## 插曲
