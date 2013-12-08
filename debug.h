/*
 * This can be used like:
 *     DEBUG_PRINT(("var1: %d; var2: %d; str: %s\n", var1, var2, str));
 */

#ifndef _DEBUG_H
    #define _DEBUG_H
    #include <stdio.h>
    
    #ifdef DEBUG
        #define DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__)
    #else
        #define DEBUG_PRINT(...) do {} while (0)
    #endif
#endif
