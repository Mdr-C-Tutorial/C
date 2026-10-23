/* private_ns.c：在私有名字空间里创建互斥量，别的进程按同一个名字打不开。 */
#include <stdio.h>
#include <windows.h>

#define BOUNDARY_NAME L"MdrTutorial.Boundary"
#define PRIVATE_NAME L"MdrTutorial.Private"

int main(void) {
    HANDLE token = NULL;
    BYTE buffer[256];
    DWORD size = 0;
    PSID sid = NULL;
    HANDLE boundary;
    HANDLE ns;
    HANDLE mutex;
    HANDLE outside;
    BOOL ok;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        printf("OpenProcessToken failed: %lu\n", GetLastError());
        return 1;
    }
    if (!GetTokenInformation(token, TokenUser, buffer, sizeof buffer, &size)) {
        printf("GetTokenInformation failed: %lu\n", GetLastError());
        CloseHandle(token);
        return 1;
    }
    sid = ((TOKEN_USER *)buffer)->User.Sid;
    CloseHandle(token);

    boundary = CreateBoundaryDescriptorW(BOUNDARY_NAME, 0);
    SetLastError(0);
    ok = boundary != NULL && AddSIDToBoundaryDescriptor(&boundary, sid);
    printf("boundary descriptor: %d, error %lu\n", ok, GetLastError());
    if (!ok) {
        return 1;
    }

    SetLastError(0);
    ns = CreatePrivateNamespaceW(NULL, boundary, PRIVATE_NAME);
    printf("CreatePrivateNamespaceW -> %p, last error %lu\n", (void *)ns, GetLastError());

    SetLastError(0);
    mutex = CreateMutexW(NULL, FALSE, PRIVATE_NAME L"\\SingleInstance");
    printf("CreateMutexW inside the namespace -> %p, last error %lu\n", (void *)mutex,
           GetLastError());

    outside = OpenMutexW(SYNCHRONIZE, FALSE,
                         L"Local\\" PRIVATE_NAME L"\\SingleInstance");
    printf("OpenMutexW on the same text outside -> %p, error %lu\n", (void *)outside,
           GetLastError());
    if (outside != NULL) {
        CloseHandle(outside);
    }
    if (mutex != NULL) {
        CloseHandle(mutex);
    }
    ClosePrivateNamespace(ns, 0);
    DeleteBoundaryDescriptor(boundary);
    return 0;
}