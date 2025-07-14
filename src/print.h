#ifndef PRINT_H
#define PRINT_H

#include <stdlib.h>
#include <stdarg.h>

/*
 * These routines do their printing without using stdio. stdio can't
 * be used because it calls malloc(). Internal routines of a malloc()
 * debugger should not re-enter malloc(), so stdio is out.
 */

/**
 * Allowed formatters:
 * - 
 */

static void vprintf(char* format_string, va_list args);




#endif // PRINT_H

