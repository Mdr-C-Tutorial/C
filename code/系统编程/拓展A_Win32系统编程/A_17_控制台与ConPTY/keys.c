/* keys.c —— 用 ReadConsoleInputW 逐键读取，读到 q 为止。
 *
 * 构建：gcc -std=c17 -Wall -Wextra -o keys.exe keys.c
 * 运行：keys.exe，然后按键；按 q 退出。
 */
#include <windows.h>
#include <stdio.h>

static void describe(const INPUT_RECORD *rec) {
    switch (rec->EventType) {
    case KEY_EVENT: {
        const KEY_EVENT_RECORD *k = &rec->Event.KeyEvent;
        const char *state = k->bKeyDown ? "down" : "up  ";
        if (k->uChar.UnicodeChar >= 0x20 && k->uChar.UnicodeChar < 0x7f) {
            printf("KEY   %s  code=0x%02X  char='%c'  repeat=%lu\n", state, k->wVirtualKeyCode,
                   k->uChar.UnicodeChar, (unsigned long)k->wRepeatCount);
        } else {
            printf("KEY   %s  code=0x%02X  char=U+%04X  repeat=%lu\n", state, k->wVirtualKeyCode,
                   k->uChar.UnicodeChar, (unsigned long)k->wRepeatCount);
        }
        break;
    }
    case WINDOW_BUFFER_SIZE_EVENT: {
        const WINDOW_BUFFER_SIZE_RECORD *s = &rec->Event.WindowBufferSizeEvent;
        printf("SIZE   %dx%d\n", s->dwSize.X, s->dwSize.Y);
        break;
    }
    case MOUSE_EVENT: {
        const MOUSE_EVENT_RECORD *m = &rec->Event.MouseEvent;
        printf("MOUSE  at (%d,%d)  buttons=0x%04lX\n", m->dwMousePosition.X, m->dwMousePosition.Y,
               (unsigned long)m->dwButtonState);
        break;
    }
    default:
        printf("event type %u\n", rec->EventType);
        break;
    }
}

int main(void) {
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    DWORD saved;
    if (!GetConsoleMode(in, &saved)) {
        printf("stdin is not a console (error %lu)\n", GetLastError());
        return 1;
    }

    DWORD mode = (saved & ~(DWORD)(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT)) | ENABLE_WINDOW_INPUT;
    if (!SetConsoleMode(in, mode)) {
        printf("SetConsoleMode failed: %lu\n", GetLastError());
        return 1;
    }

    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        printf("window %dx%d, buffer %dx%d\n", info.srWindow.Right - info.srWindow.Left + 1,
               info.srWindow.Bottom - info.srWindow.Top + 1, info.dwSize.X, info.dwSize.Y);
    }
    printf("press keys, q to quit\n");
    fflush(stdout);

    int key_downs = 0;
    for (;;) {
        INPUT_RECORD rec;
        DWORD n = 0;
        if (!ReadConsoleInputW(in, &rec, 1, &n)) {
            printf("ReadConsoleInputW failed: %lu\n", GetLastError());
            break;
        }
        if (n == 0) {
            continue;
        }
        describe(&rec);
        fflush(stdout);
        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) {
            key_downs++;
            if (rec.Event.KeyEvent.uChar.UnicodeChar == L'q') {
                break;
            }
        }
    }

    printf("key-down events: %d\n", key_downs);
    SetConsoleMode(in, saved);
    return 0;
}