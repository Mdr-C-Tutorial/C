# Mdr Lang 编译器（C 版本）

这是 C11 实现的前端起步版，当前能力：

1. 词法分析（Token）。
2. 递归下降语法分析。
3. 基础语义分析。
4. 命令行校验入口。

## 构建

```bash
cmake -S code/项目/Mdr_Lang/compiler_c -B code/项目/Mdr_Lang/compiler_c/build
cmake --build code/项目/Mdr_Lang/compiler_c/build
```

## 使用

```bash
code/项目/Mdr_Lang/compiler_c/build/mdrcc docs/教程/正文/项目/Mdr_Lang/example/vec2.mdr
```

打印 Token：

```bash
code/项目/Mdr_Lang/compiler_c/build/mdrcc docs/教程/正文/项目/Mdr_Lang/example/range_step.mdr --tokens
```

运行示例烟雾测试：

```bash
powershell -NoProfile -ExecutionPolicy Bypass -File code/项目/Mdr_Lang/compiler_c/tests/smoke.ps1
```

## 已实现的语义检查

1. 标识符解析（含作用域）。
2. `break` / `continue` 只能出现在循环语句中。
3. `return` 只能出现在函数中，且返回表达式类型与函数返回类型匹配。
4. 泛型参数与函数参数在函数体内可见。
5. 函数调用与结构体构造的参数个数检查。
6. 函数调用参数类型检查（含结构体构造参数）。
7. 赋值与复合赋值类型兼容性检查。
8. 逻辑/比较/算术/一元表达式的类型规则检查。
9. 结构体字段访问类型解析（例如 `obj.field`）。
10. 成员方法调用返回类型解析（例如 `obj.method()`）。
