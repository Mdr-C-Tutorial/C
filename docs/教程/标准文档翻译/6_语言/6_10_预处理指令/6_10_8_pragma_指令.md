# 6.10.8 pragma 指令

## 语义

1. 对于形如

```c
#pragma pp-tokensopt
```

的预处理指令，如果在任何宏替换发生之前，`pragma` 后面紧跟的预处理记号不是 `STDC`，那么实现按实现定义的方式处理它。2. 这种行为可能导致翻译失败，也可能导致翻译器或生成后的程序表现为不符合标准。3. 实现无法识别的此类 pragma 会被忽略。4. 如果在任何宏替换发生之前，`pragma` 后面紧跟的预处理记号是 `STDC`，那么该指令上不进行任何宏替换，并且它必须具有以下形式之一：

::: details 形式化语法

```bnf
standard-pragma:
    # pragma STDC FP_CONTRACT on-off-switch
    # pragma STDC FENV_ACCESS on-off-switch
    # pragma STDC FENV_DEC_ROUND dec-direction
    # pragma STDC FENV_ROUND direction
    # pragma STDC CX_LIMITED_RANGE on-off-switch

on-off-switch: one of
    ON OFF DEFAULT

direction: one of
    FE_DOWNWARD FE_TONEAREST FE_TONEARESTFROMZERO
    FE_TOWARDZERO FE_UPWARD FE_DYNAMIC

dec-direction: one of
    FE_DEC_DOWNWARD FE_DEC_TONEAREST FE_DEC_TONEARESTFROMZERO
    FE_DEC_TOWARDZERO FE_DEC_UPWARD FE_DEC_DYNAMIC
```

:::

5. 这些形式的含义由标准其他位置定义。

## 推荐实践

1. 鼓励实现对无法识别的 pragma 给出诊断。

## 前向引用

- [7.12.2 FP_CONTRACT pragma](/教程/标准文档翻译/目录.md)
- [7.6.1 FENV_ACCESS pragma](/教程/标准文档翻译/目录.md)
- [7.6.3 FENV_DEC_ROUND pragma](/教程/标准文档翻译/目录.md)
- [7.6.2 FENV_ROUND pragma](/教程/标准文档翻译/目录.md)
- [7.3.4 CX_LIMITED_RANGE pragma](/教程/标准文档翻译/目录.md)
