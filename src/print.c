
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>

#include "print.h"

static void handle_print_string(char* dst, size_t n, size_t curr_off, char* src);
static void handle_print_integer(char* dst, size_t n, size_t curr_off, int src);
static void handle_print_adress(char* dst, size_t n, size_t curr_off, void* src);

static size_t strlength(const char* s);
static void vprint(char* s, size_t n, const char* format_string, ...);

static void
vprint(char* s, size_t n, const char* format_string, ...)
{
    size_t fs_length = 0;
    size_t va_count = 0;
    size_t curr_offset = 0;
    va_list args;

    /* find the length of format string */
    fs_length = strlength(format_string);

    /* derive the number of arguments in vr */
    for (size_t i = 0; i < fs_length; i++)
    {
        if (format_string[i] == '%')
        {
            if (i + 1 < fs_length)
            {
                return;
            }
            va_count++;
        }
    }

    va_start(args, va_count);
    /* iterate through each character in format strig and accomodate it in buffer */
    for (size_t i = 0; i < fs_length, curr_offset < n; i++)
    {
        /* encountered a formatter character */
        if (format_string[i] == '%')
        {
            i++;
            switch (format_string[i])
            {
                /* string */
                case 's':
                    handle_print_string(s, n, curr_offset, va_arg(args, char*));
                    break;
                /* integer as base 10 */
                case 'd':
                    handle_print_integer(s, n, curr_offset, va_arg(args, int));
                    break;
                /* address */
                case 'a':
                    handle_print_adress(s, n, curr_offset, va_arg(args, void*));
                    break;
                default:
                    va_end(args);
                    // error
                    return;
            }

            continue;
        }

        s[curr_offset] = format_string[i];
        curr_offset++;
    }

    va_end(args);
}

static size_t
strlength(const char* s)
{
    size_t s_len = 0;

    if (s == NULL) return s_len;

    while (*s != '\0')
    {
        s_len++;
        s++;
    }

    return s_len;
}

static void 
handle_print_string(char* dst, size_t n, size_t curr_off, char* src)
{
    size_t src_len = strlength(src);

    for (size_t j = 0; j < src_len, curr_off < n; j++)
    {
        dst[curr_off++] = src[j];
    }
}

static void
handle_print_integer(char* dst, size_t n, size_t curr_off, int src)
{
    if (src == 0)
    {
        dst[curr_off++] = '0';
        return;
    }

    if (src < 0)
    {
        dst[curr_off++] = '-';
        src = src*(-1);
    }

    while(curr_off < n && src > 0)
    {
        dst[curr_off++] = (src % 10) + '0';
        src /= 10;
    }
}

static void
handle_print_adress(char* dst, size_t n, size_t curr_off, void* src)
{
    const char hex_digits[] = "0123456789abcdef";
    uintptr_t sr_addr = (uintptr_t)src;
    int num_nibbles = 0;
    int started = 0;

    /* print the 0x to represent pointer in hex */
    dst[curr_off++] = '0';
    if (curr_off < n) return;
    dst[curr_off++] = 'x';
    if (curr_off < n) return;

    num_nibbles = 2 * sizeof(uintptr_t);

    for (int shift = (num_nibbles - 1)*4; shift >= 0; shift -= 4)
    {
        char digit = (sr_addr >> shift) & 0xF;

        if (digit != 0 || started || shift == 0)
        {
            dst[curr_off++] = hex_digits[digit];
            if (curr_off < n) return;
            started = 1;
        }
    }

}
