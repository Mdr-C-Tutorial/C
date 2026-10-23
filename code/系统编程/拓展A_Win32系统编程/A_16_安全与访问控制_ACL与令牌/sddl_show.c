/* 读取文件的安全描述符，打印 SDDL 字符串，并逐条列出 DACL 中的 ACE。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>

static void fail(const char *what) {
    DWORD code = GetLastError();

    printf("%s failed: error %lu\n", what, code);
    return;
}

static const char *ace_type_name(BYTE type) {
    switch (type) {
    case ACCESS_ALLOWED_ACE_TYPE:
        return "允许";
    case ACCESS_DENIED_ACE_TYPE:
        return "拒绝";
    case SYSTEM_AUDIT_ACE_TYPE:
        return "审核";
    case SYSTEM_MANDATORY_LABEL_ACE_TYPE:
        return "完整性标签";
    case ACCESS_ALLOWED_OBJECT_ACE_TYPE:
        return "允许(对象)";
    case ACCESS_DENIED_OBJECT_ACE_TYPE:
        return "拒绝(对象)";
    default:
        return "其他";
    }
}

int main(int argc, char **argv) {
    wchar_t wpath[MAX_PATH];
    PSECURITY_DESCRIPTOR sd = NULL;
    PACL dacl = NULL;
    PSID owner = NULL;
    PSID group = NULL;
    wchar_t *sddl = NULL;
    DWORD err;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (argc < 2) {
        printf("用法: sddl_show 文件\n");
        return 2;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, wpath, MAX_PATH) == 0) {
        printf("路径转换失败\n");
        return 1;
    }

    err = GetNamedSecurityInfoW(wpath, SE_FILE_OBJECT,
                                OWNER_SECURITY_INFORMATION |
                                    GROUP_SECURITY_INFORMATION |
                                    DACL_SECURITY_INFORMATION,
                                &owner, &group, &dacl, NULL, &sd);
    if (err != ERROR_SUCCESS) {
        printf("GetNamedSecurityInfoW failed: error %lu\n", err);
        return 1;
    }

    if (!ConvertSecurityDescriptorToStringSecurityDescriptorW(
            sd, SDDL_REVISION_1,
            OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                DACL_SECURITY_INFORMATION,
            &sddl, NULL)) {
        fail("ConvertSecurityDescriptorToStringSecurityDescriptorW");
        LocalFree(sd);
        return 1;
    }
    printf("SDDL:\n%ls\n\n", sddl);
    LocalFree(sddl);

    if (dacl == NULL) {
        printf("DACL 为 NULL：任何请求都直接通过\n");
        LocalFree(sd);
        return 0;
    }
    printf("ACE 共 %lu 条（按检查顺序）:\n", (unsigned long)dacl->AceCount);
    for (DWORD i = 0; i < dacl->AceCount; i++) {
        LPVOID ace = NULL;
        ACCESS_ALLOWED_ACE *allowed;
        PSID sid;
        wchar_t *sid_text = NULL;

        if (!GetAce(dacl, i, &ace)) {
            fail("GetAce");
            break;
        }
        allowed = ace;
        sid = &allowed->SidStart;
        ConvertSidToStringSidW(sid, &sid_text);
        printf("  [%lu] %-10s 掩码 0x%08lX  %ls\n", (unsigned long)i,
               ace_type_name(((ACE_HEADER *)ace)->AceType),
               (unsigned long)allowed->Mask, sid_text ? sid_text : L"?");
        LocalFree(sid_text);
    }

    LocalFree(sd);
    return 0;
}