# A.16 安全与访问控制（ACL 与令牌）：配套代码

正文里每个有输出的程序都在这里。需要 MinGW-w64 GCC（UCRT）与 64 位 Windows。

```text
mingw32-make
```

| 文件 | 正文位置 | 说明 |
| --- | --- | --- |
| `tokeninfo.c` | 2.2 | 打印当前进程令牌的用户、提升类型、完整性级别、组与特权 |
| `deny_only_group.c` | 2.3 | DACL 只给 Administrators 与只给 Users 的两个文件，普通进程打开前者失败 |
| `low_integrity.c` | 3.2 | 把令牌降到低完整性，再写中完整性的对象 |
| `integrity_label.c` | 3.2 | 给新文件贴中、低、高三种完整性标签，高标签失败 |
| `access_mask.c` | 4.2 | 访问掩码各位的值与 `MapGenericMask` 的映射结果 |
| `order_demo.c` | 4.3 | 同样两条 ACE、顺序不同，访问结果相反 |
| `access_check.c` | 4.3 | 用 `AccessCheck` 预演访问检查，含 `MAXIMUM_ALLOWED` 与 ACE 顺序 |
| `sddl_show.c` | 4.4 | 打印文件的安全描述符 SDDL 与逐条 ACE |
| `dacl_kinds.c` | 4.5 | NULL DACL 允许一切，空 DACL 拒绝一切 |
| `private_file.c` | 5.2 | 为新文件指定只允许当前用户与 SYSTEM 的 DACL |
| `private_pipe.c` | 5.3 | 命名管道的默认 DACL 与私有 DACL 对照 |
| `add_ace.c` | 5.4 | `SetEntriesInAclW` 追加 ACE，对比受保护与未受保护的 DACL |
| `check_once.c` | 5.5 | 权限只在打开时检查：改掉 DACL 之后旧句柄照常可用 |
| `default_dacl.c` | 5.6 | 继承来的 DACL 与令牌默认 DACL 的差别 |
| `privilege.c` | 6.2 | 启用、禁用 `SeShutdownPrivilege`；`SeDebugPrivilege` 得到 1300 |
| `adjust_ret.c` | 6.2 | 单独验证 `AdjustTokenPrivileges` 返回 TRUE 而错误码为 1300 |
| `token_member.c` | 习题 46601 | 用 `CheckTokenMembership` 查当前令牌属于哪些众所周知的组 |
| `restrict_token.c` | 习题 46608 | 用无成员的限制 SID 造受限令牌，验证限制 SID 的检查 |

所有程序在 Windows 11 x64（内部版本 26300）上用 MinGW-w64 GCC 13.2 以 `-std=c17 -Wall -Wextra` 编译并运行过，输出见正文。

多数程序会写文件（`report.txt`、`plain.txt`、`label_*.txt` 等），建议在一个空的临时目录里运行。`low_integrity.c` 会把当前进程的令牌降到低完整性，运行后同一个进程里再创建文件会被拒绝，这是预期结果。`privilege.c` 只修改令牌里的特权状态，不会关机。