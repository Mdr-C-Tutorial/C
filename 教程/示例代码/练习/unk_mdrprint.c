/*
 * Powered by Mdr-C-Tutorial. No rights reserved.
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void clear_buff(char* buff, int* buff_cnt)
{
    *buff_cnt = 1;
    buff[*buff_cnt] = '\0';
}

int mdr_print(char* format, ...)
{
    va_list args;
    va_start(args, format);

    int num = 0;
    bool in_format_code = false;
    bool after_colon = false;

    char buff[16] = { '%', '\0' };
    int buff_cnt = 1;

    bool is_long = false;
    bool is_long_long = false;

    char forward = '\0';
    for (int i = 0; i < strlen(format); i++) {
        if (format[i] == '{' && in_format_code == false) {
            in_format_code = true;
            forward = format[i];
            continue;
        }
        if (format[i] == '}' && in_format_code) {
            in_format_code = false;
            after_colon = false;
            forward = format[i];
            continue;
        }

        if (format[i] == ':' && in_format_code == true) {
            after_colon = true;
            forward = format[i];
            continue;
        }

        if (in_format_code == true) {
            if (format[i] == '{' && forward == '{') {
                in_format_code = false;
                putchar('{');
                continue;
            }
            if (after_colon == true) {
                buff[buff_cnt] = format[i];
                buff_cnt++;
                buff[buff_cnt] = '\0';
                if (isalpha(format[i])) {
                    if (format[i] == 'd') {
                        if (is_long && is_long_long) {
                            num += printf(buff, va_arg(args, long long));
                        } else if (is_long) {
                            num += printf(buff, va_arg(args, long));
                        } else {
                            num += printf(buff, va_arg(args, int));
                        }
                        is_long = false;
                        is_long_long = false;
                        clear_buff(buff, &buff_cnt);
                    } else if (format[i] == 'f') {
                        num += printf(buff, va_arg(args, double));
                        clear_buff(buff, &buff_cnt);
                    } else if (format[i] == 'c') {
                        num += printf(buff, va_arg(args, int));
                        clear_buff(buff, &buff_cnt);
                    } else if (format[i] == 'l') {
                        if (!is_long) {
                            is_long = true;
                        } else if (!is_long_long) {
                            is_long_long = true;
                        } else {
                            perror("Unknown Format Code");
                        }
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
    int result = mdr_print("{{ {{:d} {:d} {:.3f} {:c} {:lld}\n", 16, 3.1415926, 'm', 1145141919810);
    printf("%d\n", result);
    return 0;
}
