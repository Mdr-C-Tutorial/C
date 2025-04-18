# 文件系统

在操作系统中，处理持久化存储信息的程序叫做文件系统 (FileSystem)。
文件系统负责有效合理的管理所有逻辑存储单元 (也就是文件 File)。
市面上主流的文件系统有：

- FAT12 FAT16 FAT32(VFAT)
- ISO9660 NFS RAMFS
- NTFS BRTFS XFS
- ext2 ext3 ext4

还有更多，教程就不一一列举了

## 存储单元

- 文件是文件系统中抽象出的一个概念，是最小的信息管理存储单元，通常具备大小，文件名，创建/修改日期，类型等特性

- 文件夹是由许多个文件组成的集合，通常为一个节点

## VFS

`Virtual File System` 虚拟文件系统

它并不履行任何文件系统的实际功能，准确来说它更像是一个统一的操作接口。 \
所有具体文件系统都将挂载到 VFS 上

> Linux 内核就是采用了这种方式，但具体实现更为复杂

## devfs pipfs

是沿袭类 unix 产生的虚拟文件系统，秉承所有的接口都可以用文件系统完成。
