/*
 * Powered by Mdr-C-Tutorial. No rights reserved.
 */

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int mdr_print(char* format, ...)
{
    va_list args;
    va_start(args, format);

    int num = 0;
    bool in_format_code = false;
    bool after_colon = false;
    char buff[16] = { '%', '\0' };
    char buff_cnt = 1;

    char forward = '\0';
    for (int i = 0; i < strlen(format); i++) {
        if (format[i] == '{' && forward != '\\') {
            assert(in_format_code == false);
            in_format_code = true;
            continue;
        }
        if (format[i] == '}' && forward != '\\') {
            assert(in_format_code == true);
            in_format_code = false;
            after_colon = false;
            continue;
        }

        if (format[i] == ':' && in_format_code == true) {
            after_colon = true;
            continue;
        }

        if (in_format_code == true) {
            if (after_colon == true) {
                buff[buff_cnt] = format[i];
                buff_cnt++;
                buff[buff_cnt] = '\0';
                if (isalpha(format[i])) {
                    if (format[i] == 'd') {
                        num += printf(buff, va_arg(args, int));
                        buff_cnt = 1;
                        buff[buff_cnt] = '\0';
                    }
                }
            }
        } else {
            putchar(format[i]);
            num++;
        }

        forward = format[i];
    }

    va_end(args);
    return num;
}

int main(void)
{
    int result = mdr_print("Mdr is {:d} years old!\n", 16);
    printf("%d\n", result);
    return 0;
}
