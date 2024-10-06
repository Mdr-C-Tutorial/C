# ACPI 

高级配置与电源管理接口(Advanced Configuration and Power Interface)

ACPI是一种描述硬件接口的方法，用于操作系统与计算机硬件之间的通信。它提供了一种标准化的方法来管理电源管理、热管理和配置功能。

ACPI允许操作系统控制计算机的电源状态,例如休眠、待机和关闭。包括：系统电源管理（System Power Management）、设备电源管理(Device Power Management)和处理器电源管理(Processor Power Management). 它还提供了一种方法来管理电源供应和电池状态(Battery Management).

ACPI可以监测和控制计算机的温度，并根据需要调整风扇的转速。它还可以降低设备的性能以防止过热。

ACPI提供了一种方法来配置计算机硬件和外部设备。它可以识别和管理各种设备，包括处理器、内存、硬盘驱动器和外部接口等。

ACPI的运行时模型用于管理在系统运行期间发生的所有ACPI事件。它处理中断事件，并评估处理这些事件所需的对象。当用户启动系统时，系统固件会完成ACPI的设置、初始化和自我测试。

ACPI可以实现的功能包括：

* 系统电源管理（System power management）
* 设备电源管理（Device power management）
* 处理器电源管理（Processor power management）
* 设备和处理器性能管理（Device and processor performance management）
* 配置/即插即用（Configuration/Plug and Play）
* 系统事件（System Event）
* 电池管理（Battery management）
* 温度管理（Thermal management）
* 嵌入式控制器（Embedded Controller）
* SMBus 控制器（SMBus Controller）

> ioapic的开启以及SMP多处理器的开启也需要ACPI

## ACPI Table

ACPI提供了一组用于描述控制设备和描述设备信息方式的结构, 它们按照一种层级结构组织.

以下是ACPI提供的一些表

* Root System Description Table (RSDT)
* Fixed ACPI Description Table (FADT)
* Firmware ACPI Control Structure (FACS)
* Differentiated System Description Table (DSDT)
* Secondary System Description Table (SSDT)
* Multiple APIC Description Table (MADT)
* Smart Battery Table (SBST)
* Extended System Description Table (XSDT)
* Embedded Controller Boot Resources Table (ECDT)
* System Locality Distance Information Table (SLIT)
* System Resource Affinity Table (SRAT)
* Corrected Platform Error Polling Table (CPEP)
* Maximum System Characteristics Table (MSCT)
* ACPI RAS Feature Table (RASF)
* Memory Power State Table (MPST)
* Platform Memory Topology Table (PMTT)
* Boot Graphics Resource Table (BGRT)
* Firmware Performance Data Table (FPDT)
* Generic Timer Description Table (GTDT)
* NVDIMM Firmware Interface Table (NFIT)

## ACPI驱动开发

本教程我们只用到ACPI的两个功能

* 系统电源管理(关机/重启)
* 获取自acpi配置好后的纳秒数

> 有关于详细的源码, 请前往[MdrOS - ACPI Driver](https://github.com/Mdr-C-Tutorial/MdrOS/blob/main/src/driver/acpi.c)

### ACPI初始化

在 `acpi.c` 中的336行开始, 我们可以看到 `acpi_install` 函数作用为初始化ACPI表, 并在内核IDLE进程 (`core/main.c - kernel_main` 函数) 中被调用

```c 
void acpi_install() {
    klogf(AcpiSysInit(),"Load acpi driver.\n");
    acpi_enable_flag = !acpi_enable();
    hpet_initialize();
    AcpiPowerInit();
}
```

* `AcpiSysInit()` 定义于97行, 用于查找ACPI的数据结构并进行初步的初始化工作
* `acpi_enable()` 定义于176行, 用于启用ACPI的功能
* `hpet_initialize()` 定义于322行, 用于HPET表的初始化, 获取纳秒数需要该表
* `AcpiPowerInit()` 定义于64行, 用于启用电源键中断监听

### 关机/重启 实现

在 `acpi.c` 367行开始定义了两个函数分别用于重启和关机, 其原理是向ACPI提供的电源管理设备端口号写入指定数据以向该设备发送重启或关机的指令来改变PC机的电源状态,实现重启和关机

```c
void power_reset() { //电源重启
    uint8_t val;
    if (!SCI_EN)
        return;
    while (1) {
        // write ICH port
        io_out8(0x92, 0x01);
        // send RESET_VAL
        io_out8((uint32_t) facp->RESET_REG.address, facp->RESET_VALUE);
    }
}

void power_off() { //电源关机
    if (!SCI_EN) return;
    while (1) {
        io_out16((uint32_t) PM1a_CNT, SLP_TYPa | SLP_EN);
        if (!PM1b_CNT) {
            io_out16((uint32_t) PM1b_CNT, SLP_TYPb | SLP_EN);
        }
    }
}
```

### 纳秒数获取

在 `acpi.c` 343行定义了`nanoTime()`函数用于获取纳秒数

```c
uint32_t nanoTime() {
    if(hpetInfo == NULL) return 0;
    uint32_t mcv =  hpetInfo->mainCounterValue;
    return mcv * hpetPeriod;
}
```

> 基于此我们可以编写一个休眠函数, 其精度为纳秒
>
>    ```c
>    void usleep(uint32_t nano) {
>        uint32_t targetTime = nanoTime();
>        uint32_t after = 0;
>        while (1) {
>            uint64_t n = nanoTime();
>            if (n < targetTime) {
>                after += 0xffffffff - targetTime + n;
>                targetTime = n;
>            } else {
>                after += n - targetTime;
>                targetTime = n;
>            }
>            if (after >= nano) {
>                return;
>            }
>        }
>    }
>    ```

## APIC

高级可编程中断控制器（Advanced Programmable Interrupt Controller）

一种中断控制器架构，通常多见于Intel 32位架构（Intel Architecture-based 32-bit）的PC系统。APIC架构支持多处理器中断管理（中断均匀地分布在所有处理器），多I/O子系统支持，与8259A 兼容，并且支持处理器内部中断（Inter-Processor Interrupt，IPI）。这个架构由直属于处理器的本地APICs（Local APICs）和在（南桥）芯片中的I/O APIC组成。