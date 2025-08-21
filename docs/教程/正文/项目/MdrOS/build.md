# 项目搭建

本教程教学如何搭建您的操作系统项目，好的项目结构可以更方便的协助你开发

## 使用现成的开发模板

- Plos-clan Multiboot i386 [multiboot-xmake-template](https://github.com/plos-clan/multiboot-xmake-template) \
   适用于 32 位 LegacyBIOS 引导的操作系统

  > 注意：不适用于现代 PC 机，您需要一台支持旧版 MBR 引导的电脑

- Plos-clan Limine x86_64 [limine-xmake-template](https://github.com/plos-clan/limine-xmake-template) \
   适用于 64 位 UEFI 引导的操作系统
  > 适用于所有现代的 PC 机，但是编写 64 位系统会很难

## 自制引导用户

### MBR LegacyBIOS 引导方式

通常你们需要的准备工作要求更多，且编写后的操作系统运行的介质非常有限

可以借鉴《30 天自制操作系统》的 MBR 软盘引导程序

### GPT UEFI BIOS 引导方式

可以使用 EDK2 开发套件或采用自己编写 EFI 程序

引导器可以获取到内存布局表后传入内核，即可开始引导。

因系统信息，文件读取等都有 UEFI 接口提供，所以编写相对简单一些
