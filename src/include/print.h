#ifndef PRINT_H
#define PRINT_H

/*
 * These routines do their printing without using stdio. stdio can't
 * be used because it calls malloc(). Internal routines of a malloc()
 * debugger should not re-enter malloc(), so stdio is out.
 */

void print(char* format_string, ...);
void print_error(char* format_string, ...);
void fl_error(char* format_string, ...);

#endif // PRINT_H

