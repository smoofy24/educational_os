#include <stdarg.h>
#include <stdbool.h>
#include "drivers/uart.h"
#include "lib/printk.h"

#define kputc(c) uart_putc(c)

typedef struct {
    bool pad_with_zeros;
    bool left_align;
    bool show_sign;
    bool alt_form;
    int width;
    int precision;
    bool has_precision;
    char length_modifier;
    char type;
} format_spec_t;

static void print_number(long num, format_spec_t spec) {
    unsigned long unum = (num < 0) ? -(unsigned long)num : (unsigned long)num;
    char buffer[21];
    bool negative = num < 0;
    int i = 0;

    if (unum == 0) {
        buffer[i++] = '0';
    } else {
        while (unum > 0) {
            buffer[i++] = '0' + (unum % 10);
            unum /= 10;
        }
    }

    int digits = i;
    int pad = (spec.width > digits + (negative ? 1 : 0))
              ? (spec.width - digits - (negative ? 1 : 0))
              : 0;

    if (negative && spec.pad_with_zeros) {
        kputc('-');
    }

    char padder = spec.pad_with_zeros ? '0' : ' ';
    while (pad-- > 0) {
        kputc(padder);
    }

    if (negative && !spec.pad_with_zeros) {
        kputc('-');
    }

    while (i > 0) {
        kputc(buffer[--i]);
    }
}

static void print_unsigned(unsigned long num, format_spec_t spec) {
    char buffer[21];
    int i = 0;

    if (num == 0) {
        buffer[i++] = '0';
    } else {
        while (num > 0) {
            buffer[i++] = '0' + (num % 10);
            num /= 10;
        }
    }

    int digits = i;
    int pad = (spec.width > digits) ? (spec.width - digits) : 0;

    char padder = spec.pad_with_zeros ? '0' : ' ';
    while (pad-- > 0) {
        kputc(padder);
    }

    while (i > 0) {
        kputc(buffer[--i]);
    }
}

static void print_hex(unsigned long num, bool capital, format_spec_t spec) {
    char buffer[16];
    int i = 0;

    if (num == 0) {
        buffer[i++] = '0';
    } else {
        while (num > 0) {
            int digit = num % 16;
            buffer[i++] = (digit < 10) ? ('0' + digit)
                                       : ((capital ? 'A' : 'a') + (digit - 10));
            num /= 16;
        }
    }

    int digits = i;
    int pad = (spec.width > digits) ? (spec.width - digits) : 0;

    char padder = spec.pad_with_zeros ? '0' : ' ';
    while (pad-- > 0) {
        kputc(padder);
    }

    while (i > 0) {
        kputc(buffer[--i]);
    }
}

static format_spec_t parse_format_spec(const char **format) {
    format_spec_t spec = { 0 };

    (*format)++;

    // Parse flags: -, +, space, #, 0
    while (**format == '-' || **format == '+' || **format == ' ' || **format == '#' || **format == '0') {
        if (**format == '-')
            spec.left_align = true;
        if (**format == '0')
            spec.pad_with_zeros = true;
        if (**format == '+')
            spec.show_sign = true;
        if (**format == '#')
            spec.alt_form = true;
        (*format)++;
    }

    // Parse width
    while (**format >= '0' && **format <= '9') {
        spec.width = spec.width * 10 + (**format - '0');
        (*format)++;
    }

    // Parse precision
    if (**format == '.') {
        spec.has_precision = true;
        (*format)++;
        while (**format >= '0' && **format <= '9') {
            spec.precision = spec.precision * 10 + (**format - '0');
            (*format)++;
        }
    }

    // Parse length modifier
    if (**format == 'l' || **format == 'h') {
        spec.length_modifier = **format;
        (*format)++;
    }

    spec.type = **format;
    (*format)++;

    return spec;
}

void printk(const char *format, ...) {
    va_list ap;
    va_start(ap, format);

    while (*format) {
        if (*format == '%') {
            format_spec_t spec = parse_format_spec(&format);

            switch (spec.type) {
                case '%': {
                    kputc('%');
                    break;
                }

                case 'c': {
                    int val = va_arg(ap, int);
                    kputc(val);
                    break;
                }

                case 's': {
                    const char *str = va_arg(ap, const char*);

                    if (str) {
                        while (*str) {
                            kputc(*str++);
                        }
                    } else {
                        kputc('('); kputc('n'); kputc('u'); kputc('l'); kputc('l'); kputc(')');
                    }

                    break;
                }

                case 'd': {
                    long num;
                    if (spec.length_modifier == 'l') {
                        num = va_arg(ap, long);
                    } else {
                        num = va_arg(ap, int);
                    }
                    print_number(num, spec);
                    break;
                }

                case 'u': {
                    unsigned long num;
                    if (spec.length_modifier == 'l') {
                        num = va_arg(ap, unsigned long);
                    } else {
                        num = va_arg(ap, unsigned int);
                    }
                    print_unsigned(num, spec);
                    break;
                }

                case 'p': {
                    void *ptr = va_arg(ap, void*);
                    if (ptr == NULL) {
                        kputc('('); kputc('n'); kputc('i'); kputc('l'); kputc(')');
                    } else {
                        unsigned long addr = (unsigned long)ptr;
                        format_spec_t ptr_spec = spec;
                        ptr_spec.width = 16;
                        ptr_spec.pad_with_zeros = true;
                        kputc('0');
                        kputc('x');
                        print_hex(addr, false, ptr_spec);
                    }
                    break;
                }

                case 'x': {
                    unsigned long num;
                    if (spec.length_modifier == 'l') {
                        num = va_arg(ap, unsigned long);
                    } else {
                        num = va_arg(ap, unsigned int);
                    }
                    print_hex(num, false, spec);
                    break;
                }

                case 'X': {
                    unsigned long num;
                    if (spec.length_modifier == 'l') {
                        num = va_arg(ap, unsigned long);
                    } else {
                        num = va_arg(ap, unsigned int);
                    }
                    print_hex(num, true, spec);
                    break;
                }
            }
        } else {
            kputc(*format++);
        }
    }

    va_end(ap);
}
