/* 命名共享内存的读方：按名字打开映射，读出写方更新过的计数 */
#include <windows.h>

#include <stdio.h>

#define SHM_SIZE 4096
#define SHM_NAME L"Local\\MdrTutorial.Shm"

struct shared_block {
    volatile LONG counter;
    volatile LONG ticks;
    char message[64];
};

int main(void) {
    HANDLE map = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, SHM_NAME);
    if (map == NULL) {
        printf("OpenFileMappingW failed: error %lu\n",
               (unsigned long)GetLastError());
        return 1;
    }
    struct shared_block *shm =
        MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);
    if (shm == NULL) {
        printf("MapViewOfFile failed: %lu\n", (unsigned long)GetLastError());
        CloseHandle(map);
        return 1;
    }

    printf("opened %ls, view at %p\n", SHM_NAME, (void *)shm);
    printf("message: \"%s\"\n", shm->message);
    for (int i = 0; i < 5; i++) {
        printf("reader sees counter = %ld, ticks = %ld\n", (long)shm->counter,
               (long)shm->ticks);
        Sleep(400);
    }

    UnmapViewOfFile(shm);
    CloseHandle(map);
    return 0;
}