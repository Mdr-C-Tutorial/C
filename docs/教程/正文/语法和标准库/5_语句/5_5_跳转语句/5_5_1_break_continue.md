# break 和 continue

`break` 与 `continue` 都用于循环控制，但语义不同：`break` 终止当前循环，`continue` 结束当前迭代并进入下一轮条件判断。

## 1. `break`

`break` 可用于 `switch` 与循环语句。在循环中，它会立刻退出最内层循环。

```c
for (int i = 0; i < 10; ++i) {
    if (i == 5) {
        break;
    }
    printf("%d ", i);
}
```

输出到 `4` 后结束循环。

## 2. `continue`

`continue` 不会结束循环，只会跳过当前迭代剩余语句。

```c
for (int i = 0; i < 10; ++i) {
    if ((i % 2) != 0) {
        continue;
    }
    printf("%d ", i);
}
```

这段代码只打印偶数。

## 3. 嵌套循环注意点

两者都只作用于“最内层”循环。若需要一次退出多层循环，通常应重构为函数并配合 `return`，或在确认可读性收益后使用受控 `goto` 清理路径。
