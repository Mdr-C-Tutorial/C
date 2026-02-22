# Mdr Lang 编译器（起步版）

这个目录提供了一个可运行的前端原型，当前阶段完成了：

1. 词法分析（Token 流）。
2. 语法分析（AST）。
3. 命令行入口。

## 目录结构

```text
compiler/
  cli.mjs
  src/
    compiler.mjs
    error.mjs
    lexer.mjs
    parser.mjs
    token.mjs
  tests/
    smoke.mjs
```

## 用法

```bash
node code/项目/Mdr_Lang/compiler/cli.mjs docs/教程/正文/项目/Mdr_Lang/example/vec2.mdr
```

输出 AST 到文件：

```bash
node code/项目/Mdr_Lang/compiler/cli.mjs docs/教程/正文/项目/Mdr_Lang/example/range_step.mdr --out temp/range_step.ast.json
```

同时打印 Token：

```bash
node code/项目/Mdr_Lang/compiler/cli.mjs docs/教程/正文/项目/Mdr_Lang/example/range_step.mdr --tokens
```

## 已覆盖的核心语法

1. 顶层：`use`、`struct`、`function`。
2. 语句：`let` / `const`、`if` / `else`、`while`、`loop`、`for`、`return`、`break`、`continue`、赋值语句、表达式语句。
3. 表达式：逻辑运算、比较、四则、前缀一元、成员访问、函数调用、元组字面量、数组字面量、`this`、`null`。
4. 范围表达式：`a..b`、`a...b`、`a..b..s`、`a...b..s`。

## 下一步建议

1. 在 AST 之后加入语义分析（名称解析、类型检查）。
2. 设计中间表示（IR）。
3. 先实现解释执行，再接入机器码后端或 C 后端。
