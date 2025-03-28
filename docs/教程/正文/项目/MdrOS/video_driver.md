# 视频驱动程序

我们开发操作系统，必须能显示各种调试信息以及启动日志。

这样我们才能明白开发过程中出现了哪些错误

## VGA (Video Graphics Array) 视频图形阵列

你可能在修电脑的时候听过这个名词，是电脑显示器的一种接口。
但是在本教程中，我们将其定义为一个`640 * 480`大小的显存区域，这块区域由 BIOS 提供给你，
其显存基址为 `0xB8000`

> 注意：UEFI 引导方式不存在这块区域，UEFI 的显存区域由`BootService`提供给你

### 编写 VGA 视频驱动程序

1. 准备工作

   ```c
   #include <stdint.h> // 导入头文件

   uint32_t *video_bass = (uint32_t*) 0xB8000; //定义显存基础地址
   uint16_t cursor_x = 0, cursor_y = 0; // 光标位置
   uint8_t terminal_color = 7 | 0 << 4; //黑底白字

   static inline void outb(uint16_t port, uint8_t data) { //out 端口操作函数 (建议您放到 io.h 头文件中，io.h 专门用于 IO 端口操作)
    asm volatile("outb %b0, %w1" : : "a"(data), "Nd"(port));
   }
   ```

   - 关于`0xB8000`: 在 OS 代码中一般用 16 进制表示一些内存地址，这样代码展现比较直观好辨认，相比十进制或者二进制，
     代码长度也会有所缩减

   [关于进制的知识](/教程/番外/77_关于进制.md)

2. 实现滚屏 光标位置设置

   ```c
   static void scroll() { //滚屏代码实现
        uint8_t attributeByte = (0 << 4) | (15 & 0x0F);
        uint16_t blank = 0x20 | (attributeByte << 8);
        if (cursor_y >= 25) {
            int i;
            for (i = 0 * 80; i < 24 * 80; i++) terminal_buffer[i] = terminal_buffer[i + 80];
            for (i = 24 * 80; i < 25 * 80; i++) terminal_buffer[i] = blank;
            cursor_y = 24;
        }
   }
   void move_cursor() { //设置光标位置
        uint16_t cursorLocation = cursor_y * 80 + cursor_x;
        outb(0x3D4, 14);
        outb(0x3D5, cursorLocation >> 8);
        outb(0x3D4, 15);
        outb(0x3D5, cursorLocation);
   }
   ```

3. 实现 putchar

   ```c
   void putchar(char c){
       uint8_t attributeByte = terminal_color; // 黑底白字
       uint16_t attribute = attributeByte << 8;
       uint16_t *location;

       if (c == 0x08 && cursor_x) { // 退格
           cursor_x--;
           location = terminal_buffer + (cursor_y * 80 + cursor_x);
           *location = ' ' | attribute;
       } else if (c == 0x09) {
          location = terminal_buffer + (cursor_y * 80 + cursor_x);
           *location = ' ' | attribute;
           cursor_x = (cursor_x + 8) & ~(8 - 1);
       } else if (c == '\r') { // 将光标设置到行首
           cursor_x = 0;
       } else if (c == '\n') { // 换行
           cursor_x = 0; // 光标回首
           cursor_y++; // 下一行
       } else if (c >= ' ' && c <= '~') {
           location = terminal_buffer + (cursor_y * 80 + cursor_x);
           *location = c | attribute;
           cursor_x++;
       }

       if (cursor_x >= 80) { // x轴输出溢出自动换行
           cursor_x = 0;
           cursor_y++;
       }

       scroll(); //滚屏
       move_cursor(); //设置光标位置
   }
   ```

4. 实现 writestring 函数，用于打印一行信息

   ```c
   void vga_write(const char *data, size_t size) {
       for (size_t i = 0; i < size; i++)
           vga_putchar(data[i]);
   }

   void vga_writestring(const char *data) {
       vga_write(data, strlen(data));
   }
   ```

> 什么？你问我 printf 函数怎么实现？\
> 我们推荐您使用 stb_sprintf 库

## VBE (VESA BIOS EXTENSION) VESA BIOS 扩展

> **SVGA (Super VGA)**\
> IBM 的 VGA 标准是显示卡发展史上的一块丰碑。
> 但后来无法满足人们的需要，于是市场上出现了 TVGA、S3 系列、
> Cirrus Logic、ET 等为首的一批显示卡，提供了比 VGA 分辨率更高，颜色更丰富的显示模式，
> 又兼容 VGA 显示卡，它们被统称为 Super VGA（SVGA）。
> 为了统一各种 SVGA 显卡，
> 视频电子学标准协会 VESA（Video Electronics Standards Association）
> 提出了一组附加的 BIOS 功能调用借口——VBE（VESA BIOS EXTENSION）标准，
> 从而在软件接口层次上实现了各种 SVGA 显示卡之间的兼容性。
> 时至今日，所有的显示卡 OEM 厂商都提供了符合 VESA SUPER 标准的扩展 BIOS。
> 通过一组 INT 10H 中断调用（AH=4FH），
> 可以方便地使用 VESA SVGA 的扩展功能而不必了解各种显示卡的硬件细节。

## 如何开启 VBE 模式

> 操作系统开发前期我们不是很建议您使用 VBE 模式，
> 以 LegacyBIOS 为例，我们刚进入引导一般都是 VGA 模式，这种显示模式的功能非常有限，只能打印字符和少许的颜色
> 而进入 VBE 高分辨率显示模式，我们可以自由绘画图形，可以显示的颜色升级为 RGB

一般情况下诸如 `grub` `limine` 这些引导程序会判断内核头给出的信息，以决定是否启用 vbe 高分辨率显示模式。
<br>
通常这些引导程序使用的引导规范会给出 vbe 显示模式的信息 (屏幕长/宽/位深/显存基址)

::: tip 小贴士

如果您没有打算自己编写一整套适用于 VBE 的终端，`plos-clan` 社区提供了

- `os_terminal` 一款适用于自制操作系统的虚拟终端，支持绝大部分常用的 VT100 控制字符

[GitHub - plos-clan/os_terminal](https://github.com/plos-clan/libos-terminal/releases)

酌情下载发行版并当作静态库链接至您的内核即可

:::
