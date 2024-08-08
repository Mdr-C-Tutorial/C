# 引言

> 开发操作系统是一个漫长的过程, 需要你有一定的计算机底层原理知识以及C和汇编语言的基础。 \
> 本章教程将教学您如何从零开始开发一个适用于`x86`平台的32位操作系统

## 概念

<p>操作系统是一种系统软件，它是控制管理计算机系统的硬软件，分配调度资源的管理者</p>
<p>其内部功能有:</p>

* 内存管理
* 设备驱动
* 中断处理
* 任务调度

#### 计算机启动过程

1. 电源键按下
2. POST加电硬件自检
3. 加载BIOS
4. 加载引导程序
5. 计算机控制权移交给操作系统内核

这样, 一个操作系统就算启动了. 接下来内核会初始化各种功能以及加载一些必要的设备驱动, 具体的启动方式还是要依据该操作系统的架构决定

## 环境准备

您需要:
* [i686-elf-tools](https://github.com/lordmilko/i686-elf-tools/releases/tag/13.2.0) 交叉编译工具链
* [nasm](https://nasm.us/) 汇编编译器

以及(选择性):
* [Python3](https://www.python.org/downloads/) 充当构建脚本
* [Makefile](https://www.gnu.org/software/make/) 充当构建脚本

## 相关文献网站

在开发操作系统的细节部分 (如某些硬盘系统的接口协议) 您可以查阅相关资料。

> 养成经常查阅资料的好习惯是个不错的选择

* [OSDEV](https://wiki.osdev.org/) 该Wiki讲解了所有有关操作系统开发的知识细节
* [CSDN](https://blog.csdn.net/) 可以查找到更多前辈们的开发经验
* [Intel文档](https://www.intel.cn/content/www/cn/zh/resources-documentation/developer.html) 关于Intel硬件平台的详细硬件开发细节, 您可以在这里查阅

## 教程目录

1. [**引言**](/教程/正文/项目/MdrOS/root.md)
2. [**引导程序**](/教程/正文/项目/MdrOS/bootloader.md)
3. [**视频驱动**](/教程/正文/项目/MdrOS/video_driver.md)
4. [**中断系统**](/教程/正文/项目/MdrOS/interrupt.md)
5. [**内存管理**](/教程/正文/项目/MdrOS/memory.md)
6. [**文件系统**](/教程/正文/项目/MdrOS/filesystem.md)
7. [**多任务**](/教程/正文/项目/MdrOS/task.md)
8. [**用户程序**](/教程/正文/项目/MdrOS/application.md)
9. [**shell程序**](/教程/正文/项目/MdrOS/shell.md)