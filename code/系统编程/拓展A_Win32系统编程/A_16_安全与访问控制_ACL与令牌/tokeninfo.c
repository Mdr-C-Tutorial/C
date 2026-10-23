/* 打印当前进程访问令牌的用户、提升类型、完整性级别、组与特权。 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <sddl.h>

static void fail(const char *what) {
    DWORD code = GetLastError();

    printf("%s failed: error %lu\n", what, code);
    exit(1);
}

/* 两次调用的写法：第一次取所需大小，第二次取数据。 */
static void *token_info(HANDLE token, TOKEN_INFORMATION_CLASS cls) {
    DWORD size = 0;
    void *buf;

    if (!GetTokenInformation(token, cls, NULL, 0, &size) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        fail("GetTokenInformation");
    }
    buf = malloc(size);
    if (buf == NULL) {
        printf("out of memory\n");
        exit(1);
    }
    if (!GetTokenInformation(token, cls, buf, size, &size)) {
        fail("GetTokenInformation");
    }
    return buf;
}

static wchar_t *sid_to_string(PSID sid) {
    wchar_t *text = NULL;

    if (!ConvertSidToStringSidW(sid, &text)) {
        fail("ConvertSidToStringSidW");
    }
    return text;
}

static void print_account(PSID sid) {
    wchar_t name[128];
    wchar_t domain[128];
    DWORD name_len = 128;
    DWORD domain_len = 128;
    SID_NAME_USE use;

    if (!LookupAccountSidW(NULL, sid, name, &name_len, domain, &domain_len, &use)) {
        printf("(无帐户名)");
    } else if (domain[0] == L'\0') {
        printf("%ls", name);
    } else {
        printf("%ls\\%ls", domain, name);
    }
}

static void print_attributes(DWORD attr) {
    if (attr & SE_GROUP_MANDATORY) {
        printf(" 强制");
    }
    if (attr & SE_GROUP_ENABLED_BY_DEFAULT) {
        printf(" 默认启用");
    }
    if (attr & SE_GROUP_ENABLED) {
        printf(" 已启用");
    }
    if (attr & SE_GROUP_USE_FOR_DENY_ONLY) {
        printf(" 仅用于拒绝");
    }
    if (attr & SE_GROUP_INTEGRITY) {
        printf(" 完整性");
    }
    if (attr & SE_GROUP_LOGON_ID) {
        printf(" 登录会话");
    }
}

static const char *elevation_name(TOKEN_ELEVATION_TYPE type) {
    switch (type) {
    case TokenElevationTypeDefault:
        return "Default（未启用 UAC 或内置管理员）";
    case TokenElevationTypeFull:
        return "Full（已提升）";
    case TokenElevationTypeLimited:
        return "Limited（UAC 筛选后的令牌）";
    default:
        return "未知";
    }
}

int main(void) {
    HANDLE token;
    TOKEN_USER *user;
    TOKEN_GROUPS *groups;
    TOKEN_PRIVILEGES *privs;
    TOKEN_ELEVATION_TYPE elevation;
    TOKEN_ELEVATION elevated;
    TOKEN_MANDATORY_LABEL *label;
    DWORD size;
    wchar_t *text;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        fail("OpenProcessToken");
    }

    user = token_info(token, TokenUser);
    text = sid_to_string(user->User.Sid);
    printf("用户 SID: %ls\n", text);
    LocalFree(text);
    printf("帐户:     ");
    print_account(user->User.Sid);
    printf("\n\n");
    free(user);

    elevation = TokenElevationTypeDefault;
    if (!GetTokenInformation(token, TokenElevationType, &elevation, sizeof elevation, &size)) {
        fail("GetTokenInformation(TokenElevationType)");
    }
    if (!GetTokenInformation(token, TokenElevation, &elevated, sizeof elevated, &size)) {
        fail("GetTokenInformation(TokenElevation)");
    }
    printf("提升类型: %s\n", elevation_name(elevation));
    printf("已提升:   %s\n\n", elevated.TokenIsElevated ? "是" : "否");

    label = token_info(token, TokenIntegrityLevel);
    text = sid_to_string(label->Label.Sid);
    printf("完整性 SID: %ls", text);
    LocalFree(text);
    {
        DWORD count = *GetSidSubAuthorityCount(label->Label.Sid);
        DWORD rid = *GetSidSubAuthority(label->Label.Sid, count - 1);

        printf("（RID 0x%lX）\n\n", (unsigned long)rid);
    }
    free(label);

    groups = token_info(token, TokenGroups);
    printf("组 %lu 个:\n", (unsigned long)groups->GroupCount);
    for (DWORD i = 0; i < groups->GroupCount; i++) {
        text = sid_to_string(groups->Groups[i].Sid);
        printf("  %-46ls ", text);
        LocalFree(text);
        print_account(groups->Groups[i].Sid);
        print_attributes(groups->Groups[i].Attributes);
        printf("\n");
    }
    printf("\n");
    free(groups);

    privs = token_info(token, TokenPrivileges);
    printf("特权 %lu 个:\n", (unsigned long)privs->PrivilegeCount);
    for (DWORD i = 0; i < privs->PrivilegeCount; i++) {
        wchar_t name[64];
        DWORD name_len = 64;

        if (LookupPrivilegeNameW(NULL, &privs->Privileges[i].Luid, name, &name_len)) {
            printf("  %-32ls", name);
        } else {
            printf("  %-32s", "(未知)");
        }
        if (privs->Privileges[i].Attributes & SE_PRIVILEGE_ENABLED) {
            printf("已启用\n");
        } else if (privs->Privileges[i].Attributes & SE_PRIVILEGE_ENABLED_BY_DEFAULT) {
            printf("默认启用\n");
        } else {
            printf("已禁用\n");
        }
    }
    free(privs);

    CloseHandle(token);
    return 0;
}