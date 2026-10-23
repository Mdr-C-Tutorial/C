/* 成员检查：令牌中「仅用于拒绝」的组不算成员。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>

int main(void) {
    struct {
        const wchar_t *name;
        WELL_KNOWN_SID_TYPE type;
    } items[] = {
        {L"Everyone", WinWorldSid},
        {L"BUILTIN\\Administrators", WinBuiltinAdministratorsSid},
        {L"BUILTIN\\Users", WinBuiltinUsersSid},
        {L"NT AUTHORITY\\SYSTEM", WinLocalSystemSid},
    };
    BYTE sid[SECURITY_MAX_SID_SIZE];

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    for (int i = 0; i < 4; i++) {
        DWORD size = sizeof sid;
        BOOL member = FALSE;

        if (!CreateWellKnownSid(items[i].type, NULL, sid, &size)) {
            printf("CreateWellKnownSid failed: error %lu\n", GetLastError());
            return 1;
        }
        if (!CheckTokenMembership(NULL, sid, &member)) {
            printf("CheckTokenMembership failed: error %lu\n", GetLastError());
            return 1;
        }
        printf("%ls：%s成员\n", items[i].name, member ? "是" : "不是");
    }
    return 0;
}