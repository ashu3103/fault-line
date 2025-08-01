
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <print.h>

/**
 * Unbuffered version of vprintf
 * We need this to remove the dependency on stdio.h library as printf implemented by
 * stdio.h uses malloc()
 */
static void handle_print_string(int stream, char* src);
static void handle_print_integer(int stream, int src);
static void handle_print_adress(int stream, void* src);
static void handle_print_char(int stream, char c);
static void handle_print_unsigned_long(int stream, unsigned long src);

static size_t strlength(const char* s);
static void vprint(int stream, char* format_string, va_list args);

void 
print(char* format_string, ...)
{
    va_list args;
    va_start(args, format_string);
    vprint(1, format_string, args);
    va_end(args);
}

void
print_error(char* format_string, ...)
{
    va_list args;
    va_start(args, format_string);
    vprint(2, format_string, args);
    va_end(args);
}

void 
fl_error(char* format_string, ...)
{
   va_list args;
    va_start(args, format_string);
    vprint(2, format_string, args);
    va_end(args);

    /* exit the process */
    _exit(1);
}

static void
vprint(int stream, char* format_string, va_list args)
{
    size_t fs_length = 0;

    /* find the length of format string */
    fs_length = strlength(format_string);

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
                    handle_print_string(stream, va_arg(args, char*));
                    break;
                /* character */
                case 'c':
                    handle_print_char(stream, (va_arg(args, int) + '0'));
                    break;
                /* integer as base 10 */
                case 'd':
                    handle_print_integer(stream, va_arg(args, int));
                    break;
                /* unsigned long */
                case 'U':
                    handle_print_unsigned_long(stream, va_arg(args, unsigned long));
                    break;
                /* address */
                case 'a':
                    handle_print_adress(stream, va_arg(args, void*));
                    break;
                default:
                    va_end(args);
                    // error
                    print_error("invalid formatter encountered\n");
                    return;
            }

            continue;
        }

        // Print the character encountered
        write(stream, format_string + i, sizeof(char));
    }
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
handle_print_string(int stream, char* src)
{
    size_t src_len = strlength(src);
    write(stream, src, sizeof(char)*src_len);
}

static void 
handle_print_unsigned_long(int stream, unsigned long src)
{
    char digits[20];  // Enough for 64-bit unsigned long (max ~20 digits)
    int i = 0;

    if (src == 0) {
        handle_print_char(stream, '0');
        return;
    }

    while (src > 0) {
        digits[i++] = '0' + (src % 10);
        src /= 10;
    }

    while (i--) {
        handle_print_char(stream, digits[i]);
    }
}

static void
handle_print_char(int stream, char c)
{
    write(stream, &c, sizeof(char));
}

static void
handle_print_integer(int stream, int src)
{
    char digits[12];

    if (src == 0)
    {
        handle_print_char(stream, '0');
        return;
    }

    if (src < 0)
    {
        handle_print_char(stream, '-');
        src = src*(-1);
    }

    int i = 0;
    while (src > 0) {
        digits[i++] = '0' + (src % 10);
        src /= 10;
    }

    while (i--) {
        handle_print_char(stream, digits[i]);
    }
}

static void
handle_print_adress(int stream, void* src)
{
    char chr;

    const char hex_digits[] = "0123456789abcdef";
    uintptr_t sr_addr = (uintptr_t)src;
    int num_nibbles = 0;
    int started = 0;

    /* print the 0x to represent pointer in hex */
    chr = '0';
    handle_print_char(stream, '0');
    chr = 'x';
    handle_print_char(stream, 'x');

    num_nibbles = 2 * sizeof(uintptr_t);

    for (int shift = (num_nibbles - 1)*4; shift >= 0; shift -= 4)
    {
        int digit = (sr_addr >> shift) & 0xF;

        if (digit != 0 || started || shift == 0)
        {
            chr = hex_digits[digit];
            handle_print_char(stream, chr);
            started = 1;
        }
    }
}


int main() {

    print("adsfg%e\n");

    return 0;
}