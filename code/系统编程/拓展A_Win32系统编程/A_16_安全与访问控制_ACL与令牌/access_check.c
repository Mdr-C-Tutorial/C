/* AccessCheck：用当前令牌的副本对一份安全描述符做访问检查，不需要真的去打开对象。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>
#include <sddl.h>

static void check(PSECURITY_DESCRIPTOR sd, DWORD mask, const char *name) {
    HANDLE token;
    HANDLE imp = NULL;
    BYTE priv[256];
    DWORD priv_len = sizeof priv;
    GENERIC_MAPPING map = {FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE,
                           FILE_ALL_ACCESS};
    DWORD granted = 0;
    BOOL status = FALSE;
    BOOL ok;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &token)) {
        printf("OpenProcessToken failed: %lu\n", GetLastError());
        return;
    }
    if (!DuplicateToken(token, SecurityImpersonation, &imp)) {
        printf("DuplicateToken failed: %lu\n", GetLastError());
        CloseHandle(token);
        return;
    }
    MapGenericMask(&mask, &map);
    ok = AccessCheck(sd, imp, mask, &map, (PPRIVILEGE_SET)priv, &priv_len, &granted,
                     &status);
    if (!ok) {
        printf("%s：AccessCheck 调用失败，error %lu\n", name, GetLastError());
    } else {
        printf("%s：请求 0x%08lX -> %s，授予 0x%08lX\n", name, (unsigned long)mask,
               status ? "允许" : "拒绝", (unsigned long)granted);
    }
    CloseHandle(imp);
    CloseHandle(token);
}

int main(void) {
    PSECURITY_DESCRIPTOR sd = NULL;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    /* AccessCheck 要求安全描述符里带所有者和主要组，否则返回 ERROR_INVALID_SECURITY_DESCR。 */
    ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"O:BAG:BAD:P(A;;FR;;;BU)", SDDL_REVISION_1, &sd, NULL);
    check(sd, GENERIC_READ, "允许 Users 读");
    check(sd, GENERIC_WRITE, "允许 Users 读");
    check(sd, MAXIMUM_ALLOWED, "允许 Users 读");
    LocalFree(sd);

    ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"O:BAG:BAD:P(A;;FA;;;BU)(D;;0x2;;;BU)", SDDL_REVISION_1, &sd, NULL);
    check(sd, GENERIC_READ, "先全权，后拒绝写");
    check(sd, GENERIC_WRITE, "先全权，后拒绝写");
    LocalFree(sd);

    ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"O:BAG:BAD:P(D;;0x2;;;BU)(A;;FA;;;BU)", SDDL_REVISION_1, &sd, NULL);
    check(sd, GENERIC_READ, "先拒绝写，后全权");
    check(sd, GENERIC_WRITE, "先拒绝写，后全权");
    LocalFree(sd);
    return 0;
}