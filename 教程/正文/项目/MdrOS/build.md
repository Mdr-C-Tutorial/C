# 项目搭建

本教程教学如何搭建您的操作系统项目, 好的项目结构可以更方便的协助你开发

## Limine 用户

其github仓库中已经有一个现成的C内核模板, 您只需要clone下来使用即可

[Limine - C Template](https://github.com/limine-bootloader/limine-c-template)

## 自制引导用户

通常你们需要的准备工作要求更多,且编写后的操作系统运行的介质非常有限

可以借鉴 《30天自制操作系统》的MBR软盘引导程序

## Multiboot用户

github上有很多使用Multiboot的自制OS实例, 可以借鉴其是如何搭建项目结构的

> 记得去GNU官网拷贝一份Multiboot头文件

## 目录结构

```text
├ target (目标文件目录, 存放编译好等待链接的目标文件)
├ src (源文件目录, 存放操作系统的所有C和汇编源码)
│  ├ boot (存放引导程序的源码文件)
│  ├ kernel (存放内核的核心功能源码文件, 如中断管理, 多进程, 内存管理等)
│  ├ util (存放一些工具函数实现, 如各种数据结构实现,printf实现等)
│  ├ driver (存放内核内置驱动的源码实现)
│  ├ fs (存放文件系统的源码实现)
│  ├ include (存放各种头文件)
│  └ sysapp (存放系统内置程序, 如shell)
├ isodir (将最终的内核文件以及所有需要的资源文件到这里的最终目标目录, 等待引导实现程序打包成ISO等映像)
├ apps (操作系统内置的应用程序源码, 一般放置移植或自己开发的各种小程序, 如lua解释器, SDL2图形库等)
└  i686_elf_tools (交叉编译工具链)
```

详细按照您自己的喜好可以自由安排项目目录结构，以上结构仅供参考

## 链接脚本

请根据你所用的引导加载器类型从其使用教程或者以下示例拷贝链接脚本

### 适用于Multiboot的链接脚本

```linker
ENTRY(_start)

SECTIONS
{

 . = 2M;

 .text BLOCK(4K) : ALIGN(4K)
 {
     code = .; _code = .; __code = .;
  *(.multiboot)
  *(.text)
 }

 .rodata BLOCK(4K) : ALIGN(4K)
 {
  *(.rodata)
 }

 .data BLOCK(4K) : ALIGN(4K)
 {
     data = .; _data = .; __data = .;
  *(.data)
 }

 .bss BLOCK(4K) : ALIGN(4K)
 {
     bss = .; _bss = .; __bss = .;
  *(COMMON)
  *(.bss)
 }

 end = .; _end = .; __end = .;
}
```

## 构建脚本

### Makefile

以下示例是`Plant-OS`的部分改编版构建脚本, 请勿直接拷贝使用，将其改成适用于你自己项目结构的构建脚本

#### 根目录构建脚本

```make

clean: # 目录清理
    rm target/*

default:
    make clean
    make -C <你的目录名>
    ... #按找你的源文件目录数量编写
    make linker

linker:
    /i686_elf_tools/bin/i686-elf-g++.exe -T linker.ld -o <内核文件输出路径> -ffreestanding -nostdlib <所有的目标文件, 中间用空格隔开> -lgcc

```

#### 子目录脚本示例

```make
gcc := /i686_elf_tools/bin/i686-elf-gcc #交叉编译器的路径
incpath_src := ../include/ #头文件目录路径
out_path_src := ../target/ #根据你的子目录相对于根目录的深度编写 ../的个数
obj_dos := $(out_path_src)源文件名.obj ... #填写你所有的源文件对应的目标文件名 (源文件名.obj)
default : $(obj_dos)

$(out_path_src)%.o : %.c Makefile #C源文件构建
  $(gcc) -I$(incpath_src) -nostdinc -nolibc -nostdlib -fno-builtin -ffreestanding -fno-stack-protector -Qn -fno-pic -fno-pie -fno-asynchronous-unwind-tables -mpreferred-stack-boundary=2 -fomit-frame-pointer -O0 -finput-charset=UTF-8 -fexec-charset=GB2312 -mno-mmx -mno-sse -mfpmath=387 -w -c $*.c -o $(out_path_src)$*.o

$(out_path_src)%.obj : %.asm Makefile
  nasm -f elf $*.asm -o $(out_path_src)$*.obj
```

### Python

以下代码是 `CoolPotOS` 的构建脚本代码, 请勿直接拷贝使用. 将其改成适配您自己项目结构的编译逻辑

```python
import os
import sys

gcc = '/i686_elf_tools/bin/i686-elf-gcc.exe -w -std=gnu99 -I include/ -std=gnu99 -ffreestanding -c -Wincompatible-pointer-types '
asm = '/i686_elf_tools/bin/i686-elf-as.exe'
nasm = "nasm -f elf32"
ld = '/i686_elf_tools/bin/i686-elf-g++.exe'
cd = os.getcwd()  # 获取当前执行目录 'D:\CrashPowerDOS-main\'
out = "target"
dir_ = "\\"
src = "src\\"

def clean():
    print("Clean target folder")
    for file in os.listdir(cd + dir_ + "target"):  # 遍历指定文件夹下所有文件
        os.remove(cd + dir_ + "target" + dir_ + file)
    return 0


def build_boot():  # 构建引导程序
    print("Building boot source code...")
    status = True
    for file in os.listdir(cd + dir_ + src + 'boot'):
        if status and file == 'boot.asm':
            cmd = cd + asm + " " + cd + dir_ + src + "boot" + dir_ + file + " -o " + cd + dir_ + "target" + dir_ + \
                  file.split(".")[0] + ".o"
            status = False
        else:
            cmd = nasm + " " + cd + dir_ + src + "boot" + dir_ + file + " -o " + cd + dir_ + "target" + dir_ + \
                  file.split(".")[0] + ".o"
        e = os.system(cmd)  # os.system 执行命令 e为返回值(非0即不正常退出,可做判断终止构建流程)
        if e != 0:
            return -1
    return 0


def build_driver():  # 构建内置驱动程序
    print("Building driver source code...")
    status_pci = True
    status_ide = True
    for file in os.listdir(cd + dir_ + src + 'driver'):
        if status_pci and (file == 'pci.c'):
            cmd = cd + gcc + "-O0 " + src + "driver" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
            status_pci = False
        elif status_ide and (file == 'ide.c'):
            cmd = cd + gcc + "-O0 " + src + "driver" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
            status_ide = False
        else:
            cmd = cd + gcc + "-O0 " + src + "driver" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
        e = os.system(cmd)
        if e != 0:
            return -1
    return 0


def build_kernel():  # 构建内核本体
    print("Building kernel source code...")
    for file in os.listdir(cd + dir_ + src + 'kernel'):
        cmd = cd + gcc + "-O0 " + src + "kernel" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
        e = os.system(cmd)
        if e != 0:
            return -1
    return 0


def build_data():  # 构建常用工具
    print("Building util source code...")
    for file in os.listdir(cd + dir_ + src + 'util'):
        cmd = cd + gcc + "-O0 " + src + "util" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
        e = os.system(cmd)
        if e != 0:
            return -1
    return 0


def build_sysapp():  # 构建内置系统应用
    print("Building sysapp source code...")
    for file in os.listdir(cd + dir_ + src + 'sysapp'):
        cmd = cd + gcc + "-Og " + src + "sysapp" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
        e = os.system(cmd)
        if e != 0:
            return -1
    return 0


def build_network():  # 构建网络系统
    print("Building network source code...")
    for file in os.listdir(cd + dir_ + src + 'network'):
        cmd = cd + gcc + "-O0 " + src + "network" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
        e = os.system(cmd)
        if e != 0:
            return -1
    return 0


def build_fs():  # 构建文件系统
    print("Building fs source code...")
    for file in os.listdir(cd + dir_ + src + 'fs'):
        cmd = cd + gcc + " " + src + "fs" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
        e = os.system(cmd)
        if e != 0:
            return -1
    return 0


def linker():  # 交叉编译链接
    print("Linking object files...")
    source_file = ""
    for file in os.listdir(cd + dir_ + 'target'):
        source_file = source_file + " target" + dir_ + file
    return os.system(
        cd + "/i686_elf_tools/bin/i686-elf-g++.exe -T linker.ld -o isodir" + dir_ + "sys" + dir_ + "kernel.elf -ffreestanding -nostdlib " + source_file + " -lgcc")


clean()
a = build_boot()
if a != 0:
    exit(-1)
a = build_driver()
if a != 0:
    exit(-1)
a = build_kernel()
if a != 0:
    exit(-1)
a = build_data()
if a != 0:
    exit(-1)
a = build_sysapp()
if a != 0:
    exit(-1)
a = build_network()
if a != 0:
    exit(-1)
a = build_fs()
if a != 0:
    exit(-1)
a = linker()
if a != 0:
    exit(-1)
print("Launching i386 vm...")

```
