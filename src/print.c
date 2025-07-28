
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

#include "print.h"

/**
 * Unbuffered version of vprintf
 */
static void handle_print_string(char* src);
static void handle_print_integer(int src);
static void handle_print_adress(void* src);

static size_t strlength(const char* s);
static void vprint(const char* format_string, ...);

static void
vprint(const char* format_string, ...)
{
    size_t fs_length = 0;
    size_t va_count = 0;
    size_t curr_offset = 0;
    va_list args;

    /* find the length of format string */
    fs_length = strlength(format_string);

    va_start(args, format_string);
    /* iterate through each character in format strig and accomodate it in buffer */
    for (size_t i = 0; i < fs_length; i++)
    {
        /* encountered a formatter character */
        if (format_string[i] == '%')
        {
            i++;
            switch (format_string[i])
            {
                /* string */
                case 's':
                    handle_print_string(va_arg(args, char*));
                    break;
                /* integer as base 10 */
                case 'd':
                    handle_print_integer(va_arg(args, int));
                    break;
                /* address */
                case 'a':
                    handle_print_adress(va_arg(args, void*));
                    break;
                default:
                    va_end(args);
                    // error
                    return;
            }

            continue;
        }

        // Print the character encountered
        write(1, format_string + i, sizeof(char));
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
handle_print_string(char* src)
{
    size_t src_len = strlength(src);
    write(1, src, sizeof(char)*src_len);
}

static void
handle_print_integer(int src)
{
    char digit;

    if (src == 0)
    {
        digit = src + '0';
        write(1, &digit, sizeof(char));
        return;
    }

    if (src < 0)
    {
        digit = '-';
        write(1, &digit, sizeof(char));
        src = src*(-1);
    }

    int d = 1000000000;
    bool is_started = false;
    while(d /= 10)
    {
        digit = ((src/d) % 10) + '0';
        if (digit == '0' && !is_started) continue;
        else is_started= true;

        write(1, &digit, sizeof(char));
    }
}

static void
handle_print_adress(void* src)
{
    // const char hex_digits[] = "0123456789abcdef";
    // uintptr_t sr_addr = (uintptr_t)src;
    // int num_nibbles = 0;
    // int started = 0;

    // /* print the 0x to represent pointer in hex */
    // dst[curr_off++] = '0';
    // if (curr_off < n) return;
    // dst[curr_off++] = 'x';
    // if (curr_off < n) return;

    // num_nibbles = 2 * sizeof(uintptr_t);

    // for (int shift = (num_nibbles - 1)*4; shift >= 0; shift -= 4)
    // {
    //     char digit = (sr_addr >> shift) & 0xF;

    //     if (digit != 0 || started || shift == 0)
    //     {
    //         dst[curr_off++] = hex_digits[digit];
    //         if (curr_off < n) return;
    //         started = 1;
    //     }
    // }

}


int main()
{
    vprint("asdh%du\n", 1020);
    return 0;
}