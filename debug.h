/*
 * This can be used like:
 *     DEBUG_PRINT(("var1: %d; var2: %d; str: %s\n", var1, var2, str));
 */

#ifndef _DEBUG_H
#define _DEBUG_H
int errprintf(const char* format, ...);
#endif


#ifdef DEBUG
# define DEBUG_PRINT(x) errprintf x
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif
