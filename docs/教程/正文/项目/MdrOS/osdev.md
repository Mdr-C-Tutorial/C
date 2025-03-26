# OSDEV 的代码校正

因 OSDEV 教程过于古老，并结合笔者实际开发经验，以下章节会有部分对 OSDEV 的代码校正。

## AHCI

该章节中部分代码可以优化以及校正错误

[AHCI_port_memory_space_initialization](https://wiki.osdev.org/AHCI#AHCI_port_memory_space_initialization)

1. 代码第 1 行

   ```c:line-numbers=1
   #define AHCI_BASE 0x400000 // 4M
   ```

   固定写死的地址不太可取，你可以用 `malloc` 函数申请一块长度为 `1048576` 的超大区域 (但是你需要保证这片区域的页映射是恒等的)

   > 在使用 AHCI_BASE 中，如果您的恒等映射有一定的偏移 (如 Limine 引导器的映射，必须将其转换成物理地址再写入设备寄存器)

2. 代码第 34 行

   ```c:line-numbers=34
   cmdheader[i].ctba = AHCI_BASE + (40<<10) + (portno<<13) + (i<<8);
   cmdheader[i].ctbau = 0;
   ```

   这段代码仅适用于 32 位，而 64 位则需要替换成以下写法

   ```c
   uint64_t phys_ctb = AHCI_BASE + (40<<10) + (portno<<13) + (i<<8);
   cmdheader[i].ctba = phy_ctb & 0xFFFFFFFF;
   cmdheader[i].ctbau = phy_ctb >> 32;
   ```

   [Example\_-_Read_hard_disk_sectors](https://wiki.osdev.org/AHCI#Example_-_Read_hard_disk_sectors)

3. 代码第 24, 31 行
   ::: code-group

   ```c:line-numbers=24
   cmdtbl->prdt_entry[i].dba = (uint32_t) buf;
   ```

   ```c:line-numbers=31
    // Last entry
    cmdtbl->prdt_entry[i].dba = (uint32_t) buf;
   ```

   :::
   同上并不适用于 64 位，不同的是需要将 buf 转换成物理地址

   ```c
   uint64_t phys_buf = (uint64_t)virt_to_phys((uint64_t)buf); //假设 buf 需要进行虚拟地址转换成物理地址
   cmdtbl->prdt_entry[i].dba = phys_buf & 0xFFFFFFFF;
   cmdtbl->prdt_entry[i].dbau = phys_buf >> 32;
   ```

   > 同理，write 函数也要进行此方法更改
