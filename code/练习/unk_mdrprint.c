#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MDR_PRINT_MAX_ARGUMENT_NUM 64

#define MDR_ALIAS typedef

MDR_ALIAS signed char byte;

struct argument {
    byte type;
    byte plain_value[8];
};

enum arg_type {
    mdr_int,
    mdr_long,
    mdr_long_long,
    mdr_short,
    mdr_byte,
    mdr_char,
    mdr_char_p,
    mdr_int_p,
    mdr_float,
    mdr_double,
    mdr_size_t,
    mdr_ptrdiff_t,
    mdr_int_max
};

/*
 * d -> int
 * u -> unsigned int
 * c -> char
 * s -> char*
 * banned: zd tu to tx tX hhn hn ln lln zn tn jn
 */
struct arg_type_specifier {
    bool is_long_long = false;    // ll
    bool is_long = false;         // l
    bool is_short = false;        // s
    bool is_byte = false;         // ss
    bool is_capital_long = false; // L
    bool is_size_t = false;       // z
    bool is_ptrdiff_t = false;    // t
    bool is_int_max = false;      // j
};

bool is_specifier_integer(char c) {
    return c == 'd' || c == 'i' || c == 'o' || c == 'x' || c == 'X' || c == 'u';
}

bool is_specifier_float(char c) {
    return c == 'f' || c == 'F' || c == 'e' || c == 'E' || c == 'a' ||
           c == 'A' || c == 'g' || c == 'G';
}

void clear_buff(char *buff, int *buff_cnt) {
    *buff_cnt = 1;
    buff[*buff_cnt] = '\0';
}

bool judge_multiplexing(const char *format) {
    bool result = true;
    for (int i = 1; i < strlen(format); i++) {
        if (format[i - 1] == '{' && format[i] == ':') {
            result = false;
        }
    }
    return result;
}

int mdr_print(char *restrict format, ...) {
    va_list args;
    va_start(args, format);

    const bool multiplexed = judge_multiplexing(format);

    int num = 0;
    bool in_format_code = false;
    bool after_colon = false;

    char buff[16] = {'%', '\0'};
    int buff_cnt = 1;

    bool is_long = false;
    bool is_long_long = false;
    bool is_capital_long = false;

    char forward = '\0';

    struct argument *arguments = NULL;

    if (multiplexed) {
        arguments = (struct argument *)malloc(sizeof(struct argument) *
                                              MDR_PRINT_MAX_ARGUMENT_NUM);

        int arg_max = 0;
        int arg_cnt = 0;

        struct arg_type_specifier specifiers = {};

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

            if (in_format_code && !after_colon && isdigit(format[i])) {
                arg_cnt = arg_cnt * 10 + (format[i] - '0');
            }

            if (format[i] == ':' && in_format_code == true) {
                after_colon = true;
                forward = format[i];
                continue;
            }

            if (in_format_code == true) {
                if (format[i] == '{' && forward == '{') {
                    in_format_code = false;
                    continue;
                }
                if (after_colon == true) {
                    if (isalpha(format[i])) {
                        if (is_specifier_integer(format[i])) {
                            if (specifiers.is_long_long) {
                                arguments[arg_cnt].type = mdr_long_long;
                                arg_max = arg_cnt;
                                arg_cnt = 0;
                            }
                            num += printf(buff, va_arg(args, int));
                        } else if (is_specifier_float(format[i])) {
                            num += printf(buff, va_arg(args, double));
                        } else if (format[i] == 'c') {
                            num += printf(buff, va_arg(args, int));
                        } else if (format[i] == 's') {
                        } else if (format[i] == 'l') {
                            if (!specifiers.is_long) {
                                specifiers.is_long = true;
                            } else if (!specifiers.is_long_long) {
                                specifiers.is_long_long = true;
                            } else {
                                perror("specifier ?");
                            }
                        }
                    }
                }

                forward = format[i];
            }
        }
        for (int i = 0; i < arg_max; i++) {
            if (arguments[i].type == mdr_int) {
                int value = va_arg(args, int);
                byte *ptr = (byte *)&value;
                for (int j = 0; j < 8; j++) {
                    arguments[arg_cnt].plain_value[j] = ptr[j];
                }
            }
        }
    }
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
                num += 1;
                continue;
            }
            if (after_colon == true) {
                buff[buff_cnt] = format[i];
                buff_cnt++;
                buff[buff_cnt] = '\0';
                if (isalpha(format[i])) {
                    if (is_specifier_integer(format[i])) {
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
                    } else if (is_specifier_float(format[i])) {
                        if (is_capital_long) {
                            num += printf(buff, va_arg(args, long double));
                        } else {
                            num += printf(buff, va_arg(args, double));
                        }
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
                    } else if (format[i] == 'L') {
                        is_capital_long = true;
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

int main(void) {
    int result = mdr_print("{{{{{{ {{:d} {:d} {:.3f} {:c} {:lld}\n", 16,
                           3.1415926, 'm', 1145141919810);
    printf("%d\n", result);
    mdr_print("{:f} {:A} {:e} {:g}\n", 16.2, 100.2222, 16.2, 16.2);
    return 0;
}
