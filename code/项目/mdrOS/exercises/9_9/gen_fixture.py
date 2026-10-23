"""生成 ACPI 习题用的固定数据（acpi_fixture.h）。
所有表都是自造的，校验和按 ACPI 规则计算，便于在主机上练习解析。
"""
import struct

def header(b, sig, length, rev, tid, oem=b'MDRTST', creator=b'MDR '):
    b[0:4] = sig
    struct.pack_into('<I', b, 4, length)
    b[8] = rev
    b[10:16] = oem
    b[16:24] = tid[:8].ljust(8, b' ')
    struct.pack_into('<I', b, 24, 1)
    b[28:32] = creator
    struct.pack_into('<I', b, 32, 1)

def finish(b):
    b[9] = (-sum(b)) & 0xFF
    assert sum(b) % 256 == 0
    return bytes(b)

# FADT：只填习题用到的字段，其余保持 0
FADT_FIELDS = {
    0x24: ('FIRMWARE_CTRL', 4, 0x1EF00),
    0x28: ('DSDT', 4, 0x1F200),
    0x2D: ('PREFERRED_PM_PROFILE', 1, 2),
    0x2E: ('SCI_INT', 2, 9),
    0x30: ('SMI_CMD', 4, 0xB2),
    0x34: ('ACPI_ENABLE', 1, 0xA0),
    0x35: ('ACPI_DISABLE', 1, 0xA1),
    0x38: ('PM1a_EVT_BLK', 4, 0x600),
    0x40: ('PM1a_CNT_BLK', 4, 0x604),
    0x4C: ('PM_TMR_BLK', 4, 0x608),
    0x50: ('GPE0_BLK', 4, 0x620),
    0x58: ('PM1_EVT_LEN', 1, 4),
    0x59: ('PM1_CNT_LEN', 1, 2),
    0x5B: ('PM_TMR_LEN', 1, 4),
    0x5C: ('GPE0_BLK_LEN', 1, 0x20),
    0x6C: ('CENTURY', 1, 0x32),
    0x6D: ('IAPC_BOOT_ARCH', 2, 0x0002),
    0x70: ('FLAGS', 4, 0x00000005),
}
f = bytearray(0x74)
header(f, b'FACP', 0x74, 1, b'FADTDEMO')
for off, (name, size, val) in FADT_FIELDS.items():
    if size == 1:
        f[off] = val
    elif size == 2:
        struct.pack_into('<H', f, off, val)
    else:
        struct.pack_into('<I', f, off, val)
fadt = finish(f)

# MADT：4 个 Local APIC（后两个未启用）、1 个 IO APIC、2 条中断源覆盖
entries = bytearray()
def ent(t, payload):
    entries.extend(bytes([t, len(payload) + 2]) + payload)
for acpi_id, apic_id, flags in [(0, 0, 1), (1, 1, 1), (2, 4, 0), (3, 5, 0)]:
    ent(0, bytes([acpi_id, apic_id]) + struct.pack('<I', flags))
ent(1, bytes([1, 0]) + struct.pack('<I', 0xFEC00000) + struct.pack('<I', 0))
ent(2, bytes([0, 0]) + struct.pack('<I', 2) + struct.pack('<H', 0x0000))   # IRQ0 -> GSI2，缺省极性
ent(2, bytes([0, 9]) + struct.pack('<I', 9) + struct.pack('<H', 0x000F))   # IRQ9 -> GSI9，低电平有效、电平触发
m = bytearray(44 + len(entries))
header(m, b'APIC', len(m), 1, b'MADTDEMO')
struct.pack_into('<I', m, 36, 0xFEE00000)
struct.pack_into('<I', m, 40, 0x00000001)
m[44:] = entries
madt = finish(m)

# DSDT：一段不完整的 AML，仅用于演示表遍历
body = bytes.fromhex('10 08 00 00 5B 80')
d = bytearray(36 + len(body))
header(d, b'DSDT', len(d), 1, b'DSDTDEMO')
d[36:] = body
dsdt = finish(d)

# SSDT：校验和故意写错
s = bytearray(36)
header(s, b'SSDT', len(s), 1, b'SSDTBAD')
s[9] = 0
ssdt = bytes(s)

# RSDT：5 个表地址，最后一个指向未映射区域
addrs = [0x1F000, 0x1F100, 0x1F200, 0x1F300, 0x1F500]
r = bytearray(36 + 4 * len(addrs))
header(r, b'RSDT', len(r), 1, b'RSDTDEMO')
struct.pack_into('<5I', r, 36, *addrs)
rsdt = finish(r)

# FACS：没有 ACPI 表头
facs = bytearray(64)
facs[0:4] = b'FACS'
struct.pack_into('<I', facs, 4, 64)
struct.pack_into('<I', facs, 8, 0x12345678)
struct.pack_into('<I', facs, 12, 0x0000F000)
struct.pack_into('<I', facs, 24, 1)
facs = bytes(facs)

# 0xE0000 起 128 字节的 RSDP 搜索区，放三个候选
area = bytearray(128)
def rsdp(off, rsdt_addr, good=True):
    area[off:off + 8] = b'RSD PTR '
    area[off + 8] = 0
    area[off + 9:off + 15] = b'MDRTST' if good else b'DECOY '
    area[off + 15] = 0
    struct.pack_into('<I', area, off + 16, rsdt_addr)
    struct.pack_into('<I', area, off + 20, 0)
    area[off + 8] = (-sum(area[off:off + 20])) & 0xFF
    if not good:
        area[off + 8] = (area[off + 8] + 1) & 0xFF
rsdp(0x10, 0xDEAD, good=False)   # 校验和错
rsdp(0x25, 0xBEEF, good=True)    # 校验和对，但没按 16 字节对齐
rsdp(0x60, 0x1F400, good=True)   # 真正的 RSDP
area = bytes(area)

assert sum(fadt) % 256 == 0 and sum(madt) % 256 == 0 and sum(dsdt) % 256 == 0
assert sum(rsdt) % 256 == 0 and sum(ssdt) % 256 != 0
for off in (0x10, 0x25, 0x60):
    s16 = sum(area[off:off + 20]) % 256
    print('rsdp', hex(off), 'aligned', off % 16 == 0, 'checksum sum', s16,
          'rsdt', hex(struct.unpack_from('<I', area, off + 16)[0]))

def carr(name, b, per=12):
    out = [f'static const uint8_t {name}[{len(b)}] = {{']
    for i in range(0, len(b), per):
        out.append('    ' + ', '.join(f'0x{x:02X}' for x in b[i:i + per]) + ',')
    out.append('};')
    return '\n'.join(out)

HEADER = f'''#ifndef MDR_ACPI_FIXTURE_H
#define MDR_ACPI_FIXTURE_H

/* 9.9 习题用的 ACPI 数据，由 gen_fixture.py 生成，不要手改。
 *
 * 物理地址布局：
 *   0xE0000  128 字节，RSDP 搜索区（含三个候选）
 *   0x1EF00  FACS，64 字节，没有 ACPI 表头
 *   0x1F000  FADT，{len(fadt)} 字节
 *   0x1F100  MADT，{len(madt)} 字节
 *   0x1F200  DSDT，{len(dsdt)} 字节
 *   0x1F300  SSDT，{len(ssdt)} 字节，校验和故意写错
 *   0x1F400  RSDT，{len(rsdt)} 字节，指向 0x1F000/0x1F100/0x1F200/0x1F300/0x1F500
 *   0x1F500  未映射，读取时应视为错误
 */

#include <stdint.h>

/* 0xE0000 起 128 字节 */
{carr('rsdp_region', area)}

/* 0x1F400：RSDT */
{carr('rsdt_table', rsdt)}

/* 0x1F000：FADT，revision 1，长度 0x74 */
{carr('fadt_table', fadt)}

/* 0x1F100：MADT */
{carr('madt_table', madt)}

/* 0x1F200：DSDT */
{carr('dsdt_table', dsdt)}

/* 0x1F300：SSDT，checksum 字段为 0，实际和为 0x{sum(ssdt)%256:02X} */
{carr('ssdt_table', ssdt)}

/* 0x1EF00：FACS */
{carr('facs_table', facs)}

#endif /* MDR_ACPI_FIXTURE_H */
'''
open('acpi_fixture.h', 'w', encoding='utf-8', newline='\n').write(HEADER)
print('fadt', len(fadt), 'madt', len(madt), 'dsdt', len(dsdt), 'ssdt', len(ssdt), 'rsdt', len(rsdt))
print('madt entries:')
off = 44
while off < len(madt):
    t, l = madt[off], madt[off + 1]
    print(' type', t, 'len', l, 'at', hex(off))
    off += l
print('FADT fields:')
for off, (name, size, val) in FADT_FIELDS.items():
    got = fadt[off] if size == 1 else struct.unpack_from('<H' if size == 2 else '<I', fadt, off)[0]
    print(f'  {hex(off)} {name} = {hex(got)}')
